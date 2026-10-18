import { useState } from 'react';
import { api, ApiError } from '../api';
import type { UserSession } from '../types';
import { X, Lock, UserCheck, ShieldAlert, Sparkles } from 'lucide-react';

interface LoginModalProps {
  isOpen: boolean;
  onClose: () => void;
  onLoginSuccess: (session: UserSession) => void;
  defaultRole?: 'VOTER' | 'ADMIN';
}

export const LoginModal: React.FC<LoginModalProps> = ({
  isOpen,
  onClose,
  onLoginSuccess,
  defaultRole = 'VOTER',
}) => {
  const [role, setRole] = useState<'VOTER' | 'ADMIN'>(defaultRole);
  const [voterId, setVoterId] = useState('');
  const [adminUsername, setAdminUsername] = useState('');
  const [password, setPassword] = useState('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  if (!isOpen) return null;

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError(null);
    setLoading(true);

    try {
      if (role === 'VOTER') {
        const session = await api.loginVoter(voterId, password);
        onLoginSuccess(session);
        onClose();
      } else {
        const session = await api.loginAdmin(adminUsername, password);
        onLoginSuccess(session);
        onClose();
      }
    } catch (err: any) {
      if (err instanceof ApiError) {
        setError(err.detail);
      } else {
        setError(err.message || 'Authentication failed');
      }
    } finally {
      setLoading(false);
    }
  };

  const handlePrefillDemo = (type: 'voter1' | 'voter2' | 'admin') => {
    setError(null);
    if (type === 'voter1') {
      setRole('VOTER');
      setVoterId('VOTER-101');
      setPassword('VoterPass2026!');
    } else if (type === 'voter2') {
      setRole('VOTER');
      setVoterId('VOTER-102');
      setPassword('VoterPass2026!');
    } else {
      setRole('ADMIN');
      setAdminUsername('admin');
      setPassword('AdminSecure2026!');
    }
  };

  return (
    <div className="modal-overlay" id="login-modal-overlay">
      <div className="modal-content" id="login-modal">
        <div className="modal-header">
          <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            {role === 'ADMIN' ? <Lock size={20} color="#f59e0b" /> : <UserCheck size={20} color="#6366f1" />}
            <h3 style={{ fontSize: '1.15rem' }}>
              {role === 'ADMIN' ? 'Administrator Authentication' : 'Voter Portal Access'}
            </h3>
          </div>
          <button
            id="close-login-modal-btn"
            onClick={onClose}
            style={{ background: 'none', border: 'none', color: 'var(--text-muted)', cursor: 'pointer' }}
          >
            <X size={20} />
          </button>
        </div>

        <div style={{ display: 'flex', borderBottom: '1px solid var(--border-subtle)', background: 'rgba(0,0,0,0.2)' }}>
          <button
            id="tab-voter-login"
            style={{
              flex: 1,
              padding: '0.75rem',
              background: role === 'VOTER' ? 'rgba(99, 102, 241, 0.15)' : 'none',
              border: 'none',
              borderBottom: role === 'VOTER' ? '2px solid var(--primary)' : '2px solid transparent',
              color: role === 'VOTER' ? 'var(--text-main)' : 'var(--text-muted)',
              fontWeight: 600,
              fontSize: '0.9rem',
              cursor: 'pointer',
            }}
            onClick={() => { setRole('VOTER'); setError(null); }}
          >
            Voter Login
          </button>
          <button
            id="tab-admin-login"
            style={{
              flex: 1,
              padding: '0.75rem',
              background: role === 'ADMIN' ? 'rgba(245, 158, 11, 0.15)' : 'none',
              border: 'none',
              borderBottom: role === 'ADMIN' ? '2px solid var(--warning)' : '2px solid transparent',
              color: role === 'ADMIN' ? 'var(--text-main)' : 'var(--text-muted)',
              fontWeight: 600,
              fontSize: '0.9rem',
              cursor: 'pointer',
            }}
            onClick={() => { setRole('ADMIN'); setError(null); }}
          >
            Admin Login
          </button>
        </div>

        <form onSubmit={handleSubmit}>
          <div className="modal-body">
            {error && (
              <div style={{
                background: 'var(--danger-bg)',
                border: '1px solid var(--danger-border)',
                color: 'var(--danger)',
                padding: '0.75rem 1rem',
                borderRadius: 'var(--radius-md)',
                marginBottom: '1rem',
                display: 'flex',
                alignItems: 'center',
                gap: '0.5rem',
                fontSize: '0.85rem',
              }}>
                <ShieldAlert size={16} />
                <span>{error}</span>
              </div>
            )}

            {role === 'VOTER' ? (
              <div className="form-group">
                <label className="form-label" htmlFor="voter-id-input">Voter ID</label>
                <input
                  id="voter-id-input"
                  className="form-input"
                  type="text"
                  placeholder="e.g. VOTER-101"
                  value={voterId}
                  onChange={(e) => setVoterId(e.target.value)}
                  required
                />
              </div>
            ) : (
              <div className="form-group">
                <label className="form-label" htmlFor="admin-username-input">Admin Username</label>
                <input
                  id="admin-username-input"
                  className="form-input"
                  type="text"
                  placeholder="e.g. admin"
                  value={adminUsername}
                  onChange={(e) => setAdminUsername(e.target.value)}
                  required
                />
              </div>
            )}

            <div className="form-group">
              <label className="form-label" htmlFor="password-input">Password</label>
              <input
                id="password-input"
                className="form-input"
                type="password"
                placeholder="Enter password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                required
              />
            </div>

            {/* Quick Demo Fill Buttons */}
            <div style={{ marginTop: '1.25rem', padding: '0.75rem', background: 'rgba(255,255,255,0.02)', borderRadius: 'var(--radius-md)', border: '1px dashed var(--border-card)' }}>
              <div style={{ fontSize: '0.75rem', fontWeight: 600, color: 'var(--text-muted)', marginBottom: '0.5rem', display: 'flex', alignItems: 'center', gap: '0.35rem' }}>
                <Sparkles size={12} color="#06b6d4" /> Synthetic Demo Credentials (Quick Fill):
              </div>
              <div style={{ display: 'flex', gap: '0.5rem', flexWrap: 'wrap' }}>
                <button
                  type="button"
                  id="demo-fill-voter1-btn"
                  className="btn btn-secondary"
                  style={{ padding: '0.25rem 0.5rem', fontSize: '0.75rem' }}
                  onClick={() => handlePrefillDemo('voter1')}
                >
                  Voter 101
                </button>
                <button
                  type="button"
                  id="demo-fill-voter2-btn"
                  className="btn btn-secondary"
                  style={{ padding: '0.25rem 0.5rem', fontSize: '0.75rem' }}
                  onClick={() => handlePrefillDemo('voter2')}
                >
                  Voter 102
                </button>
                <button
                  type="button"
                  id="demo-fill-admin-btn"
                  className="btn btn-secondary"
                  style={{ padding: '0.25rem 0.5rem', fontSize: '0.75rem' }}
                  onClick={() => handlePrefillDemo('admin')}
                >
                  Admin
                </button>
              </div>
            </div>
          </div>

          <div className="modal-footer">
            <button type="button" className="btn btn-secondary" onClick={onClose}>
              Cancel
            </button>
            <button
              id="submit-login-btn"
              type="submit"
              className="btn btn-primary"
              disabled={loading}
            >
              {loading ? 'Authenticating...' : 'Sign In'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};
