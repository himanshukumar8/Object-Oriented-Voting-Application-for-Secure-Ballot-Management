#pragma once

#include "domain/Ballot.hpp"
#include "database/DbPool.hpp"
#include <optional>
#include <string>

namespace voting::database {

class VoteRepository {
private:
    DbPool& pool;

public:
    explicit VoteRepository(DbPool& pool) : pool(pool) {}

    bool hasVoted(const std::string& electionId, const std::string& voterId);
    std::optional<domain::BallotReceipt> findReceipt(const std::string& electionId, const std::string& voterId);
    std::optional<domain::BallotReceipt> findReceiptByIdempotencyKey(const std::string& voterId, const std::string& idempotencyKey);

    domain::BallotReceipt recordVoteAtomic(
        const std::string& electionId,
        const std::string& voterId,
        const std::string& candidateId,
        const std::string& idempotencyKey,
        const std::string& receiptHash,
        const std::string& ballotNonce
    );

    domain::ElectionResult getCertifiedResults(const std::string& electionId);
};

} // namespace voting::database
