// A cached garrison is logically alive. All other groups retain vanilla OnEmpty.
modded class SCR_AIGroup
{
	override void OnEmpty()
	{
		if (IA_DynamicAISpawning.IsVirtualizingGroup(this))
			return;
		super.OnEmpty();
	}
}
