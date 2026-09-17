#include "CollapsePoint/TriggerButton.h"
#include "CollapsePoint/SlideDoor.h"
#include "CollapsePoint/SuckableInterface.h"
#include "CollapsePoint/CollapsePointImpact.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "UObject/ConstructorHelpers.h"

ATriggerButton::ATriggerButton()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	ScrapeVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ScrapeVolume"));
	ScrapeVolume->SetupAttachment(Mesh);
	ScrapeVolume->SetBoxExtent(FVector(110.f, 110.f, 60.f));
	ScrapeVolume->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
	ScrapeVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ScrapeVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ScrapeVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	ScrapeVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	ScrapeVolume->SetGenerateOverlapEvents(true);

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
	if (ScrapeVolume)
	{
		if (MinSuckMass > KINDA_SMALL_NUMBER)
		{
			ScrapeVolume->SetBoxExtent(MassReceiverExtent);
		}
		ScrapeVolume->OnComponentBeginOverlap.AddDynamic(this, &ATriggerButton::OnScrapeOverlap);
	}
}

void ATriggerButton::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bActivated || !ScrapeVolume)
	{
		return;
	}

	TArray<AActor*> Overlapping;
	ScrapeVolume->GetOverlappingActors(Overlapping);
	for (AActor* Actor : Overlapping)
	{
		if (TryActivateFromActor(Actor, nullptr, 0.f))
		{
			return;
		}
	}
}

bool ATriggerButton::TryActivateFromActor(AActor* OtherActor, UPrimitiveComponent* OtherComp, float ImpactHint)
{
	if (bActivated || !OtherActor)
	{
		return false;
	}

	ISuckable* Suckable = OtherActor->GetClass()->ImplementsInterface(USuckable::StaticClass())
		? Cast<ISuckable>(OtherActor)
		: nullptr;

	if (Suckable && Suckable->IsBeingSucked() && Suckable->GetSuckMass() >= MinSuckMass)
	{
		ActivateButton(OtherActor);
		return true;
	}

	UPrimitiveComponent* ImpactComp = OtherComp;
	if (!ImpactComp && Suckable)
	{
		ImpactComp = Suckable->GetSuckPrimitive();
	}
	if (ImpactComp && ImpactComp->IsSimulatingPhysics())
	{
		const float Score = CollapsePointImpact::ComputeImpactScore(
			ImpactComp->GetPhysicsLinearVelocity().Size(), ImpactComp->GetMass(), 1.f) + ImpactHint;
		if (Score >= MinImpactScore && (!Suckable || Suckable->GetSuckMass() >= MinSuckMass))
		{
			ActivateButton(OtherActor);
			return true;
		}
	}

	return false;
}

void ATriggerButton::OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	TryActivateFromActor(OtherActor, OtherComp, NormalImpulse.Size() * 0.01f);
}

void ATriggerButton::OnScrapeOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryActivateFromActor(OtherActor, OtherComp, 0.f);
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

void ATriggerButton::ResetButton()
{
	bActivated = false;
	if (Mesh)
	{
		if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.05f, 0.05f));
		}
	}
	if (LinkedDoor)
	{
		LinkedDoor->ResetDoor();
	}
}
