import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

/*
 * The rig serves its API from its own origin and sends no CORS headers, so a
 * browser will not let this app fetch it cross-origin. In development Vite
 * proxies /api to the rig instead, which keeps every request same-origin from
 * the browser's point of view.
 *
 * The default target is the simulated rig published by tools/dev-rig.sh, which
 * is what front-end work should normally run against: it is always up, and it
 * does not depend on a board being plugged in. Point it somewhere else with:
 *
 *   VITE_RIG=http://192.168.1.200 npm run dev     (the NUCLEO on the bench)
 *   VITE_RIG=http://192.168.7.2   npm run dev     (a sim in this namespace)
 */
const RIG = process.env.VITE_RIG || 'http://127.0.0.1:8080';

export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      '/api': {
        target: RIG,
        changeOrigin: true,
        // Without this the browser only sees an opaque 500 when the rig is
        // unplugged or its link has dropped, which reads as a front-end bug.
        configure(proxy) {
          proxy.on('error', (err, _req, res) => {
            const message = `rig unreachable at ${RIG}: ${err.message}`;
            console.warn(`[rig] ${message}`);
            if (res.writeHead && !res.headersSent) {
              res.writeHead(502, { 'Content-Type': 'application/json' });
            }
            if (res.end) res.end(JSON.stringify({ error: message }));
          });
        },
      },
    },
  },
});
