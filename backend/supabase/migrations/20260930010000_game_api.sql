-- Database side of the game API (backend/azure-functions).
--
-- The function app signs in as ia_game_api, which can do nothing except call
-- the RPCs granted below. It holds no table privileges and no Supabase API key,
-- so its credential can be rotated or dropped without touching anything else.
-- The role is created without a password; set one out of band:
--   ALTER ROLE ia_game_api PASSWORD '...';

BEGIN;

DO $$
BEGIN
  IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'ia_game_api') THEN
    CREATE ROLE ia_game_api LOGIN NOINHERIT;
  END IF;
END
$$;

GRANT USAGE ON SCHEMA public TO ia_game_api;

-- POST /submitStats. One call per batch instead of one per player.
--   p_events: the game's matchData array (IA_StatEvent.c).
-- Returns NULL for an unknown or inactive server, else {"players": int}.
CREATE OR REPLACE FUNCTION public.api_submit_stats(
  p_server_guid uuid,
  p_server_name text,
  p_events      jsonb
)
RETURNS jsonb
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = public
AS $$
DECLARE
  touched integer;
BEGIN
  IF p_events IS NULL OR jsonb_typeof(p_events) <> 'array' THEN
    RAISE EXCEPTION 'matchData must be a JSON array';
  END IF;

  UPDATE public.servers
     SET last_seen = now(),
         name      = COALESCE(NULLIF(left(p_server_name, 255), ''), name)
   WHERE id = p_server_guid AND is_active;
  IF NOT FOUND THEN
    RETURN NULL;
  END IF;

  WITH parsed AS (
    SELECT ord,
           e->>'eventType' AS event_type,
           CASE e->>'eventType'
             WHEN 'PlayerDeath'         THEN e->>'victimPlayerId'
             WHEN 'CaptureContribution' THEN e->>'playerId'
             ELSE e->>'killerPlayerId'
           END AS player_id,
           CASE e->>'eventType'
             WHEN 'PlayerDeath'         THEN e->>'victimPlayerName'
             WHEN 'CaptureContribution' THEN e->>'playerName'
             ELSE e->>'killerPlayerName'
           END AS player_name,
           CASE WHEN e->>'score' ~ '^-?[0-9]{1,9}$' THEN (e->>'score')::integer ELSE 0 END AS score
      FROM jsonb_array_elements(p_events) WITH ORDINALITY AS t(e, ord)
     WHERE jsonb_typeof(e) = 'object'
  ), totals AS (
    SELECT player_id,
           (array_agg(player_name ORDER BY ord DESC))[1]                AS player_name,
           count(*) FILTER (WHERE event_type = 'PlayerKill')::integer   AS kills,
           count(*) FILTER (WHERE event_type = 'PlayerDeath')::integer  AS deaths,
           count(*) FILTER (WHERE event_type = 'HVTKill')::integer      AS hvt_kills,
           count(*) FILTER (WHERE event_type = 'HVTGuardKill')::integer AS hvt_guard_kills,
           COALESCE(sum(score) FILTER (WHERE event_type = 'CaptureContribution'), 0)::integer AS contribution
      FROM parsed
     WHERE length(player_id) BETWEEN 1 AND 64
       AND event_type IN ('PlayerKill', 'PlayerDeath', 'HVTKill', 'HVTGuardKill', 'CaptureContribution')
     GROUP BY player_id
  )
  INSERT INTO public.player_stats AS ps
         (player_bohemia_id, server_id, player_name_last_seen,
          kills, deaths, hvt_kills, hvt_guard_kills, contribution_score, last_seen)
  SELECT player_id, p_server_guid, player_name,
         kills, deaths, hvt_kills, hvt_guard_kills, contribution, now()
    FROM totals
  ON CONFLICT (player_bohemia_id, server_id) DO UPDATE
     SET kills                 = ps.kills + EXCLUDED.kills,
         deaths                = ps.deaths + EXCLUDED.deaths,
         hvt_kills             = ps.hvt_kills + EXCLUDED.hvt_kills,
         hvt_guard_kills       = ps.hvt_guard_kills + EXCLUDED.hvt_guard_kills,
         contribution_score    = ps.contribution_score + EXCLUDED.contribution_score,
         player_name_last_seen = EXCLUDED.player_name_last_seen,
         last_seen             = now();
  GET DIAGNOSTICS touched = ROW_COUNT;

  RETURN jsonb_build_object('players', touched);
END;
$$;

-- GET /getAllLeaderboards. Returns NULL for an unknown or inactive server.
CREATE OR REPLACE FUNCTION public.api_get_all_leaderboards(p_server_guid uuid)
RETURNS json
LANGUAGE plpgsql
STABLE
SECURITY DEFINER
SET search_path = public
AS $$
BEGIN
  PERFORM 1 FROM public.servers WHERE id = p_server_guid AND is_active;
  IF NOT FOUND THEN
    RETURN NULL;
  END IF;

  RETURN json_build_object(
    'globalPlayerLeaderboard',
      (SELECT COALESCE(json_agg(t), '[]'::json) FROM public.get_global_leaderboard() t),
    'serverPlayerLeaderboard',
      (SELECT COALESCE(json_agg(t), '[]'::json) FROM public.get_server_leaderboard(p_server_guid) t),
    'globalServerLeaderboard',
      (SELECT COALESCE(json_agg(t), '[]'::json) FROM public.get_global_server_leaderboard() t)
  );
END;
$$;

REVOKE ALL ON FUNCTION public.api_submit_stats(uuid, text, jsonb) FROM PUBLIC, anon, authenticated;
REVOKE ALL ON FUNCTION public.api_get_all_leaderboards(uuid)      FROM PUBLIC, anon, authenticated;

GRANT EXECUTE ON FUNCTION public.api_submit_stats(uuid, text, jsonb)       TO ia_game_api, service_role;
GRANT EXECUTE ON FUNCTION public.api_get_all_leaderboards(uuid)            TO ia_game_api, service_role;
GRANT EXECUTE ON FUNCTION public.register_server(varchar, varchar)         TO ia_game_api;
GRANT EXECUTE ON FUNCTION public.submit_transport_batch(uuid, text, jsonb) TO ia_game_api;
GRANT EXECUTE ON FUNCTION public.get_transport_ratings(text[])             TO ia_game_api;

COMMIT;
