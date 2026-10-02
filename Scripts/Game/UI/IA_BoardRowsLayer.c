//------------------------------------------------------------------------------------------------
//! The rows of the leaderboard table. It sits inside IA_BoardViewport so what it draws is
//! clipped; the board does the drawing and takes the clicks.
//------------------------------------------------------------------------------------------------
class IA_BoardRowsLayer : MUI_Node
{
	protected IA_LeaderboardBoard m_Board;
	protected bool m_bHasClick;
	protected float m_fClickY;
	protected float m_fClickTime;

	//------------------------------------------------------------------------------------------------
	void IA_BoardRowsLayer()
	{
		m_Style.m_WidthMode = MUI_SizeMode.Fill;
		m_Style.m_HeightMode = MUI_SizeMode.Fill;
		m_Style.m_fRadius = 0;
		m_Style.m_Fill = Color.FromInt(0);
		m_Style.m_bInteractive = true;
	}

	//------------------------------------------------------------------------------------------------
	void SetBoard(IA_LeaderboardBoard board)
	{
		m_Board = board;
	}

	//------------------------------------------------------------------------------------------------
	//! The board is the gamepad stop; this layer only takes the mouse.
	override bool WantsFocus()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDragEnd(float x, float y)
	{
		m_bHasClick = true;
		m_fClickY = y;
		m_fClickTime = GetTime();
	}

	//------------------------------------------------------------------------------------------------
	override void OnClicked()
	{
		bool fresh = m_bHasClick && m_fClickTime == GetTime();
		m_bHasClick = false;
		if (fresh && m_Board)
			m_Board.OnRowsClicked(m_fClickY);
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
		float op = GetDrawOpacity();
		if (op < 0.01 || !m_Board)
			return;
		m_Board.PaintRows(surface, DrawX(), DrawY(), m_World.m_fW, m_World.m_fH, op);
	}
}
