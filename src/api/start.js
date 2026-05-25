// start.js
//
// Process entry point. Loads .env from this file's directory (so pm2 / systemd
// can start the service from any cwd), then constructs the Express app and
// listens. server.js stays side-effect-free so tests can import { createApp }
// without spawning a listener.

import { config as dotenvConfig } from 'dotenv';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

import { createApp } from './server.js';

const here = dirname(fileURLToPath(import.meta.url));
dotenvConfig({ path: join(here, '.env') });

if (!process.env.CLAUDE_ENDPOINT) {
  process.stderr.write(
    'fatal: CLAUDE_ENDPOINT is not set (copy .env.example to .env)\n',
  );
  process.exit(1);
}

const app = createApp();
const port = Number(process.env.PORT) || 3000;
const host = process.env.HOST || '0.0.0.0';
app.listen(port, host, () => {
  process.stdout.write(
    `claude-usage-led-api listening on http://${host}:${port}\n`,
  );
});
