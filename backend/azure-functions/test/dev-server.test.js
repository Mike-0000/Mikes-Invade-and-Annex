// The local dev server: the real handlers over HTTP on a seeded in-process
// Postgres. It must never be part of what is deployed.

const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { loadPGlite } = require('../dev/database');

const SKIP = loadPGlite() ? false : 'install dev dependencies to run the dev server tests';
const ROOT = path.join(__dirname, '..');
// Made up for this test. The dev server has never seen them.
const NEW_SERVER = '7e57ab1e-0000-4000-8000-000000000001';
const SECOND_SERVER = '7e57ab1e-0000-4000-8000-000000000002';
const MARKER = 'never-log-this-text';

test('dev server', { skip: SKIP }, async (t) => {
  const { startServer } = require('../dev/server');

  // Everything the server prints while the tests run.
  const lines = [];
  const realLog = console.log;
  console.log = (...args) => { lines.push(args.join(' ')); };
  t.after(() => { console.log = realLog; });

  const dev = await startServer({ port: 0, players: 400, servers: 30 });
  t.after(() => dev.close());
  const origin = 'http://127.0.0.1:' + dev.port;

  const calls = async () => (await fetch(origin + '/dev/calls')).json();
  const sync = body => fetch(dev.url + '/sync', { method: 'POST', body: JSON.stringify(body) });

  await t.test('the seed is made up, and of the size asked for', async () => {
    assert.strictEqual(dev.seeded.servers, 30);
    assert.strictEqual(dev.seeded.players, 400);
    assert.ok(dev.seeded.statRows > 400);
    assert.ok(dev.seeded.pilots > 0);

    const ids = (await dev.pg.query(`
      SELECT (SELECT count(*) FROM public.servers WHERE id::text NOT LIKE '5eed0000-0000-4000-8000-%')::integer AS servers,
             (SELECT count(*) FROM public.player_stats WHERE player_bohemia_id NOT LIKE '5eed0000-0000-4000-9000-%')::integer AS players,
             (SELECT count(*) FROM public.servers WHERE name LIKE 'Synthetic%' OR name LIKE '%PLEASE RENAME%' OR name = 'My IA Server')::integer AS named`)).rows[0];
    assert.deepStrictEqual(ids, { servers: 0, players: 0, named: 30 });
  });

  await t.test('a GUID it has not seen becomes an active server on first use', async () => {
    const page = await fetch(dev.url + '/leaderboard?serverGuid=' + NEW_SERVER + '&board=server');
    assert.strictEqual(page.status, 200);
    assert.strictEqual(page.headers.get('content-type'), 'application/json');
    const board = await page.json();
    assert.strictEqual(board.board, 'server');
    assert.ok(board.total > 0);

    const res = await sync({ serverGuid: SECOND_SERVER, boards: [{ board: 'global', etag: '' }], marker: MARKER });
    assert.strictEqual(res.status, 200);
    const body = await res.json();
    assert.strictEqual(body.status, 'ok');
    assert.strictEqual(body.boards[0].total, dev.seeded.globalBoard);
    assert.strictEqual(body.globalRows.length, 100);
    assert.strictEqual((await calls()).adoptedServers, 2);

    // Seen before: not adopted twice.
    await fetch(dev.url + '/leaderboard?serverGuid=' + NEW_SERVER.toUpperCase() + '&board=server');
    assert.strictEqual((await calls()).adoptedServers, 2);
  });

  await t.test('a GUID that is not well formed is refused, as in production', async () => {
    const res = await fetch(dev.url + '/leaderboard?serverGuid=not-a-guid');
    assert.strictEqual(res.status, 403);
    assert.strictEqual(await res.text(), 'Invalid or inactive server GUID.');
    assert.strictEqual((await sync({ serverGuid: 'not-a-guid' })).status, 403);
    assert.strictEqual((await sync('not json')).status, 400);
    assert.strictEqual((await calls()).adoptedServers, 2);
  });

  await t.test('a route it does not have is a 404 with no body', async () => {
    for (const url of [dev.url + '/nothing', dev.url + '/sync', origin + '/elsewhere']) {
      const res = await fetch(url);
      assert.strictEqual(res.status, 404);
      assert.strictEqual(await res.text(), '');
    }
  });

  await t.test('calls are counted per route and can be reset', async () => {
    const reset = await fetch(origin + '/dev/calls/reset', { method: 'POST' });
    assert.deepStrictEqual((await reset.json()).routes, []);

    await fetch(dev.url + '/leaderboard?serverGuid=' + NEW_SERVER);
    await fetch(dev.url + '/leaderboard?serverGuid=' + NEW_SERVER + '&offset=25');
    await fetch(dev.url + '/leaderboard?serverGuid=nope');
    await sync({ serverGuid: NEW_SERVER });

    const report = await calls();
    assert.strictEqual(report.total, 4);
    assert.deepStrictEqual(report.routes.map(r => [r.route, r.calls, r.ok, r.failed, r.lastStatus]), [
      ['GET /api/leaderboard', 3, 2, 1, 403],
      ['POST /api/sync', 1, 1, 0, 200]
    ]);
    assert.ok(report.routes[0].responseBytes > 0);
    assert.ok(report.routes[1].requestBytes > 0);
    // Reading the count is not itself counted.
    assert.strictEqual((await calls()).total, 4);
  });

  await t.test('it logs the route, the status, the time and the sizes, and never a body', async () => {
    await sync({ serverGuid: NEW_SERVER, serverName: MARKER, matchData: [{ eventType: 'PlayerKill', killerPlayerId: MARKER, killerPlayerName: MARKER }] });
    await fetch(dev.url + '/leaderboard?serverGuid=' + NEW_SERVER + '&playerId=' + MARKER);

    const requests = lines.filter(line => line.startsWith('[dev] '));
    assert.ok(requests.length >= 10);
    for (const line of requests)
      assert.match(line, /^\[dev\] (GET|POST) \/[A-Za-z/]+ \d{3} \d+ms in=\d+B out=\d+B( error=\w+)?$/);
    assert.ok(lines.every(line => !line.includes(MARKER)));
    assert.ok(lines.every(line => !line.includes(NEW_SERVER)));
  });
});

test('the deployed app does not hold the dev server', () => {
  // Nothing under src/ reaches into dev/, test/ or a dev dependency.
  const pending = [path.join(ROOT, 'src')];
  let files = 0;
  while (pending.length > 0) {
    const folder = pending.pop();
    for (const entry of fs.readdirSync(folder, { withFileTypes: true })) {
      const full = path.join(folder, entry.name);
      if (entry.isDirectory()) {
        pending.push(full);
        continue;
      }
      files += 1;
      const source = fs.readFileSync(full, 'utf8');
      for (const match of source.matchAll(/require\(\s*'([^']+)'\s*\)/g)) {
        const wanted = match[1];
        if (wanted.startsWith('.')) {
          const target = path.resolve(path.dirname(full), wanted);
          assert.ok(target.startsWith(path.join(ROOT, 'src')), entry.name + ' requires ' + wanted);
        } else {
          assert.ok(['@azure/functions', 'pg'].includes(wanted), entry.name + ' requires ' + wanted);
        }
      }
    }
  }
  assert.ok(files >= 6);

  const manifest = JSON.parse(fs.readFileSync(path.join(ROOT, 'package.json'), 'utf8'));
  assert.deepStrictEqual(Object.keys(manifest.dependencies).sort(), ['@azure/functions', 'pg']);
  assert.strictEqual(manifest.main, 'src/functions/*.js');

  const ignored = fs.readFileSync(path.join(ROOT, '.funcignore'), 'utf8').split(/\r?\n/);
  assert.ok(ignored.includes('dev/'));
  assert.ok(ignored.includes('test/'));
});
