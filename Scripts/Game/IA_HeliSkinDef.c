//------------------------------------------------------------------------------------------------
//! One unlockable helicopter livery: a plain paint colour. It names no files;
//! each helicopter family says which layers of its own paint materials take
//! the colour, so one livery fits every helicopter that has paint channels.
//! IA_HeliSkinPaint sets it on a paint channel without touching the entity.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinDef
{
	int m_iId;					// never reuse
	string m_sKey;				// key shared with the backend threshold table
	string m_sDisplayName;
	int m_iRequiredPoints;		// global transport rating

	// The paint, as the linear colour a material layer takes.
	vector m_vPaint;

	// Hull colour as sRGB bytes, for drawing the livery in the paint bay and on the pilot card.
	int m_iSwatchR = 78;
	int m_iSwatchG = 88;
	int m_iSwatchB = 60;
}
