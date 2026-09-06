//------------------------------------------------------------------------------------------------
//! Last-wins admin snapshot written to the server profile. Loaded after the mission
//! IA_Config .conf so the in-game Admin Config menu is the live source of truth.
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
	string m_sEnemyFactionKeysPacked;
	string m_sEnemyVehicleFactionKeysPacked;
	int m_iCivilianRevoltNotificationDelay = 30000;
	int m_iCivilianRevoltReinforcementDelay = 180000;
	int m_iHaloJumpMaxPlayers = IA_Config.HALO_JUMP_MAX_PLAYERS_DEFAULT;
	bool m_bHasFactionOverride;
	bool m_bHasVehicleFactionOverride;
	bool m_bHasRevoltNotifOverride;
	bool m_bHasRevoltReinfOverride;
	bool m_bGameMasterMode;
	bool m_bGmAutoActivateStaging;
	bool m_bGmAutoQrf = true;
	bool m_bGmAutoArty = true;
	bool m_bGmAutoSideMissions;
	bool m_bGmAutoPlaceSupport;
	bool m_bHasDefenseOverride;
	bool m_bUseLegacyDefense;
	int m_iDefendDurationMinMin = 18;
	int m_iDefendDurationMaxMin = 22;
	int m_iDefendPrepareMinSec = 60;
	int m_iDefendPrepareMaxSec = 90;
	int m_iDefendGuaranteedEvents = 2;
	float m_fDefendThirdChanceLow = 0.25;
	float m_fDefendThirdChanceMid = 0.50;
	float m_fDefendThirdChanceHigh = 0.75;
	int m_iDefendPrioritySuccessMinSec = 120;
	int m_iDefendPrioritySuccessMaxSec = 240;
	int m_iDefendPriorityFailMinSec = 180;
	int m_iDefendPriorityFailMaxSec = 300;
	int m_iDefendDoctrineMask = 15;
	int m_iDefendEventMask = 63;
	float m_fDefendHotDropChance = 0.20;
	bool m_bHasAiCombatOverride;
	int m_iAiSkillNormal = 70;
	int m_iAiSkillElite = 80;
	float m_fAiFireRateNormal = 1.0;
	float m_fAiFireRateElite = 1.25;
	float m_fAiPerceptionNormal = 1.0;
	float m_fAiPerceptionElite = 1.5;
	bool m_bHasDynamicBaseOverride;
	bool m_bDynamicBaseEnabled = true;
	bool m_bDynamicBaseEmplacementsEnabled = true;
	int m_iDynamicBaseChancePct = 100;
	bool m_bDynamicBaseInGm;
	int m_iDynamicBaseSizeMode;
	int m_iDynamicBaseCaptureSec = 90;
	int m_iDynamicBaseRegroupMinSec = 90;
	int m_iDynamicBaseRegroupMaxSec = 240;
	float m_fDynamicBaseRegroupFraction = 0.60;
	float m_fDynamicBaseGarrisonMultiplier = 1.0;

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

		IA_Log.Info("[IA][AdminOverrides] Cleared " + CONFIG_PATH);
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
		m_bGameMasterMode = config.m_bGameMasterMode;
		m_bGmAutoActivateStaging = config.m_bGmAutoActivateStaging;
		m_bGmAutoQrf = config.m_bGmAutoQrf;
		m_bGmAutoArty = config.m_bGmAutoArty;
		m_bGmAutoSideMissions = config.m_bGmAutoSideMissions;
		m_bGmAutoPlaceSupport = config.m_bGmAutoPlaceSupport;
		m_sEnemyFactionKeysPacked = IA_AdminConfigUtil.JoinKeys(config.m_sDesiredEnemyFactionKeys);
		m_sEnemyVehicleFactionKeysPacked = IA_AdminConfigUtil.JoinKeys(config.m_sDesiredEnemyVehicleFactionKeys);
		m_sEnemyFactionKey = IA_AdminConfigUtil.FirstKey(m_sEnemyFactionKeysPacked);
		m_iCivilianRevoltNotificationDelay = config.m_iCivilianRevoltNotificationDelay;
		m_iCivilianRevoltReinforcementDelay = config.m_iCivilianRevoltReinforcementDelay;
		m_bHasFactionOverride = true;
		m_bHasVehicleFactionOverride = true;
		m_bHasRevoltNotifOverride = true;
		m_bHasRevoltReinfOverride = true;
		m_bHasDefenseOverride = true;
		m_bUseLegacyDefense = config.m_bUseLegacyDefense;
		m_iDefendDurationMinMin = config.m_iDefendDurationMinMin;
		m_iDefendDurationMaxMin = config.m_iDefendDurationMaxMin;
		m_iDefendPrepareMinSec = config.m_iDefendPrepareMinSec;
		m_iDefendPrepareMaxSec = config.m_iDefendPrepareMaxSec;
		m_iDefendGuaranteedEvents = config.m_iDefendGuaranteedEvents;
		m_fDefendThirdChanceLow = config.m_fDefendThirdChanceLow;
		m_fDefendThirdChanceMid = config.m_fDefendThirdChanceMid;
		m_fDefendThirdChanceHigh = config.m_fDefendThirdChanceHigh;
		m_iDefendPrioritySuccessMinSec = config.m_iDefendPrioritySuccessMinSec;
		m_iDefendPrioritySuccessMaxSec = config.m_iDefendPrioritySuccessMaxSec;
		m_iDefendPriorityFailMinSec = config.m_iDefendPriorityFailMinSec;
		m_iDefendPriorityFailMaxSec = config.m_iDefendPriorityFailMaxSec;
		m_iDefendDoctrineMask = config.GetDefenseDoctrineMask();
		m_iDefendEventMask = config.GetDefenseEventMask();
		m_fDefendHotDropChance = config.m_fDefendHotDropChance;
		m_bHasAiCombatOverride = true;
		config.ClampAiCombatSettings();
		m_iAiSkillNormal = config.m_eAiSkillNormal;
		m_iAiSkillElite = config.m_eAiSkillElite;
		m_fAiFireRateNormal = config.m_fAiFireRateNormal;
		m_fAiFireRateElite = config.m_fAiFireRateElite;
		m_fAiPerceptionNormal = config.m_fAiPerceptionNormal;
		m_fAiPerceptionElite = config.m_fAiPerceptionElite;
		m_bHasDynamicBaseOverride = true;
		config.ClampDynamicBaseSettings();
		m_bDynamicBaseEnabled = config.m_bDynamicBaseEnabled;
		m_bDynamicBaseEmplacementsEnabled = config.m_bDynamicBaseEmplacementsEnabled;
		m_iDynamicBaseChancePct = config.m_iDynamicBaseChancePct;
		m_bDynamicBaseInGm = config.m_bDynamicBaseInGm;
		m_iDynamicBaseSizeMode = config.m_iDynamicBaseSizeMode;
		m_iDynamicBaseCaptureSec = config.m_iDynamicBaseCaptureSec;
		m_iDynamicBaseRegroupMinSec = config.m_iDynamicBaseRegroupMinSec;
		m_iDynamicBaseRegroupMaxSec = config.m_iDynamicBaseRegroupMaxSec;
		m_fDynamicBaseRegroupFraction = config.m_fDynamicBaseRegroupFraction;
		m_fDynamicBaseGarrisonMultiplier = config.m_fDynamicBaseGarrisonMultiplier;
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

		config.m_bGameMasterMode = m_bGameMasterMode;
		config.m_bGmAutoActivateStaging = m_bGmAutoActivateStaging;
		config.m_bGmAutoQrf = m_bGmAutoQrf;
		config.m_bGmAutoArty = m_bGmAutoArty;
		config.m_bGmAutoSideMissions = m_bGmAutoSideMissions;
		config.m_bGmAutoPlaceSupport = m_bGmAutoPlaceSupport;

		if (m_bHasRevoltNotifOverride)
			config.m_iCivilianRevoltNotificationDelay = m_iCivilianRevoltNotificationDelay;
		if (m_bHasRevoltReinfOverride)
			config.m_iCivilianRevoltReinforcementDelay = m_iCivilianRevoltReinforcementDelay;

		if (m_bHasFactionOverride)
		{
			string packed = m_sEnemyFactionKeysPacked;
			if (packed.IsEmpty())
				packed = m_sEnemyFactionKey;
			IA_AdminConfigUtil.ApplyPackedKeys(packed, config, false);
		}

		if (m_bHasVehicleFactionOverride)
			IA_AdminConfigUtil.ApplyPackedKeys(m_sEnemyVehicleFactionKeysPacked, config, true);

		if (m_bHasDefenseOverride)
		{
			config.m_bUseLegacyDefense = m_bUseLegacyDefense;
			config.m_iDefendDurationMinMin = m_iDefendDurationMinMin;
			config.m_iDefendDurationMaxMin = m_iDefendDurationMaxMin;
			config.m_iDefendPrepareMinSec = m_iDefendPrepareMinSec;
			config.m_iDefendPrepareMaxSec = m_iDefendPrepareMaxSec;
			config.m_iDefendGuaranteedEvents = m_iDefendGuaranteedEvents;
			config.m_fDefendThirdChanceLow = m_fDefendThirdChanceLow;
			config.m_fDefendThirdChanceMid = m_fDefendThirdChanceMid;
			config.m_fDefendThirdChanceHigh = m_fDefendThirdChanceHigh;
			config.m_iDefendPrioritySuccessMinSec = m_iDefendPrioritySuccessMinSec;
			config.m_iDefendPrioritySuccessMaxSec = m_iDefendPrioritySuccessMaxSec;
			config.m_iDefendPriorityFailMinSec = m_iDefendPriorityFailMinSec;
			config.m_iDefendPriorityFailMaxSec = m_iDefendPriorityFailMaxSec;
			config.SetDefenseDoctrineMask(m_iDefendDoctrineMask);
			config.SetDefenseEventMask(m_iDefendEventMask);
			config.m_fDefendHotDropChance = m_fDefendHotDropChance;
			config.ClampDefenseSettings();
		}

		if (m_bHasAiCombatOverride)
		{
			config.m_eAiSkillNormal = m_iAiSkillNormal;
			config.m_eAiSkillElite = m_iAiSkillElite;
			config.m_fAiFireRateNormal = m_fAiFireRateNormal;
			config.m_fAiFireRateElite = m_fAiFireRateElite;
			config.m_fAiPerceptionNormal = m_fAiPerceptionNormal;
			config.m_fAiPerceptionElite = m_fAiPerceptionElite;
			config.ClampAiCombatSettings();
		}

		if (m_bHasDynamicBaseOverride)
		{
			config.m_bDynamicBaseEnabled = m_bDynamicBaseEnabled;
			config.m_bDynamicBaseEmplacementsEnabled = m_bDynamicBaseEmplacementsEnabled;
			config.m_iDynamicBaseChancePct = m_iDynamicBaseChancePct;
			config.m_bDynamicBaseInGm = m_bDynamicBaseInGm;
			config.m_iDynamicBaseSizeMode = m_iDynamicBaseSizeMode;
			config.m_iDynamicBaseCaptureSec = m_iDynamicBaseCaptureSec;
			config.m_iDynamicBaseRegroupMinSec = m_iDynamicBaseRegroupMinSec;
			config.m_iDynamicBaseRegroupMaxSec = m_iDynamicBaseRegroupMaxSec;
			config.m_fDynamicBaseRegroupFraction = m_fDynamicBaseRegroupFraction;
			config.m_fDynamicBaseGarrisonMultiplier = m_fDynamicBaseGarrisonMultiplier;
			config.ClampDynamicBaseSettings();
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
		IA_Log.Info("[IA][AdminOverrides] Saved " + CONFIG_PATH);
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
		json = json + "\"v\":2";
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
		int gmI = 0;
		if (m_bGameMasterMode)
			gmI = 1;
		int gmActI = 0;
		if (m_bGmAutoActivateStaging)
			gmActI = 1;
		int gmQrfI = 0;
		if (m_bGmAutoQrf)
			gmQrfI = 1;
		int gmArtyI = 0;
		if (m_bGmAutoArty)
			gmArtyI = 1;
		int gmSideI = 0;
		if (m_bGmAutoSideMissions)
			gmSideI = 1;
		int gmSupI = 0;
		if (m_bGmAutoPlaceSupport)
			gmSupI = 1;
		json = json + ",\"gmMode\":" + gmI.ToString();
		json = json + ",\"gmAutoActivate\":" + gmActI.ToString();
		json = json + ",\"gmAutoQrf\":" + gmQrfI.ToString();
		json = json + ",\"gmAutoArty\":" + gmArtyI.ToString();
		json = json + ",\"gmAutoSide\":" + gmSideI.ToString();
		json = json + ",\"gmAutoSupport\":" + gmSupI.ToString();
		json = json + ",\"revoltNotif\":" + m_iCivilianRevoltNotificationDelay.ToString();
		json = json + ",\"revoltReinf\":" + m_iCivilianRevoltReinforcementDelay.ToString();
		json = json + ",\"faction\":\"" + m_sEnemyFactionKey + "\"";
		json = json + ",\"factionKeys\":\"" + m_sEnemyFactionKeysPacked + "\"";
		json = json + ",\"vehicleFactionKeys\":\"" + m_sEnemyVehicleFactionKeysPacked + "\"";
		int legacyI = 0;
		if (m_bUseLegacyDefense)
			legacyI = 1;
		json = json + ",\"defendLegacy\":" + legacyI.ToString();
		json = json + ",\"defendDurMin\":" + m_iDefendDurationMinMin.ToString();
		json = json + ",\"defendDurMax\":" + m_iDefendDurationMaxMin.ToString();
		json = json + ",\"defendPrepMin\":" + m_iDefendPrepareMinSec.ToString();
		json = json + ",\"defendPrepMax\":" + m_iDefendPrepareMaxSec.ToString();
		json = json + ",\"defendEvents\":" + m_iDefendGuaranteedEvents.ToString();
		json = json + ",\"defendThirdLow\":" + m_fDefendThirdChanceLow.ToString();
		json = json + ",\"defendThirdMid\":" + m_fDefendThirdChanceMid.ToString();
		json = json + ",\"defendThirdHigh\":" + m_fDefendThirdChanceHigh.ToString();
		json = json + ",\"defendPriSuccMin\":" + m_iDefendPrioritySuccessMinSec.ToString();
		json = json + ",\"defendPriSuccMax\":" + m_iDefendPrioritySuccessMaxSec.ToString();
		json = json + ",\"defendPriFailMin\":" + m_iDefendPriorityFailMinSec.ToString();
		json = json + ",\"defendPriFailMax\":" + m_iDefendPriorityFailMaxSec.ToString();
		json = json + ",\"defendDocMask\":" + m_iDefendDoctrineMask.ToString();
		json = json + ",\"defendEvtMask\":" + m_iDefendEventMask.ToString();
		json = json + ",\"defendHotDrop\":" + m_fDefendHotDropChance.ToString();
		json = json + ",\"aiSkillN\":" + m_iAiSkillNormal.ToString();
		json = json + ",\"aiSkillE\":" + m_iAiSkillElite.ToString();
		json = json + ",\"aiFireN\":" + m_fAiFireRateNormal.ToString();
		json = json + ",\"aiFireE\":" + m_fAiFireRateElite.ToString();
		json = json + ",\"aiPercN\":" + m_fAiPerceptionNormal.ToString();
		json = json + ",\"aiPercE\":" + m_fAiPerceptionElite.ToString();
		json = json + ",\"dynamicBaseVersion\":2";
		ref IA_Config extrasCfg = new IA_Config();
		extrasCfg.m_bDynamicBaseEnabled = m_bDynamicBaseEnabled;
		extrasCfg.m_bDynamicBaseEmplacementsEnabled = m_bDynamicBaseEmplacementsEnabled;
		extrasCfg.m_iDynamicBaseChancePct = m_iDynamicBaseChancePct;
		extrasCfg.m_bDynamicBaseInGm = m_bDynamicBaseInGm;
		extrasCfg.m_iDynamicBaseSizeMode = m_iDynamicBaseSizeMode;
		extrasCfg.m_iDynamicBaseCaptureSec = m_iDynamicBaseCaptureSec;
		extrasCfg.m_iDynamicBaseRegroupMinSec = m_iDynamicBaseRegroupMinSec;
		extrasCfg.m_iDynamicBaseRegroupMaxSec = m_iDynamicBaseRegroupMaxSec;
		extrasCfg.m_fDynamicBaseRegroupFraction = m_fDynamicBaseRegroupFraction;
		extrasCfg.m_fDynamicBaseGarrisonMultiplier = m_fDynamicBaseGarrisonMultiplier;
		json = json + ",\"dynamicBaseExtras\":\"" + IA_Config.PackDynamicBaseExtras(extrasCfg) + "\"";
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
		if (HasKey(json, "gmMode"))
			m_bGameMasterMode = ExtractValue(json, "gmMode").ToInt() != 0;
		if (HasKey(json, "gmAutoActivate"))
			m_bGmAutoActivateStaging = ExtractValue(json, "gmAutoActivate").ToInt() != 0;
		if (HasKey(json, "gmAutoQrf"))
			m_bGmAutoQrf = ExtractValue(json, "gmAutoQrf").ToInt() != 0;
		if (HasKey(json, "gmAutoArty"))
			m_bGmAutoArty = ExtractValue(json, "gmAutoArty").ToInt() != 0;
		if (HasKey(json, "gmAutoSide"))
			m_bGmAutoSideMissions = ExtractValue(json, "gmAutoSide").ToInt() != 0;
		if (HasKey(json, "gmAutoSupport"))
			m_bGmAutoPlaceSupport = ExtractValue(json, "gmAutoSupport").ToInt() != 0;
		if (HasKey(json, "factionKeys"))
		{
			m_sEnemyFactionKeysPacked = ExtractValue(json, "factionKeys");
			m_bHasFactionOverride = true;
		}
		if (HasKey(json, "faction"))
		{
			m_sEnemyFactionKey = ExtractValue(json, "faction");
			if (!m_bHasFactionOverride)
				m_bHasFactionOverride = true;
		}
		if (HasKey(json, "vehicleFactionKeys"))
		{
			m_sEnemyVehicleFactionKeysPacked = ExtractValue(json, "vehicleFactionKeys");
			m_bHasVehicleFactionOverride = true;
		}
		if (HasKey(json, "revoltNotif"))
		{
			m_iCivilianRevoltNotificationDelay = ExtractValue(json, "revoltNotif").ToInt();
			m_bHasRevoltNotifOverride = true;
		}
		if (HasKey(json, "revoltReinf"))
		{
			m_iCivilianRevoltReinforcementDelay = ExtractValue(json, "revoltReinf").ToInt();
			m_bHasRevoltReinfOverride = true;
		}
		if (HasKey(json, "defendLegacy"))
		{
			m_bHasDefenseOverride = true;
			m_bUseLegacyDefense = ExtractValue(json, "defendLegacy").ToInt() != 0;
			if (HasKey(json, "defendDurMin"))
				m_iDefendDurationMinMin = ExtractValue(json, "defendDurMin").ToInt();
			if (HasKey(json, "defendDurMax"))
				m_iDefendDurationMaxMin = ExtractValue(json, "defendDurMax").ToInt();
			if (HasKey(json, "defendPrepMin"))
				m_iDefendPrepareMinSec = ExtractValue(json, "defendPrepMin").ToInt();
			if (HasKey(json, "defendPrepMax"))
				m_iDefendPrepareMaxSec = ExtractValue(json, "defendPrepMax").ToInt();
			if (HasKey(json, "defendEvents"))
				m_iDefendGuaranteedEvents = ExtractValue(json, "defendEvents").ToInt();
			if (HasKey(json, "defendThirdLow"))
				m_fDefendThirdChanceLow = ExtractValue(json, "defendThirdLow").ToFloat();
			if (HasKey(json, "defendThirdMid"))
				m_fDefendThirdChanceMid = ExtractValue(json, "defendThirdMid").ToFloat();
			if (HasKey(json, "defendThirdHigh"))
				m_fDefendThirdChanceHigh = ExtractValue(json, "defendThirdHigh").ToFloat();
			if (HasKey(json, "defendPriSuccMin"))
				m_iDefendPrioritySuccessMinSec = ExtractValue(json, "defendPriSuccMin").ToInt();
			if (HasKey(json, "defendPriSuccMax"))
				m_iDefendPrioritySuccessMaxSec = ExtractValue(json, "defendPriSuccMax").ToInt();
			if (HasKey(json, "defendPriFailMin"))
				m_iDefendPriorityFailMinSec = ExtractValue(json, "defendPriFailMin").ToInt();
			if (HasKey(json, "defendPriFailMax"))
				m_iDefendPriorityFailMaxSec = ExtractValue(json, "defendPriFailMax").ToInt();
			if (HasKey(json, "defendDocMask"))
				m_iDefendDoctrineMask = ExtractValue(json, "defendDocMask").ToInt();
			if (HasKey(json, "defendEvtMask"))
				m_iDefendEventMask = ExtractValue(json, "defendEvtMask").ToInt();
			if (HasKey(json, "defendHotDrop"))
				m_fDefendHotDropChance = ExtractValue(json, "defendHotDrop").ToFloat();
		}
		if (HasKey(json, "aiSkillN"))
		{
			m_bHasAiCombatOverride = true;
			m_iAiSkillNormal = ExtractValue(json, "aiSkillN").ToInt();
			if (HasKey(json, "aiSkillE"))
				m_iAiSkillElite = ExtractValue(json, "aiSkillE").ToInt();
			if (HasKey(json, "aiFireN"))
				m_fAiFireRateNormal = ExtractValue(json, "aiFireN").ToFloat();
			if (HasKey(json, "aiFireE"))
				m_fAiFireRateElite = ExtractValue(json, "aiFireE").ToFloat();
			if (HasKey(json, "aiPercN"))
				m_fAiPerceptionNormal = ExtractValue(json, "aiPercN").ToFloat();
			if (HasKey(json, "aiPercE"))
				m_fAiPerceptionElite = ExtractValue(json, "aiPercE").ToFloat();
		}
		if (HasKey(json, "dynamicBaseVersion"))
		{
			int dbVersion = ExtractValue(json, "dynamicBaseVersion").ToInt();
			if ((dbVersion == 1 || dbVersion == 2) && HasKey(json, "dynamicBaseExtras"))
			{
				string extras = ExtractValue(json, "dynamicBaseExtras");
				ref array<string> extraParts = new array<string>();
				extras.Split(",", extraParts, false);
				if ((dbVersion == 1 && extraParts.Count() != 10) || (dbVersion == 2 && extraParts.Count() != 11))
					Print("[IA][AdminOverrides] dynamicBaseExtras malformed — keeping script defaults.", LogLevel.WARNING);
				else
				{
					ref IA_Config parsed = new IA_Config();
					IA_Config.UnpackDynamicBaseExtras(parsed, extras);
					m_bHasDynamicBaseOverride = true;
					m_bDynamicBaseEnabled = parsed.m_bDynamicBaseEnabled;
					m_bDynamicBaseEmplacementsEnabled = parsed.m_bDynamicBaseEmplacementsEnabled;
					m_iDynamicBaseChancePct = parsed.m_iDynamicBaseChancePct;
					m_bDynamicBaseInGm = parsed.m_bDynamicBaseInGm;
					m_iDynamicBaseSizeMode = parsed.m_iDynamicBaseSizeMode;
					m_iDynamicBaseCaptureSec = parsed.m_iDynamicBaseCaptureSec;
					m_iDynamicBaseRegroupMinSec = parsed.m_iDynamicBaseRegroupMinSec;
					m_iDynamicBaseRegroupMaxSec = parsed.m_iDynamicBaseRegroupMaxSec;
					m_fDynamicBaseRegroupFraction = parsed.m_fDynamicBaseRegroupFraction;
					m_fDynamicBaseGarrisonMultiplier = parsed.m_fDynamicBaseGarrisonMultiplier;
				}
			}
			else
				Print("[IA][AdminOverrides] Unsupported dynamicBaseVersion — keeping script defaults.", LogLevel.WARNING);
		}
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
