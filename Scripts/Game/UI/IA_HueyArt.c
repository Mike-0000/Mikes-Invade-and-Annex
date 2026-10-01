//------------------------------------------------------------------------------------------------
//! The Huey of the pilot card, drawn at any size for the paint bay: side view,
//! nose left, in a 120x40 box that is scaled by k. Every filled piece is convex.
//! The hull can be painted from the nose to a slanted front, which is how both
//! a repaint and the progress towards a locked skin are shown.
//! One instance per node that draws; the scratch arrays are reused every frame.
//------------------------------------------------------------------------------------------------
class IA_HueyArt
{
	static const float W = 120;
	static const float H = 40;
	protected static const float SKEW = 0.35;
	protected static const float SPAN_LO = 6.0;
	protected static const float SPAN_HI = 112.5;

	protected static const ref array<float> FUSELAGE = {5, 23, 8, 18.5, 15, 15.2, 26, 14, 72, 14, 78, 17, 80, 21.5, 75, 28.5, 64, 31, 18, 31, 9, 28.5, 5.5, 25.5};
	protected static const ref array<float> COWL = {40, 14, 43, 9.5, 64, 9.5, 71, 14};
	protected static const ref array<float> BOOM = {76, 16, 112, 12, 112, 15.5, 78, 22.5};
	protected static const ref array<float> FIN = {107, 15, 113.5, 2, 117.5, 2, 113.5, 15.6};
	protected static const ref array<float> STAB = {92, 15.2, 103, 14, 103, 16, 92, 17.4};
	protected static const ref array<float> WINDSHIELD = {9.5, 21.5, 11.5, 18.3, 16.5, 16.2, 22, 15.6, 22, 21.5};
	protected static const ref array<float> DOOR_FRONT = {26, 16, 39, 16, 39, 21.5, 26, 21.5};
	protected static const ref array<float> DOOR_REAR = {43, 16, 54, 16, 54, 21.5, 43, 21.5};
	protected static const ref array<float> CHIN = {8.6, 25.4, 13.5, 24.6, 15.5, 28.6, 11.2, 28.4};
	protected static const ref array<float> EXHAUST = {66.5, 10.3, 73.5, 10.8, 73.5, 13.2, 69.5, 14};

	protected ref array<float> m_aClipA = {};
	protected ref array<float> m_aClipB = {};
	protected ref array<float> m_aDraw = {};

	//------------------------------------------------------------------------------------------------
	//! The whole hull in one colour.
	void FillHull(MUI_RenderSurface surface, float x, float y, float k, Color color)
	{
		FillPiece(surface, BOOM, x, y, k, color);
		FillPiece(surface, FIN, x, y, k, color);
		FillPiece(surface, STAB, x, y, k, color);
		FillPiece(surface, COWL, x, y, k, color);
		FillPiece(surface, FUSELAGE, x, y, k, color);
	}

	//------------------------------------------------------------------------------------------------
	//! The hull from the nose as far as the paint front; front is 0 at the nose and 1 at the tail.
	void FillHullPainted(MUI_RenderSurface surface, float x, float y, float k, Color color, float front)
	{
		if (front <= 0)
			return;
		if (front >= 1)
		{
			FillHull(surface, x, y, k, color);
			return;
		}

		float limit = FrontX(front) + SKEW * H * 0.5;
		FillClipped(surface, BOOM, x, y, k, color, limit);
		FillClipped(surface, FIN, x, y, k, color, limit);
		FillClipped(surface, STAB, x, y, k, color, limit);
		FillClipped(surface, COWL, x, y, k, color, limit);
		FillClipped(surface, FUSELAGE, x, y, k, color, limit);
	}

	//------------------------------------------------------------------------------------------------
	//! A slanted band of light across the hull; sweep runs 0..1 from before the nose to past the tail.
	void FillShine(MUI_RenderSurface surface, float x, float y, float k, Color color, float sweep)
	{
		float from = SPAN_LO - 12 + (SPAN_HI - SPAN_LO + 24) * sweep + SKEW * H * 0.5;
		FillBand(surface, BOOM, x, y, k, color, from);
		FillBand(surface, FIN, x, y, k, color, from);
		FillBand(surface, COWL, x, y, k, color, from);
		FillBand(surface, FUSELAGE, x, y, k, color, from);
	}

	//------------------------------------------------------------------------------------------------
	//! Light along the roof and shade under the belly, so a flat colour reads as a hull.
	void FillHullShade(MUI_RenderSurface surface, float x, float y, float k, Color light, Color dark)
	{
		ClipHalf(FUSELAGE, 0, 1, 18.2, m_aClipA);
		FillPiece(surface, m_aClipA, x, y, k, light);
		ClipHalf(COWL, 0, 1, 11.2, m_aClipA);
		FillPiece(surface, m_aClipA, x, y, k, light);
		ClipHalf(FUSELAGE, 0, -1, -26.4, m_aClipA);
		FillPiece(surface, m_aClipA, x, y, k, dark);
		ClipHalf(BOOM, -0.206, -1, -37.3, m_aClipA);
		FillPiece(surface, m_aClipA, x, y, k, dark);
	}

	//------------------------------------------------------------------------------------------------
	//! The line of the paint front, drawn while a repaint runs or a skin is still locked.
	void DrawFront(MUI_RenderSurface surface, float x, float y, float k, Color color, float front)
	{
		float fx = FrontX(front);
		surface.DrawLine(x + (fx + SKEW * 12) * k, y + 8 * k, x + (fx - SKEW * 13) * k, y + 33 * k, color, LineWidth(k));
	}

	//------------------------------------------------------------------------------------------------
	void FillGlass(MUI_RenderSurface surface, float x, float y, float k, Color color)
	{
		FillPiece(surface, WINDSHIELD, x, y, k, color);
		FillPiece(surface, DOOR_FRONT, x, y, k, color);
		FillPiece(surface, DOOR_REAR, x, y, k, color);
	}

	//------------------------------------------------------------------------------------------------
	//! Outline of every hull piece, as on a technical drawing.
	void StrokeHull(MUI_RenderSurface surface, float x, float y, float k, Color color, float width)
	{
		StrokePiece(surface, BOOM, x, y, k, color, width);
		StrokePiece(surface, FIN, x, y, k, color, width);
		StrokePiece(surface, STAB, x, y, k, color, width);
		StrokePiece(surface, COWL, x, y, k, color, width);
		StrokePiece(surface, FUSELAGE, x, y, k, color, width);
	}

	//------------------------------------------------------------------------------------------------
	//! Rotors, mast and skids. rotor is the main rotor angle in radians.
	void DrawRig(MUI_RenderSurface surface, float x, float y, float k, Color color, Color faint, float rotor)
	{
		float lw = LineWidth(k);

		// Main rotor, seen edge on: the blades shorten and lengthen as they turn.
		surface.DrawLine(x + 3 * k, y + 5.2 * k, x + 103 * k, y + 4.2 * k, faint, lw * 0.8);
		float half = 50 * Math.AbsFloat(Math.Cos(rotor));
		if (half > 2)
			surface.DrawLine(x + (53 - half) * k, y + (4.7 + half * 0.01) * k, x + (53 + half) * k, y + (4.7 - half * 0.01) * k, color, lw);
		surface.FillRect(x + 51 * k, y + 4.5 * k, 3 * k, 5.5 * k, color, 0);

		// Skids.
		surface.DrawLine(x + 13 * k, y + 36.5 * k, x + 66 * k, y + 36.5 * k, color, lw);
		surface.DrawLine(x + 13 * k, y + 36.5 * k, x + 9.5 * k, y + 33.5 * k, color, lw);
		surface.DrawLine(x + 27 * k, y + 31 * k, x + 25 * k, y + 36.5 * k, color, lw * 0.86);
		surface.DrawLine(x + 56 * k, y + 31 * k, x + 58 * k, y + 36.5 * k, color, lw * 0.86);

		// Tail rotor.
		float tx = x + 115.5 * k;
		float ty = y + 5.5 * k;
		surface.DrawArc(tx, ty, 4.2 * k, 0, 180, faint, lw * 0.8);
		surface.DrawArc(tx, ty, 4.2 * k, 180, 180, faint, lw * 0.8);
		float ta = rotor * 1.7;
		float dx = Math.Cos(ta) * 3.6 * k;
		float dy = Math.Sin(ta) * 3.6 * k;
		surface.DrawLine(tx - dx, ty - dy, tx + dx, ty + dy, color, lw * 0.86);
	}

	//------------------------------------------------------------------------------------------------
	//! Panel lines and fittings that only read at a large size.
	void DrawDetail(MUI_RenderSurface surface, float x, float y, float k, Color line, Color dark)
	{
		FillPiece(surface, CHIN, x, y, k, dark);
		FillPiece(surface, EXHAUST, x, y, k, dark);

		// Rotor head.
		surface.FillRect(x + 48.5 * k, y + 3.6 * k, 8 * k, 1.7 * k, line, 0);

		// Cabin door, its track and the pilot's door.
		surface.DrawLine(x + 40.6 * k, y + 14.6 * k, x + 40.6 * k, y + 30.6 * k, line, 1);
		surface.DrawLine(x + 58 * k, y + 14.6 * k, x + 58 * k, y + 30.6 * k, line, 1);
		surface.DrawLine(x + 40.6 * k, y + 23.4 * k, x + 73 * k, y + 23.4 * k, line, 1);
		surface.DrawLine(x + 24.4 * k, y + 15 * k, x + 24.4 * k, y + 30.6 * k, line, 1);

		// Tail boom seam, tail skid and the pitot at the nose.
		surface.DrawLine(x + 80 * k, y + 19.2 * k, x + 110 * k, y + 14 * k, line, 1);
		surface.DrawLine(x + 104.5 * k, y + 17 * k, x + 109 * k, y + 20.5 * k, line, 1.4);
		surface.DrawLine(x + 5.2 * k, y + 23.6 * k, x + 2 * k, y + 24.2 * k, line, 1.4);

		// Skid heel and cross tubes.
		surface.DrawLine(x + 66 * k, y + 36.5 * k, x + 69 * k, y + 35.2 * k, line, 1.4);
	}

	//------------------------------------------------------------------------------------------------
	void DrawShadow(MUI_RenderSurface surface, float x, float y, float k, Color color)
	{
		surface.FillRect(x + 6 * k, y + 38 * k, 66 * k, 1.5 * k, color, 0.75 * k);
		surface.FillRect(x + 76 * k, y + 38.3 * k, 38 * k, 0.9 * k, color, 0.45 * k);
	}

	//------------------------------------------------------------------------------------------------
	//! \return x of the paint front in the 120x40 box
	static float FrontX(float front)
	{
		return SPAN_LO + (SPAN_HI - SPAN_LO) * front;
	}

	//------------------------------------------------------------------------------------------------
	protected static float LineWidth(float k)
	{
		return 1.0 + 0.4 * k;
	}

	//------------------------------------------------------------------------------------------------
	protected void FillClipped(MUI_RenderSurface surface, notnull array<float> piece, float x, float y, float k, Color color, float limit)
	{
		ClipHalf(piece, 1, SKEW, limit, m_aClipA);
		FillPiece(surface, m_aClipA, x, y, k, color);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillBand(MUI_RenderSurface surface, notnull array<float> piece, float x, float y, float k, Color color, float from)
	{
		ClipHalf(piece, 1, SKEW, from + 9, m_aClipA);
		if (m_aClipA.Count() < 6)
			return;
		ClipHalf(m_aClipA, -1, -SKEW, -from, m_aClipB);
		FillPiece(surface, m_aClipB, x, y, k, color);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillPiece(MUI_RenderSurface surface, notnull array<float> piece, float x, float y, float k, Color color)
	{
		if (!Place(piece, x, y, k))
			return;
		surface.FillPolygon(m_aDraw, color);
	}

	//------------------------------------------------------------------------------------------------
	protected void StrokePiece(MUI_RenderSurface surface, notnull array<float> piece, float x, float y, float k, Color color, float width)
	{
		if (!Place(piece, x, y, k))
			return;
		surface.StrokePolyline(m_aDraw, color, width, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Scale a piece and move it to its place on screen, into m_aDraw.
	protected bool Place(notnull array<float> piece, float x, float y, float k)
	{
		int count = piece.Count();
		if (count < 6)
			return false;

		m_aDraw.Clear();
		int i;
		for (i = 0; i < count; i = i + 2)
		{
			m_aDraw.Insert(piece[i] * k + x);
			m_aDraw.Insert(piece[i + 1] * k + y);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Keep the part of a convex polygon where nx * x + ny * y <= limit.
	protected void ClipHalf(notnull array<float> src, float nx, float ny, float limit, notnull array<float> dst)
	{
		dst.Clear();
		int points = src.Count() / 2;
		int i;
		for (i = 0; i < points; i++)
		{
			int j = i + 1;
			if (j >= points)
				j = 0;

			float ax = src[i * 2];
			float ay = src[i * 2 + 1];
			float bx = src[j * 2];
			float by = src[j * 2 + 1];
			float da = nx * ax + ny * ay - limit;
			float db = nx * bx + ny * by - limit;
			if (da <= 0)
			{
				dst.Insert(ax);
				dst.Insert(ay);
			}
			if ((da < 0 && db > 0) || (db < 0 && da > 0))
			{
				float t = da / (da - db);
				dst.Insert(ax + (bx - ax) * t);
				dst.Insert(ay + (by - ay) * t);
			}
		}
	}
}
