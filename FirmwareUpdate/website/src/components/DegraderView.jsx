import React from 'react';
import { LENSES, LENS_STATE, lensStatesFromWord, totalThickness } from '../lib/rig.js';

/**
 * The degrader, drawn as the mechanism actually behaves.
 *
 * There is no photograph or drawing of the rig anywhere in the repository, so
 * this is reconstructed from what the firmware and the schematic define: seven
 * absorber plates on a carousel, each driven into or out of a horizontal beam
 * by the DC motor once the stepper has aligned it. A plate that is "in" sits on
 * the beam axis; one that is "out" is parked above it.
 *
 * Plate WIDTH is drawn proportional to its thickness in millimetres, because
 * that is the quantity the experiment cares about - a 30 mm plate should look
 * like ten times the 3 mm one, not like another identical box.
 */

const W = 720;
const H = 250;
const AXIS_Y = 120;      /* the beam line */
const PARKED_Y = 34;     /* where an out-of-beam plate rests */
const PLATE_H = 46;
const LABEL_Y = 164;     /* the millimetre labels under each station */
const RAIL_Y = 205;      /* the carousel rail the selector runs along */

/* Plate width in px per mm of absorber, plus a floor so 2 mm stays legible. */
const PX_PER_MM = 1.5;
const MIN_PLATE_W = 16;

const plateWidth = (mm) => Math.max(MIN_PLATE_W, mm * PX_PER_MM);

export default function DegraderView({ word, currentMask, desiredMask, present,
                                      motor, motionPollMs = 350 }) {
  const states = lensStatesFromWord(word ?? 0);
  const inserted = totalThickness(currentMask ?? 0);

  /* Lay the stations out evenly across the beam, widest plate first in space so
     nothing overlaps at 30 mm. */
  const gap = 14;
  const totalW = LENSES.reduce((sum, mm) => sum + plateWidth(mm), 0) + gap * (LENSES.length - 1);
  const startX = (W - totalW) / 2;

  const motorTarget = motor?.target ?? -1;

  let x = startX;
  const stations = LENSES.map((mm, i) => {
    const w = plateWidth(mm);
    const station = { mm, i, x, w, state: states[i], desired: (desiredMask >> i) & 1,
                      isTarget: i === motorTarget };
    x += w + gap;
    return station;
  });

  return (
    <figure style={{ margin: 0 }}>
      <svg viewBox={`0 0 ${W} ${H}`} width="100%" role="img"
           aria-label={`Degrader: ${inserted} millimetres of absorber in the beam`}>
        {/* Beam: drawn behind the plates, and stopped at the first inserted
            plate so the picture shows the beam being intercepted rather than
            passing through solid material. */}
        <BeamPath stations={stations} />

        {/* Rail the parked plates hang from. */}
        <line x1={startX - 20} y1={PARKED_Y - 14} x2={startX + totalW + 20} y2={PARKED_Y - 14}
              stroke="var(--border-strong)" strokeWidth="2" strokeDasharray="4 4" />
        <text x={startX - 24} y={PARKED_Y - 18} textAnchor="end"
              fontSize="10" fill="var(--text-muted)">parked</text>

        {stations.map((s) => <Plate key={s.mm} {...s} present={present} />)}

        {/* The carousel and its stepper, drawn UNDER the lenses: the selector
            runs along this rail and whichever station it sits beneath is the
            one the DC motor can drive into the beam. */}
        <SelectorRail stations={stations} motor={motor} present={present}
                      motionMs={motionPollMs} />

        {/* Beam direction markers */}
        <text x="8" y={AXIS_Y - 10} fontSize="11" fill="var(--text-secondary)">beam in</text>
        <text x={W - 8} y={AXIS_Y - 10} fontSize="11" fill="var(--text-secondary)"
              textAnchor="end">out</text>
      </svg>

      <figcaption style={{ marginTop: '0.5rem', fontSize: '0.85rem' }}>
        <strong>{inserted} mm</strong>{' '}
        <span className="muted">
          of absorber in the beam
          {inserted === 0 ? ' — beam unattenuated' : ''}
        </span>
      </figcaption>
    </figure>
  );
}

/**
 * The carousel rail and the stepper's selector, drawn beneath the lenses.
 *
 * The selector sits under whichever station the stepper has brought round, and
 * that is the lens the DC motor is able to push into the beam. Its x position
 * comes from the COMMANDED step count, so it slides continuously between
 * stations while the motor turns.
 *
 * The carousel is a loop shown as a line, so travel past the last station wraps
 * to the first. That is a real discontinuity in the drawing, not a glitch: the
 * arrows at each end mark where the loop closes.
 */
function SelectorRail({ stations, motor, present, motionMs = 350 }) {
  const n = stations.length;
  const centres = stations.map((s) => s.x + s.w / 2);
  const first = centres[0];
  const last = centres[n - 1];
  const pitch = (last - first) / (n - 1);

  const position = (motor?.milli_stations ?? 0) / 1000;   /* fractional station */
  const confirmed = motor?.confirmed ?? -1;
  const target = motor?.target ?? -1;
  const motion = motor?.motion ?? 'idle';
  const turning = motion === 'selecting';

  /* Interpolate between station centres; past the last one the selector runs on
     toward a virtual station that is where station 0 comes round again. */
  const i = Math.floor(position) % n;
  const frac = position - Math.floor(position);
  const from = centres[i];
  const to = i === n - 1 ? last + pitch : centres[i + 1];
  const x = from + (to - from) * frac;

  return (
    <g>
      {/* Rail */}
      <line x1={first - pitch * 0.7} y1={RAIL_Y} x2={last + pitch * 0.7} y2={RAIL_Y}
            stroke="var(--border-strong)" strokeWidth="2" />
      {/* Loop-closure arrows: the rail is really a circle. */}
      <text x={first - pitch * 0.7 - 6} y={RAIL_Y + 4} textAnchor="end"
            fontSize="12" fill="var(--text-muted)">&#8630;</text>
      <text x={last + pitch * 0.7 + 6} y={RAIL_Y + 4}
            fontSize="12" fill="var(--text-muted)">&#8631;</text>

      {/* Station detents, and a ring on the one a switch has confirmed. */}
      {centres.map((cx, idx) => (
        <g key={idx}>
          <line x1={cx} y1={RAIL_Y - 5} x2={cx} y2={RAIL_Y + 5}
                stroke="var(--border-strong)" strokeWidth="1.5" />
          {idx === confirmed && (
            <circle cx={cx} cy={RAIL_Y} r="8" fill="none"
                    stroke="var(--series-1)" strokeWidth="1.5" opacity="0.7" />
          )}
          {idx === target && (
            <polygon points={`${cx - 5},${RAIL_Y + 16} ${cx + 5},${RAIL_Y + 16} ${cx},${RAIL_Y + 8}`}
                     fill="var(--target)" />
          )}
        </g>
      ))}

      {/* The selector carriage. Amber while the stepper is turning. */}
      {/* While the stepper turns, the position we are given only updates once
          per poll, so the carriage is interpolated ACROSS that interval -- a
          linear transition just longer than the poll period, which arrives
          before the next sample and keeps the motion continuous. Easing would
          make constant rotation look like it repeatedly starts and stops.
          Idle snaps: there is nothing to interpolate between. */}
      <g style={{
           transition: turning ? `transform ${motionMs}ms linear` : 'none',
         }}
         transform={`translate(${x - centres[0]} 0)`}>
        <g transform={`translate(${centres[0]} 0)`}>
          <title>
            {present
              ? `Stepper at station ${position.toFixed(2)} — ${motion}`
              : 'no hardware'}
          </title>
          {/* Post up to the lens it is under, so the link is unmistakable. */}
          <line x1="0" y1={RAIL_Y - 6} x2="0" y2={LABEL_Y + 8}
                stroke={turning ? 'var(--status-warning)' : 'var(--series-1)'}
                strokeWidth="1.5" opacity="0.55" strokeDasharray="2 3" />
          <rect x="-13" y={RAIL_Y - 9} width="26" height="18" rx="3"
                fill={turning ? 'var(--status-warning)' : 'var(--series-1)'} />
          <polygon points={`-6,${RAIL_Y - 12} 6,${RAIL_Y - 12} 0,${RAIL_Y - 19}`}
                   fill={turning ? 'var(--status-warning)' : 'var(--series-1)'} />
        </g>
      </g>

      <text x={first - pitch * 0.7} y={RAIL_Y + 32} fontSize="10"
            fill="var(--text-muted)">
        carousel — stepper {present ? motion : 'idle (no hardware)'}
        {present && ` · station ${position.toFixed(2)}`}
      </text>
    </g>
  );
}

function BeamPath({ stations }) {
  const firstIn = stations.find((s) => s.state === 1);
  const stopX = firstIn ? firstIn.x + firstIn.w / 2 : null;

  return (
    <>
      {/* Full-strength beam up to the first absorber (or all the way across). */}
      <line x1="0" y1={AXIS_Y} x2={stopX ?? 720} y2={AXIS_Y}
            stroke="var(--beam)" strokeWidth="3" strokeLinecap="round" />
      {/* Attenuated remainder, thinner and faded rather than a second colour:
          it is the same beam, not a second series. */}
      {stopX !== null && (
        <line x1={stopX} y1={AXIS_Y} x2="720" y2={AXIS_Y}
              stroke="var(--beam)" strokeWidth="1.5" opacity="0.35"
              strokeLinecap="round" strokeDasharray="6 5" />
      )}
    </>
  );
}

function Plate({ mm, x, w, state, desired, present, isTarget }) {
  const info = LENS_STATE[state] ?? LENS_STATE[3];

  /* Vertical position encodes the state: on the axis when in, parked when out,
     halfway when moving. */
  const y =
    state === 1 ? AXIS_Y - PLATE_H / 2
    : state === 2 ? (PARKED_Y + AXIS_Y - PLATE_H / 2) / 2
    : PARKED_Y;

  const fill =
    state === 1 ? 'var(--series-1)'
    : state === 2 ? 'var(--status-warning)'
    : 'var(--surface-2)';

  const stroke =
    state === 1 ? 'var(--series-1)'
    : state === 2 ? 'var(--status-warning)'
    : 'var(--border-strong)';

  const label = `${mm} mm — ${present ? info.label : 'no hardware'}`;

  return (
    <g>
      <title>{label}</title>

      {/* The lens the carousel is heading to. Green because it is a
          destination, not a health state -- hence its own token rather than
          --status-good. */}
      {isTarget && (
        <rect x={x - 5} y={y - 5} width={w + 10} height={PLATE_H + 10} rx="6"
              fill="none" stroke="var(--target)" strokeWidth="2"
              style={{ transition: 'y 0.35s ease' }} />
      )}

      {/* Where this plate would sit if inserted; shown only when it is not,
          so the geometry reads without hunting. */}
      {state !== 1 && (
        <rect x={x} y={AXIS_Y - PLATE_H / 2} width={w} height={PLATE_H} rx="3"
              fill="none" stroke="var(--border)" strokeDasharray="3 3" />
      )}

      {/* The move is animated with a CSS transition, NOT an SMIL <animate>.
          An <animate fill="freeze"> snapshots the underlying value on its first
          run and then overrides every later attribute update, so once React
          re-rendered with a new state the plate stayed frozen where it first
          appeared -- which looked like every plate being permanently parked.
          SVG geometry properties are animatable as CSS in modern browsers. */}
      <rect x={x} y={y} width={w} height={PLATE_H} rx="3"
            fill={fill} stroke={stroke} strokeWidth="1.5"
            opacity={state === 0 || state === 3 ? 0.9 : 1}
            /* Position animates; colour does NOT. Blending amber (moving)
               into blue (in beam) passes through olive, which reads as a third
               state that does not exist. State changes should snap. */
            style={{ transition: 'y 0.35s ease' }} />

      {/* Requested-but-not-yet-there marker: a caret under the station. */}
      {desired === 1 && state !== 1 && (
        <polygon points={`${x + w / 2 - 5},${LABEL_Y + 6} ${x + w / 2 + 5},${LABEL_Y + 6} ${x + w / 2},${LABEL_Y + 14}`}
                 fill="var(--series-1)" />
      )}

      <text x={x + w / 2} y={LABEL_Y} textAnchor="middle" fontSize="11"
            fill={isTarget ? 'var(--target)' : 'var(--text-secondary)'}
            fontWeight={isTarget ? 600 : 400}>{mm}</text>
    </g>
  );
}
