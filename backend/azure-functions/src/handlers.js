// Route handlers, kept free of the Functions runtime so they can be tested
// with a stubbed database. Each returns { status, body, headers? }.
//
// Responses are written with JSON.stringify on purpose: the game finds keys
// with hand-rolled string searches ("serverGuid":" and "key":[) that only
// match compact JSON.

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

// A query value that is a whole number, capped; anything else is the fallback.
function wholeNumber(text, fallback, max) {
  if (typeof text !== 'string' || !/^[0-9]{1,9}$/.test(text))
    return fallback;
  return Math.min(Number(text), max);
}

function createHandlers(db) {
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

    const result = await db.scalar(
      'SELECT public.api_get_leaderboard($1::uuid, $2::text, $3::text, $4::boolean, $5::integer, $6::integer, $7::text) AS value',
      [guid, board, sort, dir === 'desc', offset, limit, playerId]);
    if (!result)
      return FORBIDDEN;
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

  return { registerServer, submitStats, getAllLeaderboards, getLeaderboard, submitTransport, getTransportRatings };
}

module.exports = { createHandlers };
