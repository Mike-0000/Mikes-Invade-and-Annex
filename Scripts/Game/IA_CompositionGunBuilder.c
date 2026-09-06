// Installs optional guns at the vanilla composition's authored sockets. Scenery
// owns its terrain support; a rejected gun never removes its bunker/checkpoint.
class IA_CompositionGunBuilder : IA_EmplacementBuilder
{
	protected int m_iHeavyInstalled;
	protected IEntity m_Assembly;
	protected vector m_MuzzleMat[4];

	override void Step()
	{
		if (m_bDone || !m_Site || !Replication.IsServer())
			return;
		int start = System.GetTickCount();
		if (start >= m_iDeadlineMs)
		{
			Omit("optional_deadline");
			Finish();
			return;
		}
		StepSocket();
		m_iWorkMs += System.GetTickCount() - start;
	}

	protected void StepSocket()
	{
		IA_DynamicSiteLayout layout = m_Site.GetLayout();
		if (m_iStage == 0)
		{
			if (m_iCandidate >= layout.m_aEmplacements.Count() || m_Site.GetEmplacements().Count() >= IA_EmplacementProfile.GunCap(layout.m_iLayoutId))
			{
				Finish();
				return;
			}
			m_Spec = layout.m_aEmplacements[m_iCandidate];
			m_iAttempts++;
			m_Assembly = m_Site.GetPanel(m_Spec.m_sPanelId);
			if (!m_Assembly || !m_Spec.m_Socket)
			{
				Omit("missing_assembly");
				return;
			}
			int kind = m_Spec.m_Socket.m_iKind;
			if (kind > 0 && (layout.m_iLayoutId > 1 || m_iHeavyInstalled >= 1))
			{
				Omit("heavy_cap");
				return;
			}
			m_Profile = IA_EmplacementProfile.CreateKind(kind);
			if (m_Site.GetRootCount() + 1 > IA_DynamicSitePlacer.MAX_ROOTS || m_Site.CountExpandedEntities() + m_Profile.m_iExpanded > IA_DynamicSitePlacer.MAX_EXPANDED)
			{
				Omit("budget");
				return;
			}
			vector parent[4];
			m_Assembly.GetWorldTransform(parent);
			m_Spec.m_Socket.Transform(parent, m_Mat);
			Resource resource = Resource.Load(m_Profile.m_Prefab);
			if (!resource || !resource.IsValid())
			{
				Omit("resource");
				return;
			}
			ref EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			Math3D.MatrixCopy(m_Mat, params.Transform);
			IEntity gun = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
			if (!gun)
			{
				Omit("spawn");
				return;
			}
			m_Record = new IA_StaticGunRecord();
			m_Record.m_Root = gun;
			m_Record.m_Gun = IA_StaticGunComponent.Find(gun);
			m_Record.m_Spec = m_Spec;
			m_Record.m_Profile = m_Profile;
			m_Record.m_vOrigin = m_Mat[3];
			vector angles = Math3D.MatrixToAngles(m_Mat);
			m_Record.m_fYaw = angles[0];
			m_Site.AddEmplacement(m_Record);
			m_iSpawnMs = System.GetTickCount();
			m_iStage = 1;
			return;
		}
		if (m_iStage == 1)
		{
			if (System.GetTickCount() - m_iSpawnMs < 200)
				return;
			if (!m_Record.m_Gun)
			{
				Omit("missing_marker");
				return;
			}
			if (!m_Record.m_Gun.Initialize(m_Site.GetSerial(), m_Profile, m_Site.GetEnemyFaction()))
			{
				Omit("initialization_" + m_Record.m_Gun.GetInitializationFailure());
				return;
			}
			if (CountHardware(m_Record.m_Root) > m_Profile.m_iExpanded || !m_Record.m_Gun.GetMuzzleTransform(m_MuzzleMat))
			{
				Omit("hardware_or_muzzle");
				return;
			}
			m_iStage = 2;
			m_iSample = 0;
			return;
		}
		if (m_iStage == 2)
		{
			IA_DynamicSiteModule module;
			foreach (IA_DynamicSiteModule candidate : layout.m_aModules)
			{
				if (candidate.m_sId == m_Spec.m_sPanelId)
					module = candidate;
			}
			if (!module || !m_Assembly)
			{
				Omit("missing_assembly");
				return;
			}
			vector parent[4];
			m_Assembly.GetWorldTransform(parent);
			vector local = Vector(0, 0, -module.m_fHalfDepthM - 1.5);
			if (m_iSample == 1)
				local[0] = -2;
			else if (m_iSample == 2)
				local[0] = 2;
			else if (m_iSample == 3)
				local = Vector(-module.m_fHalfWidthM - 1.5, 0, 0);
			else if (m_iSample == 4)
				local = Vector(module.m_fHalfWidthM + 1.5, 0, 0);
			vector access = parent[3] + parent[0]*local[0] + parent[2]*local[2];
			access[1] = GetGame().GetWorld().GetSurfaceY(access[0], access[2]) + 0.05;
			if (StandingClear(access))
			{
				m_Record.m_bAuthoredAccess = true;
				m_Record.m_vAccess = access;
				m_iStage = 3;
				m_iSample = 0;
				m_iClearRays = 0;
				return;
			}
			m_iSample++;
			if (m_iSample >= 5)
			{
				// Wall-line sockets can fail the rear samples on berms. Keep
				// the vanilla gun; crew access is best-effort behind the bags.
				if (m_Assembly)
				{
					vector parent[4];
					m_Assembly.GetWorldTransform(parent);
					vector access = parent[3] - parent[2] * 3.5;
					access[1] = GetGame().GetWorld().GetSurfaceY(access[0], access[2]) + 0.05;
					m_Record.m_bAuthoredAccess = true;
					m_Record.m_vAccess = access;
				}
				Keep("access_blocked");
			}
			return;
		}
		// Screen useful firing lanes, not every mechanically possible shot.
		// Vanilla cover can legitimately obstruct low/side angles. Full sweeps,
		// player collision and AA acquisition remain explicit live acceptance tests.
		float yaw = (-30 + m_iSample * 15) * Math.DEG2RAD;
		float elevation;
		float distance = 25;
		if (m_iSample == 2)
			distance = 50;
		if (m_Profile.m_iKind == 3)
		{
			// Stock NSV-SPP controller elevation stops at 25 degrees relative
			// to its authored tilted base; do not test an unreachable 45.
			elevation = 20 * Math.DEG2RAD;
			distance = 100;
		}
		vector direction = m_Mat[0]*(Math.Sin(yaw)*Math.Cos(elevation)) + m_Mat[1]*Math.Sin(elevation) + m_Mat[2]*(Math.Cos(yaw)*Math.Cos(elevation));
		ref TraceSphere trace = new TraceSphere();
		trace.Start = m_MuzzleMat[3];
		trace.End = trace.Start + direction*distance;
		trace.Radius = 0.04;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		ref array<IEntity> exclusions = {};
		CollectTree(m_Record.m_Root, exclusions);
		trace.ExcludeArray = exclusions; // never exclude the sandbags or the base
		bool clear = GetGame().GetWorld().TraceMove(trace, PermanentObstacle) >= 1;
		if (!clear && m_iSample == 2)
		{
			Keep("center_firing_lane");
			return;
		}
		if (clear)
			m_iClearRays++;
		m_iSample++;
		if (m_iSample == 5)
		{
			if (m_iClearRays < 3)
			{
				Keep("field_of_fire");
				return;
			}
			Keep("");
		}
	}

	static bool StandingClear(vector position)
	{
		ref TraceSphere body = new TraceSphere();
		body.Start = position + "0 0.35 0";
		body.End = position + "0 1.5 0";
		body.Radius = 0.3;
		body.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		return GetGame().GetWorld().TraceMove(body, null) >= 1;
	}

	protected void Keep(string reason)
	{
		if (reason)
			m_Reasons.Set(reason, m_Reasons.Get(reason) + 1);
		if (m_Profile && m_Profile.m_iKind > 0)
			m_iHeavyInstalled++;
		m_Record = null;
		m_Assembly = null;
		NextCandidate();
	}

	protected void Omit(string reason)
	{
		m_Reasons.Set(reason, m_Reasons.Get(reason) + 1);
		if (m_Record && !m_Site.RemoveLastEmplacement())
		{
			m_bDone = true; // retain player-protected ownership for cancellation
			return;
		}
		m_Record = null;
		m_Assembly = null;
		NextCandidate();
	}
}
