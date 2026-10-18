#!/usr/bin/env python3
"""
AegisVote - Automated End-to-End API Verification Script
Tests complete workflows against the running C++20 REST API backend:
- Health check
- Voter authentication
- Admin authentication
- Election & candidate retrieval
- Voting transaction & cryptographic receipt generation
- Atomic duplicate-vote prevention
- Admin lifecycle management (Draft -> Scheduled -> Open -> Closed -> Results Published)
- Results aggregation and audit log inspection
"""

import urllib.request
import urllib.error
import json
import uuid
import sys

BASE_URL = "http://localhost:8080"

def request(method, path, data=None, token=None, idempotency_key=None):
    url = f"{BASE_URL}{path}"
    headers = {"Content-Type": "application/json", "Connection": "close"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    if idempotency_key:
        headers["Idempotency-Key"] = idempotency_key

    payload = json.dumps(data).encode("utf-8") if data is not None else None
    req = urllib.request.Request(url, data=payload, headers=headers, method=method)

    try:
        with urllib.request.urlopen(req) as resp:
            body = resp.read().decode("utf-8")
            return resp.status, json.loads(body) if body else {}
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8")
        try:
            parsed = json.loads(body)
        except Exception:
            parsed = {"raw": body}
        return e.code, parsed

def main():
    print("=" * 60)
    print("       AEGISVOTE REST API E2E VERIFICATION SUITE")
    print("=" * 60)

    # 1. Health check
    print("\n[1] Checking /api/health ...")
    status, res = request("GET", "/api/health")
    assert status == 200, f"Expected 200, got {status}: {res}"
    assert res.get("status") == "UP" and res.get("database") == "CONNECTED"
    print(f"  --> PASS: Server status: {res.get('status')}, Database: {res.get('database')}")

    # 2. Voter Login
    print("\n[2] Testing Voter Login (VOTER-102 / VoterPass2026!) ...")
    status, res = request("POST", "/api/auth/voter/login", {"voter_id": "VOTER-102", "password": "VoterPass2026!"})
    assert status == 200, f"Expected 200, got {status}: {res}"
    voter_token = res.get("token")
    voter_info = res.get("user")
    assert voter_token and voter_info.get("voter_id") == "VOTER-102"
    print(f"  --> PASS: Authenticated voter: {voter_info.get('full_name')} ({voter_info.get('voter_id')})")

    # 3. Admin Login
    print("\n[3] Testing Admin Login (admin / AdminSecure2026!) ...")
    status, res = request("POST", "/api/auth/admin/login", {"username": "admin", "password": "AdminSecure2026!"})
    assert status == 200, f"Expected 200, got {status}: {res}"
    admin_token = res.get("token")
    admin_info = res.get("user")
    print(f"  --> Admin info received: {admin_info}")
    assert admin_token and (admin_info.get("role") in ["SUPER_ADMIN", "ELECTION_OFFICER", "ADMIN"])
    print(f"  --> PASS: Authenticated admin: {admin_info.get('username')} [{admin_info.get('role')}]")

    # 4. Create a dynamic voter for vote test
    dyn_voter_code = f"VOTER-{uuid.uuid4().hex[:6].upper()}"
    print(f"\n[4] Registering fresh voter {dyn_voter_code} ...")
    status, reg_res = request("POST", "/api/voters", {
        "voter_id": dyn_voter_code,
        "full_name": f"Test Voter {dyn_voter_code}",
        "email": f"{dyn_voter_code.lower()}@voting.internal",
        "password": "VoterPass2026!"
    }, token=admin_token)
    assert status == 201, f"Failed to register voter: {reg_res}"

    status, v_login = request("POST", "/api/auth/voter/login", {
        "voter_id": dyn_voter_code,
        "password": "VoterPass2026!"
    })
    assert status == 200, f"Failed to login with new voter: {v_login}"
    active_voter_token = v_login.get("token")
    print(f"  --> PASS: Fresh voter registered and authenticated: {dyn_voter_code}")

    # 5. List elections
    print("\n[5] Listing all elections (/api/elections) ...")
    status, elections = request("GET", "/api/elections")
    assert status == 200, f"Expected 200, got {status}: {elections}"
    print(f"  --> PASS: Found {len(elections)} elections:")
    open_election = None
    for el in elections:
        if el['status'] == 'OPEN':
            open_election = el
    assert open_election is not None, "Expected at least one OPEN election"

    # 6. Get candidates for OPEN election
    print(f"\n[6] Fetching candidates for election {open_election['id']} ...")
    status, candidates = request("GET", f"/api/elections/{open_election['id']}/candidates")
    assert status == 200, f"Expected 200, got {status}: {candidates}"
    print(f"  --> PASS: Found {len(candidates)} candidates:")
    for c in candidates:
        party = c.get('party_name') or c.get('party_affiliation') or 'Independent'
        print(f"      - [{c.get('candidate_code')}] {c.get('full_name')} ({party})")
    assert len(candidates) > 0, "Expected at least one candidate"
    chosen_candidate = candidates[0]

    # 7. Check voter status
    print(f"\n[7] Checking voter status for {dyn_voter_code} in election {open_election['id']} ...")
    status, v_status = request("GET", f"/api/elections/{open_election['id']}/voter-status", token=active_voter_token)
    assert status == 200, f"Expected 200, got {status}: {v_status}"
    print(f"  --> PASS: is_eligible: {v_status.get('is_eligible')}, has_voted: {v_status.get('has_voted')}")
    assert v_status.get('has_voted') is False

    # 8. Cast vote
    print(f"\n[8] Casting vote for {chosen_candidate['full_name']} ...")
    idempotency_key = str(uuid.uuid4())
    vote_payload = {
        "election_id": open_election["id"],
        "candidate_id": chosen_candidate["id"]
    }
    status, receipt = request("POST", "/api/votes", data=vote_payload, token=active_voter_token, idempotency_key=idempotency_key)
    assert status == 201, f"Expected 201 Created, got {status}: {receipt}"
    assert receipt.get("receipt_hash"), "Missing receipt_hash in response"
    print(f"  --> PASS: Ballot cast successfully! Receipt: {receipt.get('receipt_hash')[:24]}...")

    # 9. Test Atomic Duplicate Vote Prevention
    print("\n[9] Testing duplicate vote rejection (same voter, same election) ...")
    diff_key = str(uuid.uuid4())
    status, dup_res = request("POST", "/api/votes", data=vote_payload, token=active_voter_token, idempotency_key=diff_key)
    assert status == 409, f"Expected 409 Conflict, got {status}: {dup_res}"
    print(f"  --> PASS: Duplicate vote blocked correctly! Error: {dup_res.get('detail')}")

    # 10. Test Idempotency Key Replay
    print("\n[10] Testing idempotency key replay with original key ...")
    status, replay_res = request("POST", "/api/votes", data=vote_payload, token=active_voter_token, idempotency_key=idempotency_key)
    assert status == 200 or status == 201, f"Expected 200/201, got {status}: {replay_res}"
    assert replay_res.get("receipt_hash") == receipt.get("receipt_hash")
    print(f"  --> PASS: Idempotent replay returned matching receipt {replay_res.get('receipt_hash')[:24]}...")


    # 10. Audit Log Retrieval
    print("\n[10] Retrieving audit logs (Admin only) ...")
    status, logs = request("GET", "/api/audit-logs", token=admin_token)
    assert status == 200, f"Expected 200, got {status}: {logs}"
    print(f"  --> PASS: Retrieved {len(logs)} audit log entries. Most recent:")
    for log in logs[:3]:
        print(f"      - [{log.get('created_at')}] {log.get('action')} by {log.get('actor_type')}: {log.get('metadata')}")

    # 11. Full Election Lifecycle Test (Draft -> Scheduled -> Open -> Closed -> Published)
    print("\n[11] Testing Full Election Lifecycle Management (Admin) ...")
    new_election = {
        "title": "Automated Test Presidential Election 2026",
        "description": "Lifecycle validation election generated by E2E test suite",
        "start_time": "2026-10-18T00:00:00Z",
        "end_time": "2026-10-19T00:00:00Z"
    }
    status, created_el = request("POST", "/api/elections", data=new_election, token=admin_token)
    assert status == 201, f"Expected 201 Created, got {status}: {created_el}"
    new_el_id = created_el["id"]
    print(f"  --> Step 11a: Created election in DRAFT state: {new_el_id}")

    # Transition to SCHEDULED
    status, t1 = request("PUT", f"/api/elections/{new_el_id}/status", data={"status": "SCHEDULED"}, token=admin_token)
    assert status == 200 and t1.get("status") == "SCHEDULED", f"Failed transition to SCHEDULED: {t1}"
    print(f"  --> Step 11b: Transitioned DRAFT -> SCHEDULED")

    # Transition to OPEN
    status, t2 = request("PUT", f"/api/elections/{new_el_id}/status", data={"status": "OPEN"}, token=admin_token)
    assert status == 200 and t2.get("status") == "OPEN", f"Failed transition to OPEN: {t2}"
    print(f"  --> Step 11c: Transitioned SCHEDULED -> OPEN")

    # Transition to CLOSED
    status, t3 = request("PUT", f"/api/elections/{new_el_id}/status", data={"status": "CLOSED"}, token=admin_token)
    assert status == 200 and t3.get("status") == "CLOSED", f"Failed transition to CLOSED: {t3}"
    print(f"  --> Step 11d: Transitioned OPEN -> CLOSED")

    # Transition to RESULTS_PUBLISHED
    status, t4 = request("PUT", f"/api/elections/{new_el_id}/status", data={"status": "RESULTS_PUBLISHED"}, token=admin_token)
    assert status == 200 and t4.get("status") == "RESULTS_PUBLISHED", f"Failed transition to RESULTS_PUBLISHED: {t4}"
    print(f"  --> Step 11e: Transitioned CLOSED -> RESULTS_PUBLISHED")

    # Fetch Results
    status, results = request("GET", f"/api/elections/{new_el_id}/results")
    assert status == 200, f"Failed to get results: {results}"
    print(f"  --> Step 11f: Retrieved certified results for published election (Total votes: {results.get('total_votes')})")

    print("\n" + "=" * 60)
    print("   ALL 11 END-TO-END VERIFICATION CHECKS PASSED (100% GREEN)")
    print("=" * 60)

if __name__ == "__main__":
    main()
