/**
  ******************************************************************************
  * @file    IndexPage.h
  * @brief   The rig's control page, as a string literal in flash.
  *
  * Self-contained: the board serves only the routes ILT_TESTRIG registers and
  * has no route to a CDN, so the CSS and script are inline.
  ******************************************************************************
  */

#ifndef ILT_APP_TESTRIG_INDEXPAGE_H
#define ILT_APP_TESTRIG_INDEXPAGE_H

namespace app {

inline constexpr char kIndexHtml[] =
    R"HTML(<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ILT Test Rig</title>
<style>
  :root { color-scheme: light dark; }
  body { margin:0; font:15px/1.5 system-ui,sans-serif; display:grid;
         place-items:start center; min-height:100vh; }
  main { width:min(46rem,92vw); padding:1.5rem; }
  h1 { font-size:1.2rem; margin:0 0 .25rem; }
  h2 { font-size:.95rem; margin:1.5rem 0 .5rem; opacity:.7;
       text-transform:uppercase; letter-spacing:.05em; }
  p.sub { margin:0 0 1rem; opacity:.6; font-size:.85rem; }
  dl { display:grid; grid-template-columns:auto 1fr; gap:.4rem 1rem; margin:0; }
  dt { opacity:.6; } dd { margin:0; font-variant-numeric:tabular-nums; }
  table { border-collapse:collapse; width:100%; font-variant-numeric:tabular-nums; }
  th,td { text-align:left; padding:.3rem .5rem; border-bottom:1px solid #8883; }
  th { font-weight:600; opacity:.6; font-size:.8rem; }
  .lens { display:flex; flex-wrap:wrap; gap:.5rem; margin:.5rem 0; }
  .lens label { display:flex; align-items:center; gap:.3rem;
                border:1px solid #8886; border-radius:.4rem; padding:.3rem .6rem; }
  button { font:inherit; padding:.35rem .9rem; border-radius:.4rem;
           border:1px solid #8886; background:#8881; cursor:pointer; }
  code { background:#8881; padding:.1rem .3rem; border-radius:.2rem; }
  .warn { color:#b45309; }
</style>
<main>
  <h1>ILT Test Rig</h1>
  <p class="sub">Degrader and dosimeter, served from the board.</p>

  <h2>Status</h2>
  <dl>
    <dt>Board</dt><dd id="board">&hellip;</dd>
    <dt>Address</dt><dd id="ip">&hellip;</dd>
    <dt>Uptime</dt><dd id="uptime">&hellip;</dd>
    <dt>Stream</dt><dd id="stream">&hellip;</dd>
  </dl>

  <h2>Degrader</h2>
  <div class="lens" id="lenses"></div>
  <button id="apply">Apply</button>
  <dl style="margin-top:1rem">
    <dt>Status</dt><dd id="degstatus">&hellip;</dd>
    <dt>Current</dt><dd id="degcurrent">&hellip;</dd>
  </dl>

  <h2>Dosimeter</h2>
  <table>
    <thead><tr><th>Channel</th><th>Counts</th></tr></thead>
    <tbody id="channels"></tbody>
  </table>
  <p class="sub" style="margin-top:.75rem">
    Period <span id="period">?</span> s &mdash; live records stream on the TCP
    port shown above, one JSON object per line.
  </p>
</main>
<script>
const SIZES = [2,3,6,8,10,12,30];
const STATUS = ["processing","awake","fault","ready"];
const set = (id,v) => document.getElementById(id).textContent = v;

const lensBox = document.getElementById("lenses");
SIZES.forEach((mm,i) => {
  const l = document.createElement("label");
  l.innerHTML = `<input type="checkbox" data-bit="${i}"> ${mm} mm`;
  lensBox.appendChild(l);
});

document.getElementById("apply").onclick = async () => {
  let mask = 0;
  document.querySelectorAll("#lenses input").forEach(c => {
    if (c.checked) mask |= (1 << Number(c.dataset.bit));
  });
  await fetch("/api/degrader/set.json?mask=" + mask, {cache:"no-store"});
  poll();
};

const duration = s => {
  const h = Math.floor(s/3600), m = Math.floor(s%3600/60);
  return (h?h+"h ":"") + (h||m?m+"m ":"") + (s%60) + "s";
};

async function poll() {
  try {
    const st = await (await fetch("/api/status.json",{cache:"no-store"})).json();
    set("board", st.board); set("ip", st.ip);
    set("uptime", duration(st.uptime_s));
    set("stream", `port ${st.stream.port} — ${st.stream.clients} client(s), ` +
                  `${st.stream.sent} sent, ${st.stream.dropped} dropped`);

    const d = await (await fetch("/api/degrader.json",{cache:"no-store"})).json();
    document.getElementById("degstatus").innerHTML =
      STATUS[d.status] + (d.present ? "" : ' <span class="warn">(no hardware)</span>');
    set("degcurrent", SIZES.filter((_,i) => d.current & (1<<i)).map(m => m+"mm")
                           .join(", ") || "none in beam");

    const b = await (await fetch("/api/dosimeter.json",{cache:"no-store"})).json();
    set("period", b.period_s);
    document.getElementById("channels").innerHTML = b.ch.map((v,i) =>
      `<tr><td>${i+1}</td><td>${v}</td></tr>`).join("");
  } catch (e) { /* leave the last good values on screen */ }
}
poll(); setInterval(poll, 2000);
</script>
</html>
)HTML";

} // namespace app

#endif /* ILT_APP_TESTRIG_INDEXPAGE_H */
