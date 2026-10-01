//------------------------------------------------------------------------------------------------
//! Unlockable helicopter skins, gated by the player's global transport rating.
//! Add a skin by adding one AddDef line and one .emat. The thresholds here are
//! defaults; the stats backend can override them for every server at once.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinCatalog
{
	static const int SKIN_NONE = 0;
	static const int SKIN_HUEY_TAN = 1;

	protected static ref array<ref IA_HeliSkinDef> s_aDefs;

	//------------------------------------------------------------------------------------------------
	protected static void EnsureDefs()
	{
		if (s_aDefs)
			return;

		s_aDefs = new array<ref IA_HeliSkinDef>();
		// Slot name is the vanilla body material plus its GUID suffix; the prefix
		// matches it on every UH-1H variant, including the shark-nose prefabs.
		AddDef(SKIN_HUEY_TAN, "huey_tan", "Desert Tan Huey", "/UH1H/", "UH_1H_Body01", "Assets/Vehicles/Helicopters/UH1H/IA_UH_1H_Body01_Tan.emat", 50000);
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddDef(int id, string key, string displayName, string prefabToken, string slotPrefix, ResourceName material, int requiredPoints)
	{
		ref IA_HeliSkinDef def = new IA_HeliSkinDef();
		def.m_iId = id;
		def.m_sKey = key;
		def.m_sDisplayName = displayName;
		def.m_sPrefabToken = prefabToken;
		def.m_sSlotPrefix = slotPrefix;
		def.m_sMaterial = material;
		def.m_iRequiredPoints = requiredPoints;
		s_aDefs.Insert(def);
	}

	//------------------------------------------------------------------------------------------------
	static array<ref IA_HeliSkinDef> GetDefs()
	{
		EnsureDefs();
		return s_aDefs;
	}

	//------------------------------------------------------------------------------------------------
	static IA_HeliSkinDef FindDef(int id)
	{
		EnsureDefs();
		foreach (IA_HeliSkinDef def : s_aDefs)
		{
			if (def.m_iId == id)
				return def;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	static IA_HeliSkinDef FindDefByKey(string key)
	{
		EnsureDefs();
		foreach (IA_HeliSkinDef def : s_aDefs)
		{
			if (def.m_sKey == key)
				return def;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsUnlocked(IA_HeliSkinDef def, int points)
	{
		if (!def)
			return false;
		return points >= def.m_iRequiredPoints;
	}

	//------------------------------------------------------------------------------------------------
	static bool FitsPrefab(IA_HeliSkinDef def, string prefabName)
	{
		if (!def || prefabName.IsEmpty())
			return false;
		return prefabName.Contains(def.m_sPrefabToken);
	}

	//------------------------------------------------------------------------------------------------
	//! Apply a centrally configured threshold. Unknown keys and nonsense values are ignored.
	static void SetRequiredPoints(string key, int requiredPoints)
	{
		IA_HeliSkinDef def = FindDefByKey(key);
		if (def && requiredPoints > 0)
			def.m_iRequiredPoints = requiredPoints;
	}

	//------------------------------------------------------------------------------------------------
	//! Best skin this rating has unlocked for this airframe; null keeps stock paint.
	static IA_HeliSkinDef ResolveForPilot(int points, string prefabName)
	{
		EnsureDefs();
		IA_HeliSkinDef best = null;
		foreach (IA_HeliSkinDef def : s_aDefs)
		{
			if (!IsUnlocked(def, points) || !FitsPrefab(def, prefabName))
				continue;
			if (!best || def.m_iRequiredPoints > best.m_iRequiredPoints)
				best = def;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Skin whose threshold lies in (before, after]; null when none was crossed.
	static IA_HeliSkinDef FindNewlyUnlocked(int before, int after)
	{
		EnsureDefs();
		foreach (IA_HeliSkinDef def : s_aDefs)
		{
			if (before < def.m_iRequiredPoints && after >= def.m_iRequiredPoints)
				return def;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Cheapest skin still locked at this total; null when everything is unlocked.
	static IA_HeliSkinDef FindNextLocked(int points)
	{
		EnsureDefs();
		IA_HeliSkinDef next = null;
		foreach (IA_HeliSkinDef def : s_aDefs)
		{
			if (points >= def.m_iRequiredPoints)
				continue;
			if (!next || def.m_iRequiredPoints < next.m_iRequiredPoints)
				next = def;
		}
		return next;
	}
}
