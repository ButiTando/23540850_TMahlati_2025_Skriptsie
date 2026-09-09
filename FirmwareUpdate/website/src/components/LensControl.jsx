import React, { useEffect, useState } from 'react';
import { LENSES, totalThickness } from '../lib/rig.js';
import { setLensMask } from '../lib/api.js';

/**
 * Lens selection.
 *
 * The checkboxes track the board's DESIRED mask whenever the user is not
 * mid-edit. The old page got this wrong: it wrote the mask but never read it
 * back, so the controls showed stale state after a reload or to a second
 * viewer.
 */
export default function LensControl({ desiredMask, currentMask, present, onChanged }) {
  const [draft, setDraft] = useState(desiredMask ?? 0);
  const [dirty, setDirty] = useState(false);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState(null);

  /* Adopt the board's value unless the user has pending edits, so an external
     change is reflected without stamping on someone mid-selection. */
  useEffect(() => {
    if (!dirty && typeof desiredMask === 'number') setDraft(desiredMask);
  }, [desiredMask, dirty]);

  const toggle = (i) => {
    setDraft((m) => m ^ (1 << i));
    setDirty(true);
  };

  const apply = async () => {
    setBusy(true);
    setError(null);
    try {
      await setLensMask(draft);
      setDirty(false);
      onChanged?.();
    } catch (e) {
      setError(e.message ?? String(e));
    } finally {
      setBusy(false);
    }
  };

  const revert = () => { setDraft(desiredMask ?? 0); setDirty(false); };

  return (
    <div>
      <div style={{ display: 'flex', flexWrap: 'wrap', gap: '0.4rem', marginBottom: '0.75rem' }}>
        {LENSES.map((mm, i) => {
          const on = (draft >> i) & 1;
          const live = (currentMask >> i) & 1;
          return (
            <label key={mm}
                   style={{
                     display: 'inline-flex', alignItems: 'center', gap: '0.4rem',
                     border: `1px solid ${on ? 'var(--series-1)' : 'var(--border-strong)'}`,
                     background: on ? 'color-mix(in srgb, var(--series-1) 12%, transparent)'
                                    : 'transparent',
                     borderRadius: '0.4rem', padding: '0.3rem 0.65rem', cursor: 'pointer',
                   }}>
              <input type="checkbox" checked={!!on} onChange={() => toggle(i)}
                     style={{ margin: 0 }} />
              {mm} mm
              {/* A dot when the board disagrees with the draft, so "asked for"
                  and "actually there" are never confused. */}
              {live !== on && (
                <span className="dot" style={{ background: 'var(--status-warning)' }}
                      title="differs from the mechanism's current position" />
              )}
            </label>
          );
        })}
      </div>

      <div style={{ display: 'flex', gap: '0.5rem', alignItems: 'center', flexWrap: 'wrap' }}>
        <button className="primary" onClick={apply} disabled={busy || !dirty}>
          {busy ? 'Applying…' : 'Apply'}
        </button>
        <button onClick={revert} disabled={!dirty}>Revert</button>
        <span className="muted" style={{ fontSize: '0.85rem' }}>
          Selected {totalThickness(draft)} mm
          {dirty && <span className="warn"> · unsaved</span>}
        </span>
      </div>

      {!present && (
        <p className="warn" style={{ fontSize: '0.82rem', margin: '0.7rem 0 0' }}>
          No degrader hardware on this board — requests are accepted and recorded,
          but nothing will move.
        </p>
      )}
      {error && <p className="warn" style={{ fontSize: '0.82rem', margin: '0.5rem 0 0' }}>{error}</p>}
    </div>
  );
}
