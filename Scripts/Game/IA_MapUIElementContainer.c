modded class SCR_MapUIElementContainer
{
	override protected void OnTaskAdded(SCR_Task task)
	{
		if (SCR_FactionManager.SGetLocalPlayerFaction())
		{
			super.OnTaskAdded(task);
			return;
		}

		// A Game Master can open the map before joining a faction.
		// Match InitTaskMarkers: the unrestricted editor can see every task.
		if (!m_bIsEditor || !m_bShowTasks || !task || !m_TaskSystem)
			return;

		foreach (Widget widget, SCR_MapUIElement element : m_mIcons)
		{
			SCR_TaskMapUIComponent taskIcon = SCR_TaskMapUIComponent.Cast(element);
			if (taskIcon && taskIcon.GetTask() == task)
				return;
		}

		InitTaskIcon(task);
		UpdateIcons();
	}
}
