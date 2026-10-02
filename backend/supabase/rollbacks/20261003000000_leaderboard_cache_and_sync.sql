-- Undoes migrations/20261003000000_leaderboard_cache_and_sync.sql.
--
-- api_get_leaderboard goes back to the function of 20261001030000, which adds
-- up the boards on every call; its body below is that file's, unchanged, and
-- CREATE OR REPLACE keeps its grants. api_sync, the cache and the stats batch
-- ids are removed. No statistics are touched: the cache only ever held copies.
--
-- Safe at any time. A function app that still has POST /sync answers it with
-- 501 once api_sync is gone, and the game goes back to the older routes.

BEGIN;

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

DROP FUNCTION IF EXISTS public.api_sync(uuid, text, text, jsonb, text, jsonb, text[], text[], jsonb);
DROP FUNCTION IF EXISTS public.ia_refresh_board(text, boolean);
DROP FUNCTION IF EXISTS public.ia_board_refresh_lock(text);
DROP FUNCTION IF EXISTS public.ia_board_row_json(bigint, text, bigint, bigint, bigint, bigint, bigint, bigint, bigint, bigint, bigint);
DROP FUNCTION IF EXISTS public.ia_board_snapshot_rows();
DROP FUNCTION IF EXISTS public.ia_board_refresh_seconds();

DROP TABLE IF EXISTS public.leaderboard_cache;
DROP TABLE IF EXISTS public.leaderboard_cache_state;
DROP TABLE IF EXISTS public.stats_batches;

COMMIT;
