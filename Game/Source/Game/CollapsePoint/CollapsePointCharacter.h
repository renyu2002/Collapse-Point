// Collapse Point — ACollapsePointCharacter

#pragma once

#include "CoreMinimal.h"
#include "GameCharacter.h"
#include "CollapsePointCharacter.generated.h"

class ASingularity;
class APhysicsObject;
class UInputAction;
class UInputMappingContext;
class AHUD;

/**
 * Third-person character with singularity create / drag / collapse controls.
 * Space = jump. LMB hold/release = singularity. R = spawn more physics cubes.
 */
UCLASS()
class GAME_API ACollapsePointCharacter : public AGameCharacter
{
	GENERATED_BODY()

public:
	ACollapsePointCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void DoLook(float Yaw, float Pitch) override;

	UFUNCTION(BlueprintCallable, Category = "CollapsePoint")
	FVector GetAimDirection() const;

	UFUNCTION(BlueprintCallable, Category = "CollapsePoint")
	FVector GetAimWorldLocation() const;

	float ConsumeFlickBoost();

protected:
	void OnSingularityStarted();
	void OnSingularityReleased();
	void OnSpawnMorePhysicsObjects();
	void CachePhysicsSpawnSlots();
	void SpawnExtraPhysicsObjects();

	void SampleFlick(float YawDelta, float PitchDelta);
	bool CanSpawnSingularity() const;
	void EnsureCrosshairHUD();
	void ApplyAimViewCamera();
	FVector ComputeSingularityLocation() const;
	void GatherAimIgnoredActors(TArray<AActor*>& OutIgnored) const;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SingularityAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ResetAction;

	/** Extra IMC that binds LMB -> singularity and R -> spawn more cubes. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> SingularityMappingContext;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	TSubclassOf<ASingularity> SingularityClass;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	TSubclassOf<AHUD> CrosshairHUDClass;

	/** Fallback class if a spawn slot somehow loses its class. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	TSubclassOf<APhysicsObject> PhysicsObjectClass;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float AimTraceDistance = 2500.f;

	/** Max distance from the player that the singularity may exist. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MaxHoldDistance = 750.f;

	/** Sphere probe radius used to keep the singularity out of walls. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float SingularityProbeRadius = 36.f;

	/** Push off hit surfaces so the ball is not embedded. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float SurfaceClearance = 10.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float SingularityCooldown = 0.3f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float FlickWindow = 0.12f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float FlickBoostScale = 200.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MaxFlickBoost = 300.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MinSpawnDistance = 120.f;

	/** Horizontal jitter so new cubes don't stack perfectly on old ones. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float ExtraCubeSpawnJitter = 40.f;

	UPROPERTY(Transient)
	TObjectPtr<ASingularity> ActiveSingularity;

	struct FPhysicsSpawnSlot
	{
		TSubclassOf<APhysicsObject> Class;
		FTransform Transform;
	};

	/** Level layout captured once at BeginPlay; R spawns extra cubes at these slots. */
	TArray<FPhysicsSpawnSlot> PhysicsSpawnSlots;

	float CooldownRemaining = 0.f;
	float FlickAccumulator = 0.f;
	float FlickTimer = 0.f;
};
