//------------------------------------------------------------------------------------------------
//! Server-authoritative session ranks. RAM only — gone on restart. No API / DB.
//!
//! The board itself is not replicated: it grows with every player who joins, and one
//! replicated string of it outgrew what the engine sends. Each player is pushed their own
//! standing through their controller, which is all the HUD chip shows, and the leaderboard
//! menu asks for the board a page at a time (IA_LeaderboardManagerComponent).
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
	protected static const int STANDING_FIRST_ASK_MS = 1500;
	protected static const int STANDING_RETRY_MS = 4000;

	protected ref map<string, ref IA_SessionRankEntry> m_mPlayers;
	protected ref map<string, int> m_mAoStartScore;
	protected ref array<ref IA_SessionRankEntry> m_aSorted;
	protected ref ScriptInvoker m_OnUpdated;
	protected bool m_bReplicatePending;
	protected ref map<int, string> m_mSentStanding;	// server: what each player was last told
	protected ref IA_SessionRankEntry m_LocalEntry;	// client: this player's own standing
	protected int m_iLocalPlace;
	protected int m_iPlayerCount;
	protected bool m_bStandingKnown;
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
	//! Server only: every entry, best session XP first.
	array<ref IA_SessionRankEntry> GetSorted()
	{
		return m_aSorted;
	}

	//------------------------------------------------------------------------------------------------
	//! \return players on the session board, as last told to this machine
	int GetPlayerCount()
	{
		if (Replication.IsServer())
		{
			if (!m_mPlayers)
				return 0;
			return m_mPlayers.Count();
		}
		return m_iPlayerCount;
	}

	//------------------------------------------------------------------------------------------------
	//! This player's own entry: the pushed standing on a client, the live entry on the server.
	IA_SessionRankEntry FindLocal()
	{
		if (!Replication.IsServer())
			return m_LocalEntry;

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
		if (!Replication.IsServer())
			return m_iLocalPlace;

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
		m_mSentStanding = new map<int, string>();

		// A push made before this component streamed in is lost, so a client also asks.
		if (!Replication.IsServer())
			GetGame().GetCallqueue().CallLater(this.PullStanding, STANDING_FIRST_ASK_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(this.PullStanding);
		GetGame().GetCallqueue().Remove(this.FlushReplication);
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
	//! Snapshot current session XP so AO-end contributors can use XP gained this AO.
	void BeginAoXpWindow()
	{
		if (!Replication.IsServer())
			return;

		m_mAoStartScore = new map<string, int>();
		if (!m_mPlayers)
			return;

		foreach (string id, IA_SessionRankEntry entry : m_mPlayers)
		{
			if (!entry)
				continue;
			m_mAoStartScore.Insert(id, entry.score);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ranked list of session XP gained since BeginAoXpWindow. Empty if nobody earned XP.
	string BuildAoTopContributorsMessage(int maxCount)
	{
		ref array<string> names = new array<string>();
		ref array<int> gained = new array<int>();

		if (!m_mPlayers)
			return "";

		foreach (string id, IA_SessionRankEntry entry : m_mPlayers)
		{
			if (!entry)
				continue;

			int startXp = 0;
			if (m_mAoStartScore && m_mAoStartScore.Contains(id))
				startXp = m_mAoStartScore[id];

			int xp = entry.score - startXp;
			if (xp < 1)
				continue;

			string name = entry.PlayerName;
			if (name.IsEmpty())
				name = "Unknown";

			names.Insert(name);
			gained.Insert(xp);
		}

		int n = gained.Count();
		int i;
		int j;
		for (i = 0; i < n; i++)
		{
			for (j = 0; j < n - 1; j++)
			{
				if (gained[j] >= gained[j + 1])
					continue;

				int tmpXp = gained[j];
				gained[j] = gained[j + 1];
				gained[j + 1] = tmpXp;

				string tmpName = names[j];
				names[j] = names[j + 1];
				names[j + 1] = tmpName;
			}
		}

		if (names.IsEmpty())
			return "";

		int shown = maxCount;
		if (shown < 1)
			shown = 3;
		if (shown > names.Count())
			shown = names.Count();

		string message = "";
		for (i = 0; i < shown; i++)
		{
			if (i > 0)
				message = message + "\n";
			message = message + string.Format("%1. %2 (%3 XP)", i + 1, names[i], gained[i]);
		}
		return message;
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
	//! Session XP for flying troops into the AO; one XP per transport rating point.
	//! Called once for each credited insertion.
	void AwardTransport(string playerId, string playerName, int points)
	{
		if (points < 1)
			return;
		AwardXp(playerId, playerName, points, 0, 0, 0, 0, 0);

		if (!m_mPlayers)
			return;
		IA_SessionRankEntry entry = m_mPlayers.Get(playerId);
		if (!entry)
			return;
		entry.transport = entry.transport + points;
		entry.insertions = entry.insertions + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tell one player where they stand now. Answers the client's own ask.
	void SendStanding(int playerId)
	{
		if (!Replication.IsServer())
			return;

		// Identity not resolved yet: say nothing, the client asks again.
		IA_SessionRankEntry entry = EnsurePlayerById(playerId);
		if (!entry)
			return;

		RebuildSorted();
		int total = m_aSorted.Count();
		for (int i = 0; i < total; i++)
		{
			if (m_aSorted[i] != entry)
				continue;
			PushStanding(entry, i + 1, total, true);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server's word on this player's own standing.
	void OnLocalStanding(int place, int total, int rankId, int kills, int deaths, int score)
	{
		if (Replication.IsServer())
			return;

		if (!m_LocalEntry)
			m_LocalEntry = new IA_SessionRankEntry();
		m_LocalEntry.playerId = GetLocalPlayerGuid();
		m_LocalEntry.netId = SCR_PlayerController.GetLocalPlayerId();
		m_LocalEntry.rankId = rankId;
		m_LocalEntry.kills = kills;
		m_LocalEntry.deaths = deaths;
		m_LocalEntry.score = score;
		m_iLocalPlace = place;
		m_iPlayerCount = total;
		m_bStandingKnown = true;
		GetOnUpdated().Invoke("");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one page of the session board in the asked order, as packed IA_BoardRow lines.
	//! \param playerGuid identity of the asking player, for their own line
	//! \param playerId player manager id of the asking player, used when the identity is unknown
	//! \param[out] rows the page
	//! \param[out] mine the asking player's own line, empty when they are not on the board
	//! \return players on the board
	int BuildBoardPage(int sortKey, bool descending, int offset, int limit, string playerGuid, int playerId, notnull array<string> rows, out string mine)
	{
		rows.Clear();
		mine = "";
		if (!m_mPlayers)
			return 0;

		ref array<IA_SessionRankEntry> order = {};
		ref array<float> values = {};
		foreach (string id, IA_SessionRankEntry entry : m_mPlayers)
		{
			if (!entry)
				continue;

			// Insertion sort: the board is one session's players, and ties keep the higher XP first.
			float value = BoardSortValue(entry, sortKey);
			int at = order.Count();
			while (at > 0)
			{
				IA_SessionRankEntry other = order[at - 1];
				float otherValue = values[at - 1];
				bool before = false;
				if (value == otherValue)
					before = entry.score > other.score;
				else if (descending)
					before = value > otherValue;
				else
					before = value < otherValue;
				if (!before)
					break;
				at = at - 1;
			}
			order.InsertAt(entry, at);
			values.InsertAt(value, at);
		}

		int total = order.Count();
		for (int i = 0; i < total; i++)
		{
			IA_SessionRankEntry listed = order[i];
			bool own = playerId > 0 && listed.netId == playerId;
			if (!playerGuid.IsEmpty() && listed.playerId == playerGuid)
				own = true;
			bool inPage = i >= offset && i < offset + limit;
			if (!own && !inPage)
				continue;

			string line = IA_BoardRow.Pack(i + 1, listed.PlayerName, listed.kills, listed.deaths, listed.hvt_kills, listed.hvt_guard_kills, listed.obj_score, listed.score, listed.transport, listed.insertions, 0, listed.rankId);
			if (inPage)
				rows.Insert(line);
			if (own)
				mine = line;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	protected float BoardSortValue(notnull IA_SessionRankEntry entry, int sortKey)
	{
		if (sortKey == IA_BoardProtocol.SORT_KILLS)
			return entry.kills;
		if (sortKey == IA_BoardProtocol.SORT_DEATHS)
			return entry.deaths;
		if (sortKey == IA_BoardProtocol.SORT_KD)
			return IA_BoardRow.KillRatio(entry.kills, entry.deaths);
		if (sortKey == IA_BoardProtocol.SORT_HVT)
			return entry.hvt_kills;
		if (sortKey == IA_BoardProtocol.SORT_GUARD)
			return entry.hvt_guard_kills;
		if (sortKey == IA_BoardProtocol.SORT_OBJ)
			return entry.obj_score;
		if (sortKey == IA_BoardProtocol.SORT_TRANSPORT)
			return entry.transport;
		if (sortKey == IA_BoardProtocol.SORT_INSERTIONS)
			return entry.insertions;
		return entry.score;
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
				existing.PlayerName = IA_SanitizePlayerName(playerName);
			if (existing.netId <= 0)
				existing.netId = ResolveNetId(playerId);
			return existing;
		}

		ref IA_SessionRankEntry entry = new IA_SessionRankEntry();
		entry.playerId = playerId;
		entry.PlayerName = IA_SanitizePlayerName(playerName);
		entry.kills = 0;
		entry.deaths = 0;
		entry.hvt_kills = 0;
		entry.hvt_guard_kills = 0;
		entry.obj_score = 0;
		entry.score = 0;
		entry.transport = 0;
		entry.insertions = 0;
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
		int total = m_aSorted.Count();
		for (int i = 0; i < total; i++)
		{
			PushStanding(m_aSorted[i], i + 1, total, false);
		}
		GetOnUpdated().Invoke("");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: send a player their standing when it differs from what they were last told.
	protected void PushStanding(IA_SessionRankEntry entry, int place, int total, bool force)
	{
		if (!entry || entry.netId <= 0)
			return;
		// A hosting player reads the live entry.
		if (entry.netId == SCR_PlayerController.GetLocalPlayerId())
			return;

		string stamp = string.Format("%1|%2|%3|%4|%5|%6", place, total, entry.rankId, entry.kills, entry.deaths, entry.score);
		if (!force && m_mSentStanding.Get(entry.netId) == stamp)
			return;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;
		SCR_PlayerController controller = SCR_PlayerController.Cast(players.GetPlayerController(entry.netId));
		if (!controller)
			return;

		m_mSentStanding.Set(entry.netId, stamp);
		controller.IA_SendSessionStanding(place, total, entry.rankId, entry.kills, entry.deaths, entry.score);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: ask until the first standing arrives.
	protected void PullStanding()
	{
		if (m_bStandingKnown)
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.IA_AskSessionStanding();
		GetGame().GetCallqueue().CallLater(this.PullStanding, STANDING_RETRY_MS, false);
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
}
