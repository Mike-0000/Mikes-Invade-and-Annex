//------------------------------------------------------------------------------------------------
//! Two controls side by side, each taking half the row. The controls in it use left and right
//! to change their value, so the row carries the gamepad's up and down across both columns in
//! reading order.
//!
//! Consumer:
//!   IA_UplinkPair p = IA_UplinkPair.Create(runtime, "aiRow", m_AIField, m_StaticAIField);
//!   page.AddChild(p);
//!   IA_UplinkPair.Create(runtime, "oddRow", m_MilVehField, null);   // right half left free
//!
//! Layout:
//!   As MUI_Row (Fill width, Hug height), gap 16. Children must be Fill width.
//------------------------------------------------------------------------------------------------
class IA_UplinkPair : MUI_Row
{
	protected static const float PAIR_GAP = 16;

	//------------------------------------------------------------------------------------------------
	static IA_UplinkPair Create(notnull MUI_Runtime runtime, string name, notnull MUI_Node left, MUI_Node right)
	{
		ref IA_UplinkPair pair = new IA_UplinkPair();
		runtime.Adopt(pair);
		pair.SetName(name);
		pair.SetGap(PAIR_GAP);
		pair.AddChild(left);
		if (right)
		{
			pair.AddChild(right);
			return pair;
		}

		ref MUI_Spacer filler = runtime.CreateSpacer(1, name + "Free");
		pair.AddChild(filler);
		return pair;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the gamepad focus one control on from \p from, down (1) or up (-1).
	//! \return false when the move leaves the block of rows, for the menu to place as usual
	bool StepFocus(notnull MUI_Node from, int dir)
	{
		if (dir == 0 || !m_Runtime)
			return false;

		int count = GetChildCount();
		int at = -1;
		int i;
		for (i = 0; i < count; i++)
		{
			if (GetChild(i) == from)
			{
				at = i;
				break;
			}
		}
		if (at < 0)
			return false;

		MUI_Node inRow = Edge(at + dir, dir);
		if (inRow)
		{
			m_Runtime.FocusNode(inRow);
			return true;
		}

		MUI_Node parent = GetParent();
		if (!parent)
			return false;

		int rows = parent.GetChildCount();
		int mine = -1;
		for (i = 0; i < rows; i++)
		{
			if (parent.GetChild(i) == this)
			{
				mine = i;
				break;
			}
		}
		if (mine < 0)
			return false;

		// Notes and captions lie between the rows; step over them to the next control.
		for (i = mine + dir; i >= 0 && i < rows; i = i + dir)
		{
			MUI_Node next = parent.GetChild(i);
			if (!next || !next.IsVisible())
				continue;

			IA_UplinkPair pair = IA_UplinkPair.Cast(next);
			if (pair)
			{
				MUI_Node entry;
				if (dir > 0)
					entry = pair.Edge(0, 1);
				else
					entry = pair.Edge(pair.GetChildCount() - 1, -1);
				if (!entry)
					continue;
				m_Runtime.FocusNode(entry);
				return true;
			}

			if (next.WantsFocus())
			{
				m_Runtime.FocusNode(next);
				return true;
			}
			if (next.GetChildCount() > 0)
				return false;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The first control that takes focus at or beyond \p start, walking in \p dir.
	MUI_Node Edge(int start, int dir)
	{
		int count = GetChildCount();
		int i;
		for (i = start; i >= 0 && i < count; i = i + dir)
		{
			MUI_Node child = GetChild(i);
			if (child && child.WantsFocus())
				return child;
		}
		return null;
	}
}
