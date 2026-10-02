//------------------------------------------------------------------------------------------------
//! One material parameter a livery sets on a paint surface, and the value the
//! stock material file has for it. IA_HeliSkinPaint writes one or the other.
//------------------------------------------------------------------------------------------------
class IA_HeliPaintParam
{
	static const int KIND_PRIMARY = 0;	// the livery's colour times m_fGain
	static const int KIND_COLOR = 1;	// a colour every livery uses on this airframe
	static const int KIND_SCALAR = 2;	// a number every livery uses on this airframe
	static const int KIND_TINT = 3;		// the livery's colour over the paint colour baked into the texture

	string m_sParam;
	int m_iKind;
	// The same linear colour reads lighter or darker from one hull texture to the next.
	float m_fGain = 1;
	// KIND_COLOR: the colour set. KIND_TINT: the paint colour baked into the surface's texture.
	vector m_vPaint;
	float m_fPaint;

	// False when no material file sets it; stock paint is then the material class default.
	bool m_bStockSet;
	vector m_vStock;
	float m_fStock;
}
