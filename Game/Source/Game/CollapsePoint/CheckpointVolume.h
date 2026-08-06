// Collapse Point — checkpoint volume for respawn

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheckpointVolume.generated.h"

class UBoxComponent;

UCLASS()
class GAME_API ACheckpointVolume : public AActor
{
	GENERATED_BODY()

public:
	ACheckpointVolume();

	virtual void BeginPlay() override;

protected:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UBoxComponent> TriggerBox;

	/** Optional explicit respawn transform; otherwise uses this actor's transform. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	FTransform RespawnTransform;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	bool bUseActorTransformAsRespawn = true;
};
