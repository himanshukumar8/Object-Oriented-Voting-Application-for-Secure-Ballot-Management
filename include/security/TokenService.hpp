#pragma once

#include <string>

namespace voting::security {

struct TokenClaims {
    std::string userId;
    std::string role; // "ADMIN" or "VOTER"
    long long issuedAt{0};
    long long expiresAt{0};
};

class TokenService {
private:
    std::string secretKey;

public:
    explicit TokenService(std::string secretKey);

    std::string generateToken(const std::string& userId, const std::string& role, int ttlSeconds = 86400) const;

    bool verifyToken(const std::string& token, TokenClaims& outClaims) const;
};

} // namespace voting::security
