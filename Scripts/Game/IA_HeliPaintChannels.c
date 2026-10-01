//------------------------------------------------------------------------------------------------
//! Paint channels for the UH-1H. A material is shared by every entity that
//! names it, so recolouring the vanilla Huey material recolours every Huey. A
//! channel is a prefab variant of a stock airframe whose hull and seats name
//! their own copies of those materials; recolouring a channel's copies changes
//! only the helicopter spawned from it. The copies inherit the vanilla
//! materials, so a channel airframe looks stock until a skin is set.
//! The files come from tools/author_heli_paint_channels.py; the names and
//! GUIDs built here follow its pattern.
//------------------------------------------------------------------------------------------------
class IA_HeliPaintChannels
{
	static const int CHANNEL_NONE = 0;
	static const int CHANNEL_COUNT = 4;

	static const int SURFACE_BODY = 0;
	static const int SURFACE_INTERIOR_1 = 1;
	static const int SURFACE_INTERIOR_2 = 2;
	static const int SURFACE_COUNT = 3;

	protected static const string GUID_PREFIX = "D3A91F5C7E20";
	protected static const string MATERIAL_DIR = "Assets/Vehicles/Helicopters/UH1H/";
	protected static const string PREFAB_DIR = "Prefabs/Vehicles/Helicopters/UH1H/";

	protected static ref array<ResourceName> s_aStockPrefabs;
	// Airframe index * CHANNEL_COUNT + channel - 1.
	protected static ref array<ResourceName> s_aChannelPrefabs;
	protected static ref array<ResourceName> s_aStockMaterials;
	// Surface * CHANNEL_COUNT + channel - 1.
	protected static ref array<ResourceName> s_aChannelMaterials;

	//------------------------------------------------------------------------------------------------
	protected static void EnsureTables()
	{
		if (s_aStockPrefabs)
			return;

		s_aStockPrefabs = {};
		s_aChannelPrefabs = {};
		s_aStockMaterials = {};
		s_aChannelMaterials = {};

		AddAirframe("{70BAEEFC2D3FEE64}", "UH1H");
		AddAirframe("{DDDD9B51F1234DF3}", "UH1H_armed");
		AddAirframe("{21E9A875C0A3C409}", "UH1H_armed_gunship_HE");
		AddAirframe("{CB4D4CF7E887B2D0}", "UH1H_armed_gunship_HEDP");

		// In SURFACE_ order.
		AddSurface("{6224CA051369DE45}", "UH_1H_Body01");
		AddSurface("{04455994F64CAB1E}", "UH_1H_Interior01");
		AddSurface("{5AAD70D749C12511}", "UH_1H_Interior02");
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddAirframe(string stockGuid, string fileName)
	{
		int airframe = s_aStockPrefabs.Count();
		s_aStockPrefabs.Insert(string.Format("%1%2%3.et", stockGuid, PREFAB_DIR, fileName));

		int channel;
		for (channel = 1; channel <= CHANNEL_COUNT; channel++)
		{
			s_aChannelPrefabs.Insert(string.Format("%1%2Paint/IA_%3_Paint%4.et", Guid("B", airframe, channel), PREFAB_DIR, fileName, channel));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddSurface(string stockGuid, string fileName)
	{
		int surface = s_aStockMaterials.Count();
		s_aStockMaterials.Insert(string.Format("%1%2Data/%3.emat", stockGuid, MATERIAL_DIR, fileName));

		int channel;
		for (channel = 1; channel <= CHANNEL_COUNT; channel++)
		{
			s_aChannelMaterials.Insert(string.Format("%1%2Paint/IA_%3_Paint%4.emat", Guid("A", surface, channel), MATERIAL_DIR, fileName, channel));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static string Guid(string kind, int index, int channel)
	{
		string guid = "{" + GUID_PREFIX + kind;
		guid = guid + index.ToString() + "0";
		guid = guid + channel.ToString() + "}";
		return guid;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsChannel(int channel)
	{
		return channel >= 1 && channel <= CHANNEL_COUNT;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the channel a prefab belongs to, CHANNEL_NONE for stock airframes and everything else
	static int FindChannel(ResourceName prefab)
	{
		EnsureTables();
		int index = IndexOf(s_aChannelPrefabs, prefab);
		if (index < 0)
			return CHANNEL_NONE;
		return index % CHANNEL_COUNT + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return a stock airframe's variant on a channel, empty when it has none
	static ResourceName FindChannelPrefab(ResourceName stockPrefab, int channel)
	{
		EnsureTables();
		if (!IsChannel(channel))
			return ResourceName.Empty;

		int airframe = IndexOf(s_aStockPrefabs, stockPrefab);
		if (airframe < 0)
			return ResourceName.Empty;
		return s_aChannelPrefabs[airframe * CHANNEL_COUNT + channel - 1];
	}

	//------------------------------------------------------------------------------------------------
	//! \return the material a channel's meshes use for a surface, empty when out of range
	static ResourceName GetMaterial(int surface, int channel)
	{
		EnsureTables();
		if (surface < 0 || surface >= SURFACE_COUNT || !IsChannel(channel))
			return ResourceName.Empty;
		return s_aChannelMaterials[surface * CHANNEL_COUNT + channel - 1];
	}

	//------------------------------------------------------------------------------------------------
	//! \return the vanilla material of a surface, which holds the stock colours
	static ResourceName GetStockMaterial(int surface)
	{
		EnsureTables();
		if (surface < 0 || surface >= SURFACE_COUNT)
			return ResourceName.Empty;
		return s_aStockMaterials[surface];
	}

	//------------------------------------------------------------------------------------------------
	static array<ResourceName> GetStockPrefabs()
	{
		EnsureTables();
		return s_aStockPrefabs;
	}

	//------------------------------------------------------------------------------------------------
	//! Compares paths, so a prefab name with or without its GUID matches.
	protected static int IndexOf(notnull array<ResourceName> names, ResourceName name)
	{
		if (name.IsEmpty())
			return -1;

		string path = name.GetPath();
		int count = names.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			if (names[i].GetPath() == path)
				return i;
		}
		return -1;
	}
}
