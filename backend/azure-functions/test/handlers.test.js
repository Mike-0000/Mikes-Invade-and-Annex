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
