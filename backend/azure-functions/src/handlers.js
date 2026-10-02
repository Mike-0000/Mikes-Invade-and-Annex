// Route handlers, kept free of the Functions runtime so they can be tested
// with a stubbed database. Each returns { status, body, headers? }.
//
// Responses are written with JSON.stringify on purpose: the game finds keys
// with hand-rolled string searches ("serverGuid":" and "key":[) that only
// match compact JSON.

const { createCache } = require('./cache');
const { readConfig } = require('./config');

const UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const MAX_BODY_BYTES = 1024 * 1024;
const MAX_PLAYER_IDS = 256;

// GET /leaderboard. The database function falls back to its defaults for
// anything it does not know, so an unknown value is refused here instead of
// answering a different board than the one asked for.
const BOARDS = ['global', 'server', 'servers'];
const SORTS = ['score', 'kills', 'deaths', 'kd', 'hvt', 'guard', 'obj', 'transport', 'insertions', 'players'];
const DEFAULT_PAGE_ROWS = 25;
const MAX_PAGE_ROWS = 100;
const MAX_OFFSET = 1000000;

// POST /sync. A part over its bound is refused as a whole, never cut short:
// half a batch must not be counted as the batch.
const MAX_SYNC_EVENTS = 5000;
const MAX_SYNC_TRANSPORT_ENTRIES = 512;
const MAX_SYNC_PLAYERS = 128;
const MAX_NAME_CHARS = 1024;
const MAX_ETAG_CHARS = 64;
// More server GUIDs than are ever active at once.
const MAX_ACTIVE_SERVERS = 2000;

const LEADERBOARD_SQL =
  'SELECT public.api_get_leaderboard($1::uuid, $2::text, $3::text, $4::boolean, $5::integer, $6::integer, $7::text) AS value';
const SYNC_SQL =
  'SELECT public.api_sync($1::uuid, $2::text, $3::text, $4::jsonb, $5::text, $6::jsonb, $7::text[], $8::text[], $9::jsonb) AS value';

const FORBIDDEN = { status: 403, body: 'Invalid or inactive server GUID.' };

function json(value, status) {
  return {
    status: status || 200,
    body: JSON.stringify(value),
    headers: { 'Content-Type': 'application/json' }
  };
}

function badRequest(message) {
  return { status: 400, body: message };
}

async function readJson(request) {
  const text = await request.text();
  if (text.length > MAX_BODY_BYTES)
    return null;
  try {
    const value = JSON.parse(text);
    if (value && typeof value === 'object' && !Array.isArray(value))
      return value;
  } catch (e) {
    // Fall through: a malformed body is the caller's error.
  }
  return null;
}

function isGuid(value) {
  return typeof value === 'string' && UUID.test(value);
}

// A player id or a batch id: 1 to 64 characters.
function isId(value) {
  return typeof value === 'string' && value.length >= 1 && value.length <= 64;
}

function isAbsent(value) {
  return value === undefined || value === null;
}

// A query value that is a whole number, capped; anything else is the fallback.
function wholeNumber(text, fallback, max) {
  if (typeof text !== 'string' || !/^[0-9]{1,9}$/.test(text))
    return fallback;
  return Math.min(Number(text), max);
}

// A page answer, in the key order the database writes it.
function pageAnswer(page, me) {
  return {
    me,
    dir: page.dir,
    rows: page.rows,
    sort: page.sort,
    board: page.board,
    total: page.total,
    offset: page.offset
  };
}

// options: the settings of config.js, and now() for tests. Each set of
// handlers has its own caches.
function createHandlers(db, options) {
  const config = Object.assign(readConfig({}), options || {});
  const now = config.now || Date.now;

  // Pages of the global and servers boards. They are the same for every
  // server, so the key holds no server GUID and no player, and an entry holds
  // no one's own row.
  const pages = createCache({ ttlMs: config.pageCacheSeconds * 1000, maxEntries: config.pageCacheEntries, now });
  // Server GUIDs the database said were active. Only a yes is kept, so an
  // unknown GUID is asked about every time and a new server works at once.
  const activeServers = createCache({ ttlMs: config.activeServerSeconds * 1000, maxEntries: MAX_ACTIVE_SERVERS, now });
  // The last global and servers snapshots /sync read: board -> { etag, rows }.
  // Every /sync shows the database the etag held here, so a stale one is
  // replaced and never served.
  const snapshots = new Map();

  async function registerServer(request) {
    const body = await readJson(request);
    if (!body || typeof body.serverName !== 'string' || body.serverName.trim() === '')
      return badRequest('serverName is required.');

    let email = null;
    if (typeof body.ownerEmail === 'string' && body.ownerEmail !== '')
      email = body.ownerEmail.slice(0, 255);

    // A placeholder name is fine here: a new row has no better name to lose.
    const guid = await db.scalar(
      'SELECT public.register_server($1::varchar, $2::varchar) AS value',
      [body.serverName.trim().slice(0, 255), email]);
    return json({ serverGuid: guid });
  }

  async function submitStats(request) {
    const body = await readJson(request);
    if (!body || !Array.isArray(body.matchData))
      return badRequest('matchData must be an array.');
    if (!isGuid(body.serverGuid))
      return FORBIDDEN;

    // The name only relabels the row the GUID selects. api_submit_stats decides
    // whether it replaces the stored one; an absent name keeps it.
    let name = '';
    if (typeof body.serverName === 'string')
      name = body.serverName;

    const result = await db.scalar(
      'SELECT public.api_submit_stats($1::uuid, $2::text, $3::jsonb) AS value',
      [body.serverGuid, name, JSON.stringify(body.matchData)]);
    if (!result)
      return FORBIDDEN;
    return json({ status: 'ok', players: result.players });
  }

  async function getAllLeaderboards(request) {
    const guid = request.query.get('serverGuid');
    if (!isGuid(guid))
      return FORBIDDEN;

    const result = await db.scalar(
      'SELECT public.api_get_all_leaderboards($1::uuid) AS value', [guid]);
    if (!result)
      return FORBIDDEN;
    return json(result);
  }

  // One page of one board in one sort order, plus the asking player's own row.
  async function getLeaderboard(request) {
    const query = request.query;
    const guid = query.get('serverGuid');
    if (!isGuid(guid))
      return FORBIDDEN;

    const board = query.get('board') || 'global';
    if (!BOARDS.includes(board))
      return badRequest('board must be one of: ' + BOARDS.join(', ') + '.');
    const sort = query.get('sort') || 'score';
    if (!SORTS.includes(sort))
      return badRequest('sort must be one of: ' + SORTS.join(', ') + '.');
    const dir = query.get('dir') || 'desc';
    if (dir !== 'asc' && dir !== 'desc')
      return badRequest('dir must be asc or desc.');

    const offset = wholeNumber(query.get('offset'), 0, MAX_OFFSET);
    const limit = wholeNumber(query.get('limit'), DEFAULT_PAGE_ROWS, MAX_PAGE_ROWS);

    let playerId = query.get('playerId');
    if (typeof playerId !== 'string' || playerId.length < 1 || playerId.length > 64)
      playerId = null;

    // A server's own board is that server's alone and is never kept here.
    const shared = board !== 'server' && limit > 0;
    const key = [board, sort, dir, offset, limit].join('|');
    const held = shared ? pages.get(key) : undefined;
    if (held) {
      // The rows are held. Who is asking still has to be an active server, and
      // an own row is looked up for this caller only. On the servers board the
      // own row is the asking server's.
      const wantsOwn = board === 'servers' || playerId !== null;
      if (!wantsOwn && activeServers.get(guid))
        return json(pageAnswer(held, []));

      const own = await db.scalar(LEADERBOARD_SQL, [guid, board, sort, dir === 'desc', 0, 0, playerId]);
      if (!own) {
        activeServers.remove(guid);
        return FORBIDDEN;
      }
      activeServers.set(guid, true);
      return json(pageAnswer(held, Array.isArray(own.me) ? own.me : []));
    }

    const result = await db.scalar(LEADERBOARD_SQL, [guid, board, sort, dir === 'desc', offset, limit, playerId]);
    if (!result) {
      activeServers.remove(guid);
      return FORBIDDEN;
    }
    activeServers.set(guid, true);
    if (shared && Array.isArray(result.rows))
      pages.set(key, pageAnswer(result, []));
    return json(result);
  }

  async function submitTransport(request) {
    const body = await readJson(request);
    if (!body || !Array.isArray(body.entries))
      return badRequest('entries must be an array.');
    if (typeof body.batchId !== 'string' || body.batchId === '' || body.batchId.length > 64)
      return badRequest('batchId must be 1 to 64 characters.');
    if (!isGuid(body.serverGuid))
      return FORBIDDEN;

    try {
      const result = await db.scalar(
        'SELECT public.submit_transport_batch($1::uuid, $2::text, $3::jsonb) AS value',
        [body.serverGuid, body.batchId, JSON.stringify(body.entries)]);
      return json(result);
    } catch (e) {
      if (e && typeof e.message === 'string' && e.message.startsWith('unknown or inactive server'))
        return FORBIDDEN;
      // A malformed entry (for example a non-numeric points value).
      if (e && typeof e.code === 'string' && e.code.startsWith('22'))
        return badRequest('entries are malformed.');
      throw e;
    }
  }

  async function getTransportRatings(request) {
    const body = await readJson(request);
    if (!body || !Array.isArray(body.playerIds))
      return badRequest('playerIds must be an array.');
    if (!isGuid(body.serverGuid))
      return FORBIDDEN;

    const ids = body.playerIds
      .filter(id => typeof id === 'string' && id.length >= 1 && id.length <= 64)
      .slice(0, MAX_PLAYER_IDS);
    const result = await db.scalar(
      'SELECT public.get_transport_ratings($1::text[]) AS value', [ids]);
    return json(result);
  }

  // Everything a game server sends and reads, in one exchange. Each part is
  // optional and reports its own result; see api_sync for what is stored
  // together. The server GUID alone selects the rows that are written.
  async function sync(request) {
    const body = await readJson(request);
    if (!body)
      return badRequest('The body must be a JSON object.');
    if (!isGuid(body.serverGuid))
      return FORBIDDEN;
    const guid = body.serverGuid;

    let name = '';
    if (typeof body.serverName === 'string')
      name = body.serverName.slice(0, MAX_NAME_CHARS);

    // A stats batch: matchData as /submitStats takes it, and an id that makes
    // sending it twice harmless.
    let events = null;
    let statsBatchId = null;
    let statsRejected = false;
    if (!isAbsent(body.matchData)) {
      if (!Array.isArray(body.matchData) || body.matchData.length > MAX_SYNC_EVENTS)
        statsRejected = true;
      else if (!isAbsent(body.statsBatchId) && !isId(body.statsBatchId))
        statsRejected = true;
      else {
        events = JSON.stringify(body.matchData);
        if (!isAbsent(body.statsBatchId))
          statsBatchId = body.statsBatchId;
      }
    }

    // A transport batch, as /submitTransport takes it.
    let transportBatchId = null;
    let transportEntries = null;
    let transportRejected = false;
    const transport = body.transport;
    if (!isAbsent(transport)) {
      if (typeof transport !== 'object' || Array.isArray(transport) || !isId(transport.batchId)
          || !Array.isArray(transport.entries) || transport.entries.length > MAX_SYNC_TRANSPORT_ENTRIES)
        transportRejected = true;
      else {
        transportBatchId = transport.batchId;
        transportEntries = JSON.stringify(transport.entries);
      }
    }

    let ratingIds = null;
    if (Array.isArray(body.ratingIds))
      ratingIds = body.ratingIds.filter(isId).slice(0, MAX_PLAYER_IDS);

    let players = [];
    if (Array.isArray(body.players))
      players = Array.from(new Set(body.players.filter(isId))).slice(0, MAX_SYNC_PLAYERS);

    // The snapshots wanted. For a board every server shares, the database is
    // also shown the etag held here: its rows then stay in the database and
    // are answered from memory.
    const asked = new Map();
    const heldSnapshots = new Map();
    const wanted = [];
    if (Array.isArray(body.boards)) {
      for (const entry of body.boards) {
        if (!entry || typeof entry !== 'object' || !BOARDS.includes(entry.board) || asked.has(entry.board))
          continue;
        let etag = '';
        if (typeof entry.etag === 'string' && entry.etag.length <= MAX_ETAG_CHARS)
          etag = entry.etag;
        asked.set(entry.board, etag);

        const etags = [];
        if (etag !== '')
          etags.push(etag);
        const snapshot = entry.board === 'server' ? undefined : snapshots.get(entry.board);
        if (snapshot) {
          heldSnapshots.set(entry.board, snapshot);
          if (snapshot.etag !== etag)
            etags.push(snapshot.etag);
        }
        wanted.push({ board: entry.board, etags });
      }
    }

    let result;
    try {
      result = await db.scalar(SYNC_SQL, [
        guid, name, statsBatchId, events, transportBatchId, transportEntries,
        ratingIds, players, JSON.stringify(wanted)]);
    } catch (e) {
      // The database has no api_sync yet, or no longer. The game then keeps to
      // the older routes.
      if (e && e.code === '42883')
        return { status: 501, body: 'Sync is not available.' };
      throw e;
    }
    if (!result) {
      activeServers.remove(guid);
      return FORBIDDEN;
    }
    activeServers.set(guid, true);

    const rows = { server: result.serverRows || [], global: result.globalRows || [], servers: result.serversRows || [] };
    const boards = [];
    for (const entry of Array.isArray(result.boards) ? result.boards : []) {
      let unchanged = entry.unchanged === true;
      if (entry.board !== 'server') {
        const snapshot = heldSnapshots.get(entry.board);
        if (!unchanged)
          snapshots.set(entry.board, { etag: entry.etag, rows: rows[entry.board] });
        else if (entry.etag !== asked.get(entry.board) && snapshot && snapshot.etag === entry.etag) {
          // The database matched the etag held here, not the caller's.
          rows[entry.board] = snapshot.rows;
          unchanged = false;
        }
      }
      // 1 or 0: the game reads numbers and strings.
      boards.push({ board: entry.board, etag: entry.etag, total: entry.total, unchanged: unchanged ? 1 : 0 });
    }

    return json({
      status: 'ok',
      statsStatus: statsRejected ? 'rejected' : result.statsStatus,
      statsPlayers: result.statsPlayers || 0,
      transportStatus: transportRejected ? 'rejected' : result.transportStatus,
      transportPlayers: result.transportPlayers || 0,
      ratingsStatus: result.ratingsStatus,
      ratings: result.ratings || [],
      skins: result.skins || [],
      boardsStatus: result.boardsStatus,
      boards,
      serverRows: rows.server,
      globalRows: rows.global,
      serversRows: rows.servers,
      own: result.own || [],
      snapshotRows: result.snapshotRows || 0,
      nextSyncSeconds: config.syncSeconds,
      pageBudgetPerHour: config.pageBudgetPerHour,
      pageBudgetBurst: config.pageBudgetBurst,
      serverPageSeconds: config.serverPageSeconds,
      globalPageSeconds: config.globalPageSeconds
    });
  }

  return { registerServer, submitStats, getAllLeaderboards, getLeaderboard, submitTransport, getTransportRatings, sync };
}

module.exports = { createHandlers };
