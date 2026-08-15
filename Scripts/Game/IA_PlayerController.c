//------------------------------------------------------------------------------------------------
//! Client-to-server channel for the admin config panel.
//! RPCs on IA_MissionInitializer fail on dedicated clients when that world entity is not
//! streamed; the local player controller is always owned by the client.
//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void IA_AskUpdateAdminConfig(string packed)
	{
		if (Replication.IsServer())
		{
			IA_ApplyAdminConfigIfAdmin(packed);
			return;
		}

		Rpc(RpcAsk_IA_UpdateAdminConfig, packed);
	}

	//------------------------------------------------------------------------------------------------
	void IA_AskForceCompleteZone()
	{
		if (Replication.IsServer())
		{
			IA_ForceCompleteZoneIfAdmin();
			return;
		}

		Rpc(RpcAsk_IA_ForceCompleteZone);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_UpdateAdminConfig(string packed)
	{
		IA_ApplyAdminConfigIfAdmin(packed);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_IA_ForceCompleteZone()
	{
		IA_ForceCompleteZoneIfAdmin();
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_ApplyAdminConfigIfAdmin(string packed)
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] Admin config update rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
		{
			Print("[IA] Admin config update rejected: mission initializer missing", LogLevel.ERROR);
			return;
		}

		init.ServerApplyAdminConfig(packed);
	}

	//------------------------------------------------------------------------------------------------
	protected void IA_ForceCompleteZoneIfAdmin()
	{
		if (!IA_IsAdminCaller())
		{
			Print("[IA] Force complete zone rejected: caller is not admin (player " + GetPlayerId().ToString() + ")", LogLevel.WARNING);
			return;
		}

		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		if (!init)
			return;

		init.ServerForceCompleteZone();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IA_IsAdminCaller()
	{
		if (!Replication.IsRunning())
			return true;

		return SCR_Global.IsAdmin(GetPlayerId());
	}
}
