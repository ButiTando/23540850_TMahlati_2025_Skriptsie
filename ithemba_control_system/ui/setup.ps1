# Create the Python environment for the Scriptsie tkinter UI (Windows).
#
# Usage:   .\setup.ps1 [-Venv .venv]
#
# If PowerShell blocks this script with an execution-policy error, run:
#     powershell -ExecutionPolicy Bypass -File .\setup.ps1
param([string]$Venv = ".venv")

$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

# --- locate a usable interpreter -------------------------------------------
function Find-Python {
    # The py launcher ships with the python.org installer and is the most reliable.
    if (Get-Command py -ErrorAction SilentlyContinue) {
        foreach ($v in @("3.12", "3.11", "3.10")) {
            try {
                & py "-$v" -c "import sys" 2>$null
                if ($LASTEXITCODE -eq 0) { return @("py", "-$v") }
            } catch { }
        }
    }
    if (Get-Command python -ErrorAction SilentlyContinue) {
        $ok = & python -c "import sys; sys.exit(0 if sys.version_info[:2] >= (3,10) else 1)" 2>$null
        if ($LASTEXITCODE -eq 0) { return @("python") }
    }
    return $null
}

$py = Find-Python
if (-not $py) {
    Write-Error "No Python >= 3.10 found. Install it from https://python.org and tick 'Add Python to PATH'."
    exit 1
}
Write-Host "Using $($py -join ' ') ($(& $py[0] $py[1..$py.Length] --version 2>&1))"

# --- tkinter ships with the python.org installer, but not always with others -
& $py[0] $py[1..$py.Length] -c "import tkinter" 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Error @"
tkinter is missing and cannot be installed with pip.
Re-run the python.org installer, choose 'Modify', and enable
'tcl/tk and IDLE'. (Microsoft Store builds of Python often omit it.)
"@
    exit 1
}

# --- build the venv ---------------------------------------------------------
if (-not (Test-Path $Venv)) {
    Write-Host "Creating virtualenv in $Venv ..."
    & $py[0] $py[1..$py.Length] -m venv $Venv
} else {
    Write-Host "Reusing existing virtualenv in $Venv"
}

$vpy = Join-Path $Venv "Scripts\python.exe"
& $vpy -m pip install --quiet --upgrade pip
Write-Host "Installing dependencies ..."
& $vpy -m pip install --quiet -r requirements.txt

# --- verify -----------------------------------------------------------------
Write-Host "Verifying imports ..."
$check = @"
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
"@
$check | & $vpy -
if ($LASTEXITCODE -ne 0) { Write-Error "Dependency verification failed."; exit 1 }

Write-Host ""
Write-Host "Done. Activate with:"
Write-Host "    .\$Venv\Scripts\Activate.ps1"
Write-Host "Then run:"
Write-Host "    python main.py"
