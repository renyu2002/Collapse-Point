#include "CollapsePoint/TriggerButton.h"
#include "CollapsePoint/SlideDoor.h"
#include "CollapsePoint/CollapsePointImpact.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ATriggerButton::ATriggerButton()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetWorldScale3D(FVector(0.6f, 0.6f, 0.25f));
	}
}

void ATriggerButton::BeginPlay()
{
	Super::BeginPlay();
	if (Mesh)
	{
		Mesh->OnComponentHit.AddDynamic(this, &ATriggerButton::OnMeshHit);
		if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.05f, 0.05f));
		}
	}
}

void ATriggerButton::OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (bActivated || !OtherActor || !OtherComp)
	{
		return;
	}

	const float Speed = OtherComp->IsSimulatingPhysics()
		? OtherComp->GetPhysicsLinearVelocity().Size()
		: NormalImpulse.Size() * 0.01f;
	const float Mass = OtherComp->IsSimulatingPhysics() ? OtherComp->GetMass() : 50.f;
	const float Score = CollapsePointImpact::ComputeImpactScore(Speed, Mass, 1.f);

	if (Score < MinImpactScore)
	{
		return;
	}

	ActivateButton(OtherActor);
}

void ATriggerButton::ActivateButton(AActor* InstigatorActor)
{
	if (bActivated)
	{
		return;
	}
	bActivated = true;

	UE_LOG(LogCollapsePoint, Warning,
		TEXT("[Button.Activate] %s by %s -> Door=%s"),
		*GetName(), *GetNameSafe(InstigatorActor), *GetNameSafe(LinkedDoor.Get()));

	if (Mesh)
	{
		if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.85f, 0.2f));
		}
	}

	if (LinkedDoor)
	{
		LinkedDoor->OpenDoor();
	}
}
