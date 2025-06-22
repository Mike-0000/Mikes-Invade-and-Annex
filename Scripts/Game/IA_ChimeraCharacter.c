modded class SCR_ChimeraCharacter{
	
	[RplProp(onRplName: "OnRoleChanged")]
	IA_PlayerRole m_eReplicatedRole;
	
	private IA_PlayerRole m_eCurrentRole;
	
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		// The server is authoritative for setting the initial role.
		if (Replication.IsServer())
		{
			// Initialize with the value set in the editor or a default.
			// Use 'false' for forceReplication initially, unless you specifically need the callback to run immediately.
			SetRole(IA_PlayerRole.RIFLEMAN, false);
		}
		if (!Replication.IsServer())
		{
			m_eCurrentRole = m_eReplicatedRole;
			// Optional: If client-side logic needs to react to the initial role, call HandleRoleChangeEffects here.
			// HandleRoleChangeEffects(m_eCurrentRole);
		}
	}
	
	void SetUIOne(string messageType, string taskTitle, int playerId){
		//IA_ReplicationWorkaround rep = IA_ReplicationWorkaround.Instance();
		//rep.TriggerGlobalNotificationFinal(messageType, taskTitle, playerId);
		
		if(!Replication.IsServer()){
						Do_TriggerSetUIOneHandler(messageType, taskTitle, playerId);

			Print("Running As Client",LogLevel.NORMAL);
			
		} else{
						Rpc(RpcDo_TriggerSetUIOneHandler, messageType, taskTitle, playerId);
						Do_TriggerSetUIOneHandler(messageType, taskTitle, playerId);

			Print("Running As Server",LogLevel.NORMAL);
		}
		
			
	
	}
	/*
	void Do_TriggerSetUIOneHandler(string messageType, string taskTitle, int playerId){
		//Print("Running Do_TriggerSetUIOneHandler for "+ messageType + " And " + taskTitle,LogLevel.NORMAL);
		PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if(!pc)
			return;
		SCR_HUDManagerComponent displayManager = SCR_HUDManagerComponent.Cast(pc.FindComponent(SCR_HUDManagerComponent)); 

		if (!displayManager)
			return;
		//Print("Running Do_TriggerSetUIOneHandler",LogLevel.NORMAL);
		IA_NotificationDisplay notificationDisplay = IA_NotificationDisplay.Cast(displayManager.FindInfoDisplay(IA_NotificationDisplay));
			
			if (!notificationDisplay)
			{
							
					Print("notificationDisplay is NULL", LogLevel.ERROR);
				
			}

			if (notificationDisplay)
			{
				if (messageType == "TaskCreated")
				{
					GetGame().GetCallqueue().CallLater(notificationDisplay.DisplayTaskCreatedNotification, 100, false, taskTitle);
				}else if (messageType == "AreaGroupCompleted")
				{
					notificationDisplay.DisplayAreaCompletedNotification("All Objectives In Area Complete, RTB");
				}
				else if (messageType == "DefendMissionStarted")
				{
					notificationDisplay.DisplayTaskCreatedNotification(taskTitle);
				}
				else if (messageType == "RadioTowerDefenseStarted")
				{
					string message = "Enemy reinforcements are responding! Destroy the "+taskTitle+" to prevent their continuous reinforcements.";
					notificationDisplay.ShowNotification(message, true, "1,0.5,0,1"); 
					GetGame().GetCallqueue().CallLater(notificationDisplay.HideNotification, 10000, false);
				}
			}
			else
			{
				Print(string.Format("[IA_AreaInstance] Could not find or create IA_NotificationDisplay for player."), LogLevel.WARNING);
			}
	
	}
	*/
	
	void Do_TriggerSetUIOneHandler(string messageType, string taskTitle, int playerId){
		//Print("Running Do_TriggerSetUIOneHandler for "+ messageType + " And " + taskTitle,LogLevel.NORMAL);
		PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if(!pc)
			return;
		SCR_HUDManagerComponent displayManager = SCR_HUDManagerComponent.Cast(pc.FindComponent(SCR_HUDManagerComponent)); 

			if (!displayManager)
				return;
		//Print("Running Do_TriggerSetUIOneHandler",LogLevel.NORMAL);
			IA_NotificationDisplay notificationDisplay = IA_NotificationDisplay.Cast(displayManager.FindInfoDisplay(IA_NotificationDisplay));
			
			if (!notificationDisplay)
			{
							
					Print("notificationDisplay is NULL", LogLevel.ERROR);
				
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
			}else if (messageType == "RadioTowerDefenseStarted")
			{
				string message = "Enemy reinforcements are responding! Destroy the "+taskTitle+" to prevent their continuous reinforcements.";
				GetGame().GetCallqueue().CallLater(notificationDisplay.ShowNotification, 20000, false, message, true, "yellow"); 
				GetGame().GetCallqueue().CallLater(notificationDisplay.HideNotification, 30000, false);
			}else if (messageType == "DefendMissionStarted")
				{
					notificationDisplay.DisplayTaskCreatedNotification(taskTitle);
				}
				else if (messageType == "CivilianRevoltStarted")
				{
					string message = "The civilian population is revolting! Be advised, they are now hostile.";
					notificationDisplay.ShowNotification(message, true, "red");
					GetGame().GetCallqueue().CallLater(notificationDisplay.HideNotification, 10000, false);
				}
				else if (messageType == "CivilianRevoltReinforcements")
				{
					string message = "Civilian militias are reinforcing the objective areas!";
					notificationDisplay.ShowNotification(message, true, "red");
					GetGame().GetCallqueue().CallLater(notificationDisplay.HideNotification, 10000, false);
				}
				else if (messageType == "CaptureStarted")
				{
					notificationDisplay.ShowNotification(taskTitle, true, "green");
					GetGame().GetCallqueue().CallLater(notificationDisplay.HideNotification, 5000, false);
				}
				else if (messageType == "CapturePaused")
				{
					notificationDisplay.ShowNotification(taskTitle, true, "yellow");
					GetGame().GetCallqueue().CallLater(notificationDisplay.HideNotification, 5000, false);
				}
				else if (messageType == "Capture50Percent")
				{
					notificationDisplay.ShowNotification(taskTitle, true, "green");
					GetGame().GetCallqueue().CallLater(notificationDisplay.HideNotification, 5000, false);
				}
			
			
			
				// Optional: Auto-hide after a few seconds
				// GetGame().GetCallqueue().CallLater(notificationDisplay.HideNotification, 5000, false);
			}
			else
			{
				Print(string.Format("[IA_AreaInstance] Could not find or create IA_NotificationDisplay for player."), LogLevel.WARNING);
			}
	
	}
	
	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcDo_TriggerSetUIOneHandler(string messageType, string taskTitle, int playerId){
		Do_TriggerSetUIOneHandler(messageType, taskTitle, playerId);
	}
/*	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RpcDo_TriggerSetUIOne(string messageType, string taskTitle){
		Print("Going For RpcDo_TriggerSetUIOne",LogLevel.NORMAL);
		
		SCR_HUDManagerComponent displayManager = SCR_HUDManagerComponent.Cast(this.FindComponent(SCR_HUDManagerComponent)); 

			if (!displayManager)
				return;

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
				Print(string.Format("[IA_AreaInstance] Could not find or create IA_NotificationDisplay for player."), LogLevel.WARNING);
			}
		
	}
	
	*/
	bool SetRole(IA_PlayerRole newRole, bool forceReplication = false)
	{
		// Enforce server-only execution for role changes.
		if (!Replication.IsServer())
		{
			Print("IA_PlayerRoleComponent::SetRole - ERROR: Attempted to set role on client!", LogLevel.ERROR);
			return false;
		}

		// Avoid redundant updates if the role is the same, unless forced.
		if (m_eReplicatedRole == newRole && !forceReplication)
			return false;

		// Update the server's internal state and the replicated property.
		m_eCurrentRole = newRole;
		m_eReplicatedRole = newRole;

		// Crucial step: Notify the replication system that this component's replicated properties have changed.
		Replication.BumpMe();

		// Perform any server-side logic associated with the role change.
		HandleRoleChangeEffects(newRole);

		//Print(string.Format("IA_PlayerRoleComponent::SetRole - Role set to %1 on server for entity %2", typename.EnumToString(IA_PlayerRole, newRole), GetOwner().GetID()), LogLevel.DEBUG);

		return true;
	}
		IA_PlayerRole GetRole()
	{
		// On clients, m_eCurrentRole is updated by the OnRoleChanged callback.
		// On the server, m_eCurrentRole is updated directly by SetRole.
		return m_eCurrentRole;
	}
	
	private void OnRoleChanged()
	{
		// Update the client's local understanding of the role.
		m_eCurrentRole = m_eReplicatedRole;

		//Print(string.Format("IA_PlayerRoleComponent::OnRoleChanged - Role updated to %1 on client for entity %2", typename.EnumToString(IA_PlayerRole, m_eCurrentRole), GetOwner().GetID()), LogLevel.DEBUG);

		// Perform any client-side logic needed when the role changes (e.g., update UI elements).
		HandleRoleChangeEffects(m_eCurrentRole);
	}
	
	private void HandleRoleChangeEffects(IA_PlayerRole newRole)
	{
		IEntity owner = this;
		if (!owner) return; // Safety check

		//Print(string.Format("IA_PlayerRoleComponent::HandleRoleChangeEffects - Processing role %1 for entity %2 (Server: %3)",
		//	typename.EnumToString(IA_PlayerRole, newRole),
		//	owner.GetID(),
		//	Replication.IsServer()), LogLevel.DEBUG);

		// --- Add Role-Specific Logic Here ---
		// Example:
		// if (Replication.IsServer())
		// {
		//     // Server-specific actions like changing loadout
		// }
		// else
		// {
		//     // Client-specific actions like updating a UI icon
		// }

		// switch (newRole)
		// {
		// 	case IA_PlayerRole.MEDIC:
		// 		// TODO: Implement logic for Medic role
		// 		break;
		// 	case IA_PlayerRole.PILOT:
		// 		// TODO: Implement logic for Pilot role
		// 		break;
		// }
	}
	
	void SetCurrentArea(IA_AreaInstance area)
	{
		// Implementation of SetCurrentArea method
	}
}


// Enum for Player Roles
enum IA_PlayerRole
{
	NONE = 0,
	RIFLEMAN,
	MACHINEGUNNER,
	GRENADIER,
	MARKSMAN,
	MEDIC,
	ANTITANK_LIGHT,
	ANTITANK_HEAVY,
	TEAMLEADER,
	PILOT,
	CREWMAN
};

