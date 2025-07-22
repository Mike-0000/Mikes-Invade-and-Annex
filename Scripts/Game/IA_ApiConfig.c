class IA_ApiConfig
{
    
    string m_sOwnerEmail = "";
    string m_sServerGuid = "";

    void IA_ApiConfig(string ownerEmail, string guid)
    {
       
        m_sOwnerEmail = ownerEmail;
        m_sServerGuid = guid;
    }

    static IA_ApiConfig GetDefault()
    {
        return new IA_ApiConfig("", "");
    }

    // Manual serialization to a JSON string
    string ToJson()
    {
        string json = "{";
        
        json = json + "\"owner_email\": \"" + m_sOwnerEmail + "\",";
        json = json + "\"server_guid\": \"" + m_sServerGuid + "\"";
        json = json + "}";
        return json;
    }

    // Manual deserialization from a JSON string
    static IA_ApiConfig FromJson(string jsonData)
    {
        // Simple parsing logic. This assumes the JSON is well-formed as we control the writing.
        // We'll extract values based on key-searching.
        //string apiUrl = m_sApiBaseUrl;
        string ownerEmail = _GetValueForKey(jsonData, "owner_email");
        string guid = _GetValueForKey(jsonData, "server_guid");

        return new IA_ApiConfig(ownerEmail, guid);
    }

    private static string _GetValueForKey(string json, string key)
    {
        string searchKey = "\"" + key + "\": \"";
        int startIndex = json.IndexOf(searchKey);
        if (startIndex == -1)
            return "";

        int valueStartIndex = startIndex + searchKey.Length();
        
		string tempJson = json.Substring(valueStartIndex, json.Length() - valueStartIndex);
		
        int endIndex = tempJson.IndexOf("\"");
        if (endIndex == -1)
            return "";

        return tempJson.Substring(0, endIndex);
    }
}

class IA_ApiConfigManager
{
    private static const string CONFIG_DIR = "$profile:MikesInvadeAndAnnex";
    private static const string CONFIG_PATH = CONFIG_DIR + "/api_config.json";
	private static const string SERVER_NAME_PATH = CONFIG_DIR + "/server_name.txt";
    
    private static ref IA_ApiConfig m_Config;

    static IA_ApiConfig GetConfig()
    {
        if (!m_Config)
            LoadConfig();
        
        return m_Config;
    }
	
	static string GetServerNameFromFile()
	{
		FileHandle file = FileIO.OpenFile(SERVER_NAME_PATH, FileMode.READ);
        if (file)
        {
            string serverName;
            int readBytes = file.ReadLine(serverName);
            file.Close();
            
            if (readBytes > 0 && serverName != "")
            {
                Print("IA_ApiConfigManager: Server name '" + serverName + "' loaded from " + SERVER_NAME_PATH, LogLevel.NORMAL);
                return serverName;
            }
        }
        
        Print("IA_ApiConfigManager: No server name file found or it's empty. Creating a new default config.", LogLevel.NORMAL);
        string defaultServerName = "Server Name -PLEASE RENAME IN server_name.txt, in Server Files";
		
		FileIO.MakeDirectory(CONFIG_DIR);
        FileHandle writeFile = FileIO.OpenFile(SERVER_NAME_PATH, FileMode.WRITE);
        if (writeFile)
        {
            writeFile.WriteLine(defaultServerName);
            writeFile.Close();
			Print("IA_ApiConfigManager: Created default server name file at " + SERVER_NAME_PATH, LogLevel.NORMAL);
            return defaultServerName;
        }
		else
		{
			Print("IA_ApiConfigManager: Failed to create default server name file at " + SERVER_NAME_PATH, LogLevel.ERROR);
		}
		
		return "My IA Server";
	}

    static void SaveConfig()
    {
        if (!m_Config)
        {
            Print("IA_ApiConfigManager: Cannot save null config.", LogLevel.ERROR);
            return;
        }

        FileIO.MakeDirectory(CONFIG_DIR);

        FileHandle file = FileIO.OpenFile(CONFIG_PATH, FileMode.WRITE);
        if (file)
        {
            file.WriteLine(m_Config.ToJson());
            file.Close();
            Print("IA_ApiConfigManager: Configuration saved to " + CONFIG_PATH, LogLevel.NORMAL);
        }
        else
        {
            Print("IA_ApiConfigManager: Failed to open file for writing at " + CONFIG_PATH, LogLevel.ERROR);
        }
    }

    private static void LoadConfig()
    {
        FileHandle file = FileIO.OpenFile(CONFIG_PATH, FileMode.READ);
        if (file)
        {
            string fileContent;
			string line;
            while (file.ReadLine(line) > -1)
			{
				fileContent += line;
			}
            file.Close();
            
            if (fileContent != "")
            {
                m_Config = IA_ApiConfig.FromJson(fileContent);
                Print("IA_ApiConfigManager: Configuration loaded from " + CONFIG_PATH, LogLevel.NORMAL);
                return;
            }
        }
        
        // If file doesn't exist, is empty, or failed to read.
        Print("IA_ApiConfigManager: No config file found or it's empty. Creating a new default config.", LogLevel.NORMAL);
        m_Config = IA_ApiConfig.GetDefault();
        SaveConfig();
    }
} 