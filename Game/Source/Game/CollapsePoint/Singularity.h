// Collapse Point — ASingularity

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Singularity.generated.h"

class USingularityAttractComponent;
class USphereComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSingularityCollapsed);

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
	void AddConsumedMass(float Mass);
	void Collapse(FVector AimDir, float FlickBoost, const TCHAR* Reason = TEXT("Release"));
	void ForceCollapseByTimeout();

	float GetCurrentMass() const { return CurrentMass; }
	float GetElapsedTime() const { return ElapsedTime; }
	bool IsCollapsed() const { return bCollapsed; }
	float GetAttractRadius() const;
	float GetVisualScale() const;
	bool IsBlackHole() const;

	UPROPERTY(BlueprintAssignable, Category = "CollapsePoint")
	FOnSingularityCollapsed OnCollapsed;

	/** Max hold time in seconds. <=0 means no timeout (hold until release). */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MaxLifetime = 0.f;

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
	float ScatterAngleDeg = 8.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float SelfDangerRadius = 0.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float SelfDangerImpulse = 0.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float SelfDangerDamage = 0.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Visual")
	float BaseVisualScale = 0.35f;

	/** Visual growth per consumed mass unit. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Visual")
	float VisualScalePerMass = 0.09f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Visual")
	float MaxVisualScale = 5.0f;

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
