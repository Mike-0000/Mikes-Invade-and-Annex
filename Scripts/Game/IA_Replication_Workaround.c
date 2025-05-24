
///////////////////////////////////////////////////////////////////////
// 10) REPLICATION WORKAROUND
///////////////////////////////////////////////////////////////////////
class IA_ReplicationWorkaroundClass : GenericEntityClass {}

class IA_ReplicatedAreaInstance
{
    void IA_ReplicatedAreaInstance(string nm, IA_Faction fac, int strVal)
    {
        //Print("[DEBUG] IA_ReplicatedAreaInstance constructor called with name: " + nm + ", faction: " + fac + ", strength: " + strVal, LogLevel.NORMAL);
        name = nm;
        faction = fac;
        strength = strVal;
    }
    string name;
    IA_Faction faction;
    int strength;
};

class IA_ReplicationWorkaround : GenericEntity
{
    static private ref array<ref IA_ReplicatedAreaInstance> m_areaList = {};
    static private ref array<ref IA_AreaInstance> m_areaInstances = {};

    static private IA_ReplicationWorkaround m_instance;

    void IA_ReplicationWorkaround(IEntitySource src, IEntity parent)
    {
        //Print("[DEBUG] IA_ReplicationWorkaround constructor called.", LogLevel.NORMAL);
        m_instance = this;
    }

    static IA_ReplicationWorkaround Instance()
    {
        return m_instance;
    }

    static void Reset()
    {
        m_areaList.Clear();
        m_areaInstances.Clear();
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
    private void RpcTo_AddArea(string areaName, IA_Faction fac, int strVal)
    {
        //Print("[DEBUG] RpcTo_AddArea called for area: " + areaName + ", faction: " + fac + ", strength: " + strVal, LogLevel.NORMAL);
        m_areaList.Insert(new IA_ReplicatedAreaInstance(areaName, fac, strVal));
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
    private void RpcDo_SetFaction(string areaName, int facInt)
    {
		if(!Replication.IsRunning())
			return;
        if (!IA_Game.HasInstance())
            return;
          IA_Game g = IA_Game.Instantiate();
        IA_AreaInstance inst = g.FindAreaInstance(areaName);
        if (!inst)
            return;
        IA_Faction f = IA_FactionFromInt(facInt);
        inst.OnFactionChange(f);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
    private void RpcDo_SetStrength(string areaName, int s)
    {
		if(!Replication.IsRunning())
			return;
        if (!IA_Game.HasInstance())
            return;
        IA_Game g = IA_Game.Instantiate();
        IA_AreaInstance inst = g.FindAreaInstance(areaName);
        if (!inst)
            return;
        inst.OnStrengthChange(s);
    }

    void AddArea(IA_AreaInstance aInst)
    {
        m_areaInstances.Insert(aInst);
        Rpc(RpcTo_AddArea, aInst.m_area.GetName(), aInst.m_faction, aInst.m_strength);
    }

    void SetFaction(string areaName, IA_Faction f)
    {
        int fi = IA_FactionToInt(f);
        Rpc(RpcDo_SetFaction, areaName, fi);
    }

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcDo_TriggerGlobalNotification(string messageType, string taskTitle, int playerID){
			PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerID);
			if (!pc)
				return;

			SCR_HUDManagerComponent displayManager = SCR_HUDManagerComponent.Cast(pc.FindComponent(SCR_HUDManagerComponent)); 

			if (!displayManager)
				return;

			// The following lines will likely cause errors if SCR_HUDManagerComponent 
			// doesn't have FindDisplay(typename) and RegisterInfoDisplay(typename, ...)
			IA_NotificationDisplay notificationDisplay = IA_NotificationDisplay.Cast(displayManager.FindInfoDisplay(IA_NotificationDisplay));
			
			if (!notificationDisplay)
			{
							
					Print("notificationDisplay is NULL", LogLevel.FATAL);
				
			}

			if (notificationDisplay)
			{
				if (messageType == "TaskCreated")
				{
					GetGame().GetCallqueue().CallLater(notificationDisplay.DisplayTaskCreatedNotification, 100, false, taskTitle);
				}else if (messageType == "AreaGroupCompleted")
			{
				// Using CallLater to avoid potential issues with immediate UI updates in certain contexts,
				// and to allow a slight delay for dramatic effect or to prevent spam if zones complete rapidly.
				// Random delay removed as it's for a group completion, not individual tasks.
				GetGame().GetCallqueue().CallLater(notificationDisplay.DisplayAreaCompletedNotification, 100, false, taskTitle); 
			}else if (messageType == "TaskCompleted")
			{
				GetGame().GetCallqueue().CallLater(notificationDisplay.DisplayTaskCompletedNotification, 100, false, taskTitle); 
			}
				// Optional: Auto-hide after a few seconds
				// GetGame().GetCallqueue().CallLater(notificationDisplay.HideNotification, 5000, false);
			}
			else
			{
				Print(string.Format("[IA_AreaInstance] Could not find or create IA_NotificationDisplay for player %1.", playerID), LogLevel.WARNING);
			}
		
	}
	
	void TriggerGlobalNotification(string messageType, string taskTitle){
		array<int> playerIDs = new array<int>();
		GetGame().GetPlayerManager().GetAllPlayers(playerIDs);
		foreach (int playerID : playerIDs)
		 	Rpc(RpcDo_TriggerGlobalNotification, messageType, taskTitle, playerID);
	}
	
    void SetStrength(string areaName, int val)
    {
        Rpc(RpcDo_SetStrength, areaName, val);
    }

    override bool RplSave(ScriptBitWriter writer)
    {
        int c = m_areaInstances.Count();
        writer.Write(c, 16);
        for (int i = 0; i < c; i = i + 1)
        {
            IA_AreaInstance inst = m_areaInstances[i];
            writer.WriteString(inst.m_area.GetName());
            writer.Write(inst.m_strength, 8);
            int fInt = IA_FactionToInt(inst.m_faction);
            writer.Write(fInt, 8);
        }
        return true;
    }

    override bool RplLoad(ScriptBitReader reader)
    {
        int areaCount;
        if (!reader.Read(areaCount, 16))
            return false;
        for (int i = 0; i < areaCount; i = i + 1)
        {
            string nm;
            int strVal;
            int facInt;
            if (!reader.ReadString(nm) || !reader.Read(strVal, 8) || !reader.Read(facInt, 8))
                return false;
            IA_Faction ff = IA_FactionFromInt(facInt);
            IA_ReplicatedAreaInstance rep = new IA_ReplicatedAreaInstance(nm, ff, strVal);
            m_areaList.Insert(rep);
        }
        return true;
    }
};
