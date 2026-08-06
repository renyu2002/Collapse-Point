// Collapse Point — explosive barrel (heavy ammo)

#pragma once

#include "CoreMinimal.h"
#include "PhysicsObject.h"
#include "ExplosiveBarrel.generated.h"

UCLASS()
class GAME_API AExplosiveBarrel : public APhysicsObject
{
	GENERATED_BODY()

public:
	AExplosiveBarrel();

	virtual void BeginPlay() override;

protected:
	UFUNCTION()
	void OnBarrelHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	void Explode(AActor* DamageCauser);

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Barrel")
	float ExplodeImpactScore = 28000.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Barrel")
	float ExplosionDamage = 160.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Barrel")
	float ExplosionRadius = 450.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Barrel")
	float ExplosionImpulse = 1200.f;

	/** Ignore collisions for this long after spawn so settle bumps don't detonate. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Barrel")
	float ArmDelay = 1.0f;

	bool bExploded = false;
	float SpawnWorldTime = -1000.f;
};
