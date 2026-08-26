//------------------------------------------------------------------------------------------------
//! F6 row: named AttributeHolder hosts WLib_EditBox. TickboxHolder stays hidden.
//------------------------------------------------------------------------------------------------
class IA_GmNameEditorAttributeUIComponent : SCR_BaseEditorAttributeUIComponent
{
	protected static const ResourceName WLIB_EDIT = "{0022F0B45ADBC5AC}UI/layouts/WidgetLibrary/EditBox/WLib_EditBox.layout";
	protected static const string EDIT_NAME = "IA_NameEdit";

	protected SCR_EditBoxComponent m_NameEdit;

	//------------------------------------------------------------------------------------------------
	override void Init(Widget w, SCR_BaseEditorAttribute attribute)
	{
		SpawnNameEdit(w);
		super.Init(w, attribute);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnNameEdit(Widget w)
	{
		if (!w)
			return;

		Widget holder = w.FindAnyWidget("AttributeHolder");
		if (!holder)
			holder = w;

		Widget child = holder.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.SetVisible(false);
			child = next;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget editRoot = workspace.CreateWidgets(WLIB_EDIT, holder);
		if (!editRoot)
		{
			Print("[IA] GM name F6 field failed to spawn WLib_EditBox", LogLevel.ERROR);
			return;
		}

		editRoot.SetName(EDIT_NAME);
		m_NameEdit = SCR_EditBoxComponent.Cast(editRoot.FindHandler(SCR_EditBoxComponent));
		if (!m_NameEdit)
			return;

		m_UIComponent = m_NameEdit;
		m_NameEdit.m_OnTextChange.Insert(OnNameTyped);
		m_NameEdit.m_OnConfirm.Insert(OnNameConfirm);
	}

	//------------------------------------------------------------------------------------------------
	override void ShowAttributeDescription()
	{
		if (!m_AttributeManager)
			return;

		m_bIsShowingDescription = true;
		SCR_BaseEditorAttribute attribute = GetAttribute();
		if (attribute)
			m_AttributeManager.SetAttributeDescription(attribute.GetUIInfo());
	}

	//------------------------------------------------------------------------------------------------
	override void SetFromVar(SCR_BaseEditorAttributeVar var)
	{
		super.SetFromVar(var);

		IA_GmNameEditorAttribute nameAttr = IA_GmNameEditorAttribute.Cast(GetAttribute());
		if (!nameAttr || !m_NameEdit)
			return;

		m_NameEdit.SetValue(nameAttr.GetDraftName());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnNameTyped(string text)
	{
		ApplyDraft(text);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnNameConfirm(SCR_EditBoxComponent editBox, string text)
	{
		ApplyDraft(text);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyDraft(string text)
	{
		IA_GmNameEditorAttribute nameAttr = IA_GmNameEditorAttribute.Cast(GetAttribute());
		if (!nameAttr)
			return;

		nameAttr.SetDraftName(text);
		SCR_BaseEditorAttributeVar var = nameAttr.GetVariable(true);
		if (var)
			var.SetInt(var.GetInt() + 1);

		AttributeValueChanged();
	}

	//------------------------------------------------------------------------------------------------
	override void HandlerDeattached(Widget w)
	{
		if (m_NameEdit)
		{
			m_NameEdit.m_OnTextChange.Remove(OnNameTyped);
			m_NameEdit.m_OnConfirm.Remove(OnNameConfirm);
		}

		super.HandlerDeattached(w);
	}
}
