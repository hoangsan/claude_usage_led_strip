// fiveHour.js
//
// GET /five-hour handler factory. The factory takes injected `fetchUsage`
// and `transform` so server.js can wire the real implementations and tests
// can swap in stubs. See specs/001-claude-usage-led/contracts/five-hour-endpoint.md
// for the wire contract this implements.
//
// In-process upstream cache. The firmware naturally polls every 5 min, but the
// endpoint is otherwise unauthenticated on the LAN — anyone could hammer it
// and amplify load onto the operator's upstream Claude quota. We cache the
// raw upstream payload for `cacheTtlMs` (default 60 s) and re-run transform()
// on every request so `remaining_minutes` stays accurate against Date.now().
// Only successful (fetch + transform) responses are cached; any failure busts
// the cache so the next call re-fetches.

const DEFAULT_CACHE_TTL_MS = 60_000;

/**
 * @param {object} deps
 * @param {() => Promise<object>} deps.fetchUsage - upstream client.
 * @param {(payload: object, nowMs?: number) => { utilization: number, remaining_minutes: number }} deps.transform
 * @param {number} [deps.cacheTtlMs] - upstream-payload cache TTL in ms (default 60 000).
 * @param {(...args: unknown[]) => void} [deps.errorLog] - operator-side error sink (default console.error).
 * @returns {import('express').RequestHandler}
 */
export function createFiveHourHandler({
  fetchUsage,
  transform,
  cacheTtlMs = DEFAULT_CACHE_TTL_MS,
  errorLog = (...args) => console.error(...args),
}) {
  /** @type {{ upstreamPayload: object, fetchedAtMs: number } | null} */
  let cache = null;

  return async function fiveHourHandler(_req, res) {
    if (!process.env.CLAUDE_ENDPOINT) {
      res.status(500).json({
        error: 'server_misconfigured',
        detail: 'CLAUDE_ENDPOINT not set',
      });
      return;
    }

    const now = Date.now();
    const cacheHit = cache && now - cache.fetchedAtMs < cacheTtlMs;

    let upstream;
    if (cacheHit) {
      upstream = cache.upstreamPayload;
    } else {
      try {
        upstream = await fetchUsage();
      } catch (err) {
        cache = null;
        errorLog('[five-hour] upstream fetch failed:', err);
        res.status(502).json({
          error: 'upstream_unavailable',
          detail: err?.message || 'upstream_request_failed',
        });
        return;
      }
    }

    let minimal;
    try {
      minimal = transform(upstream);
    } catch (err) {
      cache = null;
      errorLog('[five-hour] transform failed:', err);
      res.status(502).json({
        error: 'upstream_unavailable',
        detail: err?.message || 'upstream_invalid',
      });
      return;
    }

    if (!cacheHit) {
      cache = { upstreamPayload: upstream, fetchedAtMs: now };
    }

    res.set('Cache-Control', 'no-store');
    res.status(200).json(minimal);
  };
}
