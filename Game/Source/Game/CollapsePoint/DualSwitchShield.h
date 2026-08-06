// Collapse Point — dual-switch energy shield (C zone)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DualSwitchShield.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class GAME_API ADualSwitchShield : public AActor
{
	GENERATED_BODY()

public:
	ADualSwitchShield();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	bool IsShieldDown() const { return bShieldDown; }

protected:
	UFUNCTION()
	void OnLeftHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnRightHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	void TryActivateSwitch(bool bLeft, AActor* InstigatorActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse);
	void DropShield();

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> ShieldMesh;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> LeftSwitchMesh;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> RightSwitchMesh;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float SyncWindow = 0.65f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MinImpactScore = 25000.f;

	float LeftActivatedTime = -1000.f;
	float RightActivatedTime = -1000.f;
	bool bShieldDown = false;
};
