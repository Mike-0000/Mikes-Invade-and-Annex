//------------------------------------------------------------------------------------------------
class IA_StatisticsMenu : ChimeraMenuBase
{
	protected static const bool USE_MIKES_UI = true;

	protected Widget m_wRoot;
	protected ref IA_MikesMenuHost m_Host;
	protected ref MUI_Button m_TabServer;
	protected ref MUI_Button m_TabGlobal;
	protected ref MUI_Button m_TabGlobalServer;
	protected ref MUI_Row m_HeaderRow;
	protected ref MUI_ScrollView m_Scroll;
	protected ref array<ref IA_LeaderboardRow> m_aRows;
	protected int m_iActiveTab;

	// Legacy path
	SCR_InputButtonComponent m_backButton;
	private VerticalLayoutWidget m_LeaderboardContainer;
	private SCR_TabViewComponent m_TabView;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		m_wRoot = GetRootWidget();
		if (!m_wRoot)
			return;

		m_aRows = new array<ref IA_LeaderboardRow>();
		m_iActiveTab = 0;

		if (USE_MIKES_UI)
			OpenMikesUI();
		else
			OpenLegacy();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);
		if (!USE_MIKES_UI)
			return;
		if (m_Host)
			m_Host.Tick(tDelta);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuFocusLost()
	{
		super.OnMenuFocusLost();
		if (m_Host)
			m_Host.Blur();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		DetachLeaderboardCallbacks();

		if (m_Host)
		{
			m_Host.Close();
			m_Host = null;
		}

		if (m_TabView)
			m_TabView.GetOnChanged().Remove(OnTabChanged);

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenMikesUI()
	{
		m_Host = new IA_MikesMenuHost();
		if (!m_Host.Open(m_wRoot, "IA_StatisticsMenu"))
		{
			m_Host = null;
			OpenLegacy();
			return;
		}

		BuildStatsUI();
		m_Host.GetRuntime().GetOnBack().Insert(OnMikesClose);
		SelectTab(0);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildStatsUI()
	{
		MUI_Runtime runtime = m_Host.GetRuntime();

		ref MUI_Panel overlay = runtime.CreatePanel("overlay");
		overlay.MakeOverlay();
		overlay.GetStyle().m_Fill = Color.FromInt(0);
		overlay.SetIntro(0, 0.35, 0);

		ref MUI_FxBackdrop fx = runtime.CreateFxBackdrop("fx");
		fx.SetIntro(0, 0.55, 0);

		ref MUI_Card card = runtime.CreateCard("card");
		card.SetWidth(1100);
		card.SetPadding(28);
		card.GetStyle().m_fPadT = 22;
		card.SetGap(12);
		card.SetAlign(0.5, 0.5);
		card.SetIntro(0.06, 0.55, 46);

		ref MUI_LiveHeader header = runtime.CreateLiveHeader("LEADERBOARD", "header");
		header.SetKicker("COMMAND UPLINK  //  SECTOR IA");
		header.SetIntro(0.16, 0.4, 18);

		ref MUI_Label subtitle = runtime.CreateLabel("SERVER  •  GLOBAL  •  GLOBAL BY SERVER", "subtitle");
		subtitle.SetFontSize(MUI_Theme.FONT_SMALL);
		subtitle.SetMuted(true);
		subtitle.SetIntro(0.22, 0.4, 14);

		ref MUI_Hairline lineA = runtime.CreateHairline("lineA");
		lineA.SetIntro(0.26, 0.35, 8);

		ref MUI_Row tabs = runtime.CreateRow("tabs");
		tabs.SetGap(10);
		tabs.SetIntro(0.28, 0.4, 16);

		m_TabServer = runtime.CreateButton("Server", "tabServer");
		m_TabServer.SetIntro(0.30, 0.4, 12);
		m_TabServer.GetOnClicked().Insert(OnTabServerClicked);

		m_TabGlobal = runtime.CreateButton("Global", "tabGlobal");
		m_TabGlobal.SetIntro(0.32, 0.4, 12);
		m_TabGlobal.GetOnClicked().Insert(OnTabGlobalClicked);

		m_TabGlobalServer = runtime.CreateButton("Global by Server", "tabGlobalServer");
		m_TabGlobalServer.SetIntro(0.34, 0.4, 12);
		m_TabGlobalServer.GetOnClicked().Insert(OnTabGlobalServerClicked);

		tabs.AddChild(m_TabServer);
		tabs.AddChild(m_TabGlobal);
		tabs.AddChild(m_TabGlobalServer);

		ref IA_LeaderboardRow headerRow = IA_LeaderboardRow.Create(runtime, "hdr", true);
		m_HeaderRow = headerRow.GetRow();
		headerRow.SetObjVisible(true);
		m_HeaderRow.SetIntro(0.36, 0.35, 10);
		m_aRows.Insert(headerRow);

		m_Scroll = runtime.CreateScrollView("scroll");
		m_Scroll.SetViewportHeight(420);
		m_Scroll.SetGap(4);
		m_Scroll.SetIntro(0.38, 0.4, 16);

		ref MUI_Hairline lineB = runtime.CreateHairline("lineB");
		lineB.SetIntro(0.50, 0.35, 8);

		ref MUI_Row buttons = runtime.CreateRow("buttons");
		buttons.SetGap(12);
		buttons.SetIntro(0.52, 0.4, 18);

		ref MUI_Button closeBtn = runtime.CreateButton("Close", "close");
		closeBtn.SetIntro(0.54, 0.4, 14);
		closeBtn.GetOnClicked().Insert(OnMikesClose);
		buttons.AddChild(closeBtn);

		ref MUI_Label foot = runtime.CreateLabel("SET ./profile/MikesInvadeAndAnnex/server_name.txt FOR SERVER LEADERBOARD", "foot");
		foot.SetFontSize(MUI_Theme.FONT_SMALL);
		foot.SetMuted(true);
		foot.SetIntro(0.58, 0.4, 10);

		card.AddChild(header);
		card.AddChild(subtitle);
		card.AddChild(lineA);
		card.AddChild(tabs);
		card.AddChild(m_HeaderRow);
		card.AddChild(m_Scroll);
		card.AddChild(lineB);
		card.AddChild(buttons);
		card.AddChild(foot);

		overlay.AddChild(fx);
		overlay.AddChild(card);
		runtime.SetRoot(overlay);

		// Header row is index 0 in m_aRows — keep it for SetObjVisible
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTabServerClicked()
	{
		SelectTab(0);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTabGlobalClicked()
	{
		SelectTab(1);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTabGlobalServerClicked()
	{
		SelectTab(2);
	}

	//------------------------------------------------------------------------------------------------
	protected void SelectTab(int tabIndex)
	{
		m_iActiveTab = tabIndex;
		UpdateTabStyles();

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
	protected void UpdateTabStyles()
	{
		if (m_TabServer)
		{
			if (m_iActiveTab == 0)
				m_TabServer.MakeAccent();
			else
				m_TabServer.MakeDefault();
		}
		if (m_TabGlobal)
		{
			if (m_iActiveTab == 1)
				m_TabGlobal.MakeAccent();
			else
				m_TabGlobal.MakeDefault();
		}
		if (m_TabGlobalServer)
		{
			if (m_iActiveTab == 2)
				m_TabGlobalServer.MakeAccent();
			else
				m_TabGlobalServer.MakeDefault();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearDataRows()
	{
		if (m_Scroll)
			m_Scroll.ClearChildren();

		// Keep header at index 0; remove data rows
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
		manager.GetOnLeaderboardDataUpdated().Remove(this.PopulateLeaderboard);
		manager.GetOnServerLeaderboardDataUpdated().Remove(this.PopulateLeaderboard);
		manager.GetOnGlobalServerLeaderboardDataUpdated().Remove(this.PopulateLeaderboard);
	}

	//------------------------------------------------------------------------------------------------
	void PopulateLeaderboardMikes(string jsonData)
	{
		if (!m_Host || !m_Host.GetRuntime() || !m_Scroll)
			return;

		ClearDataRows();

		SCR_JsonLoadContext jsonContext = new SCR_JsonLoadContext();
		if (!jsonContext.ImportFromString(jsonData))
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

		MUI_Runtime runtime = m_Host.GetRuntime();
		bool showObj = true;
		if (m_iActiveTab == 2)
			showObj = false;

		int i;
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
			m_Scroll.AddChild(row.GetRow());
			m_aRows.Insert(row);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMikesClose()
	{
		Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenLegacy()
	{
		Widget rootWidget = m_wRoot;
		if (!rootWidget)
			return;

		Widget footer = rootWidget.FindAnyWidget("Footer");
		if (footer)
		{
			Widget button = footer.FindAnyWidget("Back");
			if (button)
			{
				m_backButton = SCR_InputButtonComponent.FindComponent(button);
				if (m_backButton)
					m_backButton.m_OnActivated.Insert(OnBack);
			}
		}

		Widget tabViewWidget = rootWidget.FindAnyWidget("TabView");
		if (tabViewWidget)
			m_TabView = SCR_TabViewComponent.Cast(tabViewWidget.FindHandler(SCR_TabViewComponent));

		if (m_TabView)
		{
			m_TabView.GetOnChanged().Insert(OnTabChanged);
			OnTabChanged(m_TabView, m_TabView.GetRootWidget(), m_TabView.GetShownTab());
		}
		else
		{
			Print("IA_StatisticsMenu::OnMenuOpen: ERROR: Could not find 'TabView' widget or its SCR_TabViewComponent.", LogLevel.ERROR);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnTabChanged(SCR_TabViewComponent tabView, Widget widget, int tabIndex)
	{
		if (!tabView)
			return;

		Widget tabRoot = tabView.GetContentWidget(tabIndex);
		if (!tabRoot)
			return;

		Widget timeSpentCappingHeader = tabRoot.FindAnyWidget("TimeSpentCapping");
		if (timeSpentCappingHeader)
		{
			if (tabIndex == 2)
				timeSpentCappingHeader.SetVisible(false);
			else
				timeSpentCappingHeader.SetVisible(true);
		}

		m_LeaderboardContainer = VerticalLayoutWidget.Cast(tabRoot.FindAnyWidget("LeaderboardContainer"));

		if (m_LeaderboardContainer)
		{
			IA_LeaderboardManagerComponent manager = IA_LeaderboardManagerComponent.GetInstance();
			if (!manager)
			{
				Print("IA_StatisticsMenu::OnTabChanged: Could not find IA_LeaderboardManagerComponent.", LogLevel.ERROR);
				return;
			}

			Widget child = m_LeaderboardContainer.GetChildren();
			while (child)
			{
				m_LeaderboardContainer.RemoveChild(child);
				child = m_LeaderboardContainer.GetChildren();
			}

			manager.GetOnLeaderboardDataUpdated().Remove(this.PopulateLeaderboard);
			manager.GetOnServerLeaderboardDataUpdated().Remove(this.PopulateLeaderboard);
			manager.GetOnGlobalServerLeaderboardDataUpdated().Remove(this.PopulateLeaderboard);

			if (tabIndex == 1)
			{
				string cachedData = manager.GetCachedLeaderboardData();
				if (cachedData && cachedData != "")
					PopulateLeaderboard(cachedData);
				manager.GetOnLeaderboardDataUpdated().Insert(this.PopulateLeaderboard);
			}
			else if (tabIndex == 0)
			{
				string cachedData = manager.GetCachedServerLeaderboardData();
				if (cachedData && cachedData != "")
					PopulateLeaderboard(cachedData);
				manager.GetOnServerLeaderboardDataUpdated().Insert(this.PopulateLeaderboard);
			}
			else if (tabIndex == 2)
			{
				string cachedData = manager.GetCachedGlobalServerLeaderboardData();
				if (cachedData && cachedData != "")
					PopulateLeaderboard(cachedData);
				manager.GetOnGlobalServerLeaderboardDataUpdated().Insert(this.PopulateLeaderboard);
			}
		}
		else
		{
			Print("IA_StatisticsMenu::OnTabChanged: Could not find LeaderboardContainer widget in the selected tab.", LogLevel.ERROR);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnBack()
	{
		Close();
	}

	//------------------------------------------------------------------------------------------------
	void PopulateLeaderboard(string jsonData)
	{
		if (!m_TabView)
			return;

		Widget tabRoot = m_TabView.GetContentWidget(m_TabView.GetShownTab());
		if (!tabRoot)
			return;

		m_LeaderboardContainer = VerticalLayoutWidget.Cast(tabRoot.FindAnyWidget("LeaderboardContainer"));
		if (!m_LeaderboardContainer)
			return;

		while (m_LeaderboardContainer.GetChildren())
			m_LeaderboardContainer.RemoveChild(m_LeaderboardContainer.GetChildren());

		SCR_JsonLoadContext jsonContext = new SCR_JsonLoadContext();
		if (!jsonContext.ImportFromString(jsonData))
			return;

		ref array<ref IA_PlayerStatEntry> playerStats = new array<ref IA_PlayerStatEntry>();
		if (!jsonContext.ReadValue("", playerStats))
			return;

		if (playerStats.IsEmpty())
			return;

		int i;
		for (i = 0; i < playerStats.Count(); i++)
		{
			IA_PlayerStatEntry playerStat = playerStats[i];
			if (!playerStat)
				continue;

			Widget newWidget = GetGame().GetWorkspace().CreateWidgets("{87E261D76A82FA5A}UI/IA_Player_Entry.layout", m_LeaderboardContainer);
			if (!newWidget)
				continue;

			IA_PlayerLeaderboardEntry entryController = IA_PlayerLeaderboardEntry.Cast(newWidget.FindHandler(IA_PlayerLeaderboardEntry));
			if (entryController)
			{
				entryController.SetRank((i + 1).ToString() + ".");
				entryController.SetPlayerName(playerStat.PlayerName);
				entryController.SetKills(playerStat.kills.ToString());
				entryController.SetDeaths(playerStat.deaths.ToString());
				entryController.SetHVTKills(playerStat.hvt_kills.ToString());
				entryController.SetHVTGuardKills(playerStat.hvt_guard_kills.ToString());
				entryController.SetOBJScore(playerStat.obj_score.ToString());
				entryController.SetScore(playerStat.score.ToString());

				if (m_TabView.GetShownTab() == 2)
					entryController.SetOBJScoreVisible(false);
				else
					entryController.SetOBJScoreVisible(true);
			}
		}
	}
};
