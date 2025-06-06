[EntityEditorProps(category: "GameScripted/AI", description: "Spawns and respawns a specific vehicle if it is destroyed.", color: "0 255 0 255")]
class IA_VehicleRespawnerClass : SCR_VehicleSpawnerClass
{
}

class IA_VehicleRespawner : SCR_VehicleSpawner
{
	static const float DEFAULT_RESPAWN_INTERVAL_S = 15.0;
	static const float MIN_DISTANCE_ALIVE_VEHICLE_NO_RESPAWN = 10.0; // Min distance to an existing ALIVE vehicle to prevent re-spawn
	static const int INITIAL_SPAWN_DELAY_MS = 7000; // 7 second delay for initial spawn to allow config loading

	[Attribute(category: "Catalog Parameters", desc: "Type of entity catalogs that will be allowed on this spawner.", uiwidget: UIWidgets.SearchComboBox, enums: ParamEnumArray.FromEnum(EEntityCatalogType))]
	protected ref array<EEntityCatalogType> m_aCatalogTypes;
	
	[Attribute(defvalue: EEditableEntityLabel.FACTION_US.ToString(), UIWidgets.ComboBox, "Faction of the vehicle to spawn.", enums: ParamEnumArray.FromEnum(EEditableEntityLabel))]
	private EEditableEntityLabel m_eFactionLabel;

	[Attribute(defvalue: IA_VehicleSpawnType.VEHICLE_CAR.ToString(), UIWidgets.ComboBox, "Type of vehicle to spawn from config.", enums: ParamEnumArray.FromEnum(IA_VehicleSpawnType))]
	private IA_VehicleSpawnType m_eVehicleSpawnType;

	[Attribute(uiwidget: UIWidgets.SearchComboBox, category: "Catalog Parameters", desc: "Allowed labels.", enums: ParamEnumArray.FromEnum(EEditableEntityLabel))]
	protected ref array<EEditableEntityLabel> m_aIncludedTraitLabels;

	[Attribute(uiwidget: UIWidgets.SearchComboBox, category: "Catalog Parameters", desc: "Ignored labels.", enums: ParamEnumArray.FromEnum(EEditableEntityLabel))]
	protected ref array<EEditableEntityLabel> m_aExcludedTraitLabels;

	[Attribute(DEFAULT_RESPAWN_INTERVAL_S.ToString(), UIWidgets.EditBox, "Interval in seconds to check if vehicle needs respawning.")]
	private float m_fRespawnCheckInterval;
	
	protected IEntity m_RespawnerOwnerEntity; 		// Stores the entity this component is attached to
	protected IEntity m_RespawnerSpawnedVehicle; 	// Stores the vehicle spawned by this respawner

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner); 
		m_RespawnerOwnerEntity = owner; // Store our owner

		if (RplSession.Mode() == RplMode.Client || !GetGame().InPlayMode())
			return;

		if (m_fRespawnCheckInterval <= 0)
			m_fRespawnCheckInterval = DEFAULT_RESPAWN_INTERVAL_S;
		
		//Print(string.Format("IA_VehicleRespawner %1: OnPostInit. Prefab: %2, Interval: %3s", m_RespawnerOwnerEntity, m_sVehiclePrefab, m_fRespawnCheckInterval), LogLevel.DEBUG);

		// Schedule initial spawn attempt
		GetGame().GetCallqueue().CallLater(PerformSpawn, INITIAL_SPAWN_DELAY_MS, false); 
	
		// Schedule periodic check for respawning
		GetGame().GetCallqueue().CallLater(CheckVehicleStatus, m_fRespawnCheckInterval * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	void CheckVehicleStatus()
	{
		if (!m_RespawnerOwnerEntity) // Owner might have been deleted, stop checks
		{
			//Print("IA_VehicleRespawner: Owner entity is null, removing CheckVehicleStatus call.", LogLevel.WARNING);
			if (GetGame() && GetGame().GetCallqueue())
				GetGame().GetCallqueue().Remove(CheckVehicleStatus);
			return;
		}

		bool needsRespawn = false;
		if (!m_RespawnerSpawnedVehicle)
		{
			needsRespawn = true;
			//Print(string.Format("IA_VehicleRespawner %1: No spawned entity reference (m_RespawnerSpawnedVehicle is null). Needs respawn.", m_RespawnerOwnerEntity), LogLevel.DEBUG);
		}
		else if (!IsVehicleAlive(m_RespawnerSpawnedVehicle))
		{
			needsRespawn = true;
			//Print(string.Format("IA_VehicleRespawner %1: Spawned entity %2 is not alive. Needs respawn.", m_RespawnerOwnerEntity, m_RespawnerSpawnedVehicle), LogLevel.DEBUG);
		}

		if (needsRespawn)
		{
			PerformSpawn();
		}
	}

	//------------------------------------------------------------------------------------------------
	bool IsVehicleAlive(IEntity vehicle)
	{
		if (!vehicle)
			return false;

		DamageManagerComponent damageManager = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
		if (damageManager)
		{
			return damageManager.GetState() != EDamageState.DESTROYED;
		}
		
		// Fallback: if it exists and no damage manager says it's destroyed, assume alive.
		// This also handles cases where an entity might be deleted from the world, making the 'vehicle' reference stale (effectively null).
		return true; 
	}
	void DelayedVehicleDeletion(IEntity vic){
		IA_Game.AddEntityToGc(vic);
	
	}

	//------------------------------------------------------------------------------------------------
	//! Get a random vehicle prefab from the config based on the spawn type
	string GetRandomVehiclePrefabFromConfig()
	{
		// Get the config instance from MissionInitializer
		IA_Config config = IA_MissionInitializer.GetGlobalConfig();
		if (!config)
		{
			Print(string.Format("IA_VehicleRespawner %1: Could not get config instance from MissionInitializer!", m_RespawnerOwnerEntity), LogLevel.WARNING);
			return string.Empty;
		}

		Print(string.Format("IA_VehicleRespawner %1: Got config instance, checking spawn type: %2", m_RespawnerOwnerEntity, typename.EnumToString(IA_VehicleSpawnType, m_eVehicleSpawnType)), LogLevel.DEBUG);

		array<ResourceName> vehicleArray;
		
		// Select the appropriate array based on spawn type
		switch (m_eVehicleSpawnType)
		{
			case IA_VehicleSpawnType.GENERIC_HELI:
				vehicleArray = config.m_aGenericHeliOverridePrefabs;
				break;
			case IA_VehicleSpawnType.ATTACK_HELI:
				vehicleArray = config.m_aAttackHeliOverridePrefabs;
				break;
			case IA_VehicleSpawnType.TRANSPORT_HELI:
				vehicleArray = config.m_aTransportHeliOverridePrefabs;
				break;
			case IA_VehicleSpawnType.VEHICLE_CAR:
				vehicleArray = config.m_aVehicleCarOverridePrefabs;
				break;
			case IA_VehicleSpawnType.VEHICLE_ARMOR:
				vehicleArray = config.m_aVehicleArmorOverridePrefabs;
				break;
			case IA_VehicleSpawnType.APC:
				vehicleArray = config.m_aAPC_OverridePrefabs;
				break;
			case IA_VehicleSpawnType.TRUCK:
				vehicleArray = config.m_aTruckOverridePrefabs;
				break;
			case IA_VehicleSpawnType.MEDICAL_VEHICLE:
				vehicleArray = config.m_aMedicalVehicleOverridePrefabs;
				break;
			case IA_VehicleSpawnType.MEDICAL_CAR:
				vehicleArray = config.m_aMedicalCarOverridePrefabs;
				break;
			default:
				Print(string.Format("IA_VehicleRespawner %1: Unknown vehicle spawn type: %2", m_RespawnerOwnerEntity, m_eVehicleSpawnType), LogLevel.ERROR);
				return string.Empty;
		}

		// Check if array exists and has entries
		if (!vehicleArray)
		{
			Print(string.Format("IA_VehicleRespawner %1: vehicleArray is null for spawn type: %2", m_RespawnerOwnerEntity, typename.EnumToString(IA_VehicleSpawnType, m_eVehicleSpawnType)), LogLevel.WARNING);
			return string.Empty;
		}
		
		if (vehicleArray.Count() == 0)
		{
			Print(string.Format("IA_VehicleRespawner %1: No vehicle prefabs configured for spawn type: %2 (array exists but is empty)", m_RespawnerOwnerEntity, typename.EnumToString(IA_VehicleSpawnType, m_eVehicleSpawnType)), LogLevel.DEBUG);
			return string.Empty;
		}

		// Return a random prefab from the array
		int randomIndex = Math.RandomInt(0, vehicleArray.Count());
		string selectedPrefab = vehicleArray[randomIndex];
		Print(string.Format("IA_VehicleRespawner %1: Selected prefab %2 (index %3 of %4) for spawn type: %5", m_RespawnerOwnerEntity, selectedPrefab, randomIndex, vehicleArray.Count(), typename.EnumToString(IA_VehicleSpawnType, m_eVehicleSpawnType)), LogLevel.DEBUG);
		return selectedPrefab;
	}
	//------------------------------------------------------------------------------------------------
	//! PerformSpawn is overridden to use the specific m_sVehiclePrefab and handle wreck cleanup.
	override void PerformSpawn()
	{
		if (!m_RespawnerOwnerEntity)
		{
			//Print(string.Format("IA_VehicleRespawner: PerformSpawn called but m_RespawnerOwnerEntity is null."), LogLevel.WARNING);
			return;
		}
			
		if (RplSession.Mode() == RplMode.Client || !GetGame().InPlayMode())
			return;
		
		// If an alive vehicle (spawned by this spawner) is already present and close, do nothing.
		if (m_RespawnerSpawnedVehicle && IsVehicleAlive(m_RespawnerSpawnedVehicle) && 
			vector.Distance(m_RespawnerSpawnedVehicle.GetOrigin(), m_RespawnerOwnerEntity.GetOrigin()) < MIN_DISTANCE_ALIVE_VEHICLE_NO_RESPAWN)
		{
			//Print(string.Format("IA_VehicleRespawner %1: Alive vehicle %2 already exists nearby. Skipping spawn.", m_RespawnerOwnerEntity, m_RespawnerSpawnedVehicle), LogLevel.DEBUG);
			return;
		}

		// If m_RespawnerSpawnedVehicle refers to a wreck (not alive), delete it first.
		if (m_RespawnerSpawnedVehicle && !IsVehicleAlive(m_RespawnerSpawnedVehicle))
		{
			//Print(string.Format("IA_VehicleRespawner %1: Existing entity %2 is a wreck. Deleting.", m_RespawnerOwnerEntity, m_RespawnerSpawnedVehicle), LogLevel.DEBUG);
			RplComponent rpl = RplComponent.Cast(m_RespawnerSpawnedVehicle.FindComponent(RplComponent));
			if (rpl && rpl.IsMaster()) // Ensure server (master) is deleting its own entities
			{
				IEntity vic1 = m_RespawnerSpawnedVehicle;
				GetGame().GetCallqueue().CallLater(DelayedVehicleDeletion, 120000, false, vic1);
				 // Use IA_Game's garbage collection
			}
			m_RespawnerSpawnedVehicle = null; // Clear the reference as it's now deleted or scheduled for deletion.
		}
		
		// Try to get the vehicle prefab from config first
		string vehiclePrefabToSpawn = GetRandomVehiclePrefabFromConfig();

		// If config doesn't have any prefabs, fall back to catalog system
		if (vehiclePrefabToSpawn == string.Empty)
		{
			// Validate mandatory faction label for catalog fallback
			if (m_eFactionLabel == 0) // Assuming 0 is an invalid/default unselected faction value
			{
				Print(string.Format("IA_VehicleRespawner %1: No config prefabs and Faction Label is not set! Cannot spawn!", m_RespawnerOwnerEntity), LogLevel.ERROR);
				return;
			}

			// Ensure arrays are initialized if null, though attributes should ideally initialize them as empty arrays.
			array<EEditableEntityLabel> includedTraits = {};
			if (m_aIncludedTraitLabels)
				includedTraits.Copy(m_aIncludedTraitLabels);

			array<EEditableEntityLabel> excludedTraits = {};
			if (m_aExcludedTraitLabels)
				excludedTraits.Copy(m_aExcludedTraitLabels);

			// Get the vehicle prefab string from the catalog using selected labels
			vehiclePrefabToSpawn = IA_VehicleCatalog.GetRandomVehiclePrefabBySpecificLabels(m_eFactionLabel, includedTraits, excludedTraits);

			if (vehiclePrefabToSpawn == string.Empty)
			{
				Print(string.Format("IA_VehicleRespawner %1: Could not find a vehicle prefab for spawn type: %2, Faction: %3, IncludedTraits: %4, ExcludedTraits: %5", 
					m_RespawnerOwnerEntity, 
					typename.EnumToString(IA_VehicleSpawnType, m_eVehicleSpawnType),
					typename.EnumToString(EEditableEntityLabel, m_eFactionLabel), 
					includedTraits.Count(), 
					excludedTraits.Count()), LogLevel.WARNING);
				return;
			}
			else
			{
				Print(string.Format("IA_VehicleRespawner %1: Using catalog fallback for spawn type: %2", 
					m_RespawnerOwnerEntity, 
					typename.EnumToString(IA_VehicleSpawnType, m_eVehicleSpawnType)), LogLevel.DEBUG);
			}
		}
		else
		{
			Print(string.Format("IA_VehicleRespawner %1: Using config override for spawn type: %2", 
				m_RespawnerOwnerEntity, 
				typename.EnumToString(IA_VehicleSpawnType, m_eVehicleSpawnType)), LogLevel.DEBUG);
		}
		
		Resource resource = Resource.Load(vehiclePrefabToSpawn);
		if (!resource || !resource.IsValid())
		{
			Print(string.Format("IA_VehicleRespawner %1: Failed to load resource or resource is invalid: %2", m_RespawnerOwnerEntity, vehiclePrefabToSpawn), LogLevel.ERROR);
			return;
		}
		
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		m_RespawnerOwnerEntity.GetTransform(params.Transform); // Spawn at the spawner's location and orientation
		
		//Print(string.Format("IA_VehicleRespawner %1: Attempting to spawn %2 at %3", m_RespawnerOwnerEntity, vehiclePrefabToSpawn, params.Transform[3]), LogLevel.DEBUG);
		IEntity newVehicle = GetGame().SpawnEntityPrefab(resource, m_RespawnerOwnerEntity.GetWorld(), params);

		if (newVehicle)
		{
			//Print(string.Format("IA_VehicleRespawner %1: Successfully spawned %2. New entity: %3", m_RespawnerOwnerEntity, vehiclePrefabToSpawn, newVehicle), LogLevel.DEBUG);
			m_RespawnerSpawnedVehicle = newVehicle; // Update the reference to the newly spawned vehicle
		}
		else
		{
			Print(string.Format("IA_VehicleRespawner %1: Failed to spawn entity for prefab %2", m_RespawnerOwnerEntity, vehiclePrefabToSpawn), LogLevel.ERROR);
			// m_RespawnerSpawnedVehicle remains as it was (null if the wreck was deleted)
		}
	}

	//------------------------------------------------------------------------------------------------
	void IA_VehicleRespawner(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		// Specific initialization for IA_VehicleRespawner if needed
	}

	//------------------------------------------------------------------------------------------------
	void ~IA_VehicleRespawner()
	{
		//Print(string.Format("IA_VehicleRespawner %1: Destructor called.", m_RespawnerOwnerEntity), LogLevel.DEBUG);
		if (GetGame() && GetGame().GetCallqueue()) // Check if game and callqueue exist (e.g. during editor shutdown)
		{
			GetGame().GetCallqueue().Remove(CheckVehicleStatus);
			GetGame().GetCallqueue().Remove(PerformSpawn); // Remove initial spawn call if it was pending
		}
		
		// Clean up our spawned vehicle if the respawner entity itself is deleted
		if (RplSession.Mode() != RplMode.Client && GetGame() && GetGame().InPlayMode())
		{
			if (m_RespawnerSpawnedVehicle)
			{
				//Print(string.Format("IA_VehicleRespawner %1: Destructor deleting spawned vehicle %2.", m_RespawnerOwnerEntity, m_RespawnerSpawnedVehicle), LogLevel.DEBUG);
				IA_Game.AddEntityToGc(m_RespawnerSpawnedVehicle);
				m_RespawnerSpawnedVehicle = null;
			}
		}
		// Base class SCR_VehicleSpawner destructor will be called automatically.
		// It handles its own m_pSpawnedEntity, which we are not using directly.
	}
} 