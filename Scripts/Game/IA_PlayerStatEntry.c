[BaseContainerProps(configRoot: true)]
class IA_PlayerStatEntry
{
    [Attribute("", UIWidgets.EditBox, "Player Name")]
    string PlayerName;

    [Attribute("0", UIWidgets.EditBox, "Kills")]
    int kills;

    [Attribute("0", UIWidgets.EditBox, "Deaths")]
    int deaths;
} 