//------------------------------------------------------------------------------------------------
//! The paint bay: the panel a helicopter pilot opens from the seat to change
//! the skin of the airframe they are flying. It is the pilot card grown large:
//! the same amber tab, bevelled body and side-view helicopter. The helicopter
//! on the left wears whichever livery is under the cursor, painted nose to tail,
//! and for a locked livery only as far as the pilot's rating has earned. The
//! tiles along the bottom are the liveries.
//!
//! Which helicopter is drawn, what it is called and what its factory paint is
//! come from the paint family of the airframe's channel (IA_HeliPaintFamily).
//!
//! The bay only draws and reports clicks. IA_HeliPaintMenu feeds it the state
//! and talks to the server.
//------------------------------------------------------------------------------------------------
class IA_HeliPaintBay : MUI_Surface
{
	static const float BAY_W = 960;
	protected static const float TAB_H = 18;
	protected static const float BODY_H = 322;
	protected static const float PAD = 18;
	protected static const float BEVEL = 4;
	protected static const float TAB_PAD_X = 12;

	protected static const float HERO_W = 468;
	protected static const float HERO_H = 170;
	protected static const float HERO_TOP = 16;
	protected static const float HERO_K = 3.4;
	protected static const float INFO_GAP = 26;

	protected static const float TILE_GAP = 10;
	protected static const float TILE_MAX_W = 223.5;
	protected static const float TILE_MIN_W = 108;
	protected static const int TILE_MAX = 8;

	protected static const int FONT_TAB = 10;
	protected static const int FONT_CAP = 9;
	protected static const int FONT_NAME = 28;
	protected static const int FONT_RATING = 22;
	protected static const int FONT_GOAL = 13;
	protected static const int FONT_PCT = 16;
	protected static const float TRACK_TAB = 1.5;
	protected static const float TRACK_CAP = 1.4;

	protected static const float PAINT_SPEED = 2.4;
	protected static const float ROTOR_SPEED = 11.0;
	protected static const float SCAN_PERIOD = 3.6;
	protected static const float MESSAGE_TIME = 3.2;
	protected static const int PIP_COUNT = 30;
	protected static const int SPARK_COUNT = 26;
	static const int NO_SKIN = -1;

	protected static const string TEXT_TAB = "PAINT BAY";
	protected static const string TEXT_AIRFRAME = "HELICOPTER";
	protected static const string TEXT_STOCK_NAME = "FACTORY PAINT";
	protected static const string TEXT_LIVERIES = "LIVERIES";
	protected static const string TEXT_OPEN_SLOT = "OPEN SLOT";
	protected static const string TEXT_RATING = "TRANSPORT RATING";
	protected static const string TEXT_STANDARD = "STANDARD ISSUE";
	protected static const string TEXT_NO_RATING = "NO RATING REQUIRED";
	protected static const string TEXT_LIVE = "ON AIRFRAME";
	protected static const string TEXT_PREVIEW = "PREVIEW";

	protected ref Color m_Bg;
	protected ref Color m_Tab;
	protected ref Color m_Tone;
	protected ref Color m_Gold;
	protected ref Color m_Green;
	protected ref Color m_Red;
	protected ref Color m_Muted;
	protected ref Color m_White;
	protected ref Color m_Black;
	protected ref Color m_Glass;
	protected ref Color m_Stock;

	protected ref IA_HeliArt m_Art;
	protected IA_HeliPaintFamily m_Family;
	protected bool m_bFamilySet;
	protected string m_sAirframe = TEXT_AIRFRAME;
	protected ref MUI_Row m_Row;
	protected ref array<ref IA_HeliPaintTile> m_aTiles = {};
	protected ref array<ref Color> m_aSkinColors = {};
	protected ref array<string> m_aSkinNames = {};
	protected ref ScriptInvoker m_OnPick;
	protected ref array<float> m_aPoly = {};

	// What the menu told us.
	protected bool m_bContextSet;
	protected bool m_bPaintable;
	protected int m_iChannel;
	protected int m_iWorn;
	protected int m_iRating = -1;
	protected int m_iPending = NO_SKIN;

	// The helicopter on the left.
	protected int m_iShown = NO_SKIN;
	protected int m_iShownTile;
	protected Color m_ColPaint;
	protected Color m_ColBase;
	protected float m_fFront;
	protected float m_fFrontTarget = 1;
	protected float m_fRotor;
	protected float m_fFlash;
	protected float m_fShine = -1;
	protected int m_iHovered = NO_SKIN;

	protected string m_sMessage;
	protected float m_fMessageT;
	protected bool m_bMessageBad;

	// Texts of the info column, rebuilt when something changes.
	protected bool m_bTextDirty = true;
	protected string m_sCapLivery;
	protected string m_sShownName;
	protected string m_sChip;
	protected Color m_ChipColor;
	protected bool m_bStockShown;
	protected string m_sRating;
	protected string m_sGoal;
	protected string m_sPct;
	protected string m_sFooter;
	protected Color m_FooterColor;
	protected string m_sPad;
	protected float m_fShownFrac;
	protected float m_fTabW;
	protected float m_fChipW;
	protected float m_fRatingW;
	protected float m_fPctW;
	protected float m_fPadW;
	protected float m_fTileW = TILE_MAX_W;

	protected ref array<float> m_aSparkX = {};
	protected ref array<float> m_aSparkY = {};
	protected ref array<float> m_aSparkVX = {};
	protected ref array<float> m_aSparkVY = {};
	protected ref array<float> m_aSparkLife = {};

	//------------------------------------------------------------------------------------------------
	void IA_HeliPaintBay()
	{
		m_Style.m_Layout = MUI_LayoutKind.Overlay;
		m_Style.m_WidthMode = MUI_SizeMode.Exact;
		m_Style.m_HeightMode = MUI_SizeMode.Exact;
		m_Style.m_fWidth = BAY_W;
		m_Style.m_fMinWidth = BAY_W;
		m_Style.m_fHeight = TAB_H + BODY_H;
		m_Style.m_fMinHeight = TAB_H + BODY_H;
		m_Style.m_fRadius = 0;
		m_Style.m_Fill = Color.FromInt(0);
		m_Style.SetPaddingTRBL(0, PAD, PAD, PAD);
		m_bBlurEnabled = true;
		m_fBlurIntensity = 0.90;

		m_Bg = Color.FromSRGBA(13, 20, 18, 232);
		m_Tab = Color.FromSRGBA(58, 42, 18, 224);
		m_Tone = Color.FromSRGBA(255, 184, 72, 255);
		m_Gold = Color.FromSRGBA(255, 214, 120, 255);
		m_Green = Color.FromSRGBA(94, 251, 131, 255);
		m_Red = Color.FromSRGBA(255, 112, 92, 255);
		m_Muted = Color.FromSRGBA(158, 168, 163, 255);
		m_White = Color.FromSRGBA(238, 242, 240, 255);
		m_Black = Color.FromSRGBA(0, 0, 0, 255);
		m_Glass = Color.FromSRGBA(9, 14, 13, 240);
		m_Stock = Color.FromSRGBA(78, 88, 60, 255);
		m_ColPaint = m_Stock;
		m_ColBase = m_Stock;
		m_ChipColor = m_Muted;
		m_FooterColor = m_Muted;

		m_Art = IA_HeliArt.Create(IA_HeliArt.ART_HUEY);
		m_OnPick = new ScriptInvoker();
	}

	//------------------------------------------------------------------------------------------------
	static IA_HeliPaintBay Create(notnull MUI_Runtime runtime)
	{
		ref IA_HeliPaintBay bay = new IA_HeliPaintBay();
		runtime.Adopt(bay);
		bay.SetName("paintBay");
		bay.SetAlign(0.5, 1.0);
		bay.SetIntro(0, 0.42, 22);
		bay.BuildTiles(runtime);
		return bay;
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyTheme(notnull MUI_ThemeData theme)
	{
		m_Style.m_Fill = Color.FromInt(0);
	}

	//------------------------------------------------------------------------------------------------
	//! Invoked with the skin id when the pilot picks a livery they may wear.
	ScriptInvoker GetOnPick()
	{
		return m_OnPick;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the tile of a skin, null when the bay does not list it
	IA_HeliPaintTile FindTile(int skinId)
	{
		foreach (IA_HeliPaintTile tile : m_aTiles)
		{
			if (tile.GetSkinId() == skinId)
				return tile;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! One tile for stock paint and one for every skin in the catalog.
	protected void BuildTiles(notnull MUI_Runtime runtime)
	{
		m_Row = runtime.CreateRow("liveries");
		m_Row.SetGap(TILE_GAP);
		m_Row.SetAlign(0, 1);
		AddChild(m_Row);

		AddTile(runtime, IA_HeliSkinCatalog.SKIN_NONE, TEXT_STOCK_NAME, m_Stock);

		array<ref IA_HeliSkinDef> defs = IA_HeliSkinCatalog.GetDefs();
		foreach (IA_HeliSkinDef def : defs)
		{
			if (m_aTiles.Count() >= TILE_MAX)
				break;

			ref Color colour = Color.FromSRGBA(def.m_iSwatchR, def.m_iSwatchG, def.m_iSwatchB, 255);
			AddTile(runtime, def.m_iId, def.m_sDisplayName, colour);
		}

		int count = m_aTiles.Count();
		float inner = BAY_W - PAD * 2;
		m_fTileW = (inner - TILE_GAP * (count - 1)) / count;
		if (m_fTileW > TILE_MAX_W)
			m_fTileW = TILE_MAX_W;
		if (m_fTileW < TILE_MIN_W)
			m_fTileW = TILE_MIN_W;

		int i;
		for (i = 0; i < count; i++)
		{
			m_aTiles[i].SetWidth(m_fTileW);
			m_aTiles[i].SetIntro(0.12 + 0.05 * i, 0.30, 14);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AddTile(notnull MUI_Runtime runtime, int skinId, string displayName, notnull Color colour)
	{
		string upper = displayName;
		upper.ToUpper();

		IA_HeliPaintTile tile = IA_HeliPaintTile.Create(runtime, this, skinId, m_aTiles.Count() + 1, upper, colour);
		m_aTiles.Insert(tile);
		m_aSkinColors.Insert(colour);
		m_aSkinNames.Insert(upper);
		m_Row.AddChild(tile);
	}

	//------------------------------------------------------------------------------------------------
	//! \param paintable false when this helicopter has no paint channel
	//! \param channel the airframe's paint channel; its family is the helicopter drawn and its number in the family is shown
	//! \param rating the pilot's global transport rating, negative while it is not known
	void SetContext(bool paintable, int channel, int wornSkin, int rating)
	{
		if (m_bContextSet && paintable == m_bPaintable && channel == m_iChannel && wornSkin == m_iWorn && rating == m_iRating)
			return;

		// Another airframe wearing another skin is not a repaint.
		bool repainted = m_iShown != NO_SKIN && wornSkin != m_iWorn && paintable && channel == m_iChannel;
		m_bContextSet = true;
		m_bPaintable = paintable;
		m_iChannel = channel;
		m_iWorn = wornSkin;
		m_iRating = rating;
		SetFamily(IA_HeliPaintChannels.GetFamily(channel));
		RefreshTiles();

		if (repainted)
			Celebrate();
	}

	//------------------------------------------------------------------------------------------------
	//! Draw another helicopter type: its silhouette, its name and its factory paint on the stock tile.
	//! \param family null for a helicopter without paint channels, which keeps the generic names
	protected void SetFamily(IA_HeliPaintFamily family)
	{
		if (m_bFamilySet && family == m_Family)
			return;

		m_bFamilySet = true;
		m_Family = family;

		// No key is the generic helicopter.
		string art;
		string stockName = TEXT_STOCK_NAME;
		m_sAirframe = TEXT_AIRFRAME;
		if (family)
		{
			art = family.m_sArt;
			m_sAirframe = family.m_sDisplayName;
			m_sAirframe.ToUpper();
			stockName = family.m_sStockPaint;
			stockName.ToUpper();
			m_Stock = Color.FromSRGBA(family.m_iStockR, family.m_iStockG, family.m_iStockB, 255);
		}

		m_Art = IA_HeliArt.Create(art);
		foreach (IA_HeliPaintTile tile : m_aTiles)
		{
			tile.SetAirframe(art, m_Stock);
		}

		// The first tile is the factory paint.
		if (!m_aTiles.IsEmpty())
		{
			m_aSkinColors[0] = m_Stock;
			m_aSkinNames[0] = stockName;
			m_aTiles[0].SetPaint(stockName, m_Stock);
		}

		// Nothing of the last helicopter stays on the board.
		m_iShown = NO_SKIN;
		m_ColPaint = m_Stock;
		m_ColBase = m_Stock;
		m_fFront = 1;
		m_bTextDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The catalog's thresholds changed; work the tiles out again.
	void Reload()
	{
		RefreshTiles();
	}

	//------------------------------------------------------------------------------------------------
	//! The skin the pilot asked for and the server has not answered yet; NO_SKIN (-1) for none.
	void SetPending(int skinId)
	{
		if (m_iPending == skinId)
			return;
		m_iPending = skinId;
		RefreshTiles();
	}

	//------------------------------------------------------------------------------------------------
	bool IsPending()
	{
		return m_iPending != NO_SKIN;
	}

	//------------------------------------------------------------------------------------------------
	//! A line in place of the footer for a few seconds.
	void ShowMessage(string text, bool bad)
	{
		m_sMessage = text;
		m_fMessageT = MESSAGE_TIME;
		m_bMessageBad = bad;
		m_bTextDirty = true;
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	//! The server refused a skin: its tile shakes and the reason is shown.
	void Refuse(int skinId, string reason)
	{
		IA_HeliPaintTile tile = FindTile(skinId);
		if (tile)
			tile.Shake();
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK_FAIL);
		ShowMessage(reason, true);
	}

	//------------------------------------------------------------------------------------------------
	//! The airframe took a new skin.
	protected void Celebrate()
	{
		m_fFlash = 1;
		m_fShine = 0;
		SpawnSparks();
		SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.TASK_SUCCEED);
		ShowMessage("PAINT APPLIED", false);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by a tile when it is clicked.
	void OnTilePicked(notnull IA_HeliPaintTile tile)
	{
		if (m_iPending != NO_SKIN)
			return;

		int state = tile.GetState();
		if (state == IA_HeliPaintTile.STATE_READY)
		{
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.CLICK);
			m_OnPick.Invoke(tile.GetSkinId());
			return;
		}

		if (state == IA_HeliPaintTile.STATE_WORN)
		{
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.FOCUS);
			ShowMessage("ALREADY ON THE AIRFRAME", false);
			return;
		}

		string reason = "THIS AIRFRAME HAS NO PAINT RIG";
		if (state == IA_HeliPaintTile.STATE_SYNCING)
		{
			reason = "RATING STILL SYNCING - TRY AGAIN IN A MOMENT";
		}
		else if (state == IA_HeliPaintTile.STATE_LOCKED)
		{
			IA_HeliSkinDef def = IA_HeliSkinCatalog.FindDef(tile.GetSkinId());
			if (def)
				reason = IA_PilotDropoffPayload.FormatNumber(def.m_iRequiredPoints - m_iRating) + " RATING TO GO";
		}
		Refuse(tile.GetSkinId(), reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the pilot may wear a skin, by what the server last told us.
	protected bool IsUnlocked(IA_HeliSkinDef def)
	{
		if (!def)
			return true;
		return m_iRating >= 0 && IA_HeliSkinCatalog.IsUnlocked(def, m_iRating);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshTiles()
	{
		foreach (IA_HeliPaintTile tile : m_aTiles)
		{
			int skinId = tile.GetSkinId();
			IA_HeliSkinDef def = IA_HeliSkinCatalog.FindDef(skinId);
			int state = IA_HeliPaintTile.STATE_READY;
			float progress = 1;
			string line = "READY";

			if (!m_bPaintable)
			{
				state = IA_HeliPaintTile.STATE_OFF;
				line = "NO PAINT RIG";
			}
			else if (skinId == m_iWorn)
			{
				state = IA_HeliPaintTile.STATE_WORN;
				line = "WORN";
			}
			else if (!IsUnlocked(def))
			{
				if (m_iRating < 0)
				{
					state = IA_HeliPaintTile.STATE_SYNCING;
					progress = 0;
					line = "SYNCING";
				}
				else
				{
					state = IA_HeliPaintTile.STATE_LOCKED;
					progress = m_iRating;
					progress = progress / def.m_iRequiredPoints;
					line = IA_PilotDropoffPayload.FormatNumber(def.m_iRequiredPoints);
				}
			}

			bool pending = skinId == m_iPending;
			if (pending)
				line = "PAINTING";
			tile.SetState(state, progress, line, pending);
		}

		m_bTextDirty = true;
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	//! The livery under the cursor, or with gamepad focus; NO_SKIN when neither.
	protected int FindPointedSkin()
	{
		foreach (IA_HeliPaintTile tile : m_aTiles)
		{
			if (tile.IsHover())
				return tile.GetSkinId();
		}
		foreach (IA_HeliPaintTile focused : m_aTiles)
		{
			if (focused.IsFocused())
				return focused.GetSkinId();
		}
		return NO_SKIN;
	}

	//------------------------------------------------------------------------------------------------
	//! Put a livery on the big helicopter; it is painted over whatever it shows now.
	protected void ShowSkin(int skinId)
	{
		int count = m_aTiles.Count();
		int index = 0;
		int i;
		for (i = 0; i < count; i++)
		{
			if (m_aTiles[i].GetSkinId() == skinId)
				index = i;
		}

		// Only a coat that was finished becomes the undercoat.
		if (m_fFront >= 0.999)
			m_ColBase = m_ColPaint;

		m_iShown = skinId;
		m_iShownTile = index;
		m_ColPaint = m_aSkinColors[index];
		m_fFront = 0;
		m_bTextDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnTick(float dt)
	{
		if (m_aTiles.IsEmpty())
			return;

		m_fRotor = m_fRotor + dt * ROTOR_SPEED;

		int pointed = FindPointedSkin();
		if (pointed != m_iHovered)
		{
			m_iHovered = pointed;
			if (pointed != NO_SKIN && m_iShown != NO_SKIN)
				SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_FE_BUTTON_HOVER);
		}

		int want = pointed;
		if (want == NO_SKIN)
			want = m_iWorn;
		if (want != m_iShown)
			ShowSkin(want);

		// A locked livery is painted only as far as the rating has earned.
		IA_HeliPaintTile tile = m_aTiles[m_iShownTile];
		m_fFrontTarget = 1;
		if (tile.GetState() == IA_HeliPaintTile.STATE_LOCKED)
			m_fFrontTarget = m_fShownFrac;
		else if (tile.GetState() == IA_HeliPaintTile.STATE_SYNCING)
			m_fFrontTarget = 0;

		if (m_fFront < m_fFrontTarget)
		{
			m_fFront = m_fFront + dt * PAINT_SPEED;
			if (m_fFront > m_fFrontTarget)
				m_fFront = m_fFrontTarget;
		}
		else if (m_fFront > m_fFrontTarget)
		{
			m_fFront = m_fFront - dt * PAINT_SPEED;
			if (m_fFront < m_fFrontTarget)
				m_fFront = m_fFrontTarget;
		}

		if (m_fFlash > 0)
			m_fFlash = MUI_Ease.Approach(m_fFlash, 0, dt, 3.2);
		if (m_fShine >= 0)
		{
			m_fShine = m_fShine + dt * 1.25;
			if (m_fShine > 1)
				m_fShine = -1;
		}

		if (m_fMessageT > 0)
		{
			m_fMessageT = m_fMessageT - dt;
			if (m_fMessageT <= 0)
				m_bTextDirty = true;
		}

		TickSparks(dt);
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	protected void RebuildTexts()
	{
		m_bTextDirty = false;
		if (m_aTiles.IsEmpty())
			return;

		int total = m_aTiles.Count();
		IA_HeliPaintTile tile = m_aTiles[m_iShownTile];
		int state = tile.GetState();
		IA_HeliSkinDef def = IA_HeliSkinCatalog.FindDef(m_iShown);
		bool pending = m_iPending == m_iShown && m_iPending != NO_SKIN;

		m_sCapLivery = string.Format("LIVERY %1 / %2", TwoDigits(m_iShownTile + 1), TwoDigits(total));
		m_sShownName = m_aSkinNames[m_iShownTile];
		m_bStockShown = !def;

		// Chip.
		m_sChip = "READY TO PAINT";
		m_ChipColor = m_Tone;
		if (pending)
		{
			m_sChip = "PAINTING";
		}
		else if (state == IA_HeliPaintTile.STATE_WORN)
		{
			m_sChip = TEXT_LIVE;
			m_ChipColor = m_Green;
		}
		else if (state == IA_HeliPaintTile.STATE_LOCKED)
		{
			m_sChip = "LOCKED";
			m_ChipColor = m_Muted;
		}
		else if (state == IA_HeliPaintTile.STATE_SYNCING)
		{
			m_sChip = "SYNCING";
			m_ChipColor = m_Muted;
		}
		else if (state == IA_HeliPaintTile.STATE_OFF)
		{
			m_sChip = "UNAVAILABLE";
			m_ChipColor = m_Muted;
		}
		m_fChipW = IA_TrackedText.Measure(m_Runtime, m_sChip, FONT_CAP, TRACK_CAP) + 16;

		// Rating against the threshold.
		m_sGoal = "";
		m_sPct = "";
		m_fShownFrac = 1;
		if (!def)
		{
			m_sRating = TEXT_NO_RATING;
		}
		else if (m_iRating < 0)
		{
			m_sRating = "- - -";
			m_sGoal = "/ " + IA_PilotDropoffPayload.FormatNumber(def.m_iRequiredPoints);
			m_fShownFrac = 0;
		}
		else
		{
			m_sRating = IA_PilotDropoffPayload.FormatNumber(m_iRating);
			m_sGoal = "/ " + IA_PilotDropoffPayload.FormatNumber(def.m_iRequiredPoints);
			int tenths = IA_PilotDropoffPayload.PercentTenths(m_iRating, def.m_iRequiredPoints);
			m_sPct = IA_PilotDropoffPayload.FormatPercent(tenths);
			m_fShownFrac = tenths;
			m_fShownFrac = m_fShownFrac / 1000;
		}

		int ratingFont = FONT_RATING;
		if (!def)
			ratingFont = FONT_PCT;
		m_fRatingW = IA_TrackedText.Width(m_Runtime, m_sRating, ratingFont);
		m_fPctW = IA_TrackedText.Width(m_Runtime, m_sPct, FONT_PCT);

		// Footer.
		m_FooterColor = m_Muted;
		if (m_fMessageT > 0)
		{
			m_sFooter = m_sMessage;
			m_FooterColor = m_Tone;
			if (m_bMessageBad)
				m_FooterColor = m_Red;
		}
		else if (state == IA_HeliPaintTile.STATE_OFF)
		{
			m_sFooter = "THIS AIRFRAME HAS NO PAINT RIG";
		}
		else if (state == IA_HeliPaintTile.STATE_WORN)
		{
			m_sFooter = "THIS IS THE PAINT YOU ARE FLYING";
		}
		else if (state == IA_HeliPaintTile.STATE_SYNCING)
		{
			m_sFooter = "SYNCING YOUR RATING WITH COMMAND";
		}
		else if (state == IA_HeliPaintTile.STATE_LOCKED)
		{
			m_sFooter = IA_PilotDropoffPayload.FormatNumber(def.m_iRequiredPoints - m_iRating) + " RATING TO GO - FLY TROOPS TO EARN IT";
		}
		else if (!def)
		{
			m_sFooter = "ALWAYS AVAILABLE - SELECT TO REPAINT";
		}
		else
		{
			m_sFooter = "UNLOCKED - SELECT TO REPAINT IN FLIGHT";
			m_FooterColor = m_Gold;
		}

		m_sPad = "";
		if (IA_HeliPaintChannels.IsChannel(m_iChannel))
			m_sPad = "PAD " + TwoDigits(IA_HeliPaintChannels.GetLocalChannel(m_iChannel));
		m_fPadW = IA_TrackedText.Measure(m_Runtime, m_sPad, FONT_CAP, TRACK_CAP);
		m_fTabW = IA_TrackedText.Measure(m_Runtime, TEXT_TAB, FONT_TAB, TRACK_TAB) + TAB_PAD_X * 2;
	}

	//------------------------------------------------------------------------------------------------
	protected static string TwoDigits(int value)
	{
		if (value < 10)
			return "0" + value.ToString();
		return value.ToString();
	}

	//------------------------------------------------------------------------------------------------
	override void SyncHostWidgets()
	{
		if (!m_bBlurEnabled || !IsVisible())
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
		m_wBlur.SetColor(m_Bg);
		FrameSlot.SetAnchorMin(m_wBlur, 0, 0);
		FrameSlot.SetAnchorMax(m_wBlur, 0, 0);
		FrameSlot.SetPos(m_wBlur, x, y + TAB_H);
		FrameSlot.SetSize(m_wBlur, BAY_W, BODY_H);
		m_wBlur.SetOpacity(op);
		m_wBlur.SetIntensity(m_fBlurIntensity * op);
		m_wBlur.SetSmoothBorder(4, 0, 4, 4);
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
		float op = GetDrawOpacity();
		if (op < 0.01)
			return;

		if (m_bTextDirty)
			RebuildTexts();

		float x = DrawX();
		float y = DrawY();
		float bodyY = y + TAB_H;
		float heroX = x + PAD;
		float heroY = bodyY + HERO_TOP;
		float infoX = heroX + HERO_W + INFO_GAP;
		float infoW = x + BAY_W - PAD - infoX;

		SyncHostWidgets();
		DrawBody(surface, x, bodyY, op);
		DrawTab(surface, x, y, op);
		if (m_aTiles.IsEmpty())
			return;

		DrawHero(surface, heroX, heroY, op);
		DrawInfo(surface, infoX, heroY, infoW, op);
		DrawStrip(surface, x, bodyY, op);
		DrawSparks(surface, op);
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawBody(MUI_RenderSurface surface, float x, float y, float op)
	{
		m_aPoly.Clear();
		m_aPoly.Insert(x);
		m_aPoly.Insert(y);
		m_aPoly.Insert(x + BAY_W);
		m_aPoly.Insert(y);
		m_aPoly.Insert(x + BAY_W);
		m_aPoly.Insert(y + BODY_H - BEVEL);
		m_aPoly.Insert(x + BAY_W - BEVEL);
		m_aPoly.Insert(y + BODY_H);
		m_aPoly.Insert(x + BEVEL);
		m_aPoly.Insert(y + BODY_H);
		m_aPoly.Insert(x);
		m_aPoly.Insert(y + BODY_H - BEVEL);

		surface.FillPolygon(m_aPoly, MUI_ColorUtil.Fade(m_Bg, op));
		if (m_fFlash > 0.02)
			surface.FillPolygon(m_aPoly, MUI_ColorUtil.Fade(m_Gold, op * m_fFlash * 0.045));

		Color edge = MUI_ColorUtil.Fade(m_Tone, op * 0.22);
		surface.DrawLine(x, y, x + BAY_W, y, edge, 1);
		surface.DrawLine(x, y, x, y + BODY_H - BEVEL, edge, 1);
		surface.DrawLine(x + BAY_W, y, x + BAY_W, y + BODY_H - BEVEL, edge, 1);

		// Top glow, brightest in the middle.
		float glow = 0.5 + 0.5 * MUI_Ease.Pulse(GetTime(), 0.5);
		int slices = 48;
		float sliceW = BAY_W / slices;
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
			surface.FillRect(x + sliceW * i, y, sliceW + 0.5, 1, MUI_ColorUtil.Fade(m_Tone, op * 0.6 * glow * a), 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawTab(MUI_RenderSurface surface, float x, float y, float op)
	{
		surface.FillRect(x, y, m_fTabW, TAB_H + 1, MUI_ColorUtil.Fade(m_Tab, op), 0);

		Color edge = MUI_ColorUtil.Fade(m_Tone, op * 0.22);
		surface.DrawLine(x, y, x + m_fTabW, y, edge, 1);
		surface.DrawLine(x, y, x, y + TAB_H, edge, 1);
		surface.DrawLine(x + m_fTabW, y, x + m_fTabW, y + TAB_H, edge, 1);
		IA_TrackedText.Draw(surface, m_Runtime, x + TAB_PAD_X, y, TAB_H, TEXT_TAB, FONT_TAB, TRACK_TAB, MUI_ColorUtil.Fade(m_Tone, op));
	}

	//------------------------------------------------------------------------------------------------
	//! The drawing board: the helicopter in the livery being looked at.
	protected void DrawHero(MUI_RenderSurface surface, float x, float y, float op)
	{
		float time = GetTime();
		surface.FillRect(x, y, HERO_W, HERO_H, MUI_ColorUtil.Fade(m_Black, op * 0.30), 0);
		surface.FillGradientV(x, y + HERO_H * 0.45, HERO_W, HERO_H * 0.55, MUI_ColorUtil.Fade(m_ColPaint, 0), MUI_ColorUtil.Fade(m_ColPaint, op * 0.035), 10);

		// Grid.
		Color grid = MUI_ColorUtil.Fade(m_Tone, op * 0.05);
		float g;
		for (g = 26; g < HERO_W; g = g + 26)
		{
			surface.FillRect(x + g, y, 1, HERO_H, grid, 0);
		}
		for (g = 26; g < HERO_H; g = g + 26)
		{
			surface.FillRect(x, y + g, HERO_W, 1, grid, 0);
		}

		// Scan line crossing the board.
		float scan = time / SCAN_PERIOD;
		scan = scan - Math.Floor(scan);
		float scanX = x + scan * HERO_W;
		surface.FillRect(scanX, y, 1.5, HERO_H, MUI_ColorUtil.Fade(m_Tone, op * 0.16), 0);
		int s;
		for (s = 1; s <= 5; s++)
		{
			float tail = scanX - s * 4;
			if (tail < x)
				continue;
			surface.FillRect(tail, y, 4, HERO_H, MUI_ColorUtil.Fade(m_Tone, op * 0.004 * (6 - s)), 0);
		}

		// Corner brackets.
		Color bracket = MUI_ColorUtil.Fade(m_Tone, op * 0.55);
		float b = 10;
		surface.DrawLine(x, y, x + b, y, bracket, 1);
		surface.DrawLine(x, y, x, y + b, bracket, 1);
		surface.DrawLine(x + HERO_W, y, x + HERO_W - b, y, bracket, 1);
		surface.DrawLine(x + HERO_W, y, x + HERO_W, y + b, bracket, 1);
		surface.DrawLine(x, y + HERO_H, x + b, y + HERO_H, bracket, 1);
		surface.DrawLine(x, y + HERO_H, x, y + HERO_H - b, bracket, 1);
		surface.DrawLine(x + HERO_W, y + HERO_H, x + HERO_W - b, y + HERO_H, bracket, 1);
		surface.DrawLine(x + HERO_W, y + HERO_H, x + HERO_W, y + HERO_H - b, bracket, 1);

		// Ruler along the bottom edge.
		Color tick = MUI_ColorUtil.Fade(m_Tone, op * 0.18);
		int n;
		for (n = 1; n < 39; n++)
		{
			float tickH = 3;
			if (n % 5 == 0)
				tickH = 6;
			surface.FillRect(x + n * 12, y + HERO_H - tickH, 1, tickH, tick, 0);
		}

		// Helicopter.
		float hx = x + (HERO_W - IA_HeliArt.W * HERO_K) * 0.5;
		float hy = y + (HERO_H - IA_HeliArt.H * HERO_K) * 0.5 + 6;
		bool partial = m_fFrontTarget < 0.999;

		m_Art.DrawShadow(surface, hx, hy, HERO_K, MUI_ColorUtil.Fade(m_Black, op * 0.42));
		m_Art.FillHull(surface, hx, hy, HERO_K, MUI_ColorUtil.Fade(m_ColBase, op));
		if (partial)
			m_Art.FillHull(surface, hx, hy, HERO_K, MUI_ColorUtil.Fade(m_ColPaint, op * 0.16));
		m_Art.FillHullPainted(surface, hx, hy, HERO_K, MUI_ColorUtil.Fade(m_ColPaint, op), m_fFront);
		m_Art.FillHullShade(surface, hx, hy, HERO_K, MUI_ColorUtil.Fade(m_White, op * 0.10), MUI_ColorUtil.Fade(m_Black, op * 0.24));
		if (m_fShine >= 0)
			m_Art.FillShine(surface, hx, hy, HERO_K, MUI_ColorUtil.Fade(m_White, op * 0.55), m_fShine);
		m_Art.FillGlass(surface, hx, hy, HERO_K, MUI_ColorUtil.Fade(m_Glass, op));
		m_Art.StrokeHull(surface, hx, hy, HERO_K, MUI_ColorUtil.Fade(m_Black, op * 0.45), 1);
		m_Art.DrawDetail(surface, hx, hy, HERO_K, MUI_ColorUtil.Fade(m_Black, op * 0.42), MUI_ColorUtil.Fade(m_Glass, op * 0.85));
		m_Art.DrawRig(surface, hx, hy, HERO_K, MUI_ColorUtil.Fade(m_White, op * 0.82), MUI_ColorUtil.Fade(m_White, op * 0.20), m_fRotor);

		if (m_fFront > 0.001 && m_fFront < 0.999)
		{
			float beat = 1.0;
			if (partial && m_fFront >= m_fFrontTarget)
				beat = 0.65 + 0.35 * MUI_Ease.Pulse(time, 0.9);
			m_Art.DrawFront(surface, hx, hy, HERO_K, MUI_ColorUtil.Fade(m_Tone, op * beat), m_fFront);
		}

		// Captions.
		IA_TrackedText.Draw(surface, m_Runtime, x + 10, y + 7, 12, m_sAirframe, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(m_Muted, op * 0.9));
		if (!m_sPad.IsEmpty())
			IA_TrackedText.Draw(surface, m_Runtime, x + HERO_W - 10 - m_fPadW, y + 7, 12, m_sPad, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(m_Muted, op * 0.9));

		bool live = m_iShown == m_iWorn && m_bPaintable;
		Color mark = m_Tone;
		string markText = TEXT_PREVIEW;
		if (live)
		{
			mark = m_Green;
			markText = TEXT_LIVE;
		}
		float pulse = 0.55 + 0.45 * MUI_Ease.Pulse(time, 0.7);
		surface.FillCircle(x + 14, y + HERO_H - 17, 3, MUI_ColorUtil.Fade(mark, op * pulse));
		IA_TrackedText.Draw(surface, m_Runtime, x + 23, y + HERO_H - 23, 12, markText, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(mark, op));
	}

	//------------------------------------------------------------------------------------------------
	//! Name, state and what the livery costs.
	protected void DrawInfo(MUI_RenderSurface surface, float x, float y, float w, float op)
	{
		float time = GetTime();
		float right = x + w;

		IA_TrackedText.Draw(surface, m_Runtime, x, y + 1, 12, m_sCapLivery, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(m_Muted, op));
		surface.DrawText(x - 1, y + 14, w, 38, m_sShownName, FONT_NAME, MUI_ColorUtil.Fade(m_White, op), true, false, true, false, true);

		// Chip.
		float chipOp = op;
		if (m_iPending != NO_SKIN && m_iPending == m_iShown)
			chipOp = op * (0.6 + 0.4 * MUI_Ease.Pulse(time, 1.6));
		DrawChip(surface, x, y + 58, m_fChipW, m_sChip, m_ChipColor, chipOp);

		// Rating.
		string caption = TEXT_RATING;
		if (m_bStockShown)
			caption = TEXT_STANDARD;
		IA_TrackedText.Draw(surface, m_Runtime, x, y + 88, 12, caption, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(m_Muted, op));

		if (m_bStockShown)
		{
			surface.DrawText(x, y + 102, w, 28, m_sRating, FONT_PCT, MUI_ColorUtil.Fade(m_White, op), true, false, true, false, true);
		}
		else
		{
			surface.DrawText(x - 1, y + 100, m_fRatingW + 6, 30, m_sRating, FONT_RATING, MUI_ColorUtil.Fade(m_Tone, op), true, false, true, false, true);
			surface.DrawText(x + m_fRatingW + 8, y + 106, 160, 22, m_sGoal, FONT_GOAL, MUI_ColorUtil.Fade(m_Muted, op), true, false, true, false, true);
			surface.DrawText(right - m_fPctW - 2, y + 102, m_fPctW + 6, 28, m_sPct, FONT_PCT, MUI_ColorUtil.Fade(m_White, op), true, false, true, false, true);
		}

		DrawPips(surface, x, y + 136, w, op);
		IA_TrackedText.Draw(surface, m_Runtime, x, y + 154, 14, m_sFooter, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(m_FooterColor, op));
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawChip(MUI_RenderSurface surface, float x, float y, float w, string text, Color tone, float op)
	{
		surface.FillRect(x, y, w, 17, MUI_ColorUtil.Fade(tone, op * 0.16), 0);
		surface.StrokeRect(x, y, w, 17, MUI_ColorUtil.Fade(tone, op * 0.5), 1, 0);
		IA_TrackedText.Draw(surface, m_Runtime, x + 8, y, 17, text, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(tone, op));
	}

	//------------------------------------------------------------------------------------------------
	//! The rating as slanted pips, the pilot card's progress bar at full width.
	protected void DrawPips(MUI_RenderSurface surface, float x, float y, float w, float op)
	{
		float step = w / PIP_COUNT;
		float pipW = step - 3.2;
		float filled = m_fShownFrac * PIP_COUNT;
		Color lit = m_Tone;
		if (m_fShownFrac >= 1)
			lit = m_Green;
		if (m_bStockShown)
			lit = m_Muted;

		int i;
		for (i = 0; i < PIP_COUNT; i++)
		{
			float px = x + step * i;
			m_aPoly.Clear();
			m_aPoly.Insert(px + 3.5);
			m_aPoly.Insert(y);
			m_aPoly.Insert(px + 3.5 + pipW);
			m_aPoly.Insert(y);
			m_aPoly.Insert(px + pipW);
			m_aPoly.Insert(y + 9);
			m_aPoly.Insert(px);
			m_aPoly.Insert(y + 9);

			float a = 0.14;
			Color tone = m_Tone;
			if (i < filled)
			{
				a = 0.92;
				tone = lit;
			}
			surface.FillPolygon(m_aPoly, MUI_ColorUtil.Fade(tone, op * a));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Caption of the tile strip and the slots no livery fills yet.
	protected void DrawStrip(MUI_RenderSurface surface, float x, float bodyY, float op)
	{
		float top = bodyY + BODY_H - PAD - IA_HeliPaintTile.TILE_H;
		float left = x + PAD;
		float capY = top - 17;
		IA_TrackedText.Draw(surface, m_Runtime, left, capY, 12, TEXT_LIVERIES, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(m_Muted, op));
		float capW = IA_TrackedText.Measure(m_Runtime, TEXT_LIVERIES, FONT_CAP, TRACK_CAP);
		surface.FillRect(left + capW + 10, capY + 6, BAY_W - PAD * 2 - capW - 10, 1, MUI_ColorUtil.Fade(m_Tone, op * 0.12), 0);

		float slotX = left + m_aTiles.Count() * (m_fTileW + TILE_GAP);
		float limit = x + BAY_W - PAD + 0.5;
		float textW = IA_TrackedText.Measure(m_Runtime, TEXT_OPEN_SLOT, FONT_CAP, TRACK_CAP);
		Color line = MUI_ColorUtil.Fade(m_Tone, op * 0.10);
		Color text = MUI_ColorUtil.Fade(m_Muted, op * 0.38);
		while (slotX + m_fTileW <= limit)
		{
			surface.StrokeRect(slotX, top, m_fTileW, IA_HeliPaintTile.TILE_H, line, 1, 0);
			surface.DrawLine(slotX + 8, top + IA_HeliPaintTile.TILE_H - 8, slotX + 30, top + 8, line, 1);
			surface.DrawLine(slotX + m_fTileW - 30, top + IA_HeliPaintTile.TILE_H - 8, slotX + m_fTileW - 8, top + 8, line, 1);
			IA_TrackedText.Draw(surface, m_Runtime, slotX + (m_fTileW - textW) * 0.5, top, IA_HeliPaintTile.TILE_H, TEXT_OPEN_SLOT, FONT_CAP, TRACK_CAP, text);
			slotX = slotX + m_fTileW + TILE_GAP;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sparks off the hull when a new skin goes on.
	protected void SpawnSparks()
	{
		m_aSparkX.Clear();
		m_aSparkY.Clear();
		m_aSparkVX.Clear();
		m_aSparkVY.Clear();
		m_aSparkLife.Clear();

		float spanW = IA_HeliArt.W * HERO_K;
		float baseX = PAD + (HERO_W - spanW) * 0.5;
		float baseY = TAB_H + HERO_TOP + HERO_H * 0.5;
		int i;
		for (i = 0; i < SPARK_COUNT; i++)
		{
			m_aSparkX.Insert(baseX + Math.RandomFloat(0.05, 0.92) * spanW);
			m_aSparkY.Insert(baseY + Math.RandomFloat(-22, 26));
			m_aSparkVX.Insert(Math.RandomFloat(-70, 70));
			m_aSparkVY.Insert(Math.RandomFloat(-130, -40));
			m_aSparkLife.Insert(Math.RandomFloat(0.6, 1.0));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TickSparks(float dt)
	{
		int count = m_aSparkLife.Count();
		if (count == 0)
			return;

		bool alive = false;
		int i;
		for (i = 0; i < count; i++)
		{
			if (m_aSparkLife[i] <= 0)
				continue;
			m_aSparkLife[i] = m_aSparkLife[i] - dt * 1.05;
			m_aSparkX[i] = m_aSparkX[i] + m_aSparkVX[i] * dt;
			m_aSparkY[i] = m_aSparkY[i] + m_aSparkVY[i] * dt;
			m_aSparkVY[i] = m_aSparkVY[i] + 150 * dt;
			if (m_aSparkLife[i] > 0)
				alive = true;
		}

		if (!alive)
			m_aSparkLife.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawSparks(MUI_RenderSurface surface, float op)
	{
		int count = m_aSparkLife.Count();
		if (count == 0)
			return;

		float x = DrawX();
		float y = DrawY();
		int i;
		for (i = 0; i < count; i++)
		{
			float life = m_aSparkLife[i];
			if (life <= 0)
				continue;
			Color tone = m_Gold;
			if (i % 3 == 0)
				tone = m_White;
			float size = 1.5 + life * 2.0;
			surface.FillRect(x + m_aSparkX[i], y + m_aSparkY[i], size, size, MUI_ColorUtil.Fade(tone, op * MUI_Ease.Clamp01(life * 1.4)), 0);
		}
	}
}
