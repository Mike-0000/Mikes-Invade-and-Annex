// Building garrisons reach their posts through native Defend movement.
// This helper only observes arrival and configures waypoints; it never moves pawns.
class IA_BuildingGarrison
{
	static const float FLOOR_TOLERANCE_M = 1.5;

	static bool UsesHold(int assignmentIndex)
	{
		return (assignmentIndex % 2) == 1;
	}

	static IA_AiOrder OrderFor(bool holdAfterEntry, bool arrived)
	{
		if (holdAfterEntry && arrived)
			return IA_AiOrder.Hold;
		return IA_AiOrder.DefendSmall;
	}

	static void ConfigureDefend(SCR_DefendWaypoint waypoint)
	{
		if (!waypoint)
			return;
		// Vanilla fast init teleports soldiers onto allocated smart actions.
		waypoint.SetFastInit(false);
		waypoint.SetCurrentDefendPreset(1);
		SCR_DefendWaypointPreset preset = waypoint.GetCurrentDefendPreset();
		if (!preset)
			return;
		preset.SetUseTurrets(false);
		preset.SetFractionOfSA(1.0);
		ref array<string> tags = {"CoverPost", "ObservationPost"};
		preset.SetTagsForSearch(tags);
	}

	static IEntity FindBuilding(vector post)
	{
		if (post == vector.Zero || !GetGame().GetWorld())
			return null;
		ref IA_BuildingQueryCallback query = new IA_BuildingQueryCallback();
		GetGame().GetWorld().QueryEntitiesBySphere(post, 28, query.OnBuilding, query.FilterBuilding, EQueryEntitiesFlags.STATIC);
		IEntity best;
		float bestArea = float.MAX;
		foreach (IEntity building : query.m_Buildings)
		{
			vector mins;
			vector maxs;
			building.GetWorldBounds(mins, maxs);
			if (!WithinBounds(post, mins, maxs))
				continue;
			float area = (maxs[0] - mins[0]) * (maxs[2] - mins[2]);
			if (area >= bestArea)
				continue;
			best = building;
			bestArea = area;
		}
		return best;
	}

	static bool WithinBounds(vector pos, vector mins, vector maxs)
	{
		return pos[0] > mins[0] && pos[0] < maxs[0] && pos[2] > mins[2] && pos[2] < maxs[2] && pos[1] >= mins[1] && pos[1] < maxs[1];
	}

	static bool NearPost(vector pos, vector post, float radius)
	{
		if (radius < 3)
			radius = 5;
		return vector.DistanceXZ(pos, post) <= radius && Math.AbsFloat(pos[1] - post[1]) <= FLOOR_TOLERANCE_M;
	}

	static bool HasReachedInterior(SCR_AIGroup group, IEntity building, vector post, float radius)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!group || !building || !world)
			return false;

		vector mins;
		vector maxs;
		building.GetWorldBounds(mins, maxs);
		ref array<AIAgent> agents = {};
		group.GetAgents(agents);
		ref array<IEntity> excluded = {};
		foreach (AIAgent member : agents)
		{
			if (member && member.GetControlledEntity())
				excluded.Insert(member.GetControlledEntity());
		}

		ref TraceParam roof = new TraceParam();
		roof.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		roof.ExcludeArray = excluded;
		int living = 0;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			ChimeraCharacter pawn = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (!pawn || !pawn.GetCharacterController())
				return false;
			if (pawn.GetCharacterController().GetLifeState() == ECharacterLifeState.DEAD)
				continue;
			living++;
			vector pos = pawn.GetOrigin();
			if (pawn.IsInVehicle() || !NearPost(pos, post, radius) || !WithinBounds(pos, mins, maxs))
				return false;

			// A group centre near a wall is insufficient. Each living member must
			// be beneath this building's roof/ceiling on the assigned floor.
			roof.Start = Vector(pos[0], maxs[1] + 1, pos[2]);
			roof.End = pos + Vector(0, 1.8, 0);
			if (roof.Start[1] <= roof.End[1] || world.TraceMove(roof, null) >= 1.0)
				return false;
			IEntity hit = roof.TraceEnt;
			while (hit && hit != building)
			{
				hit = hit.GetParent();
			}
			if (hit != building)
				return false;
		}
		return living > 0;
	}
}
