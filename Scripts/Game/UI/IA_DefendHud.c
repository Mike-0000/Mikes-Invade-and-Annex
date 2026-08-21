//------------------------------------------------------------------------------------------------
//! Persistent defend-hold readout. Independent of IA_NotificationToast so the
//! timer never blocks task / alert toasts. Pattern: Create(runtime) → bottom-left
//! objective strip AddChild. Keep as protected ref. Paint uses DrawX/Y +
//! GetDrawOpacity().
//!
//! Chrome matches IA_CaptureHud (bottom-docked 288×48 bezel). Shows remaining
//! M:SS and a pressure rail instead of capture percent.
//------------------------------------------------------------------------------------------------
enum IA_DefendHudState
{
	Hidden,
	Active,
	Complete
}

enum IA_DefendHudAnim
{
	Idle,
	Intro,
	Hold,
	Outro
}

//------------------------------------------------------------------------------------------------
class IA_DefendHud : MUI_Surface
{
	protected static const float HUD_W = 288;
	protected static const float GLOW_H = 48;
	protected static const float TAB_H = 18;
	protected static const float BODY_H = 48;
	protected static const float HUD_H = 66;
	protected static const float BEVEL = 4;
	protected static const float DOCK_Y = 1;
	protected static const float PAD_X = 16;
	protected static const float SPIN_R = 6;
	protected static const float SPIN_W = 2;
	protected static const float GAP_ICON = 12;
	protected static const float GAP_TIME = 6;
	protected static const float DIV_W = 2;
	protected static const float DIV_H = 24;
	protected static const float TAB_PAD_X = 12;
	protected static const float RAIL_H = 3;
	protected static const int FONT_TAB = 10;
	protected static const int FONT_KICK = 10;
	protected static const int FONT_TITLE = 13;
	protected static const int FONT_TIME = 14;
	protected static const float TRACK_TAB = 1.5;
	protected static const float TRACK_KICK = 1.2;
	protected static const float INTRO_DUR = 0.38;
	protected static const float OUTRO_DUR = 0.28;
	protected static const float SLIDE_FROM = 14;

	protected IA_DefendHudAnim m_eAnim;
	protected IA_DefendHudState m_eShownState;

	protected string m_sShownArea;

	protected float m_fServerPressure;
	protected float m_fPressureDisplay;
	protected float m_fLocalRemainSec;
	protected int m_iServerRemainSec;
	protected float m_fAnimT;
	protected float m_fCompleteHold;
	protected float m_fSpin;
	protected bool m_bArmed;

	protected ref Color m_HudBg;
	protected ref Color m_HudAmber;
	protected ref Color m_HudGreen;
	protected ref Color m_HudTab;
	protected ref Color m_HudWhite;
	protected ref Color m_HudRail;
	protected ref Color m_VignetteTop;
	protected ref Color m_VignetteBot;
	protected ref Color m_Shadow;
	protected ref array<float> m_aBodyPoly;

	//------------------------------------------------------------------------------------------------
	void IA_DefendHud()
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
		m_eAnim = IA_DefendHudAnim.Idle;
		m_eShownState = IA_DefendHudState.Hidden;
		m_sShownArea = "";
		m_fIntroDuration = 0;
		m_fIntro = 1;
		m_HudBg = Color.FromSRGBA(13, 20, 18, 230);
		m_HudAmber = Color.FromSRGBA(240, 180, 70, 255);
		m_HudGreen = Color.FromSRGBA(94, 251, 131, 255);
		m_HudTab = Color.FromSRGBA(56, 40, 18, 204);
		m_HudWhite = Color.FromSRGBA(255, 255, 255, 255);
		m_HudRail = Color.FromSRGBA(40, 32, 18, 180);
		m_VignetteTop = Color.FromSRGBA(0, 0, 0, 0);
		m_VignetteBot = Color.FromSRGBA(0, 0, 0, 153);
		m_Shadow = Color.FromSRGBA(0, 0, 0, 204);
		m_aBodyPoly = new array<float>();
	}

	//------------------------------------------------------------------------------------------------
	static IA_DefendHud Create(notnull MUI_Runtime runtime)
	{
		ref IA_DefendHud hud = new IA_DefendHud();
		runtime.Adopt(hud);
		hud.SetName("defendHold");
		hud.SetWidth(HUD_W);
		hud.SetHeight(HUD_H);
		hud.SetVisible(false);
		return hud;
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
		m_eAnim = IA_DefendHudAnim.Idle;
		m_eShownState = IA_DefendHudState.Hidden;
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
		PullServer();
		TickAnim(dt);
		TickRemain(dt);
		TickPressure(dt);
		TickSpin(dt);

		if (m_eAnim != IA_DefendHudAnim.Idle)
			InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	protected void PullServer()
	{
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		IA_DefendHudState nextState = IA_DefendHudState.Hidden;
		string nextArea = "";
		int nextRemain = 0;
		float nextPressure = 0;
		if (init)
		{
			nextState = DecodeState(init.GetDefendHudState());
			nextArea = init.GetDefendHudArea();
			nextRemain = init.GetDefendHudRemainingSec();
			nextPressure = init.GetDefendHudPressure();
		}

		if (nextPressure < 0)
			nextPressure = 0;
		if (nextPressure > 1)
			nextPressure = 1;
		if (nextRemain < 0)
			nextRemain = 0;

		m_fServerPressure = nextPressure;
		m_iServerRemainSec = nextRemain;

		if (nextState == IA_DefendHudState.Hidden)
		{
			if (m_eAnim == IA_DefendHudAnim.Intro || m_eAnim == IA_DefendHudAnim.Hold)
				BeginOutro();
			return;
		}

		bool areaChanged = m_sShownArea != nextArea;
		if (areaChanged || !m_bArmed)
			Arm(nextArea, nextState, nextRemain, nextPressure);
		else if (m_eShownState != nextState)
			m_eShownState = nextState;
	}

	//------------------------------------------------------------------------------------------------
	protected IA_DefendHudState DecodeState(int raw)
	{
		if (raw == IA_DefendHudState.Active)
			return IA_DefendHudState.Active;
		if (raw == IA_DefendHudState.Complete)
			return IA_DefendHudState.Complete;
		return IA_DefendHudState.Hidden;
	}

	//------------------------------------------------------------------------------------------------
	protected void Arm(string areaName, IA_DefendHudState state, int remainingSec, float pressure)
	{
		bool snapDisplay = !m_bArmed;
		if (m_sShownArea != areaName)
			snapDisplay = true;

		m_sShownArea = areaName;
		m_eShownState = state;
		m_iServerRemainSec = remainingSec;
		m_fServerPressure = pressure;
		if (snapDisplay)
		{
			m_fLocalRemainSec = remainingSec;
			m_fPressureDisplay = pressure;
		}
		m_bArmed = true;
		m_fCompleteHold = 0;

		if (m_eAnim == IA_DefendHudAnim.Idle || m_eAnim == IA_DefendHudAnim.Outro)
			BeginIntro();
	}

	//------------------------------------------------------------------------------------------------
	protected void BeginIntro()
	{
		m_eAnim = IA_DefendHudAnim.Intro;
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
		if (m_eAnim == IA_DefendHudAnim.Idle)
			return;
		if (m_eAnim == IA_DefendHudAnim.Outro)
			return;
		m_eAnim = IA_DefendHudAnim.Outro;
		m_fAnimT = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickAnim(float dt)
	{
		if (m_eAnim == IA_DefendHudAnim.Idle)
			return;

		if (m_eAnim == IA_DefendHudAnim.Intro)
		{
			m_fAnimT = m_fAnimT + dt / INTRO_DUR;
			float t = MUI_Ease.CubicOut(m_fAnimT);
			m_fIntro = t;
			m_fSlideY = (1.0 - t) * SLIDE_FROM + t * DOCK_Y;
			if (m_fAnimT < 1)
				return;
			m_eAnim = IA_DefendHudAnim.Hold;
			m_fIntro = 1;
			m_fSlideY = DOCK_Y;
			return;
		}

		if (m_eAnim == IA_DefendHudAnim.Hold)
		{
			m_fSlideY = DOCK_Y;
			if (m_eShownState == IA_DefendHudState.Complete)
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

		m_eAnim = IA_DefendHudAnim.Idle;
		m_bArmed = false;
		m_sShownArea = "";
		m_eShownState = IA_DefendHudState.Hidden;
		m_fIntro = 1;
		m_fSlideY = 0;
		SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickRemain(float dt)
	{
		if (m_eAnim == IA_DefendHudAnim.Idle)
			return;
		if (m_eAnim == IA_DefendHudAnim.Outro)
			return;

		if (m_eShownState == IA_DefendHudState.Complete)
		{
			m_fLocalRemainSec = 0;
			return;
		}

		m_fLocalRemainSec = m_fLocalRemainSec - dt;
		if (m_fLocalRemainSec < 0)
			m_fLocalRemainSec = 0;

		float serverRemain = m_iServerRemainSec;
		float delta = m_fLocalRemainSec - serverRemain;
		if (delta < 0)
			delta = -delta;
		if (delta > 2.5)
			m_fLocalRemainSec = serverRemain;
		else if (m_fLocalRemainSec < serverRemain - 0.35)
			m_fLocalRemainSec = serverRemain;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickPressure(float dt)
	{
		if (m_eAnim == IA_DefendHudAnim.Idle)
			return;
		if (m_eAnim == IA_DefendHudAnim.Outro)
			return;

		m_fPressureDisplay = MUI_Ease.Approach(m_fPressureDisplay, m_fServerPressure, dt, 8);
		if (m_fPressureDisplay < 0)
			m_fPressureDisplay = 0;
		if (m_fPressureDisplay > 1)
			m_fPressureDisplay = 1;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickSpin(float dt)
	{
		if (m_eAnim == IA_DefendHudAnim.Idle)
			return;
		if (m_eAnim == IA_DefendHudAnim.Outro)
			return;

		if (m_eShownState == IA_DefendHudState.Complete)
			return;

		float degPerSec = 180 + m_fPressureDisplay * 360;
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
		if (m_eAnim == IA_DefendHudAnim.Idle)
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
		DrawVignette(surface, x, tabY - GLOW_H, w, op);
		DrawBody(surface, x, bodyY, w, op, tone);
		DrawTab(surface, x, tabY, w, op, tone);
		DrawContent(surface, x, bodyY, w, op, tone);
	}

	//------------------------------------------------------------------------------------------------
	protected Color ResolveTone(notnull MUI_ThemeData theme)
	{
		if (m_eShownState == IA_DefendHudState.Complete)
			return m_HudGreen;
		if (m_fPressureDisplay >= 0.75)
			return theme.Danger;
		return m_HudAmber;
	}

	//------------------------------------------------------------------------------------------------
	protected string ResolveKicker()
	{
		if (m_eShownState == IA_DefendHudState.Complete)
			return "POSITION HELD";
		return "DEFEND";
	}

	//------------------------------------------------------------------------------------------------
	protected string ResolveTab()
	{
		if (m_eShownState == IA_DefendHudState.Complete)
			return "SECURE";
		if (m_fPressureDisplay >= 0.75)
			return "ASSAULT";
		return "HOLD";
	}

	//------------------------------------------------------------------------------------------------
	protected string FormatRemain()
	{
		int total = m_fLocalRemainSec;
		if (total < 0)
			total = 0;
		int minutes = total / 60;
		int seconds = total - minutes * 60;
		string secStr;
		if (seconds < 10)
			secStr = "0" + seconds.ToString();
		else
			secStr = seconds.ToString();
		return minutes.ToString() + ":" + secStr;
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawVignette(MUI_RenderSurface surface, float x, float y, float w, float op)
	{
		surface.FillGradientV(x, y, w, GLOW_H, MUI_ColorUtil.Fade(m_VignetteTop, op), MUI_ColorUtil.Fade(m_VignetteBot, op), 10);
		surface.FillRect(x - 8, y + GLOW_H - 18, w + 16, 28, MUI_ColorUtil.Fade(m_Shadow, op * 0.35), 16);
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
		if (m_eShownState == IA_DefendHudState.Complete)
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

		float railY = y + BODY_H - RAIL_H - 5;
		float railX = x + PAD_X;
		float railW = w - PAD_X * 2;
		if (railW < 8)
			railW = 8;
		surface.FillRect(railX, railY, railW, RAIL_H, MUI_ColorUtil.Fade(m_HudRail, op), 0);
		float fillW = railW * m_fPressureDisplay;
		if (fillW > 0)
			surface.FillRect(railX, railY, fillW, RAIL_H, MUI_ColorUtil.Fade(tone, op * 0.90), 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawTab(MUI_RenderSurface surface, float x, float y, float w, float op, Color tone)
	{
		string tab = ResolveTab();
		float textW = MeasureTracked(tab, FONT_TAB, TRACK_TAB);
		float tabW = textW + TAB_PAD_X * 2;
		if (tabW < 48)
			tabW = 48;
		float tx = x + (w - tabW) * 0.5;
		float ty = y;

		surface.FillRect(tx, ty, tabW, TAB_H + 1, MUI_ColorUtil.Fade(m_HudTab, op), 0);

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
		float cy = y + BODY_H * 0.5 - 2;
		DrawSpinner(surface, cx, cy, op, tone);

		float copyX = x + PAD_X + SPIN_R * 2 + GAP_ICON;
		float kickerH = 10;
		float titleH = 14;
		float copyGap = 4;
		float copyTop = y + (BODY_H - RAIL_H - 8 - (kickerH + copyGap + titleH)) * 0.5;

		Color kickerCol = MUI_ColorUtil.Fade(tone, op * 0.80);
		DrawTracked(surface, copyX, copyTop, kickerH, ResolveKicker(), FONT_KICK, TRACK_KICK, kickerCol);

		string area = m_sShownArea;
		if (area.IsEmpty())
			area = "Position";
		float divX = x + w - PAD_X - DIV_W;
		float titleW = divX - GAP_TIME - 48 - copyX;
		if (titleW < 48)
			titleW = 48;
		surface.DrawText(copyX, copyTop + kickerH + copyGap, titleW, titleH, area, FONT_TITLE, MUI_ColorUtil.Fade(m_HudWhite, op), true, false, true, false, true);

		string timeText = FormatRemain();
		float timeW = 44;
		float timeH = 16;
		if (m_Runtime)
			m_Runtime.MeasureText(timeText, FONT_TIME, true, 0, timeW, timeH);
		float timeX = divX - GAP_TIME - timeW;
		surface.DrawText(timeX, y, timeW, BODY_H - RAIL_H - 6, timeText, FONT_TIME, MUI_ColorUtil.Fade(m_HudWhite, op), true, false, true, false, true);

		float divY = y + (BODY_H - DIV_H - 6) * 0.5;
		surface.FillRect(divX, divY, DIV_W, DIV_H, MUI_ColorUtil.Fade(tone, op * 0.20), 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawSpinner(MUI_RenderSurface surface, float cx, float cy, float op, Color tone)
	{
		Color track = m_HudTab;
		surface.StrokeCircle(cx, cy, SPIN_R, MUI_ColorUtil.Fade(track, op), SPIN_W);

		if (m_eShownState == IA_DefendHudState.Complete)
		{
			surface.StrokeCircle(cx, cy, SPIN_R, MUI_ColorUtil.Fade(tone, op), SPIN_W);
		}
		else
		{
			float start = -90 + m_fSpin;
			surface.DrawArc(cx, cy, SPIN_R, start, 90, MUI_ColorUtil.Fade(tone, op), SPIN_W);
		}

		float pulse = 0.55 + 0.45 * MUI_Ease.Pulse(GetTime(), 0.5);
		if (m_eShownState == IA_DefendHudState.Complete)
			pulse = 1;
		surface.FillCircle(cx, cy, 2.1, MUI_ColorUtil.Fade(tone, op * pulse));
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
