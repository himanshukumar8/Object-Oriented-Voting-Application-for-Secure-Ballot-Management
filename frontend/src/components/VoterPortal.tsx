import React, { useState } from 'react';
import type { Election, Candidate, UserSession, BallotReceipt, ElectionResult } from '../types';
import { api, ApiError } from '../api';
import { Check, ShieldCheck, AlertCircle, BarChart3, Clock, Vote, CheckCircle2 } from 'lucide-react';

interface VoterPortalProps {
  elections: Election[];
  session: UserSession | null;
  onRefresh: () => void;
  onOpenLogin: (role?: 'VOTER' | 'ADMIN') => void;
  onShowReceipt: (receipt: BallotReceipt) => void;
  onShowResults: (results: ElectionResult) => void;
  showToast: (msg: string, type: 'success' | 'error' | 'info') => void;
}

export const VoterPortal: React.FC<VoterPortalProps> = ({
  elections,
  session,
  onRefresh,
  onOpenLogin,
  onShowReceipt,
  onShowResults,
  showToast,
}) => {
  const [selectedElection, setSelectedElection] = useState<Election | null>(null);
  const [selectedCandidate, setSelectedCandidate] = useState<Candidate | null>(null);
  const [isConfirmModalOpen, setIsConfirmModalOpen] = useState(false);
  const [submitting, setSubmitting] = useState(false);
  const [filter, setFilter] = useState<'ALL' | 'OPEN' | 'COMPLETED'>('ALL');

  const filteredElections = elections.filter((e) => {
    if (filter === 'OPEN') return e.status === 'OPEN';
    if (filter === 'COMPLETED') return e.status === 'CLOSED' || e.status === 'RESULTS_PUBLISHED';
    return true;
  });

  const handleSelectElection = (election: Election) => {
    setSelectedElection(election);
    setSelectedCandidate(null);
  };

  const handleOpenConfirm = () => {
    if (!session) {
      onOpenLogin('VOTER');
      return;
    }
    if (!selectedCandidate) {
      showToast('Please select a candidate before proceeding', 'error');
      return;
    }
    setIsConfirmModalOpen(true);
  };

  const handleCastBallot = async () => {
    if (!selectedElection || !selectedCandidate) return;

    setSubmitting(true);
    try {
      const receipt = await api.castVote(selectedElection.id, selectedCandidate.id);
      setIsConfirmModalOpen(false);
      setSelectedCandidate(null);
      setSelectedElection(null);
      showToast('Ballot recorded successfully!', 'success');
      onRefresh();
      onShowReceipt(receipt);
    } catch (err: any) {
      if (err instanceof ApiError) {
        showToast(err.detail, 'error');
      } else {
        showToast(err.message || 'Failed to submit ballot', 'error');
      }
    } finally {
      setSubmitting(false);
    }
  };

  const handleViewResults = async (electionId: string) => {
    try {
      const results = await api.getResults(electionId);
      onShowResults(results);
    } catch (err: any) {
      showToast(err.detail || 'Results not yet available', 'error');
    }
  };

  return (
    <div id="voter-portal-view">
      {/* Banner */}
      <div className="glass-panel" style={{ padding: '2rem', marginBottom: '2rem', position: 'relative', overflow: 'hidden' }}>
        <div style={{ maxWidth: '650px' }}>
          <div className="badge badge-published" style={{ marginBottom: '0.75rem' }}>
            <ShieldCheck size={14} /> Cryptographic Ballot Assurance
          </div>
          <h1 style={{ fontSize: '2.2rem', marginBottom: '0.5rem', lineHeight: 1.2 }}>
            Secure, Paperless Democratic Voting
          </h1>
          <p style={{ color: 'var(--text-muted)', fontSize: '0.95rem' }}>
            Cast confidential ballots with atomic double-vote prevention. Each vote produces an immutable signed receipt hash decoupled from your identity.
          </p>
        </div>
      </div>

      {/* Filter Tabs */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '1.25rem' }}>
        <div style={{ display: 'flex', gap: '0.5rem' }}>
          <button
            id="filter-all-elections-btn"
            className={`btn ${filter === 'ALL' ? 'btn-primary' : 'btn-secondary'}`}
            style={{ padding: '0.4rem 0.85rem', fontSize: '0.8rem' }}
            onClick={() => setFilter('ALL')}
          >
            All Elections ({elections.length})
          </button>
          <button
            id="filter-open-elections-btn"
            className={`btn ${filter === 'OPEN' ? 'btn-primary' : 'btn-secondary'}`}
            style={{ padding: '0.4rem 0.85rem', fontSize: '0.8rem' }}
            onClick={() => setFilter('OPEN')}
          >
            Active Voting ({elections.filter(e => e.status === 'OPEN').length})
          </button>
          <button
            id="filter-completed-elections-btn"
            className={`btn ${filter === 'COMPLETED' ? 'btn-primary' : 'btn-secondary'}`}
            style={{ padding: '0.4rem 0.85rem', fontSize: '0.8rem' }}
            onClick={() => setFilter('COMPLETED')}
          >
            Conducted / Results ({elections.filter(e => e.status === 'CLOSED' || e.status === 'RESULTS_PUBLISHED').length})
          </button>
        </div>
      </div>

      {/* Elections Grid */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(360px, 1fr))', gap: '1.25rem', marginBottom: '2.5rem' }}>
        {filteredElections.map((election) => {
          const isSelected = selectedElection?.id === election.id;
          const isOpen = election.status === 'OPEN';
          const hasVoted = election.has_voted;

          return (
            <div
              key={election.id}
              id={`election-card-${election.id}`}
              className="glass-card"
              style={{
                border: isSelected ? '1px solid var(--primary)' : undefined,
                background: isSelected ? 'rgba(30, 41, 59, 0.9)' : undefined,
              }}
            >
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '0.75rem' }}>
                <span className={`badge badge-${election.status.toLowerCase().replace('_', '-')}`}>
                  {election.status === 'OPEN' && <span style={{ width: 6, height: 6, borderRadius: '50%', background: '#10b981', display: 'inline-block' }} />}
                  {election.status}
                </span>

                {hasVoted && (
                  <span className="badge badge-voted" id={`voted-badge-${election.id}`}>
                    <CheckCircle2 size={12} /> Ballot Recorded
                  </span>
                )}
              </div>

              <h3 style={{ fontSize: '1.2rem', marginBottom: '0.5rem' }}>{election.title}</h3>
              <p style={{ fontSize: '0.85rem', color: 'var(--text-muted)', marginBottom: '1.25rem', minHeight: '2.5rem' }}>
                {election.description || 'No description provided.'}
              </p>

              <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem', fontSize: '0.8rem', color: 'var(--text-dim)', marginBottom: '1.25rem' }}>
                <Clock size={14} /> Ends: {new Date(election.end_time).toLocaleDateString()}
              </div>

              <div style={{ display: 'flex', gap: '0.5rem', marginTop: 'auto' }}>
                {isOpen ? (
                  hasVoted ? (
                    <button
                      className="btn btn-secondary"
                      style={{ flex: 1 }}
                      disabled
                    >
                      <Check size={16} /> Already Voted
                    </button>
                  ) : (
                    <button
                      id={`select-election-btn-${election.id}`}
                      className="btn btn-primary"
                      style={{ flex: 1 }}
                      onClick={() => handleSelectElection(election)}
                    >
                      <Vote size={16} /> {isSelected ? 'Selected' : 'Vote Now'}
                    </button>
                  )
                ) : election.status === 'RESULTS_PUBLISHED' ? (
                  <button
                    id={`view-results-btn-${election.id}`}
                    className="btn btn-secondary"
                    style={{ flex: 1, borderColor: 'rgba(6, 182, 212, 0.4)', color: 'var(--accent-cyan)' }}
                    onClick={() => handleViewResults(election.id)}
                  >
                    <BarChart3 size={16} /> View Results
                  </button>
                ) : (
                  <button className="btn btn-secondary" style={{ flex: 1 }} disabled>
                    Voting Closed
                  </button>
                )}
              </div>
            </div>
          );
        })}
      </div>

      {/* Selected Election Ballot Slate Section */}
      {selectedElection && (
        <section id="ballot-slate-section" className="glass-panel" style={{ padding: '2rem', marginBottom: '2.5rem' }}>
          <div style={{ borderBottom: '1px solid var(--border-subtle)', paddingBottom: '1rem', marginBottom: '1.5rem', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
            <div>
              <span className="badge badge-open" style={{ marginBottom: '0.35rem' }}>Official Ballot</span>
              <h2 style={{ fontSize: '1.5rem' }}>{selectedElection.title}</h2>
              <p style={{ color: 'var(--text-muted)', fontSize: '0.85rem' }}>
                Select exactly one candidate below and click "Review & Cast Ballot" to submit.
              </p>
            </div>
            <button
              className="btn btn-secondary"
              style={{ fontSize: '0.8rem', padding: '0.4rem 0.8rem' }}
              onClick={() => setSelectedElection(null)}
            >
              Cancel Selection
            </button>
          </div>

          {selectedElection.candidates.length === 0 ? (
            <div style={{ textAlign: 'center', padding: '2rem', color: 'var(--text-muted)' }}>
              No candidates assigned to this ballot yet.
            </div>
          ) : (
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(300px, 1fr))', gap: '1.25rem', marginBottom: '2rem' }}>
              {selectedElection.candidates.map((candidate) => {
                const isCandidateSelected = selectedCandidate?.id === candidate.id;
                return (
                  <div
                    key={candidate.id}
                    id={`candidate-card-${candidate.id}`}
                    onClick={() => setSelectedCandidate(candidate)}
                    style={{
                      background: isCandidateSelected ? 'rgba(99, 102, 241, 0.15)' : 'rgba(0, 0, 0, 0.25)',
                      border: `2px solid ${isCandidateSelected ? 'var(--primary)' : 'var(--border-subtle)'}`,
                      borderRadius: 'var(--radius-md)',
                      padding: '1.25rem',
                      cursor: 'pointer',
                      transition: 'all var(--transition-fast)',
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '0.5rem' }}>
                      <span className="badge badge-scheduled" style={{ fontSize: '0.7rem' }}>
                        {candidate.candidate_code}
                      </span>
                      <div
                        style={{
                          width: '22px',
                          height: '22px',
                          borderRadius: '50%',
                          border: `2px solid ${isCandidateSelected ? 'var(--primary)' : 'var(--border-card)'}`,
                          background: isCandidateSelected ? 'var(--primary)' : 'transparent',
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center',
                        }}
                      >
                        {isCandidateSelected && <Check size={14} color="#fff" />}
                      </div>
                    </div>

                    <h3 style={{ fontSize: '1.15rem', marginBottom: '0.2rem' }}>{candidate.full_name}</h3>
                    <div style={{ fontSize: '0.85rem', fontWeight: 600, color: 'var(--accent-cyan)', marginBottom: '0.5rem' }}>
                      {candidate.party_name} • {candidate.party_symbol}
                    </div>
                    <p style={{ fontSize: '0.8rem', color: 'var(--text-muted)', lineHeight: 1.4 }}>
                      {candidate.biography || 'Registered candidate for this election.'}
                    </p>
                  </div>
                );
              })}
            </div>
          )}

          {/* Action Bar */}
          <div style={{ display: 'flex', justifyContent: 'flex-end', alignItems: 'center', gap: '1rem', borderTop: '1px solid var(--border-subtle)', paddingTop: '1.25rem' }}>
            <span style={{ fontSize: '0.85rem', color: 'var(--text-muted)' }}>
              {selectedCandidate ? (
                <>Selected: <strong style={{ color: 'var(--text-main)' }}>{selectedCandidate.full_name}</strong></>
              ) : (
                'No candidate selected'
              )}
            </span>
            <button
              id="review-ballot-btn"
              className="btn btn-primary"
              disabled={!selectedCandidate}
              onClick={handleOpenConfirm}
            >
              Review & Cast Ballot
            </button>
          </div>
        </section>
      )}

      {/* Pre-Submission Confirmation Modal */}
      {isConfirmModalOpen && selectedCandidate && selectedElection && (
        <div className="modal-overlay" id="confirm-ballot-modal-overlay">
          <div className="modal-content" id="confirm-ballot-modal">
            <div className="modal-header">
              <h3 style={{ fontSize: '1.15rem' }}>Confirm Ballot Submission</h3>
            </div>
            <div className="modal-body">
              <div style={{ background: 'rgba(0, 0, 0, 0.3)', border: '1px solid var(--border-card)', borderRadius: 'var(--radius-md)', padding: '1.25rem', marginBottom: '1.25rem' }}>
                <div style={{ fontSize: '0.8rem', color: 'var(--text-muted)', marginBottom: '0.25rem' }}>Election</div>
                <div style={{ fontWeight: 700, fontSize: '1.05rem', marginBottom: '1rem' }}>{selectedElection.title}</div>

                <div style={{ fontSize: '0.8rem', color: 'var(--text-muted)', marginBottom: '0.25rem' }}>Selected Candidate</div>
                <div style={{ fontWeight: 800, fontSize: '1.25rem', color: 'var(--primary)' }}>
                  {selectedCandidate.full_name}
                </div>
                <div style={{ fontSize: '0.85rem', color: 'var(--accent-cyan)', fontWeight: 600 }}>
                  {selectedCandidate.party_name} ({selectedCandidate.party_symbol})
                </div>
              </div>

              <div style={{ display: 'flex', gap: '0.5rem', background: 'var(--warning-bg)', border: '1px solid var(--warning-border)', color: 'var(--warning)', padding: '0.75rem', borderRadius: 'var(--radius-md)', fontSize: '0.8rem' }}>
                <AlertCircle size={18} style={{ flexShrink: 0, marginTop: '2px' }} />
                <span>
                  <strong>Final Action Warning:</strong> Once confirmed, this ballot will be permanently committed to the PostgreSQL ledger. You cannot modify your vote or cast another ballot in this election.
                </span>
              </div>
            </div>

            <div className="modal-footer">
              <button
                className="btn btn-secondary"
                onClick={() => setIsConfirmModalOpen(false)}
                disabled={submitting}
              >
                Go Back
              </button>
              <button
                id="final-confirm-vote-btn"
                className="btn btn-primary"
                onClick={handleCastBallot}
                disabled={submitting}
              >
                {submitting ? 'Recording...' : 'Confirm & Commit Ballot'}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
