// route.test.js
//
// End-to-end-ish route tests. We build a real Express app via createApp(),
// listen on an ephemeral port via node:http, hit it with the global fetch,
// and assert. No supertest dependency.

import test from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';

import { createApp } from '../server.js';

const SUCCESS_BODY = { utilization: 42.3, remaining_minutes: 187 };

function silentLog() {}

async function withServer(app, fn) {
  const server = http.createServer(app);
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const { port } = server.address();
  const base = `http://127.0.0.1:${port}`;
  try {
    await fn(base);
  } finally {
    await new Promise((resolve) => server.close(resolve));
  }
}

test('GET /five-hour success returns 200 with correct shape and Cache-Control: no-store', async () => {
  process.env.CLAUDE_ENDPOINT = 'http://fake.test/usage';
  const app = createApp({
    fetchUsage: async () => ({ pretend: 'upstream' }),
    transform: () => ({ ...SUCCESS_BODY }),
    log: silentLog,
  });
  await withServer(app, async (base) => {
    const res = await fetch(`${base}/five-hour`);
    assert.equal(res.status, 200);
    assert.equal(res.headers.get('cache-control'), 'no-store');
    const body = await res.json();
    assert.deepEqual(body, SUCCESS_BODY);
  });
});

test('GET /five-hour returns 502 upstream_unavailable when fetchUsage throws', async () => {
  process.env.CLAUDE_ENDPOINT = 'http://fake.test/usage';
  const app = createApp({
    fetchUsage: async () => {
      throw new Error('HTTP 401 from upstream');
    },
    transform: () => {
      throw new Error('should not be called');
    },
    log: silentLog,
  });
  await withServer(app, async (base) => {
    const res = await fetch(`${base}/five-hour`);
    assert.equal(res.status, 502);
    const body = await res.json();
    assert.equal(body.error, 'upstream_unavailable');
    assert.ok(typeof body.detail === 'string' && body.detail.length > 0);
  });
});

test('GET /five-hour returns 502 upstream_unavailable when transform throws', async () => {
  process.env.CLAUDE_ENDPOINT = 'http://fake.test/usage';
  const app = createApp({
    fetchUsage: async () => ({ broken: true }),
    transform: () => {
      throw new Error('invalid_upstream: five_hour missing');
    },
    log: silentLog,
  });
  await withServer(app, async (base) => {
    const res = await fetch(`${base}/five-hour`);
    assert.equal(res.status, 502);
    const body = await res.json();
    assert.equal(body.error, 'upstream_unavailable');
    assert.match(body.detail, /invalid_upstream/);
  });
});

test('GET /five-hour returns 500 server_misconfigured when CLAUDE_ENDPOINT is missing', async () => {
  const saved = process.env.CLAUDE_ENDPOINT;
  delete process.env.CLAUDE_ENDPOINT;
  const app = createApp({
    fetchUsage: async () => ({ ignored: true }),
    transform: () => SUCCESS_BODY,
    log: silentLog,
  });
  try {
    await withServer(app, async (base) => {
      const res = await fetch(`${base}/five-hour`);
      assert.equal(res.status, 500);
      const body = await res.json();
      assert.equal(body.error, 'server_misconfigured');
      assert.equal(body.detail, 'CLAUDE_ENDPOINT not set');
    });
  } finally {
    if (saved !== undefined) process.env.CLAUDE_ENDPOINT = saved;
  }
});

test('GET /bad-path returns 404 not_found', async () => {
  process.env.CLAUDE_ENDPOINT = 'http://fake.test/usage';
  const app = createApp({
    fetchUsage: async () => SUCCESS_BODY,
    transform: () => SUCCESS_BODY,
    log: silentLog,
  });
  await withServer(app, async (base) => {
    const res = await fetch(`${base}/bad-path`);
    assert.equal(res.status, 404);
    const body = await res.json();
    assert.deepEqual(body, { error: 'not_found' });
  });
});

test('POST /five-hour returns 405 method_not_allowed', async () => {
  process.env.CLAUDE_ENDPOINT = 'http://fake.test/usage';
  const app = createApp({
    fetchUsage: async () => SUCCESS_BODY,
    transform: () => SUCCESS_BODY,
    log: silentLog,
  });
  await withServer(app, async (base) => {
    const res = await fetch(`${base}/five-hour`, { method: 'POST' });
    assert.equal(res.status, 405);
    const body = await res.json();
    assert.deepEqual(body, { error: 'method_not_allowed' });
  });
});

test('GET /health returns 200 {status:"ok"}', async () => {
  process.env.CLAUDE_ENDPOINT = 'http://fake.test/usage';
  const app = createApp({
    fetchUsage: async () => SUCCESS_BODY,
    transform: () => SUCCESS_BODY,
    log: silentLog,
  });
  await withServer(app, async (base) => {
    const res = await fetch(`${base}/health`);
    assert.equal(res.status, 200);
    const body = await res.json();
    assert.deepEqual(body, { status: 'ok' });
  });
});
