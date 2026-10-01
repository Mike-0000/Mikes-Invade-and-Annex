//------------------------------------------------------------------------------------------------
//! Solo test path for helicopter skins: the server steps the nearest helipad
//! helicopter to its next skin where it stands, so a skin can be looked at
//! without earning its rating. It works from the pilot's seat with the engine
//! running, the way a skin menu inside the vehicle would. Admin only; no
//! rating is read or changed.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinPreview
{
	protected static const float SEARCH_RADIUS_M = 75;

	protected vector m_vOrigin;
	protected IEntity m_Nearest;
	protected float m_fNearestSq;

	//------------------------------------------------------------------------------------------------
	//! Repaint the helicopter nearest to the pawn: stock, then each skin in turn, then stock again.
	//! \return what happened, for the admin to read
	static string CycleNearest(IEntity pawn)
	{
		if (!Replication.IsServer())
			return "Skin previews run on the server.";
		if (!pawn)
			return "Spawn in before previewing a skin.";

		IA_HeliSkinManagerComponent skins = IA_HeliSkinManagerComponent.GetInstance();
		if (!skins)
			return "This mission has no helicopter skin manager.";

		// A seated pawn's origin is relative to its vehicle.
		vector mat[4];
		pawn.GetWorldTransform(mat);

		ref IA_HeliSkinPreview finder = new IA_HeliSkinPreview();
		finder.m_vOrigin = mat[3];
		pawn.GetWorld().QueryEntitiesBySphere(mat[3], SEARCH_RADIUS_M, finder.OnEntity, null, EQueryEntitiesFlags.DYNAMIC | EQueryEntitiesFlags.WITH_OBJECT);
		IEntity vehicle = finder.m_Nearest;
		if (!vehicle)
			return string.Format("No helicopter with a paint channel within %1 m.", SEARCH_RADIUS_M);

		IA_HeliSkinDef next = NextSkin(IA_HeliSkinCatalog.FindDef(skins.GetVehicleSkin(vehicle)));
		int skinId = IA_HeliSkinCatalog.SKIN_NONE;
		if (next)
			skinId = next.m_iId;

		if (!skins.SetVehicleSkin(vehicle, skinId))
			return "That helicopter cannot be repainted.";

		if (next)
			return string.Format("%1 applied. Press again for the next skin.", next.m_sDisplayName);
		return "Stock paint restored.";
	}

	//------------------------------------------------------------------------------------------------
	protected bool OnEntity(IEntity ent)
	{
		IEntity root = ent.GetRootParent();
		if (!root || root == m_Nearest || IA_HeliSkinManagerComponent.GetVehicleChannel(root) == IA_HeliPaintChannels.CHANNEL_NONE)
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
	//! \return the skin after the one worn, null when the next step is stock paint
	protected static IA_HeliSkinDef NextSkin(IA_HeliSkinDef worn)
	{
		bool passedWorn = worn == null;
		foreach (IA_HeliSkinDef def : IA_HeliSkinCatalog.GetDefs())
		{
			if (passedWorn)
				return def;
			if (def == worn)
				passedWorn = true;
		}
		return null;
	}
}
