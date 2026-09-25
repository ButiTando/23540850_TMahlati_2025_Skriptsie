/**
  ******************************************************************************
  * @file    IndexPage.h
  * @brief   The served page, as a string literal in flash.
  *
  * Kept in its own header so the markup can be edited without scrolling past
  * the thread that serves it. It is a raw string literal rather than a
  * makefsdata blob, so it stays diffable and needs no build-time tool.
  *
  * Self-contained by necessity: the board has no internet route to a CDN and
  * serves nothing but the routes registered in WebServerApp.cpp, so the CSS and
  * the script are inline and there are no external references.
  ******************************************************************************
  */

#ifndef ILT_APP_WEBSERVER_INDEXPAGE_H
#define ILT_APP_WEBSERVER_INDEXPAGE_H

namespace app {

inline constexpr char kIndexHtml[] =
    R"HTML(<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ILT Degrader Dosimeter</title>
<style>
  :root { color-scheme: light dark; }
  body { margin: 0; font: 15px/1.5 system-ui, sans-serif;
         display: grid; place-items: center; min-height: 100vh; }
  main { width: min(30rem, 90vw); padding: 1.5rem; }
  h1 { font-size: 1.2rem; margin: 0 0 0.25rem; }
  p.sub { margin: 0 0 1.5rem; opacity: 0.6; font-size: 0.85rem; }
  dl { display: grid; grid-template-columns: auto 1fr; gap: 0.5rem 1rem; margin: 0; }
  dt { opacity: 0.6; }
  dd { margin: 0; font-variant-numeric: tabular-nums; }
  .dot { display: inline-block; width: 0.6em; height: 0.6em; border-radius: 50%;
         background: #16a34a; margin-right: 0.4em; }
  .stale .dot { background: #dc2626; }
</style>
<main>
  <h1>ILT Degrader Dosimeter</h1>
  <p class="sub">Served from lwIP httpd on the board itself.</p>
  <dl>
    <dt>Board</dt>      <dd id="board">&hellip;</dd>
    <dt>Address</dt>    <dd id="ip">&hellip;</dd>
    <dt>Uptime</dt>     <dd id="uptime">&hellip;</dd>
    <dt>Requests</dt>   <dd id="requests">&hellip;</dd>
    <dt>Link</dt>       <dd><span class="dot"></span><span id="link">&hellip;</span></dd>
  </dl>
</main>
<script>
const set = (id, v) => document.getElementById(id).textContent = v;
const duration = s => {
  const d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600);
  const m = Math.floor(s % 3600 / 60), sec = s % 60;
  return (d ? d + "d " : "") + (d || h ? h + "h " : "") + (d || h || m ? m + "m " : "") + sec + "s";
};
async function poll() {
  try {
    const r = await fetch("/status.json", { cache: "no-store" });
    const s = await r.json();
    set("board", s.board);
    set("ip", s.ip);
    set("uptime", duration(s.uptime_s));
    set("requests", s.requests);
    set("link", s.link ? "up" : "down");
    document.body.classList.remove("stale");
  } catch (e) {
    document.body.classList.add("stale");
    set("link", "unreachable");
  }
}
poll();
setInterval(poll, 2000);
</script>
</html>
)HTML";

} // namespace app

#endif /* ILT_APP_WEBSERVER_INDEXPAGE_H */
