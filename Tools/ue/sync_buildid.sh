#!/bin/bash
# The engine is a source build shared with other checkouts: build only this module (-Module=ProperFootball), then copy the
# engine's BuildId into this project's module manifest so the editor accepts the DLL.
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
ENG="/d/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.modules"
ID="$(grep -a -o '"BuildId": "[^"]*"' "$ENG" | head -1)"
sed -i "s/\"BuildId\": \"[^\"]*\"/$ID/" "$REPO/Binaries/Win64/UnrealEditor.modules"
echo "synced $ID"
