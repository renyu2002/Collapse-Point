// Collapse Point — ASingularity

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Singularity.generated.h"

class USingularityAttractComponent;
class USphereComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSingularityCollapsed);

/**
 * How the well launches its orbiting mass when it collapses.
 * The well is a machine that throws mass — this picks which throw.
 */
UENUM()
enum class ECollapseLaunch : uint8
{
	/** Damped orbit + converge on the crosshair ray. The aimed throw. */
	AimThrow,
	/** Keep each body's own orbital direction, amplified — the ring becomes a sling. */
	TangentialSling,
	/** Eject every body radially outward at high speed — over-feed detonation. */
	RadialBurst
};

UCLASS()
class GAME_API ASingularity : public AActor
{
	GENERATED_BODY()

public:
	ASingularity();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void BeginAttract();
	void TickDragToward(FVector WorldTarget, float DeltaTime);
	void AdjustOrbitRadius(float InputSteps);
	void AdjustOrbitTilt(float InputSteps);
	void AddConsumedMass(float Mass);
	void Collapse(FVector AimDir, float FlickBoost, const TCHAR* Reason = TEXT("Release"),
		ECollapseLaunch Mode = ECollapseLaunch::AimThrow);
	/** Eject all captured mass radially at high speed, then collapse (cascading burst). */
	void Detonate();
	void ForceCollapseByTimeout();

	float GetCurrentMass() const { return CurrentMass; }
	float GetElapsedTime() const { return ElapsedTime; }
	float GetOrbitRadius() const;
	float GetOrbitTilt() const;
	bool IsCollapsed() const { return bCollapsed; }
	float GetAttractRadius() const;
	float GetVisualScale() const;
	bool IsBlackHole() const;

	UPROPERTY(BlueprintAssignable, Category = "CollapsePoint")
	FOnSingularityCollapsed OnCollapsed;

	/** Max hold time in seconds. <=0 means no timeout (hold until release). */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MaxLifetime = 12.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float DragFollowSpeed = 1200.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float CollapseSpeedBase = 650.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float CollapseSpeedPerMass = 25.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MaxCollapseSpeed = 1100.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MaxMassForCollapse = 20.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float ScatterAngleDeg = 2.f;

	/** Fraction of orbit velocity retained on release; low values keep crosshair throws readable. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float InheritedOrbitVelocityScale = 0.03f;

	/** Orbiting bodies converge on the crosshair ray this far ahead of the well. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float FlingConvergenceDistance = 650.f;

	/** Tangential sling: multiply each body's live orbital speed by this on release. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Launch")
	float TangentialSlingScale = 2.2f;

	/** Tangential sling: floor added to the slung speed so a slow ring still throws. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Launch")
	float TangentialSlingBonus = 260.f;

	/** Radial burst: speed every body is ejected outward at when the well detonates. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Launch")
	float BurstSpeed = 1500.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float SelfDangerRadius = 160.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float SelfDangerImpulse = 420.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float SelfDangerDamage = 8.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Visual")
	float BaseVisualScale = 0.35f;

	/** Visual growth per consumed mass unit. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Visual")
	float VisualScalePerMass = 0.04f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Visual")
	float MaxVisualScale = 1.4f;

	/** Mass at which the singularity becomes a "big black hole" stage. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Visual")
	float BlackHoleMassThreshold = 8.f;

	/** Extra scale kick once black-hole stage is reached. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Visual")
	float BlackHoleScaleBonus = 0.8f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<USphereComponent> RootSphere;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<USingularityAttractComponent> AttractComponent;

	float CurrentMass = 0.f;
	float ElapsedTime = 0.f;
	bool bCollapsed = false;
	bool bAttracting = false;

	void UpdateVisualScale();
};
