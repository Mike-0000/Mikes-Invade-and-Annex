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

	protected void Check(bool condition, string description)
	{
		if (condition)
			return;
		m_iFailures = m_iFailures + 1;
		Print("[IA][TransportPilotTest] FAIL: " + description, LogLevel.ERROR);
	}
}
#endif
