#include "database/VoteRepository.hpp"
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace voting::database {

bool VoteRepository::hasVoted(const std::string& electionId, const std::string& voterId) {
    auto conn = pool.acquire();
    std::string sql = 
        "SELECT 1 FROM ballot_receipts br "
        "JOIN voters v ON br.voter_id = v.id "
        "WHERE br.election_id = $1::uuid AND (CAST(v.id AS TEXT) = $2 OR v.voter_id = $2);";
    
    auto res = conn->executeParams(sql, {electionId, voterId});
    return res.isOk() && res.rowCount() > 0;
}

std::optional<domain::BallotReceipt> VoteRepository::findReceipt(const std::string& electionId, const std::string& voterId) {
    auto conn = pool.acquire();
    std::string sql = 
        "SELECT br.* FROM ballot_receipts br "
        "JOIN voters v ON br.voter_id = v.id "
        "WHERE br.election_id = $1::uuid AND (CAST(v.id AS TEXT) = $2 OR v.voter_id = $2);";
    
    auto res = conn->executeParams(sql, {electionId, voterId});
    if (!res.isOk() || res.rowCount() == 0) {
        return std::nullopt;
    }

    domain::BallotReceipt receipt;
    receipt.id = res.getValue(0, "id");
    receipt.electionId = res.getValue(0, "election_id");
    receipt.voterId = res.getValue(0, "voter_id");
    receipt.idempotencyKey = res.getValue(0, "idempotency_key");
    receipt.receiptHash = res.getValue(0, "receipt_hash");
    receipt.castAt = res.getValue(0, "cast_at");
    return receipt;
}

std::optional<domain::BallotReceipt> VoteRepository::findReceiptByIdempotencyKey(const std::string& voterId, const std::string& idempotencyKey) {
    auto conn = pool.acquire();
    std::string sql = 
        "SELECT br.* FROM ballot_receipts br "
        "JOIN voters v ON br.voter_id = v.id "
        "WHERE (CAST(v.id AS TEXT) = $1 OR v.voter_id = $1) AND br.idempotency_key = $2;";
    
    auto res = conn->executeParams(sql, {voterId, idempotencyKey});
    if (!res.isOk() || res.rowCount() == 0) {
        return std::nullopt;
    }

    domain::BallotReceipt receipt;
    receipt.id = res.getValue(0, "id");
    receipt.electionId = res.getValue(0, "election_id");
    receipt.voterId = res.getValue(0, "voter_id");
    receipt.idempotencyKey = res.getValue(0, "idempotency_key");
    receipt.receiptHash = res.getValue(0, "receipt_hash");
    receipt.castAt = res.getValue(0, "cast_at");
    return receipt;
}

domain::BallotReceipt VoteRepository::recordVoteAtomic(
    const std::string& electionId,
    const std::string& voterId,
    const std::string& candidateId,
    const std::string& idempotencyKey,
    const std::string& receiptHash,
    const std::string& ballotNonce
) {
    auto conn = pool.acquire();

    // Begin ACID transaction
    conn->beginTransaction();

    try {
        // Step 1: Verify election is strictly OPEN with row share lock to avoid race conditions with election closure
        std::string checkSql = "SELECT status FROM elections WHERE id = $1::uuid FOR SHARE;";
        auto checkRes = conn->executeParams(checkSql, {electionId});
        if (!checkRes.isOk() || checkRes.rowCount() == 0) {
            conn->rollbackTransaction();
            throw std::runtime_error("ELECTION_NOT_FOUND");
        }
        std::string status = checkRes.getValue(0, "status");
        if (status != "OPEN") {
            conn->rollbackTransaction();
            throw std::runtime_error("ELECTION_NOT_OPEN");
        }

        // Step 2: Resolve Voter UUID if voter_id code was passed
        std::string voterUuidSql = "SELECT id FROM voters WHERE CAST(id AS TEXT) = $1 OR voter_id = $1;";
        auto voterRes = conn->executeParams(voterUuidSql, {voterId});
        if (!voterRes.isOk() || voterRes.rowCount() == 0) {
            conn->rollbackTransaction();
            throw std::runtime_error("VOTER_NOT_FOUND");
        }
        std::string resolvedVoterUuid = voterRes.getValue(0, "id");

        // Step 3: Insert into ballot_receipts (Enforces double-vote prevention via UNIQUE constraint)
        std::string receiptSql = 
            "INSERT INTO ballot_receipts (election_id, voter_id, idempotency_key, receipt_hash, cast_at) "
            "VALUES ($1::uuid, $2::uuid, $3, $4, CURRENT_TIMESTAMP) "
            "RETURNING id, cast_at;";
        
        auto receiptRes = conn->executeParams(receiptSql, {electionId, resolvedVoterUuid, idempotencyKey, receiptHash});
        if (!receiptRes.isOk()) {
            std::string code = receiptRes.errorCode();
            conn->rollbackTransaction();
            if (code == "23505") { // unique_violation
                throw std::runtime_error("DOUBLE_VOTE_DETECTED");
            }
            throw std::runtime_error("FAILED_TO_INSERT_RECEIPT: " + receiptRes.errorMessage());
        }

        std::string receiptId = receiptRes.getValue(0, "id");
        std::string castAt = receiptRes.getValue(0, "cast_at");

        // Step 4: Insert decoupled anonymous vote into votes table
        // Mitigate temporal side-channel: Truncate timestamp to hour to prevent exact microsecond correlation with ballot_receipts
        std::string voteSql = 
            "INSERT INTO votes (election_id, candidate_id, ballot_nonce, cast_at) "
            "VALUES ($1::uuid, $2::uuid, $3, date_trunc('hour', CURRENT_TIMESTAMP));";
        
        auto voteRes = conn->executeParams(voteSql, {electionId, candidateId, ballotNonce});
        if (!voteRes.isOk()) {
            conn->rollbackTransaction();
            throw std::runtime_error("FAILED_TO_RECORD_VOTE: " + voteRes.errorMessage());
        }

        // Step 5: Commit transaction
        conn->commitTransaction();

        domain::BallotReceipt receipt;
        receipt.id = receiptId;
        receipt.electionId = electionId;
        receipt.voterId = resolvedVoterUuid;
        receipt.idempotencyKey = idempotencyKey;
        receipt.receiptHash = receiptHash;
        receipt.castAt = castAt;
        return receipt;
    } catch (...) {
        conn->rollbackTransaction();
        throw;
    }
}

domain::ElectionResult VoteRepository::getCertifiedResults(const std::string& electionId) {
    auto conn = pool.acquire();

    // 1. Fetch election details
    std::string electionSql = "SELECT title, status FROM elections WHERE id = $1::uuid;";
    auto eRes = conn->executeParams(electionSql, {electionId});
    if (!eRes.isOk() || eRes.rowCount() == 0) {
        throw std::runtime_error("Election not found");
    }

    domain::ElectionResult result;
    result.electionId = electionId;
    result.title = eRes.getValue(0, "title");
    result.status = eRes.getValue(0, "status");

    // 2. Count total eligible voters
    std::string eligibleSql = 
        "SELECT COUNT(*) FROM election_voters WHERE election_id = $1::uuid AND is_eligible = TRUE;";
    auto eligRes = conn->executeParams(eligibleSql, {electionId});
    if (eligRes.isOk() && eligRes.rowCount() > 0) {
        result.totalEligibleVoters = std::stoi(eligRes.getValue(0, 0));
    }

    // 3. Count total votes cast
    std::string votesSql = "SELECT COUNT(*) FROM votes WHERE election_id = $1::uuid;";
    auto vRes = conn->executeParams(votesSql, {electionId});
    if (vRes.isOk() && vRes.rowCount() > 0) {
        result.totalVotes = std::stoi(vRes.getValue(0, 0));
    }

    if (result.totalEligibleVoters > 0) {
        result.turnoutPercentage = (static_cast<double>(result.totalVotes) / result.totalEligibleVoters) * 100.0;
    }

    // 4. Aggregate candidate tallies
    std::string tallySql = 
        "SELECT c.id, c.candidate_code, c.full_name, c.party_name, c.party_symbol, COUNT(v.id) AS vote_count "
        "FROM candidates c "
        "JOIN election_candidates ec ON c.id = ec.candidate_id "
        "LEFT JOIN votes v ON c.id = v.candidate_id AND v.election_id = ec.election_id "
        "WHERE ec.election_id = $1::uuid "
        "GROUP BY c.id, c.candidate_code, c.full_name, c.party_name, c.party_symbol "
        "ORDER BY vote_count DESC, c.candidate_code ASC;";
    
    auto tallyRes = conn->executeParams(tallySql, {electionId});
    if (tallyRes.isOk()) {
        for (int i = 0; i < tallyRes.rowCount(); ++i) {
            domain::CandidateTally t;
            t.candidateId = tallyRes.getValue(i, "id");
            t.candidateCode = tallyRes.getValue(i, "candidate_code");
            t.fullName = tallyRes.getValue(i, "full_name");
            t.partyName = tallyRes.getValue(i, "party_name");
            t.partySymbol = tallyRes.getValue(i, "party_symbol");
            t.voteCount = std::stoi(tallyRes.getValue(i, "vote_count"));

            if (result.totalVotes > 0) {
                t.percentage = (static_cast<double>(t.voteCount) / result.totalVotes) * 100.0;
            } else {
                t.percentage = 0.0;
            }
            result.tallies.push_back(std::move(t));
        }
    }

    result.certifiedAt = "CURRENT_TIMESTAMP";
    return result;
}

} // namespace voting::database
