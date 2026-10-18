#include "services/ElectionService.hpp"
#include <stdexcept>

namespace voting::services {

std::vector<domain::Election> ElectionService::listElections(const std::string& voterId) {
    return electionRepo.findAll(voterId);
}

std::optional<domain::Election> ElectionService::getElection(const std::string& id, const std::string& voterId) {
    return electionRepo.findById(id, voterId);
}

std::string ElectionService::createElection(const domain::Election& election, const std::string& adminId) {
    if (election.title.empty()) {
        throw std::invalid_argument("Election title cannot be empty");
    }

    std::string id = electionRepo.create(election);
    auditRepo.log("ELECTION_CREATED", "ADMIN", adminId, "", {
        {"election_id", id},
        {"title", election.title},
        {"status", domain::toString(election.status)}
    });
    return id;
}

void ElectionService::transitionStatus(const std::string& electionId, domain::ElectionStatus newStatus, const std::string& adminId) {
    auto electionOpt = electionRepo.findById(electionId);
    if (!electionOpt) {
        throw std::runtime_error("Election not found");
    }

    const auto& currentElection = *electionOpt;
    if (!currentElection.canTransitionTo(newStatus)) {
        throw std::logic_error(
            "Forbidden state transition from " + domain::toString(currentElection.status) +
            " to " + domain::toString(newStatus)
        );
    }

    electionRepo.updateStatus(electionId, newStatus);
    auditRepo.log("ELECTION_STATUS_CHANGED", "ADMIN", adminId, "", {
        {"election_id", electionId},
        {"from_status", domain::toString(currentElection.status)},
        {"to_status", domain::toString(newStatus)}
    });
}

void ElectionService::assignCandidate(const std::string& electionId, const std::string& candidateId, const std::string& adminId) {
    auto electionOpt = electionRepo.findById(electionId);
    if (!electionOpt) {
        throw std::runtime_error("Election not found");
    }

    // Invariant: Candidates can only be assigned during DRAFT or SCHEDULED phase
    if (electionOpt->status != domain::ElectionStatus::Draft &&
        electionOpt->status != domain::ElectionStatus::Scheduled) {
        throw std::logic_error("Candidates cannot be assigned once an election is OPEN or CLOSED");
    }

    auto candidateOpt = candidateRepo.findById(candidateId);
    if (!candidateOpt) {
        throw std::runtime_error("Candidate not found");
    }

    electionRepo.assignCandidate(electionId, candidateId);
    auditRepo.log("CANDIDATE_ASSIGNED_TO_ELECTION", "ADMIN", adminId, "", {
        {"election_id", electionId},
        {"candidate_id", candidateId}
    });
}

} // namespace voting::services
