//------------------------------------------------------------------------------------------------
//! Clock-driven Enhanced Defense: PREPARE, named doctrine, optional mini-objectives.
//! Selected from IA_DefendMission when legacy is off. Arrival never blocks the clock.
//------------------------------------------------------------------------------------------------
enum IA_DefendDoctrine
{
	Siege,
	Breakthrough,
	AirAssault,
	CommandOffensive
}

enum IA_DefendPhase
{
	Idle,
	Prepare,
	Probe,
	Assault,
	Crisis,
	Secure
}

class IA_EnhancedDefendDirector
{
	protected IA_DefendMission m_Mission;
	protected IA_DefendDoctrine m_eDoctrine;
	protected IA_DefendPhase m_ePhase;
	protected int m_iPrepareMs;
	protected int m_iPrepareStart;
	protected int m_iPrepareStartUnix;
	protected int m_iClockStart;
	protected bool m_bClockStarted;
	protected bool m_bFirstBeat;
	protected bool m_bAssaultBeat;
	protected bool m_bCrisisBeat;
	protected bool m_bCancelNextAssault;
	protected bool m_bHideInbound;
	protected bool m_bShrinkAssault;
	protected bool m_bMortarsStopped;
	protected int m_iLastMortarPulse;
	protected int m_iMortarFollowupAt;
	protected int m_iLastFillerMs;
	protected int m_iNextEventIndex;
	protected ref array<ref IA_DefendEvent> m_Events;
	protected ref array<int> m_EventStartOffsets;
	protected IA_Config m_Config;

	//------------------------------------------------------------------------------------------------
	void IA_EnhancedDefendDirector(IA_DefendMission mission)
	{
		m_Mission = mission;
		m_ePhase = IA_DefendPhase.Idle;
		m_Events = new array<ref IA_DefendEvent>();
		m_EventStartOffsets = new array<int>();
	}

	//------------------------------------------------------------------------------------------------
	static IA_EnhancedDefendDirector Create(notnull IA_DefendMission mission)
	{
		return new IA_EnhancedDefendDirector(mission);
	}

	//------------------------------------------------------------------------------------------------
	void Start()
	{
		if (m_ePhase != IA_DefendPhase.Idle)
			return;
		if (!m_Mission)
			return;

		m_Config = IA_MissionInitializer.GetGlobalConfig();
		if (m_Config)
			m_Config.ClampDefenseSettings();

		m_eDoctrine = RollDoctrine();
		ApplyDuration();
		ApplyPrepareWindow();
		BuildEventPlan();

		m_Mission.BeginEnhancedHold();
		m_ePhase = IA_DefendPhase.Prepare;
		m_iPrepareStart = System.GetTickCount();
		m_iPrepareStartUnix = System.GetUnixTime();
		m_bFirstBeat = false;

		string doctrineName = DoctrineName();
		string site = m_Mission.GetMarkerName();
		m_Mission.NotifyPlayers("DefendMissionStarted", doctrineName + " — defend " + site);
		m_Mission.NotifyPlayers("TaskCreated", "PREPARE: " + doctrineName);

		FireFirstBeat();
		m_bFirstBeat = true;
		Publish();
		GetGame().GetCallqueue().Remove(this.Update);
		GetGame().GetCallqueue().CallLater(this.Update, 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	void Update()
	{
		if (!m_Mission || !m_Mission.IsActive())
			return;

		int now = System.GetTickCount();
		if (!m_bClockStarted)
		{
			int prepareElapsedMs = (System.GetUnixTime() - m_iPrepareStartUnix) * 1000;
			bool timedOut = prepareElapsedMs >= m_iPrepareMs;
			if (m_Mission.HasPlayerContact() || timedOut)
				StartClock(now);
		}

		if (m_bClockStarted && m_Mission.GetRemainingMs() <= 0)
		{
			m_ePhase = IA_DefendPhase.Secure;
			Publish();
			m_Mission.EndDefendMission(true);
			return;
		}

		AdvancePhase();
		TickEvents(now);
		TickDoctrineBeats();
		TickFiller(now);
		Publish();
	}

	//------------------------------------------------------------------------------------------------
	void CleanupEvents()
	{
		int count = m_Events.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_DefendEvent ev = m_Events[i];
			if (ev)
				ev.Cleanup();
		}
		m_Events.Clear();
		m_iMortarFollowupAt = 0;
		GetGame().GetCallqueue().Remove(this.Update);
	}

	//------------------------------------------------------------------------------------------------
	vector GetDefendPoint()
	{
		if (!m_Mission)
			return vector.Zero;
		return m_Mission.GetDefendPoint();
	}

	IA_DefendPhase GetPhase() { return m_ePhase; }
	IA_DefendDoctrine GetDoctrine() { return m_eDoctrine; }
	bool HideInboundCues() { return m_bHideInbound; }

	//------------------------------------------------------------------------------------------------
	void Notify(string messageType, string title)
	{
		if (!m_Mission)
			return;
		m_Mission.NotifyPlayers(messageType, title);
	}

	//------------------------------------------------------------------------------------------------
	void SetHideInboundCues(bool hide)
	{
		m_bHideInbound = hide;
	}

	//------------------------------------------------------------------------------------------------
	void QueueMortarFollowup()
	{
		if (m_bMortarsStopped)
			return;
		if (m_iMortarFollowupAt != 0)
			return;
		m_iMortarFollowupAt = System.GetTickCount() + 120000;
	}

	//------------------------------------------------------------------------------------------------
	void PulseMortarBarrage()
	{
		if (m_bMortarsStopped)
			return;
		int now = System.GetTickCount();
		if (m_iLastMortarPulse != 0 && (now - m_iLastMortarPulse) < 90000)
			return;
		m_iLastMortarPulse = now;
		Notify("ReinforcementsCalled", "Enemy infantry inbound — they have your position.");
		m_Mission.SpawnDirectorWave(8, true);
	}

	//------------------------------------------------------------------------------------------------
	void RequestDoctrineBeat(IA_QRFType type)
	{
		m_Mission.SpawnDoctrineBeat(type);
	}

	//------------------------------------------------------------------------------------------------
	IA_AiGroup SpawnEventGroup(vector pos, int count, bool elite, bool hvt, bool hold)
	{
		return m_Mission.SpawnEventGroup(pos, count, elite, hvt, hold);
	}

	//------------------------------------------------------------------------------------------------
	IA_AiGroup SpawnEventConvoyVehicle(vector pos)
	{
		return m_Mission.SpawnEventConvoyVehicle(pos);
	}

	//------------------------------------------------------------------------------------------------
	void OnEventResolved(IA_DefendEvent ev, bool success, string note)
	{
		if (!ev)
			return;

		IA_DefendEventType t = ev.GetType();
		if (t == IA_DefendEventType.CommanderFob && success)
		{
			m_bCancelNextAssault = true;
			CancelOneLaterEvent(ev);
		}
		if (t == IA_DefendEventType.Convoy && success)
			m_bShrinkAssault = true;
		if (t == IA_DefendEventType.Relay && success)
			m_bHideInbound = false;
		if (t == IA_DefendEventType.MortarTeam && success)
			m_bMortarsStopped = true;
		if (ev.IsPriority() && success)
			ApplyPrioritySuccess();
	}

	//------------------------------------------------------------------------------------------------
	void OnPriorityTimeout(IA_DefendEvent ev)
	{
		ApplyPriorityFail();
	}

	//------------------------------------------------------------------------------------------------
	protected void StartClock(int now)
	{
		m_bClockStarted = true;
		m_iClockStart = now;
		m_Mission.StartEnhancedClock();
		m_ePhase = IA_DefendPhase.Probe;
		Notify("ReinforcementsCalled", "Contact — " + DoctrineName() + " clock started.");
	}

	//------------------------------------------------------------------------------------------------
	protected void AdvancePhase()
	{
		if (!m_bClockStarted)
		{
			m_ePhase = IA_DefendPhase.Prepare;
			return;
		}

		float u = m_Mission.GetElapsedClock01();
		if (u < 0.25)
			m_ePhase = IA_DefendPhase.Probe;
		else if (u < 0.65)
			m_ePhase = IA_DefendPhase.Assault;
		else
			m_ePhase = IA_DefendPhase.Crisis;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickDoctrineBeats()
	{
		if (m_ePhase == IA_DefendPhase.Assault && !m_bAssaultBeat)
		{
			m_bAssaultBeat = true;
			if (m_bCancelNextAssault)
			{
				m_bCancelNextAssault = false;
				Notify("ReinforcementsCalled", "Commander down — major assault cancelled.");
			}
			else
			{
				FireAssaultBeat();
			}
		}

		if (m_ePhase == IA_DefendPhase.Crisis && !m_bCrisisBeat)
		{
			m_bCrisisBeat = true;
			FireCrisisBeat();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TickFiller(int now)
	{
		if (!m_bClockStarted)
			return;
		if (m_ePhase == IA_DefendPhase.Secure)
			return;

		int interval = 22000;
		if (m_eDoctrine == IA_DefendDoctrine.Breakthrough)
			interval = 32000;
		if (m_ePhase == IA_DefendPhase.Crisis)
			interval = 16000;
		if (m_iLastFillerMs != 0 && (now - m_iLastFillerMs) < interval)
			return;

		int alive = m_Mission.CountWaveAI();
		int target = m_Mission.GetPhaseTargetAI(PhasePressure());
		if (alive >= target)
			return;

		int budget = 10;
		if (m_bShrinkAssault)
			budget = 6;
		if (m_eDoctrine == IA_DefendDoctrine.Breakthrough)
			budget = 6;
		if (m_ePhase == IA_DefendPhase.Crisis)
			budget = budget + 4;

		m_iLastFillerMs = now;
		m_Mission.SpawnDirectorWave(budget, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void TickEvents(int now)
	{
		if (!m_bClockStarted && m_ePhase != IA_DefendPhase.Prepare)
			return;

		int elapsed;
		if (m_bClockStarted)
			elapsed = now - m_iClockStart;
		else
			elapsed = (System.GetUnixTime() - m_iPrepareStartUnix) * 1000;

		int planned = m_EventStartOffsets.Count();
		while (m_iNextEventIndex < planned && m_iNextEventIndex < m_Events.Count())
		{
			if (elapsed < m_EventStartOffsets[m_iNextEventIndex])
				break;
			IA_DefendEvent ev = m_Events[m_iNextEventIndex];
			m_iNextEventIndex = m_iNextEventIndex + 1;
			if (ev && ev.GetState() == IA_DefendEventState.Pending)
			{
				if (m_ePhase == IA_DefendPhase.Crisis || m_ePhase == IA_DefendPhase.Secure)
					continue;
				ev.Activate(this);
				if (m_iNextEventIndex == 1)
					PromotePriority(ev);
			}
		}

		if (m_iMortarFollowupAt != 0 && now >= m_iMortarFollowupAt)
		{
			m_iMortarFollowupAt = 0;
			if (!m_bMortarsStopped)
			{
				ref IA_DefendEvent mortar = IA_DefendEvent.Create(IA_DefendEventType.MortarTeam);
				m_Events.Insert(mortar);
				mortar.Activate(this);
			}
		}

		int count = m_Events.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_DefendEvent ev = m_Events[i];
			if (ev)
				ev.Update();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void FireFirstBeat()
	{
		if (m_eDoctrine == IA_DefendDoctrine.Breakthrough)
		{
			m_Mission.SpawnDirectorWave(8, true);
			m_Mission.SpawnDoctrineBeat(IA_QRFType.Motorized);
			return;
		}
		if (m_eDoctrine == IA_DefendDoctrine.AirAssault)
		{
			m_Mission.SpawnDirectorWave(8, true);
			bool hot = RollHotDrop();
			m_Mission.SpawnAirborneBeat(hot);
			return;
		}
		m_Mission.SpawnDirectorWave(10, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void FireAssaultBeat()
	{
		int budget = 14;
		if (m_bShrinkAssault)
			budget = 8;

		if (m_eDoctrine == IA_DefendDoctrine.Siege)
		{
			m_Mission.SpawnDirectorWave(budget, false);
			m_Mission.SpawnDoctrineBeat(IA_QRFType.Infantry);
			return;
		}
		if (m_eDoctrine == IA_DefendDoctrine.Breakthrough)
		{
			m_Mission.SpawnDoctrineBeat(IA_QRFType.Mechanized);
			m_Mission.SpawnDoctrineBeat(IA_QRFType.Armoured);
			return;
		}
		if (m_eDoctrine == IA_DefendDoctrine.AirAssault)
		{
			m_Mission.SpawnDirectorWave(budget, false);
			m_Mission.SpawnAirborneBeat(RollHotDrop());
			return;
		}

		m_Mission.SpawnDirectorWave(budget, false);
		m_Mission.SpawnDoctrineBeat(IA_QRFType.Mechanized);
	}

	//------------------------------------------------------------------------------------------------
	protected void FireCrisisBeat()
	{
		m_Mission.SpawnDirectorWave(12, false);
		if (m_eDoctrine == IA_DefendDoctrine.Breakthrough)
			m_Mission.SpawnDoctrineBeat(IA_QRFType.Armoured);
		else if (m_eDoctrine == IA_DefendDoctrine.AirAssault)
			m_Mission.SpawnAirborneBeat(RollHotDrop());
		else
			m_Mission.SpawnDoctrineBeat(IA_QRFType.Motorized);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildEventPlan()
	{
		m_Events.Clear();
		m_EventStartOffsets.Clear();
		m_iNextEventIndex = 0;

		ref array<int> pool = new array<int>();
		FillEventPool(pool);
		if (m_eDoctrine == IA_DefendDoctrine.CommandOffensive)
			PreferType(pool, IA_DefendEventType.CommanderFob);
		if (m_eDoctrine == IA_DefendDoctrine.Siege)
			PreferType(pool, IA_DefendEventType.ScoutMortar);

		int want = 2;
		if (m_Config)
			want = m_Config.m_iDefendGuaranteedEvents;
		if (want < 0)
			want = 0;
		if (RollThirdEvent())
			want = want + 1;
		if (want > 3)
			want = 3;
		if (want > pool.Count())
			want = pool.Count();

		bool preferFirst = false;
		if (m_eDoctrine == IA_DefendDoctrine.CommandOffensive)
			preferFirst = true;
		else if (m_eDoctrine == IA_DefendDoctrine.Siege)
			preferFirst = true;

		int i;
		for (i = 0; i < want; i++)
		{
			if (pool.IsEmpty())
				break;
			int pick = 0;
			bool randomPick = true;
			if (i == 0 && preferFirst)
				randomPick = false;
			if (randomPick && pool.Count() > 1)
				pick = Math.RandomInt(0, pool.Count());
			IA_DefendEventType t = pool[pick];
			pool.Remove(pick);
			m_Events.Insert(IA_DefendEvent.Create(t));
		}

		int durationMs = 18 * 60000;
		if (m_Mission)
			durationMs = m_Mission.GetDurationMs();
		if (durationMs < 60000)
			durationMs = 60000;

		int planned = m_Events.Count();
		if (planned > 0)
			m_EventStartOffsets.Insert(ScaleEventOffset(durationMs, 0.10));
		if (planned > 1)
			m_EventStartOffsets.Insert(ScaleEventOffset(durationMs, 0.28));
		if (planned > 2)
			m_EventStartOffsets.Insert(ScaleEventOffset(durationMs, 0.40));

		int o0 = 0;
		int o1 = 0;
		int o2 = 0;
		if (m_EventStartOffsets.Count() > 0)
			o0 = m_EventStartOffsets[0];
		if (m_EventStartOffsets.Count() > 1)
			o1 = m_EventStartOffsets[1];
		if (m_EventStartOffsets.Count() > 2)
			o2 = m_EventStartOffsets[2];
		int preferI = 0;
		if (preferFirst)
			preferI = 1;
		Print(string.Format("[IA][Defend] Event plan count=%1 offsetsMs=%2/%3/%4 durationMs=%5 preferFirst=%6",
			planned, o0, o1, o2, durationMs, preferI), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected int ScaleEventOffset(int durationMs, float frac)
	{
		int ms = Math.Round(durationMs * frac);
		if (ms < 30000)
			ms = 30000;
		return ms;
	}

	//------------------------------------------------------------------------------------------------
	protected void FillEventPool(notnull array<int> pool)
	{
		IA_Config cfg = m_Config;
		bool all = true;
		if (cfg)
		{
			if (cfg.GetDefenseEventMask() != 0)
				all = false;
		}

		if (all || (cfg && cfg.m_bDefendEventCommander))
			pool.Insert(IA_DefendEventType.CommanderFob);
		if (all || (cfg && cfg.m_bDefendEventElite))
			pool.Insert(IA_DefendEventType.ElitePatrol);
		if (all || (cfg && cfg.m_bDefendEventScoutMortar))
			pool.Insert(IA_DefendEventType.ScoutMortar);
		if (all || (cfg && cfg.m_bDefendEventConvoy))
			pool.Insert(IA_DefendEventType.Convoy);
		if (all || (cfg && cfg.m_bDefendEventSniper))
			pool.Insert(IA_DefendEventType.Sniper);
	}

	//------------------------------------------------------------------------------------------------
	protected void PreferType(notnull array<int> pool, IA_DefendEventType prefer)
	{
		int at = pool.Find(prefer);
		if (at <= 0)
			return;
		int first = pool[0];
		pool[0] = prefer;
		pool[at] = first;
	}

	//------------------------------------------------------------------------------------------------
	protected bool RollThirdEvent()
	{
		int players = GetConnectedPlayers();
		float chance = 0.25;
		if (m_Config)
		{
			chance = m_Config.m_fDefendThirdChanceLow;
			if (players >= 17)
				chance = m_Config.m_fDefendThirdChanceHigh;
			else if (players >= 9)
				chance = m_Config.m_fDefendThirdChanceMid;
		}
		else if (players >= 17)
			chance = 0.75;
		else if (players >= 9)
			chance = 0.50;
		return IA_Game.rng.RandFloat01() < chance;
	}

	//------------------------------------------------------------------------------------------------
	protected void PromotePriority(IA_DefendEvent ev)
	{
		if (!ev)
			return;
		float dist = vector.Distance(GetDefendPoint(), ev.GetSite());
		int deadline = Math.RandomInt(600000, 840001);
		if (dist > 500)
			deadline = deadline + 120000;
		if (ev.GetType() == IA_DefendEventType.CommanderFob)
			deadline = deadline + 120000;
		ev.SetPriorityDeadline(deadline);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyPrioritySuccess()
	{
		int minS = 120;
		int maxS = 240;
		if (m_Config)
		{
			minS = m_Config.m_iDefendPrioritySuccessMinSec;
			maxS = m_Config.m_iDefendPrioritySuccessMaxSec;
		}
		if (maxS < minS)
			maxS = minS;
		int cut = Math.RandomInt(minS, maxS + 1);
		m_Mission.AdjustRemainingMs(-cut * 1000, 360000);
		Notify("ReinforcementsCalled", "HIGH PRIORITY complete — hold shortened.");
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyPriorityFail()
	{
		int minS = 180;
		int maxS = 300;
		if (m_Config)
		{
			minS = m_Config.m_iDefendPriorityFailMinSec;
			maxS = m_Config.m_iDefendPriorityFailMaxSec;
		}
		if (maxS < minS)
			maxS = minS;
		int add = Math.RandomInt(minS, maxS + 1);
		m_Mission.AdjustRemainingMs(add * 1000, 60000);
	}

	//------------------------------------------------------------------------------------------------
	protected void CancelOneLaterEvent(IA_DefendEvent except)
	{
		int count = m_Events.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			IA_DefendEvent ev = m_Events[i];
			if (!ev || ev == except)
				continue;
			if (ev.GetState() != IA_DefendEventState.Pending)
				continue;
			ev.Cancel();
			Notify("ReinforcementsCalled", "Commander down — later special event cancelled.");
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected IA_DefendDoctrine RollDoctrine()
	{
		ref array<int> options = new array<int>();
		IA_Config cfg = m_Config;
		bool any = false;
		if (cfg)
			any = cfg.GetDefenseDoctrineMask() != 0;

		if (!any || (cfg && cfg.m_bDefendDoctrineSiege))
			options.Insert(IA_DefendDoctrine.Siege);
		if (!any || (cfg && cfg.m_bDefendDoctrineBreakthrough))
		{
			if (HasRoad())
				options.Insert(IA_DefendDoctrine.Breakthrough);
		}
		if (!any || (cfg && cfg.m_bDefendDoctrineAirAssault))
		{
			if (HasLz())
				options.Insert(IA_DefendDoctrine.AirAssault);
		}
		if (!any || (cfg && cfg.m_bDefendDoctrineCommand))
			options.Insert(IA_DefendDoctrine.CommandOffensive);

		if (options.IsEmpty())
			return IA_DefendDoctrine.Siege;

		int pick = 0;
		if (options.Count() > 1)
			pick = Math.RandomInt(0, options.Count());
		return options[pick];
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasRoad()
	{
		vector road = IA_VehicleManager.FindRoadInAnnulus(GetDefendPoint(), 200, 800, -1);
		return road != vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasLz()
	{
		vector lz;
		return IA_SpawnPlacement.TryFindDefendDropLz(GetDefendPoint(), false, lz);
	}

	//------------------------------------------------------------------------------------------------
	protected bool RollHotDrop()
	{
		float chance = 0.20;
		if (m_Config)
			chance = m_Config.m_fDefendHotDropChance;
		return IA_Game.rng.RandFloat01() < chance;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyDuration()
	{
		int minM = 18;
		int maxM = 22;
		if (m_Config)
		{
			minM = m_Config.m_iDefendDurationMinMin;
			maxM = m_Config.m_iDefendDurationMaxMin;
		}
		if (maxM < minM)
			maxM = minM;
		int minutes = Math.RandomInt(minM, maxM + 1);
		m_Mission.SetDurationMs(minutes * 60000);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyPrepareWindow()
	{
		int minS = 120;
		int maxS = 180;
		if (m_Config)
		{
			minS = m_Config.m_iDefendPrepareMinSec;
			maxS = m_Config.m_iDefendPrepareMaxSec;
		}
		if (maxS < minS)
			maxS = minS;
		m_iPrepareMs = Math.RandomInt(minS, maxS + 1) * 1000;
	}

	//------------------------------------------------------------------------------------------------
	protected float PhasePressure()
	{
		if (m_ePhase == IA_DefendPhase.Prepare)
			return 0.15;
		if (m_ePhase == IA_DefendPhase.Probe)
			return 0.35;
		if (m_ePhase == IA_DefendPhase.Assault)
			return 0.70;
		if (m_ePhase == IA_DefendPhase.Crisis)
			return 1.0;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected int HudPhase()
	{
		if (m_ePhase == IA_DefendPhase.Prepare)
			return IA_DefendHudPhase.Prepare;
		if (m_ePhase == IA_DefendPhase.Probe)
			return IA_DefendHudPhase.Probe;
		if (m_ePhase == IA_DefendPhase.Assault)
			return IA_DefendHudPhase.Assault;
		if (m_ePhase == IA_DefendPhase.Crisis)
			return IA_DefendHudPhase.Crisis;
		if (m_ePhase == IA_DefendPhase.Secure)
			return IA_DefendHudPhase.Secure;
		return IA_DefendHudPhase.None;
	}

	//------------------------------------------------------------------------------------------------
	protected void Publish()
	{
		if (!m_Mission)
			return;
		IA_DefendHudState state = IA_DefendHudState.Active;
		if (m_ePhase == IA_DefendPhase.Secure)
			state = IA_DefendHudState.Complete;

		int remainSec = 0;
		float timeLeft = 0;
		if (!m_bClockStarted)
		{
			int leftSec = (m_iPrepareMs / 1000) - (System.GetUnixTime() - m_iPrepareStartUnix);
			if (leftSec < 0)
				leftSec = 0;
			remainSec = leftSec;
			if (m_iPrepareMs > 0)
				timeLeft = (leftSec * 1000.0) / m_iPrepareMs;
		}
		else
		{
			int leftMs = m_Mission.GetRemainingMs();
			remainSec = Math.Ceil(leftMs / 1000.0);
			timeLeft = 1.0 - m_Mission.GetElapsedClock01();
		}
		if (remainSec < 0)
			remainSec = 0;
		if (timeLeft < 0)
			timeLeft = 0;
		if (timeLeft > 1)
			timeLeft = 1;

		m_Mission.PublishEnhancedHud(state, HudPhase(), timeLeft, remainSec, PhasePressure());
	}

	//------------------------------------------------------------------------------------------------
	protected string DoctrineName()
	{
		if (m_eDoctrine == IA_DefendDoctrine.Siege)
			return "SIEGE";
		if (m_eDoctrine == IA_DefendDoctrine.Breakthrough)
			return "BREAKTHROUGH";
		if (m_eDoctrine == IA_DefendDoctrine.AirAssault)
			return "AIR ASSAULT";
		return "COMMAND OFFENSIVE";
	}

	//------------------------------------------------------------------------------------------------
	protected static int GetConnectedPlayers()
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return 1;
		int n = pm.GetPlayerCount();
		if (n < 1)
			return 1;
		return n;
	}
}
