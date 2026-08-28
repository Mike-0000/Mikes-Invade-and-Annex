//------------------------------------------------------------------------------------------------
//! Pack / unpack helpers for the in-game admin config. The mission .conf stays a
//! workshop baseline; packed payloads and $profile overrides are the live source.
//------------------------------------------------------------------------------------------------
class IA_AdminConfigUtil
{
	static const string FACTION_AUTO = "-";
	static const string CIV_FACTION_KEY = "CIV";

	//------------------------------------------------------------------------------------------------
	static bool IsAuto(string packed)
	{
		if (packed == FACTION_AUTO)
			return true;
		if (packed.IsEmpty())
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static string JoinKeys(array<string> keys)
	{
		if (!keys)
			return FACTION_AUTO;
		if (keys.IsEmpty())
			return FACTION_AUTO;

		string packed = "";
		int count = keys.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			string key = keys[i];
			if (key.IsEmpty())
				continue;
			if (key == FACTION_AUTO)
				continue;
			if (!packed.IsEmpty())
				packed = packed + ",";
			packed = packed + key;
		}

		if (packed.IsEmpty())
			return FACTION_AUTO;
		return packed;
	}

	//------------------------------------------------------------------------------------------------
	static void SplitKeys(string packed, notnull array<string> outKeys)
	{
		outKeys.Clear();
		if (IsAuto(packed))
			return;

		ref array<string> tokens = new array<string>();
		packed.Split(",", tokens, true);
		int count = tokens.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			string key = tokens[i];
			if (key.IsEmpty())
				continue;
			if (key == FACTION_AUTO)
				continue;
			if (ArrayContains(outKeys, key))
				continue;
			outKeys.Insert(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	static string FirstKey(string packed)
	{
		if (IsAuto(packed))
			return "";

		ref array<string> keys = new array<string>();
		SplitKeys(packed, keys);
		if (keys.IsEmpty())
			return "";
		return keys[0];
	}

	//------------------------------------------------------------------------------------------------
	static bool ArrayContains(array<string> keys, string key)
	{
		if (!keys)
			return false;
		if (key.IsEmpty())
			return false;

		int count = keys.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			if (keys[i] == key)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! packed "" = leave dest unchanged (legacy payloads). "-" = auto (clear).
	static void ApplyPackedKeys(string packed, notnull IA_Config config, bool vehicle)
	{
		if (packed.IsEmpty())
			return;

		ref array<string> keys = new array<string>();
		if (!IsAuto(packed))
			SplitKeys(packed, keys);

		if (vehicle)
			config.m_sDesiredEnemyVehicleFactionKeys = keys;
		else
			config.m_sDesiredEnemyFactionKeys = keys;
	}

	//------------------------------------------------------------------------------------------------
	static void CollectFactionChoices(EEntityCatalogType catalogType, int minEntries, notnull array<string> outKeys, notnull array<string> outNames)
	{
		outKeys.Clear();
		outNames.Clear();

		SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!factionManager)
		{
			InsertFallbackChoices(outKeys, outNames);
			return;
		}

		array<Faction> factions = {};
		factionManager.GetFactionsList(factions);
		int count = factions.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			Faction faction = factions[i];
			if (!faction)
				continue;

			string key = faction.GetFactionKey();
			if (key.IsEmpty())
				continue;
			if (key == CIV_FACTION_KEY)
				continue;
			if (ArrayContains(outKeys, key))
				continue;

			SCR_Faction scrFaction = SCR_Faction.Cast(faction);
			if (!scrFaction)
				continue;

			SCR_EntityCatalog catalog = scrFaction.GetFactionEntityCatalogOfType(catalogType, true);
			if (!catalog)
				continue;

			array<EEditableEntityLabel> excludedLabels = {};
			array<EEditableEntityLabel> includedLabels = {};
			array<SCR_EntityCatalogEntry> entries = {};
			catalog.GetFullFilteredEntityList(entries, includedLabels, excludedLabels);
			if (entries.Count() < minEntries)
				continue;

			string display = faction.GetFactionName();
			if (display.IsEmpty())
				display = key;
			else if (display.IndexOf("#") == 0)
				display = key;
			else
				display = display + "  (" + key + ")";

			outKeys.Insert(key);
			outNames.Insert(display);
		}

		if (outKeys.IsEmpty())
			InsertFallbackChoices(outKeys, outNames);
	}

	//------------------------------------------------------------------------------------------------
	protected static void InsertFallbackChoices(notnull array<string> outKeys, notnull array<string> outNames)
	{
		outKeys.Insert("USSR");
		outNames.Insert("USSR");
		outKeys.Insert("US");
		outNames.Insert("US");
		outKeys.Insert("FIA");
		outNames.Insert("FIA");
	}
}
