// Game/IA_StatEvent.c
class IA_StatEvent
{
    string m_sEventType;

    // Base constructor removed to avoid compiler conflicts
    
    string ToJson();
}

class IA_PlayerKillEvent : IA_StatEvent
{
    string m_sKillerPlayerId;
    string m_sKillerPlayerName;

    void IA_PlayerKillEvent(string killerId, string killerName)
    {
        m_sEventType = "PlayerKill"; // Set event type directly
        m_sKillerPlayerId = killerId;
        m_sKillerPlayerName = killerName;
    }

    override string ToJson()
    {
        // Manual JSON serialization
        string json = "{";
        json = json + "\"eventType\": \"" + m_sEventType + "\",";
        json = json + "\"killerPlayerId\": \"" + m_sKillerPlayerId + "\",";
        json = json + "\"killerPlayerName\": \"" + m_sKillerPlayerName + "\"";
        json = json + "}";
        return json;
    }
}

class IA_PlayerDeathEvent : IA_StatEvent
{
    string m_sVictimPlayerId;
    string m_sVictimPlayerName;

    void IA_PlayerDeathEvent(string victimId, string victimName)
    {
        m_sEventType = "PlayerDeath";
        m_sVictimPlayerId = victimId;
        m_sVictimPlayerName = victimName;
    }

    override string ToJson()
    {
        // Manual JSON serialization
        string json = "{";
        json = json + "\"eventType\": \"" + m_sEventType + "\",";
        json = json + "\"victimPlayerId\": \"" + m_sVictimPlayerId + "\",";
        json = json + "\"victimPlayerName\": \"" + m_sVictimPlayerName + "\"";
        json = json + "}";
        return json;
    }
} 

class IA_HVTKillEvent : IA_StatEvent
{
    string m_sKillerPlayerId;
    string m_sKillerPlayerName;

    void IA_HVTKillEvent(string killerId, string killerName)
    {
        m_sEventType = "HVTKill";
        m_sKillerPlayerId = killerId;
        m_sKillerPlayerName = killerName;
    }

    override string ToJson()
    {
        // Manual JSON serialization
        string json = "{";
        json = json + "\"eventType\": \"" + m_sEventType + "\",";
        json = json + "\"killerPlayerId\": \"" + m_sKillerPlayerId + "\",";
        json = json + "\"killerPlayerName\": \"" + m_sKillerPlayerName + "\"";
        json = json + "}";
        return json;
    }
}

class IA_HVTGuardKillEvent : IA_StatEvent
{
    string m_sKillerPlayerId;
    string m_sKillerPlayerName;

    void IA_HVTGuardKillEvent(string killerId, string killerName)
    {
        m_sEventType = "HVTGuardKill";
        m_sKillerPlayerId = killerId;
        m_sKillerPlayerName = killerName;
    }

    override string ToJson()
    {
        // Manual JSON serialization
        string json = "{";
        json = json + "\"eventType\": \"" + m_sEventType + "\",";
        json = json + "\"killerPlayerId\": \"" + m_sKillerPlayerId + "\",";
        json = json + "\"killerPlayerName\": \"" + m_sKillerPlayerName + "\"";
        json = json + "}";
        return json;
    }
} 