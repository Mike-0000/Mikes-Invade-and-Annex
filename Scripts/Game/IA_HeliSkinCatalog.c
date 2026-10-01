//------------------------------------------------------------------------------------------------
//! Unlockable helicopter skins, gated by the player's global transport rating.
//! Add a skin with one AddDef line, plus an AddPaint line for each surface it
//! recolours: a material that inherits the vanilla one and overrides colours.
//! The thresholds here are defaults; the stats backend can override them for
//! every server at once.
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
		IA_HeliSkinDef tan = AddDef(SKIN_HUEY_TAN, "huey_tan", "Desert Tan Huey", 50000);
		AddSwatch(tan, 226, 192, 132);
		AddPaint(tan, IA_HeliPaintChannels.SURFACE_BODY, "{E3D8521BB1595CE0}Assets/Vehicles/Helicopters/UH1H/IA_UH_1H_Body01_Tan.emat");
		AddPaint(tan, IA_HeliPaintChannels.SURFACE_INTERIOR_1, "{6F6BE8E1E361B2BE}Assets/Vehicles/Helicopters/UH1H/IA_UH_1H_Interior01_Tan.emat");
		AddPaint(tan, IA_HeliPaintChannels.SURFACE_INTERIOR_2, "{2F9BC9C4557D30F9}Assets/Vehicles/Helicopters/UH1H/IA_UH_1H_Interior02_Tan.emat");
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
	protected static void AddPaint(notnull IA_HeliSkinDef def, int surface, ResourceName paint)
	{
		def.m_aPaints[surface] = paint;
	}

	//------------------------------------------------------------------------------------------------
	//! Hull colour of the skin as sRGB bytes; the paint bay draws the skin with it.
	protected static void AddSwatch(notnull IA_HeliSkinDef def, int r, int g, int b)
	{
		def.m_iSwatchR = r;
		def.m_iSwatchG = g;
		def.m_iSwatchB = b;
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
	//! Apply a centrally configured threshold. Unknown keys and nonsense values are ignored.
	static void SetRequiredPoints(string key, int requiredPoints)
	{
		IA_HeliSkinDef def = FindDefByKey(key);
		if (def && requiredPoints > 0)
			def.m_iRequiredPoints = requiredPoints;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the thresholds in force as "key=points;key=points", for a client that draws them.
	static string PackThresholds()
	{
		EnsureDefs();
		string packed;
		foreach (IA_HeliSkinDef def : s_aDefs)
		{
			if (!packed.IsEmpty())
				packed = packed + ";";
			packed = packed + string.Format("%1=%2", def.m_sKey, def.m_iRequiredPoints);
		}
		return packed;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: take the server's thresholds. Unknown keys and nonsense values are ignored.
	static void ApplyPackedThresholds(string packed)
	{
		if (packed.IsEmpty())
			return;

		ref array<string> entries = {};
		packed.Split(";", entries, true);
		ref array<string> pair = {};
		foreach (string entry : entries)
		{
			pair.Clear();
			entry.Split("=", pair, true);
			if (pair.Count() == 2)
				SetRequiredPoints(pair[0], pair[1].ToInt());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Highest skin this total has unlocked; null keeps stock paint.
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
