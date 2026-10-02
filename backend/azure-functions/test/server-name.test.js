// The server name rule lives in the database (api_submit_stats), so these run
// the real migrations in an in-process Postgres and call them through the real
// handlers. PGlite is a dev dependency; without it the suite is skipped.

const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { createHandlers } = require('../src/handlers');

let PGlite = null;
try {
  PGlite = require('@electric-sql/pglite').PGlite;
} catch (e) {
  // Installed with --omit=dev.
}

const MIGRATIONS = path.join(__dirname, '..', '..', 'supabase', 'migrations');
const OLD_DEFAULT = 'Server Name -PLEASE RENAME IN server_name.txt, in Server Files';
const NEW_DEFAULT = 'Default Name - PLEASE RENAME IN server_name.txt, in I&A Server Profile Folder';
const HALF_EDITED = 'Ravens.txt - PLEASE RENAME IN server_name.txt, in I&A Server Profile Folder';
const UNKNOWN = '11111111-2222-4333-8444-555555555555';

// What the migrations build on: the tables and register_server were made by
// hand in the Supabase project and have no migration of their own.
const BASELINE = `
  CREATE ROLE anon NOLOGIN;
  CREATE ROLE authenticated NOLOGIN;
  CREATE ROLE service_role NOLOGIN;

  CREATE TABLE public.servers (
    id          uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    name        varchar NOT NULL,
    owner_email varchar,
    created_at  timestamptz NOT NULL DEFAULT now(),
    last_seen   timestamptz,
    is_active   boolean NOT NULL DEFAULT true
  );

  CREATE TABLE public.player_stats (
    player_bohemia_id     text NOT NULL,
    server_id             uuid NOT NULL REFERENCES public.servers(id) ON DELETE CASCADE,
    player_name_last_seen text,
    kills                 integer DEFAULT 0,
    deaths                integer DEFAULT 0,
    first_seen            timestamptz DEFAULT now(),
    last_seen             timestamptz DEFAULT now(),
    hvt_kills             integer DEFAULT 0,
    hvt_guard_kills       integer DEFAULT 0,
    contribution_score    bigint NOT NULL DEFAULT 0,
    PRIMARY KEY (player_bohemia_id, server_id)
  );

  CREATE FUNCTION public.register_server(server_name varchar, server_owner_email varchar DEFAULT NULL)
  RETURNS uuid LANGUAGE plpgsql SECURITY DEFINER AS $f$
  DECLARE
    new_server_id uuid;
  BEGIN
    INSERT INTO public.servers (name, owner_email)
    VALUES (server_name, server_owner_email)
    RETURNING id INTO new_server_id;
    RETURN new_server_id;
  END;
  $f$;
`;

function post(body) {
  return { text: async () => JSON.stringify(body), query: new URLSearchParams() };
}

function migration(name) {
  return fs.readFileSync(path.join(MIGRATIONS, name), 'utf8');
}

test('server name', { skip: PGlite ? false : 'install dev dependencies to run the database tests' }, async (t) => {
  const pg = new PGlite();
  t.after(() => pg.close());

  await pg.exec(BASELINE);
  for (const file of fs.readdirSync(MIGRATIONS).sort())
    await pg.exec(migration(file));

  const handlers = createHandlers({
    scalar: async (sql, params) => (await pg.query(sql, params)).rows[0].value
  });

  async function register(name) {
    const res = await handlers.registerServer(post({ serverName: name, ownerEmail: '' }));
    assert.strictEqual(res.status, 200);
    return JSON.parse(res.body).serverGuid;
  }

  async function submit(guid, name, events) {
    const body = { serverGuid: guid, matchData: events || [] };
    if (name !== undefined)
      body.serverName = name;
    return handlers.submitStats(post(body));
  }

  async function row(guid) {
    return (await pg.query('SELECT name, last_seen FROM public.servers WHERE id = $1', [guid])).rows[0];
  }

  async function stored(guid) {
    return (await row(guid)).name;
  }

  async function count(sql, params) {
    return Number((await pg.query(sql, params)).rows[0].n);
  }

  await t.test('a real name replaces the stored one, and the next one replaces that', async () => {
    const guid = await register(NEW_DEFAULT);
    assert.strictEqual((await submit(guid, 'Alpha | Invade & Annex')).status, 200);
    assert.strictEqual(await stored(guid), 'Alpha | Invade & Annex');
    await submit(guid, 'Bravo');
    assert.strictEqual(await stored(guid), 'Bravo');
  });

  await t.test('a missing, empty or blank name keeps the stored one and still marks the server seen', async () => {
    const guid = await register('Alpha');
    for (const name of [undefined, '', '   ', '\t', '\r\n', 7, null]) {
      assert.strictEqual((await submit(guid, name)).status, 200);
      assert.strictEqual(await stored(guid), 'Alpha');
    }
    assert.ok((await row(guid)).last_seen);
  });

  await t.test('a placeholder does not replace a real name', async () => {
    const guid = await register('Alpha');
    for (const name of [NEW_DEFAULT, OLD_DEFAULT, HALF_EDITED, NEW_DEFAULT.toLowerCase(), '  ' + OLD_DEFAULT + '\r']) {
      assert.strictEqual((await submit(guid, name)).status, 200);
      assert.strictEqual(await stored(guid), 'Alpha');
    }
  });

  await t.test('a placeholder is accepted for a new server and gives way to a real name', async () => {
    const guid = await register(NEW_DEFAULT);
    assert.strictEqual(await stored(guid), NEW_DEFAULT);

    // Nothing real is stored yet, so a placeholder still replaces a placeholder.
    await submit(guid, HALF_EDITED);
    assert.strictEqual(await stored(guid), HALF_EDITED);

    await submit(guid, 'Alpha');
    assert.strictEqual(await stored(guid), 'Alpha');
    await submit(guid, NEW_DEFAULT);
    assert.strictEqual(await stored(guid), 'Alpha');
  });

  await t.test('a stored name is trimmed and never longer than 255 characters', async () => {
    const guid = await register('  ' + 'r'.repeat(400) + '  ');
    assert.strictEqual(await stored(guid), 'r'.repeat(255));

    await submit(guid, ' \t' + 's'.repeat(400) + '\r\n');
    assert.strictEqual(await stored(guid), 's'.repeat(255));
    await submit(guid, '  Alpha \r\n');
    assert.strictEqual(await stored(guid), 'Alpha');
  });

  await t.test('the GUID alone selects the row that is written', async () => {
    const mine = await register('Mine');
    const theirs = await register('Theirs');
    const servers = await count('SELECT count(*) AS n FROM public.servers');
    const kill = [{ eventType: 'PlayerKill', killerPlayerId: 'p1', killerPlayerName: 'One' }];

    // Sending another server's name only relabels the sender.
    const res = await submit(mine, 'Theirs', kill);
    assert.strictEqual(res.body, '{"status":"ok","players":1}');
    assert.strictEqual(await stored(mine), 'Theirs');
    assert.strictEqual(await stored(theirs), 'Theirs');
    assert.strictEqual((await row(theirs)).last_seen, null);
    assert.strictEqual(await count('SELECT count(*) AS n FROM public.player_stats WHERE server_id = $1', [mine]), 1);
    assert.strictEqual(await count('SELECT count(*) AS n FROM public.player_stats WHERE server_id = $1', [theirs]), 0);

    // A name that exists does not stand in for a GUID that does not.
    const stats = await count('SELECT count(*) AS n FROM public.player_stats');
    assert.strictEqual((await submit(UNKNOWN, 'Theirs', kill)).status, 403);
    assert.strictEqual(await count('SELECT count(*) AS n FROM public.player_stats'), stats);
    assert.strictEqual(await count('SELECT count(*) AS n FROM public.servers'), servers);
  });

  await t.test('an inactive server is refused and keeps its name', async () => {
    const guid = await register('Alpha');
    await pg.query('UPDATE public.servers SET is_active = false WHERE id = $1', [guid]);
    assert.strictEqual((await submit(guid, 'Bravo')).status, 403);
    assert.strictEqual(await stored(guid), 'Alpha');
  });

  await t.test('statistics still add up across batches', async () => {
    const guid = await register('Alpha');
    const batch = [
      { eventType: 'PlayerKill', killerPlayerId: 'p1', killerPlayerName: 'One' },
      { eventType: 'PlayerDeath', victimPlayerId: 'p1', victimPlayerName: 'One' },
      { eventType: 'CaptureContribution', playerId: 'p1', playerName: 'One', score: '120' }
    ];
    await submit(guid, 'Alpha', batch);
    await submit(guid, NEW_DEFAULT, batch);
    const stats = (await pg.query(
      'SELECT kills, deaths, contribution_score FROM public.player_stats WHERE server_id = $1', [guid])).rows[0];
    assert.deepStrictEqual(
      [stats.kills, stats.deaths, Number(stats.contribution_score)], [2, 2, 240]);
  });

  await t.test('the function keeps the access it was given', async () => {
    const grants = (await pg.query(
      "SELECT grantee FROM information_schema.routine_privileges WHERE routine_schema = 'public' AND routine_name = 'api_submit_stats' AND grantee NOT IN (SELECT rolname FROM pg_roles WHERE rolsuper) ORDER BY 1")).rows;
    assert.deepStrictEqual(grants.map(g => g.grantee), ['ia_game_api', 'service_role']);

    const fn = (await pg.query(
      "SELECT prosecdef, proconfig, pg_get_function_identity_arguments(oid) AS args FROM pg_proc WHERE proname = 'api_submit_stats'")).rows;
    assert.strictEqual(fn.length, 1);
    assert.strictEqual(fn[0].prosecdef, true);
    assert.deepStrictEqual(fn[0].proconfig, ['search_path=public']);
    assert.strictEqual(fn[0].args, 'p_server_guid uuid, p_server_name text, p_events jsonb');
  });

  await t.test('only the name rule differs from the function it replaces', () => {
    // From the first statement after the server row is written to the end of the body.
    function statistics(file) {
      const sql = migration(file).replace(/\r\n/g, '\n');
      const start = sql.indexOf('  WITH parsed AS (');
      const end = sql.indexOf('\n$$;', start);
      assert.ok(start > 0 && end > start, file);
      return sql.slice(start, end);
    }
    assert.strictEqual(
      statistics('20261002000000_server_name_retention.sql'),
      statistics('20260930010000_game_api.sql'));
  });
});
