//------------------------------------------------------------------------------------------------
//! Server: replaces a parked, empty helicopter with another prefab in the same
//! place. This is how a skin goes on: paint lives in the prefab, because
//! SetObject material remaps on a live vehicle crash the vehicle animation update.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinSwap
{
	// The old hull must be gone before its twin spawns inside the same space.
	protected static const int RESPAWN_DELAY_MS = 300;
	protected static const float MAX_PARKED_SPEED_SQ = 0.25;
	protected static const int MAX_DEPTH = 4;

	//------------------------------------------------------------------------------------------------
	//! \return true when the vehicle can be replaced without taking it from anyone
	static bool IsParkedAndEmpty(IEntity vehicle)
	{
		if (!vehicle)
			return false;

		DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
		if (damage && damage.IsDestroyed())
			return false;

		BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(vehicle.FindComponent(BaseVehicleControllerComponent));
		if (controller && controller.IsEngineOn())
			return false;

		Physics physics = vehicle.GetPhysics();
		if (physics && physics.GetVelocity().LengthSq() > MAX_PARKED_SPEED_SQ)
			return false;

		return !HasOccupant(vehicle, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Replace a parked, empty vehicle with another prefab.
	//! \param[in] pad respawner that owns the vehicle and adopts its replacement; may be null
	//! \return true when the swap started
	static bool Begin(IEntity vehicle, ResourceName prefab, IA_VehicleRespawner pad)
	{
		if (!Replication.IsServer() || prefab.IsEmpty() || !IsParkedAndEmpty(vehicle))
			return false;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			Print(string.Format("[IA][HeliSkin] Cannot load %1; the helicopter keeps its paint.", prefab), LogLevel.ERROR);
			return false;
		}

		vector mat[4];
		vehicle.GetWorldTransform(mat);
		if (pad)
			pad.OnSwapStarted();

		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		GetGame().GetCallqueue().CallLater(Finish, RESPAWN_DELAY_MS, false, prefab, mat[0], mat[1], mat[2], mat[3], pad);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Finish(ResourceName prefab, vector right, vector up, vector forward, vector origin, IA_VehicleRespawner pad)
	{
		IEntity spawned;
		Resource resource = Resource.Load(prefab);
		if (resource && resource.IsValid())
		{
			ref EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[0] = right;
			params.Transform[1] = up;
			params.Transform[2] = forward;
			params.Transform[3] = origin;
			spawned = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		}

		if (!spawned)
			Print(string.Format("[IA][HeliSkin] Failed to spawn %1 after removing the helicopter it replaces.", prefab), LogLevel.ERROR);

		// A pad without a vehicle respawns a stock one on its next check.
		if (pad)
			pad.OnSwapFinished(spawned);
	}

	//------------------------------------------------------------------------------------------------
	//! Seats live on slotted parts, and a seated character is a child of its seat's entity.
	protected static bool HasOccupant(IEntity ent, int depth)
	{
		if (!ent || depth > MAX_DEPTH)
			return false;
		if (ChimeraCharacter.Cast(ent))
			return true;

		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(ent.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (compartments && IsBoarded(compartments))
			return true;

		IEntity child = ent.GetChildren();
		while (child)
		{
			if (HasOccupant(child, depth + 1))
				return true;
			child = child.GetSibling();
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A reserved seat has someone climbing in who is not a child of the vehicle yet.
	protected static bool IsBoarded(notnull SCR_BaseCompartmentManagerComponent compartments)
	{
		ref array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot && (slot.GetOccupant() || slot.IsReserved()))
				return true;
		}
		return false;
	}
}
