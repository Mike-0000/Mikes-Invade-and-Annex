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

  const raising = createHandlers(stub(new Error('unknown or inactive server ' + GUID)));
  const res = await raising.submitTransport(post({ serverGuid: GUID, batchId: 'b1', entries: [] }));
  assert.strictEqual(res.status, 403);
});

test('a malformed GUID never reaches the database', async () => {
  const db = stub({});
  const handlers = createHandlers(db);
  assert.strictEqual((await handlers.getAllLeaderboards(get({ serverGuid: 'nope' }))).status, 403);
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
