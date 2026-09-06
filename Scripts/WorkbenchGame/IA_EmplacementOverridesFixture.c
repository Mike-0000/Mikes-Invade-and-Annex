#ifdef WORKBENCH
// Exercise the actual persistence codec without touching the server profile.
class IA_EmplacementOverridesFixture : IA_AdminOverrides
{
	string Encode() { return ToJson(); }
	void Decode(string json) { ParseJson(json); }
}
#endif
