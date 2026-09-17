// Collapse Point — one test chamber: overlap sets current room, Reset restores props

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TestChamber.generated.h"

class UBoxComponent;

USTRUCT()
struct FChamberPropSlot
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<AActor> Class;

	UPROPERTY()
	FTransform Transform;

	UPROPERTY()
	FVector Scale = FVector(1.f);

	UPROPERTY()
	TWeakObjectPtr<AActor> LiveActor;
};

UCLASS()
class GAME_API ATestChamber : public AActor
{
	GENERATED_BODY()

public:
	ATestChamber();

	virtual void BeginPlay() override;

	void RegisterProp(AActor* Actor);
	void CollectPropsInBounds();
	void ResetChamber();

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	FName ChamberId = NAME_None;

protected:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UBoxComponent> Bounds;

	UPROPERTY()
	TArray<FChamberPropSlot> Props;
};
