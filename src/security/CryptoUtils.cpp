#include "security/CryptoUtils.hpp"
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/crypto.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace voting::security {

std::string CryptoUtils::toHex(const unsigned char* data, size_t length) {
    std::ostringstream oss;
    for (size_t i = 0; i < length; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::vector<unsigned char> CryptoUtils::fromHex(const std::string& hex) {
    if (hex.length() % 2 != 0) {
        throw std::invalid_argument("Hex string length must be even");
    }
    std::vector<unsigned char> bytes;
    bytes.reserve(hex.length() / 2);
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        auto byte = static_cast<unsigned char>(std::stoul(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

std::string CryptoUtils::randomHex(size_t byteCount) {
    std::vector<unsigned char> buffer(byteCount);
    if (RAND_bytes(buffer.data(), static_cast<int>(byteCount)) != 1) {
        throw std::runtime_error("OpenSSL RAND_bytes failed to produce cryptographically secure random bytes");
    }
    return toHex(buffer.data(), byteCount);
}

std::string CryptoUtils::sha256Hex(const std::string& input) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("Failed to allocate EVP_MD_CTX");

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, input.data(), input.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, &length) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP SHA256 digest computation failed");
    }
    EVP_MD_CTX_free(ctx);

    return toHex(hash, length);
}

std::string CryptoUtils::hmacSha256Hex(const std::string& key, const std::string& data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;

    EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    if (!mac) throw std::runtime_error("Failed to fetch OpenSSL HMAC");

    EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
    EVP_MAC_free(mac);
    if (!ctx) throw std::runtime_error("Failed to allocate EVP_MAC_CTX");

    OSSL_PARAM params[2];
    char mdname[] = "SHA256";
    params[0] = OSSL_PARAM_construct_utf8_string("digest", mdname, sizeof(mdname));
    params[1] = OSSL_PARAM_construct_end();

    if (EVP_MAC_init(ctx, reinterpret_cast<const unsigned char*>(key.data()), key.size(), params) != 1 ||
        EVP_MAC_update(ctx, reinterpret_cast<const unsigned char*>(data.data()), data.size()) != 1 ||
        EVP_MAC_final(ctx, hash, reinterpret_cast<size_t*>(&length), sizeof(hash)) != 1) {
        EVP_MAC_CTX_free(ctx);
        throw std::runtime_error("EVP HMAC computation failed");
    }
    EVP_MAC_CTX_free(ctx);

    return toHex(hash, length);
}

bool CryptoUtils::constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.length() != b.length()) {
        return false;
    }
    return CRYPTO_memcmp(a.data(), b.data(), a.length()) == 0;
}

} // namespace voting::security
