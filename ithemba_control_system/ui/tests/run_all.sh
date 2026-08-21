#!/usr/bin/env bash
# Run every test. Usage: ./tests/run_all.sh [python-executable]
set -u
cd "$(dirname "$0")/.."
PY="${1:-python3}"
status=0
for test in tests/test_framing.py tests/test_degrader_sim.py tests/test_dosimeter_sim.py; do
    echo "=== $test ==="
    "$PY" "$test" || status=1
    echo
done
exit $status
