import type {
  Election,
  Candidate,
  Voter,
  BallotReceipt,
  ElectionResult,
  AuditLogEntry,
  UserSession,
  ElectionStatus
} from './types';

const API_BASE = '/api';

export class ApiError extends Error {
  status: number;
  title: string;
  detail: string;

  constructor(status: number, title: string, detail: string) {
    super(detail || title);
    this.name = 'ApiError';
    this.status = status;
    this.title = title;
    this.detail = detail;
  }
}

function getStoredToken(): string | null {
  return localStorage.getItem('aegis_token');
}

export function setStoredSession(session: UserSession | null) {
  if (session) {
    localStorage.setItem('aegis_token', session.token);
    localStorage.setItem('aegis_user', JSON.stringify(session.user));
  } else {
    localStorage.removeItem('aegis_token');
    localStorage.removeItem('aegis_user');
  }
}

export function getStoredSession(): UserSession | null {
  const token = getStoredToken();
  const userStr = localStorage.getItem('aegis_user');
  if (token && userStr) {
    try {
      const user = JSON.parse(userStr);
      return { token, user };
    } catch {
      return null;
    }
  }
  return null;
}

async function request<T>(endpoint: string, options: RequestInit = {}): Promise<T> {
  const token = getStoredToken();
  const headers: Record<string, string> = {
    'Content-Type': 'application/json',
    ...(options.headers as Record<string, string>),
  };

  if (token) {
    headers['Authorization'] = `Bearer ${token}`;
  }

  const response = await fetch(`${API_BASE}${endpoint}`, {
    ...options,
    headers,
  });

  if (response.status === 204) {
    return {} as T;
  }

  let data: any;
  try {
    data = await response.json();
  } catch {
    data = null;
  }

  if (!response.ok) {
    if (response.status === 401) {
      setStoredSession(null);
    }
    const title = data?.title || 'Request Failed';
    const detail = data?.detail || data?.message || response.statusText;
    throw new ApiError(response.status, title, detail);
  }

  return data as T;
}

export const api = {
  // Authentication
  async loginAdmin(username: string, password: string): Promise<UserSession> {
    const res = await request<UserSession>('/auth/admin/login', {
      method: 'POST',
      body: JSON.stringify({ username, password }),
    });
    setStoredSession(res);
    return res;
  },

  async loginVoter(voterId: string, password: string): Promise<UserSession> {
    const res = await request<UserSession>('/auth/voter/login', {
      method: 'POST',
      body: JSON.stringify({ voter_id: voterId, password }),
    });
    setStoredSession(res);
    return res;
  },

  logout() {
    setStoredSession(null);
  },

  // Elections
  async getElections(): Promise<Election[]> {
    return request<Election[]>('/elections');
  },

  async getElection(id: string): Promise<Election> {
    return request<Election>(`/elections/${id}`);
  },

  async createElection(election: { title: string; description: string; start_time: string; end_time: string }): Promise<{ id: string }> {
    return request<{ id: string }>('/elections', {
      method: 'POST',
      body: JSON.stringify(election),
    });
  },

  async updateElectionStatus(id: string, status: ElectionStatus): Promise<void> {
    await request<void>(`/elections/${id}/status`, {
      method: 'POST',
      body: JSON.stringify({ status }),
    });
  },

  async assignCandidate(electionId: string, candidateId: string): Promise<void> {
    await request<void>(`/elections/${electionId}/candidates`, {
      method: 'POST',
      body: JSON.stringify({ candidate_id: candidateId }),
    });
  },

  // Candidates
  async getCandidates(): Promise<Candidate[]> {
    return request<Candidate[]>('/candidates');
  },

  async createCandidate(candidate: { candidate_code: string; full_name: string; party_name: string; party_symbol: string; biography?: string }): Promise<{ id: string }> {
    return request<{ id: string }>('/candidates', {
      method: 'POST',
      body: JSON.stringify(candidate),
    });
  },

  // Voters
  async getVoters(): Promise<Voter[]> {
    return request<Voter[]>('/voters');
  },

  async createVoter(voter: { voter_id: string; full_name: string; email: string; password?: string }): Promise<{ id: string }> {
    return request<{ id: string }>('/voters', {
      method: 'POST',
      body: JSON.stringify(voter),
    });
  },

  // Voting
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
  },

  async getResults(electionId: string): Promise<ElectionResult> {
    return request<ElectionResult>(`/elections/${electionId}/results`);
  },

  // Audit Logs
  async getAuditLogs(): Promise<AuditLogEntry[]> {
    return request<AuditLogEntry[]>('/admin/audit-logs');
  },
};
