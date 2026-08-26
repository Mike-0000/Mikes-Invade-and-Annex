//------------------------------------------------------------------------------------------------
//! Injects I&A GM F6 attributes into the vanilla editor attribute manager.
//! Load IA_Gm.conf — script `new` leaves m_aAttributeDynamicDescriptions null and
//! GetDynamicDescriptionArray NPEs when the attribute window opens.
//------------------------------------------------------------------------------------------------
modded class SCR_AttributesManagerEditorComponentClass
{
	protected static const ResourceName IA_GM_ATTRIBUTES = "{1A6D47B0E6C358A1}Configs/Editor/AttributeLists/IA_Gm.conf";

	protected bool m_bIAGmInserted;
	protected ref SCR_EditorAttributeList m_IAList;

	//------------------------------------------------------------------------------------------------
	void IA_InsertGmAttributes()
	{
		if (m_bIAGmInserted)
			return;
		if (!m_aAttributes)
			return;

		int i;
		int n = m_aAttributes.Count();
		for (i = 0; i < n; i++)
		{
			if (IA_GmRadiusEditorAttribute.Cast(m_aAttributes[i]))
			{
				m_bIAGmInserted = true;
				return;
			}
		}

		Resource configContainer = BaseContainerTools.LoadContainer(IA_GM_ATTRIBUTES);
		if (!configContainer || !configContainer.IsValid())
		{
			Print("[IA] GM attribute list conf failed to load", LogLevel.ERROR);
			return;
		}

		m_IAList = SCR_EditorAttributeList.Cast(BaseContainerTools.CreateInstanceFromContainer(configContainer.GetResource().ToBaseContainer()));
		if (!m_IAList)
		{
			Print("[IA] GM attribute list conf is not SCR_EditorAttributeList", LogLevel.ERROR);
			return;
		}

		m_bIAGmInserted = true;
		m_IAList.InsertAllAttributes(m_aAttributes);
	}
}

modded class SCR_AttributesManagerEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SCR_AttributesManagerEditorComponentClass data = SCR_AttributesManagerEditorComponentClass.Cast(GetComponentData(owner));
		if (data)
			data.IA_InsertGmAttributes();
	}
}
