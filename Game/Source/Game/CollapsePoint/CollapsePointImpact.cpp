#include "CollapsePoint/CollapsePointImpact.h"
#include "CollapsePoint/EnemyPawn.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

float CollapsePointImpact::ComputeImpactScore(float RelativeSpeed, float Mass, float DamageScale)
{
	return FMath::Max(0.f, RelativeSpeed) * FMath::Max(0.f, Mass) * FMath::Max(0.f, DamageScale);
}

bool CollapsePointImpact::TryApplyImpactDamage(
	AActor* DamageCauser,
	AActor* HitActor,
	float ImpactScore,
	AController* InstigatorController)
{
	if (!HitActor || !DamageCauser || ImpactScore <= KINDA_SMALL_NUMBER || HitActor == DamageCauser)
	{
		return false;
	}

	if (AEnemyPawn* Enemy = Cast<AEnemyPawn>(HitActor))
	{
		Enemy->ApplyImpactScore(ImpactScore, DamageCauser);
		return true;
	}

	// Soft generic damage for other damageable actors (e.g. player friendly-fire later).
	constexpr float GenericMinScore = 40000.f;
	if (ImpactScore < GenericMinScore)
	{
		return false;
	}

	const float Damage = FMath::Clamp(ImpactScore * 0.0008f, 5.f, 40.f);
	UE_LOG(LogCollapsePoint, Warning,
		TEXT("[Impact.Generic] %s -> %s Score=%.0f Damage=%.1f"),
		*GetNameSafe(DamageCauser), *GetNameSafe(HitActor), ImpactScore, Damage);

	UGameplayStatics::ApplyDamage(
		HitActor,
		Damage,
		InstigatorController,
		DamageCauser,
		UDamageType::StaticClass());
	return true;
}
