# Chapter 20, 21 & 22: Architecture Decision Records (ADRs) & The "Why Not?" Analysis

## 1. Summary of Architecture Decision Records (ADRs)

| ADR ID | Decision Title | Status | Rationale Summary |
| :--- | :--- | :---: | :--- |
| **ADR-001** | Modular Monolith in Modern C++20 | **Accepted** | Eliminates distributed transaction latency and complexity; C++20 provides zero-cost abstractions, RAII, and memory safety. |
| **ADR-002** | PostgreSQL Relational Persistence | **Accepted** | ACID guarantees, enforceable `UNIQUE` constraints for double-vote prevention, row-level locks, and append-only triggers. |
| **ADR-003** | Dual-Table Ballot Secrecy Schema | **Accepted** | Physical decoupling of voter participation (`ballot_receipts`) from candidate choice (`votes`) without shared keys. |
| **ADR-004** | React 19 + TypeScript Glassmorphism UI | **Accepted** | Type-safe contract alignment with C++ REST endpoints, client-side UUID idempotency generation, premium user experience. |
| **ADR-005** | Native Header-Only Micro-Frameworks (`cpp-httplib`, `nlohmann::json`)| **Accepted** | Zero external runtime dependencies; simple reproducible cross-platform builds without heavy framework bloat. |

---

## 2. Mandatory Decision Analysis: The "Why Not?" Trade-Off Breakdown

In Staff+ engineering interviews at Tier-1 companies (Amazon, Uber, PayPal, Google), an engineer is judged by their ability to explain **what technologies they rejected and why**.

### 2.1. Why PostgreSQL Instead of MongoDB (NoSQL)?
- **What Problem It Solves**: Enforcing strict double-vote prevention across relational tables.
- **Why It Fits AegisVote**: 
  PostgreSQL provides strict ACID transactions and engine-level `UNIQUE` constraints backed by B-Tree index page locks.
- **Why MongoDB Was Rejected**:
  MongoDB uses document-level atomicity. Enforcing double-vote prevention across two collections (`voters` and `votes`) requires multi-document distributed transactions, which carry significant overhead, document locks, and write conflicts under high concurrency. Furthermore, MongoDB lacks append-only table triggers and declarative row-level shared locks.
- **When MongoDB Would Be Better**:
  MongoDB would be superior for an election system if the ballot schema varied dynamically per voter (e.g. hundreds of complex localized ballot referendums with deeply nested arbitrary JSON structures) and strong relational consistency across entities was unnecessary.

---

### 2.2. Why Not Redis? (When Caching Helps vs Consistency Hazards)
- **Why Redis Was Rejected for Ballot Ingestion**:
  In a voting system, writing a vote to an in-memory Redis cache before PostgreSQL commits creates a **catastrophic consistency hazard**:
  1. If Redis crashes before flushing to disk (`AOF` / `RDB`), votes are permanently lost.
  2. If Redis records a vote, but PostgreSQL transaction rolls back due to a constraint violation, the voter is blocked from voting again while their vote was never counted!
- **When Redis Would Be Justified**:
  Redis is justified exclusively for **high-throughput read-heavy caching** (e.g. caching the static candidate list `/api/candidates` or the certified final tally `/api/elections/{id}/results` after poll closure). For write paths in elections, direct database persistence with Write-Ahead Logging (WAL) is mandatory.

---

### 2.3. Why Not Apache Kafka?
- **Why Kafka Was Rejected**:
  Introducing an asynchronous event queue (Voter -> REST -> Kafka -> Consumer -> Database) breaks the **synchronous ballot receipt contract**. 
  When a citizen votes, they require an immediate cryptographic confirmation receipt that proves their ballot is committed. If a queue sits between the voter and the database:
  - If the database later rejects the ballot (e.g. voter was disqualified), the user has already closed their browser believing they voted!
  - Network retries and consumer replays risk double-counting unless complex deduplication is engineered.
- **When Kafka Would Be Justified**:
  Kafka is justified in national-scale elections (100 million voters) for asynchronous analytical event streaming (e.g. real-time district turnout monitoring and media broadcast streams) completely decoupled from the transactional ballot write path.

---

### 2.4. Why C++20 Instead of Go or Java?
- **Why C++20 Fits**:
  1. **Deterministic Resource Management**: RAII guarantees that connections, memory buffers, and locks are freed immediately at end-of-scope without garbage collection pauses (Stop-The-World GC spikes).
  2. **Zero-Cost Abstractions**: High-throughput cryptographic operations (OpenSSL) and JSON serialization execute at bare-metal speeds.
  3. **Preservation of Repository Heritage**: The original application was written in C++; modernizing it to C++20 demonstrates profound language mastery and software modernization standards.
- **When Go or Java Would Be Better**:
  Go or Java would be superior if the engineering organization had 50+ backend engineers requiring rapid feature development, extensive third-party SDK integrations, and where standard garbage collection latency (1–5ms) was fully acceptable.

---

### 2.5. Why Not Kubernetes?
- **Why Docker Compose Was Chosen Over Kubernetes**:
  AegisVote is designed as a focused modular monolith with a dedicated PostgreSQL database. Running this on Kubernetes introduces massive operational overhead: etcd clusters, ingress controllers, persistent volume claim (PVC) binding delays, Helm charts, and cluster networking complexity with zero quantifiable reliability benefit for a single-node deployment.
- **When Kubernetes Would Be Justified**:
  Kubernetes is justified when deploying across multiple availability zones with automated horizontal pod autoscaling (HPA) for read-heavy frontend/API pods and complex multi-region replication.

---

## 3. Known Limitations & Unverified Assumptions

A Staff Engineer never pretends a project is flawless. The following known limitations are explicitly documented:

1. **Side-Channel Timing Correlation in Ballot Secrecy**:
   While `votes` and `ballot_receipts` are decoupled in the schema, an adversary with direct access to disk-level PostgreSQL Write-Ahead Logs (`WAL`) could correlate timestamps during low-traffic periods. (Mitigation: Production systems require cryptographic mixnets or homomorphic encryption).
2. **Local Machine Single Point of Failure**:
   The current demonstration environment runs on a single host. A hardware failure would take the service offline until restart. (Mitigation: PostgreSQL streaming replication with Patroni / HAProxy).
3. **Absence of Hardware Security Modules (HSM)**:
   The server's HMAC secret keys are loaded via environment variables rather than stored in a dedicated cryptographic hardware security module (e.g. AWS CloudHSM or YubiHSM).

---

## 4. The Two-Minute Staff+ Architectural Pitch

> *"AegisVote is a high-assurance electronic voting platform built on modern C++20 and PostgreSQL. We re-engineered a fragile legacy procedural codebase with 23 buffer overflow vulnerabilities into a thread-safe modular monolith.*
>
> *Our architecture solves the fundamental election paradox: how to guarantee exactly-once participation while preserving secret ballots. We achieved this through a decoupled dual-table schema: voter participation is verified and bound by an atomic unique constraint in `ballot_receipts`, while the voter's candidate selection is recorded anonymously in an append-only `votes` table with zero joinable foreign keys.*
>
> *Under concurrent conditions, double voting is prevented at the database engine level via unique index locks rather than fragile application checks. We verified this with a 20-thread simultaneous race test where exactly one ballot succeeded and 19 were rejected with deterministic 409 Conflict codes.*
>
> *Finally, our REST API implements client-side idempotency keys for reliable mobile retries and wraps everything in a clean React 19 glassmorphism interface."*
