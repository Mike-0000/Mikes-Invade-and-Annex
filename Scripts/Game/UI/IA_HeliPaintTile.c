//------------------------------------------------------------------------------------------------
//! One livery in the paint bay: a small Huey in the skin's colour, its name and
//! whether the pilot may wear it. A locked skin shows how far the pilot's
//! rating has painted it. The bay owns the tiles and tells them their state;
//! a click goes back to the bay, which decides what it means.
//------------------------------------------------------------------------------------------------
class IA_HeliPaintTile : MUI_Node
{
	static const int STATE_WORN = 0;
	static const int STATE_READY = 1;
	static const int STATE_LOCKED = 2;
	static const int STATE_SYNCING = 3;
	static const int STATE_OFF = 4;		// this helicopter cannot be repainted

	static const float TILE_H = 92;
	protected static const float BEVEL = 4;
	protected static const int FONT_CAP = 9;
	protected static const float TRACK_CAP = 1.4;

	protected IA_HeliPaintBay m_Bay;
	protected int m_iSkinId;
	protected int m_iState = STATE_SYNCING;
	protected float m_fProgress;
	protected bool m_bPending;
	protected float m_fShake;
	protected string m_sLabel;
	protected string m_sIndex;
	protected string m_sLine;

	protected ref Color m_Skin;
	protected ref Color m_Bg;
	protected ref Color m_Tone;
	protected ref Color m_Green;
	protected ref Color m_Muted;
	protected ref Color m_White;
	protected ref Color m_Olive;
	protected ref Color m_Glass;
	protected ref IA_HueyArt m_Art;
	protected ref array<float> m_aBody = {};

	//------------------------------------------------------------------------------------------------
	void IA_HeliPaintTile()
	{
		m_Style.m_WidthMode = MUI_SizeMode.Exact;
		m_Style.m_HeightMode = MUI_SizeMode.Exact;
		m_Style.m_fHeight = TILE_H;
		m_Style.m_fMinHeight = TILE_H;
		m_Style.m_fRadius = 0;
		m_Style.m_bInteractive = true;
		m_Style.m_Fill = Color.FromInt(0);

		m_Bg = Color.FromSRGBA(18, 26, 23, 238);
		m_Tone = Color.FromSRGBA(255, 184, 72, 255);
		m_Green = Color.FromSRGBA(94, 251, 131, 255);
		m_Muted = Color.FromSRGBA(158, 168, 163, 255);
		m_White = Color.FromSRGBA(238, 242, 240, 255);
		m_Olive = Color.FromSRGBA(78, 88, 60, 255);
		m_Glass = Color.FromSRGBA(9, 14, 13, 235);
		m_Skin = Color.FromSRGBA(78, 88, 60, 255);
		m_Art = new IA_HueyArt();
	}

	//------------------------------------------------------------------------------------------------
	static IA_HeliPaintTile Create(notnull MUI_Runtime runtime, notnull IA_HeliPaintBay bay, int skinId, int index, string displayName, notnull Color skin)
	{
		ref IA_HeliPaintTile tile = new IA_HeliPaintTile();
		runtime.Adopt(tile);
		tile.SetName("livery" + index.ToString());
		tile.m_Bay = bay;
		tile.m_iSkinId = skinId;
		tile.m_Skin = skin;
		tile.m_sLabel = displayName;
		tile.m_sIndex = index.ToString();
		if (index < 10)
			tile.m_sIndex = "0" + tile.m_sIndex;
		return tile;
	}

	//------------------------------------------------------------------------------------------------
	override void ApplyTheme(notnull MUI_ThemeData theme)
	{
		m_Style.m_Fill = Color.FromInt(0);
	}

	//------------------------------------------------------------------------------------------------
	int GetSkinId()
	{
		return m_iSkinId;
	}

	//------------------------------------------------------------------------------------------------
	int GetState()
	{
		return m_iState;
	}

	//------------------------------------------------------------------------------------------------
	//! \param progress how much of a locked skin the pilot's rating has earned, 0..1
	//! \param line the short state text under the name
	void SetState(int state, float progress, string line, bool pending)
	{
		if (m_iState == state && m_fProgress == progress && m_sLine == line && m_bPending == pending)
			return;

		m_iState = state;
		m_fProgress = progress;
		m_sLine = line;
		m_bPending = pending;
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	//! Refuse a click: the tile shakes its head.
	void Shake()
	{
		m_fShake = 1;
		InvalidatePaint();
	}

	//------------------------------------------------------------------------------------------------
	override void OnTick(float dt)
	{
		if (m_fShake > 0)
		{
			m_fShake = m_fShake - dt * 2.6;
			if (m_fShake < 0)
				m_fShake = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnClicked()
	{
		if (m_Bay)
			m_Bay.OnTilePicked(this);
	}

	//------------------------------------------------------------------------------------------------
	protected Color StateColor()
	{
		if (m_iState == STATE_WORN)
			return m_Green;
		if (m_iState == STATE_READY)
			return m_Tone;
		return m_Muted;
	}

	//------------------------------------------------------------------------------------------------
	override void Paint(MUI_RenderSurface surface)
	{
		float op = GetDrawOpacity();
		if (op < 0.01)
			return;

		float hover = GetHoverT();
		float press = GetPressT();
		float time = GetTime();
		float x = DrawX() + Math.Sin(m_fShake * 28.0) * 5.0 * m_fShake;
		float y = DrawY() - hover * 4.0 + press * 2.0;
		float w = m_World.m_fW;
		float h = m_World.m_fH;
		Color accent = StateColor();
		bool dim = m_iState == STATE_OFF;
		float ink = 1.0;
		if (dim)
			ink = 0.4;

		// Body.
		m_aBody.Clear();
		m_aBody.Insert(x);
		m_aBody.Insert(y);
		m_aBody.Insert(x + w);
		m_aBody.Insert(y);
		m_aBody.Insert(x + w);
		m_aBody.Insert(y + h - BEVEL);
		m_aBody.Insert(x + w - BEVEL);
		m_aBody.Insert(y + h);
		m_aBody.Insert(x + BEVEL);
		m_aBody.Insert(y + h);
		m_aBody.Insert(x);
		m_aBody.Insert(y + h - BEVEL);

		if (hover > 0.02)
			surface.FillRect(x - 3, y - 3, w + 6, h + 6, MUI_ColorUtil.Fade(accent, op * hover * 0.035), 0);
		surface.FillPolygon(m_aBody, MUI_ColorUtil.Fade(m_Bg, op));
		surface.FillPolygon(m_aBody, MUI_ColorUtil.Fade(accent, op * (0.01 + hover * 0.028)));
		if (m_iState == STATE_WORN)
			surface.FillGradientV(x, y, w, h * 0.5, MUI_ColorUtil.Fade(m_Green, op * 0.035), MUI_ColorUtil.Fade(m_Green, 0), 8);

		float edgeOp = 0.20 + hover * 0.75;
		if (m_iState == STATE_WORN && edgeOp < 0.55)
			edgeOp = 0.55;
		surface.StrokePolyline(m_aBody, MUI_ColorUtil.Fade(accent, op * edgeOp * ink), 1.0 + hover * 0.8, true);

		// The skin's own colour, as a paint chip down the left edge.
		surface.FillRect(x, y, 4, h - BEVEL, MUI_ColorUtil.Fade(m_Skin, op * ink), 0);

		float pad = 14;
		float right = x + w - 10;
		IA_TrackedText.Draw(surface, m_Runtime, x + pad, y + 7, 12, m_sIndex, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(m_Muted, op * 0.8 * ink));

		// Helicopter.
		float k = (w - pad - 12) / IA_HueyArt.W;
		if (k > 1.0)
			k = 1.0;
		float hx = x + pad;
		float hy = y + 20;
		float rotor = 0.5;
		if (hover > 0.05)
			rotor = time * 11.0;

		if (m_iState == STATE_LOCKED || m_iState == STATE_SYNCING)
		{
			m_Art.FillHull(surface, hx, hy, k, MUI_ColorUtil.Fade(m_Olive, op * 0.55));
			m_Art.FillHull(surface, hx, hy, k, MUI_ColorUtil.Fade(m_Skin, op * (0.10 + hover * 0.10)));
			if (m_iState == STATE_LOCKED)
			{
				m_Art.FillHullPainted(surface, hx, hy, k, MUI_ColorUtil.Fade(m_Skin, op), m_fProgress);
				float beat = 0.65 + 0.35 * MUI_Ease.Pulse(time, 0.9);
				m_Art.DrawFront(surface, hx, hy, k, MUI_ColorUtil.Fade(m_Tone, op * beat), m_fProgress);
			}
		}
		else
		{
			m_Art.FillHull(surface, hx, hy, k, MUI_ColorUtil.Fade(m_Skin, op * ink));
		}
		m_Art.FillGlass(surface, hx, hy, k, MUI_ColorUtil.Fade(m_Glass, op));
		m_Art.DrawRig(surface, hx, hy, k, MUI_ColorUtil.Fade(m_White, op * 0.72 * ink), MUI_ColorUtil.Fade(m_White, op * 0.18 * ink), rotor);

		if (m_bPending || m_iState == STATE_SYNCING)
		{
			float sweep = time * 0.9;
			sweep = sweep - Math.Floor(sweep);
			m_Art.FillShine(surface, hx, hy, k, MUI_ColorUtil.Fade(m_Tone, op * 0.55), sweep);
		}

		// Name and state.
		int nameSize = 13;
		if (w < 170)
			nameSize = 10;
		surface.DrawText(x + pad, y + h - 30, w - pad - 8, 16, m_sLabel, nameSize, MUI_ColorUtil.Fade(m_White, op * ink), true, false, true, false, true);

		float lineX = x + pad;
		if (m_iState == STATE_LOCKED)
		{
			DrawLock(surface, lineX, y + h - 13, MUI_ColorUtil.Fade(m_Muted, op));
			lineX = lineX + 12;
		}
		IA_TrackedText.Draw(surface, m_Runtime, lineX, y + h - 15, 12, m_sLine, FONT_CAP, TRACK_CAP, MUI_ColorUtil.Fade(accent, op * ink));

		// Worn mark in the corner.
		if (m_iState == STATE_WORN)
		{
			float pulse = 0.6 + 0.4 * MUI_Ease.Pulse(time, 0.7);
			surface.FillCircle(right - 3, y + 13, 3, MUI_ColorUtil.Fade(m_Green, op * pulse));
			surface.StrokeCircle(right - 3, y + 13, 5.5, MUI_ColorUtil.Fade(m_Green, op * 0.35 * pulse), 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawLock(MUI_RenderSurface surface, float x, float y, Color color)
	{
		surface.FillRect(x, y + 3.5, 8, 6, color, 1);
		surface.StrokeRect(x + 1.5, y, 5, 7, color, 1.2, 2.5);
	}
}
