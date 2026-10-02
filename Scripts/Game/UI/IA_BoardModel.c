//------------------------------------------------------------------------------------------------
//! Client: the part of one board the menu holds. Rows arrive a page at a time and are kept by
//! their place on the board, so the table can be scrolled anywhere and only what is looked at
//! is ever asked for.
//------------------------------------------------------------------------------------------------
class IA_BoardModel
{
	static const int PAGE_NONE = 0;
	static const int PAGE_ASKED = 1;
	static const int PAGE_LOADED = 2;
	static const int PAGE_STALE = 3;	// shown, but due a refresh

	protected static const int KEEP_PAGES = 40;

	protected ref map<int, ref IA_BoardRow> m_mRows = new map<int, ref IA_BoardRow>();
	protected ref map<int, int> m_mPages = new map<int, int>();
	protected ref IA_BoardRow m_Mine;
	protected int m_iTotal = -1;
	protected int m_iFill;
	protected int m_iFillPage = -1;

	//------------------------------------------------------------------------------------------------
	void Reset()
	{
		m_mRows.Clear();
		m_mPages.Clear();
		m_Mine = null;
		m_iTotal = -1;
		m_iFill = 0;
		m_iFillPage = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return rows on the board, -1 until the first answer
	int GetTotal()
	{
		return m_iTotal;
	}

	//------------------------------------------------------------------------------------------------
	IA_BoardRow GetRow(int index)
	{
		IA_BoardRow row;
		if (!m_mRows.Find(index, row))
			return null;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the asking player's own line, null when they are not on the board
	IA_BoardRow GetMine()
	{
		return m_Mine;
	}

	//------------------------------------------------------------------------------------------------
	bool IsMine(int index)
	{
		return m_Mine && m_Mine.m_iRank == index + 1;
	}

	//------------------------------------------------------------------------------------------------
	int PageState(int page)
	{
		int state;
		if (!m_mPages.Find(page, state))
			return PAGE_NONE;
		return state;
	}

	//------------------------------------------------------------------------------------------------
	bool HasRows()
	{
		return !m_mRows.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	void MarkAsked(int page)
	{
		m_mPages.Set(page, PAGE_ASKED);
	}

	//------------------------------------------------------------------------------------------------
	//! The request for a page got no rows; it may be asked again.
	void Fail(int page)
	{
		if (PageState(page) != PAGE_ASKED)
			return;
		if (m_mRows.Contains(page * IA_BoardProtocol.PAGE_ROWS))
			m_mPages.Set(page, PAGE_STALE);
		else
			m_mPages.Remove(page);
	}

	//------------------------------------------------------------------------------------------------
	//! The head of an answered page.
	//! \param offset index of the page's first row
	//! \param mine the player's own packed line, empty when they are not on the board
	void OnHead(int total, int offset, string mine)
	{
		if (total < 0)
			total = 0;
		m_iTotal = total;
		m_Mine = null;
		if (!mine.IsEmpty())
			m_Mine = IA_BoardRow.Unpack(mine);
		m_iFill = offset;
		m_iFillPage = offset / IA_BoardProtocol.PAGE_ROWS;
	}

	//------------------------------------------------------------------------------------------------
	//! One chunk of the page OnHead opened.
	//! \param offset index of the chunk's first row
	//! \return the page that is now complete, -1 while more chunks are due
	int OnRows(int offset, bool last, string rows)
	{
		if (m_iFillPage < 0)
			return -1;

		int index = offset;
		if (!rows.IsEmpty())
		{
			ref array<string> lines = {};
			rows.Split("\n", lines, true);
			int count = lines.Count();
			for (int i = 0; i < count; i++)
			{
				ref IA_BoardRow row = IA_BoardRow.Unpack(lines[i]);
				if (!row)
					continue;
				m_mRows.Set(index, row);
				index = index + 1;
			}
		}
		if (index > m_iFill)
			m_iFill = index;

		if (!last)
			return -1;

		// Rows the page used to hold past what arrived are no longer on the board.
		int page = m_iFillPage;
		int pageEnd = (page + 1) * IA_BoardProtocol.PAGE_ROWS;
		for (int gone = m_iFill; gone < pageEnd; gone++)
		{
			m_mRows.Remove(gone);
		}
		m_mPages.Set(page, PAGE_LOADED);
		m_iFillPage = -1;
		Trim(page);
		return page;
	}

	//------------------------------------------------------------------------------------------------
	//! Every page held is due a refresh; its rows stay on screen until the new ones arrive.
	void MarkAllStale()
	{
		ref array<int> pages = {};
		foreach (int page, int state : m_mPages)
		{
			if (state == PAGE_LOADED)
				pages.Insert(page);
		}
		foreach (int stale : pages)
		{
			m_mPages.Set(stale, PAGE_STALE);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return the page to ask for next so rows firstRow..lastRow can be drawn, -1 when none is due
	int FirstWanted(int firstRow, int lastRow)
	{
		if (firstRow < 0)
			firstRow = 0;
		if (lastRow < firstRow)
			lastRow = firstRow;

		int firstPage = firstRow / IA_BoardProtocol.PAGE_ROWS;
		int lastPage = lastRow / IA_BoardProtocol.PAGE_ROWS;
		int page;

		// What is on screen and missing comes first, then what is on screen and old.
		for (page = firstPage; page <= lastPage; page++)
		{
			if (IsOnBoard(page) && PageState(page) == PAGE_NONE)
				return page;
		}
		for (page = firstPage; page <= lastPage; page++)
		{
			if (IsOnBoard(page) && PageState(page) == PAGE_STALE)
				return page;
		}

		// Then the pages either side, so a scroll finds them waiting.
		page = lastPage + 1;
		if (IsOnBoard(page) && PageState(page) == PAGE_NONE)
			return page;
		page = firstPage - 1;
		if (page >= 0 && PageState(page) == PAGE_NONE)
			return page;
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsOnBoard(int page)
	{
		if (page == 0)
			return true;
		if (m_iTotal < 0)
			return false;
		return page * IA_BoardProtocol.PAGE_ROWS < m_iTotal;
	}

	//------------------------------------------------------------------------------------------------
	//! Forget the page furthest from the one just loaded once too many are held.
	protected void Trim(int keepPage)
	{
		if (m_mPages.Count() <= KEEP_PAGES)
			return;

		int far = -1;
		int farGap = -1;
		foreach (int page, int state : m_mPages)
		{
			if (state == PAGE_ASKED)
				continue;
			int gap = page - keepPage;
			if (gap < 0)
				gap = -gap;
			if (gap > farGap)
			{
				farGap = gap;
				far = page;
			}
		}
		if (far < 0 || far == keepPage)
			return;

		int first = far * IA_BoardProtocol.PAGE_ROWS;
		for (int i = 0; i < IA_BoardProtocol.PAGE_ROWS; i++)
		{
			m_mRows.Remove(first + i);
		}
		m_mPages.Remove(far);
	}
}
