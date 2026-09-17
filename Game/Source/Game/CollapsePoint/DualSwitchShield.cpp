#include "CollapsePoint/DualSwitchShield.h"
#include "CollapsePoint/SuckableInterface.h"
#include "CollapsePoint/CollapsePointImpact.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ADualSwitchShield::ADualSwitchShield()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
	ShieldMesh->SetupAttachment(Root);
	ShieldMesh->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	ShieldMesh->SetWorldScale3D(FVector(0.15f, 4.0f, 3.0f));
	ShieldMesh->SetCollisionProfileName(TEXT("BlockAll"));

	LeftSwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftSwitch"));
	LeftSwitchMesh->SetupAttachment(Root);
	LeftSwitchMesh->SetRelativeLocation(FVector(-200.f, -350.f, 40.f));
	LeftSwitchMesh->SetWorldScale3D(FVector(0.5f, 0.5f, 0.2f));
	LeftSwitchMesh->SetNotifyRigidBodyCollision(true);
	LeftSwitchMesh->SetGenerateOverlapEvents(true);
	LeftSwitchMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	RightSwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightSwitch"));
	RightSwitchMesh->SetupAttachment(Root);
	RightSwitchMesh->SetRelativeLocation(FVector(-200.f, 350.f, 40.f));
	RightSwitchMesh->SetWorldScale3D(FVector(0.5f, 0.5f, 0.2f));
	RightSwitchMesh->SetNotifyRigidBodyCollision(true);
	RightSwitchMesh->SetGenerateOverlapEvents(true);
	RightSwitchMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	LeftSwitchSensor = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftSwitchSensor"));
	LeftSwitchSensor->SetupAttachment(Root);
	LeftSwitchSensor->SetRelativeLocation(FVector(-200.f, -350.f, 40.f));
	LeftSwitchSensor->SetBoxExtent(FVector(220.f, 330.f, 100.f));
	LeftSwitchSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	LeftSwitchSensor->SetCollisionResponseToAllChannels(ECR_Ignore);
	LeftSwitchSensor->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	LeftSwitchSensor->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	LeftSwitchSensor->SetGenerateOverlapEvents(true);

	RightSwitchSensor = CreateDefaultSubobject<UBoxComponent>(TEXT("RightSwitchSensor"));
	RightSwitchSensor->SetupAttachment(Root);
	RightSwitchSensor->SetRelativeLocation(FVector(-200.f, 350.f, 40.f));
	RightSwitchSensor->SetBoxExtent(FVector(220.f, 330.f, 100.f));
	RightSwitchSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RightSwitchSensor->SetCollisionResponseToAllChannels(ECR_Ignore);
	RightSwitchSensor->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	RightSwitchSensor->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	RightSwitchSensor->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ShieldMesh->SetStaticMesh(CubeMesh.Object);
		LeftSwitchMesh->SetStaticMesh(CubeMesh.Object);
		RightSwitchMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void ADualSwitchShield::BeginPlay()
{
	Super::BeginPlay();
	LeftSwitchSensor->SetBoxExtent(SwitchSensorExtent);
	RightSwitchSensor->SetBoxExtent(SwitchSensorExtent);
	LeftSwitchMesh->OnComponentHit.AddDynamic(this, &ADualSwitchShield::OnLeftHit);
	RightSwitchMesh->OnComponentHit.AddDynamic(this, &ADualSwitchShield::OnRightHit);
	LeftSwitchSensor->OnComponentBeginOverlap.AddDynamic(this, &ADualSwitchShield::OnLeftOverlap);
	RightSwitchSensor->OnComponentBeginOverlap.AddDynamic(this, &ADualSwitchShield::OnRightOverlap);

	if (UMaterialInstanceDynamic* ShieldMID = ShieldMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		ShieldMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.5f, 1.f));
	}
	if (UMaterialInstanceDynamic* LMID = LeftSwitchMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		LMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.8f, 0.1f));
	}
	if (UMaterialInstanceDynamic* RMID = RightSwitchMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		RMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.8f, 0.1f));
	}
}

void ADualSwitchShield::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bShieldDown)
	{
		return;
	}

	TArray<AActor*> LeftOver;
	LeftSwitchSensor->GetOverlappingActors(LeftOver);
	for (AActor* Actor : LeftOver)
	{
		TryActivateSwitch(true, Actor, nullptr, FVector::ZeroVector);
	}
	TArray<AActor*> RightOver;
	RightSwitchSensor->GetOverlappingActors(RightOver);
	for (AActor* Actor : RightOver)
	{
		TryActivateSwitch(false, Actor, nullptr, FVector::ZeroVector);
	}
}

void ADualSwitchShield::OnLeftOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryActivateSwitch(true, OtherActor, OtherComp, FVector::ZeroVector);
}

void ADualSwitchShield::OnRightOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryActivateSwitch(false, OtherActor, OtherComp, FVector::ZeroVector);
}

bool ADualSwitchShield::ActorQualifies(AActor* InstigatorActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse) const
{
	if (!InstigatorActor)
	{
		return false;
	}

	ISuckable* Suckable = InstigatorActor->GetClass()->ImplementsInterface(USuckable::StaticClass())
		? Cast<ISuckable>(InstigatorActor)
		: nullptr;
	if (Suckable && Suckable->IsBeingSucked() && Suckable->GetSuckMass() >= MinSuckMass)
	{
		return true;
	}

	if (OtherComp && OtherComp->IsSimulatingPhysics())
	{
		const float Score = CollapsePointImpact::ComputeImpactScore(
			OtherComp->GetPhysicsLinearVelocity().Size(), OtherComp->GetMass(), 1.f)
			+ NormalImpulse.Size() * 0.01f;
		if (Score >= MinImpactScore && (!Suckable || Suckable->GetSuckMass() >= MinSuckMass))
		{
			return true;
		}
	}
	return false;
}

void ADualSwitchShield::TryActivateSwitch(bool bLeft, AActor* InstigatorActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse)
{
	if (bShieldDown || !ActorQualifies(InstigatorActor, OtherComp, NormalImpulse))
	{
		return;
	}
	if ((bLeft && RightActivator.Get() == InstigatorActor)
		|| (!bLeft && LeftActivator.Get() == InstigatorActor))
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const TWeakObjectPtr<AActor>& ExistingActivator = bLeft ? LeftActivator : RightActivator;
	const float ExistingTime = bLeft ? LeftActivatedTime : RightActivatedTime;
	if (ExistingActivator.IsValid()
		&& ExistingActivator.Get() != InstigatorActor
		&& Now - ExistingTime <= SyncWindow)
	{
		return;
	}

	if (bLeft)
	{
		const bool bNewActivator = LeftActivator.Get() != InstigatorActor;
		LeftActivatedTime = Now;
		LeftActivator = InstigatorActor;
		if (bNewActivator)
		{
			UE_LOG(LogCollapsePoint, Warning, TEXT("[Shield.Left] %s by %s"), *GetName(), *GetNameSafe(InstigatorActor));
			if (UMaterialInstanceDynamic* MID = LeftSwitchMesh->CreateAndSetMaterialInstanceDynamic(0))
			{
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.9f, 0.2f));
			}
		}
	}
	else
	{
		const bool bNewActivator = RightActivator.Get() != InstigatorActor;
		RightActivatedTime = Now;
		RightActivator = InstigatorActor;
		if (bNewActivator)
		{
			UE_LOG(LogCollapsePoint, Warning, TEXT("[Shield.Right] %s by %s"), *GetName(), *GetNameSafe(InstigatorActor));
			if (UMaterialInstanceDynamic* MID = RightSwitchMesh->CreateAndSetMaterialInstanceDynamic(0))
			{
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.9f, 0.2f));
			}
		}
	}

	if (FMath::Abs(LeftActivatedTime - RightActivatedTime) <= SyncWindow
		&& LeftActivatedTime > 0.f && RightActivatedTime > 0.f
		&& LeftActivator.IsValid() && RightActivator.IsValid()
		&& LeftActivator != RightActivator)
	{
		DropShield();
	}
}

void ADualSwitchShield::OnLeftHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	TryActivateSwitch(true, OtherActor, OtherComp, NormalImpulse);
}

void ADualSwitchShield::OnRightHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	TryActivateSwitch(false, OtherActor, OtherComp, NormalImpulse);
}

void ADualSwitchShield::DropShield()
{
	if (bShieldDown)
	{
		return;
	}
	bShieldDown = true;
	UE_LOG(LogCollapsePoint, Error, TEXT("[Shield.Down] %s — exit clear"), *GetName());

	if (ShieldMesh)
	{
		ShieldMesh->SetVisibility(false);
		ShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ADualSwitchShield::ResetShield()
{
	bShieldDown = false;
	LeftActivatedTime = -1000.f;
	RightActivatedTime = -1000.f;
	LeftActivator.Reset();
	RightActivator.Reset();

	if (ShieldMesh)
	{
		ShieldMesh->SetVisibility(true);
		ShieldMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		if (UMaterialInstanceDynamic* MID = ShieldMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.5f, 1.f));
		}
	}
	if (LeftSwitchMesh)
	{
		if (UMaterialInstanceDynamic* MID = LeftSwitchMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.8f, 0.1f));
		}
	}
	if (RightSwitchMesh)
	{
		if (UMaterialInstanceDynamic* MID = RightSwitchMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.8f, 0.1f));
		}
	}
}
