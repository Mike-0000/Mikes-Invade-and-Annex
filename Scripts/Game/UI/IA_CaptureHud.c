//------------------------------------------------------------------------------------------------
//! Persistent capture readout. Independent of IA_NotificationToast so a long hold
//! never blocks task / alert toasts. Pattern: Create(runtime) → overlay strip
//! AddChild. Keep as protected ref. Paint uses DrawX/Y + GetDrawOpacity().
//!
//! Chrome: 288×48 beveled bar (shrinks when several sit in the bottom-left
//! strip), 18px status tab on the top-left, spinning ring, area name, percent.
//! Flush to the bottom edge. Colors are the mock tokens (#0d1412 / #5efb83 /
//! #163824), not the uplink theme. Tie uses the amber warning tone; deficit
//! uses Danger. At 0% with no capture majority the ring stops and the tab
//! reads BLOCKED. Capture and loss both run 30% faster than the 120s baseline.
//------------------------------------------------------------------------------------------------
enum IA_CaptureHudState
{
	Hidden,
	Capturing,
	Paused,
	Contested,
	Complete,
	Blocked
}

enum IA_CaptureHudAnim
{
	Idle,
	Intro,
	Hold,
	Outro
}

//------------------------------------------------------------------------------------------------
class IA_CaptureHud : MUI_Surface
{
	protected static const float HUD_W = 288;
	protected static const float TAB_H = 18;
	protected static const float BODY_H = 48;
	protected static const float HUD_H = 66;
	protected static const float BEVEL = 4;
	protected static const float DOCK_Y = 1;
	protected static const float PAD_X = 14;
	protected static const float SPIN_R = 11;
	protected static const float SPIN_W = 3;
	protected static const float GAP_ICON = 10;
	protected static const float GAP_PCT = 6;
	protected static const float DIV_W = 2;
	protected static const float DIV_H = 24;
	protected static const float TAB_PAD_X = 12;
	protected static const int FONT_TAB = 10;
	protected static const int FONT_TITLE = 18;
	protected static const int FONT_PCT = 14;
	protected static const float TRACK_TAB = 1.5;
	protected static const float INTRO_DUR = 0.38;
	protected static const float OUTRO_DUR = 0.28;
	protected static const float SLIDE_FROM = 14;
	protected static const float CAPTURE_BASE_SECONDS = 120.0;
	protected static const float CAPTURE_RATE = 1.3;

	protected IA_CaptureHudAnim m_eAnim;
	protected IA_CaptureHudState m_eShownState;

	protected string m_sShownArea;

	protected float m_fServerProgress;
	protected float m_fPredicted;
	protected float m_fDisplay;
	protected float m_fAnimT;
	protected float m_fCompleteHold;
	protected float m_fSpin;
	protected bool m_bArmed;

	protected ref Color m_HudBg;
	protected ref Color m_HudGreen;
	protected ref Color m_HudAmber;
	protected ref Color m_HudTab;
	protected ref Color m_HudTabHold;
	protected ref Color m_HudTabLose;
	protected ref Color m_HudWhite;
	protected ref array<float> m_aBodyPoly;

	//------------------------------------------------------------------------------------------------
	void IA_CaptureHud()
	{
		m_Style.m_WidthMode = MUI_SizeMode.Exact;
		m_Style.m_HeightMode = MUI_SizeMode.Exact;
		m_Style.m_fWidth = HUD_W;
		m_Style.m_fHeight = HUD_H;
		m_Style.m_fMinWidth = HUD_W;
		m_Style.m_fMinHeight = HUD_H;
		m_Style.m_fRadius = 0;
		m_Style.m_Fill = Color.FromInt(0);
		m_Style.m_bBlockHit = false;
		m_Style.m_bInteractive = false;
		m_bBlurEnabled = true;
		m_fBlurIntensity = 0.90;
		m_eAnim = IA_CaptureHudAnim.Idle;
		m_eShownState = IA_CaptureHudState.Hidden;
		m_sShownArea = "";
		m_fIntroDuration = 0;
		m_fIntro = 1;
		m_HudBg = Color.FromSRGBA(13, 20, 18, 230);
		m_HudGreen = Color.FromSRGBA(94, 251, 131, 255);
		m_HudAmber = Color.FromSRGBA(240, 180, 70, 255);
		m_HudTab = Color.FromSRGBA(22, 56, 36, 204);
		m_HudTabHold = Color.FromSRGBA(56, 40, 18, 204);
		m_HudTabLose = Color.FromSRGBA(56, 18, 18, 204);
		m_HudWhite = Color.FromSRGBA(255, 255, 255, 255);
		m_aBodyPoly = new array<float>();
	}

	//------------------------------------------------------------------------------------------------
	static IA_CaptureHud Create(notnull MUI_Runtime runtime)
	{
		ref IA_CaptureHud hud = new IA_CaptureHud();
		runtime.Adopt(hud);
		hud.SetName("sectorHold");
		hud.SetWidth(HUD_W);
		hud.SetHeight(HUD_H);
		hud.SetVisible(false);
		return hud;
	}

	//------------------------------------------------------------------------------------------------
	static float GetHudWidth()
	{
		return HUD_W;
	}

	//------------------------------------------------------------------------------------------------
	static float GetHudHeight()
	{
		return HUD_H;
	}

	//------------------------------------------------------------------------------------------------
	string GetShownArea()
	{
		return m_sShownArea;
	}

	//------------------------------------------------------------------------------------------------
	bool IsIdle()
	{
		return m_eAnim == IA_CaptureHudAnim.Idle;
	}

	//------------------------------------------------------------------------------------------------
	void SetDockWidth(float w)
	{
		if (w < 180)
			w = 180;
		if (Math.AbsFloat(m_Style.m_fWidth - w) < 0.5)
			return;
		SetWidth(w);
		SetMinWidth(w);
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyTheme(notnull MUI_ThemeData theme)
	{
		m_Style.m_Fill = Color.FromInt(0);
	}

	//------------------------------------------------------------------------------------------------
	void Abort()
	{
		m_eAnim = IA_CaptureHudAnim.Idle;
		m_eShownState = IA_CaptureHudState.Hidden;
		m_bArmed = false;
		m_sShownArea = "";
		m_fIntro = 1;
		m_fSlideY = 0;
		SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnTick(float dt)
	{
		super.OnTick(dt);
		TickAnim(dt);
		TickProgress(dt);
		TickSpin(dt);

		if (m_eAnim != IA_CaptureHudAnim.Idle)
			InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	void ApplyServer(string areaName, IA_CaptureHudState state, float progress)
	{
		if (progress < 0)
			progress = 0;
		if (progress > 1)
			progress = 1;

		m_fServerProgress = progress;

		if (state == IA_CaptureHudState.Hidden)
		{
			if (m_eAnim == IA_CaptureHudAnim.Intro || m_eAnim == IA_CaptureHudAnim.Hold)
				BeginOutro();
			return;
		}

		bool areaChanged = m_sShownArea != areaName;
		if (areaChanged || !m_bArmed || m_eAnim == IA_CaptureHudAnim.Outro)
			Arm(areaName, state, progress);
		else if (m_eShownState != state)
			m_eShownState = state;
	}

	//------------------------------------------------------------------------------------------------
	static IA_CaptureHudState DecodeState(int raw)
	{
		if (raw == IA_CaptureHudState.Capturing)
			return IA_CaptureHudState.Capturing;
		if (raw == IA_CaptureHudState.Paused)
			return IA_CaptureHudState.Paused;
		if (raw == IA_CaptureHudState.Contested)
			return IA_CaptureHudState.Contested;
		if (raw == IA_CaptureHudState.Complete)
			return IA_CaptureHudState.Complete;
		if (raw == IA_CaptureHudState.Blocked)
			return IA_CaptureHudState.Blocked;
		return IA_CaptureHudState.Hidden;
	}

	//------------------------------------------------------------------------------------------------
	protected void Arm(string areaName, IA_CaptureHudState state, float progress)
	{
		bool snapDisplay = !m_bArmed;
		if (m_sShownArea != areaName)
			snapDisplay = true;

		m_sShownArea = areaName;
		m_eShownState = state;
		m_fPredicted = progress;
		if (snapDisplay)
			m_fDisplay = progress;
		m_bArmed = true;
		m_fCompleteHold = 0;

		if (m_eAnim == IA_CaptureHudAnim.Idle || m_eAnim == IA_CaptureHudAnim.Outro)
			BeginIntro();
	}

	//------------------------------------------------------------------------------------------------
	protected void BeginIntro()
	{
		m_eAnim = IA_CaptureHudAnim.Intro;
		m_fAnimT = 0;
		m_fIntroDuration = 0;
		m_fIntro = 0;
		m_fSlideY = SLIDE_FROM;
		SetVisible(true);
		PlayRipple();
	}

	//------------------------------------------------------------------------------------------------
	protected void BeginOutro()
	{
		if (m_eAnim == IA_CaptureHudAnim.Idle)
			return;
		if (m_eAnim == IA_CaptureHudAnim.Outro)
			return;
		m_eAnim = IA_CaptureHudAnim.Outro;
		m_fAnimT = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickAnim(float dt)
	{
		if (m_eAnim == IA_CaptureHudAnim.Idle)
			return;

		if (m_eAnim == IA_CaptureHudAnim.Intro)
		{
			m_fAnimT = m_fAnimT + dt / INTRO_DUR;
			float t = MUI_Ease.CubicOut(m_fAnimT);
			m_fIntro = t;
			m_fSlideY = (1.0 - t) * SLIDE_FROM + t * DOCK_Y;
			if (m_fAnimT < 1)
				return;
			m_eAnim = IA_CaptureHudAnim.Hold;
			m_fIntro = 1;
			m_fSlideY = DOCK_Y;
			return;
		}

		if (m_eAnim == IA_CaptureHudAnim.Hold)
		{
			m_fSlideY = DOCK_Y;
			if (m_eShownState == IA_CaptureHudState.Complete)
			{
				m_fCompleteHold = m_fCompleteHold + dt;
				if (m_fCompleteHold >= 2.15)
					BeginOutro();
			}
			return;
		}

		m_fAnimT = m_fAnimT + dt / OUTRO_DUR;
		float u = MUI_Ease.CubicIn(m_fAnimT);
		m_fIntro = 1.0 - u;
		m_fSlideY = DOCK_Y + u * 12;
		if (m_fAnimT < 1)
			return;

		m_eAnim = IA_CaptureHudAnim.Idle;
		m_bArmed = false;
		m_sShownArea = "";
		m_eShownState = IA_CaptureHudState.Hidden;
		m_fIntro = 1;
		m_fSlideY = 0;
		SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickProgress(float dt)
	{
		if (m_eAnim == IA_CaptureHudAnim.Idle)
			return;
		if (m_eAnim == IA_CaptureHudAnim.Outro)
			return;

		if (m_eShownState == IA_CaptureHudState.Capturing)
		{
			m_fPredicted = m_fPredicted + dt * CAPTURE_RATE / CAPTURE_BASE_SECONDS;
			if (m_fServerProgress > m_fPredicted)
				m_fPredicted = m_fServerProgress;
			else if (m_fPredicted - m_fServerProgress > 0.03)
				m_fPredicted = m_fServerProgress;
		}
		else if (m_eShownState == IA_CaptureHudState.Contested)
		{
			m_fPredicted = m_fPredicted - dt * CAPTURE_RATE / CAPTURE_BASE_SECONDS;
			if (m_fServerProgress < m_fPredicted)
				m_fPredicted = m_fServerProgress;
			else if (m_fServerProgress - m_fPredicted > 0.03)
				m_fPredicted = m_fServerProgress;
		}
		else
		{
			m_fPredicted = MUI_Ease.Approach(m_fPredicted, m_fServerProgress, dt, 8);
		}

		if (m_fPredicted < 0)
			m_fPredicted = 0;
		if (m_fPredicted > 1)
			m_fPredicted = 1;

		if (m_eShownState == IA_CaptureHudState.Complete)
			m_fPredicted = 1;
		else if (m_eShownState == IA_CaptureHudState.Blocked)
			m_fPredicted = 0;

		m_fDisplay = MUI_Ease.Approach(m_fDisplay, m_fPredicted, dt, 9);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickSpin(float dt)
	{
		if (m_eAnim == IA_CaptureHudAnim.Idle)
			return;
		if (m_eAnim == IA_CaptureHudAnim.Outro)
			return;

		float degPerSec = 0;
		if (m_eShownState == IA_CaptureHudState.Capturing)
			degPerSec = 360;
		else if (m_eShownState == IA_CaptureHudState.Contested)
			degPerSec = 540;

		if (degPerSec <= 0)
			return;

		m_fSpin = m_fSpin + dt * degPerSec;
		if (m_fSpin >= 360)
			m_fSpin = m_fSpin - 360;
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
		float bodyY = y + TAB_H;
		if (op < 0.02 || w < 2)
		{
			m_wBlur.SetVisible(false);
			return;
		}

		m_wBlur.SetVisible(true);
		m_wBlur.SetColor(m_HudBg);
		FrameSlot.SetAnchorMin(m_wBlur, 0, 0);
		FrameSlot.SetAnchorMax(m_wBlur, 0, 0);
		FrameSlot.SetPos(m_wBlur, x, bodyY);
		FrameSlot.SetSize(m_wBlur, w, BODY_H + DOCK_Y);
		m_wBlur.SetOpacity(op);
		m_wBlur.SetIntensity(m_fBlurIntensity * op);
		m_wBlur.SetSmoothBorder(4, 0, 4, 4);
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
		if (m_eAnim == IA_CaptureHudAnim.Idle)
			return;

		float x = DrawX();
		float y = DrawY();
		float w = m_World.m_fW;
		float op = GetDrawOpacity();
		if (op < 0.01)
			return;

		MUI_ThemeData theme = GetTheme();
		Color tone = ResolveTone(theme);
		float tabY = y;
		float bodyY = y + TAB_H;

		SyncHostWidgets();
		DrawBody(surface, x, bodyY, w, op, tone);
		DrawTab(surface, x, tabY, w, op, tone);
		DrawContent(surface, x, bodyY, w, op, tone);
	}

	//------------------------------------------------------------------------------------------------
	protected Color ResolveTone(notnull MUI_ThemeData theme)
	{
		if (m_eShownState == IA_CaptureHudState.Contested)
			return theme.Danger;
		if (m_eShownState == IA_CaptureHudState.Blocked)
			return theme.Danger;
		if (m_eShownState == IA_CaptureHudState.Paused)
			return m_HudAmber;
		return m_HudGreen;
	}

	//------------------------------------------------------------------------------------------------
	protected Color ResolveTabFill()
	{
		if (m_eShownState == IA_CaptureHudState.Contested)
			return m_HudTabLose;
		if (m_eShownState == IA_CaptureHudState.Blocked)
			return m_HudTabLose;
		if (m_eShownState == IA_CaptureHudState.Paused)
			return m_HudTabHold;
		return m_HudTab;
	}

	//------------------------------------------------------------------------------------------------
	protected string ResolveTab()
	{
		if (m_eShownState == IA_CaptureHudState.Capturing)
			return "SECURING";
		if (m_eShownState == IA_CaptureHudState.Contested)
			return "LOSING";
		if (m_eShownState == IA_CaptureHudState.Blocked)
			return "BLOCKED";
		if (m_eShownState == IA_CaptureHudState.Paused)
			return "HOLD";
		if (m_eShownState == IA_CaptureHudState.Complete)
			return "SECURE";
		return "LIVE";
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawBody(MUI_RenderSurface surface, float x, float y, float w, float op, Color tone)
	{
		BuildBodyPoly(x, y, w, BODY_H + DOCK_Y);
		surface.FillPolygon(m_aBodyPoly, MUI_ColorUtil.Fade(m_HudBg, op));

		Color edge = MUI_ColorUtil.Fade(tone, op * 0.20);
		surface.DrawLine(x, y, x + w, y, edge, 1);
		surface.DrawLine(x, y, x, y + BODY_H + DOCK_Y - BEVEL, edge, 1);
		surface.DrawLine(x + w, y, x + w, y + BODY_H + DOCK_Y - BEVEL, edge, 1);

		float glow = 0.5 + 0.5 * MUI_Ease.Pulse(GetTime(), 0.5);
		if (m_eShownState == IA_CaptureHudState.Paused)
			glow = 0.55;
		else if (m_eShownState == IA_CaptureHudState.Blocked)
			glow = 0.55;
		else if (m_eShownState == IA_CaptureHudState.Complete)
			glow = 1;

		int slices = 24;
		float sliceW = w / slices;
		int i;
		for (i = 0; i < slices; i++)
		{
			float t = i;
			t = t / (slices - 1);
			float a = t;
			if (t > 0.5)
				a = 1.0 - t;
			a = a * 2.0;
			if (a < 0)
				a = 0;
			surface.FillRect(x + sliceW * i, y, sliceW + 0.5, 1, MUI_ColorUtil.Fade(tone, op * 0.50 * glow * a), 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawTab(MUI_RenderSurface surface, float x, float y, float w, float op, Color tone)
	{
		string tab = ResolveTab();
		float textW = MeasureTracked(tab, FONT_TAB, TRACK_TAB);
		float tabW = textW + TAB_PAD_X * 2;
		if (tabW < 48)
			tabW = 48;
		if (tabW > w)
			tabW = w;
		float tx = x;
		float ty = y;

		surface.FillRect(tx, ty, tabW, TAB_H + 1, MUI_ColorUtil.Fade(ResolveTabFill(), op), 0);

		Color edge = MUI_ColorUtil.Fade(tone, op * 0.20);
		surface.DrawLine(tx, ty, tx + tabW, ty, edge, 1);
		surface.DrawLine(tx, ty, tx, ty + TAB_H, edge, 1);
		surface.DrawLine(tx + tabW, ty, tx + tabW, ty + TAB_H, edge, 1);

		DrawTracked(surface, tx + TAB_PAD_X, ty, TAB_H, tab, FONT_TAB, TRACK_TAB, MUI_ColorUtil.Fade(tone, op));
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawContent(MUI_RenderSurface surface, float x, float y, float w, float op, Color tone)
	{
		float cx = x + PAD_X + SPIN_R;
		float cy = y + BODY_H * 0.5;
		DrawSpinner(surface, cx, cy, op, tone);

		float copyX = x + PAD_X + SPIN_R * 2 + GAP_ICON;

		string area = m_sShownArea;
		if (area.IsEmpty())
			area = "Unknown Sector";
		float divX = x + w - PAD_X - DIV_W;
		float titleW = divX - GAP_PCT - 36 - copyX;
		if (titleW < 48)
			titleW = 48;
		surface.DrawText(copyX, y, titleW, BODY_H, area, FONT_TITLE, MUI_ColorUtil.Fade(m_HudWhite, op), true, false, true, false, true);

		int pct = Math.Round(m_fDisplay * 100);
		if (pct < 0)
			pct = 0;
		if (pct > 100)
			pct = 100;
		string pctText = pct.ToString() + "%";
		float pctW = 32;
		float pctH = 16;
		if (m_Runtime)
			m_Runtime.MeasureText(pctText, FONT_PCT, true, 0, pctW, pctH);
		float pctX = divX - GAP_PCT - pctW;
		surface.DrawText(pctX, y, pctW, BODY_H, pctText, FONT_PCT, MUI_ColorUtil.Fade(m_HudWhite, op), true, false, true, false, true);

		float divY = y + (BODY_H - DIV_H) * 0.5;
		surface.FillRect(divX, divY, DIV_W, DIV_H, MUI_ColorUtil.Fade(tone, op * 0.20), 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawSpinner(MUI_RenderSurface surface, float cx, float cy, float op, Color tone)
	{
		Color track = ResolveTabFill();
		if (m_eShownState == IA_CaptureHudState.Contested)
			track = MUI_ColorUtil.Fade(tone, op * 0.45);
		else if (m_eShownState == IA_CaptureHudState.Blocked)
			track = MUI_ColorUtil.Fade(tone, op * 0.45);
		DrawRing(surface, cx, cy, MUI_ColorUtil.Fade(track, op));

		if (m_eShownState == IA_CaptureHudState.Complete)
		{
			DrawRing(surface, cx, cy, MUI_ColorUtil.Fade(tone, op));
			return;
		}

		if (m_eShownState == IA_CaptureHudState.Blocked)
		{
			float arm = SPIN_R * 0.55;
			Color cross = MUI_ColorUtil.Fade(tone, op);
			surface.DrawLine(cx - arm, cy - arm, cx + arm, cy + arm, cross, SPIN_W);
			surface.DrawLine(cx - arm, cy + arm, cx + arm, cy - arm, cross, SPIN_W);
			return;
		}

		float start = -90 + m_fSpin;
		surface.DrawArc(cx, cy, SPIN_R, start, 90, MUI_ColorUtil.Fade(tone, op), SPIN_W);
	}

	//------------------------------------------------------------------------------------------------
	//! Two open 180° arcs instead of StrokeCircle. A closed 360° polyline plus
	//! enclose leaves a spoke at 0° through the ring interior.
	protected void DrawRing(MUI_RenderSurface surface, float cx, float cy, Color color)
	{
		surface.DrawArc(cx, cy, SPIN_R, 0, 180, color, SPIN_W);
		surface.DrawArc(cx, cy, SPIN_R, 180, 180, color, SPIN_W);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildBodyPoly(float x, float y, float w, float h)
	{
		m_aBodyPoly.Clear();
		m_aBodyPoly.Insert(x);
		m_aBodyPoly.Insert(y);
		m_aBodyPoly.Insert(x + w);
		m_aBodyPoly.Insert(y);
		m_aBodyPoly.Insert(x + w);
		m_aBodyPoly.Insert(y + h - BEVEL);
		m_aBodyPoly.Insert(x + w - BEVEL);
		m_aBodyPoly.Insert(y + h);
		m_aBodyPoly.Insert(x + BEVEL);
		m_aBodyPoly.Insert(y + h);
		m_aBodyPoly.Insert(x);
		m_aBodyPoly.Insert(y + h - BEVEL);
	}

	//------------------------------------------------------------------------------------------------
	protected float MeasureTracked(string text, int fontSize, float tracking)
	{
		if (text.IsEmpty())
			return 0;
		if (!m_Runtime)
			return text.Length() * fontSize * 0.55;

		float total = 0;
		int len = text.Length();
		int i;
		for (i = 0; i < len; i++)
		{
			float cw = 8;
			float ch = fontSize;
			m_Runtime.MeasureText(text.Substring(i, 1), fontSize, true, 0, cw, ch);
			total = total + cw;
			if (i < len - 1)
				total = total + tracking;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawTracked(MUI_RenderSurface surface, float x, float y, float h, string text, int fontSize, float tracking, Color color)
	{
		if (text.IsEmpty())
			return;

		float cx = x;
		int len = text.Length();
		int i;
		for (i = 0; i < len; i++)
		{
			string ch = text.Substring(i, 1);
			float cw = 8;
			float chh = fontSize;
			if (m_Runtime)
				m_Runtime.MeasureText(ch, fontSize, true, 0, cw, chh);
			surface.DrawText(cx, y, cw + 2, h, ch, fontSize, color, true, false, true, false, true);
			cx = cx + cw + tracking;
		}
	}
}
