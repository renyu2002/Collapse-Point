#include "CollapsePoint/SlideDoor.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ASlideDoor::ASlideDoor()
{
	PrimaryActorTick.bCanEverTick = true;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	SetRootComponent(DoorMesh);
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DoorMesh->SetCollisionProfileName(TEXT("BlockAll"));
	DoorMesh->SetSimulatePhysics(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		DoorMesh->SetStaticMesh(CubeMesh.Object);
		DoorMesh->SetWorldScale3D(FVector(0.2f, 3.0f, 3.5f));
	}
}

void ASlideDoor::BeginPlay()
{
	Super::BeginPlay();
	ClosedRelativeLocation = DoorMesh ? DoorMesh->GetRelativeLocation() : FVector::ZeroVector;
}

void ASlideDoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bMoving || !DoorMesh)
	{
		return;
	}

	const float Target = bOpen ? 1.f : 0.f;
	OpenAlpha = FMath::FInterpTo(OpenAlpha, Target, DeltaTime, OpenSpeed);
	DoorMesh->SetRelativeLocation(ClosedRelativeLocation + OpenOffset * OpenAlpha);

	if (FMath::IsNearlyEqual(OpenAlpha, Target, 0.01f))
	{
		OpenAlpha = Target;
		bMoving = false;
		DoorMesh->SetRelativeLocation(ClosedRelativeLocation + OpenOffset * OpenAlpha);
		if (bOpen)
		{
			DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		else
		{
			DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}
}

void ASlideDoor::OpenDoor()
{
	if (bOpen)
	{
		return;
	}
	bOpen = true;
	bMoving = true;
	UE_LOG(LogCollapsePoint, Warning, TEXT("[Door.Open] %s"), *GetName());
}

void ASlideDoor::CloseDoor()
{
	if (!bOpen)
	{
		return;
	}
	bOpen = false;
	bMoving = true;
	if (DoorMesh)
	{
		DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	UE_LOG(LogCollapsePoint, Warning, TEXT("[Door.Close] %s"), *GetName());
}

void ASlideDoor::ResetDoor()
{
	bOpen = false;
	bMoving = false;
	OpenAlpha = 0.f;
	if (DoorMesh)
	{
		DoorMesh->SetRelativeLocation(ClosedRelativeLocation);
		DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}
