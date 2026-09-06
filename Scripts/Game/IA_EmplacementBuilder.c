// Resumable optional construction. One trace/spawn/verification per Step; no
// survey RNG, frame callback, repeated failed pose or whole-site trace exclusion.
class IA_EmplacementBuilder
{
	protected IA_DynamicSiteInstance m_Site;
	protected ref IA_DynamicBaseEmplacementSpec m_Spec;
	protected ref IA_EmplacementProfile m_Profile;
	protected ref IA_StaticGunRecord m_Record;
	protected ref array<bool> m_Sides = {false, false, false, false};
	protected int m_iCandidate;
	protected int m_iStage;
	protected int m_iSample;
	protected int m_iClearRays;
	protected int m_iSpawnMs;
	protected int m_iDeadlineMs;
	protected int m_iAttempts;
	protected int m_iWorkMs;
	protected bool m_bFallback;
	protected bool m_bDone;
	protected vector m_Mat[4];
	protected vector m_vMuzzle;
	protected float m_fMinY;
	protected float m_fMaxY;
	protected ref map<string, int> m_Reasons = new map<string, int>();

	void Begin(IA_DynamicSiteInstance site)
	{
		m_Site = site;
		m_iDeadlineMs = System.GetTickCount() + 6000;
		if (site)
			site.GetLayout().PrepareEmplacements();
	}

	bool IsDone() { return m_bDone; }

	void Finish()
	{
		if (m_bDone)
			return;
		if (m_Record)
			Reject("optional_deadline");
		m_bDone = true;
		if (IA_Log.IsDebugEnabled() && m_Site)
		{
			string reasons;
			foreach (string reason, int count : m_Reasons)
			{
				reasons = reasons + reason + "=" + count.ToString() + " ";
			}
			m_Site.SetEmplacementBuildSummary(string.Format("attempted=%1 workMs=%2 omissions=[%3]", m_iAttempts, m_iWorkMs, reasons));
		}
	}

	void Step()
	{
		if (m_bDone || !m_Site || !Replication.IsServer())
			return;
		int started = System.GetTickCount();
		if (started >= m_iDeadlineMs)
		{
			Finish();
			return;
		}
		StepInternal();
		m_iWorkMs += System.GetTickCount() - started;
	}

	protected void StepInternal()
	{
		IA_DynamicSiteLayout layout = m_Site.GetLayout();
		if (m_iStage == 0)
		{
			if (m_Site.GetEmplacements().Count() >= IA_EmplacementProfile.GunCap(layout.m_iLayoutId) || m_iCandidate >= layout.m_aEmplacements.Count())
			{
				Finish();
				return;
			}
			m_Spec = layout.m_aEmplacements[m_iCandidate];
			if (m_Sides[m_Spec.m_iSide] || !m_Site.GetPanel(m_Spec.m_sPanelId))
			{
				Reject("side_or_panel");
				return;
			}
			m_iAttempts++;
			m_Profile = IA_EmplacementProfile.Create(m_Spec.m_bHeavy && !m_bFallback);
			vector rootMat[4];
			layout.BuildRootTransform(m_Site.GetOrigin(), m_Site.GetYawDeg(), rootMat);
			layout.LocalToWorld(rootMat, m_Spec.m_vLocalPosition, m_Spec.m_fYaw, 0, m_Mat);
			foreach (IA_StaticGunRecord installed : m_Site.GetEmplacements())
			{
				if (vector.Distance(installed.m_vOrigin, m_Mat[3]) < 12)
				{
					Reject("separation");
					return;
				}
			}
			m_iStage = 1;
			m_iSample = 0;
			m_fMinY = 100000;
			m_fMaxY = -100000;
			return;
		}
		if (m_iStage == 1)
		{
			vector foot = m_Profile.m_aFeet[m_iSample];
			vector world = WorldPoint(foot);
			float y;
			if (!Support(world, y))
			{
				Reject("support");
				return;
			}
			y -= foot[1];
			m_fMinY = Math.Min(m_fMinY, y);
			m_fMaxY = Math.Max(m_fMaxY, y);
			m_iSample++;
			if (m_iSample == 3)
			{
				// Upright only. 3 cm residual bearing tolerance is stricter than
				// the 15 cm terrain-span cap; never lift to clear a wall.
				if (m_fMaxY - m_fMinY > 0.03)
				{
					Reject("uneven_feet");
					return;
				}
				vector origin = m_Mat[3];
				origin[1] = (m_fMaxY + m_fMinY) * 0.5;
				m_Mat[3] = origin;
				m_iStage = 2;
				m_iSample = 0;
			}
			return;
		}
		if (m_iStage == 2)
		{
			// Check both the operator and the inward end of the access strip.
			vector point = m_Profile.m_vSeat;
			if (m_iSample == 1)
				point = "0 0 -3.5";
			vector pos = WorldPoint(point);
			float y;
			if (!Support(pos, y) || Math.AbsFloat(y - m_Mat[3][1]) > 0.15)
			{
				Reject("access_support");
				return;
			}
			pos[1] = y;
			if (!IA_SpawnPlacement.IsOutdoorStandPose(pos))
			{
				Reject("standing");
				return;
			}
			m_iSample++;
			if (m_iSample == 2)
			{
				m_iStage = 3;
				m_iSample = 0;
			}
			return;
		}
		if (m_iStage == 3)
		{
			// Conservative swept solid bounds, then a separate standing/access
			// body. Boxes are expanded to world AABBs (never under-estimated).
			bool clear;
			if (m_iSample == 0)
				clear = BoxClear("-1.3 0.035 -1.3", Vector(1.3, m_Profile.m_vMaxs[1] + 0.25, 1.25));
			else
				clear = BoxClear("-0.6 0.2 -4", "0.6 1.85 -0.8");
			if (!clear)
			{
				Reject("sweep_or_access");
				return;
			}
			m_iSample++;
			if (m_iSample == 2)
				m_iStage = 4;
			return;
		}
		if (m_iStage == 4)
		{
			// Reserve all remaining dressing's declared cost. Guns cannot consume
			// structural headroom or force a smaller base later in construction.
			int remainingRoots;
			int remainingEntities;
			foreach (IA_DynamicSiteModule mod : layout.m_aModules)
			{
				if (mod.m_iRole == IA_DynamicSiteModuleRole.Dressing)
				{
					remainingRoots++;
					remainingEntities += mod.m_iEstimatedExpandedEntities;
				}
			}
			if (m_Site.GetRootCount() + remainingRoots + 1 > IA_DynamicSitePlacer.MAX_ROOTS || m_Site.CountExpandedEntities() + remainingEntities + m_Profile.m_iExpanded > IA_DynamicSitePlacer.MAX_EXPANDED)
			{
				Reject("budget");
				return;
			}
			Resource resource = Resource.Load(m_Profile.m_Prefab);
			if (!resource || !resource.IsValid())
			{
				Reject("resource");
				return;
			}
			ref EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			Math3D.MatrixCopy(m_Mat, params.Transform);
			IEntity root = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
			if (!root)
			{
				Reject("spawn");
				return;
			}
			m_Record = new IA_StaticGunRecord();
			m_Record.m_Root = root;
			m_Record.m_Gun = IA_StaticGunComponent.Find(root);
			m_Record.m_Spec = m_Spec;
			m_Record.m_Profile = m_Profile;
			m_Record.m_vOrigin = m_Mat[3];
			m_Record.m_fYaw = m_Site.GetYawDeg() + m_Spec.m_fYaw;
			m_Site.AddEmplacement(m_Record);
			m_iSpawnMs = System.GetTickCount();
			m_iStage = 5;
			return;
		}
		if (m_iStage == 5)
		{
			if (System.GetTickCount() - m_iSpawnMs < 200)
				return;
			if (!m_Record.m_Gun || !m_Record.m_Gun.Initialize(m_Site.GetSerial(), m_Profile, m_Site.GetEnemyFaction()) || CountHardware(m_Record.m_Root) > m_Profile.m_iExpanded)
			{
				string reason = "initialization_missing_component";
			if (m_Record.m_Gun)
				reason = "initialization_" + m_Record.m_Gun.GetInitializationFailure();
			Reject(reason);
				return;
			}
			TurretComponent turret = TurretComponent.Cast(m_Record.m_Root.FindComponent(TurretComponent));
			vector horizontal, vertical;
			if (!turret)
			{
				Reject("turret");
				return;
			}
			turret.GetAimingLimits(horizontal, vertical);
			if (turret.HasMoveableBase() || horizontal[0] < -30.01 || horizontal[1] > 30.01 || vertical[0] < -5.01 || vertical[1] > 20.01)
			{
				Reject("limits");
				return;
			}
			vector muzzle[4];
			if (!m_Record.m_Gun.GetMuzzleTransform(muzzle))
			{
				Reject("muzzle");
				return;
			}
			vector delta = muzzle[3] - m_Mat[3];
			m_vMuzzle = Vector(vector.Dot(delta, m_Mat[0]), vector.Dot(delta, m_Mat[1]), vector.Dot(delta, m_Mat[2]));
			m_iSample = 0;
			m_iClearRays = 0;
			m_iStage = 6;
			return;
		}
		if (m_iStage == 6)
		{
			// Fifteen barrel + first-three-metre traces, plus five level FoF
			// traces. No entire-base exclusion, including the supporting panel.
			float yaw;
			float elevation;
			float distance = 3;
			if (m_iSample < 15)
			{
				int column = m_iSample % 5;
				yaw = -30 + column * 15;
				int row = m_iSample / 5;
				if (row == 0)
					elevation = -5;
				else if (row == 2)
					elevation = 20;
			}
			else
			{
				yaw = -30 + (m_iSample - 15) * 15;
				distance = 25;
				if (m_iSample == 17)
					distance = 50;
			}
			bool clear = FiringRay(yaw, elevation, distance, m_iSample < 15);
			if (!clear && (m_iSample < 15 || m_iSample == 17))
			{
				Reject("firing_arc");
				return;
			}
			if (m_iSample >= 15 && clear)
				m_iClearRays++;
			m_iSample++;
			if (m_iSample == 20)
			{
				if (m_iClearRays < 3)
				{
					Reject("field_of_fire");
					return;
				}
				m_Sides[m_Spec.m_iSide] = true;
				m_Record = null;
				NextCandidate();
			}
		}
	}

	protected vector WorldPoint(vector local)
	{
		return m_Mat[3] + m_Mat[0] * local[0] + m_Mat[1] * local[1] + m_Mat[2] * local[2];
	}

	protected bool Support(vector point, out float y)
	{
		BaseWorld world = GetGame().GetWorld();
		float terrain = world.GetSurfaceY(point[0], point[2]);
		ref TraceParam trace = new TraceParam();
		trace.Start = Vector(point[0], terrain + 4, point[2]);
		trace.End = Vector(point[0], terrain - 1, point[2]);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		float hit = world.TraceMove(trace, PermanentObstacle);
		if (hit >= 1 || trace.TraceNorm[1] < 0.9961947)
			return false; // cos(5 degrees)
		y = trace.Start[1] + (trace.End[1] - trace.Start[1]) * hit;
		return y > world.GetOceanHeight(point[0], point[2]);
	}

	protected bool PermanentObstacle(IEntity entity)
	{
		return !ChimeraCharacter.Cast(entity);
	}

	protected bool BoxClear(vector low, vector high)
	{
		vector mins = "100000 100000 100000";
		vector maxs = "-100000 -100000 -100000";
		for (int i = 0; i < 8; i++)
		{
			vector corner = low;
			for (int axis = 0; axis < 3; axis++)
			{
				if (i & (1 << axis))
					corner[axis] = high[axis];
			}
			vector p = WorldPoint(corner) - m_Mat[3];
			for (int axis = 0; axis < 3; axis++)
			{
				mins[axis] = Math.Min(mins[axis], p[axis]);
				maxs[axis] = Math.Max(maxs[axis], p[axis]);
			}
		}
		ref TraceBox trace = new TraceBox();
		trace.Start = m_Mat[3];
		trace.End = trace.Start + "0 0.001 0";
		trace.Mins = mins;
		trace.Maxs = maxs;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		return GetGame().GetWorld().TraceMove(trace, PermanentObstacle) >= 1;
	}

	protected bool FiringRay(float yaw, float elevation, float distance, bool barrel)
	{
		float y = yaw * Math.DEG2RAD;
		float e = elevation * Math.DEG2RAD;
		vector direction = Vector(Math.Sin(y) * Math.Cos(e), Math.Sin(e), Math.Cos(y) * Math.Cos(e));
		vector pivot = m_Profile.m_vPitchPivot;
		vector offset = m_vMuzzle - pivot;
		// Pitch then yaw around the measured joint; do not move the live gun.
		vector pitched = Vector(offset[0], offset[1] * Math.Cos(e) + offset[2] * Math.Sin(e), offset[2] * Math.Cos(e) - offset[1] * Math.Sin(e));
		vector muzzle = pivot + Vector(pitched[0] * Math.Cos(y) + pitched[2] * Math.Sin(y), pitched[1], pitched[2] * Math.Cos(y) - pitched[0] * Math.Sin(y));
		vector start = WorldPoint(muzzle);
		vector end = start + m_Mat[0] * direction[0] * distance + m_Mat[1] * direction[1] * distance + m_Mat[2] * direction[2] * distance;
		ref array<IEntity> exclusions = {};
		CollectTree(m_Record.m_Root, exclusions);
		ref TraceSphere trace = new TraceSphere();
		trace.Start = start;
		if (barrel)
			trace.Start = WorldPoint(pivot);
		trace.End = end;
		trace.Radius = 0.04;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.ExcludeArray = exclusions;
		return GetGame().GetWorld().TraceMove(trace, PermanentObstacle) >= 1;
	}

	protected void Reject(string reason)
	{
		int count = m_Reasons.Get(reason);
		m_Reasons.Set(reason, count + 1);
		if (m_Record)
		{
			if (!m_Site.RemoveLastEmplacement())
			{
				// Player approached a partially constructed gun: leave ownership
				// intact for proximity-safe cancellation, and stop optional work.
				m_bDone = true;
				return;
			}
			m_Record = null;
		}
		if (m_Spec && m_Spec.m_bHeavy && !m_bFallback && reason != "side_or_panel")
		{
			m_bFallback = true;
			m_iStage = 0;
			return;
		}
		NextCandidate();
	}

	protected void NextCandidate()
	{
		m_iCandidate++;
		m_iStage = 0;
		m_bFallback = false;
		m_Spec = null;
		m_Profile = null;
	}

	static int CountHardware(IEntity root)
	{
		if (!root || ChimeraCharacter.Cast(root))
			return 0;
		int total = 1;
		IEntity child = root.GetChildren();
		while (child)
		{
			total += CountHardware(child);
			child = child.GetSibling();
		}
		return total;
	}

	static void CollectTree(IEntity root, notnull array<IEntity> entities)
	{
		if (!root)
			return;
		entities.Insert(root);
		IEntity child = root.GetChildren();
		while (child)
		{
			CollectTree(child, entities);
			child = child.GetSibling();
		}
	}
}
