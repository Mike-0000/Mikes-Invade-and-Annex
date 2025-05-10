[EntityEditorProps(category: "GameScripted/AI", description: "Spawns and respawns a specific vehicle if it is destroyed.", color: "0 255 0 255")]
class IA_VehicleRespawnerClass : SCR_VehicleSpawnerClass
{
}

class IA_VehicleRespawner : SCR_VehicleSpawner
{
	static const float DEFAULT_RESPAWN_INTERVAL_S = 120.0;
	static const float MIN_DISTANCE_ALIVE_VEHICLE_NO_RESPAWN = 10.0; // Min distance to an existing ALIVE vehicle to prevent re-spawn
	static const int INITIAL_SPAWN_DELAY_MS = 1000; // 1 second delay for initial spawn

	[Attribute(category: "Catalog Parameters", desc: "Type of entity catalogs that will be allowed on this spawner.", uiwidget: UIWidgets.SearchComboBox, enums: ParamEnumArray.FromEnum(EEntityCatalogType))]
	protected ref array<EEntityCatalogType> m_aCatalogTypes;
	
	[Attribute(defvalue: EEditableEntityLabel.FACTION_US.ToString(), UIWidgets.ComboBox, "Faction of the vehicle to spawn.", enums: ParamEnumArray.FromEnum(EEditableEntityLabel))]
	private EEditableEntityLabel m_eFactionLabel;

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
				IA_Game.AddEntityToGc(m_RespawnerSpawnedVehicle); // Use IA_Game's garbage collection
			}
			m_RespawnerSpawnedVehicle = null; // Clear the reference as it's now deleted or scheduled for deletion.
		}
		
		// Validate mandatory faction label
		if (m_eFactionLabel == 0) // Assuming 0 is an invalid/default unselected faction value
		{
			Print(string.Format("IA_VehicleRespawner %1: Faction Label is not set (or is default 0)! Cannot spawn!", m_RespawnerOwnerEntity), LogLevel.ERROR);
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
		// This method will be updated in IA_VehicleCatalog.c in the next step
		string vehiclePrefabToSpawn = IA_VehicleCatalog.GetRandomVehiclePrefabBySpecificLabels(m_eFactionLabel, includedTraits, excludedTraits);

		if (vehiclePrefabToSpawn == string.Empty)
		{
			Print(string.Format("IA_VehicleRespawner %1: Could not find a matching vehicle prefab for Faction: %2, IncludedTraits: %3, ExcludedTraits: %4", 
				m_RespawnerOwnerEntity, 
				typename.EnumToString(EEditableEntityLabel, m_eFactionLabel), 
				includedTraits.Count(), 
				excludedTraits.Count()), LogLevel.WARNING);
			return;
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