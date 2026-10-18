# ADR-002: Persistence Mechanism Selection — PostgreSQL vs CSV & NoSQL

**Status:** Accepted  
**Date:** 2026-10-18  
**Context:**  
The legacy system stored application state in flat CSV files (`Data/Candidate.csv`, `Data/Polls.csv`, `Data/Voter.csv`). Updates were made by streaming file contents to `Temp.csv` and renaming over the original. This created critical data loss risks, lacked transaction atomicity, had no concurrency control, and erased double-voting tracking records upon polling exit. We evaluated:
1. Flat CSV Files (Original)
2. NoSQL / Document Store (MongoDB)
3. In-Memory Store (Redis)
4. Relational Database (PostgreSQL 18)

**Decision:**  
We chose **PostgreSQL 18** using the native `libpq` C API.

### Rationale:
1. **ACID Transaction Guarantees:** Ballot submission requires atomic updates across multiple entities (marking voter participation in `ballot_receipts` and recording candidate choice in `votes`). PostgreSQL transactions guarantee that either both succeed or both roll back, preserving absolute integrity across crashes or timeouts.
2. **Database-Enforced Unique Constraints:** Double-vote prevention must not rely on application-level checks that are vulnerable to race conditions under concurrent requests. PostgreSQL's unique constraint `UNIQUE(election_id, voter_id)` guarantees at-most-once voting at the storage engine level.
3. **Foreign Key Integrity:** Relational foreign keys ensure votes cannot be cast for deleted candidates or unassigned election slates.
4. **Why Not MongoDB?** MongoDB's document model encourages embedding ballots within voter documents or election documents, which either breaches ballot secrecy or requires multi-document distributed transactions with weaker consistency guarantees and higher operational complexity.
5. **Why Not Redis as Primary Store?** Redis is an in-memory data store. Even with AOF/RDB persistence, it lacks relational constraints, foreign keys, and enterprise auditability for permanent legal voting records. Redis is suitable for ephemeral caching, but PostgreSQL is the definitive source of truth.

### Consequences:
- **Positive:** Absolute data durability, crash-safe atomic transactions, declarative schema migrations, index-backed sub-millisecond queries.
- **Negative:** Requires PostgreSQL client library (`libpq`) linking and local/cloud database instance management.
