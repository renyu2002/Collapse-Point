// Collapse Point — AEnemyPawn (light enemy)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SuckableInterface.h"
#include "EnemyPawn.generated.h"

class UStaticMeshComponent;
class USphereComponent;

/**
 * Simple physics-driven light enemy: drifts toward the player, deals contact damage,
 * can be sucked by the singularity, dies from high-impact hits / wall slams.
 */
UCLASS()
class GAME_API AEnemyPawn : public APawn, public ISuckable
{
	GENERATED_BODY()

public:
	AEnemyPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	virtual bool CanBeSucked() const override;
	virtual float GetSuckMass() const override;
	virtual UPrimitiveComponent* GetSuckPrimitive() const override;
	virtual void OnSuckedTick(const FVector& Force) override;
	virtual bool IsBeingSucked() const override { return bBeingSucked; }

	float GetImpactThreshold() const { return bHeavyEnemy ? HeavyImpactThreshold : ImpactThreshold; }
	bool IsAlive() const { return !bDead && CurrentHP > 0.f; }

	/** Apply TECH_SPEC impact score; damages only if Score >= ImpactThreshold. */
	void ApplyImpactScore(float ImpactScore, AActor* DamageCauser);

	void ConfigureAsHeavy();

protected:
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	void ChasePlayer(float DeltaTime);
	void TryContactDamage(float DeltaTime);
	void Die(AActor* DamageCauser);

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<USphereComponent> RootCollision;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Combat")
	float MaxHP = 100.f;

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint|Combat")
	float CurrentHP = 100.f;

	/** Min Speed*Mass*DamageScale required to hurt this enemy. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Combat")
	float ImpactThreshold = 18000.f;

	/** Heavy enemies ignore light debris; need barrels / heavy slam. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Combat")
	bool bHeavyEnemy = false;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Combat")
	float HeavyImpactThreshold = 90000.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Combat")
	float ContactDamage = 12.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Combat")
	float ContactRange = 90.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Combat")
	float ContactInterval = 0.6f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Move")
	float WalkSpeed = 280.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Move")
	float MoveAccel = 1800.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Suck")
	float MassScale = 0.01f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Suck")
	float MaxMassContribution = 3.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Suck")
	bool bCanBeSucked = true;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint|Suck")
	float DamageScale = 1.2f;

	float ContactCooldown = 0.f;
	bool bDead = false;
	bool bBeingSucked = false;
	float SuckPulseTimer = 0.f;
};
