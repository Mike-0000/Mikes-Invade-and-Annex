const { app } = require('@azure/functions');
const db = require('../db');
const { createHandlers } = require('../handlers');
const { readConfig } = require('../config');
const { ROUTES } = require('../routes');

const handlers = createHandlers(db, readConfig(process.env));

// The game sends no credentials, so every route is anonymous. Writes are
// accepted only for a registered, active server GUID, checked in the database.
function route(name, method, handler) {
  app.http(name, {
    methods: [method],
    authLevel: 'anonymous',
    route: name,
    handler: async (request, context) => {
      try {
        return await handler(request);
      } catch (e) {
        context.error(name + ' failed: ' + (e && e.message));
        return { status: 500, body: 'Internal error.' };
      }
    }
  });
}

for (const entry of ROUTES)
  route(entry.name, entry.method, handlers[entry.handler]);
