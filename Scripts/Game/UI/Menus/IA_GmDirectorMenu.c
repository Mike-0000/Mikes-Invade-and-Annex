//------------------------------------------------------------------------------------------------
//! Game Master map workstation. Places the same IA_AreaMarker prefabs as the
//! vanilla editor, through IA_GmDirector. Native MapWidget is owned by MHJ_MapHost.
//------------------------------------------------------------------------------------------------
class IA_GmDirectorMenu : MUI_MenuBase
{
	protected ref MHJ_MapHost m_Map;
	protected ref MHJ_MapPicker m_Picker;
	protected ref MUI_Label m_LiveList;
	protected ref MUI_Label m_StagingList;
	protected ref MUI_Dropdown m_TypeDrop;
	protected ref MUI_Toggle m_LiveBucket;
	protected ref MUI_Toggle m_QrfClick;
	protected ref MUI_Dropdown m_QrfType;
	protected ref MUI_Label m_CoordLabel;
	protected ref MUI_NumericField m_RadiusField;
	protected bool m_bIgnoreFirstPick;
	protected bool m_bClosingMap;

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

		m_bIgnoreFirstPick = true;
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
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);
		if (m_Map)
			m_Map.Tick(m_Picker);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		if (!m_bClosingMap)
		{
			m_bClosingMap = true;
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
		ref MUI_Panel overlay = runtime.CreatePanel("overlay");
		overlay.MakeOverlay();
		overlay.GetStyle().m_Fill = Color.FromInt(0);

		ref MUI_FxBackdrop fx = runtime.CreateFxBackdrop("fx");

		ref MUI_Row split = runtime.CreateRow("split");
		split.SetFillWidth();
		split.SetFillHeight();
		split.SetGap(16);
		split.SetPadding(20);
		split.SetIntro(0.06, 0.55, 46);

		ref MUI_Card card = runtime.CreateCard("card");
		card.SetWidth(440);
		card.SetFillHeight();
		card.SetPadding(28);
		card.SetPaddingTRBL(22, 28, 24, 28);
		card.SetGap(10);
		card.SetIntro(0.16, 0.4, 18);

		ref MUI_LiveHeader liveHeader = runtime.CreateLiveHeader("GM DIRECTOR", "liveHeader");
		liveHeader.SetKicker("INVADE AND ANNEX  //  STAGING DESK");
		liveHeader.SetIntro(0.22, 0.4, 18);

		ref MUI_Label subtitle = runtime.CreateLabel("Click the map to place a site. Drag pans, wheel zooms. Default bucket is Staging.", "subtitle");
		subtitle.SetFontSize(runtime.GetTheme().FONT_SMALL);
		subtitle.SetMuted(true);

		ref MUI_Label liveHdr = runtime.CreateLabel("LIVE", "liveHdr");
		liveHdr.SetFontSize(runtime.GetTheme().FONT_SMALL);
		liveHdr.SetMuted(true);
		m_LiveList = runtime.CreateLabel("(empty)", "liveList");
		m_LiveList.SetFontSize(runtime.GetTheme().FONT_SMALL);

		ref MUI_Label stagingHdr = runtime.CreateLabel("STAGING", "stagingHdr");
		stagingHdr.SetFontSize(runtime.GetTheme().FONT_SMALL);
		stagingHdr.SetMuted(true);
		m_StagingList = runtime.CreateLabel("(empty)", "stagingList");
		m_StagingList.SetFontSize(runtime.GetTheme().FONT_SMALL);

		ref MUI_Label typeLbl = runtime.CreateLabel("Site type", "typeLbl");
		typeLbl.SetFontSize(runtime.GetTheme().FONT_SMALL);
		typeLbl.SetMuted(true);
		m_TypeDrop = runtime.CreateDropdown("siteType");
		m_TypeDrop.AddItem("Town");
		m_TypeDrop.AddItem("City");
		m_TypeDrop.AddItem("Property");
		m_TypeDrop.AddItem("Military");
		m_TypeDrop.AddItem("Small military");
		m_TypeDrop.AddItem("Docks");
		m_TypeDrop.AddItem("Radio tower");
		m_TypeDrop.AddItem("Mortar pit");
		m_TypeDrop.AddItem("Defend");
		m_TypeDrop.AddItem("Assassination");
		m_TypeDrop.SetIndex(0);

		m_LiveBucket = runtime.CreateToggle("Place into Live", "liveBucket");
		m_RadiusField = runtime.CreateNumericField("Radius (0 = type default)", "radius");
		m_RadiusField.SetRange(0, 400);
		m_RadiusField.SetStep(5);
		m_RadiusField.SetDecimals(0);
		m_RadiusField.SetValue(0);

		m_QrfClick = runtime.CreateToggle("Next click spawns QRF", "qrfClick");
		m_QrfType = runtime.CreateDropdown("qrfType");
		m_QrfType.AddItem("Infantry");
		m_QrfType.AddItem("Motorized");
		m_QrfType.AddItem("Mechanized");
		m_QrfType.AddItem("Armoured");
		m_QrfType.AddItem("Airborne");
		m_QrfType.SetIndex(0);

		m_CoordLabel = runtime.CreateLabel("Click the map to set a point", "coords");
		m_CoordLabel.SetFontSize(runtime.GetTheme().FONT_SMALL);
		m_CoordLabel.SetMuted(true);

		m_Picker = new MHJ_MapPicker();
		runtime.Adopt(m_Picker);
		m_Picker.SetName("picker");
		m_Picker.InitWorld();
		m_Picker.GetOnChanged().Insert(OnPickerChanged);
		m_Picker.SetFillWidth();
		m_Picker.SetFillHeight();
		m_Picker.SetGrow(1);

		ref MUI_Row actions = runtime.CreateRow("actions");
		actions.SetGap(12);
		ref MUI_Button activateBtn = runtime.CreateButton("Activate Staging", "activate");
		activateBtn.MakeAccent();
		activateBtn.GetOnClicked().Insert(OnActivateStaging);
		ref MUI_Button completeBtn = runtime.CreateButton("Complete Live", "complete");
		completeBtn.MakeDanger();
		completeBtn.GetOnClicked().Insert(OnCompleteLive);
		actions.AddChild(activateBtn);
		actions.AddChild(completeBtn);

		ref MUI_Row actions2 = runtime.CreateRow("actions2");
		actions2.SetGap(12);
		ref MUI_Button sideBtn = runtime.CreateButton("Start assassination", "side");
		sideBtn.GetOnClicked().Insert(OnStartSide);
		ref MUI_Button closeBtn = runtime.CreateButton("Close", "close");
		closeBtn.GetOnClicked().Insert(OnMUIBack);
		actions2.AddChild(sideBtn);
		actions2.AddChild(closeBtn);

		card.AddChild(liveHeader);
		card.AddChild(subtitle);
		card.AddChild(liveHdr);
		card.AddChild(m_LiveList);
		card.AddChild(stagingHdr);
		card.AddChild(m_StagingList);
		card.AddChild(typeLbl);
		card.AddChild(m_TypeDrop);
		card.AddChild(m_LiveBucket);
		card.AddChild(m_RadiusField);
		card.AddChild(m_QrfClick);
		card.AddChild(m_QrfType);
		card.AddChild(m_CoordLabel);
		ref MUI_Spacer railGrow = runtime.CreateSpacer(0, "railGrow");
		railGrow.SetFillHeight();
		railGrow.SetGrow(1);
		card.AddChild(railGrow);
		card.AddChild(actions);
		card.AddChild(actions2);

		split.AddChild(card);
		split.AddChild(m_Picker);
		overlay.AddChild(fx);
		overlay.AddChild(split);
		runtime.SetRoot(overlay);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPickerChanged()
	{
		if (!m_Picker)
			return;

		if (m_CoordLabel)
			m_CoordLabel.SetText(string.Format("Point  %1  %2", m_Picker.GetDropX(), m_Picker.GetDropZ()));

		if (m_bIgnoreFirstPick)
		{
			m_bIgnoreFirstPick = false;
			return;
		}

		if (m_QrfClick && m_QrfClick.IsChecked())
		{
			RequestQrfAtClick();
			return;
		}

		RequestPlaceAtClick();
	}

	//------------------------------------------------------------------------------------------------
	protected IA_AreaType SelectedAreaType()
	{
		int idx = 0;
		if (m_TypeDrop)
			idx = m_TypeDrop.GetIndex();
		if (idx == 1)
			return IA_AreaType.City;
		if (idx == 2)
			return IA_AreaType.Property;
		if (idx == 3)
			return IA_AreaType.Military;
		if (idx == 4)
			return IA_AreaType.SmallMilitary;
		if (idx == 5)
			return IA_AreaType.Docks;
		if (idx == 6)
			return IA_AreaType.RadioTower;
		if (idx == 7)
			return IA_AreaType.MortarPit;
		if (idx == 8)
			return IA_AreaType.DefendObjective;
		if (idx == 9)
			return IA_AreaType.Assassination;
		return IA_AreaType.Town;
	}

	//------------------------------------------------------------------------------------------------
	protected IA_QRFType SelectedQrfType()
	{
		int idx = 0;
		if (m_QrfType)
			idx = m_QrfType.GetIndex();
		if (idx == 1)
			return IA_QRFType.Motorized;
		if (idx == 2)
			return IA_QRFType.Mechanized;
		if (idx == 3)
			return IA_QRFType.Armoured;
		if (idx == 4)
			return IA_QRFType.Airborne;
		return IA_QRFType.Infantry;
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestPlaceAtClick()
	{
		if (!m_Picker || !m_Picker.HasDrop())
			return;

		int bucket = IA_GmBucket.Staging;
		if (m_LiveBucket && m_LiveBucket.IsChecked())
			bucket = IA_GmBucket.Live;

		float radius = 0;
		if (m_RadiusField)
			radius = m_RadiusField.GetValue();

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.IA_AskGmPlaceSite(SelectedAreaType(), m_Picker.GetDropX(), m_Picker.GetDropZ(), bucket, "", radius);

		GetGame().GetCallqueue().CallLater(this.RefreshLists, 400, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestQrfAtClick()
	{
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
		if (m_Picker && m_Picker.HasDrop())
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
		ref array<IA_AreaMarker> live = dir.CollectGroupMarkers(dir.GetLiveGroupId());
		ref array<IA_AreaMarker> staging = dir.CollectGroupMarkers(dir.GetStagingGroupId());
		if (m_LiveList)
			m_LiveList.SetText(FormatGroupList(live));
		if (m_StagingList)
			m_StagingList.SetText(FormatGroupList(staging));
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
