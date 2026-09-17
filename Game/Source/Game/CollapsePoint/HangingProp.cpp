#include "CollapsePoint/HangingProp.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "UObject/ConstructorHelpers.h"

AHangingProp::AHangingProp()
{
	PrimaryActorTick.bCanEverTick = false;

	Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Anchor"));
	SetRootComponent(Anchor);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Anchor);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	Mesh->SetSimulatePhysics(true);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetNotifyRigidBodyCollision(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetWorldScale3D(FVector(0.45f));
	}

	Hinge = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Anchor);
	Hinge->SetDisableCollision(true);
	Hinge->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 40.f);
	Hinge->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 40.f);
	Hinge->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 40.f);
}

void AHangingProp::BeginPlay()
{
	Super::BeginPlay();
	SpawnTransform = GetActorTransform();
	if (Hinge && Mesh)
	{
		Hinge->SetConstrainedComponents(nullptr, NAME_None, Mesh, NAME_None);
		Hinge->SetConstraintReferencePosition(EConstraintFrame::Frame1, FVector::ZeroVector);
		Hinge->SetConstraintReferencePosition(EConstraintFrame::Frame2, FVector(0.f, 0.f, 90.f));
	}
	if (Mesh)
	{
		if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.7f, 0.2f));
		}
	}
}

void AHangingProp::BreakHinge()
{
	if (bHingeBroken)
	{
		return;
	}
	bHingeBroken = true;
	if (Hinge)
	{
		Hinge->BreakConstraint();
	}
	UE_LOG(LogCollapsePoint, Warning, TEXT("[Hang.Drop] %s"), *GetName());
}

void AHangingProp::Restore()
{
	Strain = 0.f;
	bHingeBroken = false;
	LastSuckedWorldTime = -1000.f;
	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	if (Mesh)
	{
		Mesh->SetSimulatePhysics(false);
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
		Mesh->SetSimulatePhysics(true);
		Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	if (Hinge && Mesh)
	{
		Hinge->SetConstrainedComponents(nullptr, NAME_None, Mesh, NAME_None);
	}
}

bool AHangingProp::CanBeSucked() const
{
	return Mesh && Mesh->IsSimulatingPhysics();
}

float AHangingProp::GetSuckMass() const
{
	if (!Mesh)
	{
		return 0.f;
	}
	return FMath::Min(Mesh->GetMass() * MassScale, MaxMassContribution);
}

UPrimitiveComponent* AHangingProp::GetSuckPrimitive() const
{
	return Mesh;
}

bool AHangingProp::IsBeingSucked() const
{
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - LastSuckedWorldTime) < 0.2f;
}

void AHangingProp::OnSuckedTick(const FVector& Force)
{
	if (GetWorld())
	{
		LastSuckedWorldTime = GetWorld()->GetTimeSeconds();
	}
	if (Mesh && Mesh->IsSimulatingPhysics())
	{
		Mesh->AddForce(Force, NAME_None, true);
	}
	Strain += Force.Size() * 0.016f;
	if (!bHingeBroken && Strain >= BreakStrain)
	{
		BreakHinge();
	}
}
