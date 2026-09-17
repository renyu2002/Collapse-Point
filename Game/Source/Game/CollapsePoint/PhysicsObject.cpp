// Collapse Point — APhysicsObject

#include "CollapsePoint/PhysicsObject.h"
#include "CollapsePoint/CollapsePointImpact.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APhysicsObject::APhysicsObject()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetNotifyRigidBodyCollision(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetWorldScale3D(FVector(0.5f));
	}
}

void APhysicsObject::BeginPlay()
{
	Super::BeginPlay();
	SpawnTransform = GetActorTransform();
	if (Mesh)
	{
		if (MaxMassContribution >= 6.f)
		{
			// Still visually and physically heavier than debris, but fits the
			// compressed-orbit apertures without sub-centimetre Chaos wedging.
			Mesh->SetWorldScale3D(FVector(0.68f));
		}
		Mesh->OnComponentHit.AddDynamic(this, &APhysicsObject::OnMeshHit);
		if (bFragile)
		{
			if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
			{
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.85f, 1.f));
			}
		}
	}
}

void APhysicsObject::ConfigureAsHeavyAmmo()
{
	MassScale = 0.04f;
	MaxMassContribution = 6.f;
	DamageScale = 2.f;
	if (Mesh)
	{
		Mesh->SetWorldScale3D(FVector(0.68f));
	}
	SpawnTransform = GetActorTransform();
}

void APhysicsObject::OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (!OtherActor || OtherActor == this || !Mesh)
	{
		return;
	}

	const float Speed = Mesh->GetPhysicsLinearVelocity().Size();
	const float Mass = Mesh->GetMass();
	const float Score = CollapsePointImpact::ComputeImpactScore(Speed, Mass, DamageScale);
	if (bFragile && Score >= FragileBreakScore)
	{
		BreakFragile(OtherActor);
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const TWeakObjectPtr<AActor> TargetKey(OtherActor);
	const float* LastDamageTime = LastImpactDamageTimes.Find(TargetKey);
	if (!LastDamageTime || Now - *LastDamageTime >= ImpactDamageCooldown)
	{
		if (CollapsePointImpact::TryApplyImpactDamage(this, OtherActor, Score, GetInstigatorController()))
		{
			LastImpactDamageTimes.Add(TargetKey, Now);
		}
	}
}

void APhysicsObject::BreakFragile(AActor* HitActor)
{
	if (bBroken || !Mesh)
	{
		return;
	}

	bBroken = true;
	Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetVisibility(false);
	UE_LOG(LogCollapsePoint, Warning, TEXT("[Fragile.Break] %s against %s"),
		*GetName(), *GetNameSafe(HitActor));
}

void APhysicsObject::ResetToSpawn()
{
	LastImpactDamageTimes.Reset();
	bBroken = false;
	if (Mesh)
	{
		Mesh->SetSimulatePhysics(false);
		Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetVisibility(true);
	}

	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (Mesh)
	{
		Mesh->SetSimulatePhysics(true);
		Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}

bool APhysicsObject::CanBeSucked() const
{
	return bCanBeSucked && !bBroken && Mesh != nullptr && Mesh->IsSimulatingPhysics();
}

float APhysicsObject::GetSuckMass() const
{
	if (!Mesh)
	{
		return 0.f;
	}
	return FMath::Min(Mesh->GetMass() * MassScale, MaxMassContribution);
}

UPrimitiveComponent* APhysicsObject::GetSuckPrimitive() const
{
	return Mesh;
}

void APhysicsObject::OnSuckedTick(const FVector& Force)
{
	if (GetWorld())
	{
		LastSuckedWorldTime = GetWorld()->GetTimeSeconds();
	}
	if (Mesh && Mesh->IsSimulatingPhysics())
	{
		Mesh->AddForce(Force, NAME_None, true);
	}
}

bool APhysicsObject::IsBeingSucked() const
{
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - LastSuckedWorldTime) < 0.2f;
}
