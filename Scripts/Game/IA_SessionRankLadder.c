//------------------------------------------------------------------------------------------------
//! Session military grades. Uses vanilla SCR_ECharacterRank + West localization
//! (Pvt., Cpl., …). XP thresholds follow Conflict's BaseMilitaryRanks for Pvt–Maj.
//------------------------------------------------------------------------------------------------
class IA_SessionRankDef
{
	SCR_ECharacterRank m_eRank;
	int m_iRequiredXp;
	string m_sShortKey;
	string m_sNameKey;
	string m_sShortFallback;
	string m_sNameFallback;
}

//------------------------------------------------------------------------------------------------
class IA_SessionRankLadder
{
	protected static ref array<ref IA_SessionRankDef> s_aDefs;

	//------------------------------------------------------------------------------------------------
	protected static void EnsureDefs()
	{
		if (s_aDefs)
			return;

		s_aDefs = new array<ref IA_SessionRankDef>();
		AddDef(SCR_ECharacterRank.PRIVATE, 0, "#AR-Rank_WestPrivateShort", "#AR-Rank_WestPrivate", "Pvt.", "Private");
		AddDef(SCR_ECharacterRank.CORPORAL, 150, "#AR-Rank_WestCorporalShort", "#AR-Rank_WestCorporal", "Cpl.", "Corporal");
		AddDef(SCR_ECharacterRank.SERGEANT, 600, "#AR-Rank_WestSergeantShort", "#AR-Rank_WestSergeant", "Sgt.", "Sergeant");
		AddDef(SCR_ECharacterRank.LIEUTENANT, 1200, "#AR-Rank_WestLieutenantShort", "#AR-Rank_WestLieutenant", "Lt.", "Lieutenant");
		AddDef(SCR_ECharacterRank.CAPTAIN, 2100, "#AR-Rank_WestCaptainShort", "#AR-Rank_WestCaptain", "Cpt.", "Captain");
		AddDef(SCR_ECharacterRank.MAJOR, 3300, "#AR-Rank_WestMajorShort", "#AR-Rank_WestMajor", "Maj.", "Major");
		AddDef(SCR_ECharacterRank.COLONEL, 4800, "#AR-Rank_WestColonelShort", "#AR-Rank_WestColonel", "Col.", "Colonel");
		AddDef(SCR_ECharacterRank.GENERAL, 6600, "", "", "Gen.", "General");
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddDef(SCR_ECharacterRank rank, int xp, string shortKey, string nameKey, string shortFallback, string nameFallback)
	{
		ref IA_SessionRankDef def = new IA_SessionRankDef();
		def.m_eRank = rank;
		def.m_iRequiredXp = xp;
		def.m_sShortKey = shortKey;
		def.m_sNameKey = nameKey;
		def.m_sShortFallback = shortFallback;
		def.m_sNameFallback = nameFallback;
		s_aDefs.Insert(def);
	}

	//------------------------------------------------------------------------------------------------
	protected static IA_SessionRankDef FindDef(int rankId)
	{
		EnsureDefs();
		int i;
		for (i = 0; i < s_aDefs.Count(); i++)
		{
			if (s_aDefs[i].m_eRank == rankId)
				return s_aDefs[i];
		}
		return s_aDefs[0];
	}

	//------------------------------------------------------------------------------------------------
	static int GetRankByXp(int xp)
	{
		EnsureDefs();
		int found = SCR_ECharacterRank.PRIVATE;
		int i;
		for (i = 0; i < s_aDefs.Count(); i++)
		{
			if (xp >= s_aDefs[i].m_iRequiredXp)
				found = s_aDefs[i].m_eRank;
		}
		return found;
	}

	//------------------------------------------------------------------------------------------------
	static int GetRequiredXp(int rankId)
	{
		IA_SessionRankDef def = FindDef(rankId);
		if (!def)
			return 0;
		return def.m_iRequiredXp;
	}

	//------------------------------------------------------------------------------------------------
	static int GetNextRank(int rankId)
	{
		EnsureDefs();
		int i;
		for (i = 0; i < s_aDefs.Count(); i++)
		{
			if (s_aDefs[i].m_eRank != rankId)
				continue;
			if (i + 1 >= s_aDefs.Count())
				return SCR_ECharacterRank.INVALID;
			return s_aDefs[i + 1].m_eRank;
		}
		return SCR_ECharacterRank.INVALID;
	}

	//------------------------------------------------------------------------------------------------
	static float GetProgress(int xp)
	{
		int current = GetRankByXp(xp);
		int next = GetNextRank(current);
		if (next == SCR_ECharacterRank.INVALID)
			return 1;

		int curXp = GetRequiredXp(current);
		int nextXp = GetRequiredXp(next);
		int span = nextXp - curXp;
		if (span <= 0)
			return 1;

		float progress = xp - curXp;
		progress = progress / span;
		if (progress < 0)
			return 0;
		if (progress > 1)
			return 1;
		return progress;
	}

	//------------------------------------------------------------------------------------------------
	//! "0/150" toward the next grade, or total XP at MAX.
	static string GetXpPair(int xp)
	{
		int current = GetRankByXp(xp);
		int next = GetNextRank(current);
		if (next == SCR_ECharacterRank.INVALID)
			return xp.ToString();

		return xp.ToString() + "/" + GetRequiredXp(next).ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! "NEXT  Cpl." or "MAX".
	static string GetNextLabel(int xp)
	{
		int current = GetRankByXp(xp);
		int next = GetNextRank(current);
		if (next == SCR_ECharacterRank.INVALID)
			return "MAX";

		return "NEXT  " + GetShortName(next);
	}

	//------------------------------------------------------------------------------------------------
	static string GetShortName(int rankId)
	{
		IA_SessionRankDef def = FindDef(rankId);
		if (!def)
			return "Pvt.";
		return TranslateOr(def.m_sShortKey, def.m_sShortFallback);
	}

	//------------------------------------------------------------------------------------------------
	static string GetFullName(int rankId)
	{
		IA_SessionRankDef def = FindDef(rankId);
		if (!def)
			return "Private";
		return TranslateOr(def.m_sNameKey, def.m_sNameFallback);
	}

	//------------------------------------------------------------------------------------------------
	protected static string TranslateOr(string key, string fallback)
	{
		if (key.IsEmpty())
			return fallback;

		string translated = WidgetManager.Translate(key);
		if (translated.IsEmpty())
			return fallback;
		if (translated == key)
			return fallback;
		return translated;
	}
}
