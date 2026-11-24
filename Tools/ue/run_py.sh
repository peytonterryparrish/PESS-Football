#!/bin/bash
# Run an editor Python script headless: run_py.sh <script.py> [logname]
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$REPO/Saved/Logs"
L="$REPO/Saved/Logs/${2:-py}.log"
timeout 1800 "/d/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$(cygpath -w "$REPO/ProperFootball.uproject")" -run=pythonscript \
  -script="$(cygpath -w "$1")" -unattended -nop4 -nosplash -nopause -NoSound -stdout -abslog="$(cygpath -w "$L")" > "$L.stdout" 2>&1
echo "exit $?"
grep -a "\[PF\]\|Traceback\|Error:" "$L.stdout" | tail -30
