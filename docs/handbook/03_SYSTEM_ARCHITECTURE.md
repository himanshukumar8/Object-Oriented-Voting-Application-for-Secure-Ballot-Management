# Chapter 5, 6 & 7: Modular Monolith Architecture, C++20 Systems Engineering & Pragmatic Object-Oriented Design

## 1. High-Level Architecture: The Modular Monolith

AegisVote is architected as a **disciplined modular monolith**.

```
+-------------------------------------------------------------------------+
|                              AegisVote UI                               |
|                  React 19 + TypeScript (Vite Bundle)                    |
+-------------------------------------------------------------------------+
                                    |
                           HTTPS / REST (JSON)
                                    |
+-------------------------------------------------------------------------+
|                  C++20 Multi-Threaded HTTP Engine                       |
|                          (cpp-httplib)                                  |
+-------------------------------------------------------------------------+
       |                         |                           |
+---------------+       +------------------+       +---------------------+
|  Auth Service |       | Election Service |       |   Voting Service    |
| (PBKDF2/HMAC) |       | (State Machine)  |       | (Atomic Transaction)|
+---------------+       +------------------+       +---------------------+
       |                         |                           |
+-------------------------------------------------------------------------+
|                       Domain Layer (Plain C++20)                        |
|        Election, Candidate, Voter, Ballot, Invariant Predicates         |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                        Data Persistence Layer                           |
|        VoterRepo, ElectionRepo, CandidateRepo, VoteRepo, AuditRepo      |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                  PostgreSQL 18 Connection Pool (RAII)                   |
|                  libpq C API Wrapper + Smart Handles                    |
+-------------------------------------------------------------------------+
                                    |
                               SQL Queries
                                    |
+-------------------------------------------------------------------------+
|                       PostgreSQL Relational DB                          |
|         ACID Transactions | Row Locks | Foreign Keys | Triggers        |
+-------------------------------------------------------------------------+
```

### 1.1. Architectural Justification: Why Not Microservices?
In senior engineering interviews, candidates often propose splitting applications into 5–8 microservices (Auth Service, Election Service, Vote Service, Audit Service, Notification Service).

In an election system, microservices introduce severe architectural risks:
1. **Loss of Atomic Transactions**: Enforcing double-vote prevention across an Auth Microservice and a Voting Microservice requires distributed 2-Phase Commit (2PC) or Saga patterns. Distributed transactions have high failure rates under network partitions and allow race conditions if a participant stalls.
2. **Network Latency Overhead**: Internal gRPC/REST hops between services add 5–15ms of network latency per vote submission.
3. **Operational Fragility**: 5 services require separate deployment pipelines, service meshes, distributed tracing (Jaeger/Zipkin), and secret rotation schemes.
4. **Data Consistency Hazards**: In an election, strong consistency is mandatory. A modular monolith allows the entire vote submission to be contained in a single ACID PostgreSQL transaction without distributed consensus overhead.

---

## 2. Modern C++20 Engineering Standards

AegisVote utilizes modern ISO C++20 standards, emphasizing safety, efficiency, and clear resource ownership.

### 2.1. Resource Acquisition Is Initialization (RAII)
Every system resource (database connections, transaction lifecycles, file descriptors, memory buffers) is encapsulated inside a scoped C++ object whose destructor deterministically releases the resource.

#### Example: The RAII Database Transaction Guard (`DbTransaction`)
```cpp
// include/database/DbConnection.hpp
class DbTransaction {
private:
    DbConnection& conn;
    bool committed{false};

public:
    explicit DbTransaction(DbConnection& conn) : conn(conn) {
        conn.beginTransaction();
    }

    ~DbTransaction() {
        if (!committed) {
            try {
                conn.rollbackTransaction();
            } catch (...) {
                // Destructors must never throw exceptions in modern C++
            }
        }
    }

    void commit() {
        conn.commitTransaction();
        committed = true;
    }

    // Non-copyable (Move-only ownership)
    DbTransaction(const DbTransaction&) = delete;
    DbTransaction& operator=(const DbTransaction&) = delete;
};
```
**Correctness Guarantee**: If an exception is thrown anywhere inside the vote submission path (e.g. database timeout, duplicate key violation, invalid candidate), the stack unwinds and `~DbTransaction()` is guaranteed to execute, rolling back the transaction. Leaked open transactions are impossible by construction.

---

### 2.2. Thread-Safe Connection Pool (`DbPool`)
Rather than opening and closing expensive TCP connections to PostgreSQL on every HTTP request, AegisVote maintains a fixed-capacity thread-safe connection pool using standard C++ concurrency primitives:

```cpp
// src/database/DbPool.cpp
std::shared_ptr<DbConnection> DbPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [this] { return !availableConnections.empty() || stopped; });

    if (stopped) {
        throw std::runtime_error("Connection pool is stopped");
    }

    auto conn = availableConnections.front();
    availableConnections.pop();

    // Custom deleter returns connection to pool upon smart pointer destruction
    return std::shared_ptr<DbConnection>(conn.get(), [this, conn](DbConnection*) {
        std::unique_lock<std::mutex> lock(this->mutex);
        this->availableConnections.push(conn);
        lock.unlock();
        this->cv.notify_one();
    });
}
```
**Key Concept**: The custom lambda deleter on `std::shared_ptr<DbConnection>` automatically returns the underlying connection back to `availableConnections` when the reference count drops to zero at the end of the HTTP handler function!

---

### 2.3. Const-Correctness & Zero-Cost Abstractions
- **Pass by Const-Reference**: Large domain objects (`const domain::Election&`, `const std::string&`) are passed by reference-to-const, avoiding heap copying.
- **Move Semantics (`std::move`)**: Transient objects and constructor parameters are transferred using rvalue move semantics, transferring buffer pointers rather than copying characters.
- **Zero Raw Pointers**: Raw memory allocations (`new`, `malloc`, `free`, `delete`) are completely eliminated in favor of `std::unique_ptr` and `std::shared_ptr`.

---

## 3. Pragmatic Object-Oriented Design & SOLID Analysis

Rather than forcing textbook abstraction layers (e.g. `IVotingServiceFactoryBuilder`), AegisVote applies Object-Oriented principles where they provide genuine maintainability and testability.

### 3.1. Single Responsibility Principle (SRP)
- **`PasswordHasher`**: Responsible solely for cryptographic salting and constant-time PBKDF2 comparison. Contains zero database code.
- **`TokenService`**: Responsible solely for HMAC-SHA256 signature generation and claims parsing.
- **`VoteRepository`**: Responsible solely for executing parameterized SQL against the `votes` and `ballot_receipts` tables.
- **`VotingService`**: Responsible for business orchestration: verifying invariants, assembling receipt hashes, and directing persistence.

### 3.2. Dependency Inversion Principle (DIP)
High-level services (`VotingService`, `ElectionService`) depend on repository abstractions rather than direct SQL handles. This allows unit test harnesses (`test_invariants.exe`) to execute lightning-fast domain verification without standing up external databases:

```cpp
// In production:
services::VotingService service(voteRepo, voterRepo, electionRepo, auditRepo);

// In tests:
// Repositories can be backed by mock pools or isolated test databases
```

### 3.3. Avoidance of Inheritance Overuse
In legacy C++ code, systems often subclass `Voter` and `Admin` from an abstract `User` base class. In AegisVote, voters and administrators have completely disjoint schemas, authentication requirements, and security domains (admins do not have voter IDs; voters do not have administrative privileges). AegisVote uses **composition and discrete value objects** rather than fragile polymorphic inheritance hierarchies.
