---
name: merge-duplicate-server
description: Merges a duplicate I&A server-browser identity in Supabase so player_stats land on one live GUID. Use when a server owner reports two listings, a lost api_config.json, or asks to merge NAK / duplicate server records.
disable-model-invocation: true
---

# Merge duplicate I&A server

One-shot **data** repair. Not a schema migration. Do not `apply_migration`. Do not replay this on a fresh database. Do not edit game scripts.

Read the Supabase skill first. Use MCP `execute_sql` on project **Invade and Annex** (`qfihregafvphxkzdfbwd`).

## Why duplicates exist

`register_server(name, email)` always inserts a new UUID. The host stores that id in `$profile:MikesInvadeAndAnnex/api_config.json`. Wipe the profile and the next boot registers a second row with the same `servers.name`.

`get_global_server_leaderboard()` groups by `s.id, s.name`, so both rows appear. `is_active` is unused. Default placeholder names are already excluded from that board — never merge those.

## Do not merge

- Default / placeholder names (`Default Name - PLEASE RENAME…`, `Server Name -PLEASE RENAME…`, `My IA Server`)
- Two rows that both `last_seen` within the last 2 days (two live hosts or an unconfirmed keeper)
- More than two candidates unless the user names the exact donor and keeper UUIDs
- A pair whose overlap or totals do not match the preflight you just ran

## 1. Identify

```sql
SELECT id, name, owner_email, created_at, last_seen, is_active
FROM public.servers
WHERE name ILIKE '%NAK%'   -- owner fragment
ORDER BY last_seen DESC NULLS LAST;
```

If the owner gave no fragment, find exact-name collisions among non-placeholder names:

```sql
SELECT name, COUNT(*) AS copies,
  MIN(created_at) AS first_created, MAX(last_seen) AS last_seen
FROM public.servers
WHERE name NOT IN (
  'Default Name - PLEASE RENAME IN server_name.txt, in I&A Server Profile Folder',
  'Server Name -PLEASE RENAME IN server_name.txt, in Server Files',
  'My IA Server'
)
GROUP BY name
HAVING COUNT(*) > 1
ORDER BY copies DESC, last_seen DESC;
```

Typical split: older row last seen the day before the newer `created_at`.

## 2. Measure

Substitute the donor UUID (`old_id`, stale) and keeper UUID (`live_id`, still receiving writes). Do not send `:placeholders` to `execute_sql`.

```sql
SELECT s.id, s.created_at, s.last_seen,
  COUNT(ps.*) AS player_rows,
  COALESCE(SUM(ps.kills), 0) AS kills,
  COALESCE(SUM(ps.deaths), 0) AS deaths,
  COALESCE(SUM(ps.hvt_kills), 0) AS hvt_kills,
  COALESCE(SUM(ps.hvt_guard_kills), 0) AS hvt_guard_kills,
  COALESCE(SUM(ps.contribution_score), 0) AS contribution
FROM public.servers s
LEFT JOIN public.player_stats ps ON ps.server_id = s.id
WHERE s.id IN ('old-uuid', 'live-uuid')
GROUP BY s.id, s.created_at, s.last_seen;
```

```sql
SELECT COUNT(*) AS overlap_players
FROM public.player_stats a
JOIN public.player_stats b ON a.player_bohemia_id = b.player_bohemia_id
WHERE a.server_id = 'old-uuid' AND b.server_id = 'live-uuid';
```

Unique after merge = old + live − overlap. Overlap counters **sum**. They are additive session increments on two identities, not two copies of the same totals. `first_seen = LEAST`, `last_seen = GREATEST`, name from the row with the later `last_seen`.

Global player board already `SUM`s by `player_bohemia_id`, so a correct merge does not change global player totals. The **server** board collapses to one row and that server’s own player board grows.

## 3. Confirm, then wait

Show the owner: both UUIDs, player/overlap counts, predicted merged totals, current vs post-merge server rank, and that the live host must keep `api_config.json` with the **keeper** GUID.

**Keeper = currently writing GUID.** Merging into the stale GUID requires them to paste the old id into `api_config.json`; a missed step splits them again.

Stop until the user approves that specific pair. If they already named both UUIDs and said to merge, continue.

## 4. Merge (one `execute_sql` transaction)

Paste values from **this** step-2 run. Rename `bak_owner_yyyymmdd_*` to a new ident. If that table already exists, stop — do not drop a previous backup without asking.

Order is required: sum overlaps → delete donor overlaps → reassign remainder → delete donor server. Reassign first hits `player_stats_pkey (player_bohemia_id, server_id)`.

```sql
BEGIN;

CREATE TABLE public.bak_owner_yyyymmdd_servers AS
SELECT * FROM public.servers
WHERE id IN ('old-uuid', 'live-uuid');

CREATE TABLE public.bak_owner_yyyymmdd_player_stats AS
SELECT * FROM public.player_stats
WHERE server_id IN ('old-uuid', 'live-uuid');

ALTER TABLE public.bak_owner_yyyymmdd_servers ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.bak_owner_yyyymmdd_player_stats ENABLE ROW LEVEL SECURITY;
REVOKE ALL ON TABLE public.bak_owner_yyyymmdd_servers FROM anon, authenticated;
REVOKE ALL ON TABLE public.bak_owner_yyyymmdd_player_stats FROM anon, authenticated;

DO $$
DECLARE
  old_id uuid := 'old-uuid';
  live_id uuid := 'live-uuid';
  old_players int;
  live_players int;
  overlap_players int;
  backup_servers int;
  backup_players int;
  old_last_seen timestamptz;
  live_last_seen timestamptz;
BEGIN
  SELECT COUNT(*) INTO backup_servers FROM public.bak_owner_yyyymmdd_servers;
  SELECT COUNT(*) INTO backup_players FROM public.bak_owner_yyyymmdd_player_stats;
  SELECT COUNT(*) INTO old_players FROM public.player_stats WHERE server_id = old_id;
  SELECT COUNT(*) INTO live_players FROM public.player_stats WHERE server_id = live_id;
  SELECT COUNT(*) INTO overlap_players
  FROM public.player_stats a
  JOIN public.player_stats b ON a.player_bohemia_id = b.player_bohemia_id
  WHERE a.server_id = old_id AND b.server_id = live_id;
  SELECT last_seen INTO old_last_seen FROM public.servers WHERE id = old_id;
  SELECT last_seen INTO live_last_seen FROM public.servers WHERE id = live_id;

  IF backup_servers <> 2 THEN RAISE EXCEPTION 'backup servers %', backup_servers; END IF;
  IF backup_players <> -1 THEN RAISE EXCEPTION 'backup players %', backup_players; END IF; -- old+live
  IF old_players <> -1 THEN RAISE EXCEPTION 'old players %', old_players; END IF;
  IF live_players <> -1 THEN RAISE EXCEPTION 'live players %', live_players; END IF;
  IF overlap_players <> -1 THEN RAISE EXCEPTION 'overlap %', overlap_players; END IF;
  IF old_last_seen >= TIMESTAMPTZ '2026-01-01 00:00:00+00' THEN -- newer created_at
    RAISE EXCEPTION 'donor last_seen % too recent', old_last_seen;
  END IF;
  IF live_last_seen < now() - interval '2 days' THEN
    RAISE EXCEPTION 'keeper last_seen % stale', live_last_seen;
  END IF;
END $$;

UPDATE public.player_stats AS live
SET
  kills = COALESCE(live.kills, 0) + COALESCE(old.kills, 0),
  deaths = COALESCE(live.deaths, 0) + COALESCE(old.deaths, 0),
  hvt_kills = COALESCE(live.hvt_kills, 0) + COALESCE(old.hvt_kills, 0),
  hvt_guard_kills = COALESCE(live.hvt_guard_kills, 0) + COALESCE(old.hvt_guard_kills, 0),
  contribution_score = COALESCE(live.contribution_score, 0) + COALESCE(old.contribution_score, 0),
  first_seen = LEAST(live.first_seen, old.first_seen),
  last_seen = GREATEST(live.last_seen, old.last_seen),
  player_name_last_seen = CASE
    WHEN COALESCE(live.last_seen, '-infinity'::timestamptz)
         >= COALESCE(old.last_seen, '-infinity'::timestamptz)
      THEN live.player_name_last_seen
    ELSE old.player_name_last_seen
  END
FROM public.player_stats AS old
WHERE live.server_id = 'live-uuid'
  AND old.server_id = 'old-uuid'
  AND live.player_bohemia_id = old.player_bohemia_id;

DELETE FROM public.player_stats old
WHERE old.server_id = 'old-uuid'
  AND EXISTS (
    SELECT 1 FROM public.player_stats live
    WHERE live.server_id = 'live-uuid'
      AND live.player_bohemia_id = old.player_bohemia_id
  );

UPDATE public.player_stats SET server_id = 'live-uuid' WHERE server_id = 'old-uuid';
DELETE FROM public.servers WHERE id = 'old-uuid';

DO $$
DECLARE
  old_id uuid := 'old-uuid';
  live_id uuid := 'live-uuid';
  old_players int;
  live_players int;
  named int;
  kills bigint;
  deaths bigint;
  hvt bigint;
  guard bigint;
  contrib bigint;
BEGIN
  SELECT COUNT(*) INTO old_players FROM public.player_stats WHERE server_id = old_id;
  SELECT COUNT(*) INTO live_players FROM public.player_stats WHERE server_id = live_id;
  SELECT COUNT(*) INTO named FROM public.servers WHERE name = 'Exact Server Name';
  SELECT COALESCE(SUM(kills),0), COALESCE(SUM(deaths),0), COALESCE(SUM(hvt_kills),0),
         COALESCE(SUM(hvt_guard_kills),0), COALESCE(SUM(contribution_score),0)
    INTO kills, deaths, hvt, guard, contrib
  FROM public.player_stats WHERE server_id = live_id;

  IF old_players <> 0 THEN RAISE EXCEPTION 'old leftover %', old_players; END IF;
  IF live_players <> -1 THEN RAISE EXCEPTION 'merged players %', live_players; END IF; -- old+live-overlap
  IF named <> 1 THEN RAISE EXCEPTION 'name copies %', named; END IF;
  IF kills <> -1 THEN RAISE EXCEPTION 'kills %', kills; END IF;
  IF deaths <> -1 THEN RAISE EXCEPTION 'deaths %', deaths; END IF;
  IF hvt <> -1 THEN RAISE EXCEPTION 'hvt %', hvt; END IF;
  IF guard <> -1 THEN RAISE EXCEPTION 'guard %', guard; END IF;
  IF contrib <> -1 THEN RAISE EXCEPTION 'contrib %', contrib; END IF;
END $$;

COMMIT;
```

Every `-1` is a sentinel. Replace them with tonight's step-2 numbers or the transaction aborts. Replace the donor stale cutoff with the newer row's `created_at`. Any RAISE rolls back.

## 5. Verify

Confirm donor GUID has 0 server rows and 0 player rows, keeper has old+live−overlap players, and the server-board query (group by `s.id, s.name`, same placeholder filter as `get_global_server_leaderboard`) shows **one** row for that name.

Tell the owner: keep the current `api_config.json`. Deleting it registers a third GUID.

Leave backup tables. Drop only when the user says the live board looks right. RLS with no policies plus `REVOKE` is intentional; ignore the `rls_enabled_no_policy` INFO lint on those two tables.

## Schema (do not invent columns)

- `servers`: `id` uuid PK, `name`, `owner_email`, `created_at`, `last_seen`, `is_active`
- `player_stats`: PK `(player_bohemia_id, server_id)` → `servers.id`; counters `kills`, `deaths`, `hvt_kills`, `hvt_guard_kills`, `contribution_score`; `player_name_last_seen`, `first_seen`, `last_seen`
- Writes go Azure `invadestats` → `upsert_player_stat` (additive on conflict) and `register_server`
