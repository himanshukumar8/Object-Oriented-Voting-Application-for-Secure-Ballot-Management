# Known Limitations & Explicit Assumptions

**Document Version:** 1.1.0  
**Status:** Validated Audit Baseline  
**Audience:** Release Auditors, Security Reviewers, System Architects, and Interview Evaluators  

---

## 1. System Scope & Election Assurance Disclaimers

> [!IMPORTANT]
> This software is an advanced software engineering and systems design project demonstrating enterprise-grade C++20, atomic PostgreSQL ACID concurrency control, secure REST APIs, and clean modular architecture. It is designed for organizational elections, student councils, shareholder ballots, and Staff+ engineering interview demonstration. **It is NOT certified for governmental or national sovereign public elections.** A sovereign public election requires physical Hardware Security Modules (HSMs), Paper Audit Trails (VVPAT), multi-party key ceremonies, Zero-Knowledge Succinct Proofs (ZK-SNARKs), and independent federal certification (e.g., EAC standards).

---

## 2. Technical Limitations & Architectural Trade-offs

### 2.1 Ballot Secrecy Model (Relational Decoupling vs Homomorphic Encryption)
- **Architecture:** The platform uses a dual-table relational decoupling architecture (`ballot_receipts` tracking voter participation, decoupled from `votes` recording candidate ballots).
- **Vulnerability Discovered During Audit:** Originally, both tables recorded `CURRENT_TIMESTAMP` within the same transaction. Because PostgreSQL evaluates `CURRENT_TIMESTAMP` as the transaction start timestamp (`transaction_timestamp()`), an observer with database read access could join `ballot_receipts` and `votes` on `br.cast_at = v.cast_at` to de-anonymize ballots with microsecond precision.
- **Remediation Implemented:** `votes.cast_at` is now truncated to hourly granularity (`date_trunc('hour', CURRENT_TIMESTAMP)`), completely breaking the exact microsecond join with voter receipts.
- **Residual Risk:** If turnout is extremely sparse (e.g., exactly 1 voter casts a ballot within a given 1-hour window), an observer with direct database access could infer that voter's candidate selection. A full sovereign election system requires an asynchronous cryptographic shuffle queue (mixnet) or homomorphic ballot encryption (e.g., Paillier or ElGamal).

### 2.2 Token Authentication Architecture
- **Design:** Authenticated sessions use a compact 2-part signed bearer token (`base64(payload).signature`) signed via HMAC-SHA256 with constant-time equality validation.
- **Limitation:** The token is not a full RFC 7519 3-part JWT with asymmetric RSA/ECDSA keys and JWKS endpoints. Token revocation prior to expiration requires secret rotation or server-side blocklisting (e.g. Redis).

### 2.3 Rate Limiting & Denial-of-Service Defense
- **Limitation:** The C++ application engine does not implement in-process sliding-window or token-bucket rate limiting.
- **Operational Requirement:** In any public deployment, the application MUST sit behind a reverse proxy (e.g., NGINX, Cloudflare, Traefik, or AWS ALB) that enforces connection rate limits (`limit_req_zone`) to prevent CPU exhaustion on password hashing endpoints.

### 2.4 Transport Security & Plaintext HTTP Binding
- **Limitation:** The C++ backend server binds to `0.0.0.0:8080` over plaintext HTTP. Native TLS termination in `cpp-httplib` requires OpenSSL server certificates and is disabled in the default local profile.
- **Operational Requirement:** Production deployments must terminate TLS 1.3 at the reverse proxy or cloud ingress load balancer. The backend must never be directly exposed to the public internet without HTTPS termination.

### 2.5 Single-Node Relational Scaling Boundary
- **Limitation:** All transactions are serialized against a single primary PostgreSQL database instance.
- **Throughput Profile:** Verified up to 787 req/s for read endpoints and 235 req/s for concurrent ACID multi-table vote insertions with connection pooling (8 connections). Scaling beyond 1,500 writes/second requires sharding by election ID or specialized distributed commit log architecture.

### 2.6 Identity Federation (SSO)
- **Limitation:** User authentication relies on internal database stores and PBKDF2-HMAC-SHA256 password stretching. It does not integrate external OIDC/OAuth2/SAML identity providers (e.g., Okta, Google Workspace, Azure AD).

### 2.7 Client-Side Trust Boundary
- **Assumption:** The application assumes the voter's browser runtime is uncompromised by malicious extensions, DOM-injection keyloggers, or operating-system malware.
