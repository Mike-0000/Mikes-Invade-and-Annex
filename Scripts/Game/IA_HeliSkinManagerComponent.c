//------------------------------------------------------------------------------------------------
//! Replicates which vehicles wear which IA_HeliSkinCatalog skin and paints them
//! on every machine that renders. SetObject material remaps are local, and a
//! vehicle that streams back in is a fresh entity, so clients re-check on a timer.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Invade & Annex/Components", description: "Replicated helicopter skin assignments.")]
class IA_HeliSkinManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

class IA_HeliSkinManagerComponent : SCR_BaseGameModeComponent
{
	protected static const int APPLY_INTERVAL_MS = 2000;
	protected static const int MAX_DEPTH = 4;

	[RplProp(onRplName: "OnSkinsReplicated")]
	protected ref array<RplId> m_aSkinVehicles = {};

	[RplProp(onRplName: "OnSkinsReplicated")]
	protected ref array<int> m_aSkinIds = {};

	// Skin already painted on a live entity, keyed by entity so a re-streamed vehicle is painted again.
	protected ref map<EntityID, int> m_mPainted;
	protected static IA_HeliSkinManagerComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	static IA_HeliSkinManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
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
		m_mPainted = new map<EntityID, int>();

		// A dedicated server renders nothing.
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
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: put a skin on a vehicle. The airframe keeps it until another skin replaces it.
	void SetVehicleSkin(IEntity vehicle, int skinId)
	{
		if (!Replication.IsServer() || !vehicle)
			return;
		if (!IA_HeliSkinCatalog.FindDef(skinId))
			return;

		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (!rpl)
			return;

		RplId id = rpl.Id();
		if (!id.IsValid())
			return;

		int index = m_aSkinVehicles.Find(id);
		if (index >= 0)
		{
			if (m_aSkinIds[index] == skinId)
				return;
			m_aSkinIds[index] = skinId;
		}
		else
		{
			PruneDeleted();
			m_aSkinVehicles.Insert(id);
			m_aSkinIds.Insert(skinId);
		}

		Replication.BumpMe();
		if (IA_Log.IsDebugEnabled())
		{
			Print(string.Format("[IA][HeliSkin] Skin %1 assigned, %2 skinned vehicles tracked.", skinId, m_aSkinVehicles.Count()), LogLevel.NORMAL);
		}
		PaintAll();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: forget vehicles that no longer exist so the replicated list stays short.
	protected void PruneDeleted()
	{
		int i;
		for (i = m_aSkinVehicles.Count() - 1; i >= 0; i--)
		{
			if (Replication.FindItem(m_aSkinVehicles[i]))
				continue;
			m_aSkinVehicles.Remove(i);
			m_aSkinIds.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSkinsReplicated()
	{
		PaintAll();
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintAll()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;
		if (!m_aSkinVehicles || !m_aSkinIds || !m_mPainted)
			return;

		// The two props can arrive in separate updates.
		int count = m_aSkinVehicles.Count();
		if (m_aSkinIds.Count() < count)
			count = m_aSkinIds.Count();

		int i;
		for (i = 0; i < count; i++)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(m_aSkinVehicles[i]));
			if (!rpl)
				continue;
			IEntity vehicle = rpl.GetEntity();
			if (!vehicle)
				continue;

			int skinId = m_aSkinIds[i];
			EntityID key = vehicle.GetID();
			if (m_mPainted.Contains(key) && m_mPainted.Get(key) == skinId)
				continue;

			IA_HeliSkinDef def = IA_HeliSkinCatalog.FindDef(skinId);
			if (!def)
				continue;

			int painted = PaintTree(vehicle, def, 0);
			m_mPainted.Set(key, skinId);
			if (painted == 0)
				Print(string.Format("[IA][HeliSkin] Skin %1 matched no material slot on its vehicle.", def.m_sKey), LogLevel.WARNING);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Paint one vehicle on this machine only, to look at a skin without earning it.
	//! Nothing is replicated; a skin the server assigns later paints over it.
	bool PaintLocal(IEntity vehicle, int skinId)
	{
		if (RplSession.Mode() == RplMode.Dedicated || !vehicle || !m_mPainted)
			return false;

		IA_HeliSkinDef def = IA_HeliSkinCatalog.FindDef(skinId);
		if (!def || PaintTree(vehicle, def, 0) == 0)
			return false;

		m_mPainted.Set(vehicle.GetID(), skinId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Doors, seats and other slotted parts share the hull's materials, so walk the hierarchy.
	//! \return number of entities repainted
	protected int PaintTree(IEntity ent, IA_HeliSkinDef def, int depth)
	{
		if (!ent || depth > MAX_DEPTH)
			return 0;
		if (ChimeraCharacter.Cast(ent))
			return 0;

		int painted = 0;
		if (PaintEntity(ent, def))
			painted = 1;

		IEntity child = ent.GetChildren();
		while (child)
		{
			painted = painted + PaintTree(child, def, depth + 1);
			child = child.GetSibling();
		}
		return painted;
	}

	//------------------------------------------------------------------------------------------------
	//! Remap source names must exist on the VObject, so build them from GetMaterials.
	protected bool PaintEntity(IEntity ent, IA_HeliSkinDef def)
	{
		VObject mesh = ent.GetVObject();
		if (!mesh)
			return false;

		string materials[256];
		int numMats = mesh.GetMaterials(materials);
		string remap = "";
		ResourceName material;
		int i;
		for (i = 0; i < numMats; i++)
		{
			material = def.FindMaterial(materials[i]);
			if (material.IsEmpty())
				continue;
			remap = remap + string.Format("$remap '%1' '%2';", materials[i], material);
		}

		if (remap.IsEmpty())
			return false;

		ent.SetObject(mesh, remap);
		return true;
	}
}
