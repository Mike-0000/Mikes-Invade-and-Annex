//------------------------------------------------------------------------------------------------
//! F6 checkbox: put this site in the Live AO bucket instead of Staging.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseEditorAttributeCustomTitle()]
class IA_GmLiveBucketEditorAttribute : SCR_BaseEditorAttribute
{
	//------------------------------------------------------------------------------------------------
	void IA_GmLiveBucketEditorAttribute()
	{
		m_Layout = "{D0F3CE0C63A5AEBB}UI/layouts/Editor/Attributes/AttributePrefabs/AttributePrefab_Checkbox.layout";
		m_CategoryConfig = "{416C6E9ECC3D231D}Configs/Editor/AttributeCategories/Entity.conf";
		m_UIInfo = new SCR_EditorAttributeUIInfo();
		m_UIInfo.SetName("Live AO");
		m_UIInfo.SetDescription("On: this site belongs to the current Live AO. Off: Staging (next AO).");
	}

	//------------------------------------------------------------------------------------------------
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		IA_AreaMarker marker = IA_GmDirector.MarkerFromEditorItem(item);
		if (!marker)
			return null;

		IA_GmDirector dir = IA_GmDirector.GetInstance();
		int liveId = dir.GetLiveGroupId();
		bool isLive = false;
		if (liveId >= 0 && marker.m_areaGroup == liveId)
			isLive = true;
		return SCR_BaseEditorAttributeVar.CreateBool(isLive);
	}

	//------------------------------------------------------------------------------------------------
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;
		IA_AreaMarker marker = IA_GmDirector.MarkerFromEditorItem(item);
		if (!marker)
			return;

		IA_GmBucket bucket = IA_GmBucket.Staging;
		if (var.GetBool())
			bucket = IA_GmBucket.Live;
		IA_GmDirector.GetInstance().RegisterMarker(marker, bucket);
	}
}
