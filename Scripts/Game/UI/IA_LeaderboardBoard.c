//------------------------------------------------------------------------------------------------
//! The leaderboard table: a caption, column heads that sort, a window of rows that scrolls over
//! the whole board, a scroll bar and the player's own line pinned underneath.
//!
//! Consumer:
//!   IA_LeaderboardBoard board = IA_LeaderboardBoard.Create(runtime, "board", model);
//!   board.SetBoard(IA_BoardProtocol.BOARD_GLOBAL, "GLOBAL BOARD");
//!   board.GetOnSortChanged().Insert(OnSort);     // then read GetSort() / IsDescending()
//!   model.FirstWanted(board.GetFirstVisible(), board.GetLastVisible());
//!
//! Layout:
//!   Fill width, Exact height. The rows are painted by a child inside a clipping viewport;
//!   everything else is painted here, outside the viewport's rectangle.
//!
//! Gamepad:
//!   One stop. Up/down move the row cursor, left/right move along the column heads and
//!   Select sorts by the head the cursor is on (again to turn the order round).
//------------------------------------------------------------------------------------------------
class IA_LeaderboardBoard : MUI_Node
{
	static const float CAP_H = 22;
	static const float HEAD_H = 28;
	static const float ROW_H = 30;
	static const int VISIBLE_ROWS = 12;
	static const float MINE_GAP = 8;
	static const float MINE_H = 40;
	static const float GUTTER = 14;

	protected static const int FONT_ROW = 15;
	protected static const int FONT_NOTICE = 14;
	protected static const float NAME_PAD = 12;
	protected static const float WHEEL_ROWS = 3;
	protected static const float THUMB_MIN = 26;
	protected static const float NAV_REPEAT_S = 0.24;

	protected static const int DRAG_NONE = 0;
	protected static const int DRAG_THUMB = 1;
	protected static const int DRAG_OTHER = 2;

	protected static const string TEXT_YOU = "YOU";
	protected static const string TEXT_THIS_SERVER = "THIS SERVER";
	protected static const string TEXT_NOT_RANKED = "NOT ON THIS BOARD YET";
	protected static const string TEXT_LOCATING = "LOCATING YOUR RECORD";
	protected static const string TEXT_NO_RECORD = "RECORD UNAVAILABLE";
	protected static const string TEXT_LOADING = "RECEIVING";
	protected static const string TEXT_NO_ROWS = "NO ENTRIES";

	protected ref IA_BoardModel m_Model;
	protected ref IA_BoardViewport m_Viewport;
	protected ref IA_BoardRowsLayer m_Layer;
	protected ref ScriptInvoker m_OnSortChanged;

	// Columns, left to right.
	protected ref array<int> m_aField = {};
	protected ref array<int> m_aSortKey = {};
	protected ref array<float> m_aW = {};
	protected ref array<float> m_aX = {};
	protected ref array<string> m_aTitle = {};
	protected ref array<float> m_aTitleW = {};
	protected float m_fLaidW = -1;
	protected int m_iSortCol = -1;

	protected int m_iBoard;
	protected int m_iSort;
	protected bool m_bDescending = true;
	protected string m_sBoardCaption;

	protected float m_fScroll;
	protected float m_fScrollTarget;
	protected int m_iCursor = -1;
	protected int m_iHeadCursor = -1;
	protected int m_iPointedRow = -1;
	protected int m_iPointedCol = -1;
	protected bool m_bPointedMine;
	protected bool m_bPointedThumb;
	protected float m_fMineHot;

	protected int m_iDrag;
	protected bool m_bHasClick;
	protected float m_fClickX;
	protected float m_fClickY;
	protected float m_fClickTime;
	protected float m_fNavTime = -10;
	protected int m_iNavStreak;

	protected string m_sNoticeTitle;
	protected string m_sNoticeBody;
	protected string m_sCapLeft;
	protected string m_sCapRight;
	protected bool m_bCapDirty = true;
	protected int m_iCapFirst = -1;
	protected int m_iCapTotal = -2;

	// Colours of one paint pass; filled once so a row costs no allocation.
	protected ref Color m_cText = new Color(1, 1, 1, 1);
	protected ref Color m_cDim = new Color(1, 1, 1, 1);
	protected ref Color m_cHot = new Color(1, 1, 1, 1);
	protected ref Color m_cMuted = new Color(1, 1, 1, 1);
	protected ref Color m_cGreen = new Color(1, 1, 1, 1);
	protected ref Color m_cInk = new Color(1, 1, 1, 1);
	protected ref Color m_cTone = new Color(1, 1, 1, 1);
	protected ref Color m_cLine = new Color(1, 1, 1, 1);
	protected ref Color m_cZebra = new Color(1, 1, 1, 1);
	protected ref Color m_cBar = new Color(1, 1, 1, 1);
	protected ref Color m_cBarTrack = new Color(1, 1, 1, 1);
	protected ref Color m_cGhost = new Color(1, 1, 1, 1);
	protected ref array<float> m_aPoly = {};

	//------------------------------------------------------------------------------------------------
	void IA_LeaderboardBoard()
	{
		m_OnSortChanged = new ScriptInvoker();
		m_Style.m_Layout = MUI_LayoutKind.Overlay;
		m_Style.m_WidthMode = MUI_SizeMode.Fill;
		m_Style.m_HeightMode = MUI_SizeMode.Exact;
		float height = CAP_H + HEAD_H + ROW_H * VISIBLE_ROWS + MINE_GAP + MINE_H;
		m_Style.m_fHeight = height;
		m_Style.m_fMinHeight = height;
		m_Style.m_fRadius = 0;
		m_Style.m_Fill = Color.FromInt(0);
		m_Style.m_bInteractive = true;
		m_Style.SetPaddingTRBL(CAP_H + HEAD_H, GUTTER, MINE_GAP + MINE_H, 0);
	}

	//------------------------------------------------------------------------------------------------
	static IA_LeaderboardBoard Create(notnull MUI_Runtime runtime, string name, notnull IA_BoardModel model)
	{
		ref IA_LeaderboardBoard board = new IA_LeaderboardBoard();
		runtime.Adopt(board);
		board.SetName(name);
		board.m_Model = model;

		ref IA_BoardViewport viewport = new IA_BoardViewport();
		runtime.Adopt(viewport);
		viewport.SetName(name + "View");
		viewport.SetBoard(board);
		board.m_Viewport = viewport;
		board.AddChild(viewport);

		ref IA_BoardRowsLayer layer = new IA_BoardRowsLayer();
		runtime.Adopt(layer);
		layer.SetName(name + "Rows");
		layer.SetBoard(board);
		board.m_Layer = layer;
		viewport.AddChild(layer);

		board.SetBoard(IA_BoardProtocol.BOARD_SESSION, "");
		return board;
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnSortChanged()
	{
		return m_OnSortChanged;
	}

	//------------------------------------------------------------------------------------------------
	int GetSort()
	{
		return m_iSort;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDescending()
	{
		return m_bDescending;
	}

	//------------------------------------------------------------------------------------------------
	int GetBoard()
	{
		return m_iBoard;
	}

	//------------------------------------------------------------------------------------------------
	//! Show another board. A sort the new board has no column for falls back to score.
	//! \param caption what the caption strip calls the board
	void SetBoard(int board, string caption)
	{
		m_iBoard = board;
		m_sBoardCaption = caption;
		BuildColumns();
		if (FindSortColumn(m_iSort) < 0)
		{
			m_iSort = IA_BoardProtocol.SORT_SCORE;
			m_bDescending = true;
		}
		m_iSortCol = FindSortColumn(m_iSort);
		m_iHeadCursor = m_iSortCol;
		ResetView();
	}

	//------------------------------------------------------------------------------------------------
	//! Sort without telling the listeners; the menu uses it to restore a board's last order.
	void SetSort(int sortKey, bool descending)
	{
		if (FindSortColumn(sortKey) < 0)
			return;
		m_iSort = sortKey;
		m_bDescending = descending;
		m_iSortCol = FindSortColumn(m_iSort);
		m_iHeadCursor = m_iSortCol;
		ResetView();
	}

	//------------------------------------------------------------------------------------------------
	//! Text shown in the middle of the table when it has no rows to draw. Empty clears it.
	void SetNotice(string title, string body)
	{
		m_sNoticeTitle = title;
		m_sNoticeBody = body;
		// The caption over the table says "receiving" only while there is no notice.
		m_iCapFirst = -1;
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	//! \return first row the table is heading for; rows are asked for from here
	int GetFirstVisible()
	{
		int first = Math.Floor(m_fScrollTarget);
		if (first < 0)
			first = 0;
		return first;
	}

	//------------------------------------------------------------------------------------------------
	int GetLastVisible()
	{
		return GetFirstVisible() + VISIBLE_ROWS;
	}

	//------------------------------------------------------------------------------------------------
	//! Bring a row to the middle of the table and put the cursor on it.
	void ScrollToRow(int index)
	{
		if (index < 0)
			index = 0;
		m_iCursor = index;
		float target = index - (VISIBLE_ROWS - 1) * 0.5;
		SetScrollTarget(Math.Floor(target));
	}

	//------------------------------------------------------------------------------------------------
	void ScrollToTop()
	{
		m_iCursor = -1;
		SetScrollTarget(0);
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when the player has no line on this board
	bool ScrollToMine()
	{
		IA_BoardRow mine = m_Model.GetMine();
		if (!mine || mine.m_iRank < 1)
			return false;
		ScrollToRow(mine.m_iRank - 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void ScrollByWheel(int wheel)
	{
		SetScrollTarget(m_fScrollTarget - wheel * WHEEL_ROWS);
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetView()
	{
		m_fScroll = 0;
		m_fScrollTarget = 0;
		m_iCursor = -1;
		m_iPointedRow = -1;
		m_bCapDirty = true;
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	//! Rows the table lays out: the board's size, or a screenful of placeholders while the
	//! first answer is on its way.
	protected int RowCount()
	{
		int total = m_Model.GetTotal();
		if (total >= 0)
			return total;
		if (m_sNoticeTitle.IsEmpty())
			return VISIBLE_ROWS;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected float MaxScroll()
	{
		float maxScroll = RowCount() - VISIBLE_ROWS;
		if (maxScroll < 0)
			maxScroll = 0;
		return maxScroll;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetScrollTarget(float target)
	{
		float maxScroll = MaxScroll();
		if (target > maxScroll)
			target = maxScroll;
		if (target < 0)
			target = 0;
		m_fScrollTarget = target;

		// A long jump lands near its end and eases in, instead of streaking past thousands of rows.
		float lead = VISIBLE_ROWS * 1.5;
		if (m_fScroll < target - lead)
			m_fScroll = target - lead;
		if (m_fScroll > target + lead)
			m_fScroll = target + lead;
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	//! Scroll just far enough for a row to be in the table.
	protected void Reveal(int index)
	{
		float target = m_fScrollTarget;
		if (index < target)
			target = index;
		if (index > target + VISIBLE_ROWS - 1)
			target = index - VISIBLE_ROWS + 1;
		SetScrollTarget(target);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddColumn(int field, string title, float width, int sortKey)
	{
		m_aField.Insert(field);
		m_aTitle.Insert(title);
		m_aW.Insert(width);
		m_aX.Insert(0);
		m_aTitleW.Insert(0);
		m_aSortKey.Insert(sortKey);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildColumns()
	{
		m_aField.Clear();
		m_aTitle.Clear();
		m_aW.Clear();
		m_aX.Clear();
		m_aTitleW.Clear();
		m_aSortKey.Clear();
		m_fLaidW = -1;

		bool servers = m_iBoard == IA_BoardProtocol.BOARD_SERVERS;
		bool session = m_iBoard == IA_BoardProtocol.BOARD_SESSION;

		AddColumn(IA_BoardRow.F_RANK, "#", 60, -1);
		if (session)
			AddColumn(IA_BoardRow.F_GRADE, "GRADE", 58, -1);
		if (servers)
		{
			AddColumn(IA_BoardRow.F_NAME, "SERVER", 0, -1);
			AddColumn(IA_BoardRow.F_PLAYERS, "PLAYERS", 84, IA_BoardProtocol.SORT_PLAYERS);
		}
		else
		{
			AddColumn(IA_BoardRow.F_NAME, "PLAYER", 0, -1);
		}
		AddColumn(IA_BoardRow.F_KILLS, "KILLS", 64, IA_BoardProtocol.SORT_KILLS);
		AddColumn(IA_BoardRow.F_DEATHS, "DEATHS", 64, IA_BoardProtocol.SORT_DEATHS);
		AddColumn(IA_BoardRow.F_KD, "K/D", 64, IA_BoardProtocol.SORT_KD);
		AddColumn(IA_BoardRow.F_HVT, "HVT", 60, IA_BoardProtocol.SORT_HVT);
		AddColumn(IA_BoardRow.F_GUARD, "GUARD", 60, IA_BoardProtocol.SORT_GUARD);
		if (!servers)
			AddColumn(IA_BoardRow.F_OBJ, "OBJ", 64, IA_BoardProtocol.SORT_OBJ);
		AddColumn(IA_BoardRow.F_FLIGHT, "FLIGHT", 84, IA_BoardProtocol.SORT_TRANSPORT);
		AddColumn(IA_BoardRow.F_LIFTS, "LIFTS", 64, IA_BoardProtocol.SORT_INSERTIONS);
		if (session)
			AddColumn(IA_BoardRow.F_SCORE, "XP", 120, IA_BoardProtocol.SORT_SCORE);
		else
			AddColumn(IA_BoardRow.F_SCORE, "SCORE", 120, IA_BoardProtocol.SORT_SCORE);
	}

	//------------------------------------------------------------------------------------------------
	//! Give the name column what the fixed columns leave of the table's width.
	protected void LayoutColumns(float tableW)
	{
		if (m_fLaidW == tableW)
			return;
		m_fLaidW = tableW;

		int count = m_aField.Count();
		float fixedW = 0;
		int c;
		for (c = 0; c < count; c++)
		{
			if (m_aField[c] != IA_BoardRow.F_NAME)
				fixedW = fixedW + m_aW[c];
		}
		float nameW = tableW - fixedW;
		if (nameW < 120)
			nameW = 120;

		float x = 0;
		for (c = 0; c < count; c++)
		{
			if (m_aField[c] == IA_BoardRow.F_NAME)
				m_aW[c] = nameW;
			m_aX[c] = x;
			x = x + m_aW[c];
			m_aTitleW[c] = IA_TrackedText.Measure(m_Runtime, m_aTitle[c], IA_UplinkStyle.FONT_CAP, IA_UplinkStyle.TRACK_CAP);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected int FindSortColumn(int sortKey)
	{
		int count = m_aSortKey.Count();
		for (int c = 0; c < count; c++)
		{
			if (m_aSortKey[c] == sortKey)
				return c;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The player picked a column head: sort by it, or turn the order round when it already is.
	protected void ChooseColumn(int column)
	{
		if (column < 0 || column >= m_aSortKey.Count())
			return;
		int sortKey = m_aSortKey[column];
		if (sortKey < 0)
			return;

		if (sortKey == m_iSort)
		{
			m_bDescending = !m_bDescending;
		}
		else
		{
			m_iSort = sortKey;
			m_bDescending = true;
		}
		m_iSortCol = column;
		m_iHeadCursor = column;
		ResetView();
		IA_UplinkStyle.Click();
		m_OnSortChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Probe and menu entry to the same path a click on a column head takes.
	void ChooseSort(int sortKey)
	{
		ChooseColumn(FindSortColumn(sortKey));
	}

	//------------------------------------------------------------------------------------------------
	protected bool UsingMouse()
	{
		InputManager im = GetGame().GetInputManager();
		if (!im)
			return true;
		return im.IsUsingMouseAndKeyboard();
	}

	//------------------------------------------------------------------------------------------------
	protected float TableWidth()
	{
		return m_World.m_fW - GUTTER;
	}

	//------------------------------------------------------------------------------------------------
	protected float RowsTop()
	{
		return DrawY() + CAP_H + HEAD_H;
	}

	//------------------------------------------------------------------------------------------------
	protected float RowsHeight()
	{
		return ROW_H * VISIBLE_ROWS;
	}

	//------------------------------------------------------------------------------------------------
	protected float ThumbHeight()
	{
		int rows = RowCount();
		float trackH = RowsHeight();
		if (rows <= VISIBLE_ROWS)
			return trackH;
		float thumbH = trackH * VISIBLE_ROWS / rows;
		if (thumbH < THUMB_MIN)
			thumbH = THUMB_MIN;
		return thumbH;
	}

	//------------------------------------------------------------------------------------------------
	//! The column a host-local x falls in, -1 outside the table.
	protected int ColumnAt(float px)
	{
		float rel = px - DrawX();
		int count = m_aX.Count();
		for (int c = 0; c < count; c++)
		{
			if (rel >= m_aX[c] && rel < m_aX[c] + m_aW[c])
				return c;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected bool InHead(float px, float py)
	{
		float top = DrawY() + CAP_H;
		if (py < top || py >= top + HEAD_H)
			return false;
		return px >= DrawX() && px < DrawX() + TableWidth();
	}

	//------------------------------------------------------------------------------------------------
	protected bool InGutter(float px, float py)
	{
		float top = RowsTop();
		if (py < top || py > top + RowsHeight())
			return false;
		float left = DrawX() + TableWidth();
		return px >= left && px <= left + GUTTER;
	}

	//------------------------------------------------------------------------------------------------
	protected bool InMine(float px, float py)
	{
		float top = RowsTop() + RowsHeight() + MINE_GAP;
		if (py < top || py > top + MINE_H)
			return false;
		return px >= DrawX() && px <= DrawX() + m_World.m_fW;
	}

	//------------------------------------------------------------------------------------------------
	//! Put the middle of the scroll thumb under a host-local y.
	protected void ScrollToPointer(float py)
	{
		float thumbH = ThumbHeight();
		float span = RowsHeight() - thumbH;
		if (span < 1)
			return;
		float t = (py - RowsTop() - thumbH * 0.5) / span;
		if (t < 0)
			t = 0;
		if (t > 1)
			t = 1;
		float target = Math.Round(t * MaxScroll());
		m_fScrollTarget = target;
		m_fScroll = target;
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDrag(float x, float y)
	{
		if (m_iDrag == DRAG_NONE)
		{
			m_iDrag = DRAG_OTHER;
			if (InGutter(x, y) && MaxScroll() > 0)
				m_iDrag = DRAG_THUMB;
		}
		if (m_iDrag == DRAG_THUMB)
			ScrollToPointer(y);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDragEnd(float x, float y)
	{
		bool thumb = m_iDrag == DRAG_THUMB;
		m_iDrag = DRAG_NONE;
		if (thumb)
			return;
		m_bHasClick = true;
		m_fClickX = x;
		m_fClickY = y;
		m_fClickTime = GetTime();
	}

	//------------------------------------------------------------------------------------------------
	override void OnClicked()
	{
		bool fresh = m_bHasClick && m_fClickTime == GetTime();
		m_bHasClick = false;
		if (!fresh)
		{
			// Gamepad Select: sort by the head the cursor is on.
			if (!UsingMouse())
				ChooseColumn(m_iHeadCursor);
			return;
		}

		if (InHead(m_fClickX, m_fClickY))
		{
			ChooseColumn(ColumnAt(m_fClickX));
			return;
		}
		if (InGutter(m_fClickX, m_fClickY))
		{
			ScrollToPointer(m_fClickY);
			return;
		}
		if (InMine(m_fClickX, m_fClickY))
		{
			if (ScrollToMine())
				IA_UplinkStyle.Click();
			else
				IA_UplinkStyle.ClickFail();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A click inside the rows: the row under it takes the cursor.
	void OnRowsClicked(float py)
	{
		int index = RowAt(py);
		if (index < 0)
			return;
		m_iCursor = index;
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	protected int RowAt(float py)
	{
		float rel = (py - RowsTop()) / ROW_H + m_fScroll;
		if (rel < 0)
			return -1;
		int index = Math.Floor(rel);
		if (index >= RowCount())
			return -1;
		return index;
	}

	//------------------------------------------------------------------------------------------------
	//! Rows a held direction moves by: one at a time, then faster the longer it is held.
	protected int NavStep()
	{
		float now = GetTime();
		if (now - m_fNavTime < NAV_REPEAT_S)
			m_iNavStreak = m_iNavStreak + 1;
		else
			m_iNavStreak = 0;
		m_fNavTime = now;

		if (m_iNavStreak < 6)
			return 1;
		if (m_iNavStreak < 14)
			return 3;
		if (m_iNavStreak < 26)
			return 8;
		return IA_BoardProtocol.PAGE_ROWS;
	}

	//------------------------------------------------------------------------------------------------
	override bool HandleNavAxis(int dirX, int dirY)
	{
		if (dirX != 0)
		{
			int count = m_aSortKey.Count();
			int column = m_iHeadCursor + dirX;
			while (column >= 0 && column < count)
			{
				if (m_aSortKey[column] >= 0)
				{
					m_iHeadCursor = column;
					IA_UplinkStyle.Hover();
					InvalidatePaint();
					break;
				}
				column = column + dirX;
			}
			return true;
		}

		int rows = RowCount();
		if (rows < 1)
			return false;

		if (m_iCursor < 0)
		{
			m_iCursor = GetFirstVisible();
			Reveal(m_iCursor);
			return true;
		}

		int next = m_iCursor + dirY * NavStep();
		if (next < 0)
		{
			if (m_iCursor == 0)
				return false;
			next = 0;
		}
		if (next >= rows)
		{
			if (m_iCursor >= rows - 1)
				return false;
			next = rows - 1;
		}
		m_iCursor = next;
		Reveal(next);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnTick(float dt)
	{
		float maxScroll = MaxScroll();
		if (m_fScrollTarget > maxScroll)
			m_fScrollTarget = maxScroll;
		m_fScroll = MUI_Ease.Approach(m_fScroll, m_fScrollTarget, dt, 16);
		if (Math.AbsFloat(m_fScroll - m_fScrollTarget) < 0.004)
			m_fScroll = m_fScrollTarget;

		m_iPointedRow = -1;
		m_iPointedCol = -1;
		m_bPointedMine = false;
		m_bPointedThumb = m_iDrag == DRAG_THUMB;

		float px;
		float py;
		if (m_Runtime && UsingMouse() && m_Runtime.GetLocalPointer(px, py))
		{
			if (m_Layer && m_Layer.IsHover())
			{
				m_iPointedRow = RowAt(py);
			}
			else if (IsHover())
			{
				if (InHead(px, py))
				{
					int column = ColumnAt(px);
					if (column >= 0 && m_aSortKey[column] >= 0)
						m_iPointedCol = column;
				}
				else if (InGutter(px, py))
				{
					m_bPointedThumb = true;
				}
				else if (InMine(px, py))
				{
					m_bPointedMine = true;
				}
			}
		}

		float mineTarget = 0;
		if (m_bPointedMine)
			mineTarget = 1;
		m_fMineHot = MUI_Ease.Approach(m_fMineHot, mineTarget, dt, 14);
	}

	//------------------------------------------------------------------------------------------------
	protected void Tint(notnull Color dest, notnull Color src, float opacity)
	{
		float a = src.A() * opacity;
		if (a < 0)
			a = 0;
		if (a > 1)
			a = 1;
		dest.SetR(src.R());
		dest.SetG(src.G());
		dest.SetB(src.B());
		dest.SetA(a);
	}

	//------------------------------------------------------------------------------------------------
	protected void PrepareColours(float op)
	{
		IA_UplinkStyle look = IA_UplinkStyle.Get();
		Tint(m_cText, look.m_White, op * 0.92);
		Tint(m_cDim, look.m_Muted, op * 0.42);
		Tint(m_cHot, look.m_Gold, op);
		Tint(m_cMuted, look.m_Muted, op);
		Tint(m_cGreen, look.m_Green, op);
		Tint(m_cInk, look.m_Black, op * 0.92);
		Tint(m_cTone, look.m_Tone, op);
		Tint(m_cLine, look.m_Tone, op * 0.06);
		Tint(m_cZebra, look.m_White, op * 0.022);
		Tint(m_cBar, look.m_Tone, op * 0.75);
		Tint(m_cBarTrack, look.m_Tone, op * 0.10);
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildCaption()
	{
		m_bCapDirty = false;
		string order = "HIGH TO LOW";
		if (!m_bDescending)
			order = "LOW TO HIGH";
		string column = "SCORE";
		if (m_iSortCol >= 0 && m_iSortCol < m_aTitle.Count())
			column = m_aTitle[m_iSortCol];
		m_sCapLeft = string.Format("%1  /  SORTED BY %2  /  %3", m_sBoardCaption, column, order);
		m_iCapFirst = -1;
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildRange()
	{
		int total = m_Model.GetTotal();
		int first = Math.Round(m_fScroll);
		if (first == m_iCapFirst && total == m_iCapTotal)
			return;
		m_iCapFirst = first;
		m_iCapTotal = total;

		if (total < 0)
		{
			if (m_sNoticeTitle.IsEmpty())
				m_sCapRight = TEXT_LOADING;
			else
				m_sCapRight = "";
			return;
		}
		if (total == 0)
		{
			m_sCapRight = TEXT_NO_ROWS;
			return;
		}

		int last = first + VISIBLE_ROWS;
		if (last > total)
			last = total;
		m_sCapRight = string.Format("ROWS %1-%2 OF %3", IA_PilotDropoffPayload.FormatNumber(first + 1), IA_PilotDropoffPayload.FormatNumber(last), IA_PilotDropoffPayload.FormatNumber(total));
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
		float op = GetDrawOpacity();
		if (op < 0.01 || !m_Model)
			return;

		IA_UplinkStyle look = IA_UplinkStyle.Get();
		float x = DrawX();
		float y = DrawY();
		float w = m_World.m_fW;
		float tableW = TableWidth();
		float headY = y + CAP_H;
		float rowsY = headY + HEAD_H;
		float rowsH = RowsHeight();

		LayoutColumns(tableW);
		if (m_bCapDirty)
			RebuildCaption();
		RebuildRange();
		PrepareColours(op);

		// Caption strip.
		float rightW = 0;
		if (!m_sCapRight.IsEmpty())
		{
			rightW = IA_TrackedText.Measure(m_Runtime, m_sCapRight, IA_UplinkStyle.FONT_CAP, IA_UplinkStyle.TRACK_CAP) + 12;
			IA_TrackedText.DrawRight(surface, m_Runtime, x + w, y + 2, 12, m_sCapRight, IA_UplinkStyle.FONT_CAP, IA_UplinkStyle.TRACK_CAP, m_cMuted);
		}
		look.Caption(surface, m_Runtime, x, y + 2, w - rightW, m_sCapLeft, look.m_Tone, op);

		DrawHead(surface, x, headY, tableW, op);

		// The well the rows sit in.
		surface.FillRect(x, rowsY, tableW, rowsH, MUI_ColorUtil.Fade(look.m_Black, op * 0.22), 0);
		surface.FillRect(x, rowsY + rowsH, tableW, 1, MUI_ColorUtil.Fade(look.m_Tone, op * 0.16), 0);

		DrawScrollBar(surface, x + tableW, rowsY, rowsH, op);
		DrawMine(surface, x, rowsY + rowsH + MINE_GAP, w, op);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawHead(MUI_RenderSurface surface, float x, float y, float tableW, float op)
	{
		IA_UplinkStyle look = IA_UplinkStyle.Get();
		surface.FillRect(x, y, tableW, HEAD_H, MUI_ColorUtil.Fade(look.m_Black, op * 0.34), 0);
		surface.FillRect(x, y + HEAD_H - 1, tableW, 1, MUI_ColorUtil.Fade(look.m_Tone, op * 0.26), 0);

		int count = m_aField.Count();
		for (int c = 0; c < count; c++)
		{
			float cx = x + m_aX[c];
			float cw = m_aW[c];
			bool sortable = m_aSortKey[c] >= 0;
			bool sorted = c == m_iSortCol;

			if (sorted)
			{
				surface.FillRect(cx, y, cw, HEAD_H, MUI_ColorUtil.Fade(look.m_Tone, op * 0.12), 0);
				surface.FillRect(cx, y, cw, 2, m_cTone, 0);
			}
			else if (c == m_iPointedCol)
			{
				surface.FillRect(cx, y, cw, HEAD_H, MUI_ColorUtil.Fade(look.m_Tone, op * 0.07), 0);
			}

			Color ink = m_cMuted;
			if (sorted)
				ink = m_cTone;
			else if (c == m_iPointedCol)
				ink = m_cText;
			else if (!sortable)
				ink = m_cDim;

			float tx = cx + NAME_PAD;
			if (m_aField[c] != IA_BoardRow.F_NAME)
			{
				float used = m_aTitleW[c];
				if (sorted)
					used = used + 11;
				tx = cx + (cw - used) * 0.5;
			}
			IA_TrackedText.Draw(surface, m_Runtime, tx, y, HEAD_H, m_aTitle[c], IA_UplinkStyle.FONT_CAP, IA_UplinkStyle.TRACK_CAP, ink);

			if (sorted)
				DrawArrow(surface, tx + m_aTitleW[c] + 5, y + HEAD_H * 0.5, m_bDescending, m_cTone);
		}

		// Where the gamepad stands on the heads.
		float focus = GetFocusT();
		if (focus > 0.02 && !UsingMouse() && m_iHeadCursor >= 0 && m_iHeadCursor < count)
			look.Brackets(surface, x + m_aX[m_iHeadCursor] + 2, y + 2, m_aW[m_iHeadCursor] - 4, HEAD_H - 5, 7, MUI_ColorUtil.Fade(look.m_Cyan, op * focus), 1.6);
	}

	//------------------------------------------------------------------------------------------------
	//! A small triangle: point down for high to low.
	protected void DrawArrow(MUI_RenderSurface surface, float x, float midY, bool down, Color color)
	{
		m_aPoly.Clear();
		if (down)
		{
			m_aPoly.Insert(x);
			m_aPoly.Insert(midY - 2);
			m_aPoly.Insert(x + 7);
			m_aPoly.Insert(midY - 2);
			m_aPoly.Insert(x + 3.5);
			m_aPoly.Insert(midY + 3);
		}
		else
		{
			m_aPoly.Insert(x + 3.5);
			m_aPoly.Insert(midY - 3);
			m_aPoly.Insert(x + 7);
			m_aPoly.Insert(midY + 2);
			m_aPoly.Insert(x);
			m_aPoly.Insert(midY + 2);
		}
		surface.FillPolygon(m_aPoly, color);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawScrollBar(MUI_RenderSurface surface, float x, float y, float h, float op)
	{
		IA_UplinkStyle look = IA_UplinkStyle.Get();
		float trackX = x + 6;
		surface.FillRect(trackX + 1, y, 2, h, MUI_ColorUtil.Fade(look.m_Tone, op * 0.10), 0);

		float maxScroll = MaxScroll();
		if (maxScroll <= 0)
			return;

		float thumbH = ThumbHeight();
		float t = m_fScroll / maxScroll;
		if (t < 0)
			t = 0;
		if (t > 1)
			t = 1;
		float thumbY = y + (h - thumbH) * t;
		float lit = 0.55;
		if (m_bPointedThumb)
			lit = 1.0;
		surface.FillRect(trackX, thumbY, 4, thumbH, MUI_ColorUtil.Fade(look.m_Tone, op * lit), 0);
		surface.FillRect(trackX - 2, thumbY, 8, 1, MUI_ColorUtil.Fade(look.m_Tone, op * lit), 0);
		surface.FillRect(trackX - 2, thumbY + thumbH - 1, 8, 1, MUI_ColorUtil.Fade(look.m_Tone, op * lit), 0);
	}

	//------------------------------------------------------------------------------------------------
	//! The player's own line, always in view whatever part of the board is scrolled to.
	protected void DrawMine(MUI_RenderSurface surface, float x, float y, float w, float op)
	{
		IA_UplinkStyle look = IA_UplinkStyle.Get();
		IA_BoardRow mine = m_Model.GetMine();
		bool servers = m_iBoard == IA_BoardProtocol.BOARD_SERVERS;

		Color edge = look.m_Tone;
		float edgeOp = 0.18;
		if (mine)
		{
			edge = look.m_Green;
			edgeOp = 0.40 + m_fMineHot * 0.45;
		}
		look.FillChamfer(surface, x, y, w, MINE_H, 5, MUI_ColorUtil.Fade(look.m_Plate, op));
		if (mine)
			look.FillChamfer(surface, x, y, w, MINE_H, 5, MUI_ColorUtil.Fade(look.m_Green, op * (0.035 + m_fMineHot * 0.04)));
		look.StrokeChamfer(surface, x, y, w, MINE_H, 5, MUI_ColorUtil.Fade(edge, op * edgeOp), 1);

		string chip = TEXT_YOU;
		if (servers)
			chip = TEXT_THIS_SERVER;
		float chipW = look.ChipWidth(m_Runtime, chip);

		if (!mine)
		{
			string text = TEXT_NOT_RANKED;
			if (m_Model.GetTotal() < 0)
			{
				text = TEXT_LOCATING;
				if (!m_sNoticeTitle.IsEmpty())
					text = TEXT_NO_RECORD;
			}
			look.Chip(surface, m_Runtime, x + NAME_PAD, y + (MINE_H - IA_UplinkStyle.CHIP_H) * 0.5, chipW, chip, look.m_Muted, op * 0.8);
			IA_TrackedText.Draw(surface, m_Runtime, x + NAME_PAD + chipW + 12, y, MINE_H, text, IA_UplinkStyle.FONT_CAP, IA_UplinkStyle.TRACK_CAP, m_cMuted);
			return;
		}

		surface.FillRect(x, y + 5, 3, MINE_H - 5, m_cGreen, 0);
		PaintCells(surface, x, y, MINE_H, mine, chipW + 10, true);

		int nameCol = FindField(IA_BoardRow.F_NAME);
		if (nameCol >= 0)
			look.Chip(surface, m_Runtime, x + m_aX[nameCol] + NAME_PAD, y + (MINE_H - IA_UplinkStyle.CHIP_H) * 0.5, chipW, chip, look.m_Green, op);
	}

	//------------------------------------------------------------------------------------------------
	protected int FindField(int field)
	{
		int count = m_aField.Count();
		for (int c = 0; c < count; c++)
		{
			if (m_aField[c] == field)
				return c;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Cut a name to its column once; a row keeps the result until the column changes width.
	protected void FitName(notnull IA_BoardRow row, float availW)
	{
		if (row.m_fFitW == availW)
			return;
		row.m_fFitW = availW;
		row.m_sFit = row.m_sLabel;

		// The widest glyphs are about 13 px at this size; shorter names need no measuring.
		int length = row.m_sLabel.Length();
		if (length * 13.5 <= availW)
			return;

		float width = IA_TrackedText.Width(m_Runtime, row.m_sLabel, FONT_ROW);
		if (width <= availW)
			return;

		int keep = length * (availW / width) - 3;
		if (keep < 1)
			keep = 1;
		row.m_sFit = row.m_sLabel.Substring(0, keep) + "...";
	}

	//------------------------------------------------------------------------------------------------
	//! The cells of one row. PrepareColours must have run in this paint pass.
	//! \param nameInset room left in front of the name for a chip
	protected void PaintCells(MUI_RenderSurface surface, float x, float y, float h, notnull IA_BoardRow row, float nameInset, bool mine)
	{
		IA_UplinkStyle look = IA_UplinkStyle.Get();
		int count = m_aField.Count();
		for (int c = 0; c < count; c++)
		{
			int field = m_aField[c];
			float cx = x + m_aX[c];
			float cw = m_aW[c];

			if (field == IA_BoardRow.F_NAME)
			{
				float nameW = cw - NAME_PAD - nameInset - 6;
				FitName(row, nameW);
				Color nameInk = m_cText;
				if (mine)
					nameInk = m_cGreen;
				surface.DrawText(cx + NAME_PAD + nameInset, y, nameW, h, row.m_sFit, FONT_ROW, nameInk, true, false, true, false, true);
				continue;
			}

			if (field == IA_BoardRow.F_RANK)
			{
				// Medals go to the best three, so only while the best are at the top.
				if (m_bDescending && row.m_iRank >= 1 && row.m_iRank <= 3)
				{
					Color medal = look.m_Gold;
					if (row.m_iRank == 2)
						medal = look.m_Silver;
					if (row.m_iRank == 3)
						medal = look.m_Bronze;
					look.FillSlant(surface, cx + cw * 0.5 - 15, y + (h - 16) * 0.5, 26, 16, 4, MUI_ColorUtil.Fade(medal, m_cText.A()));
					surface.DrawText(cx, y, cw, h, row.Text(field), FONT_ROW, m_cInk, true, true, true, false, true);
				}
				else
				{
					surface.DrawText(cx, y, cw, h, row.Text(field), FONT_ROW, m_cMuted, false, true, true, false, true);
				}
				continue;
			}

			string text = row.Text(field);
			if (field == IA_BoardRow.F_GRADE)
			{
				float gradeW = IA_TrackedText.Measure(m_Runtime, text, IA_UplinkStyle.FONT_CAP, IA_UplinkStyle.TRACK_CAP);
				IA_TrackedText.Draw(surface, m_Runtime, cx + (cw - gradeW) * 0.5, y, h, text, IA_UplinkStyle.FONT_CAP, IA_UplinkStyle.TRACK_CAP, m_cTone);
				continue;
			}

			if (c == m_iSortCol)
			{
				surface.DrawText(cx, y, cw, h, text, FONT_ROW, m_cHot, true, true, true, false, true);
				continue;
			}

			Color ink = m_cText;
			if (text == "0" || text == "0.00")
				ink = m_cDim;
			surface.DrawText(cx, y, cw, h, text, FONT_ROW, ink, false, true, true, false, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Bars standing in for a row that has not arrived.
	protected void PaintGhost(MUI_RenderSurface surface, float x, float y, int index, float op)
	{
		IA_UplinkStyle look = IA_UplinkStyle.Get();
		float beat = MUI_Ease.Pulse(GetTime() + index * 0.11, 0.9);
		Tint(m_cGhost, look.m_Tone, op * (0.05 + 0.06 * beat));

		int count = m_aField.Count();
		for (int c = 0; c < count; c++)
		{
			float cx = x + m_aX[c];
			float cw = m_aW[c];
			if (m_aField[c] == IA_BoardRow.F_NAME)
			{
				float span = cw * (0.30 + 0.25 * MUI_Ease.Fract(index * 0.37));
				surface.FillRect(cx + NAME_PAD, y + 11, span, 8, m_cGhost, 0);
				continue;
			}
			surface.FillRect(cx + cw * 0.30, y + 11, cw * 0.40, 8, m_cGhost, 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the rows layer, on the viewport's clipped surface.
	void PaintRows(MUI_RenderSurface surface, float x, float y, float w, float h, float op)
	{
		if (!m_Model)
			return;

		IA_UplinkStyle look = IA_UplinkStyle.Get();
		LayoutColumns(w);
		PrepareColours(op);

		int rows = RowCount();
		if (rows < 1)
		{
			DrawNotice(surface, x, y, w, h, op);
			return;
		}

		// The column the board is sorted by runs lit down the table.
		float sortX = 0;
		float sortW = 0;
		if (m_iSortCol >= 0 && m_iSortCol < m_aX.Count())
		{
			sortX = x + m_aX[m_iSortCol];
			sortW = m_aW[m_iSortCol];
			// On a board shorter than the table it stops with the last row.
			float sortH = h;
			if (rows * ROW_H < sortH)
				sortH = rows * ROW_H;
			surface.FillRect(sortX, y, sortW, sortH, MUI_ColorUtil.Fade(look.m_Tone, op * 0.05), 0);
		}

		// What the best row scored, to size every row's bar against.
		float best = 0;
		IA_BoardRow leader = m_Model.GetRow(0);
		if (leader && m_bDescending)
			best = leader.SortValue(m_iSort);

		bool pad = !UsingMouse();
		float focus = GetFocusT();
		int first = Math.Floor(m_fScroll);
		if (first < 0)
			first = 0;
		int last = first + VISIBLE_ROWS + 1;
		if (last > rows)
			last = rows;

		for (int index = first; index < last; index++)
		{
			float rowY = y + (index - m_fScroll) * ROW_H;
			IA_BoardRow row = m_Model.GetRow(index);
			bool mine = false;
			if (row)
				mine = m_Model.IsMine(index);

			if (index % 2 == 1)
				surface.FillRect(x, rowY, w, ROW_H, m_cZebra, 0);
			surface.FillRect(x, rowY + ROW_H - 1, w, 1, m_cLine, 0);

			if (mine)
			{
				surface.FillRect(x, rowY, w, ROW_H - 1, MUI_ColorUtil.Fade(look.m_Green, op * 0.07), 0);
				surface.FillRect(x, rowY, 3, ROW_H - 1, m_cGreen, 0);
			}
			if (index == m_iPointedRow)
				surface.FillRect(x, rowY, w, ROW_H - 1, MUI_ColorUtil.Fade(look.m_Tone, op * 0.07), 0);
			if (index == m_iCursor)
			{
				float mark = 0.55;
				if (pad)
					mark = 0.35 + 0.65 * focus;
				surface.FillRect(x, rowY, w, ROW_H - 1, MUI_ColorUtil.Fade(look.m_Cyan, op * 0.05 * mark), 0);
				look.Brackets(surface, x + 1, rowY + 1, w - 2, ROW_H - 3, 7, MUI_ColorUtil.Fade(look.m_Cyan, op * mark), 1.4);
			}

			if (!row)
			{
				PaintGhost(surface, x, rowY, index, op);
				continue;
			}

			if (best > 0 && sortW > 0)
			{
				float share = row.SortValue(m_iSort) / best;
				if (share > 1)
					share = 1;
				if (share < 0)
					share = 0;
				surface.FillRect(sortX + 10, rowY + ROW_H - 5, sortW - 20, 2, m_cBarTrack, 0);
				surface.FillRect(sortX + 10, rowY + ROW_H - 5, (sortW - 20) * share, 2, m_cBar, 0);
			}

			PaintCells(surface, x, rowY, ROW_H, row, 0, mine);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Why the table is empty.
	protected void DrawNotice(MUI_RenderSurface surface, float x, float y, float w, float h, float op)
	{
		IA_UplinkStyle look = IA_UplinkStyle.Get();
		string title = m_sNoticeTitle;
		if (title.IsEmpty())
			title = TEXT_NO_ROWS;

		float cx = x + w * 0.5;
		float cy = y + h * 0.5 - 34;
		// A still mark: nothing is on its way here. A board that could not be read is struck through.
		surface.StrokeCircle(cx, cy, 16, MUI_ColorUtil.Fade(look.m_Tone, op * 0.55), 1.4);
		look.Brackets(surface, cx - 26, cy - 26, 52, 52, 7, MUI_ColorUtil.Fade(look.m_Tone, op * 0.35), 1.2);
		if (m_sNoticeTitle.IsEmpty())
			surface.FillCircle(cx, cy, 3, MUI_ColorUtil.Fade(look.m_Tone, op * 0.8));
		else
			surface.DrawLine(cx - 11, cy + 11, cx + 11, cy - 11, MUI_ColorUtil.Fade(look.m_Tone, op * 0.9), 1.6);

		float titleW = IA_TrackedText.Measure(m_Runtime, title, IA_UplinkStyle.FONT_TAB, IA_UplinkStyle.TRACK_TAB);
		IA_TrackedText.Draw(surface, m_Runtime, cx - titleW * 0.5, cy + 34, 14, title, IA_UplinkStyle.FONT_TAB, IA_UplinkStyle.TRACK_TAB, m_cTone);
		if (!m_sNoticeBody.IsEmpty())
			surface.DrawText(x + 40, cy + 54, w - 80, 44, m_sNoticeBody, FONT_NOTICE, m_cMuted, false, true, false, true, true);
	}
}
