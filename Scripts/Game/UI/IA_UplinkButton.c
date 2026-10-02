//------------------------------------------------------------------------------------------------
//! MUI_Button drawn the way the paint bay draws its tiles: a square plate with two cut corners,
//! an amber edge that lights on hover and corner marks for the gamepad. Same calls as MUI_Button.
//!
//! Consumer:
//!   IA_UplinkButton b = IA_UplinkButton.Create(runtime, "Save", "save");
//!   b.MakeAccent();                      // or MakeDanger()
//!   b.GetOnClicked().Insert(OnSave);
//!
//! Layout:
//!   As MUI_Button (Hug width, Grow 1), 38 high. SetCompact() is 26 high for tool rows.
//------------------------------------------------------------------------------------------------
class IA_UplinkButton : MUI_Button
{
	protected static const float CUT = 6;
	protected static const float PLATE_H = 38;
	protected static const float COMPACT_H = 26;
	protected static const int FONT_LABEL = 14;
	protected static const int FONT_COMPACT = 12;

	protected ref Color m_DangerPlate;
	protected ref Color m_DangerInk;
	protected bool m_bHoverHeard;

	//------------------------------------------------------------------------------------------------
	void IA_UplinkButton()
	{
		m_DangerPlate = Color.FromSRGBA(46, 19, 16, 238);
		m_DangerInk = Color.FromSRGBA(255, 178, 162, 255);
		m_Style.m_fHeight = PLATE_H;
		m_Style.m_fMinHeight = PLATE_H;
		m_Style.m_fRadius = 0;
	}

	//------------------------------------------------------------------------------------------------
	static IA_UplinkButton Create(notnull MUI_Runtime runtime, string text, string name)
	{
		ref IA_UplinkButton button = new IA_UplinkButton();
		runtime.Adopt(button);
		button.SetName(name);
		button.SetText(text);
		return button;
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyTheme(notnull MUI_ThemeData theme)
	{
		super.ApplyTheme(theme);
		if (m_bCompact)
			m_Style.m_iFontSize = FONT_COMPACT;
		else
			m_Style.m_iFontSize = FONT_LABEL;
	}

	//------------------------------------------------------------------------------------------------
	override void SetText(string text)
	{
		string shown = text;
		shown.ToUpper();
		super.SetText(shown);
		InvalidateLayout();
	}

	//------------------------------------------------------------------------------------------------
	override void SetCompact()
	{
		super.SetCompact();
		m_Style.m_fHeight = COMPACT_H;
		m_Style.m_fMinHeight = COMPACT_H;
		m_Style.m_fRadius = 0;
		InvalidateLayout();
	}

	//------------------------------------------------------------------------------------------------
	override void OnClicked()
	{
		IA_UplinkStyle.Click();
		super.OnClicked();
	}

	//------------------------------------------------------------------------------------------------
	override void OnTick(float dt)
	{
		bool hover = IsHover();
		if (hover && !m_bHoverHeard && IsEnabled())
			IA_UplinkStyle.Hover();
		m_bHoverHeard = hover;
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
		float op = GetDrawOpacity();
		if (op < 0.01)
			return;

		IA_UplinkStyle look = IA_UplinkStyle.Get();
		float hover = GetHoverT();
		float press = GetPressT();
		float focus = GetFocusT();
		if (!IsEnabled())
		{
			op = op * 0.38;
			hover = 0;
			press = 0;
		}

		float liftAmt = 2.0;
		float cut = CUT;
		if (m_bCompact)
		{
			liftAmt = 1.0;
			cut = 4;
		}
		float x = DrawX();
		float y = DrawY() - hover * liftAmt + press * 2.0;
		float w = m_World.m_fW;
		float h = m_World.m_fH;

		Color tone = look.m_Tone;
		Color plate = look.m_Plate;
		Color ink = look.m_White;
		float wash = 0.012 + hover * 0.07;
		float edge = 0.24 + hover * 0.66;
		if (m_bDanger)
		{
			tone = look.m_Red;
			plate = m_DangerPlate;
			ink = m_DangerInk;
			wash = 0.03 + hover * 0.12;
			edge = 0.50 + hover * 0.45;
		}
		else if (m_bAccent)
		{
			plate = look.m_Tab;
			ink = look.m_Gold;
			wash = 0.03 + hover * 0.10;
			edge = 0.62 + hover * 0.36;
		}

		if (hover > 0.02)
			surface.FillRect(x - 3, y - 3, w + 6, h + 6, MUI_ColorUtil.Fade(tone, op * hover * 0.05), 0);

		look.FillChamfer(surface, x, y, w, h, cut, MUI_ColorUtil.Fade(plate, op));
		look.FillChamfer(surface, x, y, w, h, cut, MUI_ColorUtil.Fade(tone, op * wash));

		// The press lands as a flash that drains out of the plate.
		float ripple = GetRipple();
		if (ripple > 0 && ripple < 1)
			look.FillChamfer(surface, x, y, w, h, cut, MUI_ColorUtil.Fade(tone, op * (1.0 - ripple) * 0.30));

		// Half a pixel in: an edge drawn on the plate's own boundary is cut off by a parent's clip.
		look.StrokeChamfer(surface, x + 0.5, y + 0.5, w - 1, h - 1, cut, MUI_ColorUtil.Fade(tone, op * edge), 1.0 + hover * 0.6);

		if (m_bAccent || m_bDanger)
			surface.FillRect(x + 1, y + cut + 1, 2, h - cut - 2, MUI_ColorUtil.Fade(tone, op * (0.75 + hover * 0.25)), 0);

		// The underline the hover draws out from the middle.
		if (hover > 0.02 && !m_bCompact)
		{
			float lineW = (w - cut * 2 - 8) * hover;
			surface.FillRect(x + (w - lineW) * 0.5, y + h - 4, lineW, 1, MUI_ColorUtil.Fade(tone, op * hover * 0.7), 0);
		}

		surface.DrawText(x, y, w, h, m_sText, m_Style.m_iFontSize, MUI_ColorUtil.Fade(ink, op * (0.86 + hover * 0.14)), true, true, true, false, true);
		look.FocusMarks(surface, x, y, w, h, focus, op);
	}
}
