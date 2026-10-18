-- ==============================================================================
-- Secure Ballot Management Platform: Relational Database Schema
-- Target: PostgreSQL 14+ (Tested on PostgreSQL 18.1)
-- ==============================================================================

-- Enable standard pgcrypto extension for gen_random_uuid() if required
CREATE EXTENSION IF NOT EXISTS "pgcrypto";

-- Clean schema teardown for repeatable migrations (development/testing)
DROP TRIGGER IF EXISTS trg_prevent_vote_tampering ON votes;
DROP FUNCTION IF EXISTS prevent_vote_tampering();
DROP TABLE IF EXISTS audit_logs CASCADE;
DROP TABLE IF EXISTS votes CASCADE;
DROP TABLE IF EXISTS ballot_receipts CASCADE;
DROP TABLE IF EXISTS election_voters CASCADE;
DROP TABLE IF EXISTS election_candidates CASCADE;
DROP TABLE IF EXISTS elections CASCADE;
DROP TABLE IF EXISTS candidates CASCADE;
DROP TABLE IF EXISTS voters CASCADE;
DROP TABLE IF EXISTS admins CASCADE;

-- ------------------------------------------------------------------------------
-- 1. Administrative Users
-- ------------------------------------------------------------------------------
CREATE TABLE admins (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(100) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_admins_username ON admins(username);

-- ------------------------------------------------------------------------------
-- 2. Registered Voters
-- ------------------------------------------------------------------------------
CREATE TABLE voters (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    voter_id VARCHAR(50) UNIQUE NOT NULL,
    full_name VARCHAR(100) NOT NULL,
    email VARCHAR(150) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_voters_voter_id ON voters(voter_id);
CREATE INDEX idx_voters_email ON voters(email);

-- ------------------------------------------------------------------------------
-- 3. Candidates Slate
-- ------------------------------------------------------------------------------
CREATE TABLE candidates (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    candidate_code VARCHAR(50) UNIQUE NOT NULL,
    full_name VARCHAR(100) NOT NULL,
    party_name VARCHAR(100) NOT NULL,
    party_symbol VARCHAR(50) NOT NULL,
    biography TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_candidates_code ON candidates(candidate_code);

-- ------------------------------------------------------------------------------
-- 4. Elections & State Machine
-- ------------------------------------------------------------------------------
CREATE TABLE elections (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    title VARCHAR(200) NOT NULL,
    description TEXT,
    status VARCHAR(30) NOT NULL DEFAULT 'DRAFT'
        CHECK (status IN ('DRAFT', 'SCHEDULED', 'OPEN', 'CLOSED', 'RESULTS_PUBLISHED')),
    start_time TIMESTAMPTZ NOT NULL,
    end_time TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT check_election_dates CHECK (end_time > start_time)
);

CREATE INDEX idx_elections_status ON elections(status);
CREATE INDEX idx_elections_window ON elections(start_time, end_time);

-- ------------------------------------------------------------------------------
-- 5. Candidate Assignment to Elections
-- ------------------------------------------------------------------------------
CREATE TABLE election_candidates (
    election_id UUID NOT NULL REFERENCES elections(id) ON DELETE CASCADE,
    candidate_id UUID NOT NULL REFERENCES candidates(id) ON DELETE CASCADE,
    assigned_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (election_id, candidate_id)
);

-- ------------------------------------------------------------------------------
-- 6. Voter Eligibility Roster per Election
-- ------------------------------------------------------------------------------
CREATE TABLE election_voters (
    election_id UUID NOT NULL REFERENCES elections(id) ON DELETE CASCADE,
    voter_id UUID NOT NULL REFERENCES voters(id) ON DELETE CASCADE,
    is_eligible BOOLEAN NOT NULL DEFAULT TRUE,
    PRIMARY KEY (election_id, voter_id)
);

-- ------------------------------------------------------------------------------
-- 7. Ballot Receipts (Tracks Voter Participation & Guarantees At-Most-Once Voting)
-- Notice: This table intentionally contains NO candidate choice column.
-- ------------------------------------------------------------------------------
CREATE TABLE ballot_receipts (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    election_id UUID NOT NULL REFERENCES elections(id) ON DELETE CASCADE,
    voter_id UUID NOT NULL REFERENCES voters(id) ON DELETE CASCADE,
    idempotency_key VARCHAR(100) NOT NULL,
    receipt_hash VARCHAR(64) UNIQUE NOT NULL,
    cast_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT unique_voter_election UNIQUE (election_id, voter_id),
    CONSTRAINT unique_voter_idempotency UNIQUE (voter_id, idempotency_key)
);

CREATE INDEX idx_ballot_receipts_election ON ballot_receipts(election_id);
CREATE INDEX idx_ballot_receipts_hash ON ballot_receipts(receipt_hash);

-- ------------------------------------------------------------------------------
-- 8. Votes (Anonymized Ballot Box)
-- Notice: This table intentionally contains NO voter_id column.
-- ------------------------------------------------------------------------------
CREATE TABLE votes (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    election_id UUID NOT NULL REFERENCES elections(id) ON DELETE CASCADE,
    candidate_id UUID NOT NULL REFERENCES candidates(id) ON DELETE RESTRICT,
    ballot_nonce VARCHAR(64) UNIQUE NOT NULL,
    cast_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_election_candidate
        FOREIGN KEY (election_id, candidate_id)
        REFERENCES election_candidates(election_id, candidate_id)
);

CREATE INDEX idx_votes_election_candidate ON votes(election_id, candidate_id);

-- Enforce Immutability on the Ballot Box via Trigger
CREATE OR REPLACE FUNCTION prevent_vote_tampering()
RETURNS TRIGGER AS $$
BEGIN
    RAISE EXCEPTION 'Votes table is append-only. Modification or deletion of ballots is strictly forbidden.';
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_prevent_vote_tampering
BEFORE UPDATE OR DELETE ON votes
FOR EACH ROW EXECUTE FUNCTION prevent_vote_tampering();

-- ------------------------------------------------------------------------------
-- 9. Immutable Administrative & Security Audit Trail
-- ------------------------------------------------------------------------------
CREATE TABLE audit_logs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    action VARCHAR(50) NOT NULL,
    actor_type VARCHAR(20) NOT NULL,
    actor_id VARCHAR(50) NOT NULL,
    ip_address VARCHAR(45),
    metadata JSONB,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_audit_logs_action ON audit_logs(action);
CREATE INDEX idx_audit_logs_created_at ON audit_logs(created_at);
