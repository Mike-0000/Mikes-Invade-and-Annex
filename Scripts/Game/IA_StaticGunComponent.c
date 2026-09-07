[ComponentEditorProps(category: "I&A/Emplacements", description: "Site-owned finite-ammunition static gun; no per-frame work")]
class IA_StaticGunComponentClass : ScriptComponentClass
{
}

class IA_StaticGunComponent : ScriptComponent
{
	protected IEntity m_Weapon;
	protected BaseWeaponManagerComponent m_Manager;
	protected TurretControllerComponent m_Controller;
	protected TurretCompartmentSlot m_Seat;
	protected bool m_bInitialized;
	protected int m_iSiteSerial;
	protected string m_sInitializationFailure;

	string GetInitializationFailure() { return m_sInitializationFailure; }
	protected bool InitializationFailed(string reason)
	{
		m_sInitializationFailure = reason;
		return false;
	}

	static IA_StaticGunComponent Find(IEntity entity)
	{
		if (!entity)
			return null;
		return IA_StaticGunComponent.Cast(entity.FindComponent(IA_StaticGunComponent));
	}

	static IA_StaticGunComponent FindInTree(IEntity entity)
	{
		if (!entity)
			return null;
		IA_StaticGunComponent gun = Find(entity);
		if (gun)
			return gun;
		IEntity child = entity.GetChildren();
		while (child)
		{
			gun = FindInTree(child);
			if (gun)
				return gun;
			child = child.GetSibling();
		}
		return null;
	}

	static IA_StaticGunComponent FindOnNearestParent(IEntity entity)
	{
		IEntity walk = entity;
		while (walk)
		{
			IA_StaticGunComponent gun = Find(walk);
			if (gun)
				return gun;
			walk = walk.GetParent();
		}
		return null;
	}

	TurretCompartmentSlot GetSeat() { return m_Seat; }
	IEntity GetWeapon() { return m_Weapon; }
	int GetSiteSerial() { return m_iSiteSerial; }
	TurretControllerComponent GetController() { return m_Controller; }

	// Initialization runs once, only during hidden construction, after EOnInit.
	// Stock spawns three reserves plus (beltSize - chambered rounds) in the gun.
	bool Initialize(int serial, IA_EmplacementProfile profile, Faction enemy)
	{
		if (!Replication.IsServer() || m_bInitialized || !profile)
			return InitializationFailed("authority_or_already_initialized");
		IEntity root = GetOwner();
		m_Manager = BaseWeaponManagerComponent.Cast(root.FindComponent(BaseWeaponManagerComponent));
		m_Controller = TurretControllerComponent.Cast(root.FindComponent(TurretControllerComponent));
		SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(root.FindComponent(SCR_AIVehicleUsageComponent));
		if (!m_Manager || !m_Controller || !usage)
			return InitializationFailed("manager_controller_usage");
		m_Seat = usage.GetTurretCompartmentSlot();
		if (!m_Seat || m_Seat.IsOccupied() || m_Seat.IsReserved())
			return InitializationFailed("seat_unavailable");
		WeaponSlotComponent weaponSlot = WeaponSlotComponent.Cast(root.FindComponent(WeaponSlotComponent));
		if (!weaponSlot)
			return InitializationFailed("weapon_slot");
		m_Weapon = weaponSlot.GetWeaponEntity();
		if (!m_Weapon || !root.FindComponent(RplComponent))
			return InitializationFailed("weapon_or_replication");
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(m_Weapon.FindComponent(BaseWeaponComponent));
		if (!weapon || !weapon.GetCurrentMuzzle() || !weapon.GetCurrentMagazine())
			return InitializationFailed("muzzle_or_loaded_belt");
		BaseMagazineComponent loaded = weapon.GetCurrentMagazine();
		ref array<BaseMagazineComponent> magazines = {};
		CollectMagazines(root, magazines);
		if (magazines.Count() != 4)
			return InitializationFailed("belt_count"); // omit, never add extra belts
		foreach (BaseMagazineComponent magazine : magazines)
		{
			if (magazine.GetOwner().GetPrefabData().GetPrefabName() != profile.m_Magazine || magazine.GetMaxAmmoCount() != profile.m_iBeltSize)
				return InitializationFailed("belt_type");
		}
		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		int chambers = ChamberCount(muzzle);
		foreach (BaseMagazineComponent magazine : magazines)
		{
			int rounds = profile.m_iBeltSize;
			if (magazine == loaded)
				rounds -= chambers;
			magazine.SetAmmoCount(rounds);
		}
		// Lock only the mounted assembly, never magazines or the storage root.
		InventoryItemComponent item = InventoryItemComponent.Cast(m_Weapon.FindComponent(InventoryItemComponent));
		if (!item)
			return InitializationFailed("mounted_item_missing");
		if (!item.IsUserLocked())
			item.RequestUserLock(null, true);
		if (!item.IsUserLocked())
			return InitializationFailed("mounted_item_lock");
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(root.FindComponent(FactionAffiliationComponent));
		if (affiliation && enemy)
			affiliation.SetAffiliatedFaction(enemy);
		// The objective is session-owned, not an independently saved vehicle.
		PersistenceSystem persistence = PersistenceSystem.GetInstance();
		if (persistence && persistence.IsTracked(root))
			persistence.StopTracking(root);
		m_iSiteSerial = serial;
		if (GetUsableRounds() != profile.m_iBeltSize * 4)
			return InitializationFailed("normalized_ammunition");
		m_bInitialized = true;
		return true;
	}

	bool IsUsable()
	{
		if (!m_bInitialized || !m_Weapon || !GetOwner())
			return false;
		WeaponSlotComponent slot = WeaponSlotComponent.Cast(GetOwner().FindComponent(WeaponSlotComponent));
		if (!slot || slot.GetWeaponEntity() != m_Weapon)
			return false;
		DamageManagerComponent rootDamage = DamageManagerComponent.Cast(GetOwner().FindComponent(DamageManagerComponent));
		DamageManagerComponent gunDamage = DamageManagerComponent.Cast(m_Weapon.FindComponent(DamageManagerComponent));
		if (rootDamage && rootDamage.GetState() == EDamageState.DESTROYED)
			return false;
		return !gunDamage || gunDamage.GetState() != EDamageState.DESTROYED;
	}

	bool GetMuzzleTransform(out vector mat[4])
	{
		return m_Manager && m_Manager.GetCurrentMuzzleTransform(mat);
	}

	int GetLoadedRounds()
	{
		if (!m_Weapon)
			return 0;
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(m_Weapon.FindComponent(BaseWeaponComponent));
		if (!weapon || !weapon.GetCurrentMuzzle())
			return 0;
		int rounds = ChamberCount(weapon.GetCurrentMuzzle());
		BaseMagazineComponent mag = weapon.GetCurrentMagazine();
		if (mag)
			rounds += mag.GetAmmoCount();
		return rounds;
	}

	int GetUsableRounds()
	{
		return CountUsableRounds(GetOwner());
	}

	static int CountUsableRounds(IEntity root)
	{
		if (!root)
			return 0;
		WeaponSlotComponent slot = WeaponSlotComponent.Cast(root.FindComponent(WeaponSlotComponent));
		if (!slot || !slot.GetWeaponEntity())
			return 0;
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(slot.GetWeaponEntity().FindComponent(BaseWeaponComponent));
		if (!weapon || !weapon.GetCurrentMuzzle())
			return 0;
		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		int rounds = ChamberCount(muzzle);
		ref array<BaseMagazineComponent> magazines = {};
		CollectMagazines(root, magazines);
		foreach (BaseMagazineComponent mag : magazines)
		{
			if (mag.GetMagazineWell() && muzzle.GetMagazineWell() && mag.GetMagazineWell().Type() == muzzle.GetMagazineWell().Type())
				rounds += mag.GetAmmoCount();
		}
		return rounds;
	}

	static int ChamberCount(BaseMuzzleComponent muzzle)
	{
		int result;
		if (!muzzle)
			return 0;
		int barrels = muzzle.GetBarrelsCount();
		for (int i = 0; i < barrels; i++)
		{
			if (muzzle.IsBarrelChambered(i))
				result++;
		}
		return result;
	}

	static void CollectMagazines(IEntity root, notnull array<BaseMagazineComponent> result)
	{
		if (!root || ChimeraCharacter.Cast(root))
			return; // never count the occupant's carried ammo
		BaseMagazineComponent magazine = BaseMagazineComponent.Cast(root.FindComponent(BaseMagazineComponent));
		if (magazine && result.Find(magazine) < 0)
			result.Insert(magazine);
		IEntity child = root.GetChildren();
		while (child)
		{
			CollectMagazines(child, result);
			child = child.GetSibling();
		}
	}
}
