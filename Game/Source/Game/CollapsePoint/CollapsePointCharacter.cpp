#include "CollapsePoint/CollapsePointCharacter.h"
#include "CollapsePoint/Singularity.h"
#include "CollapsePoint/CollapsePointHUD.h"
#include "CollapsePoint/PhysicsObject.h"
#include "CollapsePoint/EnemyPawn.h"
#include "CollapsePoint/SlideDoor.h"
#include "CollapsePoint/TriggerButton.h"
#include "CollapsePoint/DualSwitchShield.h"
#include "CollapsePoint/CheckpointVolume.h"
#include "CollapsePoint/BreakablePanel.h"
#include "CollapsePoint/HangingProp.h"
#include "CollapsePoint/ChamberBlock.h"
#include "CollapsePoint/TestChamber.h"
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
#include "Components/BoxComponent.h"
#include "InputCoreTypes.h"
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
	PlayerInputComponent->BindAxisKey(EKeys::MouseWheelAxis, this, &ACollapsePointCharacter::OnOrbitRadiusInput);

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
			EIC->BindAction(ResetAction, ETriggerEvent::Started, this, &ACollapsePointCharacter::OnResetPressed);
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
		const FVector Target = GetAimWorldLocation();
		if (!IsSingularityPathSuppressed(ActiveSingularity->GetActorLocation(), Target))
		{
			ActiveSingularity->TickDragToward(Target, DeltaTime);
		}
	}
	else if (ActiveSingularity && ActiveSingularity->IsCollapsed())
	{
		ActiveSingularity = nullptr;
		bAutomationAimOverride = false;
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
	if (bAutomationAimOverride)
	{
		return AutomationAimWorldTarget;
	}
	return ComputeSingularityLocation();
}

bool ACollapsePointCharacter::CanSpawnSingularity() const
{
	return CooldownRemaining <= 0.f
		&& SingularityClass
		&& (ActiveSingularity == nullptr || ActiveSingularity->IsCollapsed());
}

bool ACollapsePointCharacter::IsSingularityPathSuppressed(const FVector& Start, const FVector& End) const
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

void ACollapsePointCharacter::OnSingularityStarted()
{
	if (!CanSpawnSingularity() || !GetWorld())
	{
		return;
	}

	const FVector SpawnLoc = GetAimWorldLocation();
	if (IsSingularityPathSuppressed(GetActorLocation(), SpawnLoc))
	{
		UE_LOG(LogCollapsePoint, Warning, TEXT("[Singularity.Blocked] Spawn path crosses suppression field"));
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveSingularity = GetWorld()->SpawnActor<ASingularity>(SingularityClass, SpawnLoc, FRotator::ZeroRotator, Params);
	FlickAccumulator = 0.f;
	FlickTimer = 0.f;
}

void ACollapsePointCharacter::OnOrbitRadiusInput(float Value)
{
	if (ActiveSingularity && !ActiveSingularity->IsCollapsed() && !FMath::IsNearlyZero(Value))
	{
		const APlayerController* PC = Cast<APlayerController>(GetController());
		const bool bTiltModifier = PC
			&& (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
		if (bTiltModifier)
		{
			ActiveSingularity->AdjustOrbitTilt(Value);
		}
		else
		{
			ActiveSingularity->AdjustOrbitRadius(Value);
		}
	}
}

void ACollapsePointCharacter::OnSingularityReleased()
{
	if (!ActiveSingularity || ActiveSingularity->IsCollapsed())
	{
		ActiveSingularity = nullptr;
		bAutomationAimOverride = false;
		return;
	}

	const FVector AimDir = GetAimDirection();
	const float Boost = ConsumeFlickBoost();
	ActiveSingularity->Collapse(AimDir, Boost);
	ActiveSingularity = nullptr;
	bAutomationAimOverride = false;
	CooldownRemaining = SingularityCooldown;
}

bool ACollapsePointCharacter::BeginAutomationSingularityAt(const FVector& WorldTarget)
{
	bAutomationAimOverride = true;
	AutomationAimWorldTarget = WorldTarget;
	OnSingularityStarted();
	if (!ActiveSingularity)
	{
		bAutomationAimOverride = false;
		return false;
	}
	return true;
}

void ACollapsePointCharacter::SetAutomationSingularityTarget(const FVector& WorldTarget)
{
	if (ActiveSingularity && !ActiveSingularity->IsCollapsed())
	{
		bAutomationAimOverride = true;
		AutomationAimWorldTarget = WorldTarget;
	}
}

void ACollapsePointCharacter::AdjustAutomationOrbitRadius(float InputSteps)
{
	OnOrbitRadiusInput(InputSteps);
}

void ACollapsePointCharacter::AdjustAutomationOrbitTilt(float InputSteps)
{
	if (ActiveSingularity && !ActiveSingularity->IsCollapsed())
	{
		ActiveSingularity->AdjustOrbitTilt(InputSteps);
	}
}

void ACollapsePointCharacter::ReleaseAutomationSingularity(const FVector& AimDirection, bool bTangentialSling)
{
	if (!ActiveSingularity || ActiveSingularity->IsCollapsed())
	{
		ActiveSingularity = nullptr;
		bAutomationAimOverride = false;
		return;
	}

	const ECollapseLaunch Mode = bTangentialSling
		? ECollapseLaunch::TangentialSling
		: ECollapseLaunch::AimThrow;
	ActiveSingularity->Collapse(AimDirection.GetSafeNormal(), 0.f, TEXT("Automation"), Mode);
	ActiveSingularity = nullptr;
	bAutomationAimOverride = false;
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

void ACollapsePointCharacter::SetCurrentChamber(ATestChamber* Chamber)
{
	if (Chamber && Chamber != CurrentChamber)
	{
		CurrentChamber = Chamber;
		UE_LOG(LogCollapsePoint, Warning, TEXT("[Chamber.Enter] %s"), *Chamber->ChamberId.ToString());
	}
}

void ACollapsePointCharacter::OnWellTimeout()
{
	ResetCurrentChamber();
}

void ACollapsePointCharacter::ResetCurrentChamber()
{
	if (CurrentChamber)
	{
		CurrentChamber->ResetChamber();
	}
}

void ACollapsePointCharacter::OnResetPressed()
{
	ResetCurrentChamber();
}

AChamberBlock* ACollapsePointCharacter::SpawnBlock(UWorld* World, const FVector& Loc, const FRotator& Rot, const FVector& Scale, FActorSpawnParameters& Params, FLinearColor Color)
{
	AChamberBlock* Block = World->SpawnActor<AChamberBlock>(AChamberBlock::StaticClass(), Loc, Rot, Params);
	if (Block)
	{
		Block->SetActorScale3D(Scale);
		Block->SetBlockColor(Color);
	}
	return Block;
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

	ResetCurrentChamber();
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

	for (TActorIterator<ATestChamber> It(World); It; ++It)
	{
		UE_LOG(LogCollapsePoint, Warning, TEXT("[Slice] Existing chamber found — skip auto layout"));
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector Origin = GetActorLocation();
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		return;
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const FRotator Face = Forward.Rotation();
	const FLinearColor Wall(0.78f, 0.78f, 0.74f);

	auto At = [&](float F, float R, float Z)
	{
		return Origin + Forward * F + Right * R + FVector(0.f, 0.f, Z);
	};

	auto WallAt = [&](float F, float R, float Z, const FVector& Scale)
	{
		return SpawnBlock(World, At(F, R, Z), Face, Scale, Params, Wall);
	};

	UClass* CubeClass = PhysicsObjectClass ? PhysicsObjectClass.Get() : APhysicsObject::StaticClass();

	// ----- Chamber A: gravity well + scrape + warmup glass -----
	ATestChamber* ChamberA = World->SpawnActor<ATestChamber>(ATestChamber::StaticClass(), At(450.f, 0.f, 120.f), Face, Params);
	if (ChamberA)
	{
		ChamberA->ChamberId = TEXT("A");
		ChamberA->SetActorScale3D(FVector(1.f));
		if (UBoxComponent* Box = Cast<UBoxComponent>(ChamberA->GetRootComponent()))
		{
			Box->SetBoxExtent(FVector(480.f, 520.f, 240.f));
		}
	}

	WallAt(450.f, -520.f, 150.f, FVector(9.0f, 0.25f, 3.2f));
	WallAt(450.f, 520.f, 150.f, FVector(9.0f, 0.25f, 3.2f));
	// Alcove that hides the button from walking
	WallAt(720.f, -280.f, 80.f, FVector(1.2f, 2.2f, 1.6f));

	APhysicsObject* CubeA = World->SpawnActor<APhysicsObject>(CubeClass, At(280.f, 40.f, 50.f), FRotator::ZeroRotator, Params);
	APhysicsObject* CubeASpare = World->SpawnActor<APhysicsObject>(CubeClass, At(220.f, -80.f, 50.f), FRotator::ZeroRotator, Params);

	ABreakablePanel* GlassWarmup = World->SpawnActor<ABreakablePanel>(ABreakablePanel::StaticClass(), At(420.f, 250.f, 140.f), Face, Params);
	if (GlassWarmup)
	{
		GlassWarmup->SetActorScale3D(FVector(0.08f, 2.0f, 1.8f));
	}

	AHangingProp* LampA = World->SpawnActor<AHangingProp>(AHangingProp::StaticClass(), At(500.f, 80.f, 280.f), FRotator::ZeroRotator, Params);

	ASlideDoor* DoorAB = World->SpawnActor<ASlideDoor>(ASlideDoor::StaticClass(), At(900.f, 0.f, 175.f), Face, Params);
	ATriggerButton* ButtonA = World->SpawnActor<ATriggerButton>(ATriggerButton::StaticClass(), At(760.f, -380.f, 40.f), FRotator::ZeroRotator, Params);
	if (ButtonA && DoorAB)
	{
		ButtonA->LinkedDoor = DoorAB;
	}

	if (ChamberA)
	{
		ChamberA->RegisterProp(CubeA);
		ChamberA->RegisterProp(CubeASpare);
		ChamberA->RegisterProp(GlassWarmup);
		ChamberA->RegisterProp(LampA);
	}
	World->SpawnActor<ACheckpointVolume>(ACheckpointVolume::StaticClass(), At(80.f, 0.f, 0.f), Face, Params);

	// ----- Chamber B: break glass to free cube, drag across gap -----
	ATestChamber* ChamberB = World->SpawnActor<ATestChamber>(ATestChamber::StaticClass(), At(1400.f, 0.f, 120.f), Face, Params);
	if (ChamberB)
	{
		ChamberB->ChamberId = TEXT("B");
		if (UBoxComponent* Box = Cast<UBoxComponent>(ChamberB->GetRootComponent()))
		{
			Box->SetBoxExtent(FVector(520.f, 520.f, 240.f));
		}
	}

	WallAt(1400.f, -520.f, 150.f, FVector(10.f, 0.25f, 3.2f));
	WallAt(1400.f, 520.f, 150.f, FVector(10.f, 0.25f, 3.2f));
	// Gap: no floor 1280-1520. Ledges.
	WallAt(1100.f, 0.f, -10.f, FVector(4.5f, 8.0f, 0.2f));
	WallAt(1680.f, 0.f, -10.f, FVector(4.5f, 8.0f, 0.2f));

	APhysicsObject* CubeB = World->SpawnActor<APhysicsObject>(CubeClass, At(1080.f, 0.f, 55.f), FRotator::ZeroRotator, Params);
	ABreakablePanel* CageL = World->SpawnActor<ABreakablePanel>(ABreakablePanel::StaticClass(), At(1080.f, -70.f, 110.f), Face, Params);
	ABreakablePanel* CageR = World->SpawnActor<ABreakablePanel>(ABreakablePanel::StaticClass(), At(1080.f, 70.f, 110.f), Face, Params);
	ABreakablePanel* CageF = World->SpawnActor<ABreakablePanel>(ABreakablePanel::StaticClass(), At(1140.f, 0.f, 110.f), Face, Params);
	if (CageF)
	{
		CageF->SetActorRotation((Forward.Rotation() + FRotator(0.f, 90.f, 0.f)));
		CageF->SetActorScale3D(FVector(0.08f, 1.6f, 1.8f));
	}
	if (CageL) { CageL->SetActorScale3D(FVector(0.08f, 1.4f, 1.8f)); }
	if (CageR) { CageR->SetActorScale3D(FVector(0.08f, 1.4f, 1.8f)); }

	ATriggerButton* ButtonB = World->SpawnActor<ATriggerButton>(ATriggerButton::StaticClass(), At(1680.f, 180.f, 40.f), FRotator::ZeroRotator, Params);
	ASlideDoor* DoorBC = World->SpawnActor<ASlideDoor>(ASlideDoor::StaticClass(), At(1850.f, 0.f, 175.f), Face, Params);
	if (ButtonB && DoorBC)
	{
		ButtonB->LinkedDoor = DoorBC;
	}

	if (ChamberB)
	{
		ChamberB->RegisterProp(CubeB);
		ChamberB->RegisterProp(CageL);
		ChamberB->RegisterProp(CageR);
		ChamberB->RegisterProp(CageF);
	}
	World->SpawnActor<ACheckpointVolume>(ACheckpointVolume::StaticClass(), At(980.f, 0.f, 0.f), Face, Params);

	// ----- Chamber C: break barrier, one well two heavies, dual switch -----
	ATestChamber* ChamberC = World->SpawnActor<ATestChamber>(ATestChamber::StaticClass(), At(2350.f, 0.f, 120.f), Face, Params);
	if (ChamberC)
	{
		ChamberC->ChamberId = TEXT("C");
		if (UBoxComponent* Box = Cast<UBoxComponent>(ChamberC->GetRootComponent()))
		{
			Box->SetBoxExtent(FVector(520.f, 620.f, 260.f));
		}
	}

	WallAt(2350.f, -620.f, 150.f, FVector(10.f, 0.25f, 3.2f));
	WallAt(2350.f, 620.f, 150.f, FVector(10.f, 0.25f, 3.2f));

	APhysicsObject* HeavyL = World->SpawnActor<APhysicsObject>(CubeClass, At(2100.f, -280.f, 55.f), FRotator::ZeroRotator, Params);
	if (HeavyL)
	{
		HeavyL->ConfigureAsHeavyAmmo();
	}
	APhysicsObject* HeavyR = World->SpawnActor<APhysicsObject>(CubeClass, At(2100.f, 280.f, 55.f), FRotator::ZeroRotator, Params);
	if (HeavyR)
	{
		HeavyR->ConfigureAsHeavyAmmo();
	}

	ABreakablePanel* BarrierR = World->SpawnActor<ABreakablePanel>(ABreakablePanel::StaticClass(), At(2100.f, 180.f, 120.f), Face, Params);
	if (BarrierR)
	{
		BarrierR->SetActorScale3D(FVector(0.12f, 2.6f, 2.4f));
	}

	ADualSwitchShield* Shield = World->SpawnActor<ADualSwitchShield>(ADualSwitchShield::StaticClass(), At(2650.f, 0.f, 0.f), Face, Params);

	if (ChamberC)
	{
		ChamberC->RegisterProp(HeavyL);
		ChamberC->RegisterProp(HeavyR);
		ChamberC->RegisterProp(BarrierR);
	}
	World->SpawnActor<ACheckpointVolume>(ACheckpointVolume::StaticClass(), At(1920.f, 0.f, 0.f), Face, Params);

	CurrentChamber = ChamberA;

	UE_LOG(LogCollapsePoint, Error,
		TEXT("[Slice.Build] Chambers A(well scrape) B(break+gap) C(break+dual) Origin=(%.0f,%.0f,%.0f)"),
		Origin.X, Origin.Y, Origin.Z);

	(void)Shield;
}
