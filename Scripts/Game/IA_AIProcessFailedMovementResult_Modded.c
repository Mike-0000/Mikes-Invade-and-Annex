// MoveToLocation.bt runs this after Group Move fails. Vanilla then does
// m_Group.GetLeaderEntity().GetOrigin() with only m_Group checked. A
// living group with no leader (wipe, staggered spawn, leader slot empty)
// is `#return` NULL. Fail the action instead of taking that GetOrigin.
//
// UNKNOWN + a still-current waypoint is a designer debug trap: NodeErrorOnce
// then RUNNING forever so the group stays stuck (navmesh / missing component).
// I&A completes the waypoint and fails the action so DecideActivity can recover.

modded class SCR_AIProcessFailedMovementResult
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		if (m_bReturnRunning)
			return ENodeResult.RUNNING;

		if (m_Group && !m_Group.GetLeaderEntity())
		{
			if (m_GroupUtilityComponent)
				FailAction();
			return ENodeResult.FAIL;
		}

		int moveResult;
		if (GetVariableIn(PORT_MOVE_RESULT, moveResult))
		{
			bool isWaypointRelated;
			GetVariableIn(PORT_IS_WAYPOINT_RELATED, isWaypointRelated);
			if (moveResult == EMoveError.UNKNOWN && isWaypointRelated && ShouldRecoverUnknownWaypointMove())
				return RecoverUnknownWaypointMove();
		}

		return super.EOnTaskSimulate(owner, dt);
	}

	protected bool ShouldRecoverUnknownWaypointMove()
	{
		if (!m_GroupUtilityComponent)
			return true;

		SCR_AIActivityBase activity = SCR_AIActivityBase.Cast(m_GroupUtilityComponent.GetCurrentAction());
		if (activity && !activity.m_RelatedWaypoint)
			return false;

		return true;
	}

	protected ENodeResult RecoverUnknownWaypointMove()
	{
		vector moveLocation;
		GetVariableIn(PORT_MOVE_LOCATION, moveLocation);

		vector startLocation = vector.Zero;
		if (m_Group)
		{
			IEntity leader = m_Group.GetLeaderEntity();
			if (leader)
				startLocation = leader.GetOrigin();
		}

		if (m_GroupUtilityComponent)
			m_GroupUtilityComponent.OnMoveFailed(EMoveError.UNKNOWN, null, true, moveLocation);

		AIWorld aiWorld = GetGame().GetAIWorld();
		if (aiWorld)
		{
			if (startLocation != vector.Zero)
				aiWorld.RequestNavmeshLoad(startLocation);
			if (moveLocation != vector.Zero)
				aiWorld.RequestNavmeshLoad(moveLocation);
		}

		CompleteWaypoint();
		if (m_GroupUtilityComponent)
			FailAction();

		Print(string.Format("[IA] Failed move from %1 to waypoint %2, completing waypoint", startLocation, moveLocation), LogLevel.WARNING);
		return ENodeResult.FAIL;
	}
};
