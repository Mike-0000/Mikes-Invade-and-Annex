// Game/IA_StatsManager.c
class IA_StatsManager
{
    private static ref IA_StatsManager s_Instance;
    private ref array<ref IA_StatEvent> m_aEventQue;
    private const int BATCH_SEND_INTERVAL = 60; // seconds

    private void IA_StatsManager()
    {
        m_aEventQue = new array<ref IA_StatEvent>();
        GetGame().GetCallqueue().CallLater(SendBatch, BATCH_SEND_INTERVAL * 1000, true);
        Print("IA_StatsManager initialized, will send batches every " + BATCH_SEND_INTERVAL + " seconds.", LogLevel.NORMAL);
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
        m_aEventQue.Insert(newEvent);
    }
    
    void QueuePlayerDeath(string victimId, string victimName)
    {
        if (!victimId || victimId == "")
        {
            Print("IA_StatsManager: Attempted to queue PlayerDeath with invalid victimId.", LogLevel.WARNING);
            return;
        }
        
        IA_PlayerDeathEvent newEvent = new IA_PlayerDeathEvent(victimId, victimName);
        m_aEventQue.Insert(newEvent);
    }

    void QueueHVTKill(string killerId, string killerName)
    {
        if (!killerId || killerId == "")
        {
            Print("IA_StatsManager: Attempted to queue HVTKill with invalid killerId.", LogLevel.WARNING);
            return;
        }
        
        IA_HVTKillEvent newEvent = new IA_HVTKillEvent(killerId, killerName);
        m_aEventQue.Insert(newEvent);
    }

    void QueueHVTGuardKill(string killerId, string killerName)
    {
        if (!killerId || killerId == "")
        {
            Print("IA_StatsManager: Attempted to queue HVTGuardKill with invalid killerId.", LogLevel.WARNING);
            return;
        }
        
        IA_HVTGuardKillEvent newEvent = new IA_HVTGuardKillEvent(killerId, killerName);
        m_aEventQue.Insert(newEvent);
    }

    void QueueCaptureContribution(string playerId, string playerName, int score)
    {
        if (!playerId || playerId == "")
        {
            Print("IA_StatsManager: Attempted to queue CaptureContribution with invalid playerId.", LogLevel.WARNING);
            return;
        }
        
        IA_CaptureContributionEvent newEvent = new IA_CaptureContributionEvent(playerId, playerName, score);
        m_aEventQue.Insert(newEvent);
    }

    void SendBatch()
    {
        if (m_aEventQue.IsEmpty())
            return;
            
        string payload = "[";
        int eventCount = m_aEventQue.Count();
        for (int i = 0; i < eventCount; i++)
        {
            payload = payload + m_aEventQue[i].ToJson();
            if (i < eventCount - 1)
            {
                payload = payload + ",";
            }
        }
        payload = payload + "]";
        
        Print("IA_StatsManager: Constructed payload: " + payload, LogLevel.DEBUG);
        
        IA_ApiHandler.GetInstance().SubmitStats(payload);
        
        Print("IA_StatsManager: Sending batch of " + eventCount + " events.", LogLevel.NORMAL);

        // Clear the queue after sending
        m_aEventQue.Clear();
    }
} 