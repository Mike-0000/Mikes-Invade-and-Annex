//------------------------------------------------------------------------------------------------
//! UH-1H for IA_HeliArt: teardrop cabin, engine cowl, slim tail boom, skids.
//------------------------------------------------------------------------------------------------
class IA_HeliArtHuey : IA_HeliArt
{
	protected static const ref array<float> FUSELAGE = {5, 23, 8, 18.5, 15, 15.2, 26, 14, 72, 14, 78, 17, 80, 21.5, 75, 28.5, 64, 31, 18, 31, 9, 28.5, 5.5, 25.5};
	protected static const ref array<float> COWL = {40, 14, 43, 9.5, 64, 9.5, 71, 14};
	protected static const ref array<float> BOOM = {76, 16, 112, 12, 112, 15.5, 78, 22.5};
	protected static const ref array<float> FIN = {107, 15, 113.5, 2, 117.5, 2, 113.5, 15.6};
	protected static const ref array<float> STAB = {92, 15.2, 103, 14, 103, 16, 92, 17.4};
	protected static const ref array<float> WINDSHIELD = {9.5, 21.5, 11.5, 18.3, 16.5, 16.2, 22, 15.6, 22, 21.5};
	protected static const ref array<float> DOOR_FRONT = {26, 16, 39, 16, 39, 21.5, 26, 21.5};
	protected static const ref array<float> DOOR_REAR = {43, 16, 54, 16, 54, 21.5, 43, 21.5};
	protected static const ref array<float> CHIN = {8.6, 25.4, 13.5, 24.6, 15.5, 28.6, 11.2, 28.4};
	protected static const ref array<float> EXHAUST = {66.5, 10.3, 73.5, 10.8, 73.5, 13.2, 69.5, 14};

	//------------------------------------------------------------------------------------------------
	void IA_HeliArtHuey()
	{
		m_aHull.Insert(BOOM);
		m_aHull.Insert(FIN);
		m_aHull.Insert(STAB);
		m_aHull.Insert(COWL);
		m_aHull.Insert(FUSELAGE);

		m_aGlass.Insert(WINDSHIELD);
		m_aGlass.Insert(DOOR_FRONT);
		m_aGlass.Insert(DOOR_REAR);

		m_aDark.Insert(CHIN);
		m_aDark.Insert(EXHAUST);

		AddShade(FUSELAGE, 0, 1, 18.2, true);
		AddShade(COWL, 0, 1, 11.2, true);
		AddShade(FUSELAGE, 0, -1, -26.4, false);
		AddShade(BOOM, -0.206, -1, -37.3, false);

		// Skids.
		AddRecord(m_aGear, 13, 36.5, 66, 36.5, 1);
		AddRecord(m_aGear, 13, 36.5, 9.5, 33.5, 1);
		AddRecord(m_aGear, 27, 31, 25, 36.5, 0.86);
		AddRecord(m_aGear, 56, 31, 58, 36.5, 0.86);

		// Cabin door, its track and the pilot's door.
		AddRecord(m_aLines, 40.6, 14.6, 40.6, 30.6, 1);
		AddRecord(m_aLines, 58, 14.6, 58, 30.6, 1);
		AddRecord(m_aLines, 40.6, 23.4, 73, 23.4, 1);
		AddRecord(m_aLines, 24.4, 15, 24.4, 30.6, 1);
		// Tail boom seam, tail skid, the pitot at the nose and the skid heel.
		AddRecord(m_aLines, 80, 19.2, 110, 14, 1);
		AddRecord(m_aLines, 104.5, 17, 109, 20.5, 1.4);
		AddRecord(m_aLines, 5.2, 23.6, 2, 24.2, 1.4);
		AddRecord(m_aLines, 66, 36.5, 69, 35.2, 1.4);

		AddShadow(6, 38, 66, 1.5);
		AddShadow(76, 38.3, 38, 0.9);
	}
}
