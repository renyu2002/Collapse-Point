#include "CollapsePoint/CheckpointVolume.h"
#include "CollapsePoint/CollapsePointCharacter.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "Components/BoxComponent.h"

ACheckpointVolume::ACheckpointVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->InitBoxExtent(FVector(200.f, 200.f, 120.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
}

void ACheckpointVolume::BeginPlay()
{
	Super::BeginPlay();
	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ACheckpointVolume::OnOverlapBegin);
	}
}

void ACheckpointVolume::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACollapsePointCharacter* Character = Cast<ACollapsePointCharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	const FTransform Xf = bUseActorTransformAsRespawn ? GetActorTransform() : RespawnTransform;
	Character->SetCheckpoint(Xf);
	UE_LOG(LogCollapsePoint, Warning, TEXT("[Checkpoint] %s set by %s"), *GetNameSafe(Character), *GetName());
}
