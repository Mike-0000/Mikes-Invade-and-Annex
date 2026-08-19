//------------------------------------------------------------------------------------------------
//! Custom MUI_Node (I&A-only): command-uplink sector toast.
//! Pattern: Create(runtime) → Adopt inside factory → parent.AddChild(toast). Keep as protected ref.
//! Drives its own intro / hold / outro in OnTick. Paint uses DrawX/Y + GetDrawOpacity().
//------------------------------------------------------------------------------------------------
enum IA_NotificationKind
{
	Generic,
	TaskCreated,
	SideTaskCreated,
	TaskCompleted,
	AreaCompleted,
	Alert,
	Success,
	Promotion
}

enum IA_NotificationAnim
{
	Idle,
	Intro,
	Hold,
	Outro
}

//------------------------------------------------------------------------------------------------
class IA_NotificationToast : MUI_Surface
{
	protected static const float TOAST_W = 820;
	protected static const float TOAST_H = 74;
	protected static const float TOAST_H_MAX = 148;
	protected static const float COPY_TOP = 26;
	protected static const float COPY_BOTTOM = 16;
	protected static const float INTRO_DUR = 0.84;
	protected static const float OUTRO_DUR = 0.46;
	protected static const float SLIDE_FROM = -62;
	protected static const float HOLD_MIN = 0.85;
	protected static const int SPARK_COUNT = 16;
	protected static const int MOTE_COUNT = 10;

	protected ref ScriptInvoker m_OnFinished;
	protected IA_NotificationAnim m_eAnim;
	protected IA_NotificationKind m_eKind;

	protected string m_sKicker;
	protected string m_sPill;
	protected string m_sMessage;
	protected string m_sColor;

	protected float m_fLife;
	protected float m_fIntroDur;
	protected float m_fHoldDur;
	protected float m_fHoldLeft;
	protected float m_fOutroT;
	protected float m_fLock;
	protected float m_fRail;
	protected float m_fKickerT;
	protected float m_fMessageT;
	protected float m_fBarT;

	protected ref array<float> m_aSparkX;
	protected ref array<float> m_aSparkY;
	protected ref array<float> m_aSparkVX;
	protected ref array<float> m_aSparkVY;
	protected ref array<float> m_aSparkLife;
	protected ref array<float> m_aMoteX;
	protected ref array<float> m_aMoteY;
	protected ref array<float> m_aMoteV;
	protected ref array<float> m_aMoteS;
	protected ref array<float> m_aMotePhase;

	//------------------------------------------------------------------------------------------------
	void IA_NotificationToast()
	{
		m_OnFinished = new ScriptInvoker();
		m_aSparkX = new array<float>();
		m_aSparkY = new array<float>();
		m_aSparkVX = new array<float>();
		m_aSparkVY = new array<float>();
		m_aSparkLife = new array<float>();
		m_aMoteX = new array<float>();
		m_aMoteY = new array<float>();
		m_aMoteV = new array<float>();
		m_aMoteS = new array<float>();
		m_aMotePhase = new array<float>();

		m_Style.m_WidthMode = MUI_SizeMode.Exact;
		m_Style.m_HeightMode = MUI_SizeMode.Exact;
		m_Style.m_fWidth = TOAST_W;
		m_Style.m_fHeight = TOAST_H;
		m_Style.m_fMinWidth = TOAST_W;
		m_Style.m_fMinHeight = TOAST_H;
		m_Style.m_fRadius = 10;
		m_Style.m_Fill = Color.FromInt(0);
		m_Style.m_bBlockHit = false;
		m_Style.m_bInteractive = false;
		m_bBlurEnabled = true;
		m_fBlurIntensity = 0.74;
		m_eAnim = IA_NotificationAnim.Idle;
		m_eKind = IA_NotificationKind.Generic;
		m_sKicker = "COMMAND UPLINK";
		m_sPill = "LIVE";
		m_sMessage = "";
		m_sColor = "";
		m_fIntroDur = INTRO_DUR;
	}

	//------------------------------------------------------------------------------------------------
	static IA_NotificationToast Create(notnull MUI_Runtime runtime)
	{
		ref IA_NotificationToast toast = new IA_NotificationToast();
		runtime.Adopt(toast);
		toast.SetName("sectorToast");
		toast.SetWidth(TOAST_W);
		toast.SetHeight(TOAST_H);
		toast.SetAlign(0.5, 0.0);
		toast.SetVisible(false);
		return toast;
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyTheme(notnull MUI_ThemeData theme)
	{
		m_Style.m_Fill = Color.FromInt(0);
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnFinished()
	{
		return m_OnFinished;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPlaying()
	{
		return m_eAnim != IA_NotificationAnim.Idle;
	}

	//------------------------------------------------------------------------------------------------
	IA_NotificationKind GetKind()
	{
		return m_eKind;
	}

	//------------------------------------------------------------------------------------------------
	//! @param message Body copy (prefixes like "New Objective:" are stripped).
	//! @param color Legacy tone key: red / yellow / green / empty.
	//! @param durationMs Total on-screen time including intro and outro.
	//! @param kind Presentation preset (kicker, pill, motion energy).
	void Present(string message, string color, int durationMs, IA_NotificationKind kind)
	{
		m_eKind = kind;
		m_sColor = color;
		m_sMessage = StripKnownPrefix(message);
		m_sKicker = ResolveKicker(kind);
		m_sPill = ResolvePill(kind);

		m_fIntroDur = INTRO_DUR;
		if (IsAlertKind())
			m_fIntroDur = 0.62;
		else if (kind == IA_NotificationKind.AreaCompleted)
			m_fIntroDur = 0.98;
		else if (kind == IA_NotificationKind.Success)
			m_fIntroDur = 0.78;
		else if (kind == IA_NotificationKind.Promotion)
			m_fIntroDur = 0.88;

		float total = durationMs;
		total = total * 0.001;
		m_fHoldDur = total - m_fIntroDur - OUTRO_DUR;
		if (m_fHoldDur < HOLD_MIN)
			m_fHoldDur = HOLD_MIN;
		m_fHoldLeft = m_fHoldDur;

		m_fLife = 0;
		m_fOutroT = 0;
		m_fLock = 0;
		m_fRail = 0;
		m_fKickerT = 0;
		m_fMessageT = 0;
		m_fBarT = 0;
		m_fIntroDuration = 0;
		m_fIntro = 0;
		m_fSlideY = SLIDE_FROM;
		m_eAnim = IA_NotificationAnim.Intro;

		FitHeightToMessage();
		SeedSparks();
		SeedMotes();
		PlayRipple();
		SetVisible(true);
		InvalidateLayout();
	}

	//------------------------------------------------------------------------------------------------
	void Dismiss()
	{
		if (m_eAnim == IA_NotificationAnim.Idle)
			return;
		if (m_eAnim == IA_NotificationAnim.Outro)
			return;
		m_eAnim = IA_NotificationAnim.Outro;
		m_fOutroT = 0;
	}

	//------------------------------------------------------------------------------------------------
	void Abort()
	{
		m_eAnim = IA_NotificationAnim.Idle;
		m_fIntro = 1;
		m_fSlideY = 0;
		m_sMessage = "";
		SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnTick(float dt)
	{
		super.OnTick(dt);
		if (m_eAnim == IA_NotificationAnim.Idle)
			return;

		m_fLife = m_fLife + dt;
		TickSparks(dt);
		TickMotes(dt);

		if (m_eAnim == IA_NotificationAnim.Intro)
			TickIntro();
		else if (m_eAnim == IA_NotificationAnim.Hold)
			TickHold(dt);
		else
			TickOutro(dt);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickIntro()
	{
		float dur = m_fIntroDur;
		if (dur < 0.1)
			dur = INTRO_DUR;
		float t = MUI_Ease.Clamp01(m_fLife / dur);
		m_fIntro = MUI_Ease.BackOut(t);
		m_fSlideY = (1.0 - m_fIntro) * SLIDE_FROM;
		m_fLock = MUI_Ease.CubicOut(Gate(t, 0.0, 0.26));
		m_fRail = MUI_Ease.CubicOut(Gate(t, 0.18, 0.38));
		m_fKickerT = MUI_Ease.CubicOut(Gate(t, 0.28, 0.42));
		m_fMessageT = MUI_Ease.CubicOut(Gate(t, 0.44, 0.48));
		m_fBarT = MUI_Ease.CubicOut(Gate(t, 0.52, 0.32));
		if (m_fLife < dur)
			return;
		m_eAnim = IA_NotificationAnim.Hold;
		m_fIntro = 1;
		m_fSlideY = 0;
		m_fLock = 1;
		m_fRail = 1;
		m_fKickerT = 1;
		m_fMessageT = 1;
		m_fBarT = 1;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickHold(float dt)
	{
		m_fIntro = 1;
		m_fSlideY = 0;
		m_fHoldLeft = m_fHoldLeft - dt;
		if (m_fHoldLeft > 0)
			return;
		m_fHoldLeft = 0;
		m_eAnim = IA_NotificationAnim.Outro;
		m_fOutroT = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickOutro(float dt)
	{
		m_fOutroT = m_fOutroT + dt / OUTRO_DUR;
		float t = MUI_Ease.CubicIn(m_fOutroT);
		m_fIntro = 1.0 - t;
		m_fSlideY = t * -44;
		m_fLock = 1.0 - t;
		if (m_fOutroT < 1)
			return;

		m_eAnim = IA_NotificationAnim.Idle;
		m_fIntro = 1;
		m_fSlideY = 0;
		m_sMessage = "";
		SetVisible(false);
		if (m_OnFinished)
			m_OnFinished.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
		if (m_eAnim == IA_NotificationAnim.Idle)
			return;

		float x = DrawX();
		float y = DrawY();
		float w = m_World.m_fW;
		float h = m_World.m_fH;
		float restY = y - GetSlideY();
		float panelOp = GetDrawOpacity();

		MUI_ThemeData theme = GetTheme();
		Color tone = ResolveTone(theme);

		DrawLockLine(surface, x, restY, w, tone, theme);
		if (panelOp < 0.01)
			return;

		SyncHostWidgets();
		DrawChrome(surface, x, y, w, h, panelOp, tone, theme);
		DrawSparks(surface, x, y, panelOp, ResolveParticleColor(theme, tone));
		DrawMotes(surface, x, y, panelOp, theme);
		DrawCopy(surface, x, y, w, h, panelOp, tone, theme);
		DrawDurationBar(surface, x, y, w, h, panelOp, tone, theme);
		DrawRipple(surface, x, y, w, h, panelOp, tone);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawLockLine(MUI_RenderSurface surface, float x, float restY, float w, Color tone, notnull MUI_ThemeData theme)
	{
		float lock = m_fLock;
		if (lock < 0.01)
			return;

		float lineW = w * lock;
		float lineX = x + (w - lineW) * 0.5;
		float ly = restY;
		float glow = 0.35 + 0.25 * MUI_Ease.Pulse(GetTime(), 1.6);
		surface.FillRect(lineX, ly, lineW, 3, MUI_ColorUtil.Fade(tone, lock), 0);
		surface.FillRect(lineX, ly + 3, lineW, 1, MUI_ColorUtil.Fade(theme.Cyan, lock * 0.65), 0);
		surface.FillRect(lineX, ly, 8, 3, MUI_ColorUtil.Fade(theme.Sheen, lock * glow), 0);
		surface.FillRect(lineX + lineW - 8, ly, 8, 3, MUI_ColorUtil.Fade(theme.Sheen, lock * glow), 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawChrome(MUI_RenderSurface surface, float x, float y, float w, float h, float op, Color tone, notnull MUI_ThemeData theme)
	{
		float pulseHz = 0.7;
		if (IsAlertKind())
			pulseHz = 1.35;
		float glow = 0.10 + 0.10 * MUI_Ease.Pulse(GetTime(), pulseHz);
		if (IsAlertKind())
			glow = glow + 0.06 * MUI_Ease.Pulse(GetTime(), 2.6);

		surface.FillRect(x - 6, y - 4, w + 12, h + 8, MUI_ColorUtil.Fade(tone, op * glow * 0.32), 16);
		surface.FillRect(x - 2, y - 2, w + 4, h + 4, MUI_ColorUtil.Fade(theme.Cyan, op * 0.04), 14);

		Color fill = theme.DeepFrost;
		if (!m_bBlurEnabled)
			fill = theme.Deep;
		surface.FillRect(x, y, w, h, MUI_ColorUtil.Fade(fill, op), 10);

		float flash = Gate(m_fLife, 0.0, 0.07);
		if (m_fLife > 0.07)
			flash = 1.0 - Gate(m_fLife, 0.07, 0.28);
		if (m_eAnim == IA_NotificationAnim.Outro)
			flash = 0;
		if (flash > 0.02)
			surface.FillRect(x, y, w, h, MUI_ColorUtil.Fade(theme.Sheen, op * flash * 0.42), 10);

		float railH = h * m_fRail;
		if (railH > 1)
		{
			surface.FillRect(x, y, 5, railH, MUI_ColorUtil.Fade(tone, op), 0);
			surface.FillRect(x + 5, y, 1, railH, MUI_ColorUtil.Fade(theme.Cyan, op * 0.7), 0);
		}

		surface.FillRect(x, y, w, 3, MUI_ColorUtil.Fade(tone, op), 0);
		surface.FillRect(x, y + 3, w, 1, MUI_ColorUtil.Fade(theme.Cyan, op * 0.65), 0);

		float scan = MUI_Ease.Fract(GetTime() * 0.42);
		surface.FillRect(x + 8, y + 8 + (h - 16) * scan, w - 16, 1, MUI_ColorUtil.Fade(theme.Cyan, op * 0.10), 0);

		surface.StrokeRect(x, y, w, h, MUI_ColorUtil.Fade(theme.Border, op * 0.92), 1.4, 10);

		float br = MUI_Ease.CubicOut(Gate(m_fLife, 0.18, 0.28));
		if (m_eAnim == IA_NotificationAnim.Outro)
			br = m_fLock;
		DrawBracket(surface, x + 8, y + 8, 11 * br, 1, 1, op, tone);
		DrawBracket(surface, x + w - 8, y + 8, 11 * br, -1, 1, op, theme.Cyan);
		DrawBracket(surface, x + 8, y + h - 8, 11 * br, 1, -1, op, theme.Cyan);
		DrawBracket(surface, x + w - 8, y + h - 8, 11 * br, -1, -1, op, tone);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawCopy(MUI_RenderSurface surface, float x, float y, float w, float h, float op, Color tone, notnull MUI_ThemeData theme)
	{
		float left = x + 22;
		float copyW = w - 128;
		float kickerOp = op * m_fKickerT;
		string kicker = Reveal(m_sKicker, m_fKickerT);
		if (kickerOp > 0.02 && !kicker.IsEmpty())
			surface.DrawText(left, y + 6, copyW, 16, kicker, theme.FONT_SMALL, MUI_ColorUtil.Fade(tone, kickerOp), true, false, true, false);

		int i;
		for (i = 0; i < 3; i++)
		{
			float tick = Gate(m_fLife, 0.20 + i * 0.05, 0.10);
			if (m_eAnim == IA_NotificationAnim.Outro)
				tick = m_fLock;
			float tw = 5 + i;
			surface.FillRect(x + 12, y + 10 + i * 4, tw, 2, MUI_ColorUtil.Fade(tone, op * tick), 0);
		}

		float msgOp = op * m_fMessageT;
		string body = Reveal(m_sMessage, m_fMessageT);
		float wipe = 40 + (copyW - 40) * m_fMessageT;
		float msgH = h - COPY_TOP - COPY_BOTTOM;
		if (msgH < 18)
			msgH = 18;
		if (msgOp > 0.02 && !body.IsEmpty())
			surface.DrawText(left, y + COPY_TOP, wipe, msgH, body, theme.FONT_BODY, MUI_ColorUtil.Fade(theme.Text, msgOp), true, false, false, true);

		DrawPill(surface, x, y, w, op, tone, theme);
		DrawUplinkArc(surface, x, y, w, h, op, tone, theme);
	}

	//------------------------------------------------------------------------------------------------
	protected float MeasurePillWidth(notnull MUI_ThemeData theme)
	{
		float textW = 24;
		float textH = 16;
		if (m_Runtime)
			m_Runtime.MeasureText(m_sPill, theme.FONT_SMALL, true, 0, textW, textH);
		float pillW = 26 + textW;
		if (pillW < 44)
			pillW = 44;
		return pillW;
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawPill(MUI_RenderSurface surface, float x, float y, float w, float op, Color tone, notnull MUI_ThemeData theme)
	{
		float appear = MUI_Ease.CubicOut(Gate(m_fLife, 0.34, 0.22));
		if (m_eAnim == IA_NotificationAnim.Outro)
			appear = m_fLock;
		float pillOp = op * appear;
		if (pillOp < 0.02)
			return;

		float hz = 1.4;
		if (IsAlertKind())
			hz = 2.2;
		float pulse = 0.40 + 0.60 * MUI_Ease.Pulse(GetTime(), hz);
		float textW = 24;
		float textH = 16;
		if (m_Runtime)
			m_Runtime.MeasureText(m_sPill, theme.FONT_SMALL, true, 0, textW, textH);
		float pillH = 18;
		float pillW = MeasurePillWidth(theme);
		float px = x + w - pillW - 14;
		float py = y + 8;
		surface.FillRect(px, py, pillW, pillH, MUI_ColorUtil.Fade(tone, pillOp * 0.16), 6);
		surface.StrokeRect(px, py, pillW, pillH, MUI_ColorUtil.Fade(tone, pillOp * pulse), 1.2, 6);
		surface.FillCircle(px + 10, py + pillH * 0.5, 3.5, MUI_ColorUtil.Fade(tone, pillOp * pulse));
		surface.DrawText(px + 18, py, textW + 2, pillH, m_sPill, theme.FONT_SMALL, MUI_ColorUtil.Fade(tone, pillOp), true, false, true, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawUplinkArc(MUI_RenderSurface surface, float x, float y, float w, float h, float op, Color tone, notnull MUI_ThemeData theme)
	{
		float appear = m_fBarT;
		if (appear < 0.02)
			return;
		float pillW = MeasurePillWidth(theme);
		float cx = x + w - 14 - pillW - 16;
		float cy = y + 17;
		float spin = GetTime() * 42;
		if (IsAlertKind())
			spin = GetTime() * 70;
		surface.StrokeCircle(cx, cy, 7, MUI_ColorUtil.Fade(theme.CyanDim, op * appear * 0.7), 1);
		surface.DrawArc(cx, cy, 7, spin, 70, MUI_ColorUtil.Fade(tone, op * appear), 1.6);
		surface.DrawArc(cx, cy, 11, -spin * 0.7, 42, MUI_ColorUtil.Fade(theme.Cyan, op * appear * 0.55), 1.2);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawDurationBar(MUI_RenderSurface surface, float x, float y, float w, float h, float op, Color tone, notnull MUI_ThemeData theme)
	{
		if (m_fBarT < 0.02)
			return;

		float bx = x + 22;
		float by = y + h - 10;
		float bw = w - 44;
		float bh = 3;
		float remain = 1;
		if (m_eAnim == IA_NotificationAnim.Intro)
			remain = m_fBarT;
		else if (m_eAnim == IA_NotificationAnim.Hold)
		{
			if (m_fHoldDur > 0.001)
				remain = m_fHoldLeft / m_fHoldDur;
		}
		else
			remain = 0;

		remain = MUI_Ease.Clamp01(remain);
		surface.FillRect(bx, by, bw, bh, MUI_ColorUtil.Fade(theme.Field, op * m_fBarT), 0);
		float fillW = bw * remain * m_fBarT;
		if (fillW > 0.75)
			surface.FillRect(bx, by, fillW, bh, MUI_ColorUtil.Fade(tone, op * m_fBarT), 0);
		surface.FillRect(bx + fillW - 2, by - 1, 4, bh + 2, MUI_ColorUtil.Fade(theme.Sheen, op * m_fBarT), 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawRipple(MUI_RenderSurface surface, float x, float y, float w, float h, float op, Color tone)
	{
		float ripple = GetRipple();
		if (ripple <= 0)
			return;
		if (ripple >= 1)
			return;
		float cx = x + 18;
		float cy = y + h * 0.5;
		float rr = 12 + ripple * (w * 0.55);
		surface.StrokeCircle(cx, cy, rr, MUI_ColorUtil.Fade(tone, op * (1.0 - ripple) * 0.45), 2.0);
		surface.StrokeCircle(cx, cy, rr * 0.62, MUI_ColorUtil.Fade(tone, op * (1.0 - ripple) * 0.25), 1.2);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawSparks(MUI_RenderSurface surface, float x, float y, float op, Color tone)
	{
		int i;
		for (i = 0; i < m_aSparkLife.Count(); i++)
		{
			float life = m_aSparkLife[i];
			if (life <= 0)
				continue;
			float a = life * op;
			if (a < 0.02)
				continue;
			surface.FillCircle(x + m_aSparkX[i], y + m_aSparkY[i], 1.6 + life * 1.8, MUI_ColorUtil.Fade(tone, a));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawMotes(MUI_RenderSurface surface, float x, float y, float op, notnull MUI_ThemeData theme)
	{
		if (!WantsMotes())
			return;
		float t = GetTime();
		int i;
		for (i = 0; i < m_aMoteX.Count(); i++)
		{
			float pulse = 0.40 + 0.60 * Math.Sin(t * 2.1 + m_aMotePhase[i]);
			if (pulse < 0)
				pulse = 0;
			surface.FillCircle(x + m_aMoteX[i], y + m_aMoteY[i], m_aMoteS[i], MUI_ColorUtil.Fade(theme.Live, op * pulse * m_fBarT));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawBracket(MUI_RenderSurface surface, float x, float y, float arm, float dirX, float dirY, float op, Color tone)
	{
		if (arm < 1)
			return;
		Color c = MUI_ColorUtil.Fade(tone, op);
		surface.DrawLine(x, y, x + arm * dirX, y, c, 2.2);
		surface.DrawLine(x, y, x, y + arm * dirY, c, 2.2);
		surface.FillRect(x - 1, y - 1, 3, 3, c, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickSparks(float dt)
	{
		int i;
		for (i = 0; i < m_aSparkLife.Count(); i++)
		{
			if (m_aSparkLife[i] <= 0)
				continue;
			m_aSparkX[i] = m_aSparkX[i] + m_aSparkVX[i] * dt;
			m_aSparkY[i] = m_aSparkY[i] + m_aSparkVY[i] * dt;
			m_aSparkVY[i] = m_aSparkVY[i] + 70 * dt;
			m_aSparkLife[i] = m_aSparkLife[i] - dt * 1.15;
			if (m_aSparkLife[i] < 0)
				m_aSparkLife[i] = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TickMotes(float dt)
	{
		if (!WantsMotes())
			return;
		float h = m_World.m_fH;
		float w = m_World.m_fW;
		if (h < 8)
			return;
		int i;
		for (i = 0; i < m_aMoteY.Count(); i++)
		{
			m_aMoteY[i] = m_aMoteY[i] - m_aMoteV[i] * dt;
			if (m_aMoteY[i] < -6)
			{
				m_aMoteY[i] = h + 6;
				m_aMoteX[i] = Math.RandomFloat(18, w - 18);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SeedSparks()
	{
		m_aSparkX.Clear();
		m_aSparkY.Clear();
		m_aSparkVX.Clear();
		m_aSparkVY.Clear();
		m_aSparkLife.Clear();

		int extra = 0;
		if (m_eKind == IA_NotificationKind.AreaCompleted)
			extra = 6;
		else if (m_eKind == IA_NotificationKind.Success)
			extra = 4;
		else if (m_eKind == IA_NotificationKind.Promotion)
			extra = 10;
		else if (IsAlertKind())
			extra = 3;

		int count = SPARK_COUNT + extra;
		int i;
		for (i = 0; i < count; i++)
		{
			m_aSparkX.Insert(18 + Math.RandomFloat(0, 26));
			m_aSparkY.Insert(TOAST_H * 0.35 + Math.RandomFloat(-16, 16));
			m_aSparkVX.Insert(Math.RandomFloat(40, 220));
			m_aSparkVY.Insert(Math.RandomFloat(-90, 20));
			m_aSparkLife.Insert(Math.RandomFloat(0.45, 1.0));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SeedMotes()
	{
		m_aMoteX.Clear();
		m_aMoteY.Clear();
		m_aMoteV.Clear();
		m_aMoteS.Clear();
		m_aMotePhase.Clear();
		if (!WantsMotes())
			return;

		int i;
		for (i = 0; i < MOTE_COUNT; i++)
		{
			m_aMoteX.Insert(Math.RandomFloat(24, TOAST_W - 24));
			m_aMoteY.Insert(Math.RandomFloat(8, TOAST_H - 8));
			m_aMoteV.Insert(Math.RandomFloat(10, 26));
			m_aMoteS.Insert(Math.RandomFloat(1.6, 3.2));
			m_aMotePhase.Insert(Math.RandomFloat(0, Math.PI2));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void FitHeightToMessage()
	{
		MUI_ThemeData theme = GetTheme();
		float lineH = theme.FONT_BODY + 4;
		float th = lineH;
		if (m_Runtime && !m_sMessage.IsEmpty())
		{
			float tw;
			m_Runtime.MeasureText(m_sMessage, theme.FONT_BODY, true, TOAST_W - 140, tw, th);
			if (th < lineH)
				th = lineH;

			float lineCap = lineH * 2;
			if (m_sMessage.IndexOf("\n") != -1)
				lineCap = lineH * 4;
			if (th > lineCap)
				th = lineCap;
		}

		float h = COPY_TOP + th + COPY_BOTTOM;
		if (h < TOAST_H)
			h = TOAST_H;
		if (h > TOAST_H_MAX)
			h = TOAST_H_MAX;
		SetHeight(h);
		m_Style.m_fMinHeight = h;
	}

	//------------------------------------------------------------------------------------------------
	protected float Gate(float time, float start, float duration)
	{
		if (duration <= 0.001)
		{
			if (time >= start)
				return 1;
			return 0;
		}
		return MUI_Ease.Clamp01((time - start) / duration);
	}

	//------------------------------------------------------------------------------------------------
	protected string Reveal(string src, float t)
	{
		if (src.IsEmpty())
			return "";
		t = MUI_Ease.Clamp01(t);
		if (t <= 0)
			return "";
		if (t >= 1)
			return src;
		int n = src.Length();
		int count = n * t;
		if (count < 1)
			count = 1;
		if (count > n)
			count = n;
		return src.Substring(0, count);
	}

	//------------------------------------------------------------------------------------------------
	protected string StripKnownPrefix(string message)
	{
		string stripped = StripIfPrefixed(message, "New Side Objective: ");
		stripped = StripIfPrefixed(stripped, "New Objective: ");
		stripped = StripIfPrefixed(stripped, "Objective Completed: ");
		stripped = StripIfPrefixed(stripped, "Objective Area Completed, ");
		return stripped;
	}

	//------------------------------------------------------------------------------------------------
	protected string StripIfPrefixed(string message, string prefix)
	{
		if (message.IndexOf(prefix) != 0)
			return message;
		int len = message.Length() - prefix.Length();
		if (len < 1)
			return message;
		return message.Substring(prefix.Length(), len);
	}

	//------------------------------------------------------------------------------------------------
	protected string ResolveKicker(IA_NotificationKind kind)
	{
		if (kind == IA_NotificationKind.TaskCreated)
			return "SECTOR ALERT";
		if (kind == IA_NotificationKind.SideTaskCreated)
			return "SIDE TASKING";
		if (kind == IA_NotificationKind.TaskCompleted)
			return "OBJECTIVE COMPLETE";
		if (kind == IA_NotificationKind.AreaCompleted)
			return "SECTOR COMPLETE";
		if (kind == IA_NotificationKind.Alert)
			return "PRIORITY ALERT";
		if (kind == IA_NotificationKind.Success)
			return "UPLINK CONFIRMED";
		if (kind == IA_NotificationKind.Promotion)
			return "PROMOTION";
		return "COMMAND UPLINK";
	}

	//------------------------------------------------------------------------------------------------
	protected string ResolvePill(IA_NotificationKind kind)
	{
		if (kind == IA_NotificationKind.TaskCreated)
			return "NEW";
		if (kind == IA_NotificationKind.SideTaskCreated)
			return "SIDE";
		if (kind == IA_NotificationKind.TaskCompleted)
			return "DONE";
		if (kind == IA_NotificationKind.AreaCompleted)
			return "RTB";
		if (kind == IA_NotificationKind.Alert)
			return "WARN";
		if (kind == IA_NotificationKind.Success)
			return "LIVE";
		if (kind == IA_NotificationKind.Promotion)
			return "RANK";
		return "UPLINK";
	}

	//------------------------------------------------------------------------------------------------
	protected Color ResolveTone(notnull MUI_ThemeData theme)
	{
		if (m_eKind == IA_NotificationKind.TaskCreated)
			return theme.Danger;
		if (m_eKind == IA_NotificationKind.SideTaskCreated)
			return theme.Danger;
		if (m_eKind == IA_NotificationKind.Alert)
			return theme.Danger;
		if (m_eKind == IA_NotificationKind.TaskCompleted)
			return theme.Accent;
		if (m_eKind == IA_NotificationKind.AreaCompleted)
			return theme.Live;
		if (m_eKind == IA_NotificationKind.Success)
			return theme.Live;
		if (m_eKind == IA_NotificationKind.Promotion)
			return theme.Accent;
		if (m_sColor == "red")
			return theme.Danger;
		if (m_sColor == "yellow")
			return theme.Accent;
		if (m_sColor == "green")
			return theme.Live;
		return theme.Cyan;
	}

	//------------------------------------------------------------------------------------------------
	protected Color ResolveParticleColor(notnull MUI_ThemeData theme, Color tone)
	{
		if (WantsMotes())
			return theme.Live;
		return tone;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAlertKind()
	{
		if (m_eKind == IA_NotificationKind.TaskCreated)
			return true;
		if (m_eKind == IA_NotificationKind.SideTaskCreated)
			return true;
		if (m_eKind == IA_NotificationKind.Alert)
			return true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool WantsMotes()
	{
		if (m_eKind == IA_NotificationKind.AreaCompleted)
			return true;
		if (m_eKind == IA_NotificationKind.Success)
			return true;
		if (m_eKind == IA_NotificationKind.Promotion)
			return true;
		if (m_eKind == IA_NotificationKind.TaskCompleted)
			return true;
		return false;
	}
}
