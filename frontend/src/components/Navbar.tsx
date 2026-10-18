import type { UserSession } from '../types';
import { ShieldCheck, LogOut, User, Lock } from 'lucide-react';

interface NavbarProps {
  session: UserSession | null;
  onOpenLogin: (defaultRole?: 'VOTER' | 'ADMIN') => void;
  onLogout: () => void;
  activeView: 'voter' | 'admin';
  onChangeView: (view: 'voter' | 'admin') => void;
}

export const Navbar: React.FC<NavbarProps> = ({
  session,
  onOpenLogin,
  onLogout,
  activeView,
  onChangeView,
}) => {
  return (
    <header className="navbar glass-panel" id="main-navbar">
      <div className="nav-brand" id="brand-logo">
        <ShieldCheck size={32} color="#6366f1" />
        <div>
          Aegis<span>Vote</span>
          <div style={{ fontSize: '0.65rem', fontWeight: 500, letterSpacing: '0.05em', color: 'var(--text-muted)' }}>
            SECURE BALLOT PLATFORM
          </div>
        </div>
      </div>

      <nav className="nav-actions">
        {session?.user.role === 'ADMIN' && (
          <div style={{ display: 'flex', gap: '0.5rem', marginRight: '1rem' }}>
            <button
              id="view-voter-portal-btn"
              className={`btn ${activeView === 'voter' ? 'btn-primary' : 'btn-secondary'}`}
              style={{ padding: '0.45rem 0.85rem', fontSize: '0.8rem' }}
              onClick={() => onChangeView('voter')}
            >
              Voter View
            </button>
            <button
              id="view-admin-dashboard-btn"
              className={`btn ${activeView === 'admin' ? 'btn-primary' : 'btn-secondary'}`}
              style={{ padding: '0.45rem 0.85rem', fontSize: '0.8rem' }}
              onClick={() => onChangeView('admin')}
            >
              <Lock size={14} /> Admin Dashboard
            </button>
          </div>
        )}

        {session ? (
          <div style={{ display: 'flex', alignItems: 'center', gap: '1rem' }}>
            <div style={{ textAlign: 'right' }}>
              <div style={{ fontSize: '0.85rem', fontWeight: 600 }}>{session.user.full_name}</div>
              <div style={{ fontSize: '0.7rem', color: 'var(--text-muted)' }}>
                {session.user.role === 'ADMIN' ? (
                  <span className="badge badge-published" style={{ padding: '0.1rem 0.4rem', fontSize: '0.65rem' }}>Administrator</span>
                ) : (
                  <span className="badge badge-voted" style={{ padding: '0.1rem 0.4rem', fontSize: '0.65rem' }}>Voter: {session.user.voter_id}</span>
                )}
              </div>
            </div>
            <button
              id="logout-btn"
              className="btn btn-secondary"
              style={{ padding: '0.5rem 0.85rem' }}
              onClick={onLogout}
              title="Logout"
            >
              <LogOut size={16} /> Logout
            </button>
          </div>
        ) : (
          <div style={{ display: 'flex', gap: '0.75rem' }}>
            <button
              id="voter-login-trigger"
              className="btn btn-primary"
              onClick={() => onOpenLogin('VOTER')}
            >
              <User size={16} /> Voter Login
            </button>
            <button
              id="admin-login-trigger"
              className="btn btn-secondary"
              onClick={() => onOpenLogin('ADMIN')}
            >
              <Lock size={14} /> Admin Access
            </button>
          </div>
        )}
      </nav>
    </header>
  );
};
