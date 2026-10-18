#include "security/PasswordHasher.hpp"
#include "security/CryptoUtils.hpp"
#include <openssl/evp.h>
#include <stdexcept>
#include <vector>

namespace voting::security {

std::string PasswordHasher::hashPassword(const std::string& password, int iterations) {
    std::string saltHex = CryptoUtils::randomHex(SALT_BYTE_LENGTH);
    std::vector<unsigned char> saltBytes = CryptoUtils::fromHex(saltHex);
    std::vector<unsigned char> hashBytes(HASH_BYTE_LENGTH);

    int result = PKCS5_PBKDF2_HMAC(
        password.data(), static_cast<int>(password.length()),
        saltBytes.data(), static_cast<int>(saltBytes.size()),
        iterations,
        EVP_sha256(),
        static_cast<int>(hashBytes.size()), hashBytes.data()
    );

    if (result != 1) {
        throw std::runtime_error("PKCS5_PBKDF2_HMAC password hashing computation failed");
    }

    std::string hashHex = CryptoUtils::toHex(hashBytes.data(), hashBytes.size());
    return saltHex + "$" + hashHex;
}

bool PasswordHasher::verifyPassword(const std::string& candidatePassword, const std::string& storedHash, int iterations) {
    size_t delimiterPos = storedHash.find('$');
    if (delimiterPos == std::string::npos) {
        return false;
    }

    std::string saltHex = storedHash.substr(0, delimiterPos);
    std::string expectedHashHex = storedHash.substr(delimiterPos + 1);

    std::vector<unsigned char> saltBytes;
    try {
        saltBytes = CryptoUtils::fromHex(saltHex);
    } catch (...) {
        return false;
    }

    std::vector<unsigned char> candidateHashBytes(HASH_BYTE_LENGTH);
    int result = PKCS5_PBKDF2_HMAC(
        candidatePassword.data(), static_cast<int>(candidatePassword.length()),
        saltBytes.data(), static_cast<int>(saltBytes.size()),
        iterations,
        EVP_sha256(),
        static_cast<int>(candidateHashBytes.size()), candidateHashBytes.data()
    );

    if (result != 1) {
        return false;
    }

    std::string candidateHashHex = CryptoUtils::toHex(candidateHashBytes.data(), candidateHashBytes.size());
    return CryptoUtils::constantTimeEquals(candidateHashHex, expectedHashHex);
}

} // namespace voting::security
