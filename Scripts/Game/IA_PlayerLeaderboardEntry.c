// Scripts/Game/IA_PlayerLeaderboardEntry.c

class IA_PlayerLeaderboardEntry : ScriptedWidgetComponent
{
    private TextWidget m_Rank;
    private TextWidget m_PlayerName;
    private TextWidget m_Kills;
    private TextWidget m_Deaths;

    override void HandlerAttached(Widget w)
    {
        super.HandlerAttached(w);

        m_Rank = TextWidget.Cast(w.FindAnyWidget("Rank"));
        m_PlayerName = TextWidget.Cast(w.FindAnyWidget("PlayerName"));
        m_Kills = TextWidget.Cast(w.FindAnyWidget("Kills"));
        m_Deaths = TextWidget.Cast(w.FindAnyWidget("Deaths"));
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

    void SetKills(string kills)
    {
        if (m_Kills)
            m_Kills.SetText(kills);
    }

    void SetDeaths(string deaths)
    {
        if (m_Deaths)
            m_Deaths.SetText(deaths);
    }
} 