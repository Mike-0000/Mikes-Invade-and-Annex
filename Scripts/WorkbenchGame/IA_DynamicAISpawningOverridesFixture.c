#ifdef WORKBENCH
// Exercise the real profile codec without reading or writing the server profile.
class IA_DynamicAISpawningOverridesFixture : IA_AdminOverrides
{
	string Encode()
	{
		return ToJson();
	}

	void Decode(string json)
	{
		ParseJson(json);
	}
}
#endif
