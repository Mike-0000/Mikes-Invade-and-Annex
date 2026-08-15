//------------------------------------------------------------------------------------------------
//! Server-authoritative session ranks. RAM only — gone on restart. No API / DB.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Invade & Annex/Components", description: "Current-session rank board (resets on restart).")]
class IA_SessionRankManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

class IA_SessionRankManagerComponent : SCR_BaseGameModeComponent
{
	protected static const int REPLICATE_DELAY_MS = 1200;
	protected static const int XP_KILL = 15;
	protected static const int XP_HVT = 75;
	protected static const int XP_HVT_GUARD = 25;

	[RplProp(onRplName: "OnReplicated")]
	protected string m_sJson;

	protected ref map<string, ref IA_SessionRankEntry> m_mPlayers;
	protected ref array<ref IA_SessionRankEntry> m_aSorted;
	protected ref ScriptInvoker m_OnUpdated;
	protected bool m_bReplicatePending;
	protected static IA_SessionRankManagerComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	static IA_SessionRankManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnUpdated()
	{
		if (!m_OnUpdated)
			m_OnUpdated = new ScriptInvoker();
		return m_OnUpdated;
	}

	//------------------------------------------------------------------------------------------------
	string GetCachedJson()
	{
		return m_sJson;
	}

	//------------------------------------------------------------------------------------------------
	array<ref IA_SessionRankEntry> GetSorted()
	{
		return m_aSorted;
	}

	//------------------------------------------------------------------------------------------------
	IA_SessionRankEntry FindLocal()
	{
		if (!m_aSorted)
			return null;

		string guid = GetLocalPlayerGuid();
		int localId = SCR_PlayerController.GetLocalPlayerId();
		int i;
		for (i = 0; i < m_aSorted.Count(); i++)
		{
			IA_SessionRankEntry entry = m_aSorted[i];
			if (!entry)
				continue;
			if (!guid.IsEmpty() && entry.playerId == guid)
				return entry;
			if (localId > 0 && entry.netId == localId)
				return entry;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	int GetLocalPlace()
	{
		if (!m_aSorted)
			return 0;

		string guid = GetLocalPlayerGuid();
		int localId = SCR_PlayerController.GetLocalPlayerId();
		int i;
		for (i = 0; i < m_aSorted.Count(); i++)
		{
			IA_SessionRankEntry entry = m_aSorted[i];
			if (!entry)
				continue;
			if (!guid.IsEmpty() && entry.playerId == guid)
				return i + 1;
			if (localId > 0 && entry.netId == localId)
				return i + 1;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	static string GetLocalPlayerGuid()
	{
		int playerId = SCR_PlayerController.GetLocalPlayerId();
		if (playerId <= 0)
			return "";
		return SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		if (s_Instance && s_Instance != this)
		{
			Print("[IA][SessionRank] Instance already exists.", LogLevel.WARNING);
			return;
		}

		s_Instance = this;
		m_mPlayers = new map<string, ref IA_SessionRankEntry>();
		m_aSorted = new array<ref IA_SessionRankEntry>();

		if (!Replication.IsServer())
		{
			RebuildFromJson(m_sJson);
			GetOnUpdated().Invoke(m_sJson);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerRegistered(int playerId)
	{
		if (!Replication.IsServer())
			return;
		if (!EnsurePlayerById(playerId))
		{
			GetGame().GetCallqueue().CallLater(this.RetryEnsurePlayer, 500, false, playerId);
			return;
		}
		MarkDirty();
	}

	//------------------------------------------------------------------------------------------------
	protected void RetryEnsurePlayer(int playerId)
	{
		if (!Replication.IsServer())
			return;
		if (!EnsurePlayerById(playerId))
			return;
		MarkDirty();
	}

	//------------------------------------------------------------------------------------------------
	//! Admin debug: jump this player to the next grade's XP threshold.
	void PromotePlayer(int playerId)
	{
		if (!Replication.IsServer())
			return;

		IA_SessionRankEntry entry = EnsurePlayerById(playerId);
		if (!entry)
		{
			Print("[IA][SessionRank] Promote skipped, could not resolve player.", LogLevel.WARNING);
			return;
		}

		int next = IA_SessionRankLadder.GetNextRank(entry.rankId);
		if (next == SCR_ECharacterRank.INVALID)
		{
			Print("[IA][SessionRank] Promote skipped, already at max grade.", LogLevel.WARNING);
			return;
		}

		entry.score = IA_SessionRankLadder.GetRequiredXp(next);
		entry.rankId = next;
		FlushNow();
	}

	//------------------------------------------------------------------------------------------------
	void AwardKill(string playerId, string playerName)
	{
		AwardXp(playerId, playerName, XP_KILL, 1, 0, 0, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	void AwardDeath(string playerId, string playerName)
	{
		AwardXp(playerId, playerName, 0, 0, 1, 0, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	void AwardHvtKill(string playerId, string playerName)
	{
		AwardXp(playerId, playerName, XP_HVT, 0, 0, 1, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	void AwardHvtGuardKill(string playerId, string playerName)
	{
		AwardXp(playerId, playerName, XP_HVT_GUARD, 0, 0, 0, 1, 0);
	}

	//------------------------------------------------------------------------------------------------
	void AwardCapture(string playerId, string playerName, int score)
	{
		int xp = score;
		if (xp < 1)
			xp = 1;
		AwardXp(playerId, playerName, xp, 0, 0, 0, 0, score);
	}

	//------------------------------------------------------------------------------------------------
	protected void AwardXp(string playerId, string playerName, int xp, int kills, int deaths, int hvt, int guard, int obj)
	{
		if (!Replication.IsServer())
			return;
		if (playerId.IsEmpty())
		{
			Print("[IA][SessionRank] Award skipped, empty player id.", LogLevel.WARNING);
			return;
		}

		IA_SessionRankEntry entry = EnsurePlayer(playerId, playerName);
		if (!entry)
			return;

		entry.kills = entry.kills + kills;
		entry.deaths = entry.deaths + deaths;
		entry.hvt_kills = entry.hvt_kills + hvt;
		entry.hvt_guard_kills = entry.hvt_guard_kills + guard;
		entry.obj_score = entry.obj_score + obj;
		entry.score = entry.score + xp;
		entry.rankId = IA_SessionRankLadder.GetRankByXp(entry.score);
		MarkDirty();
	}

	//------------------------------------------------------------------------------------------------
	protected IA_SessionRankEntry EnsurePlayerById(int playerId)
	{
		if (playerId <= 0)
			return null;

		string guid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		if (guid.IsEmpty())
			return null;

		string name = GetGame().GetPlayerManager().GetPlayerName(playerId);
		IA_SessionRankEntry entry = EnsurePlayer(guid, name);
		if (entry)
			entry.netId = playerId;
		return entry;
	}

	//------------------------------------------------------------------------------------------------
	protected IA_SessionRankEntry EnsurePlayer(string playerId, string playerName)
	{
		if (playerId.IsEmpty())
			return null;

		if (!m_mPlayers)
			m_mPlayers = new map<string, ref IA_SessionRankEntry>();

		IA_SessionRankEntry existing = m_mPlayers.Get(playerId);
		if (existing)
		{
			if (!playerName.IsEmpty())
				existing.PlayerName = playerName;
			if (existing.netId <= 0)
				existing.netId = ResolveNetId(playerId);
			return existing;
		}

		ref IA_SessionRankEntry entry = new IA_SessionRankEntry();
		entry.playerId = playerId;
		entry.PlayerName = playerName;
		entry.kills = 0;
		entry.deaths = 0;
		entry.hvt_kills = 0;
		entry.hvt_guard_kills = 0;
		entry.obj_score = 0;
		entry.score = 0;
		entry.rankId = SCR_ECharacterRank.PRIVATE;
		entry.netId = ResolveNetId(playerId);
		m_mPlayers.Insert(playerId, entry);
		return entry;
	}

	//------------------------------------------------------------------------------------------------
	protected int ResolveNetId(string guid)
	{
		if (guid.IsEmpty())
			return 0;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return 0;

		array<int> ids = new array<int>();
		pm.GetPlayers(ids);
		int i;
		for (i = 0; i < ids.Count(); i++)
		{
			if (SCR_PlayerIdentityUtils.GetPlayerIdentityId(ids[i]) == guid)
				return ids[i];
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void MarkDirty()
	{
		if (m_bReplicatePending)
			return;

		m_bReplicatePending = true;
		GetGame().GetCallqueue().CallLater(this.FlushReplication, REPLICATE_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void FlushNow()
	{
		GetGame().GetCallqueue().Remove(this.FlushReplication);
		m_bReplicatePending = false;
		FlushReplication();
	}

	//------------------------------------------------------------------------------------------------
	protected void FlushReplication()
	{
		m_bReplicatePending = false;
		if (!Replication.IsServer())
			return;

		RebuildSorted();
		m_sJson = BuildJson();
		Replication.BumpMe();
		GetOnUpdated().Invoke(m_sJson);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnReplicated()
	{
		RebuildFromJson(m_sJson);
		GetOnUpdated().Invoke(m_sJson);
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildFromJson(string jsonData)
	{
		m_aSorted = new array<ref IA_SessionRankEntry>();
		if (!jsonData || jsonData.IsEmpty())
			return;

		JsonLoadContext jsonContext = new JsonLoadContext();
		if (!jsonContext.LoadFromString(jsonData))
		{
			Print("[IA][SessionRank] Failed to load session JSON.", LogLevel.ERROR);
			return;
		}

		ref array<ref IA_SessionRankEntry> loaded = new array<ref IA_SessionRankEntry>();
		if (!jsonContext.ReadValue("", loaded))
		{
			Print("[IA][SessionRank] Failed to read session entries.", LogLevel.ERROR);
			return;
		}

		m_aSorted = loaded;
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildSorted()
	{
		m_aSorted = new array<ref IA_SessionRankEntry>();
		if (!m_mPlayers)
			return;

		foreach (string id, IA_SessionRankEntry entry : m_mPlayers)
		{
			if (entry)
				m_aSorted.Insert(entry);
		}

		int n = m_aSorted.Count();
		int i;
		int j;
		for (i = 0; i < n; i++)
		{
			for (j = 0; j < n - 1; j++)
			{
				if (m_aSorted[j].score >= m_aSorted[j + 1].score)
					continue;

				ref IA_SessionRankEntry tmp = m_aSorted[j];
				m_aSorted[j] = m_aSorted[j + 1];
				m_aSorted[j + 1] = tmp;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected string BuildJson()
	{
		string json = "[";
		int count = 0;
		if (m_aSorted)
			count = m_aSorted.Count();

		int i;
		for (i = 0; i < count; i++)
		{
			IA_SessionRankEntry e = m_aSorted[i];
			if (!e)
				continue;

			if (i > 0)
				json = json + ",";

			json = json + "{";
			json = json + "\"playerId\": \"" + Sanitize(e.playerId) + "\",";
			json = json + "\"PlayerName\": \"" + Sanitize(e.PlayerName) + "\",";
			json = json + "\"kills\": " + e.kills.ToString() + ",";
			json = json + "\"deaths\": " + e.deaths.ToString() + ",";
			json = json + "\"hvt_kills\": " + e.hvt_kills.ToString() + ",";
			json = json + "\"hvt_guard_kills\": " + e.hvt_guard_kills.ToString() + ",";
			json = json + "\"obj_score\": " + e.obj_score.ToString() + ",";
			json = json + "\"score\": " + e.score.ToString() + ",";
			json = json + "\"rankId\": " + e.rankId.ToString() + ",";
			json = json + "\"netId\": " + e.netId.ToString();
			json = json + "}";
		}

		json = json + "]";
		return json;
	}

	//------------------------------------------------------------------------------------------------
	protected string Sanitize(string value)
	{
		if (value.IsEmpty())
			return "";
		value.Replace("\"", "'");
		return value;
	}
}
