//------------------------------------------------------------------------------------------------
//! Says which skin each paint channel shows and paints it on every machine
//! that renders. A helicopter spawned from a channel prefab is the only one
//! that uses that channel's materials, so the skin of a channel is the skin of
//! that helicopter. The server changes it with SetVehicleSkin at any time, crew
//! aboard or not; the entity is never touched or replaced.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Invade & Annex/Components", description: "Replicated helicopter skin assignments.")]
class IA_HeliSkinManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

class IA_HeliSkinManagerComponent : SCR_BaseGameModeComponent
{
	protected static const int APPLY_INTERVAL_MS = 2000;

	// Skin on each paint channel, at index channel - 1.
	[RplProp(onRplName: "OnSkinsReplicated")]
	protected ref array<int> m_aChannelSkins = {};

	// Skin this machine's materials show for each channel.
	protected ref array<int> m_aShown = {};
	// Server: channels whose skin the pilot picked in the paint bay.
	protected ref array<bool> m_aPilotChoice = {};
	protected static IA_HeliSkinManagerComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	static IA_HeliSkinManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the paint channel of a live helicopter, IA_HeliPaintChannels.CHANNEL_NONE when it has none
	static int GetVehicleChannel(IEntity vehicle)
	{
		if (!vehicle || vehicle.IsDeleted())
			return IA_HeliPaintChannels.CHANNEL_NONE;

		EntityPrefabData prefab = vehicle.GetPrefabData();
		if (!prefab)
			return IA_HeliPaintChannels.CHANNEL_NONE;

		// A wreck stays where it fell while the pad hands its channel to the next airframe.
		DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
		if (damage && damage.IsDestroyed())
			return IA_HeliPaintChannels.CHANNEL_NONE;

		return IA_HeliPaintChannels.FindChannel(prefab.GetPrefabName());
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		int channel;
		for (channel = 1; channel <= IA_HeliPaintChannels.CHANNEL_COUNT; channel++)
		{
			m_aChannelSkins.Insert(IA_HeliSkinCatalog.SKIN_NONE);
			m_aShown.Insert(IA_HeliSkinCatalog.SKIN_NONE);
			m_aPilotChoice.Insert(false);
		}
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		if (s_Instance && s_Instance != this)
		{
			Print("[IA][HeliSkin] Instance already exists.", LogLevel.WARNING);
			return;
		}

		s_Instance = this;

		// A dedicated server renders nothing. The timer covers a join, where the state arrives with the entity.
		if (RplSession.Mode() != RplMode.Dedicated)
			GetGame().GetCallqueue().CallLater(this.PaintAll, APPLY_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
		{
			s_Instance = null;
			if (GetGame() && GetGame().GetCallqueue())
				GetGame().GetCallqueue().Remove(this.PaintAll);

			// The materials outlive the mission; leave them stock for the next one.
			int count = m_aShown.Count();
			int i;
			for (i = 0; i < count; i++)
			{
				if (m_aShown[i] != IA_HeliSkinCatalog.SKIN_NONE)
					IA_HeliSkinPaint.Apply(i + 1, IA_HeliSkinCatalog.SKIN_NONE);
			}
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: put a skin on a helicopter where it stands, or stock paint with IA_HeliSkinCatalog.SKIN_NONE.
	//! \param pilotChoice the pilot picked it in the paint bay, so the pad service leaves it alone
	//! \return true when the helicopter now wears that skin; false when it has no paint channel
	bool SetVehicleSkin(IEntity vehicle, int skinId, bool pilotChoice = false)
	{
		if (!Replication.IsServer())
			return false;

		int channel = GetVehicleChannel(vehicle);
		if (channel == IA_HeliPaintChannels.CHANNEL_NONE)
			return false;
		if (skinId != IA_HeliSkinCatalog.SKIN_NONE && !IA_HeliSkinCatalog.FindDef(skinId))
			return false;

		// A new airframe on the pad is set to stock without the flag, which clears it.
		m_aPilotChoice[channel - 1] = pilotChoice;
		if (m_aChannelSkins[channel - 1] == skinId)
			return true;

		m_aChannelSkins[channel - 1] = skinId;
		Replication.BumpMe();

		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][HeliSkin] Skin %1 set on paint channel %2.", skinId, channel), LogLevel.NORMAL);
		}

		// A hosting or solo machine paints its own copy now.
		PaintAll();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return skin a helicopter wears, IA_HeliSkinCatalog.SKIN_NONE for stock paint or no paint channel
	int GetVehicleSkin(IEntity vehicle)
	{
		int channel = GetVehicleChannel(vehicle);
		if (channel == IA_HeliPaintChannels.CHANNEL_NONE || channel > m_aChannelSkins.Count())
			return IA_HeliSkinCatalog.SKIN_NONE;
		return m_aChannelSkins[channel - 1];
	}

	//------------------------------------------------------------------------------------------------
	//! Server. \return true when the skin this helicopter wears was picked by its pilot
	bool IsPilotChoice(IEntity vehicle)
	{
		int channel = GetVehicleChannel(vehicle);
		if (channel == IA_HeliPaintChannels.CHANNEL_NONE || channel > m_aPilotChoice.Count())
			return false;
		return m_aPilotChoice[channel - 1];
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSkinsReplicated()
	{
		PaintAll();
	}

	//------------------------------------------------------------------------------------------------
	//! Bring this machine's materials in line with the replicated skins.
	protected void PaintAll()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		int count = m_aChannelSkins.Count();
		if (m_aShown.Count() < count)
			count = m_aShown.Count();

		int wanted;
		int i;
		for (i = 0; i < count; i++)
		{
			wanted = m_aChannelSkins[i];
			if (m_aShown[i] == wanted)
				continue;

			// Settle it either way; a material that does not load is not retried every tick.
			m_aShown[i] = wanted;
			IA_HeliSkinPaint.Apply(i + 1, wanted);
		}
	}
}
