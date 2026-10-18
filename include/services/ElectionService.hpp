#pragma once

#include "database/ElectionRepository.hpp"
#include "database/CandidateRepository.hpp"
#include "database/AuditLogRepository.hpp"
#include "domain/Election.hpp"
#include <optional>
#include <vector>

namespace voting::services {

class ElectionService {
private:
    database::ElectionRepository& electionRepo;
    database::CandidateRepository& candidateRepo;
    database::AuditLogRepository& auditRepo;

public:
    ElectionService(database::ElectionRepository& electionRepo,
                   database::CandidateRepository& candidateRepo,
                   database::AuditLogRepository& auditRepo)
        : electionRepo(electionRepo), candidateRepo(candidateRepo), auditRepo(auditRepo) {}

    std::vector<domain::Election> listElections(const std::string& voterId = "");
    std::optional<domain::Election> getElection(const std::string& id, const std::string& voterId = "");
    std::string createElection(const domain::Election& election, const std::string& adminId);
    void transitionStatus(const std::string& electionId, domain::ElectionStatus newStatus, const std::string& adminId);
    void assignCandidate(const std::string& electionId, const std::string& candidateId, const std::string& adminId);
};

} // namespace voting::services
