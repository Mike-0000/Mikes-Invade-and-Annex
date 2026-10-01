//------------------------------------------------------------------------------------------------
//! Shows a livery on a paint channel by setting colour parameters on the
//! channel's materials. Nothing on the helicopter entity is touched, so it
//! works with the crew aboard and the engine running, as often as wanted.
//! A vehicle hull's mesh must never be changed from script: SetObject and
//! SetVObjectFromPrefab free the mesh instance the vehicle animation is bound
//! to, and the engine crashes on a later frame.
//! What is set comes from the channel's family (IA_HeliPaintManifest): which
//! parameters take the livery's colour, which take a fixed value, and what
//! each is in stock paint. No material file is read here.
//! Local to the machine that calls it; IA_HeliSkinManagerComponent replicates.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinPaint
{
	// Materials showing a livery, held so their colours last while no helicopter uses them.
	protected static ref map<string, ref Material> s_mHeld = new map<string, ref Material>();

	//------------------------------------------------------------------------------------------------
	//! Show a livery, or stock paint with IA_HeliSkinCatalog.SKIN_NONE, on a paint channel.
	//! \return number of surfaces set; 0 when the channel or the livery is unknown
	static int Apply(int channel, int skinId)
	{
		IA_HeliPaintFamily family = IA_HeliPaintChannels.GetFamily(channel);
		if (!family)
			return 0;

		IA_HeliSkinDef def;
		if (skinId != IA_HeliSkinCatalog.SKIN_NONE)
		{
			def = IA_HeliSkinCatalog.FindDef(skinId);
			if (!def)
				return 0;
		}

		int local = IA_HeliPaintChannels.GetLocalChannel(channel);
		int painted;
		foreach (IA_HeliPaintSurface surface : family.m_aSurfaces)
		{
			if (PaintSurface(surface, surface.GetMaterial(local), def))
				painted = painted + 1;
		}
		return painted;
	}

	//------------------------------------------------------------------------------------------------
	//! \param def the livery, null for stock paint
	protected static bool PaintSurface(notnull IA_HeliPaintSurface surface, ResourceName material, IA_HeliSkinDef def)
	{
		if (material.IsEmpty())
			return false;

		Material target = Material.GetOrLoadMaterial(material, 0);
		if (!target)
		{
			Print("[IA][HeliSkin] Channel material does not load: " + material, LogLevel.WARNING);
			return false;
		}

		float color[4];
		color[3] = 1;
		vector value;
		foreach (IA_HeliPaintParam param : surface.m_aParams)
		{
			// Material.ResetParam goes to the material class default, not to the file's value, so it
			// is only right for a parameter the stock file and the files it inherits leave unset.
			if (!def && !param.m_bStockSet)
			{
				target.ResetParam(param.m_sParam);
				continue;
			}

			if (param.m_iKind == IA_HeliPaintParam.KIND_SCALAR)
			{
				if (def)
					target.SetParam(param.m_sParam, param.m_fPaint);
				else
					target.SetParam(param.m_sParam, param.m_fStock);
				continue;
			}

			if (!def)
				value = param.m_vStock;
			else if (param.m_iKind == IA_HeliPaintParam.KIND_PRIMARY)
				value = def.m_vPaint * param.m_fGain;
			else
				value = param.m_vPaint;

			color[0] = value[0];
			color[1] = value[1];
			color[2] = value[2];
			target.SetParam(param.m_sParam, color);
		}

		if (def)
			s_mHeld.Set(material, target);
		else
			s_mHeld.Remove(material);
		return true;
	}
}
