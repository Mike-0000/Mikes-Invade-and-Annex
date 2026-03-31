//------------------------------------------------------------------------------------------------
class IA_AdminConfigMenu : ChimeraMenuBase
{
	protected Widget m_wRoot;
	protected EditBoxWidget m_civCountEdit;
	protected EditBoxWidget m_AIScaleEdit;
	protected CheckBoxWidget m_disableHQHeliCheck;
	protected CheckBoxWidget m_disableHQGroundCheck;
	protected EditBoxWidget m_artyCooldownEdit;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		m_wRoot = GetRootWidget();
		if (!m_wRoot) return;
		
		// Initialize Widgets with Debugging
		Widget w;
		
		w = m_wRoot.FindAnyWidget("IACivCountEdit");
		if (w) m_civCountEdit = EditBoxWidget.Cast(w);
		else Print("[IA_AdminConfigMenu] ERROR: IACivCountEdit not found!", LogLevel.ERROR);

		w = m_wRoot.FindAnyWidget("IAAIScaleEdit");
		if (w) m_AIScaleEdit = EditBoxWidget.Cast(w);
		else Print("[IA_AdminConfigMenu] ERROR: IAAIScaleEdit not found!", LogLevel.ERROR);

		w = m_wRoot.FindAnyWidget("IADisableHQHeliCheck");
		if (w) m_disableHQHeliCheck = CheckBoxWidget.Cast(w);
		else Print("[IA_AdminConfigMenu] ERROR: IADisableHQHeliCheck not found!", LogLevel.ERROR);

		w = m_wRoot.FindAnyWidget("IADisableHQGroundCheck");
		if (w) m_disableHQGroundCheck = CheckBoxWidget.Cast(w);
		else Print("[IA_AdminConfigMenu] ERROR: IADisableHQGroundCheck not found!", LogLevel.ERROR);

		w = m_wRoot.FindAnyWidget("IAArtyCooldownEdit");
		if (w) m_artyCooldownEdit = EditBoxWidget.Cast(w);
		else Print("[IA_AdminConfigMenu] ERROR: IAArtyCooldownEdit not found!", LogLevel.ERROR);
		
		Widget closeW = m_wRoot.FindAnyWidget("IACloseButton");
		if (closeW) {
			SCR_InputButtonComponent closeButton = SCR_InputButtonComponent.Cast(closeW.FindHandler(SCR_InputButtonComponent));
			if (closeButton) closeButton.m_OnActivated.Insert(OnIAMenuClose);
		} else Print("[IA_AdminConfigMenu] ERROR: IACloseButton not found!", LogLevel.ERROR);

		Widget saveW = m_wRoot.FindAnyWidget("IASaveButton");
		if (saveW) {
			SCR_InputButtonComponent saveButton = SCR_InputButtonComponent.Cast(saveW.FindHandler(SCR_InputButtonComponent));
			if (saveButton) saveButton.m_OnActivated.Insert(OnIASaveConfig);
		} else Print("[IA_AdminConfigMenu] ERROR: IASaveButton not found!", LogLevel.ERROR);

		Widget completeW = m_wRoot.FindAnyWidget("IACompleteMissionButton");
		if (completeW) {
			SCR_InputButtonComponent completeButton = SCR_InputButtonComponent.Cast(completeW.FindHandler(SCR_InputButtonComponent));
			if (completeButton) completeButton.m_OnActivated.Insert(OnIAForceCompleteMission);
		} else Print("[IA_AdminConfigMenu] ERROR: IACompleteMissionButton not found!", LogLevel.ERROR);

		// Populate with current values
		IA_MissionInitializer missionInit = IA_MissionInitializer.GetInstance();
		if (missionInit && missionInit.GetConfig())
		{
			if (m_civCountEdit) m_civCountEdit.SetText(missionInit.GetConfig().m_fCivilianCountMultiplier.ToString());
			if (m_AIScaleEdit) m_AIScaleEdit.SetText(missionInit.GetConfig().m_fAIScaleMultiplier.ToString());
			if (m_disableHQHeliCheck) m_disableHQHeliCheck.SetChecked(missionInit.GetConfig().m_bDisableHQHelipads);
			if (m_disableHQGroundCheck) m_disableHQGroundCheck.SetChecked(missionInit.GetConfig().m_bDisableHQGroundVehicles);
			if (m_artyCooldownEdit) m_artyCooldownEdit.SetText(missionInit.GetConfig().m_iArtilleryCooldown.ToString());
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnIAForceCompleteMission(SCR_InputButtonComponent comp)
	{
		Print("[IA_AdminConfigMenu] OnIAForceCompleteMission clicked", LogLevel.NORMAL);
		IA_MissionInitializer missionInit = IA_MissionInitializer.GetInstance();
		if (!missionInit) return;
		
		missionInit.ForceCompleteZone();
		GetGame().GetMenuManager().CloseMenu(this);
	}

	//------------------------------------------------------------------------------------------------
	void OnIASaveConfig(SCR_InputButtonComponent comp)
	{
		Print("[IA_AdminConfigMenu] OnIASaveConfig clicked", LogLevel.NORMAL);
		IA_MissionInitializer missionInit = IA_MissionInitializer.GetInstance();
		if (!missionInit) {
			Print("[IA_AdminConfigMenu] ERROR: IA_MissionInitializer instance not found!", LogLevel.ERROR);
			return;
		}
		
		float civCount = 1.0;
		if (m_civCountEdit) {
			string s = m_civCountEdit.GetText();
			if (!s.IsEmpty()) civCount = s.ToFloat();
		}

		float aiScale = 1.0;
		if (m_AIScaleEdit) {
			string s = m_AIScaleEdit.GetText();
			if (!s.IsEmpty()) aiScale = s.ToFloat();
		}

		bool disableHeli = false;
		if (m_disableHQHeliCheck) disableHeli = m_disableHQHeliCheck.IsChecked();

		bool disableGround = false;
		if (m_disableHQGroundCheck) disableGround = m_disableHQGroundCheck.IsChecked();

		int artyCooldown = 300;
		if (m_artyCooldownEdit) {
			string s = m_artyCooldownEdit.GetText();
			if (!s.IsEmpty()) artyCooldown = s.ToInt();
		}

		Print(string.Format("[IA_AdminConfigMenu] Saving Config: Civ=%1, AI=%2, Heli=%3, Ground=%4, Arty=%5", civCount, aiScale, disableHeli, disableGround, artyCooldown), LogLevel.NORMAL);

		// Send RPC to server to update config
		missionInit.UpdateConfig(civCount, aiScale, disableHeli, disableGround, artyCooldown);
		
		GetGame().GetMenuManager().CloseMenu(this);
	}

	//------------------------------------------------------------------------------------------------
	void OnIAMenuClose(SCR_InputButtonComponent comp)
	{
		GetGame().GetMenuManager().CloseMenu(this);
	}
};
