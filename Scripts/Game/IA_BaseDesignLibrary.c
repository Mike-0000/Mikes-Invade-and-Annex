// One bounded recipe selection per AO; no per-heading rerolls. History advances
// only for a committed site, so rejected terrain never consumes design variety.
class IA_BaseDesignLibrary
{
	protected static ref array<int> s_aRecent = {};

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

	static void Committed(int variant)
	{
		if (variant < 0)
			return;
		s_aRecent.Insert(variant);
		if (s_aRecent.Count() > 6)
			s_aRecent.Remove(0);
	}
}
