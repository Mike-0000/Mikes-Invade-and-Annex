//------------------------------------------------------------------------------------------------
//! Solo test path for helicopter skins: paints the nearest airframe on this
//! machine only, so a skin can be looked at without earning its rating.
//! Client only; nothing is replicated and no rating is touched.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinPreview
{
	protected static const float SEARCH_RADIUS_M = 75;

	// Next catalogue entry to try, so repeated presses step through every skin.
	protected static int s_iCycle;

	protected vector m_vOrigin;
	protected IEntity m_Nearest;
	protected float m_fNearestSq;

	//------------------------------------------------------------------------------------------------
	//! Paint the nearest helicopter that has a skin with the next one in the catalogue.
	//! \return what happened, for the admin to read
	static string PaintNearest()
	{
		IA_HeliSkinManagerComponent skins = IA_HeliSkinManagerComponent.GetInstance();
		PlayerController pc = GetGame().GetPlayerController();
		if (!skins || !pc)
			return "Helicopter skins are not running in this mission.";

		IEntity pawn = pc.GetControlledEntity();
		if (!pawn)
			return "Spawn in before previewing a skin.";

		// A seated pawn's origin is relative to its vehicle.
		vector mat[4];
		pawn.GetWorldTransform(mat);

		ref IA_HeliSkinPreview finder = new IA_HeliSkinPreview();
		finder.m_vOrigin = mat[3];
		GetGame().GetWorld().QueryEntitiesBySphere(mat[3], SEARCH_RADIUS_M, finder.OnEntity, null, EQueryEntitiesFlags.DYNAMIC | EQueryEntitiesFlags.WITH_OBJECT);
		if (!finder.m_Nearest)
			return string.Format("No helicopter with a skin within %1 m.", SEARCH_RADIUS_M);

		IA_HeliSkinDef def = NextFitting(finder.m_Nearest);
		if (!def)
			return "No skin fits that helicopter.";
		if (!skins.PaintLocal(finder.m_Nearest, def.m_iId))
			return string.Format("%1 matched no material on that helicopter.", def.m_sDisplayName);

		return string.Format("%1 painted on the nearest helicopter. Only you can see it.", def.m_sDisplayName);
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
	protected static bool HasSkin(IEntity vehicle)
	{
		EntityPrefabData prefab = vehicle.GetPrefabData();
		if (!prefab || !vehicle.FindComponent(VehicleHelicopterSimulation))
			return false;

		string prefabName = prefab.GetPrefabName();
		foreach (IA_HeliSkinDef def : IA_HeliSkinCatalog.GetDefs())
		{
			if (IA_HeliSkinCatalog.FitsPrefab(def, prefabName))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static IA_HeliSkinDef NextFitting(IEntity vehicle)
	{
		EntityPrefabData prefab = vehicle.GetPrefabData();
		if (!prefab)
			return null;

		string prefabName = prefab.GetPrefabName();
		array<ref IA_HeliSkinDef> defs = IA_HeliSkinCatalog.GetDefs();
		int count = defs.Count();
		IA_HeliSkinDef def;
		int i;
		for (i = 0; i < count; i++)
		{
			def = defs[(s_iCycle + i) % count];
			if (!IA_HeliSkinCatalog.FitsPrefab(def, prefabName))
				continue;

			s_iCycle = (s_iCycle + i + 1) % count;
			return def;
		}
		return null;
	}
}
