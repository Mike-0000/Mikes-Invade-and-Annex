const { Pool } = require('pg');

let pool;

// Connection settings come from the standard PGHOST, PGPORT, PGUSER, PGPASSWORD
// and PGDATABASE app settings. They point at the Supabase transaction pooler
// and the ia_game_api role, which can only execute the API's RPCs.
function getPool() {
  if (!pool) {
    pool = new Pool({
      max: 4,
      idleTimeoutMillis: 30000,
      connectionTimeoutMillis: 10000,
      ssl: { rejectUnauthorized: false }
    });
    // An idle connection dropped by the pooler must not take the worker down.
    pool.on('error', () => {});
  }
  return pool;
}

async function scalar(sql, params) {
  const result = await getPool().query(sql, params);
  return result.rows[0].value;
}

module.exports = { scalar };
