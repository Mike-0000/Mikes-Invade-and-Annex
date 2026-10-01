//------------------------------------------------------------------------------------------------
//! Unlockable helicopter skins, gated by the player's global transport rating.
//! Add a skin with one AddDef line, plus an AddVariant line for each stock
//! airframe it has a painted prefab for. The thresholds here are defaults; the
//! stats backend can override them for every server at once.
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
		// The shark-nose gunships have no tan twin; they keep their own livery.
		IA_HeliSkinDef tan = AddDef(SKIN_HUEY_TAN, "huey_tan", "Desert Tan Huey", 50000);
		AddVariant(tan, "{70BAEEFC2D3FEE64}Prefabs/Vehicles/Helicopters/UH1H/UH1H.et", "{E8F5E7E2B4B0A17E}Prefabs/Vehicles/Helicopters/UH1H/IA_UH1H_Tan.et");
		AddVariant(tan, "{DDDD9B51F1234DF3}Prefabs/Vehicles/Helicopters/UH1H/UH1H_armed.et", "{C45342DCB1889E0E}Prefabs/Vehicles/Helicopters/UH1H/IA_UH1H_armed_Tan.et");
		AddVariant(tan, "{21E9A875C0A3C409}Prefabs/Vehicles/Helicopters/UH1H/UH1H_armed_gunship_HE.et", "{9D7D4C53209E655F}Prefabs/Vehicles/Helicopters/UH1H/IA_UH1H_armed_gunship_HE_Tan.et");
		AddVariant(tan, "{CB4D4CF7E887B2D0}Prefabs/Vehicles/Helicopters/UH1H/UH1H_armed_gunship_HEDP.et", "{1892B2259FA7E22A}Prefabs/Vehicles/Helicopters/UH1H/IA_UH1H_armed_gunship_HEDP_Tan.et");
	}

	//------------------------------------------------------------------------------------------------
	protected static IA_HeliSkinDef AddDef(int id, string key, string displayName, int requiredPoints)
	{
		ref IA_HeliSkinDef def = new IA_HeliSkinDef();
		def.m_iId = id;
		def.m_sKey = key;
		def.m_sDisplayName = displayName;
		def.m_iRequiredPoints = requiredPoints;
		s_aDefs.Insert(def);
		return def;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddVariant(notnull IA_HeliSkinDef def, ResourceName stockPrefab, ResourceName skinPrefab)
	{
		def.m_aStockPrefabs.Insert(stockPrefab);
		def.m_aSkinPrefabs.Insert(skinPrefab);
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
	//! \return true when the skin has a variant of this stock airframe
	static bool FitsPrefab(IA_HeliSkinDef def, ResourceName stockPrefab)
	{
		if (!def)
			return false;
		return !def.FindVariant(stockPrefab).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Skin a prefab wears; null for stock airframes and anything uncatalogued.
	static IA_HeliSkinDef FindDefBySkinPrefab(ResourceName prefab)
	{
		EnsureDefs();
		foreach (IA_HeliSkinDef def : s_aDefs)
		{
			if (!def.FindStock(prefab).IsEmpty())
				return def;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Stock airframe behind a prefab: itself when it is stock and has a skin, empty when it is neither.
	static ResourceName FindStockPrefab(ResourceName prefab)
	{
		EnsureDefs();
		ResourceName stock;
		foreach (IA_HeliSkinDef def : s_aDefs)
		{
			stock = def.FindStock(prefab);
			if (!stock.IsEmpty())
				return stock;
			if (FitsPrefab(def, prefab))
				return prefab;
		}
		return ResourceName.Empty;
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
	//! Best skin this rating has unlocked for this stock airframe; null keeps stock paint.
	static IA_HeliSkinDef ResolveForPilot(int points, ResourceName stockPrefab)
	{
		EnsureDefs();
		IA_HeliSkinDef best = null;
		foreach (IA_HeliSkinDef def : s_aDefs)
		{
			if (!IsUnlocked(def, points) || !FitsPrefab(def, stockPrefab))
				continue;
			if (!best || def.m_iRequiredPoints > best.m_iRequiredPoints)
				best = def;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Highest skin this total has unlocked on any airframe; null when none is.
	static IA_HeliSkinDef FindBestUnlocked(int points)
	{
		EnsureDefs();
		IA_HeliSkinDef best = null;
		foreach (IA_HeliSkinDef def : s_aDefs)
		{
			if (!IsUnlocked(def, points))
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
