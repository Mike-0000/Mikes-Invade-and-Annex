// Vanilla artillery support uses the waypoint origin as the impact point.
// I&A parks the waypoint on the guns so the group does not march to the AO,
// then overrides the fire target back to the real strike position.

modded class SCR_AIWaypointArtillerySupport
{
	protected vector m_iaFireTargetPos;
	protected bool m_iaHasFireTargetOverride;

	void SetFireTargetOverride(vector pos)
	{
		m_iaFireTargetPos = pos;
		m_iaHasFireTargetOverride = true;
	}

	vector GetFireTargetPos()
	{
		if (m_iaHasFireTargetOverride)
			return m_iaFireTargetPos;

		return GetOrigin();
	}
}

modded class SCR_AIWaypointArtillerySupportState
{
	protected override void AddArtilleryActivity()
	{
		int targetShotCount = m_ArtilleryWaypoint.GetTargetShotCount();
		SCR_EAIArtilleryAmmoType ammoType = m_ArtilleryWaypoint.GetAmmoType();
		vector targetPos = m_ArtilleryWaypoint.GetFireTargetPos();
		SCR_AIStaticArtilleryActivity activity = new SCR_AIStaticArtilleryActivity(m_Utility, m_Waypoint, targetPos, ammoType, targetShotCount, priorityLevel: m_ArtilleryWaypoint.GetPriorityLevel());
		m_Utility.AddAction(activity);
		m_ArtilleryActivity = activity;
	}
}
