//------------------------------------------------------------------------------------------------
//! Injects I&A GM F6 attributes into the vanilla editor attribute manager.
//------------------------------------------------------------------------------------------------
modded class SCR_AttributesManagerEditorComponentClass
{
	protected bool m_bIAGmInserted;
	protected ref IA_GmRadiusEditorAttribute m_IARadius;
	protected ref IA_GmLiveBucketEditorAttribute m_IALive;
	protected ref IA_GmHotAddEditorAttribute m_IAHot;

	//------------------------------------------------------------------------------------------------
	void IA_InsertGmAttributes()
	{
		if (m_bIAGmInserted)
			return;
		m_bIAGmInserted = true;
		if (!m_aAttributes)
			return;

		int i;
		int n = m_aAttributes.Count();
		for (i = 0; i < n; i++)
		{
			if (IA_GmRadiusEditorAttribute.Cast(m_aAttributes[i]))
				return;
		}

		m_IARadius = new IA_GmRadiusEditorAttribute();
		m_aAttributes.Insert(m_IARadius);
		m_IALive = new IA_GmLiveBucketEditorAttribute();
		m_aAttributes.Insert(m_IALive);
		m_IAHot = new IA_GmHotAddEditorAttribute();
		m_aAttributes.Insert(m_IAHot);
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
