#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <chrono>

#include "database/DbPool.hpp"
#include "database/VoterRepository.hpp"
#include "database/CandidateRepository.hpp"
#include "database/ElectionRepository.hpp"
#include "database/VoteRepository.hpp"
#include "database/AuditLogRepository.hpp"
#include "services/VotingService.hpp"
#include "security/CryptoUtils.hpp"

using namespace voting;

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "   RUNNING ADVERSARIAL CONCURRENCY & RACE TEST HARNESS    " << std::endl;
    std::cout << "==========================================================" << std::endl;

    std::string connStr = "host=localhost port=5432 dbname=secure_voting_db user=subscribly_user password=1";
    database::DbPool pool(connStr, 25); // 25 connections for high concurrency

    database::VoterRepository voterRepo(pool);
    database::CandidateRepository candidateRepo(pool);
    database::ElectionRepository electionRepo(pool);
    database::VoteRepository voteRepo(pool);
    database::AuditLogRepository auditRepo(pool);
    services::VotingService votingService(voteRepo, voterRepo, electionRepo, auditRepo);

    // 1. Setup isolated test fixtures
    std::cout << "[SETUP] Creating dedicated concurrency test fixtures..." << std::endl;
    std::string voterIdCode = "RACE-VOTER-" + security::CryptoUtils::randomHex(4);
    std::string candCode = "RACE-CAND-" + security::CryptoUtils::randomHex(4);
    
    domain::Voter voter("", voterIdCode, "Race Voter", voterIdCode + "@example.org", "dummy_hash", true);
    std::string voterUuid = voterRepo.create(voter);

    domain::Candidate cand("", candCode, "Race Candidate", "Party Race", "Bolt");
    std::string candUuid = candidateRepo.create(cand);

    domain::Election election(
        "",
        "Concurrency Race Election " + security::CryptoUtils::randomHex(4),
        "Testing high-concurrency double-vote mitigation",
        domain::ElectionStatus::Open,
        "2026-01-01T00:00:00Z",
        "2026-12-31T23:59:59Z"
    );
    std::string electionUuid = electionRepo.create(election);

    electionRepo.assignCandidate(electionUuid, candUuid);
    electionRepo.assignVoterEligibility(electionUuid, voterUuid);

    std::cout << "[SETUP] Test fixtures created:" << std::endl;
    std::cout << "        Election: " << electionUuid << std::endl;
    std::cout << "        Voter: " << voterUuid << " (" << voterIdCode << ")" << std::endl;
    std::cout << "        Candidate: " << candUuid << std::endl;

    // ----------------------------------------------------------------------
    // TEST 1: Simultaneous Double-Vote Race Test (20 Concurrent Threads)
    // ----------------------------------------------------------------------
    std::cout << "\n[TEST 1] Launching 20 simultaneous threads attempting to cast a ballot for the SAME voter..." << std::endl;

    const int THREAD_COUNT = 20;
    std::atomic<bool> startGate{false};
    std::atomic<int> successCount{0};
    std::atomic<int> doubleVoteCount{0};
    std::atomic<int> otherErrorCount{0};

    std::vector<std::thread> workers;
    workers.reserve(THREAD_COUNT);

    for (int i = 0; i < THREAD_COUNT; ++i) {
        workers.emplace_back([&, i]() {
            // Spin-wait on start gate for synchronized simultaneous release
            while (!startGate.load(std::memory_order_relaxed)) {
                std::this_thread::yield();
            }

            domain::BallotSubmission sub;
            sub.electionId = electionUuid;
            sub.candidateId = candUuid;
            sub.idempotencyKey = "unique-attempt-" + std::to_string(i) + "-" + security::CryptoUtils::randomHex(8);

            try {
                votingService.castBallot(voterUuid, sub, "127.0.0.1");
                successCount.fetch_add(1, std::memory_order_relaxed);
            } catch (const std::exception& e) {
                std::string msg = e.what();
                if (msg.find("already") != std::string::npos || msg.find("DOUBLE_VOTE") != std::string::npos) {
                    doubleVoteCount.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::cerr << "Unexpected error: " << msg << std::endl;
                    otherErrorCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    // Release all 20 threads simultaneously
    startGate.store(true, std::memory_order_release);

    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }

    std::cout << "[RESULTS] Concurrency execution completed:" << std::endl;
    std::cout << "          Successful accepted ballots: " << successCount.load() << std::endl;
    std::cout << "          Rejected double-votes:       " << doubleVoteCount.load() << std::endl;
    std::cout << "          Other errors:                " << otherErrorCount.load() << std::endl;

    // Strict Correctness Assertions
    assert(successCount.load() == 1);
    assert(doubleVoteCount.load() == THREAD_COUNT - 1);
    assert(otherErrorCount.load() == 0);

    // Verify Database Persistence Invariants
    {
        auto conn = pool.acquire();
        auto votesRes = conn->executeParams("SELECT COUNT(*) FROM votes WHERE election_id = $1::uuid;", {electionUuid});
        assert(votesRes.isOk() && std::stoi(votesRes.getValue(0, 0)) == 1);

        auto receiptRes = conn->executeParams("SELECT COUNT(*) FROM ballot_receipts WHERE election_id = $1::uuid;", {electionUuid});
        assert(receiptRes.isOk() && std::stoi(receiptRes.getValue(0, 0)) == 1);
    }

    std::cout << "  -> PASSED: Exactly 1 vote accepted out of 20 concurrent attempts. Zero double-counting occurred." << std::endl;

    // ----------------------------------------------------------------------
    // TEST 2: Idempotency Replay Test
    // ----------------------------------------------------------------------
    std::cout << "\n[TEST 2] Testing Idempotency Replay with identical idempotency token..." << std::endl;
    {
        // Fetch original receipt
        auto originalReceipt = voteRepo.findReceipt(electionUuid, voterUuid);
        assert(originalReceipt.has_value());

        // Replay submission with same idempotency key
        domain::BallotSubmission replaySub;
        replaySub.electionId = electionUuid;
        replaySub.candidateId = candUuid;
        replaySub.idempotencyKey = originalReceipt->idempotencyKey;

        auto replayedReceipt = votingService.castBallot(voterUuid, replaySub, "127.0.0.1");
        assert(replayedReceipt.receiptHash == originalReceipt->receiptHash);
        assert(replayedReceipt.id == originalReceipt->id);

        // Verify votes count remains 1
        auto conn = pool.acquire();
        auto votesRes = conn->executeParams("SELECT COUNT(*) FROM votes WHERE election_id = $1::uuid;", {electionUuid});
        assert(std::stoi(votesRes.getValue(0, 0)) == 1);
    }
    std::cout << "  -> PASSED: Idempotent replay returned matching receipt without double-counting." << std::endl;

    // ----------------------------------------------------------------------
    // TEST 3: Election Closure Invariant Test
    // ----------------------------------------------------------------------
    std::cout << "\n[TEST 3] Testing Election Closure Invariant..." << std::endl;
    {
        // Close the election
        electionRepo.updateStatus(electionUuid, domain::ElectionStatus::Closed);

        // Create a new voter who hasn't voted yet
        std::string v2Code = "LATE-VOTER-" + security::CryptoUtils::randomHex(4);
        domain::Voter v2("", v2Code, "Late Voter", v2Code + "@example.org", "dummy_hash", true);
        std::string v2Uuid = voterRepo.create(v2);
        electionRepo.assignVoterEligibility(electionUuid, v2Uuid);

        domain::BallotSubmission lateSub;
        lateSub.electionId = electionUuid;
        lateSub.candidateId = candUuid;
        lateSub.idempotencyKey = "late-attempt-01";

        bool rejected = false;
        try {
            votingService.castBallot(v2Uuid, lateSub, "127.0.0.1");
        } catch (const std::exception& e) {
            rejected = true;
        }
        assert(rejected);
    }
    std::cout << "  -> PASSED: Votes submitted after election closure are strictly rejected." << std::endl;

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "  ALL CONCURRENCY, INVARIANT & STRESS TESTS PASSED!       " << std::endl;
    std::cout << "==========================================================" << std::endl;

    return 0;
}
