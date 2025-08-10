// IA_SideObjectiveManager.c

class IA_SideObjectiveManager
{
    private static ref IA_SideObjectiveManager s_Instance;
    protected ref array<ref IA_SideObjective> m_ActiveObjectives;
    
    protected bool m_IsOnCooldown = false;
    protected const float OBJECTIVE_COOLDOWN_SECONDS = 900; // 15 minutes
    
    // --- BEGIN ADDED: Startup delay ---
    protected bool m_IsInitialized = false;
    protected const float STARTUP_DELAY_SECONDS = 240; // 5 minutes delay after mission start
    // --- END ADDED ---
    
    // To avoid picking the same marker repeatedly
    protected ref array<IA_SideObjectiveMarker> m_AvailableMarkers;

    static IA_SideObjectiveManager GetInstance()
    {
        if (!s_Instance)
        {
            s_Instance = new IA_SideObjectiveManager();
        }
        return s_Instance;
    }

    void IA_SideObjectiveManager()
    {
        m_ActiveObjectives = new array<ref IA_SideObjective>();
        m_AvailableMarkers = new array<IA_SideObjectiveMarker>();
        
        // --- BEGIN ADDED: Schedule initialization ---
        Print(string.Format("[IA_SideObjectiveManager] Scheduling startup in %1 seconds", STARTUP_DELAY_SECONDS), LogLevel.NORMAL);
        GetGame().GetCallqueue().CallLater(Initialize, STARTUP_DELAY_SECONDS * 1000, false);
        // --- END ADDED ---
    }
    
    // --- BEGIN ADDED: Initialize method ---
    protected void Initialize()
    {
        m_IsInitialized = true;
        Print("[IA_SideObjectiveManager] Side objective system initialized. Ready to spawn objectives.", LogLevel.NORMAL);
    }
    // --- END ADDED ---

    void Update(float timeSlice)
    {
        // --- BEGIN ADDED: Check if initialized ---
        if (!m_IsInitialized)
        {
            return; // Don't process anything until initialization delay has passed
        }
        // --- END ADDED ---
        
		bool objectiveJustFinished = false;
		
        // Update active objectives
        for (int i = m_ActiveObjectives.Count() - 1; i >= 0; i--)
        {
            IA_SideObjective obj = m_ActiveObjectives[i];
            obj.Update(timeSlice);
            if (obj.GetState() == IA_SideObjectiveState.Completed || obj.GetState() == IA_SideObjectiveState.Failed)
            {
                if (obj.GetState() == IA_SideObjectiveState.Completed)
                {
                    // Call the new static method to disable artillery for 30 mins
                    IA_MissionInitializer.SetArtilleryDisabled(1800);
                    // Also disable QRF for 30 minutes
                    IA_MissionInitializer.SetQRFDisabled(1800);
                }
                m_ActiveObjectives.Remove(i);
				objectiveJustFinished = true;
            }
        }
		
		// If an objective was just completed, start the cooldown
        if (objectiveJustFinished)
        {
            StartCooldown();
        }

        // If we are not on cooldown and have no active objectives, try to start a new one.
        if (!m_IsOnCooldown && m_ActiveObjectives.IsEmpty())
        {
            TryStartNewSideObjective();
        }
    }

	protected void StartCooldown()
    {
        Print("Side objective cooldown started. No new objectives for 10 minutes.", LogLevel.NORMAL);
        m_IsOnCooldown = true;
        GetGame().GetCallqueue().CallLater(EndCooldown, OBJECTIVE_COOLDOWN_SECONDS * 1000, false);
    }

    void EndCooldown()
    {
        Print("Side objective cooldown finished. New objectives can now be started.", LogLevel.NORMAL);
        m_IsOnCooldown = false;
    }
	
    void TryStartNewSideObjective()
    {
        // If the list of available markers is empty, repopulate it.
        if (m_AvailableMarkers.IsEmpty())
        {
            m_AvailableMarkers.Copy(IA_SideObjectiveMarker.GetAllMarkers());
            if (m_AvailableMarkers.IsEmpty())
            {
                Print("No side objective markers found in the world.", LogLevel.WARNING);
                return;
            }
            Print(string.Format("Repopulated available side objective markers. Count: %1", m_AvailableMarkers.Count()), LogLevel.NORMAL);
        }

        // Select a random marker from the available list and then remove it to prevent re-selection.
        int randomIndex = Math.RandomInt(0, m_AvailableMarkers.Count());
        IA_SideObjectiveMarker selectedMarker = m_AvailableMarkers[randomIndex];
        m_AvailableMarkers.Remove(randomIndex);
		
        if (!selectedMarker) return;
		
		IA_MissionInitializer initializer = IA_MissionInitializer.GetInstance();
		if (!initializer) return;
		
		Faction enemyGameFaction = initializer.GetRandomEnemyFaction();
		if (!enemyGameFaction) return;

		IA_Faction enemyIAFaction = IA_Faction.USSR; // fallback
		string factionKey = enemyGameFaction.GetFactionKey();
		if (factionKey == "US") enemyIAFaction = IA_Faction.US;
		else if (factionKey == "USSR") enemyIAFaction = IA_Faction.USSR;
		else if (factionKey == "FIA") enemyIAFaction = IA_Faction.FIA;
		
        IA_SideObjective newObjective;

        switch(selectedMarker.GetObjectiveType())
        {
            case IA_SideObjectiveType.Assassination:
                newObjective = new IA_AssassinationObjective(selectedMarker, enemyIAFaction, enemyGameFaction);
                break;
        }

        if (newObjective)
        {
            newObjective.Start();
            m_ActiveObjectives.Insert(newObjective);
        }
    }
} 