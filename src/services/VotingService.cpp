#include "services/VotingService.hpp"
#include "security/CryptoUtils.hpp"
#include <stdexcept>
#include <iostream>

namespace voting::services {

domain::BallotReceipt VotingService::castBallot(
    const std::string& voterId,
    const domain::BallotSubmission& submission,
    const std::string& ipAddress
) {
    if (submission.electionId.empty() || submission.candidateId.empty()) {
        throw std::invalid_argument("Election ID and Candidate ID are required");
    }

    std::string idempotencyKey = submission.idempotencyKey;
    if (idempotencyKey.empty()) {
        idempotencyKey = security::CryptoUtils::randomHex(16);
    }

    // Step 1: Idempotency Check — Return existing receipt if this exact request was previously processed
    auto existingReceipt = voteRepo.findReceiptByIdempotencyKey(voterId, idempotencyKey);
    if (existingReceipt) {
        auditRepo.log("BALLOT_IDEMPOTENT_REPLAY", "VOTER", voterId, ipAddress, {
            {"election_id", submission.electionId},
            {"idempotency_key", idempotencyKey}
        });
        return *existingReceipt;
    }

    // Step 2: Invariant Check — Has the voter already voted in this election?
    if (voteRepo.hasVoted(submission.electionId, voterId)) {
        auditRepo.log("BALLOT_REJECTED_ALREADY_VOTED", "VOTER", voterId, ipAddress, {
            {"election_id", submission.electionId}
        });
        throw std::logic_error("Voter has already cast a ballot in this election");
    }

    // Step 3: Invariant Check — Is the voter verified and eligible for this specific election?
    if (!voterRepo.isEligibleForElection(submission.electionId, voterId)) {
        auditRepo.log("BALLOT_REJECTED_NOT_ELIGIBLE", "VOTER", voterId, ipAddress, {
            {"election_id", submission.electionId}
        });
        throw std::logic_error("Voter is not registered or eligible for this election");
    }

    // Step 4: Cryptographic Preparation
    // Generate deterministic signed receipt hash: HMAC-SHA256(voterId:electionId:idempotencyKey)
    std::string receiptPayload = voterId + ":" + submission.electionId + ":" + idempotencyKey;
    std::string receiptHash = security::CryptoUtils::hmacSha256Hex(receiptSecret, receiptPayload);

    // Generate decoupled anonymous ballot nonce
    std::string ballotNonce = security::CryptoUtils::randomHex(16);

    // Step 5: Execute Atomic Database Transaction
    // Enforces single-ballot invariant via DB-level UNIQUE constraint and inserts decoupled vote
    domain::BallotReceipt receipt = voteRepo.recordVoteAtomic(
        submission.electionId,
        voterId,
        submission.candidateId,
        idempotencyKey,
        receiptHash,
        ballotNonce
    );

    auditRepo.log("BALLOT_CAST_SUCCESS", "VOTER", voterId, ipAddress, {
        {"election_id", submission.electionId},
        {"receipt_hash", receiptHash}
    });

    return receipt;
}

domain::ElectionResult VotingService::getElectionResults(
    const std::string& electionId,
    const std::string& userRole
) {
    auto results = voteRepo.getCertifiedResults(electionId);

    // Invariant: Uncertified live tallies are confidential until official RESULTS_PUBLISHED phase,
    // unless requested by an authenticated Administrator.
    if (results.status != "RESULTS_PUBLISHED" && userRole != "ADMIN") {
        throw std::logic_error("Election results are not yet published for public viewing");
    }

    return results;
}

} // namespace voting::services
