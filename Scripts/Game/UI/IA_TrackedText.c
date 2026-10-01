//------------------------------------------------------------------------------------------------
//! Letter-spaced bold text for the paint bay, in the style of the pilot card
//! captions. The compositor has no tracking, so each glyph is its own draw;
//! glyph widths are measured once per font size.
//------------------------------------------------------------------------------------------------
class IA_TrackedText
{
	protected static ref map<int, float> s_mCharW;

	//------------------------------------------------------------------------------------------------
	//! \return width of one line of bold text
	static float Width(MUI_Runtime runtime, string text, int fontSize)
	{
		float w;
		float h = fontSize;
		if (runtime)
			runtime.MeasureText(text, fontSize, true, 0, w, h);
		if (w < 1)
			w = text.Length() * fontSize * 0.55;
		return w;
	}

	//------------------------------------------------------------------------------------------------
	protected static float CharWidth(MUI_Runtime runtime, string ch, int fontSize)
	{
		// A lone space measures as empty.
		if (ch == " ")
			return fontSize * 0.3;

		if (!s_mCharW)
			s_mCharW = new map<int, float>();

		int key = fontSize * 256 + ch.ToAscii();
		float w;
		if (s_mCharW.Find(key, w))
			return w;
		if (!runtime)
			return fontSize * 0.55;

		float h = fontSize;
		runtime.MeasureText(ch, fontSize, true, 0, w, h);
		if (w < 1)
			w = fontSize * 0.55;
		s_mCharW.Set(key, w);
		return w;
	}

	//------------------------------------------------------------------------------------------------
	//! \return width Draw gives this text
	static float Measure(MUI_Runtime runtime, string text, int fontSize, float tracking)
	{
		float total;
		int len = text.Length();
		int i;
		for (i = 0; i < len; i++)
		{
			total = total + CharWidth(runtime, text.Substring(i, 1), fontSize);
			if (i < len - 1)
				total = total + tracking;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Left-aligned, vertically centred in h.
	static void Draw(MUI_RenderSurface surface, MUI_Runtime runtime, float x, float y, float h, string text, int fontSize, float tracking, Color color)
	{
		float cx = x;
		int len = text.Length();
		int i;
		for (i = 0; i < len; i++)
		{
			string ch = text.Substring(i, 1);
			float cw = CharWidth(runtime, ch, fontSize);
			if (ch != " ")
				surface.DrawText(cx, y, cw + 2, h, ch, fontSize, color, true, false, true, false, true);
			cx = cx + cw + tracking;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ends at the given right edge.
	static void DrawRight(MUI_RenderSurface surface, MUI_Runtime runtime, float right, float y, float h, string text, int fontSize, float tracking, Color color)
	{
		Draw(surface, runtime, right - Measure(runtime, text, fontSize, tracking), y, h, text, fontSize, tracking, color);
	}
}
