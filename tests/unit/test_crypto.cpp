#include <cassert>
#include <iostream>
#include "security/CryptoUtils.hpp"
#include "security/PasswordHasher.hpp"
#include "security/TokenService.hpp"

using namespace voting::security;

void testSha256Digest() {
    std::cout << "[TEST] Testing SHA-256 Digest against known vector..." << std::endl;

    // SHA-256 of empty string is e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    std::string emptyHash = CryptoUtils::sha256Hex("");
    assert(emptyHash == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // SHA-256 of "hello"
    std::string helloHash = CryptoUtils::sha256Hex("hello");
    assert(helloHash == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");

    std::cout << "  -> PASSED: SHA-256 cryptographic digests match NIST vectors." << std::endl;
}

void testConstantTimeComparison() {
    std::cout << "[TEST] Testing Constant-Time Comparison..." << std::endl;

    assert(CryptoUtils::constantTimeEquals("secret_hash_abc", "secret_hash_abc"));
    assert(!CryptoUtils::constantTimeEquals("secret_hash_abc", "secret_hash_abx"));
    assert(!CryptoUtils::constantTimeEquals("short", "longer_string"));

    std::cout << "  -> PASSED: Constant-time comparison is accurate." << std::endl;
}

void testPasswordHasher() {
    std::cout << "[TEST] Testing PBKDF2 Password Hashing & Verification..." << std::endl;

    std::string password = "SuperSecretPassword2026!";
    std::string storedHash = PasswordHasher::hashPassword(password, 10000); // 10k iterations for unit test speed

    // Hash format check
    assert(storedHash.find('$') != std::string::npos);

    // Verify correct password
    assert(PasswordHasher::verifyPassword(password, storedHash, 10000));

    // Verify incorrect password
    assert(!PasswordHasher::verifyPassword("WrongPassword!", storedHash, 10000));
    assert(!PasswordHasher::verifyPassword("", storedHash, 10000));

    std::cout << "  -> PASSED: PBKDF2 password hashing & verification verified." << std::endl;
}

void testTokenService() {
    std::cout << "[TEST] Testing Bearer Token Signing & Verification..." << std::endl;

    TokenService tokenService("unit_test_secret_key_12345");
    std::string token = tokenService.generateToken("user-42", "ADMIN", 3600);

    TokenClaims claims;
    bool valid = tokenService.verifyToken(token, claims);
    if (!valid) {
        std::cerr << "[ERROR] Generated token verification failed!" << std::endl;
        std::exit(1);
    }
    assert(valid);
    assert(claims.userId == "user-42");
    assert(claims.role == "ADMIN");
    assert(claims.expiresAt > claims.issuedAt);

    // Tampered token test: Modify signature
    std::string tamperedToken = token + "xyz";
    TokenClaims tamperedClaims;
    assert(!tokenService.verifyToken(tamperedToken, tamperedClaims));

    // Expired token test: generate with -10 seconds TTL
    std::string expiredToken = tokenService.generateToken("user-99", "VOTER", -10);
    TokenClaims expiredClaims;
    assert(!tokenService.verifyToken(expiredToken, expiredClaims));

    std::cout << "  -> PASSED: Cryptographic token authentication verified." << std::endl;
}

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << "      RUNNING CRYPTOGRAPHY & SECURITY TESTS      " << std::endl;
    std::cout << "=================================================" << std::endl;

    testSha256Digest();
    testConstantTimeComparison();
    testPasswordHasher();
    testTokenService();

    std::cout << "\nALL CRYPTOGRAPHY TESTS PASSED SUCCESSFULLY." << std::endl;
    return 0;
}
