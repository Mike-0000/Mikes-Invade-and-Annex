//------------------------------------------------------------------------------------------------
//! MUI_Progress drawn as a row of slanted pips, as the paint bay rates a pilot. Same calls as
//! MUI_Progress.
//!
//! Consumer:
//!   IA_UplinkMeter m = IA_UplinkMeter.Create(runtime, "artyChanceBar");
//!   m.SetValue(0.18);
//!
//! Layout:
//!   Fill width, Exact 10.
//------------------------------------------------------------------------------------------------
class IA_UplinkMeter : MUI_Progress
{
	protected static const float PIP_W = 14;
	protected static const float PIP_GAP = 4;
	protected static const float LEAN = 4;

	//------------------------------------------------------------------------------------------------
	static IA_UplinkMeter Create(notnull MUI_Runtime runtime, string name)
	{
		ref IA_UplinkMeter meter = new IA_UplinkMeter();
		runtime.Adopt(meter);
		meter.SetName(name);
		return meter;
	}

	//------------------------------------------------------------------------------------------------
	override void PaintForeground(MUI_RenderSurface surface)
	{
		float op = GetDrawOpacity();
		if (op < 0.01)
			return;

		IA_UplinkStyle look = IA_UplinkStyle.Get();
		float x = DrawX() + 8;
		float y = DrawY();
		float w = m_World.m_fW - 16;
		float h = m_World.m_fH;

		int count = (w - LEAN + PIP_GAP) / (PIP_W + PIP_GAP);
		if (count < 1)
			return;
		float lit = m_fValue * count;

		int i;
		for (i = 0; i < count; i++)
		{
			float px = x + i * (PIP_W + PIP_GAP);
			if (i + 0.5 < lit)
				look.FillSlant(surface, px, y, PIP_W, h, LEAN, MUI_ColorUtil.Fade(look.m_Tone, op * 0.92));
			else
				look.FillSlant(surface, px, y, PIP_W, h, LEAN, MUI_ColorUtil.Fade(look.m_Tone, op * 0.10));
		}
	}
}
