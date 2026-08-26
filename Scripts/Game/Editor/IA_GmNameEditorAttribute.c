//------------------------------------------------------------------------------------------------
//! F6 field for the capture-site display name.
//! SCR_BaseEditorAttributeVar is a 12-byte vector, so the typed string lives on this
//! attribute and is applied client-side (m_bIsServer 0) then RPC'd to the server.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class IA_GmNameEditorAttribute : SCR_BaseEditorAttribute
{
	protected string m_sDraftName;

	//------------------------------------------------------------------------------------------------
	void IA_GmNameEditorAttribute()
	{
		m_aAttributeDynamicDescriptions = new array<ref SCR_BaseAttributeDynamicDescription>();
		m_Layout = "{1A6D47B0E6C358C1}UI/layouts/Editor/Attributes/IA_GmNameAttribute.layout";
		m_CategoryConfig = "{416C6E9ECC3D231D}Configs/Editor/AttributeCategories/Entity.conf";
		m_UIInfo = new SCR_EditorAttributeUIInfo();
		m_UIInfo.SetName("Objective name");
		m_UIInfo.SetDescription("Display name on tasks, Director pins, and capture HUD. Blank keeps the current name.");
	}

	//------------------------------------------------------------------------------------------------
	string GetDraftName()
	{
		return m_sDraftName;
	}

	//------------------------------------------------------------------------------------------------
	void SetDraftName(string name)
	{
		m_sDraftName = name;
	}

	//------------------------------------------------------------------------------------------------
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		IA_AreaMarker marker = IA_GmDirector.MarkerFromEditorItem(item);
		if (!marker)
			return null;

		m_sDraftName = marker.GetAreaName();
		return SCR_BaseEditorAttributeVar.CreateInt(0);
	}

	//------------------------------------------------------------------------------------------------
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		IA_AreaMarker marker = IA_GmDirector.MarkerFromEditorItem(item);
		if (!marker)
			return;
		if (m_sDraftName.IsEmpty())
			return;

		vector origin = marker.GetOrigin();
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;

		pc.IA_AskGmRenameSite(origin[0], origin[2], m_sDraftName);
	}
}
