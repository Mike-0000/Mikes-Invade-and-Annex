//------------------------------------------------------------------------------------------------
//! Dynamic-base capture tile using the existing capture chrome, body-only blur,
//! tracked tab, ring and 0.38s/0.28s slide-fades. No base-only countdowns: capture
//! hands directly to IA_DefendHud. Progress eases authoritative samples, never
//! predicts with the ordinary sector's different/configuration-independent rate.
//------------------------------------------------------------------------------------------------
class IA_BaseObjectiveHud : IA_CaptureHud
{
	protected static const float RAIL_H = 3;
	protected static const int FONT_COPY = 13;
	protected static const int FONT_HINT = 10;
	protected static const float COMPACT_W = 240;

	protected int m_iPhase;
	protected int m_iReason;
	protected int m_iSerial;
	protected int m_iGroupId;
	protected float m_fPhasePulse;
	protected string m_sTitle;
	protected string m_sBody;

	//------------------------------------------------------------------------------------------------
	static IA_BaseObjectiveHud CreateBase(notnull MUI_Runtime runtime)
	{
		ref IA_BaseObjectiveHud hud = new IA_BaseObjectiveHud();
		runtime.Adopt(hud);
		hud.SetName("baseObjective");
		hud.SetVisible(false);
		return hud;
	}

	//------------------------------------------------------------------------------------------------
	override void Abort()
	{
		super.Abort();
		m_iPhase = IA_BaseObjectivePhase.None;
		m_iReason = IA_BaseStatusReason.None;
		m_fPhasePulse = 0;
	}

	//------------------------------------------------------------------------------------------------
	override void OnTick(float dt)
	{
		PullServer();
		super.OnTick(dt);
		if (!IsIdle())
			m_fPhasePulse = MUI_Ease.Approach(m_fPhasePulse, 0, dt, 6);
	}

	//------------------------------------------------------------------------------------------------
	protected void PullServer()
	{
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		IA_BaseHudStatus status = null;
		if (init)
			status = init.GetBaseObjectiveStatus();
		if (!status || !ShowsPhase(status.m_iPhase))
		{
			// Retain the last copy and progress throughout the inherited outro.
			ApplyServer("", IA_CaptureHudState.Hidden, m_fServerProgress);
			return;
		}

		bool newObjective = m_iSerial != status.m_iSerial || m_iGroupId != status.m_iGroupId;
		bool phaseChanged = newObjective || m_iPhase != status.m_iPhase;
		bool changed = phaseChanged || m_iReason != status.m_iReason;
		m_iSerial = status.m_iSerial;
		m_iGroupId = status.m_iGroupId;
		m_iPhase = status.m_iPhase;
		m_iReason = status.m_iReason;

		float progress = MUI_Ease.Clamp01(status.m_iCapturePermille / 1000.0);
		// Capture states drive shared chrome only, not base gameplay semantics.
		IA_CaptureHudState state = IA_CaptureHudState.Capturing;
		if (IsBlocked())
			state = IA_CaptureHudState.Blocked;

		if (phaseChanged || IsIdle())
			m_fDisplay = progress;
		if (changed || IsIdle())
		{
			m_fPhasePulse = 1;
			BuildCopy();
		}
		ApplyServer("Dynamic base", state, progress);
	}

	//------------------------------------------------------------------------------------------------
	protected bool ShowsPhase(int phase)
	{
		return phase == IA_BaseObjectivePhase.Placing || phase == IA_BaseObjectivePhase.Seize || phase == IA_BaseObjectivePhase.Failed;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsBlocked()
	{
		return m_iPhase == IA_BaseObjectivePhase.Failed || m_iReason == IA_BaseStatusReason.ClearCommand || m_iReason == IA_BaseStatusReason.Contested;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildCopy()
	{
		if (m_iPhase == IA_BaseObjectivePhase.Placing)
		{
			m_sTitle = "Locating base";
			m_sBody = "Stand by for coordinates";
		}
		else if (m_iPhase == IA_BaseObjectivePhase.Failed)
		{
			m_sTitle = "Base unavailable";
			m_sBody = "Admin action needed";
		}
		else
		{
			m_sTitle = "Seize base";
			m_sBody = "Secure command area";
			if (IsBlocked())
				m_sBody = "Clear command area";
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected void TickProgress(float dt)
	{
		if (IsIdle() || m_eAnim == IA_CaptureHudAnim.Outro)
			return;
		// Base capture duration is configurable; contested capture pauses rather
		// than resetting to zero or losing progress like an ordinary sector.
		m_fDisplay = MUI_Ease.Approach(m_fDisplay, m_fServerProgress, dt, 9);
	}

	//------------------------------------------------------------------------------------------------
	override protected void TickSpin(float dt)
	{
		if (IsIdle() || m_eAnim == IA_CaptureHudAnim.Outro || IsBlocked())
			return;
		float speed = 180;
		if (m_iPhase == IA_BaseObjectivePhase.Seize)
			speed = 360;
		m_fSpin = MUI_Ease.Fract((m_fSpin + dt * speed) / 360.0) * 360;
	}

	//------------------------------------------------------------------------------------------------
	override protected Color ResolveTone(notnull MUI_ThemeData theme)
	{
		if (IsBlocked())
			return theme.Danger;
		if (m_iPhase == IA_BaseObjectivePhase.Seize)
			return m_HudGreen;
		return m_HudAmber;
	}

	//------------------------------------------------------------------------------------------------
	override protected Color ResolveTabFill()
	{
		if (IsBlocked())
			return m_HudTabLose;
		if (m_iPhase == IA_BaseObjectivePhase.Seize)
			return m_HudTab;
		return m_HudTabHold;
	}

	//------------------------------------------------------------------------------------------------
	override protected string ResolveTab()
	{
		if (m_iPhase == IA_BaseObjectivePhase.Failed)
			return "OFFLINE";
		if (IsBlocked())
			return "CONTESTED";
		if (m_iPhase == IA_BaseObjectivePhase.Placing)
			return "LOCATING";
		return "SECURING";
	}

	//------------------------------------------------------------------------------------------------
	override protected void DrawBody(MUI_RenderSurface surface, float x, float y, float w, float op, Color tone)
	{
		super.DrawBody(surface, x, y, w, op, tone);
		surface.DrawLine(x, y, x + w, y, MUI_ColorUtil.Fade(tone, op * m_fPhasePulse * 0.65), 1);

		float railX = x + PAD_X;
		float railY = y + BODY_H - RAIL_H - 3;
		float railW = w - PAD_X * 2;
		surface.FillRect(railX, railY, railW, RAIL_H, MUI_ColorUtil.Fade(ResolveTabFill(), op), 0);
		if (m_iPhase == IA_BaseObjectivePhase.Seize)
		{
			if (m_fDisplay > 0)
				surface.FillRect(railX, railY, railW * m_fDisplay, RAIL_H, MUI_ColorUtil.Fade(tone, op * 0.90), 0);
		}
		else if (m_iPhase == IA_BaseObjectivePhase.Placing)
		{
			// Indeterminate sweep, not an invented placement percentage/countdown.
			float scan = MUI_Ease.Pulse(GetTime(), 0.65);
			float scanW = railW * 0.22;
			surface.FillRect(railX + (railW - scanW) * scan, railY, scanW, RAIL_H, MUI_ColorUtil.Fade(tone, op * 0.85), 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected void DrawContent(MUI_RenderSurface surface, float x, float y, float w, float op, Color tone)
	{
		float copyX = x + PAD_X;
		if (w >= COMPACT_W)
		{
			DrawSpinner(surface, copyX + SPIN_R, y + 21, op, tone);
			copyX = copyX + SPIN_R * 2 + GAP_ICON;
		}

		float copyRight = x + w - PAD_X;
		if (m_iPhase == IA_BaseObjectivePhase.Seize)
		{
			int pct = Math.Round(m_fDisplay * 100);
			string metric = pct.ToString() + "%";
			float metricW = 48;
			float metricH = 16;
			if (m_Runtime)
				m_Runtime.MeasureText(metric, FONT_PCT, true, 0, metricW, metricH);
			metricW = Math.Max(metricW, 48);
			float divX = copyRight - DIV_W;
			float metricX = divX - GAP_PCT - metricW;
			surface.DrawText(metricX, y + 5, metricW, 18, metric, FONT_PCT, MUI_ColorUtil.Fade(m_HudWhite, op), true, false, true, false, true);
			surface.DrawText(metricX, y + 25, metricW, 12, "CAPTURE", FONT_HINT, MUI_ColorUtil.Fade(tone, op * 0.85), true, false, true, false, true);
			surface.FillRect(divX, y + 9, DIV_W, DIV_H, MUI_ColorUtil.Fade(tone, op * 0.20), 0);
			copyRight = metricX - 8;
		}

		// Narrow multi-objective docks drop the icon and reserve the metric first.
		float copyW = Math.Max(1, copyRight - copyX);
		surface.DrawText(copyX, y + 5, copyW, 18, m_sTitle, FONT_COPY, MUI_ColorUtil.Fade(m_HudWhite, op), true, false, true, false, true);
		surface.DrawText(copyX, y + 25, copyW, 12, m_sBody, FONT_HINT, MUI_ColorUtil.Fade(tone, op * 0.80), false, false, true, false, true);
	}
}
