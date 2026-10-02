// Game/IA_StatsManager.c
class IA_StatsManager
{
    private static ref IA_StatsManager s_Instance;
    private ref array<ref IA_StatEvent> m_aEventQue;

	protected static const int CHUNK_EVENTS = 50;

	// The batch on its way. It goes again unchanged, under the same id, until the stats service has it.
	protected string m_sBatch;
	protected string m_sBatchId;
	protected int m_iBatchEvents;
	protected int m_iBatchCounter;
	protected bool m_bQueueFull;

    private void IA_StatsManager()
    {
        m_aEventQue = new array<ref IA_StatEvent>();
        // The timed exchange with the stats service takes the batches from here.
        IA_ApiSync.GetInstance().Start();
        if (IA_Log.IsDebugEnabled())
        {
            Print("IA_StatsManager initialized, will send batches every " + IA_ApiTunables.SyncIntervalS() + " seconds.", LogLevel.NORMAL);
        }
    }

    static IA_StatsManager GetInstance()
    {
        if (!s_Instance)
            s_Instance = new IA_StatsManager();
        return s_Instance;
    }

    void QueuePlayerKill(string killerId, string killerName)
    {
        if (!killerId || killerId == "")
        {
            Print("IA_StatsManager: Attempted to queue PlayerKill with invalid killerId.", LogLevel.WARNING);
            return;
        }
        
        IA_PlayerKillEvent newEvent = new IA_PlayerKillEvent(killerId, killerName);
        Enqueue(newEvent);
        AwardSessionKill(killerId, killerName);
    }
    
    void QueuePlayerDeath(string victimId, string victimName)
    {
        if (!victimId || victimId == "")
        {
            Print("IA_StatsManager: Attempted to queue PlayerDeath with invalid victimId.", LogLevel.WARNING);
            return;
        }
        
        IA_PlayerDeathEvent newEvent = new IA_PlayerDeathEvent(victimId, victimName);
        Enqueue(newEvent);
        AwardSessionDeath(victimId, victimName);
    }

    void QueueHVTKill(string killerId, string killerName)
    {
        if (!killerId || killerId == "")
        {
            Print("IA_StatsManager: Attempted to queue HVTKill with invalid killerId.", LogLevel.WARNING);
            return;
        }
        
        IA_HVTKillEvent newEvent = new IA_HVTKillEvent(killerId, killerName);
        Enqueue(newEvent);
        AwardSessionHvt(killerId, killerName);
    }

    void QueueHVTGuardKill(string killerId, string killerName)
    {
        if (!killerId || killerId == "")
        {
            Print("IA_StatsManager: Attempted to queue HVTGuardKill with invalid killerId.", LogLevel.WARNING);
            return;
        }
        
        IA_HVTGuardKillEvent newEvent = new IA_HVTGuardKillEvent(killerId, killerName);
        Enqueue(newEvent);
        AwardSessionHvtGuard(killerId, killerName);
    }

    void QueueCaptureContribution(string playerId, string playerName, int score)
    {
        if (!playerId || playerId == "")
        {
            Print("IA_StatsManager: Attempted to queue CaptureContribution with invalid playerId.", LogLevel.WARNING);
            return;
        }
        
        IA_CaptureContributionEvent newEvent = new IA_CaptureContributionEvent(playerId, playerName, score);
        Enqueue(newEvent);
        AwardSessionCapture(playerId, playerName, score);
    }

	//------------------------------------------------------------------------------------------------
	//! Events wait here until a batch takes them. With the stats service out of reach for long,
	//! the oldest make room.
	protected void Enqueue(IA_StatEvent newEvent)
	{
		if (m_aEventQue.Count() >= IA_ApiTunables.STATS_QUEUE_EVENTS)
		{
			m_aEventQue.RemoveOrdered(0);
			if (!m_bQueueFull)
			{
				m_bQueueFull = true;
				Print("[IA][Stats] Statistics are not reaching the stats service; the oldest events waiting are being dropped.", LogLevel.WARNING);
			}
		}
		m_aEventQue.Insert(newEvent);
	}

	//------------------------------------------------------------------------------------------------
	//! Send what waits on the separate statistics route, for a stats service without the timed
	//! exchange. That route cannot tell a repeat, so a batch goes once and is forgotten, as before.
    void SendBatch()
    {
		if (m_sBatch.IsEmpty())
			BuildBatch();
		if (m_sBatch.IsEmpty())
			return;

        if (IA_Log.IsDebugEnabled())
        {
            Print("[IA][Stats] Batch payload prepared.", LogLevel.NORMAL);
        }

        IA_ApiHandler.GetInstance().SubmitStats(m_sBatch);

        if (IA_Log.IsDebugEnabled())
        {
            Print("IA_StatsManager: Sending batch of " + m_iBatchEvents + " events.", LogLevel.NORMAL);
        }

		ForgetBatch();
    }

	//------------------------------------------------------------------------------------------------
	//! The batch the timed exchange should carry: the one already on its way, else a new one
	//! from the events waiting.
	//! \param[out] events JSON array of the events
	//! \return false when there is nothing to send
	bool SyncBatch(out string batchId, out string events)
	{
		if (m_sBatch.IsEmpty())
			BuildBatch();
		batchId = m_sBatchId;
		events = m_sBatch;
		return !m_sBatch.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! What the stats service said of the batch the timed exchange carried.
	//! \param status the answer's statsStatus
	void OnSyncResult(string status)
	{
		// Counted now, or counted by an earlier attempt whose answer was lost.
		if (status == "accepted" || status == "duplicate")
		{
			ForgetBatch();
			return;
		}
		// Sending the same batch again would be refused again.
		if (status == "rejected")
		{
			Print(string.Format("[IA][Stats] The stats service refused a batch of %1 events; it is dropped.", m_iBatchEvents), LogLevel.WARNING);
			ForgetBatch();
		}
		// Anything else leaves the batch to go again under the same id.
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildBatch()
	{
		int waiting = m_aEventQue.Count();
		if (waiting == 0)
			return;

		int take = waiting;
		if (take > IA_ApiTunables.STATS_BATCH_EVENTS)
			take = IA_ApiTunables.STATS_BATCH_EVENTS;

		// Joined in short runs, so a large batch is not copied whole for every event added.
		string payload = "[";
		string chunk;
		for (int i = 0; i < take; i++)
		{
			if (i > 0)
				chunk = chunk + ",";
			chunk = chunk + m_aEventQue[i].ToJson();
			if (i % CHUNK_EVENTS == CHUNK_EVENTS - 1)
			{
				payload = payload + chunk;
				chunk = "";
			}
		}
		payload = payload + chunk + "]";

		if (take == waiting)
		{
			m_aEventQue.Clear();
		}
		else
		{
			for (int taken = 0; taken < take; taken++)
			{
				m_aEventQue.RemoveOrdered(0);
			}
		}

		m_iBatchCounter = m_iBatchCounter + 1;
		m_sBatch = payload;
		m_iBatchEvents = take;
		m_sBatchId = string.Format("s-%1-%2-%3", System.GetUnixTime(), System.GetTickCount(), m_iBatchCounter);
	}

	//------------------------------------------------------------------------------------------------
	protected void ForgetBatch()
	{
		m_sBatch = "";
		m_sBatchId = "";
		m_iBatchEvents = 0;
		m_bQueueFull = false;
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	//! Probe: events waiting for a batch.
	int ProbeWaiting()
	{
		return m_aEventQue.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: the id of the batch on its way, empty when none is.
	string ProbeBatchId()
	{
		return m_sBatchId;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe: drop every event and the batch on its way.
	void ProbeClear()
	{
		m_aEventQue.Clear();
		ForgetBatch();
	}
#endif

    //! Session XP for a credited combat insertion. The global transport rating
    //! travels through IA_TransportPilotStore, not the kill/capture batch.
    void QueueTransportInsertion(string pilotId, string pilotName, int points)
    {
        if (!Replication.IsServer() || pilotId.IsEmpty() || points <= 0)
            return;
        IA_SessionRankManagerComponent session = IA_SessionRankManagerComponent.GetInstance();
        if (session)
            session.AwardTransport(pilotId, pilotName, points);
    }

    protected void AwardSessionKill(string playerId, string playerName)
    {
        if (!Replication.IsServer())
            return;
        IA_SessionRankManagerComponent session = IA_SessionRankManagerComponent.GetInstance();
        if (session)
            session.AwardKill(playerId, playerName);
    }

    protected void AwardSessionDeath(string playerId, string playerName)
    {
        if (!Replication.IsServer())
            return;
        IA_SessionRankManagerComponent session = IA_SessionRankManagerComponent.GetInstance();
        if (session)
            session.AwardDeath(playerId, playerName);
    }

    protected void AwardSessionHvt(string playerId, string playerName)
    {
        if (!Replication.IsServer())
            return;
        IA_SessionRankManagerComponent session = IA_SessionRankManagerComponent.GetInstance();
        if (session)
            session.AwardHvtKill(playerId, playerName);
    }

    protected void AwardSessionHvtGuard(string playerId, string playerName)
    {
        if (!Replication.IsServer())
            return;
        IA_SessionRankManagerComponent session = IA_SessionRankManagerComponent.GetInstance();
        if (session)
            session.AwardHvtGuardKill(playerId, playerName);
    }

    protected void AwardSessionCapture(string playerId, string playerName, int score)
    {
        if (!Replication.IsServer())
            return;
        IA_SessionRankManagerComponent session = IA_SessionRankManagerComponent.GetInstance();
        if (session)
            session.AwardCapture(playerId, playerName, score);
    }
} 