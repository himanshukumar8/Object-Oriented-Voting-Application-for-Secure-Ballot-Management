# AegisVote — Independent Release Audit
## Track A: Build & Toolchain Verification

**Audit Date:** 2026-10-18
**Auditor role:** Principal C++ Engineer / Build & Release Engineer (independent)
**Audit scope:** Track A only. Tracks B–D are explicitly out of scope.

---

## 1. Verdict

> **PASS WITH LIMITATIONS**

The C++20 backend and all three test targets compile cleanly from a fresh, isolated `build-audit/` directory using MSYS2 GCC 14.2.0 and CMake 4.4.4. Two compiler warnings recorded; zero errors. CTest discovers all three registered tests. No test execution was performed in Track A. Two CMake configuration portability defects are documented as limitations.

---

## 2. Repository Branch and Commit Audited

| Field | Value |
|---|---|
| Branch | `recovery/aegisvote-v2-fullstack` |
| HEAD commit | `49d06e7` — feat(aegisvote): preserve recovered full-stack implementation |
| Working tree | Clean (git diff and git diff --cached both empty) |
| `main` branch | Untouched at `ab790f5` |

Safety preflight passed before any build operation was attempted.

---

## 3. Toolchain Versions and Detected Platform

| Tool | Path | Version | Status |
|---|---|---|---|
| cmake | C:\Program Files\CMake\bin\cmake.exe | 4.4.4 | Present |
| g++ | C:\msys64\ucrt64\bin\g++.exe | 14.2.0 (Rev2, MSYS2) | Present |
| ninja | C:\Users\HIMANSHU\AppData\Local\...\ninja.exe | — | Present |
| git | C:\Program Files\Git\cmd\git.exe | — | Present |
| psql | C:\Program Files\PostgreSQL\18\bin\psql.exe | 18.1 | Present |
| cl (MSVC) | — | Not on PATH | Not required |
| clang++ | — | Not on PATH | Not required |

**Platform:** Windows x86-64, MSYS2 UCRT64 runtime
**C++ standard configured:** C++20 (STANDARD_REQUIRED ON, EXTENSIONS OFF)

---

## 4. Required Dependencies and Observed Availability

| Dependency | Required by | Available |
|---|---|---|
| libpq-fe.h (PG client headers) | CMakeLists.txt line 18 | YES — C:\Program Files\PostgreSQL\18\include\ |
| libpq.dll (PG client DLL) | CMakeLists.txt line 46 | YES — C:\Program Files\PostgreSQL\18\bin\ |
| libpq.lib (PG import lib) | Linker | YES — C:\Program Files\PostgreSQL\18\lib\ |
| OpenSSL ssl.h header | MSYS2 ucrt64 | YES — C:\msys64\ucrt64\include\openssl\ssl.h |
| libssl.a / libssl.dll.a | MSYS2 ucrt64 lib | YES |
| libcrypto.a / libcrypto.dll.a | MSYS2 ucrt64 lib | YES |
| httplib.h (cpp-httplib) | src/api/Server.cpp | YES — VENDORED at include/third_party/httplib.h (344 KB) |
| json.hpp (nlohmann/json) | src/api/Server.cpp et al | YES — VENDORED at include/third_party/json.hpp (920 KB) |
| ws2_32 / crypt32 | CMakeLists.txt lines 49-50 | YES — Windows SDK (MSYS2 implicit) |

### Source File Verification

All 13 CORE_SOURCES .cpp files confirmed present. All 3 test .cpp files confirmed present. All 18 header files confirmed present.

---

## 5. CMake Configuration Defects (Portability Limitations)

### Defect 1 — Hard-coded PostgreSQL path (Severity: MEDIUM)

CMakeLists.txt lines 18 and 46 hard-code `C:/Program Files/PostgreSQL/18/include` and `C:/Program Files/PostgreSQL/18/bin/libpq.dll`. This breaks on any machine with a different PostgreSQL path, version, or on Linux/macOS.

Recommended fix: Replace with `find_package(PostgreSQL REQUIRED)` and use the `PostgreSQL::PostgreSQL` CMake imported target. Requires approval to modify tracked file.

### Defect 2 — Hard-coded MSYS2 include path (Severity: LOW)

CMakeLists.txt line 19 hard-codes `C:/msys64/ucrt64/include`. On machines without MSYS2 at that path, OpenSSL headers will not be found, causing silent degradation.

Recommended fix: Replace with `find_package(OpenSSL REQUIRED)` and link `OpenSSL::SSL OpenSSL::Crypto`. Requires approval.

---

## 6. Configure Command and Exit Code

```
cmake -B build-audit -G Ninja -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe
```

**Exit code: 0**

Key output:
```
-- The CXX compiler identification is GNU 14.2.0
-- Configuring done (4.3s)
-- Generating done (0.0s)
-- Build files have been written to: .../build-audit
```

---

## 7. Build Command, Exit Code, and Target Summary

```
cmake --build build-audit
      --target voting_server test_invariants test_crypto test_concurrency
      --parallel 4
```

**Exit code: 0**

All 23/23 build steps completed:
- Steps 1–13: voting_core.a object files (13 .cpp units)
- Step 14: test_crypto.cpp.obj (1 warning)
- Steps 15–17: main.cpp.obj, test_invariants.cpp.obj, test_concurrency.cpp.obj (1 warning)
- Step 18: Server.cpp.obj
- Step 19: Linking libvoting_core.a (static library)
- Steps 20–23: Linking test_invariants.exe, test_crypto.exe, test_concurrency.exe, voting_server.exe

**Compiler errors:** 0
**Compiler warnings:** 2 (both in test code only)

---

## 8. Compiled Binaries — Filesystem Verification

| Binary | Size (bytes) | Timestamp | Status |
|---|---|---|---|
| build-audit/voting_server.exe | 1,618,435 | 2026-10-18 19:23:21 | PRODUCED |
| build-audit/test_invariants.exe | 201,335 | 2026-10-18 19:23:21 | PRODUCED |
| build-audit/test_crypto.exe | 525,909 | 2026-10-18 19:23:21 | PRODUCED |
| build-audit/test_concurrency.exe | 695,589 | 2026-10-18 19:23:21 | PRODUCED |
| build-audit/libvoting_core.a | 2,365,676 | 2026-10-18 19:23:21 | PRODUCED |

> NOTE: Successful compilation is not a test pass. Test execution requires a live PostgreSQL database. No tests were run in Track A.

---

## 9. CTest Discovery

Command: `ctest --test-dir build-audit --show-only`
**Exit code: 0**

Output:
```
Test project D:/Projects/.../build-audit
  Test #1: InvariantTests
  Test #2: CryptoTests
  Test #3: ConcurrencyRaceTests

Total Tests: 3
```

All three tests declared in CMakeLists.txt are correctly registered with CTest. No missing or inconsistent registrations.

| # | Name | CMakeLists.txt line | Binary |
|---|---|---|---|
| 1 | InvariantTests | 70 | test_invariants.exe |
| 2 | CryptoTests | 74 | test_crypto.exe |
| 3 | ConcurrencyRaceTests | 78 | test_concurrency.exe |

---

## 10. Compiler Warnings

| File | Line | Warning | Severity |
|---|---|---|---|
| tests/unit/test_crypto.cpp | 59 | unused variable 'valid' (-Wunused-variable) | LOW |
| tests/concurrency/test_concurrency.cpp | 178 | variable 'rejected' set but not used (-Wunused-but-set-variable) | LOW |

Both warnings are in test code only. Production source compiled warning-free.

---

## 11. Blockers and Recommended Actions

| # | Severity | Finding | Recommended Action |
|---|---|---|---|
| B1 | MEDIUM | Hard-coded PostgreSQL path in CMakeLists.txt | Replace with find_package(PostgreSQL REQUIRED). Requires approval. |
| B2 | LOW | Hard-coded MSYS2 OpenSSL path in CMakeLists.txt | Replace with find_package(OpenSSL REQUIRED). Requires approval. |
| B3 | LOW | 2 unused-variable warnings in test files | Fix unused variables in test source (no production impact). |
| B4 | INFO | Test binaries not executed | Track B should execute tests against a live PostgreSQL instance. |
| B5 | INFO | Pre-existing build/ directory present | No action needed; confirm build/ remains in .gitignore. |

---

## 12. Fact / Inference / Unverified Claim Separation

### Verified Facts

- Branch recovery/aegisvote-v2-fullstack, HEAD 49d06e7, clean working tree.
- All declared source, test, and header files exist on disk.
- Vendored httplib.h and json.hpp present in include/third_party/.
- CMake 4.4.4 configured project, exit code 0, in build-audit/.
- GCC 14.2.0 compiled all 23 build steps, exit code 0, zero errors.
- Two compiler warnings (test code only).
- All four expected binaries produced on disk with correct timestamps.
- CTest discovers exactly 3 tests matching CMakeLists.txt.
- PostgreSQL client headers, DLL, and import library exist at hard-coded path.
- OpenSSL headers and static libs exist in MSYS2 UCRT64.

### Inferences

- voting_server.exe requires DB_HOST, DB_PORT, JWT_SECRET, RECEIPT_SECRET env vars to start without error.
- Vendored httplib.h and json.hpp versions are not pinned in CMakeLists.txt and should be reviewed.

### Unverified Claims (not addressed in Track A)

- Tests pass when executed against a live PostgreSQL database.
- Server starts and responds to GET /api/health.
- Security primitives produce correct outputs under all inputs.
- Concurrency tests are free of data races (TSan/Helgrind not run).
- Docker build produces equivalent result to native Windows build.
- Frontend builds and is served correctly.

---

## 13. Summary

Track A is complete. The AegisVote C++20 backend and all three test targets **configure and compile cleanly** on this machine using MSYS2 GCC 14.2.0, CMake 4.4.4, and Ninja, from a fresh isolated `build-audit/` directory. No source modifications were required or made. Two CMake portability defects exist but do not block the build on the development machine. Two non-blocking test-code warnings exist.

**Track B (database integration tests, test execution) may now begin.**

---

*No source files were modified. No commits or pushes were made. The main branch was not touched.*
