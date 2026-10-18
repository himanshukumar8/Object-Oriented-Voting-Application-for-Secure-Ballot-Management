# Product Requirements Document (PRD): Secure Ballot Management Platform

**Document Version:** 1.0.0  
**Status:** Approved  
**Author:** Staff Software & Security Engineering  
**Scope:** Functional & Non-Functional Requirements, Election Lifecycle, and Formal Invariants  

---

## 1. Product Mission & Goals

The Secure Ballot Management Platform is a modern, tamper-resilient electronic voting system designed to manage election lifecycles, candidate slates, voter rosters, ballot submissions, and certified result publication. 

The primary goals are:
1. **Vote Integrity & Double-Vote Prevention:** Guarantee mathematically and transactionally that an eligible voter casts at most one ballot per election.
2. **Ballot Secrecy:** Enforce separation of voter identities from cast candidate selections to safeguard voter privacy while maintaining unalterable tally counts.
3. **Auditable Lifecycle Management:** Enforce deterministic election state transitions with strict role-based access control (RBAC).
4. **Resilience & Fault Tolerance:** Provide atomic, idempotent transactions resisting network dropouts, concurrent race conditions, and system crashes.
5. **Modern, Accessible User Experience:** Offer an intuitive, accessible browser interface for voters and administrators backed by a high-performance C++ backend.

---

## 2. User Roles & Capabilities

### 2.1 System Administrator
Administrators are authenticated institutional operators entrusted with configuring elections, managing rosters, and certifying results.

**Core Capabilities:**
- **Secure Authentication:** Multi-factor capable authentication with cryptographic password hashing (bcrypt/Argon2id), session tokens, and automatic idle expiration.
- **Voter Registry Management:**
  - Register voters with verified identifiers, full names, email addresses, and cryptographic credentials.
  - Query, filter, and view voter rosters.
  - Deactivate or verify voter eligibility status.
- **Candidate Slate Management:**
  - Register candidates with identifiers, full names, party affiliations, biographical descriptions, and party symbols/logos.
  - Update candidate metadata prior to election commencement.
- **Election Lifecycle Management:**
  - Create elections with titles, descriptions, scheduled start and end timestamps.
  - Assign approved candidates to specific elections.
  - Transition elections through the formal lifecycle state machine.
  - Close active elections on demand or per scheduled deadline.
- **Result Publication & Audit Reporting:**
  - Tally votes upon election closure.
  - Publish certified results making them visible to voters and the public.
  - Review immutable administrative audit logs (e.g., election status transitions, candidate registrations).

### 2.2 Registered Voter
Voters are authenticated citizens or organization members authorized to cast confidential ballots in elections for which they are designated eligible.

**Core Capabilities:**
- **Secure Voter Authentication:** Log in using their registered voter identifier and confidential password/token.
- **Election Discovery & Eligibility Verification:**
  - View all currently OPEN elections for which they are eligible.
  - View upcoming SCHEDULED elections.
  - View published results of closed elections.
  - View explicit status indicating whether they have already voted in each election.
- **Candidate Exploration:** Inspect candidate profiles, party affiliations, and statements for a selected election.
- **Ballot Submission:**
  - Select an approved candidate.
  - Review and confirm candidate selection on a dedicated pre-submission confirmation screen.
  - Submit ballot securely with an idempotency token.
  - Receive an immutable confirmation receipt with timestamp and ballot verification code.
- **Duplicate Vote Prevention:** Clear and immediate feedback if attempting to vote again in an election where a ballot has already been recorded.

---

## 3. Formal Election Lifecycle State Machine

An election progresses through a deterministic series of state transitions. Out-of-order transitions are strictly rejected by the backend domain model and database constraints.

```
       [ DRAFT ]
           │
           │ (Admin publishes schedule & finalizes candidates)
           ▼
    [ SCHEDULED ]
           │
           │ (Start time reached OR Admin opens poll)
           ▼
        [ OPEN ] ◄──────── (Voting Permitted)
           │
           │ (End time reached OR Admin closes poll)
           ▼
       [ CLOSED ] ◄────── (Voting Blocked; Tallying)
           │
           │ (Admin certifies and publishes tallies)
           ▼
 [ RESULTS_PUBLISHED ]
```

### 3.1 State Definitions & Permitted Operations

| State | Permitted Operations | Forbidden Operations | Permitted Actors | Transition Trigger |
|:---|:---|:---|:---|:---|
| **DRAFT** | Edit title/description; add/remove candidate assignments; edit schedule. | Cast votes; publish results; view ballot tallies. | Admin | Initial state on creation. |
| **SCHEDULED** | View election details & candidate slate; edit schedule. | Cast votes; add/remove candidates; view tallies. | Admin, Voters | Admin transitions DRAFT → SCHEDULED. |
| **OPEN** | Cast votes; record participation tokens; view election info. | Add/remove candidates; modify schedule; view unfinalized tallies. | Voters (cast votes), Admin (emergency close) | Start timestamp reached OR Admin opens election. |
| **CLOSED** | Compute vote totals; verify ballot hash integrity. | Cast votes; reopen voting; alter accepted ballots. | System, Admin | End timestamp reached OR Admin closes election. |
| **RESULTS_PUBLISHED** | Publicly view candidate vote counts, percentages, turnout stats. | Cast votes; modify candidate counts; alter roster. | All (Voters, Admin, Public) | Admin certifies & publishes results. |

---

## 4. Fundamental Invariants & Correctness Guarantees

The platform enforces seven non-negotiable invariants at both the application domain layer and the PostgreSQL database layer.

### Invariant 1: Voter Eligibility Verification
- **Statement:** An individual can only cast a ballot in an election if their voter record exists, is active, and is marked eligible for that specific election.
- **Enforcement:** Relational foreign key checks and query filter `WHERE voters.is_active = TRUE AND election_voters.is_eligible = TRUE`.
- **Underlying Assumption:** Administrative registry maintains authorized voter lists prior to election opening.

### Invariant 2: At-Most-Once Vote per Voter per Election
- **Statement:** A voter can cast at most one accepted vote per election. Under no circumstances—including concurrent browser tabs, rapid retries, or distributed replay attacks—can two ballots be accepted from the same voter.
- **Enforcement:** 
  1. A `ballot_receipts` table with a database-level composite unique constraint: `UNIQUE (election_id, voter_id)`.
  2. Transactional check within `SERIALIZABLE` or `READ COMMITTED` with row-level pessimistic locking (`SELECT ... FOR UPDATE`).
- **Underlying Assumption:** The database relational engine reliably enforces unique constraints across concurrent transactions.

### Invariant 3: Strict Temporal Window Enforcement
- **Statement:** Ballots are accepted if and only if the election status is strictly `OPEN` and current system time satisfies `start_time <= CURRENT_TIMESTAMP < end_time`.
- **Enforcement:** Backend domain validation and database check constraint / transaction assertion before ballot insertion.
- **Underlying Assumption:** Server clock synchronization via NTP is maintained.

### Invariant 4: Candidate-Election Slate Integrity
- **Statement:** A vote can only be recorded for a candidate who is officially assigned to the election's ballot slate.
- **Enforcement:** Composite foreign key: `FOREIGN KEY (election_id, candidate_id) REFERENCES election_candidates(election_id, candidate_id)`.
- **Underlying Assumption:** Candidate slates are frozen when the election transitions from `DRAFT` to `SCHEDULED`.

### Invariant 5: Idempotent Request Handling
- **Statement:** If a voter submits a vote, and the network drops before the HTTP response is received, resending the identical submission with the client-generated `idempotency_key` returns the original success receipt without double-counting the vote.
- **Enforcement:** Unique constraint on `(voter_id, idempotency_key)` storing the generated receipt response.
- **Underlying Assumption:** Modern client browsers generate UUIDv4 idempotency keys before issuing ballot POST requests.

### Invariant 6: Ballot Secrecy & Decoupling
- **Statement:** The storage of voter participation must be decoupled from the candidate choice such that an operator with direct read access to the database cannot correlate which individual voted for which candidate.
- **Enforcement:** 
  - `ballot_receipts` records `(id, election_id, voter_id, timestamp, receipt_hash)`.
  - `votes` records `(id, election_id, candidate_id, cast_at, ballot_nonce)`.
  - The tables share no shared foreign key linking a `voter_id` to a specific `votes.id`.
- **Underlying Assumption:** Database insertions into `votes` and `ballot_receipts` occur within the same atomic transaction without correlated sequential row IDs.

### Invariant 7: Results Consistency
- **Statement:** The published tally for any candidate must equal the exact count of unweighted ballots recorded in the `votes` table for that candidate and election.
- **Enforcement:** Certified tallies are derived directly via `SELECT candidate_id, COUNT(*) FROM votes WHERE election_id = $1 GROUP BY candidate_id`.
- **Underlying Assumption:** The `votes` table is append-only; update and delete operations are rejected by database triggers.

---

## 5. Non-Functional Requirements (NFRs)

### 5.1 Performance & Scalability
- **Sub-100ms Read Latency:** Election listings, candidate slates, and published results load in < 50ms (p95) under 500 concurrent connections.
- **Sub-200ms Vote Processing:** End-to-end vote transaction commits and returns a signed receipt in < 150ms (p95).
- **Throughput:** Capable of sustaining ≥ 500 write transactions/sec on single-node modern hardware.

### 5.2 Reliability & Fault Tolerance
- **ACID Persistence:** Zero data loss upon abrupt process termination or server restart.
- **Graceful Error Handling:** All API errors return structured JSON with RFC 7807 problem details; no stack traces or SQL internals leaked.

### 5.3 Security & Compliance
- **Zero Plaintext Passwords:** Argon2id/bcrypt with memory cost ≥ 64MB or salt rounds ≥ 12.
- **Parameterized SQL:** 100% of queries use prepared statements; zero dynamic SQL string concatenations.
- **CORS & CSRF:** Strict CORS headers permitting only configured origins; SameSite=Strict cookies / Bearer auth tokens.
- **Input Sanitization:** All payload inputs validated with boundary rules (lengths, alphanumeric formats).
