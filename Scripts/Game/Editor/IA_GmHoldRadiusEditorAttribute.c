//------------------------------------------------------------------------------------------------
//! F6 slider for IA_GmHoldPost Hold radius. Hidden on capture sites.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class IA_GmHoldRadiusEditorAttribute : SCR_BaseValueListEditorAttribute
{
	//------------------------------------------------------------------------------------------------
	void IA_GmHoldRadiusEditorAttribute()
	{
		m_aAttributeDynamicDescriptions = new array<ref SCR_BaseAttributeDynamicDescription>();
		m_Layout = "{680E4985E42137FB}UI/layouts/Editor/Attributes/AttributePrefabs/AttributePrefab_Slider.layout";
		m_CategoryConfig = "{416C6E9ECC3D231D}Configs/Editor/AttributeCategories/Entity.conf";
		m_baseValues = new SCR_EditorAttributeBaseValues();
		m_baseValues.SetMaxValue(IA_GmHoldPost.MAX_RADIUS);

		m_UIInfo = new SCR_EditorAttributeUIInfo();
		m_UIInfo.SetName("Building hold radius");
		m_UIInfo.SetDescription("Wait-order completion radius in metres. Keep this small so occupying AI stay inside the building.");
	}

	//------------------------------------------------------------------------------------------------
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		IA_GmHoldPost post = IA_GmHoldPost.FromEditorItem(item);
		if (!post)
			return null;
		return SCR_BaseEditorAttributeVar.CreateFloat(post.GetHoldRadius());
	}

	//------------------------------------------------------------------------------------------------
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;
		IA_GmHoldPost post = IA_GmHoldPost.FromEditorItem(item);
		if (!post)
			return;
		post.SetHoldRadius(var.GetFloat());
	}
}
