#include "CollapsePoint/Singularity.h"
#include "CollapsePoint/SingularityAttractComponent.h"
#include "CollapsePoint/SuckableInterface.h"
#include "CollapsePoint/CollapsePointCharacter.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ASingularity::ASingularity()
{
	PrimaryActorTick.bCanEverTick = true;

	RootSphere = CreateDefaultSubobject<USphereComponent>(TEXT("RootSphere"));
	RootSphere->InitSphereRadius(40.f);
	RootSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(RootSphere);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(RootSphere);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(SphereMesh.Object);
		VisualMesh->SetWorldScale3D(FVector(0.4f));
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SingularityMat(TEXT("/Game/CollapsePoint/Materials/M_Singularity.M_Singularity"));
	if (SingularityMat.Succeeded())
	{
		VisualMesh->SetMaterial(0, SingularityMat.Object);
	}

	AttractComponent = CreateDefaultSubobject<USingularityAttractComponent>(TEXT("AttractComponent"));
}

void ASingularity::BeginPlay()
{
	Super::BeginPlay();
	BeginAttract();
}

void ASingularity::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AttractComponent)
	{
		AttractComponent->StopAttract();
	}
	Super::EndPlay(EndPlayReason);
}

void ASingularity::BeginAttract()
{
	bAttracting = true;
	bCollapsed = false;
	ElapsedTime = 0.f;
	UE_LOG(LogCollapsePoint, Warning,
		TEXT("[Singularity.Begin] MaxLifetime=%.2f CollapseSpeedBase=%.1f CollapseSpeedPerMass=%.1f Scatter=%.1f Loc=(%.0f,%.0f,%.0f)"),
		MaxLifetime, CollapseSpeedBase, CollapseSpeedPerMass, ScatterAngleDeg,
		GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
	if (AttractComponent)
	{
		AttractComponent->BeginAttract();
	}
}

float ASingularity::GetAttractRadius() const
{
	return AttractComponent ? AttractComponent->GetCurrentRadius() : 400.f;
}

float ASingularity::GetOrbitRadius() const
{
	return AttractComponent ? AttractComponent->GetCurrentOrbitRadius() : 150.f;
}

float ASingularity::GetOrbitTilt() const
{
	return AttractComponent ? AttractComponent->GetCurrentOrbitTilt() : 0.f;
}

void ASingularity::AdjustOrbitRadius(float InputSteps)
{
	if (!bCollapsed && AttractComponent)
	{
		AttractComponent->AdjustOrbitRadius(InputSteps);
	}
}

void ASingularity::AdjustOrbitTilt(float InputSteps)
{
	if (!bCollapsed && AttractComponent)
	{
		AttractComponent->AdjustOrbitTilt(InputSteps);
	}
}

float ASingularity::GetVisualScale() const
{
	float Scale = BaseVisualScale + CurrentMass * VisualScalePerMass;
	if (CurrentMass >= BlackHoleMassThreshold)
	{
		Scale += BlackHoleScaleBonus;
	}
	return FMath::Clamp(Scale, BaseVisualScale, MaxVisualScale);
}

bool ASingularity::IsBlackHole() const
{
	return CurrentMass >= BlackHoleMassThreshold;
}

void ASingularity::AddConsumedMass(float Mass)
{
	const bool bWasBlackHole = IsBlackHole();
	CurrentMass += FMath::Max(0.f, Mass);
	UpdateVisualScale();
	if (!bWasBlackHole && IsBlackHole())
	{
		UE_LOG(LogCollapsePoint, Error,
			TEXT("[Singularity.BlackHole] Mass=%.2f Visual=%.2f CaptureR=%.1f — big black hole stage reached"),
			CurrentMass, GetVisualScale(), GetAttractRadius());
	}
}

void ASingularity::UpdateVisualScale()
{
	if (!VisualMesh)
	{
		return;
	}
	const float Scale = GetVisualScale();
	VisualMesh->SetWorldScale3D(FVector(Scale));
	if (RootSphere)
	{
		RootSphere->SetSphereRadius(FMath::Max(20.f, Scale * 50.f));
	}
}

void ASingularity::TickDragToward(FVector WorldTarget, float DeltaTime)
{
	if (bCollapsed)
	{
		return;
	}
	const FVector Current = GetActorLocation();
	// Placement is already collision-resolved by the character; snap smoothly without tunneling.
	const FVector NewLoc = FMath::VInterpTo(Current, WorldTarget, DeltaTime, 14.f);
	SetActorLocation(NewLoc, false, nullptr, ETeleportType::None);
}

void ASingularity::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bCollapsed || !bAttracting)
	{
		return;
	}

	ElapsedTime += DeltaTime;
	if (MaxLifetime > KINDA_SMALL_NUMBER && ElapsedTime >= MaxLifetime)
	{
		ForceCollapseByTimeout();
	}
}

void ASingularity::Detonate()
{
	// Cascading collapse: eject all captured mass radially, then die. Triggered by a
	// level-authored overload/detonator so ordinary black-hole gate puzzles are
	// never disturbed — over-feeding becomes a deliberate omnidirectional launch.
	UE_LOG(LogCollapsePoint, Error,
		TEXT("[Singularity.Detonate] Mass=%.2f — radial burst"), CurrentMass);
	Collapse(FVector::ZeroVector, 0.f, TEXT("OverfeedBurst"), ECollapseLaunch::RadialBurst);
}

void ASingularity::ForceCollapseByTimeout()
{
	UE_LOG(LogCollapsePoint, Error,
		TEXT("[Singularity.Timeout] Elapsed=%.2f Mass=%.2f — swallow orbit ammo (famine)"),
		ElapsedTime, CurrentMass);

	if (AttractComponent)
	{
		AttractComponent->SwallowAllOrbiting();
		AttractComponent->StopAttract();
	}

	bCollapsed = true;
	bAttracting = false;

	if (ACollapsePointCharacter* Character = Cast<ACollapsePointCharacter>(GetInstigator()))
	{
		Character->OnWellTimeout();
	}

	OnCollapsed.Broadcast();
	Destroy();
}

void ASingularity::Collapse(FVector AimDir, float FlickBoost, const TCHAR* Reason, ECollapseLaunch Mode)
{
	if (bCollapsed)
	{
		return;
	}
	bCollapsed = true;
	bAttracting = false;

	if (AttractComponent)
	{
		AttractComponent->StopAttract();
	}

	AimDir = AimDir.GetSafeNormal();
	if (AimDir.IsNearlyZero())
	{
		AimDir = GetActorForwardVector();
	}

	const float MassForCollapse = FMath::Min(CurrentMass, MaxMassForCollapse);
	float Speed = CollapseSpeedBase + CollapseSpeedPerMass * MassForCollapse + FlickBoost;
	Speed = FMath::Min(Speed, MaxCollapseSpeed);

	const TArray<TWeakObjectPtr<AActor>>& Attracted = AttractComponent
		? AttractComponent->GetAttractedActors()
		: TArray<TWeakObjectPtr<AActor>>();

	const TCHAR* ModeName =
		Mode == ECollapseLaunch::TangentialSling ? TEXT("TangentialSling") :
		Mode == ECollapseLaunch::RadialBurst ? TEXT("RadialBurst") : TEXT("AimThrow");
	UE_LOG(LogCollapsePoint, Error,
		TEXT("[Singularity.Collapse] Reason=%s Mode=%s Elapsed=%.2f Mass=%.2f MassForCollapse=%.2f FlickBoost=%.1f Speed=%.1f Aim=(%.2f,%.2f,%.2f) Bodies=%d"),
		Reason, ModeName, ElapsedTime, CurrentMass, MassForCollapse, FlickBoost, Speed, AimDir.X, AimDir.Y, AimDir.Z, Attracted.Num());

	const FVector Center = GetActorLocation();
	for (const TWeakObjectPtr<AActor>& WeakActor : Attracted)
	{
		AActor* Actor = WeakActor.Get();
		if (!Actor || !Actor->GetClass()->ImplementsInterface(USuckable::StaticClass()))
		{
			continue;
		}

		ISuckable* Suckable = Cast<ISuckable>(Actor);
		UPrimitiveComponent* Prim = Suckable ? Suckable->GetSuckPrimitive() : nullptr;
		if (!Prim || !Prim->IsSimulatingPhysics())
		{
			continue;
		}

		const FVector BodyPos = Prim->GetComponentLocation();

		if (Mode == ECollapseLaunch::TangentialSling)
		{
			// The ring is the sling: keep the body's own travel direction, amplified.
			// Release phase — not the crosshair — decides where it goes.
			FVector Vel = Prim->GetPhysicsLinearVelocity();
			FVector Dir = Vel.GetSafeNormal();
			if (Dir.IsNearlyZero() && AttractComponent)
			{
				Dir = AttractComponent->GetTangentialDirectionAt(BodyPos);
			}
			const float SlingSpeed = FMath::Clamp(
				Vel.Size() * TangentialSlingScale + TangentialSlingBonus,
				TangentialSlingBonus, MaxCollapseSpeed * 1.6f);
			UE_LOG(LogCollapsePoint, Warning,
				TEXT("  [Collapse.Sling] %s VelBefore=%.1f SlingSpeed=%.1f Dir=(%.2f,%.2f,%.2f)"),
				*GetNameSafe(Actor), Vel.Size(), SlingSpeed, Dir.X, Dir.Y, Dir.Z);
			Prim->SetPhysicsLinearVelocity(Dir * SlingSpeed);
			continue;
		}

		if (Mode == ECollapseLaunch::RadialBurst)
		{
			// Over-feed detonation: fling every body straight outward. One collapse
			// becomes an omnidirectional launch that can feed / trigger everything around.
			FVector Dir = (BodyPos - Center).GetSafeNormal();
			if (Dir.IsNearlyZero())
			{
				Dir = FMath::VRand();
			}
			UE_LOG(LogCollapsePoint, Warning,
				TEXT("  [Collapse.Burst] %s BurstSpeed=%.1f Dir=(%.2f,%.2f,%.2f)"),
				*GetNameSafe(Actor), BurstSpeed, Dir.X, Dir.Y, Dir.Z);
			Prim->SetPhysicsLinearVelocity(Dir * BurstSpeed);
			continue;
		}

		FVector ImpulseDir = AimDir;
		if (FlingConvergenceDistance > KINDA_SMALL_NUMBER)
		{
			const FVector ConvergencePoint = Center + AimDir * FlingConvergenceDistance;
			ImpulseDir = (ConvergencePoint - BodyPos).GetSafeNormal();
		}
		if (ScatterAngleDeg > KINDA_SMALL_NUMBER)
		{
			ImpulseDir = FMath::VRandCone(AimDir, FMath::DegreesToRadians(ScatterAngleDeg));
		}

		// Velocity change (not momentum*mass) — keeps fling readable and mass-independent.
		UE_LOG(LogCollapsePoint, Warning,
			TEXT("  [Collapse.Impulse] %s PhysMass=%.2f DeltaV=%.1f VelBefore=%.1f Dir=(%.2f,%.2f,%.2f)"),
			*GetNameSafe(Actor), Prim->GetMass(), Speed, Prim->GetPhysicsLinearVelocity().Size(),
			ImpulseDir.X, ImpulseDir.Y, ImpulseDir.Z);

		// Release is a crosshair-directed throw. Retain a little orbit character,
		// but do not let tangential speed overwhelm the player's chosen direction.
		Prim->SetPhysicsLinearVelocity(Prim->GetPhysicsLinearVelocity() * InheritedOrbitVelocityScale);
		Prim->AddImpulse(ImpulseDir * Speed, NAME_None, true);
	}

	if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const float Dist = FVector::Dist(Player->GetActorLocation(), GetActorLocation());
		if (Dist <= SelfDangerRadius)
		{
			const FVector Away = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			if (ACharacter* Character = Cast<ACharacter>(Player))
			{
				Character->LaunchCharacter(Away * SelfDangerImpulse + FVector(0, 0, SelfDangerImpulse * 0.35f), true, true);
			}
			UGameplayStatics::ApplyDamage(Player, SelfDangerDamage, GetInstigatorController(), this, UDamageType::StaticClass());
		}
	}

	OnCollapsed.Broadcast();
	Destroy();
}
