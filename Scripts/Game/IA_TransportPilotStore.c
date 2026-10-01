//------------------------------------------------------------------------------------------------
//! Server-side cache of global transport ratings. The stats backend (Supabase
//! behind the invadestats API) owns the totals, so progress follows a player to
//! every server; nothing is written to this server's profile.
//!
//! Points earned here are queued, sent in idempotent batches, and counted on top
//! of the last fetched total until the backend acknowledges them.
//------------------------------------------------------------------------------------------------
class IA_TransportPilotStore
{
	protected static const int FLUSH_INTERVAL_MS = 60000;
	protected static const int REQUEST_RETRY_MS = 60000;
	// A backend that is down or not deployed yet is retried ever more slowly.
	protected static const int BACKOFF_BASE_MS = 60000;
	protected static const int BACKOFF_MAX_MS = 1800000;

	protected static ref IA_TransportPilotStore s_Instance;

	protected ref map<string, ref IA_TransportPilotRecord> m_mRecords;
	// Batch awaiting acknowledgement. Resent unchanged so the backend can drop a duplicate.
	protected string m_sSentBatchId;
	protected string m_sSentEntries;
	protected bool m_bSubmitInFlight;
	protected bool m_bFetchInFlight;
	protected ref array<string> m_aFetchIds;
	protected ref array<string> m_aFetchSent;
	protected int m_iBatchCounter;
	protected int m_iFailures;
	protected int m_iNextAttemptMs;

	//------------------------------------------------------------------------------------------------
	static IA_TransportPilotStore GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new IA_TransportPilotStore();
			GetGame().GetCallqueue().CallLater(s_Instance.Flush, FLUSH_INTERVAL_MS, true);
		}
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void IA_TransportPilotStore()
	{
		m_mRecords = new map<string, ref IA_TransportPilotRecord>();
		m_aFetchIds = new array<string>();
		m_aFetchSent = new array<string>();
	}

	//------------------------------------------------------------------------------------------------
	protected IA_TransportPilotRecord Ensure(string guid, string playerName)
	{
		if (guid.IsEmpty())
			return null;

		IA_TransportPilotRecord record = m_mRecords.Get(guid);
		if (!record)
		{
			ref IA_TransportPilotRecord created = new IA_TransportPilotRecord();
			created.m_sGuid = guid;
			m_mRecords.Insert(guid, created);
			record = created;
		}
		if (!playerName.IsEmpty())
			record.m_sName = IA_SanitizePlayerName(playerName);
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! \return global rating including unacknowledged points, RATING_UNKNOWN until fetched
	int GetRating(string guid)
	{
		IA_TransportPilotRecord record = m_mRecords.Get(guid);
		if (!record)
			return IA_TransportPilotRecord.RATING_UNKNOWN;
		return record.GetRating();
	}

	//------------------------------------------------------------------------------------------------
	//! Queue one credited insertion. \return the record, null when the identity is unusable
	IA_TransportPilotRecord AddInsertion(string guid, string playerName, int points)
	{
		if (points <= 0)
			return null;

		IA_TransportPilotRecord record = Ensure(guid, playerName);
		if (!record)
			return null;

		record.m_iPendingPoints = record.m_iPendingPoints + points;
		record.m_iPendingInsertions = record.m_iPendingInsertions + 1;
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the backend for a player's global rating if it is not known yet.
	void RequestRating(string guid, string playerName, int nowMs)
	{
		IA_TransportPilotRecord record = Ensure(guid, playerName);
		if (!record || record.m_iGlobalRating >= 0)
			return;
		if (record.m_bRequested && nowMs - record.m_iLastRequestMs < REQUEST_RETRY_MS)
			return;

		record.m_bRequested = true;
		record.m_iLastRequestMs = nowMs;
		if (!m_aFetchIds.Contains(guid))
			m_aFetchIds.Insert(guid);
	}

	//------------------------------------------------------------------------------------------------
	//! Send queued fetches now; called once per tracker tick so joins batch together.
	void SendQueuedRequests()
	{
		// One request at a time, so a ratings snapshot never predates an acknowledged batch.
		if (m_bFetchInFlight || m_bSubmitInFlight || m_aFetchIds.IsEmpty() || IsBackingOff())
			return;

		string ids = "[";
		int i;
		for (i = 0; i < m_aFetchIds.Count(); i++)
		{
			if (i > 0)
				ids = ids + ",";
			ids = ids + "\"" + IA_JsonEscape(m_aFetchIds[i]) + "\"";
		}
		ids = ids + "]";

		if (!IA_ApiHandler.GetInstance().FetchTransportRatings(ids))
			return;

		m_bFetchInFlight = true;
		m_aFetchSent.Copy(m_aFetchIds);
		m_aFetchIds.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void Flush()
	{
		if (!Replication.IsServer() || m_bSubmitInFlight || m_bFetchInFlight || IsBackingOff())
			return;

		if (m_sSentEntries.IsEmpty())
			BuildBatch();
		if (m_sSentEntries.IsEmpty())
			return;

		if (IA_ApiHandler.GetInstance().SubmitTransport(m_sSentBatchId, m_sSentEntries))
			m_bSubmitInFlight = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildBatch()
	{
		string entries = "";
		foreach (string guid, IA_TransportPilotRecord record : m_mRecords)
		{
			if (!record || record.m_iPendingPoints <= 0)
				continue;

			record.m_iSentPoints = record.m_iPendingPoints;
			record.m_iSentInsertions = record.m_iPendingInsertions;
			record.m_iPendingPoints = 0;
			record.m_iPendingInsertions = 0;

			if (!entries.IsEmpty())
				entries = entries + ",";
			entries = entries + "{";
			entries = entries + "\"playerId\":\"" + IA_JsonEscape(record.m_sGuid) + "\",";
			entries = entries + "\"playerName\":\"" + IA_JsonEscape(record.m_sName) + "\",";
			entries = entries + "\"points\":" + record.m_iSentPoints.ToString() + ",";
			entries = entries + "\"insertions\":" + record.m_iSentInsertions.ToString();
			entries = entries + "}";
		}

		if (entries.IsEmpty())
			return;

		m_iBatchCounter = m_iBatchCounter + 1;
		m_sSentEntries = "[" + entries + "]";
		m_sSentBatchId = string.Format("%1-%2-%3", System.GetUnixTime(), System.GetTickCount(), m_iBatchCounter);
	}

	//------------------------------------------------------------------------------------------------
	//! \param ok false keeps the batch for an unchanged resend on the next flush
	void OnSubmitResult(bool ok)
	{
		m_bSubmitInFlight = false;
		NoteResult(ok);
		if (!ok)
			return;

		// The backend now holds these points; fold them into the known total.
		foreach (string guid, IA_TransportPilotRecord record : m_mRecords)
		{
			if (!record || record.m_iSentPoints <= 0)
				continue;
			if (record.m_iGlobalRating >= 0)
				record.m_iGlobalRating = record.m_iGlobalRating + record.m_iSentPoints;
			record.m_iSentPoints = 0;
			record.m_iSentInsertions = 0;
		}
		m_sSentEntries = "";
		m_sSentBatchId = "";
	}

	//------------------------------------------------------------------------------------------------
	void OnRatingsFailed()
	{
		m_bFetchInFlight = false;
		m_aFetchSent.Clear();
		NoteResult(false);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsBackingOff()
	{
		return m_iFailures > 0 && System.GetTickCount() < m_iNextAttemptMs;
	}

	//------------------------------------------------------------------------------------------------
	protected void NoteResult(bool ok)
	{
		if (ok)
		{
			m_iFailures = 0;
			return;
		}

		m_iFailures = m_iFailures + 1;
		m_iNextAttemptMs = System.GetTickCount() + IA_TransportScoring.BackoffMs(m_iFailures, BACKOFF_BASE_MS, BACKOFF_MAX_MS);
	}

	//------------------------------------------------------------------------------------------------
	void OnRatingsReceived(string data)
	{
		m_bFetchInFlight = false;
		NoteResult(true);

		ref JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromString(data))
		{
			Print("[IA][TransportPilot] Ratings response was not valid JSON.", LogLevel.ERROR);
			return;
		}

		ref array<ref IA_TransportRatingEntry> ratings = {};
		if (!ctx.ReadValue("ratings", ratings))
		{
			Print("[IA][TransportPilot] Ratings response had no ratings list.", LogLevel.ERROR);
			return;
		}

		// A player the backend has never seen is a known zero, not an unknown.
		foreach (string requested : m_aFetchSent)
		{
			IA_TransportPilotRecord asked = m_mRecords.Get(requested);
			if (asked && asked.m_iGlobalRating < 0)
				asked.m_iGlobalRating = 0;
		}
		m_aFetchSent.Clear();

		foreach (IA_TransportRatingEntry entry : ratings)
		{
			if (!entry)
				continue;
			IA_TransportPilotRecord record = m_mRecords.Get(entry.playerId);
			if (!record)
				continue;
			// Ratings only grow, so never step back to an older snapshot.
			if (entry.rating > record.m_iGlobalRating)
				record.m_iGlobalRating = entry.rating;
		}

		ref array<ref IA_TransportSkinThreshold> skins = {};
		if (ctx.ReadValue("skins", skins))
		{
			foreach (IA_TransportSkinThreshold skin : skins)
			{
				if (skin)
					IA_HeliSkinCatalog.SetRequiredPoints(skin.key, skin.required);
			}
		}

		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][TransportPilot] Global ratings received for %1 players.", ratings.Count()), LogLevel.NORMAL);
		}
	}
}
