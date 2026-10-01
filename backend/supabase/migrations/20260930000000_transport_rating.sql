-- Global transport-pilot rating.
--
-- The rating belongs to the player, not to a server: one row per
-- player_bohemia_id, added to by every registered server. Skin thresholds live
-- here too, so eligibility is one central setting rather than a per-server one.
--
-- The invadestats Azure Function calls the two RPCs with the service role:
--   POST /submitTransport     -> submit_transport_batch
--   POST /getTransportRatings -> get_transport_ratings

BEGIN;

CREATE TABLE IF NOT EXISTS public.player_transport_ratings (
  player_bohemia_id     text PRIMARY KEY,
  rating                bigint NOT NULL DEFAULT 0 CHECK (rating >= 0),
  insertions            bigint NOT NULL DEFAULT 0 CHECK (insertions >= 0),
  player_name_last_seen text,
  first_seen            timestamptz NOT NULL DEFAULT now(),
  last_seen             timestamptz NOT NULL DEFAULT now()
);

-- One row per accepted batch. The primary key makes a resent batch a no-op, and
-- the stored entries are the audit trail for reversing a misbehaving server.
-- server_id is nullable so merging or deleting a server keeps the history.
CREATE TABLE IF NOT EXISTS public.transport_batches (
  batch_id    text NOT NULL,
  server_guid uuid NOT NULL,
  server_id   uuid REFERENCES public.servers(id) ON DELETE SET NULL,
  entries     jsonb NOT NULL,
  received_at timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY (server_guid, batch_id)
);

CREATE INDEX IF NOT EXISTS transport_batches_received_at_idx
  ON public.transport_batches (received_at);

-- Central unlock thresholds. skin_key matches IA_HeliSkinCatalog; the game keeps
-- its built-in default for any key missing here.
CREATE TABLE IF NOT EXISTS public.transport_skin_thresholds (
  skin_key        text PRIMARY KEY,
  required_rating integer NOT NULL CHECK (required_rating > 0),
  updated_at      timestamptz NOT NULL DEFAULT now()
);

INSERT INTO public.transport_skin_thresholds (skin_key, required_rating)
VALUES ('huey_tan', 2500)
ON CONFLICT (skin_key) DO NOTHING;

ALTER TABLE public.player_transport_ratings  ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.transport_batches         ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.transport_skin_thresholds ENABLE ROW LEVEL SECURITY;

REVOKE ALL ON public.player_transport_ratings  FROM anon, authenticated;
REVOKE ALL ON public.transport_batches         FROM anon, authenticated;
REVOKE ALL ON public.transport_skin_thresholds FROM anon, authenticated;

-- Add one server's batch to the global totals.
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

-- Global ratings for the requested players plus the central skin thresholds.
-- Players with no row are simply absent; the game treats them as rating 0.
CREATE OR REPLACE FUNCTION public.get_transport_ratings(p_player_ids text[])
RETURNS jsonb
LANGUAGE sql
STABLE
SECURITY DEFINER
SET search_path = public
AS $$
  SELECT jsonb_build_object(
    'ratings', COALESCE((
      SELECT jsonb_agg(jsonb_build_object(
               'playerId',   r.player_bohemia_id,
               'rating',     r.rating,
               'insertions', r.insertions))
        FROM public.player_transport_ratings r
       WHERE r.player_bohemia_id = ANY (p_player_ids)
    ), '[]'::jsonb),
    'skins', COALESCE((
      SELECT jsonb_agg(jsonb_build_object(
               'key',      t.skin_key,
               'required', t.required_rating))
        FROM public.transport_skin_thresholds t
    ), '[]'::jsonb)
  );
$$;

REVOKE ALL ON FUNCTION public.submit_transport_batch(uuid, text, jsonb) FROM PUBLIC, anon, authenticated;
REVOKE ALL ON FUNCTION public.get_transport_ratings(text[])             FROM PUBLIC, anon, authenticated;
GRANT EXECUTE ON FUNCTION public.submit_transport_batch(uuid, text, jsonb) TO service_role;
GRANT EXECUTE ON FUNCTION public.get_transport_ratings(text[])             TO service_role;

COMMIT;
