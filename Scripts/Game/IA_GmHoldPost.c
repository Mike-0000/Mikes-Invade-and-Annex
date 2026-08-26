// IA_GmHoldPost.c
// GM-placeable building hold. Dedicated building-garrison groups (not occupying
// patrols) claim these and get IA_AiOrder.Hold (vanilla Wait, infinite).

class IA_GmHoldPostClass : ScriptedGameTriggerEntityClass
{
};

class IA_GmHoldPost : ScriptedGameTriggerEntity
{
	static const float DEFAULT_RADIUS = 8;
	static const float MIN_RADIUS = 3;
	static const float MAX_RADIUS = 40;
	static const ResourceName PREFAB = "{1A6D47B0E6C35C01}Prefabs/IA_AreaMarkers/IA_GmHoldPost.et";

	[Attribute(defvalue: "8", UIWidgets.EditBox, "Hold radius (m). Occupying AI stay inside this sphere. Small on purpose for a single building.", category: "Hold", params: "3 40")]
	protected float m_fHoldRadius;

	protected bool m_bClaimed;

	private static ref array<IA_GmHoldPost> s_AllHoldPosts = new array<IA_GmHoldPost>();

	void IA_GmHoldPost(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}

	override void EOnInit(IEntity owner)
	{
		EnablePeriodicQueries(false);
		ApplySphereRadius();

		if (!Replication.IsServer())
			return;

		if (!s_AllHoldPosts.Contains(this))
			s_AllHoldPosts.Insert(this);
	}

	protected void ApplySphereRadius()
	{
		SetSphereRadius(GetHoldRadius());
	}

	float GetHoldRadius()
	{
		float radius = m_fHoldRadius;
		if (radius < MIN_RADIUS)
			radius = DEFAULT_RADIUS;
		if (radius > MAX_RADIUS)
			radius = MAX_RADIUS;
		return radius;
	}

	void SetHoldRadius(float radius)
	{
		if (radius < MIN_RADIUS)
			radius = DEFAULT_RADIUS;
		if (radius > MAX_RADIUS)
			radius = MAX_RADIUS;
		m_fHoldRadius = radius;
		ApplySphereRadius();
	}

	bool IsClaimed()
	{
		return m_bClaimed;
	}

	bool TryClaim(out vector pos, out float radius)
	{
		pos = vector.Zero;
		radius = DEFAULT_RADIUS;
		if (m_bClaimed)
			return false;

		m_bClaimed = true;
		pos = GetOrigin();
		radius = GetHoldRadius();
		Print(string.Format("[IA_GmHoldPost] Claimed building hold at %1 radius %2 m", pos.ToString(), radius), LogLevel.NORMAL);
		return true;
	}

	static IA_GmHoldPost SpawnAt(vector pos, float radius)
	{
		if (!Replication.IsServer())
			return null;
		if (pos == vector.Zero)
			return null;

		Resource res = Resource.Load(PREFAB);
		if (!res)
		{
			Print("[IA_GmHoldPost] Failed to load building-hold prefab", LogLevel.ERROR);
			return null;
		}

		IEntity ent = GetGame().SpawnEntityPrefab(res, null, IA_CreateSimpleSpawnParams(pos));
		IA_GmHoldPost post = IA_GmHoldPost.Cast(ent);
		if (!post)
		{
			Print("[IA_GmHoldPost] Spawned entity is not IA_GmHoldPost", LogLevel.ERROR);
			if (ent)
				IA_Game.AddEntityToGc(ent);
			return null;
		}

		if (radius > 0)
			post.SetHoldRadius(radius);

		Print(string.Format("[IA_GmHoldPost] Spawned building hold at %1 radius %2 m", post.GetOrigin().ToString(), post.GetHoldRadius()), LogLevel.NORMAL);
		return post;
	}

	static bool HasHoldNear(vector pos, float distM)
	{
		float distSq = distM * distM;
		array<IA_GmHoldPost> allPosts = GetAllHoldPosts();
		int i;
		int count = allPosts.Count();
		for (i = 0; i < count; i++)
		{
			IA_GmHoldPost post = allPosts[i];
			if (!post)
				continue;
			if (vector.DistanceSq(post.GetOrigin(), pos) <= distSq)
				return true;
		}
		return false;
	}

	static IA_GmHoldPost FromEditorItem(Managed item)
	{
		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(item);
		if (!editable)
			return null;
		return IA_GmHoldPost.Cast(editable.GetOwner());
	}

	static array<IA_GmHoldPost> GetAllHoldPosts()
	{
		int i;
		for (i = s_AllHoldPosts.Count() - 1; i >= 0; i--)
		{
			if (!s_AllHoldPosts[i])
				s_AllHoldPosts.Remove(i);
		}

		return s_AllHoldPosts;
	}

	static ref array<IA_GmHoldPost> GetHoldPostsInArea(IA_Area area)
	{
		ref array<IA_GmHoldPost> inArea = new array<IA_GmHoldPost>();
		if (!area)
			return inArea;

		array<IA_GmHoldPost> allPosts = GetAllHoldPosts();
		foreach (IA_GmHoldPost post : allPosts)
		{
			if (!post)
				continue;
			if (area.IsPositionInside(post.GetOrigin()))
				inArea.Insert(post);
		}

		return inArea;
	}

	static int CountUnclaimedInArea(IA_Area area)
	{
		int count = 0;
		ref array<IA_GmHoldPost> inArea = GetHoldPostsInArea(area);
		foreach (IA_GmHoldPost post : inArea)
		{
			if (!post)
				continue;
			if (post.IsClaimed())
				continue;
			count = count + 1;
		}
		return count;
	}

	static bool TryClaimOneInArea(IA_Area area, out vector pos, out float radius)
	{
		pos = vector.Zero;
		radius = DEFAULT_RADIUS;
		if (!area)
			return false;

		ref array<IA_GmHoldPost> free = new array<IA_GmHoldPost>();
		ref array<IA_GmHoldPost> inArea = GetHoldPostsInArea(area);
		foreach (IA_GmHoldPost post : inArea)
		{
			if (!post)
				continue;
			if (post.IsClaimed())
				continue;
			free.Insert(post);
		}

		if (free.IsEmpty())
			return false;

		int pick = Math.RandomInt(0, free.Count());
		IA_GmHoldPost chosen = free[pick];
		if (!chosen)
			return false;
		return chosen.TryClaim(pos, radius);
	}

	//! Drop auto CoverPosts that sit inside a GM building hold so the same room is not double-garrisoned.
	static void ExcludeCoveredPositions(notnull array<vector> posts, IA_Area area)
	{
		ref array<IA_GmHoldPost> holds = GetHoldPostsInArea(area);
		int h;
		int holdCount = holds.Count();
		for (h = 0; h < holdCount; h++)
		{
			IA_GmHoldPost hold = holds[h];
			if (!hold)
				continue;

			vector origin = hold.GetOrigin();
			float cover = hold.GetHoldRadius() + 4;
			float coverSq = cover * cover;
			int p = posts.Count() - 1;
			while (p >= 0)
			{
				if (vector.DistanceSq(posts[p], origin) <= coverSq)
					posts.Remove(p);
				p = p - 1;
			}
		}
	}
}
