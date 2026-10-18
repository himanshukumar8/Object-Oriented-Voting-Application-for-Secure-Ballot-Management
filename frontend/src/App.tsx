import React, { useState, useEffect } from 'react';
import { Navbar } from './components/Navbar';
import { VoterPortal } from './components/VoterPortal';
import { AdminDashboard } from './components/AdminDashboard';
import { LoginModal } from './components/LoginModal';
import { ReceiptModal } from './components/ReceiptModal';
import { ResultsModal } from './components/ResultsModal';
import type { Election, UserSession, BallotReceipt, ElectionResult } from './types';
import { api, getStoredSession } from './api';
import { ShieldCheck, Database, Cpu, CheckCircle2, AlertTriangle, Info } from 'lucide-react';

export const App: React.FC = () => {
  const [session, setSession] = useState<UserSession | null>(getStoredSession());
  const [elections, setElections] = useState<Election[]>([]);
  const [activeView, setActiveView] = useState<'voter' | 'admin'>('voter');

  // Modal states
  const [isLoginOpen, setIsLoginOpen] = useState(false);
  const [defaultLoginRole, setDefaultLoginRole] = useState<'VOTER' | 'ADMIN'>('VOTER');
  const [currentReceipt, setCurrentReceipt] = useState<BallotReceipt | null>(null);
  const [currentResults, setCurrentResults] = useState<ElectionResult | null>(null);

  // Toast notifications
  const [toast, setToast] = useState<{ message: string; type: 'success' | 'error' | 'info' } | null>(null);

  const showToast = (message: string, type: 'success' | 'error' | 'info' = 'info') => {
    setToast({ message, type });
    setTimeout(() => setToast(null), 4000);
  };

  const loadElections = async () => {
    try {
      const data = await api.getElections();
      setElections(data);
    } catch {
      showToast('Failed to connect to voting backend', 'error');
    }
  };

  useEffect(() => {
    let active = true;
    api.getElections()
      .then((data) => {
        if (active) setElections(data);
      })
      .catch(() => {
        if (active) showToast('Failed to connect to voting backend', 'error');
      });
    return () => {
      active = false;
    };
  }, [session]);

  const handleOpenLogin = (role: 'VOTER' | 'ADMIN' = 'VOTER') => {
    setDefaultLoginRole(role);
    setIsLoginOpen(true);
  };

  const handleLoginSuccess = (newSession: UserSession) => {
    setSession(newSession);
    showToast(`Welcome back, ${newSession.user.full_name}!`, 'success');
    if (newSession.user.role === 'ADMIN') {
      setActiveView('admin');
    } else {
      setActiveView('voter');
    }
    loadElections();
  };

  const handleLogout = () => {
    api.logout();
    setSession(null);
    setActiveView('voter');
    showToast('Logged out successfully', 'info');
    loadElections();
  };

  return (
    <div className="app-container" id="app-root">
      <Navbar
        session={session}
        onOpenLogin={handleOpenLogin}
        onLogout={handleLogout}
        activeView={activeView}
        onChangeView={setActiveView}
      />

      <main>
        {activeView === 'admin' && session?.user.role === 'ADMIN' ? (
          <AdminDashboard
            elections={elections}
            onRefresh={loadElections}
            showToast={showToast}
          />
        ) : (
          <VoterPortal
            elections={elections}
            session={session}
            onRefresh={loadElections}
            onOpenLogin={handleOpenLogin}
            onShowReceipt={(receipt) => setCurrentReceipt(receipt)}
            onShowResults={(results) => setCurrentResults(results)}
            showToast={showToast}
          />
        )}
      </main>

      {/* Modals */}
      <LoginModal
        isOpen={isLoginOpen}
        onClose={() => setIsLoginOpen(false)}
        onLoginSuccess={handleLoginSuccess}
        defaultRole={defaultLoginRole}
      />

      <ReceiptModal
        receipt={currentReceipt}
        onClose={() => setCurrentReceipt(null)}
      />

      <ResultsModal
        results={currentResults}
        onClose={() => setCurrentResults(null)}
      />

      {/* Toast Notifications */}
      {toast && (
        <div className="toast-container">
          <div className={`toast toast-${toast.type}`}>
            {toast.type === 'success' && <CheckCircle2 size={18} />}
            {toast.type === 'error' && <AlertTriangle size={18} />}
            {toast.type === 'info' && <Info size={18} />}
            <span>{toast.message}</span>
          </div>
        </div>
      )}

      {/* Technical Footer */}
      <footer style={{ marginTop: '4rem', borderTop: '1px solid var(--border-subtle)', paddingTop: '2rem', paddingBottom: '2rem', textAlign: 'center', color: 'var(--text-dim)', fontSize: '0.8rem' }}>
        <div style={{ display: 'flex', justifyContent: 'center', alignItems: 'center', gap: '1.5rem', marginBottom: '0.75rem', flexWrap: 'wrap' }}>
          <span style={{ display: 'flex', alignItems: 'center', gap: '0.35rem' }}>
            <Cpu size={14} color="#6366f1" /> C++20 Multi-Threaded Engine
          </span>
          <span style={{ display: 'flex', alignItems: 'center', gap: '0.35rem' }}>
            <Database size={14} color="#06b6d4" /> PostgreSQL 18 ACID Persistence
          </span>
          <span style={{ display: 'flex', alignItems: 'center', gap: '0.35rem' }}>
            <ShieldCheck size={14} color="#10b981" /> Dual-Table Ballot Secrecy
          </span>
        </div>
        <div>
          AegisVote Ballot Platform © 2026. Staff+ Certified Systems Architecture & Interview Mastery Edition.
        </div>
      </footer>
    </div>
  );
};

export default App;
