//------------------------------------------------------------------------------------------------
//! MUI_NumericField drawn as a square readout: a caption over a dark box with an amber bar, and
//! a minus and a plus at the right that step the value with the mouse. Same calls as
//! MUI_NumericField.
//!
//! Consumer:
//!   IA_UplinkField f = IA_UplinkField.Create(runtime, "AI scale", "ai");
//!   f.SetRange(0.1, 5); f.SetStep(0.1); f.SetDecimals(2); f.SetValue(1);
//!
//! Layout:
//!   Fill width, Exact 56. Two in a MUI_Row share the row equally.
//------------------------------------------------------------------------------------------------
class IA_UplinkField : MUI_NumericField
{
	protected static const float FIELD_H = 56;
	protected static const float CAP_H = 16;
	protected static const float BOX_Y = 18;
	protected static const float STEP_W = 28;
	protected static const float VALUE_X = 14;
	protected static const int FONT_CAPTION = 11;
	protected static const int FONT_VALUE = 16;

	protected bool m_bHasClick;
	protected float m_fClickX;
	protected float m_fClickTime;
	protected float m_fNudge;
	protected int m_iNudgeDir;

	//------------------------------------------------------------------------------------------------
	void IA_UplinkField()
	{
		m_Style.m_fHeight = FIELD_H;
		m_Style.m_fMinHeight = FIELD_H;
		m_Style.m_fRadius = 0;
	}

	//------------------------------------------------------------------------------------------------
	static IA_UplinkField Create(notnull MUI_Runtime runtime, string label, string name)
	{
		ref IA_UplinkField field = new IA_UplinkField();
		runtime.Adopt(field);
		field.SetName(name);
		field.SetLabel(label);
		return field;
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyTheme(notnull MUI_ThemeData theme)
	{
		super.ApplyTheme(theme);
		m_Style.m_iFontSize = FONT_VALUE;
	}

	//------------------------------------------------------------------------------------------------
	override void SetLabel(string label)
	{
		string shown = label;
		shown.ToUpper();
		super.SetLabel(shown);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDragEnd(float x, float y)
	{
		m_bHasClick = true;
		m_fClickX = x;
		m_fClickTime = GetTime();
	}

	//------------------------------------------------------------------------------------------------
	override void OnClicked()
	{
		// Only a mouse release on the minus or the plus steps; anywhere else is a click to type.
		bool fresh = m_bHasClick && m_fClickTime == GetTime();
		m_bHasClick = false;
		if (!fresh)
			return;

		float right = DrawX() + m_World.m_fW;
		if (m_fClickX < right - STEP_W * 2)
			return;
		if (m_fClickX < right - STEP_W)
			Nudge(-1);
		else
			Nudge(1);
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
		Nudge(dirX);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One step down or up; what was being typed is committed first.
	protected void Nudge(int dir)
	{
		if (m_Runtime)
			m_Runtime.StopEditing();

		float step = m_fStep;
		if (step < 0.0001)
			step = 1;
		float before = m_fValue;
		float next = Math.Round((m_fValue + dir * step) * 10000.0) / 10000.0;
		SetValue(next);

		m_fNudge = 1;
		m_iNudgeDir = dir;
		if (m_fValue == before)
			IA_UplinkStyle.ClickFail();
		else
			IA_UplinkStyle.Click();
	}

	//------------------------------------------------------------------------------------------------
	override void OnTick(float dt)
	{
		if (m_fNudge <= 0)
			return;
		m_fNudge = m_fNudge - dt * 4.0;
		if (m_fNudge < 0)
			m_fNudge = 0;
		InvalidatePaint();
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
		float focus = GetFocusT();
		float hover = GetHoverT();
		float boxY = y + BOX_Y;
		float boxH = m_World.m_fH - BOX_Y;

		Color capInk = look.m_Muted;
		if (focus > 0.5)
			capInk = look.m_Tone;
		surface.DrawText(x + 1, y, w - 2, CAP_H, m_sLabel, FONT_CAPTION, MUI_ColorUtil.Fade(capInk, op), true, false, true, false, true);

		surface.FillRect(x, boxY, w, boxH, MUI_ColorUtil.Fade(look.m_Glass, op), 0);
		if (hover > 0.02)
			surface.FillRect(x, boxY, w, boxH, MUI_ColorUtil.Fade(look.m_Tone, op * hover * 0.04), 0);
		surface.FillRect(x, boxY, 3, boxH, MUI_ColorUtil.Fade(look.m_Tone, op * (0.30 + focus * 0.70)), 0);
		surface.StrokeRect(x, boxY, w, boxH, MUI_ColorUtil.Fade(look.m_Tone, op * (0.16 + hover * 0.24 + focus * 0.50)), 1, 0);

		float stepX = x + w - STEP_W * 2;
		float valueW = stepX - 8 - (x + VALUE_X);
		surface.DrawText(x + VALUE_X, boxY, valueW, boxH, m_sValue, m_Style.m_iFontSize, MUI_ColorUtil.Fade(look.m_White, op), true, false, true, false, true);

		// The caret only while the edit box has this field.
		if (focus > 0.5 && m_Runtime && m_Runtime.IsEditing())
		{
			float blink = MUI_Ease.Pulse(GetTime(), 1.6);
			if (blink > 0.35)
			{
				float tw = 0;
				float th = 0;
				m_Runtime.MeasureText(m_sValue, m_Style.m_iFontSize, true, 0, tw, th);
				float cx = x + VALUE_X + tw + 2;
				if (cx > stepX - 6)
					cx = stepX - 6;
				surface.FillRect(cx, boxY + 9, 2, boxH - 18, MUI_ColorUtil.Fade(look.m_Tone, op * blink), 0);
			}
		}

		PaintStep(surface, look, stepX, boxY, boxH, -1, hover, op);
		PaintStep(surface, look, stepX + STEP_W, boxY, boxH, 1, hover, op);
		look.FocusMarks(surface, x, boxY, w, boxH, focus, op);
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintStep(MUI_RenderSurface surface, IA_UplinkStyle look, float x, float y, float h, int dir, float hover, float op)
	{
		float flash = 0;
		if (m_iNudgeDir == dir)
			flash = m_fNudge;

		surface.FillRect(x, y + 6, 1, h - 12, MUI_ColorUtil.Fade(look.m_Tone, op * 0.16), 0);
		if (flash > 0.02)
			surface.FillRect(x + 1, y + 1, STEP_W - 2, h - 2, MUI_ColorUtil.Fade(look.m_Tone, op * flash * 0.28), 0);

		// Whole pixels and two wide, so the upright of the plus survives a small screen.
		float cx = Math.Round(x + STEP_W * 0.5);
		float cy = Math.Round(y + h * 0.5);
		float lit = 0.40 + hover * 0.40 + flash * 0.20;
		surface.FillRect(cx - 5, cy - 1, 10, 2, MUI_ColorUtil.Fade(look.m_Tone, op * lit), 0);
		if (dir > 0)
			surface.FillRect(cx - 1, cy - 5, 2, 10, MUI_ColorUtil.Fade(look.m_Tone, op * lit), 0);
	}
}
