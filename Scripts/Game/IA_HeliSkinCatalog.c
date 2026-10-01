//------------------------------------------------------------------------------------------------
//! Unlockable helicopter liveries, gated by the player's global transport
//! rating. A livery is one plain paint colour; every helicopter family with
//! paint channels takes it, so a new livery is one AddDef line and no files.
//! Patterns are not offered: a camouflage texture cannot be laid over
//! helicopters with different UV layouts (see docs/transport-pilot-progression.md).
//! The thresholds here are defaults; the stats backend can override them for
//! every server at once.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinCatalog
{
	static const int SKIN_NONE = 0;
	static const int SKIN_DESERT_TAN = 1;
	static const int SKIN_FOREST_GREEN = 2;
	static const int SKIN_FIELD_DRAB = 3;
	static const int SKIN_GUNSHIP_GREY = 4;
	static const int SKIN_NIGHT_BLACK = 5;

	protected static ref array<ref IA_HeliSkinDef> s_aDefs;

	//------------------------------------------------------------------------------------------------
	protected static void EnsureDefs()
	{
		if (s_aDefs)
			return;

		// In the order the paint bay shows them. Paint is a linear colour, the swatch sRGB bytes.
		// A swatch is drawn on a near-black panel, so the black one is lighter than the paint.
		// "huey_tan" is the key the backend already holds a threshold for.
		s_aDefs = new array<ref IA_HeliSkinDef>();
		AddDef(SKIN_FOREST_GREEN, "heli_green", "Forest Green", 5000, Vector(0.040, 0.064, 0.027), 70, 92, 58);
		AddDef(SKIN_FIELD_DRAB, "heli_drab", "Field Drab", 12000, Vector(0.110, 0.078, 0.048), 128, 106, 80);
		AddDef(SKIN_GUNSHIP_GREY, "heli_grey", "Gunship Grey", 25000, Vector(0.085, 0.092, 0.100), 112, 118, 124);
		AddDef(SKIN_DESERT_TAN, "huey_tan", "Desert Tan", 50000, Vector(0.205, 0.168, 0.117), 226, 192, 132);
		AddDef(SKIN_NIGHT_BLACK, "heli_black", "Night Black", 100000, Vector(0.012, 0.012, 0.013), 58, 60, 65);
	}

	//------------------------------------------------------------------------------------------------
	protected static IA_HeliSkinDef AddDef(int id, string key, string displayName, int requiredPoints, vector paint, int swatchR, int swatchG, int swatchB)
	{
		ref IA_HeliSkinDef def = new IA_HeliSkinDef();
		def.m_iId = id;
		def.m_sKey = key;
		def.m_sDisplayName = displayName;
		def.m_iRequiredPoints = requiredPoints;
		def.m_vPaint = paint;
		def.m_iSwatchR = swatchR;
		def.m_iSwatchG = swatchG;
		def.m_iSwatchB = swatchB;
		s_aDefs.Insert(def);
		return def;
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
	//! \return the livery with this display name, null when there is none; the pilot card is sent names
	static IA_HeliSkinDef FindDefByName(string displayName)
	{
		EnsureDefs();
		foreach (IA_HeliSkinDef def : s_aDefs)
		{
			if (def.m_sDisplayName == displayName)
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
	//! Highest skin whose threshold lies in (before, after]; null when none was crossed.
	static IA_HeliSkinDef FindNewlyUnlocked(int before, int after)
	{
		EnsureDefs();
		IA_HeliSkinDef crossed = null;
		foreach (IA_HeliSkinDef def : s_aDefs)
		{
			if (before >= def.m_iRequiredPoints || after < def.m_iRequiredPoints)
				continue;
			if (!crossed || def.m_iRequiredPoints > crossed.m_iRequiredPoints)
				crossed = def;
		}
		return crossed;
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
