#include "CollapsePoint/CheckpointVolume.h"
#include "CollapsePoint/CollapsePointCharacter.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ACheckpointVolume::ACheckpointVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->InitBoxExtent(FVector(200.f, 200.f, 120.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);

	FieldVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FieldVisual"));
	FieldVisual->SetupAttachment(TriggerBox);
	FieldVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FieldVisual->SetVisibility(false);
	FieldVisual->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CubeMesh.Succeeded())
	{
		FieldVisual->SetStaticMesh(CubeMesh.Object);
	}
	if (BasicMaterial.Succeeded())
	{
		FieldVisual->SetMaterial(0, BasicMaterial.Object);
	}
}

void ACheckpointVolume::BeginPlay()
{
	Super::BeginPlay();
	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACheckpointVolume::OnOverlapBegin);
	}

	const bool bIsSuppressionField = bSuppressesSingularity || bInvertContainment || ActorHasTag(TEXT("CP_Suppressor"));
	if (bIsSuppressionField && TriggerBox && FieldVisual)
	{
		const FVector Extent = TriggerBox->GetUnscaledBoxExtent();
		FieldVisual->SetRelativeScale3D(Extent / 50.f);
		FieldVisual->SetVisibility(true);
		FieldMID = FieldVisual->CreateAndSetMaterialInstanceDynamic(0);
		bPulseActive = true;
		PulseElapsed = 0.f;
		SetActorTickEnabled(bPulseSuppression);
		UpdateSuppressionVisual();
	}
}

void ACheckpointVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bPulseSuppression)
	{
		return;
	}

	PulseElapsed += DeltaTime;
	const float CurrentDuration = bPulseActive ? PulseActiveDuration : PulseInactiveDuration;
	if (PulseElapsed >= CurrentDuration)
	{
		PulseElapsed = 0.f;
		bPulseActive = !bPulseActive;
		UpdateSuppressionVisual();
		UE_LOG(LogCollapsePoint, Warning, TEXT("[Suppression.Pulse] %s %s"),
			*GetName(), bPulseActive ? TEXT("ACTIVE") : TEXT("OPEN"));
	}
}

bool ACheckpointVolume::IsSuppressionActive() const
{
	const bool bIsSuppressionField = bSuppressesSingularity || ActorHasTag(TEXT("CP_Suppressor"));
	return bIsSuppressionField && (!bPulseSuppression || bPulseActive);
}

void ACheckpointVolume::UpdateSuppressionVisual()
{
	if (!FieldVisual)
	{
		return;
	}

	FieldVisual->SetVisibility(true);
	if (FieldMID)
	{
		FLinearColor Color = FLinearColor(0.04f, 0.18f, 0.22f);
		if (bInvertContainment)
		{
			// Containment cell: cool teal — "the only place the well may live".
			Color = FLinearColor(0.05f, 0.35f, 0.5f);
		}
		else if (IsSuppressionActive())
		{
			Color = FLinearColor(1.f, 0.03f, 0.01f);
		}
		FieldMID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

bool ACheckpointVolume::BlocksSingularityPath(const FVector& Start, const FVector& End) const
{
	if (!TriggerBox)
	{
		return false;
	}

	const FTransform BoxTransform = TriggerBox->GetComponentTransform();
	const FVector LocalStart = BoxTransform.InverseTransformPosition(Start);
	const FVector LocalEnd = BoxTransform.InverseTransformPosition(End);
	const FVector Extent = TriggerBox->GetUnscaledBoxExtent();
	const FBox LocalBox(-Extent, Extent);

	if (bInvertContainment)
	{
		// The well may only exist inside the cell. Block any attempt to drag it
		// from inside to outside (scoped: only fires when the well is already inside).
		const bool bStartInside = LocalBox.IsInsideOrOn(LocalStart);
		const bool bEndInside = LocalBox.IsInsideOrOn(LocalEnd);
		return bStartInside && !bEndInside;
	}

	if (!IsSuppressionActive())
	{
		return false;
	}

	const FVector LocalDelta = LocalEnd - LocalStart;
	return LocalBox.IsInsideOrOn(LocalStart)
		|| LocalBox.IsInsideOrOn(LocalEnd)
		|| (!LocalDelta.IsNearlyZero()
			&& FMath::LineBoxIntersection(LocalBox, LocalStart, LocalEnd, LocalDelta));
}

void ACheckpointVolume::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bSuppressesSingularity || bInvertContainment || ActorHasTag(TEXT("CP_Suppressor")))
	{
		return;
	}

	ACollapsePointCharacter* Character = Cast<ACollapsePointCharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	const FTransform Xf = bUseActorTransformAsRespawn ? GetActorTransform() : RespawnTransform;
	Character->SetCheckpoint(Xf);
	UE_LOG(LogCollapsePoint, Warning, TEXT("[Checkpoint] %s set by %s"), *GetNameSafe(Character), *GetName());
}
