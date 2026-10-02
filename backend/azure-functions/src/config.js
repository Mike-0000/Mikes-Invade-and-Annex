// Settings read from app settings (environment variables). A value that is
// missing or not a whole number takes the default; one outside its range is
// brought to the nearest end of it.

const SETTINGS = {
  // How long this instance keeps a global or servers page. 0 turns it off.
  pageCacheSeconds:    { env: 'PAGE_CACHE_SECONDS',          fallback: 60,  min: 0,  max: 600 },
  pageCacheEntries:    { env: 'PAGE_CACHE_ENTRIES',          fallback: 500, min: 0,  max: 5000 },
  // How long a server GUID seen to be active is trusted for a cached page.
  activeServerSeconds: { env: 'ACTIVE_SERVER_CACHE_SECONDS', fallback: 60,  min: 0,  max: 300 },

  // Hints sent with every /sync answer. The game obeys them within its own limits.
  syncSeconds:         { env: 'SYNC_INTERVAL_SECONDS',       fallback: 60,  min: 30, max: 3600 },
  pageBudgetPerHour:   { env: 'PAGE_BUDGET_PER_HOUR',        fallback: 12,  min: 0,  max: 3600 },
  pageBudgetBurst:     { env: 'PAGE_BUDGET_BURST',           fallback: 20,  min: 0,  max: 500 },
  serverPageSeconds:   { env: 'SERVER_PAGE_CACHE_SECONDS',   fallback: 120, min: 30, max: 3600 },
  globalPageSeconds:   { env: 'GLOBAL_PAGE_CACHE_SECONDS',   fallback: 300, min: 60, max: 3600 }
};

function readConfig(env) {
  const config = {};
  for (const [name, setting] of Object.entries(SETTINGS)) {
    const text = env ? env[setting.env] : undefined;
    let value = setting.fallback;
    if (typeof text === 'string' && /^[0-9]{1,9}$/.test(text.trim()))
      value = Number(text.trim());
    config[name] = Math.min(Math.max(value, setting.min), setting.max);
  }
  return config;
}

module.exports = { readConfig, SETTINGS };
