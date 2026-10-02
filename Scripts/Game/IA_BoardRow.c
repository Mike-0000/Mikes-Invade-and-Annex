//------------------------------------------------------------------------------------------------
//! One line of a leaderboard: a player, or a server on the board of servers. The server packs
//! it into a short string for the wire; the menu unpacks it and keeps the cell texts, so
//! drawing a row builds no strings.
//------------------------------------------------------------------------------------------------
class IA_BoardRow
{
	static const int F_RANK = 0;
	static const int F_GRADE = 1;
	static const int F_NAME = 2;
	static const int F_KILLS = 3;
	static const int F_DEATHS = 4;
	static const int F_KD = 5;
	static const int F_HVT = 6;
	static const int F_GUARD = 7;
	static const int F_OBJ = 8;
	static const int F_FLIGHT = 9;
	static const int F_LIFTS = 10;
	static const int F_SCORE = 11;
	static const int F_PLAYERS = 12;
	static const int F_COUNT = 13;

	protected static const string SEP = "|";
	protected static const int NAME_PART = 11;

	int m_iRank;
	string m_sLabel;
	int m_iKills;
	int m_iDeaths;
	int m_iHvt;
	int m_iGuard;
	int m_iObj;
	int m_iScore;
	int m_iTransport;
	int m_iInsertions;
	int m_iPlayers;
	int m_iGrade;

	//! The name cut to the column it was last drawn in, and that column's width.
	string m_sFit;
	float m_fFitW = -1;

	protected ref array<string> m_aText = {};

	//------------------------------------------------------------------------------------------------
	//! The name goes last so it may hold the separator.
	//! \param grade session rank id, 0 on the boards that have no grade
	static string Pack(int rank, string name, int kills, int deaths, int hvt, int guard, int obj, int score, int transport, int insertions, int players, int grade)
	{
		string clean = name;
		clean.Replace("\n", " ");
		clean.Replace("\r", " ");

		string line = rank.ToString() + SEP + kills.ToString() + SEP + deaths.ToString();
		line = line + SEP + hvt.ToString() + SEP + guard.ToString() + SEP + obj.ToString();
		line = line + SEP + score.ToString() + SEP + transport.ToString() + SEP + insertions.ToString();
		line = line + SEP + players.ToString() + SEP + grade.ToString() + SEP + clean;
		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! \return null when the line is not a packed row
	static IA_BoardRow Unpack(string line)
	{
		if (line.IsEmpty())
			return null;

		ref array<string> parts = {};
		line.Split(SEP, parts, false);
		int count = parts.Count();
		if (count <= NAME_PART)
			return null;

		ref IA_BoardRow row = new IA_BoardRow();
		row.m_iRank = parts[0].ToInt();
		row.m_iKills = parts[1].ToInt();
		row.m_iDeaths = parts[2].ToInt();
		row.m_iHvt = parts[3].ToInt();
		row.m_iGuard = parts[4].ToInt();
		row.m_iObj = parts[5].ToInt();
		row.m_iScore = parts[6].ToInt();
		row.m_iTransport = parts[7].ToInt();
		row.m_iInsertions = parts[8].ToInt();
		row.m_iPlayers = parts[9].ToInt();
		row.m_iGrade = parts[10].ToInt();

		string name = parts[NAME_PART];
		for (int part = NAME_PART + 1; part < count; part++)
		{
			name = name + SEP + parts[part];
		}
		if (name.IsEmpty())
			name = "Unknown";
		row.m_sLabel = name;
		row.BuildTexts();
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the text of one cell, see the F_ constants
	string Text(int field)
	{
		if (field < 0 || field >= m_aText.Count())
			return "";
		return m_aText[field];
	}

	//------------------------------------------------------------------------------------------------
	//! \return the number a sort key orders this row by
	float SortValue(int sortKey)
	{
		if (sortKey == IA_BoardProtocol.SORT_KILLS)
			return m_iKills;
		if (sortKey == IA_BoardProtocol.SORT_DEATHS)
			return m_iDeaths;
		if (sortKey == IA_BoardProtocol.SORT_KD)
			return KillRatio(m_iKills, m_iDeaths);
		if (sortKey == IA_BoardProtocol.SORT_HVT)
			return m_iHvt;
		if (sortKey == IA_BoardProtocol.SORT_GUARD)
			return m_iGuard;
		if (sortKey == IA_BoardProtocol.SORT_OBJ)
			return m_iObj;
		if (sortKey == IA_BoardProtocol.SORT_TRANSPORT)
			return m_iTransport;
		if (sortKey == IA_BoardProtocol.SORT_INSERTIONS)
			return m_iInsertions;
		if (sortKey == IA_BoardProtocol.SORT_PLAYERS)
			return m_iPlayers;
		return m_iScore;
	}

	//------------------------------------------------------------------------------------------------
	//! Kills for each death; a player who never died is rated on their kills.
	static float KillRatio(int kills, int deaths)
	{
		float lives = deaths;
		if (lives < 1)
			lives = 1;
		return kills / lives;
	}

	//------------------------------------------------------------------------------------------------
	//! 2.5 -> "2.50"
	static string FormatRatio(float ratio)
	{
		int hundredths = Math.Round(ratio * 100.0);
		int whole = hundredths / 100;
		int rest = hundredths - whole * 100;
		string decimals = rest.ToString();
		if (rest < 10)
			decimals = "0" + decimals;
		return whole.ToString() + "." + decimals;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildTexts()
	{
		m_aText.Clear();
		m_aText.Insert(IA_PilotDropoffPayload.FormatNumber(m_iRank));
		if (m_iGrade > 0)
			m_aText.Insert(IA_SessionRankLadder.GetShortName(m_iGrade));
		else
			m_aText.Insert("");
		m_aText.Insert(m_sLabel);
		m_aText.Insert(IA_PilotDropoffPayload.FormatNumber(m_iKills));
		m_aText.Insert(IA_PilotDropoffPayload.FormatNumber(m_iDeaths));
		m_aText.Insert(FormatRatio(KillRatio(m_iKills, m_iDeaths)));
		m_aText.Insert(IA_PilotDropoffPayload.FormatNumber(m_iHvt));
		m_aText.Insert(IA_PilotDropoffPayload.FormatNumber(m_iGuard));
		m_aText.Insert(IA_PilotDropoffPayload.FormatNumber(m_iObj));
		m_aText.Insert(IA_PilotDropoffPayload.FormatNumber(m_iTransport));
		m_aText.Insert(IA_PilotDropoffPayload.FormatNumber(m_iInsertions));
		m_aText.Insert(IA_PilotDropoffPayload.FormatNumber(m_iScore));
		m_aText.Insert(IA_PilotDropoffPayload.FormatNumber(m_iPlayers));
	}
}
