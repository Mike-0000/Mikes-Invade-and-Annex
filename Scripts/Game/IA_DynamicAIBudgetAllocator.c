// Pure demand allocation. Already-live protected soldiers may keep the cost
// above the target until they can be evicted; optional restore does not.
class IA_DynamicAIBudgetAllocator
{
	static const float RETENTION_BIAS_M = 50;
	static const float WAITING_NONE_M = 10000000;

	static bool IsClearlyFarther(float candidateM, float waitingM, int biasM)
	{
		if (waitingM >= WAITING_NONE_M)
			return false;
		return candidateM > waitingM + Math.Max(0, biasM);
	}

	// A wake-range squad that still has reserves, or that has not filled its last
	// allocation, is waiting. Physical occupancy of a farther incumbent must not
	// keep consuming leftover budget while that squad is empty.
	static float NearestWaitingDistance(array<ref IA_DynamicAIBudgetEntry> entries, int wakeM)
	{
		float best = WAITING_NONE_M;
		if (!entries)
			return best;
		foreach (IA_DynamicAIBudgetEntry entry : entries)
		{
			if (!entry)
				continue;
			if (entry.m_fDistance > wakeM)
				continue;
			int physical = Math.Max(0, entry.m_iPhysical);
			int alive = Math.Max(0, entry.m_iAlive);
			if (physical >= alive && physical >= Math.Max(0, entry.m_iPreviousDesired))
				continue;
			if (entry.m_fDistance < best)
				best = entry.m_fDistance;
		}
		return best;
	}

	static void DropOuterIncumbentEligibility(array<ref IA_DynamicAIBudgetEntry> entries, float waitingM, int wakeM, int biasM)
	{
		if (!entries)
			return;
		foreach (IA_DynamicAIBudgetEntry entry : entries)
		{
			if (!entry)
				continue;
			if (!IsClearlyFarther(entry.m_fDistance, waitingM, biasM))
				continue;
			if (entry.m_fDistance > wakeM)
				entry.m_bInRange = false;
		}
	}

	static void Allocate(array<ref IA_DynamicAIBudgetEntry> entries, int budget)
	{
		if (!entries)
			return;
		int remaining = Math.Max(0, budget);
		ref array<IA_DynamicAIBudgetEntry> ranked = {};
		foreach (IA_DynamicAIBudgetEntry entry : entries)
		{
			if (!entry)
				continue;
			int alive = Math.Max(0, entry.m_iAlive);
			entry.m_iDesired = Math.Min(alive, Math.Max(0, entry.m_iProtected));
			remaining = Math.Max(0, remaining - entry.m_iDesired);
			if (!entry.m_bInRange || entry.m_iDesired == alive)
				continue;
			if (budget <= 0)
			{
				entry.m_iDesired = alive;
				continue;
			}
			InsertRanked(ranked, entry);
		}
		// Every protected demand was reserved before optional groups receive any
		// slots, including protected groups outside the ordinary wake distance.
		foreach (IA_DynamicAIBudgetEntry candidate : ranked)
		{
			if (remaining <= 0)
				break;
			int additional = Math.Min(remaining, Math.Max(0, candidate.m_iAlive) - candidate.m_iDesired);
			candidate.m_iDesired += additional;
			remaining -= additional;
		}
	}

	protected static float RankingDistance(IA_DynamicAIBudgetEntry entry)
	{
		float distance = entry.m_fDistance;
		if (entry.m_iPreviousDesired > 0)
			distance -= Math.Max(0, entry.m_iRetentionBiasM);
		return Math.Max(0, distance);
	}

	protected static bool ComesBefore(IA_DynamicAIBudgetEntry candidate, IA_DynamicAIBudgetEntry existing)
	{
		float candidateDistance = RankingDistance(candidate);
		float existingDistance = RankingDistance(existing);
		if (candidateDistance != existingDistance)
			return candidateDistance < existingDistance;
		return candidate.m_iOrder < existing.m_iOrder;
	}

	protected static void InsertRanked(array<IA_DynamicAIBudgetEntry> ranked, IA_DynamicAIBudgetEntry entry)
	{
		// Sort references separately so registry order and census cursors stay
		// stable. Equal distance/order retains the caller's original order.
		int lower = 0;
		int upper = ranked.Count();
		while (lower < upper)
		{
			int middle = lower + (upper - lower) / 2;
			if (ComesBefore(entry, ranked[middle]))
				upper = middle;
			else
				lower = middle + 1;
		}
		ranked.InsertAt(entry, lower);
	}
}
