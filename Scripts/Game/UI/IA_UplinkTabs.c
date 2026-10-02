//------------------------------------------------------------------------------------------------
//! Section rail: slanted segments, the open one lit amber. Same calls as MUI_Tabs.
//!
//! Consumer:
//!   IA_UplinkTabs tabs = IA_UplinkTabs.Create(runtime, "tabs");
//!   tabs.AddTab("Session"); tabs.AddTab("Global");
//!   tabs.GetOnChanged().Insert(OnTab);
//!
//! Layout:
//!   Fill width, Exact height. One painted leaf, so the rail is one gamepad stop: left/right
//!   and MenuTabLeft/Right (LB/RB) change the section, up/down leave it.
//------------------------------------------------------------------------------------------------
class IA_UplinkTabs : MUI_Node
{
	protected static const float RAIL_H = 34;
	protected static const float GAP = 4;
	protected static const float LEAN = 8;
	protected static const float INDEX_MIN_W = 150;
	protected static const int FONT_LABEL = 14;

	protected ref array<string> m_aLabels = {};
	protected ref array<string> m_aNumbers = {};
	protected ref array<float> m_aHot = {};
	protected ref ScriptInvoker m_OnChanged;
	protected int m_iIndex = -1;
	protected int m_iPointed = -1;
	protected float m_fSegMax = 216;
	protected float m_fFlash;
	protected bool m_bTabActionsBound;
	protected bool m_bHasClick;
	protected float m_fClickX;
	protected float m_fClickY;
	protected float m_fClickTime;

	//------------------------------------------------------------------------------------------------
	void IA_UplinkTabs()
	{
		m_OnChanged = new ScriptInvoker();
		m_Style.m_WidthMode = MUI_SizeMode.Fill;
		m_Style.m_HeightMode = MUI_SizeMode.Exact;
		m_Style.m_fHeight = RAIL_H;
		m_Style.m_fMinHeight = RAIL_H;
		m_Style.m_fRadius = 0;
		m_Style.m_Fill = Color.FromInt(0);
		m_Style.m_bInteractive = true;
	}

	//------------------------------------------------------------------------------------------------
	void ~IA_UplinkTabs()
	{
		UnbindTabActions();
	}

	//------------------------------------------------------------------------------------------------
	static IA_UplinkTabs Create(notnull MUI_Runtime runtime, string name)
	{
		ref IA_UplinkTabs tabs = new IA_UplinkTabs();
		runtime.Adopt(tabs);
		tabs.SetName(name);
		return tabs;
	}

	//------------------------------------------------------------------------------------------------
	override void SetRuntime(MUI_Runtime runtime)
	{
		UnbindTabActions();
		super.SetRuntime(runtime);
		if (m_Runtime)
			BindTabActions();
	}

	//------------------------------------------------------------------------------------------------
	override void DestroyHostWidgets()
	{
		UnbindTabActions();
		super.DestroyHostWidgets();
	}

	//------------------------------------------------------------------------------------------------
	//! Widest a segment may grow; few sections then sit at the left instead of stretching.
	void SetSegmentMax(float width)
	{
		m_fSegMax = width;
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	int AddTab(string label)
	{
		string shown = label;
		shown.ToUpper();
		int index = m_aLabels.Count();
		m_aLabels.Insert(shown);
		m_aHot.Insert(0);

		int shownIndex = index + 1;
		string number = shownIndex.ToString();
		if (index < 9)
			number = "0" + number;
		m_aNumbers.Insert(number);

		if (m_iIndex < 0)
			SetIndex(0);
		InvalidatePaint();
		return index;
	}

	//------------------------------------------------------------------------------------------------
	void SetIndex(int index)
	{
		if (index < 0)
			return;
		if (index >= m_aLabels.Count())
			return;
		if (m_iIndex == index)
			return;
		m_iIndex = index;
		m_fFlash = 1;
		InvalidatePaint();
		FocusSelfOnGamepad();
		if (m_OnChanged)
			m_OnChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	int GetIndex()
	{
		return m_iIndex;
	}

	//------------------------------------------------------------------------------------------------
	string GetTabLabel(int index)
	{
		if (index < 0)
			return "";
		if (index >= m_aLabels.Count())
			return "";
		return m_aLabels[index];
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnChanged()
	{
		return m_OnChanged;
	}

	//------------------------------------------------------------------------------------------------
	//! A change the player asked for: the same as SetIndex, with the click heard.
	protected void Choose(int index)
	{
		if (index < 0 || index >= m_aLabels.Count())
			return;
		if (index == m_iIndex)
			return;
		IA_UplinkStyle.Click();
		SetIndex(index);
	}

	//------------------------------------------------------------------------------------------------
	protected float SegmentWidth()
	{
		int count = m_aLabels.Count();
		if (count < 1)
			return 0;
		float segW = (m_World.m_fW - LEAN - GAP * (count - 1)) / count;
		if (segW > m_fSegMax)
			segW = m_fSegMax;
		if (segW < 8)
			segW = 8;
		return segW;
	}

	//------------------------------------------------------------------------------------------------
	//! The segment under a host-local point, or -1.
	protected int SegmentAt(float px, float py)
	{
		float y = DrawY();
		if (py < y || py > y + m_World.m_fH)
			return -1;
		float segW = SegmentWidth();
		if (segW <= 0)
			return -1;
		float rel = px - DrawX();
		if (rel < 0)
			return -1;
		int index = Math.Floor(rel / (segW + GAP));
		if (index >= m_aLabels.Count())
			return -1;
		if (rel - index * (segW + GAP) > segW + LEAN)
			return -1;
		return index;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDragEnd(float x, float y)
	{
		m_bHasClick = true;
		m_fClickX = x;
		m_fClickY = y;
		m_fClickTime = GetTime();
	}

	//------------------------------------------------------------------------------------------------
	override void OnClicked()
	{
		// Only a mouse release names a segment; a gamepad press on the rail has nothing to pick.
		bool fresh = m_bHasClick && m_fClickTime == GetTime();
		m_bHasClick = false;
		if (!fresh)
			return;
		Choose(SegmentAt(m_fClickX, m_fClickY));
	}

	//------------------------------------------------------------------------------------------------
	override bool HandleNavAxis(int dirX, int dirY)
	{
		if (dirX == 0)
			return false;
		int next = m_iIndex + dirX;
		if (next < 0)
			next = 0;
		if (next >= m_aLabels.Count())
			next = m_aLabels.Count() - 1;
		Choose(next);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnTick(float dt)
	{
		if (m_fFlash > 0)
		{
			m_fFlash = m_fFlash - dt * 3.0;
			if (m_fFlash < 0)
				m_fFlash = 0;
		}

		int pointed = -1;
		float px;
		float py;
		if (IsHover() && m_Runtime && UsingMouse() && m_Runtime.GetLocalPointer(px, py))
			pointed = SegmentAt(px, py);
		if (pointed != m_iPointed)
		{
			m_iPointed = pointed;
			if (pointed >= 0 && pointed != m_iIndex)
				IA_UplinkStyle.Hover();
		}

		int count = m_aHot.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			float target = 0;
			if (i == m_iPointed)
				target = 1;
			m_aHot[i] = MUI_Ease.Approach(m_aHot[i], target, dt, 14);
		}
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
		float segW = SegmentWidth();
		int count = m_aLabels.Count();

		// The line the segments stand on.
		surface.FillRect(x, y + h, w, 1, MUI_ColorUtil.Fade(look.m_Tone, op * 0.22), 0);

		int i;
		for (i = 0; i < count; i++)
		{
			float sx = x + i * (segW + GAP);
			float hot = m_aHot[i];
			float labelX = sx + LEAN * 0.5;
			float labelW = segW;
			bool numbered = segW >= INDEX_MIN_W;

			if (i == m_iIndex)
			{
				look.FillSlant(surface, sx, y, segW, h, LEAN, MUI_ColorUtil.Fade(look.m_Tab, op));
				if (m_fFlash > 0.02)
					look.FillSlant(surface, sx, y, segW, h, LEAN, MUI_ColorUtil.Fade(look.m_Gold, op * m_fFlash * 0.22));
				look.StrokeSlant(surface, sx, y, segW, h, LEAN, MUI_ColorUtil.Fade(look.m_Tone, op * 0.6), 1);
				surface.FillRect(sx + 1, y + h - 2, segW - 1, 3, MUI_ColorUtil.Fade(look.m_Tone, op), 0);
				if (numbered)
					IA_TrackedText.Draw(surface, m_Runtime, sx + LEAN + 8, y, h, m_aNumbers[i], IA_UplinkStyle.FONT_CAP, IA_UplinkStyle.TRACK_CAP, MUI_ColorUtil.Fade(look.m_Tone, op * 0.75));
				surface.DrawText(labelX, y, labelW, h, m_aLabels[i], FONT_LABEL, MUI_ColorUtil.Fade(look.m_Gold, op), true, true, true, false, true);
				continue;
			}

			look.FillSlant(surface, sx, y, segW, h, LEAN, MUI_ColorUtil.Fade(look.m_Plate, op * (0.55 + hot * 0.35)));
			if (hot > 0.02)
				look.FillSlant(surface, sx, y, segW, h, LEAN, MUI_ColorUtil.Fade(look.m_Tone, op * hot * 0.06));
			look.StrokeSlant(surface, sx, y, segW, h, LEAN, MUI_ColorUtil.Fade(look.m_Tone, op * (0.14 + hot * 0.45)), 1);
			if (numbered)
				IA_TrackedText.Draw(surface, m_Runtime, sx + LEAN + 8, y, h, m_aNumbers[i], IA_UplinkStyle.FONT_CAP, IA_UplinkStyle.TRACK_CAP, MUI_ColorUtil.Fade(look.m_Muted, op * 0.55));
			Color ink = look.m_Muted;
			if (hot > 0.5)
				ink = look.m_White;
			surface.DrawText(labelX, y, labelW, h, m_aLabels[i], FONT_LABEL, MUI_ColorUtil.Fade(ink, op), true, true, true, false, true);
		}

		if (m_iIndex >= 0 && m_iIndex < count)
			look.FocusMarks(surface, x + m_iIndex * (segW + GAP), y, segW + LEAN, h, GetFocusT(), op);
	}

	//------------------------------------------------------------------------------------------------
	protected bool UsingMouse()
	{
		InputManager im = GetGame().GetInputManager();
		if (!im)
			return true;
		return im.IsUsingMouseAndKeyboard();
	}

	//------------------------------------------------------------------------------------------------
	protected void FocusSelfOnGamepad()
	{
		if (!m_Runtime)
			return;
		if (UsingMouse())
			return;
		m_Runtime.FocusNode(this);
	}

	//------------------------------------------------------------------------------------------------
	protected void BindTabActions()
	{
		if (m_bTabActionsBound)
			return;
		InputManager im = GetGame().GetInputManager();
		if (!im)
			return;
		im.AddActionListener("MenuTabLeft", EActionTrigger.DOWN, OnMenuTabLeft);
		im.AddActionListener("MenuTabRight", EActionTrigger.DOWN, OnMenuTabRight);
		m_bTabActionsBound = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UnbindTabActions()
	{
		if (!m_bTabActionsBound)
			return;
		InputManager im = GetGame().GetInputManager();
		if (im)
		{
			im.RemoveActionListener("MenuTabLeft", EActionTrigger.DOWN, OnMenuTabLeft);
			im.RemoveActionListener("MenuTabRight", EActionTrigger.DOWN, OnMenuTabRight);
		}
		m_bTabActionsBound = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMenuTabLeft()
	{
		HandleNavAxis(-1, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMenuTabRight()
	{
		HandleNavAxis(1, 0);
	}
}
