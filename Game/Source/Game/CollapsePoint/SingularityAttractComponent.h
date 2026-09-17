// Collapse Point — USingularityAttractComponent
// Gravity well: capture into a stable orbit. Swallow only on timeout.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SingularityAttractComponent.generated.h"

class ISuckable;
class ASingularity;
class UPrimitiveComponent;

UCLASS(ClassGroup = (CollapsePoint), meta = (BlueprintSpawnableComponent))
class GAME_API USingularityAttractComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USingularityAttractComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void BeginAttract();
	void StopAttract();
	void SwallowAllOrbiting();
	void AdjustOrbitRadius(float InputSteps);
	void AdjustOrbitTilt(float InputSteps);
	bool IsActorOrbiting(const AActor* Actor) const;

	const TArray<TWeakObjectPtr<AActor>>& GetAttractedActors() const { return AttractedActors; }

	/** Orbit tangent (travel direction) for a body at the given world position. */
	FVector GetTangentialDirectionAt(const FVector& BodyWorldPos) const;

	float GetCurrentRadius() const;
	float GetCurrentOrbitRadius() const { return TargetOrbitRadius; }
	float GetCurrentOrbitTilt() const { return CurrentOrbitTiltDegrees; }
	float GetGravityScale() const;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Capture")
	float BaseAttractRadius = 420.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Capture")
	float RadiusPerMass = 18.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Capture")
	float MaxCaptureRadius = 700.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float OrbitRadius = 150.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float MinOrbitRadius = 80.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float MaxOrbitRadius = 380.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float OrbitRadiusStep = 42.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float OrbitRadiusJitter = 40.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float OrbitSpeed = 620.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float OrbitAlignStrength = 10.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float RadialSpring = 12.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float CapturePullAccel = 1600.f;

	/** Straight-line approach speed before a body is close enough to orbit. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float CaptureApproachSpeed = 760.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float MaxSuckSpeed = 1100.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float MaxOrbitAccel = 3800.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	FVector OrbitAxis = FVector(0.f, 0.f, 1.f);

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	FVector OrbitTiltAxis = FVector(1.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float OrbitTiltStepDegrees = 15.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Orbit")
	float MaxOrbitTiltDegrees = 90.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Gravity")
	float GravityPerMass = 0.08f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Gravity")
	float MaxGravityScale = 2.2f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float PlayerAttractScale = 0.18f;

protected:
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> AttractedActors;

	TMap<TWeakObjectPtr<AActor>, float> BodyOrbitRadius;
	TSet<TWeakObjectPtr<AActor>> MassContributed;

	bool bAttracting = false;
	float TargetOrbitRadius = 150.f;
	float CurrentOrbitTiltDegrees = 0.f;

	ASingularity* GetSingularity() const;
	void AttractPlayer(APawn* Player, float Radius, float ForceMag) const;
	float GetOrAssignOrbitRadius(AActor* Actor);
	FVector GetCurrentOrbitAxis() const;
	FVector ComputeOrbitTangential(const FVector& RadialOut) const;
	void ConsumeBody(AActor* Actor, ISuckable* Suckable, ASingularity* Singularity);
	void PullBreakables(const FVector& Center, float Radius, float DeltaTime) const;
	bool IsPathSuppressed(const FVector& Start, const FVector& End) const;
	bool IsBlockedByIntactBreakable(const FVector& Start, const UPrimitiveComponent* TargetPrimitive) const;

	float DebugLogTimer = 0.f;
	float DebugLogInterval = 0.35f;
};
