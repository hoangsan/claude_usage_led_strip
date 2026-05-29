// claudeClient.js
//
// Loads Claude OAuth credentials from disk and calls the upstream usage
// endpoint with the same headers as docs/claude-usage-node.js. The module
// exports a default `fetchUsage()` plus a `createClient(...)` factory that
// accepts injected fetch / loadCredentials / claudeVersion implementations so
// the route layer and tests can stub them.
//
// Token refresh: the access token in ~/.claude/.credentials.json lives only
// ~8 hours. When the device is powered off overnight nothing keeps it warm, so
// by morning it is expired and the upstream returns 401 -> the route maps that
// to a 502 (the "502 every morning" symptom). To self-heal, this module
// refreshes the access token from the stored refresh token when it is expired
// (or within `EXPIRY_SKEW_MS` of expiry) and writes the new credentials back to
// disk atomically, preserving every other field. A 401 from the usage endpoint
// also forces one refresh + retry in case `expiresAt` is wrong or the token was
// revoked early.

import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execSync } from 'node:child_process';

const DEFAULT_TIMEOUT_MS = 10_000;
const DEFAULT_USER_AGENT_VERSION = '2.1.146';

// Claude Code's public OAuth client. The token endpoint and client id are the
// same ones the CLI uses for `claude setup-token`; the refresh grant needs no
// client secret.
const OAUTH_TOKEN_URL = 'https://console.anthropic.com/v1/oauth/token';
const OAUTH_CLIENT_ID = '9d1c250a-e61b-44d9-88ed-5944d1962f5e';

// Refresh this far ahead of the stored expiry so a poll never races the
// boundary. 5 min comfortably covers clock skew + the upstream round-trip.
const EXPIRY_SKEW_MS = 5 * 60_000;

function defaultCredentialsPath() {
  return (
    process.env.CLAUDE_CREDENTIALS_FILE ||
    path.join(os.homedir(), '.claude', '.credentials.json')
  );
}

// Read + parse the credentials file, returning the full parsed object (so the
// caller can preserve unrelated fields on write-back) and its oauth subtree.
// Error messages omit `filePath` for the same LAN-leak reason documented on
// loadCredentials(); the original fs error is retained on `err.cause`.
function readCredentials(filePath) {
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
  return { parsed, oauth: parsed?.claudeAiOauth ?? null };
}

// Atomically persist `parsed` back to the credentials file: write a sibling
// temp file with 0600 perms, then rename over the original. The rename is
// atomic on POSIX, so a concurrent reader (e.g. the CLI) never sees a partial
// file. Other top-level keys and oauth subfields are preserved because we
// write back the same object we read.
function writeCredentials(filePath, parsed) {
  const dir = path.dirname(filePath);
  const tmp = path.join(dir, `.credentials.${process.pid}.${Date.now()}.tmp`);
  const data = JSON.stringify(parsed, null, 2) + '\n';
  fs.writeFileSync(tmp, data, { mode: 0o600 });
  try {
    fs.renameSync(tmp, filePath);
  } catch (err) {
    try {
      fs.unlinkSync(tmp);
    } catch {
      /* best effort */
    }
    throw err;
  }
}

export function loadCredentials(filePath = defaultCredentialsPath()) {
  // Error messages deliberately omit `filePath` so a leaky 502 detail body on
  // the LAN can't reveal the operator's home directory. The full path is still
  // available to operator-side logs via `err.cause` (which carries the original
  // fs error with its `.path`).
  const { oauth } = readCredentials(filePath);
  const token = oauth?.accessToken;
  if (!token || typeof token !== 'string') {
    throw new Error('credentials_missing_token');
  }
  return token;
}

// Exchange the stored refresh token for a fresh access token. Returns the new
// { accessToken, refreshToken, expiresAt(ms) }. Bare-coded errors only; the
// upstream body (which may carry detail) is retained on `err.cause`.
async function refreshAccessToken({
  refreshToken,
  fetchImpl,
  tokenUrl,
  clientId,
  timeoutMs,
  nowMs = Date.now(),
}) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  let res;
  try {
    res = await fetchImpl(tokenUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        grant_type: 'refresh_token',
        refresh_token: refreshToken,
        client_id: clientId,
      }),
      signal: controller.signal,
    });
  } catch (cause) {
    const isAbort =
      cause && (cause.name === 'AbortError' || cause.code === 'ABORT_ERR');
    const err = new Error(
      isAbort ? 'token_refresh_timeout' : 'token_refresh_request_failed',
    );
    err.cause = cause;
    throw err;
  } finally {
    clearTimeout(timer);
  }

  const body = await res.text();
  if (!res.ok) {
    const err = new Error(`token_refresh_status_${res.status}`);
    err.cause = new Error(body.slice(0, 500));
    throw err;
  }
  let json;
  try {
    json = JSON.parse(body);
  } catch (cause) {
    const err = new Error('token_refresh_invalid_json');
    err.cause = cause;
    throw err;
  }
  const accessToken = json.access_token;
  if (!accessToken || typeof accessToken !== 'string') {
    throw new Error('token_refresh_no_access_token');
  }
  const expiresIn = Number(json.expires_in);
  return {
    accessToken,
    // Some servers rotate the refresh token on every use; keep the new one if
    // present, otherwise the existing one stays valid.
    refreshToken:
      typeof json.refresh_token === 'string' && json.refresh_token
        ? json.refresh_token
        : refreshToken,
    expiresAt: Number.isFinite(expiresIn)
      ? nowMs + expiresIn * 1000
      : undefined,
  };
}

/**
 * Return a usable access token, refreshing + persisting it first if it is
 * expired, near expiry, or `force` is set. Falls back to the stored token when
 * no refresh token is available (best effort; the upstream call will then 401).
 *
 * @param {object} [opts]
 * @param {string} [opts.filePath]
 * @param {typeof fetch} [opts.fetchImpl]
 * @param {number} [opts.now] - injectable clock (ms).
 * @param {number} [opts.skewMs]
 * @param {string} [opts.tokenUrl]
 * @param {string} [opts.clientId]
 * @param {number} [opts.timeoutMs]
 * @param {boolean} [opts.force] - refresh even if not yet expired.
 * @param {(...args: unknown[]) => void} [opts.log] - operator-side log sink.
 * @returns {Promise<string>} the access token.
 */
export async function ensureFreshToken({
  filePath = defaultCredentialsPath(),
  fetchImpl = globalThis.fetch,
  now = Date.now(),
  skewMs = EXPIRY_SKEW_MS,
  tokenUrl = OAUTH_TOKEN_URL,
  clientId = OAUTH_CLIENT_ID,
  timeoutMs = DEFAULT_TIMEOUT_MS,
  force = false,
  log = (...args) => console.error(...args),
} = {}) {
  const { parsed, oauth } = readCredentials(filePath);
  const accessToken = oauth?.accessToken;
  const refreshToken = oauth?.refreshToken;
  const expiresAt = Number(oauth?.expiresAt);

  const hasToken = typeof accessToken === 'string' && accessToken.length > 0;
  const expiringSoon =
    Number.isFinite(expiresAt) && now >= expiresAt - skewMs;
  const needsRefresh = force || !hasToken || expiringSoon;

  if (!needsRefresh) {
    return accessToken;
  }

  if (typeof refreshToken !== 'string' || !refreshToken) {
    // Nothing to refresh with. Hand back whatever token we have so the caller
    // produces the usual upstream_status_* error rather than a confusing one.
    if (hasToken) return accessToken;
    throw new Error('credentials_missing_token');
  }

  const reason = force ? 'forced (401)' : !hasToken ? 'missing' : 'expiring';
  log(`[token] refreshing access token (${reason})`);
  const next = await refreshAccessToken({
    refreshToken,
    fetchImpl,
    tokenUrl,
    clientId,
    timeoutMs,
    nowMs: now,
  });

  // Persist the rotated credentials. Re-read is unnecessary — we already hold
  // the full parsed object and only mutate the oauth subtree.
  const updatedOauth = { ...(oauth ?? {}), accessToken: next.accessToken };
  updatedOauth.refreshToken = next.refreshToken;
  if (next.expiresAt !== undefined) updatedOauth.expiresAt = next.expiresAt;
  parsed.claudeAiOauth = updatedOauth;
  try {
    writeCredentials(filePath, parsed);
    if (next.expiresAt !== undefined) {
      log(
        `[token] refreshed; new expiry ${new Date(next.expiresAt).toISOString()}`,
      );
    }
  } catch (cause) {
    // The refresh itself succeeded, so the in-memory token is good for this
    // request; only persistence failed. Log loudly (next boot will refresh
    // again) but don't fail the request.
    log('[token] WARNING: refreshed but could not persist credentials:', cause);
  }
  return next.accessToken;
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
 * @param {(force?: boolean) => string | Promise<string>} [deps.loadToken] - returns the bearer token; pass `true` to force a refresh.
 * @param {() => string} [deps.versionFn] - returns the claude CLI version.
 * @param {number} [deps.timeoutMs] - per-call timeout in ms.
 * @returns {() => Promise<object>} fetchUsage function.
 */
export function createClient(deps = {}) {
  const {
    fetchImpl = globalThis.fetch,
    loadToken = (force = false) => ensureFreshToken({ fetchImpl, force }),
    versionFn = claudeVersion,
    timeoutMs = DEFAULT_TIMEOUT_MS,
  } = deps;

  // One usage GET with the given bearer token. Network/timeout failures are
  // normalized to bare-coded errors; the native cause is retained for logging.
  async function getUsage(endpoint, token) {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), timeoutMs);
    try {
      return await fetchImpl(endpoint, {
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
  }

  return async function fetchUsage() {
    const endpoint = process.env.CLAUDE_ENDPOINT;
    if (!endpoint) {
      throw new Error('CLAUDE_ENDPOINT not set');
    }

    let token = await loadToken();
    let res = await getUsage(endpoint, token);

    // A 401 despite a token we believed valid (clock skew, early revocation):
    // force one refresh and retry before giving up.
    if (res.status === 401) {
      token = await loadToken(true);
      res = await getUsage(endpoint, token);
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
