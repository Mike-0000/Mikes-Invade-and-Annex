class IA_PauseMenuStatsButtonComponent: SCR_ButtonTextComponent
{
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		
		if (SCR_Global.IsEditMode())
			return;
			
		m_OnClicked.Insert(OpenStatisticsMenu);
	}
	
	void OpenStatisticsMenu() {
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.IA_StatisticsMenu);
	}
}; 