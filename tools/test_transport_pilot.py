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

    def test_a_livery_is_one_plain_colour_with_a_threshold_and_no_files(self):
        catalog = source("IA_HeliSkinCatalog.c")
        defs = re.findall(r'AddDef\((\w+), "(\w+)", "([^"]+)", (\d+), Vector\(([\d., ]+)\), (\d+), (\d+), (\d+)\);', catalog)
        self.assertGreaterEqual(len(defs), 2)
        for column in (0, 1, 2):
            self.assertEqual(len({row[column] for row in defs}), len(defs), "ids, keys and names are unique")
        thresholds = [int(row[3]) for row in defs]
        self.assertEqual(thresholds, sorted(set(thresholds)), "the bay lists liveries cheapest first, no two alike")
        for row in defs:
            self.assertRegex(catalog, r"static const int %s = [1-9]\d*;" % row[0])
            paint = [float(v) for v in row[4].split(",")]
            # A military paint, not a signal colour: dark, and close to grey.
            self.assertLess(max(paint), 0.25, row[2])
            self.assertLess(max(paint) - min(paint), 0.12, row[2])
        # A livery names no material or prefab: the family says what its colour is set on.
        self.assertNotRegex(catalog, r"\.emat|\.et\b|AddPaint")

        # The backend row that already exists keeps its key; the others run on these defaults until a row is added.
        sql = MIGRATION.read_text(encoding="utf-8")
        self.assertIn("'huey_tan'", sql)
        self.assertIn("huey_tan", [row[1] for row in defs])
        self.assertIn("SetRequiredPoints(pair[0], pair[1].ToInt())", method(catalog, "ApplyPackedThresholds"))

    def test_paint_channel_files_match_their_registry_and_the_script_manifest(self):
        registry = channels.read_json(ROOT / channels.REGISTRY)
        files = channels.build(registry)
        self.assertEqual(channels.stale(files, ROOT), [])
        self.assertEqual(channels.leftovers(registry, files, ROOT), [])

        table = source("IA_HeliPaintChannels.c")
        self.assertEqual(int(constant(table, "CHANNEL_COUNT")), registry["channel_count"])
        self.assertIn("IA_HeliPaintManifest.Register();", method(table, "EnsureFamilies"))
        manifest = source("IA_HeliPaintManifest.c")
        self.assertEqual(re.findall(r'AddFamily\("(\w+)"', manifest), [family["key"] for family in registry["families"]])

        for family in registry["families"]:
            # A stock material is never named by a twin: it would recolour every helicopter of the type.
            stock = {surface["material"] for surface in family["surfaces"]}
            for surface in family["surfaces"]:
                self.assertTrue(surface["paint"], family["key"] + " surface without a recipe")
                self.assertTrue(any("primary" in spec for surface in family["surfaces"] for spec in surface["paint"].values()))
            for airframe in range(len(family["airframes"])):
                for channel in range(1, registry["channel_count"] + 1):
                    text = files[channels.hull_path(family, airframe, channel)]
                    # Every hull carries the component that tells the server its channel is in use.
                    self.assertIn('  %s "{%s}" {' % (registry["rig_component"], registry["rig_component_id"]), text)
                    for material in stock:
                        self.assertNotIn('AssignedMaterial "%s"' % material, text)

        # A mesh naming another channel's material would be recoloured with that channel's helicopter.
        for path, text in files.items():
            if path.endswith(".et"):
                channel = re.search(r"_Paint(\d+)\.et$", path).group(1)
                assigned = re.findall(r'AssignedMaterial "\{\w+\}[^"]*_Paint(\d+)\.emat"', text)
                self.assertTrue(assigned, path)
                self.assertEqual(set(assigned), {channel}, path)
            if path.endswith(".emat"):
                # A channel material inherits its stock material and sets nothing, so it reads as stock.
                self.assertRegex(text, r'^\w+ : "\{\w{16}\}[^"]+\.emat" \{\n\}\n$', path)

        # The first family keeps the GUIDs it shipped with; a later one gets GUIDs from its file paths.
        first = registry["families"][0]
        self.assertEqual(channels.hull_guid(first, 0, 1), first["legacy_guid_prefix"] + "B001")
        self.assertEqual(channels.hull_guid(first, 0, 12), first["legacy_guid_prefix"] + "B012")
        guids = [text.splitlines()[1] for path, text in files.items() if path.endswith(".meta")]
        self.assertEqual(len(guids), len(set(guids)))

        rig = source("IA_HeliPaintRigComponent.c")
        self.assertIn("class %s : ScriptComponent" % registry["rig_component"], rig)

    def test_a_modded_helicopter_is_a_registry_entry_not_new_code(self):
        # A compatibility addon keeps its own registry and adds its families to the manifest with a modded class.
        registry = channels.read_json(ROOT / channels.REGISTRY)
        addon = {
            "channel_count": registry["channel_count"],
            "rig_component": registry["rig_component"],
            "rig_component_id": registry["rig_component_id"],
            "manifest": {"path": "Scripts/Game/XX_HeliPaintManifest.c", "modded": True, "source": "its registry"},
            "families": [dict(registry["families"][-1], key="mod_heli", art="something_unknown")],
        }
        addon["families"][0].pop("legacy_guid_prefix", None)
        text = channels.build(addon)[addon["manifest"]["path"]]
        self.assertIn("modded class IA_HeliPaintManifest", text)
        self.assertLess(text.index("super.Register();"), text.index("RegisterModHeli();"))

        # Nothing in the game scripts names a helicopter type: families are looked up, liveries apply to all.
        for name in ("IA_HeliSkinPaint.c", "IA_HeliPaintChannels.c", "IA_HeliPaintRigComponent.c", "IA_HeliSkinManagerComponent.c", "IA_HeliPaintService.c", "IA_HeliSkinPadService.c", "IA_HeliSkinCatalog.c"):
            self.assertNotRegex(re.sub(r'"huey_tan"', "", source(name)), r"(?i)uh1h|huey|mi8|\.emat\"|\.et\"", name)

        # An art key nobody knows draws the generic helicopter, so the bay works before a silhouette is drawn.
        art = (ROOT / "Scripts" / "Game" / "UI" / "IA_HeliArt.c").read_text(encoding="utf-8")
        self.assertRegex(method(art, "Create"), r"return new IA_HeliArtGeneric\(\);\s*$")
        bay = (ROOT / "Scripts" / "Game" / "UI" / "IA_HeliPaintBay.c").read_text(encoding="utf-8")
        self.assertIn("IA_HeliPaintChannels.GetFamily(channel)", method(bay, "SetContext"))

    def test_a_skin_is_set_in_place_and_never_changes_or_replaces_the_helicopter(self):
        # Changing a vehicle hull's mesh from script frees the instance its animation is bound to.
        scripts = sorted((ROOT / "Scripts" / "Game").rglob("IA_HeliSkin*.c")) + sorted((ROOT / "Scripts" / "Game").rglob("IA_HeliPaint*.c"))
        self.assertTrue(scripts)
        for path in scripts:
            self.assertNotRegex(path.read_text(encoding="utf-8"), r"SetObject\(|SetVObjectFromPrefab\(|DeleteEntityAndChildren\(|SpawnEntityPrefab\(", path.name)
        self.assertFalse((ROOT / "Scripts" / "Game" / "IA_HeliSkinSwap.c").exists())

        paint = source("IA_HeliSkinPaint.c")
        self.assertIn("Material.GetOrLoadMaterial(material, 0)", method(paint, "PaintSurface"))
        self.assertIn("IA_HeliPaintChannels.GetFamily(channel)", method(paint, "Apply"))
        self.assertIn("surface.GetMaterial(local)", method(paint, "Apply"))
        # ResetParam is the class default, so it is only used where no stock file sets the parameter.
        self.assertRegex(method(paint, "PaintSurface"), r"if \(!def && !param\.m_bStockSet\)\s*\{\s*target\.ResetParam\(")

        # The server says which skin a channel shows; every machine that renders paints it.
        manager = source("IA_HeliSkinManagerComponent.c")
        self.assertRegex(manager, r'\[RplProp\(onRplName: "OnSkinsReplicated"\)\]\s*protected ref array<int> m_aChannelSkins')
        setter = method(manager, "SetVehicleSkin")
        self.assertLess(setter.index("if (!Replication.IsServer())"), setter.index("m_aChannelSkins[channel - 1] = skinId"))
        self.assertIn("Replication.BumpMe()", setter)
        self.assertIn("PaintAll();", method(manager, "OnSkinsReplicated"))
        self.assertIn("IA_HeliSkinPaint.Apply(i + 1, wanted)", method(manager, "PaintAll"))
        self.assertIn("IA_HeliSkinManagerComponent", (ROOT / "Prefabs" / "GameMode_IA.et").read_text(encoding="utf-8"))

        respawner = source("IA_VehicleRespawner.c")
        self.assertNotIn("m_bSwapPending", respawner)

    def test_a_stock_helicopter_spawns_on_a_free_paint_channel_whoever_spawns_it(self):
        # A channel belongs to a live helicopter, not to a pad: the airframe says which one it holds.
        rig = source("IA_HeliPaintRigComponent.c")
        init = method(rig, "OnPostInit")
        self.assertLess(init.index("if (!Replication.IsServer() || !GetGame().InPlayMode())"), init.index("s_aHolders[m_iChannel - 1] = owner"))
        # The channel may still show the last airframe's skin and its pilot's choice.
        self.assertLess(init.index("s_aHolders[m_iChannel - 1] = owner"), init.index("skins.ResetChannel(m_iChannel)"))
        self.assertIn("s_aHolders[m_iChannel - 1] == owner", method(rig, "OnDelete"))
        reset = method(source("IA_HeliSkinManagerComponent.c"), "ResetChannel")
        self.assertLess(reset.index("if (!Replication.IsServer())"), reset.index("m_aPilotChoice[channel - 1] = false"))
        self.assertIn("m_aChannelSkins[channel - 1] = IA_HeliSkinCatalog.SKIN_NONE", reset)

        # An empty channel goes before one whose holder is a wreck; a deleted holder is no holder.
        free = method(rig, "FindFreeChannel")
        self.assertLess(free.index("if (!holder)\n\t\t\t\treturn channel;"), free.index("damage.IsDestroyed()"))
        # Only the channels of the helicopter's own family: another type's materials are not on its meshes.
        self.assertIn("IA_HeliPaintChannels.ToChannel(family, local)", free)
        self.assertIn("holder.IsDeleted()", method(rig, "GetHolder"))

        resolve = method(rig, "ResolveSpawnPrefab")
        self.assertLess(resolve.index("if (!Replication.IsServer() || !GetGame().InPlayMode())"), resolve.index("FindFreeChannel(family)"))
        self.assertLess(resolve.index("IA_HeliPaintChannels.FindStockFamily(prefab)"), resolve.index("FindFreeChannel(family)"))
        self.assertIn("IA_HeliPaintChannels.FindChannelPrefab(prefab, channel)", resolve)
        self.assertIn("family != GetFamily(channel)", method(source("IA_HeliPaintChannels.c"), "FindChannelPrefab"))

        # A pad and the editor (Game Master, build mode) both ask before they spawn.
        spawn = method(source("IA_VehicleRespawner.c"), "PerformSpawn")
        self.assertLess(spawn.index("IA_HeliPaintRigComponent.ResolveSpawnPrefab(vehiclePrefabToSpawn)"), spawn.index("Resource.Load(vehiclePrefabToSpawn)"))
        self.assertNotIn("m_iPaintChannel", source("IA_VehicleRespawner.c"))
        editor = source("Editor/IA_HeliPaintEditorSpawn.c")
        self.assertIn("modded class SCR_EditableEntityComponentClass", editor)
        self.assertIn("IA_HeliPaintRigComponent.ResolveSpawnPrefab(super.GetRandomVariant(prefab))", method(editor, "GetRandomVariant"))

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

    def test_the_paint_bay_asks_and_the_server_decides(self):
        # The client names a skin; the seat, the channel and the unlock are checked on the server, in that order.
        service = source("IA_HeliPaintService.c")
        attempt = method(service, "TrySetSkin")
        order = [
            "if (!Replication.IsServer())",
            "GetPilotedHelicopter(pawn)",
            "IA_HeliPaintChannels.CHANNEL_NONE",
            "IA_HeliSkinCatalog.FindDef(skinId)",
            "if (rating < 0)",
            "IA_HeliSkinCatalog.IsUnlocked(def, rating)",
            "skins.SetVehicleSkin(vehicle, skinId, true)",
        ]
        positions = [attempt.index(step) for step in order]
        self.assertEqual(positions, sorted(positions))
        # The pilot's seat, not the co-pilot's or a passenger's.
        self.assertIn("vehicle.GetPilot() != pawn", method(service, "GetPilotedHelicopter"))

        controller = source("IA_PlayerController.c")
        answer = method(controller, "IA_AnswerHeliPaint")
        # The rating and the admin flag are read on the server; the request carries neither.
        self.assertIn("IA_HeliPaintService.ReadRating(GetPlayerId())", answer)
        self.assertIn("IA_HeliPaintService.TrySetSkin(GetControlledEntity(), skinId, rating, admin)", answer)
        self.assertLess(answer.index("IA_HELI_PAINT_MIN_GAP_MS"), answer.index("IA_HeliPaintService.ReadRating("))
        self.assertRegex(controller, r"RplRcver\.Server\)\]\s*protected void RpcAsk_IA_SetHeliSkin\(int skinId\)")
        self.assertRegex(controller, r"RplRcver\.Owner\)\]\s*protected void RpcDo_IA_HeliPaintReply\(")

        # The menu and its widgets never set a skin or paint a material themselves.
        ui = ROOT / "Scripts" / "Game" / "UI"
        for path in (ui / "Menus" / "IA_HeliPaintMenu.c", ui / "IA_HeliPaintBay.c", ui / "IA_HeliPaintTile.c", ROOT / "Scripts" / "Game" / "IA_HeliPaintHotkey.c"):
            self.assertNotRegex(path.read_text(encoding="utf-8"), r"SetVehicleSkin\(|IA_HeliSkinPaint\.|TrySetSkin\(|Material\.", path.name)
        menu = (ui / "Menus" / "IA_HeliPaintMenu.c").read_text(encoding="utf-8")
        self.assertIn("controller.IA_AskSetHeliSkin(skinId)", method(menu, "OnPick"))
        # It shows what the replicated skin manager says the airframe wears, and leaves with the seat.
        self.assertIn("skins.GetVehicleSkin(vehicle)", method(menu, "Refresh"))
        self.assertLess(method(menu, "Refresh").index("Close();"), method(menu, "Refresh").rindex("m_Bay.SetContext("))
        # A livery the bay believes locked is refused locally and never sent.
        picked = method((ui / "IA_HeliPaintBay.c").read_text(encoding="utf-8"), "OnTilePicked")
        self.assertEqual(picked.count("m_OnPick.Invoke("), 1)
        self.assertLess(picked.index("IA_HeliPaintTile.STATE_READY"), picked.index("m_OnPick.Invoke("))

        # A skin the pilot chose is not taken back by the pad service.
        pads = source("IA_HeliSkinPadService.c")
        self.assertIn("skins.IsPilotChoice(vehicle)", pads)
        self.assertIn("m_aPilotChoice[channel - 1] = pilotChoice", method(source("IA_HeliSkinManagerComponent.c"), "SetVehicleSkin"))

    def test_the_paint_bay_key_is_declared_and_only_live_in_the_pilot_seat(self):
        hotkey = source("IA_HeliPaintHotkey.c")
        action = re.search(r'ACTION = "(\w+)"', hotkey).group(1)
        context = re.search(r'CONTEXT = "(\w+)"', hotkey).group(1)
        inputs = (ROOT / "Configs" / "System" / "chimeraInputCommon.conf").read_text(encoding="utf-8")
        self.assertIn("Action " + action + " {", inputs)
        self.assertRegex(inputs, r"ActionContext " + context + r" \{[^}]*ActionRefs \{\s*\"" + action + r"\"")
        self.assertIn('"keyboard:KC_I"', inputs)
        self.assertIn('"gamepad0:pad_left"', inputs)
        bindings = (ROOT / "Configs" / "System" / "keyBindingMenu.conf").read_text(encoding="utf-8")
        self.assertIn('m_sActionName "' + action + '"', bindings)

        # The context is kept alive only while the local player is the pilot; it is never reset.
        self.assertNotIn("ResetContext", hotkey)
        tick = method(hotkey, "Tick")
        self.assertIn("IA_HeliPaintService.GetPilotedHelicopter(", tick)
        opened = method(hotkey, "OnHotkey")
        self.assertLess(opened.index("IA_HeliPaintService.GetPilotedHelicopter("), opened.index("OpenMenu(ChimeraMenuPreset.IA_HeliPaintMenu)"))
        self.assertIn("IsAnyMenuOpen()", opened)
        self.assertIn("EditBoxWidget", opened)

        display = source("IA_NotificationDisplay.c")
        self.assertIn("m_PaintHotkey.Tick(", method(display, "DisplayUpdate"))
        self.assertIn("m_PaintHotkey.Stop();", method(display, "CloseMikesUI"))

        presets = (ROOT / "Configs" / "System" / "chimeraMenus.conf").read_text(encoding="utf-8")
        self.assertRegex(presets, r'MenuPreset IA_HeliPaintMenu \{\s*Layout "[^"]+"\s*Class "IA_HeliPaintMenu"')
        self.assertIn("IA_HeliPaintMenu", source("IA_ChimeraMenuPresets.c"))

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
