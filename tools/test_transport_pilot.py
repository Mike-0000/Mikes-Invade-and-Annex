"""Source integration guards for transport-pilot progression.

Scoring and eligibility maths are checked natively in Workbench
(IA_TransportPilotTest). Skins, insertions and the backend round trip still
need a mission playtest against a deployed backend.
"""
import re
import unittest

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
        apply = method(source("IA_TransportPilotTracker.c"), "ApplyPilotSkin")
        self.assertRegex(apply, r"if \(rating < 0\)\s*\{[\s\S]*?RequestRating[\s\S]*?return;")
        self.assertLess(apply.index("if (rating < 0)"), apply.index("SetVehicleSkin"))

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

    def test_catalogued_skins_have_a_central_threshold_and_material(self):
        catalog = source("IA_HeliSkinCatalog.c")
        sql = MIGRATION.read_text(encoding="utf-8")
        defs = re.findall(r'AddDef\(\w+, "(\w+)", "[^"]+", "[^"]+", "[^"]+", "([^"]+)", (\d+)\)', catalog)
        self.assertTrue(defs)
        for key, material, required in defs:
            self.assertIn("('%s', %s)" % (key, required), sql)

        # A material without a GUID is unregistered: it resolves to a null GUID
        # and is left out of a published build. IA_HeliSkinAssetCheck registers it.
        materials = [material for _, material, _ in defs]
        materials += re.findall(r'AddSlot\(\w+, "[^"]+", "([^"]+)"\)', catalog)
        self.assertGreater(len(materials), len(defs), "the tan Huey repaints its interior too")
        for material in materials:
            guid, path = re.fullmatch(r"(\{[0-9A-F]{16}\})(.+\.emat)", material).groups()
            if (ROOT / path).is_file():
                self.assertIn('Name "%s"' % material, (ROOT / (path + ".meta")).read_text(encoding="utf-8"))
            else:
                # Only the game's own materials may be referenced without a file here.
                self.assertTrue(path.startswith("Assets/Vehicles/"), material)
                self.assertNotIn("/IA_", path)

    def test_a_skin_can_repaint_several_slots_and_be_previewed_solo(self):
        paint = method(source("IA_HeliSkinManagerComponent.c"), "PaintEntity")
        self.assertIn("def.FindMaterial(materials[i])", paint)
        # The preview paints locally; it must not assign the skin for everyone.
        local = method(source("IA_HeliSkinManagerComponent.c"), "PaintLocal")
        self.assertNotRegex(local, r"m_aSkinVehicles|m_aSkinIds|BumpMe")
        preview = source("IA_HeliSkinPreview.c")
        self.assertIn("skins.PaintLocal(", method(preview, "PaintNearest"))
        self.assertNotRegex(preview, r"SetVehicleSkin|IA_TransportPilotStore|Rpc\(")
        menu = (ROOT / "Scripts" / "Game" / "UI" / "Menus" / "IA_AdminConfigMenu.c").read_text(encoding="utf-8")
        self.assertIn("IA_HeliSkinPreview.PaintNearest()", method(menu, "OnSkinPreview"))

    def test_feature_is_wired_into_the_mission(self):
        self.assertIn("IA_TransportPilotTracker.EnsureStarted();", source("IA_MissionInitializer.c"))
        self.assertIn("IA_HeliSkinManagerComponent", (ROOT / "Prefabs" / "GameMode_IA.et").read_text(encoding="utf-8"))
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
