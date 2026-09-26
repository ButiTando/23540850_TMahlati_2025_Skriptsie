import React, { useState } from 'react';
import { CHANNELS } from '../lib/rig.js';

const SERIES = CHANNELS.map((_, i) => `var(--series-${i + 1})`);

/**
 * Current intensity per channel.
 *
 * A bar chart, because the job is magnitude compared across a handful of named
 * things read at one instant. One shared axis for all six - never a second
 * scale - so a tall bar genuinely means more counts.
 */
export default function ChannelIntensity({ counts, periodS }) {
  const [hover, setHover] = useState(null);
  const values = CHANNELS.map((_, i) => counts?.[i] ?? 0);
  const peak = Math.max(...values, 1);
  const allZero = values.every((v) => v === 0);

  return (
    <div>
      <div style={{ display: 'grid', gap: '0.45rem' }}>
        {CHANNELS.map((ch, i) => {
          const v = values[i];
          const pct = (v / peak) * 100;
          return (
            <div key={ch}
                 onMouseEnter={() => setHover(i)}
                 onMouseLeave={() => setHover(null)}
                 style={{
                   display: 'grid',
                   gridTemplateColumns: '2.6rem 1fr 5rem',
                   alignItems: 'center', gap: '0.6rem',
                   padding: '0.15rem 0.25rem', borderRadius: '0.3rem',
                   background: hover === i ? 'var(--surface-2)' : 'transparent',
                 }}>
              <span style={{ display: 'inline-flex', alignItems: 'center', gap: '0.35rem',
                             color: 'var(--text-secondary)', fontSize: '0.8rem' }}>
                <span className="dot" style={{ background: SERIES[i] }} />
                {ch}
              </span>

              {/* Track then fill. 4px rounded end, anchored at the baseline. */}
              <span style={{ position: 'relative', height: '0.85rem',
                             background: 'var(--surface-2)', borderRadius: '0.2rem' }}>
                <span style={{
                  position: 'absolute', inset: '0 auto 0 0',
                  width: `${Math.max(pct, v > 0 ? 1.5 : 0)}%`,
                  background: SERIES[i],
                  borderRadius: '0.2rem 4px 4px 0.2rem',
                  transition: 'width 0.3s ease',
                }} />
              </span>

              {/* Direct label on every bar: the relief rule, since three of the
                  light-mode series sit under 3:1 against the surface. */}
              <span style={{ textAlign: 'right', fontSize: '0.85rem' }}>
                {v.toLocaleString()}
              </span>
            </div>
          );
        })}
      </div>

      <p className="muted" style={{ fontSize: '0.8rem', margin: '0.8rem 0 0' }}>
        {allZero
          ? 'All channels reading zero — no detector pulses.'
          : `Counts in the last ${periodS ?? 1} s window. Peak ${peak.toLocaleString()}.`}
      </p>
    </div>
  );
}
