// Collapse Point — USingularityAttractComponent
// Bodies spiral inward, get consumed, and feed the growing black hole.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SingularityAttractComponent.generated.h"

class ISuckable;
class ASingularity;

UCLASS(ClassGroup = (CollapsePoint), meta = (BlueprintSpawnableComponent))
class GAME_API USingularityAttractComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USingularityAttractComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void BeginAttract();
	void StopAttract();

	const TArray<TWeakObjectPtr<AActor>>& GetAttractedActors() const { return AttractedActors; }

	float GetCurrentRadius() const;
	float GetGravityScale() const;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Capture")
	float BaseAttractRadius = 520.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Capture")
	float RadiusPerMass = 25.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Capture")
	float MaxCaptureRadius = 1200.f;

	/** Starting orbital radius when first captured. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float OrbitRadius = 160.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float OrbitRadiusJitter = 50.f;

	/** Orbit spiral target shrinks toward this (keep near 0 so bodies reach the center). */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float OrbitRadiusMin = 0.f;

	/** Seconds for a body to spiral from outer ring into the core, then be consumed. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float OrbitShrinkDuration = 4.0f;

	/**
	 * Fixed swallow distance from singularity center (NOT visual mesh radius).
	 * Bodies are also force-consumed when their spiral timer finishes.
	 */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float ConsumeDistance = 40.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float OrbitSpeed = 650.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float OrbitAlignStrength = 9.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float RadialSpring = 10.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float CapturePullAccel = 1200.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float MaxSuckSpeed = 1100.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float MaxOrbitAccel = 3200.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	FVector OrbitAxis = FVector(0.f, 0.f, 1.f);

	/** Extra gravity multiplier per consumed mass unit. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Gravity")
	float GravityPerMass = 0.12f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Gravity")
	float MaxGravityScale = 4.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float PlayerAttractScale = 0.f;

protected:
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> AttractedActors;

	/** Initial ring radius assigned on first capture (before time shrink). */
	TMap<TWeakObjectPtr<AActor>, float> BodyOrbitRadius;

	/** Local time since each body was captured (drives per-body spiral). */
	TMap<TWeakObjectPtr<AActor>, float> BodyCaptureTime;

	bool bAttracting = false;

	ASingularity* GetSingularity() const;
	void AttractPlayer(APawn* Player, float Radius, float ForceMag) const;
	float GetOrAssignOrbitRadius(AActor* Actor);
	float GetSpiralOrbitRadius(AActor* Actor, float BodyAge) const;
	FVector ComputeOrbitTangential(const FVector& RadialOut) const;
	void ConsumeBody(AActor* Actor, ISuckable* Suckable, ASingularity* Singularity);

	float DebugLogTimer = 0.f;
	float DebugLogInterval = 0.25f;
};
