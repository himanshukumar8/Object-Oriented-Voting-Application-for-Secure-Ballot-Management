#include "security/TokenService.hpp"
#include "security/CryptoUtils.hpp"
#include "third_party/json.hpp"
#include <chrono>
#include <stdexcept>
#include <vector>

namespace voting::security {

namespace {

const std::string b64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string base64Encode(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string base64Decode(const std::string& in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[b64_chars[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

} // anonymous namespace

TokenService::TokenService(std::string secretKey)
    : secretKey(std::move(secretKey)) {
    if (this->secretKey.empty()) {
        this->secretKey = "default_secure_voting_secret_key_change_in_production";
    }
}

std::string TokenService::generateToken(const std::string& userId, const std::string& role, int ttlSeconds) const {
    auto now = std::chrono::system_clock::now();
    auto epochSeconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    long long expSeconds = epochSeconds + ttlSeconds;

    nlohmann::json payload = {
        {"sub", userId},
        {"role", role},
        {"iat", epochSeconds},
        {"exp", expSeconds}
    };

    std::string payloadStr = payload.dump();
    std::string encodedPayload = base64Encode(payloadStr);
    std::string signature = CryptoUtils::hmacSha256Hex(secretKey, encodedPayload);

    return encodedPayload + "." + signature;
}

bool TokenService::verifyToken(const std::string& token, TokenClaims& outClaims) const {
    size_t dotPos = token.find('.');
    if (dotPos == std::string::npos) {
        return false;
    }

    std::string encodedPayload = token.substr(0, dotPos);
    std::string providedSignature = token.substr(dotPos + 1);

    std::string expectedSignature = CryptoUtils::hmacSha256Hex(secretKey, encodedPayload);
    if (!CryptoUtils::constantTimeEquals(providedSignature, expectedSignature)) {
        return false;
    }

    try {
        std::string payloadStr = base64Decode(encodedPayload);
        auto j = nlohmann::json::parse(payloadStr);

        outClaims.userId = j["sub"].get<std::string>();
        outClaims.role = j["role"].get<std::string>();
        outClaims.issuedAt = j["iat"].get<long long>();
        outClaims.expiresAt = j["exp"].get<long long>();

        auto now = std::chrono::system_clock::now();
        auto currentSeconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        if (currentSeconds > outClaims.expiresAt) {
            return false; // Token expired
        }

        return true;
    } catch (...) {
        return false;
    }
}

} // namespace voting::security
