//------------------------------------------------------------------------------------------------
//! F6 trigger: spawn occupying forces and join the Live AO immediately.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class IA_GmHotAddEditorAttribute : SCR_BaseEditorAttribute
{
	//------------------------------------------------------------------------------------------------
	void IA_GmHotAddEditorAttribute()
	{
		m_aAttributeDynamicDescriptions = new array<ref SCR_BaseAttributeDynamicDescription>();
		m_Layout = "{D0F3CE0C63A5AEBB}UI/layouts/Editor/Attributes/AttributePrefabs/AttributePrefab_Checkbox.layout";
		m_CategoryConfig = "{416C6E9ECC3D231D}Configs/Editor/AttributeCategories/Entity.conf";
		m_UIInfo = new SCR_EditorAttributeUIInfo();
		m_UIInfo.SetName("Add to Live now");
		m_UIInfo.SetDescription("Check to spawn occupying forces on this site and append it to the Live AO.");
	}

	//------------------------------------------------------------------------------------------------
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		IA_AreaMarker marker = IA_GmDirector.MarkerFromEditorItem(item);
		if (!marker)
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(false);
	}

	//------------------------------------------------------------------------------------------------
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;
		if (!var.GetBool())
			return;
		IA_AreaMarker marker = IA_GmDirector.MarkerFromEditorItem(item);
		if (!marker)
			return;
		IA_GmDirector.GetInstance().HotAdd(marker);

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;
		PlayerController pc = pm.GetPlayerController(playerID);
		SCR_PlayerController scrPc = SCR_PlayerController.Cast(pc);
		if (scrPc)
			scrPc.IA_BroadcastGmBuckets();
	}
}
