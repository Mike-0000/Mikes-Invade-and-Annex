//------------------------------------------------------------------------------------------------
//! One player's current-session stats. In-memory only; JSON field names must match
//! these members for JsonLoadContext on clients.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class IA_SessionRankEntry
{
	[Attribute("", UIWidgets.EditBox, "Player id")]
	string playerId;

	[Attribute("", UIWidgets.EditBox, "Player Name")]
	string PlayerName;

	[Attribute("0", UIWidgets.EditBox, "Kills")]
	int kills;

	[Attribute("0", UIWidgets.EditBox, "Deaths")]
	int deaths;

	[Attribute("0", UIWidgets.EditBox, "HVT_Kills")]
	int hvt_kills;

	[Attribute("0", UIWidgets.EditBox, "HVT_Guard_Kills")]
	int hvt_guard_kills;

	[Attribute("0", UIWidgets.EditBox, "obj_score")]
	int obj_score;

	[Attribute("0", UIWidgets.EditBox, "Score / session XP")]
	int score;

	[Attribute("1", UIWidgets.EditBox, "SCR_ECharacterRank id")]
	int rankId;

	[Attribute("0", UIWidgets.EditBox, "Player manager id")]
	int netId;
}
