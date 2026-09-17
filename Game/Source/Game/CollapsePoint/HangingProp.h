// Collapse Point — hanging physics mass that drops when the well pulls hard enough

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SuckableInterface.h"
#include "HangingProp.generated.h"

class UStaticMeshComponent;
class UPhysicsConstraintComponent;
class USceneComponent;

UCLASS()
class GAME_API AHangingProp : public AActor, public ISuckable
{
	GENERATED_BODY()

public:
	AHangingProp();

	virtual void BeginPlay() override;

	virtual bool CanBeSucked() const override;
	virtual float GetSuckMass() const override;
	virtual UPrimitiveComponent* GetSuckPrimitive() const override;
	virtual void OnSuckedTick(const FVector& Force) override;
	virtual bool IsBeingSucked() const override;

	void Restore();

protected:
	void BreakHinge();

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<USceneComponent> Anchor;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UPhysicsConstraintComponent> Hinge;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float BreakStrain = 1800.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MassScale = 0.01f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float MaxMassContribution = 2.f;

	float Strain = 0.f;
	float LastSuckedWorldTime = -1000.f;
	bool bHingeBroken = false;
	FTransform SpawnTransform;
};
