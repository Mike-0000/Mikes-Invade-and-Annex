#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA transport pilot regression", wbModules: {"ResourceManager"})]
class IA_TransportPilotTest : WorkbenchPlugin
{
	protected int m_iFailures;

	override void RunCommandline()
	{
		TestWeighting();
		TestCreditRules();
		TestGlobalRating();
		TestSkinEligibility();
		TestBackoff();
		TestCardPayload();
		TestCardMerge();
		TestCardText();
		TestCardTiming();
		TestCardPreview();

		if (m_iFailures == 0)
			Print("[IA][TransportPilotTest] PASS", LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	protected void TestWeighting()
	{
		Check(IA_TransportScoring.InsertionPoints(0) == 30, "a dropoff inside the objective pays the hot weight");
		Check(IA_TransportScoring.InsertionPoints(150) == 30, "the hot band reaches 150 m past the circle");
		Check(IA_TransportScoring.InsertionPoints(575) == 20, "the weight falls linearly across the band");
		Check(IA_TransportScoring.InsertionPoints(1000) == 10, "the band edge pays base points");
		Check(IA_TransportScoring.InsertionPoints(1001) == 0, "a dropoff past the band pays nothing");
		Check(IA_TransportScoring.InsertionPoints(300) > IA_TransportScoring.InsertionPoints(800), "closer dropoffs pay more");
		Check(IA_TransportScoring.MaxPointsPerInsertion() == 30, "the backend cap matches the scoring maximum");

		Check(IA_TransportScoring.EdgeDistance("100 0 0", "0 50 0", 40) == 60, "edge distance is horizontal and measured from the circle");
		Check(IA_TransportScoring.EdgeDistance("10 0 0", "0 0 0", 40) == 0, "a point inside the circle is at distance zero");
	}

	protected void TestCreditRules()
	{
		Check(!IA_TransportScoring.IsCreditableRide(299, 500000, -1), "a seat shuffle is not transport");
		Check(IA_TransportScoring.IsCreditableRide(300, 500000, -1), "a first ride of 300 m is creditable");
		Check(!IA_TransportScoring.IsCreditableRide(2000, 500000, 400000), "the same passenger cannot be credited twice inside the cooldown");
		Check(IA_TransportScoring.IsCreditableRide(2000, 520000, 400000), "the passenger is creditable again after the cooldown");
	}

	protected void TestGlobalRating()
	{
		ref IA_TransportPilotRecord record = new IA_TransportPilotRecord();
		record.m_iPendingPoints = 3000;
		Check(record.GetRating() == IA_TransportPilotRecord.RATING_UNKNOWN, "local points alone never produce a rating");

		record.m_iGlobalRating = 2400;
		record.m_iPendingPoints = 60;
		record.m_iSentPoints = 50;
		Check(record.GetRating() == 2510, "unacknowledged points count on top of the global total");
	}

	protected void TestSkinEligibility()
	{
		string huey = "Prefabs/Vehicles/Helicopters/UH1H/UH1H.et";
		string hip = "Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_transport.et";
		IA_HeliSkinDef tan = IA_HeliSkinCatalog.FindDefByKey("huey_tan");
		Check(tan != null, "the tan Huey skin is catalogued");
		if (!tan)
			return;

		int required = tan.m_iRequiredPoints;
		Check(IA_HeliSkinCatalog.ResolveForPilot(IA_TransportPilotRecord.RATING_UNKNOWN, huey) == null, "an unknown rating unlocks nothing");
		Check(IA_HeliSkinCatalog.ResolveForPilot(required - 1, huey) == null, "a rating below the threshold unlocks nothing");
		Check(IA_HeliSkinCatalog.ResolveForPilot(required, huey) == tan, "the threshold rating unlocks the tan Huey");
		Check(IA_HeliSkinCatalog.ResolveForPilot(required, hip) == null, "a Huey skin is not offered on another airframe");
		Check(IA_HeliSkinCatalog.FindNewlyUnlocked(required - 10, required + 20) == tan, "crossing the threshold reports the unlock");
		Check(IA_HeliSkinCatalog.FindNewlyUnlocked(required, required + 30) == null, "an already unlocked skin is not reported again");
		Check(IA_HeliSkinCatalog.FindNextLocked(0) == tan, "the next locked skin is the progress target");

		// The backend's threshold replaces the built-in default for every server.
		IA_HeliSkinCatalog.SetRequiredPoints("huey_tan", required + 500);
		Check(IA_HeliSkinCatalog.ResolveForPilot(required, huey) == null, "a raised central threshold locks the skin again");
		IA_HeliSkinCatalog.SetRequiredPoints("huey_tan", 0);
		IA_HeliSkinCatalog.SetRequiredPoints("unknown_skin", 1);
		Check(tan.m_iRequiredPoints == required + 500, "invalid thresholds and unknown keys are ignored");
		IA_HeliSkinCatalog.SetRequiredPoints("huey_tan", required);
	}

	protected void TestBackoff()
	{
		Check(IA_TransportScoring.BackoffMs(1, 60000, 1800000) == 60000, "the first failure waits the base interval");
		Check(IA_TransportScoring.BackoffMs(3, 60000, 1800000) == 240000, "each further failure doubles the wait");
		Check(IA_TransportScoring.BackoffMs(40, 60000, 1800000) == 1800000, "the wait is capped without overflowing");
	}

	protected void TestCardPayload()
	{
		ref IA_PilotDropoffPayload sent = new IA_PilotDropoffPayload();
		sent.m_iKind = IA_PilotDropoffPayload.KIND_DROP;
		sent.m_iTroops = 12;
		sent.m_iPoints = 360;
		sent.m_iRating = 12700;
		sent.m_iRequired = 50000;
		sent.m_iEdgeM = 40;
		sent.m_sSkinName = "Desert Tan Huey";
		Check(sent.Pack() == "0|12|360|12700|50000|40|0|-|Desert Tan Huey", "the card payload wire format is pinned");

		IA_PilotDropoffPayload got = IA_PilotDropoffPayload.Parse(sent.Pack());
		Check(got != null, "a packed payload parses");
		if (got)
		{
			Check(got.IsDrop() && got.m_iTroops == 12 && got.m_iPoints == 360, "troops and points survive the round trip");
			Check(got.m_iRating == 12700 && got.m_iRequired == 50000 && got.m_iEdgeM == 40, "rating, threshold and distance survive the round trip");
			Check(got.m_sSkinName == "Desert Tan Huey" && !got.HasUnlock(), "an absent unlock stays absent");
			Check(got.WeightTenths() == 30 && got.IsHotLz(), "a full hot-LZ cabin reads x3.0");
		}

		sent.m_iRating = IA_TransportPilotRecord.RATING_UNKNOWN;
		got = IA_PilotDropoffPayload.Parse(sent.Pack());
		Check(got && got.m_iRating < 0, "an unknown rating stays unknown");

		Check(IA_PilotDropoffPayload.Parse("Combat insertion: +30 transport rating") == null, "plain text is not a payload");
		Check(IA_PilotDropoffPayload.Parse("0|0|0|100|50000|0|0|-|-") == null, "a drop with no passengers is rejected");
		Check(IA_PilotDropoffPayload.Parse("7|1|30|100|50000|0|0|-|-") == null, "an unknown kind is rejected");
		Check(IA_PilotDropoffPayload.Parse("1|0|0|100|50000|0|0|-|-") != null, "a status needs no passengers");
	}

	protected void TestCardMerge()
	{
		// Two ticks of one landing, the first before the global total was known.
		IA_PilotDropoffPayload card = IA_PilotDropoffPayload.Parse("0|4|120|-1|0|0|0|-|-");
		IA_PilotDropoffPayload second = IA_PilotDropoffPayload.Parse("0|8|160|12700|50000|600|0|-|Desert Tan Huey");
		Check(card != null && second != null, "merge inputs parse");
		if (!card || !second)
			return;

		card.Merge(second);
		Check(card.m_iTroops == 12 && card.m_iPoints == 280, "passengers and points add up on one card");
		Check(card.m_iEdgeM == 400, "the distance is averaged per passenger");
		Check(card.m_iRating == 12700 && card.m_iRequired == 50000, "the newer total wins");
		Check(card.WeightTenths() == 23 && !card.IsHotLz(), "a mixed landing is not a hot LZ");

		// A rating card that arrives while the landing is on screen must not downgrade it.
		IA_PilotDropoffPayload status = IA_PilotDropoffPayload.Parse("1|0|0|50020|0|0|50000|Desert Tan Huey|Desert Tan Huey");
		Check(status != null && status.HasUnlock(), "a status can carry an unlock");
		if (!status)
			return;

		card.Merge(status);
		Check(card.IsDrop() && card.m_iTroops == 12, "a status merged into a landing keeps the landing");
		Check(card.HasUnlock() && card.m_iUnlockedRequired == 50000 && card.m_iRating == 50020, "the unlock and the new total are taken");

		card.Merge(second);
		Check(card.HasUnlock(), "an unlock stays announced once reported");

		IA_PilotDropoffPayload seat = IA_PilotDropoffPayload.Parse("1|0|0|900|50000|0|0|-|Desert Tan Huey");
		if (seat)
		{
			seat.Merge(second);
			Check(seat.IsDrop(), "a landing merged into a rating card upgrades it");
		}
	}

	protected void TestCardText()
	{
		Check(IA_PilotDropoffPayload.FormatNumber(0) == "0", "zero has no separator");
		Check(IA_PilotDropoffPayload.FormatNumber(999) == "999", "three digits have no separator");
		Check(IA_PilotDropoffPayload.FormatNumber(12700) == "12,700", "thousands are separated");
		Check(IA_PilotDropoffPayload.FormatNumber(1234567) == "1,234,567", "millions are separated twice");

		Check(IA_PilotDropoffPayload.PercentTenths(12700, 50000) == 254, "progress is in tenths of a percent");
		Check(IA_PilotDropoffPayload.PercentTenths(49999, 50000) == 999, "progress never rounds up to complete");
		Check(IA_PilotDropoffPayload.PercentTenths(50000, 50000) == 1000, "the threshold is complete");
		Check(IA_PilotDropoffPayload.PercentTenths(100, 0) == 0, "no threshold is no progress");
		Check(IA_PilotDropoffPayload.FormatPercent(254) == "25.4%", "tenths are shown");
		Check(IA_PilotDropoffPayload.FormatPercent(5) == "0.5%", "a small start is still visible");
		Check(IA_PilotDropoffPayload.FormatPercent(1000) == "100%", "complete has no decimals");

		IA_PilotDropoffPayload drop = IA_PilotDropoffPayload.Parse("0|12|360|12700|50000|40|0|-|Desert Tan Huey");
		IA_PilotDropoffPayload status = IA_PilotDropoffPayload.Parse("1|0|0|12700|50000|0|0|-|Desert Tan Huey");
		if (drop && status)
		{
			Check(drop.ToLine() == "Combat insertion: 12 troops, +360 transport rating (12,700 / 50,000 Desert Tan Huey)", "the legacy HUD gets one line per landing");
			Check(status.ToLine().IsEmpty(), "the legacy HUD shows no rating-only line");
		}
	}

	protected void TestCardTiming()
	{
		Check(IA_TransportScoring.IsNewPilotSeat(10000, -1), "a first pilot seat is new");
		Check(!IA_TransportScoring.IsNewPilotSeat(10000, 9000), "staying in the seat is not a new seat");
		Check(IA_TransportScoring.IsNewPilotSeat(20000, 9000), "sitting down again after a break is a new seat");
		Check(IA_TransportScoring.IsStatusDue(10000, -1), "the first rating card is due");
		Check(!IA_TransportScoring.IsStatusDue(100000, 40000), "a rating card is not repeated inside the cooldown");
		Check(IA_TransportScoring.IsStatusDue(160000, 40000), "a rating card is due again after the cooldown");

		IA_HeliSkinDef tan = IA_HeliSkinCatalog.FindDefByKey("huey_tan");
		if (!tan)
			return;
		Check(IA_HeliSkinCatalog.FindBestUnlocked(tan.m_iRequiredPoints - 1) == null, "nothing is unlocked below the threshold");
		Check(IA_HeliSkinCatalog.FindBestUnlocked(tan.m_iRequiredPoints) == tan, "the best unlocked skin is reported once earned");
	}

	protected void TestCardPreview()
	{
		IA_HeliSkinDef tan = IA_HeliSkinCatalog.FindDefByKey("huey_tan");
		if (!tan)
			return;
		int required = tan.m_iRequiredPoints;

		ref IA_PilotHudPreview seat = IA_PilotHudPreview.Create(IA_PilotHudPreviewScene.Seat);
		Check(seat.Count() == 1 && seat.NextOpensCard(), "a preview scene opens its own card");
		Check(seat.NextDelayMs() == IA_PilotHudPreview.LEAD_IN_MS, "the first preview card waits for the menus to close");

		ref IA_PilotDropoffPayload card = FoldPreview(IA_PilotHudPreviewScene.Seat);
		Check(card && !card.IsDrop() && card.m_iRating > 0 && card.m_iRequired == required, "the seat preview is a rating card below the threshold");
		int start = 0;
		if (card)
			start = card.m_iRating;

		card = FoldPreview(IA_PilotHudPreviewScene.Landing);
		Check(card && card.m_iTroops == 12 && card.m_iPoints == 360 && card.IsHotLz(), "the landing preview is a full hot-LZ cabin on one card");
		Check(card && card.m_iRating == start + 360 && !card.HasUnlock(), "the landing preview adds its points to the total");

		card = FoldPreview(IA_PilotHudPreviewScene.FarLanding);
		Check(card && card.m_iTroops == 5 && card.m_iPoints == 99 && !card.IsHotLz(), "the far landing preview is paid by the real scoring rules");

		ref IA_PilotHudPreview syncing = IA_PilotHudPreview.Create(IA_PilotHudPreviewScene.Syncing);
		IA_PilotDropoffPayload banked = IA_PilotDropoffPayload.Parse(syncing.TakeNext());
		Check(banked && banked.IsDrop() && banked.m_iRating < 0, "the syncing preview lands before the total is known");
		Check(syncing.NextDelayMs() == IA_PilotHudPreview.SYNC_MS && !syncing.NextOpensCard(), "the total follows into the same card");
		card = FoldPreview(IA_PilotHudPreviewScene.Syncing);
		Check(card && card.IsDrop() && card.m_iRating == start + card.m_iPoints, "the syncing preview ends with the total");

		card = FoldPreview(IA_PilotHudPreviewScene.Unlock);
		Check(card && card.HasUnlock() && card.m_iUnlockedRequired == required, "the unlock preview crosses the threshold");
		Check(card && card.m_iTroops == 10 && card.m_iRating == required + 100 && card.m_iRequired == 0, "passengers after the unlock stay on the unlock card");

		card = FoldPreview(IA_PilotHudPreviewScene.UnlockedSeat);
		Check(card && !card.IsDrop() && !card.HasUnlock() && card.m_iRequired == 0 && !card.m_sSkinName.IsEmpty(), "the unlocked seat preview names the owned skin without announcing it again");

		card = FoldPreview(IA_PilotHudPreviewScene.LateUnlock);
		Check(card && !card.IsDrop() && card.HasUnlock(), "the late unlock preview arrives on a rating card");

		ref IA_PilotHudPreview all = IA_PilotHudPreview.Create(IA_PilotHudPreviewScene.All);
		int updates = all.Count();
		int cards = 0;
		while (!all.IsDone())
		{
			if (all.NextOpensCard())
				cards = cards + 1;
			if (!IA_PilotDropoffPayload.Parse(all.TakeNext()))
				cards = -100;
		}
		Check(updates == 14 && cards == 7, "play all runs every scene as its own card");
	}

	protected IA_PilotDropoffPayload FoldPreview(IA_PilotHudPreviewScene scene)
	{
		ref IA_PilotHudPreview preview = IA_PilotHudPreview.Create(scene);
		ref IA_PilotDropoffPayload card;
		ref IA_PilotDropoffPayload update;
		while (!preview.IsDone())
		{
			update = IA_PilotDropoffPayload.Parse(preview.TakeNext());
			if (!update)
				return null;
			if (card)
				card.Merge(update);
			else
				card = update;
		}
		return card;
	}

	protected void Check(bool condition, string description)
	{
		if (condition)
			return;
		m_iFailures = m_iFailures + 1;
		Print("[IA][TransportPilotTest] FAIL: " + description, LogLevel.ERROR);
	}
}
#endif
