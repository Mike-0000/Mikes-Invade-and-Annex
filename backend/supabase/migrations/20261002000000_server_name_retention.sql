-- A server keeps the last real name it reported.
--
-- POST /submitStats (backend/azure-functions) calls api_submit_stats, which
-- stores the name sent with each batch. It stored anything that was not empty,
-- so a batch carrying the game's placeholder ("... PLEASE RENAME IN
-- server_name.txt ...") replaced a real name. The game now sends the server's
-- live name, and a build that does not know one sends the placeholder or
-- nothing; neither may take a real name away.
--
-- Only the name rule changes. The server GUID alone still selects the row that
-- is written; a name never does. The statistics part is as in
-- 20260930010000_game_api.sql, and CREATE OR REPLACE keeps that file's grants.
--
-- Not covered: the original invadestats app, which older builds call, updates
-- servers.name itself and does not go through this function.

BEGIN;

-- POST /submitStats. One call per batch instead of one per player.
--   p_server_name: the name to remember. Kept as it was when this is empty or
--     blank, or when it is a placeholder and the stored name is a real one.
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
  -- Every default the game writes into server_name.txt carries this, as does a
  -- name an owner only half replaced. Compared in lower case.
  placeholder_mark constant text := 'please rename in server_name.txt';
  v_name  text := left(btrim(COALESCE(p_server_name, ''), E' \t\r\n'), 255);
  touched integer;
BEGIN
  IF p_events IS NULL OR jsonb_typeof(p_events) <> 'array' THEN
    RAISE EXCEPTION 'matchData must be a JSON array';
  END IF;

  UPDATE public.servers
     SET last_seen = now(),
         name      = CASE
                       WHEN v_name = '' THEN name
                       WHEN strpos(lower(v_name), placeholder_mark) > 0
                            AND strpos(lower(name), placeholder_mark) = 0 THEN name
                       ELSE v_name
                     END
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

COMMIT;
