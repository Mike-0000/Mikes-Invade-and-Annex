#ifdef WORKBENCH
//------------------------------------------------------------------------------------------------
//! Checks the name the server reports to the statistics API: which source wins, what is kept on
//! disk between restarts, and what the engine gives for a live name on this machine. Nothing is
//! sent anywhere.
//!
//! The file tests run in a folder of the probe's own with a made-up live name; the profile's real
//! files are compared before and after. Marks go to $profile:IA_ServerNameProbe.log.
//------------------------------------------------------------------------------------------------
[WorkbenchPluginAttribute(name: "IA server name probe", wbModules: {"ResourceManager"})]
class IA_ServerNameProbe : WorkbenchPlugin
{
	protected static const string WORLD = "worlds/Showcase/PBR_Vehicles.ent";
	protected static const string FOLDER = "$profile:IA_ServerNameProbe";
	protected static const string OVERRIDE_PATH = FOLDER + "/server_name.txt";
	protected static const string LAST_PATH = FOLDER + "/last_server_name.txt";
	protected static const string REAL_FOLDER = "$profile:MikesInvadeAndAnnex";
	protected static const string OTHER_DEFAULT = "Server Name -PLEASE RENAME IN server_name.txt, in Server Files";
	protected static const int GAME_MODE_TIMEOUT_MS = 240000;

	protected int m_iFailures;

	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		Mark("start");
		string realOverride = Whole(REAL_FOLDER + "/server_name.txt");
		string realLast = Whole(REAL_FOLDER + "/last_server_name.txt");
		string realConfig = Whole(REAL_FOLDER + "/api_config.json");

		TestRules();
		TestFiles();
		TestEngine();

		Expect(Whole(REAL_FOLDER + "/server_name.txt") == realOverride, "the profile's server_name.txt is as it was");
		Expect(Whole(REAL_FOLDER + "/last_server_name.txt") == realLast, "the profile's last_server_name.txt is as it was");
		Expect(Whole(REAL_FOLDER + "/api_config.json") == realConfig, "the profile's api_config.json is as it was");

		Mark(string.Format("done failures=%1", m_iFailures));
		if (m_iFailures == 0)
			Print("[IA][ServerNameProbe] PASS", LogLevel.NORMAL);
		else
			Print(string.Format("[IA][ServerNameProbe] FAIL failures=%1", m_iFailures), LogLevel.ERROR);
		Workbench.Exit(m_iFailures);
	}

	//------------------------------------------------------------------------------------------------
	//! The order of precedence and the clean-up of a name, on values handed in.
	protected void TestRules()
	{
		int source;
		string name = IA_ServerNameResolver.Pick("My Static Name", "Live Name", "Old Name", source);
		Expect(name == "My Static Name" && source == IA_ServerNameResolver.SOURCE_FILE, "an owner's name in the file wins over the live name");

		name = IA_ServerNameResolver.Pick(IA_ServerNameResolver.LEGACY_DEFAULT, "Live Name", "Old Name", source);
		Expect(name == "Live Name" && source == IA_ServerNameResolver.SOURCE_LIVE, "the text older builds wrote gives way to the live name");

		name = IA_ServerNameResolver.Pick(OTHER_DEFAULT, "Live Name", "", source);
		Expect(name == "Live Name" && source == IA_ServerNameResolver.SOURCE_LIVE, "the older default text gives way too");

		name = IA_ServerNameResolver.Pick("59th Ravens.txt - PLEASE RENAME IN server_name.txt, in I&A Server Profile Folder", "59th Ravens", "", source);
		Expect(name == "59th Ravens" && source == IA_ServerNameResolver.SOURCE_LIVE, "a half-edited default gives way");

		name = IA_ServerNameResolver.Pick("", "Live Name", "Old Name", source);
		Expect(name == "Live Name" && source == IA_ServerNameResolver.SOURCE_LIVE, "no file falls through to the live name");

		name = IA_ServerNameResolver.Pick("  \t ", "Live Name", "Old Name", source);
		Expect(name == "Live Name" && source == IA_ServerNameResolver.SOURCE_LIVE, "a blank file falls through to the live name");

		name = IA_ServerNameResolver.Pick("", "", "Old Name", source);
		Expect(name == "Old Name" && source == IA_ServerNameResolver.SOURCE_LAST, "no live name falls back to the last one seen");

		name = IA_ServerNameResolver.Pick(IA_ServerNameResolver.LEGACY_DEFAULT, "", "", source);
		Expect(name.IsEmpty() && source == IA_ServerNameResolver.SOURCE_NONE, "nothing known gives no name");

		name = IA_ServerNameResolver.Pick("  Spaced Name \r", "Live Name", "", source);
		Expect(name == "Spaced Name" && source == IA_ServerNameResolver.SOURCE_FILE, "a file name loses its outer spaces and line end");

		name = IA_ServerNameResolver.Pick("   ", " ", "\t", source);
		Expect(name.IsEmpty() && source == IA_ServerNameResolver.SOURCE_NONE, "names of spaces only give no name, never spaces");

		Expect(IA_ServerNameResolver.Clean("a\tb\r\n") == "a b", "tabs and line breaks become spaces");
		string tab = "\t";
		string lineEnd = "\r\n";
		Expect(tab.Length() == 1 && lineEnd.Length() == 2, "the tab and line-end escapes are single characters");
		Expect(IA_ServerNameResolver.IsPlaceholder("x - please rename in SERVER_NAME.txt"), "the default text is known in any case");
		Expect(!IA_ServerNameResolver.IsPlaceholder("Please Rename Me"), "a name that says rename is still a name");
		Expect(!IA_ServerNameResolver.IsPlaceholder("My server_name.txt server"), "a name that mentions the file is still a name");

		string longName;
		for (int i = 0; i < 60; i++)
		{
			longName = longName + "12345";
		}
		Expect(IA_ServerNameResolver.Clean(longName).Length() == IA_ServerNameResolver.MAX_LENGTH, "an over-long name is cut to what the backend keeps");
		Expect(IA_ServerNameResolver.Clean("short") == "short", "a short name is kept whole");
		Mark("rules checked");
	}

	//------------------------------------------------------------------------------------------------
	//! The whole path, with files in the probe's folder and a made-up live name.
	protected void TestFiles()
	{
		FileIO.MakeDirectory(FOLDER);
		FileIO.DeleteFile(OVERRIDE_PATH);
		FileIO.DeleteFile(LAST_PATH);
		IA_ServerNameResolver.ProbeBegin(FOLDER);
		IA_ServerNameResolver.ProbeLiveName("");

		int source;
		Expect(IA_ServerNameResolver.ForStats().IsEmpty(), "nothing known sends no name with statistics");
		Expect(IA_ServerNameResolver.ForRegistration() == IA_ServerNameResolver.LEGACY_DEFAULT, "nothing known registers under the old default");
		Expect(!FileIO.FileExists(OVERRIDE_PATH), "server_name.txt is no longer created");
		Expect(!FileIO.FileExists(LAST_PATH), "no last name is written while none is known");

		IA_ServerNameResolver.ProbeLiveName("Alpha Server");
		string name = IA_ServerNameResolver.Resolve(source);
		Expect(name == "Alpha Server" && source == IA_ServerNameResolver.SOURCE_LIVE, "the live name is reported");
		Expect(FirstLine(LAST_PATH) == "Alpha Server", "the live name is kept on disk");
		Expect(IA_ServerNameResolver.ForRegistration() == "Alpha Server", "a known name is what registers");

		IA_ServerNameResolver.ProbeRestart();
		IA_ServerNameResolver.ProbeLiveName("");
		name = IA_ServerNameResolver.Resolve(source);
		Expect(name == "Alpha Server" && source == IA_ServerNameResolver.SOURCE_LAST, "after a restart with no live name the kept one is reported");

		IA_ServerNameResolver.ProbeLiveName("Bravo Server");
		name = IA_ServerNameResolver.Resolve(source);
		Expect(name == "Bravo Server" && source == IA_ServerNameResolver.SOURCE_LIVE, "a renamed server reports its new name");
		Expect(FirstLine(LAST_PATH) == "Bravo Server", "the new name replaces the kept one");

		// A second line the resolver never writes: it survives only if the file is left alone.
		WriteLines(LAST_PATH, "Bravo Server", "untouched");
		name = IA_ServerNameResolver.Resolve(source);
		Expect(Whole(LAST_PATH) == "Bravo Server|untouched|", "an unchanged name is not written again");

		WriteLines(OVERRIDE_PATH, IA_ServerNameResolver.LEGACY_DEFAULT, "");
		IA_ServerNameResolver.ProbeRestart();
		name = IA_ServerNameResolver.Resolve(source);
		Expect(name == "Bravo Server" && source == IA_ServerNameResolver.SOURCE_LIVE, "a file still holding the default does not override");
		Expect(FirstLine(OVERRIDE_PATH) == IA_ServerNameResolver.LEGACY_DEFAULT, "a file holding the default is left as it is");

		WriteLines(OVERRIDE_PATH, "Static Name  ", "");
		IA_ServerNameResolver.ProbeRestart();
		IA_ServerNameResolver.ProbeLiveName("Charlie Server");
		name = IA_ServerNameResolver.Resolve(source);
		Expect(name == "Static Name" && source == IA_ServerNameResolver.SOURCE_FILE, "an owner's file overrides the live name");
		Expect(FirstLine(LAST_PATH) == "Charlie Server", "the live name is still kept while a file overrides it");
		Expect(FirstLine(OVERRIDE_PATH) == "Static Name  ", "an owner's file is left as it is");

		// The file is not read on every call.
		WriteLines(OVERRIDE_PATH, "Other Static", "");
		Expect(IA_ServerNameResolver.ForStats() == "Static Name", "the file is not read again straight away");
		IA_ServerNameResolver.ProbeRestart();
		Expect(IA_ServerNameResolver.ForStats() == "Other Static", "an edited file is taken on the next read");

		FileIO.DeleteFile(OVERRIDE_PATH);
		IA_ServerNameResolver.ProbeRestart();
		IA_ServerNameResolver.ProbeLiveName("");
		name = IA_ServerNameResolver.Resolve(source);
		Expect(name == "Charlie Server" && source == IA_ServerNameResolver.SOURCE_LAST, "with the file gone the kept name is reported");

		IA_ServerNameResolver.ProbeEnd();
		FileIO.DeleteFile(OVERRIDE_PATH);
		FileIO.DeleteFile(LAST_PATH);
		Mark("files checked");
	}

	//------------------------------------------------------------------------------------------------
	//! What the engine gives for a live name here, before and in play mode. Reads only.
	protected void TestEngine()
	{
		Mark("engine, no world: " + IA_ServerNameResolver.ProbeSources());

		Workbench.OpenModule(WorldEditor);
		WorldEditor editor = Workbench.GetModule(WorldEditor);
		if (!editor || !editor.SetOpenedResource(WORLD))
		{
			Fail("could not open the probe world");
			return;
		}

		Sleep(500);
		editor.SwitchToGameMode(false, false);
		int waited;
		while (editor.GetApi() && !editor.GetApi().IsGameMode())
		{
			Sleep(100);
			waited = waited + 100;
			if (waited > GAME_MODE_TIMEOUT_MS)
			{
				Fail("game mode did not start");
				return;
			}
		}
		Sleep(6000);
		Mark("engine, play mode: " + IA_ServerNameResolver.ProbeSources());

		// What this profile would report, worked out without the resolver's own file writes.
		int source;
		string name = IA_ServerNameResolver.Pick(FirstLine(REAL_FOLDER + "/server_name.txt"), IA_ServerNameResolver.ReadLiveName(), FirstLine(REAL_FOLDER + "/last_server_name.txt"), source);
		Mark("this profile would report '" + name + "' (" + IA_ServerNameResolver.SourceLabel(source) + ")");

		editor.SwitchToEditMode();
		Sleep(1000);
	}

	//------------------------------------------------------------------------------------------------
	protected string FirstLine(string path)
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
	//! Every line of a file, each closed with a bar; "<none>" when there is no file.
	protected string Whole(string path)
	{
		if (!FileIO.FileExists(path))
			return "<none>";

		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (!file)
			return "<unreadable>";

		string whole;
		string line;
		while (file.ReadLine(line) > -1)
		{
			whole = whole + line + "|";
		}
		file.Close();
		return whole;
	}

	//------------------------------------------------------------------------------------------------
	protected void WriteLines(string path, string first, string second)
	{
		FileHandle file = FileIO.OpenFile(path, FileMode.WRITE);
		if (!file)
		{
			Fail("could not write " + path);
			return;
		}

		file.WriteLine(first);
		if (!second.IsEmpty())
			file.WriteLine(second);
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void Expect(bool held, string what)
	{
		if (!held)
			Fail(what);
	}

	//------------------------------------------------------------------------------------------------
	protected void Fail(string message)
	{
		m_iFailures = m_iFailures + 1;
		Print("[IA][ServerNameProbe] FAIL: " + message, LogLevel.ERROR);
		Mark("FAIL: " + message);
	}

	//------------------------------------------------------------------------------------------------
	protected void Mark(string message)
	{
		Print("[IA][ServerNameProbe] " + message, LogLevel.NORMAL);
		FileHandle file = FileIO.OpenFile("$profile:IA_ServerNameProbe.log", FileMode.APPEND);
		if (!file)
			return;
		file.WriteLine(string.Format("%1 %2", System.GetTickCount(), message));
		file.Close();
	}
}
#endif
