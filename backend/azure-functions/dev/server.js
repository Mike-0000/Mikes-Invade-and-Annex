// Local stand-in for the API, for development only: the real handlers over an
// in-process Postgres with every migration applied and made-up data.
//
//   npm run dev            http://127.0.0.1:7071/api/<route>
//
// It is not part of the deployed app: the deploy zip holds src/ and the
// production dependencies, and this folder and PGlite are in neither.
//
//   * Any well-formed server GUID it has not seen is taken in as an active
//     server on first use and given a copy of one seeded server's players, so
//     no real GUID ever has to be written down and its own board is not empty.
//   * It logs one line per request: route, status, time taken and sizes.
//     Never a body.
//   * GET /dev/calls counts the requests per route; POST /dev/calls/reset
//     starts the count again.
//
// Settings (environment):
//   IA_DEV_PORT             port, default 7071
//   IA_DEV_PLAYERS          seeded players, default 20000
//   IA_DEV_SERVERS          seeded servers, default 100
//   IA_DEV_REFRESH_SECONDS  how old a cached board may get, default 60
//   and every setting of src/config.js.

const http = require('node:http');
const { createHandlers } = require('../src/handlers');
const { readConfig } = require('../src/config');
const { ROUTES } = require('../src/routes');
const { openDatabase } = require('./database');
const { seed, serverId, DEFAULT_PLAYERS, DEFAULT_SERVERS } = require('./seed');

const UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const MAX_BODY_BYTES = 2 * 1024 * 1024;
const DEFAULT_PORT = 7071;
// The seeded server whose players a newly seen GUID starts with.
const TEMPLATE_SERVER = 12;

function wholeNumber(text, fallback) {
  if (typeof text !== 'string' || !/^[0-9]{1,9}$/.test(text))
    return fallback;
  return Number(text);
}

function readBody(request) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    let size = 0;
    request.on('data', chunk => {
      size += chunk.length;
      if (size <= MAX_BODY_BYTES)
        chunks.push(chunk);
    });
    request.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
    request.on('error', reject);
  });
}

// options: { port, players, servers, refreshSeconds, config, quiet }.
// Returns { port, url, pg, calls, close }.
async function startServer(options) {
  const settings = options || {};
  const quiet = settings.quiet === true;
  const { pg, db } = await openDatabase({ legacy: true });

  if (settings.refreshSeconds !== undefined) {
    await pg.exec(
      'CREATE OR REPLACE FUNCTION public.ia_board_refresh_seconds() RETURNS integer LANGUAGE sql STABLE AS $f$ SELECT '
      + Math.max(0, Math.floor(settings.refreshSeconds)) + ' $f$;');
  }
  const seeded = await seed(pg, { players: settings.players, servers: settings.servers });

  const handlers = createHandlers(db, settings.config || readConfig(process.env));
  const routes = new Map();
  for (const entry of ROUTES)
    routes.set(entry.method + ' ' + entry.name, handlers[entry.handler]);

  const known = new Set();
  let adopted = 0;

  // A GUID that could be a server's, seen for the first time, becomes one.
  async function adopt(guid) {
    if (typeof guid !== 'string' || !UUID.test(guid))
      return;
    const id = guid.toLowerCase();
    if (known.has(id))
      return;
    known.add(id);

    const made = await pg.query(
      'INSERT INTO public.servers (id, name) VALUES ($1::uuid, $2) ON CONFLICT (id) DO NOTHING RETURNING 1',
      [id, 'Synthetic Dev Server ' + id.slice(0, 8)]);
    if (made.rows.length === 0)
      return;
    adopted += 1;

    await pg.query(`
      INSERT INTO public.player_stats
             (player_bohemia_id, server_id, player_name_last_seen,
              kills, deaths, hvt_kills, hvt_guard_kills, contribution_score, last_seen)
      SELECT player_bohemia_id, $1::uuid, player_name_last_seen,
             kills, deaths, hvt_kills, hvt_guard_kills, contribution_score, last_seen
        FROM public.player_stats WHERE server_id = $2::uuid`,
      [id, serverId(TEMPLATE_SERVER)]);
    await pg.query(`
      INSERT INTO public.player_transport_server
             (player_bohemia_id, server_id, rating, insertions, player_name_last_seen)
      SELECT player_bohemia_id, $1::uuid, rating, insertions, player_name_last_seen
        FROM public.player_transport_server WHERE server_id = $2::uuid`,
      [id, serverId(TEMPLATE_SERVER)]);
  }

  let calls = new Map();
  let since = new Date().toISOString();

  function count(route, status, requestBytes, responseBytes) {
    let entry = calls.get(route);
    if (!entry) {
      entry = { route, calls: 0, ok: 0, failed: 0, requestBytes: 0, responseBytes: 0, lastStatus: 0 };
      calls.set(route, entry);
    }
    entry.calls += 1;
    if (status >= 200 && status < 300)
      entry.ok += 1;
    else
      entry.failed += 1;
    entry.requestBytes += requestBytes;
    entry.responseBytes += responseBytes;
    entry.lastStatus = status;
  }

  function callReport() {
    const list = Array.from(calls.values()).sort((a, b) => a.route.localeCompare(b.route));
    let total = 0;
    for (const entry of list)
      total += entry.calls;
    return { since, total, adoptedServers: adopted, routes: list };
  }

  function send(response, status, body, headers) {
    const text = body || '';
    response.writeHead(status, Object.assign({ 'Content-Length': Buffer.byteLength(text) }, headers || {}));
    response.end(text);
    return Buffer.byteLength(text);
  }

  const server = http.createServer(async (request, response) => {
    const started = Date.now();
    const url = new URL(request.url, 'http://127.0.0.1');
    const text = await readBody(request).catch(() => '');

    if (url.pathname === '/dev/calls' && request.method === 'GET') {
      send(response, 200, JSON.stringify(callReport()), { 'Content-Type': 'application/json' });
      return;
    }
    if (url.pathname === '/dev/calls/reset' && request.method === 'POST') {
      calls = new Map();
      since = new Date().toISOString();
      send(response, 200, JSON.stringify(callReport()), { 'Content-Type': 'application/json' });
      return;
    }

    const name = url.pathname.startsWith('/api/') ? url.pathname.slice(5) : '';
    const handler = routes.get(request.method + ' ' + name);
    let status = 404;
    let sent = 0;
    let note = '';
    if (!handler) {
      // What the Functions host answers for a route it does not have.
      sent = send(response, 404, '');
    } else {
      try {
        let guid = url.searchParams.get('serverGuid');
        if (request.method === 'POST') {
          try {
            guid = JSON.parse(text).serverGuid;
          } catch (e) {
            guid = null;
          }
        }
        await adopt(guid);

        const result = await handler({ text: async () => text, query: url.searchParams });
        status = result.status;
        sent = send(response, result.status, result.body, result.headers);
      } catch (e) {
        status = 500;
        // The error's code only: its message can quote what was sent.
        note = ' error=' + ((e && e.code) || 'unknown');
        sent = send(response, 500, 'Internal error.');
      }
    }

    if (name !== '')
      count(request.method + ' /api/' + name, status, Buffer.byteLength(text), sent);
    if (!quiet) {
      console.log('[dev] ' + request.method + ' ' + url.pathname + ' ' + status + ' '
        + (Date.now() - started) + 'ms in=' + Buffer.byteLength(text) + 'B out=' + sent + 'B' + note);
    }
  });

  const wantedPort = settings.port === undefined ? DEFAULT_PORT : settings.port;
  await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(wantedPort, '127.0.0.1', resolve);
  });
  const port = server.address().port;

  async function close() {
    await new Promise(resolve => server.close(resolve));
    await pg.close();
  }

  return { port, url: 'http://127.0.0.1:' + port + '/api', pg, seeded, calls: callReport, close };
}

if (require.main === module) {
  const env = process.env;
  const options = {
    port: wholeNumber(env.IA_DEV_PORT, DEFAULT_PORT),
    players: wholeNumber(env.IA_DEV_PLAYERS, DEFAULT_PLAYERS),
    servers: wholeNumber(env.IA_DEV_SERVERS, DEFAULT_SERVERS)
  };
  if (env.IA_DEV_REFRESH_SECONDS !== undefined)
    options.refreshSeconds = wholeNumber(env.IA_DEV_REFRESH_SECONDS, 60);

  console.log('[dev] building the database and seeding it...');
  startServer(options).then(dev => {
    console.log('[dev] seeded ' + JSON.stringify(dev.seeded));
    console.log('[dev] API   ' + dev.url);
    console.log('[dev] calls http://127.0.0.1:' + dev.port + '/dev/calls');
  }).catch(e => {
    console.error('[dev] could not start: ' + (e && e.message));
    process.exit(1);
  });
}

module.exports = { startServer };
