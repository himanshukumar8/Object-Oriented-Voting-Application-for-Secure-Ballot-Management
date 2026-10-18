#include "services/AuthService.hpp"
#include <iostream>

namespace voting::services {

AuthResult AuthService::adminLogin(const std::string& username, const std::string& password, const std::string& ipAddress) {
    AuthResult result;

    try {
        auto conn = pool.acquire();
        std::string sql = "SELECT id, username, password_hash, full_name FROM admins WHERE username = $1;";
        auto res = conn->executeParams(sql, {username});

        if (!res.isOk() || res.rowCount() == 0) {
            result.errorMessage = "Invalid admin credentials";
            auditRepo.log("ADMIN_LOGIN_FAILED", "ADMIN", username, ipAddress, {{"reason", "user_not_found"}});
            return result;
        }

        std::string adminId = res.getValue(0, "id");
        std::string storedHash = res.getValue(0, "password_hash");
        std::string fullName = res.getValue(0, "full_name");

        if (!security::PasswordHasher::verifyPassword(password, storedHash)) {
            result.errorMessage = "Invalid admin credentials";
            auditRepo.log("ADMIN_LOGIN_FAILED", "ADMIN", username, ipAddress, {{"reason", "invalid_password"}});
            return result;
        }

        std::string token = tokenService.generateToken(adminId, "ADMIN");
        result.success = true;
        result.token = token;
        result.role = "ADMIN";
        result.user = {
            {"id", adminId},
            {"username", username},
            {"full_name", fullName},
            {"role", "ADMIN"}
        };

        auditRepo.log("ADMIN_LOGIN_SUCCESS", "ADMIN", adminId, ipAddress);
        return result;
    } catch (const std::exception& e) {
        result.errorMessage = "Internal authentication error";
        return result;
    }
}

AuthResult AuthService::voterLogin(const std::string& voterId, const std::string& password, const std::string& ipAddress) {
    AuthResult result;

    try {
        auto voterOpt = voterRepo.findByVoterId(voterId);
        if (!voterOpt) {
            result.errorMessage = "Invalid voter credentials";
            auditRepo.log("VOTER_LOGIN_FAILED", "VOTER", voterId, ipAddress, {{"reason", "voter_not_found"}});
            return result;
        }

        const auto& voter = *voterOpt;
        if (!voter.isActive) {
            result.errorMessage = "Voter account is inactive";
            auditRepo.log("VOTER_LOGIN_BLOCKED", "VOTER", voter.id, ipAddress, {{"reason", "account_inactive"}});
            return result;
        }

        if (!security::PasswordHasher::verifyPassword(password, voter.passwordHash)) {
            result.errorMessage = "Invalid voter credentials";
            auditRepo.log("VOTER_LOGIN_FAILED", "VOTER", voter.id, ipAddress, {{"reason", "invalid_password"}});
            return result;
        }

        std::string token = tokenService.generateToken(voter.id, "VOTER");
        result.success = true;
        result.token = token;
        result.role = "VOTER";
        result.user = {
            {"id", voter.id},
            {"voter_id", voter.voterId},
            {"full_name", voter.fullName},
            {"email", voter.email},
            {"role", "VOTER"}
        };

        auditRepo.log("VOTER_LOGIN_SUCCESS", "VOTER", voter.id, ipAddress);
        return result;
    } catch (const std::exception& e) {
        result.errorMessage = "Internal authentication error";
        return result;
    }
}

} // namespace voting::services
