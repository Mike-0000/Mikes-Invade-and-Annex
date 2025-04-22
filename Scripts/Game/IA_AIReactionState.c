enum IA_AIReactionType
{
    None,               // No special reaction
    EnemySpotted,       // Enemy spotted in distance
    UnderFire,          // NEW: Replaces TakingFire and UnderAttack (projectile impacts/flybys)
    GroupMemberHit,     // Group member was hit but still alive
    GroupMemberKilled,  // Group member was killed
    VehicleDisabled,    // Vehicle damaged or disabled
    LowAmmunition,      // Running low on ammunition
    Suppressed,         // Being suppressed by heavy fire
    Flanked,            // Enemies spotted on flank
    Surrounded,         // Enemies on multiple sides
    HighCasualties      // Lost significant portion of the group
}

class IA_AIReactionState
{
    private IA_AIReactionType m_reactionType = IA_AIReactionType.None;
    
    private int m_reactionStartTime = 0;
    
    private int m_reactionDuration = 0;
    
    private float m_intensity = 0.0;
    
    private vector m_sourcePosition = vector.Zero;
    private IEntity m_sourceEntity = null;
    private IA_Faction m_sourceFaction = IA_Faction.NONE;
    
    void SetReaction(IA_AIReactionType type, float intensity, int duration, vector sourcePos = vector.Zero, IEntity sourceEntity = null, IA_Faction sourceFaction = IA_Faction.NONE)
    {
        m_reactionType = type;
        m_intensity = intensity;
        m_reactionDuration = duration;
        m_reactionStartTime = System.GetUnixTime();
        m_sourcePosition = sourcePos;
        m_sourceEntity = sourceEntity;
        m_sourceFaction = sourceFaction;
    }
    
    bool IsReactionActive()
    {
        if (m_reactionType == IA_AIReactionType.None)
            return false;
            
        int currentTime = System.GetUnixTime();
        int elapsedTime = currentTime - m_reactionStartTime;
        bool isActive = (elapsedTime < m_reactionDuration);

        
        return isActive;
    }
    
    void ClearReaction()
    {
        m_reactionType = IA_AIReactionType.None;
        m_intensity = 0.0;
        m_sourcePosition = vector.Zero;
        m_sourceEntity = null;
        m_sourceFaction = IA_Faction.NONE;
    }
    
    IA_AIReactionType GetReactionType() { return m_reactionType; }
    float GetIntensity() { return m_intensity; }
    vector GetSourcePosition() { return m_sourcePosition; }
    IEntity GetSourceEntity() { return m_sourceEntity; }
    IA_Faction GetSourceFaction() { return m_sourceFaction; }
    int GetTimeRemaining()
    {
        int currentTime = System.GetUnixTime();
        int elapsed = currentTime - m_reactionStartTime;
        return Math.Max(0, m_reactionDuration - elapsed);
    }
}

class IA_AIReactionManager
{
    private ref IA_AIReactionState m_currentReaction = new IA_AIReactionState();
    
    private ref array<ref IA_AIReactionState> m_pendingReactions = {};
    
    private int m_lastReactionProcessTime = 0;
    
    private static const int LOG_RATE_LIMIT_SECONDS = 3;
    private int m_lastTriggerLogTime = 0;
    private int m_lastStateChangeLogTime = 0; // For supersede/new current logs
    
    void IA_AIReactionManager()
    {
        m_pendingReactions = new array<ref IA_AIReactionState>();
    }
    
    bool HasActiveReaction()
    {
        return m_currentReaction.IsReactionActive();
    }
    
    IA_AIReactionState GetCurrentReaction()
    {
        return m_currentReaction;
    }
    
    void ProcessReactions()
    {
        int currentTime = System.GetUnixTime();
        if (currentTime - m_lastReactionProcessTime < 1) // Process only once per second at most
            return;
            
        m_lastReactionProcessTime = currentTime;
        

        bool currentActive = m_currentReaction.IsReactionActive();
        int pendingCount = m_pendingReactions.Count();

        
        if (!m_currentReaction.IsReactionActive())
        {

            
            if (!m_pendingReactions.IsEmpty())
            {

                
                IA_AIReactionState nextReaction = SelectHighestPriorityReaction();
                if (nextReaction)
                {

                    
                    m_currentReaction = nextReaction;
                    m_pendingReactions.RemoveItem(nextReaction);
                }
            }
        }
    }
    
    void TriggerReaction(IA_AIReactionType type, float intensity, vector sourcePos = vector.Zero, IEntity sourceEntity = null, IA_Faction sourceFaction = IA_Faction.NONE)
    {
        int duration = CalculateReactionDuration(type, intensity);
        
        IA_AIReactionState newReaction = new IA_AIReactionState();
        newReaction.SetReaction(type, intensity, duration, sourcePos, sourceEntity, sourceFaction);
        
        int currentTime = System.GetUnixTime();

              
        if (currentTime - m_lastTriggerLogTime > LOG_RATE_LIMIT_SECONDS)
        {

            m_lastTriggerLogTime = currentTime;
        }
        
        if (!m_currentReaction.IsReactionActive())
        {

                
            if (currentTime - m_lastStateChangeLogTime > LOG_RATE_LIMIT_SECONDS)
            {
                m_lastStateChangeLogTime = currentTime;
            }
            
            m_currentReaction = newReaction;
        }
        else
        {
            if (ShouldSupersede(newReaction, m_currentReaction))
            {

                
                if (currentTime - m_lastStateChangeLogTime > LOG_RATE_LIMIT_SECONDS)
                {

                    m_lastStateChangeLogTime = currentTime;
                }
                
                m_pendingReactions.Insert(m_currentReaction);
                
                m_currentReaction = newReaction;
            }
            else
            {

                
                m_pendingReactions.Insert(newReaction);
            }
        }
    }
    
    private IA_AIReactionState SelectHighestPriorityReaction()
    {
        if (m_pendingReactions.IsEmpty())
            return null;
            
        IA_AIReactionState highestPriorityReaction = m_pendingReactions[0];
        int highestPriority = GetReactionPriority(highestPriorityReaction.GetReactionType());
        
        for (int i = 1; i < m_pendingReactions.Count(); i++)
        {
            IA_AIReactionState reaction = m_pendingReactions[i];
            int priority = GetReactionPriority(reaction.GetReactionType());
            
            if (priority > highestPriority || 
                (priority == highestPriority && reaction.GetIntensity() > highestPriorityReaction.GetIntensity()))
            {
                highestPriorityReaction = reaction;
                highestPriority = priority;
            }
        }
        
        return highestPriorityReaction;
    }
    
    private bool ShouldSupersede(IA_AIReactionState newReaction, IA_AIReactionState currentReaction)
    {
        int newPriority = GetReactionPriority(newReaction.GetReactionType());
        int currentPriority = GetReactionPriority(currentReaction.GetReactionType());
        
        // Higher priority always supersedes
        if (newPriority > currentPriority)
            return true;
            
        // If same priority, higher intensity supersedes
        if (newPriority == currentPriority && newReaction.GetIntensity() > currentReaction.GetIntensity())
            return true;
            
        return false;
    }
    
    private int GetReactionPriority(IA_AIReactionType type)
    {
        switch (type)
        {
            case IA_AIReactionType.GroupMemberKilled: return 10;
            case IA_AIReactionType.Surrounded: return 9;
            case IA_AIReactionType.HighCasualties: return 8;
            case IA_AIReactionType.VehicleDisabled: return 7;
            case IA_AIReactionType.UnderFire: return 6;
            case IA_AIReactionType.Flanked: return 5;
            case IA_AIReactionType.GroupMemberHit: return 4;
            case IA_AIReactionType.Suppressed: return 2;
            case IA_AIReactionType.EnemySpotted: return 1;
            default: return 0;
        }
		return 0;
    }
    
    private int CalculateReactionDuration(IA_AIReactionType type, float intensity)
    {
        // Base durations for each reaction type
        int baseDuration = 0;
        
        switch (type)
        {
            case IA_AIReactionType.EnemySpotted: baseDuration = 30; break;
            case IA_AIReactionType.UnderFire: baseDuration = 60; break;
            case IA_AIReactionType.GroupMemberHit: baseDuration = 30; break;
            case IA_AIReactionType.GroupMemberKilled: baseDuration = 90; break;
            case IA_AIReactionType.VehicleDisabled: baseDuration = 120; break;
            case IA_AIReactionType.LowAmmunition: baseDuration = 300; break;
            case IA_AIReactionType.Suppressed: baseDuration = 20; break;
            case IA_AIReactionType.Flanked: baseDuration = 45; break;
            case IA_AIReactionType.Surrounded: baseDuration = 120; break;
            case IA_AIReactionType.HighCasualties: baseDuration = 180; break;
            default: baseDuration = 30;
        }
        
        // Adjust duration based on intensity (0.0-1.0)
        return baseDuration + Math.Round(baseDuration * intensity * 0.5);
    }
    
    // Clear all reactions and reset the manager state
    void ClearAllReactions()
    {
        // Clear current reaction
        m_currentReaction.ClearReaction();
        
        // Clear pending reactions queue
        m_pendingReactions.Clear();
        
    }
} 