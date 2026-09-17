#include "CollapsePoint/SingularityAttractComponent.h"
#include "CollapsePoint/Singularity.h"
#include "CollapsePoint/SuckableInterface.h"
#include "CollapsePoint/BreakablePanel.h"
#include "CollapsePoint/CheckpointVolume.h"
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

bool USingularityAttractComponent::IsActorOrbiting(const AActor* Actor) const
{
	return Actor && AttractedActors.Contains(const_cast<AActor*>(Actor));
}

float USingularityAttractComponent::GetOrAssignOrbitRadius(AActor* Actor)
{
	TWeakObjectPtr<AActor> Key(Actor);
	if (const float* Existing = BodyOrbitRadius.Find(Key))
	{
		return FMath::Clamp(TargetOrbitRadius + *Existing, MinOrbitRadius, MaxOrbitRadius);
	}

	const float HashAlpha = static_cast<float>(GetTypeHash(Actor) % 1000) / 1000.f;
	const float Jitter = OrbitRadiusJitter * (HashAlpha - 0.5f);
	BodyOrbitRadius.Add(Key, Jitter);
	return FMath::Clamp(TargetOrbitRadius + Jitter, MinOrbitRadius, MaxOrbitRadius);
}

FVector USingularityAttractComponent::ComputeOrbitTangential(const FVector& RadialOut) const
{
	const FVector Axis = GetCurrentOrbitAxis();
	FVector Tangential = FVector::CrossProduct(Axis, RadialOut);
	if (!Tangential.Normalize())
	{
		Tangential = FVector::CrossProduct(FVector::RightVector, RadialOut);
		Tangential.Normalize();
	}
	return Tangential;
}

FVector USingularityAttractComponent::GetTangentialDirectionAt(const FVector& BodyWorldPos) const
{
	const ASingularity* Singularity = GetSingularity();
	const FVector Center = Singularity ? Singularity->GetActorLocation() : GetOwner()->GetActorLocation();
	FVector RadialOut = BodyWorldPos - Center;
	if (!RadialOut.Normalize())
	{
		RadialOut = FVector::ForwardVector;
	}
	return ComputeOrbitTangential(RadialOut);
}

FVector USingularityAttractComponent::GetCurrentOrbitAxis() const
{
	const FVector BaseAxis = OrbitAxis.GetSafeNormal(KINDA_SMALL_NUMBER, FVector::UpVector);
	const FVector TiltAxis = OrbitTiltAxis.GetSafeNormal(KINDA_SMALL_NUMBER, FVector::ForwardVector);
	return BaseAxis.RotateAngleAxis(CurrentOrbitTiltDegrees, TiltAxis).GetSafeNormal();
}

void USingularityAttractComponent::ConsumeBody(AActor* Actor, ISuckable* Suckable, ASingularity* Singularity)
{
	if (!Actor || !Suckable || !Singularity)
	{
		return;
	}

	UE_LOG(LogCollapsePoint, Warning, TEXT("[Attract.Swallow] %s (timeout famine)"), *GetNameSafe(Actor));

	const TWeakObjectPtr<AActor> Key(Actor);
	AttractedActors.Remove(Key);
	BodyOrbitRadius.Remove(Key);
	MassContributed.Remove(Key);
	Actor->Destroy();
}

void USingularityAttractComponent::SwallowAllOrbiting()
{
	ASingularity* Singularity = GetSingularity();
	TArray<TWeakObjectPtr<AActor>> Copy = AttractedActors;
	for (const TWeakObjectPtr<AActor>& Weak : Copy)
	{
		AActor* Actor = Weak.Get();
		if (!IsValid(Actor))
		{
			continue;
		}
		ISuckable* Suckable = Cast<ISuckable>(Actor);
		ConsumeBody(Actor, Suckable, Singularity);
	}
	AttractedActors.Reset();
	BodyOrbitRadius.Reset();
}

void USingularityAttractComponent::BeginAttract()
{
	bAttracting = true;
	TargetOrbitRadius = FMath::Clamp(OrbitRadius, MinOrbitRadius, MaxOrbitRadius);
	CurrentOrbitTiltDegrees = 0.f;
	AttractedActors.Reset();
	BodyOrbitRadius.Reset();
	MassContributed.Reset();
	DebugLogTimer = 0.f;
	SetComponentTickEnabled(true);

	UE_LOG(LogCollapsePoint, Warning,
		TEXT("[Attract.Begin.Orbit] CaptureR=%.1f OrbitR=%.1f PlayerPull=%.2f"),
		BaseAttractRadius, TargetOrbitRadius, PlayerAttractScale);
}

void USingularityAttractComponent::AdjustOrbitRadius(float InputSteps)
{
	if (!bAttracting || FMath::IsNearlyZero(InputSteps))
	{
		return;
	}

	const float Previous = TargetOrbitRadius;
	TargetOrbitRadius = FMath::Clamp(
		TargetOrbitRadius + InputSteps * OrbitRadiusStep,
		MinOrbitRadius,
		MaxOrbitRadius);

	if (!FMath::IsNearlyEqual(Previous, TargetOrbitRadius))
	{
		UE_LOG(LogCollapsePoint, Warning,
			TEXT("[Attract.Resize] OrbitR %.0f -> %.0f"),
			Previous, TargetOrbitRadius);
	}
}

void USingularityAttractComponent::AdjustOrbitTilt(float InputSteps)
{
	if (!bAttracting || FMath::IsNearlyZero(InputSteps))
	{
		return;
	}

	const float Previous = CurrentOrbitTiltDegrees;
	CurrentOrbitTiltDegrees = FMath::Clamp(
		CurrentOrbitTiltDegrees + InputSteps * OrbitTiltStepDegrees,
		0.f,
		MaxOrbitTiltDegrees);

	if (!FMath::IsNearlyEqual(Previous, CurrentOrbitTiltDegrees))
	{
		UE_LOG(LogCollapsePoint, Warning,
			TEXT("[Attract.Tilt] Plane %.0f -> %.0f degrees"),
			Previous, CurrentOrbitTiltDegrees);
	}
}

void USingularityAttractComponent::StopAttract()
{
	bAttracting = false;
	SetComponentTickEnabled(false);
	UE_LOG(LogCollapsePoint, Warning, TEXT("[Attract.Stop] AttractedCount=%d"), AttractedActors.Num());
}

void USingularityAttractComponent::PullBreakables(const FVector& Center, float Radius, float DeltaTime) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ABreakablePanel> It(World); It; ++It)
	{
		ABreakablePanel* Panel = *It;
		if (!Panel || Panel->IsShattered())
		{
			continue;
		}
		const float Dist = FVector::Dist(Panel->GetActorLocation(), Center);
		if (Dist <= Radius && !IsPathSuppressed(Center, Panel->GetActorLocation()))
		{
			Panel->ApplyWellPull(DeltaTime, Dist);
		}
	}
}

bool USingularityAttractComponent::IsPathSuppressed(const FVector& Start, const FVector& End) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (TActorIterator<ACheckpointVolume> It(World); It; ++It)
	{
		if (It->BlocksSingularityPath(Start, End))
		{
			return true;
		}
	}
	return false;
}

bool USingularityAttractComponent::IsBlockedByIntactBreakable(
	const FVector& Start,
	const UPrimitiveComponent* TargetPrimitive) const
{
	UWorld* World = GetWorld();
	if (!World || !TargetPrimitive)
	{
		return false;
	}

	const FVector End = TargetPrimitive->GetComponentLocation();
	for (TActorIterator<ABreakablePanel> It(World); It; ++It)
	{
		if (It->BlocksAttractionPath(Start, End))
		{
			return true;
		}
	}
	return false;
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

	PullBreakables(Center, CaptureRadius, DeltaTime);

	DebugLogTimer += DeltaTime;
	const bool bDoLog = DebugLogTimer >= DebugLogInterval;
	if (bDoLog)
	{
		DebugLogTimer = 0.f;
		UE_LOG(LogCollapsePoint, Warning,
			TEXT("[Attract.Tick.Orbit] t=%.2f Mass=%.2f Gravity=%.2f CaptureR=%.1f Attracted=%d"),
			Elapsed, Mass, GravityScale, CaptureRadius, AttractedActors.Num());
	}

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
		if (Dist > CaptureRadius || Dist < KINDA_SMALL_NUMBER
			|| IsPathSuppressed(Center, BodyLoc)
			|| IsBlockedByIntactBreakable(Center, Prim))
		{
			continue;
		}

		const TWeakObjectPtr<AActor> Key(Actor);
		const bool bFirstCapture = !AttractedActors.Contains(Actor);
		if (bFirstCapture)
		{
			AttractedActors.Add(Actor);
			GetOrAssignOrbitRadius(Actor);
			if (!MassContributed.Contains(Key))
			{
				MassContributed.Add(Key);
				Singularity->AddConsumedMass(Suckable->GetSuckMass());
			}
			UE_LOG(LogCollapsePoint, Warning, TEXT("[Attract.Enter] %s Dist=%.1f"), *GetNameSafe(Actor), Dist);
		}

		const float TargetR = GetOrAssignOrbitRadius(Actor);
		const FVector RadialOut = FromCenter / Dist;
		const FVector Tangential = ComputeOrbitTangential(RadialOut);
		const FVector Vel = Prim->GetPhysicsLinearVelocity();
		// Light debris whips around quickly; heavy ammunition trades speed for impact.
		const float BodyMass = FMath::Max(0.f, Suckable->GetSuckMass());
		const float MassSpeedScale = FMath::Clamp(1.12f - BodyMass * 0.07f, 0.68f, 1.1f);
		const float DesiredOrbitSpeed = OrbitSpeed * GravityScale * MassSpeedScale;
		const bool bApproachingOrbit = Dist > TargetR * 1.3f;

		if (bFirstCapture && !bApproachingOrbit)
		{
			const float TangentialNow = FVector::DotProduct(Vel, Tangential);
			if (TangentialNow < DesiredOrbitSpeed * 0.45f)
			{
				Prim->AddImpulse(Tangential * (DesiredOrbitSpeed - FMath::Max(0.f, TangentialNow)), NAME_None, true);
			}
		}

		const float RadialError = Dist - TargetR;
		const float CompressionControl = FMath::Clamp(OrbitRadius / FMath::Max(TargetR, 1.f), 1.f, 2.5f);
		const FVector DesiredVel = bApproachingOrbit
			? -RadialOut * (CaptureApproachSpeed * GravityScale)
			: Tangential * DesiredOrbitSpeed
				- RadialOut * (RadialError * RadialSpring * GravityScale * CompressionControl);

		FVector Accel = (DesiredVel - Vel)
			* (OrbitAlignStrength * GravityScale * (bApproachingOrbit ? 1.f : CompressionControl));

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
		}
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
	if (Dist > Radius || Dist < KINDA_SMALL_NUMBER
		|| IsPathSuppressed(Center, Player->GetActorLocation()))
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
