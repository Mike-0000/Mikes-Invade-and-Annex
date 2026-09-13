//------------------------------------------------------------------------------------------------
//! Complete + Defend action table for a live field-base job.
//! START_CHAIN is used when no in-flight job owns the AO (including after Failed recover).
//------------------------------------------------------------------------------------------------
class IA_AdminCompleteAndDefend
{
	static const int START_CHAIN = 0;
	static const int BYPASS_TO_DEFEND = 1;
	static const int ALREADY_DEFENDING = 2;
	static const int WAIT_PLACING = 3;
	static const int RECOVER_FAILED = 4;

	//------------------------------------------------------------------------------------------------
	static int Resolve(int phase)
	{
		if (phase == IA_BaseObjectivePhase.Defend)
			return ALREADY_DEFENDING;
		if (phase == IA_BaseObjectivePhase.Seize)
			return BYPASS_TO_DEFEND;
		if (phase == IA_BaseObjectivePhase.Regroup)
			return BYPASS_TO_DEFEND;
		if (phase == IA_BaseObjectivePhase.Warning)
			return BYPASS_TO_DEFEND;
		if (phase == IA_BaseObjectivePhase.Placing)
			return WAIT_PLACING;
		if (phase == IA_BaseObjectivePhase.Failed)
			return RECOVER_FAILED;
		return START_CHAIN;
	}
}
