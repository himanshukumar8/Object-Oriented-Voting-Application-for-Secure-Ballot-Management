import type { ElectionResult } from '../types';
import { Award, BarChart3, CheckCircle, X } from 'lucide-react';

interface ResultsModalProps {
  results: ElectionResult | null;
  onClose: () => void;
}

export const ResultsModal: React.FC<ResultsModalProps> = ({ results, onClose }) => {
  if (!results) return null;

  return (
    <div className="modal-overlay" id="results-modal-overlay">
      <div className="modal-content" id="results-modal" style={{ maxWidth: '640px' }}>
        <div className="modal-header">
          <div style={{ display: 'flex', alignItems: 'center', gap: '0.65rem' }}>
            <BarChart3 size={22} color="#06b6d4" />
            <div>
              <h3 style={{ fontSize: '1.15rem' }}>Certified Election Results</h3>
              <div style={{ fontSize: '0.75rem', color: 'var(--text-muted)' }}>{results.title}</div>
            </div>
          </div>
          <button
            id="close-results-modal-btn"
            onClick={onClose}
            style={{ background: 'none', border: 'none', color: 'var(--text-muted)', cursor: 'pointer' }}
          >
            <X size={20} />
          </button>
        </div>

        <div className="modal-body">
          {/* Turnout Stats Cards */}
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '0.75rem', marginBottom: '1.5rem' }}>
            <div style={{ background: 'rgba(255, 255, 255, 0.03)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-md)', padding: '0.85rem', textAlign: 'center' }}>
              <div style={{ fontSize: '0.7rem', color: 'var(--text-muted)', textTransform: 'uppercase', fontWeight: 600 }}>Total Votes</div>
              <div style={{ fontSize: '1.35rem', fontWeight: 800, color: 'var(--accent-cyan)' }}>{results.total_votes}</div>
            </div>
            <div style={{ background: 'rgba(255, 255, 255, 0.03)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-md)', padding: '0.85rem', textAlign: 'center' }}>
              <div style={{ fontSize: '0.7rem', color: 'var(--text-muted)', textTransform: 'uppercase', fontWeight: 600 }}>Eligible Voters</div>
              <div style={{ fontSize: '1.35rem', fontWeight: 800 }}>{results.total_eligible_voters}</div>
            </div>
            <div style={{ background: 'rgba(255, 255, 255, 0.03)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-md)', padding: '0.85rem', textAlign: 'center' }}>
              <div style={{ fontSize: '0.7rem', color: 'var(--text-muted)', textTransform: 'uppercase', fontWeight: 600 }}>Turnout</div>
              <div style={{ fontSize: '1.35rem', fontWeight: 800, color: 'var(--success)' }}>
                {results.turnout_percentage.toFixed(1)}%
              </div>
            </div>
          </div>

          <h4 style={{ fontSize: '0.95rem', marginBottom: '0.85rem', color: 'var(--text-muted)' }}>Candidate Ballot Distribution</h4>

          {/* Candidate Breakdown */}
          <div style={{ display: 'flex', flexDirection: 'column', gap: '0.85rem' }}>
            {results.tallies.map((tally, index) => {
              const isWinner = index === 0 && tally.vote_count > 0;
              return (
                <div
                  key={tally.candidate_id}
                  style={{
                    background: isWinner ? 'rgba(99, 102, 241, 0.12)' : 'rgba(0, 0, 0, 0.25)',
                    border: `1px solid ${isWinner ? 'rgba(99, 102, 241, 0.4)' : 'var(--border-subtle)'}`,
                    borderRadius: 'var(--radius-md)',
                    padding: '1rem',
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '0.5rem' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
                      {isWinner && <Award size={18} color="#f59e0b" />}
                      <div>
                        <span style={{ fontWeight: 700, fontSize: '0.95rem' }}>{tally.full_name}</span>
                        <span style={{ fontSize: '0.75rem', color: 'var(--text-muted)', marginLeft: '0.5rem' }}>
                          ({tally.party_name} - {tally.party_symbol})
                        </span>
                      </div>
                    </div>
                    <div style={{ textAlign: 'right' }}>
                      <span style={{ fontWeight: 800, fontSize: '1.1rem' }}>{tally.vote_count}</span>
                      <span style={{ fontSize: '0.8rem', color: 'var(--text-muted)', marginLeft: '0.35rem' }}>
                        ({tally.percentage.toFixed(1)}%)
                      </span>
                    </div>
                  </div>

                  {/* Progress Bar */}
                  <div style={{ width: '100%', height: '8px', background: 'rgba(255, 255, 255, 0.08)', borderRadius: 'var(--radius-full)', overflow: 'hidden' }}>
                    <div
                      style={{
                        height: '100%',
                        width: `${Math.min(100, Math.max(0, tally.percentage))}%`,
                        background: isWinner ? 'linear-gradient(90deg, #6366f1, #06b6d4)' : 'rgba(255, 255, 255, 0.3)',
                        borderRadius: 'var(--radius-full)',
                        transition: 'width 0.6s ease',
                      }}
                    />
                  </div>
                </div>
              );
            })}
          </div>

          <div style={{ marginTop: '1.25rem', display: 'flex', alignItems: 'center', gap: '0.5rem', fontSize: '0.75rem', color: 'var(--text-dim)' }}>
            <CheckCircle size={14} color="#10b981" /> Certified from PostgreSQL append-only votes ledger.
          </div>
        </div>

        <div className="modal-footer">
          <button id="close-results-btn" className="btn btn-secondary" onClick={onClose}>
            Close
          </button>
        </div>
      </div>
    </div>
  );
};
