import React, { useState } from 'react';
import { csvFilename, downloadCsv, rowsToCsv } from '../lib/csv.js';
import { MAX_ROWS } from '../lib/useRigData.js';

/**
 * Client-side CSV recording.
 *
 * Samples accumulate in the tab's memory; Download joins them into a string and
 * hands it to the browser as a Blob. The board stores nothing and no server is
 * involved, so the file exists only where it was made.
 */
export default function Recorder({ rows, recording, setRecording, clearRows, dropped }) {
  const count = rows.length;
  const pct = Math.min(100, (count / MAX_ROWS) * 100);

  const [note, setNote] = useState(null);

  const download = async () => {
    if (count === 0) return;
    setNote(null);
    try {
      const outcome = await downloadCsv(rowsToCsv(rows), csvFilename());
      if (outcome === 'declined') setNote('Download cancelled.');
    } catch (e) {
      /* Say what happened, rather than failing silently. */
      setNote(e?.code === 'unavailable' || e?.code === 'not_granted'
        ? 'Downloads are not available in this view.'
        : `Could not save the file${e?.message ? `: ${e.message}` : '.'}`);
    }
  };

  const span = count > 1
    ? Math.round((rows[count - 1].t_ms - rows[0].t_ms) / 1000)
    : 0;

  return (
    <div>
      <div style={{ display: 'flex', gap: '0.5rem', flexWrap: 'wrap', marginBottom: '0.75rem' }}>
        <button className={recording ? '' : 'primary'}
                onClick={() => setRecording(!recording)}>
          {recording ? 'Stop recording' : 'Start recording'}
        </button>
        <button onClick={download} disabled={count === 0}>
          Download CSV{count > 0 ? ` (${count.toLocaleString()} rows)` : ''}
        </button>
        <button onClick={clearRows} disabled={count === 0}>Clear</button>
      </div>

      <dl className="kv" style={{ fontSize: '0.85rem' }}>
        <dt>State</dt>
        <dd>
          <span className="pill">
            <span className="dot" style={{
              background: recording ? 'var(--status-good)' : 'var(--text-muted)',
            }} />
            {recording ? 'Recording' : 'Idle'}
          </span>
        </dd>
        <dt>Rows</dt>
        <dd>{count.toLocaleString()} <span className="muted">of {MAX_ROWS.toLocaleString()} max</span></dd>
        <dt>Span</dt>
        <dd>{count > 1 ? `${span} s` : <span className="muted">—</span>}</dd>
      </dl>

      <div style={{ marginTop: '0.6rem', height: '0.35rem', background: 'var(--surface-2)',
                    borderRadius: '0.2rem', overflow: 'hidden' }}>
        <div style={{ width: `${pct}%`, height: '100%', background: 'var(--series-1)' }} />
      </div>

      {note && (
        <p className="warn" style={{ fontSize: '0.8rem', margin: '0.6rem 0 0' }}>{note}</p>
      )}

      {dropped > 0 && (
        <p className="warn" style={{ fontSize: '0.8rem', margin: '0.6rem 0 0' }}>
          Buffer full — {dropped.toLocaleString()} oldest row{dropped === 1 ? '' : 's'} dropped.
          Download and clear to start a fresh file.
        </p>
      )}

      <p className="muted" style={{ fontSize: '0.8rem', margin: '0.6rem 0 0' }}>
        The file is built in your browser; nothing is written on the board.
      </p>
    </div>
  );
}
