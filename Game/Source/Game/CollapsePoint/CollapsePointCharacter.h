// Collapse Point — ACollapsePointCharacter

#pragma once

#include "CoreMinimal.h"
#include "GameCharacter.h"
#include "CollapsePointCharacter.generated.h"

class ASingularity;
class APhysicsObject;
class AEnemyPawn;
class ASlideDoor;
class ATriggerButton;
class AExplosiveBarrel;
class ADualSwitchShield;
class ACheckpointVolume;
class ATestChamber;
class AChamberBlock;
class UWorld;
struct FActorSpawnParameters;
class UInputAction;
class UInputMappingContext;
class AHUD;

/**
 * Third-person character with singularity create / drag / collapse controls.
 * Space = jump. LMB hold/release = gravity well. R = reset current chamber props.
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
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category = "CollapsePoint")
	FVector GetAimDirection() const;

	UFUNCTION(BlueprintCallable, Category = "CollapsePoint")
	FVector GetAimWorldLocation() const;

	float ConsumeFlickBoost();

	void SetCheckpoint(const FTransform& Transform);
	void RespawnAtCheckpoint();
	void SetCurrentChamber(ATestChamber* Chamber);
	void OnWellTimeout();
	void ResetCurrentChamber();

	/** Test-only control surface used by the unattended playthrough driver. */
	bool BeginAutomationSingularityAt(const FVector& WorldTarget);
	void SetAutomationSingularityTarget(const FVector& WorldTarget);
	void AdjustAutomationOrbitRadius(float InputSteps);
	void AdjustAutomationOrbitTilt(float InputSteps);
	void ReleaseAutomationSingularity(const FVector& AimDirection, bool bTangentialSling = false);
	ASingularity* GetAutomationSingularity() const { return ActiveSingularity; }

protected:
	void OnSingularityStarted();
	void OnSingularityReleased();
	void OnOrbitRadiusInput(float Value);
	void OnResetPressed();
	void CachePhysicsSpawnSlots();
	void SpawnLightEnemies();
	void BuildVerticalSliceLayout();
	void DieAndRespawn();

	AChamberBlock* SpawnBlock(UWorld* World, const FVector& Loc, const FRotator& Rot, const FVector& Scale, FActorSpawnParameters& Params, FLinearColor Color);

	void SampleFlick(float YawDelta, float PitchDelta);
	bool CanSpawnSingularity() const;
	bool IsSingularityPathSuppressed(const FVector& Start, const FVector& End) const;
	void EnsureCrosshairHUD();
	void ApplyAimViewCamera();
	FVector ComputeSingularityLocation() const;
	void GatherAimIgnoredActors(TArray<AActor*>& OutIgnored) const;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SingularityAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ResetAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> SingularityMappingContext;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	TSubclassOf<ASingularity> SingularityClass;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	TSubclassOf<AHUD> CrosshairHUDClass;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	TSubclassOf<APhysicsObject> PhysicsObjectClass;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float AimTraceDistance = 2500.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MaxHoldDistance = 750.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float SingularityProbeRadius = 36.f;

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

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float ExtraCubeSpawnJitter = 40.f;

	/** Spawn this many light enemies near cube slots on BeginPlay (0 = none). Chambers spawn none. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	int32 AutoSpawnEnemyCount = 0;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	TSubclassOf<AEnemyPawn> EnemyClass;

	/** Auto-spawn A/B/C demo props once per session if none exist. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	bool bAutoBuildVerticalSlice = false;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Player")
	float MaxHP = 100.f;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint|Player")
	float CurrentHP = 100.f;

	UPROPERTY(Transient)
	TObjectPtr<ASingularity> ActiveSingularity;

	struct FPhysicsSpawnSlot
	{
		TSubclassOf<APhysicsObject> Class;
		FTransform Transform;
	};

	TArray<FPhysicsSpawnSlot> PhysicsSpawnSlots;

	FTransform CheckpointTransform;
	bool bHasCheckpoint = false;
	bool bIsDead = false;
	float RespawnDelay = 1.2f;
	float RespawnTimer = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<ATestChamber> CurrentChamber;

	bool bAutomationAimOverride = false;
	FVector AutomationAimWorldTarget = FVector::ZeroVector;

	float CooldownRemaining = 0.f;
	float FlickAccumulator = 0.f;
	float FlickTimer = 0.f;
};
