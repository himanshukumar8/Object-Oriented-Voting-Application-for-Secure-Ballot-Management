# Project Audit & Baseline Report: Secure Ballot Management System

**Audit Date:** October 18, 2026  
**Auditor:** Staff Software & Security Engineering  
**Repository:** `himanshukumar8/Object-Oriented-Voting-Application-for-Secure-Ballot-Management`  
**Current Branch:** `main`  
**Baseline Commit:** `ab790f5770e9c9c6e797ca17346c28f9c2350e80` ("Update vote.exe")  

---

## 1. Executive Summary

An exhaustive technical, architectural, security, and build audit was conducted on the repository. The application is currently a single-file C++ console program (`vote.cpp`, 1,165 lines) coupled directly to Windows Win32 console APIs and flat comma-separated value (CSV) files stored in `Data/`. 

### Critical Findings:
1. **Repository Broken at HEAD**: The latest commit (`ab790f5`) contains syntax errors and variable redeclarations that fail compilation under all C++ compilers (`g++`, `clang++`, `cl.exe`).
2. **Extreme Memory & Remote Code Execution Risk**: Rampant usage of `gets()` (23 occurrences) writing unbounded terminal inputs into small fixed-size stack buffers (`CandidateID[5]`, `PollID[5]`, `VoterID[10]`).
3. **Severe Authentication & Authorization Flaws**: Hardcoded administrative credentials (`admin` / `123`) in C++ code; voter "passwords" are plaintext dates of birth (`DOB`) read directly from `Data/Voter.csv`.
4. **Data Loss & Concurrency Corruption**: Non-atomic file replacement using temporary files (`remove()` and `rename()`) without locking, thread safety, or crash recovery; duplicate voting checks rely on a temporary CSV file (`Data/Polls/Temp.csv`) that is explicitly deleted upon polling completion (`remove("Data/Polls/Temp.csv")`).
5. **Architectural Anti-Patterns**: Unbounded recursive loops leading to stack overflow, misuse of inheritance (`Admin` inherits from `Candidate`, `Voter`, and `Poll`), manual destructor calls on automatic stack objects causing undefined behavior, and global mutable state (`DVSOfficer`).

---

## 2. Repository Inventory & Environmental Diagnostics

### 2.1 File Structure
```
.
├── .gitattributes
├── README.md
├── vote.cpp                             (1,165 lines, 28,741 bytes)
├── vote.exe                             (Precompiled binary, 312,501 bytes)
├── .vscode/
│   ├── settings.json
│   └── tasks.json
├── Data/
│   ├── Candidate.csv                   (5 candidate records)
│   ├── Polls.csv                       (4 poll records)
│   └── Voter.csv                       (2 voter records)
└── ScreenShot/                         (14 UI walkthrough images)
```

### 2.2 Version Control & Commit Lineage
- `ab790f5` (HEAD): Attempted to remove explicit destructor invocations (`C.~Candidate()`), but introduced fatal syntax corruptions (lines 867–869).
- `2e8a7ef`: Tested adding records to `Data/Candidate.csv` and `Data/Polls.csv`, updating `vote.exe`.
- `4f9718f`, `b5f3a0c`, `a79e146`: Setup VS Code settings and documentation.
- **Untracked Artifacts in Git**: A 312 KB executable `vote.exe` was directly committed to Git, violating hygiene rules for binary artifacts.
- **CI/CD & Workflows**: Zero GitHub Actions or automation workflows exist (`.github/` missing).
- **Automated Tests**: Zero unit, integration, or regression test harnesses exist.

---

## 3. Compiler & Build Baseline Analysis

### 3.1 Build Attempt at HEAD (`ab790f5`)
**Command Executed:**
```powershell
g++ -std=c++17 -Wall -Wextra vote.cpp -o vote_baseline_test.exe
```

**Compiler Diagnostics:**
```
vote.cpp:869:100: error: stray '\' in program
vote.cpp:869:145: error: missing terminating " character
vote.cpp:829:39: error: redeclaration of 'Admin V'
vote.cpp:866:47: error: redeclaration of 'Admin C'
vote.cpp:867:1: error: 'te' was not declared in this scope
vote.cpp:867:8: error: '::UpdateData' has not been declared
vote.cpp:891:47: error: redeclaration of 'Admin V'
vote.cpp:922:39: error: redeclaration of 'Admin P'
```
*Result:* **Compilation Failed.** The codebase in its present state cannot be built or run.

### 3.2 Compilation of Preceding Commit (`2e8a7ef`)
When inspecting the code prior to the accidental corruptions in `ab790f5`:
- **Compilation succeeded**, but emitted over **20 severe compiler warnings**:
  - `call to 'gets' declared with attribute warning: Using gets() is always unsafe - use fgets() instead [-Wattribute-warning]` (23 locations).
  - `control reaches end of non-void function [-Wreturn-type]` in `bool Admin::login()` (line 639) and `int VoteCast::Exit()` (line 679).
  - `ignoring return value of operator[] declared with attribute 'nodiscard' [-Wunused-result]` (line 249).
  - `comparison of integer expressions of different signedness` (lines 68, 243).

---

## 4. Deep Architectural & Code Defect Catalog

### 4.1 Memory Safety & Vulnerabilities
| Defect ID | File & Line | Severity | Description & Impact |
|:---|:---|:---|:---|
| **SEC-01** | `vote.cpp:340, 343, 346, 349, 370, 397, 405, 413, 434, 437, 440, 443, 464, 491, 499, 507, 528, 531, 534, 561, 721, 953, 1014` | **Critical** | **Unbounded buffer writes via `gets()`**. Fixed stack buffers such as `CandidateID[5]`, `Gender[5]`, `DOB[10]`, and `PollID[5]` are filled with unbounded input from `stdin`. An input longer than 4 chars triggers immediate stack overflow and arbitrary code execution. |
| **SEC-02** | `vote.cpp:48–51` | **High** | **Hardcoded Plaintext Credentials**. `AdminID = "admin"`, `AdminPass = "123"` hardcoded into class definition. Credentials cannot be rotated without binary recompilation. |
| **SEC-03** | `vote.cpp:682–690` | **High** | **Trivial Voter Impersonation**. Voter password is their DOB string (`23/08/2000`) stored unhashed in `Data/Voter.csv`. Anyone with knowledge of a voter's birthday can cast their vote. |
| **SEC-04** | `vote.cpp:726–733` | **Medium** | **Ballot Secrecy Violation**. Screen displays voter's real name alongside their selected candidate ID, candidate name, and party symbol. |

### 4.2 Data Integrity & Concurrency
| Defect ID | File & Line | Severity | Description & Impact |
|:---|:---|:---|:---|
| **DAT-01** | `vote.cpp:194–237` | **Critical** | **Non-Atomic File Updates & Race Conditions**. `UpdateRecord` streams file lines to `Data/Temp.csv`, deletes original via `remove()`, and renames `Temp.csv`. If process is interrupted or multiple requests hit this concurrently, data is permanently destroyed. |
| **DAT-02** | `vote.cpp:283, 690, 746` | **Critical** | **Ephemeral Duplicate Vote Tracking**. Duplicate voting checks test `!CheckRecord(TempFile, VoterID, 2)` against `Data/Polls/Temp.csv`. In `ExitPolling` (line 283), `remove("Data/Polls/Temp.csv")` deletes this file, erasing all record of who cast ballots! |
| **DAT-03** | `vote.cpp:66–74` | **Medium** | **Imprecise Integer Parsing**. `StrToInt` calculates digits using `pow(10, str.length()-i-1)` with floating-point arithmetic (`pow` returns `double`), risking precision errors on large numbers. |
| **DAT-04** | `vote.cpp:136–145, 160, 175` | **Medium** | **Fragile CSV Parsing**. `getline(..., ',')` assumes no quoted commas, newlines, or escaped characters. Any comma in a candidate name or party corrupts column alignment across all records. |

### 4.3 Architecture & Object-Oriented Flaws
| Defect ID | File & Line | Severity | Description & Impact |
|:---|:---|:---|:---|
| **ARC-01** | `vote.cpp:45` | **High** | **Violated LSP / False Inheritance Hierarchy**. `class Admin: protected Candidate, protected Voter, protected Poll`. Inheritance represents "is-a", not "has-access-to". An Administrator is not a Ballot, Candidate, or Poll. |
| **ARC-02** | `vote.cpp:323, 330` | **High** | **Manual Destructor Invocation on Stack Objects**. Explicit calls to `Vote.~VoteCast()` on automatic stack-allocated objects result in double-destruction when the object leaves its lexical scope, causing undefined behavior and heap/stack corruption. |
| **ARC-03** | `vote.cpp:332, 306, 637, 817, 1133` | **High** | **Unbounded Recursion & Stack Overflow**. `TakePoll` calls `TakePoll` recursively; `AdminPage` calls `AdminPage` recursively on every interaction; `login()` calls `login()` on failure. This exhausts call stack frames on sustained operation. |
| **ARC-04** | `vote.cpp:123` | **Medium** | **Global Mutable State**. Global object `Admin DVSOfficer;` holds state across functions, creating hidden dependencies and preventing multithreaded operation. |
| **ARC-05** | `vote.cpp:5–6, 76–86, 126` | **Medium** | **Platform Lock-In**. Inclusion of `<conio.h>`, `<windows.h>`, `SetConsoleTextAttribute`, `system("cls")`, and raw `getch()` prevents compiling or running on Linux, macOS, or inside cloud containers. |

---

## 5. Existing Feature Set vs Required Scope

| Functional Area | Existing Implementation | Required Production Scope | Gap Analysis |
|:---|:---|:---|:---|
| **Admin Auth** | Hardcoded `admin` / `123` | Argon2id/bcrypt hashed passwords, JWT/session management, RBAC | Must be completely re-implemented with secure cryptographic storage. |
| **Voter Auth** | Plaintext DOB comparison | Cryptographic voter tokens / credentials, rate-limiting, secure sessions | Must decouple public identifiers from auth tokens. |
| **Election Lifecycle** | "Upcoming" vs "Conducted" | DRAFT → SCHEDULED → OPEN → CLOSED → RESULTS_PUBLISHED | Formally model state machine with strict transition guards. |
| **Vote Submission** | Writes to `Data/Polls/<id>.csv` | PostgreSQL ACID transaction, ballot secrecy decoupling, unique constraints | Migrate to relational persistence with atomic commit guarantees. |
| **Duplicate Prevention** | Deleted temp file | DB-level unique constraint (`UNIQUE(election_id, voter_id)`), idempotent token | Enforce invariant at database engine level. |
| **Auditing & Logs** | None | Structured audit trail for admin actions without violating ballot secrecy | Introduce immutable audit logs. |
| **User Interface** | Windows console (`getch`, `system("cls")`) | Modern responsive browser interface (React + TypeScript) | Build accessible, secure web frontend talking to REST API. |
| **API Layer** | None (monolithic console loop) | Clean RESTful HTTP JSON API | Build modern C++ HTTP backend service with OpenAPI schema. |

---

## 6. Audit Verdict & Migration Mandate

The existing monolithic console code serves as a reference for historical domain intent (candidate enrollment, voter registration, poll creation, voting, and result tallying), but **cannot be directly patched or run in a production web environment**. 

Per non-negotiable guidelines:
1. Preserve the repository history and original `vote.cpp` as the baseline artifact.
2. Establish a modern C++ modular service architecture backed by PostgreSQL.
3. Design and implement a high-performance REST API.
4. Construct a responsive, accessible React/TypeScript web interface.
5. Create an automated test harness covering invariants, concurrency, and security.
