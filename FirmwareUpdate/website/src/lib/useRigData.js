import { useCallback, useEffect, useRef, useState } from 'react';
import { getDegrader, getDosimeter, getStatus } from './api.js';
import { CHANNELS } from './rig.js';

/**
 * Rows held in memory for the CSV. At one sample a second this is a bit over
 * three hours. The cap exists because the buffer is the tab's memory: an
 * unbounded log left running over a weekend would eventually take the page
 * down, which is a worse failure than losing the oldest rows. When it fills,
 * the oldest are dropped and the UI says so rather than quietly truncating.
 */
export const MAX_ROWS = 12000;

/** Points kept for the history chart. Small: it is a live view, not the record. */
const MAX_HISTORY = 180;

/**
 * Polls the rig and accumulates samples.
 *
 * Polling rather than the firmware's TCP stream is a browser limitation, not a
 * preference — see the README. The interval follows the board's own reported
 * period so the UI does not sample faster than the data actually changes.
 */
export function useRigData() {
  const [status, setStatus] = useState(null);
  const [degrader, setDegrader] = useState(null);
  const [dosimeter, setDosimeter] = useState(null);
  const [error, setError] = useState(null);
  const [history, setHistory] = useState([]);

  const [rows, setRows] = useState([]);
  const [recording, setRecording] = useState(false);
  const [dropped, setDropped] = useState(0);

  /* Refs, not state: the poll loop reads these and must not be a dependency of
     the effect that owns the interval, or it would tear the timer down and
     rebuild it on every sample. */
  const recordingRef = useRef(recording);
  recordingRef.current = recording;
  const lastSeqRef = useRef(-1);

  const poll = useCallback(async () => {
    try {
      const [s, d, b] = await Promise.all([getStatus(), getDegrader(), getDosimeter()]);
      setStatus(s);
      setDegrader(d);
      setDosimeter(b);
      setError(null);

      /* The board numbers its samples. Only take a genuinely new one, so a poll
         that happens to land twice inside one period cannot duplicate a row. */
      if (typeof b?.seq === 'number' && b.seq !== lastSeqRef.current) {
        const isFirst = lastSeqRef.current === -1;
        lastSeqRef.current = b.seq;

        if (!isFirst || b.seq > 0) {
          const point = {
            seq: b.seq,
            t_ms: b.t_ms,
            wall: new Date().toISOString(),
            counts: b.ch ?? CHANNELS.map(() => 0),
            period_s: b.period_s,
            lens_mask: d?.current ?? 0,
            desired_mask: d?.desired ?? 0,
            status: d?.status ?? 3,
            motor: d?.motor ?? null,
          };

          setHistory((prev) => [...prev, point].slice(-MAX_HISTORY));

          if (recordingRef.current) {
            setRows((prev) => {
              if (prev.length >= MAX_ROWS) {
                setDropped((n) => n + 1);
                return [...prev.slice(1), point];
              }
              return [...prev, point];
            });
          }
        }
      }
    } catch (e) {
      setError(e.message ?? String(e));
    }
  }, []);

  /* Two cadences. At rest, follow the board's own sampling period so a rig
     sampling every 10 s is not polled ten times per sample. While a motor is
     running, poll much faster: the carousel position is the thing being watched
     and once a second is too coarse to animate from. The extra requests are
     small JSON and last only as long as the move. */
  const moving = (degrader?.motor?.motion ?? 'idle') !== 'idle';
  const MOVING_POLL_MS = 250;
  const periodMs = moving
    ? MOVING_POLL_MS
    : Math.max(500, (dosimeter?.period_s ?? 1) * 1000);

  useEffect(() => {
    poll();
    const id = setInterval(poll, periodMs);
    return () => clearInterval(id);
  }, [poll, periodMs]);

  const clearRows = useCallback(() => {
    setRows([]);
    setDropped(0);
  }, []);

  return {
    status, degrader, dosimeter, error, history,
    rows, recording, setRecording, clearRows, dropped,
    /* So the view can interpolate over exactly the interval it will wait. */
    pollMs: periodMs,
    refresh: poll,
  };
}
