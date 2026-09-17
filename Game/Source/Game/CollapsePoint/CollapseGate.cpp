#include "CollapsePoint/CollapseGate.h"
#include "CollapsePoint/Singularity.h"
#include "CollapsePoint/SlideDoor.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ACollapseGate::ACollapseGate()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetWorldScale3D(FVector(1.2f, 1.2f, 0.4f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	if (BasicMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, BasicMaterial.Object);
	}
}

void ACollapseGate::BeginPlay()
{
	Super::BeginPlay();
	if (Mesh)
	{
		MeshMID = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	bOpen = false;
	UpdateVisual();
}

void ACollapseGate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bOpen)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center = GetActorLocation();
	for (TActorIterator<ASingularity> It(World); It; ++It)
	{
		ASingularity* Well = *It;
		if (!Well || Well->IsCollapsed())
		{
			continue;
		}
		if (FVector::Dist(Well->GetActorLocation(), Center) > TriggerRadius)
		{
			continue;
		}
		if (bDetonateWell)
		{
			// Overload trigger: wait for a full pile, then force the well to burst.
			if (Well->GetCurrentMass() >= DetonateMassThreshold)
			{
				bOpen = true;
				UpdateVisual();
				UE_LOG(LogCollapsePoint, Error,
					TEXT("[CollapseGate.Detonate] %s force-bursts well Mass=%.2f"),
					*GetName(), Well->GetCurrentMass());
				Well->Detonate();
				break;
			}
		}
		else if (Well->IsBlackHole())
		{
			OpenGate();
			break;
		}
	}
}

void ACollapseGate::OpenGate()
{
	if (bOpen)
	{
		return;
	}
	bOpen = true;
	UpdateVisual();
	if (LinkedDoor)
	{
		LinkedDoor->OpenDoor();
	}
	UE_LOG(LogCollapsePoint, Error, TEXT("[CollapseGate.Open] %s tripped by black-hole well"), *GetName());
}

void ACollapseGate::ResetGate()
{
	bOpen = false;
	UpdateVisual();
	if (LinkedDoor)
	{
		LinkedDoor->ResetDoor();
	}
}

void ACollapseGate::UpdateVisual()
{
	if (MeshMID)
	{
		// Dormant violet -> bright gold when the black hole feeds it.
		MeshMID->SetVectorParameterValue(
			TEXT("Color"),
			bOpen ? FLinearColor(1.f, 0.78f, 0.12f) : FLinearColor(0.22f, 0.05f, 0.32f));
	}
}
