//------------------------------------------------------------------------------------------------
//! One update for the pilot card (IA_PilotHud), carried as the text of the
//! PilotProgress message. A drop reports passengers credited since the last
//! update; a status reports only the rating. The client merges updates that
//! arrive while the card is open, so a landing is one card however many
//! passengers step out. Pure data so the Workbench regression can pin the
//! wire format and the merge.
//------------------------------------------------------------------------------------------------
class IA_PilotDropoffPayload
{
	static const int KIND_DROP = 0;
	static const int KIND_STATUS = 1;
	static const int FIELD_COUNT = 9;
	protected static const string SEPARATOR = "|";
	protected static const string NO_NAME = "-";

	int m_iKind;
	//! Passengers and points credited by this update; both 0 on a status.
	int m_iTroops;
	int m_iPoints;
	//! Global rating after these points, negative while the backend total is unknown.
	int m_iRating = -1;
	//! Threshold of the skin being worked towards, 0 when nothing is left to unlock.
	int m_iRequired;
	//! Average metres from the dropoffs to the objective circle.
	int m_iEdgeM;
	//! Threshold and name of a skin these points unlocked; 0 and empty otherwise.
	int m_iUnlockedRequired;
	string m_sUnlockedName;
	//! Skin the progress refers to: the next locked one, else the best unlocked one.
	string m_sSkinName;

	//------------------------------------------------------------------------------------------------
	string Pack()
	{
		return string.Format("%1|%2|%3|%4|%5|%6|%7|%8|%9", m_iKind, m_iTroops, m_iPoints, m_iRating, m_iRequired, m_iEdgeM, m_iUnlockedRequired, PackName(m_sUnlockedName), PackName(m_sSkinName));
	}

	//------------------------------------------------------------------------------------------------
	//! \return null when the text is not a payload this build understands
	static IA_PilotDropoffPayload Parse(string text)
	{
		ref array<string> tokens = {};
		text.Split(SEPARATOR, tokens, false);
		if (tokens.Count() < FIELD_COUNT)
			return null;

		ref IA_PilotDropoffPayload data = new IA_PilotDropoffPayload();
		data.m_iKind = tokens[0].ToInt();
		data.m_iTroops = tokens[1].ToInt();
		data.m_iPoints = tokens[2].ToInt();
		data.m_iRating = tokens[3].ToInt();
		data.m_iRequired = tokens[4].ToInt();
		data.m_iEdgeM = tokens[5].ToInt();
		data.m_iUnlockedRequired = tokens[6].ToInt();
		data.m_sUnlockedName = UnpackName(tokens[7]);
		data.m_sSkinName = UnpackName(tokens[8]);

		if (data.m_iKind != KIND_DROP && data.m_iKind != KIND_STATUS)
			return null;
		if (data.m_iTroops < 0 || data.m_iPoints < 0 || data.m_iRequired < 0 || data.m_iEdgeM < 0)
			return null;
		if (data.m_iKind == KIND_DROP && (data.m_iTroops < 1 || data.m_iPoints < 1))
			return null;
		if (data.m_iRating < 0)
			data.m_iRating = -1;
		return data;
	}

	//------------------------------------------------------------------------------------------------
	//! Set the total and what it means against the skin catalog.
	//! \param earned points credited since the pilot last saw a total; crossing a threshold with them announces the unlock
	//! \return the skin those points unlocked, null when none
	IA_HeliSkinDef SetProgress(int rating, int earned)
	{
		m_iRating = rating;
		IA_HeliSkinDef unlocked = null;
		if (earned > 0)
		{
			unlocked = IA_HeliSkinCatalog.FindNewlyUnlocked(rating - earned, rating);
			if (unlocked)
			{
				m_sUnlockedName = unlocked.m_sDisplayName;
				m_iUnlockedRequired = unlocked.m_iRequiredPoints;
			}
		}

		IA_HeliSkinDef next = IA_HeliSkinCatalog.FindNextLocked(rating);
		if (next)
		{
			m_iRequired = next.m_iRequiredPoints;
			m_sSkinName = next.m_sDisplayName;
			return unlocked;
		}

		IA_HeliSkinDef best = IA_HeliSkinCatalog.FindBestUnlocked(rating);
		if (best)
			m_sSkinName = best.m_sDisplayName;
		return unlocked;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDrop()
	{
		return m_iKind == KIND_DROP;
	}

	//------------------------------------------------------------------------------------------------
	bool HasUnlock()
	{
		return !m_sUnlockedName.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Fold a later update into this one: passengers and points add up, the newer
	//! total wins, and an unlock stays announced once it has been reported.
	void Merge(notnull IA_PilotDropoffPayload newer)
	{
		int troops = m_iTroops + newer.m_iTroops;
		if (troops > 0)
		{
			float edgeSum = m_iEdgeM * m_iTroops + newer.m_iEdgeM * newer.m_iTroops;
			m_iEdgeM = Math.Round(edgeSum / troops);
		}
		m_iTroops = troops;
		m_iPoints = m_iPoints + newer.m_iPoints;
		if (newer.IsDrop())
			m_iKind = KIND_DROP;

		if (newer.m_iRating >= 0)
		{
			m_iRating = newer.m_iRating;
			m_iRequired = newer.m_iRequired;
			m_sSkinName = newer.m_sSkinName;
		}
		if (newer.HasUnlock())
		{
			m_sUnlockedName = newer.m_sUnlockedName;
			m_iUnlockedRequired = newer.m_iUnlockedRequired;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Insertion weight shown on the card, in tenths: 30 is the x3.0 hot-LZ weight.
	int WeightTenths()
	{
		if (m_iTroops <= 0)
			return 0;
		float perTroop = m_iPoints;
		perTroop = perTroop / m_iTroops;
		return Math.Round(perTroop * 10 / IA_TransportScoring.BASE_POINTS);
	}

	//------------------------------------------------------------------------------------------------
	//! True when every passenger of the drop was paid the hot-LZ weight.
	bool IsHotLz()
	{
		return m_iTroops > 0 && WeightTenths() >= Math.Round(IA_TransportScoring.HOT_WEIGHT * 10);
	}

	//------------------------------------------------------------------------------------------------
	//! One sentence for the legacy text HUD; empty when there is nothing to announce.
	string ToLine()
	{
		if (HasUnlock())
			return string.Format("Skin unlocked: %1. Take the pilot seat to fly it.", m_sUnlockedName);
		if (!IsDrop())
			return "";

		string troops = "1 troop";
		if (m_iTroops != 1)
			troops = string.Format("%1 troops", m_iTroops);
		string line = string.Format("Combat insertion: %1, +%2 transport rating", troops, FormatNumber(m_iPoints));
		if (m_iRating < 0)
			return line;
		if (m_iRequired > 0)
			return line + string.Format(" (%1 / %2 %3)", FormatNumber(m_iRating), FormatNumber(m_iRequired), m_sSkinName);
		return line + string.Format(" (%1 total)", FormatNumber(m_iRating));
	}

	//------------------------------------------------------------------------------------------------
	//! Progress towards a threshold in tenths of a percent, 0 to 1000.
	static int PercentTenths(int rating, int required)
	{
		if (rating <= 0 || required <= 0)
			return 0;
		if (rating >= required)
			return 1000;
		float tenths = rating;
		tenths = tenths * 1000 / required;
		return Math.Floor(tenths);
	}

	//------------------------------------------------------------------------------------------------
	static string FormatPercent(int tenths)
	{
		if (tenths >= 1000)
			return "100%";
		if (tenths < 0)
			tenths = 0;
		int whole = tenths / 10;
		int frac = tenths % 10;
		return whole.ToString() + "." + frac.ToString() + "%";
	}

	//------------------------------------------------------------------------------------------------
	//! 12700 -> "12,700"
	static string FormatNumber(int value)
	{
		string digits = value.ToString();
		bool negative = value < 0;
		if (negative)
			digits = digits.Substring(1, digits.Length() - 1);

		string result = "";
		int len = digits.Length();
		int i;
		for (i = 0; i < len; i++)
		{
			if (i > 0 && (len - i) % 3 == 0)
				result = result + ",";
			result = result + digits.Substring(i, 1);
		}
		if (negative)
			result = "-" + result;
		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected static string PackName(string name)
	{
		name.Replace(SEPARATOR, " ");
		name.TrimInPlace();
		if (name.IsEmpty())
			return NO_NAME;
		return name;
	}

	//------------------------------------------------------------------------------------------------
	protected static string UnpackName(string token)
	{
		if (token == NO_NAME)
			return "";
		return token;
	}
}
