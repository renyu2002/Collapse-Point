#include "CollapsePoint/TestChamber.h"
#include "CollapsePoint/CollapsePointCharacter.h"
#include "CollapsePoint/PhysicsObject.h"
#include "CollapsePoint/BreakablePanel.h"
#include "CollapsePoint/CollapseGate.h"
#include "CollapsePoint/DualSwitchShield.h"
#include "CollapsePoint/HangingProp.h"
#include "CollapsePoint/SlideDoor.h"
#include "CollapsePoint/TriggerButton.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"

ATestChamber::ATestChamber()
{
	PrimaryActorTick.bCanEverTick = false;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	SetRootComponent(Bounds);
	Bounds->InitBoxExtent(FVector(400.f, 500.f, 220.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	Bounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Bounds->SetGenerateOverlapEvents(true);
}

void ATestChamber::BeginPlay()
{
	Super::BeginPlay();
	if (Bounds)
	{
		Bounds->OnComponentBeginOverlap.AddDynamic(this, &ATestChamber::OnOverlapBegin);
	}
	CollectPropsInBounds();
}

void ATestChamber::CollectPropsInBounds()
{
	UWorld* World = GetWorld();
	if (!World || !Bounds)
	{
		return;
	}

	const FTransform BoxXf = Bounds->GetComponentTransform();
	const FVector Extent = Bounds->GetScaledBoxExtent();
	auto TryRegister = [this, &BoxXf, &Extent](AActor* Actor)
	{
		if (!Actor || Actor == this)
		{
			return;
		}
		const FVector Local = BoxXf.InverseTransformPosition(Actor->GetActorLocation());
		if (FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z)
		{
			RegisterProp(Actor);
		}
	};

	for (TActorIterator<APhysicsObject> It(World); It; ++It)
	{
		TryRegister(*It);
	}
	for (TActorIterator<ABreakablePanel> It(World); It; ++It)
	{
		TryRegister(*It);
	}
	for (TActorIterator<AHangingProp> It(World); It; ++It)
	{
		TryRegister(*It);
	}
	for (TActorIterator<ATriggerButton> It(World); It; ++It)
	{
		TryRegister(*It);
	}
	for (TActorIterator<ADualSwitchShield> It(World); It; ++It)
	{
		TryRegister(*It);
	}
	for (TActorIterator<ASlideDoor> It(World); It; ++It)
	{
		TryRegister(*It);
	}
	for (TActorIterator<ACollapseGate> It(World); It; ++It)
	{
		TryRegister(*It);
	}

	UE_LOG(LogCollapsePoint, Warning, TEXT("[Chamber.Collect] %s props=%d"), *ChamberId.ToString(), Props.Num());
}

void ATestChamber::RegisterProp(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	if (Props.ContainsByPredicate([Actor](const FChamberPropSlot& Slot)
		{
			return Slot.LiveActor.Get() == Actor;
		}))
	{
		return;
	}
	FChamberPropSlot Slot;
	Slot.Class = Actor->GetClass();
	Slot.Transform = Actor->GetActorTransform();
	Slot.Scale = Actor->GetActorScale3D();
	Slot.LiveActor = Actor;
	Props.Add(Slot);
}

void ATestChamber::ResetChamber()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (FChamberPropSlot& Slot : Props)
	{
		AActor* Live = Slot.LiveActor.Get();
		if (ABreakablePanel* Panel = Cast<ABreakablePanel>(Live))
		{
			Panel->Restore();
			continue;
		}
		if (AHangingProp* Hang = Cast<AHangingProp>(Live))
		{
			Hang->Restore();
			continue;
		}
		if (APhysicsObject* Phys = Cast<APhysicsObject>(Live))
		{
			Phys->ResetToSpawn();
			continue;
		}
		if (ATriggerButton* Button = Cast<ATriggerButton>(Live))
		{
			Button->ResetButton();
			continue;
		}
		if (ADualSwitchShield* Shield = Cast<ADualSwitchShield>(Live))
		{
			Shield->ResetShield();
			continue;
		}
		if (ASlideDoor* Door = Cast<ASlideDoor>(Live))
		{
			Door->ResetDoor();
			continue;
		}
		if (ACollapseGate* Gate = Cast<ACollapseGate>(Live))
		{
			Gate->ResetGate();
			continue;
		}

		if (!IsValid(Live) && Slot.Class)
		{
			AActor* Spawned = World->SpawnActor<AActor>(Slot.Class.Get(), Slot.Transform, Params);
			if (Spawned)
			{
				Spawned->SetActorScale3D(Slot.Scale);
				Slot.LiveActor = Spawned;
			}
		}
	}

	UE_LOG(LogCollapsePoint, Warning, TEXT("[Chamber.Reset] %s props=%d"), *ChamberId.ToString(), Props.Num());
}

void ATestChamber::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ACollapsePointCharacter* Character = Cast<ACollapsePointCharacter>(OtherActor))
	{
		Character->SetCurrentChamber(this);
	}
}
