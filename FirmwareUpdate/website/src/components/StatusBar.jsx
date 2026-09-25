import React from 'react';
import { PROCESS_STATUS, formatUptime } from '../lib/rig.js';

const TONE = {
  good: 'var(--status-good)',
  warning: 'var(--status-warning)',
  serious: 'var(--status-serious)',
  critical: 'var(--status-critical)',
};

export default function StatusBar({ status, degrader, error }) {
  const proc = PROCESS_STATUS[degrader?.status] ?? PROCESS_STATUS[3];

  return (
    <div className="card">
      <div className="card-head">
        <h2>Status</h2>
        <span className="pill">
          <span className="dot" style={{
            background: error ? TONE.critical : TONE[proc.tone],
          }} />
          {/* Status is never colour alone: the word is always here too. */}
          {error ? 'Unreachable' : proc.label}
        </span>
      </div>

      {error && (
        <p className="warn" style={{ margin: '0 0 0.75rem', fontSize: '0.85rem' }}>
          {error} — showing the last values received.
        </p>
      )}

      <dl className="kv" style={{ fontSize: '0.88rem' }}>
        <dt>Board</dt>    <dd>{status?.board ?? '—'}</dd>
        <dt>Address</dt>  <dd>{status?.ip ?? '—'}</dd>
        <dt>Uptime</dt>   <dd>{status ? formatUptime(status.uptime_s) : '—'}</dd>
        <dt>Requests</dt> <dd>{status?.http_requests?.toLocaleString() ?? '—'}</dd>
        <dt>Stream</dt>
        <dd>
          {status
            ? `port ${status.stream.port} · ${status.stream.clients} client(s) · ` +
              `${status.stream.sent.toLocaleString()} sent · ${status.stream.dropped.toLocaleString()} dropped`
            : '—'}
        </dd>
        <dt>Hardware</dt>
        <dd>
          degrader {status?.degrader_present ? 'present' : <span className="warn">absent</span>}
          {' · '}
          dosimeter {status?.dosimeter_present ? 'present' : <span className="warn">absent</span>}
        </dd>
      </dl>
    </div>
  );
}
