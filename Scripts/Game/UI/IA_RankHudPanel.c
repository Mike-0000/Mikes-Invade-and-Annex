//------------------------------------------------------------------------------------------------
//! Compact top-right session chip: place number, grade, XP bar, personal stats.
//! Composite (not a MUI widget). Parent AddChild(GetRoot()). Keep as protected ref.
//------------------------------------------------------------------------------------------------
class IA_RankHudPanel
{
	protected static const float CHIP_W = 268;

	protected ref MUI_Surface m_Root;
	protected ref MUI_Label m_Place;
	protected ref MUI_Label m_Grade;
	protected ref MUI_Label m_Next;
	protected ref MUI_Progress m_Bar;
	protected ref MUI_Label m_Kills;
	protected ref MUI_Label m_Deaths;
	protected ref MUI_Label m_Xp;
	protected bool m_bBound;
	protected bool m_bHasRankSample;
	protected int m_iLastRankId;
	protected ref ScriptInvoker m_OnPromoted;

	//------------------------------------------------------------------------------------------------
	static IA_RankHudPanel Create(notnull MUI_Runtime runtime)
	{
		ref IA_RankHudPanel panel = new IA_RankHudPanel();
		panel.m_OnPromoted = new ScriptInvoker();
		panel.Build(runtime);
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

		m_Kills = MakeStat(runtime, "K  0", "sessionK", 52);
		m_Deaths = MakeStat(runtime, "D  0", "sessionD", 52);
		m_Xp = MakeStat(runtime, "XP  0/150", "sessionXpLbl", 0);
		m_Xp.SetFillWidth();
		m_Xp.SetGrow(1);

		statsRow.AddChild(m_Kills);
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

		if (!entry)
			return;

		if (m_bHasRankSample && rankId > m_iLastRankId)
		{
			if (m_Root)
				m_Root.SetIntro(0, 0.32, -10);
			GetOnPromoted().Invoke(rankId);
		}

		m_iLastRankId = rankId;
		m_bHasRankSample = true;
	}
}
