import React, { useState } from 'react';
import { useRigData } from './lib/useRigData.js';
import { setPeriod } from './lib/api.js';
import StatusBar from './components/StatusBar.jsx';
import DegraderView from './components/DegraderView.jsx';
import MotorReadout from './components/MotorReadout.jsx';
import LensControl from './components/LensControl.jsx';
import ChannelIntensity from './components/ChannelIntensity.jsx';
import ChannelHistory from './components/ChannelHistory.jsx';
import ChannelTable from './components/ChannelTable.jsx';
import Recorder from './components/Recorder.jsx';

export default function App() {
  const rig = useRigData();
  const { status, degrader, dosimeter, error, history } = rig;
  const [periodDraft, setPeriodDraft] = useState('');

  const applyPeriod = async (e) => {
    e.preventDefault();
    const n = Number(periodDraft);
    if (!Number.isFinite(n) || n < 1 || n > 3600) return;
    try { await setPeriod(n); setPeriodDraft(''); rig.refresh(); } catch { /* surfaced by poll */ }
  };

  return (
    <div className="app">
      <header className="masthead">
        <h1>ILT Test Rig</h1>
        <span className="sub">Degrader and dosimeter · {status?.board ?? 'connecting…'}</span>
      </header>

      <StatusBar status={status} degrader={degrader} error={error} />

      <div className="card">
        <div className="card-head">
          <h2>Degrader</h2>
          <span className="muted" style={{ fontSize: '0.8rem' }}>
            plate width drawn to scale with thickness
          </span>
        </div>
        <DegraderView
          word={degrader?.word ?? 0}
          currentMask={degrader?.current ?? 0}
          desiredMask={degrader?.desired ?? 0}
          present={!!degrader?.present}
          motor={degrader?.motor}
          motionPollMs={rig.pollMs}
        />
        <hr style={{ border: 0, borderTop: '1px solid var(--border)', margin: '1rem 0' }} />
        <MotorReadout motor={degrader?.motor} present={!!degrader?.present} />
        <hr style={{ border: 0, borderTop: '1px solid var(--border)', margin: '1rem 0' }} />
        <LensControl
          desiredMask={degrader?.desired ?? 0}
          currentMask={degrader?.current ?? 0}
          present={!!degrader?.present}
          onChanged={rig.refresh}
        />
      </div>

      <div className="grid-2">
        <div className="card">
          <div className="card-head">
            <h2>Channel intensity</h2>
            <form onSubmit={applyPeriod} style={{ display: 'flex', gap: '0.4rem' }}>
              <input type="number" min="1" max="3600"
                     placeholder={`${dosimeter?.period_s ?? 1} s`}
                     value={periodDraft}
                     onChange={(e) => setPeriodDraft(e.target.value)}
                     style={{
                       width: '5.5rem', font: 'inherit', padding: '0.25rem 0.4rem',
                       background: 'var(--surface-2)', color: 'var(--text-primary)',
                       border: '1px solid var(--border-strong)', borderRadius: '0.3rem',
                     }} />
              <button type="submit" disabled={!periodDraft}>Set period</button>
            </form>
          </div>
          <ChannelIntensity counts={dosimeter?.ch} periodS={dosimeter?.period_s} />
        </div>

        <div className="card">
          <h2>Channel readings</h2>
          <ChannelTable counts={dosimeter?.ch} history={history} />
        </div>
      </div>

      <div className="card">
        <div className="card-head">
          <h2>Counts over time</h2>
          <span className="muted" style={{ fontSize: '0.8rem' }}>
            sample {dosimeter?.seq ?? 0} · every {dosimeter?.period_s ?? 1} s
          </span>
        </div>
        <ChannelHistory history={history} />
      </div>

      <div className="card">
        <h2>Record to CSV</h2>
        <Recorder
          rows={rig.rows}
          recording={rig.recording}
          setRecording={rig.setRecording}
          clearRows={rig.clearRows}
          dropped={rig.dropped}
        />
      </div>

      <p className="muted" style={{ fontSize: '0.78rem' }}>
        Data is polled over HTTP. The firmware's TCP stream on port{' '}
        {status?.stream?.port ?? 5000} carries the same records, but a browser
        cannot open a raw socket — see the README.
      </p>
    </div>
  );
}
