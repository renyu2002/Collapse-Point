#include "CollapsePoint/ExplosiveBarrel.h"
#include "CollapsePoint/CollapsePointImpact.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "CollapsePoint/EnemyPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AExplosiveBarrel::AExplosiveBarrel()
{
	DamageScale = 2.5f;
	MaxMassContribution = 5.f;
	MassScale = 0.015f;

	if (Mesh)
	{
		Mesh->SetWorldScale3D(FVector(0.7f, 0.7f, 1.0f));
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (Cylinder.Succeeded())
		{
			Mesh->SetStaticMesh(Cylinder.Object);
		}
	}
}

void AExplosiveBarrel::BeginPlay()
{
	Super::BeginPlay();
	SpawnWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Mesh)
	{
		Mesh->OnComponentHit.AddDynamic(this, &AExplosiveBarrel::OnBarrelHit);
		if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.25f, 0.05f));
		}
	}
}

void AExplosiveBarrel::OnBarrelHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (bExploded || !Mesh)
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - SpawnWorldTime < ArmDelay)
	{
		return;
	}

	const float Speed = Mesh->GetPhysicsLinearVelocity().Size();
	const float Mass = Mesh->GetMass();
	const float Score = CollapsePointImpact::ComputeImpactScore(Speed, Mass, DamageScale);
	if (Score >= ExplodeImpactScore)
	{
		Explode(OtherActor);
	}
}

void AExplosiveBarrel::Explode(AActor* DamageCauser)
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;
	bCanBeSucked = false;

	const FVector Origin = GetActorLocation();
	UE_LOG(LogCollapsePoint, Error,
		TEXT("[Barrel.Explode] %s Origin=(%.0f,%.0f,%.0f) R=%.0f Dmg=%.0f"),
		*GetName(), Origin.X, Origin.Y, Origin.Z, ExplosionRadius, ExplosionDamage);

	UGameplayStatics::ApplyRadialDamage(
		this,
		ExplosionDamage,
		Origin,
		ExplosionRadius,
		UDamageType::StaticClass(),
		TArray<AActor*>(),
		this,
		nullptr,
		true);

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BarrelBlast), false, this);
	GetWorld()->OverlapMultiByChannel(
		Overlaps,
		Origin,
		FQuat::Identity,
		ECC_PhysicsBody,
		FCollisionShape::MakeSphere(ExplosionRadius),
		Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Prim = Overlap.GetComponent();
		if (!Prim || !Prim->IsSimulatingPhysics())
		{
			continue;
		}
		const FVector Away = (Prim->GetComponentLocation() - Origin).GetSafeNormal();
		const float Dist = FVector::Dist(Prim->GetComponentLocation(), Origin);
		const float Falloff = 1.f - FMath::Clamp(Dist / ExplosionRadius, 0.f, 1.f);
		Prim->AddImpulse(Away * ExplosionImpulse * Falloff, NAME_None, true);

		if (AEnemyPawn* Enemy = Cast<AEnemyPawn>(Overlap.GetActor()))
		{
			const float Score = CollapsePointImpact::ComputeImpactScore(
				ExplosionImpulse * Falloff, Prim->GetMass(), 2.f);
			Enemy->ApplyImpactScore(Score, this);
		}
	}

	Destroy();
}
