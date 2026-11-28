// Proper Football v0.10 - presentation helpers the HUD calls (the goal show's letters, score, replay tag and fade).
#pragma once

#include "CoreMinimal.h"

class UCanvas;
class FPFMatch;

namespace PFPresent
{
	// the goal sequence over the pitch: G-O-A-L dropping in, the score ticking up, the REPLAY tag and the cut to black.
	// U = HUD units (screen height / 1080). Returns true while the score card is up (the HUD adds the scorer's name under it).
	bool DrawGoalOverlay(UCanvas* C, const FPFMatch& M, float U, float& OutNameY);
	// chunky pixel text (our own 5x7 font: G O A L R E P Y ! - and digits), face / side / shadow colours, Px = one font pixel
	void PixelText(UCanvas* C, const FString& S, float CX, float Y, float Px, FLinearColor Face, FLinearColor Side, FLinearColor Shadow, float ScaleX = 1.f, float ScaleY = 1.f);
	float PixelTextWidth(const FString& S, float Px);
}
