//------------------------------------------------------------------------------------------------
//! Solo test path for helicopter skins: the server swaps the nearest parked,
//! empty helicopter for its next skin variant, so a skin can be looked at
//! without earning its rating. Admin only; no rating is read or changed.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinPreview
{
	protected static const float SEARCH_RADIUS_M = 75;

	protected vector m_vOrigin;
	protected IEntity m_Nearest;
	protected float m_fNearestSq;

	//------------------------------------------------------------------------------------------------
	//! Swap the helicopter nearest to the pawn: stock, then each skin in turn, then stock again.
	//! \return what happened, for the admin to read
	static string SwapNearest(IEntity pawn)
	{
		if (!Replication.IsServer())
			return "Skin previews run on the server.";
		if (!pawn)
			return "Spawn in before previewing a skin.";

		// A seated pawn's origin is relative to its vehicle.
		vector mat[4];
		pawn.GetWorldTransform(mat);

		ref IA_HeliSkinPreview finder = new IA_HeliSkinPreview();
		finder.m_vOrigin = mat[3];
		pawn.GetWorld().QueryEntitiesBySphere(mat[3], SEARCH_RADIUS_M, finder.OnEntity, null, EQueryEntitiesFlags.DYNAMIC | EQueryEntitiesFlags.WITH_OBJECT);
		IEntity vehicle = finder.m_Nearest;
		if (!vehicle)
			return string.Format("No helicopter with a skin within %1 m.", SEARCH_RADIUS_M);

		if (!IA_HeliSkinSwap.IsParkedAndEmpty(vehicle))
			return "That helicopter is in use. A skin only goes on while it is parked, empty and shut down.";

		ResourceName prefab = vehicle.GetPrefabData().GetPrefabName();
		ResourceName stock = IA_HeliSkinCatalog.FindStockPrefab(prefab);
		IA_HeliSkinDef next = NextSkin(stock, IA_HeliSkinCatalog.FindDefBySkinPrefab(prefab));
		ResourceName target = stock;
		if (next)
			target = next.FindVariant(stock);

		if (!IA_HeliSkinSwap.Begin(vehicle, target, FindPad(vehicle)))
			return "The helicopter could not be swapped; see the server log.";

		if (next)
			return string.Format("%1 swapped in. Press again for the next skin.", next.m_sDisplayName);
		return "Stock paint restored.";
	}

	//------------------------------------------------------------------------------------------------
	protected bool OnEntity(IEntity ent)
	{
		IEntity root = ent.GetRootParent();
		if (!root || root == m_Nearest || !HasSkin(root))
			return true;

		vector mat[4];
		root.GetWorldTransform(mat);
		float distSq = vector.DistanceSq(mat[3], m_vOrigin);
		if (!m_Nearest || distSq < m_fNearestSq)
		{
			m_Nearest = root;
			m_fNearestSq = distSq;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true for a helicopter that is a catalogued stock airframe or one of its skins
	protected static bool HasSkin(IEntity vehicle)
	{
		EntityPrefabData prefab = vehicle.GetPrefabData();
		if (!prefab || !vehicle.FindComponent(VehicleHelicopterSimulation))
			return false;

		return !IA_HeliSkinCatalog.FindStockPrefab(prefab.GetPrefabName()).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! \return the skin after the one worn, null when the next step is stock paint
	protected static IA_HeliSkinDef NextSkin(ResourceName stock, IA_HeliSkinDef worn)
	{
		bool passedWorn = worn == null;
		foreach (IA_HeliSkinDef def : IA_HeliSkinCatalog.GetDefs())
		{
			if (!IA_HeliSkinCatalog.FitsPrefab(def, stock))
				continue;
			if (passedWorn)
				return def;
			if (def == worn)
				passedWorn = true;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the pad that spawned this helicopter, so it adopts the replacement; null for any other
	protected static IA_VehicleRespawner FindPad(IEntity vehicle)
	{
		array<IA_VehicleRespawner> pads = IA_VehicleRespawner.GetHeliPads();
		if (!pads)
			return null;

		foreach (IA_VehicleRespawner pad : pads)
		{
			if (pad && pad.OwnsVehicle(vehicle))
				return pad;
		}
		return null;
	}
}
