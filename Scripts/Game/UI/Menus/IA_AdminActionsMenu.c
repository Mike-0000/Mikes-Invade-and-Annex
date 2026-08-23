//------------------------------------------------------------------------------------------------
//! Fire-and-forget admin commands. V1 is QRF type buttons using the mission path.
//------------------------------------------------------------------------------------------------
class IA_AdminActionsMenu : MUI_MenuBase
{
	protected ref MUI_Button m_InfantryBtn;
	protected ref MUI_Button m_MotorizedBtn;
	protected ref MUI_Button m_MechanizedBtn;
	protected ref MUI_Button m_ArmouredBtn;
	protected ref MUI_Button m_AirborneBtn;

	//------------------------------------------------------------------------------------------------
	override void OnMUIMountFailed()
	{
		Print("[IA_AdminActionsMenu] MUI mount failed — blank layout required.", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	override string GetMUILogTag()
	{
		return "IA_AdminActionsMenu";
	}

	//------------------------------------------------------------------------------------------------
	override void BuildUI(notnull MUI_Runtime runtime)
	{
		ref IA_MuiShell shell = IA_MuiShell.Create(
			runtime,
			"ADMIN ACTIONS",
			"COMMAND UPLINK",
			"Spawn QRF through the normal mission path",
			560
		);

		m_InfantryBtn = runtime.CreateButton("Infantry QRF", "qrfInf");
		m_InfantryBtn.GetOnClicked().Insert(OnInfantry);

		m_MotorizedBtn = runtime.CreateButton("Motorized QRF", "qrfMotor");
		m_MotorizedBtn.GetOnClicked().Insert(OnMotorized);

		m_MechanizedBtn = runtime.CreateButton("Mechanized QRF", "qrfMech");
		m_MechanizedBtn.GetOnClicked().Insert(OnMechanized);

		m_ArmouredBtn = runtime.CreateButton("Armoured QRF", "qrfArmour");
		m_ArmouredBtn.GetOnClicked().Insert(OnArmoured);

		m_AirborneBtn = runtime.CreateButton("Airborne QRF", "qrfAir");
		m_AirborneBtn.MakeAccent();
		m_AirborneBtn.GetOnClicked().Insert(OnAirborne);

		shell.GetCard().AddChild(m_InfantryBtn);
		shell.GetCard().AddChild(m_MotorizedBtn);
		shell.GetCard().AddChild(m_MechanizedBtn);
		shell.GetCard().AddChild(m_ArmouredBtn);
		shell.GetCard().AddChild(m_AirborneBtn);

		ref MUI_Panel footerBtns = runtime.CreatePanel("footerBtns");
		footerBtns.GetStyle().m_Fill = Color.FromInt(0);
		footerBtns.GetStyle().m_fRadius = 0;
		footerBtns.GetStyle().m_fGap = 12;
		footerBtns.GetStyle().m_bBlockHit = false;
		footerBtns.SetFillWidth();

		ref MUI_Row closeRow = runtime.CreateRow("closeRow");
		closeRow.SetGap(12);

		ref MUI_Button closeBtn = runtime.CreateButton("Close", "close");
		closeBtn.GetOnClicked().Insert(OnMUIBack);
		closeRow.AddChild(closeBtn);
		footerBtns.AddChild(closeRow);

		shell.AddFooter(runtime, "Each button starts a QRF the same way a mission roll would", footerBtns);
		shell.Mount(runtime);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnInfantry()
	{
		RequestQRF(IA_QRFType.Infantry);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMotorized()
	{
		RequestQRF(IA_QRFType.Motorized);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMechanized()
	{
		RequestQRF(IA_QRFType.Mechanized);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnArmoured()
	{
		RequestQRF(IA_QRFType.Armoured);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAirborne()
	{
		RequestQRF(IA_QRFType.Airborne);
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestQRF(IA_QRFType type)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.IA_AskForceQRF(type);
		GetGame().GetMenuManager().CloseMenu(this);
	}
}
