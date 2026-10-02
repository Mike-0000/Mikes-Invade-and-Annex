//------------------------------------------------------------------------------------------------
//! MUI_ScrollView with no box of its own: the frame is the box. It keeps a square amber scroll
//! bar and marks the edge more content lies beyond. Same calls as MUI_ScrollView.
//!
//! Consumer:
//!   IA_UplinkScroll s = IA_UplinkScroll.Create(runtime, "scroll");
//!   s.SetViewportHeight(430);
//!
//! Layout:
//!   As MUI_ScrollView. Padded so the gamepad's corner marks are not cut by the clip.
//------------------------------------------------------------------------------------------------
class IA_UplinkScroll : MUI_ScrollView
{
	protected static const float WHEEL_STEP = 44;
	protected static const float BAR_W = 3;

	//------------------------------------------------------------------------------------------------
	void IA_UplinkScroll()
	{
		m_Style.SetPaddingTRBL(12, 22, 12, 12);
		m_Style.m_fGap = 12;
		m_Style.m_fRadius = 0;
	}

	//------------------------------------------------------------------------------------------------
	static IA_UplinkScroll Create(notnull MUI_Runtime runtime, string name)
	{
		ref IA_UplinkScroll scroll = new IA_UplinkScroll();
		runtime.Adopt(scroll);
		scroll.SetName(name);
		return scroll;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMouseWheel(int wheel)
	{
		SetScrollY(m_fScrollY - wheel * WHEEL_STEP);
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
		float op = GetDrawOpacity();
		if (op < 0.01)
			return;

		IA_UplinkStyle look = IA_UplinkStyle.Get();
		float x = DrawX();
		float y = DrawY();
		float w = m_World.m_fW;
		float h = m_World.m_fH;
		surface.FillRect(x, y, w, h, MUI_ColorUtil.Fade(look.m_Black, op * 0.16), 0);

		float innerH = h - m_Style.m_fPadT - m_Style.m_fPadB;
		float maxScroll = m_fContentH - innerH;
		if (maxScroll <= 1)
			return;

		// A lit edge where there is more to scroll to.
		if (m_fScrollY > 1)
			surface.FillRect(x, y, w - BAR_W - 6, 1, MUI_ColorUtil.Fade(look.m_Tone, op * 0.30), 0);
		if (m_fScrollY < maxScroll - 1)
			surface.FillRect(x, y + h - 1, w - BAR_W - 6, 1, MUI_ColorUtil.Fade(look.m_Tone, op * 0.30), 0);

		float trackX = x + w - BAR_W - 4;
		float trackY = y + 4;
		float trackH = h - 8;
		surface.FillRect(trackX, trackY, BAR_W, trackH, MUI_ColorUtil.Fade(look.m_Tone, op * 0.10), 0);

		float thumbH = trackH * (innerH / m_fContentH);
		if (thumbH < 22)
			thumbH = 22;
		float t = m_fScrollY / maxScroll;
		if (t < 0)
			t = 0;
		if (t > 1)
			t = 1;
		surface.FillRect(trackX, trackY + (trackH - thumbH) * t, BAR_W, thumbH, MUI_ColorUtil.Fade(look.m_Tone, op * 0.85), 0);
	}
}
