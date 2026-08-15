//------------------------------------------------------------------------------------------------
//! Client-only HUD preferences. Stored under $profile:MikesInvadeAndAnnex, never replicated.
//------------------------------------------------------------------------------------------------
class IA_LocalOptions
{
	protected static const string CONFIG_DIR = "$profile:MikesInvadeAndAnnex";
	protected static const string CONFIG_PATH = CONFIG_DIR + "/local_options.json";

	protected static ref IA_LocalOptions s_Instance;

	protected bool m_bHideRankHud;
	protected bool m_bHidePromotionNotifications;
	protected ref ScriptInvoker m_OnChanged;

	//------------------------------------------------------------------------------------------------
	void IA_LocalOptions()
	{
		m_OnChanged = new ScriptInvoker();
	}

	//------------------------------------------------------------------------------------------------
	static IA_LocalOptions Get()
	{
		if (!s_Instance)
		{
			s_Instance = new IA_LocalOptions();
			s_Instance.Load();
		}

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnChanged()
	{
		if (!m_OnChanged)
			m_OnChanged = new ScriptInvoker();
		return m_OnChanged;
	}

	//------------------------------------------------------------------------------------------------
	bool HideRankHud()
	{
		return m_bHideRankHud;
	}

	//------------------------------------------------------------------------------------------------
	bool HidePromotionNotifications()
	{
		return m_bHidePromotionNotifications;
	}

	//------------------------------------------------------------------------------------------------
	void SetHideRankHud(bool hide)
	{
		if (m_bHideRankHud == hide)
			return;

		m_bHideRankHud = hide;
		Save();
		NotifyChanged();
	}

	//------------------------------------------------------------------------------------------------
	void SetHidePromotionNotifications(bool hide)
	{
		if (m_bHidePromotionNotifications == hide)
			return;

		m_bHidePromotionNotifications = hide;
		Save();
		NotifyChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void NotifyChanged()
	{
		if (m_OnChanged)
			m_OnChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	protected void Load()
	{
		FileHandle file = FileIO.OpenFile(CONFIG_PATH, FileMode.READ);
		if (!file)
			return;

		string fileContent;
		string line;
		while (file.ReadLine(line) > -1)
		{
			fileContent = fileContent + line;
		}
		file.Close();

		if (fileContent.IsEmpty())
			return;

		m_bHideRankHud = ReadFlag(fileContent, "hideRankHud");
		m_bHidePromotionNotifications = ReadFlag(fileContent, "hidePromotionNotifications");
	}

	//------------------------------------------------------------------------------------------------
	protected void Save()
	{
		FileIO.MakeDirectory(CONFIG_DIR);

		FileHandle file = FileIO.OpenFile(CONFIG_PATH, FileMode.WRITE);
		if (!file)
		{
			Print("[IA][LocalOptions] Failed to write " + CONFIG_PATH, LogLevel.ERROR);
			return;
		}

		file.WriteLine(ToJson());
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected string ToJson()
	{
		int hideHud = 0;
		if (m_bHideRankHud)
			hideHud = 1;

		int hidePromo = 0;
		if (m_bHidePromotionNotifications)
			hidePromo = 1;

		return "{\"hideRankHud\":" + hideHud.ToString() + ",\"hidePromotionNotifications\":" + hidePromo.ToString() + "}";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ReadFlag(string json, string key)
	{
		string searchKey = "\"" + key + "\":";
		int startIndex = json.IndexOf(searchKey);
		if (startIndex == -1)
			return false;

		int valueStart = startIndex + searchKey.Length();
		if (valueStart >= json.Length())
			return false;

		string rest = json.Substring(valueStart, json.Length() - valueStart);
		rest.Replace(" ", "");
		rest.Replace("\t", "");
		if (rest.IsEmpty())
			return false;

		if (rest.IndexOf("true") == 0)
			return true;
		if (rest.IndexOf("1") == 0)
			return true;

		return false;
	}
}
