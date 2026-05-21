#!/usr/bin/env node
// Print Claude Code subscription usage (same data as the /usage slash command).
// Usage:
//   claude-usage-node            # pretty JSON
//   claude-usage-node --raw      # raw JSON, one line
//   claude-usage-node --summary  # short human-readable summary
//   claude-usage-node --help

const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { execSync } = require("node:child_process");

const ENDPOINT = "https://api.anthropic.com/api/oauth/usage";
const CREDS = process.env.CLAUDE_CREDENTIALS_FILE
  || path.join(os.homedir(), ".claude", ".credentials.json");

function die(msg, code = 1) {
  process.stderr.write(`error: ${msg}\n`);
  process.exit(code);
}

function loadToken() {
  if (!fs.existsSync(CREDS)) {
    die(`cannot read credentials file: ${CREDS}\nhint: run 'claude auth login' first`);
  }
  let creds;
  try {
    creds = JSON.parse(fs.readFileSync(CREDS, "utf8"));
  } catch (e) {
    die(`credentials file is not valid JSON: ${e.message}`);
  }
  const token = creds?.claudeAiOauth?.accessToken;
  if (!token) die(`no accessToken found in ${CREDS}`);
  const expiresAt = creds?.claudeAiOauth?.expiresAt;
  if (expiresAt && Date.now() > expiresAt) {
    process.stderr.write("warning: access token appears expired; request may fail\n");
  }
  return token;
}

function claudeVersion() {
  try {
    return execSync("claude --version", { stdio: ["ignore", "pipe", "ignore"] })
      .toString().trim().split(/\s+/)[0] || "2.1.146";
  } catch {
    return "2.1.146";
  }
}

async function fetchUsage() {
  const token = loadToken();
  const res = await fetch(ENDPOINT, {
    headers: {
      "Authorization": `Bearer ${token}`,
      "anthropic-beta": "oauth-2025-04-20",
      "User-Agent": `claude-cli/${claudeVersion()} (external, cli)`,
    },
  });
  const body = await res.text();
  if (!res.ok) die(`HTTP ${res.status} from ${ENDPOINT}\n${body}`);
  try {
    return JSON.parse(body);
  } catch {
    die(`response was not JSON:\n${body}`);
  }
}

function fmtResets(iso) {
  if (!iso) return "no reset window";
  const dt = new Date(iso);
  const deltaMs = dt - Date.now();
  if (deltaMs <= 0) return "reset due";
  const hrs = Math.floor(deltaMs / 3600000);
  const mins = Math.floor((deltaMs % 3600000) / 60000);
  return `resets in ${hrs}h${String(mins).padStart(2, "0")}m`;
}

function printSummary(d) {
  const rows = [
    ["5-hour window",  d.five_hour],
    ["7-day rolling",  d.seven_day],
    ["7-day Opus",     d.seven_day_opus],
    ["7-day Sonnet",   d.seven_day_sonnet],
  ];
  console.log("Claude usage");
  for (const [label, v] of rows) {
    if (!v || v.utilization == null) continue;
    const util = v.utilization.toFixed(1).padStart(5);
    console.log(`  ${label.padEnd(20)} ${util}%   ${fmtResets(v.resets_at)}`);
  }
  const extra = d.extra_usage || {};
  if (extra.is_enabled) {
    console.log(`  extra credits        ${(extra.utilization ?? 0).toFixed(1)}%  (${extra.used_credits} / ${extra.monthly_limit} ${extra.currency || ""})`);
  } else {
    console.log("  extra credits        disabled");
  }
}

function printHelp() {
  console.log("usage: claude-usage-node [--json|--raw|--summary|--help]");
  console.log("  --json      pretty-printed JSON (default)");
  console.log("  --raw       single-line raw JSON");
  console.log("  --summary   short human-readable summary");
}

(async () => {
  const arg = process.argv[2] || "--json";
  if (arg === "-h" || arg === "--help") return printHelp();
  const data = await fetchUsage();
  switch (arg) {
    case "--raw":     console.log(JSON.stringify(data)); break;
    case "--summary": printSummary(data); break;
    case "--json":    console.log(JSON.stringify(data, null, 2)); break;
    default:
      process.stderr.write(`unknown flag: ${arg}\n`);
      printHelp();
      process.exit(2);
  }
})().catch((e) => die(e.message || String(e)));
