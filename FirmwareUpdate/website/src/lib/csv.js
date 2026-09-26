/**
 * CSV assembly and download — entirely in the browser.
 *
 * Nothing is written on the board and no server is involved: the rows live in
 * the tab's memory, are joined into a string here, and handed to the browser as
 * a Blob. That is the whole mechanism.
 */

import { CHANNELS, LENSES, PROCESS_STATUS } from './rig.js';

const HEADER = [
  'wall_clock_iso',
  'sample_seq',
  'board_uptime_ms',
  'period_s',
  ...CHANNELS.map((c) => `ch${c}_counts`),
  'ch_total_counts',
  'lens_mask',
  'lenses_in_beam_mm',
  'total_thickness_mm',
  'degrader_status',
  'motor_motion',
  'motor_steps',
  'motor_position_stations',
  'motor_confirmed_station_mm',
];

/**
 * @param {Array} rows samples collected by useRigData
 * @returns {string} RFC 4180-ish CSV, CRLF line endings
 */
export function rowsToCsv(rows) {
  const lines = [HEADER.join(',')];

  for (const r of rows) {
    const counts = CHANNELS.map((_, i) => r.counts[i] ?? 0);
    const inBeam = LENSES.filter((_, i) => (r.lens_mask >> i) & 1);

    lines.push([
      r.wall,
      r.seq,
      r.t_ms,
      r.period_s,
      ...counts,
      counts.reduce((a, b) => a + b, 0),
      r.lens_mask,
      /* Quoted: this field contains commas when several lenses are inserted. */
      `"${inBeam.join(' ')}"`,
      inBeam.reduce((a, b) => a + b, 0),
      PROCESS_STATUS[r.status]?.key ?? 'unknown',
      r.motor?.motion ?? '',
      r.motor?.steps ?? 0,
      /* Commanded, not measured - the stepper is open loop. */
      ((r.motor?.milli_stations ?? 0) / 1000).toFixed(3),
      r.motor?.confirmed >= 0 ? LENSES[r.motor.confirmed] : '',
    ].join(','));
  }

  return lines.join('\r\n') + '\r\n';
}

/**
 * Hand `text` to the viewer as `filename`.
 *
 * Two routes, because this app runs in two places. Served normally it is an
 * ordinary page, so an anchor with a blob: URL is the download. Published as an
 * Artifact it runs sandboxed, where anchor downloads are inert by design and the
 * `downloads` capability is the only way to give someone a file - the viewer
 * gets a confirmation prompt and may decline.
 *
 * @returns 'saved' | 'declined' | 'downloaded'
 * @throws  the capability's error when a save fails for any other reason
 */
export async function downloadCsv(text, filename) {
  /* A BOM so Excel opens UTF-8 correctly; harmless to everything else. */
  const body = '\uFEFF' + text;

  let downloads = null;
  try {
    downloads = (await window.claude?.use?.('downloads')) ?? null;
  } catch {
    downloads = null; /* not in an artifact, or the capability is unavailable */
  }

  if (downloads) {
    try {
      await downloads.save({ filename, data: body });
      return 'saved';
    } catch (e) {
      if (e?.code === 'declined') return 'declined';
      throw e;
    }
  }

  const blob = new Blob([body], { type: 'text/csv;charset=utf-8;' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  /* Revoking immediately can cancel the download in some browsers. */
  setTimeout(() => URL.revokeObjectURL(url), 5000);
  return 'downloaded';
}

export const csvFilename = () => {
  const t = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
  return `ilt-testrig-${t}.csv`;
};
