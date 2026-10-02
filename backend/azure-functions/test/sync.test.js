// POST /sync end to end: the real handlers over an in-process Postgres with
// every migration applied.

const test = require('node:test');
const assert = require('node:assert');
const { createHandlers } = require('../src/handlers');
const { openDatabase, loadPGlite, migration } = require('../dev/database');

const SKIP = loadPGlite() ? false : 'install dev dependencies to run the database tests';
const UNKNOWN = '11111111-2222-4333-8444-555555555555';
const NEW_DEFAULT = 'Default Name - PLEASE RENAME IN server_name.txt, in I&A Server Profile Folder';
const ALL_BOARDS = [{ board: 'server', etag: '' }, { board: 'global', etag: '' }, { board: 'servers', etag: '' }];

function post(body) {
  return { text: async () => JSON.stringify(body), query: new URLSearchParams() };
}

function get(query) {
  return { text: async () => '', query: new URLSearchParams(query) };
}

function kills(player, name, n) {
  const events = [];
  for (let i = 0; i < n; i++)
    events.push({ eventType: 'PlayerKill', killerPlayerId: player, killerPlayerName: name });
  return events;
}

// What a test needs of one database: handlers, and a few questions to ask it.
function tools(pg, db, options) {
  const handlers = createHandlers(db, options);

  async function register(name) {
    const res = await handlers.registerServer(post({ serverName: name, ownerEmail: '' }));
    return JSON.parse(res.body).serverGuid;
  }
  async function sync(body) {
    const res = await handlers.sync(post(body));
    assert.strictEqual(res.status, 200, res.body);
    return JSON.parse(res.body);
  }
  async function number(sql, params) {
    return Number((await pg.query(sql, params)).rows[0].n);
  }
  const killsOf = (guid, player) => number(
    'SELECT COALESCE(sum(kills), 0) AS n FROM public.player_stats WHERE server_id = $1 AND player_bohemia_id = $2', [guid, player]);
  const ratingOf = player => number(
    'SELECT COALESCE(sum(rating), 0) AS n FROM public.player_transport_ratings WHERE player_bohemia_id = $1', [player]);
  const refresh = () => pg.exec(
    "SELECT public.ia_refresh_board('global', true); SELECT public.ia_refresh_board('servers', true);");

  return { handlers, register, sync, number, killsOf, ratingOf, refresh };
}

test('sync', { skip: SKIP }, async (t) => {
  const { pg, db } = await openDatabase({ legacy: true });
  t.after(() => pg.close());
  const { handlers, register, sync, number, killsOf, ratingOf, refresh } = tools(pg, db);

  await t.test('with nothing to send it marks the server seen and applies the name rule', async () => {
    const guid = await register(NEW_DEFAULT);
    const body = await sync({ serverGuid: guid, serverName: '  Alpha  ' });
    assert.deepStrictEqual(
      [body.status, body.statsStatus, body.transportStatus, body.ratingsStatus, body.boardsStatus],
      ['ok', 'none', 'none', 'none', 'none']);
    assert.deepStrictEqual([body.boards, body.serverRows, body.globalRows, body.serversRows, body.own, body.ratings], [[], [], [], [], [], []]);

    let row = (await pg.query('SELECT name, last_seen FROM public.servers WHERE id = $1', [guid])).rows[0];
    assert.strictEqual(row.name, 'Alpha');
    assert.ok(row.last_seen);

    // A placeholder, a blank and a missing name keep the real one.
    for (const name of [NEW_DEFAULT, '   ', undefined, 7])
      await sync({ serverGuid: guid, serverName: name });
    row = (await pg.query('SELECT name FROM public.servers WHERE id = $1', [guid])).rows[0];
    assert.strictEqual(row.name, 'Alpha');
  });

  await t.test('a stats batch is counted once however often it is sent', async () => {
    const guid = await register('Stats');
    const events = kills('s-p1', 'One', 3).concat(kills('s-p2', 'Two', 1));

    const first = await sync({ serverGuid: guid, statsBatchId: 'batch-1', matchData: events });
    assert.deepStrictEqual([first.statsStatus, first.statsPlayers], ['accepted', 2]);
    for (let i = 0; i < 3; i++) {
      const again = await sync({ serverGuid: guid, statsBatchId: 'batch-1', matchData: events });
      assert.deepStrictEqual([again.statsStatus, again.statsPlayers], ['duplicate', 2]);
    }
    assert.strictEqual(await killsOf(guid, 's-p1'), 3);

    // Another id is another batch, and an id belongs to the server that sent it.
    assert.strictEqual((await sync({ serverGuid: guid, statsBatchId: 'batch-2', matchData: events })).statsStatus, 'accepted');
    assert.strictEqual(await killsOf(guid, 's-p1'), 6);
    const other = await register('Stats Two');
    assert.strictEqual((await sync({ serverGuid: other, statsBatchId: 'batch-1', matchData: events })).statsStatus, 'accepted');
    assert.strictEqual(await killsOf(other, 's-p1'), 3);

    // Without an id every send counts, as on /submitStats.
    await sync({ serverGuid: guid, matchData: kills('s-p1', 'One', 1) });
    await sync({ serverGuid: guid, matchData: kills('s-p1', 'One', 1) });
    assert.strictEqual(await killsOf(guid, 's-p1'), 8);

    // An empty batch with an id is accepted and remembered too.
    assert.strictEqual((await sync({ serverGuid: guid, statsBatchId: 'empty', matchData: [] })).statsStatus, 'accepted');
    assert.strictEqual((await sync({ serverGuid: guid, statsBatchId: 'empty', matchData: [] })).statsStatus, 'duplicate');
  });

  await t.test('it stores what /submitStats stores', async () => {
    const a = await register('Route A');
    const b = await register('Route B');
    const events = kills('same-p1', 'One', 2).concat([
      { eventType: 'PlayerDeath', victimPlayerId: 'same-p1', victimPlayerName: 'One' },
      { eventType: 'CaptureContribution', playerId: 'same-p2', playerName: 'Two', score: '180' },
      { eventType: 'HVTKill', killerPlayerId: 'same-p2', killerPlayerName: 'Two' },
      { eventType: 'Nonsense', killerPlayerId: 'same-p3' }, 7, 'text', null
    ]);
    const old = await handlers.submitStats(post({ serverGuid: a, serverName: 'Route A', matchData: events }));
    const now = await sync({ serverGuid: b, serverName: 'Route B', statsBatchId: 'x', matchData: events });
    assert.strictEqual(old.body, '{"status":"ok","players":' + now.statsPlayers + '}');

    const rows = guid => pg.query(
      `SELECT player_bohemia_id, player_name_last_seen, kills, deaths, hvt_kills, hvt_guard_kills, contribution_score
         FROM public.player_stats WHERE server_id = $1 ORDER BY 1`, [guid]);
    assert.deepStrictEqual((await rows(b)).rows, (await rows(a)).rows);
    assert.ok((await rows(a)).rows.length >= 2);
  });

  await t.test('a transport batch is counted once, and ratings read in the same call include it', async () => {
    const guid = await register('Transport');
    const transport = { batchId: 't-1', entries: [{ playerId: 't-p1', playerName: 'Pilot', points: 20, insertions: 1 }] };

    const first = await sync({ serverGuid: guid, transport, ratingIds: ['t-p1', 'nobody'] });
    assert.deepStrictEqual([first.transportStatus, first.transportPlayers, first.ratingsStatus], ['accepted', 1, 'ok']);
    assert.deepStrictEqual(first.ratings, [{ rating: 20, playerId: 't-p1', insertions: 1 }]);
    assert.ok(first.skins.length > 0);
    assert.deepStrictEqual(Object.keys(first.skins[0]).sort(), ['key', 'required']);

    const again = await sync({ serverGuid: guid, transport });
    assert.deepStrictEqual([again.transportStatus, again.transportPlayers, again.ratingsStatus], ['duplicate', 0, 'none']);
    assert.strictEqual(await ratingOf('t-p1'), 20);

    // The older route and this one share their batch ids.
    const old = await handlers.submitTransport(post({ serverGuid: guid, batchId: 't-1', entries: transport.entries }));
    assert.strictEqual(old.body, '{"applied":false,"players":0}');
    const fresh = await handlers.submitTransport(post({ serverGuid: guid, batchId: 't-2', entries: transport.entries }));
    assert.strictEqual(fresh.body, '{"applied":true,"players":1}');
    assert.strictEqual((await sync({ serverGuid: guid, transport: { batchId: 't-2', entries: transport.entries } })).transportStatus, 'duplicate');
    assert.strictEqual(await ratingOf('t-p1'), 40);
  });

  await t.test('ratings alone answer as /getTransportRatings does', async () => {
    const guid = await register('Ratings');
    const body = await sync({ serverGuid: guid, ratingIds: ['t-p1'] });
    const old = JSON.parse((await handlers.getTransportRatings(post({ serverGuid: guid, playerIds: ['t-p1'] }))).body);
    assert.strictEqual(body.ratingsStatus, 'ok');
    assert.deepStrictEqual({ ratings: body.ratings, skins: body.skins }, old);
    assert.deepStrictEqual([body.statsStatus, body.transportStatus, body.boardsStatus], ['none', 'none', 'none']);

    const empty = await sync({ serverGuid: guid, ratingIds: [] });
    assert.deepStrictEqual([empty.ratingsStatus, empty.ratings], ['ok', []]);
    assert.ok(empty.skins.length > 0);
  });

  await t.test('boards come with an etag, a total and the first page by score, as /leaderboard gives it', async () => {
    const guid = await register('Boards');
    for (let p = 1; p <= 130; p++)
      await pg.query('INSERT INTO public.player_stats (player_bohemia_id, server_id, player_name_last_seen, kills) VALUES ($1, $2, $3, $4)',
        ['b-p' + p, guid, 'Board Player ' + p, p % 40]);
    await refresh();

    const body = await sync({ serverGuid: guid, boards: ALL_BOARDS });
    assert.strictEqual(body.boardsStatus, 'ok');
    assert.strictEqual(body.snapshotRows, 100);
    assert.deepStrictEqual(body.boards.map(b => b.board), ['server', 'global', 'servers']);
    for (const board of body.boards) {
      assert.match(board.etag, /^[0-9a-f]{32}$/);
      assert.strictEqual(board.unchanged, 0);
    }

    const page = async board => JSON.parse((await createHandlers(db).getLeaderboard(
      get({ serverGuid: guid, board, limit: '100' }))).body);
    const rows = { server: body.serverRows, global: body.globalRows, servers: body.serversRows };
    for (const board of body.boards) {
      const old = await page(board.board);
      assert.deepStrictEqual(rows[board.board], old.rows);
      assert.strictEqual(board.total, old.total);
    }
    assert.strictEqual(body.serverRows.length, 100);
    assert.strictEqual(body.boards[0].total, 130);

    // Only the boards asked for are answered.
    const one = await sync({ serverGuid: guid, boards: [{ board: 'servers', etag: '' }] });
    assert.deepStrictEqual(one.boards.map(b => b.board), ['servers']);
    assert.deepStrictEqual([one.serverRows, one.globalRows], [[], []]);
    assert.ok(one.serversRows.length > 0);
  });

  await t.test('a board the caller already holds comes back unchanged, with no rows', async () => {
    const guid = await register('Etags');
    await pg.query("INSERT INTO public.player_stats (player_bohemia_id, server_id, player_name_last_seen, kills) VALUES ('e-p1', $1, 'Etag Player', 9000)", [guid]);
    await refresh();

    const first = await sync({ serverGuid: guid, boards: ALL_BOARDS });
    const held = first.boards.map(b => ({ board: b.board, etag: b.etag }));

    // Handlers that hold nothing in memory, so only the caller's etag counts.
    const second = JSON.parse((await createHandlers(db).sync(post({ serverGuid: guid, boards: held }))).body);
    assert.deepStrictEqual(second.boards, first.boards.map(b => ({ board: b.board, etag: b.etag, total: b.total, unchanged: 1 })));
    assert.deepStrictEqual([second.serverRows, second.globalRows, second.serversRows], [[], [], []]);

    // In the database itself: the rows of a held snapshot are not read out.
    const direct = await db.scalar(
      'SELECT public.api_sync($1::uuid, $2::text, $3::text, $4::jsonb, $5::text, $6::jsonb, $7::text[], $8::text[], $9::jsonb) AS value',
      [guid, '', null, null, null, null, null, [], JSON.stringify(held.map(h => ({ board: h.board, etags: ['stale', h.etag] })))]);
    assert.deepStrictEqual(direct.boards.map(b => b.unchanged), [true, true, true]);
    assert.deepStrictEqual([direct.serverRows, direct.globalRows, direct.serversRows], [[], [], []]);

    // The top of the boards moves: new etags, and the rows again.
    await sync({ serverGuid: guid, statsBatchId: 'e-1', matchData: kills('e-p1', 'Etag Player', 5) });
    await refresh();
    const third = JSON.parse((await createHandlers(db).sync(post({ serverGuid: guid, boards: held }))).body);
    assert.deepStrictEqual(third.boards.map(b => b.unchanged), [0, 0, 0]);
    for (let i = 0; i < 3; i++)
      assert.notStrictEqual(third.boards[i].etag, first.boards[i].etag);
    assert.strictEqual(third.serverRows[0].k, 9005);
    assert.strictEqual(third.globalRows[0].k, 9005);
  });

  await t.test('own lines are the rows /leaderboard gives each player, and are sent whether or not a board changed', async () => {
    const guid = await register('Own Lines');
    const other = await register('Own Lines Two');
    await sync({ serverGuid: guid, matchData: kills('o-p1', 'Own One', 4).concat(kills('o-p2', 'Own Two', 2)) });
    await sync({ serverGuid: other, matchData: kills('o-p1', 'Own One', 10).concat(kills('o-p3', 'Own Three', 1)) });
    await refresh();

    const first = await sync({ serverGuid: guid, boards: ALL_BOARDS, players: ['o-p1', 'o-p2', 'o-p3', 'o-nobody', 'o-p1'] });
    const held = first.boards.map(b => ({ board: b.board, etag: b.etag }));
    const body = await sync({ serverGuid: guid, boards: held, players: ['o-p1', 'o-p2', 'o-p3', 'o-nobody', 'o-p1'] });
    assert.deepStrictEqual(body.boards.map(b => b.unchanged), [1, 1, 1]);
    assert.deepStrictEqual(body.own, first.own);

    const fresh = createHandlers(db);
    const lines = body.own.map(line => line.board + ':' + line.id).sort();
    assert.deepStrictEqual(lines, ['global:o-p1', 'global:o-p2', 'global:o-p3', 'server:o-p1', 'server:o-p2', 'servers:']);
    for (const line of body.own) {
      const query = { serverGuid: guid, board: line.board, limit: '0' };
      if (line.id !== '')
        query.playerId = line.id;
      const old = JSON.parse((await fresh.getLeaderboard(get(query))).body).me[0];
      const { board, id, ...row } = line;
      assert.deepStrictEqual(row, old);
    }
    // o-p1 has 4 kills here and 14 everywhere.
    assert.strictEqual(body.own.find(l => l.board === 'server' && l.id === 'o-p1').k, 4);
    assert.strictEqual(body.own.find(l => l.board === 'global' && l.id === 'o-p1').k, 14);

    // Own lines without any board asked for: the players' lines and no server line.
    const only = await sync({ serverGuid: guid, players: ['o-p2'] });
    assert.deepStrictEqual(only.boards, []);
    assert.deepStrictEqual(only.own.map(l => l.board + ':' + l.id).sort(), ['global:o-p2', 'server:o-p2']);
  });

  await t.test('every part in one call', async () => {
    const guid = await register(NEW_DEFAULT);
    const body = await sync({
      serverGuid: guid, serverName: 'Combined',
      statsBatchId: 'c-1', matchData: kills('c-p1', 'Combined One', 7),
      transport: { batchId: 'c-t1', entries: [{ playerId: 'c-p1', playerName: 'Combined One', points: 30, insertions: 1 }] },
      ratingIds: ['c-p1'], players: ['c-p1'], boards: ALL_BOARDS
    });
    assert.deepStrictEqual(
      [body.statsStatus, body.statsPlayers, body.transportStatus, body.transportPlayers, body.ratingsStatus, body.boardsStatus],
      ['accepted', 1, 'accepted', 1, 'ok', 'ok']);
    assert.deepStrictEqual(body.ratings, [{ rating: 30, playerId: 'c-p1', insertions: 1 }]);
    assert.strictEqual((await pg.query('SELECT name FROM public.servers WHERE id = $1', [guid])).rows[0].name, 'Combined');

    // The server's own board is read live, so it already holds this batch.
    assert.deepStrictEqual(body.serverRows, [{ d: 0, g: 0, h: 0, i: 1, k: 7, n: 'Combined One', o: 0, p: 1, r: 1, s: 7, t: 30 }]);
    assert.strictEqual(body.own.find(l => l.board === 'server').k, 7);

    // The answer is flat: numbers and strings, and lists of objects holding numbers and strings.
    for (const [name, value] of Object.entries(body)) {
      if (!Array.isArray(value)) {
        assert.ok(['string', 'number'].includes(typeof value), name);
        continue;
      }
      for (const item of value) {
        assert.ok(item && typeof item === 'object' && !Array.isArray(item), name);
        for (const [key, member] of Object.entries(item))
          assert.ok(['string', 'number'].includes(typeof member), name + '.' + key);
      }
    }
    assert.deepStrictEqual(Object.keys(body), [
      'status', 'statsStatus', 'statsPlayers', 'transportStatus', 'transportPlayers', 'ratingsStatus', 'ratings', 'skins',
      'boardsStatus', 'boards', 'serverRows', 'globalRows', 'serversRows', 'own', 'snapshotRows',
      'nextSyncSeconds', 'pageBudgetPerHour', 'pageBudgetBurst', 'serverPageSeconds', 'globalPageSeconds']);
  });

  await t.test('a part that cannot be stored is reported and the others stand', async () => {
    const guid = await register('Partial');
    const entries = [{ playerId: 'x-p1', playerName: 'Pilot', points: 'lots', insertions: 1 }];
    const body = await sync({
      serverGuid: guid, statsBatchId: 'x-1', matchData: kills('x-p1', 'One', 2),
      transport: { batchId: 'x-t1', entries }, ratingIds: ['x-p1'], boards: ALL_BOARDS
    });
    assert.deepStrictEqual(
      [body.statsStatus, body.statsPlayers, body.transportStatus, body.transportPlayers, body.ratingsStatus, body.boardsStatus],
      ['accepted', 1, 'rejected', 0, 'ok', 'ok']);
    assert.strictEqual(await killsOf(guid, 'x-p1'), 2);
    assert.strictEqual(await ratingOf('x-p1'), 0);

    // Nothing of the refused batch was kept, so its id is free for a batch that can be read.
    assert.strictEqual(await number('SELECT count(*) AS n FROM public.transport_batches WHERE server_guid = $1', [guid]), 0);
    entries[0].points = 10;
    const fixed = await sync({ serverGuid: guid, statsBatchId: 'x-1', matchData: kills('x-p1', 'One', 2), transport: { batchId: 'x-t1', entries } });
    assert.deepStrictEqual([fixed.statsStatus, fixed.transportStatus], ['duplicate', 'accepted']);
    assert.strictEqual(await killsOf(guid, 'x-p1'), 2);
    assert.strictEqual(await ratingOf('x-p1'), 10);
  });

  await t.test('a part over its bound is refused whole and stores nothing', async () => {
    const guid = await register('Bounds');
    const tooMany = await sync({ serverGuid: guid, statsBatchId: 'big', matchData: kills('bound-p1', 'One', 5001) });
    assert.strictEqual(tooMany.statsStatus, 'rejected');
    assert.strictEqual(await killsOf(guid, 'bound-p1'), 0);
    assert.strictEqual(await number('SELECT count(*) AS n FROM public.stats_batches WHERE server_guid = $1', [guid]), 0);

    const most = await sync({ serverGuid: guid, statsBatchId: 'big', matchData: kills('bound-p1', 'One', 5000) });
    assert.deepStrictEqual([most.statsStatus, most.statsPlayers], ['accepted', 1]);
    assert.strictEqual(await killsOf(guid, 'bound-p1'), 5000);

    const longId = await sync({ serverGuid: guid, statsBatchId: 'i'.repeat(65), matchData: kills('bound-p1', 'One', 1) });
    assert.strictEqual(longId.statsStatus, 'rejected');
    assert.strictEqual(await killsOf(guid, 'bound-p1'), 5000);

    const entries = [];
    for (let i = 0; i < 513; i++)
      entries.push({ playerId: 'bound-t' + i, playerName: 'Pilot', points: 10, insertions: 1 });
    assert.strictEqual((await sync({ serverGuid: guid, transport: { batchId: 'big', entries } })).transportStatus, 'rejected');
    assert.strictEqual(await ratingOf('bound-t0'), 0);

    // Own lines for at most 128 players, each player once.
    for (let p = 0; p < 150; p++)
      await pg.query('INSERT INTO public.player_stats (player_bohemia_id, server_id, kills) VALUES ($1, $2, 1)', ['bound-o' + p, guid]);
    const players = [];
    for (let p = 0; p < 150; p++)
      players.push('bound-o' + p, 'bound-o' + p);
    const own = await sync({ serverGuid: guid, players });
    assert.strictEqual(own.own.filter(l => l.board === 'server').length, 128);
    assert.strictEqual(new Set(own.own.filter(l => l.board === 'server').map(l => l.id)).size, 128);
  });

  await t.test('the GUID alone selects the rows that are written', async () => {
    const mine = await register('Mine');
    const theirs = await register('Theirs');
    const event = Object.assign(kills('g-p1', 'One', 1)[0], { serverGuid: theirs, serverId: theirs, server_id: theirs });
    const entry = { playerId: 'g-p1', playerName: 'One', points: 10, insertions: 1, serverGuid: theirs, server_id: theirs };

    const body = await sync({
      serverGuid: mine, serverName: 'Theirs', serverId: theirs, targetServerGuid: theirs, viewAsServer: theirs,
      statsBatchId: 'g-1', matchData: [event],
      transport: { batchId: 'g-t1', entries: [entry], serverGuid: theirs }
    });
    assert.deepStrictEqual([body.statsStatus, body.transportStatus], ['accepted', 'accepted']);

    const at = (table, guid) => number('SELECT count(*) AS n FROM public.' + table + ' WHERE server_id = $1', [guid]);
    assert.deepStrictEqual([await at('player_stats', mine), await at('player_stats', theirs)], [1, 0]);
    assert.deepStrictEqual([await at('player_transport_server', mine), await at('player_transport_server', theirs)], [1, 0]);
    assert.strictEqual(await number('SELECT count(*) AS n FROM public.stats_batches WHERE server_guid = $1', [theirs]), 0);
    assert.strictEqual(await number('SELECT count(*) AS n FROM public.transport_batches WHERE server_guid = $1', [theirs]), 0);
    // The name relabelled the sender and nothing else.
    const names = (await pg.query('SELECT id, name, last_seen FROM public.servers WHERE id IN ($1, $2)', [mine, theirs])).rows;
    assert.strictEqual(names.find(r => r.id === mine).name, 'Theirs');
    assert.strictEqual(names.find(r => r.id === theirs).last_seen, null);

    // A GUID that is unknown or switched off writes nothing and reads nothing.
    const counts = async () => [
      await number('SELECT count(*) AS n FROM public.player_stats'),
      await number('SELECT count(*) AS n FROM public.servers'),
      await number('SELECT count(*) AS n FROM public.stats_batches'),
      await number('SELECT count(*) AS n FROM public.transport_batches')
    ];
    const before = await counts();
    await pg.query('UPDATE public.servers SET is_active = false WHERE id = $1', [theirs]);
    for (const guid of [UNKNOWN, theirs]) {
      const res = await handlers.sync(post({
        serverGuid: guid, serverName: 'Mine', statsBatchId: 'g-2', matchData: [event],
        transport: { batchId: 'g-t2', entries: [entry] }, ratingIds: ['g-p1'], players: ['g-p1'], boards: ALL_BOARDS
      }));
      assert.strictEqual(res.status, 403);
      assert.strictEqual(res.body, 'Invalid or inactive server GUID.');
    }
    assert.deepStrictEqual(await counts(), before);
  });

  await t.test('batch ids older than a week are cleared as new ones arrive', async () => {
    const guid = await register('Housekeeping');
    await sync({ serverGuid: guid, statsBatchId: 'old-1', matchData: [] });
    await pg.query("UPDATE public.stats_batches SET received_at = now() - interval '8 days' WHERE server_guid = $1", [guid]);
    await sync({ serverGuid: guid, statsBatchId: 'new-1', matchData: [] });
    const ids = (await pg.query('SELECT batch_id FROM public.stats_batches WHERE server_guid = $1', [guid])).rows.map(r => r.batch_id);
    assert.deepStrictEqual(ids, ['new-1']);
  });

  await t.test('the older routes answer as they did', async () => {
    const guid = await register('Older Routes');
    const stats = await handlers.submitStats(post({ serverGuid: guid, serverName: 'Older Routes', matchData: kills('r-p1', 'One', 1) }));
    assert.strictEqual(stats.body, '{"status":"ok","players":1}');
    const transport = await handlers.submitTransport(post({
      serverGuid: guid, batchId: 'r-1', entries: [{ playerId: 'r-p1', playerName: 'One', points: 10, insertions: 1 }] }));
    assert.strictEqual(transport.body, '{"applied":true,"players":1}');
    const ratings = await handlers.getTransportRatings(post({ serverGuid: guid, playerIds: ['r-p1'] }));
    assert.ok(ratings.body.startsWith('{"skins":[') || ratings.body.startsWith('{"ratings":['));
    assert.ok(ratings.body.includes('"ratings":[{"rating":10,"playerId":"r-p1","insertions":1}]'));

    const all = await handlers.getAllLeaderboards(get({ serverGuid: guid }));
    assert.strictEqual(all.status, 200);
    for (const key of ['globalPlayerLeaderboard', 'serverPlayerLeaderboard', 'globalServerLeaderboard'])
      assert.ok(all.body.includes('"' + key + '":['), key);

    const page = await handlers.getLeaderboard(get({ serverGuid: guid, board: 'server', playerId: 'r-p1' }));
    assert.strictEqual(page.body,
      '{"me":[{"d":0,"g":0,"h":0,"i":1,"k":1,"n":"One","o":0,"p":1,"r":1,"s":1,"t":10}],"dir":"desc",'
      + '"rows":[{"d":0,"g":0,"h":0,"i":1,"k":1,"n":"One","o":0,"p":1,"r":1,"s":1,"t":10}],'
      + '"sort":"score","board":"server","total":1,"offset":0}');
    assert.strictEqual((await handlers.registerServer(post({ serverName: '' }))).status, 400);
  });
});

test('sync when part of the database fails', { skip: SKIP }, async (t) => {
  const { pg, db } = await openDatabase();
  t.after(() => pg.close());
  const { register, sync, killsOf, ratingOf, number } = tools(pg, db);
  const fail = (signature, returns) => pg.exec(
    'CREATE OR REPLACE FUNCTION public.' + signature + ' RETURNS ' + returns
    + " LANGUAGE plpgsql AS $f$ BEGIN RAISE EXCEPTION 'injected' USING ERRCODE = 'XX000'; END $f$;");
  const definition = async signature => (await pg.query('SELECT pg_get_functiondef($1::regprocedure) AS def', ['public.' + signature])).rows[0].def;

  const guid = await register('Failing');
  const request = id => ({
    serverGuid: guid, statsBatchId: 's-' + id, matchData: kills('f-p1', 'One', 1),
    transport: { batchId: 't-' + id, entries: [{ playerId: 'f-p1', playerName: 'One', points: 10, insertions: 1 }] },
    ratingIds: ['f-p1'], players: ['f-p1'], boards: ALL_BOARDS
  });
  const statuses = body => [body.statsStatus, body.transportStatus, body.ratingsStatus, body.boardsStatus];

  await t.test('statistics fail: the batch is not stored and its id stays free to send again', async () => {
    const real = await definition('api_submit_stats(uuid, text, jsonb)');
    await fail('api_submit_stats(p_server_guid uuid, p_server_name text, p_events jsonb)', 'jsonb');
    const body = await sync(request(1));
    assert.deepStrictEqual(statuses(body), ['failed', 'accepted', 'ok', 'ok']);
    assert.strictEqual(body.statsPlayers, 0);
    assert.strictEqual(await killsOf(guid, 'f-p1'), 0);
    assert.strictEqual(await number('SELECT count(*) AS n FROM public.stats_batches'), 0);
    assert.strictEqual(await ratingOf('f-p1'), 10);

    await pg.exec(real);
    const again = await sync(request(1));
    assert.deepStrictEqual(statuses(again), ['accepted', 'duplicate', 'ok', 'ok']);
    assert.strictEqual(await killsOf(guid, 'f-p1'), 1);
    assert.strictEqual(await ratingOf('f-p1'), 10);
  });

  await t.test('transport fails: the batch is not stored and the statistics are', async () => {
    const real = await definition('submit_transport_batch(uuid, text, jsonb)');
    await fail('submit_transport_batch(p_server_guid uuid, p_batch_id text, p_entries jsonb)', 'jsonb');
    const body = await sync(request(2));
    assert.deepStrictEqual(statuses(body), ['accepted', 'failed', 'ok', 'ok']);
    assert.strictEqual(await killsOf(guid, 'f-p1'), 2);
    assert.strictEqual(await ratingOf('f-p1'), 10);

    await pg.exec(real);
    assert.deepStrictEqual(statuses(await sync(request(2))), ['duplicate', 'accepted', 'ok', 'ok']);
    assert.strictEqual(await ratingOf('f-p1'), 20);
  });

  await t.test('ratings fail: both batches are stored', async () => {
    const real = await definition('get_transport_ratings(text[])');
    await fail('get_transport_ratings(p_player_ids text[])', 'jsonb');
    const body = await sync(request(3));
    assert.deepStrictEqual(statuses(body), ['accepted', 'accepted', 'failed', 'ok']);
    assert.deepStrictEqual([body.ratings, body.skins], [[], []]);
    assert.strictEqual(await killsOf(guid, 'f-p1'), 3);
    assert.strictEqual(await ratingOf('f-p1'), 30);
    await pg.exec(real);
  });

  await t.test('boards fail: both batches are stored and no half-read board is answered', async () => {
    const real = await definition('ia_refresh_board(text, boolean)');
    await fail('ia_refresh_board(p_board text, p_force boolean DEFAULT false)', 'boolean');
    const body = await sync(request(4));
    assert.deepStrictEqual(statuses(body), ['accepted', 'accepted', 'ok', 'failed']);
    assert.deepStrictEqual([body.boards, body.serverRows, body.globalRows, body.serversRows, body.own], [[], [], [], [], []]);
    assert.strictEqual(await killsOf(guid, 'f-p1'), 4);
    assert.strictEqual(await ratingOf('f-p1'), 40);

    await pg.exec(real);
    const again = await sync(request(4));
    assert.deepStrictEqual(statuses(again), ['duplicate', 'duplicate', 'ok', 'ok']);
    assert.strictEqual(again.boards.length, 3);
  });

  await t.test('the migration can be run again afterwards', async () => {
    await pg.exec(migration('20261003000000_leaderboard_cache_and_sync.sql'));
    assert.deepStrictEqual(statuses(await sync(request(5))), ['accepted', 'accepted', 'ok', 'ok']);
  });
});

test('sync on a database with no players', { skip: SKIP }, async (t) => {
  const { pg, db } = await openDatabase();
  t.after(() => pg.close());
  const { register, sync } = tools(pg, db);

  const guid = await register('Empty');
  const body = await sync({ serverGuid: guid, players: ['nobody'], ratingIds: ['nobody'], boards: ALL_BOARDS });
  assert.deepStrictEqual([body.boardsStatus, body.ratingsStatus], ['ok', 'ok']);
  assert.deepStrictEqual(body.boards.map(b => [b.board, b.total, b.unchanged]), [['server', 0, 0], ['global', 0, 0], ['servers', 0, 0]]);
  assert.deepStrictEqual([body.serverRows, body.globalRows, body.serversRows, body.own, body.ratings], [[], [], [], [], []]);

  // An empty board has an etag too, and holding it is enough.
  const held = body.boards.map(b => ({ board: b.board, etag: b.etag }));
  const again = JSON.parse((await createHandlers(db).sync(post({ serverGuid: guid, boards: held }))).body);
  assert.deepStrictEqual(again.boards.map(b => b.unchanged), [1, 1, 1]);
});
