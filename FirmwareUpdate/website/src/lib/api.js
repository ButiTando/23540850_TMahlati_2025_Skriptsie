/**
 * Thin wrapper over the board's HTTP API.
 *
 * Requests go to same-origin /api/... and Vite proxies them to the rig in
 * development (see vite.config.js). Every call has a timeout: the board is a
 * microcontroller on a possibly unplugged cable, and a fetch with no deadline
 * would leave the UI showing stale data forever with no indication why.
 */

const TIMEOUT_MS = 3000;

async function getJson(path) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
  try {
    const response = await fetch(path, {
      cache: 'no-store',
      signal: controller.signal,
    });
    if (!response.ok) throw new Error(`${path} -> HTTP ${response.status}`);
    return await response.json();
  } finally {
    clearTimeout(timer);
  }
}

export const getStatus = () => getJson('/api/status.json');
export const getDegrader = () => getJson('/api/degrader.json');
export const getDosimeter = () => getJson('/api/dosimeter.json');

/** Ask for a lens configuration. `mask` is one bit per lens, 2mm = bit 0. */
export const setLensMask = (mask) =>
  getJson(`/api/degrader/set.json?mask=${mask >>> 0}`);

/** Change the dosimeter sampling period, in seconds. */
export const setPeriod = (seconds) =>
  getJson(`/api/dosimeter/period.json?seconds=${seconds >>> 0}`);
