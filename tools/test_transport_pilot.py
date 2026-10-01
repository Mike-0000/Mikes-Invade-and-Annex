"""Source integration guards for transport-pilot progression.

Scoring and eligibility maths are checked natively in Workbench
(IA_TransportPilotTest), paint channel assets by IA_HeliSkinAssetCheck. Skins
as other clients see them, insertions and the backend round trip still need a
mission playtest against a deployed backend.
"""
import re
import unittest

import author_heli_paint_channels as channels
from test_dynamic_base_flow import ROOT, method, source

MIGRATION = ROOT / "backend" / "supabase" / "migrations" / "20260930000000_transport_rating.sql"


def constant(text, name):
    return float(re.search(name + r"\s*=\s*([\d.]+)", text).group(1))


class TransportPilotTests(unittest.TestCase):
    def test_rating_is_global_and_never_stored_on_the_server(self):
        store = source("IA_TransportPilotStore.c")
        self.assertNotRegex(store, r"FileIO|FileHandle|\$profile")
        self.assertIn("SubmitTransport(m_sSentBatchId, m_sSentEntries)", method(store, "Flush"))
        api = source("IA_ApiHandler.c")
        self.assertIn('"/submitTransport"', method(api, "SubmitTransport"))
        self.assertIn('"/getTransportRatings"', method(api, "FetchTransportRatings"))

    def test_unknown_global_rating_never_unlocks(self):
        rating = method(source("IA_TransportPilotRecord.c"), "GetRating")
        self.assertLess(rating.index("return RATING_UNKNOWN"), rating.index("m_iPendingPoints"))
        pad = method(source("IA_HeliSkinPadService.c"), "TickPad")
        self.assertRegex(pad, r"if \(rating < 0\)\s*\{[\s\S]*?RequestRating[\s\S]*?return;")
        self.assertLess(pad.index("if (rating < 0)"), pad.index("skins.SetVehicleSkin("))

    def test_failed_batch_is_resent_unchanged(self):
        store = source("IA_TransportPilotStore.c")
        flush = method(store, "Flush")
        self.assertRegex(flush, r"if \(m_sSentEntries\.IsEmpty\(\)\)\s*BuildBatch\(\)")
        result = method(store, "OnSubmitResult")
        self.assertLess(result.index("if (!ok)"), result.index('m_sSentEntries = ""'))

    def test_passenger_is_credited_once(self):
        tracker = source("IA_TransportPilotTracker.c")
        credit = method(tracker, "CreditRide")
        self.assertIn("passengerGuid == ride.m_sPilotGuid", credit)
        self.assertIn("IsCreditableRide(travel, now, lastCredit)", credit)
        self.assertLess(credit.index("m_mLastCreditMs.Set(passengerGuid, now)"), credit.index("Award("))
        tick = method(tracker, "TickPlayer")
        self.assertRegex(tick, r"CreditRide\(playerId, pawn, ride, now\);\s*m_mRides\.Remove\(playerId\)")
        self.assertIn("!IsIntact(ride.m_Vehicle)", tick)

    def test_backend_schema_matches_the_game_contract(self):
        sql = MIGRATION.read_text(encoding="utf-8")
        self.assertRegex(sql, r"player_transport_ratings \(\s*player_bohemia_id\s+text PRIMARY KEY")
        table = sql[sql.index("public.player_transport_ratings ("):sql.index("public.transport_batches (")]
        self.assertNotIn("server", table.split(");")[0])
        self.assertIn("PRIMARY KEY (server_guid, batch_id)", sql)

        scoring = source("IA_TransportScoring.c")
        cap = round(constant(scoring, "BASE_POINTS") * constant(scoring, "HOT_WEIGHT"))
        self.assertIn("max_points_per_insertion constant integer := %d;" % cap, sql)

        store = source("IA_TransportPilotStore.c")
        for key in ("playerId", "playerName", "points", "insertions"):
            self.assertIn('\\"%s\\"' % key, store)
            self.assertIn("e->>'%s'" % key, sql)
        entry = source("IA_TransportRatingEntry.c") + source("IA_TransportSkinThreshold.c")
        for key in ("playerId", "rating", "insertions", "key", "required"):
            self.assertRegex(entry, r"\b%s;" % key)
            self.assertIn("'%s'," % key, sql)

    def assert_registered(self, name):
        """A file without a GUID is unregistered: it resolves to a null GUID and
        is left out of a published build. IA_HeliSkinAssetCheck registers it."""
        guid, path = re.fullmatch(r"(\{[0-9A-F]{16}\})(.+)", name).groups()
        self.assertIn('Name "%s"' % name, (ROOT / (path + ".meta")).read_text(encoding="utf-8"))
        return ROOT / path

    def test_catalogued_skins_have_a_central_threshold_and_registered_paints(self):
        catalog = source("IA_HeliSkinCatalog.c")
        sql = MIGRATION.read_text(encoding="utf-8")
        defs = re.findall(r'(\w+) = AddDef\(\w+, "(\w+)", "[^"]+", (\d+)\)', catalog)
        self.assertTrue(defs)
        surfaces = ("SURFACE_BODY", "SURFACE_INTERIOR_1", "SURFACE_INTERIOR_2")
        for variable, key, required in defs:
            self.assertIn("('%s', %s)" % (key, required), sql)
            paints = re.findall(r'AddPaint\(%s, IA_HeliPaintChannels\.(\w+), "([^"]+)"\)' % variable, catalog)
            self.assertTrue(paints, key + " colours at least one surface")
            for surface, paint in paints:
                stock_guid, stock_file = channels.SURFACES[surfaces.index(surface)]
                # A paint inherits the stock material of its surface, so what it leaves out reads as stock.
                first = self.assert_registered(paint).read_text(encoding="utf-8").splitlines()[0]
                self.assertIn('"{%s}%sData/%s.emat"' % (stock_guid, channels.MATERIAL_DIR, stock_file), first)

    def test_paint_channel_files_match_their_generator_and_the_script_table(self):
        files = channels.build()
        self.assertEqual(channels.stale(files), [])

        table = source("IA_HeliPaintChannels.c")
        self.assertEqual(int(constant(table, "CHANNEL_COUNT")), channels.CHANNEL_COUNT)
        self.assertEqual(int(constant(table, "SURFACE_COUNT")), len(channels.SURFACES))
        self.assertIn('GUID_PREFIX = "%s"' % channels.GUID_PREFIX, table)
        self.assertEqual(re.findall(r'AddAirframe\("\{(\w+)\}", "(\w+)"\)', table), [row[:2] for row in channels.AIRFRAMES])
        self.assertEqual(re.findall(r'AddSurface\("\{(\w+)\}", "(\w+)"\)', table), list(channels.SURFACES))

        # A mesh naming another channel's material would be recoloured with that channel's helicopter.
        for path, text in files.items():
            if path.endswith(".et"):
                channel = re.search(r"_Paint(\d)\.et$", path).group(1)
                assigned = re.findall(r'AssignedMaterial "\{\w+\}[^"]*_Paint(\d)\.emat"', text)
                self.assertTrue(assigned, path)
                self.assertEqual(set(assigned), {channel}, path)

    def test_a_skin_is_set_in_place_and_never_changes_or_replaces_the_helicopter(self):
        # Changing a vehicle hull's mesh from script frees the instance its animation is bound to.
        scripts = sorted((ROOT / "Scripts" / "Game").rglob("IA_HeliSkin*.c")) + sorted((ROOT / "Scripts" / "Game").rglob("IA_HeliPaint*.c"))
        self.assertTrue(scripts)
        for path in scripts:
            self.assertNotRegex(path.read_text(encoding="utf-8"), r"SetObject\(|SetVObjectFromPrefab\(|DeleteEntityAndChildren\(|SpawnEntityPrefab\(", path.name)
        self.assertFalse((ROOT / "Scripts" / "Game" / "IA_HeliSkinSwap.c").exists())

        paint = source("IA_HeliSkinPaint.c")
        self.assertIn("Material.GetOrLoadMaterial(to, 0)", method(paint, "CopyParams"))
        self.assertIn("IA_HeliPaintChannels.GetMaterial(surface, channel)", method(paint, "Apply"))

        # The server says which skin a channel shows; every machine that renders paints it.
        manager = source("IA_HeliSkinManagerComponent.c")
        self.assertRegex(manager, r'\[RplProp\(onRplName: "OnSkinsReplicated"\)\]\s*protected ref array<int> m_aChannelSkins')
        setter = method(manager, "SetVehicleSkin")
        self.assertLess(setter.index("if (!Replication.IsServer())"), setter.index("m_aChannelSkins[channel - 1] = skinId"))
        self.assertIn("Replication.BumpMe()", setter)
        self.assertIn("PaintAll();", method(manager, "OnSkinsReplicated"))
        self.assertIn("IA_HeliSkinPaint.Apply(i + 1, wanted)", method(manager, "PaintAll"))
        self.assertIn("IA_HeliSkinManagerComponent", (ROOT / "Prefabs" / "GameMode_IA.et").read_text(encoding="utf-8"))

        # A pad owns one channel and spawns its Huey as that channel's twin, in stock paint.
        respawner = source("IA_VehicleRespawner.c")
        self.assertIn("m_iPaintChannel = FreePaintChannel();", method(respawner, "OnPostInit"))
        spawn = method(respawner, "PerformSpawn")
        self.assertLess(spawn.index("IA_HeliPaintChannels.FindChannelPrefab(vehiclePrefabToSpawn, m_iPaintChannel)"), spawn.index("Resource.Load(vehiclePrefabToSpawn)"))
        self.assertIn("skins.SetVehicleSkin(newVehicle, IA_HeliSkinCatalog.SKIN_NONE)", spawn)
        self.assertNotIn("m_bSwapPending", respawner)

    def test_a_skin_can_be_previewed_solo_without_a_rating(self):
        preview = source("IA_HeliSkinPreview.c")
        cycle = method(preview, "CycleNearest")
        self.assertIn("skins.SetVehicleSkin(vehicle, skinId)", cycle)
        # A skin menu inside the helicopter will change it with the crew aboard, so the preview must not ask for it empty.
        self.assertNotRegex(preview, r"IsParkedAndEmpty|GetOccupant|IsOccupied|EngineOn")
        self.assertNotIn("IA_TransportPilotStore", preview)
        # The repaint is visible to everyone, so only an admin may ask for it.
        ask = method(source("IA_PlayerController.c"), "IA_PreviewHeliSkinIfAdmin")
        self.assertLess(ask.index("if (!IA_IsAdminCaller())"), ask.index("IA_HeliSkinPreview.CycleNearest("))
        menu = (ROOT / "Scripts" / "Game" / "UI" / "Menus" / "IA_AdminConfigMenu.c").read_text(encoding="utf-8")
        self.assertIn("pc.IA_AskPreviewHeliSkin()", method(menu, "OnSkinPreview"))

    def test_feature_is_wired_into_the_mission(self):
        self.assertIn("IA_TransportPilotTracker.EnsureStarted();", source("IA_MissionInitializer.c"))
        self.assertIn("IA_HeliSkinPadService.Tick(pm, players, now)", method(source("IA_TransportPilotTracker.c"), "Tick"))
        self.assertIn('messageType == "PilotProgress"', source("IA_ChimeraCharacter.c"))

    def test_a_landing_is_one_card_not_one_toast_per_passenger(self):
        tracker = source("IA_TransportPilotTracker.c")
        # Crediting a passenger only accumulates; nothing is sent per passenger.
        award = method(tracker, "Award")
        self.assertNotIn("SetUIOne", award)
        self.assertIn("dropoff.Add(points, edge)", award)
        self.assertEqual(tracker.count('SetUIOne("PilotProgress"'), 2)
        for name in ("FlushDropoffs", "FlushStatus"):
            self.assertIn('SetUIOne("PilotProgress", payload.Pack(), pilotId)', method(tracker, name))
        tick = method(tracker, "Tick")
        self.assertLess(tick.index("TickPlayer("), tick.index("FlushDropoffs(pm, now)"))
        self.assertLess(tick.index("FlushDropoffs(pm, now)"), tick.index("FlushStatus(pm, players, now)"))

        # The client keeps pilot progress out of the toast queue and merges into an open card.
        character = source("IA_ChimeraCharacter.c")
        branch = character[character.index('messageType == "PilotProgress"'):]
        branch = branch[:branch.index("}")]
        self.assertIn("ShowPilotProgress(taskTitle)", branch)
        self.assertNotIn("QueueNotification", branch)

        display = source("IA_NotificationDisplay.c")
        build = method(display, "BuildToastUI")
        self.assertIn("IA_PilotHud.Create(m_Runtime)", build)
        self.assertIn("overlay.AddChild(m_PilotHud)", build)
        show = method(display, "ShowPilotProgress")
        self.assertLess(show.index("m_PilotHud.Present(data)"), show.index("m_PendingPilotLine"))
        self.assertNotIn("QueueNotification", show)

        hud = (ROOT / "Scripts" / "Game" / "UI" / "IA_PilotHud.c").read_text(encoding="utf-8")
        present = method(hud, "Present")
        self.assertLess(present.index("MergeUpdate(data)"), present.index("BeginIntro()"))
        self.assertIn("m_Data.Merge(data)", method(hud, "MergeUpdate"))

    def test_unlock_is_announced_once_with_the_points_that_crossed_it(self):
        tracker = source("IA_TransportPilotTracker.c")
        self.assertIn("payload.SetProgress(rating, earned)", method(tracker, "FillProgress"))
        fill = method(source("IA_PilotDropoffPayload.c"), "SetProgress")
        self.assertIn("FindNewlyUnlocked(rating - earned, rating)", fill)
        # Points banked while the total was unknown still count towards the crossing.
        self.assertIn("dropoff.m_iPoints + TakeUnreported(guid)", method(tracker, "FlushDropoffs"))
        status = method(tracker, "FlushStatus")
        self.assertRegex(status, r"FillProgress\(payload, rating, earned\);\s*m_mUnreported\.Remove\(guid\);")
        hud = (ROOT / "Scripts" / "Game" / "UI" / "IA_PilotHud.c").read_text(encoding="utf-8")
        self.assertEqual(hud.count("SCR_UISoundEntity.SoundEvent("), 1)
        self.assertIn("SCR_UISoundEntity.SoundEvent(", method(hud, "TickUnlock"))

    def test_card_can_be_previewed_solo_without_awarding_points(self):
        preview = (ROOT / "Scripts" / "Game" / "UI" / "IA_PilotHudPreview.c").read_text(encoding="utf-8")
        # Client only: nothing is credited, stored or sent.
        self.assertNotRegex(preview, r"AddInsertion|IA_TransportPilotStore|IA_StatsManager|SetUIOne|Rpc\(")
        # Cards are built by the rules the server uses, so the preview cannot drift from a real flight.
        self.assertIn("IA_TransportScoring.InsertionPoints(edgeM)", method(preview, "AddDrop"))
        self.assertIn("payload.SetProgress(", method(preview, "AddDrop"))
        self.assertIn("payload.SetProgress(", method(preview, "AddStatus"))
        self.assertIn("display.PlayPilotPreview(preview)", method(preview, "PlayLocal"))

        # The preview enters through the same call as a real server update.
        display = source("IA_NotificationDisplay.c")
        step = method(display, "StepPilotPreview")
        self.assertIn("ShowPilotProgress(m_PilotPreview.TakeNext())", step)
        self.assertLess(step.index("!m_PilotHud.IsIdle()"), step.index("ShowPilotProgress("))
        self.assertIn("StopPilotPreview();", method(display, "CloseMikesUI"))

        menu = (ROOT / "Scripts" / "Game" / "UI" / "Menus" / "IA_AdminConfigMenu.c").read_text(encoding="utf-8")
        play = method(menu, "PlayPilotPreview")
        self.assertIn("IA_PilotHudPreview.PlayLocal(scene)", play)
        # The pause menu hides the HUD the card is drawn on.
        self.assertIn("CloseMenuByPreset(ChimeraMenuPreset.PauseMenu)", play)
        scenes = re.findall(r"PlayPilotPreview\(IA_PilotHudPreviewScene\.(\w+)\)", menu)
        declared = re.search(r"enum IA_PilotHudPreviewScene\s*\{([^}]*)\}", preview).group(1)
        self.assertEqual(sorted(scenes), sorted(name.strip() for name in declared.split(",")))


if __name__ == "__main__":
    unittest.main()
