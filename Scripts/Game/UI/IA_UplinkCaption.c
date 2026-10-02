//------------------------------------------------------------------------------------------------
//! A section heading: a tracked amber caption with a hairline running out to the right edge.
//!
//! Consumer:
//!   IA_UplinkCaption c = IA_UplinkCaption.Create(runtime, "Timing (seconds)", "dynamicAiTiming");
//!   page.AddChild(c);
//!
//! Layout:
//!   Fill width, Exact 24. Text is ASCII, drawn in capitals.
//------------------------------------------------------------------------------------------------
class IA_UplinkCaption : MUI_Node
{
	protected static const float CAPTION_H = 24;

	protected string m_sCaption;

	//------------------------------------------------------------------------------------------------
	void IA_UplinkCaption()
	{
		m_Style.m_WidthMode = MUI_SizeMode.Fill;
		m_Style.m_HeightMode = MUI_SizeMode.Exact;
		m_Style.m_fHeight = CAPTION_H;
		m_Style.m_fMinHeight = CAPTION_H;
		m_Style.m_fRadius = 0;
		m_Style.m_Fill = Color.FromInt(0);
		m_Style.m_bInteractive = false;
	}

	//------------------------------------------------------------------------------------------------
	static IA_UplinkCaption Create(notnull MUI_Runtime runtime, string text, string name)
	{
		ref IA_UplinkCaption caption = new IA_UplinkCaption();
		runtime.Adopt(caption);
		caption.SetName(name);
		caption.SetText(text);
		return caption;
	}

	//------------------------------------------------------------------------------------------------
	void SetText(string text)
	{
		m_sCaption = text;
		m_sCaption.ToUpper();
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
		float op = GetDrawOpacity();
		if (op < 0.01)
			return;

		IA_UplinkStyle look = IA_UplinkStyle.Get();
		float x = DrawX();
		float y = DrawY() + m_World.m_fH - 14;
		surface.FillRect(x, y + 2, 3, 9, MUI_ColorUtil.Fade(look.m_Tone, op * 0.9), 0);
		look.Caption(surface, m_Runtime, x + 10, y, m_World.m_fW - 10, m_sCaption, look.m_Tone, op);
	}
}
