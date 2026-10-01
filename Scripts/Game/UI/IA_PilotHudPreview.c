//------------------------------------------------------------------------------------------------
//! Solo test path for the pilot card (IA_PilotHud). Builds the updates a server
//! would send for a scene and replays them on the local HUD only: no passengers,
//! no points, no backend. Every update is filled by the same
//! IA_PilotDropoffPayload.SetProgress and IA_TransportScoring the server uses,
//! so the card shows what a real flight would. Started from the admin menu.
//------------------------------------------------------------------------------------------------
enum IA_PilotHudPreviewScene
{
	Seat,
	Landing,
	FarLanding,
	Syncing,
	Unlock,
	UnlockedSeat,
	LateUnlock,
	All
}

class IA_PilotHudPreview
{
	//! Lets the menus that started the preview close before the first card.
	static const int LEAD_IN_MS = 700;
	//! The server reports a pilot's landing at most once per tracker tick.
	static const int TICK_MS = 1000;
	static const int SCENE_GAP_MS = 900;
	//! How long the backend total takes to arrive in the Syncing scene.
	static const int SYNC_MS = 3500;
	//! Progress of the pilot in the scenes that are not about the unlock, in tenths of a percent.
	protected static const int START_TENTHS = 254;

	protected ref array<int> m_aDelayMs = new array<int>();
	protected ref array<bool> m_aOpensCard = new array<bool>();
	protected ref array<string> m_aPayloads = new array<string>();
	protected int m_iNext;

	// The pilot being imitated while a scene is built.
	protected int m_iRequired;
	protected int m_iRating;
	protected int m_iUnreported;

	//------------------------------------------------------------------------------------------------
	//! \return the updates of one scene, or of every scene in turn for All
	static IA_PilotHudPreview Create(IA_PilotHudPreviewScene scene)
	{
		ref IA_PilotHudPreview preview = new IA_PilotHudPreview();
		IA_HeliSkinDef goal = IA_HeliSkinCatalog.FindNextLocked(0);
		if (!goal)
			return preview;

		preview.m_iRequired = goal.m_iRequiredPoints;
		if (scene != IA_PilotHudPreviewScene.All)
		{
			preview.AddScene(scene);
			return preview;
		}

		for (int i = 0; i < IA_PilotHudPreviewScene.All; i++)
		{
			preview.AddScene(i);
		}
		return preview;
	}

	//------------------------------------------------------------------------------------------------
	//! Replay a scene on the local player's HUD.
	//! \return false when there is no HUD to show it on
	static bool PlayLocal(IA_PilotHudPreviewScene scene)
	{
		SCR_HUDManagerComponent hud = GetGame().GetHUDManager();
		if (!hud)
			return false;

		IA_NotificationDisplay display = IA_NotificationDisplay.Cast(hud.FindInfoDisplay(IA_NotificationDisplay));
		if (!display)
			return false;

		ref IA_PilotHudPreview preview = Create(scene);
		if (preview.IsDone())
			return false;

		display.PlayPilotPreview(preview);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_aPayloads.Count();
	}

	//------------------------------------------------------------------------------------------------
	bool IsDone()
	{
		return m_iNext >= m_aPayloads.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Milliseconds between the previous update and the next one.
	int NextDelayMs()
	{
		if (IsDone())
			return 0;
		return m_aDelayMs[m_iNext];
	}

	//------------------------------------------------------------------------------------------------
	//! True when the next update starts a scene, so it must not be merged into a card still on screen.
	bool NextOpensCard()
	{
		if (IsDone())
			return false;
		return m_aOpensCard[m_iNext];
	}

	//------------------------------------------------------------------------------------------------
	//! \return the next packed IA_PilotDropoffPayload, empty when the preview is over
	string TakeNext()
	{
		if (IsDone())
			return "";
		string payload = m_aPayloads[m_iNext];
		m_iNext = m_iNext + 1;
		return payload;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddScene(IA_PilotHudPreviewScene scene)
	{
		m_iRating = StartRating();
		m_iUnreported = 0;

		switch (scene)
		{
			// Taking the pilot seat: the rating card alone.
			case IA_PilotHudPreviewScene.Seat:
				AddStatus(true);
				break;

			// A full cabin set down inside the objective; the passengers reach the ground over four ticks.
			case IA_PilotHudPreviewScene.Landing:
				AddDrop(true, 3, 0, true);
				AddDrop(false, 4, 40, true);
				AddDrop(false, 3, 90, true);
				AddDrop(false, 2, 120, true);
				break;

			// A part-full cabin set down short of the objective, for the lower weight and the distance label.
			case IA_PilotHudPreviewScene.FarLanding:
				AddDrop(true, 3, 520, true);
				AddDrop(false, 2, 640, true);
				break;

			// The backend total is unknown at touchdown and arrives while the card is open.
			case IA_PilotHudPreviewScene.Syncing:
				AddDrop(true, 6, 0, false);
				AddStatus(false);
				m_aDelayMs[m_aDelayMs.Count() - 1] = SYNC_MS;
				break;

			// The landing that crosses the threshold, with two passengers still stepping out after it.
			case IA_PilotHudPreviewScene.Unlock:
				m_iRating = m_iRequired - 200;
				AddDrop(true, 4, 0, true);
				AddDrop(false, 4, 0, true);
				AddDrop(false, 2, 0, true);
				break;

			// A pilot who already owns the skin takes the seat.
			case IA_PilotHudPreviewScene.UnlockedSeat:
				m_iRating = m_iRequired + 1240;
				AddStatus(true);
				break;

			// Points banked while the total was unknown crossed the threshold: the unlock arrives on a rating card.
			case IA_PilotHudPreviewScene.LateUnlock:
				m_iRating = m_iRequired + 80;
				m_iUnreported = 180;
				AddStatus(true);
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected int StartRating()
	{
		float rating = m_iRequired;
		rating = rating * START_TENTHS / 1000;
		return Math.Round(rating);
	}

	//------------------------------------------------------------------------------------------------
	//! \param known false imitates a landing reported before the backend total arrived
	protected void AddDrop(bool opensCard, int troops, int edgeM, bool known)
	{
		ref IA_PilotDropoffPayload payload = new IA_PilotDropoffPayload();
		payload.m_iKind = IA_PilotDropoffPayload.KIND_DROP;
		payload.m_iTroops = troops;
		payload.m_iPoints = troops * IA_TransportScoring.InsertionPoints(edgeM);
		payload.m_iEdgeM = edgeM;

		m_iRating = m_iRating + payload.m_iPoints;
		if (known)
		{
			payload.SetProgress(m_iRating, payload.m_iPoints + m_iUnreported);
			m_iUnreported = 0;
		}
		else
		{
			m_iUnreported = m_iUnreported + payload.m_iPoints;
		}
		Add(opensCard, payload);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddStatus(bool opensCard)
	{
		ref IA_PilotDropoffPayload payload = new IA_PilotDropoffPayload();
		payload.m_iKind = IA_PilotDropoffPayload.KIND_STATUS;
		payload.SetProgress(m_iRating, m_iUnreported);
		m_iUnreported = 0;
		Add(opensCard, payload);
	}

	//------------------------------------------------------------------------------------------------
	protected void Add(bool opensCard, notnull IA_PilotDropoffPayload payload)
	{
		int delay = TICK_MS;
		if (m_aPayloads.IsEmpty())
			delay = LEAD_IN_MS;
		else if (opensCard)
			delay = SCENE_GAP_MS;

		m_aDelayMs.Insert(delay);
		m_aOpensCard.Insert(opensCard);
		m_aPayloads.Insert(payload.Pack());
	}
}
