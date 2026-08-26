//------------------------------------------------------------------------------------------------
//! Game Master map workstation. Places the same IA_AreaMarker prefabs as the
//! vanilla editor, through IA_GmDirector. Native MapWidget is owned by MHJ_MapHost.
//! Map click only sets a location. Place / QRF buttons commit it.
//! Types, destination, QRF, and Live/Staging lists stay on the rail — no dropdowns or scroll.
//------------------------------------------------------------------------------------------------
class IA_GmDirectorMenu : MUI_MenuBase
{
	protected static const int CHIP_TYPE = 0;
	protected static const int CHIP_BUCKET = 1;
	protected static const int CHIP_QRF = 2;

	protected ref MHJ_MapHost m_Map;
	protected ref MHJ_MapPicker m_Picker;
	protected ref MUI_Label m_LiveList;
	protected ref MUI_Label m_StagingList;
	protected ref MUI_Label m_CoordLabel;
	protected ref MUI_NumericField m_RadiusField;
	protected ref MUI_TextField m_NameField;
	protected ref MUI_Button m_PlaceBtn;
	protected ref MUI_Button m_QrfBtn;
	protected ref array<ref MUI_Button> m_TypeBtns;
	protected ref array<ref MUI_Button> m_BucketBtns;
	protected ref array<ref MUI_Button> m_QrfBtns;
	protected ref array<ref IA_GmDirectorChipBind> m_Binds;
	protected int m_iType;
	protected int m_iBucket;
	protected int m_iQrf;
	protected bool m_bHasUserPoint;
	protected bool m_bClosingMap;
	protected ref array<ref MapItem> m_SitePips;
	protected string m_sPipKey;
	protected float m_fPipAge;

	//------------------------------------------------------------------------------------------------
	override string GetMUILogTag()
	{
		return "IA_GmDirectorMenu";
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		if (!IsMUIOpen())
			return;

		m_bHasUserPoint = false;
		m_Map = new MHJ_MapHost();
		if (m_Map.Open(m_wRoot, GetRuntime()))
		{
			if (m_Picker)
				m_Picker.SetMapHost(m_Map);
		}
		else
		{
			m_Map = null;
		}

		RefreshLists();
		UpdatePlaceButtons();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);
		if (m_Map)
			m_Map.Tick(m_Picker);

		m_fPipAge = m_fPipAge + tDelta;
		if (m_fPipAge >= 0.4)
		{
			m_fPipAge = 0;
			RefreshLists();
		}

		MUI_Runtime runtime = GetRuntime();
		if (!runtime)
			return;

		if (m_Picker && m_Picker.IsFocused())
			runtime.SetPromptText("<action name='MenuSelect' scale='1.35'/>  Set location", "<action name='MenuBack' scale='1.35'/>  Back");
		else
			runtime.SetPromptText("<action name='MenuSelect' scale='1.35'/>  Select", "<action name='MenuBack' scale='1.35'/>  Back");
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		if (!m_bClosingMap)
		{
			m_bClosingMap = true;
			RecycleSitePips();
			if (m_Picker)
				m_Picker.SetMapHost(null);
			if (m_Map)
			{
				m_Map.Close();
				m_Map = null;
			}
		}
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	override void BuildUI(notnull MUI_Runtime runtime)
	{
		m_TypeBtns = new array<ref MUI_Button>();
		m_BucketBtns = new array<ref MUI_Button>();
		m_QrfBtns = new array<ref MUI_Button>();
		m_Binds = new array<ref IA_GmDirectorChipBind>();
		m_iType = 0;
		m_iBucket = 0;
		m_iQrf = 0;

		ref MUI_Panel overlay = runtime.CreatePanel("overlay");
		overlay.MakeOverlay();
		overlay.GetStyle().m_Fill = Color.FromInt(0);

		ref MUI_FxBackdrop fx = runtime.CreateFxBackdrop("fx");

		ref MUI_Row split = runtime.CreateRow("split");
		split.SetFillWidth();
		split.SetFillHeight();
		split.SetGap(12);
		split.SetPadding(16);
		split.SetIntro(0.06, 0.55, 46);

		ref MUI_Card card = runtime.CreateCard("card");
		card.SetWidth(540);
		card.SetFillHeight();
		card.SetPadding(16);
		card.SetPaddingTRBL(16, 16, 16, 16);
		card.SetGap(6);
		card.SetIntro(0.16, 0.4, 18);

		ref MUI_LiveHeader liveHeader = runtime.CreateLiveHeader("GM DIRECTOR", "liveHeader");
		liveHeader.SetKicker("STAGING DESK");
		liveHeader.SetIntro(0.22, 0.4, 18);

		ref MUI_Label subtitle = runtime.CreateLabel("Click the map, then Place. Name is optional.", "subtitle");
		subtitle.SetFontSize(runtime.GetTheme().FONT_SMALL);
		subtitle.SetMuted(true);

		// Stack in the card. A row of Fill-width columns measures each pane as the
		// full rail width, so Staging paints over the map and a FillHeight lists
		// row pushes every chip and action below the screen.
		ref MUI_Panel sites = runtime.CreatePanel("sites");
		sites.GetStyle().m_Fill = Color.FromInt(0);
		sites.SetGap(2);

		ref MUI_Label liveHdr = runtime.CreateLabel("LIVE", "liveHdr");
		liveHdr.SetFontSize(runtime.GetTheme().FONT_SMALL);
		liveHdr.SetBold(true);
		m_LiveList = runtime.CreateLabel("(empty)", "liveList");
		m_LiveList.SetFontSize(runtime.GetTheme().FONT_SMALL);

		ref MUI_Label stagingHdr = runtime.CreateLabel("STAGING", "stagingHdr");
		stagingHdr.SetFontSize(runtime.GetTheme().FONT_SMALL);
		stagingHdr.SetBold(true);
		m_StagingList = runtime.CreateLabel("(empty)", "stagingList");
		m_StagingList.SetFontSize(runtime.GetTheme().FONT_SMALL);

		sites.AddChild(liveHdr);
		sites.AddChild(m_LiveList);
		sites.AddChild(stagingHdr);
		sites.AddChild(m_StagingList);

		ref MUI_Hairline placeLine = runtime.CreateHairline("placeLine");
		ref MUI_Label placeHdr = runtime.CreateLabel("PLACE OBJECTIVE", "placeHdr");
		placeHdr.SetFontSize(runtime.GetTheme().FONT_SMALL);
		placeHdr.SetMuted(true);

		ref MUI_Row typeRow1 = runtime.CreateRow("typeRow1");
		typeRow1.SetGap(6);
		AddChip(runtime, typeRow1, m_TypeBtns, "Town", CHIP_TYPE, 0);
		AddChip(runtime, typeRow1, m_TypeBtns, "City", CHIP_TYPE, 1);
		AddChip(runtime, typeRow1, m_TypeBtns, "Property", CHIP_TYPE, 2);
		AddChip(runtime, typeRow1, m_TypeBtns, "Military", CHIP_TYPE, 3);
		AddChip(runtime, typeRow1, m_TypeBtns, "Small mil", CHIP_TYPE, 4);

		ref MUI_Row typeRow2 = runtime.CreateRow("typeRow2");
		typeRow2.SetGap(6);
		AddChip(runtime, typeRow2, m_TypeBtns, "Docks", CHIP_TYPE, 5);
		AddChip(runtime, typeRow2, m_TypeBtns, "Radio", CHIP_TYPE, 6);
		AddChip(runtime, typeRow2, m_TypeBtns, "Mortar", CHIP_TYPE, 7);
		AddChip(runtime, typeRow2, m_TypeBtns, "Defend", CHIP_TYPE, 8);
		AddChip(runtime, typeRow2, m_TypeBtns, "HVT", CHIP_TYPE, 9);

		m_NameField = runtime.CreateTextField("Objective name", "objName");
		m_NameField.SetHeight(52);
		m_NameField.SetMinHeight(52);

		ref MUI_Label destLbl = runtime.CreateLabel("Staging waits for Activate. Live adds to the current AO without replacing existing sites.", "destLbl");
		destLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);
		destLbl.SetMuted(true);

		ref MUI_Row bucketRow = runtime.CreateRow("bucketRow");
		bucketRow.SetGap(6);
		AddChip(runtime, bucketRow, m_BucketBtns, "Staging", CHIP_BUCKET, 0);
		AddChip(runtime, bucketRow, m_BucketBtns, "Live", CHIP_BUCKET, 1);

		m_RadiusField = runtime.CreateNumericField("Radius (0 = type default)", "radius");
		m_RadiusField.SetRange(0, 400);
		m_RadiusField.SetStep(5);
		m_RadiusField.SetDecimals(0);
		m_RadiusField.SetValue(0);
		m_RadiusField.SetHeight(52);
		m_RadiusField.SetMinHeight(52);

		m_CoordLabel = runtime.CreateLabel("No location yet. Click the map.", "coords");
		m_CoordLabel.SetFontSize(runtime.GetTheme().FONT_SMALL);
		m_CoordLabel.SetMuted(true);

		ref MUI_Row placeRow = runtime.CreateRow("placeRow");
		placeRow.SetGap(8);
		m_PlaceBtn = runtime.CreateButton("Place objective", "place");
		m_PlaceBtn.MakeAccent();
		StyleDirectorChip(m_PlaceBtn);
		m_PlaceBtn.SetEnabled(false);
		m_PlaceBtn.GetOnClicked().Insert(OnPlaceObjective);
		placeRow.AddChild(m_PlaceBtn);

		ref MUI_Hairline qrfLine = runtime.CreateHairline("qrfLine");
		ref MUI_Label qrfHdr = runtime.CreateLabel("QRF AT LOCATION", "qrfHdr");
		qrfHdr.SetFontSize(runtime.GetTheme().FONT_SMALL);
		qrfHdr.SetMuted(true);

		ref MUI_Row qrfTypeRow = runtime.CreateRow("qrfTypeRow");
		qrfTypeRow.SetGap(6);
		AddChip(runtime, qrfTypeRow, m_QrfBtns, "Infantry", CHIP_QRF, 0);
		AddChip(runtime, qrfTypeRow, m_QrfBtns, "Motor", CHIP_QRF, 1);
		AddChip(runtime, qrfTypeRow, m_QrfBtns, "Mech", CHIP_QRF, 2);
		AddChip(runtime, qrfTypeRow, m_QrfBtns, "Armor", CHIP_QRF, 3);
		AddChip(runtime, qrfTypeRow, m_QrfBtns, "Air", CHIP_QRF, 4);

		ref MUI_Row qrfRow = runtime.CreateRow("qrfRow");
		qrfRow.SetGap(8);
		m_QrfBtn = runtime.CreateButton("Spawn QRF here", "qrf");
		StyleDirectorChip(m_QrfBtn);
		m_QrfBtn.SetEnabled(false);
		m_QrfBtn.GetOnClicked().Insert(OnSpawnQrf);
		qrfRow.AddChild(m_QrfBtn);

		m_Picker = new MHJ_MapPicker();
		runtime.Adopt(m_Picker);
		m_Picker.SetName("picker");
		m_Picker.InitWorld();
		m_Picker.GetOnChanged().Insert(OnPickerChanged);
		m_Picker.SetFillWidth();
		m_Picker.SetFillHeight();
		m_Picker.SetGrow(1);

		ref MUI_Row actions = runtime.CreateRow("actions");
		actions.SetGap(8);
		ref MUI_Button activateBtn = runtime.CreateButton("Activate Staging", "activate");
		StyleDirectorChip(activateBtn);
		activateBtn.GetOnClicked().Insert(OnActivateStaging);
		ref MUI_Button completeBtn = runtime.CreateButton("Complete Live", "complete");
		completeBtn.MakeDanger();
		StyleDirectorChip(completeBtn);
		completeBtn.GetOnClicked().Insert(OnCompleteLive);
		actions.AddChild(activateBtn);
		actions.AddChild(completeBtn);

		ref MUI_Row actions2 = runtime.CreateRow("actions2");
		actions2.SetGap(8);
		ref MUI_Button sideBtn = runtime.CreateButton("Start assassination", "side");
		StyleDirectorChip(sideBtn);
		sideBtn.GetOnClicked().Insert(OnStartSide);
		ref MUI_Button closeBtn = runtime.CreateButton("Close", "close");
		StyleDirectorChip(closeBtn);
		closeBtn.GetOnClicked().Insert(OnMUIBack);
		actions2.AddChild(sideBtn);
		actions2.AddChild(closeBtn);

		RefreshChipLooks();

		card.AddChild(liveHeader);
		card.AddChild(subtitle);
		card.AddChild(sites);
		card.AddChild(placeLine);
		card.AddChild(placeHdr);
		card.AddChild(typeRow1);
		card.AddChild(typeRow2);
		card.AddChild(m_NameField);
		card.AddChild(destLbl);
		card.AddChild(bucketRow);
		card.AddChild(m_RadiusField);
		card.AddChild(m_CoordLabel);
		card.AddChild(placeRow);
		card.AddChild(qrfLine);
		card.AddChild(qrfHdr);
		card.AddChild(qrfTypeRow);
		card.AddChild(qrfRow);
		card.AddChild(actions);
		card.AddChild(actions2);

		split.AddChild(card);
		split.AddChild(m_Picker);
		overlay.AddChild(fx);
		overlay.AddChild(split);
		runtime.SetRoot(overlay);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddChip(notnull MUI_Runtime runtime, notnull MUI_Row row, notnull array<ref MUI_Button> store, string text, int kind, int index)
	{
		ref MUI_Button b = runtime.CreateButton(text, text);
		StyleDirectorChip(b);
		ref IA_GmDirectorChipBind bind = new IA_GmDirectorChipBind();
		bind.Init(this, kind, index);
		b.GetOnClicked().Insert(bind.OnClicked);
		m_Binds.Insert(bind);
		store.Insert(b);
		row.AddChild(b);
	}

	//------------------------------------------------------------------------------------------------
	void OnChipPicked(int kind, int index)
	{
		if (kind == CHIP_TYPE)
			m_iType = index;
		else if (kind == CHIP_BUCKET)
			m_iBucket = index;
		else if (kind == CHIP_QRF)
			m_iQrf = index;
		RefreshChipLooks();
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshChipLooks()
	{
		ApplyChipSelection(m_TypeBtns, m_iType);
		ApplyChipSelection(m_BucketBtns, m_iBucket);
		ApplyChipSelection(m_QrfBtns, m_iQrf);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyChipSelection(array<ref MUI_Button> buttons, int selected)
	{
		if (!buttons)
			return;

		int i;
		int n = buttons.Count();
		for (i = 0; i < n; i++)
		{
			MUI_Button b = buttons[i];
			if (!b)
				continue;
			if (i == selected)
				b.MakeAccent();
			else
				b.MakeDefault();
			StyleDirectorChip(b);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void StyleDirectorChip(notnull MUI_Button b)
	{
		b.SetHeight(28);
		b.SetMinHeight(28);
		b.SetMinWidth(52);
		b.GetStyle().m_fRadius = 6;
		int fontSize = MUI_Theme.FONT_SMALL;
		MUI_ThemeData theme = b.GetTheme();
		if (theme)
			fontSize = theme.FONT_SMALL;
		b.GetStyle().m_iFontSize = fontSize;
		b.InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPickerChanged()
	{
		m_bHasUserPoint = true;
		UpdatePlaceButtons();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdatePlaceButtons()
	{
		string coordText = "No location yet. Click the map.";
		if (m_bHasUserPoint && m_Picker)
			coordText = string.Format("Location  %1  %2", m_Picker.GetDropX(), m_Picker.GetDropZ());
		if (m_CoordLabel)
			m_CoordLabel.SetText(coordText);

		if (m_PlaceBtn)
		{
			m_PlaceBtn.SetEnabled(m_bHasUserPoint);
			if (m_bHasUserPoint)
				m_PlaceBtn.SetText("Place objective");
			else
				m_PlaceBtn.SetText("Click map first");
		}

		if (m_QrfBtn)
		{
			m_QrfBtn.SetEnabled(m_bHasUserPoint);
			if (m_bHasUserPoint)
				m_QrfBtn.SetText("Spawn QRF here");
			else
				m_QrfBtn.SetText("Click map first");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected IA_AreaType SelectedAreaType()
	{
		if (m_iType == 1)
			return IA_AreaType.City;
		if (m_iType == 2)
			return IA_AreaType.Property;
		if (m_iType == 3)
			return IA_AreaType.Military;
		if (m_iType == 4)
			return IA_AreaType.SmallMilitary;
		if (m_iType == 5)
			return IA_AreaType.Docks;
		if (m_iType == 6)
			return IA_AreaType.RadioTower;
		if (m_iType == 7)
			return IA_AreaType.MortarPit;
		if (m_iType == 8)
			return IA_AreaType.DefendObjective;
		if (m_iType == 9)
			return IA_AreaType.Assassination;
		return IA_AreaType.Town;
	}

	//------------------------------------------------------------------------------------------------
	protected IA_GmBucket SelectedBucket()
	{
		if (m_iBucket == 1)
			return IA_GmBucket.Live;
		return IA_GmBucket.Staging;
	}

	//------------------------------------------------------------------------------------------------
	protected IA_QRFType SelectedQrfType()
	{
		if (m_iQrf == 1)
			return IA_QRFType.Motorized;
		if (m_iQrf == 2)
			return IA_QRFType.Mechanized;
		if (m_iQrf == 3)
			return IA_QRFType.Armoured;
		if (m_iQrf == 4)
			return IA_QRFType.Airborne;
		return IA_QRFType.Infantry;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPlaceObjective()
	{
		if (!m_bHasUserPoint)
			return;
		if (!m_Picker || !m_Picker.HasDrop())
			return;

		float radius = 0;
		if (m_RadiusField)
			radius = m_RadiusField.GetValue();

		IA_AreaType areaType = SelectedAreaType();
		IA_GmBucket bucket = SelectedBucket();
		float dropX = m_Picker.GetDropX();
		float dropZ = m_Picker.GetDropZ();
		if (radius <= 0)
			radius = IA_GmDirector.DefaultRadiusForType(areaType);

		string name = "";
		if (m_NameField)
			name = m_NameField.GetText();

		string pinName = name;
		if (pinName.IsEmpty())
			pinName = IA_GmDirector.AreaTypeToString(areaType);

		IA_GmDirector dir = IA_GmDirector.GetInstance();
		dir.RememberPlacedSite(areaType, dropX, dropZ, dir.GetGroupIdForBucket(bucket), radius, pinName);

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.IA_AskGmPlaceSite(areaType, dropX, dropZ, bucket, name, radius);

		RefreshLists();
		GetGame().GetCallqueue().CallLater(this.RefreshLists, 250, false);
		GetGame().GetCallqueue().CallLater(this.RefreshLists, 800, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSpawnQrf()
	{
		if (!m_bHasUserPoint)
			return;
		if (!m_Picker || !m_Picker.HasDrop())
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.IA_AskForceQRFAt(SelectedQrfType(), m_Picker.GetDropX(), m_Picker.GetDropZ());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnActivateStaging()
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.IA_AskGmActivateStaging();
		GetGame().GetCallqueue().CallLater(this.RefreshLists, 400, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCompleteLive()
	{
		IA_MissionInitializer.ForceCompleteZone();
		GetGame().GetCallqueue().CallLater(this.RefreshLists, 400, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStartSide()
	{
		float x = 0;
		float z = 0;
		if (m_bHasUserPoint && m_Picker && m_Picker.HasDrop())
		{
			x = m_Picker.GetDropX();
			z = m_Picker.GetDropZ();
		}
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.IA_AskGmStartSideAt(x, z);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshLists()
	{
		IA_GmDirector dir = IA_GmDirector.GetInstance();
		int liveId = dir.GetLiveGroupId();
		int stagingId = dir.GetStagingGroupId();
		ref array<IA_AreaMarker> live = dir.CollectGroupMarkers(liveId);
		ref array<IA_AreaMarker> staging = dir.CollectGroupMarkers(stagingId);
		if (m_LiveList)
			m_LiveList.SetText(FormatSiteList(live, dir.CollectKnownSites(liveId)));
		if (m_StagingList)
			m_StagingList.SetText(FormatSiteList(staging, dir.CollectKnownSites(stagingId)));
		RefreshSitePips();
	}

	//------------------------------------------------------------------------------------------------
	protected string FormatSiteList(array<IA_AreaMarker> markers, array<ref IA_GmSiteRecord> known)
	{
		string text = FormatGroupList(markers);
		if (text != "(empty)")
			return text;

		if (!known || known.IsEmpty())
			return "(empty)";

		text = "";
		int i;
		int count = known.Count();
		for (i = 0; i < count; i++)
		{
			IA_GmSiteRecord rec = known[i];
			if (!rec)
				continue;
			if (!text.IsEmpty())
				text = text + "\n";
			if (rec.m_sName.IsEmpty())
				text = text + IA_GmDirector.AreaTypeToString(rec.m_iType);
			else
				text = text + rec.m_sName;
		}
		if (text.IsEmpty())
			return "(empty)";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshSitePips()
	{
		if (!m_Map || !m_Map.IsLive())
			return;

		SCR_MapEntity mapEnt = SCR_MapEntity.GetMapInstance();
		if (!mapEnt)
			return;

		IA_GmDirector dir = IA_GmDirector.GetInstance();
		int liveId = dir.GetLiveGroupId();
		int stagingId = dir.GetStagingGroupId();
		ref array<ref IA_GmSiteRecord> pins = new array<ref IA_GmSiteRecord>();
		AppendMarkerPins(pins, dir.CollectGroupMarkers(liveId));
		AppendMarkerPins(pins, dir.CollectGroupMarkers(stagingId));
		AppendKnownPins(pins, dir.CollectKnownSites(liveId));
		AppendKnownPins(pins, dir.CollectKnownSites(stagingId));

		string key = BuildPipKey(pins);
		if (key == m_sPipKey && m_SitePips && !m_SitePips.IsEmpty())
			return;

		RecycleSitePips();
		m_sPipKey = key;

		int i;
		int count = pins.Count();
		for (i = 0; i < count; i++)
		{
			IA_GmSiteRecord rec = pins[i];
			if (!rec)
				continue;

			bool isLive = false;
			if (liveId >= 0 && rec.m_iGroupId == liveId)
				isLive = true;
			CreateSitePip(mapEnt, rec, isLive);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AppendMarkerPins(notnull array<ref IA_GmSiteRecord> pins, array<IA_AreaMarker> markers)
	{
		if (!markers)
			return;

		int i;
		int count = markers.Count();
		for (i = 0; i < count; i++)
		{
			IA_AreaMarker marker = markers[i];
			if (!marker)
				continue;

			vector origin = marker.GetOrigin();
			if (PinAlreadyListed(pins, origin[0], origin[2]))
				continue;

			ref IA_GmSiteRecord rec = new IA_GmSiteRecord();
			rec.m_iType = marker.GetAreaType();
			rec.m_iGroupId = marker.m_areaGroup;
			rec.m_fX = origin[0];
			rec.m_fZ = origin[2];
			rec.m_fRadius = marker.GetRadius();
			rec.m_sName = marker.GetAreaName();
			pins.Insert(rec);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AppendKnownPins(notnull array<ref IA_GmSiteRecord> pins, array<ref IA_GmSiteRecord> known)
	{
		if (!known)
			return;

		int i;
		int count = known.Count();
		for (i = 0; i < count; i++)
		{
			IA_GmSiteRecord rec = known[i];
			if (!rec)
				continue;
			if (PinAlreadyListed(pins, rec.m_fX, rec.m_fZ))
				continue;
			pins.Insert(rec);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool PinAlreadyListed(notnull array<ref IA_GmSiteRecord> pins, float x, float z)
	{
		int i;
		int count = pins.Count();
		for (i = 0; i < count; i++)
		{
			IA_GmSiteRecord rec = pins[i];
			if (!rec)
				continue;
			float dx = rec.m_fX - x;
			float dz = rec.m_fZ - z;
			if ((dx * dx) + (dz * dz) < 64)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected string BuildPipKey(notnull array<ref IA_GmSiteRecord> pins)
	{
		string key = "";
		int i;
		int count = pins.Count();
		for (i = 0; i < count; i++)
		{
			IA_GmSiteRecord rec = pins[i];
			if (!rec)
				continue;
			key = key + rec.m_iGroupId.ToString() + ":" + rec.m_sName + ":" + rec.m_fX.ToString() + "," + rec.m_fZ.ToString() + ";";
		}
		return key;
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateSitePip(notnull SCR_MapEntity mapEnt, notnull IA_GmSiteRecord rec, bool isLive)
	{
		MapItem pip = mapEnt.CreateCustomMapItem();
		if (!pip)
			return;

		if (isLive)
			pip.SetBaseType(EMapDescriptorType.MDT_BASE);
		else
			pip.SetBaseType(EMapDescriptorType.MDT_TASK);

		pip.SetPos(rec.m_fX, rec.m_fZ);
		if (rec.m_fRadius > 0)
			pip.SetRange(rec.m_fRadius);

		string label = rec.m_sName;
		if (label.IsEmpty())
			label = IA_GmDirector.AreaTypeToString(rec.m_iType);
		pip.SetDisplayName(label);
		pip.SetVisible(true);

		MapDescriptorProps props = pip.GetProps();
		if (props)
		{
			if (isLive)
				props.SetFrontColor(Color.FromSRGBA(89, 235, 224, 255));
			else
				props.SetFrontColor(Color.FromSRGBA(237, 158, 41, 255));
			props.SetOutlineColor(Color.FromSRGBA(0, 0, 0, 255));
			props.SetIconSize(1, 0.5, 0.5);
			props.SetTextVisible(true);
			props.Activate(true);
			pip.SetProps(props);
		}

		if (!m_SitePips)
			m_SitePips = new array<ref MapItem>();
		m_SitePips.Insert(pip);
	}

	//------------------------------------------------------------------------------------------------
	protected void RecycleSitePips()
	{
		m_sPipKey = "";
		if (!m_SitePips)
			return;

		int i;
		int count = m_SitePips.Count();
		for (i = 0; i < count; i++)
		{
			MapItem pip = m_SitePips[i];
			if (pip)
				pip.Recycle();
		}
		m_SitePips.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected string FormatGroupList(array<IA_AreaMarker> markers)
	{
		if (!markers || markers.IsEmpty())
			return "(empty)";

		string text = "";
		int i;
		int count = markers.Count();
		for (i = 0; i < count; i++)
		{
			IA_AreaMarker marker = markers[i];
			if (!marker)
				continue;
			if (!text.IsEmpty())
				text = text + "\n";
			text = text + marker.GetAreaName();
		}
		if (text.IsEmpty())
			return "(empty)";
		return text;
	}
}
