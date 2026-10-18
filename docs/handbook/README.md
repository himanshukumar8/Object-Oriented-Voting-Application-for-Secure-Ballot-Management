# AegisVote Engineering Handbook & Systems Design Monograph
## Staff+ Engineering Standards | C++20 | PostgreSQL | Cryptography | Concurrency | System Design & Interview Mastery

**Author:** Staff/Principal Engineering Team  
**Platform Target:** AegisVote Secure Electronic Ballot Management Platform  
**Target Audience:** Software Engineers, Systems Architects, and Candidates preparing for SDE-1/SDE-2 engineering interviews at Amazon, Uber, PayPal, IBM, Cisco, and Google.

---

## Executive Overview

This Engineering Handbook provides an exhaustive, production-grade examination of **AegisVote**, an enterprise-grade secure voting platform engineered to replace an insecure, legacy procedural C++ console script with a modern, multi-threaded C++20 REST backend and React 19 TypeScript frontend.

Rather than relying on generic software engineering tropes or textbook generalities, this handbook explores the concrete technical implementation, formal invariants, relational schemas, concurrency failure modes, cryptographic guarantees, and performance benchmarks of the actual working system.

---

## Handbook Structure & Table of Contents

```
docs/handbook/
├── README.md                                 # Master Index, Reading Guide & Executive Summary
├── 01_PROBLEM_AND_LEGACY_AUDIT.md            # Problem Statement, Legacy C++ Vulnerability Audit (CVE Analysis)
├── 02_REQUIREMENTS_AND_INVARIANTS.md         # Formal Functional/Non-Functional Specs & The 7 Core Invariants
├── 03_SYSTEM_ARCHITECTURE.md                 # Modular Monolith Architecture, C++20 Idioms, RAII & Pragmatic SOLID
├── 04_DATABASE_DESIGN_AND_TRANSACTIONS.md    # Relational Schema, Dual-Table Ballot Secrecy, Atomic Duplicate Prevention
├── 05_SECURITY_AND_CRYPTOGRAPHY.md           # STRIDE/DREAD Threat Model, PBKDF2-HMAC-SHA256, Constant-Time Logic
├── 06_API_AND_FRONTEND_ENGINEERING.md        # REST API Contract (RFC 7807), State Machine & React 19 Frontend
├── 07_TESTING_BENCHMARKS_AND_OPERATIONS.md   # Concurrency Race Harness, Benchmark Results, Docker & Chaos Scenarios
├── 08_DECISION_RECORDS_AND_TRADE_OFFS.md     # ADRs & The "Why Not?" Analysis (Redis, Kafka, Mongo, Microservices)
└── INTERVIEW_QUESTIONS.md                    # Tiered Technical Interview Guide (Levels 1 through 5)
```

---

## Reading Guide by Engineering Role

| Role / Objective | Priority Chapters | Focus Areas |
| :--- | :--- | :--- |
| **Backend & C++ Engineer** | Chapters 1, 3, 4, 7 | C++20 RAII, connection pooling, zero-copy JSON, POSIX/Win32 abstraction, memory safety. |
| **Database & Systems Engineer** | Chapters 4, 7, 8 | PostgreSQL transactions, row-level locking (`FOR SHARE`), unique constraints, ballot secrecy. |
| **Application Security Engineer**| Chapters 1, 5, 8 | Buffer overflow remediation, STRIDE threat model, PBKDF2 key stretching, HMAC-SHA256 tokens. |
| **Frontend Engineer** | Chapters 2, 6 | React 19 TypeScript architecture, glassmorphism design system, idempotency key generation. |
| **DevOps & Site Reliability** | Chapters 7, 8 | Docker multi-stage builds, health check orchestration, resource sizing, disaster recovery. |
| **Interview Candidates (Amazon/Uber/PayPal)** | All Chapters + `INTERVIEW_QUESTIONS.md` | Deep dive into trade-offs, concurrency race handling, idempotency, and defense against tough interviewers. |

---

## Core Engineering Principles

1. **Correctness over Complexity**: We do not introduce microservices, distributed queues, or NoSQL stores without explicit, quantifiable domain requirements.
2. **Database-Enforced Invariants**: Application-level validation is necessary for user experience, but database-level constraints (`UNIQUE`, `FOREIGN KEY`, `CHECK`) are mandatory for truth.
3. **Defense in Depth**: Passwords are cryptographically salted and stretched; session tokens are cryptographically signed; ballot receipts are cryptographically sealed; and database rows are decoupled.
4. **Transparent Failure Modes**: Failures are deterministic and categorized according to RFC 7807 Problem Details.
