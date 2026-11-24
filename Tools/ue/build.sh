#!/bin/bash
# Build the editor target (game module). Log: Saved/Logs/build.log
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$REPO/Saved/Logs"
export UE_SKIP_UBT_SDK_SETUP=1
timeout 2400 "/d/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" ProperFootballEditor Win64 Development -Project="$(cygpath -w "$REPO/ProperFootball.uproject")" -WaitMutex -NoHotReloadFromIDE -Module=ProperFootball > "$REPO/Saved/Logs/build.log" 2>&1
R=$?
grep -E "error |error:|LNK[0-9]|Result:" "$REPO/Saved/Logs/build.log" | head -40
echo "build exit $R"
bash "$REPO/Tools/ue/sync_buildid.sh"
