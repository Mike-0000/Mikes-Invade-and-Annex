//------------------------------------------------------------------------------------------------
//! Leaderboard menu. Uses MUI blank layout + CreateTabs (no legacy TabView path).
//------------------------------------------------------------------------------------------------
class IA_StatisticsMenu : MUI_MenuBase
{
	protected ref MUI_Tabs m_Tabs;
	protected ref MUI_Panel m_HeaderRow;
	protected ref MUI_ScrollView m_Scroll;
	protected ref array<ref IA_LeaderboardRow> m_aRows;
	protected int m_iActiveTab;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		m_aRows = new array<ref IA_LeaderboardRow>();
		m_iActiveTab = 0;
		super.OnMenuOpen();
		if (IsMUIOpen())
			SelectTab(0);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		DetachLeaderboardCallbacks();
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMUIMountFailed()
	{
		Print("[IA_StatisticsMenu] MUI mount failed — blank layout required.", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	override string GetMUILogTag()
	{
		return "IA_StatisticsMenu";
	}

	//------------------------------------------------------------------------------------------------
	override void BuildUI(notnull MUI_Runtime runtime)
	{
		ref IA_MuiShell shell = IA_MuiShell.Create(
			runtime,
			"LEADERBOARD",
			"COMMAND UPLINK",
			"Server  •  Global  •  Global by server",
			1100
		);

		m_Tabs = runtime.CreateTabs("tabs");
		m_Tabs.SetIntro(0.28, 0.4, 16);
		m_Tabs.AddTab("Server");
		m_Tabs.AddTab("Global");
		m_Tabs.AddTab("Global by Server");
		m_Tabs.GetOnChanged().Insert(OnTabsChanged);

		ref IA_LeaderboardRow headerRow = IA_LeaderboardRow.Create(runtime, "hdr", true);
		m_HeaderRow = headerRow.GetRow();
		headerRow.SetObjVisible(true);
		m_HeaderRow.SetIntro(0.36, 0.35, 10);
		m_aRows.Insert(headerRow);

		ref MUI_Divider headerDiv = runtime.CreateDivider("headerDiv");

		m_Scroll = runtime.CreateScrollView("scroll");
		m_Scroll.SetViewportHeight(420);
		m_Scroll.SetGap(4);
		m_Scroll.SetIntro(0.38, 0.4, 16);

		ref MUI_Row buttons = runtime.CreateRow("buttons");
		buttons.SetGap(12);
		buttons.SetIntro(0.52, 0.4, 18);

		ref MUI_Button closeBtn = runtime.CreateButton("Close", "close");
		closeBtn.GetOnClicked().Insert(OnMikesClose);
		buttons.AddChild(closeBtn);

		shell.GetCard().AddChild(m_Tabs);
		shell.GetCard().AddChild(m_HeaderRow);
		shell.GetCard().AddChild(headerDiv);
		shell.GetCard().AddChild(m_Scroll);
		shell.AddFooter(
			runtime,
			"Set ./profile/MikesInvadeAndAnnex/server_name.txt for the server board",
			buttons
		);
		shell.Mount(runtime);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTabsChanged()
	{
		if (!m_Tabs)
			return;
		SelectTab(m_Tabs.GetIndex());
	}

	//------------------------------------------------------------------------------------------------
	protected void SelectTab(int tabIndex)
	{
		m_iActiveTab = tabIndex;
		if (m_Tabs && m_Tabs.GetIndex() != tabIndex)
			m_Tabs.SetIndex(tabIndex);

		bool showObj = true;
		if (tabIndex == 2)
			showObj = false;

		if (m_aRows.Count() > 0 && m_aRows[0])
			m_aRows[0].SetObjVisible(showObj);

		DetachLeaderboardCallbacks();
		ClearDataRows();

		IA_LeaderboardManagerComponent manager = IA_LeaderboardManagerComponent.GetInstance();
		if (!manager)
		{
			Print("[IA_StatisticsMenu] Could not find IA_LeaderboardManagerComponent.", LogLevel.ERROR);
			return;
		}

		string cachedData = "";
		if (tabIndex == 0)
		{
			cachedData = manager.GetCachedServerLeaderboardData();
			manager.GetOnServerLeaderboardDataUpdated().Insert(this.PopulateLeaderboardMikes);
		}
		else if (tabIndex == 1)
		{
			cachedData = manager.GetCachedLeaderboardData();
			manager.GetOnLeaderboardDataUpdated().Insert(this.PopulateLeaderboardMikes);
		}
		else if (tabIndex == 2)
		{
			cachedData = manager.GetCachedGlobalServerLeaderboardData();
			manager.GetOnGlobalServerLeaderboardDataUpdated().Insert(this.PopulateLeaderboardMikes);
		}

		if (cachedData && cachedData != "")
			PopulateLeaderboardMikes(cachedData);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearDataRows()
	{
		if (m_Scroll)
			m_Scroll.ClearChildren();

		while (m_aRows.Count() > 1)
			m_aRows.Remove(m_aRows.Count() - 1);
	}

	//------------------------------------------------------------------------------------------------
	protected void DetachLeaderboardCallbacks()
	{
		IA_LeaderboardManagerComponent manager = IA_LeaderboardManagerComponent.GetInstance();
		if (!manager)
			return;

		manager.GetOnLeaderboardDataUpdated().Remove(this.PopulateLeaderboardMikes);
		manager.GetOnServerLeaderboardDataUpdated().Remove(this.PopulateLeaderboardMikes);
		manager.GetOnGlobalServerLeaderboardDataUpdated().Remove(this.PopulateLeaderboardMikes);
	}

	//------------------------------------------------------------------------------------------------
	void PopulateLeaderboardMikes(string jsonData)
	{
		if (!GetRuntime() || !m_Scroll)
			return;

		ClearDataRows();

		JsonLoadContext jsonContext = new JsonLoadContext();
		if (!jsonContext.LoadFromString(jsonData))
		{
			Print("[IA_StatisticsMenu] Failed to import JSON from string.", LogLevel.ERROR);
			return;
		}

		ref array<ref IA_PlayerStatEntry> playerStats = new array<ref IA_PlayerStatEntry>();
		if (!jsonContext.ReadValue("", playerStats))
		{
			Print("[IA_StatisticsMenu] Failed to read player stats from JSON.", LogLevel.ERROR);
			return;
		}

		if (playerStats.IsEmpty())
			return;

		MUI_Runtime runtime = GetRuntime();
		bool showObj = true;
		if (m_iActiveTab == 2)
			showObj = false;

		float topScore = 1;
		int i;
		for (i = 0; i < playerStats.Count(); i++)
		{
			if (!playerStats[i])
				continue;
			if (playerStats[i].score > topScore)
				topScore = playerStats[i].score;
		}

		for (i = 0; i < playerStats.Count(); i++)
		{
			IA_PlayerStatEntry playerStat = playerStats[i];
			if (!playerStat)
				continue;

			ref IA_LeaderboardRow row = IA_LeaderboardRow.Create(runtime, i.ToString(), false);
			row.SetValues(
				(i + 1).ToString() + ".",
				playerStat.PlayerName,
				playerStat.kills.ToString(),
				playerStat.deaths.ToString(),
				playerStat.hvt_kills.ToString(),
				playerStat.hvt_guard_kills.ToString(),
				playerStat.obj_score.ToString(),
				playerStat.score.ToString()
			);
			row.SetObjVisible(showObj);
			row.SetRankHighlight(i);
			float ratio = 0;
			if (topScore > 0)
				ratio = playerStat.score / topScore;
			row.SetScoreRatio(ratio);
			m_Scroll.AddChild(row.GetRow());
			m_aRows.Insert(row);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMikesClose()
	{
		Close();
	}
}
