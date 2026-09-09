import React from 'react';
import { LENSES } from '../lib/rig.js';

/**
 * The numbers behind the selector rail drawn in DegraderView.
 *
 * Position and Confirmed are deliberately separate rows. Position is the
 * COMMANDED station - steps issued divided by steps per station - because the
 * A4988 has no encoder. Confirmed is the last station a SELECT switch actually
 * closed on. They agreeing is the mechanism tracking; them drifting apart is
 * the motor losing steps, which is the failure an open-loop stepper has.
 */
export default function MotorReadout({ motor, present }) {
  const position = (motor?.milli_stations ?? 0) / 1000;
  const confirmed = motor?.confirmed ?? -1;
  const target = motor?.target ?? -1;
  const motion = motor?.motion ?? 'idle';
  const stations = motor?.stations ?? LENSES.length;

  return (
    <dl className="kv" style={{ fontSize: '0.85rem' }}>
      <dt>Stepper</dt>
      <dd>
        <span className="pill">
          <span className="dot" style={{
            background: motion === 'idle' ? 'var(--text-muted)' : 'var(--status-warning)',
          }} />
          {present ? motion : 'no hardware'}
        </span>
      </dd>

      <dt>Position</dt>
      <dd>
        station {position.toFixed(2)} of {stations}{' '}
        <span className="muted">(commanded — open loop, no encoder)</span>
      </dd>

      <dt>Confirmed</dt>
      <dd>
        {confirmed >= 0
          ? <>{LENSES[confirmed]} mm <span className="muted">by SELECT switch</span></>
          : <span className="muted">not yet homed</span>}
      </dd>

      <dt>Target</dt>
      <dd>
        {target >= 0
          ? <strong style={{ color: 'var(--target)' }}>{LENSES[target]} mm</strong>
          : <span className="muted">—</span>}
      </dd>

      <dt>Steps</dt>
      <dd>
        {(motor?.steps ?? 0).toLocaleString()}{' '}
        <span className="muted">
          @ {motor?.step_rate_hz ?? 0} Hz · {motor?.steps_per_station ?? 0}/station
        </span>
      </dd>
    </dl>
  );
}
