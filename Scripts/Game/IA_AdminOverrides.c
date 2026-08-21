//------------------------------------------------------------------------------------------------
//! Last-wins admin snapshot written to the server profile. Loaded after IA_Config.conf.
//! Path: $profile:MikesInvadeAndAnnex/admin_overrides.json
//------------------------------------------------------------------------------------------------
class IA_AdminOverrides
{
	protected static const string CONFIG_DIR = "$profile:MikesInvadeAndAnnex";
	protected static const string CONFIG_PATH = CONFIG_DIR + "/admin_overrides.json";

	float m_fCivilianCountMultiplier = 1.0;
	float m_fAIScaleMultiplier = 1.0;
	bool m_bDisableHQHelipads;
	bool m_bDisableHQGroundVehicles;
	int m_iArtilleryCooldown = 300;
	float m_fStaticAIScaleOverride;
	float m_fMilitaryVehicleCountMultiplier = 1.0;
	float m_fCivilianVehicleCountMultiplier = 1.0;
	float m_fCivilianRevoltThreshold = 0.11;
	bool m_bEnableCivilianSpawning = true;
	bool m_bEnforceRoleRestrictions;
	float m_fArtilleryStrikeChance = 0.18;
	int m_iArtilleryMinDelay = 45;
	int m_iArtilleryMaxDelay = 70;
	string m_sEnemyFactionKey;
	int m_iHaloJumpMaxPlayers = 12;

	//------------------------------------------------------------------------------------------------
	static string GetPath()
	{
		return CONFIG_PATH;
	}

	//------------------------------------------------------------------------------------------------
	static bool FileExists()
	{
		return FileIO.FileExists(CONFIG_PATH);
	}

	//------------------------------------------------------------------------------------------------
	static bool ApplyIfPresent(notnull IA_Config config)
	{
		ref IA_AdminOverrides overlay = Load();
		if (!overlay)
			return false;

		overlay.ApplyTo(config);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool SaveFrom(notnull IA_Config config)
	{
		ref IA_AdminOverrides overlay = new IA_AdminOverrides();
		overlay.FillFrom(config);
		return overlay.WriteFile();
	}

	//------------------------------------------------------------------------------------------------
	static bool ClearFile()
	{
		if (!FileExists())
			return true;

		if (!FileIO.DeleteFile(CONFIG_PATH))
		{
			Print("[IA][AdminOverrides] Failed to delete " + CONFIG_PATH, LogLevel.ERROR);
			return false;
		}

		Print("[IA][AdminOverrides] Cleared " + CONFIG_PATH, LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static IA_AdminOverrides Load()
	{
		FileHandle file = FileIO.OpenFile(CONFIG_PATH, FileMode.READ);
		if (!file)
			return null;

		string fileContent;
		string line;
		while (file.ReadLine(line) > -1)
		{
			fileContent = fileContent + line;
		}
		file.Close();

		if (fileContent.IsEmpty())
			return null;

		ref IA_AdminOverrides overlay = new IA_AdminOverrides();
		overlay.ParseJson(fileContent);
		return overlay;
	}

	//------------------------------------------------------------------------------------------------
	void FillFrom(notnull IA_Config config)
	{
		m_fCivilianCountMultiplier = config.m_fCivilianCountMultiplier;
		m_fAIScaleMultiplier = config.m_fAIScaleMultiplier;
		m_bDisableHQHelipads = config.m_bDisableHQHelipads;
		m_bDisableHQGroundVehicles = config.m_bDisableHQGroundVehicles;
		m_iArtilleryCooldown = config.m_iArtilleryCooldown;
		m_fStaticAIScaleOverride = config.m_fStaticAIScaleOverride;
		m_fMilitaryVehicleCountMultiplier = config.m_fMilitaryVehicleCountMultiplier;
		m_fCivilianVehicleCountMultiplier = config.m_fCivilianVehicleCountMultiplier;
		m_fCivilianRevoltThreshold = config.m_fCivilianRevoltThreshold;
		m_bEnableCivilianSpawning = config.m_bEnableCivilianSpawning;
		m_bEnforceRoleRestrictions = config.m_bEnforceRoleRestrictions;
		m_fArtilleryStrikeChance = config.m_fArtilleryStrikeChance;
		m_iArtilleryMinDelay = config.m_iArtilleryMinDelay;
		m_iArtilleryMaxDelay = config.m_iArtilleryMaxDelay;
		m_iHaloJumpMaxPlayers = config.m_iHaloJumpMaxPlayers;
		m_sEnemyFactionKey = "";
		if (config.m_sDesiredEnemyFactionKeys && config.m_sDesiredEnemyFactionKeys.Count() > 0)
			m_sEnemyFactionKey = config.m_sDesiredEnemyFactionKeys[0];
	}

	//------------------------------------------------------------------------------------------------
	void ApplyTo(notnull IA_Config config)
	{
		config.m_fCivilianCountMultiplier = m_fCivilianCountMultiplier;
		config.m_fAIScaleMultiplier = m_fAIScaleMultiplier;
		config.m_bDisableHQHelipads = m_bDisableHQHelipads;
		config.m_bDisableHQGroundVehicles = m_bDisableHQGroundVehicles;
		config.m_iArtilleryCooldown = m_iArtilleryCooldown;
		config.m_fStaticAIScaleOverride = m_fStaticAIScaleOverride;
		config.m_fMilitaryVehicleCountMultiplier = m_fMilitaryVehicleCountMultiplier;
		config.m_fCivilianVehicleCountMultiplier = m_fCivilianVehicleCountMultiplier;
		config.m_fCivilianRevoltThreshold = m_fCivilianRevoltThreshold;
		config.m_bEnableCivilianSpawning = m_bEnableCivilianSpawning;
		config.m_bEnforceRoleRestrictions = m_bEnforceRoleRestrictions;
		config.m_fArtilleryStrikeChance = m_fArtilleryStrikeChance;
		config.m_iArtilleryMinDelay = m_iArtilleryMinDelay;
		config.m_iArtilleryMaxDelay = m_iArtilleryMaxDelay;

		int haloMax = m_iHaloJumpMaxPlayers;
		if (haloMax < 0)
			haloMax = 0;
		if (haloMax > 128)
			haloMax = 128;
		config.m_iHaloJumpMaxPlayers = haloMax;

		if (m_sEnemyFactionKey != "")
		{
			if (!config.m_sDesiredEnemyFactionKeys)
				config.m_sDesiredEnemyFactionKeys = new array<string>();
			config.m_sDesiredEnemyFactionKeys.Clear();
			config.m_sDesiredEnemyFactionKeys.Insert(m_sEnemyFactionKey);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool WriteFile()
	{
		FileIO.MakeDirectory(CONFIG_DIR);

		FileHandle file = FileIO.OpenFile(CONFIG_PATH, FileMode.WRITE);
		if (!file)
		{
			Print("[IA][AdminOverrides] Failed to write " + CONFIG_PATH, LogLevel.ERROR);
			return false;
		}

		file.WriteLine(ToJson());
		file.Close();
		Print("[IA][AdminOverrides] Saved " + CONFIG_PATH, LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected string ToJson()
	{
		int heliI = 0;
		if (m_bDisableHQHelipads)
			heliI = 1;
		int groundI = 0;
		if (m_bDisableHQGroundVehicles)
			groundI = 1;
		int civI = 0;
		if (m_bEnableCivilianSpawning)
			civI = 1;
		int rolesI = 0;
		if (m_bEnforceRoleRestrictions)
			rolesI = 1;

		string json = "{";
		json = json + "\"v\":1";
		json = json + ",\"civCount\":" + m_fCivilianCountMultiplier.ToString();
		json = json + ",\"aiScale\":" + m_fAIScaleMultiplier.ToString();
		json = json + ",\"disableHeli\":" + heliI.ToString();
		json = json + ",\"disableGround\":" + groundI.ToString();
		json = json + ",\"artyCooldown\":" + m_iArtilleryCooldown.ToString();
		json = json + ",\"staticAi\":" + m_fStaticAIScaleOverride.ToString();
		json = json + ",\"milVeh\":" + m_fMilitaryVehicleCountMultiplier.ToString();
		json = json + ",\"civVeh\":" + m_fCivilianVehicleCountMultiplier.ToString();
		json = json + ",\"revolt\":" + m_fCivilianRevoltThreshold.ToString();
		json = json + ",\"enableCiv\":" + civI.ToString();
		json = json + ",\"enforceRoles\":" + rolesI.ToString();
		json = json + ",\"artyChance\":" + m_fArtilleryStrikeChance.ToString();
		json = json + ",\"artyMin\":" + m_iArtilleryMinDelay.ToString();
		json = json + ",\"artyMax\":" + m_iArtilleryMaxDelay.ToString();
		json = json + ",\"haloMax\":" + m_iHaloJumpMaxPlayers.ToString();
		json = json + ",\"faction\":\"" + m_sEnemyFactionKey + "\"";
		json = json + "}";
		return json;
	}

	//------------------------------------------------------------------------------------------------
	protected void ParseJson(string json)
	{
		if (HasKey(json, "civCount"))
			m_fCivilianCountMultiplier = ExtractValue(json, "civCount").ToFloat();
		if (HasKey(json, "aiScale"))
			m_fAIScaleMultiplier = ExtractValue(json, "aiScale").ToFloat();
		if (HasKey(json, "disableHeli"))
			m_bDisableHQHelipads = ExtractValue(json, "disableHeli").ToInt() != 0;
		if (HasKey(json, "disableGround"))
			m_bDisableHQGroundVehicles = ExtractValue(json, "disableGround").ToInt() != 0;
		if (HasKey(json, "artyCooldown"))
			m_iArtilleryCooldown = ExtractValue(json, "artyCooldown").ToInt();
		if (HasKey(json, "staticAi"))
			m_fStaticAIScaleOverride = ExtractValue(json, "staticAi").ToFloat();
		if (HasKey(json, "milVeh"))
			m_fMilitaryVehicleCountMultiplier = ExtractValue(json, "milVeh").ToFloat();
		if (HasKey(json, "civVeh"))
			m_fCivilianVehicleCountMultiplier = ExtractValue(json, "civVeh").ToFloat();
		if (HasKey(json, "revolt"))
			m_fCivilianRevoltThreshold = ExtractValue(json, "revolt").ToFloat();
		if (HasKey(json, "enableCiv"))
			m_bEnableCivilianSpawning = ExtractValue(json, "enableCiv").ToInt() != 0;
		if (HasKey(json, "enforceRoles"))
			m_bEnforceRoleRestrictions = ExtractValue(json, "enforceRoles").ToInt() != 0;
		if (HasKey(json, "artyChance"))
			m_fArtilleryStrikeChance = ExtractValue(json, "artyChance").ToFloat();
		if (HasKey(json, "artyMin"))
			m_iArtilleryMinDelay = ExtractValue(json, "artyMin").ToInt();
		if (HasKey(json, "artyMax"))
			m_iArtilleryMaxDelay = ExtractValue(json, "artyMax").ToInt();
		if (HasKey(json, "haloMax"))
			m_iHaloJumpMaxPlayers = ExtractValue(json, "haloMax").ToInt();
		if (HasKey(json, "faction"))
			m_sEnemyFactionKey = ExtractValue(json, "faction");
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasKey(string json, string key)
	{
		return json.IndexOf("\"" + key + "\":") != -1;
	}

	//------------------------------------------------------------------------------------------------
	protected static string ExtractValue(string json, string key)
	{
		string needle = "\"" + key + "\":";
		int at = json.IndexOf(needle);
		if (at < 0)
			return string.Empty;

		int valueStart = at + needle.Length();
		if (valueStart >= json.Length())
			return string.Empty;

		string rest = json.Substring(valueStart, json.Length() - valueStart);
		rest.Replace(" ", "");
		rest.Replace("\t", "");
		if (rest.IsEmpty())
			return string.Empty;

		if (rest.Substring(0, 1) == "\"")
		{
			string inner = rest.Substring(1, rest.Length() - 1);
			int endQuote = inner.IndexOf("\"");
			if (endQuote < 0)
				return string.Empty;
			return inner.Substring(0, endQuote);
		}

		int endComma = rest.IndexOf(",");
		int endBrace = rest.IndexOf("}");
		int endAt = rest.Length();
		if (endComma >= 0)
			endAt = endComma;
		if (endBrace >= 0 && endBrace < endAt)
			endAt = endBrace;

		return rest.Substring(0, endAt);
	}
}
