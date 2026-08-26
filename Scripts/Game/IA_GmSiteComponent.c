//------------------------------------------------------------------------------------------------
//! Registers an editable I&A capture site with the GM director (default Staging).
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Mike's I&A/GM", description: "Registers this area marker with the Game Master director.")]
class IA_GmSiteComponentClass : ScriptComponentClass
{
}

class IA_GmSiteComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer())
			return;
		if (!IA_GmDirector.IsGameMasterMode())
			return;

		IA_AreaMarker marker = IA_AreaMarker.Cast(owner);
		if (!marker)
			return;

		IA_GmDirector.GetInstance().RegisterMarker(marker, IA_GmBucket.Staging);
	}
}
