// The cached boards of 20261003000000_leaderboard_cache_and_sync.sql, in an
// in-process Postgres. The api_get_leaderboard it replaces is kept under
// another name while the migrations run, so both answer from the same rows.

const test = require('node:test');
const assert = require('node:assert');
const { createHandlers } = require('../src/handlers');
const { openDatabase, loadPGlite, migration, rollback } = require('../dev/database');
const { seed, serverId, playerId, NEW_DEFAULT } = require('../dev/seed');

const NEW_MIGRATION = '20261003000000_leaderboard_cache_and_sync.sql';
const OLD_MIGRATION = '20261001030000_leaderboard_pages.sql';
const KEEP_OLD =
  'ALTER FUNCTION public.api_get_leaderboard(uuid, text, text, boolean, integer, integer, text) RENAME TO api_get_leaderboard_before;';
const ARGS = '($1::uuid, $2::text, $3::text, $4::boolean, $5::integer, $6::integer, $7::text)';
const SORTS = ['score', 'kills', 'deaths', 'kd', 'hvt', 'guard', 'obj', 'transport', 'insertions', 'players'];
const SKIP = loadPGlite() ? false : 'install dev dependencies to run the database tests';

const SERVERS = 60;
const PLAYERS = 1500;
const HOME = serverId(1);
// Has statistics on several servers, has a rating and no statistics, is on no board.
const PLAYER = playerId(700);
const PILOT_ONLY = playerId(PLAYERS + 3);
const NOBODY = playerId(999999);

function get(query) {
  return { text: async () => '', query: new URLSearchParams(query) };
}

test('cached leaderboards', { skip: SKIP }, async (t) => {
  const { pg, db } = await openDatabase({ before: { [NEW_MIGRATION]: KEEP_OLD } });
  t.after(() => pg.close());
  await seed(pg, { servers: SERVERS, players: PLAYERS });

  // Rows that tie on every sort, a name of exactly 48 and of 49 characters, an
  // inactive server and a score below zero.
  await pg.exec(`
    INSERT INTO public.player_stats (player_bohemia_id, server_id, player_name_last_seen, kills, deaths, hvt_kills, hvt_guard_kills, contribution_score)
    VALUES ('tie-a', '${HOME}', 'Tie A', 5, 5, 1, 1, 600),
           ('tie-b', '${HOME}', 'Tie B', 5, 5, 1, 1, 600),
           ('tie-c', '${serverId(2)}', 'Tie C', 5, 5, 1, 1, 600),
           ('name-48', '${HOME}', '${'a'.repeat(48)}', 1, 0, 0, 0, 0),
           ('name-49', '${HOME}', '${'b'.repeat(49)}', 1, 0, 0, 0, 0),
           ('below-zero', '${HOME}', 'Below Zero', 0, 900, 0, 0, 0);
    INSERT INTO public.servers (id, name, is_active) VALUES ('${serverId(900)}', 'Synthetic Inactive', false);
    INSERT INTO public.player_stats (player_bohemia_id, server_id, player_name_last_seen, kills)
    VALUES ('inactive-only', '${serverId(900)}', 'Inactive Only', 3);
  `);

  async function refresh() {
    await pg.exec("SELECT public.ia_refresh_board('global', true); SELECT public.ia_refresh_board('servers', true);");
  }
  await refresh();

  const before = params => db.scalar('SELECT public.api_get_leaderboard_before' + ARGS + ' AS value', params);
  const after = params => db.scalar('SELECT public.api_get_leaderboard' + ARGS + ' AS value', params);
  const state = async board => (await pg.query(
    'SELECT refreshed_at, total, etag, rows_written FROM public.leaderboard_cache_state WHERE board = $1', [board])).rows[0];
  const age = (board, seconds) => pg.query(
    "UPDATE public.leaderboard_cache_state SET refreshed_at = now() - make_interval(secs => $2) WHERE board = $1",
    [board, seconds]);

  async function same(params) {
    const [was, is] = [await before(params), await after(params)];
    assert.deepStrictEqual(is, was, JSON.stringify(params));
    // Byte for byte too: the game finds its keys by string search.
    assert.strictEqual(JSON.stringify(is), JSON.stringify(was));
    return is;
  }

  await t.test('every board, sort and direction answers as it did before', async () => {
    const totals = {};
    for (const board of ['global', 'server', 'servers']) {
      const players = board === 'servers' ? [null] : [PLAYER, PILOT_ONLY, 'tie-b', NOBODY, null];
      totals[board] = (await after([HOME, board, 'score', true, 0, 0, null])).total;
      const last = Math.max(totals[board] - 7, 0);
      for (const sort of SORTS) {
        for (const desc of [true, false]) {
          let i = 0;
          for (const [offset, limit] of [[0, 25], [13, 100], [last, 25], [totals[board] + 50, 25], [0, 0]]) {
            const answer = await same([HOME, board, sort, desc, offset, limit, players[i++ % players.length]]);
            assert.strictEqual(answer.rows.length, Math.max(Math.min(limit, totals[board] - offset), 0));
          }
        }
      }
    }
    // The seeded players, the fifteen who only have a rating, and the seven added above.
    assert.strictEqual(totals.global, PLAYERS + 15 + 7);
    assert.ok(totals.server > 100);
    assert.ok(totals.servers > 40 && totals.servers < SERVERS);
  });

  await t.test('a whole board walked a page at a time is the old board, with no row missed or repeated', async () => {
    for (const [sort, desc] of [['kd', false], ['transport', true], ['players', true]]) {
      const total = (await after([HOME, 'global', sort, desc, 0, 0, null])).total;
      const ranks = [];
      for (let offset = 0; offset < total; offset += 100) {
        const page = await same([HOME, 'global', sort, desc, offset, 100, null]);
        for (const row of page.rows)
          ranks.push(row.r);
      }
      assert.strictEqual(ranks.length, total);
      assert.ok(ranks.every((rank, i) => rank === i + 1));
    }
  });

  await t.test('an own row is found at the rank the page shows it at, in every sort', async () => {
    for (const sort of SORTS) {
      for (const desc of [true, false]) {
        for (const id of ['tie-a', 'tie-b', 'tie-c', PLAYER, 'below-zero']) {
          const own = await same([HOME, 'global', sort, desc, 0, 0, id]);
          assert.strictEqual(own.me.length, 1);
          assert.deepStrictEqual(own.rows, []);
          const page = await after([HOME, 'global', sort, desc, own.me[0].r - 1, 1, null]);
          assert.deepStrictEqual(page.rows[0], own.me[0]);
        }
      }
    }
    // Ties are broken by id, so three players level on everything stay in order.
    const ranks = [];
    for (const id of ['tie-a', 'tie-b', 'tie-c'])
      ranks.push((await after([HOME, 'global', 'kills', true, 0, 0, id])).me[0].r);
    assert.deepStrictEqual(ranks, [ranks[0], ranks[0] + 1, ranks[0] + 2]);
  });

  await t.test('names are cut at 48 characters and default-named and inactive servers are left out', async () => {
    const cut = await same([HOME, 'global', 'score', true, 0, 0, 'name-49']);
    assert.strictEqual(cut.me[0].n, 'b'.repeat(48));
    const whole = await same([HOME, 'global', 'score', true, 0, 0, 'name-48']);
    assert.strictEqual(whole.me[0].n, 'a'.repeat(48));

    const names = (await pg.query("SELECT name FROM public.leaderboard_cache WHERE board = 'servers'")).rows.map(r => r.name);
    assert.ok(names.length > 0);
    assert.ok(names.every(name => name !== NEW_DEFAULT && name !== 'My IA Server' && name !== 'Synthetic Inactive'));
    assert.ok(names.every(name => !name.startsWith('Server Name -PLEASE RENAME')));
    assert.ok(names.some(name => name.startsWith('Synthetic.txt - PLEASE RENAME')));
    assert.ok(names.some(name => name.length > 48));

    // A server on a default name has no own row on the servers board.
    const hidden = await same([serverId(20), 'servers', 'score', true, 0, 5, null]);
    assert.deepStrictEqual(hidden.me, []);
    const shown = await same([HOME, 'servers', 'score', true, 0, 5, 'ignored']);
    assert.strictEqual(shown.me.length, 1);
  });

  await t.test('an unknown or inactive server is refused, and unknown values fall back as before', async () => {
    assert.strictEqual(await after([serverId(4242), 'global', 'score', true, 0, 25, null]), null);
    assert.strictEqual(await after([serverId(900), 'global', 'score', true, 0, 25, null]), null);
    await same([HOME, 'nonsense', 'nonsense', null, -5, 500, null]);
    await same([HOME, null, null, null, null, null, null]);
  });

  await t.test('a board is rebuilt at most once per interval, and only the rows that changed are written', async () => {
    const was = await after([HOME, 'global', 'kills', true, 0, 0, PLAYER]);
    const rebuilt = (await state('global')).refreshed_at;
    await pg.query(
      'UPDATE public.player_stats SET kills = kills + 1000 WHERE player_bohemia_id = $1 AND server_id = (SELECT min(server_id::text)::uuid FROM public.player_stats WHERE player_bohemia_id = $1)',
      [PLAYER]);

    // Not due: the board is served as it was.
    assert.strictEqual((await pg.query("SELECT public.ia_refresh_board('global') AS value")).rows[0].value, false);
    assert.deepStrictEqual(await after([HOME, 'global', 'kills', true, 0, 0, PLAYER]), was);
    assert.deepStrictEqual((await state('global')).refreshed_at, rebuilt);
    await age('global', 59);
    assert.deepStrictEqual(await after([HOME, 'global', 'kills', true, 0, 0, PLAYER]), was);

    // Due: the next caller rebuilds it and sees the change.
    await age('global', 61);
    const is = await same([HOME, 'global', 'kills', true, 0, 0, PLAYER]);
    assert.strictEqual(is.me[0].k, was.me[0].k + 1000);
    assert.strictEqual(is.me[0].r, 1);
    const now = await state('global');
    assert.strictEqual(now.rows_written, 1);
    assert.ok(now.refreshed_at > rebuilt);

    // And nothing is written when nothing changed.
    await age('global', 61);
    await after([HOME, 'global', 'score', true, 0, 25, null]);
    assert.strictEqual((await state('global')).rows_written, 0);
  });

  await t.test('rows that left the source leave the cache', async () => {
    await refresh();
    const total = (await state('global')).total;
    await pg.exec("DELETE FROM public.player_stats WHERE player_bohemia_id = 'inactive-only'");
    await pg.query('UPDATE public.servers SET name = $2 WHERE id = $1', [serverId(3), NEW_DEFAULT]);
    await pg.query('UPDATE public.servers SET is_active = false WHERE id = $1', [serverId(4)]);
    const servers = (await state('servers')).total;

    await age('global', 61);
    await age('servers', 61);
    assert.strictEqual((await same([HOME, 'global', 'score', true, 0, 0, 'inactive-only'])).total, total - 1);
    assert.strictEqual((await same([HOME, 'servers', 'score', true, 0, 100, null])).total, servers - 2);
    assert.strictEqual((await state('global')).rows_written, 1);
    assert.strictEqual((await state('servers')).rows_written, 2);
  });

  await t.test('a caller that cannot take the rebuild lock serves the board as it was', async () => {
    const real = (await pg.query("SELECT pg_get_functiondef('public.ia_board_refresh_lock(text)'::regprocedure) AS def")).rows[0].def;
    const was = await after([HOME, 'global', 'deaths', true, 0, 25, 'tie-a']);
    await pg.exec("UPDATE public.player_stats SET deaths = deaths + 5000 WHERE player_bohemia_id = 'tie-a'");

    // As if another caller held the lock.
    await pg.exec('CREATE OR REPLACE FUNCTION public.ia_board_refresh_lock(p_board text) RETURNS boolean LANGUAGE sql AS $f$ SELECT false $f$;');
    await age('global', 600);
    const stale = (await state('global')).refreshed_at;
    assert.strictEqual((await pg.query("SELECT public.ia_refresh_board('global') AS value")).rows[0].value, false);
    assert.deepStrictEqual(await after([HOME, 'global', 'deaths', true, 0, 25, 'tie-a']), was);
    assert.deepStrictEqual((await state('global')).refreshed_at, stale);

    // The lock is free again: the next caller rebuilds.
    await pg.exec(real);
    const is = await same([HOME, 'global', 'deaths', true, 0, 25, 'tie-a']);
    assert.strictEqual(is.me[0].d, was.me[0].d + 5000);
    assert.strictEqual(is.me[0].r, 1);
  });

  await t.test('the etag follows the top of the board and nothing else', async () => {
    const etag = (await state('global')).etag;
    assert.match(etag, /^[0-9a-f]{32}$/);
    const snapshot = (await pg.query("SELECT jsonb_array_length(snapshot) AS n FROM public.leaderboard_cache_state WHERE board = 'global'")).rows[0].n;
    assert.strictEqual(snapshot, 100);

    // A change far below the top 100.
    await pg.exec("UPDATE public.player_stats SET kills = kills + 1 WHERE player_bohemia_id = 'below-zero'");
    await pg.exec("SELECT public.ia_refresh_board('global', true)");
    assert.strictEqual((await state('global')).rows_written, 1);
    assert.strictEqual((await state('global')).etag, etag);

    // A change at the top.
    const top = (await pg.query("SELECT id FROM public.leaderboard_cache WHERE board = 'global' ORDER BY score DESC, id LIMIT 1")).rows[0].id;
    await pg.query('UPDATE public.player_stats SET kills = kills + 1 WHERE player_bohemia_id = $1', [top]);
    await pg.exec("SELECT public.ia_refresh_board('global', true)");
    assert.notStrictEqual((await state('global')).etag, etag);
  });

  await t.test('the snapshot is the first page by score, as the old function gave it', async () => {
    await refresh();
    for (const board of ['global', 'servers']) {
      const snapshot = (await pg.query('SELECT snapshot FROM public.leaderboard_cache_state WHERE board = $1', [board])).rows[0].snapshot;
      assert.deepStrictEqual(snapshot, (await before([HOME, board, 'score', true, 0, 100, null])).rows);
    }
  });

  await t.test('a page served from memory is the page the database gave, and an own row is still fresh', async () => {
    const handlers = createHandlers(db);
    const asks = [
      { board: 'global', sort: 'kd', dir: 'asc', offset: '40', limit: '30' },
      { board: 'servers', sort: 'players' }
    ];
    for (const ask of asks) {
      const query = Object.assign({ serverGuid: HOME }, ask);
      const first = await handlers.getLeaderboard(get(query));
      const held = await handlers.getLeaderboard(get(query));
      assert.strictEqual(held.body, first.body);

      const mine = await handlers.getLeaderboard(get(Object.assign({ playerId: PLAYER }, query)));
      const direct = await before([HOME, ask.board, ask.sort, ask.dir !== 'asc', Number(ask.offset || 0), Number(ask.limit || 25), PLAYER]);
      assert.strictEqual(mine.body, JSON.stringify(direct));
    }
    assert.strictEqual((await handlers.getLeaderboard(get({ serverGuid: serverId(4242), board: 'global', sort: 'kd', dir: 'asc', offset: '40', limit: '30' }))).status, 403);
  });

  await t.test('the API role can read and rebuild the boards through the functions and nothing else', async () => {
    await age('global', 600);
    await pg.exec('SET ROLE ia_game_api');
    try {
      const page = await after([HOME, 'global', 'score', true, 0, 25, PLAYER]);
      assert.strictEqual(page.rows.length, 25);
      await assert.rejects(pg.query('SELECT count(*) FROM public.leaderboard_cache'));
      await assert.rejects(pg.query('SELECT count(*) FROM public.leaderboard_cache_state'));
      await assert.rejects(pg.query('SELECT count(*) FROM public.stats_batches'));
      await assert.rejects(pg.query("SELECT public.ia_refresh_board('global', true)"));
      await assert.rejects(pg.query('SELECT public.ia_board_refresh_seconds()'));
    } finally {
      await pg.exec('RESET ROLE');
    }
    assert.ok((await state('global')).refreshed_at > new Date(Date.now() - 60000));

    const grants = (await pg.query(`
      SELECT routine_name, string_agg(grantee, ',' ORDER BY grantee) AS grantees
        FROM information_schema.routine_privileges
       WHERE routine_schema = 'public' AND routine_name IN ('api_get_leaderboard', 'api_sync')
         AND grantee NOT IN (SELECT rolname FROM pg_roles WHERE rolsuper)
       GROUP BY routine_name ORDER BY routine_name`)).rows;
    assert.deepStrictEqual(grants, [
      { routine_name: 'api_get_leaderboard', grantees: 'ia_game_api,service_role' },
      { routine_name: 'api_sync', grantees: 'ia_game_api,service_role' }
    ]);

    const helpers = (await pg.query(`
      SELECT p.proname FROM pg_proc p
       WHERE p.pronamespace = 'public'::regnamespace
         AND p.proname IN ('ia_refresh_board', 'ia_board_refresh_lock', 'ia_board_row_json', 'ia_board_refresh_seconds', 'ia_board_snapshot_rows')
         AND (has_function_privilege('ia_game_api', p.oid, 'EXECUTE') OR has_function_privilege('anon', p.oid, 'EXECUTE')
              OR has_function_privilege('authenticated', p.oid, 'EXECUTE'))`)).rows;
    assert.deepStrictEqual(helpers, []);

    const fn = (await pg.query(
      "SELECT prosecdef, proconfig, pg_get_function_identity_arguments(oid) AS args FROM pg_proc WHERE proname = 'api_get_leaderboard'")).rows;
    assert.strictEqual(fn.length, 1);
    assert.strictEqual(fn[0].prosecdef, true);
    assert.deepStrictEqual(fn[0].proconfig, ['search_path=public']);
    assert.strictEqual(fn[0].args,
      'p_server_guid uuid, p_board text, p_sort text, p_desc boolean, p_offset integer, p_limit integer, p_player_id text');
  });

  await t.test('the migration can be run again and keeps the boards', async () => {
    const total = (await state('global')).total;
    await pg.exec(migration(NEW_MIGRATION));
    assert.strictEqual((await state('global')).total, total);
    await same([HOME, 'global', 'score', true, 0, 25, PLAYER]);
  });

  await t.test('the rollback puts the old function back, removes the rest, and the migration applies again', async () => {
    // The rollback holds the function of 20261001030000, word for word.
    function leaderboardFunction(sql) {
      const text = sql.replace(/\r\n/g, '\n');
      const start = text.indexOf('CREATE OR REPLACE FUNCTION public.api_get_leaderboard(');
      const end = text.indexOf('\n$$;', start);
      assert.ok(start > 0 && end > start);
      return text.slice(start, end);
    }
    assert.strictEqual(leaderboardFunction(rollback(NEW_MIGRATION)), leaderboardFunction(migration(OLD_MIGRATION)));

    const was = await before([HOME, 'global', 'kd', true, 10, 25, PLAYER]);
    await pg.exec(rollback(NEW_MIGRATION));
    assert.deepStrictEqual(await after([HOME, 'global', 'kd', true, 10, 25, PLAYER]), was);
    const left = (await pg.query(`
      SELECT (SELECT count(*) FROM pg_class WHERE relnamespace = 'public'::regnamespace
                AND relname IN ('leaderboard_cache', 'leaderboard_cache_state', 'stats_batches'))::integer AS tables,
             (SELECT count(*) FROM pg_proc WHERE pronamespace = 'public'::regnamespace
                AND proname IN ('api_sync', 'ia_refresh_board', 'ia_board_refresh_lock', 'ia_board_row_json',
                                'ia_board_refresh_seconds', 'ia_board_snapshot_rows'))::integer AS functions,
             (SELECT provolatile FROM pg_proc WHERE proname = 'api_get_leaderboard') AS volatility,
             has_function_privilege('ia_game_api', 'public.api_get_leaderboard(uuid, text, text, boolean, integer, integer, text)', 'EXECUTE') AS granted`)).rows[0];
    assert.deepStrictEqual(left, { tables: 0, functions: 0, volatility: 's', granted: true });

    // With api_sync gone the route says so, and the page route carries on.
    const handlers = createHandlers(db);
    const sync = await handlers.sync({ text: async () => JSON.stringify({ serverGuid: HOME }), query: new URLSearchParams() });
    assert.strictEqual(sync.status, 501);
    assert.strictEqual(sync.body, 'Sync is not available.');
    assert.strictEqual((await handlers.getLeaderboard(get({ serverGuid: HOME }))).status, 200);

    // Running the rollback twice is harmless, and the migration goes back on.
    await pg.exec(rollback(NEW_MIGRATION));
    await pg.exec(migration(NEW_MIGRATION));
    assert.deepStrictEqual(await after([HOME, 'global', 'kd', true, 10, 25, PLAYER]), was);
    assert.strictEqual((await state('global')).total, was.total);
  });
});
