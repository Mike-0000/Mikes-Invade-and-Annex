// SuppressBehavior.bt Parallel keeps running after the suppress action
// Completes/Fails (cluster dropped, members inside the box, order swap).
// The volume lives as a ref on the action, so the blackboard pointer goes
// null. Vanilla then NodeError / Debug.Error. Sibling
// SCR_AIUpdateTargetSuppressionData already FAILs on a missing volume.

modded class SCR_AIGetSuppressionVolumeCenterPosition
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		SCR_AISuppressionVolumeBase volume;
		GetVariableIn(SUPPRESSION_VOLUME, volume);
		if (!volume)
			return ENodeResult.FAIL;

		return super.EOnTaskSimulate(owner, dt);
	}
};
