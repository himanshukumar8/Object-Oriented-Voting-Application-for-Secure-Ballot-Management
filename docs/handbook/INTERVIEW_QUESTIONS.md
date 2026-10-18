# AegisVote Master Technical Interview Guide
## Levels 1 through 5: Tiered Technical Questions, Source References, Failure Analysis & Pitfall Traps

This guide is curated for software engineers interviewing for SDE-1 / SDE-2 roles at Amazon, Uber, PayPal, IBM, Cisco, and Google. It provides battle-tested answers, deep technical explanations, real source references, concrete code examples, follow-up questions, and common candidate traps.

---

# LEVEL 1: FUNDAMENTALS

## Question 1.1: Explain the project in two minutes.
- **Concise Interview Answer**:
  AegisVote is an enterprise electronic voting platform engineered in modern C++20 and PostgreSQL with a React 19 TypeScript frontend. It replaces an insecure, procedural console application with a thread-safe, modular monolith that guarantees exactly-once voting, preserves ballot secrecy, and prevents concurrency race conditions at the database engine level.
- **Detailed Technical Explanation**:
  The system decouples voter participation auditing from secret candidate choices using a dual-table architecture (`ballot_receipts` vs `votes`). Double voting is prevented by an atomic unique constraint on `(election_id, voter_id)`. The API supports client-side idempotency keys for reliable mobile retries, while cryptographic security is ensured via PBKDF2-HMAC-SHA256 password salting, HMAC-SHA256 bearer tokens, and constant-time string comparisons.
- **Source Code References**:
  - `src/main.cpp` (Application bootstrap)
  - `include/domain/Election.hpp` (Domain entity)
  - `database/schema.sql` (PostgreSQL relational schema)
- **Common Candidate Mistakes**:
  - Describing the legacy console script instead of the modernized architecture.
  - Claiming "blockchain" or "microservices" without having actually implemented them.

---

## Question 1.2: Walk through the database schema.
- **Concise Interview Answer**:
  The schema consists of 8 relational tables: `admins`, `voters`, `candidates`, `elections`, two join tables (`election_voters`, `election_candidates`), an append-only `audit_logs` table, and the decoupled voting ledger: `ballot_receipts` and `votes`.
- **Detailed Technical Explanation**:
  - `admins` and `voters`: Store identity, role, and PBKDF2 credential hashes (`<salt>$<hash>`).
  - `election_voters`: Junction table enforcing eligibility (`is_eligible = TRUE`).
  - `election_candidates`: Junction table binding candidates to elections.
  - `ballot_receipts`: Stores `(election_id, voter_id, idempotency_key, receipt_hash)`. Contains a unique constraint on `(election_id, voter_id)`.
  - `votes`: Stores `(election_id, candidate_id, ballot_nonce)`. Has **zero foreign keys** to voters or receipts.
  - `audit_logs`: Append-only security ledger protected by a database trigger preventing `UPDATE` or `DELETE`.
- **Source Code References**:
  - `database/schema.sql:L1-L150`
- **Follow-Up Questions**:
  - *Why are there two separate tables for voting instead of one `votes` table with a `voter_id` column?*
  - **Answer**: Storing `voter_id` in the same row as `candidate_id` violates ballot secrecy, allowing anyone with database read access to view how specific citizens voted.

---

## Question 1.3: Explain the class structure and layering.
- **Concise Interview Answer**:
  The codebase follows clean architecture with clear separation of concerns across 5 layers: Domain Models, Persistence Repositories, Cryptographic Services, Business Services, and REST API Layer.
- **Detailed Technical Explanation**:
  - **Domain**: Pure C++20 value objects (`Election`, `Candidate`, `Voter`, `BallotSubmission`, `BallotReceipt`) with zero database dependencies.
  - **Persistence**: Repositories (`VoterRepository`, `ElectionRepository`, `VoteRepository`) encapsulating parameterized SQL execution.
  - **Database Core**: RAII transaction wrappers (`DbTransaction`) and thread-safe connection pool (`DbPool`).
  - **Services**: `AuthService`, `ElectionService`, `VotingService` enforcing domain invariants and transaction boundaries.
  - **API**: `Server.cpp` mapping HTTP routes, deserializing JSON, and returning RFC 7807 problem details.
- **Source Code References**:
  - `include/domain/`
  - `include/database/`
  - `include/services/`
  - `include/api/Server.hpp`

---

# LEVEL 2: IMPLEMENTATION & TRANSACTIONS

## Question 2.1: Walk through the exact vote submission path.
- **Concise Interview Answer**:
  When a voter submits a vote, the request passes through token authentication, input validation, row-level election locking, idempotency checking, eligibility verification, atomic receipt insertion, decoupled ballot insertion, audit logging, and commits.
- **Detailed Technical Explanation**:
  1. `POST /api/votes` receives JSON payload and `Authorization: Bearer <token>`.
  2. `Server.cpp` validates HMAC-SHA256 signature and checks claims role (`VOTER`).
  3. `VotingService::castBallot` acquires connection from `DbPool` and instantiates `DbTransaction`.
  4. SQL: `SELECT status FROM elections WHERE id = $1 FOR SHARE;` locks the election row against concurrent closure.
  5. SQL: Checks idempotency key in `ballot_receipts`. If found, returns existing receipt.
  6. SQL: Verifies voter eligibility in `election_voters`.
  7. SQL: Verifies candidate is assigned to election in `election_candidates`.
  8. SQL: `INSERT INTO ballot_receipts ...` enforces `UNIQUE(election_id, voter_id)`.
  9. SQL: `INSERT INTO votes ...` inserts candidate choice with a random 256-bit nonce.
  10. SQL: `INSERT INTO audit_logs ...` logs the submission.
  11. `transaction.commit()` persists mutations to WAL and returns SHA-256 HMAC receipt.
- **Source Code References**:
  - `src/services/VotingService.cpp:L20-L80`
  - `src/database/VoteRepository.cpp:L70-L140`

---

## Question 2.2: How is error handling designed?
- **Concise Interview Answer**:
  Error handling follows C++ exception hierarchies translated into RFC 7807 Problem Details HTTP responses, avoiding internal database leaks.
- **Detailed Technical Explanation**:
  - Domain violations (e.g. invalid state transition, duplicate vote) throw `std::logic_error`.
  - Missing records throw `std::runtime_error` ("Not found").
  - The API layer catches `std::logic_error` and inspects messages: double-vote triggers HTTP 409 Conflict; eligibility triggers HTTP 403 Forbidden.
  - Unhandled exceptions trigger RAII transaction rollback and return HTTP 500 without leaking stack traces or SQL strings.
- **Source Code References**:
  - `src/api/Server.cpp:L20-L28` (`sendError` helper)
  - `src/api/Server.cpp:L435-L448` (Exception translation)

---

# LEVEL 3: FAILURE ANALYSIS & CONCURRENCY

## Question 3.1: What happens if two devices submit votes for the same voter simultaneously?
- **Concise Interview Answer**:
  PostgreSQL's unique constraint on `(election_id, voter_id)` guarantees that exactly one transaction succeeds while the other is rejected with a unique constraint violation.
- **Detailed Technical Explanation**:
  Even if Thread A and Thread B reach the database at the identical microsecond:
  - Both open concurrent transactions.
  - Thread A inserts into `ballot_receipts`. PostgreSQL acquires an exclusive index row lock on the unique B-Tree index key `(election_id, voter_id)`.
  - Thread B attempts to insert the identical key. Thread B blocks waiting for Thread A to complete.
  - When Thread A commits, Thread B immediately encounters `ERROR: duplicate key value violates unique constraint "uq_ballot_receipts_election_voter"` (`SQLSTATE 23505`).
  - Thread B's transaction automatically aborts.
  - AegisVote catches `23505` and returns HTTP 409 Conflict.
- **Evidence**:
  - Verified by `test_concurrency.exe` with 20 simultaneous threads: exactly 1 succeeded, 19 rejected.
- **Common Candidate Trap**:
  - Answering "I check if `has_voted` is true first." An application-level check suffers from a classic Time-of-Check to Time-of-Use (TOCTOU) race condition.

---

## Question 3.2: What happens if the database commits, but the client times out?
- **Concise Interview Answer**:
  The client uses an `Idempotency-Key`. Upon reconnecting, the client resends the vote with the same key; the backend detects the key, skips re-inserting the vote, and returns the original receipt.
- **Detailed Technical Explanation**:
  Before making the HTTP request, the client generates a unique UUIDv4 `Idempotency-Key`.
  When the vote transaction commits in PostgreSQL, the key is saved in `ballot_receipts.idempotency_key`.
  If the network drops before the client receives HTTP 201, the client browser retries.
  The server queries `SELECT receipt_hash FROM ballot_receipts WHERE voter_id = $1 AND idempotency_key = $2`.
  Finding the existing committed receipt, the server returns the receipt immediately with HTTP 200 OK without executing `INSERT INTO votes`.
- **Source Code References**:
  - `frontend/src/api.ts:L172` (`crypto.randomUUID()`)
  - `src/database/VoteRepository.cpp:L85-L95` (`findReceiptByIdempotencyKey`)

---

## Question 3.3: What if an administrator closes an election while a vote is being processed?
- **Concise Interview Answer**:
  Row-level locking via `SELECT ... FOR SHARE` ensures that in-flight vote submissions complete before the election status update is permitted to commit.
- **Detailed Technical Explanation**:
  In Step 2 of `castBallot`, the voting transaction acquires a shared row lock:
  ```sql
  SELECT status FROM elections WHERE id = $1 FOR SHARE;
  ```
  When the admin submits `PUT /api/elections/{id}/status` with `{"status":"CLOSED"}`, the update query requires an exclusive row lock (`FOR UPDATE`):
  ```sql
  UPDATE elections SET status = 'CLOSED' WHERE id = $1;
  ```
  PostgreSQL's lock manager puts the admin update query into a queue until all in-flight `FOR SHARE` locks are released by commit or rollback. Once the in-flight votes finish, the election closes, and subsequent votes read `status = 'CLOSED'` and are rejected.

---

# LEVEL 4: ARCHITECTURE & SCALING

## Question 4.1: What is the primary bottleneck at 10,000 concurrent voters?
- **Concise Interview Answer**:
  The primary CPU bottleneck is password hashing (PBKDF2-HMAC-SHA256), and the primary database bottleneck is write contention on the `ballot_receipts` unique index.
- **Detailed Technical Explanation**:
  1. **Password Key Stretching**: At 100,000 iterations of PBKDF2, each login consumes ~100ms of CPU time. 10,000 simultaneous logins will saturate all CPU cores immediately.
     - *Mitigation*: Offload authentication to horizontally scaled stateless auth workers, or issue pre-authenticated voter tokens before election day.
  2. **Database Write IOPS**: Every vote requires an atomic transaction with 3 SQL inserts (`ballot_receipts`, `votes`, `audit_logs`). 10,000 concurrent writes require high IOPS on the PostgreSQL Write-Ahead Log disk.
     - *Mitigation*: Ensure NVMe storage with high write IOPS, tune PostgreSQL `wal_buffers` and `checkpoint_completion_target`, or partition elections across multiple database instances.

---

## Question 4.2: Would adding Redis improve write performance for vote submissions?
- **Concise Interview Answer**:
  No. For vote submissions, Redis introduces consistency hazards and data loss risks. Redis is only appropriate for read caching (e.g. candidate lists and published results).
- **Detailed Technical Explanation**:
  Voting requires strict durability. If votes are accepted into Redis memory and Redis crashes before asynchronous disk persistence (`AOF`), votes are permanently lost. If Redis accepts a vote but the database rejects it due to a constraint violation, the system enters an inconsistent state.
  Direct-to-database persistence with Write-Ahead Logging is the correct engineering architecture for irreversible democratic ballots.

---

## Question 4.3: How would you scale AegisVote to millions of voters?
- **Concise Interview Answer**:
  Scale by sharding elections horizontally across independent PostgreSQL clusters by `election_id`, while placing stateless C++ API instances behind a Layer-7 load balancer.
- **Detailed Technical Explanation**:
  - Elections are completely independent entities. Voter participation in Election A has zero relation to Election B.
  - A routing layer hashes `election_id` to direct requests to dedicated PostgreSQL shards.
  - Read queries (`/api/candidates`, `/api/elections`, and final tallies) can be cached in a distributed Redis cluster or CDN edge cache.
  - Write transactions for an election remain contained on a single database shard, avoiding distributed two-phase commit overhead while scaling throughput linearly.

---

# LEVEL 5: CRITICAL REVIEW & SELF-CRITIQUE

## Question 5.1: If you had another month, what architectural decisions would you improve?
- **Concise Interview Answer**:
  I would replace side-channel-vulnerable timestamp decoupling with a cryptographic mixing network (mixnet) and introduce HSM-backed key management.
- **Detailed Technical Explanation**:
  While the dual-table schema eliminates relational foreign keys between voters and candidates, high-resolution database timestamps create a potential side-channel timing correlation under low traffic volume.
  With more time, I would implement an asynchronous cryptographic mixnet: votes would be encrypted with an election public key and placed into a shuffle queue. When polls close, an offline batch worker shuffles and decrypts the ballots in zero-knowledge batches, breaking all temporal correlation.

---

## Question 5.2: What would change if this were deployed for a real national presidential election?
- **Concise Interview Answer**:
  A national presidential election requires end-to-end verifiability (E2E-V), homomorphic encryption, air-gapped tallying ceremonies, and formal government certification.
- **Detailed Technical Explanation**:
  - **End-to-End Verifiability**: Implement cryptographic protocols (e.g. ElectionGuard or Helios) using homomorphic encryption (Paillier/ElGamal) allowing voters to verify that their ballot was cast-as-intended, recorded-as-cast, and tallied-as-recorded without trusting the server.
  - **Threshold Cryptography**: Split the private tallying key among multiple election trustees across political parties using Shamir's Secret Sharing ($k$-of-$n$), so no single authority or server can decrypt tallies before closure.
  - **Legal & Air-Gapped Infrastructure**: Physical air-gapped tally machines, paper voter-verified audit trails (VVPAT), and compliance with federal voting system standards (VVSG).
