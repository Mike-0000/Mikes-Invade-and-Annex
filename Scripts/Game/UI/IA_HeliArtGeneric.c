//------------------------------------------------------------------------------------------------
//! A helicopter of no particular type for IA_HeliArt: what the paint bay draws
//! for a paint family whose "art" key has no silhouette of its own, such as a
//! modded helicopter. A medium utility shape on wheels.
//------------------------------------------------------------------------------------------------
class IA_HeliArtGeneric : IA_HeliArt
{
	protected static const ref array<float> FUSELAGE = {6, 24, 9, 19.5, 15, 16.5, 24, 15, 66, 15, 76, 17.5, 78, 21, 70, 28.5, 60, 30.5, 16, 30.5, 9, 28, 6, 26};
	protected static const ref array<float> COWL = {34, 15, 37, 10.5, 62, 10.5, 70, 15};
	protected static const ref array<float> BOOM = {74, 16.5, 110, 13, 110, 16.5, 76, 22.5};
	protected static const ref array<float> FIN = {105, 16, 112, 3, 116.5, 3, 112, 16.6};
	protected static const ref array<float> STAB = {96, 15, 106, 14, 106, 16, 96, 17.2};
	protected static const ref array<float> WINDSHIELD = {10, 22, 12, 19, 17, 16.8, 23, 16.2, 23, 22};
	protected static const ref array<float> DOOR_FRONT = {27, 17, 38, 17, 38, 22, 27, 22};
	protected static const ref array<float> DOOR_REAR = {43, 17, 56, 17, 56, 22, 43, 22};
	protected static const ref array<float> EXHAUST = {63, 11.4, 70, 12, 70, 14.2, 66, 15};

	//------------------------------------------------------------------------------------------------
	void IA_HeliArtGeneric()
	{
		m_aHull.Insert(BOOM);
		m_aHull.Insert(FIN);
		m_aHull.Insert(STAB);
		m_aHull.Insert(COWL);
		m_aHull.Insert(FUSELAGE);

		m_aGlass.Insert(WINDSHIELD);
		m_aGlass.Insert(DOOR_FRONT);
		m_aGlass.Insert(DOOR_REAR);

		m_aDark.Insert(EXHAUST);

		AddShade(FUSELAGE, 0, 1, 18.8, true);
		AddShade(COWL, 0, 1, 12.2, true);
		AddShade(FUSELAGE, 0, -1, -26.4, false);
		AddShade(BOOM, -0.176, -1, -34.6, false);

		// Main leg and tail leg.
		AddRecord(m_aGear, 27, 30.5, 26, 33.4, 1);
		AddRecord(m_aGear, 86, 20.6, 88, 32.4, 0.86);
		AddRound(m_aWheels, 26, 35.2, 2.4);
		AddRound(m_aWheels, 88, 34, 1.6);

		// Pilot's door, cabin door and its track, tail boom seam.
		AddRecord(m_aLines, 25, 16, 25, 30, 1);
		AddRecord(m_aLines, 40.6, 15.6, 40.6, 30, 1);
		AddRecord(m_aLines, 58.5, 15.6, 58.5, 30, 1);
		AddRecord(m_aLines, 40.6, 24, 70, 24, 1);
		AddRecord(m_aLines, 79, 19.6, 108, 15, 1);

		AddShadow(8, 38, 64, 1.5);
		AddShadow(76, 38.3, 36, 0.9);

		m_fRotorX = 52;
		m_fRotorHalf = 50;
		m_fMastH = 6;
		m_fTailX = 114.5;
		m_fTailY = 6;
		m_fTailR = 4.4;
	}
}
