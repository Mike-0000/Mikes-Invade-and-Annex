//------------------------------------------------------------------------------------------------
//! Shared uplink chrome for I&A menus (overlay + FX + Card + LiveHeader).
//! Keeps Admin / Leaderboard visually consistent without editing Mikes-UI.
//------------------------------------------------------------------------------------------------
class IA_MuiShell
{
	protected ref MUI_Panel m_Overlay;
	protected ref MUI_FxBackdrop m_Fx;
	protected ref MUI_Card m_Card;
	protected ref MUI_LiveHeader m_Header;
	protected ref MUI_Label m_Subtitle;
	protected ref MUI_Hairline m_LineTop;

	//------------------------------------------------------------------------------------------------
	static IA_MuiShell Create(notnull MUI_Runtime runtime, string title, string kicker, string subtitle, float cardWidth)
	{
		ref IA_MuiShell shell = new IA_MuiShell();
		shell.Build(runtime, title, kicker, subtitle, cardWidth);
		return shell;
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull MUI_Runtime runtime, string title, string kicker, string subtitle, float cardWidth)
	{
		m_Overlay = runtime.CreatePanel("overlay");
		m_Overlay.MakeOverlay();
		m_Overlay.GetStyle().m_Fill = Color.FromInt(0);
		m_Overlay.SetIntro(0, 0.35, 0);

		m_Fx = runtime.CreateFxBackdrop("fx");
		m_Fx.SetIntro(0, 0.55, 0);

		m_Card = runtime.CreateCard("card");
		m_Card.SetWidth(cardWidth);
		m_Card.SetPadding(28);
		m_Card.GetStyle().m_fPadT = 22;
		m_Card.SetGap(12);
		m_Card.SetAlign(0.5, 0.5);
		m_Card.SetIntro(0.06, 0.55, 46);

		m_Header = runtime.CreateLiveHeader(title, "header");
		m_Header.SetKicker(kicker);
		m_Header.SetIntro(0.16, 0.4, 18);

		m_Subtitle = runtime.CreateLabel(subtitle, "subtitle");
		m_Subtitle.SetFontSize(runtime.GetTheme().FONT_SMALL);
		m_Subtitle.SetMuted(true);
		m_Subtitle.SetIntro(0.22, 0.4, 14);

		m_LineTop = runtime.CreateHairline("lineTop");
		m_LineTop.SetIntro(0.26, 0.35, 8);

		m_Card.AddChild(m_Header);
		m_Card.AddChild(m_Subtitle);
		m_Card.AddChild(m_LineTop);
	}

	//------------------------------------------------------------------------------------------------
	MUI_Card GetCard()
	{
		return m_Card;
	}

	//------------------------------------------------------------------------------------------------
	MUI_Panel GetOverlay()
	{
		return m_Overlay;
	}

	//------------------------------------------------------------------------------------------------
	void AddFooter(notnull MUI_Runtime runtime, string footText, notnull MUI_Row buttons)
	{
		ref MUI_Hairline lineBottom = runtime.CreateHairline("lineBottom");
		lineBottom.SetIntro(0.50, 0.35, 8);

		ref MUI_Label foot = runtime.CreateLabel(footText, "foot");
		foot.SetFontSize(runtime.GetTheme().FONT_SMALL);
		foot.SetMuted(true);
		foot.SetIntro(0.58, 0.4, 10);

		m_Card.AddChild(lineBottom);
		m_Card.AddChild(buttons);
		m_Card.AddChild(foot);
	}

	//------------------------------------------------------------------------------------------------
	void Mount(notnull MUI_Runtime runtime)
	{
		m_Overlay.AddChild(m_Fx);
		m_Overlay.AddChild(m_Card);
		runtime.SetRoot(m_Overlay);
	}
}
