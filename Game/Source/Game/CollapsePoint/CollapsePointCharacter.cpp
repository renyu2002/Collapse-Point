#include "CollapsePoint/CollapsePointCharacter.h"
#include "CollapsePoint/Singularity.h"
#include "CollapsePoint/CollapsePointHUD.h"
#include "CollapsePoint/PhysicsObject.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

ACollapsePointCharacter::ACollapsePointCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	SingularityClass = ASingularity::StaticClass();
	CrosshairHUDClass = ACollapsePointHUD::StaticClass();
	PhysicsObjectClass = APhysicsObject::StaticClass();

	// Mouse rotates the *player* facing (yaw). Camera rides with that aim view
	// over the shoulder so the singularity in front is not blocked by the body.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = false;
		Move->bUseControllerDesiredRotation = false;
		Move->RotationRate = FRotator(0.f, 0.f, 0.f);
	}

	if (USpringArmComponent* Boom = GetCameraBoom())
	{
		Boom->TargetArmLength = 220.f;
		Boom->SocketOffset = FVector(0.f, 75.f, 60.f);
		Boom->bUsePawnControlRotation = true;
		Boom->bDoCollisionTest = true;
		Boom->bEnableCameraLag = false;
		Boom->bEnableCameraRotationLag = false;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> SingularityIA(TEXT("/Game/Input/Actions/IA_Singularity.IA_Singularity"));
	if (SingularityIA.Succeeded())
	{
		SingularityAction = SingularityIA.Object;
	}
	static ConstructorHelpers::FObjectFinder<UInputAction> ResetIA(TEXT("/Game/Input/Actions/IA_Reset.IA_Reset"));
	if (ResetIA.Succeeded())
	{
		ResetAction = ResetIA.Object;
	}
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> SingularityIMC(TEXT("/Game/Input/IMC_CollapsePoint.IMC_CollapsePoint"));
	if (SingularityIMC.Succeeded())
	{
		SingularityMappingContext = SingularityIMC.Object;
	}
}

void ACollapsePointCharacter::ApplyAimViewCamera()
{
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = false;
		Move->bUseControllerDesiredRotation = false;
	}

	if (USpringArmComponent* Boom = GetCameraBoom())
	{
		Boom->TargetArmLength = 220.f;
		Boom->SocketOffset = FVector(0.f, 75.f, 60.f);
		Boom->bUsePawnControlRotation = true;
		Boom->bDoCollisionTest = true;
		Boom->bEnableCameraLag = false;
		Boom->bEnableCameraRotationLag = false;
	}
}

void ACollapsePointCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyAimViewCamera();
	EnsureCrosshairHUD();
	CachePhysicsSpawnSlots();

	if (SingularityMappingContext)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				Subsystem->AddMappingContext(SingularityMappingContext, 1);
			}
		}
	}
}

void ACollapsePointCharacter::EnsureCrosshairHUD()
{
	if (!CrosshairHUDClass)
	{
		return;
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ClientSetHUD(CrosshairHUDClass);
	}
}

void ACollapsePointCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (SingularityAction)
		{
			EIC->BindAction(SingularityAction, ETriggerEvent::Started, this, &ACollapsePointCharacter::OnSingularityStarted);
			EIC->BindAction(SingularityAction, ETriggerEvent::Completed, this, &ACollapsePointCharacter::OnSingularityReleased);
			EIC->BindAction(SingularityAction, ETriggerEvent::Canceled, this, &ACollapsePointCharacter::OnSingularityReleased);
		}
		if (ResetAction)
		{
			EIC->BindAction(ResetAction, ETriggerEvent::Started, this, &ACollapsePointCharacter::OnSpawnMorePhysicsObjects);
		}
	}
}

void ACollapsePointCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaTime);

	if (FlickTimer > 0.f)
	{
		FlickTimer = FMath::Max(0.f, FlickTimer - DeltaTime);
		if (FlickTimer <= 0.f)
		{
			FlickAccumulator = 0.f;
		}
	}

	if (ActiveSingularity && !ActiveSingularity->IsCollapsed())
	{
		ActiveSingularity->TickDragToward(GetAimWorldLocation(), DeltaTime);
	}
	else if (ActiveSingularity && ActiveSingularity->IsCollapsed())
	{
		ActiveSingularity = nullptr;
	}
}

void ACollapsePointCharacter::DoLook(float Yaw, float Pitch)
{
	Super::DoLook(Yaw, Pitch);
	SampleFlick(Yaw, Pitch);
}

void ACollapsePointCharacter::SampleFlick(float YawDelta, float PitchDelta)
{
	if (!ActiveSingularity)
	{
		return;
	}
	const float Mag = FMath::Sqrt(YawDelta * YawDelta + PitchDelta * PitchDelta);
	FlickAccumulator += Mag;
	FlickTimer = FlickWindow;
}

float ACollapsePointCharacter::ConsumeFlickBoost()
{
	const float Boost = FMath::Min(FMath::Clamp(FlickAccumulator, 0.f, 3.f) * FlickBoostScale, MaxFlickBoost);
	FlickAccumulator = 0.f;
	FlickTimer = 0.f;
	return Boost;
}

FVector ACollapsePointCharacter::GetAimDirection() const
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		return PC->GetControlRotation().Vector();
	}
	return GetActorForwardVector();
}

void ACollapsePointCharacter::GatherAimIgnoredActors(TArray<AActor*>& OutIgnored) const
{
	OutIgnored.Reset();
	OutIgnored.Add(const_cast<ACollapsePointCharacter*>(this));
	if (ActiveSingularity)
	{
		OutIgnored.Add(ActiveSingularity);
	}

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<APhysicsObject> It(World); It; ++It)
		{
			OutIgnored.Add(*It);
		}
	}
}

FVector ACollapsePointCharacter::ComputeSingularityLocation() const
{
	UWorld* World = GetWorld();
	const FVector PlayerOrigin = GetActorLocation();
	FVector CamLoc = PlayerOrigin + FVector(0.f, 0.f, BaseEyeHeight);
	FVector CamDir = GetAimDirection();

	if (const UCameraComponent* Cam = GetFollowCamera())
	{
		CamLoc = Cam->GetComponentLocation();
		CamDir = Cam->GetForwardVector();
	}
	CamDir = CamDir.GetSafeNormal();

	TArray<AActor*> Ignored;
	GatherAimIgnoredActors(Ignored);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SingularityAim), false);
	Params.AddIgnoredActors(Ignored);

	// Only collide with world geometry (not physics cubes).
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	// 1) Camera look candidate (clamped to MaxHoldDistance along view).
	const float ViewReach = FMath::Min(AimTraceDistance, MaxHoldDistance + (CamLoc - PlayerOrigin).Size());
	const FVector ViewEnd = CamLoc + CamDir * ViewReach;

	FHitResult ViewHit;
	FVector Desired = ViewEnd;
	if (World && World->LineTraceSingleByObjectType(ViewHit, CamLoc, ViewEnd, ObjParams, Params))
	{
		Desired = ViewHit.ImpactPoint + ViewHit.ImpactNormal * SurfaceClearance;
	}

	// 2) Clamp to [MinSpawnDistance, MaxHoldDistance] from the player.
	FVector FromPlayer = Desired - PlayerOrigin;
	float Dist = FromPlayer.Size();
	FVector DirFromPlayer = FromPlayer.GetSafeNormal();
	if (DirFromPlayer.IsNearlyZero())
	{
		DirFromPlayer = CamDir;
	}

	if (Dist > MaxHoldDistance)
	{
		Desired = PlayerOrigin + DirFromPlayer * MaxHoldDistance;
		Dist = MaxHoldDistance;
	}
	if (Dist < MinSpawnDistance)
	{
		Desired = PlayerOrigin + DirFromPlayer * MinSpawnDistance;
	}

	// 3) Sphere sweep player -> Desired so the ball never embeds in walls.
	if (!World)
	{
		return Desired;
	}

	const FCollisionShape Probe = FCollisionShape::MakeSphere(SingularityProbeRadius);
	FHitResult SweepHit;
	const bool bBlocked = World->SweepSingleByObjectType(
		SweepHit,
		PlayerOrigin,
		Desired,
		FQuat::Identity,
		ObjParams,
		Probe,
		Params);

	if (bBlocked)
	{
		FVector Safe = SweepHit.Location;
		if (SweepHit.bStartPenetrating)
		{
			Safe = PlayerOrigin + DirFromPlayer * MinSpawnDistance;
		}
		else
		{
			Safe = SweepHit.Location + SweepHit.ImpactNormal * SurfaceClearance;
		}

		// Re-clamp after surface push.
		FromPlayer = Safe - PlayerOrigin;
		Dist = FromPlayer.Size();
		if (Dist > MaxHoldDistance)
		{
			Safe = PlayerOrigin + FromPlayer.GetSafeNormal() * MaxHoldDistance;
		}
		if (Dist < MinSpawnDistance && !FromPlayer.IsNearlyZero())
		{
			Safe = PlayerOrigin + FromPlayer.GetSafeNormal() * MinSpawnDistance;
		}
		return Safe;
	}

	return Desired;
}

FVector ACollapsePointCharacter::GetAimWorldLocation() const
{
	return ComputeSingularityLocation();
}

bool ACollapsePointCharacter::CanSpawnSingularity() const
{
	return CooldownRemaining <= 0.f
		&& SingularityClass
		&& (ActiveSingularity == nullptr || ActiveSingularity->IsCollapsed());
}

void ACollapsePointCharacter::OnSingularityStarted()
{
	if (!CanSpawnSingularity() || !GetWorld())
	{
		return;
	}

	const FVector SpawnLoc = GetAimWorldLocation();
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveSingularity = GetWorld()->SpawnActor<ASingularity>(SingularityClass, SpawnLoc, FRotator::ZeroRotator, Params);
	FlickAccumulator = 0.f;
	FlickTimer = 0.f;
}

void ACollapsePointCharacter::OnSingularityReleased()
{
	if (!ActiveSingularity || ActiveSingularity->IsCollapsed())
	{
		ActiveSingularity = nullptr;
		return;
	}

	const FVector AimDir = GetAimDirection();
	const float Boost = ConsumeFlickBoost();
	ActiveSingularity->Collapse(AimDir, Boost);
	ActiveSingularity = nullptr;
	CooldownRemaining = SingularityCooldown;
}

void ACollapsePointCharacter::CachePhysicsSpawnSlots()
{
	if (PhysicsSpawnSlots.Num() > 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<APhysicsObject> It(World); It; ++It)
	{
		APhysicsObject* Obj = *It;
		if (!Obj)
		{
			continue;
		}

		FPhysicsSpawnSlot Slot;
		Slot.Class = Obj->GetClass();
		Slot.Transform = Obj->GetActorTransform();
		PhysicsSpawnSlots.Add(Slot);
	}

	UE_LOG(LogCollapsePoint, Warning, TEXT("[Respawn.Cache] Captured %d physics spawn slots"), PhysicsSpawnSlots.Num());
}

void ACollapsePointCharacter::SpawnExtraPhysicsObjects()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (PhysicsSpawnSlots.Num() == 0)
	{
		CachePhysicsSpawnSlots();
	}

	if (PhysicsSpawnSlots.Num() == 0)
	{
		UE_LOG(LogCollapsePoint, Warning, TEXT("[SpawnMore] No spawn slots cached"));
		return;
	}

	int32 Spawned = 0;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const FPhysicsSpawnSlot& Slot : PhysicsSpawnSlots)
	{
		UClass* ClassToSpawn = Slot.Class ? Slot.Class.Get() : PhysicsObjectClass.Get();
		if (!ClassToSpawn)
		{
			ClassToSpawn = APhysicsObject::StaticClass();
		}

		FTransform SpawnXf = Slot.Transform;
		const float Jitter = ExtraCubeSpawnJitter;
		const FVector Offset(
			FMath::FRandRange(-Jitter, Jitter),
			FMath::FRandRange(-Jitter, Jitter),
			FMath::FRandRange(0.f, Jitter * 0.5f));
		SpawnXf.AddToTranslation(Offset);

		APhysicsObject* NewObj = World->SpawnActor<APhysicsObject>(ClassToSpawn, SpawnXf, Params);
		if (NewObj)
		{
			++Spawned;
		}
	}

	UE_LOG(LogCollapsePoint, Warning, TEXT("[SpawnMore] Spawned %d extra cubes (slots=%d)"), Spawned, PhysicsSpawnSlots.Num());
}

void ACollapsePointCharacter::OnSpawnMorePhysicsObjects()
{
	// Do not collapse the active singularity — R is only for feeding more cubes into it.
	SpawnExtraPhysicsObjects();
}
