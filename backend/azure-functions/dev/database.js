// An in-process Postgres (PGlite) built the way the production database was:
// the tables that were made by hand, then every file in
// backend/supabase/migrations. The dev server and the database tests use it.
// PGlite is a dev dependency, and nothing under src/ may require this folder.

const fs = require('node:fs');
const path = require('node:path');

const MIGRATIONS = path.join(__dirname, '..', '..', 'supabase', 'migrations');
const ROLLBACKS = path.join(__dirname, '..', '..', 'supabase', 'rollbacks');

// What the migrations build on. The tables and register_server were made by
// hand in the Supabase project and have no migration of their own.
const BASELINE = `
  DO $f$
  DECLARE
    wanted text;
  BEGIN
    FOREACH wanted IN ARRAY ARRAY['anon', 'authenticated', 'service_role'] LOOP
      IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = wanted) THEN
        EXECUTE format('CREATE ROLE %I NOLOGIN', wanted);
      END IF;
    END LOOP;
  END
  $f$;

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

// Stand-ins for the three functions behind GET /getAllLeaderboards. The real
// ones were made by hand and have no source here, so these only give the route
// its three arrays; the columns inside are not production's. They are created
// after the migrations, because they read ia_leaderboard_rows.
const LEGACY_STANDINS = `
  CREATE FUNCTION public.get_global_leaderboard()
  RETURNS TABLE ("PlayerName" text, kills bigint, deaths bigint, score bigint)
  LANGUAGE sql STABLE AS $f$
    SELECT r.name, r.kills, r.deaths, r.score
      FROM public.ia_leaderboard_rows('global', NULL) r
     ORDER BY r.score DESC, r.id LIMIT 100
  $f$;

  CREATE FUNCTION public.get_server_leaderboard(p_server uuid)
  RETURNS TABLE ("PlayerName" text, kills bigint, deaths bigint, score bigint)
  LANGUAGE sql STABLE AS $f$
    SELECT r.name, r.kills, r.deaths, r.score
      FROM public.ia_leaderboard_rows('server', p_server) r
     ORDER BY r.score DESC, r.id LIMIT 100
  $f$;

  CREATE FUNCTION public.get_global_server_leaderboard()
  RETURNS TABLE ("ServerName" text, kills bigint, deaths bigint, score bigint)
  LANGUAGE sql STABLE AS $f$
    SELECT r.name, r.kills, r.deaths, r.score
      FROM public.ia_leaderboard_rows('servers', NULL) r
     ORDER BY r.score DESC, r.id LIMIT 100
  $f$;
`;

function migrationFiles() {
  return fs.readdirSync(MIGRATIONS).filter(name => name.endsWith('.sql')).sort();
}

function migration(name) {
  return fs.readFileSync(path.join(MIGRATIONS, name), 'utf8');
}

function rollback(name) {
  return fs.readFileSync(path.join(ROLLBACKS, name), 'utf8');
}

function loadPGlite() {
  try {
    return require('@electric-sql/pglite').PGlite;
  } catch (e) {
    // Installed with --omit=dev.
    return null;
  }
}

// Builds the schema in a database that is empty. pg is anything with
// exec(sql) and query(sql, params), as PGlite has.
// options.before: { '<migration file>': sql } run just before that file.
// options.after:  { '<migration file>': sql } run just after it.
// options.legacy: also create the /getAllLeaderboards stand-ins.
async function buildDatabase(pg, options) {
  const settings = options || {};
  await pg.exec(BASELINE);
  for (const file of migrationFiles()) {
    if (settings.before && settings.before[file])
      await pg.exec(settings.before[file]);
    await pg.exec(migration(file));
    if (settings.after && settings.after[file])
      await pg.exec(settings.after[file]);
  }
  if (settings.legacy)
    await pg.exec(LEGACY_STANDINS);
}

// What createHandlers takes, over pg.
function scalarDb(pg) {
  return {
    scalar: async (sql, params) => (await pg.query(sql, params)).rows[0].value
  };
}

// A new in-process database. Options are those of buildDatabase.
// Returns { pg, db } where db is what createHandlers takes.
async function openDatabase(options) {
  const PGlite = loadPGlite();
  if (!PGlite)
    throw new Error('PGlite is not installed. Run npm install in backend/azure-functions.');

  const pg = new PGlite();
  await buildDatabase(pg, options);
  return { pg, db: scalarDb(pg) };
}

module.exports = { openDatabase, buildDatabase, scalarDb, loadPGlite, migrationFiles, migration, rollback, BASELINE, LEGACY_STANDINS };
