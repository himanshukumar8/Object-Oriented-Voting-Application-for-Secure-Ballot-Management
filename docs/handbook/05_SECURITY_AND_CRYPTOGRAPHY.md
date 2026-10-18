# Chapter 13: Security Architecture, STRIDE Threat Model & Cryptographic Implementation

## 1. Threat Modeling: STRIDE & DREAD Analysis

Prior to declaring AegisVote secure, the engineering team executed a formal STRIDE threat model to identify attack surfaces and design defensive mitigations.

| Threat Category | Threat Description | Attack Vector | AegisVote Defense & Mitigation |
| :--- | :--- | :--- | :--- |
| **Spoofing** | Attacker impersonates eligible voter to cast unauthorized ballot. | Credential stuffing, session hijacking. | PBKDF2-HMAC-SHA256 password stretching; HMAC-SHA256 signed bearer tokens with 24h expiration. |
| **Tampering** | Rogue DBA or insider modifies vote tally in database. | Direct SQL update (`UPDATE votes SET candidate_id = ...`). | Append-only database trigger `trg_prevent_vote_tamper` blocking `UPDATE` and `DELETE`; HMAC receipts. |
| **Repudiation** | Voter claims their vote was not recorded or miscounted. | Dispute over unrecorded ballots. | Immutable, digitally signed Ballot Receipt hash issued upon commit; verifiable audit logs. |
| **Information Disclosure** | Administrator views voter political preferences. | Inspecting database rows, SQL joins. | Dual-table ballot secrecy decoupling voter participation from candidate choice. |
| **Denial of Service** | Script floods ballot endpoint to exhaust DB connections. | Concurrent HTTP flood. | Fixed-capacity connection pool (8 conn max), rate limiting, bounded worker threads. |
| **Elevation of Privilege**| Voter accesses administrative election creation endpoints. | Forging admin role in JWT/token payload. | Cryptographic HMAC signature verification; claims validation rejecting non-admin tokens with HTTP 403. |

---

## 2. Cryptographic Implementations in C++20

AegisVote avoids custom ("roll-your-own") cryptographic algorithms. All primitives utilize OpenSSL 3.x with FIPS-compliant standards.

### 2.1. Password Hashing: PBKDF2-HMAC-SHA256 Key Stretching

To protect stored voter and administrator credentials against GPU/ASIC brute-force dictionary attacks:
- **Algorithm**: `PKCS5_PBKDF2_HMAC`
- **Digest**: SHA-256 (32-byte digest)
- **Salt**: 16 bytes (128 bits) of cryptographically secure pseudo-random entropy generated via `RAND_bytes()`
- **Iteration Count**: 100,000 iterations (standard NIST SP 800-132 recommendation)
- **Format**: Stored as `<salt_hex>$<hash_hex>`

```cpp
// src/security/PasswordHasher.cpp
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
        throw std::runtime_error("Password hashing failed");
    }

    return saltHex + "$" + CryptoUtils::toHex(hashBytes.data(), hashBytes.size());
}
```

---

### 2.2. Timing Attack Prevention: Constant-Time Comparison
A common security vulnerability in web services is string comparison via `==` or `strcmp()`. Standard string comparison algorithms exit on the first mismatched byte, creating measurable timing differences (timing side-channel) that allow an attacker to guess tokens and hashes byte by byte.

AegisVote eliminates timing leaks using OpenSSL's `CRYPTO_memcmp()`:

```cpp
// src/security/CryptoUtils.cpp
bool CryptoUtils::constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.length() != b.length()) {
        return false;
    }
    return CRYPTO_memcmp(a.data(), b.data(), a.length()) == 0;
}
```

---

### 2.3. Cryptographic Ballot Receipt Generation
When a ballot transaction commits, AegisVote generates a unique proof of submission for the voter:

$$\text{Receipt Hash} = \text{HMAC-SHA256}\big(K_{\text{receipt}}, \text{election\_id} \parallel \text{voter\_id} \parallel \text{nonce} \parallel \text{timestamp}\big)$$

```cpp
// src/services/VotingService.cpp
std::string receiptData = submission.electionId + ":" + voter.id + ":" + ballotNonce + ":" + std::to_string(nowSec);
std::string receiptHash = security::CryptoUtils::hmacSha256(receiptSecret, receiptData);
```

**Security Property**: 
1. The voter can prove they voted by presenting their receipt hash.
2. The receipt hash cannot be forged without knowing the server's private `receiptSecret`.
3. The receipt hash does **not** disclose which candidate the voter chose, preventing voter coercion or vote buying!

---

### 2.4. Bearer Token Service (HMAC-SHA256 Token Claims)
Instead of stateful database sessions requiring table lookups on every HTTP request, AegisVote issues compact, signed bearer tokens:
- **Header & Payload**: Encodes user identity, voter code, role (`ADMIN` or `VOTER`), and UNIX expiration timestamp.
- **Signature**: `HMAC-SHA256(SecretKey, Payload)`
- **Format**: `base64(Payload).base64(Signature)`
- **Tamper Resistance**: If an attacker modifies the payload (e.g. changing `"role":"VOTER"` to `"role":"ADMIN"`), the computed HMAC signature fails to match, and the request is immediately rejected with HTTP 401 Unauthorized.
