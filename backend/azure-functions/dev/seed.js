// Made-up data for the dev database, the same on every start. Nothing here is
// copied from production: every id is 5eed0000-... and every name says
// Synthetic.
//
//   servers : 5eed0000-0000-4000-8000-<n as 12 hex digits>, n from 1
//   players : 5eed0000-0000-4000-9000-<n as 12 hex digits>, n from 1
//
// With the defaults: 100 servers, 20,000 players with statistics on one to six
// servers each (about 31,000 rows, a few servers holding most of them), every
// seventh player with a transport rating, and 200 more players who have a
// rating and no statistics. Among the servers are ones still on each default
// name (left off the servers board), one with a half-replaced placeholder, one
// inactive and some with names longer than the 48 characters a board shows.
// Some players have long names and some have none.

const NEW_DEFAULT = 'Default Name - PLEASE RENAME IN server_name.txt, in I&A Server Profile Folder';
const OLD_DEFAULT = 'Server Name -PLEASE RENAME IN server_name.txt, in Server Files';
const HALF_EDITED = 'Synthetic.txt - PLEASE RENAME IN server_name.txt, in I&A Server Profile Folder';

const DEFAULT_SERVERS = 100;
const DEFAULT_PLAYERS = 20000;
const TRANSPORT_ONLY_PLAYERS = 200;

function serverId(n) {
  return '5eed0000-0000-4000-8000-' + n.toString(16).padStart(12, '0');
}

function playerId(n) {
  return '5eed0000-0000-4000-9000-' + n.toString(16).padStart(12, '0');
}

// A number from 0 up to 1 that depends only on its arguments.
const HELPERS = `
  CREATE SCHEMA IF NOT EXISTS dev_seed;

  CREATE OR REPLACE FUNCTION dev_seed.unit(p_salt text, p_n bigint)
  RETURNS double precision LANGUAGE sql IMMUTABLE AS $f$
    SELECT (('x' || substr(md5(p_salt || ':' || p_n), 1, 8))::bit(32)::bigint) / 4294967296.0
  $f$;

  CREATE OR REPLACE FUNCTION dev_seed.server_id(p_n bigint)
  RETURNS uuid LANGUAGE sql IMMUTABLE AS $f$
    SELECT ('5eed0000-0000-4000-8000-' || lpad(to_hex(p_n), 12, '0'))::uuid
  $f$;

  CREATE OR REPLACE FUNCTION dev_seed.player_id(p_n bigint)
  RETURNS text LANGUAGE sql IMMUTABLE AS $f$
    SELECT '5eed0000-0000-4000-9000-' || lpad(to_hex(p_n), 12, '0')
  $f$;
`;

// options: { servers, players }. Returns the row counts.
async function seed(pg, options) {
  const settings = options || {};
  const servers = settings.servers || DEFAULT_SERVERS;
  const players = settings.players || DEFAULT_PLAYERS;
  const transportOnly = Math.min(TRANSPORT_ONLY_PLAYERS, Math.ceil(players / 100));

  await pg.exec(HELPERS);

  await pg.query(`
    INSERT INTO public.servers (id, name, is_active, last_seen)
    SELECT dev_seed.server_id(n),
           CASE
             WHEN n % 20 = 0 THEN $2
             WHEN n % 33 = 0 THEN $3
             WHEN n = 41 THEN 'My IA Server'
             WHEN n = 57 THEN $4
             WHEN n % 9 = 0 THEN 'Synthetic Server ' || lpad(n::text, 3, '0')
                                 || ' | Invade & Annex | Made-up community with a very long name'
             ELSE 'Synthetic Server ' || lpad(n::text, 3, '0')
           END,
           n <> 97,
           now()
      FROM generate_series(1, $1::integer) AS n`,
    [servers, NEW_DEFAULT, OLD_DEFAULT, HALF_EDITED]);

  // Each player is on one to six servers. The first is drawn towards the low
  // numbers, so a few servers are large; the others step by 37, which shares
  // no factor with 100, so one player never lands on a server twice.
  await pg.query(`
    INSERT INTO public.player_stats
           (player_bohemia_id, server_id, player_name_last_seen,
            kills, deaths, hvt_kills, hvt_guard_kills, contribution_score, last_seen)
    SELECT dev_seed.player_id(p),
           dev_seed.server_id(((floor($2::integer * power(dev_seed.unit('home', p), 2.2))::integer + j * 37) % $2::integer) + 1),
           CASE
             WHEN p % 211 = 0 THEN ''
             WHEN p % 50 = 0 THEN 'Synthetic Player ' || lpad(p::text, 5, '0') || ' with a name that runs well past the board'
             ELSE 'Synthetic Player ' || lpad(p::text, 5, '0')
           END,
           floor(-ln(1 - dev_seed.unit('kills', p * 7 + j)) * 40)::integer,
           floor(-ln(1 - dev_seed.unit('deaths', p * 7 + j)) * 25)::integer,
           floor(power(dev_seed.unit('hvt', p * 7 + j), 6) * 20)::integer,
           floor(power(dev_seed.unit('guard', p * 7 + j), 4) * 60)::integer,
           floor(-ln(1 - dev_seed.unit('obj', p * 7 + j)) * 3000)::bigint,
           now() - make_interval(mins => j)
      FROM generate_series(1, $1::integer) AS p
     CROSS JOIN LATERAL (
       SELECT CASE
                WHEN dev_seed.unit('count', p) < 0.62 THEN 1
                WHEN dev_seed.unit('count', p) < 0.87 THEN 2
                WHEN dev_seed.unit('count', p) < 0.97 THEN 3
                ELSE 4 + (p % 3)
              END AS on_servers) c
     CROSS JOIN LATERAL generate_series(0, LEAST(c.on_servers, $2::integer) - 1) AS j`,
    [players, servers]);

  // Every seventh player flies, on the first of their servers. A further few
  // have a rating and no statistics at all.
  await pg.query(`
    INSERT INTO public.player_transport_server
           (player_bohemia_id, server_id, rating, insertions, player_name_last_seen)
    SELECT dev_seed.player_id(p),
           dev_seed.server_id((floor($2::integer * power(dev_seed.unit('home', p), 2.2))::integer % $2::integer) + 1),
           r.rating, r.rating / 20 + 1,
           'Synthetic Player ' || lpad(p::text, 5, '0')
      FROM generate_series(1, $1::integer + $3::integer) AS p
     CROSS JOIN LATERAL (SELECT 1 + floor(-ln(1 - dev_seed.unit('rating', p)) * 2000)::bigint AS rating) r
     WHERE p % 7 = 0 OR p > $1::integer`,
    [players, servers, transportOnly]);

  await pg.exec(`
    INSERT INTO public.player_transport_ratings
           (player_bohemia_id, rating, insertions, player_name_last_seen)
    SELECT ts.player_bohemia_id, sum(ts.rating), sum(ts.insertions), max(ts.player_name_last_seen)
      FROM public.player_transport_server ts
     GROUP BY ts.player_bohemia_id;

    SELECT public.ia_refresh_board('global', true);
    SELECT public.ia_refresh_board('servers', true);
  `);

  const counts = (await pg.query(`
    SELECT (SELECT count(*) FROM public.servers)::integer AS servers,
           (SELECT count(DISTINCT player_bohemia_id) FROM public.player_stats)::integer AS players,
           (SELECT count(*) FROM public.player_stats)::integer AS "statRows",
           (SELECT count(*) FROM public.player_transport_ratings)::integer AS pilots,
           (SELECT total FROM public.leaderboard_cache_state WHERE board = 'global') AS "globalBoard",
           (SELECT total FROM public.leaderboard_cache_state WHERE board = 'servers') AS "serversBoard",
           (SELECT max(n) FROM (SELECT count(*)::integer AS n FROM public.player_stats GROUP BY server_id) x) AS "largestServer"`)).rows[0];
  return counts;
}

module.exports = { seed, serverId, playerId, NEW_DEFAULT, OLD_DEFAULT, HALF_EDITED, DEFAULT_SERVERS, DEFAULT_PLAYERS };
