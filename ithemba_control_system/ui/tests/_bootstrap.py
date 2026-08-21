"""Shared test setup: make the project importable and report results.

Run any test file directly:  python tests/test_framing.py
"""

import pathlib
import sys
import types

PROJECT_ROOT = pathlib.Path(__file__).resolve().parent.parent
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

# The package __init__ pulls in the camera widget, which needs requests; the
# socket code under test does not, so stub it if it is missing.
try:
    import requests  # noqa: F401
except ImportError:  # pragma: no cover
    sys.modules["requests"] = types.ModuleType("requests")


class Results:
    """Collects pass/fail so a file can exit with a useful status."""

    def __init__(self):
        self.failures = []

    def check(self, name, ok, extra=""):
        print(f"{'PASS' if ok else 'FAIL'}  {name} {extra}")
        if not ok:
            self.failures.append(name)
        return ok

    def finish(self):
        print("\n" + ("ALL PASSED" if not self.failures
                      else f"FAILURES: {self.failures}"))
        return 1 if self.failures else 0


def wait_for(predicate, timeout=10.0, interval=0.05):
    """Poll until predicate holds, rather than sleeping a fixed time."""
    import time
    deadline = time.time() + timeout
    while time.time() < deadline:
        if predicate():
            return True
        time.sleep(interval)
    return False
