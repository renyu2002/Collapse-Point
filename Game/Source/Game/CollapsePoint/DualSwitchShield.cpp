#include "CollapsePoint/DualSwitchShield.h"
#include "CollapsePoint/CollapsePointImpact.h"
#include "CollapsePoint/CollapsePointLog.h"
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
	LeftSwitchMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	RightSwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightSwitch"));
	RightSwitchMesh->SetupAttachment(Root);
	RightSwitchMesh->SetRelativeLocation(FVector(-200.f, 350.f, 40.f));
	RightSwitchMesh->SetWorldScale3D(FVector(0.5f, 0.5f, 0.2f));
	RightSwitchMesh->SetNotifyRigidBodyCollision(true);
	RightSwitchMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

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
	LeftSwitchMesh->OnComponentHit.AddDynamic(this, &ADualSwitchShield::OnLeftHit);
	RightSwitchMesh->OnComponentHit.AddDynamic(this, &ADualSwitchShield::OnRightHit);

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

void ADualSwitchShield::TryActivateSwitch(bool bLeft, AActor* InstigatorActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse)
{
	if (bShieldDown || !OtherComp)
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

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (bLeft)
	{
		LeftActivatedTime = Now;
		UE_LOG(LogCollapsePoint, Warning, TEXT("[Shield.Left] %s by %s Score=%.0f"), *GetName(), *GetNameSafe(InstigatorActor), Score);
		if (UMaterialInstanceDynamic* MID = LeftSwitchMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.9f, 0.2f));
		}
	}
	else
	{
		RightActivatedTime = Now;
		UE_LOG(LogCollapsePoint, Warning, TEXT("[Shield.Right] %s by %s Score=%.0f"), *GetName(), *GetNameSafe(InstigatorActor), Score);
		if (UMaterialInstanceDynamic* MID = RightSwitchMesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.9f, 0.2f));
		}
	}

	if (FMath::Abs(LeftActivatedTime - RightActivatedTime) <= SyncWindow
		&& LeftActivatedTime > 0.f && RightActivatedTime > 0.f)
	{
		DropShield();
	}
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
