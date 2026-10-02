//------------------------------------------------------------------------------------------------
//! MUI_Toggle drawn as a slanted switch with its state spelled out at the right of the row.
//! Same calls as MUI_Toggle.
//!
//! Consumer:
//!   IA_UplinkToggle t = IA_UplinkToggle.Create(runtime, "Auto QRF on Live AOs", "gmAutoQrf");
//!   t.SetChecked(true); t.GetOnChanged().Insert(OnChanged);
//!
//! Layout:
//!   Fill width, Exact 34. Two in a MUI_Row share the row equally.
//------------------------------------------------------------------------------------------------
class IA_UplinkToggle : MUI_Toggle
{
	protected static const float ROW_H = 34;
	protected static const float SWITCH_W = 38;
	protected static const float SWITCH_H = 16;
	protected static const float LEAN = 5;
	protected static const float KNOB_W = 13;
	protected static const float STATE_W = 34;
	protected static const int FONT_LABEL = 15;
	protected static const string TEXT_ON = "ON";
	protected static const string TEXT_OFF = "OFF";

	//------------------------------------------------------------------------------------------------
	void IA_UplinkToggle()
	{
		m_Style.m_fHeight = ROW_H;
		m_Style.m_fMinHeight = ROW_H;
		m_Style.m_fRadius = 0;
	}

	//------------------------------------------------------------------------------------------------
	static IA_UplinkToggle Create(notnull MUI_Runtime runtime, string text, string name)
	{
		ref IA_UplinkToggle toggle = new IA_UplinkToggle();
		runtime.Adopt(toggle);
		toggle.SetName(name);
		toggle.SetText(text);
		return toggle;
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyTheme(notnull MUI_ThemeData theme)
	{
		super.ApplyTheme(theme);
		m_Style.m_iFontSize = FONT_LABEL;
	}

	//------------------------------------------------------------------------------------------------
	override void OnClicked()
	{
		IA_UplinkStyle.Click();
		super.OnClicked();
	}

	//------------------------------------------------------------------------------------------------
	override bool HandleNavAxis(int dirX, int dirY)
	{
		if (dirX == 0)
		{
			IA_UplinkPair pair = IA_UplinkPair.Cast(GetParent());
			if (pair)
				return pair.StepFocus(this, dirY);
			return false;
		}
		return super.HandleNavAxis(dirX, dirY);
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
		float op = GetDrawOpacity();
		if (op < 0.01)
			return;

		IA_UplinkStyle look = IA_UplinkStyle.Get();
		float x = DrawX();
		float y = DrawY();
		float w = m_World.m_fW;
		float h = m_World.m_fH;
		float hover = GetHoverT();
		float t = m_fCheckT;
		if (!IsEnabled())
		{
			op = op * 0.38;
			hover = 0;
		}

		if (hover > 0.02)
			surface.FillRect(x, y + 2, w, h - 4, MUI_ColorUtil.Fade(look.m_Tone, op * hover * 0.05), 0);
		surface.FillRect(x, y + h - 1, w, 1, MUI_ColorUtil.Fade(look.m_Tone, op * 0.07), 0);

		// The switch.
		float sx = x + 6;
		float sy = y + (h - SWITCH_H) * 0.5;
		look.FillSlant(surface, sx, sy, SWITCH_W, SWITCH_H, LEAN, MUI_ColorUtil.Fade(look.m_Glass, op));
		if (t > 0.02)
			look.FillSlant(surface, sx, sy, SWITCH_W, SWITCH_H, LEAN, MUI_ColorUtil.Fade(look.m_Tone, op * t * 0.24));
		MUI_ColorUtil.Mix(look.m_Muted, look.m_Tone, t, m_Mix);
		look.StrokeSlant(surface, sx, sy, SWITCH_W, SWITCH_H, LEAN, MUI_ColorUtil.Fade(m_Mix, op * (0.62 + t * 0.28 + hover * 0.10)), 1.4);

		float knobH = SWITCH_H - 6;
		float knobLean = LEAN * knobH / SWITCH_H;
		float travel = SWITCH_W - KNOB_W - 6;
		float kx = sx + 3 + LEAN * 3 / SWITCH_H + t * travel;
		look.FillSlant(surface, kx, sy + 3, KNOB_W, knobH, knobLean, MUI_ColorUtil.Fade(m_Mix, op * (0.75 + t * 0.25)));

		// What it switches, and where it stands.
		float labelX = sx + SWITCH_W + LEAN + 12;
		float labelW = x + w - STATE_W - 8 - labelX;
		float lit = t;
		if (hover > lit)
			lit = hover;
		surface.DrawText(labelX, y, labelW, h, m_sText, m_Style.m_iFontSize, MUI_ColorUtil.Fade(look.m_White, op * (0.70 + lit * 0.30)), false, false, true, false, true);

		if (m_bChecked)
			IA_TrackedText.DrawRight(surface, m_Runtime, x + w - 8, y, h, TEXT_ON, IA_UplinkStyle.FONT_CAP, IA_UplinkStyle.TRACK_CAP, MUI_ColorUtil.Fade(look.m_Tone, op));
		else
			IA_TrackedText.DrawRight(surface, m_Runtime, x + w - 8, y, h, TEXT_OFF, IA_UplinkStyle.FONT_CAP, IA_UplinkStyle.TRACK_CAP, MUI_ColorUtil.Fade(look.m_Muted, op * 0.85));

		look.FocusMarks(surface, x, y + 2, w, h - 4, GetFocusT(), op);
	}
}
