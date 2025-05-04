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

// Game Component Class (Metadata for the component)
[ComponentEditorProps(category: "Invade & Annex/Player", description: "Tracks player role.")]
class IA_PlayerRoleComponentClass : ScriptComponentClass
{
	// This class typically remains empty unless you need static configuration.
};

// Game Component Implementation (The actual logic)
class IA_PlayerRoleComponent : ScriptComponent
{
	// This attribute allows setting a default role in the editor, but the actual role will be managed by gameplay logic.
	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, desc: "Initial/Default player role (can be overridden)", enums: ParamEnumArray.FromEnum(IA_PlayerRole))]
	private IA_PlayerRole m_eEditorAssignedRole;

	// The currently assigned role for this player instance.
	private IA_PlayerRole m_eCurrentRole;

	// Network replication: This variable's value will be synchronized across server and clients.
	// 'OnRoleChanged' is the callback function triggered on clients when the value is updated by the server.
	[RplProp(onRplName: "OnRoleChanged")]
	private IA_PlayerRole m_eReplicatedRole;

	//------------------------------------------------------------------------------------------------
	// LIFECYCLE & INITIALIZATION
	//------------------------------------------------------------------------------------------------

	// OnPostInit is called after the component is created and basic properties are set.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		// We need EOnInit for further setup after all components are ready.
		SetEventMask(owner, EntityEvent.INIT);

		// The server is authoritative for setting the initial role.
		if (Replication.IsServer())
		{
			// Initialize with the value set in the editor or a default.
			// Use 'false' for forceReplication initially, unless you specifically need the callback to run immediately.
			SetRole(m_eEditorAssignedRole, false);
		}
	}

	// EOnInit is called after the entity and all its components are fully initialized.
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		// For clients, when the entity initializes, the replicated role might already be set.
		// We sync the local m_eCurrentRole to match the initial replicated state.
		if (!Replication.IsServer())
		{
			m_eCurrentRole = m_eReplicatedRole;
			// Optional: If client-side logic needs to react to the initial role, call HandleRoleChangeEffects here.
			// HandleRoleChangeEffects(m_eCurrentRole);
		}
	}

	//------------------------------------------------------------------------------------------------
	// ROLE MANAGEMENT (Server-Side Logic)
	//------------------------------------------------------------------------------------------------

	// Sets the player's role. This should ONLY be called on the server.
	// Returns true if the role was actually changed, false otherwise.
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

	//------------------------------------------------------------------------------------------------
	// ROLE ACCESS (Client & Server)
	//------------------------------------------------------------------------------------------------

	// Gets the player's current role. Safe to call on both server and clients.
	IA_PlayerRole GetRole()
	{
		// On clients, m_eCurrentRole is updated by the OnRoleChanged callback.
		// On the server, m_eCurrentRole is updated directly by SetRole.
		return m_eCurrentRole;
	}

	//------------------------------------------------------------------------------------------------
	// REPLICATION CALLBACK (Client-Side Logic)
	//------------------------------------------------------------------------------------------------

	// This function is automatically called on clients when the server changes 'm_eReplicatedRole'.
	private void OnRoleChanged()
	{
		// Update the client's local understanding of the role.
		m_eCurrentRole = m_eReplicatedRole;

		//Print(string.Format("IA_PlayerRoleComponent::OnRoleChanged - Role updated to %1 on client for entity %2", typename.EnumToString(IA_PlayerRole, m_eCurrentRole), GetOwner().GetID()), LogLevel.DEBUG);

		// Perform any client-side logic needed when the role changes (e.g., update UI elements).
		HandleRoleChangeEffects(m_eCurrentRole);
	}

	//------------------------------------------------------------------------------------------------
	// ROLE CHANGE EFFECTS (Server & Client)
	//------------------------------------------------------------------------------------------------

	// Centralized function to handle the consequences of a role change.
	// This is called on the server immediately when SetRole is called,
	// and on clients when the OnRoleChanged callback is triggered.
	private void HandleRoleChangeEffects(IA_PlayerRole newRole)
	{
		IEntity owner = GetOwner();
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

	//------------------------------------------------------------------------------------------------
	// STATIC HELPER FUNCTION
	//------------------------------------------------------------------------------------------------

	// Provides a convenient way to get this component from any player entity.
	static IA_PlayerRoleComponent GetRoleComponent(IEntity playerEntity)
	{
		if (!playerEntity)
			return null;
		return IA_PlayerRoleComponent.Cast(playerEntity.FindComponent(IA_PlayerRoleComponent));
	}
}; 