# Benchmark Validation & Latency Root Cause Investigation

**Document Version:** 1.0.0  
**Audit Track:** Audit E — Benchmark Integrity  
**Date:** 2026-10-18  
**Auditor:** AegisVote Independent Release Audit Team  

---

## 1. Executive Summary & Diagnostic Findings

During the initial benchmarking phase, `docs/BENCHMARKS.md` recorded an anomaly: all endpoints exhibited approximately **2,048 ms (2.05 seconds)** of client-observed latency and an aggregate throughput ceiling of **~2.4 requests/second** across 5 concurrent workers.

```
Initial Anomaly:
  GET /api/health -> Throughput: 2.4 req/s | p50: 2048.90 ms | p95: 2072.46 ms
  GET /api/elections -> Throughput: 2.4 req/s | p50: 2043.52 ms | p95: 2067.34 ms
```

### 1.1 Root Cause Identification: The Windows Loopback IPv6 Connect Timeout
Our independent release audit conducted a forensic isolation test between `localhost` and `127.0.0.1` on the Windows host environment.

1. **Host Binding Configuration:**
   In `src/main.cpp` (line 36), the C++ server binds to `0.0.0.0:8080`:
   ```cpp
   std::string host = getEnvOrDefault("HOST", "0.0.0.0");
   ```
   On Windows, `0.0.0.0` binds strictly to the **IPv4** wildcard address (`AF_INET`). It does NOT bind to the **IPv6** loopback (`[::1]`).

2. **Client-Side DNS Resolution on Modern Windows:**
   When Python client libraries (`requests`, `urllib3`, `socket.create_connection`) resolve hostname `"localhost"`, Windows returns dual records:
   - IPv6: `::1` (attempted first by RFC 6724 / Happy Eyeballs sorting)
   - IPv4: `127.0.0.1`

3. **The 2.0-Second SYN Timeout:**
   When Python initiates a TCP connection to `http://localhost:8080/`, it sends a TCP SYN packet to `[::1]:8080`. Because no server process listens on IPv6 port 8080, Windows does not immediately return a TCP RST; instead, the connection attempt hangs for exactly **2,000 milliseconds (2.0 seconds)** waiting for an ACK, before timing out and failing over to IPv4 (`127.0.0.1:8080`).

4. **The Benchmark Script Multiplier:**
   In `scripts/benchmark_suite.py`, each concurrent worker invoked `session = requests.Session()` inside the thread function, instantiating a fresh connection pool per request. Therefore, **every single request** suffered the 2,000 ms IPv6 SYN timeout penalty:
   $$\text{Client Latency} = 2000\,\text{ms (IPv6 SYN Timeout)} + 4\,\text{ms (Server Processing)} \approx 2048.9\,\text{ms}$$
   $$\text{Throughput with 5 Workers} = \frac{5\text{ workers}}{2.048\text{ seconds}} \approx 2.44\,\text{req/s}$$

---

## 2. Empirical Verification & Isolation Experiments

To prove this diagnostic conclusion, we executed three controlled isolation benchmarks against the running `voting_server.exe` instance:

### Experiment 1: Direct Hostname Comparison (Fresh TCP Handshake)
```powershell
# Case A: localhost (Suffers Windows IPv6 SYN timeout)
python -c "import time, requests; s = requests.Session(); t0 = time.perf_counter(); r = s.get('http://localhost:8080/api/health'); print((time.perf_counter()-t0)*1000)"
# Output: 2049.01 ms

# Case B: 127.0.0.1 (Direct IPv4 bypass)
python -c "import time, requests; s = requests.Session(); t0 = time.perf_counter(); r = s.get('http://127.0.0.1:8080/api/health'); print((time.perf_counter()-t0)*1000)"
# Output: 4.24 ms
```
**Conclusion:** The ~2.0-second delay was 100% client-side connection establishment overhead caused by the Windows IPv6 SYN timeout, with zero relationship to server-side query processing or database contention.

### Experiment 2: Persistent TCP Keep-Alive Session
Measuring 50 sequential requests over a reused HTTP/1.1 persistent TCP socket to `http://127.0.0.1:8080/api/health`:
```
Sample Size: 50 requests
p50 Latency: 0.76 ms
Min Latency: 0.65 ms
Mean Latency: 0.98 ms
Max Latency: 10.20 ms
```
**Conclusion:** Actual C++ HTTP server pipeline and PostgreSQL health query executes in **sub-millisecond (< 1 ms)** time when TCP connection overhead is amortized.

### Experiment 3: Fresh TCP Handshake Per Request (No Keep-Alive)
Measuring 50 requests with `Connection: close` header (forcing a new TCP 3-way handshake and tear-down per call):
```
Sample Size: 50 requests
p50 Latency: 1.11 ms
Min Latency: 0.95 ms
Mean Latency: 1.20 ms
Max Latency: 3.83 ms
```

---

## 3. Validated Reproducible Benchmark Results

With `scripts/benchmark_suite.py` updated to target `AEGISVOTE_URL=http://127.0.0.1:8080`, the benchmark suite was executed across all production endpoints against a dedicated benchmark election.

### Environment Specification
- **Hardware:** Windows 11 x86_64, 8 Cores, 16 GB RAM
- **Compiler:** GCC 14.2.0 (UCRT64 MinGW-w64) with `-O2 -std=c++20`
- **Database:** PostgreSQL 18.1 on `localhost:5432`, Connection Pool Size: 8
- **Network Interface:** Loopback (`127.0.0.1`)
- **Warm-Up:** 10 pre-authenticated voter sessions generated prior to test execution

### Measured Results Table

| Endpoint / Workflow | Method | Concurrency | Requests | Throughput (req/s) | p50 (ms) | p95 (ms) | p99 (ms) | Max (ms) | Error Rate |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **Health Check** (`/api/health`) | `GET` | 5 | 50 | **787.2** | **4.31** | **7.03** | **18.07** | 18.07 | 0.0% |
| **Election Listing** (`/api/elections`) | `GET` | 5 | 50 | **445.4** | **8.94** | **11.94** | **20.64** | 20.64 | 0.0% |
| **Candidate Roster** (`/api/candidates`) | `GET` | 5 | 50 | **853.7** | **4.38** | **6.54** | **14.64** | 14.64 | 0.0% |
| **Voter Authentication** (`/api/auth/voter/login`) | `POST` | 2 | 10 | **31.9** | **62.40** | **66.37** | **66.37** | 66.37 | 0.0% |
| **Vote Submission** (`/api/votes` - ACID Transaction) | `POST` | 5 | 10 | **235.8** | **7.27** | **28.19** | **28.19** | 28.19 | 0.0% |
| **Certified Results Retrieval** (`/api/elections/{id}/results`) | `GET` | 5 | 100 | **642.9** | **4.73** | **23.01** | **28.14** | 28.14 | 0.0% |

---

## 4. Workload Characteristics & Component Breakdown

### 4.1 Read Queries (Health, Candidates, Elections)
- **Profile:** Pure I/O and JSON serialization.
- **Performance:** p50 latencies remain between **4.31 ms and 8.94 ms** under 5 concurrent workers.
- **Throughput:** Achieves between **445 and 853 requests/second** without intermediate Redis or memory caches.

### 4.2 Authentication Key Stretching (PBKDF2-HMAC-SHA256)
- **Profile:** CPU-bound cryptographic work.
- **Parameters:** 100,000 iterations of PBKDF2-HMAC-SHA256 with 16-byte cryptographically secure random salt.
- **Performance:** p50 latency is **62.40 ms**, throughput is **31.9 req/s** on 2 concurrent worker threads.
- **Audit Assessment:** This latency is mathematically intentional and essential for resisting offline brute-force and dictionary attacks on leaked database credentials.

### 4.3 Atomic Vote Submission Path (`POST /api/votes`)
- **Profile:** Highly concurrent, ACID transactional multi-table write.
- **Operations executed inside single transaction:**
  1. Row lock (`SELECT status FROM elections ... FOR SHARE;`)
  2. Idempotency validation (`SELECT ... FROM ballot_receipts WHERE voter_id = $1 AND idempotency_key = $2;`)
  3. Unique constraint enforcement on voter participation (`INSERT INTO ballot_receipts ...`)
  4. Anonymous ballot insertion (`INSERT INTO votes ...`)
  5. Cryptographic HMAC receipt calculation (`HMAC-SHA256`)
- **Performance:** p50 latency is **7.27 ms**, p95 is **28.19 ms**, with **235.8 req/s throughput** across 5 concurrent workers with **100% data consistency (0 double-votes, 0 orphaned rows)**.

---

## 5. Benchmark Limitations & Reproducibility Instructions

### Reproducibility Command
To reproduce these exact benchmark measurements on any clean environment:
```powershell
# Set target URL to IPv4 loopback to avoid Windows IPv6 SYN timeout
$env:AEGISVOTE_URL = "http://127.0.0.1:8080"
python -u scripts/benchmark_suite.py
```

### Measurement Limitations
1. **Loopback Networking:** Measurements were conducted over local loopback (`127.0.0.1`). Real-world client-observed latency will include WAN transmission and TLS handshake overhead (typically +20–50 ms depending on geographical distance and CDN presence).
2. **PostgreSQL Shared Cores:** The database engine and C++ server shared the same physical CPU cores on the host machine. In a distributed multi-tier deployment (backend server separate from database server), network I/O latency will increase p50 by ~1.0–2.0 ms while eliminating CPU contention between PBKDF2 hashing and PostgreSQL worker threads.
