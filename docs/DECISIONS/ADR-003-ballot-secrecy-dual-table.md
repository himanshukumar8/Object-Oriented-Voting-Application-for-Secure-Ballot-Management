# ADR-003: Ballot Secrecy & Verification via Dual-Table Architecture

**Status:** Accepted  
**Date:** 2026-10-18  
**Context:**  
A foundational requirement of voting systems is ballot secrecy: nobody—not election administrators, database administrators, or external observers—should be able to discover which candidate an individual voter voted for. At the same time, the system must enforce eligibility and prevent any voter from casting more than one ballot.

We evaluated three approaches:
1. **Single Table with `voter_id` and `candidate_id`:** Directly links voter to choice. Completely fails ballot secrecy.
2. **End-to-End Cryptographic Zero-Knowledge Proofs (ZKP) / Homomorphic Encryption:** Advanced cryptography (e.g. Paillier cryptosystem, Helios Voting). Adds immense mathematical complexity, huge computational overhead, complex key ceremonies, and is disproportionate for this web application scope.
3. **Decoupled Dual-Table Architecture:** Two relational tables inserted in the same atomic transaction:
   - `ballot_receipts`: Stores `(election_id, voter_id, receipt_hash, cast_at)`.
   - `votes`: Stores `(election_id, candidate_id, ballot_nonce, cast_at)`.

**Decision:**  
We chose the **Decoupled Dual-Table Architecture**.

### Rationale:
1. **Enforceable Double-Vote Prevention:** The unique constraint `UNIQUE(election_id, voter_id)` on `ballot_receipts` reliably prevents multiple votes from the same user.
2. **Separation of Voter and Choice:** The `votes` table contains NO foreign key to `voter_id` or `ballot_receipts`. A database administrator inspecting the raw tables cannot link a specific row in `votes` to a row in `ballot_receipts`.
3. **Receipt Verification:** The voter receives a signed receipt hash proving their ballot was counted, without revealing their candidate choice on the receipt itself.
4. **Pragmatic Security:** Achieves strong privacy within relational database bounds without the extreme overhead of homomorphic encryption, while clearly documenting the privacy boundary and assumptions.

### Consequences:
- **Positive:** Clear, auditable implementation; mathematical double-vote prevention; protects voter privacy from table-level correlation.
- **Negative:** In elections with very low voter volume, timing analysis (correlating `cast_at` timestamps) could infer choices. Mitigated by randomizing transaction order or truncating timestamp precision to minutes/hours.
