//------------------------------------------------------------------------------------------------
//! One paint material of a helicopter family: the stock material, the copy each
//! paint channel uses, and the parameters a livery sets on it. Filled by
//! IA_HeliPaintManifest.
//------------------------------------------------------------------------------------------------
class IA_HeliPaintSurface
{
	ResourceName m_sStockMaterial;
	// The copy each channel of the family uses, at index local channel - 1.
	ref array<ResourceName> m_aChannelMaterials = {};
	ref array<ref IA_HeliPaintParam> m_aParams = {};

	//------------------------------------------------------------------------------------------------
	//! A colour layer that takes the livery's colour.
	void AddPrimary(string param, float gain, bool stockSet, float stockR, float stockG, float stockB)
	{
		IA_HeliPaintParam entry = AddParam(param, IA_HeliPaintParam.KIND_PRIMARY, stockSet);
		entry.m_fGain = gain;
		entry.m_vStock = Vector(stockR, stockG, stockB);
	}

	//------------------------------------------------------------------------------------------------
	//! A colour layer every livery sets to the same colour, such as trim.
	void AddColor(string param, float r, float g, float b, bool stockSet, float stockR, float stockG, float stockB)
	{
		IA_HeliPaintParam entry = AddParam(param, IA_HeliPaintParam.KIND_COLOR, stockSet);
		entry.m_vPaint = Vector(r, g, b);
		entry.m_vStock = Vector(stockR, stockG, stockB);
	}

	//------------------------------------------------------------------------------------------------
	//! A number every livery sets to the same value.
	void AddScalar(string param, float value, bool stockSet, float stock)
	{
		IA_HeliPaintParam entry = AddParam(param, IA_HeliPaintParam.KIND_SCALAR, stockSet);
		entry.m_fPaint = value;
		entry.m_fStock = stock;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the material a channel of the family uses for this surface, empty when out of range
	ResourceName GetMaterial(int localChannel)
	{
		if (localChannel < 1 || localChannel > m_aChannelMaterials.Count())
			return ResourceName.Empty;
		return m_aChannelMaterials[localChannel - 1];
	}

	//------------------------------------------------------------------------------------------------
	protected IA_HeliPaintParam AddParam(string param, int kind, bool stockSet)
	{
		ref IA_HeliPaintParam entry = new IA_HeliPaintParam();
		entry.m_sParam = param;
		entry.m_iKind = kind;
		entry.m_bStockSet = stockSet;
		m_aParams.Insert(entry);
		return entry;
	}
}
