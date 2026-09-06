// Runtime diagnostics are opt-in on each server/client process: -iaDebug 1.
// Guard at the call site so production does not build strings or query debug data.
class IA_Log
{
	protected static bool s_bInitialized;
	protected static bool s_bDebugEnabled;

	static bool IsDebugEnabled()
	{
		if (!s_bInitialized)
		{
			string value;
			s_bDebugEnabled = System.GetCLIParam("iaDebug", value) && value == "1";
			s_bInitialized = true;
			if (s_bDebugEnabled)
				Info("[IA][Log] Runtime diagnostics enabled (-iaDebug 1).");
		}
		return s_bDebugEnabled;
	}

	// Reserved for infrequent operational milestones and admin/configuration audit.
	static void Info(string message)
	{
		Print(message, LogLevel.NORMAL);
	}
}
