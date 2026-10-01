//------------------------------------------------------------------------------------------------
//! A helicopter drawn for the pilot card and the paint bay: side view, nose
//! left, in a 120x40 box that is scaled by k. The hull can be painted from the
//! nose to a slanted front, which is how both a repaint and the progress
//! towards a locked livery are shown.
//!
//! This class only draws; the shape is data. A silhouette is a subclass whose
//! constructor adds its pieces (IA_HeliArtHuey, IA_HeliArtHip), and a paint
//! family names one with the "art" key of its registry entry. A family whose
//! key is not known here, such as a modded helicopter, gets IA_HeliArtGeneric.
//! Every filled piece must be convex.
//! One instance per node that draws; the scratch arrays are reused every frame.
//------------------------------------------------------------------------------------------------
class IA_HeliArt
{
	static const float W = 120;
	static const float H = 40;
	static const string ART_HUEY = "huey";
	static const string ART_HIP = "hip";
	protected static const float SKEW = 0.35;
	protected static const float SPAN_LO = 6.0;
	protected static const float SPAN_HI = 112.5;

	// Painted hull pieces, glazing and dark fittings.
	protected ref array<ref array<float>> m_aHull = {};
	protected ref array<ref array<float>> m_aGlass = {};
	protected ref array<ref array<float>> m_aDark = {};
	// Round windows as x, y, radius.
	protected ref array<float> m_aPorts = {};
	// Light and shade: the half of a hull piece where nx * x + ny * y <= limit.
	protected ref array<ref array<float>> m_aShadePiece = {};
	protected ref array<float> m_aShadeHalf = {};
	protected ref array<bool> m_aShadeLight = {};
	// Undercarriage as x0, y0, x1, y1, width scale, and wheels as x, y, radius.
	protected ref array<float> m_aGear = {};
	protected ref array<float> m_aWheels = {};
	// Panel lines as x0, y0, x1, y1, width in pixels.
	protected ref array<float> m_aLines = {};
	// Ground shadow bars as x, y, width, height.
	protected ref array<float> m_aShadows = {};

	// Main rotor hub and blade half span, mast height, tail rotor centre and radius.
	protected float m_fRotorX = 53;
	protected float m_fRotorY = 4.7;
	protected float m_fRotorHalf = 50;
	protected float m_fMastH = 5.5;
	protected float m_fTailX = 115.5;
	protected float m_fTailY = 5.5;
	protected float m_fTailR = 4.2;

	protected ref array<float> m_aClipA = {};
	protected ref array<float> m_aClipB = {};
	protected ref array<float> m_aDraw = {};

	//------------------------------------------------------------------------------------------------
	//! \param art the "art" key of a paint family
	//! \return the silhouette for it, the generic helicopter when the key is not known
	static IA_HeliArt Create(string art)
	{
		if (art == ART_HUEY)
			return new IA_HeliArtHuey();
		if (art == ART_HIP)
			return new IA_HeliArtHip();
		return new IA_HeliArtGeneric();
	}

	//------------------------------------------------------------------------------------------------
	//! The whole hull in one colour.
	void FillHull(MUI_RenderSurface surface, float x, float y, float k, Color color)
	{
		foreach (array<float> piece : m_aHull)
		{
			FillPiece(surface, piece, x, y, k, color);
		}
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
		foreach (array<float> piece : m_aHull)
		{
			ClipHalf(piece, 1, SKEW, limit, m_aClipA);
			FillPiece(surface, m_aClipA, x, y, k, color);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A slanted band of light across the hull; sweep runs 0..1 from before the nose to past the tail.
	void FillShine(MUI_RenderSurface surface, float x, float y, float k, Color color, float sweep)
	{
		float from = SPAN_LO - 12 + (SPAN_HI - SPAN_LO + 24) * sweep + SKEW * H * 0.5;
		foreach (array<float> piece : m_aHull)
		{
			ClipHalf(piece, 1, SKEW, from + 9, m_aClipA);
			if (m_aClipA.Count() < 6)
				continue;
			ClipHalf(m_aClipA, -1, -SKEW, -from, m_aClipB);
			FillPiece(surface, m_aClipB, x, y, k, color);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Light along the roof and shade under the belly, so a flat colour reads as a hull.
	void FillHullShade(MUI_RenderSurface surface, float x, float y, float k, Color light, Color dark)
	{
		int count = m_aShadePiece.Count();
		int i;
		for (i = 0; i < count; i++)
		{
			ClipHalf(m_aShadePiece[i], m_aShadeHalf[i * 3], m_aShadeHalf[i * 3 + 1], m_aShadeHalf[i * 3 + 2], m_aClipA);
			if (m_aShadeLight[i])
				FillPiece(surface, m_aClipA, x, y, k, light);
			else
				FillPiece(surface, m_aClipA, x, y, k, dark);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The line of the paint front, drawn while a repaint runs or a livery is still locked.
	void DrawFront(MUI_RenderSurface surface, float x, float y, float k, Color color, float front)
	{
		float fx = FrontX(front);
		surface.DrawLine(x + (fx + SKEW * 12) * k, y + 8 * k, x + (fx - SKEW * 13) * k, y + 33 * k, color, LineWidth(k));
	}

	//------------------------------------------------------------------------------------------------
	void FillGlass(MUI_RenderSurface surface, float x, float y, float k, Color color)
	{
		foreach (array<float> piece : m_aGlass)
		{
			FillPiece(surface, piece, x, y, k, color);
		}

		int count = m_aPorts.Count();
		int i;
		for (i = 0; i < count; i = i + 3)
		{
			surface.FillCircle(x + m_aPorts[i] * k, y + m_aPorts[i + 1] * k, m_aPorts[i + 2] * k, color);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Outline of every hull piece, as on a technical drawing.
	void StrokeHull(MUI_RenderSurface surface, float x, float y, float k, Color color, float width)
	{
		foreach (array<float> piece : m_aHull)
		{
			if (Place(piece, x, y, k))
				surface.StrokePolyline(m_aDraw, color, width, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rotors, mast and undercarriage. rotor is the main rotor angle in radians.
	void DrawRig(MUI_RenderSurface surface, float x, float y, float k, Color color, Color faint, float rotor)
	{
		float lw = LineWidth(k);

		// Main rotor, seen edge on: the blades shorten and lengthen as they turn.
		surface.DrawLine(x + (m_fRotorX - m_fRotorHalf) * k, y + (m_fRotorY + 0.5) * k, x + (m_fRotorX + m_fRotorHalf) * k, y + (m_fRotorY - 0.5) * k, faint, lw * 0.8);
		float half = m_fRotorHalf * Math.AbsFloat(Math.Cos(rotor));
		if (half > 2)
			surface.DrawLine(x + (m_fRotorX - half) * k, y + (m_fRotorY + half * 0.01) * k, x + (m_fRotorX + half) * k, y + (m_fRotorY - half * 0.01) * k, color, lw);
		surface.FillRect(x + (m_fRotorX - 2) * k, y + (m_fRotorY - 0.2) * k, 3 * k, m_fMastH * k, color, 0);

		// Skids or wheels.
		int count = m_aGear.Count();
		int i;
		for (i = 0; i < count; i = i + 5)
		{
			surface.DrawLine(x + m_aGear[i] * k, y + m_aGear[i + 1] * k, x + m_aGear[i + 2] * k, y + m_aGear[i + 3] * k, color, lw * m_aGear[i + 4]);
		}
		// A tyre is dimmer than its leg: a bright disc reads as a lamp.
		count = m_aWheels.Count();
		for (i = 0; i < count; i = i + 3)
		{
			surface.FillCircle(x + m_aWheels[i] * k, y + m_aWheels[i + 1] * k, m_aWheels[i + 2] * k, faint);
		}

		// Tail rotor. Two open 180° arcs: a closed 360° polyline leaves a spoke through the ring.
		float tx = x + m_fTailX * k;
		float ty = y + m_fTailY * k;
		surface.DrawArc(tx, ty, m_fTailR * k, 0, 180, faint, lw * 0.8);
		surface.DrawArc(tx, ty, m_fTailR * k, 180, 180, faint, lw * 0.8);
		float ta = rotor * 1.7;
		float dx = Math.Cos(ta) * m_fTailR * 0.86 * k;
		float dy = Math.Sin(ta) * m_fTailR * 0.86 * k;
		surface.DrawLine(tx - dx, ty - dy, tx + dx, ty + dy, color, lw * 0.86);
	}

	//------------------------------------------------------------------------------------------------
	//! Panel lines and fittings that only read at a large size.
	void DrawDetail(MUI_RenderSurface surface, float x, float y, float k, Color line, Color dark)
	{
		foreach (array<float> piece : m_aDark)
		{
			FillPiece(surface, piece, x, y, k, dark);
		}

		// Rotor head.
		surface.FillRect(x + (m_fRotorX - 4.5) * k, y + (m_fRotorY - 1.1) * k, 8 * k, 1.7 * k, line, 0);

		int count = m_aLines.Count();
		int i;
		for (i = 0; i < count; i = i + 5)
		{
			surface.DrawLine(x + m_aLines[i] * k, y + m_aLines[i + 1] * k, x + m_aLines[i + 2] * k, y + m_aLines[i + 3] * k, line, m_aLines[i + 4]);
		}
	}

	//------------------------------------------------------------------------------------------------
	void DrawShadow(MUI_RenderSurface surface, float x, float y, float k, Color color)
	{
		int count = m_aShadows.Count();
		int i;
		for (i = 0; i < count; i = i + 4)
		{
			surface.FillRect(x + m_aShadows[i] * k, y + m_aShadows[i + 1] * k, m_aShadows[i + 2] * k, m_aShadows[i + 3] * k, color, m_aShadows[i + 3] * 0.5 * k);
		}
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
	protected void AddShade(notnull array<float> piece, float nx, float ny, float limit, bool light)
	{
		m_aShadePiece.Insert(piece);
		m_aShadeHalf.Insert(nx);
		m_aShadeHalf.Insert(ny);
		m_aShadeHalf.Insert(limit);
		m_aShadeLight.Insert(light);
	}

	//------------------------------------------------------------------------------------------------
	//! Add five numbers to one of the line tables, or the first values of a shorter record.
	protected void AddRecord(notnull array<float> table, float a, float b, float c, float d, float e)
	{
		table.Insert(a);
		table.Insert(b);
		table.Insert(c);
		table.Insert(d);
		table.Insert(e);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddRound(notnull array<float> table, float cx, float cy, float radius)
	{
		table.Insert(cx);
		table.Insert(cy);
		table.Insert(radius);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddShadow(float sx, float sy, float width, float height)
	{
		m_aShadows.Insert(sx);
		m_aShadows.Insert(sy);
		m_aShadows.Insert(width);
		m_aShadows.Insert(height);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillPiece(MUI_RenderSurface surface, notnull array<float> piece, float x, float y, float k, Color color)
	{
		if (!Place(piece, x, y, k))
			return;
		surface.FillPolygon(m_aDraw, color);
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
