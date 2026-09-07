// Hardware profile; geometry evidence and authoring tolerances are recorded in
// docs/dynamic-base-emplacements.json. No runtime terrain search belongs here.
class IA_EmplacementProfile
{
	static const ResourceName PKM = "{A80E963CB15F40D1}Prefabs/Emplacements/IA_Emplacement_PKM.et";
	static const ResourceName NSV = "{A80E963CB15F40D2}Prefabs/Emplacements/IA_Emplacement_NSV.et";
	static const ResourceName NSV_SPP = "{C1C3E8920E925A44}Prefabs/Emplacements/IA_Emplacement_NSV_SPP.et";
	static const ResourceName AA = "{787A4DC1D2A65D01}Prefabs/Emplacements/IA_Emplacement_AA.et";
	int m_iKind;
	ResourceName m_Prefab;
	ResourceName m_Magazine;
	int m_iBeltSize;
	int m_iExpanded;
	vector m_vPitchPivot;
	vector m_vSeat;
	vector m_vMins;
	vector m_vMaxs;
	ref array<vector> m_aFeet;

	static IA_EmplacementProfile Create(bool heavy)
	{
		ref IA_EmplacementProfile p = new IA_EmplacementProfile();
		p.m_Prefab = PKM;
		p.m_Magazine = "{E5E9C5897CF47F44}Prefabs/Weapons/Magazines/Box_762x54_PK_100rnd_4Ball_1Tracer.et";
		p.m_iBeltSize = 100;
		p.m_iExpanded = 8; // preview: 6; retain room for runtime children
		p.m_vPitchPivot = "0 0.29 0.001443";
		p.m_vSeat = "-0.0773 0 -0.6825";
		p.m_vMins = "-0.39 -0.014 -0.70";
		p.m_vMaxs = "0.39 0.43 0.80";
		// Conservative bearing samples within the stock tripod footprint.
		p.m_aFeet = {"0 0 0.70", "-0.33 0 -0.60", "0.33 0 -0.60"};
		if (heavy)
		{
			p.m_Prefab = NSV;
			p.m_Magazine = "{843D9B182BCD2765}Prefabs/Weapons/Magazines/NSV/Box_127x108_NSV_50rnd_4AP_1APIT.et";
			p.m_iBeltSize = 50;
			p.m_iExpanded = 10; // preview: 7 including dovetail
			p.m_vPitchPivot = "0.0009811 0.394005 0.00282682";
			p.m_vSeat = "0 0 -1.1591";
			p.m_vMins = "-0.37 -0.014 -0.98";
			p.m_vMaxs = "0.45 0.70 0.97";
			p.m_aFeet = {"0 -0.003471 0.483057", "-0.281262 0.001725 -0.633374", "0.281262 0.001725 -0.633374"};
		}
		return p;
	}

	static IA_EmplacementProfile CreateKind(int kind)
	{
		ref IA_EmplacementProfile profile = Create(kind > 0);
		profile.m_iKind = kind;
		if (kind == 2)
			profile.m_Prefab = NSV_SPP;
		else if (kind == 3)
			profile.m_Prefab = AA;
		if (kind >= 2)
			profile.m_iExpanded = 12;
		return profile;
	}

	static const float BARREN_WALL_CHANCE = 0.7;

	static int GunCap(int layoutId)
	{
		if (layoutId == IA_DynamicSiteLayout.LAYOUT_FULL)
			return 6;
		if (layoutId == IA_DynamicSiteLayout.LAYOUT_COMPACT)
			return 5;
		return 4;
	}

	//! -1 keeps every face armed. 0-3 leaves that sandbag face empty.
	static int ChooseBarrenSide(int seed)
	{
		ref RandomGenerator rng = new RandomGenerator();
		rng.SetSeed(seed);
		if (rng.RandFloat01() >= BARREN_WALL_CHANCE)
			return -1;
		return rng.RandInt(0, 4);
	}

	static int CrewBudget(int installed, int budget)
	{
		return Math.Min(installed, Math.Min(Math.Floor(budget / 4.0), Math.Max(0, budget - 4)));
	}
}
