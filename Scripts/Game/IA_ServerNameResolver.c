//------------------------------------------------------------------------------------------------
//! Works out the name this server reports to the statistics API, so an owner does not have to
//! keep a file in step with their server's name:
//!   1. server_name.txt, when the owner has written a name of their own into it
//!   2. the name the running server holds, asked of the engine each time
//!   3. the last such name seen, kept in last_server_name.txt
//! It deals in names only. Which server the statistics belong to is IA_ApiConfigManager's
//! business (api_config.json), and nothing here reads or writes it.
//------------------------------------------------------------------------------------------------
class IA_ServerNameResolver
{
	static const int SOURCE_NONE = 0;
	static const int SOURCE_FILE = 1;
	static const int SOURCE_LIVE = 2;
	static const int SOURCE_LAST = 3;

	//! What builds before this one wrote into server_name.txt. /registerServer refuses an empty
	//! name, and the servers board leaves out a server still called this.
	static const string LEGACY_DEFAULT = "Default Name - PLEASE RENAME IN server_name.txt, in I&A Server Profile Folder";

	//! The backend keeps this many characters of a name.
	static const int MAX_LENGTH = 255;

	// Both texts older builds wrote hold this, and so does a file whose owner changed only the
	// first words. Compared in lower case; the backend tests a name the same way.
	protected static const string PLACEHOLDER_MARK = "please rename in server_name.txt";

	protected static const string FOLDER = "$profile:MikesInvadeAndAnnex";
	protected static const string OVERRIDE_FILE = "server_name.txt";
	protected static const string LAST_FILE = "last_server_name.txt";

	// Statistics go out once a minute; the file is read again this often, so a name edited
	// while the server runs is picked up without a restart.
	protected static const int OVERRIDE_REFRESH_MS = 300000;

	protected static bool s_bOverrideRead;
	protected static int s_iOverrideReadTick;
	protected static string s_sOverrideLine;
	protected static bool s_bLastRead;
	protected static string s_sLastName;
	protected static bool s_bReported;
	protected static int s_iReportedSource;
	protected static string s_sReportedName;
	protected static string s_sLiveOrigin;

#ifdef WORKBENCH
	protected static string s_sProbeFolder;
	protected static bool s_bProbeLive;
	protected static string s_sProbeLiveName;
#endif

	//------------------------------------------------------------------------------------------------
	//! \return the name for POST /submitStats; empty when none is known, which the backend takes
	//! as "keep the name on record"
	static string ForStats()
	{
		int source;
		return Resolve(source);
	}

	//------------------------------------------------------------------------------------------------
	//! \return the name for POST /registerServer, which must not be empty
	static string ForRegistration()
	{
		int source;
		string name = Resolve(source);
		if (name.IsEmpty())
			return LEGACY_DEFAULT;

		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the three sources, keeps the live name for later, and picks.
	//! \param[out] source one of the SOURCE_ values
	static string Resolve(out int source)
	{
		string live = ReadLiveName();
		RememberLiveName(live);

		string name = Pick(ReadOverrideLine(), live, ReadLastName(), source);
		Report(name, source);
		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! The order of precedence, on values already read.
	//! \param fileLine first line of server_name.txt, empty when there is no file
	//! \param[out] source one of the SOURCE_ values
	static string Pick(string fileLine, string liveName, string lastName, out int source)
	{
		string name = Clean(fileLine);
		if (!name.IsEmpty() && !IsPlaceholder(name))
		{
			source = SOURCE_FILE;
			return name;
		}

		name = Clean(liveName);
		if (!name.IsEmpty())
		{
			source = SOURCE_LIVE;
			return name;
		}

		name = Clean(lastName);
		if (!name.IsEmpty())
		{
			source = SOURCE_LAST;
			return name;
		}

		source = SOURCE_NONE;
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return true for the text older builds put in server_name.txt for the owner to replace
	static bool IsPlaceholder(string name)
	{
		string lower = name;
		lower.ToLower();
		return lower.Contains(PLACEHOLDER_MARK);
	}

	//------------------------------------------------------------------------------------------------
	//! One line, no outer spaces, no longer than the backend keeps.
	static string Clean(string raw)
	{
		if (raw.IsEmpty())
			return "";

		string name = raw;
		name.Replace("\r", " ");
		name.Replace("\n", " ");
		name.Replace("\t", " ");
		name = name.Trim();

		int length = name.Length();
		if (length <= MAX_LENGTH)
			return name;

		// The cut may fall inside a character of several bytes; give up the letters past the last plain one.
		length = MAX_LENGTH;
		while (length > 0)
		{
			int code = name.ToAscii(length - 1);
			if (code >= 32 && code < 127)
				break;

			length = length - 1;
		}

		name = name.Substring(0, length);
		return name.Trim();
	}

	//------------------------------------------------------------------------------------------------
	//! The name the engine holds for the running server. Empty where it has none, as in Workbench.
	static string ReadLiveName()
	{
#ifdef WORKBENCH
		if (s_bProbeLive)
			return s_sProbeLiveName;
#endif

		// The engine has three places for it. None is filled in Workbench or on a client.
		string name = ReadServerInfoName();
		if (!name.IsEmpty())
		{
			s_sLiveOrigin = "ServerInfo";
			return name;
		}

		name = ReadLobbyConfigName();
		if (!name.IsEmpty())
		{
			s_sLiveOrigin = "lobby config";
			return name;
		}

		name = ReadServerConfigName();
		if (!name.IsEmpty())
			s_sLiveOrigin = "server config";

		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! The name the game shows players for this server.
	protected static string ReadServerInfoName()
	{
		ArmaReforgerScripted game = GetGame();
		if (!game)
			return "";

		ServerInfo info = game.GetServerInfo();
		if (!info)
			return "";

		return Clean(info.GetName());
	}

	//------------------------------------------------------------------------------------------------
	//! The name the server is listed under in the server browser.
	protected static string ReadLobbyConfigName()
	{
		ServerConfig config = ServerLobbyApi.GetServerConfig();
		if (!config)
			return "";

		return Clean(config.GetName());
	}

	//------------------------------------------------------------------------------------------------
	//! The name in the config a dedicated server was started with.
	protected static string ReadServerConfigName()
	{
		ArmaReforgerScripted game = GetGame();
		if (!game)
			return "";

		BackendApi backend = game.GetBackendApi();
		if (!backend)
			return "";

		ref SCR_DSConfig config = new SCR_DSConfig();
		if (!backend.GetRunningDSConfig(config) || !config.game)
			return "";

		return Clean(config.game.name);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Folder()
	{
#ifdef WORKBENCH
		if (!s_sProbeFolder.IsEmpty())
			return s_sProbeFolder;
#endif

		return FOLDER;
	}

	//------------------------------------------------------------------------------------------------
	protected static string ReadOverrideLine()
	{
		int now = System.GetTickCount();
		int age = now - s_iOverrideReadTick;
		if (s_bOverrideRead && age >= 0 && age < OVERRIDE_REFRESH_MS)
			return s_sOverrideLine;

		s_bOverrideRead = true;
		s_iOverrideReadTick = now;
		s_sOverrideLine = ReadFirstLine(Folder() + "/" + OVERRIDE_FILE);
		return s_sOverrideLine;
	}

	//------------------------------------------------------------------------------------------------
	protected static string ReadLastName()
	{
		if (s_bLastRead)
			return s_sLastName;

		s_bLastRead = true;
		s_sLastName = Clean(ReadFirstLine(Folder() + "/" + LAST_FILE));
		return s_sLastName;
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps a live name on disk for a start on which the engine has none to give.
	protected static void RememberLiveName(string liveName)
	{
		string name = Clean(liveName);
		if (name.IsEmpty() || name == ReadLastName())
			return;

		// Held in memory whether or not the write lands, so a profile that cannot be written is reported once.
		s_sLastName = name;

		FileIO.MakeDirectory(Folder());
		string path = Folder() + "/" + LAST_FILE;
		FileHandle file = FileIO.OpenFile(path, FileMode.WRITE);
		if (!file)
		{
			Print("[IA][ServerName] Could not write " + path + "; the server name will not be remembered across restarts.", LogLevel.WARNING);
			return;
		}

		file.WriteLine(name);
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected static string ReadFirstLine(string path)
	{
		if (!FileIO.FileExists(path))
			return "";

		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (!file)
			return "";

		string line;
		file.ReadLine(line);
		file.Close();
		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! One record each time the reported name or where it comes from changes.
	protected static void Report(string name, int source)
	{
		if (s_bReported && source == s_iReportedSource && name == s_sReportedName)
			return;

		s_bReported = true;
		s_iReportedSource = source;
		s_sReportedName = name;

		if (source == SOURCE_NONE)
		{
			IA_Log.Info("[IA][ServerName] No server name known yet; the statistics service keeps the one it has.");
			return;
		}

		string label = SourceLabel(source);
		if (source == SOURCE_LIVE && !s_sLiveOrigin.IsEmpty())
			label = label + ", from " + s_sLiveOrigin;

		IA_Log.Info("[IA][ServerName] Reporting '" + name + "' to the statistics service (" + label + ").");
	}

	//------------------------------------------------------------------------------------------------
	static string SourceLabel(int source)
	{
		if (source == SOURCE_FILE)
			return "server_name.txt override";
		if (source == SOURCE_LIVE)
			return "live server name";
		if (source == SOURCE_LAST)
			return "last known server name";

		return "none";
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	//! Points the resolver at a folder of the probe's own and forgets what it has read, so a test
	//! never touches the profile's real files.
	static void ProbeBegin(string folder)
	{
		s_sProbeFolder = folder;
		ProbeRestart();
	}

	//------------------------------------------------------------------------------------------------
	//! Stands in for the engine: the live name is this until ProbeEnd.
	static void ProbeLiveName(string name)
	{
		s_bProbeLive = true;
		s_sProbeLiveName = name;
		s_sLiveOrigin = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets everything held in memory, as a server restart does.
	static void ProbeRestart()
	{
		s_bOverrideRead = false;
		s_iOverrideReadTick = 0;
		s_sOverrideLine = "";
		s_bLastRead = false;
		s_sLastName = "";
		s_bReported = false;
		s_iReportedSource = SOURCE_NONE;
		s_sReportedName = "";
	}

	//------------------------------------------------------------------------------------------------
	static void ProbeEnd()
	{
		s_sProbeFolder = "";
		s_bProbeLive = false;
		s_sProbeLiveName = "";
		ProbeRestart();
	}

	//------------------------------------------------------------------------------------------------
	//! What each engine source gives on this machine; reads only.
	static string ProbeSources()
	{
		string report = "serverInfo=";
		ArmaReforgerScripted game = GetGame();
		ServerInfo info;
		if (game)
			info = game.GetServerInfo();
		if (info)
			report = report + "'" + info.GetName() + "'";
		else
			report = report + "null";

		report = report + " dsConfig=";
		BackendApi backend;
		if (game)
			backend = game.GetBackendApi();
		ref SCR_DSConfig config = new SCR_DSConfig();
		if (!backend)
			report = report + "no backend";
		else if (!backend.GetRunningDSConfig(config))
			report = report + "false";
		else if (!config.game)
			report = report + "no game block";
		else
			report = report + "'" + config.game.name + "'";

		report = report + " lobbyEnabled=" + ServerLobbyApi.IsEnabled().ToString() + " lobbyConfig=";
		ServerConfig lobby = ServerLobbyApi.GetServerConfig();
		if (lobby)
			report = report + "'" + lobby.GetName() + "'";
		else
			report = report + "null";

		return report + " live='" + ReadLiveName() + "'";
	}
#endif
}
