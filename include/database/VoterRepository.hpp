#pragma once

#include "domain/Voter.hpp"
#include "database/DbPool.hpp"
#include <optional>
#include <vector>

namespace voting::database {

class VoterRepository {
private:
    DbPool& pool;

public:
    explicit VoterRepository(DbPool& pool) : pool(pool) {}

    std::string create(const domain::Voter& voter);
    std::optional<domain::Voter> findById(const std::string& id);
    std::optional<domain::Voter> findByVoterId(const std::string& voterId);
    std::optional<domain::Voter> findByEmail(const std::string& email);
    std::vector<domain::Voter> findAll();
    bool isEligibleForElection(const std::string& electionId, const std::string& voterId);
};

} // namespace voting::database
