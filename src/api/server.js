// server.js
//
// Express bootstrap. Loads .env, wires the /five-hour route with the real
// claudeClient + transform, exposes /health, and listens on PORT (or 3000).
//
// The createApp() factory exists so tests can build an app without crashing
// the process; only the entry-script branch at the bottom calls process.exit
// or app.listen.

import 'dotenv/config';
import express from 'express';
import { fileURLToPath } from 'node:url';

import { fetchUsage as defaultFetchUsage } from './lib/claudeClient.js';
import { upstreamToMinimal as defaultTransform } from './lib/transform.js';
import { createFiveHourHandler } from './routes/fiveHour.js';

/**
 * Build a configured Express app. Dependencies can be injected for tests.
 *
 * @param {object} [deps]
 * @param {() => Promise<object>} [deps.fetchUsage]
 * @param {(payload: object, nowMs?: number) => { utilization: number, remaining_minutes: number }} [deps.transform]
 * @param {(line: string) => void} [deps.log]
 * @returns {import('express').Express}
 */
export function createApp(deps = {}) {
  const {
    fetchUsage = defaultFetchUsage,
    transform = defaultTransform,
    log = (line) => process.stdout.write(line + '\n'),
    errorLog = (...args) => console.error(...args),
    cacheTtlMs = Number(process.env.CACHE_TTL_MS) || 60_000,
  } = deps;

  const app = express();

  // Per-request logging: <ISO> <method> <path> -> <status>.
  app.use((req, res, next) => {
    res.on('finish', () => {
      log(
        `${new Date().toISOString()} ${req.method} ${req.path} -> ${res.statusCode}`,
      );
    });
    next();
  });

  app.get(
    '/five-hour',
    createFiveHourHandler({ fetchUsage, transform, cacheTtlMs, errorLog }),
  );
  app.all('/five-hour', (_req, res) => {
    res.status(405).json({ error: 'method_not_allowed' });
  });

  app.get('/health', (_req, res) => {
    res.status(200).json({ status: 'ok' });
  });

  // 404 catch-all.
  app.use((_req, res) => {
    res.status(404).json({ error: 'not_found' });
  });

  return app;
}

function isEntryScript() {
  if (!process.argv[1]) return false;
  try {
    return import.meta.url === new URL(`file://${process.argv[1]}`).href;
  } catch {
    return false;
  }
}

// Robust check that handles symlinks, node -e wrappers, etc.
const invokedDirectly =
  process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];

if (invokedDirectly || isEntryScript()) {
  if (!process.env.CLAUDE_ENDPOINT) {
    process.stderr.write(
      'fatal: CLAUDE_ENDPOINT is not set (copy .env.example to .env)\n',
    );
    process.exit(1);
  }
  const app = createApp();
  const port = Number(process.env.PORT) || 3000;
  // HOST defaults to 0.0.0.0 for backward compatibility. Set HOST=127.0.0.1
  // (or your LAN IP) in .env to restrict exposure on multi-homed machines.
  const host = process.env.HOST || '0.0.0.0';
  app.listen(port, host, () => {
    process.stdout.write(
      `claude-usage-led-api listening on http://${host}:${port}\n`,
    );
  });
}
