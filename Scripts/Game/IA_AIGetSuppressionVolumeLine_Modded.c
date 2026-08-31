// Fire_Suppressive.bt is a Parallel sibling of the center-position node
// in SuppressBehavior.bt. Same missing-volume race: the action is gone,
// the blackboard object is null, vanilla NodeErrors. Fail the line
// instead of Debug.Error.

modded class SCR_AIGetSuppressionVolumeLine
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		SCR_AISuppressionVolumeBase volume;
		GetVariableIn(SUPPRESSION_VOLUME_PORT, volume);
		if (!volume)
			return ENodeResult.FAIL;

		return super.EOnTaskSimulate(owner, dt);
	}
};
