#include <cassert>
#include <iostream>
#include "domain/ElectionStatus.hpp"
#include "domain/Election.hpp"
#include "domain/Candidate.hpp"
#include "domain/Voter.hpp"
#include "domain/Ballot.hpp"

using namespace voting::domain;

void testElectionStateTransitions() {
    std::cout << "[TEST] Testing Election State Transitions..." << std::endl;

    // Allowed linear transitions
    assert(canTransition(ElectionStatus::Draft, ElectionStatus::Scheduled));
    assert(canTransition(ElectionStatus::Scheduled, ElectionStatus::Open));
    assert(canTransition(ElectionStatus::Open, ElectionStatus::Closed));
    assert(canTransition(ElectionStatus::Closed, ElectionStatus::ResultsPublished));

    // Idempotent self-transitions
    assert(canTransition(ElectionStatus::Draft, ElectionStatus::Draft));
    assert(canTransition(ElectionStatus::Open, ElectionStatus::Open));

    // Strictly forbidden transitions (skipping or reversing states)
    assert(!canTransition(ElectionStatus::Draft, ElectionStatus::Open));             // Cannot skip SCHEDULED
    assert(!canTransition(ElectionStatus::Draft, ElectionStatus::Closed));           // Cannot skip OPEN
    assert(!canTransition(ElectionStatus::Scheduled, ElectionStatus::Closed));       // Cannot close without opening
    assert(!canTransition(ElectionStatus::Open, ElectionStatus::Draft));            // Cannot revert OPEN to DRAFT
    assert(!canTransition(ElectionStatus::Closed, ElectionStatus::Open));           // Cannot reopen CLOSED poll
    assert(!canTransition(ElectionStatus::ResultsPublished, ElectionStatus::Open)); // Cannot reopen published poll
    assert(!canTransition(ElectionStatus::ResultsPublished, ElectionStatus::Draft));

    std::cout << "  -> PASSED: All transition invariants enforced." << std::endl;
}

void testVotingAllowedRule() {
    std::cout << "[TEST] Testing Voting Allowed Invariant..." << std::endl;

    Election eDraft("1", "Draft Election", "", ElectionStatus::Draft, "2026-01-01", "2026-01-02");
    Election eSched("2", "Sched Election", "", ElectionStatus::Scheduled, "2026-01-01", "2026-01-02");
    Election eOpen("3", "Open Election", "", ElectionStatus::Open, "2026-01-01", "2026-01-02");
    Election eClosed("4", "Closed Election", "", ElectionStatus::Closed, "2026-01-01", "2026-01-02");
    Election ePub("5", "Pub Election", "", ElectionStatus::ResultsPublished, "2026-01-01", "2026-01-02");

    assert(!eDraft.isVotingAllowed());
    assert(!eSched.isVotingAllowed());
    assert(eOpen.isVotingAllowed());
    assert(!eClosed.isVotingAllowed());
    assert(!ePub.isVotingAllowed());

    std::cout << "  -> PASSED: Only OPEN elections permit ballot submissions." << std::endl;
}

void testVoterClientSanitization() {
    std::cout << "[TEST] Testing Voter Client Sanitization..." << std::endl;

    Voter v("v-1", "VOTER-99", "Jane Doe", "jane@example.org", "secret_hash_value_123", true, "2026-10-18");
    nlohmann::json clientJson = v.toClientJson();

    assert(clientJson["voter_id"] == "VOTER-99");
    assert(clientJson["full_name"] == "Jane Doe");
    assert(!clientJson.contains("password_hash")); // Invariant: Password hash must never leak in client serialization

    std::cout << "  -> PASSED: Voter confidential fields cleanly omitted." << std::endl;
}

void testCandidateSerialization() {
    std::cout << "[TEST] Testing Candidate Model Serialization..." << std::endl;

    Candidate c("c-1", "CAND-01", "Candidate One", "Party A", "Tree", "Bio info");
    nlohmann::json j = c.toJson();

    Candidate reconstructed = Candidate::fromJson(j);
    assert(reconstructed.id == "c-1");
    assert(reconstructed.candidateCode == "CAND-01");
    assert(reconstructed.partySymbol == "Tree");

    std::cout << "  -> PASSED: Candidate JSON serialization is symmetric." << std::endl;
}

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << "   RUNNING DOMAIN & INVARIANT UNIT TESTS         " << std::endl;
    std::cout << "=================================================" << std::endl;

    testElectionStateTransitions();
    testVotingAllowedRule();
    testVoterClientSanitization();
    testCandidateSerialization();

    std::cout << "\nALL DOMAIN INVARIANT TESTS PASSED SUCCESSFULLY." << std::endl;
    return 0;
}
