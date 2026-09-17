// Collapse Point — pressure plate: orbit scrape OR high-speed impact

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TriggerButton.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class ASlideDoor;

UCLASS()
class GAME_API ATriggerButton : public AActor
{
	GENERATED_BODY()

public:
	ATriggerButton();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "CollapsePoint")
	void ActivateButton(AActor* InstigatorActor);

	bool IsActivated() const { return bActivated; }
	void ResetButton();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CollapsePoint")
	TObjectPtr<ASlideDoor> LinkedDoor;

protected:
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnScrapeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	bool TryActivateFromActor(AActor* OtherActor, UPrimitiveComponent* OtherComp, float ImpactHint);

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UBoxComponent> ScrapeVolume;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MinImpactScore = 20000.f;

	/** 0 = any orbiting mass can scrape. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MinSuckMass = 0.f;

	/** Heavy receivers are broad catchment pads, not precision projectile targets. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	FVector MassReceiverExtent = FVector(260.f, 260.f, 100.f);

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	bool bActivated = false;
};
