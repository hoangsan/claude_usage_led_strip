// server.js
//
// Express app factory. Wires the /five-hour route with the real claudeClient +
// transform and exposes /health. Side-effect free: starting the listener and
// loading .env happens in start.js so tests can import { createApp } without
// spawning a server.

import express from 'express';

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
