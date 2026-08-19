//------------------------------------------------------------------------------------------------
//! Persistent capture readout. Independent of IA_NotificationToast so a long hold
//! never blocks task / alert toasts. Pattern: Create(runtime) → overlay.AddChild.
//! Keep as protected ref. Paint uses DrawX/Y + GetDrawOpacity().
//------------------------------------------------------------------------------------------------
enum IA_CaptureHudState
{
	Hidden,
	Capturing,
	Paused,
	Contested,
	Complete
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
	protected static const float HUD_W = 512;
	protected static const float HUD_H = 72;
	protected static const float INTRO_DUR = 0.52;
	protected static const float OUTRO_DUR = 0.38;
	protected static const float SLIDE_FROM = 22;
	protected static const float CAPTURE_SECONDS = 120.0;

	protected IA_CaptureHudAnim m_eAnim;
	protected IA_CaptureHudState m_eShownState;

	protected string m_sShownArea;

	protected float m_fServerProgress;
	protected float m_fPredicted;
	protected float m_fDisplay;
	protected float m_fAnimT;
	protected float m_fRail;
	protected float m_fBarReveal;
	protected float m_fCompleteHold;
	protected bool m_bArmed;

	//------------------------------------------------------------------------------------------------
	void IA_CaptureHud()
	{
		m_Style.m_WidthMode = MUI_SizeMode.Exact;
		m_Style.m_HeightMode = MUI_SizeMode.Exact;
		m_Style.m_fWidth = HUD_W;
		m_Style.m_fHeight = HUD_H;
		m_Style.m_fMinWidth = HUD_W;
		m_Style.m_fMinHeight = HUD_H;
		m_Style.m_fRadius = 10;
		m_Style.m_Fill = Color.FromInt(0);
		m_Style.m_bBlockHit = false;
		m_Style.m_bInteractive = false;
		m_bBlurEnabled = true;
		m_fBlurIntensity = 0.70;
		m_eAnim = IA_CaptureHudAnim.Idle;
		m_eShownState = IA_CaptureHudState.Hidden;
		m_sShownArea = "";
		m_fIntroDuration = 0;
		m_fIntro = 1;
	}

	//------------------------------------------------------------------------------------------------
	static IA_CaptureHud Create(notnull MUI_Runtime runtime)
	{
		ref IA_CaptureHud hud = new IA_CaptureHud();
		runtime.Adopt(hud);
		hud.SetName("sectorHold");
		hud.SetWidth(HUD_W);
		hud.SetHeight(HUD_H);
		hud.SetAlign(0.5, 1.0);
		hud.SetVisible(false);
		return hud;
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
		PullServer();
		TickAnim(dt);
		TickProgress(dt);

		if (m_eAnim != IA_CaptureHudAnim.Idle)
			InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	protected void PullServer()
	{
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		IA_CaptureHudState nextState = IA_CaptureHudState.Hidden;
		string nextArea = "";
		float nextProgress = 0;
		if (init)
		{
			nextState = DecodeState(init.GetCaptureHudState());
			nextArea = init.GetCaptureHudArea();
			nextProgress = init.GetCaptureHudProgress();
		}

		if (nextProgress < 0)
			nextProgress = 0;
		if (nextProgress > 1)
			nextProgress = 1;

		m_fServerProgress = nextProgress;

		if (nextState == IA_CaptureHudState.Hidden)
		{
			if (m_eAnim == IA_CaptureHudAnim.Intro || m_eAnim == IA_CaptureHudAnim.Hold)
				BeginOutro();
			return;
		}

		bool areaChanged = m_sShownArea != nextArea;
		if (areaChanged || !m_bArmed)
			Arm(nextArea, nextState, nextProgress);
		else if (m_eShownState != nextState)
			m_eShownState = nextState;
	}

	//------------------------------------------------------------------------------------------------
	protected IA_CaptureHudState DecodeState(int raw)
	{
		if (raw == IA_CaptureHudState.Capturing)
			return IA_CaptureHudState.Capturing;
		if (raw == IA_CaptureHudState.Paused)
			return IA_CaptureHudState.Paused;
		if (raw == IA_CaptureHudState.Contested)
			return IA_CaptureHudState.Contested;
		if (raw == IA_CaptureHudState.Complete)
			return IA_CaptureHudState.Complete;
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
		m_fRail = 0;
		m_fBarReveal = 0;
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
			m_fSlideY = (1.0 - t) * SLIDE_FROM;
			m_fRail = MUI_Ease.CubicOut(m_fAnimT * 1.35);
			m_fBarReveal = MUI_Ease.CubicOut((m_fAnimT - 0.18) / 0.55);
			if (m_fAnimT < 1)
				return;
			m_eAnim = IA_CaptureHudAnim.Hold;
			m_fIntro = 1;
			m_fSlideY = 0;
			m_fRail = 1;
			m_fBarReveal = 1;
			return;
		}

		if (m_eAnim == IA_CaptureHudAnim.Hold)
		{
			m_fRail = 1;
			m_fBarReveal = 1;
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
		m_fSlideY = u * 16;
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
			m_fPredicted = m_fPredicted + dt / CAPTURE_SECONDS;
			if (m_fServerProgress > m_fPredicted)
				m_fPredicted = m_fServerProgress;
			else if (m_fPredicted - m_fServerProgress > 0.03)
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

		m_fDisplay = MUI_Ease.Approach(m_fDisplay, m_fPredicted, dt, 9);
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
		if (m_eAnim == IA_CaptureHudAnim.Idle)
			return;

		float x = DrawX();
		float y = DrawY();
		float w = m_World.m_fW;
		float h = m_World.m_fH;
		float op = GetDrawOpacity();
		if (op < 0.01)
			return;

		MUI_ThemeData theme = GetTheme();
		Color tone = ResolveTone(theme);

		SyncHostWidgets();
		DrawChrome(surface, x, y, w, h, op, tone, theme);
		DrawCopy(surface, x, y, w, h, op, tone, theme);
		DrawBar(surface, x, y, w, h, op, tone, theme);
	}

	//------------------------------------------------------------------------------------------------
	protected Color ResolveTone(notnull MUI_ThemeData theme)
	{
		if (m_eShownState == IA_CaptureHudState.Contested)
			return theme.Danger;
		if (m_eShownState == IA_CaptureHudState.Paused)
			return theme.Accent;
		if (m_eShownState == IA_CaptureHudState.Complete)
			return theme.Cyan;
		return theme.Live;
	}

	//------------------------------------------------------------------------------------------------
	protected string ResolveKicker()
	{
		if (m_eShownState == IA_CaptureHudState.Complete)
			return "SECTOR SECURE";
		if (m_eShownState == IA_CaptureHudState.Contested)
			return "SECTOR CONTESTED";
		if (m_eShownState == IA_CaptureHudState.Paused)
			return "SECTOR HOLD";
		return "SECTOR HOLD";
	}

	//------------------------------------------------------------------------------------------------
	protected string ResolvePill()
	{
		if (m_eShownState == IA_CaptureHudState.Capturing)
			return "SECURING";
		if (m_eShownState == IA_CaptureHudState.Contested)
			return "CONTACT";
		if (m_eShownState == IA_CaptureHudState.Paused)
			return "HOLD";
		if (m_eShownState == IA_CaptureHudState.Complete)
			return "SECURE";
		return "LIVE";
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawChrome(MUI_RenderSurface surface, float x, float y, float w, float h, float op, Color tone, notnull MUI_ThemeData theme)
	{
		float pulseHz = 0.55;
		if (m_eShownState == IA_CaptureHudState.Capturing)
			pulseHz = 0.85;
		else if (m_eShownState == IA_CaptureHudState.Contested)
			pulseHz = 1.45;

		float glow = 0.10 + 0.10 * MUI_Ease.Pulse(GetTime(), pulseHz);
		surface.FillRect(x - 8, y - 6, w + 16, h + 12, MUI_ColorUtil.Fade(tone, op * glow * 0.28), 16);
		surface.FillRect(x - 2, y - 2, w + 4, h + 4, MUI_ColorUtil.Fade(theme.Cyan, op * 0.05), 12);

		Color fill = theme.DeepFrost;
		if (!m_bBlurEnabled)
			fill = theme.Deep;
		surface.FillRect(x, y, w, h, MUI_ColorUtil.Fade(fill, op), 10);

		float railH = h * m_fRail;
		if (railH > 1)
		{
			surface.FillRect(x, y, 4, railH, MUI_ColorUtil.Fade(tone, op), 0);
			surface.FillRect(x + 4, y, 1, railH, MUI_ColorUtil.Fade(theme.Cyan, op * 0.65), 0);
		}

		surface.FillRect(x, y, w, 2, MUI_ColorUtil.Fade(tone, op), 0);
		surface.FillRect(x, y + 2, w, 1, MUI_ColorUtil.Fade(theme.Cyan, op * 0.55), 0);

		if (m_eShownState == IA_CaptureHudState.Capturing)
		{
			float sweep = MUI_Ease.Fract(GetTime() * 0.28);
			float sx = x + sweep * (w + 90) - 60;
			surface.FillRect(sx, y, 54, 2, MUI_ColorUtil.Fade(theme.Sheen, op), 0);
		}

		surface.StrokeRect(x, y, w, h, MUI_ColorUtil.Fade(theme.Border, op * 0.9), 1.3, 10);

		float br = m_fRail;
		DrawBracket(surface, x + 8, y + 8, 10 * br, 1, 1, op, tone);
		DrawBracket(surface, x + w - 8, y + 8, 10 * br, -1, 1, op, theme.Cyan);
		DrawBracket(surface, x + 8, y + h - 8, 10 * br, 1, -1, op, theme.Cyan);
		DrawBracket(surface, x + w - 8, y + h - 8, 10 * br, -1, -1, op, tone);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawCopy(MUI_RenderSurface surface, float x, float y, float w, float h, float op, Color tone, notnull MUI_ThemeData theme)
	{
		float left = x + 18;
		string kicker = ResolveKicker();
		surface.DrawText(left, y + 7, w * 0.55, 15, kicker, theme.FONT_SMALL, MUI_ColorUtil.Fade(tone, op), true, false, true, false);

		string area = m_sShownArea;
		if (area.IsEmpty())
			area = "UNKNOWN SECTOR";
		surface.DrawText(left, y + 22, w - 160, 22, area, theme.FONT_BODY, MUI_ColorUtil.Fade(theme.Text, op), true, false, true, false);

		DrawPill(surface, x, y, w, op, tone, theme);
		DrawMeta(surface, x, y, w, op, tone, theme);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawPill(MUI_RenderSurface surface, float x, float y, float w, float op, Color tone, notnull MUI_ThemeData theme)
	{
		string pill = ResolvePill();
		float textW = 28;
		float textH = 16;
		if (m_Runtime)
			m_Runtime.MeasureText(pill, theme.FONT_SMALL, true, 0, textW, textH);

		float pillH = 18;
		float pillW = 28 + textW;
		if (pillW < 52)
			pillW = 52;
		float px = x + w - 14 - pillW;
		float py = y + 8;
		float pulse = 0.45 + 0.55 * MUI_Ease.Pulse(GetTime(), 1.25);
		if (m_eShownState == IA_CaptureHudState.Paused)
			pulse = 0.55;
		else if (m_eShownState == IA_CaptureHudState.Complete)
			pulse = 1;

		surface.FillRect(px, py, pillW, pillH, MUI_ColorUtil.Fade(tone, op * 0.16), 7);
		surface.StrokeRect(px, py, pillW, pillH, MUI_ColorUtil.Fade(tone, op * pulse), 1.15, 7);
		surface.FillCircle(px + 9, py + pillH * 0.5, 3.2, MUI_ColorUtil.Fade(tone, op * pulse));
		surface.DrawText(px + 16, py, textW + 4, pillH, pill, theme.FONT_SMALL, MUI_ColorUtil.Fade(tone, op), true, false, true, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawMeta(MUI_RenderSurface surface, float x, float y, float w, float op, Color tone, notnull MUI_ThemeData theme)
	{
		int pct = Math.Round(m_fDisplay * 100);
		if (pct < 0)
			pct = 0;
		if (pct > 100)
			pct = 100;
		string pctText = pct.ToString() + "%";

		string meta = FormatClock();
		if (m_eShownState == IA_CaptureHudState.Paused)
			meta = "PAUSED";
		else if (m_eShownState == IA_CaptureHudState.Contested)
			meta = "CONTESTED";
		else if (m_eShownState == IA_CaptureHudState.Complete)
			meta = "HELD";

		float metaW = 78;
		surface.DrawText(x + w - 14 - metaW, y + 28, metaW, 16, meta, theme.FONT_SMALL, MUI_ColorUtil.Fade(theme.TextMuted, op), false, false, true, false);
		surface.DrawText(x + w - 14 - metaW - 48, y + 26, 44, 18, pctText, theme.FONT_SMALL, MUI_ColorUtil.Fade(theme.Text, op), true, false, true, false);
	}

	//------------------------------------------------------------------------------------------------
	protected string FormatClock()
	{
		float remain = (1.0 - m_fDisplay) * CAPTURE_SECONDS;
		int secs = Math.Ceil(remain);
		if (secs < 0)
			secs = 0;
		int minutes = secs / 60;
		int seconds = secs - minutes * 60;
		string clock = minutes.ToString() + ":";
		if (seconds < 10)
			clock = clock + "0";
		clock = clock + seconds.ToString();
		return clock;
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawBar(MUI_RenderSurface surface, float x, float y, float w, float h, float op, Color tone, notnull MUI_ThemeData theme)
	{
		float reveal = m_fBarReveal;
		if (reveal < 0.02)
			return;

		float bx = x + 18;
		float by = y + h - 16;
		float bw = (w - 36) * reveal;
		float bh = 6;
		surface.FillRect(bx, by, w - 36, bh, MUI_ColorUtil.Fade(theme.Field, op * reveal), 2);

		int tick = 1;
		while (tick < 10)
		{
			float tx = bx + (w - 36) * (tick * 0.1);
			surface.FillRect(tx, by, 1, bh, MUI_ColorUtil.Fade(theme.Border, op * 0.35 * reveal), 0);
			tick = tick + 1;
		}

		float fillW = bw * m_fDisplay;
		if (fillW > 0.8)
		{
			surface.FillRect(bx, by, fillW, bh, MUI_ColorUtil.Fade(tone, op * reveal), 2);
			surface.FillRect(bx, by, fillW, 2, MUI_ColorUtil.Fade(theme.Sheen, op * reveal * 0.45), 0);
			surface.FillRect(bx + fillW - 3, by - 1, 4, bh + 2, MUI_ColorUtil.Fade(theme.Sheen, op * reveal), 0);
		}

		if (m_eShownState == IA_CaptureHudState.Capturing)
		{
			float scan = MUI_Ease.Fract(GetTime() * 0.7);
			float sx = bx + fillW * scan;
			if (fillW > 8)
				surface.FillRect(sx, by, 10, bh, MUI_ColorUtil.Fade(theme.Sheen, op * reveal * 0.55), 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawBracket(MUI_RenderSurface surface, float x, float y, float arm, float dirX, float dirY, float op, Color tone)
	{
		if (arm < 1)
			return;
		Color c = MUI_ColorUtil.Fade(tone, op);
		surface.DrawLine(x, y, x + arm * dirX, y, c, 2.0);
		surface.DrawLine(x, y, x, y + arm * dirY, c, 2.0);
		surface.FillRect(x - 1, y - 1, 3, 3, c, 0);
	}
}
