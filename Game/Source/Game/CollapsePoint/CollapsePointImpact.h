// Collapse Point — shared impact damage helpers

#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;
class AActor;
class AController;

namespace CollapsePointImpact
{
	/** Impact score = RelativeSpeed * Mass * DamageScale (TECH_SPEC). */
	float ComputeImpactScore(float RelativeSpeed, float Mass, float DamageScale);

	/**
	 * If OtherActor can take damage and Score >= Threshold on target (when known),
	 * apply damage. Returns true if damage was applied.
	 */
	bool TryApplyImpactDamage(
		AActor* DamageCauser,
		AActor* HitActor,
		float ImpactScore,
		AController* InstigatorController = nullptr);
}
