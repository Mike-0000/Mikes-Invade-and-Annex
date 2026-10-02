//------------------------------------------------------------------------------------------------
//! Transport pilot card. One card per landing: IA_TransportPilotTracker sends an
//! IA_PilotDropoffPayload at most once a second and Present() merges updates into
//! the open card, so twelve passengers light twelve pips on one card instead of
//! queueing twelve toasts. Independent of IA_NotificationToast, so it never
//! delays task or alert toasts. Pattern: Create(runtime) → overlay AddChild,
//! SetAlign(1, 0). Keep as protected ref.
//!
//! Chrome matches IA_CaptureHud (beveled #0d1412 body, 18px tab) in the amber
//! transport tone. Docked top-right under the rank chip, slides in from the right.
//!   Drop    tab COMBAT INSERTION: +points count-up, troop pips, weight chip,
//!           HOT LZ or the distance out, then the progress block.
//!   Status  tab TRANSPORT RATING: progress block alone, shown on taking a seat.
//!   Unlock  tab SKIN UNLOCKED: gold tone, skin name, sweep, sparks, one sound.
//! Progress block: a helicopter that is painted nose to tail in the colour of
//! the livery being worked towards as the rating nears its threshold, with the
//! total, the threshold and the percent. The card is sent the livery's name and
//! not the airframe, so the helicopter is always the Huey.
//------------------------------------------------------------------------------------------------
enum IA_PilotHudAnim
{
	Idle,
	Intro,
	Hold,
	Outro
}

//------------------------------------------------------------------------------------------------
class IA_PilotHud : MUI_Surface
{
	protected static const float CARD_W = 328;
	protected static const float CARD_H = 148;
	protected static const float TAB_H = 18;
	protected static const float HERO_H = 76;
	protected static const float PROG_H = 54;
	protected static const float HINT_H = 20;
	protected static const float PAD = 14;
	protected static const float BEVEL = 4;
	protected static const float TAB_PAD_X = 12;
	// Below the rank chip, which shares the top-right corner.
	protected static const float DOCK_Y = 78;
	protected static const float SLIDE_FROM = 22;

	protected static const float SIL_W = 120;
	protected static const float SIL_H = 40;
	protected static const float SKEW = 0.35;
	protected static const float SPAN_LO = 6.0;
	protected static const float SPAN_HI = 112.5;
	protected static const int MAX_PIPS = 24;
	protected static const int OVERFLOW_PIPS = 21;
	protected static const float PIP_STEP = 12.5;

	protected static const int FONT_TAB = 10;
	protected static const int FONT_CAP = 9;
	protected static const int FONT_HERO = 30;
	protected static const int FONT_UNLOCK = 22;
	protected static const int FONT_SIDE = 16;
	protected static const int FONT_CHIP = 11;
	protected static const int FONT_TOTAL = 20;
	protected static const int FONT_GOAL = 12;
	protected static const int FONT_PCT = 13;
	protected static const float TRACK_TAB = 1.5;
	protected static const float TRACK_CAP = 1.4;

	protected static const float INTRO_DUR = 0.42;
	protected static const float OUTRO_DUR = 0.30;
	protected static const float HOLD_STATUS = 6.0;
	protected static const float HOLD_DROP = 7.5;
	protected static const float HOLD_UNLOCK = 12.0;
	protected static const float COUNT_SPEED = 5.0;
	protected static const float PIP_RATE = 14.0;
	protected static const float PAINT_SPEED = 3.5;
	protected static const float ROTOR_SPEED = 11.0;
	protected static const float UNLOCK_DELAY = 1.1;
	protected static const float UNLOCK_DELAY_SHORT = 0.45;
	protected static const float UNLOCK_DUR = 0.55;
	protected static const float SWEEP_PERIOD = 3.2;
	protected static const float SWEEP_DUR = 0.9;
	protected static const int SPARK_COUNT = 16;

	protected static const string TEXT_RATING = "TRANSPORT RATING";
	protected static const string TEXT_FLY = "TAKE THE PILOT SEAT TO FLY IT";
	protected static const string TEXT_HINT = "DELIVER TROOPS NEAR THE OBJECTIVE";
	protected static const string TEXT_UNLOCKED = "UNLOCKED";
	protected static const string TEXT_RATING_WORD = "RATING";

	// Huey, side view, nose left, in a 120x40 box. Every piece is convex.
	protected static const ref array<float> FUSELAGE = {5, 23, 8, 18.5, 15, 15.2, 26, 14, 72, 14, 78, 17, 80, 21.5, 75, 28.5, 64, 31, 18, 31, 9, 28.5, 5.5, 25.5};
	protected static const ref array<float> COWL = {40, 14, 43, 9.5, 64, 9.5, 71, 14};
	protected static const ref array<float> BOOM = {76, 16, 112, 12, 112, 15.5, 78, 22.5};
	protected static const ref array<float> FIN = {107, 15, 113.5, 2, 117.5, 2, 113.5, 15.6};
	protected static const ref array<float> STAB = {92, 15.2, 103, 14, 103, 16, 92, 17.4};
	protected static const ref array<float> WINDSHIELD = {9.5, 21.5, 11.5, 18.3, 16.5, 16.2, 22, 15.6, 22, 21.5};
	protected static const ref array<float> DOOR_FRONT = {26, 16, 39, 16, 39, 21.5, 26, 21.5};
	protected static const ref array<float> DOOR_REAR = {43, 16, 54, 16, 54, 21.5, 43, 21.5};

	protected IA_PilotHudAnim m_eAnim;
	protected ref IA_PilotDropoffPayload m_Data;

	protected float m_fAnimT;
	protected float m_fHold;
	protected float m_fAge;
	protected float m_fSlideX;
	protected float m_fHeroT;
	protected float m_fShownPoints;
	protected float m_fShownRating;
	protected float m_fPips;
	protected float m_fPaint;
	protected float m_fGain;
	protected float m_fRotor;

	protected bool m_bUnlocked;
	protected float m_fUnlockDelay;
	protected float m_fUnlockT;
	protected float m_fFlash;
	protected float m_fSweepClock;

	// Strings and widths are rebuilt only when a shown number changes.
	protected bool m_bTextDirty;
	protected int m_iTextPoints;
	protected int m_iTextRating;
	protected bool m_bHotLz;
	protected string m_sTab;
	protected string m_sPoints;
	protected string m_sTroops;
	protected string m_sChip;
	protected string m_sLz;
	protected string m_sMore;
	protected string m_sSkin;
	protected string m_sUnlockName;
	protected string m_sTotal;
	protected string m_sGoal;
	protected string m_sPct;
	protected float m_fTabW;
	protected float m_fPointsSideW;
	protected float m_fTroopsW;
	protected float m_fTroopsCapW;
	protected float m_fChipW;
	protected float m_fLzW;
	protected float m_fSkinW;
	protected float m_fTotalW;
	protected float m_fGoalW;
	protected float m_fPctW;
	protected float m_fUnlockedW;
	protected float m_fRatingWordW;
	protected ref map<int, float> m_mCharW;

	protected ref Color m_HudBg;
	protected ref Color m_HudTab;
	protected ref Color m_HudAmber;
	protected ref Color m_HudGold;
	protected ref Color m_HudSand;
	protected ref Color m_HudOlive;
	protected ref Color m_HudGlass;
	protected ref Color m_HudWhite;
	protected ref Color m_HudMuted;
	protected ref Color m_HudGreen;
	protected ref Color m_HudTone;
	protected ref Color m_HudPaint;
	// The livery's own colour, and that colour on its way to gold.
	protected ref Color m_HudCoat;
	protected ref Color m_HudHull;

	protected ref array<float> m_aBodyPoly;
	protected ref array<float> m_aClipA;
	protected ref array<float> m_aClipB;
	protected ref array<float> m_aDraw;
	protected ref array<float> m_aSparkX;
	protected ref array<float> m_aSparkY;
	protected ref array<float> m_aSparkVX;
	protected ref array<float> m_aSparkVY;
	protected ref array<float> m_aSparkLife;

	//------------------------------------------------------------------------------------------------
	void IA_PilotHud()
	{
		m_Style.m_WidthMode = MUI_SizeMode.Exact;
		m_Style.m_HeightMode = MUI_SizeMode.Exact;
		m_Style.m_fWidth = CARD_W;
		m_Style.m_fHeight = CARD_H;
		m_Style.m_fMinWidth = CARD_W;
		m_Style.m_fMinHeight = CARD_H;
		m_Style.m_fRadius = 0;
		m_Style.m_Fill = Color.FromInt(0);
		m_Style.m_bBlockHit = false;
		m_Style.m_bInteractive = false;
		m_bBlurEnabled = true;
		m_fBlurIntensity = 0.90;
		m_eAnim = IA_PilotHudAnim.Idle;
		m_fIntroDuration = 0;
		m_fIntro = 1;

		m_HudBg = Color.FromSRGBA(13, 20, 18, 230);
		m_HudTab = Color.FromSRGBA(58, 42, 18, 214);
		m_HudAmber = Color.FromSRGBA(255, 184, 72, 255);
		m_HudGold = Color.FromSRGBA(255, 214, 120, 255);
		m_HudSand = Color.FromSRGBA(226, 192, 132, 255);
		m_HudOlive = Color.FromSRGBA(78, 88, 60, 255);
		m_HudGlass = Color.FromSRGBA(13, 20, 18, 255);
		m_HudWhite = Color.FromSRGBA(255, 255, 255, 255);
		m_HudMuted = Color.FromSRGBA(158, 168, 163, 255);
		m_HudGreen = Color.FromSRGBA(94, 251, 131, 255);
		m_HudTone = Color.FromSRGBA(255, 184, 72, 255);
		m_HudPaint = Color.FromSRGBA(226, 192, 132, 255);
		m_HudCoat = Color.FromSRGBA(226, 192, 132, 255);
		m_HudHull = Color.FromSRGBA(226, 192, 132, 255);

		m_mCharW = new map<int, float>();
		m_aBodyPoly = new array<float>();
		m_aClipA = new array<float>();
		m_aClipB = new array<float>();
		m_aDraw = new array<float>();
		m_aSparkX = new array<float>();
		m_aSparkY = new array<float>();
		m_aSparkVX = new array<float>();
		m_aSparkVY = new array<float>();
		m_aSparkLife = new array<float>();
	}

	//------------------------------------------------------------------------------------------------
	static IA_PilotHud Create(notnull MUI_Runtime runtime)
	{
		ref IA_PilotHud hud = new IA_PilotHud();
		runtime.Adopt(hud);
		hud.SetName("pilotCard");
		hud.SetWidth(CARD_W);
		hud.SetHeight(CARD_H);
		hud.SetVisible(false);
		return hud;
	}

	//------------------------------------------------------------------------------------------------
	bool IsIdle()
	{
		return m_eAnim == IA_PilotHudAnim.Idle;
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyTheme(notnull MUI_ThemeData theme)
	{
		m_Style.m_Fill = Color.FromInt(0);
	}

	//------------------------------------------------------------------------------------------------
	void Abort()
	{
		m_eAnim = IA_PilotHudAnim.Idle;
		m_Data = null;
		m_fIntro = 1;
		m_fSlideY = 0;
		m_fSlideX = 0;
		SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Show an update. While the card is open the update is folded into it; a
	//! status never replaces a drop that is still on screen.
	void Present(notnull IA_PilotDropoffPayload data)
	{
		if (m_Data && (m_eAnim == IA_PilotHudAnim.Intro || m_eAnim == IA_PilotHudAnim.Hold))
		{
			MergeUpdate(data);
			return;
		}

		// A status with no total has nothing to say.
		if (!data.IsDrop() && !data.HasUnlock() && data.m_iRating < 0)
			return;

		m_Data = data;
		m_fAge = 0;
		m_fShownPoints = 0;
		m_fPips = 0;
		m_fPaint = 0;
		m_fGain = 0;
		m_fFlash = 0;
		m_fSweepClock = 0;
		m_fUnlockT = 0;
		m_bUnlocked = false;
		ClearSparks();

		m_fShownRating = 0;
		if (data.m_iRating >= 0)
		{
			m_fShownRating = data.m_iRating - data.m_iPoints;
			if (m_fShownRating < 0)
				m_fShownRating = 0;
		}

		m_fUnlockDelay = UNLOCK_DELAY_SHORT;
		if (data.IsDrop())
		{
			m_fGain = 1;
			m_fUnlockDelay = UNLOCK_DELAY;
		}

		m_fHeroT = HeroTarget();
		m_fHold = HoldFor();
		m_iTextPoints = 0;
		m_iTextRating = Math.Round(m_fShownRating);
		m_bTextDirty = true;
		MixTone();
		BeginIntro();
	}

	//------------------------------------------------------------------------------------------------
	protected void MergeUpdate(notnull IA_PilotDropoffPayload data)
	{
		bool hadUnlock = m_Data.HasUnlock();
		int oldRating = m_Data.m_iRating;
		m_Data.Merge(data);

		// The total arrived after the card opened: count it up from before this landing.
		if (oldRating < 0 && m_Data.m_iRating >= 0)
		{
			m_fShownRating = m_Data.m_iRating - m_Data.m_iPoints;
			if (m_fShownRating < 0)
				m_fShownRating = 0;
			m_fPaint = 0;
		}

		bool newUnlock = !hadUnlock && m_Data.HasUnlock();
		if (newUnlock)
			m_fUnlockDelay = UNLOCK_DELAY_SHORT;
		if (data.IsDrop())
			m_fGain = 1;

		if (data.IsDrop() || newUnlock || m_Data.m_iRating != oldRating)
		{
			float hold = HoldFor();
			if (m_fHold < hold)
				m_fHold = hold;
		}
		m_bTextDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	protected float HoldFor()
	{
		if (m_Data.HasUnlock())
			return HOLD_UNLOCK;
		if (m_Data.IsDrop())
			return HOLD_DROP;
		return HOLD_STATUS;
	}

	//------------------------------------------------------------------------------------------------
	//! The hero row opens for a landing, and for a status only once its unlock fires.
	protected float HeroTarget()
	{
		if (m_Data.IsDrop() || m_bUnlocked)
			return 1;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected float BodyHeight()
	{
		return HERO_H * m_fHeroT + PROG_H + HINT_H * (1.0 - m_fHeroT);
	}

	//------------------------------------------------------------------------------------------------
	//! Threshold the progress block counts towards; 0 when nothing is left to unlock.
	protected int GoalPoints()
	{
		if (m_Data.HasUnlock())
			return m_Data.m_iUnlockedRequired;
		return m_Data.m_iRequired;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ShowsUnlocked()
	{
		if (m_Data.HasUnlock())
			return m_bUnlocked;
		return m_Data.m_iRequired <= 0;
	}

	//------------------------------------------------------------------------------------------------
	protected float ProgressTarget()
	{
		if (m_Data.m_iRating < 0)
			return 0;

		int goal = GoalPoints();
		if (goal <= 0 || m_bUnlocked)
			return 1;
		return MUI_Ease.Clamp01(m_fShownRating / goal);
	}

	//------------------------------------------------------------------------------------------------
	override void OnTick(float dt)
	{
		super.OnTick(dt);
		if (m_eAnim == IA_PilotHudAnim.Idle)
			return;

		TickAnim(dt);
		if (m_eAnim == IA_PilotHudAnim.Idle || !m_Data)
			return;

		TickContent(dt);
		TickSparks(dt);
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	protected void BeginIntro()
	{
		m_eAnim = IA_PilotHudAnim.Intro;
		m_fAnimT = 0;
		m_fIntroDuration = 0;
		m_fIntro = 0;
		m_fSlideY = DOCK_Y;
		m_fSlideX = SLIDE_FROM;
		SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void BeginOutro()
	{
		if (m_eAnim == IA_PilotHudAnim.Idle || m_eAnim == IA_PilotHudAnim.Outro)
			return;
		m_eAnim = IA_PilotHudAnim.Outro;
		m_fAnimT = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickAnim(float dt)
	{
		m_fSlideY = DOCK_Y;

		if (m_eAnim == IA_PilotHudAnim.Intro)
		{
			m_fAnimT = m_fAnimT + dt / INTRO_DUR;
			float t = MUI_Ease.CubicOut(m_fAnimT);
			m_fIntro = t;
			m_fSlideX = (1.0 - t) * SLIDE_FROM;
			if (m_fAnimT < 1)
				return;
			m_eAnim = IA_PilotHudAnim.Hold;
			m_fIntro = 1;
			m_fSlideX = 0;
			return;
		}

		if (m_eAnim == IA_PilotHudAnim.Hold)
		{
			m_fHold = m_fHold - dt;
			if (m_fHold <= 0)
				BeginOutro();
			return;
		}

		m_fAnimT = m_fAnimT + dt / OUTRO_DUR;
		float u = MUI_Ease.CubicIn(m_fAnimT);
		m_fIntro = 1.0 - u;
		m_fSlideX = u * SLIDE_FROM * 0.6;
		if (m_fAnimT < 1)
			return;

		Abort();
	}

	//------------------------------------------------------------------------------------------------
	protected void TickContent(float dt)
	{
		m_fAge = m_fAge + dt;
		m_fRotor = m_fRotor + dt * ROTOR_SPEED;
		if (m_fRotor > Math.PI2)
			m_fRotor = m_fRotor - Math.PI2;

		m_fHeroT = MUI_Ease.Approach(m_fHeroT, HeroTarget(), dt, 9.0);
		if (m_fGain > 0)
		{
			m_fGain = m_fGain - dt * 0.8;
			if (m_fGain < 0)
				m_fGain = 0;
		}

		// Staged reveal: points, then the pips, then the total.
		if (m_fAge > 0.2)
			m_fShownPoints = CountTowards(m_fShownPoints, m_Data.m_iPoints, dt);
		if (m_fAge > 0.3 && m_fPips < m_Data.m_iTroops)
		{
			m_fPips = m_fPips + dt * PIP_RATE;
			if (m_fPips > m_Data.m_iTroops)
				m_fPips = m_Data.m_iTroops;
		}
		if (m_fAge > 0.45 && m_Data.m_iRating >= 0)
			m_fShownRating = CountTowards(m_fShownRating, m_Data.m_iRating, dt);

		m_fPaint = MUI_Ease.Approach(m_fPaint, ProgressTarget(), dt, PAINT_SPEED);

		int points = Math.Round(m_fShownPoints);
		int rating = Math.Round(m_fShownRating);
		if (points != m_iTextPoints || rating != m_iTextRating)
		{
			m_iTextPoints = points;
			m_iTextRating = rating;
			m_bTextDirty = true;
		}

		TickUnlock(dt);
	}

	//------------------------------------------------------------------------------------------------
	protected float CountTowards(float shown, float target, float dt)
	{
		float next = MUI_Ease.Approach(shown, target, dt, COUNT_SPEED);
		if (Math.AbsFloat(target - next) < 0.5)
			return target;
		return next;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickUnlock(float dt)
	{
		if (!m_Data.HasUnlock())
			return;

		if (!m_bUnlocked)
		{
			// Let the total finish climbing to the threshold before the card turns gold.
			m_fUnlockDelay = m_fUnlockDelay - dt;
			if (m_fUnlockDelay > 0 || m_fShownRating < m_Data.m_iRating)
				return;

			m_bUnlocked = true;
			m_fUnlockT = 0;
			m_fFlash = 1;
			m_fSweepClock = 0;
			if (m_fHold < HOLD_UNLOCK)
				m_fHold = HOLD_UNLOCK;
			m_bTextDirty = true;
			SeedSparks();
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.TASK_SUCCEED);
			return;
		}

		if (m_fUnlockT < 1)
		{
			m_fUnlockT = m_fUnlockT + dt / UNLOCK_DUR;
			if (m_fUnlockT > 1)
				m_fUnlockT = 1;
			MixTone();
		}
		if (m_fFlash > 0)
		{
			m_fFlash = m_fFlash - dt * 1.6;
			if (m_fFlash < 0)
				m_fFlash = 0;
		}
		m_fSweepClock = m_fSweepClock + dt;
		if (m_fSweepClock > SWEEP_PERIOD)
			m_fSweepClock = m_fSweepClock - SWEEP_PERIOD;
	}

	//------------------------------------------------------------------------------------------------
	protected void MixTone()
	{
		float t = MUI_Ease.CubicOut(m_fUnlockT);
		MUI_ColorUtil.Mix(m_HudAmber, m_HudGold, t, m_HudTone);
		MUI_ColorUtil.Mix(m_HudSand, m_HudGold, t, m_HudPaint);
		MUI_ColorUtil.Mix(m_HudCoat, m_HudGold, t * 0.5, m_HudHull);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearSparks()
	{
		m_aSparkX.Clear();
		m_aSparkY.Clear();
		m_aSparkVX.Clear();
		m_aSparkVY.Clear();
		m_aSparkLife.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Sparks rise off the airframe. Positions are relative to the card origin.
	protected void SeedSparks()
	{
		ClearSparks();
		float top = TAB_H + HERO_H * m_fHeroT + 12;
		int i;
		for (i = 0; i < SPARK_COUNT; i++)
		{
			m_aSparkX.Insert(PAD + Math.RandomFloat(8, SIL_W - 8));
			m_aSparkY.Insert(top + Math.RandomFloat(0, 22));
			m_aSparkVX.Insert(Math.RandomFloat(-46, 46));
			m_aSparkVY.Insert(Math.RandomFloat(-95, -30));
			m_aSparkLife.Insert(Math.RandomFloat(0.65, 1.0));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TickSparks(float dt)
	{
		int count = m_aSparkLife.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			if (m_aSparkLife[i] <= 0)
				continue;

			m_aSparkX[i] = m_aSparkX[i] + m_aSparkVX[i] * dt;
			m_aSparkY[i] = m_aSparkY[i] + m_aSparkVY[i] * dt;
			m_aSparkVY[i] = m_aSparkVY[i] + 70 * dt;
			float life = m_aSparkLife[i] - dt * 1.15;
			if (life < 0)
				life = 0;
			m_aSparkLife[i] = life;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildTexts()
	{
		m_bTextDirty = false;

		if (m_bUnlocked)
			m_sTab = "SKIN UNLOCKED";
		else if (m_Data.IsDrop())
			m_sTab = "COMBAT INSERTION";
		else
			m_sTab = TEXT_RATING;
		m_fTabW = MeasureTracked(m_sTab, FONT_TAB, TRACK_TAB) + TAB_PAD_X * 2;

		m_sPoints = "+" + IA_PilotDropoffPayload.FormatNumber(m_iTextPoints);
		m_fPointsSideW = TextWidth(m_sPoints, FONT_SIDE);

		int troops = m_Data.m_iTroops;
		if (troops == 1)
			m_sTroops = "1 TROOP";
		else
			m_sTroops = string.Format("%1 TROOPS", troops);
		m_fTroopsW = TextWidth(m_sTroops, FONT_SIDE);
		m_fTroopsCapW = MeasureTracked(m_sTroops, FONT_CAP, TRACK_CAP);

		m_sMore = "";
		if (troops > MAX_PIPS)
			m_sMore = string.Format("+%1", troops - OVERFLOW_PIPS);

		int weight = m_Data.WeightTenths();
		int weightWhole = weight / 10;
		int weightFrac = weight % 10;
		m_sChip = string.Format("x%1.%2", weightWhole, weightFrac);
		m_fChipW = TextWidth(m_sChip, FONT_CHIP) + 10;

		m_bHotLz = m_Data.IsHotLz();
		if (m_bHotLz)
		{
			m_sLz = "HOT LZ";
		}
		else
		{
			float tens = m_Data.m_iEdgeM;
			int edge = Math.Round(tens / 10) * 10;
			m_sLz = string.Format("LZ %1 M OUT", edge);
		}
		m_fLzW = MeasureTracked(m_sLz, FONT_CAP, TRACK_CAP);

		m_sUnlockName = m_Data.m_sUnlockedName;
		m_sUnlockName.ToUpper();
		m_sSkin = m_Data.m_sSkinName;
		if (m_Data.HasUnlock())
			m_sSkin = m_Data.m_sUnlockedName;

		// The hull takes the colour of that livery; sand when the name is not one of ours.
		IA_HeliSkinDef livery = IA_HeliSkinCatalog.FindDefByName(m_sSkin);
		if (livery)
			m_HudCoat = Color.FromSRGBA(livery.m_iSwatchR, livery.m_iSwatchG, livery.m_iSwatchB, 255);
		else
			m_HudCoat = Color.FromSRGBA(226, 192, 132, 255);
		MixTone();

		if (m_sSkin.IsEmpty())
			m_sSkin = "TRANSPORT PILOT";
		m_sSkin.ToUpper();
		m_fSkinW = MeasureTracked(m_sSkin, FONT_TAB, TRACK_TAB);

		int goal = GoalPoints();
		m_sTotal = IA_PilotDropoffPayload.FormatNumber(m_iTextRating);
		m_fTotalW = TextWidth(m_sTotal, FONT_TOTAL);
		m_sGoal = "/ " + IA_PilotDropoffPayload.FormatNumber(goal);
		m_fGoalW = TextWidth(m_sGoal, FONT_GOAL);
		m_sPct = IA_PilotDropoffPayload.FormatPercent(IA_PilotDropoffPayload.PercentTenths(m_iTextRating, goal));
		m_fPctW = TextWidth(m_sPct, FONT_PCT);
		m_fUnlockedW = MeasureTracked(TEXT_UNLOCKED, FONT_CAP, TRACK_CAP);
		m_fRatingWordW = TextWidth(TEXT_RATING_WORD, FONT_GOAL);
	}

	//------------------------------------------------------------------------------------------------
	override void SyncHostWidgets()
	{
		if (!m_bBlurEnabled || !IsVisible() || m_eAnim == IA_PilotHudAnim.Idle)
		{
			if (m_wBlur)
				m_wBlur.SetVisible(false);
			return;
		}

		if (!EnsureBlurWidget())
			return;

		float op = GetDrawOpacity();
		float x;
		float y;
		m_Runtime.GetHostLocalPos(this, x, y);
		if (op < 0.02)
		{
			m_wBlur.SetVisible(false);
			return;
		}

		m_wBlur.SetVisible(true);
		m_wBlur.SetColor(m_HudBg);
		FrameSlot.SetAnchorMin(m_wBlur, 0, 0);
		FrameSlot.SetAnchorMax(m_wBlur, 0, 0);
		FrameSlot.SetPos(m_wBlur, x + m_fSlideX, y + TAB_H);
		FrameSlot.SetSize(m_wBlur, CARD_W, BodyHeight());
		m_wBlur.SetOpacity(op);
		m_wBlur.SetIntensity(m_fBlurIntensity * op);
		m_wBlur.SetSmoothBorder(4, 0, 4, 4);
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
		if (m_eAnim == IA_PilotHudAnim.Idle || !m_Data)
			return;

		float op = GetDrawOpacity();
		if (op < 0.01)
			return;

		if (m_bTextDirty)
			RebuildTexts();

		float x = DrawX() + m_fSlideX;
		float y = DrawY();
		float bodyY = y + TAB_H;
		float progY = bodyY + HERO_H * m_fHeroT;
		float heroOp = op * MUI_Ease.Clamp01(m_fHeroT * 2.0 - 1.0);
		float hintOp = op * MUI_Ease.Clamp01(1.0 - m_fHeroT * 2.0);

		SyncHostWidgets();
		DrawBody(surface, x, bodyY, BodyHeight(), op);
		DrawTab(surface, x, y, op);
		if (heroOp > 0.02)
			DrawHero(surface, x, bodyY, heroOp);
		DrawProgress(surface, x, progY, op);
		if (hintOp > 0.02)
			DrawTracked(surface, x + PAD, progY + PROG_H - 1, 14, TEXT_HINT, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(m_HudMuted, hintOp));
		DrawSparks(surface, x, y, op);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawBody(MUI_RenderSurface surface, float x, float y, float h, float op)
	{
		BuildBodyPoly(x, y, CARD_W, h);
		surface.FillPolygon(m_aBodyPoly, MUI_ColorUtil.Fade(m_HudBg, op));
		if (m_fFlash > 0.02)
			surface.FillPolygon(m_aBodyPoly, MUI_ColorUtil.Fade(m_HudGold, op * m_fFlash * 0.20));

		Color edge = MUI_ColorUtil.Fade(m_HudTone, op * 0.22);
		surface.DrawLine(x, y, x + CARD_W, y, edge, 1);
		surface.DrawLine(x, y, x, y + h - BEVEL, edge, 1);
		surface.DrawLine(x + CARD_W, y, x + CARD_W, y + h - BEVEL, edge, 1);

		// Top glow, brightest in the middle.
		float glow = 0.5 + 0.5 * MUI_Ease.Pulse(GetTime(), 0.5);
		int slices = 24;
		float sliceW = CARD_W / slices;
		int i;
		for (i = 0; i < slices; i++)
		{
			float t = i;
			t = t / (slices - 1);
			float a = t * 2.0;
			if (t > 0.5)
				a = (1.0 - t) * 2.0;
			if (a < 0.02)
				continue;
			surface.FillRect(x + sliceW * i, y, sliceW + 0.5, 1, MUI_ColorUtil.Fade(m_HudTone, op * 0.6 * glow * a), 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawTab(MUI_RenderSurface surface, float x, float y, float op)
	{
		surface.FillRect(x, y, m_fTabW, TAB_H + 1, MUI_ColorUtil.Fade(m_HudTab, op), 0);

		Color edge = MUI_ColorUtil.Fade(m_HudTone, op * 0.22);
		surface.DrawLine(x, y, x + m_fTabW, y, edge, 1);
		surface.DrawLine(x, y, x, y + TAB_H, edge, 1);
		surface.DrawLine(x + m_fTabW, y, x + m_fTabW, y + TAB_H, edge, 1);
		DrawTracked(surface, x + TAB_PAD_X, y, TAB_H, m_sTab, FONT_TAB, TRACK_TAB, MUI_ColorUtil.Fade(m_HudTone, op));
	}

	//------------------------------------------------------------------------------------------------
	//! What this landing earned; turns into the unlock announcement when a skin is won.
	protected void DrawHero(MUI_RenderSurface surface, float x, float y, float op)
	{
		float right = x + CARD_W - PAD;
		float dropOp = op * (1.0 - MUI_Ease.Clamp01(m_fUnlockT * 2.0));
		float unlockOp = op * MUI_Ease.Clamp01(m_fUnlockT * 2.0 - 1.0);
		bool hasTroops = m_Data.m_iTroops > 0;
		if (!hasTroops)
			dropOp = 0;

		if (dropOp > 0.02)
		{
			surface.DrawText(x + PAD, y + 5, 200, 34, m_sPoints, FONT_HERO, MUI_ColorUtil.Fade(m_HudTone, dropOp), true, false, true, false, true);
			DrawTracked(surface, x + PAD, y + 37, 12, TEXT_RATING, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(m_HudMuted, dropOp));
			surface.DrawText(right - m_fTroopsW, y + 8, m_fTroopsW + 4, 20, m_sTroops, FONT_SIDE, MUI_ColorUtil.Fade(m_HudWhite, dropOp), true, false, true, false, true);

			Color lzColor = m_HudMuted;
			if (m_bHotLz)
				lzColor = m_HudTone;
			DrawTracked(surface, right - m_fChipW - 8 - m_fLzW, y + 33.5, 12, m_sLz, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(lzColor, dropOp));
		}

		if (unlockOp > 0.02)
		{
			surface.DrawText(x + PAD, y + 7, 236, 30, m_sUnlockName, FONT_UNLOCK, MUI_ColorUtil.Fade(m_HudGold, unlockOp), true, false, true, false, true);
			DrawTracked(surface, x + PAD, y + 37, 12, TEXT_FLY, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(m_HudWhite, unlockOp * 0.72));
			if (hasTroops)
			{
				surface.DrawText(right - m_fPointsSideW, y + 8, m_fPointsSideW + 4, 20, m_sPoints, FONT_SIDE, MUI_ColorUtil.Fade(m_HudWhite, unlockOp), true, false, true, false, true);
				DrawTracked(surface, right - m_fChipW - 8 - m_fTroopsCapW, y + 33.5, 12, m_sTroops, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(m_HudMuted, unlockOp));
			}
		}

		if (hasTroops)
		{
			float chipX = right - m_fChipW;
			surface.FillRect(chipX, y + 32, m_fChipW, 15, MUI_ColorUtil.Fade(m_HudTone, op * 0.16), 0);
			surface.StrokeRect(chipX, y + 32, m_fChipW, 15, MUI_ColorUtil.Fade(m_HudTone, op * 0.5), 1, 0);
			surface.DrawText(chipX + 5, y + 32, m_fChipW, 15, m_sChip, FONT_CHIP, MUI_ColorUtil.Fade(m_HudTone, op), true, false, true, false, true);
			DrawPips(surface, x + PAD, y + 56, op);
		}

		surface.FillRect(x + PAD, y + HERO_H - 4, CARD_W - PAD * 2, 1, MUI_ColorUtil.Fade(m_HudTone, op * 0.16), 0);
	}

	//------------------------------------------------------------------------------------------------
	//! One slanted pip per passenger delivered, lit in sequence as they are credited.
	protected void DrawPips(MUI_RenderSurface surface, float x, float y, float op)
	{
		int shown = m_Data.m_iTroops;
		bool overflow = shown > MAX_PIPS;
		if (overflow)
			shown = OVERFLOW_PIPS;

		float pipOp = 0.55;
		if (m_bHotLz)
			pipOp = 0.95;

		int i;
		for (i = 0; i < shown; i++)
		{
			float lit = MUI_Ease.Clamp01(m_fPips - i);
			if (lit < 0.02)
				break;

			float e = MUI_Ease.CubicOut(lit);
			float px = x + i * PIP_STEP;
			float py = y + (1.0 - e) * 4.0;
			m_aDraw.Clear();
			m_aDraw.Insert(px + 3.5);
			m_aDraw.Insert(py);
			m_aDraw.Insert(px + 11.5);
			m_aDraw.Insert(py);
			m_aDraw.Insert(px + 8);
			m_aDraw.Insert(py + 9);
			m_aDraw.Insert(px);
			m_aDraw.Insert(py + 9);
			surface.FillPolygon(m_aDraw, MUI_ColorUtil.Fade(m_HudTone, op * pipOp * e));
			// The pip being lit flashes white.
			if (lit < 1)
				surface.FillPolygon(m_aDraw, MUI_ColorUtil.Fade(m_HudWhite, op * (1.0 - e) * 0.7));
		}

		if (overflow && m_fPips >= shown)
			surface.DrawText(x + shown * PIP_STEP + 3, y - 2, 60, 13, m_sMore, FONT_CHIP, MUI_ColorUtil.Fade(m_HudTone, op), true, false, true, false, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Where the pilot stands: the airframe paints up as the rating nears the threshold.
	protected void DrawProgress(MUI_RenderSurface surface, float x, float y, float op)
	{
		DrawSilhouette(surface, x + PAD, y + 7, op);

		float cx = x + PAD + SIL_W + 14;
		float right = x + CARD_W - PAD;
		if (m_Data.m_iRating < 0)
		{
			float wait = 0.65 + 0.35 * MUI_Ease.Pulse(GetTime(), 0.8);
			DrawTracked(surface, cx, y + 9, 14, "POINTS BANKED", FONT_TAB, TRACK_TAB, MUI_ColorUtil.Fade(m_HudSand, op));
			surface.DrawText(cx, y + 24, 170, 22, "SYNCING TOTAL", FONT_SIDE, MUI_ColorUtil.Fade(m_HudWhite, op * wait), true, false, true, false, true);
			return;
		}

		DrawTracked(surface, cx, y + 8, 14, m_sSkin, FONT_TAB, TRACK_TAB, MUI_ColorUtil.Fade(m_HudPaint, op));
		surface.DrawText(cx, y + 23, m_fTotalW + 4, 24, m_sTotal, FONT_TOTAL, MUI_ColorUtil.Fade(m_HudWhite, op), true, false, true, false, true);

		float afterTotal = cx + m_fTotalW + 5;
		if (ShowsUnlocked())
		{
			float unlockedX = right - m_fUnlockedW;
			if (afterTotal + m_fRatingWordW + 6 <= unlockedX)
				surface.DrawText(afterTotal, y + 26, m_fRatingWordW + 4, 20, TEXT_RATING_WORD, FONT_GOAL, MUI_ColorUtil.Fade(m_HudMuted, op), true, false, true, false, true);
			DrawTracked(surface, unlockedX, y + 28, 14, TEXT_UNLOCKED, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(m_HudGreen, op));
			return;
		}

		// The percent sits beside the skin name so the threshold always has room
		// under it; a long skin name pushes the percent down to the total's row.
		float pctX = right - m_fPctW;
		float goalLimit = right;
		Color pctColor = MUI_ColorUtil.Fade(m_HudTone, op);
		if (cx + m_fSkinW + 8 <= pctX)
		{
			surface.DrawText(pctX, y + 6, m_fPctW + 4, 18, m_sPct, FONT_PCT, pctColor, true, false, true, false, true);
		}
		else
		{
			surface.DrawText(pctX, y + 26, m_fPctW + 4, 20, m_sPct, FONT_PCT, pctColor, true, false, true, false, true);
			goalLimit = pctX - 6;
		}
		if (afterTotal + m_fGoalW <= goalLimit)
			surface.DrawText(afterTotal, y + 26, m_fGoalW + 4, 20, m_sGoal, FONT_GOAL, MUI_ColorUtil.Fade(m_HudMuted, op), true, false, true, false, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawSilhouette(MUI_RenderSurface surface, float x, float y, float op)
	{
		float front = SPAN_LO + (SPAN_HI - SPAN_LO) * m_fPaint;
		bool full = m_fPaint > 0.998;

		if (full)
		{
			Color whole = MUI_ColorUtil.Fade(m_HudHull, op);
			FillPiece(surface, FUSELAGE, x, y, whole);
			FillPiece(surface, COWL, x, y, whole);
			FillPiece(surface, BOOM, x, y, whole);
			FillPiece(surface, FIN, x, y, whole);
			FillPiece(surface, STAB, x, y, whole);
		}
		else
		{
			Color bare = MUI_ColorUtil.Fade(m_HudOlive, op * 0.9);
			FillPiece(surface, FUSELAGE, x, y, bare);
			FillPiece(surface, COWL, x, y, bare);
			FillPiece(surface, BOOM, x, y, bare);
			FillPiece(surface, FIN, x, y, bare);
			FillPiece(surface, STAB, x, y, bare);

			if (m_fPaint > 0.001)
			{
				// Painted side of a slanted front, parallel to the pips.
				float limit = front + SKEW * SIL_H * 0.5;
				Color painted = MUI_ColorUtil.Fade(m_HudHull, op);
				FillPainted(surface, FUSELAGE, limit, x, y, painted);
				FillPainted(surface, COWL, limit, x, y, painted);
				FillPainted(surface, BOOM, limit, x, y, painted);
				FillPainted(surface, FIN, limit, x, y, painted);
				FillPainted(surface, STAB, limit, x, y, painted);
			}
		}

		if (m_bUnlocked && m_fSweepClock < SWEEP_DUR)
		{
			float s = m_fSweepClock / SWEEP_DUR;
			float from = SPAN_LO - 12 + (SPAN_HI - SPAN_LO + 24) * s + SKEW * SIL_H * 0.5;
			Color shine = MUI_ColorUtil.Fade(m_HudWhite, op * 0.75 * Math.Sin(s * Math.PI));
			FillBand(surface, FUSELAGE, from, x, y, shine);
			FillBand(surface, COWL, from, x, y, shine);
			FillBand(surface, BOOM, from, x, y, shine);
			FillBand(surface, FIN, from, x, y, shine);
			FillBand(surface, STAB, from, x, y, shine);
		}

		Color glass = MUI_ColorUtil.Fade(m_HudGlass, op * 0.92);
		FillPiece(surface, WINDSHIELD, x, y, glass);
		FillPiece(surface, DOOR_FRONT, x, y, glass);
		FillPiece(surface, DOOR_REAR, x, y, glass);

		if (!full && m_Data.m_iRating >= 0)
		{
			float topX = x + front + SKEW * 12;
			float botX = x + front - SKEW * 13;
			if (m_fGain > 0.02)
				surface.DrawLine(topX, y + 8, botX, y + 33, MUI_ColorUtil.Fade(m_HudWhite, op * m_fGain * 0.8), 3.0);
			float beat = 0.65 + 0.35 * MUI_Ease.Pulse(GetTime(), 0.9);
			surface.DrawLine(topX, y + 8, botX, y + 33, MUI_ColorUtil.Fade(m_HudAmber, op * 0.9 * beat), 1.4);
		}

		// Rotor, mast, skids and tail rotor stay line work.
		float lineOp = op * 0.75;
		Color line = MUI_ColorUtil.Fade(m_HudPaint, lineOp);
		surface.DrawLine(x + 3, y + 5.2, x + 103, y + 4.2, MUI_ColorUtil.Fade(m_HudPaint, lineOp * 0.3), 1.0);
		float half = 50 * Math.AbsFloat(Math.Cos(m_fRotor));
		if (half > 2)
			surface.DrawLine(x + 53 - half, y + 4.7 + half * 0.01, x + 53 + half, y + 4.7 - half * 0.01, line, 1.4);
		surface.FillRect(x + 51, y + 4.5, 3, 5.5, line, 0);
		surface.DrawLine(x + 13, y + 36.5, x + 66, y + 36.5, line, 1.4);
		surface.DrawLine(x + 13, y + 36.5, x + 9.5, y + 33.5, line, 1.4);
		surface.DrawLine(x + 27, y + 31, x + 25, y + 36.5, line, 1.2);
		surface.DrawLine(x + 56, y + 31, x + 58, y + 36.5, line, 1.2);

		// Two open 180° arcs: a closed 360° polyline leaves a spoke through the ring.
		Color ring = MUI_ColorUtil.Fade(m_HudPaint, lineOp * 0.8);
		surface.DrawArc(x + 115.5, y + 5.5, 4.2, 0, 180, ring, 1.0);
		surface.DrawArc(x + 115.5, y + 5.5, 4.2, 180, 180, ring, 1.0);
		float tailAngle = m_fRotor * 1.7;
		float tailX = Math.Cos(tailAngle) * 3.6;
		float tailY = Math.Sin(tailAngle) * 3.6;
		surface.DrawLine(x + 115.5 - tailX, y + 5.5 - tailY, x + 115.5 + tailX, y + 5.5 + tailY, ring, 1.0);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawSparks(MUI_RenderSurface surface, float x, float y, float op)
	{
		int count = m_aSparkLife.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			float life = m_aSparkLife[i];
			if (life * op < 0.02)
				continue;
			surface.FillCircle(x + m_aSparkX[i], y + m_aSparkY[i], 1.3 + life * 1.6, MUI_ColorUtil.Fade(m_HudGold, life * op));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fill a silhouette piece at an offset. FillPolygon copies the vertices, so the scratch array is reused.
	protected void FillPiece(MUI_RenderSurface surface, notnull array<float> piece, float ox, float oy, Color color)
	{
		int count = piece.Count();
		if (count < 6)
			return;

		m_aDraw.Clear();
		int i;
		for (i = 0; i < count; i = i + 2)
		{
			m_aDraw.Insert(piece[i] + ox);
			m_aDraw.Insert(piece[i + 1] + oy);
		}
		surface.FillPolygon(m_aDraw, color);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillPainted(MUI_RenderSurface surface, notnull array<float> piece, float limit, float ox, float oy, Color color)
	{
		ClipHalf(piece, 1, SKEW, limit, m_aClipA);
		FillPiece(surface, m_aClipA, ox, oy, color);
	}

	//------------------------------------------------------------------------------------------------
	//! Fill the 9 px slanted band of a piece that starts at `from`.
	protected void FillBand(MUI_RenderSurface surface, notnull array<float> piece, float from, float ox, float oy, Color color)
	{
		ClipHalf(piece, 1, SKEW, from + 9, m_aClipA);
		if (m_aClipA.Count() < 6)
			return;
		ClipHalf(m_aClipA, -1, -SKEW, -from, m_aClipB);
		FillPiece(surface, m_aClipB, ox, oy, color);
	}

	//------------------------------------------------------------------------------------------------
	//! Keep the part of a convex polygon where nx*x + ny*y <= limit (Sutherland-Hodgman, one edge).
	protected void ClipHalf(notnull array<float> src, float nx, float ny, float limit, notnull array<float> dst)
	{
		dst.Clear();
		int points = src.Count() / 2;
		int i;
		for (i = 0; i < points; i++)
		{
			int j = i + 1;
			if (j >= points)
				j = 0;

			float ax = src[i * 2];
			float ay = src[i * 2 + 1];
			float bx = src[j * 2];
			float by = src[j * 2 + 1];
			float da = nx * ax + ny * ay - limit;
			float db = nx * bx + ny * by - limit;
			if (da <= 0)
			{
				dst.Insert(ax);
				dst.Insert(ay);
			}
			if ((da < 0 && db > 0) || (db < 0 && da > 0))
			{
				float t = da / (da - db);
				dst.Insert(ax + (bx - ax) * t);
				dst.Insert(ay + (by - ay) * t);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildBodyPoly(float x, float y, float w, float h)
	{
		m_aBodyPoly.Clear();
		m_aBodyPoly.Insert(x);
		m_aBodyPoly.Insert(y);
		m_aBodyPoly.Insert(x + w);
		m_aBodyPoly.Insert(y);
		m_aBodyPoly.Insert(x + w);
		m_aBodyPoly.Insert(y + h - BEVEL);
		m_aBodyPoly.Insert(x + w - BEVEL);
		m_aBodyPoly.Insert(y + h);
		m_aBodyPoly.Insert(x + BEVEL);
		m_aBodyPoly.Insert(y + h);
		m_aBodyPoly.Insert(x);
		m_aBodyPoly.Insert(y + h - BEVEL);
	}

	//------------------------------------------------------------------------------------------------
	protected float TextWidth(string text, int fontSize)
	{
		float w = 0;
		float h = fontSize;
		if (m_Runtime && !text.IsEmpty())
			m_Runtime.MeasureText(text, fontSize, true, 0, w, h);
		if (w < 1)
			w = text.Length() * fontSize * 0.55;
		return w;
	}

	//------------------------------------------------------------------------------------------------
	//! Width of one tracked glyph. Measuring reconfigures a hidden TextWidget, so each glyph is measured once.
	protected float CharWidth(string ch, int fontSize)
	{
		// A lone space measures as empty.
		if (ch == " ")
			return fontSize * 0.3;

		int key = fontSize * 256 + ch.ToAscii();
		float w;
		if (m_mCharW.Find(key, w))
			return w;
		if (!m_Runtime)
			return fontSize * 0.55;

		float h = fontSize;
		m_Runtime.MeasureText(ch, fontSize, true, 0, w, h);
		if (w < 1)
			w = fontSize * 0.55;
		m_mCharW.Set(key, w);
		return w;
	}

	//------------------------------------------------------------------------------------------------
	protected float MeasureTracked(string text, int fontSize, float tracking)
	{
		float total = 0;
		int len = text.Length();
		int i;
		for (i = 0; i < len; i++)
		{
			total = total + CharWidth(text.Substring(i, 1), fontSize);
			if (i < len - 1)
				total = total + tracking;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawTracked(MUI_RenderSurface surface, float x, float y, float h, string text, int fontSize, float tracking, Color color)
	{
		float cx = x;
		int len = text.Length();
		int i;
		for (i = 0; i < len; i++)
		{
			string ch = text.Substring(i, 1);
			float cw = CharWidth(ch, fontSize);
			if (ch != " ")
				surface.DrawText(cx, y, cw + 2, h, ch, fontSize, color, true, false, true, false, true);
			cx = cx + cw + tracking;
		}
	}
}
