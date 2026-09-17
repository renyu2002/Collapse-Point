// Collapse Point — checkpoint volume for respawn

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheckpointVolume.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class GAME_API ACheckpointVolume : public AActor
{
	GENERATED_BODY()

public:
	ACheckpointVolume();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Suppression fields block gravity-well placement/drag paths but let the player and released props pass. */
	bool BlocksSingularityPath(const FVector& Start, const FVector& End) const;
	bool IsSuppressionActive() const;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Suppression")
	bool bSuppressesSingularity = false;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Suppression")
	bool bPulseSuppression = false;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Suppression", meta = (ClampMin = "0.1"))
	float PulseActiveDuration = 1.5f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Suppression", meta = (ClampMin = "0.1"))
	float PulseInactiveDuration = 2.f;

	/** Inverted field: the well may only exist INSIDE this box; dragging it out is blocked. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Suppression")
	bool bInvertContainment = false;

protected:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint|Suppression")
	TObjectPtr<UStaticMeshComponent> FieldVisual;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FieldMID;

	/** Optional explicit respawn transform; otherwise uses this actor's transform. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	FTransform RespawnTransform;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	bool bUseActorTransformAsRespawn = true;

	bool bPulseActive = true;
	float PulseElapsed = 0.f;

	void UpdateSuppressionVisual();
};
