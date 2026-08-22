// HandleFailedNavlinkDoor.bt runs a Parallel sibling ToggleMaxLOD with
// PerformOnAbort so AllowMaxLOD fires when the door handler is cancelled.
// AgentIn is only written after SelectDoorOperatorAgent. Aborting
// MoveToLocation first (order swap, member death) makes GetVariableIn
// fail and vanilla NodeErrors. A written-null agent is already SUCCESS.

modded class SCR_AIToggleMaxLOD
{
	override void OnAbort(AIAgent owner, Node nodeCausingAbort)
	{
		if (m_bPerformOnAbort && !m_bAbortFinished)
		{
			AIAgent agent;
			if (!GetVariableIn(PORT_AGENT, agent))
			{
				m_bAbortFinished = true;
				return;
			}
		}

		super.OnAbort(owner, nodeCausingAbort);
	}
};
