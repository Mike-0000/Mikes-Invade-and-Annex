//------------------------------------------------------------------------------------------------
//! Shared enums and small records for the terminal BaseAssault objective.
//! One class-per-file is relaxed here so the runbook contract stays in one place.
//------------------------------------------------------------------------------------------------

enum IA_BaseObjectivePhase
{
	None,
	Placing,
	Seize,
	Regroup, // Retired wire value; capture now hands directly to defense.
	Warning, // Retired wire value; no base-only counterattack countdown.
	Defend,
	Completed,
	Cancelled,
	Failed
}

enum IA_DynamicObjectiveResult
{
	Completed,
	Fallback,
	Aborted,
	Failed
}

enum IA_BaseCancelReason
{
	None,
	AdminComplete,
	AdminDirectDefense,
	AoReplaced,
	MissionShutdown,
	RuntimeFailure
}

enum IA_BaseStatusReason
{
	None,
	Contested,
	AwaitingForces,
	Failed,
	ClearCommand
}

enum IA_DynamicSiteSizeMode
{
	Auto,
	Full,
	Compact,
	Courtyard,
	Roadside,
	CommandPost,
	RallyPost
}

enum IA_DynamicSiteGrounding
{
	UprightPad,
	TerrainSegment
}

enum IA_DynamicSiteModuleRole
{
	Hq,
	Radio,
	Barracks,
	Medical,
	MotorPool,
	Supply,
	Fuel,
	Cover,
	Tower,
	Dressing
}

class IA_BaseObjectiveSettings
{
	bool m_bEnabled = true;
	bool m_bEmplacementsEnabled = true;
	int m_iChancePct = 100;
	bool m_bInGm;
	int m_iSizeMode;
	int m_iCaptureSec = 90;
	float m_fGarrisonMultiplier = 1.0;
	Faction m_EnemyFaction;
	ref IA_Config m_DefenseSnapshot;

	//------------------------------------------------------------------------------------------------
	static IA_BaseObjectiveSettings SnapshotFrom(notnull IA_Config cfg, Faction enemyFaction)
	{
		ref IA_BaseObjectiveSettings settings = new IA_BaseObjectiveSettings();
		settings.m_bEnabled = cfg.m_bDynamicBaseEnabled;
		settings.m_bEmplacementsEnabled = cfg.m_bDynamicBaseEmplacementsEnabled;
		settings.m_iChancePct = cfg.m_iDynamicBaseChancePct;
		settings.m_bInGm = cfg.m_bDynamicBaseInGm;
		settings.m_iSizeMode = cfg.m_iDynamicBaseSizeMode;
		settings.m_iCaptureSec = cfg.m_iDynamicBaseCaptureSec;
		settings.m_fGarrisonMultiplier = cfg.m_fDynamicBaseGarrisonMultiplier;
		settings.m_EnemyFaction = enemyFaction;
		ref IA_Config defense = new IA_Config();
		IA_Config.UnpackDefenseExtras(defense, IA_Config.PackDefenseExtras(cfg));
		settings.m_DefenseSnapshot = defense;
		return settings;
	}

	//------------------------------------------------------------------------------------------------
	int GetCaptureMs()
	{
		int sec = m_iCaptureSec;
		if (sec < 30)
			sec = 30;
		return sec * 1000;
	}

	//------------------------------------------------------------------------------------------------
	int ComputeGarrisonBudget()
	{
		// Occupying garrison is locked at 1x player/admin AI scale. Defense
		// reinforcements and QRF still use IA_Game.GetAIScaleFactor().
		float scaled = 24.0 * m_fGarrisonMultiplier;
		int budget = Math.Round(scaled);
		if (budget < 4)
			budget = 4;
		if (budget > 36)
			budget = 36;
		return budget;
	}

	//------------------------------------------------------------------------------------------------
	//! Initial occupying spawn is 1.75x the layout-capped 1x budget so every
	//! layout (Full through Rally post) fields more guards than the previous baseline.
	int ComputeInitialGarrison(int layoutMax)
	{
		int budget = ComputeGarrisonBudget();
		if (budget > layoutMax)
			budget = layoutMax;
		int boosted = Math.Round(budget * 1.75);
		if (boosted < 4)
			boosted = 4;
		return boosted;
	}
}

class IA_DynamicSiteCandidate
{
	vector m_vCenter;
	vector m_vHqAnchor;
	float m_fYawDeg;
	float m_fSurveyYawDeg;
	int m_iLayoutId;
	int m_iSeed;
	float m_fScore = 999999;
	float m_fHeightSpanM;
	int m_iTerrainPass;
	bool m_bRoadAligned;
	bool m_bVehicleAccess;
	int m_iInfantryEntries;
}

class IA_DynamicSiteResult
{
	int m_iSerial;
	bool m_bSuccess;
	string m_sReason;
	ref IA_DynamicSiteInstance m_Site;
}

class IA_BaseRosterEntry
{
	int m_iPlayerId;
	string m_sIdentity;
	int m_iLastSeenUnix;
	int m_iGraceStartUnix;
	bool m_bEligible;
}

class IA_BaseHudStatus
{
	int m_iVersion;
	int m_iSerial;
	int m_iGroupId;
	int m_iPhase;
	string m_sSiteId;
	vector m_vSite;
	vector m_vCapture;
	float m_fCaptureRadius;
	int m_iCapturePermille;
	int m_iEligiblePresent;
	int m_iTarget;
	int m_iAllPresent;
	int m_iRemainingSec;
	int m_iReason;
}

class IA_DynamicParse
{
	//------------------------------------------------------------------------------------------------
	static bool TryParseIntToken(string token, out int value)
	{
		value = 0;
		if (token.IsEmpty())
			return false;

		int parsed = 0;
		value = token.ToInt(0, 0, parsed);
		if (parsed != token.Length())
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool TryParseFloatToken(string token, out float value)
	{
		value = 0;
		if (token.IsEmpty())
			return false;

		int parsed = 0;
		value = token.ToFloat(0, 0, parsed);
		if (parsed != token.Length())
			return false;
		if (value != value)
			return false;
		if (value > 1000000 || value < -1000000)
			return false;
		return true;
	}
}
