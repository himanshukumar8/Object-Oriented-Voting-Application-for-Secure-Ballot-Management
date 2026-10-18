# Security Threat Model & Risk Assessment: Secure Ballot Management Platform

**Document Version:** 1.0.0  
**Status:** Approved  
**Author:** Staff Application Security Engineer  
**Methodology:** STRIDE Classification & DREAD Risk Scoring  

---

## 1. System Overview & Trust Boundaries

The Secure Ballot Management Platform processes sensitive democratic workflows where trust, privacy, and tamper-resistance are foundational.

```
       [ Public Internet / Untrusted Network ]
                         │
                         ▼
   ┌───────────────────────────────────────────────┐
   │ TRUST BOUNDARY 1: Browser Client              │
   │  - Voter Device & Administrator Browser       │
   │  - Untrusted Environment (XSS, Extensions)    │
   └───────────────────────┬───────────────────────┘
                           │ HTTPS / TLS 1.3 (Restricted CORS)
                           ▼
   ┌───────────────────────────────────────────────┐
   │ TRUST BOUNDARY 2: Reverse Proxy & API Gateway │
   │  - Rate Limiting, SSL Termination, Headers    │
   └───────────────────────┬───────────────────────┘
                           │ Internal HTTP / Localhost Loopback
                           ▼
   ┌───────────────────────────────────────────────┐
   │ TRUST BOUNDARY 3: C++20 Backend Application   │
   │  - Authentication & RBAC Middleware           │
   │  - Business Logic & Input Validation          │
   │  - Cryptographic Hashing & Token Verification │
   └───────────────────────┬───────────────────────┘
                           │ libpq Parameterized Wire Protocol
                           ▼
   ┌───────────────────────────────────────────────┐
   │ TRUST BOUNDARY 4: PostgreSQL Database Engine  │
   │  - Tables: voters, elections, candidates      │
   │  - Decoupled: ballot_receipts vs votes        │
   │  - Append-Only Audit Logs                     │
   └───────────────────────────────────────────────┘
```

---

## 2. Identified Threat Actors & Attack Motives

1. **Adversarial Voter:** Attempts to vote multiple times, vote in elections they are not eligible for, or replay captured tokens.
2. **External Network Attacker:** Attempts Denial of Service (DoS), brute-force credential stuffing, SQL injection, buffer overflows, or cross-site request forgery (CSRF).
3. **Malicious / Compromised Administrator:** Attempts to unilaterally manipulate ballot tallies, inject unapproved candidates after schedule lock, or alter closed election outcomes.
4. **Curious / Malicious Database Operator:** Has direct read access to the database tables and seeks to discover how specific voters voted (violating ballot secrecy).

---

## 3. STRIDE Threat Analysis & Engineered Mitigations

### 3.1 Spoofing (Identity Deception)
| Threat ID | Threat Description | Vulnerability in Baseline | Engineered Mitigation | Residual Risk |
|:---|:---|:---|:---|:---|
| **STR-S1** | Attacker impersonates administrator using default credentials. | `AdminID="admin"`, `AdminPass="123"` hardcoded in plain text (`vote.cpp:49`). | Replaced with dynamic database user store; passwords hashed using OpenSSL PBKDF2-HMAC-SHA256 (100,000 iterations); constant-time comparison. | Weak admin password chosen by operator. Enforced with minimum password complexity policy. |
| **STR-S2** | Attacker impersonates voter using publicly available date of birth (DOB). | Plaintext DOB stored in CSV and used directly as password (`vote.cpp:690`). | Decoupled confidential voter credentials from public demographics; voter registration issues cryptographically random initial secrets. | Physical theft of voter device/credentials. |
| **STR-S3** | Session hijacking via stolen or forged auth token. | None existed; sessions were unmanaged local console state. | Cryptographically signed tokens (HMAC-SHA256) with short expiration window (1 hour), bound to user ID, role, and client signature. | XSS on client browser capturing in-memory token. Addressed via CSP. |

### 3.2 Tampering (Data Modification)
| Threat ID | Threat Description | Vulnerability in Baseline | Engineered Mitigation | Residual Risk |
|:---|:---|:---|:---|:---|
| **STR-T1** | SQL Injection to alter vote counts or bypass authentication. | Naive string formatting with CSV columns (`vote.cpp:358`). | 100% prepared SQL statements with parameterized inputs via `libpq`. Dynamic SQL string concatenation strictly prohibited. | None; prepared queries completely eliminate SQL injection syntax modification. |
| **STR-T2** | Memory corruption / arbitrary code execution via buffer overflow. | 23 instances of `gets()` writing unbounded input to small fixed arrays (`char CandidateID[5]`). | Completely eliminated `gets()`, raw pointers, and C fixed-size buffers. Replaced with `std::string`, `std::vector`, and RAII memory safety in modern C++20. | None in application domain code. |
| **STR-T3** | Modification of candidate tallies or election outcomes by admin. | In-memory sort and write to CSV (`vote.cpp:1050–1062`). | Tallies are not stored as mutable counters. Results are dynamically computed directly from the append-only `votes` table. Triggers block `UPDATE` or `DELETE` on `votes`. | Direct database manipulation by PostgreSQL superuser with trigger disable rights. Mitigated by append-only audit trail. |

### 3.3 Repudiation (Denial of Actions)
| Threat ID | Threat Description | Vulnerability in Baseline | Engineered Mitigation | Residual Risk |
|:---|:---|:---|:---|:---|
| **STR-R1** | Voter denies having cast a ballot, or claims their vote was lost. | No cryptographic receipt or confirmation code provided. | Upon successful submission, voter receives a signed SHA-256 `receipt_hash` derived from `(election_id, voter_id, timestamp, server_salt)`. Voter can independently verify their ballot was recorded. | Voter loses their digital receipt. |
| **STR-R2** | Administrator denies opening or closing an election prematurely. | Zero audit logging existed in baseline. | Append-only `audit_logs` table tracking every state transition, actor ID, IP address, and timestamp. | Log truncation by root database operator. |

### 3.4 Information Disclosure (Privacy & Secrecy Violations)
| Threat ID | Threat Description | Vulnerability in Baseline | Engineered Mitigation | Residual Risk |
|:---|:---|:---|:---|:---|
| **STR-I1** | Linkage of voter identity to cast candidate choice (Ballot Secrecy Breach). | Console printed voter name and chosen candidate together (`vote.cpp:726–733`). | Dual-table architecture: `ballot_receipts` records that voter $V$ voted in election $E$. `votes` records candidate $C$ received a vote in election $E$ with a random nonce. No relational link exists between $V$ and $C$. | Traffic timing side-channel on very low-traffic elections (mitigated by bulk processing / delayed publication). |
| **STR-I2** | Information leakage via verbose database error messages. | Raw exceptions and console dumps. | RFC 7807 problem details error format; generic messages to clients ("Invalid request", "Resource conflict") with details only in private server logs. | None. |
| **STR-I3** | Leaking uncertified intermediate tallies during voting. | Poll results accessible while poll is running if file inspected. | Results endpoint strictly enforces election status == `RESULTS_PUBLISHED`. Tallies cannot be read while status is `OPEN` or `CLOSED`. | Insider reading database directly. |

### 3.5 Denial of Service (System Availability)
| Threat ID | Threat Description | Vulnerability in Baseline | Engineered Mitigation | Residual Risk |
|:---|:---|:---|:---|:---|
| **STR-D1** | Stack exhaustion / crash via unbounded recursive function calls. | `TakePoll` and `AdminPage` called recursively on every user interaction (`vote.cpp:332, 817`). | Replaced recursive control flow with iterative event loops and non-blocking asynchronous request dispatch. | None. |
| **STR-D2** | Endpoint flooding / vote submission DoS. | No request throttling or concurrency protection. | In-memory token-bucket rate limiter per IP/client token; database connection pool with bounded queueing. | Distributed denial of service (DDoS) exceeding link capacity (requires upstream cloud CDN/WAF). |
| **STR-D3** | Database starvation via unindexed heavy joins. | Full file scans across CSV files (`vote.cpp:156–164`). | Relational B-tree indexes on `(election_id, voter_id)`, `(election_id, candidate_id)`, and foreign keys. | Highly concurrent read storms. Mitigated by query caching. |

### 3.6 Elevation of Privilege (Unauthorized Access)
| Threat ID | Threat Description | Vulnerability in Baseline | Engineered Mitigation | Residual Risk |
|:---|:---|:---|:---|:---|
| **STR-E1** | Voter accesses administrative endpoints (e.g. creating candidates). | No separation between client and server; single console menu with weak gating. | Role-Based Access Control (RBAC) middleware verifying `role == "ADMIN"` on all `/api/admin/*` and administrative election endpoints. | Stolen admin token before expiration. |
| **STR-E2** | Voter casts ballot in election where they are not eligible. | In baseline, any registered voter in `Voter.csv` could vote in any poll (`vote.cpp:690`). | Mandatory eligibility roster check: `election_voters.is_eligible = TRUE` before ballot acceptance. | Administrative misconfiguration of voter eligibility rosters. |

---

## 4. Cryptographic Specifications & Key Management

1. **Password Hashing:**
   - Algorithm: PBKDF2-HMAC-SHA256
   - Iteration Count: 100,000 (standard NIST SP 800-132 recommendation)
   - Salt Length: 16 bytes (128 bits) cryptographically random per user via OpenSSL `RAND_bytes`.
2. **Session / Auth Tokens:**
   - Format: HMAC-SHA256 signed bearer token
   - Signature Secret: 256-bit cryptographically secure key loaded via environment variable (`JWT_SECRET`).
   - Expiration: Strict 3600-second TTL with explicit `exp` claim validation.
3. **Ballot Receipt Hashing:**
   - Format: SHA-256 hex digest: `HMAC-SHA256(voter_id + ":" + election_id + ":" + timestamp, server_secret)`.
   - Allows voters to prove their vote was registered without revealing candidate choice.

---

## 5. Synthetic Demo Credentials Policy

To allow public demonstration and assessment without exposing real personal identifiable information (PII):
- Default synthetic administrator:
  - Username: `admin`
  - Password: `AdminSecure2026!`
- Default synthetic test voters:
  - `voter_001` / `VoterPass2026!`
  - `voter_002` / `VoterPass2026!`
  - `voter_003` / `VoterPass2026!`
- Demo database reset endpoint/script provided (`scripts/seed_demo_data.sql` and `scripts/reset_demo.ps1`).
