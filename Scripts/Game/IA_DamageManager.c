/*
// Damage Manager for IA mod
// Handles damage events and triggers appropriate AI reactions

class IA_DamageManager
{
    // Singleton instance
    private static ref IA_DamageManager s_Instance;
    
    // Dictionary to map entities to their owning AI groups
    private ref map<IEntity, IA_AiGroup> m_EntityToGroupMap = new map<IEntity, IA_AiGroup>();
    
    // Private constructor for singleton
    private void IA_DamageManager()
    {
        m_EntityToGroupMap = new map<IEntity, IA_AiGroup>();
        
        // Hook into global damage events
        ScriptInvokerBase<BaseDamageHandlerComponent, DamageHandlerData, IEntity> damageInvoker = BaseDamageHandlerComponent.s_OnDamageStateChanged;
        if (damageInvoker)
        {
            damageInvoker.Insert(OnDamageReceived);
        }
    }
    
    // Singleton accessor
    static IA_DamageManager GetInstance()
    {
        if (!s_Instance)
            s_Instance = new IA_DamageManager();
        
        return s_Instance;
    }
    
    // Register an entity as part of an AI group
    void RegisterEntity(IEntity entity, IA_AiGroup group)
    {
        if (!entity || !group)
            return;
            
        m_EntityToGroupMap.Set(entity, group);
    }
    
    // Unregister an entity when it's no longer relevant
    void UnregisterEntity(IEntity entity)
    {
        if (!entity)
            return;
            
        m_EntityToGroupMap.Remove(entity);
    }
    
    // Handle damage events globally
    private void OnDamageReceived(notnull BaseDamageHandlerComponent damageHandler, DamageHandlerData data, IEntity instigator)
    {
        // Get the entity that was damaged
        IEntity damagedEntity = damageHandler.GetOwner();
        if (!damagedEntity)
            return;
            
        // Find the AI group this entity belongs to
        IA_AiGroup owningGroup;
        if (!m_EntityToGroupMap.Find(damagedEntity, owningGroup) || !owningGroup)
            return;
            
        // Check if unit is still alive
        if (damageHandler.GetDeathState() == EDamageState.DESTROYED)
            return; // Death is handled separately through the death callback
        
        // Trigger the OnMemberDamaged method for the group
        float damageAmount = data.GetDamage();
        owningGroup.OnMemberDamaged(damagedEntity, instigator, damageAmount);

    }
    
    // Register all units in an AI group
    void RegisterAIGroup(IA_AiGroup group)
    {
        if (!group)
            return;
            
        array<SCR_ChimeraCharacter> characters = group.GetGroupCharacters();
        foreach (SCR_ChimeraCharacter character : characters)
        {
            if (character)
                RegisterEntity(character, group);
        }
    }
} 
*/ 