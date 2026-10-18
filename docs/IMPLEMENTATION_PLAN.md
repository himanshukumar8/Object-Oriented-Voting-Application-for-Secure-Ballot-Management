# Implementation Plan: Secure Ballot Management Platform

**Document Version:** 1.0.0  
**Status:** Approved  
**Author:** Staff Software Engineer & Technical Lead  
**Target Completion:** Comprehensive Production & Interview Ready Delivery  

---

## 1. Plan Overview & Engineering Milestones

This document specifies the phased technical execution plan to transform the legacy C++ console voting application into a high-performance, secure, tested, and demonstrable voting product with an accompanying comprehensive Engineering Handbook.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ Phase 0: Baseline Discovery & Repository Audit                    [COMPLETED]│
├─────────────────────────────────────────────────────────────────────────────┤
│ Phase 1: Requirements & Architecture Formulation                  [COMPLETED]│
├─────────────────────────────────────────────────────────────────────────────┤
│ Phase 2: Database Schema & Relational Migrations                  [COMPLETED]│
├─────────────────────────────────────────────────────────────────────────────┤
│ Phase 3: Modern C++20 Core, RAII Domain Models & CMake Build      [COMPLETED]│
├─────────────────────────────────────────────────────────────────────────────┤
│ Phase 4: Persistence Layer, Connection Pool & Vote Transactions   [COMPLETED]│
├─────────────────────────────────────────────────────────────────────────────┤
│ Phase 5: REST API Engine, Auth Middleware & Endpoints             [COMPLETED]│
├─────────────────────────────────────────────────────────────────────────────┤
│ Phase 6: React + TypeScript + Vite Web Application                [COMPLETED]│
├─────────────────────────────────────────────────────────────────────────────┤
│ Phase 7: Automated Test Suite (Unit, Integration, Concurrency)    [COMPLETED]│
├─────────────────────────────────────────────────────────────────────────────┤
│ Phase 8: Benchmarking & Dockerization (Container runtime BLOCKED) [COMPLETED]│
├─────────────────────────────────────────────────────────────────────────────┤
│ Phase 9: Comprehensive Engineering Handbook (8 monographs)        [COMPLETED]│
├─────────────────────────────────────────────────────────────────────────────┤
│ Phase 10: Technical Interview Mastery Guide & Defense Scenarios   [COMPLETED]│
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Phase-by-Phase Task Breakdown

### Phase 2: Database Schema & Relational Migrations
- **Objective:** Establish clean relational schema in PostgreSQL 18 with enforced constraints.
- **Tasks:**
  1. Author `database/schema.sql` defining:
     - `admins` (UUID, username, password_hash, full_name, created_at)
     - `voters` (UUID, voter_id, full_name, email, password_hash, is_active, created_at)
     - `candidates` (UUID, candidate_code, full_name, party_name, party_symbol, biography, created_at)
     - `elections` (UUID, title, description, status, start_time, end_time, created_at)
     - `election_candidates` (election_id, candidate_id, assigned_at)
     - `election_voters` (election_id, voter_id, is_eligible)
     - `ballot_receipts` (id, election_id, voter_id, idempotency_key, receipt_hash, cast_at) with `UNIQUE(election_id, voter_id)`
     - `votes` (id, election_id, candidate_id, ballot_nonce, cast_at)
     - `audit_logs` (id, action, actor_type, actor_id, ip_address, metadata, created_at)
  2. Author `database/seed_demo_data.sql` with safe synthetic test records.
  3. Verify schema execution against `secure_voting_db`.

### Phase 3: Modern C++20 Core & CMake Build System
- **Objective:** Build reproducible C++20 project structure replacing legacy memory-unsafe patterns.
- **Tasks:**
  1. Author root `CMakeLists.txt` configuring C++20 standard, compiler warning flags (`-Wall -Wextra -Wpedantic`), OpenSSL, libpq linking, and build targets.
  2. Implement Domain Entities in `src/domain/`:
     - `Election.hpp/.cpp`: State machine invariants (`DRAFT`, `SCHEDULED`, `OPEN`, `CLOSED`, `RESULTS_PUBLISHED`).
     - `Candidate.hpp/.cpp`: Immutability, validation, symbol metadata.
     - `Voter.hpp/.cpp`: Profile, status, eligibility checks.
     - `Ballot.hpp/.cpp`: Receipt tokens, verification hashes, nonces.
  3. Implement Cryptographic Services in `src/security/`:
     - `PasswordHasher`: OpenSSL PBKDF2-HMAC-SHA256 with 16-byte random salt.
     - `TokenService`: HMAC-SHA256 bearer tokens with 1-hour expiration.
     - `CryptoUtils`: Secure random nonces and constant-time string comparisons.

### Phase 4: Persistence Layer & Vote Transaction Engine
- **Objective:** Thread-safe database access with ACID transaction guarantees.
- **Tasks:**
  1. Implement `DatabasePool` in `src/database/`:
     - Thread-safe connection pool with RAII connection handles.
     - Parameterized query execution with `PQexecParams`.
  2. Implement Repositories in `src/database/`:
     - `VoterRepository`, `CandidateRepository`, `ElectionRepository`.
  3. Implement `VotingService` in `src/services/`:
     - Single ACID transaction performing double-vote checks, `ballot_receipts` insertion, and decoupled `votes` insertion.
     - Automatic rollback on unique constraint violation (PostgreSQL 23505).
     - Idempotency handling: return existing receipt on matching `(voter_id, idempotency_key)`.

### Phase 5: REST API Engine & HTTP Transport
- **Objective:** Provide a fast, multi-threaded REST API.
- **Tasks:**
  1. Implement HTTP server in `src/api/` using `cpp-httplib` + `nlohmann/json`.
  2. Implement Middleware:
     - CORS handler with configurable allowed origins.
     - Token authentication & RBAC middleware.
     - Structured JSON request logging.
     - Global exception handler returning RFC 7807 problem details.
  3. Register Endpoints:
     - `/api/health`
     - `/api/auth/admin/login`, `/api/auth/voter/login`
     - `/api/elections` (GET list, POST create, GET :id, POST :id/status)
     - `/api/candidates` (GET list, POST create)
     - `/api/elections/:id/candidates` (GET, POST assign)
     - `/api/votes` (POST cast ballot with idempotency)
     - `/api/elections/:id/results` (GET certified tallies)
     - `/api/admin/audit-logs` (GET logs)
  4. Generate `docs/openapi.yaml` OpenAPI 3.0 specification.

### Phase 6: Modern React + TypeScript Web Frontend
- **Objective:** Professional, accessible, dynamic browser interface.
- **Tasks:**
  1. Initialize Vite project with React 19 + TypeScript.
  2. Create curated Vanilla CSS design system (`frontend/src/index.css`) with:
     - Dark-mode palette with electric sapphire, emerald green, and coral crimson accent tokens.
     - Glassmorphism card surfaces with subtle backdrop blurs and borders.
     - Accessible typography using Outfit & Inter.
  3. Build components:
     - Voter Portal: Login, active elections, candidate cards, selection modal, review confirmation, digital signed receipt.
     - Admin Dashboard: Auth, metrics overview, election state transition controls, candidate enrollment form, voter registry, certified results view.
  4. Connect 100% of UI actions to real backend endpoints.

### Phase 7: Automated Test Suite & Validation
- **Objective:** Verify 100% of invariants under adversarial conditions.
- **Tasks:**
  1. Unit tests: State transitions, domain validation, crypto verification.
  2. Database Integration tests: Foreign keys, rollback assertions.
  3. Concurrency Stress tests: 20 simultaneous threads submitting identical voter ballots to verify exactly one accepted vote.
  4. End-to-End tests: Full workflow from admin setup to voter receipt to result tally.

### Phase 8: Benchmarking & Dockerization
- **Objective:** Measure real throughput and package for reproducible deployment.
- **Tasks:**
  1. Run load testing measuring p50, p95, p99 latencies and write throughput.
  2. Create production `Dockerfile` (multi-stage build) and `docker-compose.yml` linking C++ backend, PostgreSQL, and frontend.

### Phase 9 & 10: Engineering Handbook & Interview Mastery
- **Objective:** Deliver comprehensive ~70-100 page equivalent guide and technical Q&A in `docs/handbook/`.
