//------------------------------------------------------------------------------------------------
//! Mi-8MT for IA_HeliArt: glazed nose, long cabin with portholes, engines over
//! the cabin, a high tail boom and wheels.
//------------------------------------------------------------------------------------------------
class IA_HeliArtHip : IA_HeliArt
{
	protected static const ref array<float> FUSELAGE = {7, 22, 9, 17.5, 14, 14.5, 22, 13, 70, 13, 80, 15.5, 80, 19, 62, 30, 56, 31.5, 16, 31.5, 9.5, 28.5, 7, 25};
	protected static const ref array<float> COWL = {26, 13, 29, 9.2, 56, 8.6, 66, 10.5, 70, 13};
	protected static const ref array<float> BOOM = {76, 13.5, 108, 10.5, 108, 14, 78, 19.5};
	protected static const ref array<float> FIN = {103, 13.5, 111, 2.5, 115.5, 2.5, 110, 14.2};
	protected static const ref array<float> STAB = {90, 13.6, 100, 12.6, 100, 14.5, 90, 15.6};
	protected static const ref array<float> WINDSHIELD = {9.5, 21, 10.8, 17.8, 15, 15.4, 22, 14.5, 22, 21};
	protected static const ref array<float> CHIN = {9.8, 23, 16, 23, 16, 27, 11.5, 26.5};
	protected static const ref array<float> INTAKE = {27.2, 12.4, 29.6, 9.6, 32, 9.6, 30.5, 12.4};
	protected static const ref array<float> EXHAUST = {46, 10, 52, 10, 52.5, 12.4, 46.5, 12.4};

	//------------------------------------------------------------------------------------------------
	void IA_HeliArtHip()
	{
		m_aHull.Insert(BOOM);
		m_aHull.Insert(FIN);
		m_aHull.Insert(STAB);
		m_aHull.Insert(COWL);
		m_aHull.Insert(FUSELAGE);

		m_aGlass.Insert(WINDSHIELD);
		m_aGlass.Insert(CHIN);
		AddRound(m_aPorts, 30, 19.6, 1.7);
		AddRound(m_aPorts, 38, 19.6, 1.7);
		AddRound(m_aPorts, 46, 19.6, 1.7);
		AddRound(m_aPorts, 54, 19.6, 1.7);
		AddRound(m_aPorts, 62, 19.6, 1.7);

		m_aDark.Insert(INTAKE);
		m_aDark.Insert(EXHAUST);

		AddShade(FUSELAGE, 0, 1, 17, true);
		AddShade(COWL, 0, 1, 10.4, true);
		AddShade(FUSELAGE, 0, -1, -27, false);
		AddShade(BOOM, -0.183, -1, -32.5, false);

		// Nose leg, main leg and its brace.
		AddRecord(m_aGear, 19, 31.5, 19, 34, 1);
		AddRecord(m_aGear, 57, 30, 60, 33.2, 1);
		AddRecord(m_aGear, 63, 27.5, 60, 33.2, 0.86);
		AddRound(m_aWheels, 19, 35.6, 1.9);
		AddRound(m_aWheels, 60, 35, 2.6);

		// Sliding door, the fuel tank along the side and the clamshell doors.
		AddRecord(m_aLines, 25, 14, 25, 30.6, 1);
		AddRecord(m_aLines, 33.5, 14, 33.5, 30.6, 1);
		AddRecord(m_aLines, 28, 26.2, 60, 26.2, 1);
		AddRecord(m_aLines, 67, 13.6, 61, 30, 1);
		// Tail boom seam and tail bumper.
		AddRecord(m_aLines, 81, 17, 106, 12.4, 1);
		AddRecord(m_aLines, 100, 15.6, 104, 19, 1.4);

		AddShadow(8, 38, 62, 1.5);
		AddShadow(72, 38.3, 40, 0.9);

		m_fRotorX = 50;
		m_fRotorHalf = 48;
		m_fMastH = 4.4;
		m_fTailX = 113.5;
		m_fTailY = 5.2;
		m_fTailR = 5;
	}
}
