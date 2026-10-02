const test = require('node:test');
const assert = require('node:assert');
const { createCache } = require('../src/cache');
const { readConfig, SETTINGS } = require('../src/config');

function clock() {
  let at = 1000;
  const now = () => at;
  now.advance = ms => { at += ms; };
  return now;
}

test('a value is returned until its time is up, and not after', () => {
  const now = clock();
  const cache = createCache({ ttlMs: 60000, maxEntries: 10, now });
  assert.strictEqual(cache.get('a'), undefined);

  cache.set('a', 1);
  assert.strictEqual(cache.get('a'), 1);
  now.advance(59999);
  assert.strictEqual(cache.get('a'), 1);
  now.advance(1);
  assert.strictEqual(cache.get('a'), undefined);
  assert.strictEqual(cache.size(), 0);
});

test('reading a value does not make it live longer', () => {
  const now = clock();
  const cache = createCache({ ttlMs: 1000, maxEntries: 10, now });
  cache.set('a', 1);
  now.advance(900);
  assert.strictEqual(cache.get('a'), 1);
  now.advance(100);
  assert.strictEqual(cache.get('a'), undefined);
});

test('setting a value again restarts its time', () => {
  const now = clock();
  const cache = createCache({ ttlMs: 1000, maxEntries: 10, now });
  cache.set('a', 1);
  now.advance(900);
  cache.set('a', 2);
  now.advance(900);
  assert.strictEqual(cache.get('a'), 2);
});

test('at the size cap the least recently used value is dropped', () => {
  const cache = createCache({ ttlMs: 60000, maxEntries: 3, now: clock() });
  cache.set('a', 1);
  cache.set('b', 2);
  cache.set('c', 3);
  assert.strictEqual(cache.get('a'), 1);

  cache.set('d', 4);
  assert.strictEqual(cache.size(), 3);
  assert.strictEqual(cache.get('b'), undefined);
  assert.strictEqual(cache.get('a'), 1);
  assert.strictEqual(cache.get('c'), 3);
  assert.strictEqual(cache.get('d'), 4);
});

test('the cache never grows past its cap', () => {
  const cache = createCache({ ttlMs: 60000, maxEntries: 50, now: clock() });
  for (let i = 0; i < 5000; i++)
    cache.set('k' + i, i);
  assert.strictEqual(cache.size(), 50);
  assert.strictEqual(cache.get('k4999'), 4999);
  assert.strictEqual(cache.get('k4949'), undefined);
});

test('a removed value is gone', () => {
  const cache = createCache({ ttlMs: 60000, maxEntries: 3, now: clock() });
  cache.set('a', 1);
  cache.remove('a');
  cache.remove('never there');
  assert.strictEqual(cache.get('a'), undefined);
});

test('a time or a cap of zero keeps nothing', () => {
  const off = createCache({ ttlMs: 0, maxEntries: 10, now: clock() });
  off.set('a', 1);
  assert.strictEqual(off.get('a'), undefined);

  const empty = createCache({ ttlMs: 60000, maxEntries: 0, now: clock() });
  empty.set('a', 1);
  assert.strictEqual(empty.get('a'), undefined);
});

test('settings take their defaults when nothing is set', () => {
  assert.deepStrictEqual(readConfig({}), {
    pageCacheSeconds: 60,
    pageCacheEntries: 500,
    activeServerSeconds: 60,
    syncSeconds: 60,
    pageBudgetPerHour: 12,
    pageBudgetBurst: 20,
    serverPageSeconds: 120,
    globalPageSeconds: 300
  });
  assert.deepStrictEqual(readConfig(undefined), readConfig({}));
});

test('a setting outside its range is brought to the nearest end, and one that is not a whole number is ignored', () => {
  const config = readConfig({
    PAGE_CACHE_SECONDS: '100000',
    SYNC_INTERVAL_SECONDS: '1',
    PAGE_BUDGET_PER_HOUR: ' 120 ',
    PAGE_BUDGET_BURST: '-5',
    SERVER_PAGE_CACHE_SECONDS: '12.5',
    GLOBAL_PAGE_CACHE_SECONDS: 'soon',
    ACTIVE_SERVER_CACHE_SECONDS: '0'
  });
  assert.strictEqual(config.pageCacheSeconds, SETTINGS.pageCacheSeconds.max);
  assert.strictEqual(config.syncSeconds, SETTINGS.syncSeconds.min);
  assert.strictEqual(config.pageBudgetPerHour, 120);
  assert.strictEqual(config.pageBudgetBurst, 20);
  assert.strictEqual(config.serverPageSeconds, 120);
  assert.strictEqual(config.globalPageSeconds, 300);
  assert.strictEqual(config.activeServerSeconds, 0);
});
