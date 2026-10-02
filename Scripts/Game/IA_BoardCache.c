//------------------------------------------------------------------------------------------------
//! Server: answers of the stats service kept by key, up to a fixed number. When one too many
//! goes in, the one held longest is dropped and the rest stay.
//------------------------------------------------------------------------------------------------
class IA_BoardCache
{
	protected int m_iMax;
	protected ref map<string, ref IA_BoardPage> m_mPages = new map<string, ref IA_BoardPage>();
	protected ref array<string> m_aOrder = {};	// keys, the one held longest first

	//------------------------------------------------------------------------------------------------
	void IA_BoardCache(int max)
	{
		m_iMax = max;
		if (m_iMax < 1)
			m_iMax = 1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the answer under a key however old it is, null when none is held
	IA_BoardPage Get(string key)
	{
		return m_mPages.Get(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Keep an answer. One already under the key is replaced and counts as just arrived.
	void Put(string key, notnull IA_BoardPage page)
	{
		if (m_mPages.Contains(key))
			m_aOrder.RemoveItemOrdered(key);
		m_mPages.Set(key, page);
		m_aOrder.Insert(key);

		while (m_aOrder.Count() > m_iMax)
		{
			m_mPages.Remove(m_aOrder[0]);
			m_aOrder.RemoveOrdered(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_aOrder.Count();
	}
}
