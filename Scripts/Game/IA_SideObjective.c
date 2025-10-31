// IA_SideObjective.c

enum IA_SideObjectiveType
{
    Assassination
    // Future side objectives can be added here
    // e.g., Sabotage, Rescue
}

enum IA_SideObjectiveState
{
    Inactive,
    Active,
    Completed,
    Failed
}

// --- BEGIN ADDED: Callback for HVT Marker Query ---
class HVTMarkerQueryCallback
{
    protected ref array<IA_HVTSpawnPositionMarker> m_FoundMarkers;

    void HVTMarkerQueryCallback(notnull out array<IA_HVTSpawnPositionMarker> markers)
    {
        m_FoundMarkers = markers;
    }

    bool OnEntityFound(IEntity entity)
    {
        IA_HVTSpawnPositionMarker marker = IA_HVTSpawnPositionMarker.Cast(entity);
        if (marker)
        {
            m_FoundMarkers.Insert(marker);
        }
        return true; // Continue searching
    }
}
// --- END ADDED ---

// --- BEGIN ADDITION: Callback for Escape Vehicle Marker Query ---
class EscapeVehicleMarkerQueryCallback
{
    protected ref array<IA_EscapeVehiclePositionMarker> m_FoundMarkers;

    void EscapeVehicleMarkerQueryCallback(notnull out array<IA_EscapeVehiclePositionMarker> markers)
    {
        m_FoundMarkers = markers;
    }

    bool OnEntityFound(IEntity entity)
    {
        IA_EscapeVehiclePositionMarker marker = IA_EscapeVehiclePositionMarker.Cast(entity);
        if (marker)
        {
            m_FoundMarkers.Insert(marker);
        }
        return true; // Continue searching
    }
}
// --- END ADDITION ---

// Base class for all side objectives
class IA_SideObjective
{
	protected const int OBJECTIVE_COOLDOWN_MILLASECONDS = 900000; // 15 minutes

    protected IA_SideObjectiveType m_Type;
    protected IA_SideObjectiveState m_State;
    protected vector m_Position;
    protected string m_Title;
    protected string m_Description;
    protected IEntity m_PlayerTask;

    void IA_SideObjective()
    {
        // Parameter-less constructor
    }

    void Init(IA_SideObjectiveType type, vector position)
    {
        m_Type = type;
        m_State = IA_SideObjectiveState.Inactive;
        m_Position = position;
    }

    void Start()
    {
        // Activate the side-objective. Sub-classes perform their own initialization in their constructor,
        // so the only mandatory step here is to toggle the state flag so that callbacks (e.g. OnHVTKilled)
        // recognise the objective as active.
        if (m_State != IA_SideObjectiveState.Inactive)
            return;

        m_State = IA_SideObjectiveState.Active;
        Print(string.Format("[IA_SideObjective] Objective '%1' started at %2", m_Title, m_Position.ToString()), LogLevel.DEBUG);
    }

    void Update(float timeSlice)
    {
        // Default no-op implementations so that subclasses only override what they need.
    }

    void Complete()
    {
        // Default no-op implementations so that subclasses only override what they need.
    }

    void Fail()
    {
        // Default no-op implementations so that subclasses only override what they need.
    }
    
    // Helper function to ensure task has required child entity
    protected void EnsureTaskHasChildEntity(IEntity taskEntity)
    {
        if (!taskEntity)
            return;
            
        // Check if child entity already exists
        // GetChildren() now returns a single IEntity (first child), so we iterate through children using GetSibling()
        bool hasChild = false;
        IEntity child = taskEntity.GetChildren();
        while (child)
        {
            SCR_BaseFactionTriggerEntity factionTrigger = SCR_BaseFactionTriggerEntity.Cast(child);
            if (factionTrigger)
            {
                hasChild = true;
                break;
            }
            child = child.GetSibling();
        }
        
        // If no child exists, log a warning
        if (!hasChild)
        {
            Print("[WARNING] IA_SideObjective: Task entity missing required SCR_BaseFactionTriggerEntity child. The prefab may need to be updated.", LogLevel.WARNING);
        }
    }
    
    protected IEntity _CreateTaskAtPosition(string title, string desc, vector pos)
    {
        Resource res = Resource.Load("{33DA4D098C409421}Prefabs/Tasks/TriggerTask.et");
        if (!res) return null;

        IEntity taskEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(pos));
        if (!taskEnt) return null;

        // Ensure required child entity exists
        EnsureTaskHasChildEntity(taskEnt);

        SCR_TriggerTask task = SCR_TriggerTask.Cast(taskEnt);
        if (!task)
        {
            IA_Game.AddEntityToGc(taskEnt);
            return null;
        }
        
        SCR_ExtendedTask extendedTask = SCR_ExtendedTask.Cast(task);
        if (extendedTask)
        {
            extendedTask.SetTaskName("Side Objective: " + title);
            extendedTask.SetTaskDescription(desc);
            extendedTask.SetTaskState(SCR_ETaskState.CREATED);
        }
        
        // Use a global notification helper to inform players
        IA_Game.S_TriggerGlobalNotification("SideTaskCreated", title);
        
        return task;
    }
    
    void CreatePlayerTask(string title, string desc)
    {
		m_PlayerTask = _CreateTaskAtPosition(title, desc, m_Position);
    }
    
    void CompletePlayerTask()
    {
        if (m_PlayerTask)
        {
            SCR_ExtendedTask extendedTask = SCR_ExtendedTask.Cast(m_PlayerTask);
            if (extendedTask)
                extendedTask.SetTaskState(SCR_ETaskState.COMPLETED);
            IA_Game.S_TriggerGlobalNotification("SideTaskCompleted", "Side Objective Completed. The HVT has been eliminated. Enemy artillery and QRF is unavailable for 30 minutes.");
        }
    }

    IA_SideObjectiveState GetState() { return m_State; }

    protected vector _FindHVTSpawnPosition()
    {
        array<IA_HVTSpawnPositionMarker> hvtMarkers = {};
        HVTMarkerQueryCallback callback = new HVTMarkerQueryCallback(hvtMarkers);
        GetGame().GetWorld().QueryEntitiesBySphere(m_Position, 150.0, callback.OnEntityFound);
        
        if (hvtMarkers.IsEmpty())
        {
            return vector.Zero;
        }
        
        int randomIndex = Math.RandomInt(0, hvtMarkers.Count());
        IA_HVTSpawnPositionMarker selectedMarker = hvtMarkers[randomIndex];
        
        if (selectedMarker)
        {
            return selectedMarker.GetOrigin();
        }
        
        return vector.Zero;
    }

    protected vector _FindEscapeVehicleSpawnPosition()
    {
        array<IA_EscapeVehiclePositionMarker> escapeMarkers = {};
        EscapeVehicleMarkerQueryCallback callback = new EscapeVehicleMarkerQueryCallback(escapeMarkers);
        // Search using a slightly larger radius than HVT to cover wider area
        GetGame().GetWorld().QueryEntitiesBySphere(m_Position, 300.0, callback.OnEntityFound);

        if (escapeMarkers.IsEmpty())
            return vector.Zero;

        int randomIndex = Math.RandomInt(0, escapeMarkers.Count());
        IA_EscapeVehiclePositionMarker selectedMarker = escapeMarkers[randomIndex];

        if (selectedMarker)
            return selectedMarker.GetOrigin();

        return vector.Zero;
    }
}

// Assassination objective implementation
class IA_AssassinationObjective : IA_SideObjective
{
    protected IEntity m_HVT;
	protected bool m_bHVTSpawned = false;
    protected ref array<ref IA_AiGroup> m_ObjectiveGroups = {}; // Renamed from m_Guards, will hold all objective AI
    protected IA_Faction m_EnemyIAFaction;
    protected Faction m_EnemyGameFaction;
    protected ref IA_AreaInstance m_ObjectiveAreaInstance; // The private area instance for this objective
    protected ref IA_Area m_TransientArea; // The transient area definition
    
    // Generator-related members
    protected IEntity m_Generator;
    protected IEntity m_GeneratorTask;
    protected bool m_GeneratorDestroyed = false;
    protected IA_SideObjectiveMarker m_Marker;
	protected vector m_HVTSpawnPosition;

    protected IEntity m_EscapeVehicle; // Newly spawned escape vehicle entity
    protected bool m_EscapeCountdownActive = false;
    protected int m_EscapeCountdownEndTime = 0;
    protected vector m_EscapePoint = vector.Zero;
    protected IEntity m_EscapeTask;
    protected IA_AiGroup m_HVTGroup;
    protected bool m_EscapeTaskCreated = false;
    protected bool m_EscapeBehaviorTriggered = false;
    // Track whether we have already turned on reinforcement waves for this objective
    protected bool m_DefenseWavesActive = false;
	
	// --- BEGIN ADDED: HVT final escape timer ---
    protected bool m_bHVTEscaping = false;
    protected int m_iHVTEscapeTimeEnd = 0;
    // --- END ADDED ---

    static void S_CleanupAssassinationObjective(IA_AreaInstance objectiveAreaInstance)
    {
        if (!objectiveAreaInstance)
            return;
            
        IA_Game.Instantiate().RemoveTransientArea(objectiveAreaInstance);
        objectiveAreaInstance.Cleanup();
    }

    void IA_AssassinationObjective(IA_SideObjectiveMarker marker, IA_Faction enemyIAFaction, Faction enemyGameFaction)
    {
        Init(IA_SideObjectiveType.Assassination, marker.GetOrigin()); // Initialize parent properties

        m_Title = "Assassinate HVT";
        m_Description = "An enemy High-Value Target has been located. Eliminate them.";
        m_EnemyIAFaction = enemyIAFaction;
        m_EnemyGameFaction = enemyGameFaction;
        m_Marker = marker;
		m_HVTSpawnPosition = vector.Zero;
        m_EscapeVehicle = null;
		m_EscapeBehaviorTriggered = false;

        // --- Determine HVT spawn position ---
        vector hvtSpawnPos = _FindHVTSpawnPosition();
        if (hvtSpawnPos == vector.Zero)
        {
            // Fallback: spawn at main marker position if no dedicated spawn marker found
            hvtSpawnPos = m_Position;
        }
        // Store for later reference
        m_HVTSpawnPosition = hvtSpawnPos;

        vector generatorSpawnPos;
        bool shouldSpawnGenerator = (m_Marker && m_Marker.GetGeneratorPositionOffset());
		PointInfo genPosInfo;
        if (shouldSpawnGenerator)
        {
              genPosInfo = m_Marker.GetGeneratorPositionOffset();
            vector genMat[4];
            genPosInfo.GetTransform(genMat);          // Retrieve full local transform matrix
            vector genOffset = genMat[3];             // Translation component gives the offset vector
            generatorSpawnPos = m_Position + genOffset; // Apply offset relative to main marker
        }

        // 3. Transient Area Center: Centered on the HVT
        vector areaCenter = hvtSpawnPos;
        
        // --- NEW: Escape Vehicle Spawn Position ---
        vector escapeVehicleSpawnPos = _FindEscapeVehicleSpawnPosition();
        if (escapeVehicleSpawnPos != vector.Zero)
        {
            SpawnEscapeVehicleAt(escapeVehicleSpawnPos);
        }

        // --- Now perform actions with the calculated positions ---
        Print(string.Format("Starting Assassination objective. Area centered at %1", areaCenter.ToString()), LogLevel.NORMAL);
		
        // Create a transient area and a private area instance for this objective's AI
        string areaName = "SideObjective_Assassination_" + Math.RandomInt(0, 100000);
        m_TransientArea = IA_Area.CreateTransient(areaName, IA_AreaType.Assassination, areaCenter, 110);
        
		if (m_TransientArea)
             Print(string.Format("[IA_AssassinationObjective] Successfully created transient area: %1", m_TransientArea.GetName()), LogLevel.NORMAL);
        else
             Print(string.Format("[IA_AssassinationObjective] FAILED to create transient area."), LogLevel.ERROR);
			 
        m_ObjectiveAreaInstance = IA_AreaInstance.Create(m_TransientArea, m_EnemyIAFaction, m_EnemyGameFaction, 0, -1);
        
        if (m_ObjectiveAreaInstance)
		{
             Print(string.Format("[IA_AssassinationObjective] Created private AreaInstance for objective."), LogLevel.NORMAL);
			 IA_Game.Instantiate().AddTransientArea(m_ObjectiveAreaInstance);
		}
        else
             Print(string.Format("[IA_AssassinationObjective] FAILED to create private AreaInstance."), LogLevel.ERROR);

        // Spawn entities at their specific locations
        if (shouldSpawnGenerator)
        {
            // --- Spawn generator with full transform (offset + orientation) ---
            vector localGenMat[4];
            genPosInfo.GetTransform(localGenMat);

            // 2) World transform of the marker itself
            vector markerMat[4];
            m_Marker.GetTransform(markerMat);

            // 3) Combine -> world transform for the generator (marker * local)
            vector worldGenMat[4];
            Math3D.MatrixMultiply4(markerMat, localGenMat, worldGenMat);

            // 4) Build spawn params with full matrix
            EntitySpawnParams genParams = new EntitySpawnParams();
            genParams.TransformMode = ETransformMode.WORLD;
            genParams.Transform[0] = worldGenMat[0];
            genParams.Transform[1] = worldGenMat[1];
            genParams.Transform[2] = worldGenMat[2];
            genParams.Transform[3] = worldGenMat[3];

            // 5) Load resource and spawn
            Resource genResource = Resource.Load("{ADE488DB19F05050}Prefabs/IA_Generator1.et");
            if (genResource)
            {
                m_Generator = GetGame().SpawnEntityPrefab(genResource, null, genParams);

                if (m_Generator)
                {
                    Print(string.Format("[IA_AssassinationObjective] Spawned generator at %1 with orientation", worldGenMat[3].ToString()), LogLevel.NORMAL);

                    // Attach damage listener
                    SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.Cast(m_Generator.FindComponent(SCR_DamageManagerComponent));
                    if (dmg)
                        dmg.GetOnDamageStateChanged().Insert(OnGeneratorDamageStateChanged);

                    // Optional task to destroy generator
                    string title = "Destroy Generator (Optional)";
                    string desc  = "Destroy the enemy generator to prevent them from calling reinforcements.";
                    m_GeneratorTask = _CreateTaskAtPosition(title, desc, worldGenMat[3]);
                }
                else
                {
                    Print("[IA_AssassinationObjective] FAILED to spawn generator entity", LogLevel.ERROR);
                }
            }
            else
            {
                Print("[IA_AssassinationObjective] Failed to load generator resource!", LogLevel.ERROR);
            }
        }
        SpawnHVTAndGuardsAt(hvtSpawnPos);
        
        // Create player task pointing to the main objective marker, not the hidden HVT
        CreatePlayerTask(m_Title, m_Description);
    }

    protected void SpawnHVTAndGuardsAt(vector hvtSpawnPos)
    {
        // Spawn HVT
        IA_AiGroup hvtGroup = IA_AiGroup.CreateMilitaryGroupFromUnits(hvtSpawnPos, m_EnemyIAFaction, 1, m_EnemyGameFaction, true);
        if (hvtGroup)
        {
            hvtGroup.SetOwningSideObjective(this);
            
            // Spawn first with no initial order
            hvtGroup.Spawn();
            
            // Then immediately set tactical state to Neutral to override any default behavior
            hvtGroup.SetTacticalState(IA_GroupTacticalState.Neutral, hvtSpawnPos, null, true);
            hvtGroup.RemoveAllOrders(true); // Ensure no default orders remain
            
            if (m_ObjectiveAreaInstance) m_ObjectiveAreaInstance.AddMilitaryGroup(hvtGroup);

            m_HVTGroup = hvtGroup; // store reference for later escape orders

            // Give the defend order after a short delay
            GetGame().GetCallqueue().CallLater(GiveHVTDefendOrder, 3000, false, hvtGroup, hvtSpawnPos);

            array<SCR_ChimeraCharacter> chars = hvtGroup.GetGroupCharacters();
            if (!chars.IsEmpty())
            {
                m_HVT = chars[0];
				m_bHVTSpawned = true;
                
                SCR_CharacterControllerComponent ccc = SCR_CharacterControllerComponent.Cast(m_HVT.FindComponent(SCR_CharacterControllerComponent));
                if (ccc)
                {
                    ccc.GetOnPlayerDeathWithParam().Insert(OnHVTKilled);
                }
            }
        }

        // Spawn guards
        for (int i = 0; i < 5; i++)
        {
            vector guardPos = hvtSpawnPos + IA_Game.rng.GenerateRandomPointInRadius(10, 25, vector.Zero);
            IA_AiGroup guardGroup = IA_AiGroup.CreateMilitaryGroupFromUnits(guardPos, m_EnemyIAFaction, 4, m_EnemyGameFaction);
            if (guardGroup)
            {
                guardGroup.SetOwningSideObjective(this);
                
                // Spawn first with no initial order
                guardGroup.Spawn();
                
                // Then immediately set tactical state to Neutral to override any default behavior
                guardGroup.SetTacticalState(IA_GroupTacticalState.Neutral, guardPos, null, true);
                guardGroup.RemoveAllOrders(true); // Ensure no default orders remain
                
                if (m_ObjectiveAreaInstance) m_ObjectiveAreaInstance.AddMilitaryGroup(guardGroup);
				m_ObjectiveGroups.Insert(guardGroup);
				
				// Give the defend order after a short delay, parented to the HVT
                GetGame().GetCallqueue().CallLater(GiveGuardDefendOrder, 3100, false, guardGroup, m_HVT);
            }
        }
    }
    
    protected void GiveHVTDefendOrder(IA_AiGroup hvtGroup, vector defendPos)
    {
        if (hvtGroup && hvtGroup.IsSpawned() && hvtGroup.GetAliveCount() > 0)
        {
            // Use the standard order system for consistency with regular troops
            hvtGroup.SetTacticalState(IA_GroupTacticalState.Neutral, defendPos, null, true);
            
            // Optional: If you need extra priority for HVT, you can add a high-priority defend order
            // This will be processed through the standard AddOrder logic with all its checks
            hvtGroup.RemoveAllOrders(true); // Clear any default orders first
			
			ResourceName rname = "{2C442840ED2B495D}Prefabs/AI/Waypoints/AIWaypoint_DefendSmallHVT.et";
            Resource res = Resource.Load(rname);
			IEntity waypointEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(defendPos));
        	SCR_AIWaypoint w = null; // Initialize w to null
			w = SCR_AIWaypoint.Cast(waypointEnt);
            if(!w)
				return;
			w.SetPriorityLevel(150);
			hvtGroup.GetSCR_AIGroup().AddWaypointToGroup(w);
			
            //hvtGroup.AddOrder(defendPos, IA_AiOrder.DefendSmall, true); // Using DefendSmall for tighter defense
            
            //Print(string.Format("[IA_AssassinationObjective] HVT defend order given at %1 using standard order system", defendPos.ToString()), LogLevel.DEBUG);
        }
    }
    
    protected void GiveGuardDefendOrder(IA_AiGroup guardGroup, IEntity hvt)
    {
        if (guardGroup && guardGroup.IsSpawned() && guardGroup.GetAliveCount() > 0 && hvt)
        {
			
            vector hvtPos = hvt.GetOrigin();
				       vector guardPos = IA_Game.rng.GenerateRandomPointInRadius(5, 20, hvtPos);
            guardPos[1] = GetGame().GetWorld().GetSurfaceY(guardPos[0], guardPos[2]);	
			
			guardGroup.SetTacticalState(IA_GroupTacticalState.Neutral, guardPos, null, true);
			// Generate a random position around the HVT for this guard group
     
			
			ResourceName rname = "{FAD1D789EE291964}Prefabs/AI/Waypoints/AIWaypoint_Defend_Large.et";
            Resource res = Resource.Load(rname);
			IEntity waypointEnt = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(guardPos));
        	SCR_AIWaypoint w = null; // Initialize w to null
			w = SCR_AIWaypoint.Cast(waypointEnt);
			
            if(!w)
				return;
			w.SetPriorityLevel(80);
			guardGroup.GetSCR_AIGroup().AddWaypointToGroup(w);

            // Use the standard tactical state system
            
            //guardGroup.AddOrder(guardPos, IA_AiOrder.Defend, true); // Using DefendSmall for tighter defense
            // Guards don't need explicit orders beyond the tactical state
            // The defending state will handle creating appropriate waypoints
            
            //Print(string.Format("[IA_AssassinationObjective] Guard defend order given at %1 using standard order system", guardPos.ToString()), LogLevel.DEBUG);
        }
    }
    
    protected void SpawnGeneratorAt(vector genPos)
    {
        // Spawn the generator prefab
        Resource genResource = Resource.Load("{ADE488DB19F05050}Prefabs/IA_Generator1.et");
        if (!genResource)
        {
            Print("[IA_AssassinationObjective] Failed to load generator resource!", LogLevel.ERROR);
            return;
        }
        
        m_Generator = GetGame().SpawnEntityPrefab(genResource, null, IA_CreateSimpleSpawnParams(genPos));
        if (m_Generator)
        {
            Print(string.Format("[IA_AssassinationObjective] Spawned generator at %1", genPos.ToString()), LogLevel.NORMAL);
            
            // Add damage handler to detect destruction
            SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(m_Generator.FindComponent(SCR_DamageManagerComponent));
            if (damageManager)
            {
                damageManager.GetOnDamageStateChanged().Insert(OnGeneratorDamageStateChanged);
            }
			
			// Create optional task for destroying the generator
            string title = "Destroy Generator (Optional)";
            string desc = "Destroy the enemy generator to prevent them from calling reinforcements.";
            m_GeneratorTask = _CreateTaskAtPosition(title, desc, genPos);
        }
    }
    
    protected void OnGeneratorDamageStateChanged(EDamageState state)
    {
        if (state == EDamageState.DESTROYED && !m_GeneratorDestroyed)
        {
            m_GeneratorDestroyed = true;
            Print("[IA_AssassinationObjective] Generator destroyed! Reinforcements disabled.", LogLevel.NORMAL);
            
            // If we have a private area instance, tell it to cancel its reinforcements
            if (m_ObjectiveAreaInstance)
            {
                m_ObjectiveAreaInstance.SetSideObjectiveDefenseActive(false, null);
            }
            
			if (m_GeneratorTask)
            {
                SCR_ExtendedTask extendedTask = SCR_ExtendedTask.Cast(m_GeneratorTask);
                if (extendedTask)
                    extendedTask.SetTaskState(SCR_ETaskState.COMPLETED);
                m_GeneratorTask = null;
            }
			
            // Notify players

            IA_Game.S_TriggerGlobalNotification("GeneratorDestroyed", "Side Objective: Generator Destroyed! Enemy reinforcements disabled!");
        }
    }
    
    protected void SpawnReinforcements()
    {
        // This method is now fully deprecated as IA_AreaInstance handles all reinforcement logic.
        Print("[IA_AssassinationObjective] SpawnReinforcements is deprecated.", LogLevel.WARNING);
    }

    protected void SpawnEscapeVehicleAt(vector spawnPos)
    {
        string vehiclePrefabPath = IA_VehicleCatalog.GetRandomVehiclePrefab(m_EnemyIAFaction, m_EnemyGameFaction);
        if (vehiclePrefabPath == "")
        {
            Print("[IA_AssassinationObjective] No valid escape vehicle prefab found!", LogLevel.WARNING);
            return;
        }

        Resource vehRes = Resource.Load(vehiclePrefabPath);
        if (!vehRes)
        {
            Print(string.Format("[IA_AssassinationObjective] Failed to load escape vehicle resource: %1", vehiclePrefabPath), LogLevel.ERROR);
            return;
        }

        m_EscapeVehicle = GetGame().SpawnEntityPrefab(vehRes, null, IA_CreateSimpleSpawnParams(spawnPos));
        if (m_EscapeVehicle)
        {
            Print(string.Format("[IA_AssassinationObjective] Spawned escape vehicle at %1", spawnPos.ToString()), LogLevel.NORMAL);
        }
    }

    override void Complete()
    {
        if (m_State != IA_SideObjectiveState.Active) return;

        m_State = IA_SideObjectiveState.Completed;
        Print(string.Format("Assassination objective at %1 completed.", m_Position.ToString()), LogLevel.NORMAL);
        
        CompletePlayerTask();

        // Use the private area instance to clean up all its managed units
        if (m_ObjectiveAreaInstance)
        {
			GetGame().GetCallqueue().CallLater(S_CleanupAssassinationObjective, OBJECTIVE_COOLDOWN_MILLASECONDS, false, m_ObjectiveAreaInstance);
			m_ObjectiveAreaInstance = null;
        }

        // Clean up the transient area definition
        if (m_TransientArea)
        {
            delete m_TransientArea;
        }
        
        // Clean up generator
        if (m_Generator)
        {
            IA_Game.AddEntityToGc(m_Generator);
            m_Generator = null;
        }
		
		if (m_GeneratorTask)
        {
            SCR_ExtendedTask extendedTask = SCR_ExtendedTask.Cast(m_GeneratorTask);
            if (extendedTask)
                extendedTask.SetTaskState(SCR_ETaskState.COMPLETED);
            m_GeneratorTask = null;
        }

        // --- NEW: Cleanup escape vehicle ---
        if (m_EscapeVehicle)
        {
            IA_Game.AddEntityToGc(m_EscapeVehicle);
            m_EscapeVehicle = null;
        }

        if (m_EscapeTask)
        {
            SCR_ExtendedTask extendedTask = SCR_ExtendedTask.Cast(m_EscapeTask);
            if (extendedTask)
                extendedTask.SetTaskState(SCR_ETaskState.COMPLETED);
            m_EscapeTask = null;
        }
    }
    
    override void Update(float timeSlice)
    {
        if (m_State != IA_SideObjectiveState.Active)
            return;

		if(m_bHVTSpawned && !m_HVT)
		{
			Print("[IA_AssassinationObjective] HVT entity is null after being spawned, assuming objective complete.", LogLevel.NORMAL);
            _CleanupObjective(true);
            return; 
		}

        // If the area comes under attack and we haven't activated reinforcement waves yet,
        // do so now and also start the escape countdown.
        if (m_ObjectiveAreaInstance && m_ObjectiveAreaInstance.IsUnderAttack() && !m_EscapeBehaviorTriggered)
        {
            if (!m_DefenseWavesActive)
            {
                m_ObjectiveAreaInstance.SetSideObjectiveDefenseActive(true, m_EnemyGameFaction);
                m_DefenseWavesActive = true;
            }

            if (!m_EscapeCountdownActive)
            {
                int randMinutes = Math.RandomInt(5, 16); // 5–16 minutes
                m_EscapeCountdownEndTime = System.GetTickCount() + (randMinutes * 60 * 1000);
                m_EscapeCountdownActive = true;
                m_EscapeBehaviorTriggered = true; // Lock in the escape sequence
                Print(string.Format("[IA_AssassinationObjective] Area under attack – escape countdown started (%1 minutes).", randMinutes), LogLevel.NORMAL);

                // Inform players reinforcements incoming. If a generator is still operational, add hint to destroy it.
                string notifText = "Side Objective: Reinforcements Inbound!";
                if (m_Generator && !m_GeneratorDestroyed)
                    notifText += " Destroy the Generator to stop them.";

                IA_Game.S_TriggerGlobalNotification("ReinforcementsCalled", notifText);
            }
        }

        // Check countdown completion
        if (m_EscapeCountdownActive && System.GetTickCount() >= m_EscapeCountdownEndTime)
        {
            TriggerEscapeMode();
        }

        // One-minute warning (create escape task if not yet)
        if (m_EscapeCountdownActive && !m_EscapeTaskCreated)
        {
            int remainingMs = m_EscapeCountdownEndTime - System.GetTickCount();
            if (remainingMs <= 60 * 1000) // 60 seconds left
            {
                _SetupEscapeLocation();
            }
        }

        // Monitor HVT reaching escape point
        if (m_EscapePoint != vector.Zero && m_HVT && m_State == IA_SideObjectiveState.Active)
        {
            vector hvPos = m_HVT.GetOrigin();
            bool hvtAtEscapePoint = vector.DistanceSq(hvPos, m_EscapePoint) < (12*12);

            if (hvtAtEscapePoint && !m_bHVTEscaping)
            {
                // HVT just reached the escape point
                m_bHVTEscaping = true;
                m_iHVTEscapeTimeEnd = System.GetTickCount() + 60000; // 60 second timer
                Print("[IA_AssassinationObjective] HVT reached escape point. Starting 30s extraction timer.", LogLevel.NORMAL);
                
                // Notify players
                IA_Game.S_TriggerGlobalNotification("HVTEscaping", "Side Objective: HVT has reached the extraction point! Eliminate them immediately!");
            }
            
            if (m_bHVTEscaping)
            {
                hvtAtEscapePoint = vector.DistanceSq(hvPos, m_EscapePoint) < (35*35);
                if (!hvtAtEscapePoint)
                {
                    // HVT left the escape point
                    m_bHVTEscaping = false;
                    Print("[IA_AssassinationObjective] HVT left the escape point. Extraction timer reset.", LogLevel.NORMAL);
                }
                else if (System.GetTickCount() >= m_iHVTEscapeTimeEnd)
                {
                    // Timer finished, HVT successfully escaped
                    Print("[IA_AssassinationObjective] HVT successfully extracted – objective failed.", LogLevel.NORMAL);
					IA_Game.S_TriggerGlobalNotification("HVTEscaped", "Side Objective: The HVT has Escaped! Mission Failed.");

                    Fail();
                }
            }
        }
    }

    protected void _GiveDelayedEscapeOrders()
    {
        if (m_EscapePoint == vector.Zero) return;

        Print("[IA_AssassinationObjective] Giving delayed escape move orders.", LogLevel.NORMAL);

        // Order every AI in the objective area instance to move to escape point
        if (m_ObjectiveAreaInstance)
        {
            array<ref IA_AiGroup> groups = m_ObjectiveAreaInstance.GetMilitaryGroups();
            foreach (IA_AiGroup grp : groups)
            {
                if (!grp || !grp.IsSpawned() || grp.GetAliveCount() == 0) continue;
				
				// If group is already at the destination, clear their orders and do nothing.
                float distSq = vector.DistanceSq(grp.GetOrigin(), m_EscapePoint);
                if (distSq < (40*40)) // 40m arrival radius
                {
                    grp.SetTacticalState(IA_GroupTacticalState.Neutral, grp.GetOrigin(), null, true);
                    continue; 
                }
                
                vector escapePoint;
                
                // HVT group goes directly to the exact escape point
                if (IsHVTGroup(grp))
                {
                    escapePoint = m_EscapePoint;
                    Print("[IA_AssassinationObjective] HVT group ordered to escape point via tactical state.", LogLevel.DEBUG);
                }
                else
                {
                    // Other groups get randomized positions nearby
                    escapePoint = IA_Game.rng.GenerateRandomPointInRadius(2, 8, m_EscapePoint);
                    escapePoint[1] = GetGame().GetWorld().GetSurfaceY(escapePoint[0], escapePoint[2]);
					Print("[IA_AssassinationObjective] Guard/Reinforcement group ordered to escape point via tactical state.", LogLevel.DEBUG);
                }
                
				// Set the state to Escaping for all groups. This correctly sets the highest waypoint priority
				// and issues the uninterruptible PriorityMove order internally.
				grp.SetTacticalState(IA_GroupTacticalState.Escaping, escapePoint, null, true);
            }
        }
    }
    
    protected void _ReactivateAndSetNeutralState(array<ref IA_AiGroup> groupsToProcess)
    {
        if (!groupsToProcess)
    	    return;
    
        foreach (IA_AiGroup grp : groupsToProcess)
        {
    	    if (!grp) continue;
    	    
    	    // Setting state to Neutral clears existing orders.
    	    grp.SetTacticalState(IA_GroupTacticalState.Neutral, grp.GetOrigin(), null, true);
    	    
    	    SCR_AIGroup scrGroup = grp.GetSCR_AIGroup();
    	    if (scrGroup)
    	        scrGroup.ActivateAI();
        }
    }
    
    protected void TriggerEscapeMode()
    {
        // This check is to prevent this block from ever running more than once.
        // The m_EscapeCountdownActive flag is our gatekeeper.
        if (!m_EscapeCountdownActive) return;

        Print("[IA_AssassinationObjective] Escape countdown finished – switching AI to ESCAPE mode.", LogLevel.NORMAL);

        // Deactivate AI, then schedule a delayed reactivation to ensure the engine processes the state change.
        if (m_ObjectiveAreaInstance)
        {
			// Tell the AreaInstance to stop issuing orders, as we are now in an escape sequence.
            m_ObjectiveAreaInstance.SetEscapeSequenceActive(true);
			
            array<ref IA_AiGroup> allGroups = m_ObjectiveAreaInstance.GetMilitaryGroups();
            array<ref IA_AiGroup> groupsToProcess = {}; // Array to hold groups we actually deactivate

            foreach (IA_AiGroup grp : allGroups)
            {
                if (!grp || !grp.IsSpawned() || grp.GetAliveCount() == 0) continue;
				
				// --- NEW: Process all groups, not just reinforcements ---
				// bool isObjectiveGuard = m_ObjectiveGroups.Find(grp) != -1;
                // if (IsHVTGroup(grp) || isObjectiveGuard)
                // {
                //    continue; // Skip HVT and Guard groups
                // }
                
				SCR_AIGroup scrGroup = grp.GetSCR_AIGroup();
				
				// Deactivating the AI to force a reset of its internal behavior tree state.
				if (scrGroup)
                {
					scrGroup.DeactivateAI();
                    groupsToProcess.Insert(grp);
                }
            }

            if (!groupsToProcess.IsEmpty())
            {
                // Reactivate after a short delay to ensure the deactivation is processed by the engine.
                GetGame().GetCallqueue().CallLater(this._ReactivateAndSetNeutralState, 100, false, groupsToProcess);
            }
            
            // Disable side-objective defense waves
            m_ObjectiveAreaInstance.SetSideObjectiveDefenseActive(false, null);
        }

        // Call the function to give move orders after a short delay to allow reactivation
        GetGame().GetCallqueue().CallLater(_GiveDelayedEscapeOrders, 200, false);
        
        // Prevent this from being called again
        m_EscapeCountdownActive = false;
    }
    
    override void Fail()
    {
         if (m_State != IA_SideObjectiveState.Active) return;
         Print(string.Format("Assassination objective at %1 failed.", m_Position.ToString()), LogLevel.NORMAL);

         // Delete the HVT immediately when the objective fails
         if (m_HVT)
         {
             Print("[IA_AssassinationObjective] Deleting HVT entity immediately due to objective failure.", LogLevel.NORMAL);
             IA_Game.AddEntityToGc(m_HVT);
             m_HVT = null;
         }

         // Clean up everything - reusing Complete() but it won't run due to state check.
         // A dedicated cleanup function would be better, but for now, let's ensure it runs.
         // We bypass the internal state check of Complete() by calling a new cleanup function.
         _CleanupObjective(false);
    }

    void OnHVTKilled(notnull SCR_CharacterControllerComponent memberCtrl, IEntity killerEntity, Instigator killer)
    {
        if (killer.GetInstigatorType() == InstigatorType.INSTIGATOR_PLAYER)
        {
            int playerID = killer.GetInstigatorPlayerID();
            if (playerID > 0)
            {
                string playerGuid = GetGame().GetBackendApi().GetPlayerUID(playerID);
                string playerName = GetGame().GetPlayerManager().GetPlayerName(playerID);
                
                IA_StatsManager.GetInstance().QueueHVTKill(playerGuid, playerName);
                Print(string.Format("HVT for objective at %1 has been killed by player %2 (GUID: %3).", m_Position.ToString(), playerName, playerGuid), LogLevel.NORMAL);
            }
        }
        else
        {
             Print(string.Format("HVT for objective at %1 has been killed by non-player.", m_Position.ToString()), LogLevel.NORMAL);
        }

        _CleanupObjective(true);
    }
	
	protected void _CleanupObjective(bool success)
	{
		if (m_State != IA_SideObjectiveState.Active) return;

        if (success)
        {
            m_State = IA_SideObjectiveState.Completed;
            CompletePlayerTask();
        }
        else
        {
            m_State = IA_SideObjectiveState.Failed;
            // Fail the main task
            if (m_PlayerTask)
            {
                SCR_ExtendedTask extendedTask = SCR_ExtendedTask.Cast(m_PlayerTask);
                if (extendedTask)
                    extendedTask.SetTaskState(SCR_ETaskState.FAILED);
                m_PlayerTask = null;
            }
        }

		if (m_ObjectiveAreaInstance)
        {
			GetGame().GetCallqueue().CallLater(S_CleanupAssassinationObjective, OBJECTIVE_COOLDOWN_MILLASECONDS, false, m_ObjectiveAreaInstance);
			m_ObjectiveAreaInstance = null;
        }

        if (m_TransientArea)
            delete m_TransientArea;
        
        if (m_Generator)
        {
            IA_Game.AddEntityToGc(m_Generator);
            m_Generator = null;
        }
		
		if (m_GeneratorTask)
        {
            SCR_ExtendedTask extendedTask = SCR_ExtendedTask.Cast(m_GeneratorTask);
            if (extendedTask)
            {
                if (success)
                    extendedTask.SetTaskState(SCR_ETaskState.COMPLETED);
                else
                    extendedTask.SetTaskState(SCR_ETaskState.FAILED);
            }
            m_GeneratorTask = null;
        }

        if (m_EscapeVehicle)
        {
            IA_Game.AddEntityToGc(m_EscapeVehicle);
            m_EscapeVehicle = null;
        }

        if (m_EscapeTask)
        {
            SCR_ExtendedTask extendedTask = SCR_ExtendedTask.Cast(m_EscapeTask);
            if (extendedTask)
            {
                if (success)
                    extendedTask.SetTaskState(SCR_ETaskState.COMPLETED);
                else
                    extendedTask.SetTaskState(SCR_ETaskState.FAILED);
            }
            m_EscapeTask = null;
        }
	}

    // --- NEW: Generates a random escape point a good distance from the objective center ---
    protected vector _GenerateEscapePoint()
    {
        // Prefer road points 500-750m away on current group area
        int activeGroup = IA_VehicleManager.GetActiveGroup();
        vector roadPoint = IA_VehicleManager.FindRandomRoadEntityInZone(m_Position, 750, activeGroup, 500);
        if (roadPoint != vector.Zero)
            return roadPoint;

        // Fallback to random ground point (still 500-750m)
        const float MIN_DIST = 500.0;
        const float MAX_DIST = 750.0;
        vector pos = m_Position;
        for (int attempt = 0; attempt < 10; attempt++)
        {
            float angle = IA_Game.rng.RandFloat01() * Math.PI2;
            float dist = IA_Game.rng.RandFloatXY(MIN_DIST, MAX_DIST);
            vector candidate;
            candidate[0] = pos[0] + Math.Cos(angle) * dist;
            candidate[2] = pos[2] + Math.Sin(angle) * dist;
            candidate[1] = GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]);
            if (candidate != vector.Zero)
                return candidate;
        }
        return vector.Zero;
    }

    // --- BEGIN ADDED: Method to check if a group is the HVT group ---
    bool IsHVTGroup(IA_AiGroup group)
    {
        return m_HVTGroup == group;
    }
    // --- END ADDED ---

    // --- NEW: Setup escape location and player task ---
    protected void _SetupEscapeLocation()
    {
        if (m_EscapeTask) return; // Already created

        m_EscapePoint = _GenerateEscapePoint();
        if (m_EscapePoint == vector.Zero)
        {
            Print("[IA_AssassinationObjective] Failed to generate escape point!", LogLevel.WARNING);
            return;
        }

        string title = "HVT Escape Point";
        string desc = "Stop the HVT from reaching the escape point.";
        m_EscapeTask = _CreateTaskAtPosition(title, desc, m_EscapePoint);

        Print(string.Format("[IA_AssassinationObjective] Escape task created at %1", m_EscapePoint.ToString()), LogLevel.NORMAL);

        // Notify players
        IA_Game.S_TriggerGlobalNotification("HVTPreparingEscape", "Side Objective: The HVT is preparing to make their escape!");

        m_EscapeTaskCreated = true;
    }
} 