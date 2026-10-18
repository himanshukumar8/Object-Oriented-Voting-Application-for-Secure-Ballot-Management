import React, { useState, useEffect } from 'react';
import type { Election, Candidate, Voter, AuditLogEntry, ElectionStatus } from '../types';
import { api } from '../api';
import {
  Vote,
  Users,
  Award,
  PlusCircle,
  Play,
  StopCircle,
  FileCheck,
  ShieldAlert,
  Calendar,
  Layers
} from 'lucide-react';

interface AdminDashboardProps {
  elections: Election[];
  onRefresh: () => void;
  showToast: (msg: string, type: 'success' | 'error' | 'info') => void;
}

export const AdminDashboard: React.FC<AdminDashboardProps> = ({
  elections,
  onRefresh,
  showToast,
}) => {
  const [activeTab, setActiveTab] = useState<'elections' | 'candidates' | 'voters' | 'results' | 'audit'>('elections');
  
  // Data states
  const [candidates, setCandidates] = useState<Candidate[]>([]);
  const [voters, setVoters] = useState<Voter[]>([]);
  const [auditLogs, setAuditLogs] = useState<AuditLogEntry[]>([]);
  
  // Modals & form states
  const [isNewElectionOpen, setIsNewElectionOpen] = useState(false);
  const [isAssignModalOpen, setIsAssignModalOpen] = useState(false);
  const [targetElectionForAssign, setTargetElectionForAssign] = useState<Election | null>(null);
  const [candidateToAssign, setCandidateToAssign] = useState('');

  // Form inputs for new election
  const [newElectionTitle, setNewElectionTitle] = useState('');
  const [newElectionDesc, setNewElectionDesc] = useState('');
  const [newElectionStart, setNewElectionStart] = useState('');
  const [newElectionEnd, setNewElectionEnd] = useState('');

  // Form inputs for new candidate
  const [newCandCode, setNewCandCode] = useState('');
  const [newCandName, setNewCandName] = useState('');
  const [newCandParty, setNewCandParty] = useState('');
  const [newCandSymbol, setNewCandSymbol] = useState('');
  const [newCandBio, setNewCandBio] = useState('');

  // Form inputs for new voter
  const [newVoterId, setNewVoterId] = useState('');
  const [newVoterName, setNewVoterName] = useState('');
  const [newVoterEmail, setNewVoterEmail] = useState('');

  const loadCandidates = async () => {
    try {
      const data = await api.getCandidates();
      setCandidates(data);
    } catch (err: any) {
      console.error(err);
    }
  };

  const loadVoters = async () => {
    try {
      const data = await api.getVoters();
      setVoters(data);
    } catch (err: any) {
      console.error(err);
    }
  };

  const loadAuditLogs = async () => {
    try {
      const data = await api.getAuditLogs();
      setAuditLogs(data);
    } catch (err: any) {
      console.error(err);
    }
  };

  useEffect(() => {
    let active = true;
    api.getCandidates().then(data => { if (active) setCandidates(data); }).catch(console.error);
    api.getVoters().then(data => { if (active) setVoters(data); }).catch(console.error);
    api.getAuditLogs().then(data => { if (active) setAuditLogs(data); }).catch(console.error);
    return () => {
      active = false;
    };
  }, [activeTab]);

  // State Transition Action
  const handleTransition = async (electionId: string, nextStatus: ElectionStatus) => {
    try {
      await api.updateElectionStatus(electionId, nextStatus);
      showToast(`Election transitioned to ${nextStatus}`, 'success');
      onRefresh();
      loadAuditLogs();
    } catch (err: any) {
      showToast(err.detail || 'State transition rejected', 'error');
    }
  };

  // Create Election Submit
  const handleCreateElection = async (e: React.FormEvent) => {
    e.preventDefault();
    try {
      const startTime = newElectionStart ? new Date(newElectionStart).toISOString() : new Date().toISOString();
      const endTime = newElectionEnd ? new Date(newElectionEnd).toISOString() : new Date(Date.now() + 86400000 * 7).toISOString();

      await api.createElection({
        title: newElectionTitle,
        description: newElectionDesc,
        start_time: startTime,
        end_time: endTime,
      });

      showToast('New election created successfully', 'success');
      setIsNewElectionOpen(false);
      setNewElectionTitle('');
      setNewElectionDesc('');
      onRefresh();
      loadAuditLogs();
    } catch (err: any) {
      showToast(err.detail || 'Failed to create election', 'error');
    }
  };

  // Assign Candidate Submit
  const handleAssignCandidate = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!targetElectionForAssign || !candidateToAssign) return;

    try {
      await api.assignCandidate(targetElectionForAssign.id, candidateToAssign);
      showToast('Candidate assigned to election slate', 'success');
      setIsAssignModalOpen(false);
      setCandidateToAssign('');
      onRefresh();
      loadAuditLogs();
    } catch (err: any) {
      showToast(err.detail || 'Failed to assign candidate', 'error');
    }
  };

  // Create Candidate Submit
  const handleCreateCandidate = async (e: React.FormEvent) => {
    e.preventDefault();
    try {
      await api.createCandidate({
        candidate_code: newCandCode,
        full_name: newCandName,
        party_name: newCandParty,
        party_symbol: newCandSymbol,
        biography: newCandBio,
      });
      showToast('Candidate enrolled successfully', 'success');
      setNewCandCode('');
      setNewCandName('');
      setNewCandParty('');
      setNewCandSymbol('');
      setNewCandBio('');
      loadCandidates();
      loadAuditLogs();
    } catch (err: any) {
      showToast(err.detail || 'Failed to enroll candidate', 'error');
    }
  };

  // Register Voter Submit
  const handleCreateVoter = async (e: React.FormEvent) => {
    e.preventDefault();
    try {
      await api.createVoter({
        voter_id: newVoterId,
        full_name: newVoterName,
        email: newVoterEmail,
        password: 'VoterPass2026!',
      });
      showToast('Voter registered successfully with default credentials', 'success');
      setNewVoterId('');
      setNewVoterName('');
      setNewVoterEmail('');
      loadVoters();
      loadAuditLogs();
    } catch (err: any) {
      showToast(err.detail || 'Failed to register voter', 'error');
    }
  };

  return (
    <div id="admin-dashboard-view">
      {/* Metrics Row */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '1.25rem', marginBottom: '2rem' }}>
        <div className="glass-card" style={{ display: 'flex', alignItems: 'center', gap: '1rem' }}>
          <div style={{ padding: '0.85rem', borderRadius: 'var(--radius-md)', background: 'rgba(99, 102, 241, 0.15)', color: 'var(--primary)' }}>
            <Vote size={26} />
          </div>
          <div>
            <div style={{ fontSize: '0.75rem', color: 'var(--text-muted)', textTransform: 'uppercase', fontWeight: 600 }}>Total Elections</div>
            <div style={{ fontSize: '1.6rem', fontWeight: 800 }}>{elections.length}</div>
          </div>
        </div>

        <div className="glass-card" style={{ display: 'flex', alignItems: 'center', gap: '1rem' }}>
          <div style={{ padding: '0.85rem', borderRadius: 'var(--radius-md)', background: 'var(--success-bg)', color: 'var(--success)' }}>
            <Play size={26} />
          </div>
          <div>
            <div style={{ fontSize: '0.75rem', color: 'var(--text-muted)', textTransform: 'uppercase', fontWeight: 600 }}>Open Elections</div>
            <div style={{ fontSize: '1.6rem', fontWeight: 800, color: 'var(--success)' }}>
              {elections.filter(e => e.status === 'OPEN').length}
            </div>
          </div>
        </div>

        <div className="glass-card" style={{ display: 'flex', alignItems: 'center', gap: '1rem' }}>
          <div style={{ padding: '0.85rem', borderRadius: 'var(--radius-md)', background: 'rgba(245, 158, 11, 0.15)', color: 'var(--warning)' }}>
            <Award size={26} />
          </div>
          <div>
            <div style={{ fontSize: '0.75rem', color: 'var(--text-muted)', textTransform: 'uppercase', fontWeight: 600 }}>Candidates</div>
            <div style={{ fontSize: '1.6rem', fontWeight: 800 }}>{candidates.length}</div>
          </div>
        </div>

        <div className="glass-card" style={{ display: 'flex', alignItems: 'center', gap: '1rem' }}>
          <div style={{ padding: '0.85rem', borderRadius: 'var(--radius-md)', background: 'rgba(6, 182, 212, 0.15)', color: 'var(--accent-cyan)' }}>
            <Users size={26} />
          </div>
          <div>
            <div style={{ fontSize: '0.75rem', color: 'var(--text-muted)', textTransform: 'uppercase', fontWeight: 600 }}>Registered Voters</div>
            <div style={{ fontSize: '1.6rem', fontWeight: 800 }}>{voters.length}</div>
          </div>
        </div>
      </div>

      {/* Navigation Tabs */}
      <div style={{ display: 'flex', gap: '0.5rem', borderBottom: '1px solid var(--border-subtle)', marginBottom: '1.75rem', paddingBottom: '0.5rem' }}>
        <button
          id="admin-tab-elections"
          className={`btn ${activeTab === 'elections' ? 'btn-primary' : 'btn-secondary'}`}
          style={{ padding: '0.5rem 1rem', fontSize: '0.85rem' }}
          onClick={() => setActiveTab('elections')}
        >
          <Layers size={16} /> Elections & Lifecycle
        </button>
        <button
          id="admin-tab-candidates"
          className={`btn ${activeTab === 'candidates' ? 'btn-primary' : 'btn-secondary'}`}
          style={{ padding: '0.5rem 1rem', fontSize: '0.85rem' }}
          onClick={() => setActiveTab('candidates')}
        >
          <Award size={16} /> Candidates Roster
        </button>
        <button
          id="admin-tab-voters"
          className={`btn ${activeTab === 'voters' ? 'btn-primary' : 'btn-secondary'}`}
          style={{ padding: '0.5rem 1rem', fontSize: '0.85rem' }}
          onClick={() => setActiveTab('voters')}
        >
          <Users size={16} /> Voter Registry
        </button>
        <button
          id="admin-tab-audit"
          className={`btn ${activeTab === 'audit' ? 'btn-primary' : 'btn-secondary'}`}
          style={{ padding: '0.5rem 1rem', fontSize: '0.85rem' }}
          onClick={() => setActiveTab('audit')}
        >
          <ShieldAlert size={16} /> Security Audit Trail
        </button>
      </div>

      {/* TAB 1: ELECTIONS & LIFECYCLE */}
      {activeTab === 'elections' && (
        <div>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '1.25rem' }}>
            <h2 style={{ fontSize: '1.35rem' }}>Elections Control & Deterministic Lifecycle</h2>
            <button
              id="create-election-open-btn"
              className="btn btn-primary"
              onClick={() => setIsNewElectionOpen(true)}
            >
              <PlusCircle size={16} /> Create New Election
            </button>
          </div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '1rem' }}>
            {elections.map((election) => (
              <div key={election.id} className="glass-card" id={`admin-election-${election.id}`}>
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '1rem' }}>
                  <div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem', marginBottom: '0.35rem' }}>
                      <span className={`badge badge-${election.status.toLowerCase().replace('_', '-')}`}>
                        {election.status}
                      </span>
                      <span style={{ fontSize: '0.75rem', color: 'var(--text-dim)', fontFamily: 'var(--font-mono)' }}>
                        {election.id}
                      </span>
                    </div>
                    <h3 style={{ fontSize: '1.2rem', marginBottom: '0.35rem' }}>{election.title}</h3>
                    <p style={{ fontSize: '0.85rem', color: 'var(--text-muted)' }}>{election.description}</p>
                  </div>

                  {/* Lifecycle State Transitions Buttons */}
                  <div style={{ display: 'flex', gap: '0.5rem', flexWrap: 'wrap', justifyContent: 'flex-end' }}>
                    {election.status === 'DRAFT' && (
                      <button
                        id={`transition-sched-btn-${election.id}`}
                        className="btn btn-secondary"
                        style={{ padding: '0.45rem 0.85rem', fontSize: '0.8rem', color: 'var(--warning)', borderColor: 'var(--warning-border)' }}
                        onClick={() => handleTransition(election.id, 'SCHEDULED')}
                      >
                        <Calendar size={14} /> Publish Schedule
                      </button>
                    )}

                    {(election.status === 'DRAFT' || election.status === 'SCHEDULED') && (
                      <button
                        id={`transition-open-btn-${election.id}`}
                        className="btn btn-success"
                        style={{ padding: '0.45rem 0.85rem', fontSize: '0.8rem' }}
                        onClick={() => handleTransition(election.id, 'OPEN')}
                      >
                        <Play size={14} /> Open Polling
                      </button>
                    )}

                    {election.status === 'OPEN' && (
                      <button
                        id={`transition-close-btn-${election.id}`}
                        className="btn btn-danger"
                        style={{ padding: '0.45rem 0.85rem', fontSize: '0.8rem' }}
                        onClick={() => handleTransition(election.id, 'CLOSED')}
                      >
                        <StopCircle size={14} /> Close Polling
                      </button>
                    )}

                    {election.status === 'CLOSED' && (
                      <button
                        id={`transition-publish-btn-${election.id}`}
                        className="btn btn-primary"
                        style={{ padding: '0.45rem 0.85rem', fontSize: '0.8rem' }}
                        onClick={() => handleTransition(election.id, 'RESULTS_PUBLISHED')}
                      >
                        <FileCheck size={14} /> Certify & Publish Results
                      </button>
                    )}

                    {/* Candidate Assignment trigger (allowed in DRAFT/SCHEDULED) */}
                    {(election.status === 'DRAFT' || election.status === 'SCHEDULED') && (
                      <button
                        id={`assign-candidates-trigger-${election.id}`}
                        className="btn btn-secondary"
                        style={{ padding: '0.45rem 0.85rem', fontSize: '0.8rem' }}
                        onClick={() => {
                          setTargetElectionForAssign(election);
                          setIsAssignModalOpen(true);
                        }}
                      >
                        <PlusCircle size={14} /> Assign Candidates
                      </button>
                    )}
                  </div>
                </div>

                {/* Candidate Slate for this election */}
                <div style={{ borderTop: '1px solid var(--border-subtle)', paddingTop: '0.75rem', marginTop: '0.5rem' }}>
                  <div style={{ fontSize: '0.8rem', fontWeight: 600, color: 'var(--text-muted)', marginBottom: '0.5rem' }}>
                    Assigned Candidate Slate ({election.candidates.length}):
                  </div>
                  {election.candidates.length === 0 ? (
                    <span style={{ fontSize: '0.75rem', color: 'var(--text-dim)', fontStyle: 'italic' }}>
                      No candidates assigned yet.
                    </span>
                  ) : (
                    <div style={{ display: 'flex', gap: '0.5rem', flexWrap: 'wrap' }}>
                      {election.candidates.map((c) => (
                        <div
                          key={c.id}
                          style={{
                            background: 'rgba(255, 255, 255, 0.03)',
                            border: '1px solid var(--border-subtle)',
                            padding: '0.35rem 0.65rem',
                            borderRadius: 'var(--radius-sm)',
                            fontSize: '0.75rem',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '0.4rem',
                          }}
                        >
                          <strong style={{ color: 'var(--accent-cyan)' }}>{c.candidate_code}</strong>
                          <span>{c.full_name}</span>
                          <span style={{ color: 'var(--text-dim)' }}>({c.party_symbol})</span>
                        </div>
                      ))}
                    </div>
                  )}
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* TAB 2: CANDIDATES MANAGEMENT */}
      {activeTab === 'candidates' && (
        <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr', gap: '2rem' }}>
          <div>
            <h2 style={{ fontSize: '1.35rem', marginBottom: '1.25rem' }}>Enrolled Candidates Directory</h2>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '1rem' }}>
              {candidates.map((c) => (
                <div key={c.id} className="glass-card" style={{ padding: '1.25rem' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '0.35rem' }}>
                    <h3 style={{ fontSize: '1.15rem' }}>{c.full_name}</h3>
                    <span className="badge badge-scheduled">{c.candidate_code}</span>
                  </div>
                  <div style={{ fontSize: '0.85rem', color: 'var(--accent-cyan)', fontWeight: 600, marginBottom: '0.5rem' }}>
                    {c.party_name} • Symbol: {c.party_symbol}
                  </div>
                  <p style={{ fontSize: '0.8rem', color: 'var(--text-muted)' }}>{c.biography}</p>
                </div>
              ))}
            </div>
          </div>

          {/* New Candidate Form */}
          <div className="glass-panel" style={{ padding: '1.5rem', height: 'fit-content' }}>
            <h3 style={{ fontSize: '1.15rem', marginBottom: '1.25rem' }}>Enroll New Candidate</h3>
            <form onSubmit={handleCreateCandidate}>
              <div className="form-group">
                <label className="form-label">Candidate Code</label>
                <input
                  id="cand-code-input"
                  className="form-input"
                  placeholder="e.g. CAND-105"
                  value={newCandCode}
                  onChange={(e) => setNewCandCode(e.target.value)}
                  required
                />
              </div>
              <div className="form-group">
                <label className="form-label">Full Name</label>
                <input
                  id="cand-name-input"
                  className="form-input"
                  placeholder="Full candidate name"
                  value={newCandName}
                  onChange={(e) => setNewCandName(e.target.value)}
                  required
                />
              </div>
              <div className="form-group">
                <label className="form-label">Party Name</label>
                <input
                  id="cand-party-input"
                  className="form-input"
                  placeholder="e.g. Civic Coalition"
                  value={newCandParty}
                  onChange={(e) => setNewCandParty(e.target.value)}
                  required
                />
              </div>
              <div className="form-group">
                <label className="form-label">Party Symbol / Icon</label>
                <input
                  id="cand-symbol-input"
                  className="form-input"
                  placeholder="e.g. Rising Sun"
                  value={newCandSymbol}
                  onChange={(e) => setNewCandSymbol(e.target.value)}
                  required
                />
              </div>
              <div className="form-group">
                <label className="form-label">Biography</label>
                <textarea
                  id="cand-bio-input"
                  className="form-input"
                  rows={3}
                  placeholder="Platform manifesto and bio"
                  value={newCandBio}
                  onChange={(e) => setNewCandBio(e.target.value)}
                />
              </div>
              <button id="submit-candidate-btn" type="submit" className="btn btn-primary" style={{ width: '100%' }}>
                Enroll Candidate
              </button>
            </form>
          </div>
        </div>
      )}

      {/* TAB 3: VOTER REGISTRY */}
      {activeTab === 'voters' && (
        <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr', gap: '2rem' }}>
          <div>
            <h2 style={{ fontSize: '1.35rem', marginBottom: '1.25rem' }}>Registered Voters Ledger</h2>
            <div className="glass-panel" style={{ overflow: 'hidden' }}>
              <table style={{ width: '100%', borderCollapse: 'collapse', textAlign: 'left', fontSize: '0.85rem' }}>
                <thead>
                  <tr style={{ background: 'rgba(0,0,0,0.3)', borderBottom: '1px solid var(--border-subtle)', color: 'var(--text-muted)' }}>
                    <th style={{ padding: '0.85rem 1rem' }}>Voter ID</th>
                    <th style={{ padding: '0.85rem 1rem' }}>Full Name</th>
                    <th style={{ padding: '0.85rem 1rem' }}>Email</th>
                    <th style={{ padding: '0.85rem 1rem' }}>Status</th>
                  </tr>
                </thead>
                <tbody>
                  {voters.map((v) => (
                    <tr key={v.id} style={{ borderBottom: '1px solid var(--border-subtle)' }}>
                      <td style={{ padding: '0.85rem 1rem', fontFamily: 'var(--font-mono)', fontWeight: 600 }}>{v.voter_id}</td>
                      <td style={{ padding: '0.85rem 1rem', fontWeight: 600 }}>{v.full_name}</td>
                      <td style={{ padding: '0.85rem 1rem', color: 'var(--text-muted)' }}>{v.email}</td>
                      <td style={{ padding: '0.85rem 1rem' }}>
                        <span className="badge badge-open" style={{ fontSize: '0.65rem' }}>Active</span>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>

          {/* New Voter Registration Form */}
          <div className="glass-panel" style={{ padding: '1.5rem', height: 'fit-content' }}>
            <h3 style={{ fontSize: '1.15rem', marginBottom: '1.25rem' }}>Register New Voter</h3>
            <form onSubmit={handleCreateVoter}>
              <div className="form-group">
                <label className="form-label">Voter Identification Code</label>
                <input
                  id="reg-voter-id-input"
                  className="form-input"
                  placeholder="e.g. VOTER-205"
                  value={newVoterId}
                  onChange={(e) => setNewVoterId(e.target.value)}
                  required
                />
              </div>
              <div className="form-group">
                <label className="form-label">Full Name</label>
                <input
                  id="reg-voter-name-input"
                  className="form-input"
                  placeholder="Full legal name"
                  value={newVoterName}
                  onChange={(e) => setNewVoterName(e.target.value)}
                  required
                />
              </div>
              <div className="form-group">
                <label className="form-label">Email Address</label>
                <input
                  id="reg-voter-email-input"
                  className="form-input"
                  type="email"
                  placeholder="user@example.org"
                  value={newVoterEmail}
                  onChange={(e) => setNewVoterEmail(e.target.value)}
                  required
                />
              </div>
              <div style={{ fontSize: '0.75rem', color: 'var(--text-dim)', marginBottom: '1.25rem' }}>
                Note: Initial credentials default to <code>VoterPass2026!</code> and will be hashed via PBKDF2-HMAC-SHA256 upon submission.
              </div>
              <button id="submit-voter-btn" type="submit" className="btn btn-primary" style={{ width: '100%' }}>
                Register Voter
              </button>
            </form>
          </div>
        </div>
      )}

      {/* TAB 4: AUDIT TRAIL */}
      {activeTab === 'audit' && (
        <div>
          <h2 style={{ fontSize: '1.35rem', marginBottom: '1.25rem' }}>Security & Administrative Audit Log</h2>
          <div className="glass-panel" style={{ overflow: 'hidden' }}>
            <table style={{ width: '100%', borderCollapse: 'collapse', textAlign: 'left', fontSize: '0.8rem' }}>
              <thead>
                <tr style={{ background: 'rgba(0,0,0,0.3)', borderBottom: '1px solid var(--border-subtle)', color: 'var(--text-muted)' }}>
                  <th style={{ padding: '0.75rem 1rem' }}>Timestamp</th>
                  <th style={{ padding: '0.75rem 1rem' }}>Action</th>
                  <th style={{ padding: '0.75rem 1rem' }}>Actor Type</th>
                  <th style={{ padding: '0.75rem 1rem' }}>Actor ID</th>
                  <th style={{ padding: '0.75rem 1rem' }}>Details / Metadata</th>
                </tr>
              </thead>
              <tbody>
                {auditLogs.map((log) => (
                  <tr key={log.id} style={{ borderBottom: '1px solid var(--border-subtle)' }}>
                    <td style={{ padding: '0.75rem 1rem', color: 'var(--text-muted)', whiteSpace: 'nowrap' }}>
                      {new Date(log.created_at).toLocaleString()}
                    </td>
                    <td style={{ padding: '0.75rem 1rem' }}>
                      <span className="badge badge-published" style={{ fontSize: '0.7rem' }}>
                        {log.action}
                      </span>
                    </td>
                    <td style={{ padding: '0.75rem 1rem', fontWeight: 600 }}>{log.actor_type}</td>
                    <td style={{ padding: '0.75rem 1rem', fontFamily: 'var(--font-mono)' }}>
                      {log.actor_id.substring(0, 12)}...
                    </td>
                    <td style={{ padding: '0.75rem 1rem', color: 'var(--text-dim)', fontFamily: 'var(--font-mono)', fontSize: '0.7rem' }}>
                      {JSON.stringify(log.metadata)}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}

      {/* Modal: Create New Election */}
      {isNewElectionOpen && (
        <div className="modal-overlay">
          <div className="modal-content">
            <div className="modal-header">
              <h3>Create New Election</h3>
            </div>
            <form onSubmit={handleCreateElection}>
              <div className="modal-body">
                <div className="form-group">
                  <label className="form-label">Election Title</label>
                  <input
                    id="new-election-title-input"
                    className="form-input"
                    placeholder="e.g. 2026 Faculty Senate Election"
                    value={newElectionTitle}
                    onChange={(e) => setNewElectionTitle(e.target.value)}
                    required
                  />
                </div>
                <div className="form-group">
                  <label className="form-label">Description</label>
                  <textarea
                    id="new-election-desc-input"
                    className="form-input"
                    rows={3}
                    placeholder="Scope, guidelines, and terms"
                    value={newElectionDesc}
                    onChange={(e) => setNewElectionDesc(e.target.value)}
                  />
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '1rem' }}>
                  <div className="form-group">
                    <label className="form-label">Start Date</label>
                    <input
                      id="new-election-start-input"
                      className="form-input"
                      type="date"
                      value={newElectionStart}
                      onChange={(e) => setNewElectionStart(e.target.value)}
                    />
                  </div>
                  <div className="form-group">
                    <label className="form-label">End Date</label>
                    <input
                      id="new-election-end-input"
                      className="form-input"
                      type="date"
                      value={newElectionEnd}
                      onChange={(e) => setNewElectionEnd(e.target.value)}
                    />
                  </div>
                </div>
              </div>
              <div className="modal-footer">
                <button type="button" className="btn btn-secondary" onClick={() => setIsNewElectionOpen(false)}>
                  Cancel
                </button>
                <button id="confirm-create-election-btn" type="submit" className="btn btn-primary">
                  Create Election (Draft)
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* Modal: Assign Candidate */}
      {isAssignModalOpen && targetElectionForAssign && (
        <div className="modal-overlay">
          <div className="modal-content">
            <div className="modal-header">
              <h3>Assign Candidate to Slate</h3>
            </div>
            <form onSubmit={handleAssignCandidate}>
              <div className="modal-body">
                <div style={{ marginBottom: '1rem', fontSize: '0.9rem' }}>
                  Election: <strong>{targetElectionForAssign.title}</strong>
                </div>

                <div className="form-group">
                  <label className="form-label">Select Candidate to Assign</label>
                  <select
                    id="select-candidate-dropdown"
                    className="form-select"
                    value={candidateToAssign}
                    onChange={(e) => setCandidateToAssign(e.target.value)}
                    required
                  >
                    <option value="">-- Choose Candidate --</option>
                    {candidates.map((c) => (
                      <option key={c.id} value={c.id}>
                        {c.candidate_code} - {c.full_name} ({c.party_name})
                      </option>
                    ))}
                  </select>
                </div>
              </div>
              <div className="modal-footer">
                <button type="button" className="btn btn-secondary" onClick={() => setIsAssignModalOpen(false)}>
                  Cancel
                </button>
                <button id="confirm-assign-candidate-btn" type="submit" className="btn btn-primary">
                  Assign to Slate
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  );
};
