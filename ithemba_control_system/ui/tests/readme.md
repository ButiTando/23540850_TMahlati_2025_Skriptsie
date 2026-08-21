# Tests

No extra dependencies beyond the app's own `requirements.txt`.

```bash
./tests/run_all.sh            # everything
python tests/test_framing.py  # or one at a time
```

| File | Covers |
|---|---|
| `test_framing.py` | Recovering response messages from the degrader's unframed digit stream. Pure logic, no sockets. |
| `test_degrader_sim.py` | The real client against `simulators/degrader.py`: framing, a full lens move, the probe handshake, faults, reconnects. |
| `test_dosimeter_sim.py` | The real client against `simulators/dosimeter.py`: telemetry, command opcodes, the simulated clock, restart. |

The loopback tests bind ports in the 219xx range, so they can run while the
control station is live on the real ones. See `simulators/README.md` for the
protocols they exercise.
