//------------------------------------------------------------------------------------------------
//! The window of the leaderboard table the rows scroll in. It clips its child, so a row half
//! scrolled out is cut at the edge, and hands the mouse wheel to the board.
//------------------------------------------------------------------------------------------------
class IA_BoardViewport : MUI_Node
{
	protected IA_LeaderboardBoard m_Board;

	//------------------------------------------------------------------------------------------------
	void IA_BoardViewport()
	{
		m_Style.m_Layout = MUI_LayoutKind.Overlay;
		m_Style.m_WidthMode = MUI_SizeMode.Fill;
		m_Style.m_HeightMode = MUI_SizeMode.Fill;
		m_Style.m_fRadius = 0;
		m_Style.m_Fill = Color.FromInt(0);
		m_Style.m_bClipChildren = true;
		m_Style.m_bBlockHit = true;
	}

	//------------------------------------------------------------------------------------------------
	void SetBoard(IA_LeaderboardBoard board)
	{
		m_Board = board;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMouseWheel(int wheel)
	{
		if (m_Board)
			m_Board.ScrollByWheel(wheel);
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
	}
}
