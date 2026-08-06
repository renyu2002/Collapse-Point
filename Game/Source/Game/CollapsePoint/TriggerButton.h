// Collapse Point — impact-activated button that opens a linked door

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TriggerButton.generated.h"

class UStaticMeshComponent;
class ASlideDoor;

UCLASS()
class GAME_API ATriggerButton : public AActor
{
	GENERATED_BODY()

public:
	ATriggerButton();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "CollapsePoint")
	void ActivateButton(AActor* InstigatorActor);

	bool IsActivated() const { return bActivated; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CollapsePoint")
	TObjectPtr<ASlideDoor> LinkedDoor;

protected:
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MinImpactScore = 20000.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	bool bActivated = false;
};
