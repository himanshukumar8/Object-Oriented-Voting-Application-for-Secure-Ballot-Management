-- ==============================================================================
-- Secure Ballot Management Platform: Seed Synthetic Demo Data
-- Safe Synthetic Records for Demonstration, Testing & Verification
-- ==============================================================================

-- 1. Insert Default Administrator
-- Username: admin | Password: AdminSecure2026!
INSERT INTO admins (id, username, password_hash, full_name)
VALUES (
    'a0000000-0000-0000-0000-000000000001',
    'admin',
    'a1b2c3d4e5f67890123456789abcdef0$35c698266ef10f9d8f221761bfb5d53af3ddf7c5df0e33dcfe91eb22412d6e66',
    'Chief Election Commissioner'
) ON CONFLICT (username) DO NOTHING;

-- 2. Insert Synthetic Voters
-- Password for all test voters: VoterPass2026!
INSERT INTO voters (id, voter_id, full_name, email, password_hash, is_active)
VALUES
    ('b0000000-0000-0000-0000-000000000101', 'VOTER-101', 'Alex Chen', 'alex.chen@example.org', 'b2c3d4e5f67890123456789abcdef0a1$17e162fcaeec9f949b2f846c696bae9a45347ce4cb76e8a1dcb02a8d454bffa0', TRUE),
    ('b0000000-0000-0000-0000-000000000102', 'VOTER-102', 'Maya Patel', 'maya.patel@example.org', 'b2c3d4e5f67890123456789abcdef0a1$17e162fcaeec9f949b2f846c696bae9a45347ce4cb76e8a1dcb02a8d454bffa0', TRUE),
    ('b0000000-0000-0000-0000-000000000103', 'VOTER-103', 'Jordan Taylor', 'jordan.taylor@example.org', 'b2c3d4e5f67890123456789abcdef0a1$17e162fcaeec9f949b2f846c696bae9a45347ce4cb76e8a1dcb02a8d454bffa0', TRUE),
    ('b0000000-0000-0000-0000-000000000104', 'VOTER-104', 'Liam O''Connor', 'liam.oconnor@example.org', 'b2c3d4e5f67890123456789abcdef0a1$17e162fcaeec9f949b2f846c696bae9a45347ce4cb76e8a1dcb02a8d454bffa0', TRUE)
ON CONFLICT (voter_id) DO NOTHING;

-- 3. Insert Synthetic Candidates
INSERT INTO candidates (id, candidate_code, full_name, party_name, party_symbol, biography)
VALUES
    ('c0000000-0000-0000-0000-000000000001', 'CAND-001', 'Dr. Elena Vance', 'Progressive Tech Slate', 'Quantum Atom', 'Advancing transparent distributed governance, open data standards, and ethical digital systems.'),
    ('c0000000-0000-0000-0000-000000000002', 'CAND-002', 'Marcus Sterling', 'Civic Alliance', 'Golden Torch', 'Championing independent fiscal auditing, community infrastructure, and ballot secrecy.'),
    ('c0000000-0000-0000-0000-000000000003', 'CAND-003', 'Sarah Jenkins', 'Green Future Initiative', 'Evergreen Oak', 'Committed to sustainable public transit, renewable municipal power, and conservation reserves.')
ON CONFLICT (candidate_code) DO NOTHING;

-- 4. Insert Demo Elections
-- Election 1: Active OPEN election ready for voting
INSERT INTO elections (id, title, description, status, start_time, end_time)
VALUES (
    'e0000000-0000-0000-0000-000000000001',
    '2026 General Council Election',
    'Annual democratic election to select representatives for the General Governance Council.',
    'OPEN',
    CURRENT_TIMESTAMP - INTERVAL '1 day',
    CURRENT_TIMESTAMP + INTERVAL '7 days'
) ON CONFLICT (id) DO NOTHING;

-- Election 2: SCHEDULED upcoming election
INSERT INTO elections (id, title, description, status, start_time, end_time)
VALUES (
    'e0000000-0000-0000-0000-000000000002',
    'Technology Advisory Board 2026',
    'Upcoming election for senior technical oversight and architecture governance board.',
    'SCHEDULED',
    CURRENT_TIMESTAMP + INTERVAL '2 days',
    CURRENT_TIMESTAMP + INTERVAL '9 days'
) ON CONFLICT (id) DO NOTHING;

-- Election 3: RESULTS_PUBLISHED closed election with certified results
INSERT INTO elections (id, title, description, status, start_time, end_time)
VALUES (
    'e0000000-0000-0000-0000-000000000003',
    '2025 Special Constitutional Ballot',
    'Ratified charter amendments regarding digital privacy and institutional voting rights.',
    'RESULTS_PUBLISHED',
    CURRENT_TIMESTAMP - INTERVAL '30 days',
    CURRENT_TIMESTAMP - INTERVAL '23 days'
) ON CONFLICT (id) DO NOTHING;

-- 5. Assign Candidates to Elections
-- Election 1 has CAND-001, CAND-002, CAND-003
INSERT INTO election_candidates (election_id, candidate_id)
VALUES
    ('e0000000-0000-0000-0000-000000000001', 'c0000000-0000-0000-0000-000000000001'),
    ('e0000000-0000-0000-0000-000000000001', 'c0000000-0000-0000-0000-000000000002'),
    ('e0000000-0000-0000-0000-000000000001', 'c0000000-0000-0000-0000-000000000003'),
-- Election 2 has CAND-001, CAND-002
    ('e0000000-0000-0000-0000-000000000002', 'c0000000-0000-0000-0000-000000000001'),
    ('e0000000-0000-0000-0000-000000000002', 'c0000000-0000-0000-0000-000000000002'),
-- Election 3 has CAND-002, CAND-003
    ('e0000000-0000-0000-0000-000000000003', 'c0000000-0000-0000-0000-000000000002'),
    ('e0000000-0000-0000-0000-000000000003', 'c0000000-0000-0000-0000-000000000003')
ON CONFLICT DO NOTHING;

-- 6. Assign Voter Eligibility
INSERT INTO election_voters (election_id, voter_id, is_eligible)
SELECT 'e0000000-0000-0000-0000-000000000001', id, TRUE FROM voters
ON CONFLICT DO NOTHING;

INSERT INTO election_voters (election_id, voter_id, is_eligible)
SELECT 'e0000000-0000-0000-0000-000000000002', id, TRUE FROM voters
ON CONFLICT DO NOTHING;

INSERT INTO election_voters (election_id, voter_id, is_eligible)
SELECT 'e0000000-0000-0000-0000-000000000003', id, TRUE FROM voters
ON CONFLICT DO NOTHING;

-- 7. Seed Past Certified Ballots for Election 3 (Published Results)
-- Notice: ballot_receipts records who voted; votes records the candidate selections anonymously.
INSERT INTO ballot_receipts (election_id, voter_id, idempotency_key, receipt_hash, cast_at)
VALUES
    ('e0000000-0000-0000-0000-000000000003', 'b0000000-0000-0000-0000-000000000101', 'seed-idemp-01', 'a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f60001', CURRENT_TIMESTAMP - INTERVAL '25 days'),
    ('e0000000-0000-0000-0000-000000000003', 'b0000000-0000-0000-0000-000000000102', 'seed-idemp-02', 'b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6a10002', CURRENT_TIMESTAMP - INTERVAL '25 days'),
    ('e0000000-0000-0000-0000-000000000003', 'b0000000-0000-0000-0000-000000000103', 'seed-idemp-03', 'c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6a1b20003', CURRENT_TIMESTAMP - INTERVAL '24 days')
ON CONFLICT DO NOTHING;

INSERT INTO votes (election_id, candidate_id, ballot_nonce, cast_at)
VALUES
    ('e0000000-0000-0000-0000-000000000003', 'c0000000-0000-0000-0000-000000000002', 'nonce-past-001', CURRENT_TIMESTAMP - INTERVAL '25 days'),
    ('e0000000-0000-0000-0000-000000000003', 'c0000000-0000-0000-0000-000000000002', 'nonce-past-002', CURRENT_TIMESTAMP - INTERVAL '25 days'),
    ('e0000000-0000-0000-0000-000000000003', 'c0000000-0000-0000-0000-000000000003', 'nonce-past-003', CURRENT_TIMESTAMP - INTERVAL '24 days')
ON CONFLICT DO NOTHING;

-- 8. Record Initial Audit Log
INSERT INTO audit_logs (action, actor_type, actor_id, ip_address, metadata)
VALUES (
    'SYSTEM_INITIALIZED',
    'SYSTEM',
    'system',
    '127.0.0.1',
    '{"message": "Database initialized with secure synthetic seed data", "elections_created": 3}'
);
