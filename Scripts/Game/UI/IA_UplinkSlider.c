//------------------------------------------------------------------------------------------------
//! MUI_Slider drawn as a ruled track with a slanted amber marker. Same calls as MUI_Slider, and
//! the same track geometry, so the base class's pointer maths still lands on the marker.
//!
//! Consumer:
//!   IA_UplinkSlider s = IA_UplinkSlider.Create(runtime, "artyChance");
//!   s.SetRange(0, 1); s.SetStep(0.01); s.SetValue(0.18);
//!
//! Layout:
//!   Fill width, Exact 30.
//------------------------------------------------------------------------------------------------
class IA_UplinkSlider : MUI_Slider
{
	protected static const float SLIDER_H = 30;
	protected static const float TRACK_H = 4;
	protected static const float TRACK_INSET = 8;
	protected static const int TICKS = 20;

	//------------------------------------------------------------------------------------------------
	void IA_UplinkSlider()
	{
		m_Style.m_fHeight = SLIDER_H;
		m_Style.m_fMinHeight = SLIDER_H;
		m_Style.m_fRadius = 0;
	}

	//------------------------------------------------------------------------------------------------
	static IA_UplinkSlider Create(notnull MUI_Runtime runtime, string name)
	{
		ref IA_UplinkSlider slider = new IA_UplinkSlider();
		runtime.Adopt(slider);
		slider.SetName(name);
		return slider;
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
		float hover = GetHoverT();

		float trackX = x + TRACK_INSET;
		float trackW = w - TRACK_INSET * 2;
		float trackY = y + (h - TRACK_H) * 0.5 - 3;
		surface.FillRect(trackX, trackY, trackW, TRACK_H, MUI_ColorUtil.Fade(look.m_Glass, op), 0);
		surface.FillRect(trackX, trackY, trackW, TRACK_H, MUI_ColorUtil.Fade(look.m_Tone, op * 0.16), 0);
		surface.FillRect(trackX, trackY + TRACK_H, trackW, 1, MUI_ColorUtil.Fade(look.m_Tone, op * 0.30), 0);

		// The rule under the track: a tick each twentieth, a long one each quarter.
		Color tick = MUI_ColorUtil.Fade(look.m_Tone, op * 0.26);
		int n;
		for (n = 0; n <= TICKS; n++)
		{
			float tickH = 3;
			if (n % 5 == 0)
				tickH = 6;
			surface.FillRect(trackX + trackW * n / TICKS, trackY + TRACK_H + 3, 1, tickH, tick, 0);
		}

		float span = m_fMax - m_fMin;
		float t = 0;
		if (span > 0.0001)
			t = (m_fValue - m_fMin) / span;
		float fillW = trackW * t;
		if (fillW > 0.75)
			surface.FillRect(trackX, trackY, fillW, TRACK_H, MUI_ColorUtil.Fade(look.m_Tone, op), 0);

		float markX = trackX + fillW;
		float markY = trackY - 6;
		float markH = TRACK_H + 12;
		if (hover > 0.02)
			look.FillSlant(surface, markX - 8, markY - 2, 12, markH + 4, 4, MUI_ColorUtil.Fade(look.m_Tone, op * hover * 0.18));
		look.FillSlant(surface, markX - 5, markY, 7, markH, 3, MUI_ColorUtil.Fade(look.m_Gold, op));

		look.FocusMarks(surface, x, y, w, h, GetFocusT(), op);
	}
}
