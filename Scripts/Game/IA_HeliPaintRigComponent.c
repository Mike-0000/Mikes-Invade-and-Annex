//------------------------------------------------------------------------------------------------
//! Sits on every paint channel airframe (tools/author_heli_paint_channels.py
//! writes it into the hull prefabs) and tells the server which live helicopter
//! holds each channel. Whatever spawns a stock Huey asks ResolveSpawnPrefab
//! first and gets its twin on a channel nobody holds, so a helicopter from a
//! pad or from the editor can be repainted alone.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Invade & Annex/Components", description: "Marks a helicopter spawned on a paint channel.")]
class IA_HeliPaintRigComponentClass : ScriptComponentClass
{
}

class IA_HeliPaintRigComponent : ScriptComponent
{
	// Server: the helicopter on each paint channel, at index channel - 1.
	protected static ref array<IEntity> s_aHolders = {};
	protected static bool s_bWarnedFull;

	protected int m_iChannel = IA_HeliPaintChannels.CHANNEL_NONE;

	//------------------------------------------------------------------------------------------------
	//! Server. \return the stock airframe's twin on a free paint channel; the prefab itself when it
	//! is not a stock airframe or every channel is held
	static ResourceName ResolveSpawnPrefab(ResourceName prefab)
	{
		if (!Replication.IsServer() || !GetGame().InPlayMode())
			return prefab;
		if (!IA_HeliPaintChannels.IsStockAirframe(prefab))
			return prefab;

		int channel = FindFreeChannel();
		if (channel == IA_HeliPaintChannels.CHANNEL_NONE)
		{
			if (!s_bWarnedFull)
			{
				s_bWarnedFull = true;
				Print(string.Format("[IA][HeliSkin] All %1 paint channels are held; further Hueys spawn in stock paint and cannot be repainted.", IA_HeliPaintChannels.CHANNEL_COUNT), LogLevel.WARNING);
			}
			return prefab;
		}

		s_bWarnedFull = false;
		return IA_HeliPaintChannels.FindChannelPrefab(prefab, channel);
	}

	//------------------------------------------------------------------------------------------------
	//! Server. \return a channel no live helicopter holds, CHANNEL_NONE when all are held
	static int FindFreeChannel()
	{
		// A wreck still shows its channel's paint, so an empty channel goes first.
		int wrecked = IA_HeliPaintChannels.CHANNEL_NONE;
		IEntity holder;
		DamageManagerComponent damage;
		int channel;
		for (channel = 1; channel <= IA_HeliPaintChannels.CHANNEL_COUNT; channel++)
		{
			holder = GetHolder(channel);
			if (!holder)
				return channel;
			if (wrecked != IA_HeliPaintChannels.CHANNEL_NONE)
				continue;

			damage = DamageManagerComponent.Cast(holder.FindComponent(DamageManagerComponent));
			if (damage && damage.IsDestroyed())
				wrecked = channel;
		}
		return wrecked;
	}

	//------------------------------------------------------------------------------------------------
	//! Server. \return the helicopter holding a channel, null when it is free
	static IEntity GetHolder(int channel)
	{
		if (channel < 1 || channel > s_aHolders.Count())
			return null;

		IEntity holder = s_aHolders[channel - 1];
		if (!holder || holder.IsDeleted())
			return null;
		return holder;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer() || !GetGame().InPlayMode())
			return;

		EntityPrefabData prefab = owner.GetPrefabData();
		if (!prefab)
			return;

		m_iChannel = IA_HeliPaintChannels.FindChannel(prefab.GetPrefabName());
		if (m_iChannel == IA_HeliPaintChannels.CHANNEL_NONE)
			return;

		while (s_aHolders.Count() < IA_HeliPaintChannels.CHANNEL_COUNT)
		{
			s_aHolders.Insert(null);
		}
		s_aHolders[m_iChannel - 1] = owner;

		// The channel may still show the skin of the airframe that held it before.
		IA_HeliSkinManagerComponent skins = IA_HeliSkinManagerComponent.GetInstance();
		if (skins)
			skins.ResetChannel(m_iChannel);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		// A wreck's channel may already belong to the next airframe.
		if (m_iChannel >= 1 && m_iChannel <= s_aHolders.Count() && s_aHolders[m_iChannel - 1] == owner)
			s_aHolders[m_iChannel - 1] = null;

		super.OnDelete(owner);
	}
}
