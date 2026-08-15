//------------------------------------------------------------------------------------------------
//! Composes a single leaderboard row from MUI_Row + MUI_Label columns (not a MUI widget).
//! Pattern for consumer composites: factory Create(runtime), keep `protected ref` columns,
//! expose GetRow() so the parent can AddChild. Do not add this class to Mikes-UI.
//------------------------------------------------------------------------------------------------
class IA_LeaderboardRow
{
	protected ref MUI_Panel m_Root;
	protected ref MUI_Row m_Row;
	protected ref MUI_Label m_Rank;
	protected ref MUI_Label m_Name;
	protected ref MUI_Label m_Kills;
	protected ref MUI_Label m_Deaths;
	protected ref MUI_Label m_Hvt;
	protected ref MUI_Label m_Guard;
	protected ref MUI_Label m_Obj;
	protected ref MUI_Label m_Score;
	protected ref MUI_Progress m_ScoreBar;
	protected bool m_bHeader;

	//------------------------------------------------------------------------------------------------
	static IA_LeaderboardRow Create(notnull MUI_Runtime runtime, string nameSuffix, bool header)
	{
		ref IA_LeaderboardRow row = new IA_LeaderboardRow();
		row.Build(runtime, nameSuffix, header);
		return row;
	}

	//------------------------------------------------------------------------------------------------
	protected void Build(notnull MUI_Runtime runtime, string nameSuffix, bool header)
	{
		m_bHeader = header;

		m_Root = runtime.CreatePanel("lbRoot_" + nameSuffix);
		m_Root.GetStyle().m_Fill = Color.FromInt(0);
		m_Root.GetStyle().m_fRadius = 0;
		m_Root.GetStyle().m_fGap = 2;
		m_Root.GetStyle().m_bBlockHit = false;
		m_Root.SetFillWidth();

		m_Row = runtime.CreateRow("lbRow_" + nameSuffix);
		m_Row.SetGap(8);
		m_Row.SetHeight(32);
		m_Row.GetStyle().m_fMinHeight = 32;

		m_Rank = MakeCol(runtime, "#", "rank_" + nameSuffix, 44, header);
		m_Name = MakeCol(runtime, "NAME", "name_" + nameSuffix, 220, header);
		m_Name.SetFillWidth();
		m_Name.SetGrow(1);
		m_Kills = MakeCol(runtime, "K", "kills_" + nameSuffix, 56, header);
		m_Deaths = MakeCol(runtime, "D", "deaths_" + nameSuffix, 56, header);
		m_Hvt = MakeCol(runtime, "HVT", "hvt_" + nameSuffix, 56, header);
		m_Guard = MakeCol(runtime, "GRD", "guard_" + nameSuffix, 56, header);
		m_Obj = MakeCol(runtime, "OBJ", "obj_" + nameSuffix, 56, header);
		m_Score = MakeCol(runtime, "SCORE", "score_" + nameSuffix, 72, header);

		m_Row.AddChild(m_Rank);
		m_Row.AddChild(m_Name);
		m_Row.AddChild(m_Kills);
		m_Row.AddChild(m_Deaths);
		m_Row.AddChild(m_Hvt);
		m_Row.AddChild(m_Guard);
		m_Row.AddChild(m_Obj);
		m_Row.AddChild(m_Score);

		m_Root.AddChild(m_Row);

		if (!header)
		{
			m_ScoreBar = runtime.CreateProgress("lbBar_" + nameSuffix);
			m_ScoreBar.SetValue(0);
			m_Root.AddChild(m_ScoreBar);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected MUI_Label MakeCol(notnull MUI_Runtime runtime, string text, string name, float width, bool header)
	{
		ref MUI_Label label = runtime.CreateLabel(text, name);
		label.SetWidth(width);
		label.SetFontSize(runtime.GetTheme().FONT_SMALL);
		if (header)
		{
			label.SetBold(true);
			label.SetMuted(true);
		}
		else
		{
			label.SetBold(false);
		}
		return label;
	}

	//------------------------------------------------------------------------------------------------
	//! Parent should AddChild this (includes optional score bar).
	MUI_Panel GetRow()
	{
		return m_Root;
	}

	//------------------------------------------------------------------------------------------------
	void SetValues(string rank, string playerName, string kills, string deaths, string hvt, string guard, string obj, string score)
	{
		if (m_Rank)
			m_Rank.SetText(rank);
		if (m_Name)
			m_Name.SetText(playerName);
		if (m_Kills)
			m_Kills.SetText(kills);
		if (m_Deaths)
			m_Deaths.SetText(deaths);
		if (m_Hvt)
			m_Hvt.SetText(hvt);
		if (m_Guard)
			m_Guard.SetText(guard);
		if (m_Obj)
			m_Obj.SetText(obj);
		if (m_Score)
			m_Score.SetText(score);
	}

	//------------------------------------------------------------------------------------------------
	void SetObjVisible(bool visible)
	{
		if (m_Obj)
			m_Obj.SetVisible(visible);
	}

	//------------------------------------------------------------------------------------------------
	//! rankIndex 0 = first place. Uses runtime theme accents.
	void SetRankHighlight(int rankIndex)
	{
		if (m_bHeader || !m_Rank)
			return;

		MUI_ThemeData theme = m_Rank.GetTheme();
		if (rankIndex == 0)
		{
			m_Rank.SetColor(theme.Accent);
			m_Rank.SetBold(true);
			if (m_Name)
			{
				m_Name.SetColor(theme.Accent);
				m_Name.SetBold(true);
			}
		}
		else if (rankIndex == 1)
		{
			m_Rank.SetColor(theme.Cyan);
			m_Rank.SetBold(true);
			if (m_Name)
				m_Name.SetColor(theme.Text);
		}
		else if (rankIndex == 2)
		{
			m_Rank.SetColor(theme.CyanDim);
			m_Rank.SetBold(true);
			if (m_Name)
				m_Name.SetColor(theme.Text);
		}
		else
		{
			m_Rank.SetColor(theme.Text);
			m_Rank.SetBold(false);
			if (m_Name)
			{
				m_Name.SetColor(theme.Text);
				m_Name.SetBold(false);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	void SetScoreRatio(float ratio)
	{
		if (m_ScoreBar)
			m_ScoreBar.SetValue(ratio);
	}
}
