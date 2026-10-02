//------------------------------------------------------------------------------------------------
//! The paint bay's look as shared pieces, so the admin menu and the leaderboard are drawn the
//! same way: square bevelled bodies, amber tracked captions, corner brackets and chips.
//!
//! Consumer:
//!   IA_UplinkStyle look = IA_UplinkStyle.Get();
//!   look.FillChamfer(surface, x, y, w, h, 5, MUI_ColorUtil.Fade(look.m_Bg, op));
//!
//! Colours handed in are used at once; pass a freshly faded colour straight into the call.
//------------------------------------------------------------------------------------------------
class IA_UplinkStyle
{
	static const int FONT_TAB = 10;
	static const int FONT_CAP = 9;
	static const float TRACK_TAB = 1.5;
	static const float TRACK_CAP = 1.4;
	static const float BEVEL = 4;
	static const float CHIP_H = 17;
	static const float CHIP_PAD = 8;

	ref Color m_Bg;
	ref Color m_Tab;
	ref Color m_Tone;
	ref Color m_Gold;
	ref Color m_Green;
	ref Color m_Red;
	ref Color m_Cyan;
	ref Color m_Muted;
	ref Color m_White;
	ref Color m_Black;
	ref Color m_Glass;
	ref Color m_Plate;
	ref Color m_Silver;
	ref Color m_Bronze;

	protected ref array<float> m_aPoly = {};
	protected static ref IA_UplinkStyle s_Instance;

	//------------------------------------------------------------------------------------------------
	void IA_UplinkStyle()
	{
		m_Bg = Color.FromSRGBA(13, 20, 18, 232);
		m_Tab = Color.FromSRGBA(58, 42, 18, 224);
		m_Tone = Color.FromSRGBA(255, 184, 72, 255);
		m_Gold = Color.FromSRGBA(255, 214, 120, 255);
		m_Green = Color.FromSRGBA(94, 251, 131, 255);
		m_Red = Color.FromSRGBA(255, 112, 92, 255);
		m_Cyan = Color.FromSRGBA(89, 235, 224, 255);
		m_Muted = Color.FromSRGBA(158, 168, 163, 255);
		m_White = Color.FromSRGBA(238, 242, 240, 255);
		m_Black = Color.FromSRGBA(0, 0, 0, 255);
		m_Glass = Color.FromSRGBA(9, 14, 13, 240);
		m_Plate = Color.FromSRGBA(18, 26, 23, 238);
		m_Silver = Color.FromSRGBA(196, 208, 214, 255);
		m_Bronze = Color.FromSRGBA(214, 142, 92, 255);
	}

	//------------------------------------------------------------------------------------------------
	static IA_UplinkStyle Get()
	{
		if (!s_Instance)
			s_Instance = new IA_UplinkStyle();
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! A rectangle with its top-left and bottom-right corners cut.
	protected void BuildChamfer(float x, float y, float w, float h, float cut)
	{
		m_aPoly.Clear();
		m_aPoly.Insert(x + cut);
		m_aPoly.Insert(y);
		m_aPoly.Insert(x + w);
		m_aPoly.Insert(y);
		m_aPoly.Insert(x + w);
		m_aPoly.Insert(y + h - cut);
		m_aPoly.Insert(x + w - cut);
		m_aPoly.Insert(y + h);
		m_aPoly.Insert(x);
		m_aPoly.Insert(y + h);
		m_aPoly.Insert(x);
		m_aPoly.Insert(y + cut);
	}

	//------------------------------------------------------------------------------------------------
	void FillChamfer(MUI_RenderSurface surface, float x, float y, float w, float h, float cut, Color color)
	{
		BuildChamfer(x, y, w, h, cut);
		surface.FillPolygon(m_aPoly, color);
	}

	//------------------------------------------------------------------------------------------------
	void StrokeChamfer(MUI_RenderSurface surface, float x, float y, float w, float h, float cut, Color color, float width)
	{
		BuildChamfer(x, y, w, h, cut);
		surface.StrokePolyline(m_aPoly, color, width, true);
	}

	//------------------------------------------------------------------------------------------------
	//! A parallelogram leaning right by \p lean, as the paint bay's rating pips do.
	void FillSlant(MUI_RenderSurface surface, float x, float y, float w, float h, float lean, Color color)
	{
		m_aPoly.Clear();
		m_aPoly.Insert(x + lean);
		m_aPoly.Insert(y);
		m_aPoly.Insert(x + lean + w);
		m_aPoly.Insert(y);
		m_aPoly.Insert(x + w);
		m_aPoly.Insert(y + h);
		m_aPoly.Insert(x);
		m_aPoly.Insert(y + h);
		surface.FillPolygon(m_aPoly, color);
	}

	//------------------------------------------------------------------------------------------------
	void StrokeSlant(MUI_RenderSurface surface, float x, float y, float w, float h, float lean, Color color, float width)
	{
		m_aPoly.Clear();
		m_aPoly.Insert(x + lean);
		m_aPoly.Insert(y);
		m_aPoly.Insert(x + lean + w);
		m_aPoly.Insert(y);
		m_aPoly.Insert(x + w);
		m_aPoly.Insert(y + h);
		m_aPoly.Insert(x);
		m_aPoly.Insert(y + h);
		surface.StrokePolyline(m_aPoly, color, width, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Four corner marks on a rectangle.
	void Brackets(MUI_RenderSurface surface, float x, float y, float w, float h, float arm, Color color, float width)
	{
		float r = x + w;
		float b = y + h;
		surface.DrawLine(x, y, x + arm, y, color, width);
		surface.DrawLine(x, y, x, y + arm, color, width);
		surface.DrawLine(r, y, r - arm, y, color, width);
		surface.DrawLine(r, y, r, y + arm, color, width);
		surface.DrawLine(x, b, x + arm, b, color, width);
		surface.DrawLine(x, b, x, b - arm, color, width);
		surface.DrawLine(r, b, r - arm, b, color, width);
		surface.DrawLine(r, b, r, b - arm, color, width);
	}

	//------------------------------------------------------------------------------------------------
	//! Where the gamepad is: cyan corner marks just outside the control. \p t is GetFocusT().
	void FocusMarks(MUI_RenderSurface surface, float x, float y, float w, float h, float t, float op)
	{
		if (t < 0.02)
			return;
		float grow = (1.0 - t) * 6.0 + 4.0;
		Brackets(surface, x - grow, y - grow, w + grow * 2, h + grow * 2, 9, MUI_ColorUtil.Fade(m_Cyan, op * t * 0.95), 1.8);
	}

	//------------------------------------------------------------------------------------------------
	float ChipWidth(MUI_Runtime runtime, string text)
	{
		return IA_TrackedText.Measure(runtime, text, FONT_CAP, TRACK_CAP) + CHIP_PAD * 2;
	}

	//------------------------------------------------------------------------------------------------
	void Chip(MUI_RenderSurface surface, MUI_Runtime runtime, float x, float y, float w, string text, Color tone, float op)
	{
		surface.FillRect(x, y, w, CHIP_H, MUI_ColorUtil.Fade(tone, op * 0.16), 0);
		surface.StrokeRect(x, y, w, CHIP_H, MUI_ColorUtil.Fade(tone, op * 0.5), 1, 0);
		IA_TrackedText.Draw(surface, runtime, x + CHIP_PAD, y, CHIP_H, text, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(tone, op));
	}

	//------------------------------------------------------------------------------------------------
	//! A tracked caption with a hairline running out to \p w.
	void Caption(MUI_RenderSurface surface, MUI_Runtime runtime, float x, float y, float w, string text, Color tone, float op)
	{
		IA_TrackedText.Draw(surface, runtime, x, y, 12, text, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(tone, op));
		float capW = IA_TrackedText.Measure(runtime, text, FONT_CAP, TRACK_CAP);
		float lineW = w - capW - 10;
		if (lineW > 2)
			surface.FillRect(x + capW + 10, y + 6, lineW, 1, MUI_ColorUtil.Fade(m_Tone, op * 0.12), 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Ruler ticks standing on the line y = \p baseY.
	void Ruler(MUI_RenderSurface surface, float x, float baseY, float w, float op)
	{
		Color tick = MUI_ColorUtil.Fade(m_Tone, op * 0.18);
		int count = w / 12;
		int n;
		for (n = 1; n < count; n++)
		{
			float tickH = 3;
			if (n % 5 == 0)
				tickH = 6;
			surface.FillRect(x + n * 12, baseY - tickH, 1, tickH, tick, 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void Click()
	{
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK);
	}

	//------------------------------------------------------------------------------------------------
	static void ClickFail()
	{
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK_FAIL);
	}

	//------------------------------------------------------------------------------------------------
	static void Hover()
	{
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_FE_BUTTON_HOVER);
	}
}
