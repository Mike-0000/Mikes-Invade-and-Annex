//------------------------------------------------------------------------------------------------
//! One generated base recipe as data: the layout calls it makes, in order, one
//! op string each, fields split on "|". Written by tools/author_base_designs.py
//! and tools/author_headquarters.py with the literal text the generated script
//! used to compile, so a replay builds the same layout.
//!   I|id|variant|name|halfWidth|halfDepth|garrison   Initialize (variant + base)
//!   K|x y z                                          m_vCaptureLocal
//!   C|key|x y z|yaw|role|side|required               AddComposition
//!   W|x y z|yaw|side|style                           AddPerimeterWall
//!   S|x y z|yaw                                      AddAirRaidBunker
//!   G|x y z                                          m_aGuardPosts.Insert
//!   R|key|x y z|yaw|side                             AddWallRun (headquarters)
//!   O|key|x y z|yaw                                  AddObstacle (headquarters)
//!   D|key|x y z|yaw|maxResidual                      AddDressingItem (headquarters)
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class IA_SiteRecipe
{
	[Attribute()]
	ref array<string> m_aOps;

	//! Returns the index of the first op it could not replay, or -1 when all applied.
	int Apply(notnull IA_ComposedSiteLayout layout, int variantBase)
	{
		if (!m_aOps)
			return 0;
		IA_HeadquartersSiteLayout headquarters = IA_HeadquartersSiteLayout.Cast(layout);
		array<string> f = {};
		int count = m_aOps.Count();
		for (int i = 0; i < count; i++)
		{
			f.Clear();
			m_aOps[i].Split("|", f, false);
			if (!ApplyOp(layout, headquarters, f, variantBase))
				return i;
		}
		return -1;
	}

	protected bool ApplyOp(IA_ComposedSiteLayout layout, IA_HeadquartersSiteLayout headquarters, array<string> f, int variantBase)
	{
		int n = f.Count();
		if (n < 2)
			return false;
		string code = f[0];
		if (code == "W" && n == 5)
		{
			layout.AddPerimeterWall(f[1].ToVector(), f[2].ToFloat(), f[3].ToInt(), f[4].ToInt());
			return true;
		}
		if (code == "G" && n == 2)
		{
			layout.m_aGuardPosts.Insert(f[1].ToVector());
			return true;
		}
		if (code == "C" && n == 7)
		{
			int role = typename.StringToEnum(IA_DynamicSiteModuleRole, f[4]);
			if (typename.EnumToString(IA_DynamicSiteModuleRole, role) != f[4])
				return false;
			layout.AddComposition(f[1], f[2].ToVector(), f[3].ToFloat(), role, f[5].ToInt(), f[6] == "1");
			return true;
		}
		if (code == "S" && n == 3)
		{
			layout.AddAirRaidBunker(f[1].ToVector(), f[2].ToFloat());
			return true;
		}
		if (code == "I" && n == 7)
		{
			layout.Initialize(f[1].ToInt(), variantBase + f[2].ToInt(), f[3], f[4].ToFloat(), f[5].ToFloat(), f[6].ToInt());
			return true;
		}
		if (code == "K" && n == 2)
		{
			layout.m_vCaptureLocal = f[1].ToVector();
			return true;
		}
		if (!headquarters)
			return false;
		if (code == "R" && n == 5)
		{
			headquarters.AddWallRun(f[1], f[2].ToVector(), f[3].ToFloat(), f[4].ToInt());
			return true;
		}
		if (code == "O" && n == 4)
		{
			headquarters.AddObstacle(f[1], f[2].ToVector(), f[3].ToFloat());
			return true;
		}
		if (code == "D" && n == 5)
		{
			headquarters.AddDressingItem(f[1], f[2].ToVector(), f[3].ToFloat(), f[4].ToFloat());
			return true;
		}
		return false;
	}
}
