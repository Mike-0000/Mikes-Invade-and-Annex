//------------------------------------------------------------------------------------------------
//! Click bind for GM Director chip rows. ScriptInvoker has no sender, so each
//! chip keeps its kind + index here.
//------------------------------------------------------------------------------------------------
class IA_GmDirectorChipBind
{
	protected IA_GmDirectorMenu m_Menu;
	protected int m_iKind;
	protected int m_iIndex;

	//------------------------------------------------------------------------------------------------
	void Init(IA_GmDirectorMenu menu, int kind, int index)
	{
		m_Menu = menu;
		m_iKind = kind;
		m_iIndex = index;
	}

	//------------------------------------------------------------------------------------------------
	void OnClicked()
	{
		if (!m_Menu)
			return;
		m_Menu.OnChipPicked(m_iKind, m_iIndex);
	}
}
