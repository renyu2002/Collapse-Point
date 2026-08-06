// Collapse Point — APhysicsObject

#include "CollapsePoint/PhysicsObject.h"
#include "CollapsePoint/CollapsePointImpact.h"
#include "Components/StaticMeshComponent.h"
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
		Mesh->OnComponentHit.AddDynamic(this, &APhysicsObject::OnMeshHit);
	}
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
	CollapsePointImpact::TryApplyImpactDamage(this, OtherActor, Score, GetInstigatorController());
}

void APhysicsObject::ResetToSpawn()
{
	if (Mesh && Mesh->IsSimulatingPhysics())
	{
		Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (Mesh && Mesh->IsSimulatingPhysics())
	{
		Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}

bool APhysicsObject::CanBeSucked() const
{
	return bCanBeSucked && Mesh != nullptr && Mesh->IsSimulatingPhysics();
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
	if (Mesh && Mesh->IsSimulatingPhysics())
	{
		Mesh->AddForce(Force, NAME_None, true);
	}
}
