import { useState } from 'react';
import type { BallotReceipt } from '../types';
import { CheckCircle2, Copy, Check, ShieldCheck, X } from 'lucide-react';

interface ReceiptModalProps {
  receipt: BallotReceipt | null;
  onClose: () => void;
}

export const ReceiptModal: React.FC<ReceiptModalProps> = ({ receipt, onClose }) => {
  const [copied, setCopied] = useState(false);

  if (!receipt) return null;

  const handleCopy = () => {
    navigator.clipboard.writeText(receipt.receipt_hash);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div className="modal-overlay" id="receipt-modal-overlay">
      <div className="modal-content" id="ballot-receipt-modal" style={{ maxWidth: '580px' }}>
        <div className="modal-header" style={{ background: 'rgba(16, 185, 129, 0.08)', borderBottom: '1px solid rgba(16, 185, 129, 0.2)' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '0.65rem' }}>
            <CheckCircle2 size={24} color="#10b981" />
            <h3 style={{ fontSize: '1.2rem', color: '#10b981' }}>Ballot Confirmed & Recorded</h3>
          </div>
          <button
            id="close-receipt-modal-btn"
            onClick={onClose}
            style={{ background: 'none', border: 'none', color: 'var(--text-muted)', cursor: 'pointer' }}
          >
            <X size={20} />
          </button>
        </div>

        <div className="modal-body">
          <div style={{ textAlign: 'center', marginBottom: '1.5rem' }}>
            <div style={{ display: 'inline-flex', padding: '1rem', borderRadius: 'var(--radius-full)', background: 'var(--success-bg)', marginBottom: '0.75rem' }}>
              <ShieldCheck size={40} color="#10b981" />
            </div>
            <h4 style={{ fontSize: '1.1rem', marginBottom: '0.25rem' }}>Your Vote is Cryptographically Sealed</h4>
            <p style={{ fontSize: '0.85rem', color: 'var(--text-muted)' }}>
              Transaction committed to PostgreSQL database. Your ballot has been decoupled from your identity.
            </p>
          </div>

          <div style={{ background: 'rgba(0, 0, 0, 0.35)', border: '1px solid var(--border-card)', borderRadius: 'var(--radius-md)', padding: '1.25rem', marginBottom: '1.25rem' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '0.75rem', fontSize: '0.85rem' }}>
              <span style={{ color: 'var(--text-muted)' }}>Receipt Identifier:</span>
              <span style={{ fontFamily: 'var(--font-mono)', fontWeight: 600 }}>{receipt.receipt_id.substring(0, 18)}...</span>
            </div>

            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '0.75rem', fontSize: '0.85rem' }}>
              <span style={{ color: 'var(--text-muted)' }}>Timestamp:</span>
              <span>{new Date(receipt.cast_at).toLocaleString()}</span>
            </div>

            <div style={{ marginTop: '0.75rem', paddingTop: '0.75rem', borderTop: '1px solid var(--border-subtle)' }}>
              <div style={{ fontSize: '0.8rem', color: 'var(--text-muted)', marginBottom: '0.35rem' }}>
                Cryptographic Verification Hash (SHA-256):
              </div>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '0.5rem',
                background: 'rgba(15, 23, 42, 0.8)',
                padding: '0.65rem 0.85rem',
                borderRadius: 'var(--radius-sm)',
                border: '1px solid var(--border-subtle)',
              }}>
                <span
                  id="receipt-hash-display"
                  style={{
                    fontFamily: 'var(--font-mono)',
                    fontSize: '0.75rem',
                    color: 'var(--accent-cyan)',
                    wordBreak: 'break-all',
                    flex: 1,
                  }}
                >
                  {receipt.receipt_hash}
                </span>
                <button
                  id="copy-receipt-hash-btn"
                  className="btn btn-secondary"
                  style={{ padding: '0.35rem 0.65rem', fontSize: '0.75rem' }}
                  onClick={handleCopy}
                  title="Copy verification hash"
                >
                  {copied ? <Check size={14} color="#10b981" /> : <Copy size={14} />}
                </button>
              </div>
            </div>
          </div>

          <div style={{
            background: 'rgba(99, 102, 241, 0.08)',
            border: '1px solid rgba(99, 102, 241, 0.25)',
            borderRadius: 'var(--radius-md)',
            padding: '0.85rem 1rem',
            fontSize: '0.8rem',
            color: 'var(--text-muted)',
            lineHeight: 1.5,
          }}>
            <strong style={{ color: 'var(--text-main)', display: 'block', marginBottom: '0.2rem' }}>
              🔒 Ballot Secrecy Guarantee:
            </strong>
            This signed receipt mathematically proves your participation was recorded. Under our dual-table relational architecture, this receipt contains zero information linking your identity to your chosen candidate.
          </div>
        </div>

        <div className="modal-footer">
          <button id="dismiss-receipt-btn" className="btn btn-primary" onClick={onClose}>
            Done
          </button>
        </div>
      </div>
    </div>
  );
};
