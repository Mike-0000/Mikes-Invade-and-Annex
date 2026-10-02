//------------------------------------------------------------------------------------------------
//! The window both menus sit in, drawn as the paint bay is: a named tab on a dark bevelled
//! body over blur, a header band with the title, and a status chip at the right.
//!
//! Consumer:
//!   IA_UplinkFrame frame = IA_UplinkFrame.Create(runtime, "frame", 1180, "COMMAND UPLINK", "LEADERBOARD", "…");
//!   overlay.AddChild(frame);
//!   frame.AddChild(tabs);            // children stack under the header band
//!   frame.SetStatus("LIVE", look.m_Green, false);
//!
//! Layout:
//!   Exact width, Hug height, StackVertical, centred in an Overlay parent.
//------------------------------------------------------------------------------------------------
class IA_UplinkFrame : MUI_Surface
{
	static const float TAB_H = 18;
	static const float HEAD_H = 62;
	static const float PAD = 20;
	protected static const float TAB_PAD_X = 12;
	protected static const int FONT_TITLE = 26;
	protected static const int FONT_SUB = 13;
	protected static const float SCAN_PERIOD = 5.2;

	protected string m_sTab;
	protected string m_sTitle;
	protected string m_sSubtitle;
	protected string m_sStatus;
	protected string m_sNote;
	protected ref Color m_StatusTone;
	protected bool m_bBusy;
	protected bool m_bTextDirty = true;
	protected float m_fTabW;
	protected float m_fTitleW;
	protected float m_fStatusW;
	protected ref array<float> m_aPoly = {};

	//------------------------------------------------------------------------------------------------
	void IA_UplinkFrame()
	{
		m_Style.m_Layout = MUI_LayoutKind.StackVertical;
		m_Style.m_WidthMode = MUI_SizeMode.Exact;
		m_Style.m_HeightMode = MUI_SizeMode.Hug;
		m_Style.m_fRadius = 0;
		m_Style.m_fGap = 10;
		m_Style.m_Fill = Color.FromInt(0);
		m_Style.SetPaddingTRBL(TAB_H + HEAD_H + 12, PAD, PAD, PAD);
		m_bBlurEnabled = true;
		m_fBlurIntensity = 0.90;
		m_StatusTone = Color.FromSRGBA(255, 184, 72, 255);
	}

	//------------------------------------------------------------------------------------------------
	static IA_UplinkFrame Create(notnull MUI_Runtime runtime, string name, float width, string tab, string title, string subtitle)
	{
		ref IA_UplinkFrame frame = new IA_UplinkFrame();
		runtime.Adopt(frame);
		frame.SetName(name);
		frame.SetWidth(width);
		frame.SetMinWidth(width);
		frame.SetTabText(tab);
		frame.SetTitle(title);
		frame.SetSubtitle(subtitle);
		frame.SetAlign(0.5, 0.5);
		frame.SetIntro(0, 0.42, 22);
		return frame;
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyTheme(notnull MUI_ThemeData theme)
	{
		m_Style.m_Fill = Color.FromInt(0);
	}

	//------------------------------------------------------------------------------------------------
	void SetTabText(string text)
	{
		if (m_sTab == text)
			return;
		m_sTab = text;
		m_bTextDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	void SetTitle(string text)
	{
		if (m_sTitle == text)
			return;
		m_sTitle = text;
		m_bTextDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	void SetSubtitle(string text)
	{
		m_sSubtitle = text;
	}

	//------------------------------------------------------------------------------------------------
	//! The chip at the right of the header. \p busy adds the turning uplink arcs.
	void SetStatus(string text, notnull Color tone, bool busy)
	{
		m_bBusy = busy;
		m_StatusTone.SetR(tone.R());
		m_StatusTone.SetG(tone.G());
		m_StatusTone.SetB(tone.B());
		m_StatusTone.SetA(1);
		if (m_sStatus == text)
			return;
		m_sStatus = text;
		m_bTextDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! A short tracked line under the status chip.
	void SetNote(string text)
	{
		m_sNote = text;
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildTexts()
	{
		m_bTextDirty = false;
		if (!m_Runtime)
			return;
		IA_UplinkStyle look = IA_UplinkStyle.Get();
		m_fTabW = IA_TrackedText.Measure(m_Runtime, m_sTab, IA_UplinkStyle.FONT_TAB, IA_UplinkStyle.TRACK_TAB) + TAB_PAD_X * 2;
		m_fTitleW = IA_TrackedText.Width(m_Runtime, m_sTitle, FONT_TITLE);
		m_fStatusW = 0;
		if (!m_sStatus.IsEmpty())
			m_fStatusW = look.ChipWidth(m_Runtime, m_sStatus) + 12;
	}

	//------------------------------------------------------------------------------------------------
	override void SyncHostWidgets()
	{
		if (!m_bBlurEnabled || !IsVisible())
		{
			if (m_wBlur)
				m_wBlur.SetVisible(false);
			return;
		}

		if (!EnsureBlurWidget())
			return;

		float op = GetDrawOpacity();
		float x;
		float y;
		m_Runtime.GetHostLocalPos(this, x, y);
		float w = m_World.m_fW;
		float h = m_World.m_fH - TAB_H;
		if (op < 0.02 || w < 2 || h < 2)
		{
			m_wBlur.SetVisible(false);
			return;
		}

		m_wBlur.SetVisible(true);
		m_wBlur.SetColor(IA_UplinkStyle.Get().m_Bg);
		FrameSlot.SetAnchorMin(m_wBlur, 0, 0);
		FrameSlot.SetAnchorMax(m_wBlur, 0, 0);
		FrameSlot.SetPos(m_wBlur, x, y + TAB_H);
		FrameSlot.SetSize(m_wBlur, w, h);
		m_wBlur.SetOpacity(op);
		m_wBlur.SetIntensity(m_fBlurIntensity * op);
		m_wBlur.SetSmoothBorder(4, 0, 4, 4);
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
		float op = GetDrawOpacity();
		if (op < 0.01)
			return;

		if (m_bTextDirty)
			RebuildTexts();

		float x = DrawX();
		float y = DrawY();
		float w = m_World.m_fW;
		float bodyY = y + TAB_H;
		float bodyH = m_World.m_fH - TAB_H;

		SyncHostWidgets();
		DrawBody(surface, x, bodyY, w, bodyH, op);
		DrawTab(surface, x, y, op);
		DrawHead(surface, x, bodyY, w, op);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawBody(MUI_RenderSurface surface, float x, float y, float w, float h, float op)
	{
		IA_UplinkStyle look = IA_UplinkStyle.Get();
		float bevel = IA_UplinkStyle.BEVEL;

		m_aPoly.Clear();
		m_aPoly.Insert(x);
		m_aPoly.Insert(y);
		m_aPoly.Insert(x + w);
		m_aPoly.Insert(y);
		m_aPoly.Insert(x + w);
		m_aPoly.Insert(y + h - bevel);
		m_aPoly.Insert(x + w - bevel);
		m_aPoly.Insert(y + h);
		m_aPoly.Insert(x + bevel);
		m_aPoly.Insert(y + h);
		m_aPoly.Insert(x);
		m_aPoly.Insert(y + h - bevel);
		surface.FillPolygon(m_aPoly, MUI_ColorUtil.Fade(look.m_Bg, op));

		Color edge = MUI_ColorUtil.Fade(look.m_Tone, op * 0.22);
		surface.DrawLine(x, y, x + w, y, edge, 1);
		surface.DrawLine(x, y, x, y + h - bevel, edge, 1);
		surface.DrawLine(x + w, y, x + w, y + h - bevel, edge, 1);
		surface.DrawLine(x + bevel, y + h, x + w - bevel, y + h, MUI_ColorUtil.Fade(look.m_Tone, op * 0.10), 1);

		// Top glow, brightest in the middle.
		float glow = 0.5 + 0.5 * MUI_Ease.Pulse(GetTime(), 0.5);
		int slices = 48;
		float sliceW = w / slices;
		int i;
		for (i = 0; i < slices; i++)
		{
			float t = i;
			t = t / (slices - 1);
			float a = t * 2.0;
			if (t > 0.5)
				a = (1.0 - t) * 2.0;
			if (a < 0.02)
				continue;
			surface.FillRect(x + sliceW * i, y, sliceW + 0.5, 1, MUI_ColorUtil.Fade(look.m_Tone, op * 0.6 * glow * a), 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawTab(MUI_RenderSurface surface, float x, float y, float op)
	{
		IA_UplinkStyle look = IA_UplinkStyle.Get();
		surface.FillRect(x, y, m_fTabW, TAB_H + 1, MUI_ColorUtil.Fade(look.m_Tab, op), 0);

		Color edge = MUI_ColorUtil.Fade(look.m_Tone, op * 0.22);
		surface.DrawLine(x, y, x + m_fTabW, y, edge, 1);
		surface.DrawLine(x, y, x, y + TAB_H, edge, 1);
		surface.DrawLine(x + m_fTabW, y, x + m_fTabW, y + TAB_H, edge, 1);
		IA_TrackedText.Draw(surface, m_Runtime, x + TAB_PAD_X, y, TAB_H, m_sTab, IA_UplinkStyle.FONT_TAB, IA_UplinkStyle.TRACK_TAB, MUI_ColorUtil.Fade(look.m_Tone, op));
	}

	//------------------------------------------------------------------------------------------------
	//! The header band: title, what the screen is for, and the state of the link.
	protected void DrawHead(MUI_RenderSurface surface, float x, float y, float w, float op)
	{
		IA_UplinkStyle look = IA_UplinkStyle.Get();
		float time = GetTime();
		float left = x + PAD;
		float right = x + w - PAD;

		surface.FillRect(x + 1, y + 1, w - 2, HEAD_H, MUI_ColorUtil.Fade(look.m_Black, op * 0.30), 0);
		surface.FillRect(x + 1, y + HEAD_H, w - 2, 1, MUI_ColorUtil.Fade(look.m_Tone, op * 0.16), 0);
		look.Ruler(surface, x + 1, y + HEAD_H, w - 2, op);

		// Scan line crossing the band.
		float scan = time / SCAN_PERIOD;
		scan = scan - Math.Floor(scan);
		float scanX = x + 1 + scan * (w - 4);
		surface.FillRect(scanX, y + 1, 1.5, HEAD_H - 1, MUI_ColorUtil.Fade(look.m_Tone, op * 0.14), 0);
		int s;
		for (s = 1; s <= 5; s++)
		{
			float tail = scanX - s * 4;
			if (tail < x + 1)
				continue;
			surface.FillRect(tail, y + 1, 4, HEAD_H - 1, MUI_ColorUtil.Fade(look.m_Tone, op * 0.004 * (6 - s)), 0);
		}

		// Title with the kicker ticks the toasts carry.
		int i;
		for (i = 0; i < 3; i++)
		{
			surface.FillRect(left, y + 20 + i * 6, 5 + i * 3, 2, MUI_ColorUtil.Fade(look.m_Tone, op * (0.9 - i * 0.2)), 0);
		}
		float titleX = left + 20;
		surface.DrawText(titleX, y + 6, m_fTitleW + 12, HEAD_H - 14, m_sTitle, FONT_TITLE, MUI_ColorUtil.Fade(look.m_White, op), true, false, true, false, true);

		float subX = titleX + m_fTitleW + 18;
		float subW = right - m_fStatusW - 16 - subX;
		if (subW > 60 && !m_sSubtitle.IsEmpty())
		{
			surface.FillRect(subX - 9, y + 20, 1, 22, MUI_ColorUtil.Fade(look.m_Tone, op * 0.30), 0);
			surface.DrawText(subX, y + 8, subW, HEAD_H - 14, m_sSubtitle, FONT_SUB, MUI_ColorUtil.Fade(look.m_Muted, op), false, false, true, false, true);
		}

		// Link state.
		if (m_sStatus.IsEmpty())
			return;

		float chipX = right - m_fStatusW;
		float chipY = y + 14;
		float beat = 1.0;
		if (m_bBusy)
			beat = 0.6 + 0.4 * MUI_Ease.Pulse(time, 1.6);
		surface.FillRect(chipX, chipY, m_fStatusW, IA_UplinkStyle.CHIP_H, MUI_ColorUtil.Fade(m_StatusTone, op * 0.16 * beat), 0);
		surface.StrokeRect(chipX, chipY, m_fStatusW, IA_UplinkStyle.CHIP_H, MUI_ColorUtil.Fade(m_StatusTone, op * 0.5 * beat), 1, 0);
		float pulse = 0.55 + 0.45 * MUI_Ease.Pulse(time, 0.7);
		surface.FillCircle(chipX + 9, chipY + IA_UplinkStyle.CHIP_H * 0.5, 3, MUI_ColorUtil.Fade(m_StatusTone, op * pulse));
		IA_TrackedText.Draw(surface, m_Runtime, chipX + 18, chipY, IA_UplinkStyle.CHIP_H, m_sStatus, IA_UplinkStyle.FONT_CAP, IA_UplinkStyle.TRACK_CAP, MUI_ColorUtil.Fade(m_StatusTone, op));

		if (m_bBusy)
		{
			float cx = chipX - 18;
			float cy = chipY + IA_UplinkStyle.CHIP_H * 0.5;
			float spin = time * 240;
			surface.StrokeCircle(cx, cy, 7, MUI_ColorUtil.Fade(look.m_Tone, op * 0.18), 1);
			surface.DrawArc(cx, cy, 7, spin, 80, MUI_ColorUtil.Fade(m_StatusTone, op), 1.6);
			surface.DrawArc(cx, cy, 11, -spin * 0.7, 46, MUI_ColorUtil.Fade(look.m_Cyan, op * 0.55), 1.2);
		}

		if (!m_sNote.IsEmpty())
			IA_TrackedText.DrawRight(surface, m_Runtime, right, chipY + IA_UplinkStyle.CHIP_H + 6, 12, m_sNote, IA_UplinkStyle.FONT_CAP, IA_UplinkStyle.TRACK_CAP, MUI_ColorUtil.Fade(look.m_Muted, op * 0.9));
	}
}
