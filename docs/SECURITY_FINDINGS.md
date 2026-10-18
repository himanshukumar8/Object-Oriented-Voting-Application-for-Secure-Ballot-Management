# Independent Security Review & Findings Report

**Document Version:** 1.0.0  
**Audit Track:** Audit D — Security Architecture & Vulnerability Analysis  
**Date:** 2026-10-18  
**Auditor:** AegisVote Independent Release Audit Team  
**Scope:** Source Code, SQL Schemas, Cryptographic Services, Docker Environment, Seed Data, API Endpoints  

> [!CAUTION]
> **Disclaimer:** This document reflects an internal engineering security audit and threat analysis conducted against the AegisVote codebase. It does NOT constitute an external independent certification from a licensed security audit firm (e.g. ISO 27001, SOC 2 Type II, or EAC voting system certification).

---

## 1. Executive Summary & Security Posture

AegisVote replaces a legacy console prototype (`vote.cpp`) that contained 23 buffer overflow vulnerabilities (`gets()`), plaintext credential storage (`admin`/`123`), and no cryptographic security. The modern C++20 backend introduces:
- OpenSSL 3.x PBKDF2-HMAC-SHA256 password hashing (100,000 iterations).
- HMAC-SHA256 signed bearer token authentication with constant-time equality validation.
- 100% prepared SQL queries with parameter binding via `libpq`.
- PostgreSQL row-level locks and unique constraints guaranteeing at-most-once voting.
- An append-only administrative audit log.

During this release audit, **4 confirmed security findings** were identified and evaluated. One critical temporal side-channel vulnerability in the dual-table ballot architecture was **remediated and verified** during the audit.

---

## 2. Confirmed Findings Summary Matrix

| ID | Title | Component | Initial Severity | Remediated During Audit? | Current Status |
|:---|:---|:---|:---:|:---:|:---:|
| **SEC-001** | Microsecond Timestamp Correlation Side-Channel in Dual-Table Architecture | `VoteRepository.cpp` | **HIGH** | **YES** | **RESOLVED** |
| **SEC-002** | Absence of In-Process Rate Limiting Middleware | `Server.cpp` | **MEDIUM** | Architectural Fix Documented | **OPEN (Compensating Control Required)** |
| **SEC-003** | Plaintext HTTP Binding & Absence of Application-Layer TLS Termination | `main.cpp` | **MEDIUM** | Operational Requirement Documented | **OPEN (Reverse Proxy Required)** |
| **SEC-004** | Dual-Part HMAC Bearer Token Architecture vs Full Asymmetric JWT | `TokenService.cpp` | **LOW** | Documented & Verified | **ACCEPTED DESIGN** |

---

## 3. Detailed Security Findings

### SEC-001: Microsecond Timestamp Correlation Side-Channel in Dual-Table Architecture
- **Severity:** **HIGH** (Initial) $\rightarrow$ **RESOLVED** (Post-remediation)
- **CWE:** CWE-200 (Exposure of Sensitive Information to an Unauthorized Actor), CWE-385 (Covert Timing Channel)
- **Component:** `src/database/VoteRepository.cpp` (lines 101, 120), `database/schema.sql` (lines 114, 131)

#### Evidence & Empirical Vulnerability Proof
In the original implementation, `VoteRepository::recordVoteAtomic` performed two table insertions within a single ACID transaction:
1. `INSERT INTO ballot_receipts (..., cast_at) VALUES (..., CURRENT_TIMESTAMP)`
2. `INSERT INTO votes (..., cast_at) VALUES (..., CURRENT_TIMESTAMP)`

In PostgreSQL, `CURRENT_TIMESTAMP` evaluates to `transaction_timestamp()`, which records the exact time the enclosing transaction began. Consequently, both the voter's participation record (`ballot_receipts`) and the anonymous ballot record (`votes`) received **identical microsecond timestamps**.

During the audit, the following SQL query executed against the test database:
```sql
SELECT br.voter_id, v.candidate_id, br.cast_at, v.cast_at 
FROM ballot_receipts br 
JOIN votes v ON br.election_id = v.election_id AND br.cast_at = v.cast_at;
```
**Empirical Result:** 32 out of 32 cast ballots (100%) were immediately and deterministically correlated with microsecond precision:
```
               voter_id               |             candidate_id             |             cast_at              |             cast_at              
--------------------------------------+--------------------------------------+----------------------------------+----------------------------------
 1960a29d-cff7-40f9-b0e4-ca148e351494 | c0000000-0000-0000-0000-000000000001 | 2026-10-18 04:46:25.450864+05:30 | 2026-10-18 04:46:25.450864+05:30
 a56067ac-3a71-415e-83a1-91b92df9b390 | c0000000-0000-0000-0000-000000000001 | 2026-10-18 04:46:25.450839+05:30 | 2026-10-18 04:46:25.450839+05:30
 e44927a4-53ce-4669-8784-bbd8c1857965 | c0000000-0000-0000-0000-000000000001 | 2026-10-18 04:46:25.451398+05:30 | 2026-10-18 04:46:25.451398+05:30
```
Any database administrator, database operator, or attacker with access to database backups could join these two tables and completely nullify ballot secrecy.

#### Remediation Applied
In `src/database/VoteRepository.cpp` (lines 118–122), the `votes` table insertion was updated to truncate the ballot timestamp to hourly granularity:
```cpp
// Mitigate temporal side-channel: Truncate timestamp to hour to prevent exact microsecond correlation with ballot_receipts
std::string voteSql = 
    "INSERT INTO votes (election_id, candidate_id, ballot_nonce, cast_at) "
    "VALUES ($1::uuid, $2::uuid, $3, date_trunc('hour', CURRENT_TIMESTAMP));";
```

#### Re-Verification
After recompilation, server restart, and casting new ballots via `scripts/verify_api_e2e.py`:
- `ballot_receipts.cast_at`: `2026-10-18 04:48:31.657795+05:30` (retained for voter receipt verification).
- `votes.cast_at`: `2026-10-18 04:00:00+05:30` (coarse hourly bucket).
- Correlating join query: Returned `(0 rows)` for all newly submitted ballots.

---

### SEC-002: Absence of In-Process Rate Limiting Middleware
- **Severity:** **MEDIUM**
- **CWE:** CWE-307 (Improper Restriction of Excessive Authentication Attempts), CWE-400 (Uncontrolled Resource Consumption)
- **Component:** `src/api/Server.cpp`

#### Evidence & Analysis
The C++ HTTP engine (`httplib::Server`) does not maintain an in-memory token bucket, sliding window, or IP connection rate limiter. 
- While password hashing correctly uses PBKDF2-HMAC-SHA256 with 100,000 iterations (taking ~60 ms of single-core CPU time per attempt), an unauthenticated attacker could flood `POST /api/auth/voter/login` or `POST /api/auth/admin/login` with 50 concurrent requests, saturating all host CPU cores and causing denial-of-service for legitimate voting traffic.

#### Recommended Remediation & Compensating Control
1. **Compensating Control (Production Requirement):** Enforce strict rate limiting at the reverse proxy (e.g., NGINX `limit_req_zone $binary_remote_addr zone=login:10m rate=5r/s;`).
2. **Codebase Enhancement (Post-Release):** Implement a thread-safe token-bucket filter in `Server::configureCors()` or an explicit middleware hook tracking IP addresses with automatic cool-down lockouts after 5 failed authentication attempts.

---

### SEC-003: Plaintext HTTP Binding & Absence of Application-Layer TLS Termination
- **Severity:** **MEDIUM**
- **CWE:** CWE-319 (Cleartext Transmission of Sensitive Information)
- **Component:** `src/main.cpp` (line 36), `src/api/Server.cpp` (line 503)

#### Evidence & Analysis
`voting_server.exe` initializes a standard plaintext HTTP server on `0.0.0.0:8080`.
- Native OpenSSL HTTPS server sockets (`httplib::SSLServer`) are supported by `httplib.h` but require static local certificate and private key files (`cert.pem`, `key.pem`) which are not bundled into the repository to prevent accidental credential leakage.
- If deployed directly to the internet without an edge proxy, voter credentials (`VoterPass2026!`) and bearer tokens would be transmitted over plaintext HTTP, vulnerable to Wi-Fi sniffing and MITM interception.

#### Recommended Remediation
- The application MUST be deployed behind a TLS-terminating reverse proxy (NGINX, Caddy, AWS ALB, or Cloudflare) configured with TLS 1.3, HSTS (`Strict-Transport-Security`), and valid trusted certificates (Let's Encrypt / DigiCert).

---

### SEC-004: Dual-Part HMAC Bearer Token Architecture vs Full Asymmetric JWT
- **Severity:** **LOW** (Design Note)
- **CWE:** CWE-1188 (Insecure Default Initialization of Resource)
- **Component:** `src/security/TokenService.cpp`

#### Evidence & Analysis
The authentication service issues a compact bearer token formatted as:
$$\text{token} = \text{base64}(\text{JSON payload}) \mathbin{\Vert} \text{"."} \mathbin{\Vert} \text{HMAC-SHA256}(\text{secret}, \text{base64}(\text{JSON payload}))$$
- **Strengths:** 
  - Token signature is verified using OpenSSL constant-time equality (`CryptoUtils::constantTimeEquals`).
  - Token payload contains strict `exp` (1 hour) and `iat` claims validated against system clock.
  - No secret values or sensitive password hashes are stored in the token.
- **Architectural Note:** This is a symmetric HMAC-signed token, not an RFC 7519 3-part JWT (`header.payload.signature`) signed with asymmetric public/private keys (RS256/ES256). For single-service monolithic deployments, symmetric HMAC is secure and fast. In a multi-service or distributed microservice fleet, asymmetric RSA/ECDSA signing with public key JWKS distribution is standard.

---

## 4. Security Verification of System Components

### 4.1 SQL Parameterization (CWE-89 Review)
- All 5 repositories (`VoterRepository`, `ElectionRepository`, `CandidateRepository`, `VoteRepository`, `AuditLogRepository`) were audited line-by-line.
- **Result:** 100% of SQL statements containing user-controlled variables use PostgreSQL parameter placeholders (`$1, $2, ...`) executed via `PQexecParams`. Zero dynamic SQL string concatenations with user input exist in the codebase.

### 4.2 Demo Credentials & Seed Data Integrity
- In `database/seed_demo_data.sql`, all passwords are stored as PBKDF2-HMAC-SHA256 salted hashes. No plaintext passwords exist in the database.
- Default demo credentials (`admin / AdminSecure2026!`, `VOTER-101 / VoterPass2026!`) are synthetic and documented in `README.md` strictly for local evaluation. Production setup documentation specifies updating administrator credentials immediately after migration.

### 4.3 Container Hardening (Docker Review)
- In `Dockerfile`:
  - Uses multi-stage builds (frontend builder, backend builder, minimal runtime).
  - Production runtime uses `debian:bookworm-slim` with only `libpq5`, `libssl3`, and `ca-certificates`.
  - Service runs as an unprivileged, non-root user (`USER aegisvote`, group `aegisvote`).
  - Container includes an automated healthcheck: `HEALTHCHECK --interval=15s CMD curl -f http://localhost:8080/api/health || exit 1`.
