//------------------------------------------------------------------------------------------------
//! A helicopter of no particular type for IA_HeliArt: what the paint bay draws
//! for a paint family whose "art" key has no silhouette of its own, such as a
//! modded helicopter. A medium utility shape on wheels.
//------------------------------------------------------------------------------------------------
class IA_HeliArtGeneric : IA_HeliArt
{
	//------------------------------------------------------------------------------------------------
	void IA_HeliArtGeneric()
	{
		// Built here, not as static members: every static initializer in every loaded
		// mod shares one engine init function with a fixed instruction cap.
		ref array<float> fuselage = {6, 24, 9, 19.5, 15, 16.5, 24, 15, 66, 15, 76, 17.5, 78, 21, 70, 28.5, 60, 30.5, 16, 30.5, 9, 28, 6, 26};
		ref array<float> cowl = {34, 15, 37, 10.5, 62, 10.5, 70, 15};
		ref array<float> boom = {74, 16.5, 110, 13, 110, 16.5, 76, 22.5};
		ref array<float> fin = {105, 16, 112, 3, 116.5, 3, 112, 16.6};
		ref array<float> stab = {96, 15, 106, 14, 106, 16, 96, 17.2};
		ref array<float> windshield = {10, 22, 12, 19, 17, 16.8, 23, 16.2, 23, 22};
		ref array<float> doorFront = {27, 17, 38, 17, 38, 22, 27, 22};
		ref array<float> doorRear = {43, 17, 56, 17, 56, 22, 43, 22};
		ref array<float> exhaust = {63, 11.4, 70, 12, 70, 14.2, 66, 15};

		m_aHull.Insert(boom);
		m_aHull.Insert(fin);
		m_aHull.Insert(stab);
		m_aHull.Insert(cowl);
		m_aHull.Insert(fuselage);

		m_aGlass.Insert(windshield);
		m_aGlass.Insert(doorFront);
		m_aGlass.Insert(doorRear);

		m_aDark.Insert(exhaust);

		AddShade(fuselage, 0, 1, 18.8, true);
		AddShade(cowl, 0, 1, 12.2, true);
		AddShade(fuselage, 0, -1, -26.4, false);
		AddShade(boom, -0.176, -1, -34.6, false);

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
