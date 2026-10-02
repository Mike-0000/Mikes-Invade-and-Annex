-- Paged, sortable leaderboards with transport pilots on them.
--
-- GET /leaderboard (backend/azure-functions) calls api_get_leaderboard. It
-- answers one page of one board in one sort order, plus the asking player's own
-- row and rank, so the game never has to carry a whole board in one string.
--
-- The transport rating stays global (player_transport_ratings). This adds a
-- per-server share of it, so a server's board can rank its own pilots.
--
-- Everything here is additive. The functions older builds call
-- (get_global_leaderboard, get_server_leaderboard, get_global_server_leaderboard,
-- api_get_all_leaderboards) are left as they are.

BEGIN;

-- What a player has flown on one server. The global rating is the sum of these
-- from the day this table exists; earlier batches are backfilled below.
CREATE TABLE IF NOT EXISTS public.player_transport_server (
  player_bohemia_id     text NOT NULL,
  server_id             uuid NOT NULL REFERENCES public.servers(id) ON DELETE CASCADE,
  rating                bigint NOT NULL DEFAULT 0 CHECK (rating >= 0),
  insertions            bigint NOT NULL DEFAULT 0 CHECK (insertions >= 0),
  player_name_last_seen text,
  first_seen            timestamptz NOT NULL DEFAULT now(),
  last_seen             timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY (player_bohemia_id, server_id)
);

CREATE INDEX IF NOT EXISTS player_transport_server_server_id_idx
  ON public.player_transport_server (server_id);

-- The server board reads one server's rows; the primary key leads with the player.
CREATE INDEX IF NOT EXISTS player_stats_server_id_idx
  ON public.player_stats (server_id);

ALTER TABLE public.player_transport_server ENABLE ROW LEVEL SECURITY;
REVOKE ALL ON public.player_transport_server FROM anon, authenticated;

-- Batches accepted before this table existed. Runs once: only into an empty table.
INSERT INTO public.player_transport_server
       (player_bohemia_id, server_id, rating, insertions, player_name_last_seen)
SELECT e->>'playerId',
       b.server_id,
       sum(LEAST((e->>'points')::integer, (e->>'insertions')::integer * 30)),
       sum((e->>'insertions')::integer),
       max(NULLIF(left(e->>'playerName', 64), ''))
  FROM public.transport_batches b
 CROSS JOIN LATERAL jsonb_array_elements(b.entries) AS e
 WHERE b.server_id IS NOT NULL
   AND NOT EXISTS (SELECT 1 FROM public.player_transport_server)
   AND length(e->>'playerId') BETWEEN 1 AND 64
   AND (e->>'points')::integer > 0
   AND (e->>'insertions')::integer > 0
 GROUP BY e->>'playerId', b.server_id;

-- Add one server's batch to the global totals and to that server's own totals.
--   p_entries: [{"playerId": text, "playerName": text, "points": int, "insertions": int}]
-- Returns {"applied": bool, "players": int}; applied=false means the batch was
-- already recorded. Raises for an unknown or inactive server.
CREATE OR REPLACE FUNCTION public.submit_transport_batch(
  p_server_guid uuid,
  p_batch_id    text,
  p_entries     jsonb
)
RETURNS jsonb
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = public
AS $$
DECLARE
  -- IA_TransportScoring: BASE_POINTS (10) * HOT_WEIGHT (3.0).
  max_points_per_insertion constant integer := 30;
  inserted integer;
  touched  integer;
BEGIN
  IF p_batch_id IS NULL OR length(p_batch_id) = 0 OR length(p_batch_id) > 64 THEN
    RAISE EXCEPTION 'invalid batch id';
  END IF;
  IF p_entries IS NULL OR jsonb_typeof(p_entries) <> 'array' THEN
    RAISE EXCEPTION 'entries must be a JSON array';
  END IF;

  PERFORM 1 FROM public.servers s
   WHERE s.id = p_server_guid AND COALESCE(s.is_active, true);
  IF NOT FOUND THEN
    RAISE EXCEPTION 'unknown or inactive server %', p_server_guid;
  END IF;

  INSERT INTO public.transport_batches (batch_id, server_guid, server_id, entries)
  VALUES (p_batch_id, p_server_guid, p_server_guid, p_entries)
  ON CONFLICT (server_guid, batch_id) DO NOTHING;
  GET DIAGNOSTICS inserted = ROW_COUNT;
  IF inserted = 0 THEN
    RETURN jsonb_build_object('applied', false, 'players', 0);
  END IF;

  WITH parsed AS (
    SELECT e->>'playerId'                               AS player_id,
           NULLIF(left(e->>'playerName', 64), '')        AS player_name,
           (e->>'points')::integer                       AS points,
           (e->>'insertions')::integer                   AS insertions
      FROM jsonb_array_elements(p_entries) AS e
  ), valid AS (
    -- A server cannot credit more than the scoring rules allow per insertion.
    SELECT player_id,
           max(player_name)                                              AS player_name,
           sum(LEAST(points, insertions * max_points_per_insertion))     AS points,
           sum(insertions)                                               AS insertions
      FROM parsed
     WHERE player_id IS NOT NULL AND length(player_id) BETWEEN 1 AND 64
       AND points > 0 AND insertions > 0
     GROUP BY player_id
  ), per_server AS (
    INSERT INTO public.player_transport_server AS ts
           (player_bohemia_id, server_id, rating, insertions, player_name_last_seen)
    SELECT player_id, p_server_guid, points, insertions, player_name FROM valid
    ON CONFLICT (player_bohemia_id, server_id) DO UPDATE
       SET rating                = ts.rating + EXCLUDED.rating,
           insertions            = ts.insertions + EXCLUDED.insertions,
           player_name_last_seen = COALESCE(EXCLUDED.player_name_last_seen, ts.player_name_last_seen),
           last_seen             = now()
  )
  INSERT INTO public.player_transport_ratings AS r
         (player_bohemia_id, rating, insertions, player_name_last_seen)
  SELECT player_id, points, insertions, player_name FROM valid
  ON CONFLICT (player_bohemia_id) DO UPDATE
     SET rating                = r.rating + EXCLUDED.rating,
         insertions            = r.insertions + EXCLUDED.insertions,
         player_name_last_seen = COALESCE(EXCLUDED.player_name_last_seen, r.player_name_last_seen),
         last_seen             = now();
  GET DIAGNOSTICS touched = ROW_COUNT;

  UPDATE public.servers SET last_seen = now() WHERE id = p_server_guid;

  RETURN jsonb_build_object('applied', true, 'players', touched);
END;
$$;

-- Every row of one board, unsorted.
--   global  : one row per player, all servers added up, global transport rating
--   server  : one row per player on p_server, that server's share of the rating
--   servers : one row per server, its players added up
-- obj and score use the formulas of the boards older builds read. Transport is
-- its own column and is not part of score.
--   players : servers the player has stats on (global), 1 (server), players (servers)
CREATE OR REPLACE FUNCTION public.ia_leaderboard_rows(p_board text, p_server uuid)
RETURNS TABLE (
  id text, name text,
  kills bigint, deaths bigint, hvt bigint, guard bigint, obj bigint, score bigint,
  transport bigint, insertions bigint, players bigint
)
LANGUAGE plpgsql
STABLE
SET search_path = public
AS $$
#variable_conflict use_column
DECLARE
  mike constant text := 'a7745174-ef89-4093-91c5-9966167f747d';
BEGIN
  IF p_board = 'global' THEN
    RETURN QUERY
    WITH st AS (
      SELECT ps.player_bohemia_id AS pid,
             (array_agg(ps.player_name_last_seen ORDER BY ps.last_seen DESC NULLS LAST))[1] AS pname,
             sum(COALESCE(ps.kills, 0))::bigint            AS c_kills,
             sum(COALESCE(ps.deaths, 0))::bigint           AS c_deaths,
             sum(COALESCE(ps.hvt_kills, 0))::bigint        AS c_hvt,
             sum(COALESCE(ps.hvt_guard_kills, 0))::bigint  AS c_guard,
             sum(ps.contribution_score / 60)::bigint       AS c_obj,
             sum(COALESCE(ps.hvt_kills, 0) * 25 + COALESCE(ps.hvt_guard_kills, 0) * 2
                 + COALESCE(ps.kills, 0) - COALESCE(ps.deaths, 0)
                 + ps.contribution_score / 12)::bigint     AS c_score,
             count(*)::bigint                              AS c_servers
        FROM public.player_stats ps
       GROUP BY ps.player_bohemia_id
    )
    SELECT COALESCE(st.pid, tr.player_bohemia_id),
           CASE WHEN COALESCE(st.pid, tr.player_bohemia_id) = mike THEN 'Mike'
                ELSE COALESCE(NULLIF(st.pname, ''), NULLIF(tr.player_name_last_seen, ''), 'Unknown') END,
           COALESCE(st.c_kills, 0), COALESCE(st.c_deaths, 0), COALESCE(st.c_hvt, 0),
           COALESCE(st.c_guard, 0), COALESCE(st.c_obj, 0), COALESCE(st.c_score, 0),
           COALESCE(tr.rating, 0), COALESCE(tr.insertions, 0), COALESCE(st.c_servers, 0)
      FROM st
      FULL OUTER JOIN public.player_transport_ratings tr ON tr.player_bohemia_id = st.pid;

  ELSIF p_board = 'server' THEN
    RETURN QUERY
    SELECT COALESCE(ps.player_bohemia_id, ts.player_bohemia_id),
           CASE WHEN COALESCE(ps.player_bohemia_id, ts.player_bohemia_id) = mike THEN 'Mike'
                ELSE COALESCE(NULLIF(ps.player_name_last_seen, ''), NULLIF(ts.player_name_last_seen, ''), 'Unknown') END,
           COALESCE(ps.kills, 0)::bigint, COALESCE(ps.deaths, 0)::bigint,
           COALESCE(ps.hvt_kills, 0)::bigint, COALESCE(ps.hvt_guard_kills, 0)::bigint,
           COALESCE(ps.contribution_score / 60, 0)::bigint,
           (COALESCE(ps.hvt_kills, 0) * 25 + COALESCE(ps.hvt_guard_kills, 0) * 2
            + COALESCE(ps.kills, 0) - COALESCE(ps.deaths, 0)
            + COALESCE(ps.contribution_score / 12, 0))::bigint,
           COALESCE(ts.rating, 0), COALESCE(ts.insertions, 0), 1::bigint
      FROM (SELECT * FROM public.player_stats x WHERE x.server_id = p_server) ps
      FULL OUTER JOIN (SELECT * FROM public.player_transport_server y WHERE y.server_id = p_server) ts
        ON ts.player_bohemia_id = ps.player_bohemia_id;

  ELSE
    RETURN QUERY
    WITH st AS (
      SELECT ps.server_id AS sid,
             sum(COALESCE(ps.kills, 0))::bigint            AS c_kills,
             sum(COALESCE(ps.deaths, 0))::bigint           AS c_deaths,
             sum(COALESCE(ps.hvt_kills, 0))::bigint        AS c_hvt,
             sum(COALESCE(ps.hvt_guard_kills, 0))::bigint  AS c_guard,
             sum(ps.contribution_score / 60)::bigint       AS c_obj,
             sum(COALESCE(ps.hvt_kills, 0) * 25 + COALESCE(ps.hvt_guard_kills, 0) * 2
                 + COALESCE(ps.kills, 0) - COALESCE(ps.deaths, 0)
                 + ps.contribution_score / 12)::bigint     AS c_score,
             count(*)::bigint                              AS c_players
        FROM public.player_stats ps
       GROUP BY ps.server_id
    ), tr AS (
      SELECT ts.server_id AS sid,
             sum(ts.rating)::bigint     AS c_rating,
             sum(ts.insertions)::bigint AS c_insertions
        FROM public.player_transport_server ts
       GROUP BY ts.server_id
    )
    SELECT s.id::text, s.name::text,
           st.c_kills, st.c_deaths, st.c_hvt, st.c_guard, st.c_obj, st.c_score,
           COALESCE(tr.c_rating, 0), COALESCE(tr.c_insertions, 0), st.c_players
      FROM st
      JOIN public.servers s ON s.id = st.sid
      LEFT JOIN tr ON tr.sid = st.sid
     WHERE s.is_active
       AND s.name NOT IN (
         'Default Name - PLEASE RENAME IN server_name.txt, in I&A Server Profile Folder',
         'Server Name -PLEASE RENAME IN server_name.txt, in Server Files',
         'My IA Server');
  END IF;
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
STABLE
SECURITY DEFINER
SET search_path = public
AS $$
DECLARE
  -- The game reads these as 32-bit integers.
  int_cap  constant bigint := 2000000000;
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
      FROM public.ia_leaderboard_rows(v_board, p_server_guid) b
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
           jsonb_build_object(
             'r', z.rank,
             'n', left(z.name, 48),
             'k', LEAST(z.kills, int_cap),
             'd', LEAST(z.deaths, int_cap),
             'h', LEAST(z.hvt, int_cap),
             'g', LEAST(z.guard, int_cap),
             'o', LEAST(z.obj, int_cap),
             's', GREATEST(LEAST(z.score, int_cap), -int_cap),
             't', LEAST(z.transport, int_cap),
             'i', LEAST(z.insertions, int_cap),
             'p', LEAST(z.players, int_cap)) AS row_json
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
END;
$$;

REVOKE ALL ON FUNCTION public.ia_leaderboard_rows(text, uuid) FROM PUBLIC, anon, authenticated;
REVOKE ALL ON FUNCTION public.api_get_leaderboard(uuid, text, text, boolean, integer, integer, text)
  FROM PUBLIC, anon, authenticated;
REVOKE ALL ON FUNCTION public.submit_transport_batch(uuid, text, jsonb) FROM PUBLIC, anon, authenticated;

GRANT EXECUTE ON FUNCTION public.api_get_leaderboard(uuid, text, text, boolean, integer, integer, text)
  TO ia_game_api, service_role;
GRANT EXECUTE ON FUNCTION public.submit_transport_batch(uuid, text, jsonb) TO ia_game_api, service_role;

COMMIT;
