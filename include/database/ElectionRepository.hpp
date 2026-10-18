#pragma once

#include "domain/Election.hpp"
#include "database/DbPool.hpp"
#include <optional>
#include <vector>

namespace voting::database {

class ElectionRepository {
private:
    DbPool& pool;

public:
    explicit ElectionRepository(DbPool& pool) : pool(pool) {}

    std::string create(const domain::Election& election);
    std::optional<domain::Election> findById(const std::string& id, const std::string& voterId = "");
    std::vector<domain::Election> findAll(const std::string& voterId = "");
    void updateStatus(const std::string& id, domain::ElectionStatus newStatus);
    void assignCandidate(const std::string& electionId, const std::string& candidateId);
    void assignVoterEligibility(const std::string& electionId, const std::string& voterId);
};

} // namespace voting::database
