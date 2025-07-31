// Scripts/Game/IA_PlayerLeaderboardEntry.c

class IA_PlayerLeaderboardEntry : ScriptedWidgetComponent
{
    private TextWidget m_Rank;
    private TextWidget m_PlayerName;
    private TextWidget m_Kills;
    private TextWidget m_Deaths;
    private TextWidget m_hvt_kills;
    private TextWidget m_hvt_guard_kills;
    private TextWidget m_score;
	private TextWidget m_OBJScore;



    override void HandlerAttached(Widget w)
    {
        super.HandlerAttached(w);

        m_Rank = TextWidget.Cast(w.FindAnyWidget("Rank"));
        m_PlayerName = TextWidget.Cast(w.FindAnyWidget("PlayerName"));
        m_Kills = TextWidget.Cast(w.FindAnyWidget("Kills"));
        m_Deaths = TextWidget.Cast(w.FindAnyWidget("Deaths"));
		m_hvt_kills = TextWidget.Cast(w.FindAnyWidget("HVTKills"));
		m_hvt_guard_kills = TextWidget.Cast(w.FindAnyWidget("HVTGuardKills"));
		m_OBJScore = TextWidget.Cast(w.FindAnyWidget("OBJScore"));
		m_score = TextWidget.Cast(w.FindAnyWidget("Score"));


    }

    void SetRank(string rank)
    {
        if (m_Rank)
            m_Rank.SetText(rank);
    }

    void SetPlayerName(string name)
    {
        if (m_PlayerName)
            m_PlayerName.SetText(name);
    }

    void SetHVTKills(string hvtkills)
    {
        if (hvtkills)
            m_hvt_kills.SetText(hvtkills);
    }
	void SetHVTGuardKills(string hvtguardkills)
    {
        if (hvtguardkills)
            m_hvt_guard_kills.SetText(hvtguardkills);
    }
	void SetScore(string score)
    {
        if (score)
            m_score.SetText(score);
    }
	void SetKills(string kills)
    {
        if (m_Kills)
            m_Kills.SetText(kills);
    }
	
	void SetOBJScore(string score)
    {
        if (score)
            m_OBJScore.SetText(score);
    }

    void SetDeaths(string deaths)
    {
        if (m_Deaths)
            m_Deaths.SetText(deaths);
    }
	
	void SetOBJScoreVisible(bool visible)
	{
		if (m_OBJScore)
			m_OBJScore.SetVisible(visible);
	}
} 