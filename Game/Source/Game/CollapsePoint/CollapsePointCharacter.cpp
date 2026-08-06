#include "CollapsePoint/CollapsePointCharacter.h"
#include "CollapsePoint/Singularity.h"
#include "CollapsePoint/CollapsePointHUD.h"
#include "CollapsePoint/PhysicsObject.h"
#include "CollapsePoint/EnemyPawn.h"
#include "CollapsePoint/SlideDoor.h"
#include "CollapsePoint/TriggerButton.h"
#include "CollapsePoint/ExplosiveBarrel.h"
#include "CollapsePoint/DualSwitchShield.h"
#include "CollapsePoint/CheckpointVolume.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
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
	EnemyClass = AEnemyPawn::StaticClass();

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
	CurrentHP = MaxHP;
	bIsDead = false;
	CheckpointTransform = GetActorTransform();
	bHasCheckpoint = true;

	ApplyAimViewCamera();
	EnsureCrosshairHUD();
	CachePhysicsSpawnSlots();
	SpawnLightEnemies();
	BuildVerticalSliceLayout();

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

	if (bIsDead)
	{
		RespawnTimer -= DeltaTime;
		if (RespawnTimer <= 0.f)
		{
			RespawnAtCheckpoint();
		}
		return;
	}

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

void ACollapsePointCharacter::SpawnLightEnemies()
{
	if (AutoSpawnEnemyCount <= 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UClass* ClassToSpawn = EnemyClass ? EnemyClass.Get() : AEnemyPawn::StaticClass();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	int32 Spawned = 0;
	const int32 SlotCount = PhysicsSpawnSlots.Num();

	for (int32 i = 0; i < AutoSpawnEnemyCount; ++i)
	{
		FVector Loc = GetActorLocation() + GetActorForwardVector() * 400.f + FVector(0.f, (i - 1) * 180.f, 80.f);
		if (SlotCount > 0)
		{
			const FPhysicsSpawnSlot& Slot = PhysicsSpawnSlots[i % SlotCount];
			Loc = Slot.Transform.GetLocation() + FVector(
				FMath::FRandRange(-120.f, 120.f),
				FMath::FRandRange(-120.f, 120.f),
				80.f);
		}

		AEnemyPawn* Enemy = World->SpawnActor<AEnemyPawn>(ClassToSpawn, Loc, FRotator::ZeroRotator, Params);
		if (Enemy)
		{
			++Spawned;
		}
	}

	UE_LOG(LogCollapsePoint, Warning, TEXT("[Enemy.AutoSpawn] Spawned %d light enemies"), Spawned);
}

void ACollapsePointCharacter::OnSpawnMorePhysicsObjects()
{
	// Do not collapse the active singularity — R is only for feeding more cubes into it.
	SpawnExtraPhysicsObjects();
}

void ACollapsePointCharacter::SetCheckpoint(const FTransform& Transform)
{
	CheckpointTransform = Transform;
	bHasCheckpoint = true;
}

float ACollapsePointCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead)
	{
		return 0.f;
	}

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	CurrentHP = FMath::Max(0.f, CurrentHP - Applied);
	UE_LOG(LogCollapsePoint, Warning,
		TEXT("[Player.Damage] -%.1f -> HP=%.0f Causer=%s"), Applied, CurrentHP, *GetNameSafe(DamageCauser));

	if (CurrentHP <= 0.f)
	{
		DieAndRespawn();
	}
	return Applied;
}

void ACollapsePointCharacter::DieAndRespawn()
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;
	RespawnTimer = RespawnDelay;

	if (ActiveSingularity && !ActiveSingularity->IsCollapsed())
	{
		ActiveSingularity->Collapse(GetAimDirection(), 0.f, TEXT("PlayerDeath"));
		ActiveSingularity = nullptr;
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->DisableMovement();
	}
	SetActorEnableCollision(false);
	UE_LOG(LogCollapsePoint, Error, TEXT("[Player.Die] respawn in %.1fs"), RespawnDelay);
}

void ACollapsePointCharacter::RespawnAtCheckpoint()
{
	const FTransform Xf = bHasCheckpoint ? CheckpointTransform : GetActorTransform();
	SetActorTransform(Xf, false, nullptr, ETeleportType::TeleportPhysics);
	CurrentHP = MaxHP;
	bIsDead = false;
	SetActorEnableCollision(true);
	SetActorHiddenInGame(false);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
		Move->StopMovementImmediately();
	}

	UE_LOG(LogCollapsePoint, Warning,
		TEXT("[Player.Respawn] HP=%.0f Loc=(%.0f,%.0f,%.0f)"),
		CurrentHP, Xf.GetLocation().X, Xf.GetLocation().Y, Xf.GetLocation().Z);
}

void ACollapsePointCharacter::BuildVerticalSliceLayout()
{
	if (!bAutoBuildVerticalSlice)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Skip if a slice door already exists (level authored or previous PIE leftover).
	for (TActorIterator<ASlideDoor> It(World); It; ++It)
	{
		UE_LOG(LogCollapsePoint, Warning, TEXT("[Slice] Existing door found — skip auto layout"));
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector Origin = GetActorLocation();
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

	auto At = [&](float ForwardDist, float RightDist, float Z = 0.f)
	{
		return Origin + Forward * ForwardDist + Right * RightDist + FVector(0.f, 0.f, Z);
	};

	// --- A zone: button + door ---
	ASlideDoor* DoorAB = World->SpawnActor<ASlideDoor>(ASlideDoor::StaticClass(), At(900.f, 0.f, 175.f), Forward.Rotation(), Params);
	ATriggerButton* Button = World->SpawnActor<ATriggerButton>(ATriggerButton::StaticClass(), At(700.f, -220.f, 30.f), FRotator::ZeroRotator, Params);
	if (Button && DoorAB)
	{
		Button->LinkedDoor = DoorAB;
	}

	ACheckpointVolume* CP_A = World->SpawnActor<ACheckpointVolume>(ACheckpointVolume::StaticClass(), At(100.f, 0.f, 0.f), FRotator::ZeroRotator, Params);
	if (CP_A)
	{
		CP_A->SetActorScale3D(FVector(1.f));
	}

	// --- B zone: debris pile + more light enemies past the door ---
	const FVector BCenter = At(1400.f, 0.f, 60.f);
	UClass* CubeClass = PhysicsObjectClass ? PhysicsObjectClass.Get() : APhysicsObject::StaticClass();
	for (int32 i = 0; i < 24; ++i)
	{
		const FVector Jitter(
			FMath::FRandRange(-180.f, 180.f),
			FMath::FRandRange(-180.f, 180.f),
			FMath::FRandRange(0.f, 120.f));
		World->SpawnActor<APhysicsObject>(CubeClass, BCenter + Jitter, FRotator::ZeroRotator, Params);
	}

	UClass* EnemyCls = EnemyClass ? EnemyClass.Get() : AEnemyPawn::StaticClass();
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Loc = At(1500.f, (i - 1.5f) * 160.f, 80.f);
		World->SpawnActor<AEnemyPawn>(EnemyCls, Loc, FRotator::ZeroRotator, Params);
	}

	ACheckpointVolume* CP_B = World->SpawnActor<ACheckpointVolume>(ACheckpointVolume::StaticClass(), At(1200.f, 0.f, 0.f), FRotator::ZeroRotator, Params);

	// --- C zone: heavy enemy, barrels, dual shield ---
	AEnemyPawn* Heavy = World->SpawnActor<AEnemyPawn>(EnemyCls, At(2300.f, 0.f, 90.f), FRotator::ZeroRotator, Params);
	if (Heavy)
	{
		Heavy->ConfigureAsHeavy();
	}

	World->SpawnActor<AExplosiveBarrel>(AExplosiveBarrel::StaticClass(), At(2150.f, -320.f, 80.f), FRotator::ZeroRotator, Params);
	World->SpawnActor<AExplosiveBarrel>(AExplosiveBarrel::StaticClass(), At(2150.f, 320.f, 80.f), FRotator::ZeroRotator, Params);
	World->SpawnActor<AExplosiveBarrel>(AExplosiveBarrel::StaticClass(), At(1950.f, 0.f, 80.f), FRotator::ZeroRotator, Params);

	// Heavier physics cubes as alternate ammo near C.
	for (int32 i = 0; i < 4; ++i)
	{
		APhysicsObject* HeavyCube = World->SpawnActor<APhysicsObject>(
			CubeClass, At(2050.f, (i - 1.5f) * 120.f, 50.f), FRotator::ZeroRotator, Params);
		if (HeavyCube)
		{
			HeavyCube->SetActorScale3D(FVector(0.9f));
		}
	}

	World->SpawnActor<ADualSwitchShield>(
		ADualSwitchShield::StaticClass(),
		At(2700.f, 0.f, 0.f),
		Forward.Rotation(),
		Params);

	ACheckpointVolume* CP_C = World->SpawnActor<ACheckpointVolume>(ACheckpointVolume::StaticClass(), At(1900.f, 0.f, 0.f), FRotator::ZeroRotator, Params);

	UE_LOG(LogCollapsePoint, Error,
		TEXT("[Slice.Build] A(button+door) B(debris+4enemies) C(heavy+barrels+shield) Origin=(%.0f,%.0f,%.0f)"),
		Origin.X, Origin.Y, Origin.Z);

	(void)CP_A;
	(void)CP_B;
	(void)CP_C;
}
