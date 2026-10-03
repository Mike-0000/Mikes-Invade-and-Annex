//------------------------------------------------------------------------------------------------
//! Generated base recipes ship as .conf data (Configs/IA/*Recipes.conf), not
//! script. As generated script they were ~35,000 lines that every server
//! compiles into its Game module, and a server running many mods failed
//! "Too many instructions per function" / "Can't compile Game script module".
//! IA_BaseDesignRecipes / IA_HeadquartersRecipes load a book once and replay
//! one recipe per layout request. Recipe i sits at index i.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class IA_SiteRecipeBook
{
	[Attribute()]
	ref array<ref IA_SiteRecipe> m_aRecipes;

	//! Null, with one error, when the resource is missing or not the expected size.
	static IA_SiteRecipeBook Load(ResourceName resource, int expectedCount)
	{
		Resource container = BaseContainerTools.LoadContainer(resource);
		if (!container || !container.IsValid())
		{
			Print("[IA][SiteRecipes] cannot load " + resource, LogLevel.ERROR);
			return null;
		}
		ref IA_SiteRecipeBook book = IA_SiteRecipeBook.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!book || !book.m_aRecipes || book.m_aRecipes.Count() != expectedCount)
		{
			Print(string.Format("[IA][SiteRecipes] %1 is not a book of %2 recipes", resource, expectedCount), LogLevel.ERROR);
			return null;
		}
		return book;
	}

	//! Replays recipe onto a fresh layout. False, with an error, on malformed data.
	bool Apply(int recipe, notnull IA_ComposedSiteLayout layout, int variantBase)
	{
		if (recipe < 0 || recipe >= m_aRecipes.Count() || !m_aRecipes[recipe])
			return false;
		int failed = m_aRecipes[recipe].Apply(layout, variantBase);
		if (failed < 0)
			return true;
		Print(string.Format("[IA][SiteRecipes] recipe %1 op %2 is malformed", recipe, failed), LogLevel.ERROR);
		return false;
	}
}
