import React from 'react';
import { CHANNELS } from '../lib/rig.js';

const SERIES = CHANNELS.map((_, i) => `var(--series-${i + 1})`);

/**
 * The table view.
 *
 * Required, not optional: three of the light-mode series colours fall below 3:1
 * against the surface, and the relief rule says such a chart ships either
 * visible labels or a table. It also happens to be the fastest way to read an
 * exact number, and it is what a screen reader gets.
 */
export default function ChannelTable({ counts, history }) {
  const latest = CHANNELS.map((_, i) => counts?.[i] ?? 0);

  const stat = (i) => {
    const series = (history ?? []).map((p) => p.counts?.[i] ?? 0);
    if (series.length === 0) return { peak: 0, mean: 0 };
    return {
      peak: Math.max(...series),
      mean: Math.round(series.reduce((a, b) => a + b, 0) / series.length),
    };
  };

  return (
    <table className="data">
      <caption className="muted" style={{ captionSide: 'bottom', textAlign: 'left',
                                          fontSize: '0.78rem', paddingTop: '0.5rem' }}>
        Peak and mean are over the samples currently held for the chart.
      </caption>
      <thead>
        <tr><th>Channel</th><th>Latest</th><th>Peak</th><th>Mean</th></tr>
      </thead>
      <tbody>
        {CHANNELS.map((ch, i) => {
          const s = stat(i);
          return (
            <tr key={ch}>
              <td>
                <span className="dot" style={{ background: SERIES[i], marginRight: '0.45rem',
                                               display: 'inline-block' }} />
                Channel {ch}
              </td>
              <td>{latest[i].toLocaleString()}</td>
              <td>{s.peak.toLocaleString()}</td>
              <td>{s.mean.toLocaleString()}</td>
            </tr>
          );
        })}
        <tr style={{ fontWeight: 600 }}>
          <td>Total</td>
          <td>{latest.reduce((a, b) => a + b, 0).toLocaleString()}</td>
          <td colSpan={2} />
        </tr>
      </tbody>
    </table>
  );
}
