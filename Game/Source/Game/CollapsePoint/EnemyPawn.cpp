#include "CollapsePoint/EnemyPawn.h"
#include "CollapsePoint/CollapsePointImpact.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "CollapsePoint/PhysicsObject.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AEnemyPawn::AEnemyPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	RootCollision = CreateDefaultSubobject<USphereComponent>(TEXT("RootCollision"));
	RootCollision->InitSphereRadius(42.f);
	RootCollision->SetCollisionProfileName(TEXT("PhysicsActor"));
	RootCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RootCollision->SetSimulatePhysics(true);
	RootCollision->SetNotifyRigidBodyCollision(true);
	RootCollision->SetLinearDamping(2.0f);
	RootCollision->SetAngularDamping(4.0f);
	SetRootComponent(RootCollision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootCollision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.85f));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> EnemyMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (EnemyMat.Succeeded())
	{
		Mesh->SetMaterial(0, EnemyMat.Object);
	}
}

void AEnemyPawn::BeginPlay()
{
	Super::BeginPlay();
	CurrentHP = MaxHP;
	bDead = false;

	if (RootCollision)
	{
		RootCollision->OnComponentHit.AddDynamic(this, &AEnemyPawn::OnMeshHit);
		if (Mesh)
		{
			UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0);
			if (MID)
			{
				const FLinearColor Tint = bHeavyEnemy
					? FLinearColor(0.35f, 0.05f, 0.55f)
					: FLinearColor(0.85f, 0.12f, 0.1f);
				MID->SetVectorParameterValue(TEXT("Color"), Tint);
				if (bHeavyEnemy)
				{
					Mesh->SetRelativeScale3D(FVector(1.25f));
					if (RootCollision)
					{
						RootCollision->SetSphereRadius(55.f);
					}
				}
			}
		}
	}

	UE_LOG(LogCollapsePoint, Warning,
		TEXT("[Enemy.Spawn] %s HP=%.0f Threshold=%.0f Heavy=%d WalkSpeed=%.0f Loc=(%.0f,%.0f,%.0f)"),
		*GetName(), MaxHP, GetImpactThreshold(), bHeavyEnemy ? 1 : 0, WalkSpeed,
		GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
}

void AEnemyPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bDead)
	{
		return;
	}

	SuckPulseTimer = FMath::Max(0.f, SuckPulseTimer - DeltaTime);
	bBeingSucked = SuckPulseTimer > 0.f;

	ContactCooldown = FMath::Max(0.f, ContactCooldown - DeltaTime);

	if (!bBeingSucked)
	{
		ChasePlayer(DeltaTime);
	}

	TryContactDamage(DeltaTime);
}

void AEnemyPawn::ChasePlayer(float DeltaTime)
{
	if (!RootCollision || !RootCollision->IsSimulatingPhysics())
	{
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}

	FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
	ToPlayer.Z = 0.f;
	const float Dist = ToPlayer.Size();
	if (Dist < 40.f)
	{
		return;
	}

	const FVector Dir = ToPlayer / Dist;
	const FVector Vel = RootCollision->GetPhysicsLinearVelocity();
	FVector HorizVel = Vel;
	HorizVel.Z = 0.f;

	const FVector Desired = Dir * WalkSpeed;
	const FVector Accel = (Desired - HorizVel) * (MoveAccel / FMath::Max(WalkSpeed, 1.f));
	RootCollision->AddForce(Accel * RootCollision->GetMass(), NAME_None, false);

	// Keep upright-ish: kill excessive angular spin.
	const FVector Ang = RootCollision->GetPhysicsAngularVelocityInDegrees();
	if (Ang.SizeSquared() > FMath::Square(180.f))
	{
		RootCollision->SetPhysicsAngularVelocityInDegrees(Ang.GetClampedToMaxSize(120.f));
	}
}

void AEnemyPawn::TryContactDamage(float DeltaTime)
{
	if (ContactCooldown > 0.f)
	{
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}

	const float Dist = FVector::Dist(Player->GetActorLocation(), GetActorLocation());
	if (Dist > ContactRange)
	{
		return;
	}

	ContactCooldown = ContactInterval;
	UE_LOG(LogCollapsePoint, Warning, TEXT("[Enemy.Contact] %s -> Player Damage=%.1f Dist=%.0f"), *GetName(), ContactDamage, Dist);
	UGameplayStatics::ApplyDamage(Player, ContactDamage, GetController(), this, UDamageType::StaticClass());
}

void AEnemyPawn::OnMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (bDead || !OtherActor || OtherActor == this)
	{
		return;
	}

	const float RelSpeed = RootCollision
		? RootCollision->GetPhysicsLinearVelocity().Size()
		: NormalImpulse.Size() * 0.01f;
	const float Mass = RootCollision ? RootCollision->GetMass() : 50.f;

	// Self slam into walls / static while moving fast (flung by collapse).
	const bool bHitWorld = OtherComp && (OtherComp->GetCollisionObjectType() == ECC_WorldStatic
		|| OtherComp->GetCollisionObjectType() == ECC_WorldDynamic);
	if (bHitWorld && !Cast<APhysicsObject>(OtherActor) && !Cast<AEnemyPawn>(OtherActor))
	{
		const float Score = CollapsePointImpact::ComputeImpactScore(RelSpeed, Mass, DamageScale);
		ApplyImpactScore(Score, OtherActor);
		return;
	}

	// Hit by a physics cube: score uses the cube's mass/speed.
	if (APhysicsObject* Cube = Cast<APhysicsObject>(OtherActor))
	{
		if (UPrimitiveComponent* OtherPrim = Cube->GetSuckPrimitive())
		{
			const float OtherSpeed = OtherPrim->GetPhysicsLinearVelocity().Size();
			const float OtherMass = OtherPrim->GetMass();
			const float Score = CollapsePointImpact::ComputeImpactScore(OtherSpeed, OtherMass, Cube->GetDamageScale());
			ApplyImpactScore(Score, Cube);
		}
		return;
	}

	// Enemy-enemy slam.
	if (AEnemyPawn* OtherEnemy = Cast<AEnemyPawn>(OtherActor))
	{
		const float Score = CollapsePointImpact::ComputeImpactScore(RelSpeed, Mass, DamageScale);
		OtherEnemy->ApplyImpactScore(Score, this);
	}
}

void AEnemyPawn::ApplyImpactScore(float ImpactScore, AActor* DamageCauser)
{
	if (bDead || ImpactScore < GetImpactThreshold())
	{
		if (!bDead && ImpactScore > KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogCollapsePoint, Verbose,
				TEXT("[Enemy.ImpactIgnore] %s Score=%.0f < Threshold=%.0f"),
				*GetName(), ImpactScore, GetImpactThreshold());
		}
		return;
	}

	// Convert excess impact into HP; a solid fling one-shots light enemies.
	const float Damage = FMath::Clamp((ImpactScore - GetImpactThreshold() * 0.35f) * 0.0025f, 25.f, 200.f);
	UE_LOG(LogCollapsePoint, Warning,
		TEXT("[Enemy.Impact] %s Score=%.0f Threshold=%.0f Damage=%.1f HP=%.0f Causer=%s"),
		*GetName(), ImpactScore, GetImpactThreshold(), Damage, CurrentHP, *GetNameSafe(DamageCauser));

	TakeDamage(Damage, FDamageEvent(UDamageType::StaticClass()), nullptr, DamageCauser);
}

float AEnemyPawn::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead)
	{
		return 0.f;
	}

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	CurrentHP = FMath::Max(0.f, CurrentHP - Applied);

	UE_LOG(LogCollapsePoint, Warning,
		TEXT("[Enemy.Damage] %s -%.1f -> HP=%.0f/%0.f Causer=%s"),
		*GetName(), Applied, CurrentHP, MaxHP, *GetNameSafe(DamageCauser));

	if (CurrentHP <= 0.f)
	{
		Die(DamageCauser);
	}
	return Applied;
}

void AEnemyPawn::Die(AActor* DamageCauser)
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	bCanBeSucked = false;

	UE_LOG(LogCollapsePoint, Error, TEXT("[Enemy.Die] %s killed by %s"), *GetName(), *GetNameSafe(DamageCauser));

	if (RootCollision)
	{
		RootCollision->SetNotifyRigidBodyCollision(false);
		RootCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RootCollision->SetSimulatePhysics(false);
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	Destroy();
}

void AEnemyPawn::ConfigureAsHeavy()
{
	bHeavyEnemy = true;
	MaxHP = 160.f;
	CurrentHP = MaxHP;
	WalkSpeed = 160.f;
	ContactDamage = 20.f;

	if (RootCollision)
	{
		RootCollision->SetSphereRadius(55.f);
	}
	if (Mesh)
	{
		Mesh->SetRelativeScale3D(FVector(1.25f));
		if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.35f, 0.05f, 0.55f));
		}
	}
}

bool AEnemyPawn::CanBeSucked() const
{
	return bCanBeSucked && !bDead && RootCollision && RootCollision->IsSimulatingPhysics();
}

float AEnemyPawn::GetSuckMass() const
{
	if (!RootCollision)
	{
		return 0.f;
	}
	return FMath::Min(RootCollision->GetMass() * MassScale, MaxMassContribution);
}

UPrimitiveComponent* AEnemyPawn::GetSuckPrimitive() const
{
	return RootCollision;
}

void AEnemyPawn::OnSuckedTick(const FVector& Force)
{
	SuckPulseTimer = 0.2f;
	bBeingSucked = true;
	if (RootCollision && RootCollision->IsSimulatingPhysics())
	{
		RootCollision->AddForce(Force, NAME_None, true);
	}
}
