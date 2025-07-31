class IA_StatisticsMenu : ChimeraMenuBase
{
	SCR_InputButtonComponent m_backButton;
    private VerticalLayoutWidget m_LeaderboardContainer;
    private SCR_TabViewComponent m_TabView;
	
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		
		Widget rootWidget = GetRootWidget();
		if (!rootWidget)
		{
			Print("IA_StatisticsMenu::OnMenuOpen: Could not get root widget", LogLevel.NORMAL);
			return;
		}
		
		Widget footer = rootWidget.FindAnyWidget("Footer");
		if (footer)
		{
		Widget button = footer.FindAnyWidget("Back");
			if (button)
		{
		m_backButton = SCR_InputButtonComponent.FindComponent(button);
		if (m_backButton)
		{
			m_backButton.m_OnActivated.Insert(OnBack);
				}
			}
		}

        Widget tabViewWidget = rootWidget.FindAnyWidget("TabView");
        if (tabViewWidget)
        {
            m_TabView = SCR_TabViewComponent.Cast(tabViewWidget.FindHandler(SCR_TabViewComponent));
        }

        if (m_TabView)
        {
            m_TabView.GetOnChanged().Insert(OnTabChanged);
            // Manually trigger for the initially selected tab
            OnTabChanged(m_TabView, m_TabView.GetRootWidget(), m_TabView.GetShownTab());
        }
        else
        {
            Print("IA_StatisticsMenu::OnMenuOpen: ERROR: Could not find 'TabView' widget or its SCR_TabViewComponent.", LogLevel.ERROR);
        }
	}
	
	void OnTabChanged(SCR_TabViewComponent tabView, Widget widget, int tabIndex)
    {
        if (!tabView) return;

        Widget tabRoot = tabView.GetContentWidget(tabIndex);
        if (!tabRoot) return;

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

            // Always clear the existing leaderboard before populating
            Widget child = m_LeaderboardContainer.GetChildren();
            while (child)
            {
                m_LeaderboardContainer.RemoveChild(child);
                child = m_LeaderboardContainer.GetChildren();
            }

            // Detach old callbacks to prevent multiple triggers
            manager.GetOnLeaderboardDataUpdated().Remove(this.PopulateLeaderboard);
            manager.GetOnServerLeaderboardDataUpdated().Remove(this.PopulateLeaderboard);
			manager.GetOnGlobalServerLeaderboardDataUpdated().Remove(this.PopulateLeaderboard);

            // Tab 1: Global Leaderboard
            if (tabIndex == 1)
            {
                Print("IA_StatisticsMenu::OnTabChanged: Global Leaderboard tab selected.", LogLevel.NORMAL);
                string cachedData = manager.GetCachedLeaderboardData();
                if (cachedData && cachedData != "")
                {
                    PopulateLeaderboard(cachedData);
                }
                manager.GetOnLeaderboardDataUpdated().Insert(this.PopulateLeaderboard);
            }
            // Tab 0: Server Leaderboard
            else if (tabIndex == 0)
            {
                Print("IA_StatisticsMenu::OnTabChanged: Server Leaderboard tab selected.", LogLevel.NORMAL);
                string cachedData = manager.GetCachedServerLeaderboardData();
                if (cachedData && cachedData != "")
                {
                    PopulateLeaderboard(cachedData);
                }
                manager.GetOnServerLeaderboardDataUpdated().Insert(this.PopulateLeaderboard);
            }
			// Tab 2: Global Server Leaderboard
			else if (tabIndex == 2)
			{
				Print("IA_StatisticsMenu::OnTabChanged: Global Server Leaderboard tab selected.", LogLevel.NORMAL);
                string cachedData = manager.GetCachedGlobalServerLeaderboardData();
                if (cachedData && cachedData != "")
                {
                    PopulateLeaderboard(cachedData);
                }
                manager.GetOnGlobalServerLeaderboardDataUpdated().Insert(this.PopulateLeaderboard);
			}
        }
        else
        {
            Print("IA_StatisticsMenu::OnTabChanged: Could not find LeaderboardContainer widget in the selected tab.", LogLevel.ERROR);
        }
	}
	
	override void OnMenuClose()
	{
		super.OnMenuClose();
        IA_LeaderboardManagerComponent manager = IA_LeaderboardManagerComponent.GetInstance();
        if (manager)
        {
            manager.GetOnLeaderboardDataUpdated().Remove(this.PopulateLeaderboard);
			manager.GetOnServerLeaderboardDataUpdated().Remove(this.PopulateLeaderboard);
			manager.GetOnGlobalServerLeaderboardDataUpdated().Remove(this.PopulateLeaderboard);
        }
		
		if (m_TabView)
        {
            m_TabView.GetOnChanged().Remove(OnTabChanged);
        }
	}
	
	void OnBack()
	{
		Print("IA_StatisticsMenu::OnBack: Closing menu", LogLevel.NORMAL);
		Close();
	}
    
    void PopulateLeaderboard(string jsonData)
    {
		Print("IA_StatisticsMenu::PopulateLeaderboard: Method invoked.", LogLevel.WARNING);
		
		if (!m_TabView)
		{
			Print("IA_StatisticsMenu::PopulateLeaderboard: TabView is null, cannot proceed.", LogLevel.ERROR);
			return;
		}
		
		Widget tabRoot = m_TabView.GetContentWidget(m_TabView.GetShownTab());
		if (!tabRoot)
		{
			Print("IA_StatisticsMenu::PopulateLeaderboard: tabRoot for current tab is null, cannot proceed.", LogLevel.ERROR);
			return;
		}

		// Re-acquire the leaderboard container to ensure it's valid
		m_LeaderboardContainer = VerticalLayoutWidget.Cast(tabRoot.FindAnyWidget("LeaderboardContainer"));
		
        if (!m_LeaderboardContainer)
        {
            Print("IA_StatisticsMenu::PopulateLeaderboard: LeaderboardContainer is null after re-acquiring, cannot proceed.", LogLevel.ERROR);
            return;
        }

        Print("IA_StatisticsMenu::PopulateLeaderboard: Received JSON data: " + jsonData, LogLevel.NORMAL);

        // Clear existing entries
		Print("IA_StatisticsMenu::PopulateLeaderboard: Clearing existing leaderboard entries...", LogLevel.NORMAL);
        Widget child = m_LeaderboardContainer.GetChildren();
		int childrenCount = 0;
        while (child)
        {
			childrenCount++;
            child = child.GetSibling();
        }
		Print(string.Format("IA_StatisticsMenu::PopulateLeaderboard: Found %1 child widgets to clear.", childrenCount), LogLevel.NORMAL);
		
        while (m_LeaderboardContainer.GetChildren())
        {
            m_LeaderboardContainer.RemoveChild(m_LeaderboardContainer.GetChildren());
        }
		
		Print("IA_StatisticsMenu::PopulateLeaderboard: Finished clearing entries.", LogLevel.NORMAL);

        SCR_JsonLoadContext jsonContext = new SCR_JsonLoadContext();
        if (!jsonContext.ImportFromString(jsonData))
        {
            Print("IA_StatisticsMenu::PopulateLeaderboard: ERROR: Failed to import JSON from string.", LogLevel.ERROR);
            return;
        }
        Print("IA_StatisticsMenu::PopulateLeaderboard: Successfully imported JSON from string.", LogLevel.NORMAL);

        ref array<ref IA_PlayerStatEntry> playerStats = new array<ref IA_PlayerStatEntry>();
        if (!jsonContext.ReadValue("", playerStats))
        {
            Print("IA_StatisticsMenu::PopulateLeaderboard: ERROR: Failed to read player stats from JSON. This might mean the JSON is not an array at the root, or the structure is incorrect.", LogLevel.ERROR);
            return;
        }
        Print("IA_StatisticsMenu::PopulateLeaderboard: Successfully read value into playerStats array.", LogLevel.NORMAL);

        Print("IA_StatisticsMenu::PopulateLeaderboard: Found " + playerStats.Count() + " player entries in the JSON data.", LogLevel.NORMAL);

        if (playerStats.IsEmpty())
        {
            Print("IA_StatisticsMenu::PopulateLeaderboard: The player stats array is empty. No widgets will be created.", LogLevel.WARNING);
            return;
        }

        for (int i = 0; i < playerStats.Count(); i++)
        {
            IA_PlayerStatEntry playerStat = playerStats[i];
            if (!playerStat)
            {
                Print("IA_StatisticsMenu::PopulateLeaderboard: Entry at index " + i + " is null.", LogLevel.WARNING);
                continue;
            }

            //Print(string.Format("IA_StatisticsMenu::PopulateLeaderboard: Processing entry #%1: PlayerName=%2, Kills=%3, Deaths=%4", (i + 1), playerStat.PlayerName, playerStat.kills, playerStat.deaths), LogLevel.NORMAL);
            
            Widget newWidget = GetGame().GetWorkspace().CreateWidgets("{87E261D76A82FA5A}UI/IA_Player_Entry.layout", m_LeaderboardContainer);
			if (!newWidget)
			{
				Print("IA_StatisticsMenu::PopulateLeaderboard: ERROR: Failed to create widget from layout {87E261D76A82FA5A}UI/IA_Player_Entry.layout", LogLevel.ERROR);
				continue;
			}

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
                //Print("IA_StatisticsMenu::PopulateLeaderboard: Successfully created and set data for widget for player " + playerStat.PlayerName, LogLevel.NORMAL);
            }
            else
            {
                Print("IA_StatisticsMenu::PopulateLeaderboard: ERROR: Failed to find controller for player entry " + (i + 1) + ". Check the layout path and controller setup.", LogLevel.ERROR);
            }
        }
		
		Print("IA_StatisticsMenu::PopulateLeaderboard: Finished populating leaderboard.", LogLevel.NORMAL);
	}
}; 