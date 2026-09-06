#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA emplacement asset measurement", wbModules: {"ResourceManager"})]
class IA_BaseEmplacementProbe : IA_BaseFoundationProbe
{
	override void RunCommandline()
	{
		ref SharedItemRef preview = BaseWorld.CreateWorld("Preview", "IAEmplacementProbe");
		BaseWorld world = preview.GetRef();
		vector stockHorizontal, stockVertical;
		ResourceName stockScoped = "{3EA26CFD7A202636}Prefabs/Weapons/Tripods/6T7/Tripod_6T7_NSV_SPP.et";
		array<ResourceName> resources = {
			stockScoped,
			"{723870DBB19D30B0}Prefabs/Weapons/Tripods/Tripod_6T5_PKM.et",
			"{29F0CC704A582154}Prefabs/Weapons/Tripods/6T7/Tripod_6T7_NSV.et",
			IA_EmplacementProfile.PKM,
			IA_EmplacementProfile.NSV,
			IA_EmplacementProfile.NSV_SPP,
			IA_EmplacementProfile.AA,
			IA_DynamicSiteLayout.PREFAB_COVER,
			IA_DynamicSiteLayout.PREFAB_COVER_WINDOW,
			IA_DynamicSiteLayout.PREFAB_COVER_HIGH,
			IA_DynamicSiteLayout.PREFAB_COVER_ROUND
		};
		foreach (ResourceName name : resources)
		{
			ref EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			Math3D.MatrixIdentity4(params.Transform);
			IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(name), world, params);
			Check(entity != null, "spawn " + name);
			if (!entity)
				continue;
			vector low = "99999 99999 99999";
			vector high = "-99999 -99999 -99999";
			MeasureHierarchy(entity, low, high);
			Print(string.Format("[IA][EmplacementMeasure] %1 low=%2 high=%3", name, low, high), LogLevel.NORMAL);
			Dump(entity);
			if (name == stockScoped)
			{
				TurretComponent stockTurret = TurretComponent.Cast(entity.FindComponent(TurretComponent));
				stockTurret.GetAimingLimits(stockHorizontal, stockVertical);
			}
			IA_StaticGunComponent ownGun = IA_StaticGunComponent.Find(entity);
			if (ownGun)
			{
				TurretComponent turret = TurretComponent.Cast(entity.FindComponent(TurretComponent));
				vector horizontal, vertical;
				turret.GetAimingLimits(horizontal, vertical);
				int kind = 0;
				if (name == IA_EmplacementProfile.NSV)
					kind = 1;
				else if (name == IA_EmplacementProfile.NSV_SPP)
					kind = 2;
				else if (name == IA_EmplacementProfile.AA)
					kind = 3;
				if (kind == 3)
					Check(vector.Distance(horizontal, stockHorizontal) < 0.01 && vector.Distance(vertical, stockVertical) < 0.01, "AA retains stock mechanical limits");
				else
					Check(Math.AbsFloat(horizontal[0] + 30) < 0.01 && Math.AbsFloat(horizontal[1] - 30) < 0.01 && Math.AbsFloat(vertical[0] + 5) < 0.01 && Math.AbsFloat(vertical[1] - 20) < 0.01, "IA mechanical limits");
				ref array<BaseMagazineComponent> magazines = {};
				IA_StaticGunComponent.CollectMagazines(entity, magazines);
				Check(magazines.Count() == 4, "one loaded and three reserve magazines");
				int total;
				foreach (BaseMagazineComponent magazine : magazines)
				{
					total += magazine.GetAmmoCount();
				}
				WeaponSlotComponent slot = WeaponSlotComponent.Cast(entity.FindComponent(WeaponSlotComponent));
				BaseWeaponComponent weapon = BaseWeaponComponent.Cast(slot.GetWeaponEntity().FindComponent(BaseWeaponComponent));
				total += IA_StaticGunComponent.ChamberCount(weapon.GetCurrentMuzzle());
				int expected = 400;
				if (kind > 0)
					expected = 200;
				ref IA_EmplacementProfile profile = IA_EmplacementProfile.CreateKind(kind);
				Check(IA_EmplacementBuilder.CountHardware(entity) <= profile.m_iExpanded, "profile hardware allowance");
				Check(total == expected, "exact ammunition including chamber");
				Check(IA_StaticGunComponent.CountUsableRounds(entity) == expected, "runtime usable-ammunition counter");
				SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(entity.FindComponent(SCR_AIVehicleUsageComponent));
				if (Replication.IsServer() && usage && usage.GetTurretCompartmentSlot())
				{
					WeaponSlotComponent mount = WeaponSlotComponent.Cast(entity.FindComponent(WeaponSlotComponent));
					InventoryItemComponent item = InventoryItemComponent.Cast(mount.GetWeaponEntity().FindComponent(InventoryItemComponent));
					RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
					if (item)
						Print(string.Format("[IA][EmplacementMeasure] lock BEFORE user=%1 system=%2 authority=%3", item.IsUserLocked(), item.IsSystemLocked(), rpl.IsMaster()), LogLevel.NORMAL);
					else
						Print("[IA][EmplacementMeasure] no mounted inventory item", LogLevel.NORMAL);
					bool initialized = ownGun.Initialize(1, profile, null);
					if (item)
						Print(string.Format("[IA][EmplacementMeasure] lock AFTER user=%1 system=%2", item.IsUserLocked(), item.IsSystemLocked()), LogLevel.NORMAL);
					Check(initialized, "native inventory lock/initialization: " + ownGun.GetInitializationFailure());
					Check(ownGun.GetUsableRounds() == expected, "initialization preserves exact ammunition");
					DamageManagerComponent damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
					if (damage && damage.GetMaxHealth() > 0)
						Check(ownGun.IsUsable(), "initialized gun remains usable");
					else
						Print("[IA][EmplacementMeasure] destruction/usability skipped: Preview damage system is uninitialized (stock also reports 0/0)", LogLevel.NORMAL);
				}
				else
					Print("[IA][EmplacementMeasure] live initialization skipped: preview has no authority/initialized seat", LogLevel.NORMAL);
				Print(string.Format("[IA][EmplacementMeasure] IA hardware=%1 totalRounds=%2", IA_EmplacementBuilder.CountHardware(entity), total), LogLevel.NORMAL);
				foreach (BaseMagazineComponent magazine : magazines)
				{
					InventoryItemComponent magItem = InventoryItemComponent.Cast(magazine.GetOwner().FindComponent(InventoryItemComponent));
					Check(magItem && !magItem.IsUserLocked(), "magazine lock untouched");
					magazine.SetAmmoCount(0);
				}
				BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
				Check(ownGun.GetUsableRounds() == IA_StaticGunComponent.ChamberCount(muzzle), "empty belts retain only chambered ammunition");
				int barrels = muzzle.GetBarrelsCount();
				for (int barrel = 0; barrel < barrels; barrel++)
				{
					muzzle.ClearChamber(barrel);
				}
				Check(ownGun.GetUsableRounds() == 0, "finite ammunition reaches true zero");
			}
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
		Print(string.Format("[IA][EmplacementMeasure] failures=%1 (preview geometry only; live physics/AI/RPL not tested)", m_iFailures), LogLevel.NORMAL);
		Workbench.Exit(m_iFailures);
	}

	protected void Dump(IEntity entity)
	{
		vector mat[4];
		entity.GetWorldTransform(mat);
		Print(string.Format("[IA][EmplacementMeasure] entity=%1 origin=%2", entity.GetPrefabData().GetPrefabName(), mat[3]), LogLevel.NORMAL);
		DamageManagerComponent damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
		if (damage)
			Print(string.Format("[IA][EmplacementMeasure] damage state=%1 health=%2/%3", damage.GetState(), damage.GetHealth(), damage.GetMaxHealth()), LogLevel.NORMAL);
		BaseMagazineComponent magazine = BaseMagazineComponent.Cast(entity.FindComponent(BaseMagazineComponent));
		if (magazine)
			Print(string.Format("[IA][EmplacementMeasure] magazine=%1/%2", magazine.GetAmmoCount(), magazine.GetMaxAmmoCount()), LogLevel.NORMAL);
		TurretComponent turret = TurretComponent.Cast(entity.FindComponent(TurretComponent));
		if (turret)
		{
			vector horizontal, vertical;
			turret.GetAimingLimits(horizontal, vertical);
			Print(string.Format("[IA][EmplacementMeasure] limits=%1/%2 movable=%3", horizontal, vertical, turret.HasMoveableBase()), LogLevel.NORMAL);
		}
		array<string> bones = {"w_H_Turning_axis_tripod", "w_V_Turning_axis_tripod", "w_weapon_holder_tripod", "wtp_yaw", "wtp_pitch", "slot_gun", "barrel_muzzle", "slot_barrel_muzzle"};
		foreach (string bone : bones)
		{
			if (!entity.GetAnimation() || entity.GetAnimation().GetBoneIndex(bone) < 0)
				continue;
			ref PointInfo point = new PointInfo();
			vector identity[4];
			Math3D.MatrixIdentity4(identity);
			point.Set(entity, bone, identity);
			if (point.GetNodeId() < 0)
				continue;
			point.GetWorldTransform(mat);
			Print(string.Format("[IA][EmplacementMeasure] bone=%1 world=%2", bone, mat[3]), LogLevel.NORMAL);
		}
		BaseWeaponManagerComponent manager = BaseWeaponManagerComponent.Cast(entity.FindComponent(BaseWeaponManagerComponent));
		if (manager && manager.GetCurrentMuzzleTransform(mat))
			Print(string.Format("[IA][EmplacementMeasure] muzzleWorld=%1", mat[3]), LogLevel.NORMAL);
		if (entity.GetAnimation() && turret)
		{
			array<string> names = {};
			entity.GetAnimation().GetBoneNames(names);
			foreach (string name : names)
			{
				entity.GetAnimation().GetBoneMatrix(entity.GetAnimation().GetBoneIndex(name), mat);
				Print(string.Format("[IA][EmplacementMeasure] tripodBone=%1 model=%2", name, mat[3]), LogLevel.NORMAL);
			}
		}
		IEntity child = entity.GetChildren();
		while (child)
		{
			Dump(child);
			child = child.GetSibling();
		}
	}
}
#endif
