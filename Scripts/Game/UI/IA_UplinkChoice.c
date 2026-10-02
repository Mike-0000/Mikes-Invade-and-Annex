//------------------------------------------------------------------------------------------------
//! One choice out of a short list, stepped through with the arrows at either end: the menu's
//! stand-in for a dropdown, which would open over the controls under it. Same calls as
//! MUI_Dropdown for filling and reading it.
//!
//! Consumer:
//!   IA_UplinkChoice c = IA_UplinkChoice.Create(runtime, "Normal troop skill", "normalSkill");
//!   c.AddItem("Rookie"); c.AddItem("Regular");
//!   c.SetIndex(1); c.GetOnChanged().Insert(OnSkill);
//!
//! Layout:
//!   Fill width, Exact 56, the same as IA_UplinkField so the two pair in a row.
//!
//! Input:
//!   Mouse: the arrows step, the value steps forward and wraps. Gamepad: left/right step,
//!   Select steps forward and wraps.
//------------------------------------------------------------------------------------------------
class IA_UplinkChoice : MUI_Node
{
	protected static const float CHOICE_H = 56;
	protected static const float CAP_H = 16;
	protected static const float BOX_Y = 18;
	protected static const float ARROW_W = 34;
	protected static const int FONT_CAPTION = 11;
	protected static const int FONT_VALUE = 16;

	protected ref array<string> m_aItems = {};
	protected ref ScriptInvoker m_OnChanged;
	protected string m_sCaption;
	protected int m_iIndex = -1;
	protected float m_fFlash;
	protected int m_iFlashDir;
	protected bool m_bHasClick;
	protected float m_fClickX;
	protected float m_fClickTime;

	//------------------------------------------------------------------------------------------------
	void IA_UplinkChoice()
	{
		m_OnChanged = new ScriptInvoker();
		m_Style.m_WidthMode = MUI_SizeMode.Fill;
		m_Style.m_HeightMode = MUI_SizeMode.Exact;
		m_Style.m_fHeight = CHOICE_H;
		m_Style.m_fMinHeight = CHOICE_H;
		m_Style.m_fRadius = 0;
		m_Style.m_Fill = Color.FromInt(0);
		m_Style.m_bInteractive = true;
	}

	//------------------------------------------------------------------------------------------------
	static IA_UplinkChoice Create(notnull MUI_Runtime runtime, string caption, string name)
	{
		ref IA_UplinkChoice choice = new IA_UplinkChoice();
		runtime.Adopt(choice);
		choice.SetName(name);
		choice.SetCaption(caption);
		return choice;
	}

	//------------------------------------------------------------------------------------------------
	void SetCaption(string caption)
	{
		m_sCaption = caption;
		m_sCaption.ToUpper();
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	int AddItem(string label)
	{
		m_aItems.Insert(label);
		if (m_iIndex < 0)
			SetIndex(0);
		InvalidatePaint();
		return m_aItems.Count() - 1;
	}

	//------------------------------------------------------------------------------------------------
	void SetIndex(int index)
	{
		if (index < 0)
			return;
		if (index >= m_aItems.Count())
			return;
		if (m_iIndex == index)
			return;
		m_iIndex = index;
		InvalidatePaint();
		if (m_OnChanged)
			m_OnChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	int GetIndex()
	{
		return m_iIndex;
	}

	//------------------------------------------------------------------------------------------------
	string GetText()
	{
		if (m_iIndex < 0)
			return "";
		if (m_iIndex >= m_aItems.Count())
			return "";
		return m_aItems[m_iIndex];
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnChanged()
	{
		return m_OnChanged;
	}

	//------------------------------------------------------------------------------------------------
	//! \param wrap past the last item go back to the first; otherwise stop at the ends
	protected void Step(int dir, bool wrap)
	{
		int count = m_aItems.Count();
		if (count < 1)
			return;

		int next = m_iIndex + dir;
		if (wrap)
		{
			if (next >= count)
				next = 0;
			if (next < 0)
				next = count - 1;
		}
		else
		{
			if (next >= count)
				next = count - 1;
			if (next < 0)
				next = 0;
		}

		m_fFlash = 1;
		m_iFlashDir = dir;
		if (next == m_iIndex)
		{
			IA_UplinkStyle.ClickFail();
			InvalidatePaint();
			return;
		}
		IA_UplinkStyle.Click();
		SetIndex(next);
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
		bool fresh = m_bHasClick && m_fClickTime == GetTime();
		m_bHasClick = false;
		if (!fresh)
		{
			Step(1, true);
			return;
		}

		float left = DrawX();
		float right = left + m_World.m_fW;
		if (m_fClickX < left + ARROW_W)
			Step(-1, false);
		else if (m_fClickX > right - ARROW_W)
			Step(1, false);
		else
			Step(1, true);
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
		Step(dirX, false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnTick(float dt)
	{
		if (m_fFlash <= 0)
			return;
		m_fFlash = m_fFlash - dt * 4.0;
		if (m_fFlash < 0)
			m_fFlash = 0;
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
		int count = m_aItems.Count();

		Color capInk = look.m_Muted;
		if (focus > 0.5)
			capInk = look.m_Tone;
		surface.DrawText(x + 1, y, w - 2, CAP_H, m_sCaption, FONT_CAPTION, MUI_ColorUtil.Fade(capInk, op), true, false, true, false, true);

		surface.FillRect(x, boxY, w, boxH, MUI_ColorUtil.Fade(look.m_Glass, op), 0);
		if (hover > 0.02)
			surface.FillRect(x, boxY, w, boxH, MUI_ColorUtil.Fade(look.m_Tone, op * hover * 0.04), 0);
		surface.StrokeRect(x, boxY, w, boxH, MUI_ColorUtil.Fade(look.m_Tone, op * (0.16 + hover * 0.24 + focus * 0.50)), 1, 0);

		PaintArrow(surface, look, x, boxY, boxH, -1, m_iIndex > 0, hover, op);
		PaintArrow(surface, look, x + w - ARROW_W, boxY, boxH, 1, m_iIndex < count - 1, hover, op);

		surface.DrawText(x + ARROW_W, boxY, w - ARROW_W * 2, boxH - 5, GetText(), FONT_VALUE, MUI_ColorUtil.Fade(look.m_Gold, op), true, true, true, false, true);

		// Where in the list it stands.
		if (count > 1)
		{
			float pipW = 12;
			float pipGap = 3;
			float pipsW = count * pipW + (count - 1) * pipGap;
			float px = x + (w - pipsW) * 0.5;
			int i;
			for (i = 0; i < count; i++)
			{
				if (i == m_iIndex)
					surface.FillRect(px, boxY + boxH - 6, pipW, 2, MUI_ColorUtil.Fade(look.m_Tone, op), 0);
				else
					surface.FillRect(px, boxY + boxH - 6, pipW, 2, MUI_ColorUtil.Fade(look.m_Tone, op * 0.18), 0);
				px = px + pipW + pipGap;
			}
		}

		look.FocusMarks(surface, x, boxY, w, boxH, focus, op);
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintArrow(MUI_RenderSurface surface, IA_UplinkStyle look, float x, float y, float h, int dir, bool live, float hover, float op)
	{
		float flash = 0;
		if (m_iFlashDir == dir)
			flash = m_fFlash;

		float edgeX = x + ARROW_W;
		if (dir > 0)
			edgeX = x;
		surface.FillRect(edgeX, y + 6, 1, h - 12, MUI_ColorUtil.Fade(look.m_Tone, op * 0.16), 0);
		if (flash > 0.02)
			surface.FillRect(x + 1, y + 1, ARROW_W - 2, h - 2, MUI_ColorUtil.Fade(look.m_Tone, op * flash * 0.28), 0);

		float lit = 0.16;
		if (live)
			lit = 0.50 + hover * 0.40;
		float cx = x + ARROW_W * 0.5;
		float cy = y + h * 0.5;
		float reach = 4.0 * dir;
		surface.DrawLine(cx - reach * 0.5, cy - 6, cx + reach, cy, MUI_ColorUtil.Fade(look.m_Tone, op * lit), 1.6);
		surface.DrawLine(cx + reach, cy, cx - reach * 0.5, cy + 6, MUI_ColorUtil.Fade(look.m_Tone, op * lit), 1.6);
	}
}
