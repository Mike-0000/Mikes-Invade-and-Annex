//------------------------------------------------------------------------------------------------
//! Shows a skin on a paint channel by setting colour parameters on the
//! channel's materials. Nothing on the helicopter entity is touched, so it
//! works with the crew aboard and the engine running, as often as wanted.
//! A vehicle hull's mesh must never be changed from script: SetObject and
//! SetVObjectFromPrefab free the mesh instance the vehicle animation is bound
//! to, and the engine crashes on a later frame.
//! Local to the machine that calls it; IA_HeliSkinManagerComponent replicates.
//------------------------------------------------------------------------------------------------
class IA_HeliSkinPaint
{
	// What a skin's material may override. Anything else it sets, textures included, is ignored.
	protected static ref array<string> s_aColorParams;
	protected static ref array<string> s_aScalarParams;
	// Materials showing a skin, held so their colours last while no helicopter uses them.
	protected static ref map<string, ref Material> s_mHeld = new map<string, ref Material>();

	//------------------------------------------------------------------------------------------------
	//! Show a skin, or stock paint with IA_HeliSkinCatalog.SKIN_NONE, on a paint channel.
	//! \return number of surfaces set; 0 when the channel or the skin is unknown
	static int Apply(int channel, int skinId)
	{
		if (!IA_HeliPaintChannels.IsChannel(channel))
			return 0;

		IA_HeliSkinDef def;
		if (skinId != IA_HeliSkinCatalog.SKIN_NONE)
		{
			def = IA_HeliSkinCatalog.FindDef(skinId);
			if (!def)
				return 0;
		}

		EnsureParams();
		int painted;
		ResourceName colours;
		int surface;
		for (surface = 0; surface < IA_HeliPaintChannels.SURFACE_COUNT; surface++)
		{
			colours = ResourceName.Empty;
			if (def)
				colours = def.GetPaint(surface);
			// A surface the skin leaves alone goes back to stock, as does every surface for no skin.
			if (colours.IsEmpty())
				colours = IA_HeliPaintChannels.GetStockMaterial(surface);

			if (CopyParams(colours, IA_HeliPaintChannels.GetMaterial(surface, channel), def != null))
				painted = painted + 1;
		}
		return painted;
	}

	//------------------------------------------------------------------------------------------------
	//! Set every overridable parameter of a channel material to the value another material file has.
	//! Material.ResetParam goes to the material class default, not to the file's value, so it is
	//! only right for a parameter the file and the files it inherits leave unset.
	protected static bool CopyParams(ResourceName from, ResourceName to, bool hold)
	{
		Resource resource = Resource.Load(from);
		if (!resource || !resource.IsValid())
		{
			Print("[IA][HeliSkin] Colour material does not load: " + from, LogLevel.WARNING);
			return false;
		}

		BaseContainer source = resource.GetResource().ToBaseContainer();
		Material target = Material.GetOrLoadMaterial(to, 0);
		if (!source || !target)
		{
			Print("[IA][HeliSkin] Channel material does not load: " + to, LogLevel.WARNING);
			return false;
		}

		float color[4];
		foreach (string colorName : s_aColorParams)
		{
			if (ReadColor(source, colorName, color))
				target.SetParam(colorName, color);
			else
				target.ResetParam(colorName);
		}

		float scalar;
		foreach (string scalarName : s_aScalarParams)
		{
			if (source.Get(scalarName, scalar))
				target.SetParam(scalarName, scalar);
			else
				target.ResetParam(scalarName);
		}

		if (hold)
			s_mHeld.Set(to, target);
		else
			s_mHeld.Remove(to);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A material file stores a colour as "r g b a"; one it leaves unset is looked up in the file it inherits.
	//! eturn false when no file in the chain sets it
	static bool ReadColor(notnull BaseContainer source, string name, out float color[4])
	{
		string text;
		BaseContainer container = source;
		while (container)
		{
			text = string.Empty;
			if (container.Get(name, text) && !text.IsEmpty())
				break;
			container = container.GetAncestor();
		}
		if (!container)
			return false;

		ref array<string> parts = {};
		text.Split(" ", parts, true);
		if (parts.Count() < 3)
			return false;

		color[0] = parts[0].ToFloat();
		color[1] = parts[1].ToFloat();
		color[2] = parts[2].ToFloat();
		color[3] = 1;
		if (parts.Count() > 3)
			color[3] = parts[3].ToFloat();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return names of the colour parameters a skin may set
	static array<string> GetColorParams()
	{
		EnsureParams();
		return s_aColorParams;
	}

	//------------------------------------------------------------------------------------------------
	//! \return names of the number parameters a skin may set
	static array<string> GetScalarParams()
	{
		EnsureParams();
		return s_aScalarParams;
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureParams()
	{
		if (s_aColorParams)
			return;

		s_aColorParams = {"Color_1", "Color_2", "Color_3", "Color_4", "DirtColor"};
		s_aScalarParams = {"AO_1", "AO_2", "AO_3", "AO_4", "Roughness_1", "Roughness_2", "Roughness_3", "Roughness_4", "Metalness_1", "Metalness_2", "Metalness_3", "Metalness_4", "DirtOpacity"};
	}
}
