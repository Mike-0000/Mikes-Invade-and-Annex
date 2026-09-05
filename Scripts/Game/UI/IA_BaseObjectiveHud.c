//------------------------------------------------------------------------------------------------
//! Global seize/regroup/warning tile. Hidden once the existing defend HUD owns
//! the hold. Server remaining seconds are authoritative.
//------------------------------------------------------------------------------------------------
class IA_BaseObjectiveHud : MUI_Surface
{
	protected static const float HUD_W = 288;
	protected static const float HUD_H = 66;
	protected static const float PAD_X = 16;
	protected static const int FONT_TITLE = 13;
	protected static const int FONT_BODY = 11;

	protected int m_iPhase;
	protected int m_iReason;
	protected int m_iRemain;
	protected int m_iPresent;
	protected int m_iTarget;
	protected int m_iCapturePermille;
	protected string m_sTitle;
	protected string m_sBody;
	protected ref Color m_HudBg;
	protected ref Color m_HudAmber;
	protected ref Color m_HudWhite;
	protected ref Color m_HudGreen;
	protected ref Color m_HudFail;
	protected ref array<float> m_aBodyPoly;

	//------------------------------------------------------------------------------------------------
	void IA_BaseObjectiveHud()
	{
		m_Style.m_WidthMode = MUI_SizeMode.Exact;
		m_Style.m_HeightMode = MUI_SizeMode.Exact;
		m_Style.m_fWidth = HUD_W;
		m_Style.m_fHeight = HUD_H;
		m_Style.m_fMinWidth = HUD_W;
		m_Style.m_fMinHeight = HUD_H;
		m_Style.m_Fill = Color.FromInt(0);
		m_Style.m_bBlockHit = false;
		m_Style.m_bInteractive = false;
		m_bBlurEnabled = true;
		m_fBlurIntensity = 0.90;
		m_HudBg = Color.FromSRGBA(13, 20, 18, 230);
		m_HudAmber = Color.FromSRGBA(240, 180, 70, 255);
		m_HudWhite = Color.FromSRGBA(255, 255, 255, 255);
		m_HudGreen = Color.FromSRGBA(94, 251, 131, 255);
		m_HudFail = Color.FromSRGBA(240, 90, 70, 255);
		m_aBodyPoly = new array<float>();
		m_sTitle = "";
		m_sBody = "";
	}

	//------------------------------------------------------------------------------------------------
	static IA_BaseObjectiveHud Create(notnull MUI_Runtime runtime)
	{
		ref IA_BaseObjectiveHud hud = new IA_BaseObjectiveHud();
		runtime.Adopt(hud);
		hud.SetName("baseObjective");
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
	void Abort()
	{
		m_iPhase = IA_BaseObjectivePhase.None;
		m_sTitle = "";
		m_sBody = "";
		SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnTick(float dt)
	{
		super.OnTick(dt);
		PullServer();
	}

	//------------------------------------------------------------------------------------------------
	protected void PullServer()
	{
		IA_MissionInitializer init = IA_MissionInitializer.GetInstance();
		IA_BaseHudStatus status = null;
		if (init)
			status = init.GetBaseObjectiveStatus();

		int phase = IA_BaseObjectivePhase.None;
		if (status)
			phase = status.m_iPhase;

		bool show = false;
		if (phase == IA_BaseObjectivePhase.Seize)
			show = true;
		else if (phase == IA_BaseObjectivePhase.Regroup)
			show = true;
		else if (phase == IA_BaseObjectivePhase.Warning)
			show = true;
		else if (phase == IA_BaseObjectivePhase.Failed)
			show = true;
		else if (phase == IA_BaseObjectivePhase.Placing)
			show = true;

		if (!show)
		{
			if (IsVisible())
				SetVisible(false);
			return;
		}

		m_iPhase = phase;
		m_iReason = 0;
		m_iRemain = 0;
		m_iPresent = 0;
		m_iTarget = 0;
		m_iCapturePermille = 0;
		if (status)
		{
			m_iReason = status.m_iReason;
			m_iRemain = status.m_iRemainingSec;
			m_iPresent = status.m_iEligiblePresent;
			m_iTarget = status.m_iTarget;
			m_iCapturePermille = status.m_iCapturePermille;
		}

		BuildCopy();
		if (!IsVisible())
			SetVisible(true);
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildCopy()
	{
		if (m_iPhase == IA_BaseObjectivePhase.Placing)
		{
			m_sTitle = "LOCATING BASE";
			m_sBody = "Validating an operating base site.";
			return;
		}
		if (m_iPhase == IA_BaseObjectivePhase.Failed)
		{
			m_sTitle = "BASE UNAVAILABLE";
			m_sBody = "Objective unavailable — admin action needed.";
			return;
		}
		if (m_iReason == IA_BaseStatusReason.ClearCommand)
		{
			m_sTitle = "COMMAND CONTESTED";
			m_sBody = "Clear the command area.";
			return;
		}
		if (m_iPhase == IA_BaseObjectivePhase.Seize)
		{
			m_sTitle = "SEIZE BASE";
			int pct = Math.Round(m_iCapturePermille / 10.0);
			m_sBody = "Secure the command area  " + pct.ToString() + "%";
			return;
		}
		if (m_iPhase == IA_BaseObjectivePhase.Warning)
		{
			m_sTitle = "COUNTERATTACK";
			m_sBody = "Inbound in " + m_iRemain.ToString() + "s";
			return;
		}

		m_sTitle = "REGROUP";
		if (m_iReason == IA_BaseStatusReason.AwaitingForces)
			m_sBody = "Awaiting friendly forces";
		else
		{
			m_sBody = m_iPresent.ToString() + "/" + m_iTarget.ToString() + " assembled";
			if (m_iRemain > 0)
				m_sBody = m_sBody + "  " + m_iRemain.ToString() + "s";
		}
	}

	//------------------------------------------------------------------------------------------------
	override void PaintForeground(MUI_RenderSurface surface)
	{
		if (!IsVisible())
			return;

		float x = DrawX();
		float y = DrawY();
		float w = m_Style.m_fWidth;
		float h = m_Style.m_fHeight;
		float op = GetDrawOpacity();

		m_aBodyPoly.Clear();
		m_aBodyPoly.Insert(x);
		m_aBodyPoly.Insert(y);
		m_aBodyPoly.Insert(x + w);
		m_aBodyPoly.Insert(y);
		m_aBodyPoly.Insert(x + w);
		m_aBodyPoly.Insert(y + h);
		m_aBodyPoly.Insert(x);
		m_aBodyPoly.Insert(y + h);
		surface.FillPolygon(m_aBodyPoly, MUI_ColorUtil.Fade(m_HudBg, op));

		Color titleCol = m_HudAmber;
		if (m_iPhase == IA_BaseObjectivePhase.Failed)
			titleCol = m_HudFail;
		else if (m_iPhase == IA_BaseObjectivePhase.Warning)
			titleCol = m_HudGreen;

		surface.DrawText(x + PAD_X, y + 8, w - (PAD_X * 2), 22, m_sTitle, FONT_TITLE, MUI_ColorUtil.Fade(titleCol, op), true, false, true, false, true);
		surface.DrawText(x + PAD_X, y + 34, w - (PAD_X * 2), 22, m_sBody, FONT_BODY, MUI_ColorUtil.Fade(m_HudWhite, op), false, false, true, false, true);
	}
}
