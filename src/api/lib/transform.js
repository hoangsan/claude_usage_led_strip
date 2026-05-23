// transform.js
//
// Pure mapper from the upstream Claude usage payload to the minimal two-field
// contract the firmware consumes. No I/O, no globals other than the optional
// nowMs argument (which defaults to Date.now()).
//
// See specs/001-claude-usage-led/data-model.md §1 for the upstream shape and
// §2 for the minimal shape. See research.md §12 for the floor-minute rule.

function fail(reason) {
  throw new Error(`invalid_upstream: ${reason}`);
}

/**
 * @param {unknown} payload - parsed upstream JSON.
 * @param {number} [nowMs] - current wall-clock instant in ms since epoch.
 * @returns {{ utilization: number, remaining_minutes: number }}
 */
export function upstreamToMinimal(payload, nowMs = Date.now()) {
  if (payload === null || typeof payload !== 'object' || Array.isArray(payload)) {
    fail('payload is not an object');
  }
  const fiveHour = payload.five_hour;
  if (fiveHour === undefined || fiveHour === null) {
    fail('five_hour missing');
  }
  if (typeof fiveHour !== 'object' || Array.isArray(fiveHour)) {
    fail('five_hour is not an object');
  }
  const { utilization, resets_at: resetsAt } = fiveHour;
  if (utilization === undefined || utilization === null) {
    fail('five_hour.utilization missing');
  }
  if (typeof utilization !== 'number' || !Number.isFinite(utilization)) {
    fail('five_hour.utilization is not a finite number');
  }
  if (utilization < 0) {
    fail('five_hour.utilization is negative');
  }
  // Idle account: upstream sends utilization=0 with resets_at=null when no
  // 5-hour window is currently active. That's a healthy state, not an error —
  // surface it as 0% / 0 minutes so the firmware shows an empty strip rather
  // than the error blink.
  if (resetsAt === null && utilization === 0) {
    return { utilization: 0, remaining_minutes: 0 };
  }
  if (resetsAt === undefined || resetsAt === null) {
    fail('five_hour.resets_at missing');
  }
  if (typeof resetsAt !== 'string') {
    fail('five_hour.resets_at is not a string');
  }
  const resetsAtMs = Date.parse(resetsAt);
  if (Number.isNaN(resetsAtMs)) {
    fail('five_hour.resets_at is unparseable');
  }
  const remainingMinutes = Math.max(
    0,
    Math.floor((resetsAtMs - nowMs) / 60_000),
  );
  return {
    utilization,
    remaining_minutes: remainingMinutes,
  };
}
