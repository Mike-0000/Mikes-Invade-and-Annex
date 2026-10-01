//------------------------------------------------------------------------------------------------
//! Paint channel families, written by tools/author_heli_paint_channels.py from
//! tools/heli_paint_families.json. Do not edit: change the registry and run the tool.
//------------------------------------------------------------------------------------------------
class IA_HeliPaintManifest
{
	//------------------------------------------------------------------------------------------------
	static void Register()
	{
		RegisterUh1h();
		RegisterMi8mt();
	}

	//------------------------------------------------------------------------------------------------
	protected static void RegisterUh1h()
	{
		IA_HeliPaintFamily family = IA_HeliPaintChannels.AddFamily("uh1h", "UH-1H IROQUOIS", "Factory Olive", "huey");
		if (!family)
			return;

		family.SetStockSwatch(78, 88, 60);
		family.AddAirframe("{70BAEEFC2D3FEE64}Prefabs/Vehicles/Helicopters/UH1H/UH1H.et", "Prefabs/Vehicles/Helicopters/UH1H/Paint/IA_UH1H_Paint%1.et", "D3A91F5C7E20B001 D3A91F5C7E20B002 D3A91F5C7E20B003 D3A91F5C7E20B004 D3A91F5C7E20B005 D3A91F5C7E20B006 D3A91F5C7E20B007 D3A91F5C7E20B008 D3A91F5C7E20B009 D3A91F5C7E20B010 D3A91F5C7E20B011 D3A91F5C7E20B012");
		family.AddAirframe("{DDDD9B51F1234DF3}Prefabs/Vehicles/Helicopters/UH1H/UH1H_armed.et", "Prefabs/Vehicles/Helicopters/UH1H/Paint/IA_UH1H_armed_Paint%1.et", "D3A91F5C7E20B101 D3A91F5C7E20B102 D3A91F5C7E20B103 D3A91F5C7E20B104 D3A91F5C7E20B105 D3A91F5C7E20B106 D3A91F5C7E20B107 D3A91F5C7E20B108 D3A91F5C7E20B109 D3A91F5C7E20B110 D3A91F5C7E20B111 D3A91F5C7E20B112");
		family.AddAirframe("{21E9A875C0A3C409}Prefabs/Vehicles/Helicopters/UH1H/UH1H_armed_gunship_HE.et", "Prefabs/Vehicles/Helicopters/UH1H/Paint/IA_UH1H_armed_gunship_HE_Paint%1.et", "D3A91F5C7E20B201 D3A91F5C7E20B202 D3A91F5C7E20B203 D3A91F5C7E20B204 D3A91F5C7E20B205 D3A91F5C7E20B206 D3A91F5C7E20B207 D3A91F5C7E20B208 D3A91F5C7E20B209 D3A91F5C7E20B210 D3A91F5C7E20B211 D3A91F5C7E20B212");
		family.AddAirframe("{CB4D4CF7E887B2D0}Prefabs/Vehicles/Helicopters/UH1H/UH1H_armed_gunship_HEDP.et", "Prefabs/Vehicles/Helicopters/UH1H/Paint/IA_UH1H_armed_gunship_HEDP_Paint%1.et", "D3A91F5C7E20B301 D3A91F5C7E20B302 D3A91F5C7E20B303 D3A91F5C7E20B304 D3A91F5C7E20B305 D3A91F5C7E20B306 D3A91F5C7E20B307 D3A91F5C7E20B308 D3A91F5C7E20B309 D3A91F5C7E20B310 D3A91F5C7E20B311 D3A91F5C7E20B312");

		IA_HeliPaintSurface surface;
		surface = family.AddSurface("{6224CA051369DE45}Assets/Vehicles/Helicopters/UH1H/Data/UH_1H_Body01.emat", "Assets/Vehicles/Helicopters/UH1H/Paint/IA_UH_1H_Body01_Paint%1.emat", "D3A91F5C7E20A001 D3A91F5C7E20A002 D3A91F5C7E20A003 D3A91F5C7E20A004 D3A91F5C7E20A005 D3A91F5C7E20A006 D3A91F5C7E20A007 D3A91F5C7E20A008 D3A91F5C7E20A009 D3A91F5C7E20A010 D3A91F5C7E20A011 D3A91F5C7E20A012");
		surface.AddPrimary("Color_1", 1, true, 0.042, 0.037, 0.026);
		surface.AddPrimary("Color_2", 1.55, true, 0.065, 0.061, 0.038);
		surface.AddPrimary("Color_4", 9, true, 0.591, 0.591, 0.591);
		surface.AddScalar("Roughness_4", 3, true, 1.4);
		surface = family.AddSurface("{04455994F64CAB1E}Assets/Vehicles/Helicopters/UH1H/Data/UH_1H_Interior01.emat", "Assets/Vehicles/Helicopters/UH1H/Paint/IA_UH_1H_Interior01_Paint%1.emat", "D3A91F5C7E20A101 D3A91F5C7E20A102 D3A91F5C7E20A103 D3A91F5C7E20A104 D3A91F5C7E20A105 D3A91F5C7E20A106 D3A91F5C7E20A107 D3A91F5C7E20A108 D3A91F5C7E20A109 D3A91F5C7E20A110 D3A91F5C7E20A111 D3A91F5C7E20A112");
		surface.AddColor("Color_1", 0.184, 0.184, 0.184, true, 0.366, 0.366, 0.366);
		surface.AddColor("Color_2", 0.209, 0, 0, true, 0.235, 0.235, 0.235);
		surface.AddColor("Color_3", 0.004, 0.004, 0.004, true, 0.024, 0.024, 0.024);
		surface = family.AddSurface("{5AAD70D749C12511}Assets/Vehicles/Helicopters/UH1H/Data/UH_1H_Interior02.emat", "Assets/Vehicles/Helicopters/UH1H/Paint/IA_UH_1H_Interior02_Paint%1.emat", "D3A91F5C7E20A201 D3A91F5C7E20A202 D3A91F5C7E20A203 D3A91F5C7E20A204 D3A91F5C7E20A205 D3A91F5C7E20A206 D3A91F5C7E20A207 D3A91F5C7E20A208 D3A91F5C7E20A209 D3A91F5C7E20A210 D3A91F5C7E20A211 D3A91F5C7E20A212");
		surface.AddColor("Color_1", 0.004, 0.004, 0.004, true, 0.051, 0.051, 0.051);
		surface.AddPrimary("Color_2", 1, true, 0.31, 0.278, 0.204);
		surface.AddPrimary("Color_3", 1, true, 0.694, 0.694, 0.694);
		surface.AddPrimary("DirtColor", 1, true, 0.043, 0.035, 0.02);
		surface.AddScalar("DirtOpacity", 1.5, true, 1.87);
		surface.AddScalar("AO_3", 0.357, false, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected static void RegisterMi8mt()
	{
		IA_HeliPaintFamily family = IA_HeliPaintChannels.AddFamily("mi8mt", "MI-8MT HIP", "Factory Camo", "hip");
		if (!family)
			return;

		family.SetStockSwatch(112, 110, 82);
		family.AddAirframe("{DF5CCB7C0FF049F4}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_transport.et", "Prefabs/Vehicles/Helicopters/Mi8MT/Paint/IA_Mi8MT_unarmed_transport_Paint%1.et", "0D2DEF60AEE1051E A5216EBDAC85C313 82E3C08DA6DD72D5 7CEBF6E72EAE69C8 CBCC83B2A3919686 13E306929D3968EC 7AEEFC338F8A9CC8 7507147036261749 EB52A85FAB6313DF 64D6AA6C98A56A4B 692EF525217F6E93 5467937EEF38D117");
		family.AddAirframe("{7BD282AF716ED639}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_armed.et", "Prefabs/Vehicles/Helicopters/Mi8MT/Paint/IA_Mi8MT_armed_Paint%1.et", "8067ED406F21D42B A9C48BB391AF3C04 D00EBA25E738385C 5A49752A0A7F8749 D992DE3E06DD7C89 9C6E0FDE52A17ECD B6428ACDCD17D013 B02A0BBB88C56B2E 61949D8566965854 21FE8A90A2959640 A69B9E2C6F88C6AD 33D1D34DEC603AB4");
		family.AddAirframe("{3C6B3ED0C3AC30D5}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_armed_gunship_HE.et", "Prefabs/Vehicles/Helicopters/Mi8MT/Paint/IA_Mi8MT_armed_gunship_HE_Paint%1.et", "71E6C72AEFBE77A2 2221C13F8A474E19 B71A9419172E47C2 1F449C38A129BE00 8E42E14AC271B238 62B47EFF923558B6 08633E23DDCFD7C4 7755B8944F889CCD 4CCAEAF0852E8AF5 6CE2FCF107941FDD D4BCF7B985C462C1 52EE4FE4A7A2093B");
		family.AddAirframe("{DBEC63C9DEE4358C}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_armed_gunship_HEDP.et", "Prefabs/Vehicles/Helicopters/Mi8MT/Paint/IA_Mi8MT_armed_gunship_HEDP_Paint%1.et", "264E570692C4ABBF 1CC54FFBEAB66857 3A71241EA79AB9BF 966EC83944CE9513 A242765D5547CBCB 1CC1111CEBCF2908 50B6E21A1A6A55A3 AC506E886FAC39AD F8BEACB4A48F0E1B E8A404651F36E5BD C35D90CFF93683FE 59689C159061CF9A");

		IA_HeliPaintSurface surface;
		surface = family.AddSurface("{35715EB3956D22B9}Assets/Vehicles/Helicopters/Mi8/Data/Mi8_Body.emat", "Assets/Vehicles/Helicopters/Mi8MT/Paint/IA_Mi8_Body_Paint%1.emat", "B7A4D22AA9F2188E 63B39F3E7280B1B9 19DDEB8B00F6EA61 802DC23A11ED14B1 09176C80A51717AB C1B7766D5DF37B33 199A751BBEE183D1 0B89DCA6B46DCBE9 28410A3800797F51 28E5E49794662326 F063054C7D0B5F8F 9830105C284E1DB1");
		surface.AddPrimary("Color_1", 1, true, 0.223, 0.209, 0.143);
		surface.AddPrimary("Color_2", 1, true, 0.089, 0.095, 0.049);
		surface.AddPrimary("Color_3", 1, true, 0.181, 0.355, 0.451);
		surface.AddColor("Specular", 0.39, 0.39, 0.39, true, 0.418, 0.402, 0.326);
		surface.AddColor("SpecularIBL", 0.74, 0.74, 0.74, true, 0.823, 0.756, 0.59);
	}
}
