#pragma once

#include "domain/Candidate.hpp"
#include "database/DbPool.hpp"
#include <optional>
#include <vector>

namespace voting::database {

class CandidateRepository {
private:
    DbPool& pool;

public:
    explicit CandidateRepository(DbPool& pool) : pool(pool) {}

    std::string create(const domain::Candidate& candidate);
    std::optional<domain::Candidate> findById(const std::string& id);
    std::optional<domain::Candidate> findByCode(const std::string& code);
    std::vector<domain::Candidate> findAll();
    std::vector<domain::Candidate> findForElection(const std::string& electionId);
};

} // namespace voting::database
