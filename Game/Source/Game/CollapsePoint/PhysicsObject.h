// Collapse Point — APhysicsObject

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SuckableInterface.h"
#include "PhysicsObject.generated.h"

class UStaticMeshComponent;

UCLASS()
class GAME_API APhysicsObject : public AActor, public ISuckable
{
	GENERATED_BODY()

public:
	APhysicsObject();

	virtual void BeginPlay() override;

	virtual bool CanBeSucked() const override;
	virtual float GetSuckMass() const override;
	virtual UPrimitiveComponent* GetSuckPrimitive() const override;
	virtual void OnSuckedTick(const FVector& Force) override;

	/** Snap back to spawn transform and clear velocity. */
	UFUNCTION(BlueprintCallable, Category = "CollapsePoint")
	void ResetToSpawn();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CollapsePoint")
	float MassScale = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CollapsePoint")
	float MaxMassContribution = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CollapsePoint")
	bool bCanBeSucked = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CollapsePoint")
	float DamageScale = 1.f;

	FTransform SpawnTransform;
};
