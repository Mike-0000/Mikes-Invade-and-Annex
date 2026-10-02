//------------------------------------------------------------------------------------------------
//! MUI_Label for the small explanatory lines of the uplink menus: muted, wrapped, and drawn
//! without the outline menu text carries. Same calls as MUI_Label.
//!
//! Consumer:
//!   IA_UplinkNote n = IA_UplinkNote.Create(runtime, "Applies to every player.", "note");
//!   n.SetColor(IA_UplinkStyle.Get().m_Gold);   // a readout rather than a note
//!
//! Layout:
//!   Fill width, Hug height.
//------------------------------------------------------------------------------------------------
class IA_UplinkNote : MUI_Label
{
	protected static const int FONT_NOTE = 14;

	//------------------------------------------------------------------------------------------------
	static IA_UplinkNote Create(notnull MUI_Runtime runtime, string text, string name)
	{
		ref IA_UplinkNote note = new IA_UplinkNote();
		runtime.Adopt(note);
		note.SetName(name);
		note.SetMuted(true);
		note.SetText(text);
		return note;
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyTheme(notnull MUI_ThemeData theme)
	{
		super.ApplyTheme(theme);
		m_Style.m_iFontSize = FONT_NOTE;
	}

	//------------------------------------------------------------------------------------------------
	override void MeasureIntrinsic(float availW, float availH)
	{
		super.MeasureIntrinsic(availW, availH);
		m_fDesiredH = m_fDesiredH - 4;
	}

	//------------------------------------------------------------------------------------------------
	override void PaintForeground(MUI_RenderSurface surface)
	{
		surface.DrawText(DrawX(), DrawY(), m_World.m_fW, m_World.m_fH, m_sText, m_Style.m_iFontSize, MUI_ColorUtil.Fade(m_Style.m_Text, GetDrawOpacity()), m_Style.m_bBold, false, false, true, true);
	}
}
