# AegisVote: Secure Ballot Management Platform
## Enterprise-Grade C++20 Backend | PostgreSQL 18 | React 19 TypeScript Frontend | Staff+ Engineering Standards

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![PostgreSQL](https://img.shields.io/badge/PostgreSQL-18.1-blue.svg)](https://www.postgresql.org/)
[![React](https://img.shields.io/badge/React-19.0-61dafb.svg)](https://react.dev/)
[![TypeScript](https://img.shields.io/badge/TypeScript-5.9-blue.svg)](https://www.typescriptlang.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

---

## 1. Project Overview & Evolution

**AegisVote** is an enterprise-grade secure electronic voting platform. It re-engineers a fragile, legacy procedural C++ console prototype (`vote.cpp`) that contained 23 unbounded `gets()` buffer overflow vulnerabilities, plaintext passwords, and race conditions, transforming it into a hardened, high-assurance distributed platform with a modern browser interface and a multi-threaded C++20 REST API engine.

### Core Architectural Guarantees:
- **Database-Enforced Invariants**: Double voting is prevented at the database engine level via `UNIQUE(election_id, voter_id)` backed by atomic PostgreSQL transactions.
- **Dual-Table Ballot Secrecy**: Physical decoupling of voter participation (`ballot_receipts`) from candidate choice (`votes`) without shared keys or joinable foreign keys.
- **Network Idempotency**: Seamless client retry semantics using UUIDv4 `Idempotency-Key` headers.
- **Cryptographic Rigor**: OpenSSL 3.x PBKDF2-HMAC-SHA256 password salting (100,000 iterations), HMAC-SHA256 bearer tokens, and constant-time string comparisons.
- **Zero Raw Pointers & RAII**: Complete memory and resource safety with `std::unique_ptr`, `std::shared_ptr`, and RAII connection and transaction guards.

---

## 2. System Architecture

```
+-------------------------------------------------------------------------+
|                              AegisVote UI                               |
|                  React 19 + TypeScript (Vite Bundle)                    |
+-------------------------------------------------------------------------+
                                    |
                           HTTPS / REST (JSON)
                                    |
+-------------------------------------------------------------------------+
|                  C++20 Multi-Threaded HTTP Engine                       |
|                          (cpp-httplib)                                  |
+-------------------------------------------------------------------------+
       |                         |                           |
+---------------+       +------------------+       +---------------------+
|  Auth Service |       | Election Service |       |   Voting Service    |
| (PBKDF2/HMAC) |       | (State Machine)  |       | (Atomic Transaction)|
+---------------+       +------------------+       +---------------------+
       |                         |                           |
+-------------------------------------------------------------------------+
|                        Data Persistence Layer                           |
|        VoterRepo, ElectionRepo, CandidateRepo, VoteRepo, AuditRepo      |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                  PostgreSQL 18 Connection Pool (RAII)                   |
|                  libpq C API Wrapper + Smart Handles                    |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                       PostgreSQL Relational DB                          |
|         ACID Transactions | Row Locks | Foreign Keys | Triggers        |
+-------------------------------------------------------------------------+
```

---

## 3. Quick Start & Local Setup

### Prerequisites
- **Compiler**: GCC 14+ or Clang 16+ supporting C++20
- **Build System**: CMake 3.20+ and Ninja
- **Database**: PostgreSQL 16+ (running on `localhost:5432`)
- **Libraries**: OpenSSL 3.x, `libpq` (PostgreSQL client library)
- **Node.js**: Node 20+ (for frontend bundle builds)

### Step 1: Database Initialization
```bash
# Connect to PostgreSQL and create database
psql -U postgres -c "CREATE DATABASE secure_voting_db;"

# Apply relational schema and seed demo records
psql -U postgres -d secure_voting_db -f database/schema.sql
psql -U postgres -d secure_voting_db -f database/seed_demo_data.sql
```

### Step 2: Build C++20 Backend
```bash
# Configure with CMake and Ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Compile all targets (server and automated test suites)
cmake --build build
```

### Step 3: Build Frontend Assets
```bash
cd frontend
npm install
npm run build
cd ..
```

### Step 4: Run the Application
```bash
# Start backend (serves API on :8080 and static frontend on /)
./build/voting_server
```
Open **`http://localhost:8080/`** in your browser.

---

## 4. Default Demo Credentials

| Role | Identifier | Password | Access Rights |
| :--- | :--- | :--- | :--- |
| **Administrator** | `admin` | `AdminSecure2026!` | Full lifecycle governance, audit logs, rosters |
| **Voter 1** | `VOTER-101` | `VoterPass2026!` | Casts ballot in 2026 General Council Election |
| **Voter 2** | `VOTER-102` | `VoterPass2026!` | Casts ballot in 2026 General Council Election |
| **Voter 3** | `VOTER-103` | `VoterPass2026!` | Casts ballot in 2026 General Council Election |
| **Voter 4** | `VOTER-104` | `VoterPass2026!` | Casts ballot in 2026 General Council Election |

*(All demo credentials use safe synthetic profiles and PBKDF2-HMAC-SHA256 password stretching).*

---

## 5. Automated Verification & Test Commands

```bash
# Run all automated tests via CTest
ctest --test-dir build --output-on-failure

# 1. Domain Invariant Unit Tests (State Machine & Logic)
./build/test_invariants

# 2. Cryptographic Security Tests (PBKDF2, HMAC, Constant-Time Comparison)
./build/test_crypto

# 3. Concurrency Stress Test (20 simultaneous threads racing to double-vote)
./build/test_concurrency

# 4. End-to-End API Suite (11 comprehensive integration steps)
python scripts/verify_api_e2e.py

# 5. Full Performance Benchmark Suite (p50, p95, p99 latencies)
python scripts/benchmark_suite.py
```

---

## 6. Docker Deployment

```bash
# Spin up complete production stack (PostgreSQL + C++20 Backend + React UI)
docker compose up -d --build

# Inspect logs
docker compose logs -f backend

# Verify health status
curl -i http://localhost:8080/api/health
```

---

## 7. API Specification

The formal OpenAPI 3.0 specification for the REST API is version-controlled in [`docs/openapi.yaml`](docs/openapi.yaml).

---

## 8. Historical Baseline Documentation

The original legacy prototype code has been preserved in its original location (`vote.cpp`) for academic comparison and vulnerability auditing against this re-engineered C++20 platform.
