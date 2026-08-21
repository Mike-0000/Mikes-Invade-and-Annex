//------------------------------------------------------------------------------------------------
//! Middle-left dock for live capture and defend bars. Pattern: Create(runtime)
//! → overlay.AddChild. Keep as protected ref. Children are IA_CaptureHud tiles
//! plus one IA_DefendHud; they Hug their chrome and pack left-to-right. Align
//! 0.25 sits the group halfway between the left edge and screen center.
//! Capture tiles only appear for zones the local player is standing in.
//------------------------------------------------------------------------------------------------
class IA_ObjectiveHudStrip : MUI_Row
{
	protected static const int CAPTURE_TILES = 6;
	protected static const float TILE_MIN_W = 200;
	protected static const float STRIP_GAP = 8;

	protected ref array<ref IA_CaptureHud> m_aCaptures;
	protected ref IA_DefendHud m_Defend;
	protected ref array<string> m_aAreas;
	protected ref array<int> m_aStates;
	protected ref array<float> m_aProgress;
	protected ref array<string> m_aInside;
	protected string m_sLastPacked;

	//------------------------------------------------------------------------------------------------
	void IA_ObjectiveHudStrip()
	{
		m_Style.m_bBlockHit = false;
		m_Style.m_bInteractive = false;
		m_Style.m_Fill = Color.FromInt(0);
		m_aCaptures = new array<ref IA_CaptureHud>();
		m_aAreas = new array<string>();
		m_aStates = new array<int>();
		m_aProgress = new array<float>();
		m_aInside = new array<string>();
		m_sLastPacked = "";
	}

	//------------------------------------------------------------------------------------------------
	static IA_ObjectiveHudStrip Create(notnull MUI_Runtime runtime)
	{
		ref IA_ObjectiveHudStrip strip = new IA_ObjectiveHudStrip();
		runtime.Adopt(strip);
		strip.SetName("objectiveHud");
		strip.SetHugWidth();
		strip.SetHugHeight();
		strip.SetAlign(0.25, 1);
		strip.SetGap(STRIP_GAP);
		strip.BuildTiles(runtime);
		return strip;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildTiles(notnull MUI_Runtime runtime)
	{
		int i;
		for (i = 0; i < CAPTURE_TILES; i++)
		{
			ref IA_CaptureHud tile = IA_CaptureHud.Create(runtime);
			m_aCaptures.Insert(tile);
			AddChild(tile);
		}

		m_Defend = IA_DefendHud.Create(runtime);
		AddChild(m_Defend);
	}

	//------------------------------------------------------------------------------------------------
	void Abort()
	{
		int count = m_aCaptures.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			if (m_aCaptures[i])
				m_aCaptures[i].Abort();
		}
		if (m_Defend)
			m_Defend.Abort();
		m_sLastPacked = "";
		m_aAreas.Clear();
		m_aStates.Clear();
		m_aProgress.Clear();
		m_aInside.Clear();
	}

	//------------------------------------------------------------------------------------------------
	override void OnTick(float dt)
	{
		super.OnTick(dt);
		SyncCaptures();
		ApplyDockWidths();
	}

	//------------------------------------------------------------------------------------------------
	protected void SyncCaptures()
	{
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		string packed = "";
		if (init)
			packed = init.GetCaptureHudPacked();

		if (packed != m_sLastPacked)
		{
			m_sLastPacked = packed;
			if (init)
				init.GetCaptureHudSlots(m_aAreas, m_aStates, m_aProgress);
			else
			{
				m_aAreas.Clear();
				m_aStates.Clear();
				m_aProgress.Clear();
			}
		}

		IA_AreaMarker.CollectAreasContainingLocalPlayer(m_aInside);

		int slotCount = m_aAreas.Count();
		int i;
		for (i = 0; i < slotCount; i++)
		{
			if (m_aInside.Find(m_aAreas[i]) < 0)
				continue;

			IA_CaptureHudState state = IA_CaptureHud.DecodeState(m_aStates[i]);
			IA_CaptureHud tile = FindCaptureTile(m_aAreas[i]);
			if (!tile)
				tile = FindIdleCaptureTile();
			if (!tile)
				continue;
			tile.ApplyServer(m_aAreas[i], state, m_aProgress[i]);
		}

		int tileCount = m_aCaptures.Count();
		for (i = 0; i < tileCount; i++)
		{
			IA_CaptureHud tile = m_aCaptures[i];
			if (!tile)
				continue;
			if (tile.IsIdle())
				continue;
			if (m_aInside.Find(tile.GetShownArea()) >= 0)
				continue;
			tile.ApplyServer("", IA_CaptureHudState.Hidden, 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected IA_CaptureHud FindCaptureTile(string area)
	{
		if (area.IsEmpty())
			return null;

		int count = m_aCaptures.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_CaptureHud tile = m_aCaptures[i];
			if (!tile)
				continue;
			if (tile.GetShownArea() == area)
				return tile;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected IA_CaptureHud FindIdleCaptureTile()
	{
		int count = m_aCaptures.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_CaptureHud tile = m_aCaptures[i];
			if (tile && tile.IsIdle())
				return tile;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected int SlotIndexForArea(string area)
	{
		if (area.IsEmpty())
			return -1;

		int count = m_aAreas.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			if (m_aAreas[i] == area)
				return i;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyDockWidths()
	{
		int live = 0;
		int tileCount = m_aCaptures.Count();
		int i;
		for (i = 0; i < tileCount; i++)
		{
			IA_CaptureHud tile = m_aCaptures[i];
			if (tile && tile.IsVisible())
				live = live + 1;
		}
		if (m_Defend && m_Defend.IsVisible())
			live = live + 1;
		if (live <= 0)
			return;

		float avail = 0;
		if (m_Parent)
		{
			MUI_Rect parentRect = m_Parent.GetWorldRect();
			MUI_Style parentStyle = m_Parent.GetStyle();
			avail = parentRect.m_fW - parentStyle.m_fPadL - parentStyle.m_fPadR;
		}

		float tileW = IA_CaptureHud.GetHudWidth();
		if (live > 1 && avail >= TILE_MIN_W)
		{
			float gaps = STRIP_GAP * (live - 1);
			float needed = tileW * live + gaps;
			if (needed > avail)
			{
				tileW = (avail - gaps) / live;
				if (tileW < TILE_MIN_W)
					tileW = TILE_MIN_W;
			}
		}

		for (i = 0; i < tileCount; i++)
		{
			IA_CaptureHud tile = m_aCaptures[i];
			if (tile && tile.IsVisible())
				tile.SetDockWidth(tileW);
		}
		if (m_Defend && m_Defend.IsVisible())
			m_Defend.SetDockWidth(tileW);
	}
}
