//------------------------------------------------------------------------------------------------
//! One helicopter type that takes liveries: its stock airframes, the twin of
//! each on every paint channel, and its paint surfaces. A family comes from
//! IA_HeliPaintManifest, which tools/author_heli_paint_channels.py writes for
//! vanilla and modded helicopters alike.
//------------------------------------------------------------------------------------------------
class IA_HeliPaintFamily
{
	int m_iIndex;
	string m_sKey;
	// Shown in the paint bay.
	string m_sDisplayName;
	string m_sStockPaint;
	// Which silhouette the paint bay draws; see IA_HeliArt.
	string m_sArt;
	// Hull colour of stock paint as sRGB bytes.
	int m_iStockR = 78;
	int m_iStockG = 88;
	int m_iStockB = 60;

	ref array<ref IA_HeliPaintSurface> m_aSurfaces = {};
	ref array<ResourceName> m_aStockPrefabs = {};
	// Airframe index * IA_HeliPaintChannels.CHANNEL_COUNT + local channel - 1.
	ref array<ResourceName> m_aChannelPrefabs = {};

	//------------------------------------------------------------------------------------------------
	void SetStockSwatch(int r, int g, int b)
	{
		m_iStockR = r;
		m_iStockG = g;
		m_iStockB = b;
	}

	//------------------------------------------------------------------------------------------------
	//! \param pattern path of the airframe's twins with %1 for the channel
	//! \param guids the GUID of each channel's twin, separated by spaces
	void AddAirframe(ResourceName stockPrefab, string pattern, string guids)
	{
		m_aStockPrefabs.Insert(stockPrefab);
		Expand(pattern, guids, m_aChannelPrefabs);
	}

	//------------------------------------------------------------------------------------------------
	//! \param pattern path of the material's copies with %1 for the channel
	//! \param guids the GUID of each channel's copy, separated by spaces
	IA_HeliPaintSurface AddSurface(ResourceName stockMaterial, string pattern, string guids)
	{
		ref IA_HeliPaintSurface surface = new IA_HeliPaintSurface();
		surface.m_sStockMaterial = stockMaterial;
		Expand(pattern, guids, surface.m_aChannelMaterials);
		m_aSurfaces.Insert(surface);
		return surface;
	}

	//------------------------------------------------------------------------------------------------
	//! \return a stock airframe's twin on a channel of this family, empty when out of range
	ResourceName GetChannelPrefab(int airframe, int localChannel)
	{
		if (airframe < 0 || localChannel < 1 || localChannel > IA_HeliPaintChannels.CHANNEL_COUNT)
			return ResourceName.Empty;

		int index = airframe * IA_HeliPaintChannels.CHANNEL_COUNT + localChannel - 1;
		if (index >= m_aChannelPrefabs.Count())
			return ResourceName.Empty;
		return m_aChannelPrefabs[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Always adds one name per channel, so the tables keep their shape when a manifest line is wrong.
	protected void Expand(string pattern, string guids, notnull array<ResourceName> into)
	{
		ref array<string> parts = {};
		guids.Split(" ", parts, true);
		if (parts.Count() != IA_HeliPaintChannels.CHANNEL_COUNT)
			Print(string.Format("[IA][HeliSkin] %1 lists %2 paint channels, not %3: %4", m_sKey, parts.Count(), IA_HeliPaintChannels.CHANNEL_COUNT, pattern), LogLevel.ERROR);

		int channel;
		for (channel = 1; channel <= IA_HeliPaintChannels.CHANNEL_COUNT; channel++)
		{
			if (channel > parts.Count())
			{
				into.Insert(ResourceName.Empty);
				continue;
			}
			into.Insert("{" + parts[channel - 1] + "}" + string.Format(pattern, channel));
		}
	}
}
