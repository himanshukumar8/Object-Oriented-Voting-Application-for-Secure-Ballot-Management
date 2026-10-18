# ADR-001: Architecture Style Selection — Modular Monolith in Modern C++20

**Status:** Accepted  
**Date:** 2026-10-18  
**Context:**  
The legacy system was a monolithic single-file console program written in C++ with direct file I/O. We need to evolve this into a robust, high-performance, and maintainable backend supporting real-time concurrent voting. We evaluated three architectural styles:
1. Microservices Architecture (independent election service, voter service, tally service, auth service with message brokers)
2. Modular Monolith in C++20
3. Monolith in Go or Java/Spring Boot

**Decision:**  
We chose a **Modular Monolith built in Modern C++20**.

### Rationale:
1. **Domain Cohesion & Consistency:** Voting operations require atomic state transitions across elections, candidate slates, voter rosters, and ballot tallies. In a microservices architecture, preventing duplicate votes would require distributed transactions (2PC or Saga pattern), which introduce latency, failure points, and eventual consistency risks incompatible with strict ballot integrity invariants.
2. **Modern C++20 Standard:** Modern C++ (using RAII, `std::string`, `std::unique_ptr`, move semantics, and standard algorithms) provides total control over memory, predictable sub-millisecond latency, zero garbage-collection pauses, and minimal memory footprint.
3. **Operational Simplicity:** A single compiled binary with a PostgreSQL database minimizes operational overhead. It eliminates service discovery, container orchestrators, and network hops between internal services, making it straightforward to build, test, and deploy reliably.
4. **Interview Demonstration Value:** Building a secure, multi-threaded C++ backend with clean OOP, clean architecture, and PostgreSQL integration demonstrates advanced systems programming, thread safety, and software design mastery to engineering hiring managers.

### Consequences:
- **Positive:** Single deployment artifact, strong compile-time type safety, sub-millisecond transaction execution, atomic relational integrity.
- **Negative:** Requires rigorous internal modular boundaries to prevent code coupling. Team must adhere to domain-driven layer boundaries.
