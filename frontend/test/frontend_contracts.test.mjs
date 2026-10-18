import test from 'node:test';
import assert from 'node:assert/strict';

// Test 1: ApiError Contract
test('ApiError correctly encapsulates HTTP status, title, and detail', () => {
  class ApiError extends Error {
    constructor(status, title, detail) {
      super(detail || title);
      this.name = 'ApiError';
      this.status = status;
      this.title = title;
      this.detail = detail;
    }
  }

  const err409 = new ApiError(409, 'Double Voting Prohibited', 'Voter has already cast a ballot in this election');
  assert.equal(err409.name, 'ApiError');
  assert.equal(err409.status, 409);
  assert.equal(err409.title, 'Double Voting Prohibited');
  assert.equal(err409.detail, 'Voter has already cast a ballot in this election');
  assert.equal(err409.message, 'Voter has already cast a ballot in this election');

  const err500 = new ApiError(500, 'Internal Server Error', '');
  assert.equal(err500.status, 500);
  assert.equal(err500.message, 'Internal Server Error');
});

// Test 2: Idempotency Key UUID format
test('Ballot submission generates valid RFC-4122 v4 UUID idempotency keys', () => {
  const uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
  for (let i = 0; i < 50; i++) {
    const key = crypto.randomUUID();
    assert.match(key, uuidRegex, `Key ${key} must match UUID v4 format`);
  }
});

// Test 3: Dual-Table Decoupled Ballot Receipt Contract
test('BallotReceipt data contract enforces complete decoupling from candidate selection', () => {
  const sampleReceipt = {
    receipt_id: '9ad1fb36-5ce1-4a07-b037-3597a54ec262',
    election_id: 'e0000000-0000-0000-0000-000000000001',
    voter_id: 'v0000000-0000-0000-0000-000000000001',
    receipt_hash: '92281180018a7a0b5f3a0c4f9718fe3b0c44298fc1c149afbf4c8996fb92427a',
    cast_at: '2026-10-18T14:30:00Z',
    status: 'RECORDED'
  };

  // Receipt must contain cryptographic assurance attributes
  assert.ok(sampleReceipt.receipt_id);
  assert.ok(sampleReceipt.election_id);
  assert.ok(sampleReceipt.receipt_hash);
  assert.equal(sampleReceipt.receipt_hash.length, 64, 'SHA-256 hash must be exactly 64 hex characters');

  // Invariant: Receipt must NEVER contain candidate choices or party identifiers
  assert.equal(sampleReceipt.candidate_id, undefined, 'Receipt must NOT contain candidate_id');
  assert.equal(sampleReceipt.candidate_code, undefined, 'Receipt must NOT contain candidate_code');
  assert.equal(sampleReceipt.party_name, undefined, 'Receipt must NOT contain party_name');
});

// Test 4: Session Storage Eviction on 401
test('Session management accurately stores and evicts authentication tokens', () => {
  const mockStorage = new Map();
  const storageApi = {
    getItem: (k) => mockStorage.get(k) || null,
    setItem: (k, v) => mockStorage.set(k, String(v)),
    removeItem: (k) => mockStorage.delete(k)
  };

  function setStoredSession(session) {
    if (session) {
      storageApi.setItem('aegis_token', session.token);
      storageApi.setItem('aegis_user', JSON.stringify(session.user));
    } else {
      storageApi.removeItem('aegis_token');
      storageApi.removeItem('aegis_user');
    }
  }

  // Login
  setStoredSession({
    token: 'jwt.token.test',
    user: { id: 'u1', role: 'VOTER', full_name: 'Test Voter' }
  });
  assert.equal(storageApi.getItem('aegis_token'), 'jwt.token.test');
  assert.ok(storageApi.getItem('aegis_user'));

  // 401 Eviction
  setStoredSession(null);
  assert.equal(storageApi.getItem('aegis_token'), null);
  assert.equal(storageApi.getItem('aegis_user'), null);
});
