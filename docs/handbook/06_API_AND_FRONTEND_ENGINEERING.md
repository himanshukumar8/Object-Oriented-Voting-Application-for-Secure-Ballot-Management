# Chapter 14 & 15: REST API Specification, RFC 7807 Standard & React 19 Frontend Architecture

## 1. REST API Architecture & Endpoint Specification

AegisVote exposes a clean, predictable RESTful API adhering to HTTP semantics and standardized JSON payloads.

### 1.1. Core API Endpoints

| Method | Endpoint | Access Role | Description | Idempotent |
| :--- | :--- | :--- | :--- | :---: |
| `GET` | `/api/health` | Public | Readiness and liveness probe (checks DB pool). | Yes |
| `POST` | `/api/auth/admin/login` | Public | Administrator login (returns HMAC token & role). | No |
| `POST` | `/api/auth/voter/login` | Public | Voter login (returns HMAC token & voter info). | No |
| `GET` | `/api/auth/me` | Authenticated | Introspect current user claims and expiration. | Yes |
| `GET` | `/api/elections` | Public / Voter | List elections (flags `has_voted` if voter token). | Yes |
| `POST` | `/api/elections` | Admin Only | Create new election in `DRAFT` state. | No |
| `GET` | `/api/elections/{id}` | Public / Voter | Retrieve election details and candidate slate. | Yes |
| `POST/PUT` | `/api/elections/{id}/status` | Admin Only | Advance election lifecycle along state machine. | Yes |
| `GET` | `/api/elections/{id}/candidates` | Public | List candidates officially assigned to election. | Yes |
| `POST` | `/api/elections/{id}/candidates` | Admin Only | Assign enrolled candidate to election. | Yes |
| `GET` | `/api/elections/{id}/voter-status` | Voter Only | Check voter eligibility and participation flag. | Yes |
| `GET` | `/api/candidates` | Public | List all enrolled political candidates. | Yes |
| `POST` | `/api/candidates` | Admin Only | Enroll candidate with biography and symbol. | No |
| `GET` | `/api/voters` | Admin Only | View voter registry. | Yes |
| `POST` | `/api/voters` | Admin Only | Register new voter and assign initial credentials. | No |
| `POST` | `/api/votes` | Voter Only | **Cast ballot** (requires UUID `Idempotency-Key`). | **Yes (Idempotent)** |
| `GET` | `/api/elections/{id}/results` | Conditional | Retrieve certified tallies (Public if published, Admin anytime). | Yes |
| `GET` | `/api/admin/audit-logs` | Admin Only | Retrieve immutable security audit trail. | Yes |

---

## 2. Standardized Error Handling: RFC 7807 Problem Details

Rather than returning unstructured strings or arbitrary error objects (`{ "error": "something failed" }`), AegisVote implements **RFC 7807 (Problem Details for HTTP APIs)**:

```json
{
  "status": 409,
  "title": "Double Voting Prohibited",
  "detail": "Voter has already cast a ballot in this election",
  "type": "about:blank"
}
```

### HTTP Status Code Semantics in AegisVote:
- **`200 OK`**: Successful query, state transition, or idempotent replay.
- **`201 Created`**: Successful entity creation or new ballot cast.
- **`400 Bad Request`**: Malformed JSON, missing fields, or validation failures.
- **`401 Unauthorized`**: Missing, invalid, or expired Bearer token.
- **`403 Forbidden`**: Valid token, but insufficient permissions (e.g. voter accessing admin API, or voter ineligible for election).
- **`404 Not Found`**: Target election, candidate, or voter does not exist.
- **`409 Conflict`**: Domain invariant violation (e.g. double vote attempt, illegal state transition).
- **`500 Internal Server Error`**: Database connection failure or unhandled exception (details sanitized to avoid leaking database internals).

---

## 3. Frontend Architecture: React 19 + TypeScript

The browser client is a single-page application built using React 19, TypeScript, and Vite, designed with a custom glassmorphism dark-mode aesthetic.

### 3.1. Design System & Tokens (`index.css`)
To ensure visual excellence, the UI avoids generic browser defaults in favor of a curated design system:
- **Color Palette**: Deep cosmic darks (`#0b0f19`, `#111827`), violet/indigo primary accents (`#6366f1`, `#4f46e5`), emerald success (`#10b981`), amber warnings (`#f59e0b`), rose error (`#f43f5e`).
- **Typography**: Google Fonts **Outfit** for modern headers; **Inter** for crisp body text; **JetBrains Mono** for cryptographic hashes and IDs.
- **Glassmorphism**: Semi-transparent panels with backdrop blur (`backdrop-filter: blur(16px)`), subtle gradient borders (`rgba(255, 255, 255, 0.08)`), and soft drop shadows.

### 3.2. Seamless Client-Side Idempotency
When a voter clicks "Confirm & Submit Ballot", the frontend client automatically generates a cryptographically random UUIDv4:

```typescript
// frontend/src/api.ts
async castVote(electionId: string, candidateId: string): Promise<BallotReceipt> {
  const idempotencyKey = crypto.randomUUID();
  return request<BallotReceipt>('/votes', {
    method: 'POST',
    headers: {
      'Idempotency-Key': idempotencyKey,
    },
    body: JSON.stringify({
      election_id: electionId,
      candidate_id: candidateId,
      idempotency_key: idempotencyKey,
    }),
  });
}
```
If the network drops and the voter clicks "Retry", the browser client re-transmits the identical `idempotencyKey`, ensuring the backend returns the original receipt without duplicating vote counts!

### 3.3. Zero-Placeholder Production Policy
Every component in the frontend interacts with the live backend:
- The Voter Portal displays real election state from PostgreSQL.
- The Admin Dashboard features working lifecycle controls that trigger real database transitions.
- The Candidate Roster and Voter Ledger forms perform real database inserts.
- The Security Audit Trail renders real audit logs recorded by the C++ backend.
