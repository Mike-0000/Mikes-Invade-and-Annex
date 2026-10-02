// Database time and answer size of the leaderboard calls, before and after
// 20261003000000_leaderboard_cache_and_sync.sql, on the seeded dev database.
//
//   npm run measure
//
// "before" is the api_get_leaderboard of 20261001030000, kept under another
// name while the new one is installed beside it.
//
// It runs on PGlite, which is several times slower than a real server; the
// ratio between before and after is what carries over. To time a real
// Postgres, name one on this machine and nowhere else:
//   IA_MEASURE_PG_URL=postgres://user:password@127.0.0.1:5432/postgres
// It makes a database called ia_measure there and drops it first if it exists.

const { performance } = require('node:perf_hooks');
const { openDatabase, buildDatabase, scalarDb } = require('./database');
const { seed, serverId, playerId } = require('./seed');

const NEW_MIGRATION = '20261003000000_leaderboard_cache_and_sync.sql';
const KEEP_OLD =
  'ALTER FUNCTION public.api_get_leaderboard(uuid, text, text, boolean, integer, integer, text) RENAME TO api_get_leaderboard_before;';
const RUNS = 25;

const PAGE = 'SELECT public.%NAME%($1::uuid, $2::text, $3::text, $4::boolean, $5::integer, $6::integer, $7::text) AS value';
const SYNC = 'SELECT public.api_sync($1::uuid, $2::text, $3::text, $4::jsonb, $5::text, $6::jsonb, $7::text[], $8::text[], $9::jsonb) AS value';

async function openLocalPostgres(url) {
  const target = new URL(url);
  if (target.hostname !== '127.0.0.1' && target.hostname !== 'localhost')
    throw new Error('IA_MEASURE_PG_URL must name a database on this machine.');

  const { Client } = require('pg');
  const admin = new Client({ connectionString: url });
  await admin.connect();
  await admin.query('DROP DATABASE IF EXISTS ia_measure');
  await admin.query('CREATE DATABASE ia_measure');
  await admin.end();

  target.pathname = '/ia_measure';
  const client = new Client({ connectionString: target.toString() });
  await client.connect();
  const pg = {
    exec: sql => client.query(sql),
    query: (sql, params) => client.query(sql, params),
    close: () => client.end()
  };
  await buildDatabase(pg, { before: { [NEW_MIGRATION]: KEEP_OLD } });
  return { pg, db: scalarDb(pg) };
}

function median(values) {
  const sorted = values.slice().sort((a, b) => a - b);
  return sorted[Math.floor(sorted.length / 2)];
}

async function main() {
  const url = process.env.IA_MEASURE_PG_URL;
  const opened = url
    ? await openLocalPostgres(url)
    : await openDatabase({ before: { [NEW_MIGRATION]: KEEP_OLD } });
  const { pg, db } = opened;
  const seeded = await seed(pg);
  await pg.exec('ANALYZE;');
  console.log((url ? 'Postgres on this machine' : 'PGlite') + ', seeded ' + JSON.stringify(seeded));

  const guid = serverId(1);
  const me = playerId(1234);
  const online = [];
  for (let n = 1; n <= 16; n++)
    online.push(playerId(n * 97));

  const results = [];
  async function time(label, sql, params) {
    const times = [];
    let bytes = 0;
    for (let i = 0; i < RUNS; i++) {
      const args = typeof params === 'function' ? params(i) : params;
      const started = performance.now();
      const value = await db.scalar(sql, args);
      times.push(performance.now() - started);
      bytes = Buffer.byteLength(JSON.stringify(value));
    }
    results.push({ call: label, ms: median(times).toFixed(1), bytes });
  }

  const pages = [
    ['page: global, score, first 25, with own row', ['global', 'score', true, 0, 25, me]],
    ['page: global, kd ascending, rows 5001-5025', ['global', 'kd', false, 5000, 25, me]],
    ['own row only: global (limit 0)', ['global', 'score', true, 0, 0, me]],
    ['own row only: global, by kills', ['global', 'kills', true, 0, 0, me]],
    ['page: servers, score, first 25', ['servers', 'score', true, 0, 25, null]],
    ['page: server, score, first 25, with own row', ['server', 'score', true, 0, 25, me]]
  ];
  for (const [label, args] of pages) {
    await time('before  ' + label, PAGE.replace('%NAME%', 'api_get_leaderboard_before'), [guid].concat(args));
    await time('after   ' + label, PAGE.replace('%NAME%', 'api_get_leaderboard'), [guid].concat(args));
  }

  // Syncs as the largest server sends them. The events are deaths of its
  // lowest player, which leave the top of every board as it is, or kills by
  // its best player, which change the top of its own board.
  const NAME = 'Synthetic Server 001';
  const ranked = (await pg.query(`
    SELECT r.id FROM public.ia_leaderboard_rows('server', $1::uuid) r ORDER BY r.score DESC, r.id`, [guid])).rows;
  const quiet = [];
  const loud = [];
  for (let n = 0; n < 4; n++) {
    quiet.push({ eventType: 'PlayerDeath', victimPlayerId: ranked[ranked.length - 1].id, victimPlayerName: 'Synthetic Player' });
    loud.push({ eventType: 'PlayerKill', killerPlayerId: ranked[0].id, killerPlayerName: 'Synthetic Player' });
  }
  const none = JSON.stringify([{ board: 'server', etags: [] }, { board: 'global', etags: [] }, { board: 'servers', etags: [] }]);
  const first = await db.scalar(SYNC, [guid, NAME, null, null, null, null, null, [], none]);
  const etags = {};
  for (const board of first.boards)
    etags[board.board] = board.etag;
  const held = JSON.stringify(['server', 'global', 'servers'].map(board => ({ board, etags: [etags[board]] })));

  let batch = 0;
  await time('after   sync: nothing to send, nothing wanted', SYNC,
    [guid, NAME, null, null, null, null, null, [], '[]']);
  await time('after   sync: stats only', SYNC,
    () => [guid, NAME, 'm' + (batch++), JSON.stringify(quiet), null, null, null, [], '[]']);
  await time('after   sync: stats, three boards held and unchanged', SYNC,
    () => [guid, NAME, 'm' + (batch++), JSON.stringify(quiet), null, null, null, [], held]);
  await time('after   sync: stats, three boards unchanged, 16 own lines', SYNC,
    () => [guid, NAME, 'm' + (batch++), JSON.stringify(quiet), null, null, null, online, held]);
  await time('after   sync: stats, own board changed, 16 own lines', SYNC,
    () => [guid, NAME, 'm' + (batch++), JSON.stringify(loud), null, null, null, online, held]);
  await time('after   sync: everything, nothing held, 16 own lines', SYNC,
    () => [guid, NAME, 'm' + (batch++), JSON.stringify(quiet), null, null, online, online, none]);
  await time('before  submitStats (api_submit_stats)',
    'SELECT public.api_submit_stats($1::uuid, $2::text, $3::jsonb) AS value',
    [guid, NAME, JSON.stringify(quiet)]);

  await time('after   rebuild: global board', 'SELECT public.ia_refresh_board($1, true) AS value', ['global']);
  await time('after   rebuild: servers board', 'SELECT public.ia_refresh_board($1, true) AS value', ['servers']);

  console.table(results);
  await pg.close();
}

main().catch(e => {
  console.error(e && e.message);
  process.exit(1);
});
