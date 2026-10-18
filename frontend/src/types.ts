export type ElectionStatus = 'DRAFT' | 'SCHEDULED' | 'OPEN' | 'CLOSED' | 'RESULTS_PUBLISHED';

export interface Candidate {
  id: string;
  candidate_code: string;
  full_name: string;
  party_name: string;
  party_symbol: string;
  biography?: string;
  created_at?: string;
}

export interface Election {
  id: string;
  title: string;
  description: string;
  status: ElectionStatus;
  start_time: string;
  end_time: string;
  created_at?: string;
  candidates: Candidate[];
  has_voted?: boolean;
}

export interface Voter {
  id: string;
  voter_id: string;
  full_name: string;
  email: string;
  is_active: boolean;
  created_at?: string;
}

export interface BallotSubmission {
  election_id: string;
  candidate_id: string;
  idempotency_key?: string;
}

export interface BallotReceipt {
  receipt_id: string;
  election_id: string;
  voter_id: string;
  receipt_hash: string;
  cast_at: string;
  status: string;
}

export interface CandidateTally {
  candidate_id: string;
  candidate_code: string;
  full_name: string;
  party_name: string;
  party_symbol: string;
  vote_count: number;
  percentage: number;
}

export interface ElectionResult {
  election_id: string;
  title: string;
  status: string;
  total_votes: number;
  total_eligible_voters: number;
  turnout_percentage: number;
  tallies: CandidateTally[];
  certified_at: string;
}

export interface AuditLogEntry {
  id: string;
  action: string;
  actor_type: string;
  actor_id: string;
  ip_address: string;
  metadata: any;
  created_at: string;
}

export interface UserSession {
  token: string;
  user: {
    id: string;
    username?: string;
    voter_id?: string;
    full_name: string;
    email?: string;
    role: 'ADMIN' | 'VOTER';
  };
}
