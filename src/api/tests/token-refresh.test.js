// token-refresh.test.js
//
// Unit tests for ensureFreshToken() — the access-token auto-refresh that fixes
// the "502 every morning" expired-token symptom. All I/O is faked: the
// credentials file lives in a temp dir and the OAuth endpoint is a stub fetch.
// No real network or real ~/.claude is touched.

import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

import { ensureFreshToken } from '../lib/claudeClient.js';

function tmpCredsFile(oauth) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'creds-'));
  const file = path.join(dir, '.credentials.json');
  fs.writeFileSync(
    file,
    JSON.stringify({ claudeAiOauth: oauth, otherTopLevel: 'keep-me' }),
  );
  return file;
}

function readOauth(file) {
  return JSON.parse(fs.readFileSync(file, 'utf8')).claudeAiOauth;
}

// A stub fetch that returns one OAuth token response and records the request.
function stubTokenFetch(responseBody, { status = 200 } = {}) {
  const calls = [];
  const fetchImpl = async (url, init) => {
    calls.push({ url, init });
    return {
      ok: status >= 200 && status < 300,
      status,
      text: async () => JSON.stringify(responseBody),
    };
  };
  return { fetchImpl, calls };
}

const NOW = Date.parse('2026-05-27T07:00:00Z');

test('valid (not-yet-expiring) token is reused without any refresh call', async () => {
  const file = tmpCredsFile({
    accessToken: 'still-good',
    refreshToken: 'rt-1',
    expiresAt: NOW + 60 * 60_000, // 1h out — well past the skew window
  });
  const { fetchImpl, calls } = stubTokenFetch({});

  const token = await ensureFreshToken({
    filePath: file,
    fetchImpl,
    now: NOW,
    log: () => {},
  });

  assert.equal(token, 'still-good');
  assert.equal(calls.length, 0, 'no refresh should be attempted');
});

test('expired token is refreshed and persisted, preserving other fields', async () => {
  const file = tmpCredsFile({
    accessToken: 'old',
    refreshToken: 'rt-old',
    expiresAt: NOW - 60_000, // already expired
    subscriptionType: 'max',
  });
  const { fetchImpl, calls } = stubTokenFetch({
    access_token: 'new-access',
    refresh_token: 'rt-new',
    expires_in: 28_800, // 8h
  });

  const token = await ensureFreshToken({
    filePath: file,
    fetchImpl,
    now: NOW,
    log: () => {},
  });

  assert.equal(token, 'new-access');
  assert.equal(calls.length, 1);
  // Correct grant sent to the OAuth endpoint.
  const sent = JSON.parse(calls[0].init.body);
  assert.equal(sent.grant_type, 'refresh_token');
  assert.equal(sent.refresh_token, 'rt-old');
  assert.ok(sent.client_id, 'client_id is included');

  // Persisted: rotated tokens + recomputed expiry, unrelated fields untouched.
  const after = readOauth(file);
  assert.equal(after.accessToken, 'new-access');
  assert.equal(after.refreshToken, 'rt-new');
  assert.equal(after.expiresAt, NOW + 28_800 * 1000);
  assert.equal(after.subscriptionType, 'max');
  assert.equal(
    JSON.parse(fs.readFileSync(file, 'utf8')).otherTopLevel,
    'keep-me',
  );
});

test('token within the skew window is refreshed proactively', async () => {
  const file = tmpCredsFile({
    accessToken: 'old',
    refreshToken: 'rt-old',
    expiresAt: NOW + 60_000, // 1 min out — inside the 5-min skew
  });
  const { fetchImpl, calls } = stubTokenFetch({
    access_token: 'fresh',
    expires_in: 28_800,
  });

  const token = await ensureFreshToken({
    filePath: file,
    fetchImpl,
    now: NOW,
    log: () => {},
  });

  assert.equal(token, 'fresh');
  assert.equal(calls.length, 1);
  // No refresh_token in the response -> keep the existing one.
  assert.equal(readOauth(file).refreshToken, 'rt-old');
});

test('force=true refreshes even when the token is still valid', async () => {
  const file = tmpCredsFile({
    accessToken: 'good',
    refreshToken: 'rt-1',
    expiresAt: NOW + 60 * 60_000,
  });
  const { fetchImpl, calls } = stubTokenFetch({
    access_token: 'forced',
    expires_in: 28_800,
  });

  const token = await ensureFreshToken({
    filePath: file,
    fetchImpl,
    now: NOW,
    force: true,
    log: () => {},
  });

  assert.equal(token, 'forced');
  assert.equal(calls.length, 1);
});

test('expired token with no refresh token falls back to the stale token (best effort)', async () => {
  // Cannot self-heal without a refresh token; rather than invent an error we
  // hand back the stale token so the upstream produces the real
  // upstream_status_401 (more informative to the operator). No refresh call.
  const file = tmpCredsFile({
    accessToken: 'stale-but-only-option',
    expiresAt: NOW - 60_000,
  });
  const { fetchImpl, calls } = stubTokenFetch({});

  const token = await ensureFreshToken({
    filePath: file,
    fetchImpl,
    now: NOW,
    log: () => {},
  });

  assert.equal(token, 'stale-but-only-option');
  assert.equal(calls.length, 0, 'no refresh possible without a refresh token');
});

test('a failed refresh surfaces a bare token_refresh_status_ error', async () => {
  const file = tmpCredsFile({
    accessToken: 'old',
    refreshToken: 'rt-bad',
    expiresAt: NOW - 60_000,
  });
  const { fetchImpl } = stubTokenFetch(
    { error: 'invalid_grant' },
    { status: 400 },
  );

  await assert.rejects(
    ensureFreshToken({ filePath: file, fetchImpl, now: NOW, log: () => {} }),
    /token_refresh_status_400/,
  );
  // The stale credentials are left intact (not corrupted) on failure.
  assert.equal(readOauth(file).accessToken, 'old');
});
