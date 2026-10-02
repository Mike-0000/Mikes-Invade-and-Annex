// A small in-memory cache. An entry is dropped when its time is up, and the
// least recently used one is dropped at the size cap. Each Function instance
// has its own, and an instance can go away at any moment, so nothing may
// depend on a hit.

function createCache(options) {
  const ttlMs = options.ttlMs;
  const maxEntries = options.maxEntries;
  const now = options.now || Date.now;
  const entries = new Map();

  function get(key) {
    const entry = entries.get(key);
    if (!entry)
      return undefined;
    if (entry.expires <= now()) {
      entries.delete(key);
      return undefined;
    }
    // A Map keeps insertion order, so the first key is the least recently used.
    entries.delete(key);
    entries.set(key, entry);
    return entry.value;
  }

  function set(key, value) {
    if (ttlMs <= 0 || maxEntries <= 0)
      return;
    entries.delete(key);
    entries.set(key, { value, expires: now() + ttlMs });
    while (entries.size > maxEntries)
      entries.delete(entries.keys().next().value);
  }

  function remove(key) {
    entries.delete(key);
  }

  function size() {
    return entries.size;
  }

  return { get, set, remove, size };
}

module.exports = { createCache };
