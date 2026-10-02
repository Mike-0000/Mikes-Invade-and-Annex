const test = require('node:test');
const assert = require('node:assert');
const { createHandlers } = require('../src/handlers');

const GUID = '0fb6b420-c245-4ad9-95fa-844141562807';

function post(body) {
  let text = body;
  if (typeof body !== 'string')
    text = JSON.stringify(body);
  return { text: async () => text, query: new URLSearchParams() };
}

function get(query) {
  return { text: async () => '', query: new URLSearchParams(query) };
}

function stub(value) {
  const calls = [];
  return {
    calls,
    scalar: async (sql, params) => {
      calls.push({ sql, params });
      if (value instanceof Error)
        throw value;
      return value;
    }
  };
}

test('registerServer answers in the compact form the game searches for', async () => {
  const db = stub(GUID);
  const res = await createHandlers(db).registerServer(post('{"serverName": "My Server","ownerEmail": ""}'));
  assert.strictEqual(res.status, 200);
  assert.strictEqual(res.body, '{"serverGuid":"' + GUID + '"}');
  assert.deepStrictEqual(db.calls[0].params, ['My Server', null]);
});

test('registerServer rejects a missing name without touching the database', async () => {
  const db = stub(GUID);
  const res = await createHandlers(db).registerServer(post({ ownerEmail: 'a@b.c' }));
  assert.strictEqual(res.status, 400);
  assert.strictEqual(db.calls.length, 0);
});

test('registerServer rejects a blank name without touching the database', async () => {
  const db = stub(GUID);
  const res = await createHandlers(db).registerServer(post({ serverName: ' \t\r\n', ownerEmail: '' }));
  assert.strictEqual(res.status, 400);
  assert.strictEqual(db.calls.length, 0);
});

test('registerServer trims the name and bounds it to 255 characters', async () => {
  const db = stub(GUID);
  await createHandlers(db).registerServer(post({ serverName: '  ' + 'n'.repeat(300) + '\r\n' }));
  assert.deepStrictEqual(db.calls[0].params, ['n'.repeat(255), null]);
});

test('registerServer accepts the placeholder name a new server starts with', async () => {
  const db = stub(GUID);
  const placeholder = 'Default Name - PLEASE RENAME IN server_name.txt, in I&A Server Profile Folder';
  const res = await createHandlers(db).registerServer(post({ serverName: placeholder, ownerEmail: '' }));
  assert.strictEqual(res.status, 200);
  assert.deepStrictEqual(db.calls[0].params, [placeholder, null]);
});

test('submitStats sends a name that is absent or not a string as empty, which keeps the stored one', async () => {
  const db = stub({ players: 0 });
  const handlers = createHandlers(db);
  await handlers.submitStats(post({ serverGuid: GUID, matchData: [] }));
  await handlers.submitStats(post({ serverGuid: GUID, serverName: 7, matchData: [] }));
  await handlers.submitStats(post({ serverGuid: GUID, serverName: null, matchData: [] }));
  assert.deepStrictEqual(db.calls.map(c => c.params[1]), ['', '', '']);
});

test('submitStats writes to the GUID it was given, whatever the name says', async () => {
  const db = stub({ players: 0 });
  const other = '9d2b8c1e-3f4a-4b5c-8d6e-7f8091a2b3c4';
  await createHandlers(db).submitStats(post({ serverGuid: GUID, serverName: other, matchData: [] }));
  assert.strictEqual(db.calls.length, 1);
  assert.ok(db.calls[0].sql.includes('public.api_submit_stats($1::uuid, $2::text, $3::jsonb)'));
  assert.strictEqual(db.calls[0].params[0], GUID);
  assert.strictEqual(db.calls[0].params[1], other);
});

test('submitStats passes the event array through and reports success', async () => {
  const db = stub({ players: 1 });
  const events = [{ eventType: 'PlayerKill', killerPlayerId: 'p1', killerPlayerName: 'One' }];
  const res = await createHandlers(db).submitStats(post({ serverGuid: GUID, serverName: 'S', matchData: events }));
  assert.strictEqual(res.status, 200);
  assert.deepStrictEqual(db.calls[0].params, [GUID, 'S', JSON.stringify(events)]);
});

test('an unknown server is refused with 403 on every guarded route', async () => {
  const handlers = createHandlers(stub(null));
  assert.strictEqual((await handlers.submitStats(post({ serverGuid: GUID, matchData: [] }))).status, 403);
  assert.strictEqual((await handlers.getAllLeaderboards(get({ serverGuid: GUID }))).status, 403);
  assert.strictEqual((await handlers.getLeaderboard(get({ serverGuid: GUID }))).status, 403);

  const raising = createHandlers(stub(new Error('unknown or inactive server ' + GUID)));
  const res = await raising.submitTransport(post({ serverGuid: GUID, batchId: 'b1', entries: [] }));
  assert.strictEqual(res.status, 403);
});

test('a malformed GUID never reaches the database', async () => {
  const db = stub({});
  const handlers = createHandlers(db);
  assert.strictEqual((await handlers.getAllLeaderboards(get({ serverGuid: 'nope' }))).status, 403);
  assert.strictEqual((await handlers.getLeaderboard(get({ serverGuid: 'nope', board: 'global' }))).status, 403);
  assert.strictEqual((await handlers.getLeaderboard(get({ board: 'global' }))).status, 403);
  assert.strictEqual((await handlers.submitStats(post({ serverGuid: 'nope', matchData: [] }))).status, 403);
  assert.strictEqual((await handlers.getTransportRatings(post({ serverGuid: 'nope', playerIds: [] }))).status, 403);
  assert.strictEqual(db.calls.length, 0);
});

test('getAllLeaderboards keeps each array directly after its key', async () => {
  const db = stub({ globalPlayerLeaderboard: [{ PlayerName: 'A', kills: 1 }], serverPlayerLeaderboard: [], globalServerLeaderboard: [] });
  const res = await createHandlers(db).getAllLeaderboards(get({ serverGuid: GUID }));
  assert.strictEqual(res.status, 200);
  assert.ok(res.body.includes('"globalPlayerLeaderboard":[{"PlayerName":"A","kills":1}]'));
  assert.ok(res.body.includes('"serverPlayerLeaderboard":[]'));
  assert.ok(res.body.includes('"globalServerLeaderboard":[]'));
});

test('getLeaderboard asks for the first page of the global board by score when nothing is said', async () => {
  const db = stub({ board: 'global', total: 0, rows: [], me: [] });
  const res = await createHandlers(db).getLeaderboard(get({ serverGuid: GUID }));
  assert.strictEqual(res.status, 200);
  assert.ok(db.calls[0].sql.includes('public.api_get_leaderboard('));
  assert.deepStrictEqual(db.calls[0].params, [GUID, 'global', 'score', true, 0, 25, null]);
});

test('getLeaderboard passes the board, sort, direction, page and player through', async () => {
  const db = stub({ board: 'server', total: 3, rows: [{ r: 51, n: 'A', t: 900 }], me: [{ r: 7, n: 'Me', t: 4200 }] });
  const res = await createHandlers(db).getLeaderboard(get({
    serverGuid: GUID, board: 'server', sort: 'transport', dir: 'asc', offset: '50', limit: '10', playerId: 'p1'
  }));
  assert.deepStrictEqual(db.calls[0].params, [GUID, 'server', 'transport', false, 50, 10, 'p1']);
  assert.strictEqual(res.headers['Content-Type'], 'application/json');
  assert.ok(res.body.includes('"rows":[{"r":51,"n":"A","t":900}]'));
  assert.ok(res.body.includes('"me":[{"r":7,"n":"Me","t":4200}]'));
});

test('getLeaderboard caps the page size and ignores numbers it cannot read', async () => {
  const db = stub({ rows: [], me: [] });
  const handlers = createHandlers(db);
  await handlers.getLeaderboard(get({ serverGuid: GUID, limit: '5000', offset: '-3' }));
  await handlers.getLeaderboard(get({ serverGuid: GUID, limit: '0', offset: '99999999' }));
  await handlers.getLeaderboard(get({ serverGuid: GUID, limit: '1e3', offset: '12abc', playerId: 'x'.repeat(65) }));
  assert.deepStrictEqual(db.calls[0].params.slice(4), [0, 100, null]);
  assert.deepStrictEqual(db.calls[1].params.slice(4), [1000000, 0, null]);
  assert.deepStrictEqual(db.calls[2].params.slice(4), [0, 25, null]);
});

test('getLeaderboard refuses a board, sort or direction it does not know without touching the database', async () => {
  const db = stub({ rows: [], me: [] });
  const handlers = createHandlers(db);
  assert.strictEqual((await handlers.getLeaderboard(get({ serverGuid: GUID, board: 'players; DROP' }))).status, 400);
  assert.strictEqual((await handlers.getLeaderboard(get({ serverGuid: GUID, sort: 'name' }))).status, 400);
  assert.strictEqual((await handlers.getLeaderboard(get({ serverGuid: GUID, dir: 'up' }))).status, 400);
  assert.strictEqual(db.calls.length, 0);
});

test('submitTransport returns the duplicate result as success so the game stops resending', async () => {
  const db = stub({ applied: false, players: 0 });
  const res = await createHandlers(db).submitTransport(post({ serverGuid: GUID, batchId: 'b1', entries: [] }));
  assert.strictEqual(res.status, 200);
  assert.strictEqual(res.body, '{"applied":false,"players":0}');
});

test('submitTransport surfaces an unexpected database error', async () => {
  const handlers = createHandlers(stub(new Error('connection lost')));
  await assert.rejects(handlers.submitTransport(post({ serverGuid: GUID, batchId: 'b1', entries: [] })));
});

test('getTransportRatings drops ids that are not usable strings', async () => {
  const db = stub({ ratings: [], skins: [] });
  await createHandlers(db).getTransportRatings(post({ serverGuid: GUID, playerIds: ['p1', '', 7, null, 'p2'] }));
  assert.deepStrictEqual(db.calls[0].params, [['p1', 'p2']]);
});

test('a body that is not a JSON object is a bad request', async () => {
  const handlers = createHandlers(stub({}));
  assert.strictEqual((await handlers.submitStats(post('not json'))).status, 400);
  assert.strictEqual((await handlers.submitTransport(post('[]'))).status, 400);
});

// A database that answers each call with what answer(sql, params) returns, and
// throws what it returns when that is an Error.
function scripted(answer) {
  const calls = [];
  return {
    calls,
    scalar: async (sql, params) => {
      calls.push({ sql, params });
      const value = answer(sql, params);
      if (value instanceof Error)
        throw value;
      return value;
    }
  };
}

function clock() {
  let at = 1000;
  const now = () => at;
  now.advance = seconds => { at += seconds * 1000; };
  return now;
}

const OTHER = '9d2b8c1e-3f4a-4b5c-8d6e-7f8091a2b3c4';
const THIRD = '3c1d2e4f-5a6b-4c7d-8e9f-0a1b2c3d4e5f';

// api_get_leaderboard as far as the handlers can tell: a page of two rows, and
// an own row named after the player asked for.
function boardDb(isActive) {
  return scripted((sql, params) => {
    if (isActive && !isActive(params[0]))
      return null;
    const [, board, sort, desc, offset, limit, playerId] = params;
    const rows = [];
    for (let i = 1; i <= Math.min(limit, 2); i++)
      rows.push({ r: offset + i, n: 'Row ' + (offset + i) });
    let me = [];
    if (board === 'servers')
      me = [{ r: 4, n: 'Server ' + params[0].slice(0, 4) }];
    else if (playerId)
      me = [{ r: 77, n: 'Own ' + playerId }];
    return { me, dir: desc ? 'desc' : 'asc', rows, sort, board, total: 500, offset };
  });
}

test('a global page is read from the database once and then served from memory', async () => {
  const db = boardDb();
  const handlers = createHandlers(db, { now: clock() });

  const first = await handlers.getLeaderboard(get({ serverGuid: GUID, board: 'global', sort: 'kills', offset: '50' }));
  const second = await handlers.getLeaderboard(get({ serverGuid: GUID, board: 'global', sort: 'kills', offset: '50' }));
  assert.strictEqual(db.calls.length, 1);
  assert.deepStrictEqual(db.calls[0].params, [GUID, 'global', 'kills', true, 50, 25, null]);
  assert.strictEqual(second.status, 200);
  assert.strictEqual(second.body, first.body);
  assert.strictEqual(second.headers['Content-Type'], 'application/json');
});

test('a held page is not served to a GUID the database has not vouched for', async () => {
  const db = boardDb(guid => guid !== OTHER);
  const handlers = createHandlers(db, { now: clock() });
  const first = await handlers.getLeaderboard(get({ serverGuid: GUID }));

  // Unknown here: the database is asked, with no rows wanted.
  const refused = await handlers.getLeaderboard(get({ serverGuid: OTHER }));
  assert.strictEqual(refused.status, 403);
  assert.strictEqual(refused.body, 'Invalid or inactive server GUID.');
  assert.deepStrictEqual(db.calls[1].params, [OTHER, 'global', 'score', true, 0, 0, null]);

  // And asked again the next time: a refusal is never remembered as a yes.
  assert.strictEqual((await handlers.getLeaderboard(get({ serverGuid: OTHER }))).status, 403);
  assert.strictEqual(db.calls.length, 3);

  // A server seen for the first time gets the held rows after one small call.
  const third = await handlers.getLeaderboard(get({ serverGuid: THIRD }));
  assert.deepStrictEqual(db.calls[3].params, [THIRD, 'global', 'score', true, 0, 0, null]);
  assert.strictEqual(third.body, first.body);
  await handlers.getLeaderboard(get({ serverGuid: THIRD }));
  assert.strictEqual(db.calls.length, 4);
});

test('a held page never carries the own line of another caller', async () => {
  const db = boardDb();
  const handlers = createHandlers(db, { now: clock() });

  const one = await handlers.getLeaderboard(get({ serverGuid: GUID, playerId: 'p1' }));
  assert.deepStrictEqual(JSON.parse(one.body).me, [{ r: 77, n: 'Own p1' }]);

  // Same page, another server and player: the rows are held, the own line is theirs.
  const two = await handlers.getLeaderboard(get({ serverGuid: OTHER, playerId: 'p2' }));
  assert.deepStrictEqual(db.calls[1].params, [OTHER, 'global', 'score', true, 0, 0, 'p2']);
  assert.deepStrictEqual(JSON.parse(two.body).me, [{ r: 77, n: 'Own p2' }]);
  assert.deepStrictEqual(JSON.parse(two.body).rows, JSON.parse(one.body).rows);
  assert.ok(!two.body.includes('p1'));

  // No player: no own line, and nothing left over from the callers before.
  const none = await handlers.getLeaderboard(get({ serverGuid: OTHER }));
  assert.strictEqual(db.calls.length, 2);
  assert.deepStrictEqual(JSON.parse(none.body).me, []);
  assert.ok(!none.body.includes('Own'));

  // On the servers board the own line is the asking server's, looked up each time.
  const mine = await handlers.getLeaderboard(get({ serverGuid: GUID, board: 'servers' }));
  const theirs = await handlers.getLeaderboard(get({ serverGuid: OTHER, board: 'servers' }));
  assert.deepStrictEqual(db.calls[3].params, [OTHER, 'servers', 'score', true, 0, 0, null]);
  assert.deepStrictEqual(JSON.parse(mine.body).me, [{ r: 4, n: 'Server ' + GUID.slice(0, 4) }]);
  assert.deepStrictEqual(JSON.parse(theirs.body).me, [{ r: 4, n: 'Server ' + OTHER.slice(0, 4) }]);
  assert.deepStrictEqual(JSON.parse(theirs.body).rows, JSON.parse(mine.body).rows);
});

test('the board of one server and an own-row-only answer are never held', async () => {
  const db = boardDb();
  const handlers = createHandlers(db, { now: clock() });
  await handlers.getLeaderboard(get({ serverGuid: GUID, board: 'server' }));
  await handlers.getLeaderboard(get({ serverGuid: GUID, board: 'server' }));
  await handlers.getLeaderboard(get({ serverGuid: GUID, limit: '0', playerId: 'p1' }));
  await handlers.getLeaderboard(get({ serverGuid: GUID, limit: '0', playerId: 'p1' }));
  assert.strictEqual(db.calls.length, 4);
  assert.deepStrictEqual(db.calls[1].params, [GUID, 'server', 'score', true, 0, 25, null]);
  assert.deepStrictEqual(db.calls[3].params, [GUID, 'global', 'score', true, 0, 0, 'p1']);
});

test('each board, sort, direction and page is held on its own', async () => {
  const db = boardDb();
  const handlers = createHandlers(db, { now: clock() });
  const asks = [
    {}, { board: 'servers' }, { sort: 'kd' }, { dir: 'asc' }, { offset: '25' }, { limit: '50' }
  ];
  for (const ask of asks)
    await handlers.getLeaderboard(get(Object.assign({ serverGuid: GUID }, ask)));
  assert.strictEqual(db.calls.length, asks.length);

  const again = await handlers.getLeaderboard(get({ serverGuid: GUID, dir: 'asc' }));
  assert.strictEqual(JSON.parse(again.body).dir, 'asc');
  assert.strictEqual(db.calls.length, asks.length);
});

test('a held page is read again when its time is up', async () => {
  const db = boardDb();
  const now = clock();
  const handlers = createHandlers(db, { now, pageCacheSeconds: 60, activeServerSeconds: 600 });
  await handlers.getLeaderboard(get({ serverGuid: GUID }));
  now.advance(59);
  await handlers.getLeaderboard(get({ serverGuid: GUID }));
  assert.strictEqual(db.calls.length, 1);

  now.advance(1);
  await handlers.getLeaderboard(get({ serverGuid: GUID }));
  assert.strictEqual(db.calls.length, 2);
  assert.deepStrictEqual(db.calls[1].params, [GUID, 'global', 'score', true, 0, 25, null]);
});

test('a GUID is vouched for only for a while, and a refusal ends it at once', async () => {
  let active = true;
  const db = boardDb(() => active);
  const now = clock();
  const handlers = createHandlers(db, { now, pageCacheSeconds: 600, activeServerSeconds: 60 });
  await handlers.getLeaderboard(get({ serverGuid: GUID }));
  await handlers.getLeaderboard(get({ serverGuid: GUID }));
  assert.strictEqual(db.calls.length, 1);

  now.advance(60);
  await handlers.getLeaderboard(get({ serverGuid: GUID }));
  assert.deepStrictEqual(db.calls[1].params, [GUID, 'global', 'score', true, 0, 0, null]);

  // Switched off: the next call that reaches the database is refused, and so
  // is every one after it, held page or not.
  active = false;
  assert.strictEqual((await handlers.getLeaderboard(get({ serverGuid: GUID, board: 'server' }))).status, 403);
  assert.strictEqual((await handlers.getLeaderboard(get({ serverGuid: GUID }))).status, 403);
  assert.strictEqual(db.calls.length, 4);
});

test('with the page cache off every page is read from the database', async () => {
  const db = boardDb();
  const handlers = createHandlers(db, { now: clock(), pageCacheSeconds: 0 });
  await handlers.getLeaderboard(get({ serverGuid: GUID }));
  await handlers.getLeaderboard(get({ serverGuid: GUID }));
  assert.strictEqual(db.calls.length, 2);
  assert.deepStrictEqual(db.calls[1].params, [GUID, 'global', 'score', true, 0, 25, null]);
});

test('the page cache holds no more pages than its cap', async () => {
  const db = boardDb();
  const handlers = createHandlers(db, { now: clock(), pageCacheEntries: 2 });
  for (const offset of ['0', '25', '50'])
    await handlers.getLeaderboard(get({ serverGuid: GUID, offset }));
  await handlers.getLeaderboard(get({ serverGuid: GUID, offset: '50' }));
  await handlers.getLeaderboard(get({ serverGuid: GUID, offset: '25' }));
  assert.strictEqual(db.calls.length, 3);
  await handlers.getLeaderboard(get({ serverGuid: GUID, offset: '0' }));
  assert.strictEqual(db.calls.length, 4);
});

test('each set of handlers has its own memory', async () => {
  const db = boardDb();
  await createHandlers(db, { now: clock() }).getLeaderboard(get({ serverGuid: GUID }));
  await createHandlers(db, { now: clock() }).getLeaderboard(get({ serverGuid: GUID }));
  assert.strictEqual(db.calls.length, 2);
  assert.deepStrictEqual(db.calls[1].params, [GUID, 'global', 'score', true, 0, 25, null]);
});

// What api_sync answers when nothing was sent.
function syncAnswer(extra) {
  return Object.assign({
    statsStatus: 'none', statsPlayers: 0, transportStatus: 'none', transportPlayers: 0,
    ratingsStatus: 'none', ratings: [], skins: [], boardsStatus: 'none', boards: [],
    serverRows: [], globalRows: [], serversRows: [], own: [], snapshotRows: 100
  }, extra || {});
}

test('sync with only a GUID sends nothing else and answers with the hints', async () => {
  const db = stub(syncAnswer());
  const res = await createHandlers(db).sync(post({ serverGuid: GUID }));
  assert.strictEqual(res.status, 200);
  assert.ok(db.calls[0].sql.includes('public.api_sync('));
  assert.deepStrictEqual(db.calls[0].params, [GUID, '', null, null, null, null, null, [], '[]']);
  assert.strictEqual(res.body,
    '{"status":"ok","statsStatus":"none","statsPlayers":0,"transportStatus":"none","transportPlayers":0,'
    + '"ratingsStatus":"none","ratings":[],"skins":[],"boardsStatus":"none","boards":[],'
    + '"serverRows":[],"globalRows":[],"serversRows":[],"own":[],"snapshotRows":100,'
    + '"nextSyncSeconds":60,"pageBudgetPerHour":120,"pageBudgetBurst":200,"serverPageSeconds":120,"globalPageSeconds":300}');
});

test('sync answers with the hints it was configured with', async () => {
  const handlers = createHandlers(stub(syncAnswer()), {
    syncSeconds: 90, pageBudgetPerHour: 30, pageBudgetBurst: 5, serverPageSeconds: 240, globalPageSeconds: 600
  });
  const body = JSON.parse((await handlers.sync(post({ serverGuid: GUID }))).body);
  assert.deepStrictEqual(
    [body.nextSyncSeconds, body.pageBudgetPerHour, body.pageBudgetBurst, body.serverPageSeconds, body.globalPageSeconds],
    [90, 30, 5, 240, 600]);
});

test('sync passes every part through, and only the GUID says where it is written', async () => {
  const db = stub(syncAnswer());
  const events = [{ eventType: 'PlayerKill', killerPlayerId: 'p1', killerPlayerName: 'One', serverGuid: OTHER }];
  const entries = [{ playerId: 'p1', playerName: 'One', points: 10, insertions: 1, serverGuid: OTHER }];
  await createHandlers(db).sync(post({
    serverGuid: GUID, serverId: OTHER, targetServerGuid: OTHER, serverName: OTHER,
    statsBatchId: 's1', matchData: events,
    transport: { batchId: 't1', entries, serverGuid: OTHER },
    ratingIds: ['p1', 'p2'], players: ['p1', 'p2'],
    boards: [{ board: 'server', etag: 'a' }, { board: 'global', etag: 'b' }, { board: 'servers', etag: '' }]
  }));
  assert.strictEqual(db.calls.length, 1);
  assert.deepStrictEqual(db.calls[0].params, [
    GUID, OTHER, 's1', JSON.stringify(events), 't1', JSON.stringify(entries),
    ['p1', 'p2'], ['p1', 'p2'],
    JSON.stringify([
      { board: 'server', etags: ['a'] }, { board: 'global', etags: ['b'] }, { board: 'servers', etags: [] }])
  ]);
});

test('sync refuses a body that is not an object, a malformed GUID and an unknown server', async () => {
  const db = stub(syncAnswer());
  const handlers = createHandlers(db);
  assert.strictEqual((await handlers.sync(post('not json'))).status, 400);
  assert.strictEqual((await handlers.sync(post('[]'))).status, 400);
  assert.strictEqual((await handlers.sync(post({ serverGuid: 'nope' }))).status, 403);
  assert.strictEqual((await handlers.sync(post({ matchData: [] }))).status, 403);
  assert.strictEqual(db.calls.length, 0);

  const refused = await createHandlers(stub(null)).sync(post({ serverGuid: GUID }));
  assert.strictEqual(refused.status, 403);
  assert.strictEqual(refused.body, 'Invalid or inactive server GUID.');
});

test('sync refuses a stats part it cannot take whole and still runs the rest', async () => {
  const db = stub(syncAnswer({ transportStatus: 'accepted', transportPlayers: 1 }));
  const handlers = createHandlers(db);
  const transport = { batchId: 't1', entries: [] };
  const bad = [
    { matchData: 'events' },
    { matchData: {} },
    { matchData: new Array(5001).fill({}) },
    { matchData: [], statsBatchId: '' },
    { matchData: [], statsBatchId: 'x'.repeat(65) },
    { matchData: [], statsBatchId: 7 }
  ];
  for (const part of bad) {
    const res = await handlers.sync(post(Object.assign({ serverGuid: GUID, transport }, part)));
    const body = JSON.parse(res.body);
    assert.strictEqual(res.status, 200);
    assert.strictEqual(body.statsStatus, 'rejected');
    assert.strictEqual(body.transportStatus, 'accepted');
  }
  for (const call of db.calls) {
    assert.strictEqual(call.params[2], null);
    assert.strictEqual(call.params[3], null);
    assert.strictEqual(call.params[4], 't1');
  }

  // The most it takes, with and without an id.
  await handlers.sync(post({ serverGuid: GUID, matchData: new Array(5000).fill({}), statsBatchId: 'x'.repeat(64) }));
  await handlers.sync(post({ serverGuid: GUID, matchData: [] }));
  assert.strictEqual(db.calls[bad.length].params[2], 'x'.repeat(64));
  assert.strictEqual(JSON.parse(db.calls[bad.length].params[3]).length, 5000);
  assert.deepStrictEqual(db.calls[bad.length + 1].params.slice(2, 4), [null, '[]']);
});

test('sync refuses a transport part it cannot take whole and still runs the rest', async () => {
  const db = stub(syncAnswer({ statsStatus: 'accepted', statsPlayers: 2 }));
  const handlers = createHandlers(db);
  const bad = [
    'batch', [], { entries: [] }, { batchId: '', entries: [] }, { batchId: 'x'.repeat(65), entries: [] },
    { batchId: 't1' }, { batchId: 't1', entries: {} }, { batchId: 't1', entries: new Array(513).fill({}) }
  ];
  for (const transport of bad) {
    const body = JSON.parse((await handlers.sync(post({ serverGuid: GUID, matchData: [], transport }))).body);
    assert.strictEqual(body.transportStatus, 'rejected');
    assert.strictEqual(body.statsStatus, 'accepted');
    assert.strictEqual(body.statsPlayers, 2);
  }
  for (const call of db.calls)
    assert.deepStrictEqual(call.params.slice(3, 6), ['[]', null, null]);

  await handlers.sync(post({ serverGuid: GUID, transport: { batchId: 't1', entries: new Array(512).fill({}) } }));
  assert.strictEqual(JSON.parse(db.calls[bad.length].params[5]).length, 512);
});

test('sync bounds the name, the player lists and the boards it asks for', async () => {
  const db = stub(syncAnswer());
  const many = [];
  for (let i = 0; i < 400; i++)
    many.push('p' + i);
  await createHandlers(db).sync(post({
    serverGuid: GUID,
    serverName: 'n'.repeat(5000),
    ratingIds: ['', 7, null, 'x'.repeat(65)].concat(many),
    players: ['p0', 'p0', '', 7, null, 'x'.repeat(65)].concat(many),
    boards: [
      null, 'global', { board: 'session' }, { board: 'global', etag: 'e'.repeat(65) },
      { board: 'global', etag: 'second' }, { board: 'server', etag: 9 }, { etag: 'x' }
    ]
  }));
  const params = db.calls[0].params;
  assert.strictEqual(params[1], 'n'.repeat(1024));
  assert.deepStrictEqual(params[6], many.slice(0, 256));
  assert.deepStrictEqual(params[7], many.slice(0, 128));
  assert.strictEqual(params[8], JSON.stringify([{ board: 'global', etags: [] }, { board: 'server', etags: [] }]));

  await createHandlers(db).sync(post({ serverGuid: GUID, serverName: 7, ratingIds: 'p1', players: 'p1', boards: {} }));
  assert.deepStrictEqual(db.calls[1].params, [GUID, '', null, null, null, null, null, [], '[]']);
});

test('sync says so when the database has no api_sync, and passes any other error on', async () => {
  const missing = new Error('function public.api_sync(uuid, text, ...) does not exist');
  missing.code = '42883';
  const res = await createHandlers(stub(missing)).sync(post({ serverGuid: GUID }));
  assert.strictEqual(res.status, 501);
  assert.strictEqual(res.body, 'Sync is not available.');

  await assert.rejects(createHandlers(stub(new Error('connection lost'))).sync(post({ serverGuid: GUID })));
});

test('sync keeps the shared snapshots in memory and reads them from the database only when they change', async () => {
  const snapshot = {
    global: { etag: 'g1', rows: [{ r: 1, n: 'Top' }] },
    servers: { etag: 's1', rows: [{ r: 1, n: 'Server' }] }
  };
  // api_sync as far as a snapshot goes: rows leave only when no etag shown matches.
  const db = scripted((sql, params) => {
    const boards = [];
    const rows = { server: [], global: [], servers: [] };
    for (const want of JSON.parse(params[8])) {
      if (want.board === 'server') {
        const unchanged = want.etags.includes('own1');
        boards.push({ board: 'server', etag: 'own1', total: 3, unchanged });
        if (!unchanged)
          rows.server = [{ r: 1, n: 'Mine ' + params[0].slice(0, 4) }];
        continue;
      }
      const current = snapshot[want.board];
      const unchanged = want.etags.includes(current.etag);
      boards.push({ board: want.board, etag: current.etag, total: 9, unchanged });
      if (!unchanged)
        rows[want.board] = current.rows;
    }
    return syncAnswer({
      boardsStatus: 'ok', boards, serverRows: rows.server, globalRows: rows.global, serversRows: rows.servers
    });
  });
  const handlers = createHandlers(db);
  async function ask(guid, etags) {
    const boards = [
      { board: 'server', etag: etags[0] }, { board: 'global', etag: etags[1] }, { board: 'servers', etag: etags[2] }
    ];
    return JSON.parse((await handlers.sync(post({ serverGuid: guid, boards }))).body);
  }
  const wanted = () => JSON.parse(db.calls[db.calls.length - 1].params[8]);

  // Nothing held anywhere: the rows come from the database.
  let body = await ask(GUID, ['', '', '']);
  assert.deepStrictEqual(wanted().map(w => w.etags), [[], [], []]);
  assert.deepStrictEqual(body.boards.map(b => b.unchanged), [0, 0, 0]);
  assert.deepStrictEqual(body.globalRows, [{ r: 1, n: 'Top' }]);

  // Another server that holds nothing: the database is shown the etags held
  // here, sends no shared rows, and the answer still carries them. A server's
  // own board is never held here.
  body = await ask(OTHER, ['', '', '']);
  assert.deepStrictEqual(wanted().map(w => w.etags), [[], ['g1'], ['s1']]);
  assert.deepStrictEqual(body.boards, [
    { board: 'server', etag: 'own1', total: 3, unchanged: 0 },
    { board: 'global', etag: 'g1', total: 9, unchanged: 0 },
    { board: 'servers', etag: 's1', total: 9, unchanged: 0 }
  ]);
  assert.deepStrictEqual(body.serverRows, [{ r: 1, n: 'Mine ' + OTHER.slice(0, 4) }]);
  assert.deepStrictEqual(body.globalRows, [{ r: 1, n: 'Top' }]);
  assert.deepStrictEqual(body.serversRows, [{ r: 1, n: 'Server' }]);

  // A server that holds them: unchanged, and no rows at all.
  body = await ask(OTHER, ['own1', 'g1', 's1']);
  assert.deepStrictEqual(wanted().map(w => w.etags), [['own1'], ['g1'], ['s1']]);
  assert.deepStrictEqual(body.boards.map(b => b.unchanged), [1, 1, 1]);
  assert.deepStrictEqual([body.serverRows, body.globalRows, body.serversRows], [[], [], []]);
  assert.deepStrictEqual(body.boards.map(b => b.total), [3, 9, 9]);

  // The board changed: the caller's etag and the one held here are both stale,
  // the new rows come from the database and replace what is held.
  snapshot.global = { etag: 'g2', rows: [{ r: 1, n: 'New top' }] };
  body = await ask(GUID, ['own1', 'g1', 's1']);
  assert.deepStrictEqual(wanted()[1].etags, ['g1']);
  assert.deepStrictEqual(body.boards[1], { board: 'global', etag: 'g2', total: 9, unchanged: 0 });
  assert.deepStrictEqual(body.globalRows, [{ r: 1, n: 'New top' }]);

  // A caller still on the old etag is given the new rows from memory.
  body = await ask(OTHER, ['own1', 'g1', 's1']);
  assert.deepStrictEqual(wanted()[1].etags, ['g1', 'g2']);
  assert.deepStrictEqual(body.boards[1], { board: 'global', etag: 'g2', total: 9, unchanged: 0 });
  assert.deepStrictEqual(body.globalRows, [{ r: 1, n: 'New top' }]);
});
