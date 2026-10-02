//------------------------------------------------------------------------------------------------
//! A stock helicopter of a paint family placed through the editor (Game Master
//! or build mode) spawns as its twin on a free paint channel, so its pilot can
//! repaint it like a pad helicopter. The editor asks for a prefab's variant just before it spawns it,
//! on the server; the twin inherits the stock prefab, so the editor sees the
//! same entity.
//------------------------------------------------------------------------------------------------
modded class SCR_EditableEntityComponentClass
{
	//------------------------------------------------------------------------------------------------
	override static ResourceName GetRandomVariant(ResourceName prefab)
	{
		return IA_HeliPaintRigComponent.ResolveSpawnPrefab(super.GetRandomVariant(prefab));
	}
}
