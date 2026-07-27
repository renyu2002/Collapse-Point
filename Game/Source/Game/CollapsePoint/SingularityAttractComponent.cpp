#include "CollapsePoint/SingularityAttractComponent.h"
#include "CollapsePoint/Singularity.h"
#include "CollapsePoint/SuckableInterface.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

USingularityAttractComponent::USingularityAttractComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

ASingularity* USingularityAttractComponent::GetSingularity() const
{
	return Cast<ASingularity>(GetOwner());
}

float USingularityAttractComponent::GetGravityScale() const
{
	const ASingularity* Singularity = GetSingularity();
	const float Mass = Singularity ? Singularity->GetCurrentMass() : 0.f;
	return FMath::Min(1.f + Mass * GravityPerMass, MaxGravityScale);
}

float USingularityAttractComponent::GetCurrentRadius() const
{
	const ASingularity* Singularity = GetSingularity();
	const float Mass = Singularity ? Singularity->GetCurrentMass() : 0.f;
	return FMath::Min(BaseAttractRadius + RadiusPerMass * Mass, MaxCaptureRadius);
}

float USingularityAttractComponent::GetOrAssignOrbitRadius(AActor* Actor)
{
	TWeakObjectPtr<AActor> Key(Actor);
	if (const float* Existing = BodyOrbitRadius.Find(Key))
	{
		return *Existing;
	}

	const float Jitter = OrbitRadiusJitter * (static_cast<float>(GetTypeHash(Actor) % 1000) / 1000.f);
	const float Assigned = OrbitRadius + Jitter;
	BodyOrbitRadius.Add(Key, Assigned);
	return Assigned;
}

float USingularityAttractComponent::GetSpiralOrbitRadius(AActor* Actor, float BodyAge) const
{
	const TWeakObjectPtr<AActor> Key(Actor);
	const float* Existing = BodyOrbitRadius.Find(Key);
	const float BaseR = Existing ? *Existing : OrbitRadius;

	const float Duration = FMath::Max(OrbitShrinkDuration, KINDA_SMALL_NUMBER);
	float Alpha = FMath::Clamp(BodyAge / Duration, 0.f, 1.f);
	// Slow start, then accelerate into the core.
	Alpha = FMath::Pow(Alpha, 1.4f);

	return FMath::Lerp(BaseR, OrbitRadiusMin, Alpha);
}

FVector USingularityAttractComponent::ComputeOrbitTangential(const FVector& RadialOut) const
{
	const FVector Axis = OrbitAxis.GetSafeNormal();
	FVector Tangential = FVector::CrossProduct(Axis, RadialOut);
	if (!Tangential.Normalize())
	{
		Tangential = FVector::CrossProduct(FVector::RightVector, RadialOut);
		Tangential.Normalize();
	}
	return Tangential;
}

void USingularityAttractComponent::ConsumeBody(AActor* Actor, ISuckable* Suckable, ASingularity* Singularity)
{
	if (!Actor || !Suckable || !Singularity)
	{
		return;
	}

	const float Added = Suckable->GetSuckMass();
	Singularity->AddConsumedMass(Added);

	UE_LOG(LogCollapsePoint, Warning,
		TEXT("[Attract.Consume] %s AddedMass=%.2f -> CurrentMass=%.2f Gravity=%.2f Visual=%.2f BlackHole=%d"),
		*GetNameSafe(Actor), Added, Singularity->GetCurrentMass(), GetGravityScale(),
		Singularity->GetVisualScale(), Singularity->IsBlackHole() ? 1 : 0);

	const TWeakObjectPtr<AActor> Key(Actor);
	AttractedActors.Remove(Key);
	BodyOrbitRadius.Remove(Key);
	BodyCaptureTime.Remove(Key);

	Actor->Destroy();
}

void USingularityAttractComponent::BeginAttract()
{
	bAttracting = true;
	AttractedActors.Reset();
	BodyOrbitRadius.Reset();
	BodyCaptureTime.Reset();
	DebugLogTimer = 0.f;
	SetComponentTickEnabled(true);

	UE_LOG(LogCollapsePoint, Warning,
		TEXT("[Attract.Begin.Spiral] CaptureR=%.1f OrbitR=%.1f->Min=%.1f Shrink=%.1fs ConsumeDist=%.1f GravityPerMass=%.2f"),
		BaseAttractRadius, OrbitRadius, OrbitRadiusMin, OrbitShrinkDuration, ConsumeDistance, GravityPerMass);
}

void USingularityAttractComponent::StopAttract()
{
	bAttracting = false;
	SetComponentTickEnabled(false);
	UE_LOG(LogCollapsePoint, Warning, TEXT("[Attract.Stop] AttractedCount=%d"), AttractedActors.Num());
}

void USingularityAttractComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bAttracting)
	{
		return;
	}

	ASingularity* Singularity = GetSingularity();
	if (!Singularity)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center = Singularity->GetActorLocation();
	const float CaptureRadius = GetCurrentRadius();
	const float GravityScale = GetGravityScale();
	const float Elapsed = Singularity->GetElapsedTime();
	const float Mass = Singularity->GetCurrentMass();

	AttractedActors.RemoveAll([](const TWeakObjectPtr<AActor>& Ptr) { return !Ptr.IsValid(); });

	DebugLogTimer += DeltaTime;
	const bool bDoLog = DebugLogTimer >= DebugLogInterval;
	if (bDoLog)
	{
		DebugLogTimer = 0.f;
		UE_LOG(LogCollapsePoint, Warning,
			TEXT("[Attract.Tick.Spiral] t=%.2f Mass=%.2f Gravity=%.2f CaptureR=%.1f ConsumeDist=%.1f Attracted=%d BlackHole=%d"),
			Elapsed, Mass, GravityScale, CaptureRadius, ConsumeDistance, AttractedActors.Num(),
			Singularity->IsBlackHole() ? 1 : 0);
	}

	TArray<AActor*> ToConsume;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == Singularity || !IsValid(Actor))
		{
			continue;
		}

		if (!Actor->GetClass()->ImplementsInterface(USuckable::StaticClass()))
		{
			continue;
		}

		ISuckable* Suckable = Cast<ISuckable>(Actor);
		if (!Suckable || !Suckable->CanBeSucked())
		{
			continue;
		}

		UPrimitiveComponent* Prim = Suckable->GetSuckPrimitive();
		if (!Prim || !Prim->IsSimulatingPhysics())
		{
			continue;
		}

		const FVector BodyLoc = Prim->GetComponentLocation();
		const FVector FromCenter = BodyLoc - Center;
		const float Dist = FromCenter.Size();
		if (Dist > CaptureRadius)
		{
			continue;
		}

		const TWeakObjectPtr<AActor> Key(Actor);
		const bool bFirstCapture = !AttractedActors.Contains(Actor);
		if (bFirstCapture)
		{
			AttractedActors.Add(Actor);
			BodyCaptureTime.Add(Key, 0.f);
			GetOrAssignOrbitRadius(Actor);
			UE_LOG(LogCollapsePoint, Warning, TEXT("[Attract.Enter] %s Dist=%.1f"), *GetNameSafe(Actor), Dist);
		}

		float* AgePtr = BodyCaptureTime.Find(Key);
		float BodyAge = AgePtr ? *AgePtr : 0.f;
		BodyAge += DeltaTime;
		BodyCaptureTime.Add(Key, BodyAge);

		const float ShrinkDuration = FMath::Max(OrbitShrinkDuration, KINDA_SMALL_NUMBER);
		const float Infall = FMath::Clamp(BodyAge / ShrinkDuration, 0.f, 1.f);

		// Swallow by fixed center distance OR when spiral timer finishes — never by visual mesh size.
		if (Dist <= ConsumeDistance || BodyAge >= ShrinkDuration || Dist < KINDA_SMALL_NUMBER)
		{
			ToConsume.Add(Actor);
			continue;
		}

		const float BaseOrbitR = GetOrAssignOrbitRadius(Actor);
		const float TargetR = GetSpiralOrbitRadius(Actor, BodyAge);
		const FVector RadialOut = FromCenter / Dist;
		const FVector Tangential = ComputeOrbitTangential(RadialOut);
		const FVector Vel = Prim->GetPhysicsLinearVelocity();

		const float SpeedScale = FMath::Clamp(BaseOrbitR / FMath::Max(TargetR, 1.f), 1.f, 2.2f) * GravityScale;
		// Tangential fades out as the body falls into the core so it looks sucked in, not stuck orbiting.
		const float DesiredOrbitSpeed = OrbitSpeed * SpeedScale * (1.f - 0.85f * Infall);

		if (bFirstCapture)
		{
			const float TangentialNow = FVector::DotProduct(Vel, Tangential);
			if (TangentialNow < DesiredOrbitSpeed * 0.45f)
			{
				Prim->AddImpulse(Tangential * (DesiredOrbitSpeed - FMath::Max(0.f, TangentialNow)), NAME_None, true);
			}
		}

		const float RadialError = Dist - TargetR;
		FVector DesiredVel = Tangential * DesiredOrbitSpeed - RadialOut * (RadialError * RadialSpring * GravityScale);
		DesiredVel += -RadialOut * (DesiredOrbitSpeed * 0.55f * Infall * GravityScale + CapturePullAccel * 0.15f * Infall);

		FVector Accel = (DesiredVel - Vel) * (OrbitAlignStrength * GravityScale);
		if (Dist > FMath::Max(TargetR * 1.25f, ConsumeDistance))
		{
			Accel += -RadialOut * (CapturePullAccel * GravityScale);
		}

		if (Accel.SizeSquared() > FMath::Square(MaxOrbitAccel * GravityScale))
		{
			Accel = Accel.GetSafeNormal() * (MaxOrbitAccel * GravityScale);
		}

		Suckable->OnSuckedTick(Accel);

		FVector NewVel = Prim->GetPhysicsLinearVelocity();
		const float SpeedCap = MaxSuckSpeed * GravityScale;
		if (NewVel.SizeSquared() > FMath::Square(SpeedCap))
		{
			Prim->SetPhysicsLinearVelocity(NewVel.GetSafeNormal() * SpeedCap);
			NewVel = Prim->GetPhysicsLinearVelocity();
		}

		if (bDoLog)
		{
			UE_LOG(LogCollapsePoint, Warning,
				TEXT("  [Body] %s Age=%.2f Dist=%.1f TargetR=%.1f ConsumeDist=%.1f Infall=%.2f Vel=%.1f"),
				*GetNameSafe(Actor), BodyAge, Dist, TargetR, ConsumeDistance, Infall, NewVel.Size());
		}
	}

	for (AActor* Actor : ToConsume)
	{
		if (!IsValid(Actor))
		{
			continue;
		}
		ISuckable* Suckable = Cast<ISuckable>(Actor);
		ConsumeBody(Actor, Suckable, Singularity);
	}

	if (APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		if (PlayerAttractScale > KINDA_SMALL_NUMBER)
		{
			AttractPlayer(Player, CaptureRadius, CapturePullAccel * GetGravityScale() * PlayerAttractScale);
		}
	}
}

void USingularityAttractComponent::AttractPlayer(APawn* Player, float Radius, float ForceMag) const
{
	if (!Player)
	{
		return;
	}

	const FVector Center = GetOwner()->GetActorLocation();
	const FVector ToCenter = Center - Player->GetActorLocation();
	const float Dist = ToCenter.Size();
	if (Dist > Radius || Dist < KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Dir = ToCenter / Dist;
	if (ACharacter* Character = Cast<ACharacter>(Player))
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->AddForce(Dir * ForceMag * 0.2f);
		}
	}
}
