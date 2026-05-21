// claudeClient.js
//
// Loads Claude OAuth credentials from disk and calls the upstream usage
// endpoint with the same headers as docs/claude-usage-node.js. The module
// exports a default `fetchUsage()` plus a `createClient(...)` factory that
// accepts injected fetch / loadCredentials / claudeVersion implementations so
// the route layer and tests can stub them.

import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execSync } from 'node:child_process';

const DEFAULT_TIMEOUT_MS = 10_000;
const DEFAULT_USER_AGENT_VERSION = '2.1.146';

function defaultCredentialsPath() {
  return (
    process.env.CLAUDE_CREDENTIALS_FILE ||
    path.join(os.homedir(), '.claude', '.credentials.json')
  );
}

export function loadCredentials(filePath = defaultCredentialsPath()) {
  // Error messages deliberately omit `filePath` so a leaky 502 detail body on
  // the LAN can't reveal the operator's home directory. The full path is still
  // available to operator-side logs via `err.cause` (which carries the original
  // fs error with its `.path`).
  let raw;
  try {
    raw = fs.readFileSync(filePath, 'utf8');
  } catch (cause) {
    const err = new Error('credentials_unreadable');
    err.cause = cause;
    throw err;
  }
  let parsed;
  try {
    parsed = JSON.parse(raw);
  } catch (cause) {
    const err = new Error('credentials_invalid_json');
    err.cause = cause;
    throw err;
  }
  const token = parsed?.claudeAiOauth?.accessToken;
  if (!token || typeof token !== 'string') {
    throw new Error('credentials_missing_token');
  }
  return token;
}

export function claudeVersion() {
  try {
    const out = execSync('claude --version', {
      stdio: ['ignore', 'pipe', 'ignore'],
    })
      .toString()
      .trim()
      .split(/\s+/)[0];
    return out || DEFAULT_USER_AGENT_VERSION;
  } catch {
    return DEFAULT_USER_AGENT_VERSION;
  }
}

/**
 * Build a fetchUsage() bound to the given dependencies. Useful for tests.
 *
 * @param {object} [deps]
 * @param {typeof fetch} [deps.fetchImpl] - injected fetch (defaults to global).
 * @param {() => string} [deps.loadToken] - returns the bearer token.
 * @param {() => string} [deps.versionFn] - returns the claude CLI version.
 * @param {number} [deps.timeoutMs] - per-call timeout in ms.
 * @returns {() => Promise<object>} fetchUsage function.
 */
export function createClient(deps = {}) {
  const {
    fetchImpl = globalThis.fetch,
    loadToken = () => loadCredentials(),
    versionFn = claudeVersion,
    timeoutMs = DEFAULT_TIMEOUT_MS,
  } = deps;

  return async function fetchUsage() {
    const endpoint = process.env.CLAUDE_ENDPOINT;
    if (!endpoint) {
      throw new Error('CLAUDE_ENDPOINT not set');
    }
    const token = loadToken();
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), timeoutMs);
    let res;
    try {
      res = await fetchImpl(endpoint, {
        headers: {
          Authorization: `Bearer ${token}`,
          'anthropic-beta': 'oauth-2025-04-20',
          'User-Agent': `claude-cli/${versionFn()} (external, cli)`,
        },
        signal: controller.signal,
      });
    } catch (cause) {
      const isAbort =
        cause && (cause.name === 'AbortError' || cause.code === 'ABORT_ERR');
      // Bare codes only — the native cause (which can carry IP addresses,
      // hostnames, DNS errors) is retained on `err.cause` for stdout logging
      // but is NOT included in `err.message`, which the route layer surfaces
      // to LAN clients.
      const msg = isAbort ? 'upstream_timeout' : 'upstream_request_failed';
      const err = new Error(msg);
      err.cause = cause;
      throw err;
    } finally {
      clearTimeout(timer);
    }
    const body = await res.text();
    if (!res.ok) {
      // Status codes are safe to expose (no PII), the body is not.
      const err = new Error(`upstream_status_${res.status}`);
      err.cause = new Error(body.slice(0, 500));
      throw err;
    }
    try {
      return JSON.parse(body);
    } catch (cause) {
      const err = new Error('upstream_invalid_json');
      err.cause = cause;
      throw err;
    }
  };
}

// Default export: a fetchUsage() backed by the real filesystem + fetch.
export const fetchUsage = createClient();
