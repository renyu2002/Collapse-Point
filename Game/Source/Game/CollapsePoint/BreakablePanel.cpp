#include "CollapsePoint/BreakablePanel.h"
#include "CollapsePoint/SuckableInterface.h"
#include "CollapsePoint/CollapsePointImpact.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ABreakablePanel::ABreakablePanel()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetWorldScale3D(FVector(0.08f, 2.4f, 2.2f));
	}
}

void ABreakablePanel::BeginPlay()
{
	Super::BeginPlay();
	SpawnTransform = GetActorTransform();
	if (Mesh)
	{
		Mesh->OnComponentHit.AddDynamic(this, &ABreakablePanel::OnPanelHit);
		Mesh->OnComponentBeginOverlap.AddDynamic(this, &ABreakablePanel::OnPanelOverlap);
		if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), IntactColor);
		}
	}
}

bool ABreakablePanel::BlocksAttractionPath(const FVector& Start, const FVector& End) const
{
	if (bShattered || !Mesh)
	{
		return false;
	}

	const FBox BarrierBox = Mesh->Bounds.GetBox().ExpandBy(4.f);
	const FVector Delta = End - Start;
	return BarrierBox.IsInsideOrOn(Start)
		|| BarrierBox.IsInsideOrOn(End)
		|| (!Delta.IsNearlyZero() && FMath::LineBoxIntersection(BarrierBox, Start, End, Delta));
}

void ABreakablePanel::ApplyWellPull(float DeltaTime, float DistToWell)
{
	if (bShattered || !bAllowWellPullBreak)
	{
		return;
	}
	if (DistToWell > 220.f)
	{
		return;
	}
	WellPullAccum += DeltaTime;
	if (WellPullAccum >= WellPullBreakTime)
	{
		Shatter(nullptr);
	}
}

void ABreakablePanel::TryBreakFromActor(AActor* OtherActor, UPrimitiveComponent* OtherComp, float ExtraScore)
{
	if (bShattered || !OtherActor)
	{
		return;
	}

	const bool bSucked = OtherActor->GetClass()->ImplementsInterface(USuckable::StaticClass())
		&& Cast<ISuckable>(OtherActor) && Cast<ISuckable>(OtherActor)->IsBeingSucked();

	float Score = ExtraScore;
	if (OtherComp && OtherComp->IsSimulatingPhysics())
	{
		Score += CollapsePointImpact::ComputeImpactScore(OtherComp->GetPhysicsLinearVelocity().Size(), OtherComp->GetMass(), 1.f);
	}

	if (bSucked || Score >= ImpactBreakScore)
	{
		Shatter(OtherActor);
	}
}

void ABreakablePanel::OnPanelHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	TryBreakFromActor(OtherActor, OtherComp, NormalImpulse.Size() * 0.01f);
}

void ABreakablePanel::OnPanelOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryBreakFromActor(OtherActor, OtherComp, 0.f);
}

void ABreakablePanel::Shatter(AActor* Causer)
{
	if (bShattered)
	{
		return;
	}
	bShattered = true;
	WellPullAccum = 0.f;
	UE_LOG(LogCollapsePoint, Warning, TEXT("[Breakable.Shatter] %s by %s"), *GetName(), *GetNameSafe(Causer));

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ABreakablePanel::Restore()
{
	bShattered = false;
	WellPullAccum = 0.f;
	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	if (Mesh)
	{
		if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), IntactColor);
		}
	}
}
