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
	virtual bool IsBeingSucked() const override;

	float GetDamageScale() const { return DamageScale; }
	bool IsBroken() const { return bBroken; }

	void ConfigureAsHeavyAmmo();

	/** Snap back to spawn transform and clear velocity. */
	UFUNCTION(BlueprintCallable, Category = "CollapsePoint")
	void ResetToSpawn();

protected:
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

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

	/** Prevent a single Chaos contact from applying damage every sub-step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CollapsePoint")
	float ImpactDamageCooldown = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CollapsePoint|Fragile")
	bool bFragile = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CollapsePoint|Fragile")
	float FragileBreakScore = 120000.f;

	FTransform SpawnTransform;
	float LastSuckedWorldTime = -1000.f;
	TMap<TWeakObjectPtr<AActor>, float> LastImpactDamageTimes;
	bool bBroken = false;

	void BreakFragile(AActor* HitActor);
};
