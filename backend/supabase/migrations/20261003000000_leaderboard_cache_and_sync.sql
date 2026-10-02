-- Leaderboards that cost the same however many players there are, and one
-- exchange per interval for a game server.
--
-- api_get_leaderboard added up every row of player_stats on each call for the
-- global and servers boards, whatever was asked: a page, or only the asking
-- player's own line. Those two boards are now kept in leaderboard_cache. A call
-- reads the cache, and rebuilds it first when it is older than
-- ia_board_refresh_seconds().
--
--   * The cache is rebuilt from the source tables by ia_leaderboard_rows, the
--     same query the boards used before. Nothing is added to it in passing, so
--     it cannot drift; a rebuild only writes the rows that differ.
--   * One caller rebuilds. The others, and anyone who asks while it runs, read
--     the board as it was. A board is at most one interval behind.
--   * The per-server board is read from player_stats by its server index, as
--     before.
--
-- api_get_leaderboard keeps its arguments, its answer and its grants. It was
-- STABLE; it is VOLATILE now, because it may rebuild the cache.
--
-- api_sync is new: POST /sync (backend/azure-functions). One call carries a
-- stats batch, a transport batch, a ratings request, the boards a server wants
-- and the players whose own lines it wants. Each part reports its own result.
--
-- Everything here is additive. Rollback: backend/supabase/rollbacks/ holds the
-- file of the same name.

BEGIN;

-- How old a cached board may be before the next caller rebuilds it. This is
-- the one place to change it.
CREATE OR REPLACE FUNCTION public.ia_board_refresh_seconds()
RETURNS integer
LANGUAGE sql
STABLE
AS $$ SELECT 60 $$;

-- Rows in a board snapshot: the top of the board by score, which /sync hands
-- out. At most 100, the most a page may hold.
CREATE OR REPLACE FUNCTION public.ia_board_snapshot_rows()
RETURNS integer
LANGUAGE sql
STABLE
AS $$ SELECT 100 $$;

-- The global and servers boards, one row per player or server, unsorted.
-- Columns are those of ia_leaderboard_rows.
CREATE TABLE IF NOT EXISTS public.leaderboard_cache (
  board      text   NOT NULL CHECK (board IN ('global', 'servers')),
  id         text   NOT NULL,
  name       text   NOT NULL,
  kills      bigint NOT NULL,
  deaths     bigint NOT NULL,
  hvt        bigint NOT NULL,
  guard      bigint NOT NULL,
  obj        bigint NOT NULL,
  score      bigint NOT NULL,
  transport  bigint NOT NULL,
  insertions bigint NOT NULL,
  players    bigint NOT NULL,
  PRIMARY KEY (board, id)
);

-- One row per cached board: when it was rebuilt and its snapshot. etag is a
-- hash of the snapshot rows, so it changes only when the top of the board does.
CREATE TABLE IF NOT EXISTS public.leaderboard_cache_state (
  board        text PRIMARY KEY CHECK (board IN ('global', 'servers')),
  refreshed_at timestamptz,
  total        integer NOT NULL DEFAULT 0,
  etag         text    NOT NULL DEFAULT '',
  snapshot     jsonb   NOT NULL DEFAULT '[]'::jsonb,
  refresh_ms   integer NOT NULL DEFAULT 0,
  rows_written integer NOT NULL DEFAULT 0
);

INSERT INTO public.leaderboard_cache_state (board)
VALUES ('global'), ('servers')
ON CONFLICT (board) DO NOTHING;

-- One row per stats batch that came with an id, so a batch sent twice is
-- counted once. Rows older than a week are removed as new ones arrive.
CREATE TABLE IF NOT EXISTS public.stats_batches (
  server_guid uuid    NOT NULL REFERENCES public.servers(id) ON DELETE CASCADE,
  batch_id    text    NOT NULL,
  players     integer NOT NULL DEFAULT 0,
  received_at timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY (server_guid, batch_id)
);

CREATE INDEX IF NOT EXISTS stats_batches_received_at_idx
  ON public.stats_batches (received_at);

ALTER TABLE public.leaderboard_cache       ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.leaderboard_cache_state ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.stats_batches           ENABLE ROW LEVEL SECURITY;
REVOKE ALL ON public.leaderboard_cache       FROM anon, authenticated;
REVOKE ALL ON public.leaderboard_cache_state FROM anon, authenticated;
REVOKE ALL ON public.stats_batches           FROM anon, authenticated;

-- One board row as the game reads it. The game reads 32-bit integers.
CREATE OR REPLACE FUNCTION public.ia_board_row_json(
  p_rank bigint, p_name text,
  p_kills bigint, p_deaths bigint, p_hvt bigint, p_guard bigint, p_obj bigint, p_score bigint,
  p_transport bigint, p_insertions bigint, p_players bigint
)
RETURNS jsonb
LANGUAGE sql
IMMUTABLE
AS $$
  SELECT jsonb_build_object(
    'r', p_rank,
    'n', left(p_name, 48),
    'k', LEAST(p_kills, 2000000000),
    'd', LEAST(p_deaths, 2000000000),
    'h', LEAST(p_hvt, 2000000000),
    'g', LEAST(p_guard, 2000000000),
    'o', LEAST(p_obj, 2000000000),
    's', GREATEST(LEAST(p_score, 2000000000), -2000000000),
    't', LEAST(p_transport, 2000000000),
    'i', LEAST(p_insertions, 2000000000),
    'p', LEAST(p_players, 2000000000))
$$;

-- True when this transaction may rebuild p_board. The lock is released when
-- the transaction ends, which suits the transaction pooler the API connects
-- through.
CREATE OR REPLACE FUNCTION public.ia_board_refresh_lock(p_board text)
RETURNS boolean
LANGUAGE sql
VOLATILE
AS $$
  SELECT pg_try_advisory_xact_lock(1229201473, CASE p_board WHEN 'global' THEN 1 ELSE 2 END)
$$;

-- Rebuild a cached board when it is due. Returns true when this call rebuilt it.
--   p_force: rebuild whatever its age.
CREATE OR REPLACE FUNCTION public.ia_refresh_board(p_board text, p_force boolean DEFAULT false)
RETURNS boolean
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = public
AS $$
DECLARE
  v_started  timestamptz := clock_timestamp();
  v_at       timestamptz;
  v_written  integer;
  v_total    integer;
  v_snapshot jsonb;
  v_etag     text;
BEGIN
  IF p_board IS NULL OR p_board NOT IN ('global', 'servers') THEN
    RETURN false;
  END IF;

  SELECT s.refreshed_at INTO v_at FROM public.leaderboard_cache_state s WHERE s.board = p_board;
  IF NOT COALESCE(p_force, false) AND v_at IS NOT NULL
     AND v_at > clock_timestamp() - make_interval(secs => public.ia_board_refresh_seconds()) THEN
    RETURN false;
  END IF;

  -- Someone else is rebuilding: this caller reads the board as it was.
  IF NOT public.ia_board_refresh_lock(p_board) THEN
    RETURN false;
  END IF;

  -- It may have been rebuilt between the check above and the lock.
  SELECT s.refreshed_at INTO v_at FROM public.leaderboard_cache_state s WHERE s.board = p_board;
  IF NOT COALESCE(p_force, false) AND v_at IS NOT NULL
     AND v_at > clock_timestamp() - make_interval(secs => public.ia_board_refresh_seconds()) THEN
    RETURN false;
  END IF;

  WITH fresh AS MATERIALIZED (
    SELECT f.id, COALESCE(f.name, '') AS name,
           COALESCE(f.kills, 0) AS kills, COALESCE(f.deaths, 0) AS deaths,
           COALESCE(f.hvt, 0) AS hvt, COALESCE(f.guard, 0) AS guard,
           COALESCE(f.obj, 0) AS obj, COALESCE(f.score, 0) AS score,
           COALESCE(f.transport, 0) AS transport, COALESCE(f.insertions, 0) AS insertions,
           COALESCE(f.players, 0) AS players
      FROM public.ia_leaderboard_rows(p_board, NULL) f
     WHERE f.id IS NOT NULL
  ), gone AS (
    DELETE FROM public.leaderboard_cache c
     WHERE c.board = p_board
       AND NOT EXISTS (SELECT 1 FROM fresh f WHERE f.id = c.id)
    RETURNING 1
  ), put AS (
    INSERT INTO public.leaderboard_cache AS c
           (board, id, name, kills, deaths, hvt, guard, obj, score, transport, insertions, players)
    SELECT p_board, f.id, f.name, f.kills, f.deaths, f.hvt, f.guard, f.obj, f.score,
           f.transport, f.insertions, f.players
      FROM fresh f
      LEFT JOIN public.leaderboard_cache o ON o.board = p_board AND o.id = f.id
     -- Only a row that is new or differs is written; the rest are not touched.
     WHERE o.id IS NULL
        OR (o.name, o.kills, o.deaths, o.hvt, o.guard, o.obj, o.score, o.transport, o.insertions, o.players)
           IS DISTINCT FROM
           (f.name, f.kills, f.deaths, f.hvt, f.guard, f.obj, f.score, f.transport, f.insertions, f.players)
    ON CONFLICT (board, id) DO UPDATE
       SET name = EXCLUDED.name, kills = EXCLUDED.kills, deaths = EXCLUDED.deaths,
           hvt = EXCLUDED.hvt, guard = EXCLUDED.guard, obj = EXCLUDED.obj, score = EXCLUDED.score,
           transport = EXCLUDED.transport, insertions = EXCLUDED.insertions, players = EXCLUDED.players
    RETURNING 1
  )
  SELECT (SELECT count(*) FROM gone) + (SELECT count(*) FROM put) INTO v_written;

  SELECT count(*) INTO v_total FROM public.leaderboard_cache c WHERE c.board = p_board;

  SELECT COALESCE(jsonb_agg(public.ia_board_row_json(
           t.rank, t.name, t.kills, t.deaths, t.hvt, t.guard, t.obj, t.score,
           t.transport, t.insertions, t.players) ORDER BY t.rank), '[]'::jsonb)
    INTO v_snapshot
    FROM (SELECT c.*, row_number() OVER (ORDER BY c.score DESC, c.id) AS rank
            FROM public.leaderboard_cache c
           WHERE c.board = p_board
           ORDER BY c.score DESC, c.id
           LIMIT public.ia_board_snapshot_rows()) t;
  v_etag := md5(v_snapshot::text);

  -- The snapshot is stored again only when it changed.
  UPDATE public.leaderboard_cache_state s
     SET etag = v_etag, snapshot = v_snapshot
   WHERE s.board = p_board AND s.etag IS DISTINCT FROM v_etag;

  UPDATE public.leaderboard_cache_state s
     SET refreshed_at = clock_timestamp(),
         total        = v_total,
         rows_written = v_written,
         refresh_ms   = (extract(epoch FROM clock_timestamp() - v_started) * 1000)::integer
   WHERE s.board = p_board;

  RETURN true;
END;
$$;

-- GET /leaderboard. One page of one board.
--   p_board : global | server | servers            (anything else: global)
--   p_sort  : score | kills | deaths | kd | hvt | guard | obj | transport |
--             insertions | players                 (anything else: score)
--   p_limit : 0..100 rows; 0 asks for the totals and "me" only
--   p_player_id : whose own row to return in "me"; ignored on the servers board,
--                 where "me" is the asking server
-- Returns NULL for an unknown or inactive server, else
--   {"board","sort","dir","offset","total","rows":[row],"me":[row] or []}
--   row = {"r" rank in this sort,"n" name,"k","d","h","g","o","s","t","i","p"}
-- Rows carry no player ids. Ties are broken by score, then by id, so a page is
-- stable between calls.
--
-- global and servers are read from leaderboard_cache: a page sorts the cached
-- rows and keeps the ones asked for, and the own row's rank is a count of the
-- rows ahead of it. The answer is the one 20261001030000 gave on the same rows.
CREATE OR REPLACE FUNCTION public.api_get_leaderboard(
  p_server_guid uuid,
  p_board       text,
  p_sort        text,
  p_desc        boolean,
  p_offset      integer,
  p_limit       integer,
  p_player_id   text
)
RETURNS jsonb
LANGUAGE plpgsql
VOLATILE
SECURITY DEFINER
SET search_path = public
AS $$
DECLARE
  v_board  text    := COALESCE(p_board, 'global');
  v_sort   text    := COALESCE(p_sort, 'score');
  v_desc   boolean := COALESCE(p_desc, true);
  v_offset integer := GREATEST(COALESCE(p_offset, 0), 0);
  v_limit  integer := LEAST(GREATEST(COALESCE(p_limit, 25), 0), 100);
  v_me     text    := p_player_id;
  v_result jsonb;
BEGIN
  PERFORM 1 FROM public.servers sv WHERE sv.id = p_server_guid AND sv.is_active;
  IF NOT FOUND THEN
    RETURN NULL;
  END IF;

  IF v_board NOT IN ('global', 'server', 'servers') THEN
    v_board := 'global';
  END IF;
  IF v_sort NOT IN ('score', 'kills', 'deaths', 'kd', 'hvt', 'guard', 'obj', 'transport', 'insertions', 'players') THEN
    v_sort := 'score';
  END IF;
  IF v_board = 'servers' THEN
    v_me := p_server_guid::text;
  END IF;

  IF v_board = 'server' THEN
    -- One server's rows, found by index. As in 20261001030000.
    WITH keyed AS (
      SELECT b.*,
             CASE v_sort
               WHEN 'kills'      THEN b.kills::numeric
               WHEN 'deaths'     THEN b.deaths::numeric
               WHEN 'kd'         THEN b.kills::numeric / GREATEST(b.deaths, 1)
               WHEN 'hvt'        THEN b.hvt::numeric
               WHEN 'guard'      THEN b.guard::numeric
               WHEN 'obj'        THEN b.obj::numeric
               WHEN 'transport'  THEN b.transport::numeric
               WHEN 'insertions' THEN b.insertions::numeric
               WHEN 'players'    THEN b.players::numeric
               ELSE b.score::numeric
             END AS sort_value
        FROM public.ia_leaderboard_rows('server', p_server_guid) b
    ), ranked AS (
      SELECT q.*,
             row_number() OVER (ORDER BY
               CASE WHEN v_desc THEN q.sort_value END DESC NULLS LAST,
               CASE WHEN NOT v_desc THEN q.sort_value END ASC NULLS LAST,
               q.score DESC, q.id) AS rank
        FROM keyed q
    ), shaped AS (
      -- Only the rows that are answered are turned into JSON.
      SELECT z.id, z.rank,
             public.ia_board_row_json(z.rank, z.name, z.kills, z.deaths, z.hvt, z.guard, z.obj,
                                      z.score, z.transport, z.insertions, z.players) AS row_json
        FROM ranked z
       WHERE (z.rank > v_offset AND z.rank <= v_offset + v_limit)
          OR (v_me IS NOT NULL AND z.id = v_me)
    )
    SELECT jsonb_build_object(
             'board',  v_board,
             'sort',   v_sort,
             'dir',    CASE WHEN v_desc THEN 'desc' ELSE 'asc' END,
             'offset', v_offset,
             'total',  (SELECT count(*) FROM ranked),
             'rows',   COALESCE((SELECT jsonb_agg(a.row_json ORDER BY a.rank)
                                   FROM shaped a
                                  WHERE a.rank > v_offset AND a.rank <= v_offset + v_limit), '[]'::jsonb),
             'me',     COALESCE((SELECT jsonb_agg(m.row_json)
                                   FROM shaped m
                                  WHERE v_me IS NOT NULL AND m.id = v_me), '[]'::jsonb))
      INTO v_result;
    RETURN v_result;
  END IF;

  PERFORM public.ia_refresh_board(v_board);

  -- One statement, so the total, the page and the own row are of one rebuild.
  WITH keyed AS NOT MATERIALIZED (
    SELECT c.*,
           CASE v_sort
             WHEN 'kills'      THEN c.kills::numeric
             WHEN 'deaths'     THEN c.deaths::numeric
             WHEN 'kd'         THEN c.kills::numeric / GREATEST(c.deaths, 1)
             WHEN 'hvt'        THEN c.hvt::numeric
             WHEN 'guard'      THEN c.guard::numeric
             WHEN 'obj'        THEN c.obj::numeric
             WHEN 'transport'  THEN c.transport::numeric
             WHEN 'insertions' THEN c.insertions::numeric
             WHEN 'players'    THEN c.players::numeric
             ELSE c.score::numeric
           END AS sort_value
      FROM public.leaderboard_cache c
     WHERE c.board = v_board
  ), page AS (
    -- The rows asked for and no others; their rank is their place after the offset.
    SELECT k.*,
           v_offset + row_number() OVER (ORDER BY
             CASE WHEN v_desc THEN k.sort_value END DESC NULLS LAST,
             CASE WHEN NOT v_desc THEN k.sort_value END ASC NULLS LAST,
             k.score DESC, k.id) AS rank
      FROM (SELECT q.*
              FROM keyed q
             ORDER BY CASE WHEN v_desc THEN q.sort_value END DESC NULLS LAST,
                      CASE WHEN NOT v_desc THEN q.sort_value END ASC NULLS LAST,
                      q.score DESC, q.id
             LIMIT v_limit OFFSET v_offset) k
  ), mine AS (
    -- The own row's rank is one more than the rows ahead of it in this order.
    SELECT m.*,
           1 + (SELECT count(*)
                  FROM keyed o
                 WHERE CASE WHEN v_desc THEN o.sort_value > m.sort_value
                            ELSE o.sort_value < m.sort_value END
                    OR (o.sort_value = m.sort_value
                        AND (o.score > m.score OR (o.score = m.score AND o.id < m.id)))) AS rank
      FROM keyed m
     WHERE v_me IS NOT NULL AND m.id = v_me
  )
  SELECT jsonb_build_object(
           'board',  v_board,
           'sort',   v_sort,
           'dir',    CASE WHEN v_desc THEN 'desc' ELSE 'asc' END,
           'offset', v_offset,
           'total',  (SELECT s.total FROM public.leaderboard_cache_state s WHERE s.board = v_board),
           'rows',   COALESCE((SELECT jsonb_agg(public.ia_board_row_json(
                                        a.rank, a.name, a.kills, a.deaths, a.hvt, a.guard, a.obj,
                                        a.score, a.transport, a.insertions, a.players) ORDER BY a.rank)
                                 FROM page a), '[]'::jsonb),
           'me',     COALESCE((SELECT jsonb_agg(public.ia_board_row_json(
                                        m.rank, m.name, m.kills, m.deaths, m.hvt, m.guard, m.obj,
                                        m.score, m.transport, m.insertions, m.players))
                                 FROM mine m), '[]'::jsonb))
    INTO v_result;

  RETURN v_result;
END;
$$;

-- POST /sync. One exchange for everything a game server sends and reads.
--   p_server_name         : as api_submit_stats; applied on every call.
--   p_events              : a stats batch (matchData), or NULL for none.
--   p_stats_batch_id      : its id, or NULL. A batch whose id was already
--                           accepted from this server is not counted again.
--   p_transport_entries   : a transport batch as submit_transport_batch reads it,
--   p_transport_batch_id    with its id; both NULL for none.
--   p_rating_ids          : player ids as get_transport_ratings reads them, or NULL.
--   p_players             : players whose own lines are wanted, at most 128.
--   p_boards              : [{"board": "server" | "global" | "servers",
--                             "etags": [text]}], the snapshots wanted and the
--                           etags the caller already holds for each.
-- Returns NULL for an unknown or inactive server, else
--   {"statsStatus","statsPlayers","transportStatus","transportPlayers",
--    "ratingsStatus","ratings":[],"skins":[],
--    "boardsStatus","boards":[{"board","etag","total","unchanged"}],
--    "serverRows":[row],"globalRows":[row],"serversRows":[row],
--    "own":[row + {"board","id"}],"snapshotRows"}
--   status : none (not sent) | accepted | duplicate | rejected (the batch is
--            malformed and would be refused again) | failed (not stored; send
--            it again with the same id). ratings and boards: none | ok | failed.
--
-- Each part runs in its own block: one that fails is undone and reported, and
-- the others stand. They commit together when the call returns, so a caller
-- that gets no answer resends the whole request, and the batch ids make that
-- harmless. p_server_guid alone selects the rows that are written.
CREATE OR REPLACE FUNCTION public.api_sync(
  p_server_guid        uuid,
  p_server_name        text,
  p_stats_batch_id     text,
  p_events             jsonb,
  p_transport_batch_id text,
  p_transport_entries  jsonb,
  p_rating_ids         text[],
  p_players            text[],
  p_boards             jsonb
)
RETURNS jsonb
LANGUAGE plpgsql
VOLATILE
SECURITY DEFINER
SET search_path = public
AS $$
DECLARE
  v_top       integer := public.ia_board_snapshot_rows();
  v_players   text[]  := '{}';
  v_inserted  integer;
  v_reply     jsonb;
  v_state     text;

  v_stats_status      text    := 'none';
  v_stats_players     integer := 0;
  v_transport_status  text    := 'none';
  v_transport_players integer := 0;
  v_ratings_status    text    := 'none';
  v_ratings           jsonb   := '[]'::jsonb;
  v_skins             jsonb   := '[]'::jsonb;

  v_boards_status text  := 'none';
  v_boards        jsonb := '[]'::jsonb;
  v_server_rows   jsonb := '[]'::jsonb;
  v_global_rows   jsonb := '[]'::jsonb;
  v_servers_rows  jsonb := '[]'::jsonb;
  v_own           jsonb := '[]'::jsonb;

  v_want  jsonb;
  v_held  text[];
  v_total integer;
  v_etag  text;
  v_rows  jsonb;
  v_lines jsonb;
BEGIN
  PERFORM 1 FROM public.servers sv WHERE sv.id = p_server_guid AND sv.is_active;
  IF NOT FOUND THEN
    RETURN NULL;
  END IF;

  -- Statistics. The name rule and last_seen are api_submit_stats's, applied
  -- with or without a batch.
  BEGIN
    IF p_events IS NOT NULL AND p_stats_batch_id IS NOT NULL THEN
      INSERT INTO public.stats_batches (server_guid, batch_id)
      VALUES (p_server_guid, p_stats_batch_id)
      ON CONFLICT (server_guid, batch_id) DO NOTHING;
      GET DIAGNOSTICS v_inserted = ROW_COUNT;
      IF v_inserted = 0 THEN
        v_stats_status := 'duplicate';
        SELECT b.players INTO v_stats_players
          FROM public.stats_batches b
         WHERE b.server_guid = p_server_guid AND b.batch_id = p_stats_batch_id;
      END IF;
    END IF;

    IF p_events IS NOT NULL AND v_stats_status <> 'duplicate' THEN
      v_reply := public.api_submit_stats(p_server_guid, p_server_name, p_events);
      v_stats_players := COALESCE((v_reply->>'players')::integer, 0);
      v_stats_status := 'accepted';
      IF p_stats_batch_id IS NOT NULL THEN
        UPDATE public.stats_batches b
           SET players = v_stats_players
         WHERE b.server_guid = p_server_guid AND b.batch_id = p_stats_batch_id;
      END IF;
    ELSE
      PERFORM public.api_submit_stats(p_server_guid, p_server_name, '[]'::jsonb);
    END IF;
  EXCEPTION WHEN OTHERS THEN
    GET STACKED DIAGNOSTICS v_state = RETURNED_SQLSTATE;
    v_stats_players := 0;
    IF v_state LIKE '22%' OR v_state LIKE '23%' OR v_state = 'P0001' THEN
      v_stats_status := 'rejected';
    ELSE
      v_stats_status := 'failed';
    END IF;
  END;

  -- Old batch ids. A batch is only ever resent within minutes.
  IF v_stats_status = 'accepted' AND p_stats_batch_id IS NOT NULL THEN
    BEGIN
      DELETE FROM public.stats_batches b
       USING (SELECT o.server_guid, o.batch_id
                FROM public.stats_batches o
               WHERE o.received_at < now() - interval '7 days'
               LIMIT 500
                 FOR UPDATE SKIP LOCKED) old
       WHERE b.server_guid = old.server_guid AND b.batch_id = old.batch_id;
    EXCEPTION WHEN OTHERS THEN
      NULL; -- Housekeeping never fails a batch; the next one tries again.
    END;
  END IF;

  -- Transport.
  IF p_transport_batch_id IS NOT NULL AND p_transport_entries IS NOT NULL THEN
    BEGIN
      v_reply := public.submit_transport_batch(p_server_guid, p_transport_batch_id, p_transport_entries);
      v_transport_players := COALESCE((v_reply->>'players')::integer, 0);
      IF (v_reply->>'applied')::boolean THEN
        v_transport_status := 'accepted';
      ELSE
        v_transport_status := 'duplicate';
      END IF;
    EXCEPTION WHEN OTHERS THEN
      GET STACKED DIAGNOSTICS v_state = RETURNED_SQLSTATE;
      v_transport_players := 0;
      IF v_state LIKE '22%' OR v_state LIKE '23%' OR v_state = 'P0001' THEN
        v_transport_status := 'rejected';
      ELSE
        v_transport_status := 'failed';
      END IF;
    END;
  END IF;

  -- Ratings, read after the transport batch so they include it.
  IF p_rating_ids IS NOT NULL THEN
    BEGIN
      v_reply := public.get_transport_ratings(p_rating_ids[1:256]);
      v_ratings := COALESCE(v_reply->'ratings', '[]'::jsonb);
      v_skins := COALESCE(v_reply->'skins', '[]'::jsonb);
      v_ratings_status := 'ok';
    EXCEPTION WHEN OTHERS THEN
      v_ratings := '[]'::jsonb;
      v_skins := '[]'::jsonb;
      v_ratings_status := 'failed';
    END;
  END IF;

  -- Boards and own lines, in the default order: score, high to low.
  IF p_players IS NOT NULL THEN
    SELECT COALESCE(array_agg(DISTINCT x.id), '{}') INTO v_players
      FROM unnest(p_players[1:128]) AS x(id)
     WHERE length(x.id) BETWEEN 1 AND 64;
  END IF;

  IF cardinality(v_players) > 0
     OR (p_boards IS NOT NULL AND jsonb_typeof(p_boards) = 'array' AND jsonb_array_length(p_boards) > 0) THEN
    BEGIN
      -- This server's board, and its players' lines on it.
      SELECT w.entry INTO v_want
        FROM jsonb_array_elements(COALESCE(p_boards, '[]'::jsonb)) AS w(entry)
       WHERE w.entry->>'board' = 'server'
       LIMIT 1;
      IF v_want IS NOT NULL OR cardinality(v_players) > 0 THEN
        WITH ranked AS (
          SELECT b.*, row_number() OVER (ORDER BY b.score DESC, b.id) AS rank
            FROM public.ia_leaderboard_rows('server', p_server_guid) b
        )
        SELECT count(*),
               COALESCE(jsonb_agg(public.ia_board_row_json(
                          z.rank, z.name, z.kills, z.deaths, z.hvt, z.guard, z.obj, z.score,
                          z.transport, z.insertions, z.players) ORDER BY z.rank)
                        FILTER (WHERE z.rank <= v_top), '[]'::jsonb),
               COALESCE(jsonb_agg(public.ia_board_row_json(
                          z.rank, z.name, z.kills, z.deaths, z.hvt, z.guard, z.obj, z.score,
                          z.transport, z.insertions, z.players)
                          || jsonb_build_object('board', 'server', 'id', z.id) ORDER BY z.rank)
                        FILTER (WHERE z.id = ANY (v_players)), '[]'::jsonb)
          INTO v_total, v_rows, v_lines
          FROM ranked z;
        v_own := v_own || v_lines;

        IF v_want IS NOT NULL THEN
          v_etag := md5(v_rows::text);
          SELECT COALESCE(array_agg(h.etag), '{}') INTO v_held
            FROM jsonb_array_elements_text(
                   CASE WHEN jsonb_typeof(v_want->'etags') = 'array' THEN v_want->'etags' ELSE '[]'::jsonb END) AS h(etag);
          IF v_etag = ANY (v_held) THEN
            v_boards := v_boards || jsonb_build_object('board', 'server', 'etag', v_etag, 'total', v_total, 'unchanged', true);
          ELSE
            v_boards := v_boards || jsonb_build_object('board', 'server', 'etag', v_etag, 'total', v_total, 'unchanged', false);
            v_server_rows := v_rows;
          END IF;
        END IF;
      END IF;

      -- The global board, and the players' lines on it.
      SELECT w.entry INTO v_want
        FROM jsonb_array_elements(COALESCE(p_boards, '[]'::jsonb)) AS w(entry)
       WHERE w.entry->>'board' = 'global'
       LIMIT 1;
      IF v_want IS NOT NULL OR cardinality(v_players) > 0 THEN
        PERFORM public.ia_refresh_board('global');
      END IF;
      IF cardinality(v_players) > 0 THEN
        SELECT COALESCE(jsonb_agg(public.ia_board_row_json(
                          t.rank, t.name, t.kills, t.deaths, t.hvt, t.guard, t.obj, t.score,
                          t.transport, t.insertions, t.players)
                          || jsonb_build_object('board', 'global', 'id', t.id) ORDER BY t.rank), '[]'::jsonb)
          INTO v_lines
          FROM (SELECT c.*, row_number() OVER (ORDER BY c.score DESC, c.id) AS rank
                  FROM public.leaderboard_cache c
                 WHERE c.board = 'global') t
         WHERE t.id = ANY (v_players);
        v_own := v_own || v_lines;
      END IF;
      IF v_want IS NOT NULL THEN
        SELECT COALESCE(array_agg(h.etag), '{}') INTO v_held
          FROM jsonb_array_elements_text(
                 CASE WHEN jsonb_typeof(v_want->'etags') = 'array' THEN v_want->'etags' ELSE '[]'::jsonb END) AS h(etag);
        -- The rows leave the table only when the caller does not hold them.
        SELECT s.etag, s.total,
               CASE WHEN s.etag = ANY (v_held) THEN NULL ELSE s.snapshot END
          INTO v_etag, v_total, v_rows
          FROM public.leaderboard_cache_state s
         WHERE s.board = 'global';
        v_boards := v_boards || jsonb_build_object('board', 'global', 'etag', v_etag, 'total', v_total, 'unchanged', v_rows IS NULL);
        v_global_rows := COALESCE(v_rows, '[]'::jsonb);
      END IF;

      -- The servers board, and this server's line on it.
      SELECT w.entry INTO v_want
        FROM jsonb_array_elements(COALESCE(p_boards, '[]'::jsonb)) AS w(entry)
       WHERE w.entry->>'board' = 'servers'
       LIMIT 1;
      IF v_want IS NOT NULL THEN
        PERFORM public.ia_refresh_board('servers');
        SELECT COALESCE(array_agg(h.etag), '{}') INTO v_held
          FROM jsonb_array_elements_text(
                 CASE WHEN jsonb_typeof(v_want->'etags') = 'array' THEN v_want->'etags' ELSE '[]'::jsonb END) AS h(etag);
        SELECT s.etag, s.total,
               CASE WHEN s.etag = ANY (v_held) THEN NULL ELSE s.snapshot END
          INTO v_etag, v_total, v_rows
          FROM public.leaderboard_cache_state s
         WHERE s.board = 'servers';
        v_boards := v_boards || jsonb_build_object('board', 'servers', 'etag', v_etag, 'total', v_total, 'unchanged', v_rows IS NULL);
        v_servers_rows := COALESCE(v_rows, '[]'::jsonb);

        -- The id is left empty: the line is the asking server's and no other's.
        SELECT COALESCE(jsonb_agg(public.ia_board_row_json(
                          t.rank, t.name, t.kills, t.deaths, t.hvt, t.guard, t.obj, t.score,
                          t.transport, t.insertions, t.players)
                          || jsonb_build_object('board', 'servers', 'id', '')), '[]'::jsonb)
          INTO v_lines
          FROM (SELECT c.*, row_number() OVER (ORDER BY c.score DESC, c.id) AS rank
                  FROM public.leaderboard_cache c
                 WHERE c.board = 'servers') t
         WHERE t.id = p_server_guid::text;
        v_own := v_own || v_lines;
      END IF;

      v_boards_status := 'ok';
    EXCEPTION WHEN OTHERS THEN
      v_boards_status := 'failed';
      v_boards := '[]'::jsonb;
      v_server_rows := '[]'::jsonb;
      v_global_rows := '[]'::jsonb;
      v_servers_rows := '[]'::jsonb;
      v_own := '[]'::jsonb;
    END;
  END IF;

  RETURN jsonb_build_object(
    'statsStatus',      v_stats_status,
    'statsPlayers',     v_stats_players,
    'transportStatus',  v_transport_status,
    'transportPlayers', v_transport_players,
    'ratingsStatus',    v_ratings_status,
    'ratings',          v_ratings,
    'skins',            v_skins,
    'boardsStatus',     v_boards_status,
    'boards',           v_boards,
    'serverRows',       v_server_rows,
    'globalRows',       v_global_rows,
    'serversRows',      v_servers_rows,
    'own',              v_own,
    'snapshotRows',     v_top);
END;
$$;

REVOKE ALL ON FUNCTION public.ia_board_refresh_seconds() FROM PUBLIC, anon, authenticated;
REVOKE ALL ON FUNCTION public.ia_board_snapshot_rows()   FROM PUBLIC, anon, authenticated;
REVOKE ALL ON FUNCTION public.ia_board_refresh_lock(text) FROM PUBLIC, anon, authenticated;
REVOKE ALL ON FUNCTION public.ia_refresh_board(text, boolean) FROM PUBLIC, anon, authenticated;
REVOKE ALL ON FUNCTION public.ia_board_row_json(bigint, text, bigint, bigint, bigint, bigint, bigint, bigint, bigint, bigint, bigint)
  FROM PUBLIC, anon, authenticated;
REVOKE ALL ON FUNCTION public.api_get_leaderboard(uuid, text, text, boolean, integer, integer, text)
  FROM PUBLIC, anon, authenticated;
REVOKE ALL ON FUNCTION public.api_sync(uuid, text, text, jsonb, text, jsonb, text[], text[], jsonb)
  FROM PUBLIC, anon, authenticated;

GRANT EXECUTE ON FUNCTION public.api_get_leaderboard(uuid, text, text, boolean, integer, integer, text)
  TO ia_game_api, service_role;
GRANT EXECUTE ON FUNCTION public.api_sync(uuid, text, text, jsonb, text, jsonb, text[], text[], jsonb)
  TO ia_game_api, service_role;

-- So the first caller does not wait for the first rebuild.
SELECT public.ia_refresh_board('global', true);
SELECT public.ia_refresh_board('servers', true);

COMMIT;
