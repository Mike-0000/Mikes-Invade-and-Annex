// All occupying groups defend the whole base, rather than their spawn posts.
// Vanilla Defend uses a circle, not a rectangle: covering the corners also
// permits local movement outside the walls. This is intentionally a soft leash.
class IA_BaseGarrisonArea
{
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
		radius = Math.Sqrt(halfWidth * halfWidth + halfDepth * halfDepth);
		return true;
	}
}
