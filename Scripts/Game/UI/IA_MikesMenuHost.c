//------------------------------------------------------------------------------------------------
//! Shared bootstrap for I&A Chimera menus that host Mike's UI on a fullscreen Frame.
//! HUD toasts do not use this — they MountPassive and tick from DisplayUpdate.
class IA_MikesMenuHost
{
	protected Widget m_wRoot;
	protected Widget m_wHost;
	protected ref MUI_Runtime m_Runtime;
	protected string m_sLogTag;

	//------------------------------------------------------------------------------------------------
	//! Creates the host Frame, mounts MUI, hides legacy layout children.
	//! Returns false on failure (caller should fall back to legacy UI).
	bool Open(notnull Widget root, string logTag)
	{
		Close();

		m_wRoot = root;
		m_sLogTag = logTag;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
		{
			Print(string.Format("[%1] No workspace for Mike's UI", m_sLogTag), LogLevel.ERROR);
			return false;
		}

		m_wHost = workspace.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 50, m_wRoot);
		if (!m_wHost)
		{
			Print(string.Format("[%1] Failed to create Mike's UI host", m_sLogTag), LogLevel.ERROR);
			return false;
		}

		FrameSlot.SetAnchorMin(m_wHost, 0, 0);
		FrameSlot.SetAnchorMax(m_wHost, 1, 1);
		FrameSlot.SetOffsets(m_wHost, 0, 0, 0, 0);

		m_Runtime = new MUI_Runtime();
		if (!m_Runtime.Mount(m_wHost))
		{
			Print(string.Format("[%1] Mike's UI mount failed", m_sLogTag), LogLevel.ERROR);
			m_Runtime.Unmount();
			m_Runtime = null;
			m_wHost.RemoveFromHierarchy();
			m_wHost = null;
			return false;
		}

		HideLegacyChildren();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Tick(float tDelta)
	{
		GetGame().GetInputManager().ActivateContext("MenuWithEditorContext");
		if (m_Runtime)
			m_Runtime.Tick(tDelta);
	}

	//------------------------------------------------------------------------------------------------
	void Blur()
	{
		if (m_Runtime)
			m_Runtime.Blur();
	}

	//------------------------------------------------------------------------------------------------
	void Close()
	{
		if (m_Runtime)
		{
			m_Runtime.Unmount();
			m_Runtime = null;
		}
		if (m_wHost)
		{
			m_wHost.RemoveFromHierarchy();
			m_wHost = null;
		}
		m_wRoot = null;
	}

	//------------------------------------------------------------------------------------------------
	bool IsOpen()
	{
		if (!m_Runtime)
			return false;
		return m_Runtime.IsMounted();
	}

	//------------------------------------------------------------------------------------------------
	MUI_Runtime GetRuntime()
	{
		return m_Runtime;
	}

	//------------------------------------------------------------------------------------------------
	protected void HideLegacyChildren()
	{
		if (!m_wRoot)
			return;

		Widget child = m_wRoot.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			if (child != m_wHost)
				child.SetVisible(false);
			child = next;
		}
	}
}
