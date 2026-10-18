#pragma once

#include <string>

namespace voting::security {

class PasswordHasher {
public:
    static constexpr int DEFAULT_ITERATIONS = 100000;
    static constexpr size_t SALT_BYTE_LENGTH = 16;
    static constexpr size_t HASH_BYTE_LENGTH = 32;

    /**
     * Hashes a plaintext password using PBKDF2-HMAC-SHA256 with a cryptographically secure random salt.
     * Returns: "<salt_hex>$<hash_hex>"
     */
    static std::string hashPassword(const std::string& password, int iterations = DEFAULT_ITERATIONS);

    /**
     * Verifies a candidate password against an existing "<salt_hex>$<hash_hex>" stored string.
     * Compares in constant time to resist side-channel timing attacks.
     */
    static bool verifyPassword(const std::string& candidatePassword, const std::string& storedHash, int iterations = DEFAULT_ITERATIONS);
};

} // namespace voting::security
