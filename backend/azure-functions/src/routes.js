// Every route of the API: its path under /api, its method and the handler in
// handlers.js that answers it. The Functions app and the local dev server both
// read this list, so they serve the same routes.

const ROUTES = [
  { name: 'registerServer', method: 'POST', handler: 'registerServer' },
  { name: 'submitStats', method: 'POST', handler: 'submitStats' },
  { name: 'getAllLeaderboards', method: 'GET', handler: 'getAllLeaderboards' },
  { name: 'leaderboard', method: 'GET', handler: 'getLeaderboard' },
  { name: 'submitTransport', method: 'POST', handler: 'submitTransport' },
  { name: 'getTransportRatings', method: 'POST', handler: 'getTransportRatings' },
  { name: 'sync', method: 'POST', handler: 'sync' }
];

module.exports = { ROUTES };
