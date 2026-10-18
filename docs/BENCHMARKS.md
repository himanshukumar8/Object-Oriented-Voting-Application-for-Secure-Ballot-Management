# Performance Benchmark & Latency Distribution Report

## 1. Test Environment & System Configuration

- **Backend Engine**: C++20 Native Multi-Threaded HTTP Server (`voting_server.exe`)
- **Persistence Layer**: PostgreSQL 18.1 with RAII Connection Pool (8 active workers)
- **Host Hardware**: Windows x86_64, Multi-Core Dedicated Host
- **Compiler**: GCC 14.2.0 (UCRT64) with `-O2 -std=c++20`
- **Cryptographic Engine**: OpenSSL 3.x (PBKDF2-HMAC-SHA256, HMAC-SHA256 receipts)
- **Benchmarking Tool**: AegisVote Automated Latency Harness (`scripts/benchmark_suite.py`)

## 2. Benchmark Summary Results

| Endpoint / Operation | Concurrency | Total Requests | Throughput (req/s) | p50 (ms) | p95 (ms) | p99 (ms) | Error Rate |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Health Check (GET /api/health)** | 5 | 50 | 787.2 | 4.31 | 7.03 | 18.07 | 0.0% |
| **Election Listing (GET /api/elections)** | 5 | 50 | 445.4 | 8.94 | 11.94 | 20.64 | 0.0% |
| **Candidate Roster (GET /api/candidates)** | 5 | 50 | 853.7 | 4.38 | 6.54 | 14.64 | 0.0% |
| **Voter Authentication (POST /api/auth/voter/login)** | 2 | 10 | 31.9 | 62.40 | 66.37 | 66.37 | 0.0% |
| **Vote Submissions (POST /api/votes - ACID Multi-Table)** | 5 | 10 | 235.8 | 7.27 | 28.19 | 28.19 | 0.0% |
| **Certified Results Retrieval (GET /api/elections/{id}/results)** | 5 | 100 | 642.9 | 4.73 | 23.01 | 28.14 | 0.0% |

## 3. Engineering Analysis

### 3.1. Read Latency & Throughput Profile
Under concurrent load (5 workers, fresh TCP connections with `Connection: close`), read endpoints demonstrate client-observed p50 latency between **4.31 ms and 8.94 ms**, and p95 latency between **6.54 ms and 11.94 ms** (with throughput between 445 and 853 req/s). In micro-benchmarks with reused persistent TCP keep-alive sockets, raw internal server query processing averaged **0.76 ms** (p50). Client-observed latency includes local loopback TCP handshakes and should not be conflated with raw server processing time.

### 3.2. Authentication Latency
Voter and Admin authentication endpoints exhibit ~60–66 ms latency (p50: 62.40 ms) by design due to PBKDF2-HMAC-SHA256 key stretching (100,000 iterations). This deliberate CPU cost protects against brute-force password cracking while easily maintaining acceptable interactive response times.

### 3.3. Write Path & Atomic Vote Submission
The vote submission path performs an atomic PostgreSQL transaction consisting of: row-level lock (`FOR SHARE`) on the election, idempotency check, dual-table insertion (`ballot_receipts` with unique constraint enforcement and decoupled `votes` table), and audit logging. Under 5 concurrent client worker threads, the system achieved zero duplicate entries, 100% data consistency, client-observed p50 latency of **7.27 ms**, and p95 of **28.19 ms** (235.8 req/s throughput).

---

## 4. Benchmark Scope & Environmental Qualifications

1. **Loopback Environment:** All benchmarks were measured over IPv4 loopback (`127.0.0.1:8080`) on a single dedicated Windows development workstation. Real-world deployments behind a reverse proxy (e.g. NGINX) with WAN latency and TLS handshakes will observe higher end-to-end response times (+20–50 ms).
2. **Shared Host Resources:** PostgreSQL and `voting_server.exe` shared the same host CPU cores during testing.
3. **Capacity Boundaries:** The system was verified up to 20 concurrent threads locally. High-throughput distributed clustering (>10,000 req/s) was not evaluated and requires multi-node database sharding.
4. **Historical Windows IPv6 Anomaly:** Initial benchmarks using hostname `localhost` experienced an artifactual ~2,000 ms delay due to the Windows loopback IPv6 SYN timeout. Explicitly binding and targeting `127.0.0.1` resolved this.
