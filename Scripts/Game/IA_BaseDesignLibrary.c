// One bounded recipe selection per AO; no per-heading rerolls. History advances
// only for a committed site, so rejected terrain never consumes design variety.
// Variants below HQ_BASE are scrappy sandbag recipes; HQ_BASE + n are permanent
// headquarters recipes. Both share one int so site/instance plumbing is unchanged.
class IA_BaseDesignLibrary
{
	static const int HQ_BASE = 100;
	static const int HQ_HISTORY = 4;
	static const int STYLE_HISTORY = 4;

	protected static ref array<int> s_aRecent = {};
	protected static ref array<int> s_aRecentHeadquarters = {};
	protected static ref array<int> s_aRecentStyles = {}; // 1 headquarters, 0 scrappy

	static int Select(int seed)
	{
		int start = Math.AbsInt(seed % 20);
		for (int step = 0; step < 20; step++)
		{
			int variant = (start + step * 7) % 20;
			if (s_aRecent.Find(variant) >= 0)
				continue;
			int theme = variant / 4;
			bool repeatedTheme = false;
			int count = s_aRecent.Count();
			for (int i = Math.Max(0, count - 2); i < count; i++)
			{
				int previousTheme = s_aRecent[i] / 4;
				if (theme == previousTheme)
					repeatedTheme = true;
			}
			if (!repeatedTheme)
				return variant;
		}
		return start;
	}

	static bool IsHeadquarters(int variant)
	{
		return variant >= HQ_BASE;
	}

	// Style first, then a variant within that style. The roll is a hash of the
	// AO seed so it is independent of Select(), which reads seed % 20 directly.
	static int SelectDesign(int seed, int headquartersChancePct)
	{
		if (RollHeadquarters(seed, headquartersChancePct))
			return HQ_BASE + SelectHeadquarters(seed);
		return Select(seed);
	}

	static bool RollHeadquarters(int seed, int headquartersChancePct)
	{
		if (headquartersChancePct <= 0)
			return false;
		if (headquartersChancePct >= 100)
			return true;
		int roll = Mix(seed) % 100;
		bool headquarters = roll < headquartersChancePct;
		// Near an even split, three in a row of one style reads as a bug, not luck.
		int count = s_aRecentStyles.Count();
		if (headquartersChancePct >= 34 && headquartersChancePct <= 66 && count >= 2)
		{
			int last = s_aRecentStyles[count - 1];
			int beforeLast = s_aRecentStyles[count - 2];
			if (last == beforeLast)
				headquarters = last == 0;
		}
		return headquarters;
	}

	// Index 0..VARIANTS-1. Skips recent recipes and the last archetype (v / 3).
	static int SelectHeadquarters(int seed)
	{
		int total = IA_HeadquartersRecipes.VARIANTS;
		int hash = Mix(seed) / 128;
		int start = hash % total;
		int lastArchetype = -1;
		int count = s_aRecentHeadquarters.Count();
		if (count > 0)
			lastArchetype = s_aRecentHeadquarters[count - 1] / 3;
		for (int step = 0; step < total; step++)
		{
			// 5 is coprime with 12, so the walk visits every recipe once.
			int variant = (start + step * 5) % total;
			if (s_aRecentHeadquarters.Find(variant) >= 0)
				continue;
			int archetype = variant / 3;
			if (archetype == lastArchetype)
				continue;
			return variant;
		}
		return start;
	}

	static IA_DynamicSiteLayout CreateLayout(int size, int variant)
	{
		if (IsHeadquarters(variant))
			return IA_HeadquartersRecipes.Create(size, variant - HQ_BASE);
		return IA_BaseDesignRecipes.Create(size, variant);
	}

	static void Committed(int variant)
	{
		if (variant < 0)
			return;
		bool headquarters = IsHeadquarters(variant);
		if (headquarters)
			s_aRecentStyles.Insert(1);
		else
			s_aRecentStyles.Insert(0);
		// Remove() swaps the last element in; history order matters here.
		if (s_aRecentStyles.Count() > STYLE_HISTORY)
			s_aRecentStyles.RemoveOrdered(0);
		if (headquarters)
		{
			s_aRecentHeadquarters.Insert(variant - HQ_BASE);
			if (s_aRecentHeadquarters.Count() > HQ_HISTORY)
				s_aRecentHeadquarters.RemoveOrdered(0);
			return;
		}
		s_aRecent.Insert(variant);
		if (s_aRecent.Count() > 6)
			s_aRecent.Remove(0);
	}

	// Workbench tests only; live history is process-lifetime by design.
	static void ResetHistory()
	{
		s_aRecent.Clear();
		s_aRecentHeadquarters.Clear();
		s_aRecentStyles.Clear();
	}

	// Non-negative 31-bit avalanche hash (murmur3 finaliser on a golden-ratio
	// multiply). Shifts are masked so the result does not depend on whether
	// >> sign-extends; multiplication wraps like C int32.
	protected static int Mix(int seed)
	{
		int h = seed * -1640531535;
		h = h ^ ((h >> 15) & 131071);
		h = h * -2048144789;
		h = h ^ ((h >> 13) & 524287);
		h = h * -1028477387;
		h = h ^ ((h >> 16) & 65535);
		return h & 2147483647;
	}
}
