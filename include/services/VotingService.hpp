#pragma once

#include "database/VoteRepository.hpp"
#include "database/VoterRepository.hpp"
#include "database/ElectionRepository.hpp"
#include "database/AuditLogRepository.hpp"
#include "domain/Ballot.hpp"
#include <string>

namespace voting::services {

class VotingService {
private:
    database::VoteRepository& voteRepo;
    database::VoterRepository& voterRepo;
    database::ElectionRepository& electionRepo;
    database::AuditLogRepository& auditRepo;
    std::string receiptSecret;

public:
    VotingService(database::VoteRepository& voteRepo,
                  database::VoterRepository& voterRepo,
                  database::ElectionRepository& electionRepo,
                  database::AuditLogRepository& auditRepo,
                  std::string receiptSecret = "secret_receipt_salt_2026")
        : voteRepo(voteRepo), voterRepo(voterRepo), electionRepo(electionRepo),
          auditRepo(auditRepo), receiptSecret(std::move(receiptSecret)) {}

    domain::BallotReceipt castBallot(
        const std::string& voterId,
        const domain::BallotSubmission& submission,
        const std::string& ipAddress = ""
    );

    domain::ElectionResult getElectionResults(
        const std::string& electionId,
        const std::string& userRole
    );
};

} // namespace voting::services
