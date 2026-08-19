//------------------------------------------------------------------------------------------------
//! Leaderboard menu. Uses MUI blank layout + CreateTabs (no legacy TabView path).
//------------------------------------------------------------------------------------------------
class IA_StatisticsMenu : MUI_MenuBase
{
	protected static const int TAB_OPTIONS = 4;
	protected static const string FOOT_LEADERBOARD = "Session board is local to this restart. Set ./profile/MikesInvadeAndAnnex/server_name.txt for the server board";
	protected static const string FOOT_OPTIONS = "Stored in ./profile/MikesInvadeAndAnnex/local_options.json  •  This machine only";
	protected static const string SUB_LEADERBOARD = "Session  •  Server  •  Global  •  Global by server  •  Options";
	protected static const string SUB_OPTIONS = "Local HUD settings  •  This machine only";

	protected ref IA_MuiShell m_Shell;
	protected ref MUI_Tabs m_Tabs;
	protected ref MUI_Panel m_HeaderRow;
	protected ref IA_LeaderboardRow m_Header;
	protected ref MUI_Divider m_HeaderDiv;
	protected ref MUI_ScrollView m_Scroll;
	protected ref MUI_ScrollView m_OptionsScroll;
	protected ref MUI_Toggle m_HideRankToggle;
	protected ref MUI_Toggle m_HidePromoToggle;
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
		m_Shell = IA_MuiShell.Create(
			runtime,
			"LEADERBOARD",
			"COMMAND UPLINK",
			SUB_LEADERBOARD,
			1100
		);

		m_Tabs = runtime.CreateTabs("tabs");
		m_Tabs.SetIntro(0.28, 0.4, 16);
		m_Tabs.AddTab("Session");
		m_Tabs.AddTab("Server");
		m_Tabs.AddTab("Global");
		m_Tabs.AddTab("Global by Server");
		m_Tabs.AddTab("Options");
		m_Tabs.GetOnChanged().Insert(OnTabsChanged);

		ref IA_LeaderboardRow headerRow = IA_LeaderboardRow.Create(runtime, "hdr", true);
		m_Header = headerRow;
		m_HeaderRow = headerRow.GetRow();
		headerRow.SetObjVisible(true);
		m_HeaderRow.SetIntro(0.36, 0.35, 10);
		m_aRows.Insert(headerRow);

		m_HeaderDiv = runtime.CreateDivider("headerDiv");

		m_Scroll = runtime.CreateScrollView("scroll");
		m_Scroll.SetViewportHeight(420);
		m_Scroll.SetGap(4);
		m_Scroll.SetIntro(0.38, 0.4, 16);

		BuildOptionsPage(runtime);

		ref MUI_Row buttons = runtime.CreateRow("buttons");
		buttons.SetGap(12);
		buttons.SetIntro(0.52, 0.4, 18);

		ref MUI_Button closeBtn = runtime.CreateButton("Close", "close");
		closeBtn.GetOnClicked().Insert(OnMikesClose);
		buttons.AddChild(closeBtn);

		m_Shell.GetCard().AddChild(m_Tabs);
		m_Shell.GetCard().AddChild(m_HeaderRow);
		m_Shell.GetCard().AddChild(m_HeaderDiv);
		m_Shell.GetCard().AddChild(m_Scroll);
		m_Shell.GetCard().AddChild(m_OptionsScroll);
		m_Shell.AddFooter(runtime, FOOT_LEADERBOARD, buttons);
		m_Shell.Mount(runtime);

		ShowOptionsPage(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildOptionsPage(notnull MUI_Runtime runtime)
	{
		m_OptionsScroll = runtime.CreateScrollView("options");
		m_OptionsScroll.SetViewportHeight(420);
		m_OptionsScroll.SetGap(12);
		m_OptionsScroll.SetIntro(0.38, 0.4, 16);

		ref MUI_Label intro = runtime.CreateLabel("These options apply only on this machine. They do not sync to the server or other players.", "optIntro");
		intro.SetFontSize(runtime.GetTheme().FONT_SMALL);
		intro.SetMuted(true);

		IA_LocalOptions options = IA_LocalOptions.Get();

		m_HideRankToggle = runtime.CreateToggle("Hide ranking HUD", "hideRank");
		m_HideRankToggle.SetChecked(options.HideRankHud());
		m_HideRankToggle.GetOnChanged().Insert(OnHideRankChanged);

		ref MUI_Label rankHint = runtime.CreateLabel("Hides the session rank chip in the top-right of the HUD.", "hideRankHint");
		rankHint.SetFontSize(runtime.GetTheme().FONT_SMALL);
		rankHint.SetMuted(true);

		m_HidePromoToggle = runtime.CreateToggle("Hide promotion notifications", "hidePromo");
		m_HidePromoToggle.SetChecked(options.HidePromotionNotifications());
		m_HidePromoToggle.GetOnChanged().Insert(OnHidePromoChanged);

		ref MUI_Label promoHint = runtime.CreateLabel("Skips the on-screen toast when you are promoted.", "hidePromoHint");
		promoHint.SetFontSize(runtime.GetTheme().FONT_SMALL);
		promoHint.SetMuted(true);

		m_OptionsScroll.AddChild(intro);
		m_OptionsScroll.AddChild(m_HideRankToggle);
		m_OptionsScroll.AddChild(rankHint);
		m_OptionsScroll.AddChild(m_HidePromoToggle);
		m_OptionsScroll.AddChild(promoHint);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHideRankChanged()
	{
		if (!m_HideRankToggle)
			return;
		IA_LocalOptions.Get().SetHideRankHud(m_HideRankToggle.IsChecked());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHidePromoChanged()
	{
		if (!m_HidePromoToggle)
			return;
		IA_LocalOptions.Get().SetHidePromotionNotifications(m_HidePromoToggle.IsChecked());
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowOptionsPage(bool show)
	{
		if (m_HeaderRow)
			m_HeaderRow.SetVisible(!show);
		if (m_HeaderDiv)
			m_HeaderDiv.SetVisible(!show);
		if (m_Scroll)
			m_Scroll.SetVisible(!show);
		if (m_OptionsScroll)
			m_OptionsScroll.SetVisible(show);

		if (!m_Shell)
			return;

		MUI_LiveHeader header = m_Shell.GetHeader();
		if (show)
		{
			if (header)
				header.SetTitle("OPTIONS");
			m_Shell.SetSubtitle(SUB_OPTIONS);
			m_Shell.SetFooterText(FOOT_OPTIONS);
			return;
		}

		if (header)
			header.SetTitle("LEADERBOARD");
		m_Shell.SetSubtitle(SUB_LEADERBOARD);
		m_Shell.SetFooterText(FOOT_LEADERBOARD);
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

		if (tabIndex == TAB_OPTIONS)
		{
			DetachLeaderboardCallbacks();
			ShowOptionsPage(true);
			return;
		}

		ShowOptionsPage(false);

		bool showObj = true;
		bool showGrade = false;
		if (tabIndex == 3)
			showObj = false;
		if (tabIndex == 0)
			showGrade = true;

		if (m_Header)
		{
			m_Header.SetObjVisible(showObj);
			m_Header.SetGradeVisible(showGrade);
			if (showGrade)
				m_Header.SetScoreHeader("XP");
			else
				m_Header.SetScoreHeader("SCORE");
		}

		DetachLeaderboardCallbacks();
		ClearDataRows();

		if (tabIndex == 0)
		{
			IA_SessionRankManagerComponent session = IA_SessionRankManagerComponent.GetInstance();
			if (!session)
			{
				Print("[IA_StatisticsMenu] Could not find IA_SessionRankManagerComponent.", LogLevel.ERROR);
				return;
			}

			session.GetOnUpdated().Insert(this.PopulateSessionRank);
			PopulateSessionRank(session.GetCachedJson());
			return;
		}

		IA_LeaderboardManagerComponent manager = IA_LeaderboardManagerComponent.GetInstance();
		if (!manager)
		{
			Print("[IA_StatisticsMenu] Could not find IA_LeaderboardManagerComponent.", LogLevel.ERROR);
			return;
		}

		string cachedData = "";
		if (tabIndex == 1)
		{
			cachedData = manager.GetCachedServerLeaderboardData();
			manager.GetOnServerLeaderboardDataUpdated().Insert(this.PopulateLeaderboardMikes);
		}
		else if (tabIndex == 2)
		{
			cachedData = manager.GetCachedLeaderboardData();
			manager.GetOnLeaderboardDataUpdated().Insert(this.PopulateLeaderboardMikes);
		}
		else if (tabIndex == 3)
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
		IA_SessionRankManagerComponent session = IA_SessionRankManagerComponent.GetInstance();
		if (session)
			session.GetOnUpdated().Remove(this.PopulateSessionRank);

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
		if (m_iActiveTab == 3)
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
	void PopulateSessionRank(string jsonData)
	{
		if (!GetRuntime() || !m_Scroll)
			return;

		ClearDataRows();

		if (!jsonData || jsonData.IsEmpty() || jsonData == "[]")
			return;

		JsonLoadContext jsonContext = new JsonLoadContext();
		if (!jsonContext.LoadFromString(jsonData))
		{
			Print("[IA_StatisticsMenu] Failed to import session JSON.", LogLevel.ERROR);
			return;
		}

		ref array<ref IA_SessionRankEntry> playerStats = new array<ref IA_SessionRankEntry>();
		if (!jsonContext.ReadValue("", playerStats))
		{
			Print("[IA_StatisticsMenu] Failed to read session stats from JSON.", LogLevel.ERROR);
			return;
		}

		if (playerStats.IsEmpty())
			return;

		MUI_Runtime runtime = GetRuntime();
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
			IA_SessionRankEntry playerStat = playerStats[i];
			if (!playerStat)
				continue;

			ref IA_LeaderboardRow row = IA_LeaderboardRow.Create(runtime, "s" + i.ToString(), false);
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
			row.SetGradeVisible(true);
			row.SetGrade(IA_SessionRankLadder.GetShortName(playerStat.rankId));
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
