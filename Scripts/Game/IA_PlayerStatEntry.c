[BaseContainerProps(configRoot: true)]
class IA_PlayerStatEntry
{
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
	
	[Attribute("0", UIWidgets.EditBox, "Score")]
    int score;
	[Attribute("0", UIWidgets.EditBox, "obj_score")]
    int obj_score;
} 