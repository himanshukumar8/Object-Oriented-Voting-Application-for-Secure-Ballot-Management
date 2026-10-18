#pragma once

#include "database/DbPool.hpp"
#include "security/TokenService.hpp"
#include "security/PasswordHasher.hpp"
#include "database/VoterRepository.hpp"
#include "database/AuditLogRepository.hpp"
#include "third_party/json.hpp"

namespace voting::services {

struct AuthResult {
    bool success{false};
    std::string token;
    std::string role;
    nlohmann::json user;
    std::string errorMessage;
};

class AuthService {
private:
    database::DbPool& pool;
    security::TokenService& tokenService;
    database::VoterRepository& voterRepo;
    database::AuditLogRepository& auditRepo;

public:
    AuthService(database::DbPool& pool,
                security::TokenService& tokenService,
                database::VoterRepository& voterRepo,
                database::AuditLogRepository& auditRepo)
        : pool(pool), tokenService(tokenService), voterRepo(voterRepo), auditRepo(auditRepo) {}

    AuthResult adminLogin(const std::string& username, const std::string& password, const std::string& ipAddress = "");
    AuthResult voterLogin(const std::string& voterId, const std::string& password, const std::string& ipAddress = "");
};

} // namespace voting::services
