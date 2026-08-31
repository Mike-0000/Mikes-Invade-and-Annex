//------------------------------------------------------------------------------------------------
//! Compact top-right session chip: place number, grade, XP bar, personal stats.
//! Composite (not a MUI widget). Parent AddChild(GetRoot()). Keep as protected ref.
//! Full opacity for 30s after spawn and 15s after a local kill, then fades to
//! 70% transparency (0.3 opacity) across the whole chip.
//! Local kills also show a green +N beside K. The pip lives in a reserved
//! slot so D/XP never shift when it appears. Further kills within 3s stack
//! the same pip (+2, +3, ...); the window refreshes on each kill, then the
//! pip fades out in 0.45s.
//------------------------------------------------------------------------------------------------
class IA_RankHudPanel
{
	protected static const float CHIP_W = 268;
	protected static const float FULL_OPACITY = 1.0;
	protected static const float IDLE_OPACITY = 0.3;
	protected static const float SPAWN_HOLD_SEC = 30.0;
	protected static const float KILL_HOLD_SEC = 15.0;
	protected static const float FADE_SPEED = 7.0;
	protected static const float STREAK_WINDOW_SEC = 3.0;
	protected static const float STREAK_FADE_SEC = 0.45;
	protected static const float KILL_DELTA_W = 28;

	protected ref MUI_Surface m_Root;
	protected ref MUI_Label m_Place;
	protected ref MUI_Label m_Grade;
	protected ref MUI_Label m_Next;
	protected ref MUI_Progress m_Bar;
	protected ref MUI_Label m_Kills;
	protected ref MUI_Label m_KillDelta;
	protected ref MUI_Label m_Deaths;
	protected ref MUI_Label m_Xp;
	protected bool m_bBound;
	protected bool m_bHasRankSample;
	protected bool m_bHasKillSample;
	protected int m_iLastRankId;
	protected int m_iLastKills;
	protected int m_iStreakCount;
	protected float m_fStreakHold;
	protected float m_fDeltaOp;
	protected float m_fHoldLeft;
	protected float m_fShownOp;
	protected ref ScriptInvoker m_OnPromoted;

	//------------------------------------------------------------------------------------------------
	static IA_RankHudPanel Create(notnull MUI_Runtime runtime)
	{
		ref IA_RankHudPanel panel = new IA_RankHudPanel();
		panel.m_OnPromoted = new ScriptInvoker();
		panel.m_fShownOp = FULL_OPACITY;
		panel.m_fHoldLeft = SPAWN_HOLD_SEC;
		panel.Build(runtime);
		panel.ApplyShownOpacity();
		return panel;
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull MUI_Runtime runtime)
	{
		MUI_ThemeData theme = runtime.GetTheme();

		m_Root = runtime.CreateSurface("sessionRank");
		m_Root.SetWidth(CHIP_W);
		m_Root.SetPadding(8);
		m_Root.SetGap(3);
		m_Root.SetRadius(10);
		m_Root.SetFill(theme.DeepFrost);
		m_Root.SetStroke(theme.Border, 1.2);
		m_Root.SetBlurEnabled(true);
		m_Root.GetStyle().m_bBlockHit = false;
		m_Root.SetAlign(1, 0);
		m_Root.SetIntro(0.1, 0.4, -16);

		ref MUI_Row gradeRow = runtime.CreateRow("sessionGradeRow");
		gradeRow.SetGap(8);
		gradeRow.GetStyle().m_bBlockHit = false;

		m_Grade = runtime.CreateLabel("Pvt.", "sessionGrade");
		m_Grade.SetFontSize(theme.FONT_SMALL);
		m_Grade.SetBold(true);
		m_Grade.SetColor(theme.Cyan);
		m_Grade.SetWidth(48);

		m_Next = runtime.CreateLabel("Next  Cpl.", "sessionNext");
		m_Next.SetFontSize(theme.FONT_SMALL);
		m_Next.SetMuted(true);
		m_Next.SetFillWidth();
		m_Next.SetGrow(1);

		m_Place = runtime.CreateLabel("#-", "sessionPlace");
		m_Place.SetFontSize(theme.FONT_SMALL);
		m_Place.SetBold(true);
		m_Place.SetColor(theme.Accent);
		m_Place.SetHugWidth();

		gradeRow.AddChild(m_Grade);
		gradeRow.AddChild(m_Next);
		gradeRow.AddChild(m_Place);

		m_Bar = runtime.CreateProgress("sessionXp");
		m_Bar.SetHeight(4);
		m_Bar.SetMinHeight(4);
		m_Bar.SetRadius(2);
		m_Bar.SetValue(0);

		ref MUI_Row statsRow = runtime.CreateRow("sessionStatsRow");
		statsRow.SetGap(10);
		statsRow.GetStyle().m_bBlockHit = false;

		ref MUI_Row killCell = runtime.CreateRow("sessionKillCell");
		killCell.SetGap(6);
		killCell.GetStyle().m_bBlockHit = false;
		killCell.SetHugWidth();

		m_Kills = MakeStat(runtime, "K  0", "sessionK", 0);
		m_Kills.SetHugWidth();
		m_KillDelta = runtime.CreateLabel("", "sessionKDelta");
		m_KillDelta.SetFontSize(theme.FONT_SMALL);
		m_KillDelta.SetBold(true);
		m_KillDelta.SetColor(theme.Live);
		m_KillDelta.SetWidth(KILL_DELTA_W);
		// Stay in-flow at opacity 0. SetVisible(false) drops the slot and
		// shoves D/XP sideways when the pip comes back.
		m_KillDelta.SetOpacity(0);

		killCell.AddChild(m_Kills);
		killCell.AddChild(m_KillDelta);

		m_Deaths = MakeStat(runtime, "D  0", "sessionD", 52);
		m_Xp = MakeStat(runtime, "XP  0/150", "sessionXpLbl", 0);
		m_Xp.SetFillWidth();
		m_Xp.SetGrow(1);

		statsRow.AddChild(killCell);
		statsRow.AddChild(m_Deaths);
		statsRow.AddChild(m_Xp);

		m_Root.AddChild(gradeRow);
		m_Root.AddChild(m_Bar);
		m_Root.AddChild(statsRow);
	}

	//------------------------------------------------------------------------------------------------
	protected MUI_Label MakeStat(notnull MUI_Runtime runtime, string text, string name, float width)
	{
		ref MUI_Label label = runtime.CreateLabel(text, name);
		label.SetFontSize(runtime.GetTheme().FONT_SMALL);
		label.SetMuted(true);
		if (width > 0)
			label.SetWidth(width);
		return label;
	}

	//------------------------------------------------------------------------------------------------
	MUI_Surface GetRoot()
	{
		return m_Root;
	}

	//------------------------------------------------------------------------------------------------
	void SetHudVisible(bool visible)
	{
		if (m_Root)
			m_Root.SetVisible(visible);
	}

	//------------------------------------------------------------------------------------------------
	void PulseSpawn()
	{
		m_fHoldLeft = SPAWN_HOLD_SEC;
	}

	//------------------------------------------------------------------------------------------------
	void PulseKill()
	{
		if (m_fHoldLeft < KILL_HOLD_SEC)
			m_fHoldLeft = KILL_HOLD_SEC;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddKillStreak(int gained)
	{
		if (gained < 1)
			return;

		if (m_fStreakHold > 0)
			m_iStreakCount = m_iStreakCount + gained;
		else
			m_iStreakCount = gained;

		m_fStreakHold = STREAK_WINDOW_SEC;
		m_fDeltaOp = FULL_OPACITY;
		ShowKillDelta();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowKillDelta()
	{
		if (!m_KillDelta)
			return;

		m_KillDelta.SetText("+" + m_iStreakCount.ToString());
		m_KillDelta.SetOpacity(FULL_OPACITY);
		m_KillDelta.SetIntro(0, 0.2, -10);
	}

	//------------------------------------------------------------------------------------------------
	protected void HideKillDelta()
	{
		m_iStreakCount = 0;
		m_fStreakHold = 0;
		m_fDeltaOp = 0;
		if (!m_KillDelta)
			return;

		m_KillDelta.SetOpacity(0);
		m_KillDelta.SetText("");
	}

	//------------------------------------------------------------------------------------------------
	protected void TickKillDelta(float dt)
	{
		if (m_iStreakCount < 1)
			return;

		if (m_fStreakHold > 0)
		{
			m_fStreakHold = m_fStreakHold - dt;
			if (m_fStreakHold < 0)
				m_fStreakHold = 0;
			return;
		}

		float fadeStep = dt / STREAK_FADE_SEC;
		m_fDeltaOp = m_fDeltaOp - fadeStep;
		if (m_fDeltaOp <= 0)
		{
			HideKillDelta();
			return;
		}

		if (m_KillDelta)
			m_KillDelta.SetOpacity(m_fDeltaOp);
	}

	//------------------------------------------------------------------------------------------------
	void Tick(float dt)
	{
		if (dt < 0)
			dt = 0;

		if (m_fHoldLeft > 0)
		{
			m_fHoldLeft = m_fHoldLeft - dt;
			if (m_fHoldLeft < 0)
				m_fHoldLeft = 0;
		}

		float target = IDLE_OPACITY;
		if (m_fHoldLeft > 0)
			target = FULL_OPACITY;

		m_fShownOp = MUI_Ease.Approach(m_fShownOp, target, dt, FADE_SPEED);
		ApplyShownOpacity();
		TickKillDelta(dt);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyShownOpacity()
	{
		if (m_Root)
			m_Root.SetOpacity(m_fShownOp);
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnPromoted()
	{
		if (!m_OnPromoted)
			m_OnPromoted = new ScriptInvoker();
		return m_OnPromoted;
	}

	//------------------------------------------------------------------------------------------------
	void Bind()
	{
		if (m_bBound)
			return;

		IA_SessionRankManagerComponent manager = IA_SessionRankManagerComponent.GetInstance();
		if (!manager)
		{
			GetGame().GetCallqueue().CallLater(this.Bind, 400, false);
			return;
		}

		manager.GetOnUpdated().Insert(this.OnSessionUpdated);
		m_bBound = true;
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	void Unbind()
	{
		GetGame().GetCallqueue().Remove(this.Bind);
		if (!m_bBound)
			return;

		IA_SessionRankManagerComponent manager = IA_SessionRankManagerComponent.GetInstance();
		if (manager)
			manager.GetOnUpdated().Remove(this.OnSessionUpdated);

		HideKillDelta();
		m_bBound = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSessionUpdated(string jsonData)
	{
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	void Refresh()
	{
		IA_SessionRankManagerComponent manager = IA_SessionRankManagerComponent.GetInstance();
		IA_SessionRankEntry entry = null;
		int place = 0;
		if (manager)
		{
			entry = manager.FindLocal();
			place = manager.GetLocalPlace();
		}

		int rankId = SCR_ECharacterRank.PRIVATE;
		int kills = 0;
		int deaths = 0;
		int xp = 0;
		if (entry)
		{
			rankId = entry.rankId;
			kills = entry.kills;
			deaths = entry.deaths;
			xp = entry.score;
		}

		if (m_Place)
		{
			// Ranking prefix, not a string-table id. MUI_TextUtil stamps
			// NO_LOCALIZATION so "#1" is not looked up every HUD paint.
			if (place > 0)
				m_Place.SetText("#" + place.ToString());
			else
				m_Place.SetText("#-");
		}

		if (m_Grade)
			m_Grade.SetText(IA_SessionRankLadder.GetShortName(rankId));
		if (m_Next)
			m_Next.SetText(IA_SessionRankLadder.GetNextLabel(xp));
		if (m_Bar)
			m_Bar.SetValue(IA_SessionRankLadder.GetProgress(xp));
		if (m_Kills)
			m_Kills.SetText("K  " + kills.ToString());
		if (m_Deaths)
			m_Deaths.SetText("D  " + deaths.ToString());
		if (m_Xp)
			m_Xp.SetText("XP  " + IA_SessionRankLadder.GetXpPair(xp));

		if (m_bHasKillSample && kills > m_iLastKills)
		{
			PulseKill();
			AddKillStreak(kills - m_iLastKills);
		}
		else if (m_bHasKillSample && kills < m_iLastKills)
		{
			HideKillDelta();
		}
		m_iLastKills = kills;
		m_bHasKillSample = true;

		if (!entry)
			return;

		if (m_bHasRankSample && rankId > m_iLastRankId)
		{
			if (!IA_LocalOptions.Get().HidePromotionNotifications())
			{
				if (m_Root)
					m_Root.SetIntro(0, 0.32, -10);
				GetOnPromoted().Invoke(rankId);
			}
		}

		m_iLastRankId = rankId;
		m_bHasRankSample = true;
	}
}
