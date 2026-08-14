//------------------------------------------------------------------------------------------------
//! Composes a single leaderboard row from MUI_Row + MUI_Label columns (not a MUI widget).
class IA_LeaderboardRow
{
	protected ref MUI_Row m_Row;
	protected ref MUI_Label m_Rank;
	protected ref MUI_Label m_Name;
	protected ref MUI_Label m_Kills;
	protected ref MUI_Label m_Deaths;
	protected ref MUI_Label m_Hvt;
	protected ref MUI_Label m_Guard;
	protected ref MUI_Label m_Obj;
	protected ref MUI_Label m_Score;

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
	}

	//------------------------------------------------------------------------------------------------
	protected MUI_Label MakeCol(notnull MUI_Runtime runtime, string text, string name, float width, bool header)
	{
		ref MUI_Label label = runtime.CreateLabel(text, name);
		label.SetWidth(width);
		label.SetFontSize(MUI_Theme.FONT_SMALL);
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
	MUI_Row GetRow()
	{
		return m_Row;
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
}
