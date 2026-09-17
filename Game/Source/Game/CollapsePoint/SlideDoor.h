// Collapse Point — sliding door opened by buttons / shields

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlideDoor.generated.h"

class UStaticMeshComponent;

UCLASS()
class GAME_API ASlideDoor : public AActor
{
	GENERATED_BODY()

public:
	ASlideDoor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "CollapsePoint")
	void OpenDoor();

	UFUNCTION(BlueprintCallable, Category = "CollapsePoint")
	void CloseDoor();

	bool IsOpen() const { return bOpen; }
	void ResetDoor();

protected:
	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	/** Local-space offset applied when fully open. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	FVector OpenOffset = FVector(0.f, 0.f, 280.f);

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float OpenSpeed = 3.f;

	FVector ClosedRelativeLocation = FVector::ZeroVector;
	float OpenAlpha = 0.f;
	bool bOpen = false;
	bool bMoving = false;
};
