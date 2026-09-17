#include "CollapsePoint/ChamberBlock.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AChamberBlock::AChamberBlock()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AChamberBlock::BeginPlay()
{
	Super::BeginPlay();
	SetBlockColor(FLinearColor(0.82f, 0.82f, 0.78f));
}

void AChamberBlock::SetBlockColor(FLinearColor Color)
{
	if (!Mesh)
	{
		return;
	}
	if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}
