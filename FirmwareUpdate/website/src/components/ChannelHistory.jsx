import React, { useState } from 'react';
import { CHANNELS } from '../lib/rig.js';

const SERIES = CHANNELS.map((_, i) => `var(--series-${i + 1})`);
const W = 720, H = 210;
const PAD = { top: 12, right: 14, bottom: 26, left: 46 };

/**
 * Counts per channel over time - six series, one shared y-axis.
 *
 * Never two scales: if one channel dwarfs the others that is the finding, and a
 * second axis would hide it. The legend is always present, and every series is
 * also direct-labelled at its last point, so identity never rests on colour
 * alone.
 */
export default function ChannelHistory({ history }) {
  const [hoverIndex, setHoverIndex] = useState(null);

  if (!history || history.length < 2) {
    return <p className="muted" style={{ margin: 0, fontSize: '0.85rem' }}>
      Collecting samples… the chart appears once two have arrived.
    </p>;
  }

  const n = history.length;
  const peak = Math.max(
    1,
    ...history.flatMap((p) => CHANNELS.map((_, i) => p.counts?.[i] ?? 0)),
  );

  const plotW = W - PAD.left - PAD.right;
  const plotH = H - PAD.top - PAD.bottom;
  const xAt = (i) => PAD.left + (n === 1 ? plotW / 2 : (i / (n - 1)) * plotW);
  const yAt = (v) => PAD.top + plotH - (v / peak) * plotH;

  const ticks = [0, 0.5, 1].map((f) => Math.round(peak * f));
  const hovered = hoverIndex != null ? history[hoverIndex] : null;

  return (
    <div>
      <svg viewBox={`0 0 ${W} ${H}`} width="100%" role="img"
           aria-label="Counts per channel over time"
           onMouseLeave={() => setHoverIndex(null)}
           onMouseMove={(e) => {
             const r = e.currentTarget.getBoundingClientRect();
             const px = ((e.clientX - r.left) / r.width) * W;
             const i = Math.round(((px - PAD.left) / plotW) * (n - 1));
             setHoverIndex(Math.max(0, Math.min(n - 1, i)));
           }}>
        {/* Recessive grid */}
        {ticks.map((t) => (
          <g key={t}>
            <line x1={PAD.left} y1={yAt(t)} x2={W - PAD.right} y2={yAt(t)}
                  stroke="var(--border)" strokeWidth="1" />
            <text x={PAD.left - 8} y={yAt(t) + 3} textAnchor="end"
                  fontSize="10" fill="var(--text-muted)">{t.toLocaleString()}</text>
          </g>
        ))}

        {CHANNELS.map((ch, ci) => {
          const d = history
            .map((p, i) => `${i === 0 ? 'M' : 'L'}${xAt(i).toFixed(1)},${yAt(p.counts?.[ci] ?? 0).toFixed(1)}`)
            .join(' ');
          const lastY = yAt(history[n - 1].counts?.[ci] ?? 0);
          return (
            <g key={ch}>
              <path d={d} fill="none" stroke={SERIES[ci]} strokeWidth="2"
                    strokeLinejoin="round" strokeLinecap="round" />
              {/* Direct label at the series end - identity without the legend. */}
              <text x={W - PAD.right + 2} y={lastY + 3} fontSize="9"
                    fill="var(--text-secondary)">{ch}</text>
            </g>
          );
        })}

        {hovered && (
          <>
            <line x1={xAt(hoverIndex)} y1={PAD.top} x2={xAt(hoverIndex)} y2={H - PAD.bottom}
                  stroke="var(--text-muted)" strokeWidth="1" strokeDasharray="3 3" />
            {CHANNELS.map((ch, ci) => (
              <circle key={ch} cx={xAt(hoverIndex)} cy={yAt(hovered.counts?.[ci] ?? 0)}
                      r="3.5" fill={SERIES[ci]}
                      stroke="var(--surface-1)" strokeWidth="2" />
            ))}
          </>
        )}

        <text x={PAD.left} y={H - 8} fontSize="10" fill="var(--text-muted)">
          {n} samples
        </text>
        <text x={W - PAD.right} y={H - 8} textAnchor="end" fontSize="10"
              fill="var(--text-muted)">now</text>
      </svg>

      <div className="legend" style={{ marginTop: '0.5rem' }}>
        {CHANNELS.map((ch, i) => (
          <span key={ch}>
            <span className="dot" style={{ background: SERIES[i] }} />
            Channel {ch}
            {hovered && <strong style={{ marginLeft: '0.3rem' }}>
              {(hovered.counts?.[i] ?? 0).toLocaleString()}
            </strong>}
          </span>
        ))}
      </div>
    </div>
  );
}
