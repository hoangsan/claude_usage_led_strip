# Contract: `GET /five-hour`

The single HTTP interface between the Node.js API server and the ESP32 firmware. Per FR-007 the firmware must depend on no other endpoint or shape from the server.

---

## Endpoint

**Method**: `GET`

**Path**: `/five-hour`

**Full URL** (as the firmware sees it): set by the operator in `config.h` as `API_ENDPOINT`. Example: `http://192.168.1.42:3000/five-hour`.

**Transport**: Plain HTTP/1.1 over the local network. No TLS, no authentication, no cookies, no CORS preflight (per spec Assumptions: local-network only in v1).

**Request headers**: none required. The server ignores `Accept`, `User-Agent`, etc.

**Request body**: none. `GET` with no body, no query string.

---

## Successful response

**Status**: `200 OK`

**Headers**:
- `Content-Type: application/json; charset=utf-8`
- `Cache-Control: no-store` (always fresh)

**Body** (UTF-8 JSON, no leading/trailing whitespace beyond what `JSON.stringify` produces):

```json
{
  "utilization": 42.3,
  "remaining_minutes": 187
}
```

### Field definitions

| Field | Type | Constraints |
|---|---|---|
| `utilization` | JSON number | Finite, ≥ 0. May exceed 100 during overage. Decimal precision is whatever upstream provides; the firmware does not depend on precision beyond 0.1%. |
| `remaining_minutes` | JSON integer | Whole minutes, ≥ 0. Computed server-side as `max(0, floor((resets_at - now) / 60000))`. |

Both fields are **required**. Additional fields MUST NOT be added without coordinating a firmware update.

---

## Failure responses

The server distinguishes upstream failures from misuse:

| Scenario | Status | `error` | `detail` (one of)
|---|---|---|---|
| Upstream call failed (network, non-2xx, malformed JSON, expired token, missing `five_hour` object, unparseable `resets_at`) | `502 Bad Gateway` | `upstream_unavailable` | `credentials_unreadable` · `credentials_invalid_json` · `credentials_missing_token` · `upstream_timeout` · `upstream_request_failed` · `upstream_status_<N>` · `upstream_invalid_json` · `invalid_upstream: <short reason>` |
| Server misconfigured (e.g., `CLAUDE_ENDPOINT` missing in `.env`) | `500 Internal Server Error` | `server_misconfigured` | `CLAUDE_ENDPOINT not set` |
| Method other than `GET` on `/five-hour` | `405 Method Not Allowed` | `method_not_allowed` | — |
| Path other than `/five-hour` (and `/health`) | `404 Not Found` | `not_found` | — |

`detail` is a **stable, short identifier** — the server intentionally does NOT include filesystem paths, hostnames, native socket errors, or any other operator-side detail in the response body. Full diagnostics (with stack and cause) are written to the operator's stderr. New `detail` values may be added in future versions but existing ones will not change meaning.

**Firmware-side behavior** for any non-2xx response, any non-JSON body, any timeout, any missing/invalid field in a 2xx body: enter the error state (FR-011, FR-007 last clause). The firmware does not inspect the `error` / `detail` strings — they are for the operator's logs.

## Server-side caching (transparent to the firmware)

The server caches the most recent **successful** upstream payload for `CACHE_TTL_MS` (default 60 000 ms, configurable in `.env`). Subsequent requests within that window do NOT re-call the upstream Claude endpoint — they return the cached upstream payload re-transformed against the current wall clock, so `remaining_minutes` is always fresh. This is an internal performance and abuse-mitigation behavior and is **not observable** in the wire contract: the 200 body shape, status code, and `Cache-Control: no-store` header are unchanged. Any failure (fetch or transform) busts the cache so the next request re-fetches.

---

## Optional health endpoint

The server also exposes `GET /health` returning `200 OK` with `{"status":"ok"}` and `Content-Type: application/json`. The firmware does not use this endpoint; it exists for operator troubleshooting only.

---

## Timeouts

- Server upstream timeout: 10 s. If `fetch` to `CLAUDE_ENDPOINT` does not complete in 10 s, the server returns 502.
- Firmware client timeout: 5 s. If the server does not respond within 5 s, the firmware treats it as a failure (FR-011). This is tighter than the server's upstream timeout so that the firmware is not blocked behind a slow upstream — the server will return 502 to the next poll, and the firmware shows the error in the meantime.

---

## Versioning posture (v1)

This contract is v1 and there is no version field. Breaking changes require coordinated firmware + server updates. The contract is intentionally minimal so that future additions (e.g., `seven_day_utilization`) can be appended without breaking the firmware — as long as `utilization` and `remaining_minutes` keep their current names, types, and meanings.

---

## Example exchange

```text
> GET /five-hour HTTP/1.1
> Host: 192.168.1.42:3000

< HTTP/1.1 200 OK
< Content-Type: application/json; charset=utf-8
< Cache-Control: no-store
<
< {"utilization":42.3,"remaining_minutes":187}
```

Failure example:

```text
> GET /five-hour HTTP/1.1
> Host: 192.168.1.42:3000

< HTTP/1.1 502 Bad Gateway
< Content-Type: application/json; charset=utf-8
<
< {"error":"upstream_unavailable","detail":"HTTP 401 from api.anthropic.com"}
```
