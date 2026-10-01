//------------------------------------------------------------------------------------------------
//! Paint channels for helicopters. A material is shared by every entity that
//! names it, so recolouring a stock hull material recolours every helicopter of
//! that type. A channel is a prefab variant of a stock airframe whose hull and
//! parts name their own copies of the paint materials; recolouring a channel's
//! copies changes only the helicopter spawned from it. The copies inherit the
//! stock materials, so a channel airframe looks stock until a livery is set.
//! Each helicopter type is a family with CHANNEL_COUNT channels of its own. A
//! channel number here is global: family index * CHANNEL_COUNT + 1..CHANNEL_COUNT.
//! The families and their files come from tools/author_heli_paint_channels.py
//! by way of IA_HeliPaintManifest.
//------------------------------------------------------------------------------------------------
class IA_HeliPaintChannels
{
	static const int CHANNEL_NONE = 0;
	static const int CHANNEL_COUNT = 12;

	protected static ref array<ref IA_HeliPaintFamily> s_aFamilies;
	// Path of a channel prefab -> its channel; path of a stock airframe -> family index * STOCK_STRIDE + airframe.
	protected static ref map<string, int> s_mChannels;
	protected static ref map<string, int> s_mStock;
	protected static const int STOCK_STRIDE = 1000;

	//------------------------------------------------------------------------------------------------
	protected static void EnsureFamilies()
	{
		if (s_aFamilies)
			return;

		s_aFamilies = {};
		s_mChannels = new map<string, int>();
		s_mStock = new map<string, int>();
		IA_HeliPaintManifest.Register();

		IA_HeliPaintFamily family;
		int familyCount = s_aFamilies.Count();
		int airframeCount;
		int f;
		int airframe;
		int local;
		ResourceName twin;
		for (f = 0; f < familyCount; f++)
		{
			family = s_aFamilies[f];
			airframeCount = family.m_aStockPrefabs.Count();
			for (airframe = 0; airframe < airframeCount; airframe++)
			{
				s_mStock.Set(family.m_aStockPrefabs[airframe].GetPath(), f * STOCK_STRIDE + airframe);
				for (local = 1; local <= CHANNEL_COUNT; local++)
				{
					twin = family.GetChannelPrefab(airframe, local);
					if (!twin.IsEmpty())
						s_mChannels.Set(twin.GetPath(), f * CHANNEL_COUNT + local);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Called by IA_HeliPaintManifest for each helicopter type.
	//! \return the new family to fill, null when the key is already taken
	static IA_HeliPaintFamily AddFamily(string key, string displayName, string stockPaint, string art)
	{
		if (!s_aFamilies)
			return null;

		foreach (IA_HeliPaintFamily known : s_aFamilies)
		{
			if (known.m_sKey == key)
			{
				Print("[IA][HeliSkin] Paint family registered twice: " + key, LogLevel.WARNING);
				return null;
			}
		}

		ref IA_HeliPaintFamily family = new IA_HeliPaintFamily();
		family.m_iIndex = s_aFamilies.Count();
		family.m_sKey = key;
		family.m_sDisplayName = displayName;
		family.m_sStockPaint = stockPaint;
		family.m_sArt = art;
		s_aFamilies.Insert(family);
		return family;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref IA_HeliPaintFamily> GetFamilies()
	{
		EnsureFamilies();
		return s_aFamilies;
	}

	//------------------------------------------------------------------------------------------------
	static IA_HeliPaintFamily FindFamily(string key)
	{
		EnsureFamilies();
		foreach (IA_HeliPaintFamily family : s_aFamilies)
		{
			if (family.m_sKey == key)
				return family;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the highest channel number
	static int GetChannelTotal()
	{
		EnsureFamilies();
		return s_aFamilies.Count() * CHANNEL_COUNT;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsChannel(int channel)
	{
		return channel >= 1 && channel <= GetChannelTotal();
	}

	//------------------------------------------------------------------------------------------------
	//! \return the family a channel belongs to, null when it is not a channel
	static IA_HeliPaintFamily GetFamily(int channel)
	{
		if (!IsChannel(channel))
			return null;
		return s_aFamilies[(channel - 1) / CHANNEL_COUNT];
	}

	//------------------------------------------------------------------------------------------------
	//! \return a channel's number within its family, 1..CHANNEL_COUNT; CHANNEL_NONE when it is not a channel
	static int GetLocalChannel(int channel)
	{
		if (!IsChannel(channel))
			return CHANNEL_NONE;
		return (channel - 1) % CHANNEL_COUNT + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the global number of a family's channel, CHANNEL_NONE when out of range
	static int ToChannel(IA_HeliPaintFamily family, int localChannel)
	{
		if (!family || localChannel < 1 || localChannel > CHANNEL_COUNT)
			return CHANNEL_NONE;
		return family.m_iIndex * CHANNEL_COUNT + localChannel;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the channel a prefab belongs to, CHANNEL_NONE for stock airframes and everything else
	static int FindChannel(ResourceName prefab)
	{
		EnsureFamilies();
		if (prefab.IsEmpty())
			return CHANNEL_NONE;

		// Paths are compared, so a prefab name with or without its GUID matches.
		int channel;
		if (s_mChannels.Find(prefab.GetPath(), channel))
			return channel;
		return CHANNEL_NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the family a stock airframe has channel variants in, null for everything else
	static IA_HeliPaintFamily FindStockFamily(ResourceName prefab)
	{
		EnsureFamilies();
		if (prefab.IsEmpty())
			return null;

		int entry;
		if (!s_mStock.Find(prefab.GetPath(), entry))
			return null;
		return s_aFamilies[entry / STOCK_STRIDE];
	}

	//------------------------------------------------------------------------------------------------
	//! \return true for a stock airframe that has channel variants
	static bool IsStockAirframe(ResourceName prefab)
	{
		return FindStockFamily(prefab) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return a stock airframe's variant on a channel of its family, empty when it has none
	static ResourceName FindChannelPrefab(ResourceName stockPrefab, int channel)
	{
		EnsureFamilies();
		if (stockPrefab.IsEmpty() || !IsChannel(channel))
			return ResourceName.Empty;

		int entry;
		if (!s_mStock.Find(stockPrefab.GetPath(), entry))
			return ResourceName.Empty;

		IA_HeliPaintFamily family = s_aFamilies[entry / STOCK_STRIDE];
		if (family != GetFamily(channel))
			return ResourceName.Empty;
		return family.GetChannelPrefab(entry % STOCK_STRIDE, GetLocalChannel(channel));
	}

	//------------------------------------------------------------------------------------------------
	//! \param surface index in the family's surfaces
	//! \return the material a channel's meshes use for a surface, empty when out of range
	static ResourceName GetMaterial(int surface, int channel)
	{
		IA_HeliPaintFamily family = GetFamily(channel);
		if (!family || surface < 0 || surface >= family.m_aSurfaces.Count())
			return ResourceName.Empty;
		return family.m_aSurfaces[surface].GetMaterial(GetLocalChannel(channel));
	}
}
