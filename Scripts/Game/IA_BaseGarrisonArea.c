// All occupying groups share a base-centred defense area, not separate posts.
// Trim corner coverage to reduce movement outside the rectangular walls.
// Vanilla Defend remains a circular soft leash, not a hard perimeter boundary.
class IA_BaseGarrisonArea
{
	static const float DEFEND_RADIUS_SCALE = 0.85;

	static bool Resolve(notnull IA_DynamicSiteLayout layout, vector localPost, out vector localCenter, out float radius)
	{
		localCenter = vector.Zero;
		radius = 0;
		// Keep the existing perimeter validation for the soldier's spawn point.
		if (layout.GetDefendPostRadius(localPost) <= 0)
			return false;

		float halfWidth = layout.m_fHalfWidthM;
		float halfDepth = layout.m_fHalfDepthM;
		if (halfWidth <= 0 || halfDepth <= 0)
			return false;
		radius = Math.Sqrt(halfWidth * halfWidth + halfDepth * halfDepth) * DEFEND_RADIUS_SCALE;
		return true;
	}
}
