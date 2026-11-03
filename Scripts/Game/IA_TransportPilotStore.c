//------------------------------------------------------------------------------------------------
//! Server-side cache of global transport ratings. The stats backend (Supabase
//! behind the game API) owns the totals, so progress follows a player to
//! every server; nothing is written to this server's profile.
//!
//! Points earned here are queued, sent in idempotent batches, and counted on top
//! of the last fetched total until the backend acknowledges them.
//!
//! Batches and rating lookups travel in the timed exchange (IA_ApiSync). A backend
//! without it is served through the separate transport routes, as before.
//------------------------------------------------------------------------------------------------
class IA_TransportPilotStore
{
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
			// The timed exchange takes the batches from here, or calls Flush for the separate route.
			IA_ApiSync.GetInstance().Start();
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
		if (m_aFetchIds.IsEmpty())
			return;

		// A player waits on the rating, so the timed exchange is asked to go early.
		IA_ApiSync sync = IA_ApiSync.GetInstance();
		if (sync.UsesSync())
		{
			sync.Hurry();
			return;
		}

		// One request at a time, so a ratings snapshot never predates an acknowledged batch.
		if (m_bFetchInFlight || m_bSubmitInFlight || IsBackingOff())
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
	//! Send the waiting batch on the separate transport route. IA_ApiSync calls this on its
	//! timer for a backend without the timed exchange.
	void Flush()
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
		int taken;
		foreach (string guid, IA_TransportPilotRecord record : m_mRecords)
		{
			if (!record || record.m_iPendingPoints <= 0)
				continue;
			// The backend takes so many players in a batch; the rest go in the next.
			if (taken >= IA_ApiTunables.TRANSPORT_BATCH_ENTRIES)
				continue;
			taken = taken + 1;

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
	//! The batch the timed exchange should carry: the one awaiting acknowledgement, else a new one.
	//! \param[out] entries JSON array of {playerId, playerName, points, insertions}
	//! \return false when there is none, or a request on the separate routes is still out
	bool SyncBatch(out string batchId, out string entries)
	{
		batchId = "";
		entries = "";
		if (!Replication.IsServer() || m_bSubmitInFlight || m_bFetchInFlight)
			return false;

		if (m_sSentEntries.IsEmpty())
			BuildBatch();
		batchId = m_sSentBatchId;
		entries = m_sSentEntries;
		return !m_sSentEntries.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! \return JSON array of the identities whose rating the timed exchange should ask for, empty when none
	string SyncRatingIds()
	{
		if (m_bSubmitInFlight || m_bFetchInFlight || m_aFetchIds.IsEmpty())
			return "";

		int count = SyncRatingCount();
		string ids = "[";
		for (int i = 0; i < count; i++)
		{
			if (i > 0)
				ids = ids + ",";
			ids = ids + "\"" + IA_JsonEscape(m_aFetchIds[i]) + "\"";
		}
		return ids + "]";
	}

	//------------------------------------------------------------------------------------------------
	//! The timed exchange went out with what SyncBatch and SyncRatingIds gave it.
	void OnSyncSent(bool batch, bool ratings)
	{
		if (batch)
			m_bSubmitInFlight = true;
		if (!ratings)
			return;

		m_bFetchInFlight = true;
		m_aFetchSent.Clear();
		int count = SyncRatingCount();
		for (int i = 0; i < count; i++)
		{
			m_aFetchSent.Insert(m_aFetchIds[0]);
			m_aFetchIds.RemoveOrdered(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The stats service has no timed exchange, so nothing it carried was looked at: all of it
	//! waits again, for the separate routes, and none of it counts as a failure.
	void OnSyncUnsent(bool batch, bool ratings)
	{
		if (batch)
			m_bSubmitInFlight = false;
		if (!ratings)
			return;

		m_bFetchInFlight = false;
		foreach (string asked : m_aFetchSent)
		{
			if (!m_aFetchIds.Contains(asked))
				m_aFetchIds.Insert(asked);
		}
		m_aFetchSent.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! What the stats service said of the batch the timed exchange carried.
	//! \param status the answer's transportStatus
	void OnSyncBatchResult(string status)
	{
		// Added now, or added by an earlier attempt whose answer was lost.
		if (status == "accepted" || status == "duplicate")
		{
			OnSubmitResult(true);
			return;
		}
		if (status != "rejected")
		{
			OnSubmitResult(false);
			return;
		}

		// Sending the same batch again would be refused again, so its points are given up.
		Print("[IA][TransportPilot] The stats service refused a transport batch; it is dropped.", LogLevel.WARNING);
		m_bSubmitInFlight = false;
		foreach (string guid, IA_TransportPilotRecord record : m_mRecords)
		{
			if (!record)
				continue;
			record.m_iSentPoints = 0;
			record.m_iSentInsertions = 0;
		}
		m_sSentEntries = "";
		m_sSentBatchId = "";
	}

	//------------------------------------------------------------------------------------------------
	//! The ratings and skin thresholds that came with the timed exchange.
	void OnSyncRatings(notnull JsonLoadContext ctx)
	{
		m_bFetchInFlight = false;
		NoteResult(true);
		ReadRatings(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected int SyncRatingCount()
	{
		int count = m_aFetchIds.Count();
		if (count > IA_ApiTunables.RATING_IDS)
			count = IA_ApiTunables.RATING_IDS;
		return count;
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
		ReadRatings(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! Take the "ratings" and "skins" of an answer.
	protected void ReadRatings(notnull JsonLoadContext ctx)
	{
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

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	//! Probe: the id of the batch awaiting acknowledgement, empty when none is.
	string ProbeBatchId()
	{
		return m_sSentBatchId;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: rating lookups waiting to be asked.
	int ProbeWanted()
	{
		return m_aFetchIds.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: forget every player, batch and failure.
	void ProbeClear()
	{
		m_mRecords.Clear();
		m_aFetchIds.Clear();
		m_aFetchSent.Clear();
		m_sSentEntries = "";
		m_sSentBatchId = "";
		m_bSubmitInFlight = false;
		m_bFetchInFlight = false;
		m_iFailures = 0;
	}
#endif
}
