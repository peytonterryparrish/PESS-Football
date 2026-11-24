#!/bin/bash
# Run the game uncooked, windowed. run_game.sh [extra args]  e.g. -pfmatch -pfwatch -pfshots=4
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$REPO/Saved/Logs"
timeout ${PF_TIMEOUT:-600} "/d/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "$(cygpath -w "$REPO/ProperFootball.uproject")" -game -windowed -ResX=1920 -ResY=1080 -nosplash -NoSound "$@" -abslog="$(cygpath -w "$REPO/Saved/Logs/game.log")" > /dev/null 2>&1
echo "exit $?"
grep -a "PF \|Error\|Fatal" "$REPO/Saved/Logs/game.log" | grep -v "ZenServer" | tail -25
