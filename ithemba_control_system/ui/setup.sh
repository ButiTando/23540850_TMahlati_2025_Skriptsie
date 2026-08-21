#!/usr/bin/env bash
# Create the Python environment for the Scriptsie tkinter UI (Linux / macOS).
# Usage:  ./setup.sh [venv-dir]      (default: .venv)
set -euo pipefail

VENV="${1:-.venv}"
cd "$(dirname "$0")"

# --- locate a usable interpreter -------------------------------------------
PY=""
for c in python3.12 python3.11 python3.10 python3; do
    command -v "$c" >/dev/null 2>&1 || continue
    if "$c" -c 'import sys; sys.exit(0 if sys.version_info[:2] >= (3,10) else 1)' 2>/dev/null; then
        PY="$c"; break
    fi
done
if [ -z "$PY" ]; then
    echo "ERROR: no python >= 3.10 found. Install python3 and re-run." >&2
    exit 1
fi
echo "Using $PY ($("$PY" --version 2>&1))"

# --- tkinter is stdlib but is packaged separately on most distros -----------
if ! "$PY" -c 'import tkinter' 2>/dev/null; then
    echo "ERROR: tkinter is missing. It cannot be installed with pip." >&2
    if command -v apt-get >/dev/null 2>&1; then
        echo "  Debian/Ubuntu:  sudo apt install python3-tk" >&2
    elif command -v dnf >/dev/null 2>&1; then
        echo "  Fedora/RHEL:    sudo dnf install python3-tkinter" >&2
    elif command -v pacman >/dev/null 2>&1; then
        echo "  Arch:           sudo pacman -S tk" >&2
    elif command -v brew >/dev/null 2>&1; then
        echo "  macOS/Homebrew: brew install python-tk" >&2
    fi
    exit 1
fi

# --- build the venv ---------------------------------------------------------
if [ ! -d "$VENV" ]; then
    echo "Creating virtualenv in $VENV ..."
    "$PY" -m venv "$VENV"
else
    echo "Reusing existing virtualenv in $VENV"
fi

"$VENV/bin/python" -m pip install --quiet --upgrade pip
echo "Installing dependencies ..."
"$VENV/bin/python" -m pip install --quiet -r requirements.txt

# --- verify -----------------------------------------------------------------
echo "Verifying imports ..."
"$VENV/bin/python" - <<'PYEOF'
import sys
mods = ["numpy", "cv2", "PIL", "requests", "tkinter"]
bad = []
for m in mods:
    try:
        __import__(m)
        print(f"  ok      {m}")
    except Exception as e:
        bad.append(m)
        print(f"  FAILED  {m}: {e}")
sys.exit(1 if bad else 0)
PYEOF

echo
echo "Done. Activate with:"
echo "    source $VENV/bin/activate"
echo "Then run:"
echo "    python main.py"
