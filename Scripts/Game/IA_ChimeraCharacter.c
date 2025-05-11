modded class SCR_ChimeraCharacter{
	
	[RplProp(onRplName: "OnRoleChanged")]
	private IA_PlayerRole m_eReplicatedRole;
	
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

