//------------------------------------------------------------------------------------------------
//! UH-1H for IA_HeliArt: teardrop cabin, engine cowl, slim tail boom, skids.
//------------------------------------------------------------------------------------------------
class IA_HeliArtHuey : IA_HeliArt
{
	//------------------------------------------------------------------------------------------------
	void IA_HeliArtHuey()
	{
		// Built here, not as static members: every static initializer in every loaded
		// mod shares one engine init function with a fixed instruction cap.
		ref array<float> fuselage = {5, 23, 8, 18.5, 15, 15.2, 26, 14, 72, 14, 78, 17, 80, 21.5, 75, 28.5, 64, 31, 18, 31, 9, 28.5, 5.5, 25.5};
		ref array<float> cowl = {40, 14, 43, 9.5, 64, 9.5, 71, 14};
		ref array<float> boom = {76, 16, 112, 12, 112, 15.5, 78, 22.5};
		ref array<float> fin = {107, 15, 113.5, 2, 117.5, 2, 113.5, 15.6};
		ref array<float> stab = {92, 15.2, 103, 14, 103, 16, 92, 17.4};
		ref array<float> windshield = {9.5, 21.5, 11.5, 18.3, 16.5, 16.2, 22, 15.6, 22, 21.5};
		ref array<float> doorFront = {26, 16, 39, 16, 39, 21.5, 26, 21.5};
		ref array<float> doorRear = {43, 16, 54, 16, 54, 21.5, 43, 21.5};
		ref array<float> chin = {8.6, 25.4, 13.5, 24.6, 15.5, 28.6, 11.2, 28.4};
		ref array<float> exhaust = {66.5, 10.3, 73.5, 10.8, 73.5, 13.2, 69.5, 14};

		m_aHull.Insert(boom);
		m_aHull.Insert(fin);
		m_aHull.Insert(stab);
		m_aHull.Insert(cowl);
		m_aHull.Insert(fuselage);

		m_aGlass.Insert(windshield);
		m_aGlass.Insert(doorFront);
		m_aGlass.Insert(doorRear);

		m_aDark.Insert(chin);
		m_aDark.Insert(exhaust);

		AddShade(fuselage, 0, 1, 18.2, true);
		AddShade(cowl, 0, 1, 11.2, true);
		AddShade(fuselage, 0, -1, -26.4, false);
		AddShade(boom, -0.206, -1, -37.3, false);

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
