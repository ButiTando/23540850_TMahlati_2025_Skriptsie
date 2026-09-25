# ILT Test Rig — web UI

`FirmwareUpdate/website/` — it sits beside the firmware it talks to, because it
is part of the same deliverable: the API it consumes is defined by
`ILT_OS/Applications/ILT_TESTRIG/TestRigApp.cpp`, and the two change together.

React front end for the `ILT_TESTRIG` firmware. Talks to the board's HTTP API,
draws the degrader mechanism, plots the six dosimeter channels, and records
every sample to a CSV you can download.

```sh
npm install
npm run dev                       # defaults to http://192.168.7.2
VITE_RIG=http://192.168.1.200 npm run dev   # a different rig
```

## Why it polls instead of using the stream

The firmware pushes records down a plain TCP socket on port 5000. **A browser
cannot open a raw TCP socket** — only HTTP, WebSocket, or SSE — so this app
cannot consume that stream directly. It polls `GET /api/dosimeter.json` at the
board's own sampling period instead, which is equivalent at 1 Hz and needs no
firmware change.

The stream is still the right interface for a control station or a logger
process; it is just not reachable from a browser. If push into the browser
becomes a requirement, Server-Sent Events is the cheapest firmware addition —
it is plain HTTP, one long-lived response, and needs none of the WebSocket
handshake.

## CORS

The board sends no CORS headers, so a browser blocks cross-origin fetches to it.
`vite.config.js` proxies `/api` to the rig in development so everything is
same-origin. For a deployed build, serve this app behind the same reverse proxy
as the board, or add the CORS header in the firmware.

## CSV recording

Recording is entirely client-side: samples accumulate in memory and are written
to a `Blob` that the browser downloads. Nothing is stored on the board, and no
server is involved. The buffer is capped (see `MAX_ROWS` in `lib/useRigData.js`)
so a long session cannot exhaust the tab's memory; the oldest rows are dropped
first and the UI says so.
