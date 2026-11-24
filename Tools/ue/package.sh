#!/bin/bash
# Package a Windows build: build + cook + stage + pak/iostore, then zip.
#   Tools/ue/package.sh <version> [Shipping|Development]
# Output: D:/PIXFootball/Builds/ProperFootball_v<version>_Windows (+ .zip). Logs: Saved/Logs/package_<version>.log
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
UAT="/d/Epic Games/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat"
VERSION="${1:?version}"
CONFIG="${2:-Shipping}"
OUT="/c/PFBuild/Builds/ProperFootball_v${VERSION}_Windows"
LOG="$REPO/Saved/Logs/package_${VERSION}.log"
mkdir -p "$REPO/Saved/Logs" "$(dirname "$OUT")"
sed -i "s/^ProjectVersion=.*/ProjectVersion=$VERSION/" "$REPO/Config/DefaultGame.ini"
rm -rf "$OUT" "$OUT.staging"
ARGS=(BuildCookRun "-project=$(cygpath -w "$REPO/ProperFootball.uproject")" -noP4 -platform=Win64 "-clientconfig=$CONFIG" -utf8output -unattended
      -build -nocompileeditor -cook -stage -pak -iostore -compressed -prereqs "-stagingdirectory=$(cygpath -w "$OUT.staging")" -nodebuginfo)
T0=$(date +%s)
while true; do
    MSYS2_ARG_CONV_EXCL="*" timeout 18000 "$UAT" "${ARGS[@]}" > "$LOG" 2>&1
    RC=$?
    if [ $RC -ne 0 ] && grep -a -q "ConflictingInstance" "$LOG" && [ $(( $(date +%s) - T0 )) -lt 14400 ]; then echo "UBT busy, retrying in 60 s"; sleep 60; continue; fi
    break
done
echo "UAT exit $RC ($(( $(date +%s) - T0 )) s)"
grep -a -E "Error:|error C|BUILD FAILED|BUILD SUCCESSFUL|AutomationTool exiting" "$LOG" | tail -8
[ $RC -ne 0 ] && exit $RC
mv "$OUT.staging/Windows" "$OUT" && rm -rf "$OUT.staging"
du -sh "$OUT"
(cd "$(dirname "$OUT")" && rm -f "$(basename "$OUT").zip" && powershell.exe -NoProfile -Command "Compress-Archive -Path '$(cygpath -w "$OUT")\\*' -DestinationPath '$(cygpath -w "$OUT").zip' -CompressionLevel Optimal")
ls -la "$OUT.zip"
