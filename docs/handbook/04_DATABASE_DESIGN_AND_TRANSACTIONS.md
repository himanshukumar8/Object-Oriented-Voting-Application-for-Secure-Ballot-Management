# Chapter 8, 9, 10, 11 & 12: Relational Schema, Dual-Table Ballot Secrecy, Transaction Boundaries & Atomic Duplicate-Vote Prevention

## 1. Relational Data Model & PostgreSQL 18 Schema

AegisVote relies on PostgreSQL 18 as the single authoritative source of persistence truth.

```
+--------------------+        +---------------------+        +--------------------+
|       admins       |        |      elections      |        |     candidates     |
+--------------------+        +---------------------+        +--------------------+
| id (PK, UUID)      |        | id (PK, UUID)       |        | id (PK, UUID)      |
| username (UNIQUE)  |        | title               |        | candidate_code(UQ) |
| password_hash      |        | status              |        | full_name          |
| role               |        | start_time          |        | party_name         |
+--------------------+        | end_time            |        +--------------------+
                              +---------------------+                  |
                                         |                             |
                       +-----------------+-----------------+           |
                       |                                   |           |
                       v                                   v           v
+-----------------------------+                 +-----------------------------+
|       election_voters       |                 |     election_candidates     |
+-----------------------------+                 +-----------------------------+
| election_id (FK)            |                 | election_id (FK)            |
| voter_id (FK)               |                 | candidate_id (FK)           |
| is_eligible (BOOL)          |                 | PRIMARY KEY (election,cand) |
| PRIMARY KEY (election,voter)|                 +-----------------------------+
+-----------------------------+
               |
               v
+-----------------------------+
|           voters            |
+-----------------------------+
| id (PK, UUID)               |
| voter_id (UNIQUE, VARCHAR)  |
| full_name                   |
| password_hash               |
| is_active (BOOL)            |
+-----------------------------+
```

### The Decoupled Voting Ledger:

```
                  +-----------------------------------+
                  |             ELECTION              |
                  +-----------------------------------+
                                    |
          +-------------------------+-------------------------+
          |                                                   |
          v                                                   v
+-----------------------------+                 +-----------------------------+
|       ballot_receipts       |                 |            votes            |
|     (VOTER AUDIT TRAIL)     |                 |       (SECRET TALLY)        |
+-----------------------------+                 +-----------------------------+
| id (PK, UUID)               |                 | id (PK, UUID)               |
| election_id (FK)            |                 | election_id (FK)            |
| voter_id (FK)               |                 | candidate_id (FK)           |
| idempotency_key (UNIQUE)    |                 | ballot_nonce (CHAR 64)      |
| receipt_hash (CHAR 64)      |                 | created_at                  |
| cast_at                     |                 +-----------------------------+
|                             |                  * NO VOTER_ID                |
| UNIQUE (election_id,        |                  * NO RECEIPT_HASH            |
|         voter_id)           |                  * NO FOREIGN KEY TO RECEIPT  |
+-----------------------------+                 +-----------------------------+
```

---

## 2. Dual-Table Ballot Secrecy & Privacy Limitations

### 2.1. The Secrecy Design
In traditional paper ballots, when a voter enters a voting booth, an election worker stamps their voter card (proving they participated) and hands them an unmarked ballot. The voter drops the unmarked ballot into a physical ballot box. The ballot box contains candidate selections, but no voter names.

AegisVote mirrors this physical privacy mechanism in relational database architecture through **Dual-Table Decoupling**:

1. **Table 1: `ballot_receipts`**:
   - Stores `(election_id, voter_id, receipt_hash, cast_at)`.
   - Records that voter $V$ participated in election $E$.
   - Does **not** contain candidate choice or candidate foreign key.

2. **Table 2: `votes`**:
   - Stores `(election_id, candidate_id, ballot_nonce, created_at)`.
   - Records that candidate $C$ received one vote in election $E$.
   - Does **not** contain voter ID, voter IP, or receipt hash.
   - Includes a cryptographically random 256-bit `ballot_nonce` ensuring row entropy.

### 2.2. Honest Engineering: Privacy Boundaries & Known Limitations
In senior engineering interviews, never claim "zero privacy risks" or "unbreakable end-to-end homomorphic anonymity." State the actual security boundaries clearly:

- **What AegisVote Guarantees**:
  An administrator or database operator running SQL queries (`SELECT * FROM votes`) cannot join `votes` with `ballot_receipts` because no common foreign key or receipt identifier exists between the tables.
- **What AegisVote Does Not Guarantee (Side-Channel Vulnerability)**:
  If an adversary inspects raw PostgreSQL write-ahead logs (`WAL`) or high-resolution server audit timestamps during an election with low submission volume (e.g. 1 vote every 30 minutes), the adversary could correlate the timestamp of `ballot_receipts` with the timestamp of `votes`.
- **Mitigation at Scale**: In large-scale production elections, insert times are bucketed or anonymized, votes are batched via a cryptographic mixing network, or homomorphic encryption (such as Paillier or ElGamal cryptosystems) is employed.

---

## 3. The Vote Submission Transaction: Exact SQL & Row Locking

The vote submission endpoint (`POST /api/votes`) executes a single atomic PostgreSQL transaction with serializable guarantees.

### 3.1. Step-by-Step Transaction Execution Path

```sql
-- Step 1: Start Transaction
BEGIN;

-- Step 2: Acquire Row-Level Shared Lock on Election
-- Prevents administrator from closing election while vote transaction is executing
SELECT status FROM elections WHERE id = $1::uuid FOR SHARE;
-- Domain Check: if status != 'OPEN', RAISE EXCEPTION 'ELECTION_NOT_OPEN';

-- Step 3: Check Idempotency Key Replay
SELECT id, receipt_hash, cast_at FROM ballot_receipts 
WHERE voter_id = $2::uuid AND idempotency_key = $3;
-- If found, return existing receipt immediately (Idempotent success)

-- Step 4: Verify Voter Eligibility
SELECT 1 FROM election_voters ev
JOIN voters v ON ev.voter_id = v.id
WHERE ev.election_id = $1::uuid AND v.id = $2::uuid 
  AND ev.is_eligible = TRUE AND v.is_active = TRUE;
-- If 0 rows, RAISE EXCEPTION 'INELIGIBLE_VOTER';

-- Step 5: Verify Candidate Assigned to Election
SELECT 1 FROM election_candidates
WHERE election_id = $1::uuid AND candidate_id = $4::uuid;
-- If 0 rows, RAISE EXCEPTION 'CANDIDATE_NOT_IN_ELECTION';

-- Step 6: Atomic Insert into ballot_receipts
-- ENFORCES DOUBLE-VOTE PREVENTION VIA UNIQUE(election_id, voter_id)
INSERT INTO ballot_receipts (election_id, voter_id, idempotency_key, receipt_hash, cast_at)
VALUES ($1::uuid, $2::uuid, $3, $5, CURRENT_TIMESTAMP);
-- If Postgres returns Error 23505 (unique_violation), RAISE EXCEPTION 'DOUBLE_VOTE_DETECTED';

-- Step 7: Decoupled Insert into votes table
INSERT INTO votes (election_id, candidate_id, ballot_nonce, created_at)
VALUES ($1::uuid, $4::uuid, $6, CURRENT_TIMESTAMP);

-- Step 8: Append-Only Tamper-Evident Audit Log
INSERT INTO audit_logs (action, actor_type, actor_id, ip_address, metadata, created_at)
VALUES ('BALLOT_CAST_SUCCESS', 'VOTER', $2::uuid, $7, $8::jsonb, CURRENT_TIMESTAMP);

-- Step 9: Commit All Mutations
COMMIT;
```

---

## 4. Double-Vote Prevention: Application Check vs Database Constraint

A common candidate mistake in system design interviews is relying on application-level checks:
```cpp
// INSECURE PATTERN:
if (voteRepo.hasVoted(electionId, voterId)) {
    throw runtime_error("Already voted");
}
voteRepo.recordVote(...);
```

### Why Application Checks Fail Under Concurrency (Time-of-Check to Time-of-Use - TOCTOU):
If a voter sends two concurrent HTTP requests from two browser tabs simultaneously:
1. Thread A queries `hasVoted`: returns `false`.
2. Thread B queries `hasVoted`: returns `false`.
3. Thread A executes `recordVote`: commits successfully.
4. Thread B executes `recordVote`: commits successfully!
**Result**: The voter has successfully double-voted, and election integrity is destroyed.

### How AegisVote Prevents This:
AegisVote enforces duplicate prevention at the database engine level via:
```sql
CONSTRAINT uq_ballot_receipts_election_voter UNIQUE (election_id, voter_id)
```
When Thread A and Thread B submit concurrently:
- PostgreSQL evaluates the unique B-Tree index on `(election_id, voter_id)`.
- The first thread to acquire the index page lock inserts its key.
- The second thread encounters duplicate key violation (`SQLSTATE 23505`) and is instantly aborted.
- AegisVote's C++ connection wrapper detects `23505` and translates it into an RFC 7807 `409 Conflict: Double Voting Prohibited` response.

---

## 5. Idempotency Key Design & Network Retries

### 5.1. The Problem of Network Drops After Commit
Consider the following real-world mobile failure scenario:
1. Voter submits ballot on cell phone.
2. The server processes the transaction, commits to PostgreSQL, and begins writing the HTTP 201 response.
3. The voter's train enters a tunnel; the mobile connection drops.
4. The client browser encounters `TCP ECONNRESET` or timeout.
5. Did the vote succeed or fail? The voter presses "Submit" again.

Without idempotency, the second submit is rejected as a "Double Vote", and the voter never receives their receipt hash, leaving them in panic and uncertainty.

### 5.2. How AegisVote Resolves This
- The client generates a UUIDv4 `Idempotency-Key` (e.g. `9b1deb4d-3b7d-4bad-9bdd-2b0d7b3dcb6d`) before transmitting the request.
- When the request arrives, Step 3 queries `ballot_receipts` for `(voter_id, idempotency_key)`.
- If a committed row exists with that key, the backend bypasses insertion and immediately returns the previously generated receipt hash with HTTP 200 OK!
- **Invariant Preserved**: Exactly one ballot is counted; the voter safely recovers their cryptographic receipt.
