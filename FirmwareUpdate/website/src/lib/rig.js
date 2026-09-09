/** Shared vocabulary for the rig — kept in one place so the UI and the CSV agree. */

/** Lens thicknesses in carousel order; the index is the bit in the lens mask. */
export const LENSES = [2, 3, 6, 8, 10, 12, 30];

export const CHANNELS = [1, 2, 3, 4, 5, 6];

/** Two-bit lens field from the firmware's response word. */
export const LENS_STATE = {
  0: { key: 'out', label: 'Out of beam' },
  1: { key: 'in', label: 'In beam' },
  2: { key: 'moving', label: 'Moving' },
  3: { key: 'unknown', label: 'Unknown' },
};

/** Top two bits of the response word. */
export const PROCESS_STATUS = {
  0: { key: 'processing', label: 'Processing', tone: 'warning' },
  1: { key: 'awake', label: 'Awake', tone: 'warning' },
  2: { key: 'fault', label: 'Fault', tone: 'critical' },
  3: { key: 'ready', label: 'Ready', tone: 'good' },
};

/** Unpack the packed 16-bit response word into per-lens states. */
export function lensStatesFromWord(word) {
  return LENSES.map((_, i) => (word >> (2 * i)) & 0x3);
}

export const maskToLenses = (mask) =>
  LENSES.filter((_, i) => (mask >> i) & 1);

/** Total material in the beam, which is what the physics actually cares about. */
export const totalThickness = (mask) =>
  maskToLenses(mask).reduce((sum, mm) => sum + mm, 0);

export const formatUptime = (s) => {
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  const sec = s % 60;
  return (
    (d ? `${d}d ` : '') + (d || h ? `${h}h ` : '') + (d || h || m ? `${m}m ` : '') + `${sec}s`
  );
};
