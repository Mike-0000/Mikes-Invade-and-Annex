//------------------------------------------------------------------------------------------------
//! F6 slider for IA_AreaMarker capture radius.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class IA_GmRadiusEditorAttribute : SCR_BaseValueListEditorAttribute
{
	//------------------------------------------------------------------------------------------------
	void IA_GmRadiusEditorAttribute()
	{
		m_Layout = "{680E4985E42137FB}UI/layouts/Editor/Attributes/AttributePrefabs/AttributePrefab_Slider.layout";
		m_CategoryConfig = "{416C6E9ECC3D231D}Configs/Editor/AttributeCategories/Entity.conf";
		m_baseValues = new SCR_EditorAttributeBaseValues();
		m_baseValues.SetMaxValue(400);

		m_UIInfo = new SCR_EditorAttributeUIInfo();
		m_UIInfo.SetName("Capture radius");
		m_UIInfo.SetDescription("I&A capture / occupy radius in metres.");
	}

	//------------------------------------------------------------------------------------------------
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		IA_AreaMarker marker = IA_GmDirector.MarkerFromEditorItem(item);
		if (!marker)
			return null;
		return SCR_BaseEditorAttributeVar.CreateFloat(marker.GetRadius());
	}

	//------------------------------------------------------------------------------------------------
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;
		IA_AreaMarker marker = IA_GmDirector.MarkerFromEditorItem(item);
		if (!marker)
			return;
		marker.SetRadius(var.GetFloat());
	}
}
