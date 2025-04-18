enum IA_AIReactionType
{
    None,               // No special reaction
    TakingFire,         // Under fire but source unknown
    EnemySpotted,       // Enemy spotted in distance
    UnderAttack,        // Taking direct fire from known enemy
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
    // The type of reaction currently active
    private IA_AIReactionType m_reactionType = IA_AIReactionType.None;
    
    // The timestamp when this reaction began
    private int m_reactionStartTime = 0;
    
    // Duration for which this reaction should be active (in seconds)
    private int m_reactionDuration = 0;
    
    // Intensity level of the reaction (0.0-1.0)
    private float m_intensity = 0.0;
    
    // Source of the reaction (position, entity, faction)
    private vector m_sourcePosition = vector.Zero;
    private IEntity m_sourceEntity = null;
    private IA_Faction m_sourceFaction = IA_Faction.NONE;
    
    // Methods for updating and querying the reaction state
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
        return (currentTime - m_reactionStartTime < m_reactionDuration);
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

// Manager class to handle reaction states for an AI group
class IA_AIReactionManager
{
    // Current active reaction
    private ref IA_AIReactionState m_currentReaction = new IA_AIReactionState();
    
    // Queue of pending reactions
    private ref array<ref IA_AIReactionState> m_pendingReactions = {};
    
    // Last time reactions were processed
    private int m_lastReactionProcessTime = 0;
    
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
    
    // Process reactions and update current state
    void ProcessReactions()
    {
        // Skip frequent processing
        int currentTime = System.GetUnixTime();
        if (currentTime - m_lastReactionProcessTime < 1) // Process only once per second at most
            return;
            
        m_lastReactionProcessTime = currentTime;
        
        // Check if current reaction has expired
        if (!m_currentReaction.IsReactionActive())
        {
            // If we have pending reactions, process the next one
            if (!m_pendingReactions.IsEmpty())
            {
                // Get highest priority reaction from queue
                IA_AIReactionState nextReaction = SelectHighestPriorityReaction();
                if (nextReaction)
                {
                    m_currentReaction = nextReaction;
                    m_pendingReactions.RemoveItem(nextReaction);
                }
            }
        }
    }
    
    // Add a new reaction to be processed
    void TriggerReaction(IA_AIReactionType type, float intensity, vector sourcePos = vector.Zero, IEntity sourceEntity = null, IA_Faction sourceFaction = IA_Faction.NONE)
    {
        // Set reaction duration based on type and intensity
        int duration = CalculateReactionDuration(type, intensity);
        
        // Create the new reaction state
        IA_AIReactionState newReaction = new IA_AIReactionState();
        newReaction.SetReaction(type, intensity, duration, sourcePos, sourceEntity, sourceFaction);
        
        // If no active reaction, set this as current
        if (!m_currentReaction.IsReactionActive())
        {
            m_currentReaction = newReaction;
        }
        else
        {
            // Check if this reaction should supersede the current one
            if (ShouldSupersede(newReaction, m_currentReaction))
            {
                // Add current reaction to pending queue
                m_pendingReactions.Insert(m_currentReaction);
                
                // Set new reaction as current
                m_currentReaction = newReaction;
            }
            else
            {
                // Add to pending queue
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
            case IA_AIReactionType.UnderAttack: return 6;
            case IA_AIReactionType.Flanked: return 5;
            case IA_AIReactionType.GroupMemberHit: return 4;
            case IA_AIReactionType.TakingFire: return 3;
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
            case IA_AIReactionType.TakingFire: baseDuration = 45; break;
            case IA_AIReactionType.UnderAttack: baseDuration = 60; break;
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
        
        Print("[IA_AI_REACTION] Cleared all reactions", LogLevel.WARNING);
    }
} 