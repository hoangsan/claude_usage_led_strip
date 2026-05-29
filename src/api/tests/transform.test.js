// transform.test.js
//
// Pure-function tests for upstreamToMinimal. Run with `node --test`.

import test from 'node:test';
import assert from 'node:assert/strict';

import { upstreamToMinimal } from '../lib/transform.js';

const NOW_MS = Date.parse('2026-05-21T20:00:00Z');

test('nominal payload returns floor-minute math', () => {
  // resets_at is 3h07m12s ahead of nowMs → floor((187*60_000 + 12_000)/60_000) = 187.
  const payload = {
    five_hour: {
      utilization: 42.3,
      resets_at: '2026-05-21T23:07:12Z',
    },
  };
  const out = upstreamToMinimal(payload, NOW_MS);
  assert.deepEqual(out, { utilization: 42.3, remaining_minutes: 187 });
});

test('five_hour missing throws invalid_upstream', () => {
  assert.throws(
    () => upstreamToMinimal({}, NOW_MS),
    /invalid_upstream: five_hour missing/,
  );
});

test('utilization missing throws invalid_upstream', () => {
  assert.throws(
    () =>
      upstreamToMinimal(
        { five_hour: { resets_at: '2026-05-21T22:00:00Z' } },
        NOW_MS,
      ),
    /invalid_upstream: five_hour.utilization missing/,
  );
});

test('resets_at missing throws invalid_upstream', () => {
  assert.throws(
    () => upstreamToMinimal({ five_hour: { utilization: 10 } }, NOW_MS),
    /invalid_upstream: five_hour.resets_at missing/,
  );
});

test('idle account (utilization 0, resets_at null) returns 0/0 without throwing', () => {
  const payload = {
    five_hour: { utilization: 0, resets_at: null },
  };
  const out = upstreamToMinimal(payload, NOW_MS);
  assert.deepEqual(out, { utilization: 0, remaining_minutes: 0 });
});

test('resets_at null with nonzero utilization still throws (malformed upstream)', () => {
  assert.throws(
    () =>
      upstreamToMinimal(
        { five_hour: { utilization: 10, resets_at: null } },
        NOW_MS,
      ),
    /invalid_upstream: five_hour.resets_at missing/,
  );
});

test('resets_at unparseable throws invalid_upstream', () => {
  assert.throws(
    () =>
      upstreamToMinimal(
        { five_hour: { utilization: 10, resets_at: 'not-a-date' } },
        NOW_MS,
      ),
    /invalid_upstream: five_hour.resets_at is unparseable/,
  );
});

test('resets_at in the past returns remaining_minutes: 0', () => {
  const payload = {
    five_hour: {
      utilization: 12.5,
      resets_at: '2026-05-21T19:00:00Z', // 1h before NOW_MS
    },
  };
  const out = upstreamToMinimal(payload, NOW_MS);
  assert.equal(out.utilization, 12.5);
  assert.equal(out.remaining_minutes, 0);
});

test('utilization above 100 (overage) is passed through unchanged', () => {
  const payload = {
    five_hour: {
      utilization: 150,
      resets_at: '2026-05-21T21:30:00Z', // 90 min ahead
    },
  };
  const out = upstreamToMinimal(payload, NOW_MS);
  assert.equal(out.utilization, 150);
  assert.equal(out.remaining_minutes, 90);
});

test('payload itself null/array throws', () => {
  assert.throws(() => upstreamToMinimal(null, NOW_MS), /invalid_upstream/);
  assert.throws(() => upstreamToMinimal([], NOW_MS), /invalid_upstream/);
});
