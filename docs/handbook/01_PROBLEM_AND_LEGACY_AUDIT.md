# Chapter 1 & 2: Problem Statement & Legacy Application Audit

## 1. Executive Summary & Problem Domain

Democratic elections, academic governance, and corporate shareholder ballots share a common, unforgiving requirement: **absolute data integrity under adversarial conditions**. Unlike e-commerce or social media systems where eventual consistency and duplicate message reconciliation are acceptable compromises, an electronic voting platform cannot permit:
1. **Phantom Ballots**: Ballots injected by unauthorized parties or automated scripts.
2. **Double Voting**: A voter casting more than one accepted ballot.
3. **Ballot Attribution**: Associating a voter's real identity with their secret political choice.
4. **Retroactive Tally Tampering**: Modifying cast votes post-submission.
5. **State Inversion**: Accepting ballots before an election opens or after it closes.

The objective of the AegisVote engineering initiative was to take a fragile, insecure legacy C++ console prototype (`vote.cpp`) and re-engineer it into a hardened, production-grade distributed software system with an enterprise web interface and a C++20 backend.

---

## 2. In-Depth Audit of the Legacy Application (`vote.cpp`)

To appreciate the architectural requirements of AegisVote, an engineer must understand the severe security, correctness, and reliability defects present in the legacy codebase.

The legacy application was a single-file C++ console program (`vote.cpp`, 973 lines) written using procedural techniques disguised inside pseudo-classes (`VotingSystem`, `Admin`, `Voter`).

### 2.1. Critical Security Vulnerability: Buffer Overflows via Unbounded `gets()`

The legacy application contained **23 distinct invocations of `gets()`** on fixed-size stack arrays:

```cpp
// Legacy Vulnerable Pattern in vote.cpp:
char name[20];
char dob[15];
char id[10];

cout << "Enter Name: ";
gets(name); // CWE-120: Buffer Copy without Checking Size of Input
cout << "Enter DOB: ";
gets(dob);
```

#### Vulnerability Mechanics & Exploitability Analysis:
- `gets()` reads from standard input until a newline or EOF is reached, performing **zero bounds checking**.
- If a user inputs 30 bytes into `name[20]`, the remaining 10 bytes immediately overwrite adjacent stack variables, frame pointers, and the return address.
- On modern operating systems, this causes immediate segmentation faults (`SIGSEGV` / `STATUS_ACCESS_VIOLATION`), or under targeted exploitation, arbitrary code execution (stack buffer overflow).
- **Standards Note**: `gets()` was formally deprecated in C99, deprecated in C++11, and completely **removed from the C11 and C++14 standards** due to being inherently unsafe.

---

### 2.2. Authentication & Credential Storage Weaknesses

1. **Hardcoded Administrative Credentials**:
   ```cpp
   // Legacy vote.cpp:
   if (strcmp(username, "admin") == 0 && strcmp(password, "123") == 0) { ... }
   ```
   Administrative privileges were protected by a hardcoded plaintext username `admin` and password `123`. Any actor with binary inspection tools (`strings`, disassembler) could compromise the entire system in seconds.

2. **Plaintext Password Storage**:
   Voter records were written to flat text files with plaintext credentials:
   ```
   101,John Doe,12/05/1990,password123
   ```
   No cryptographic hashing (such as Argon2id, bcrypt, or PBKDF2), salting, or key stretching was employed.

3. **Weak Identity Verification**:
   Voter authentication relied on Date of Birth (`dob`) strings as a secondary factor, which is publicly discoverable biographical information rather than a secure authentication credential.

---

### 2.3. Data Corruption & File Update Race Conditions

The legacy application updated voter participation status and vote counts by reading a file into memory, writing to a temporary file (`temp.txt`), deleting the original file with `remove()`, and renaming the temp file with `rename()`:

```cpp
// Legacy update pattern in vote.cpp:
ifstream fin("voters.txt");
ofstream fout("temp.txt");
while (fin >> id >> name >> hasVoted) {
    if (id == targetId) hasVoted = 1;
    fout << id << " " << name << " " << hasVoted << "\n";
}
fin.close();
fout.close();
remove("voters.txt");
rename("temp.txt", "voters.txt");
```

#### Concurrency & Atomicity Breakdown:
- **Non-Atomic Window of Data Loss**: Between `remove("voters.txt")` and `rename("temp.txt", "voters.txt")`, the original file does not exist. If the process crashes, the OS experiences a power failure, or an unhandled exception occurs in this window, **all voter records are permanently destroyed**.
- **Concurrent Write Corruption**: If two voters cast votes simultaneously on a multi-core machine, both threads read `voters.txt` concurrently, both create conflicting `temp.txt` files, and the second rename overwrites the first rename, resulting in **lost updates** and corrupted vote counts.
- **Race Condition on Temporary Duplicate File**: The legacy system attempted to track double voting by creating a local file `voted_voters.txt` and deleting it upon application exit. If the application was restarted, the double-voting ledger vanished completely!

---

### 2.4. Build Blocker at HEAD Commit (`ab790f5`)

Upon auditing the checked-out Git repository, the baseline commit was found to be in a broken, non-compilable state:
- Line 867 contained a corrupt, copy-pasted fragment: `te();::UpdateData(`.
- Several functions lacked return types or return statements across execution paths.
- Compiling commit `2e8a7ef` with GCC yielded over 20 fatal errors and warnings regarding control reaching the end of non-void functions and deprecated headers (`conio.h`, `windows.h`).

---

### 2.5. Summary Comparison Matrix

| Quality Attribute | Legacy Implementation (`vote.cpp`) | AegisVote C++20 Production Engine |
| :--- | :--- | :--- |
| **Input Safety** | Unbounded `gets()` (Buffer Overflow) | `std::string`, `nlohmann::json`, strict JSON schema validators |
| **Authentication** | Hardcoded `admin`/`123`, plaintext voter DOB | PBKDF2-HMAC-SHA256 (100,000 iterations), HMAC bearer tokens |
| **Persistence** | Unlocked CSV flat files (`remove`/`rename`) | PostgreSQL 18 with ACID transactions & connection pooling |
| **Duplicate Prevention** | Temporary text file deleted on process exit | Database-enforced `UNIQUE(election_id, voter_id)` constraint |
| **Ballot Privacy** | Voter ID stored adjacent to vote count in file | Decoupled dual-table secrecy schema (`ballot_receipts` vs `votes`) |
| **Concurrency** | Zero thread safety; concurrent writes corrupt files | Thread pool, row-level locks (`FOR SHARE`), optimistic idempotency |
| **Observability** | Console `cout` statements only | Immutable PostgreSQL `audit_logs` table with actor metadata |
