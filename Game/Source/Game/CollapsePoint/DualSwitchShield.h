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
	bool HasLeftActivated() const { return LeftActivatedTime > 0.f; }
	bool HasRightActivated() const { return RightActivatedTime > 0.f; }
	void ResetShield();

protected:
	UFUNCTION()
	void OnLeftHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnRightHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnLeftOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnRightOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void TryActivateSwitch(bool bLeft, AActor* InstigatorActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse);
	void DropShield();
	bool ActorQualifies(AActor* InstigatorActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse) const;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> ShieldMesh;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> LeftSwitchMesh;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> RightSwitchMesh;

	/** Generous query volumes make an orbiting body's sweep readable and reliable. */
	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UBoxComponent> LeftSwitchSensor;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UBoxComponent> RightSwitchSensor;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	FVector SwitchSensorExtent = FVector(220.f, 330.f, 100.f);

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float SyncWindow = 0.65f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MinImpactScore = 25000.f;

	/** Small debris cannot scrape C-zone switches. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MinSuckMass = 2.2f;

	float LeftActivatedTime = -1000.f;
	float RightActivatedTime = -1000.f;
	TWeakObjectPtr<AActor> LeftActivator;
	TWeakObjectPtr<AActor> RightActivator;
	bool bShieldDown = false;
};
