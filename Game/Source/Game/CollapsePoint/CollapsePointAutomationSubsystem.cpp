// Collapse Point — unattended, command-line gameplay playthrough

#include "CollapsePoint/CollapsePointAutomationSubsystem.h"

#include "CollapsePoint/BreakablePanel.h"
#include "CollapsePoint/CheckpointVolume.h"
#include "CollapsePoint/CollapseGate.h"
#include "CollapsePoint/CollapsePointCharacter.h"
#include "CollapsePoint/CollapsePointLog.h"
#include "CollapsePoint/DualSwitchShield.h"
#include "CollapsePoint/PhysicsObject.h"
#include "CollapsePoint/Singularity.h"
#include "CollapsePoint/SlideDoor.h"
#include "CollapsePoint/TestChamber.h"
#include "CollapsePoint/TriggerButton.h"
#include "Components/PrimitiveComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
template <typename T>
T* FindNearestActor(UWorld* World, const FVector& ExpectedLocation, float MaxDistance)
{
	T* Best = nullptr;
	float BestDistanceSq = FMath::Square(MaxDistance);
	for (TActorIterator<T> It(World); It; ++It)
	{
		const float DistanceSq = FVector::DistSquared(It->GetActorLocation(), ExpectedLocation);
		if (DistanceSq <= BestDistanceSq)
		{
			Best = *It;
			BestDistanceSq = DistanceSq;
		}
	}
	return Best;
}

FCollapsePointAutomationStep Step(
	ECollapsePointAutomationAction Action,
	float Duration,
	const FVector& Target = FVector::ZeroVector,
	float Value = 0.f)
{
	FCollapsePointAutomationStep Result;
	Result.Action = Action;
	Result.Duration = Duration;
	Result.Target = Target;
	Result.Value = Value;
	return Result;
}
}

bool UCollapsePointAutomationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return Super::ShouldCreateSubsystem(Outer)
		&& FParse::Param(FCommandLine::Get(), TEXT("CollapsePointAutoTest"));
#endif
}

bool UCollapsePointAutomationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCollapsePointAutomationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bWorldReady = true;
	UE_LOG(LogCollapsePoint, Warning, TEXT("[AutoTest.Begin] unattended thirteen-chamber playthrough"));
}

TStatId UCollapsePointAutomationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCollapsePointAutomationSubsystem, STATGROUP_Tickables);
}

void UCollapsePointAutomationSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !bWorldReady)
	{
		return;
	}

	WorldElapsed += DeltaTime;
	if (!Character.IsValid())
	{
		TryStart();
		if (!Character.IsValid())
		{
			if (WorldElapsed > 8.f)
			{
				FCollapsePointScenarioResult& Failure = Results.AddDefaulted_GetRef();
				Failure.Name = TEXT("Bootstrap");
				Failure.Details = TEXT("No ACollapsePointCharacter was possessed within 8 seconds.");
				WriteReportAndExit();
			}
			return;
		}
	}

	if (WorldElapsed > 155.f)
	{
		FCollapsePointScenarioResult& Failure = Results.AddDefaulted_GetRef();
		Failure.Name = TEXT("Timeout");
		Failure.Details = TEXT("The unattended playthrough exceeded 130 seconds.");
		WriteReportAndExit();
		return;
	}

	if (!bScenarioRunning)
	{
		InterScenarioDelay -= DeltaTime;
		if (InterScenarioDelay <= 0.f)
		{
			const int32 NextScenario = ScenarioIndex + 1;
			if (NextScenario >= 13)
			{
				WriteReportAndExit();
			}
			else
			{
				BeginScenario(NextScenario);
			}
		}
		return;
	}

	if (ACollapsePointCharacter* Player = Character.Get())
	{
		Player->SetActorLocation(CharacterAnchor, false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
		if (ASingularity* Well = Player->GetAutomationSingularity())
		{
			Results.Last().MaxWellX = FMath::Max(Results.Last().MaxWellX, Well->GetActorLocation().X);
			Results.Last().MaxOrbitTilt = FMath::Max(Results.Last().MaxOrbitTilt, Well->GetOrbitTilt());
		}
	}
	if (ScenarioIndex == 6)
	{
		if (const ACheckpointVolume* PulseField =
			FindNearestActor<ACheckpointVolume>(GetWorld(), FVector(7500.f, 0.f, 160.f), 160.f))
		{
			bGSawPulseActive |= PulseField->IsSuppressionActive();
			bGSawPulseInactive |= !PulseField->IsSuppressionActive();
		}
	}

	ScenarioElapsed += DeltaTime;
	StepElapsed += DeltaTime;
	SampleAccumulator += DeltaTime;
	if (SampleAccumulator >= 0.05f)
	{
		SampleAccumulator = 0.f;
		SampleTrajectories();
	}

	bool bStepComplete = Steps.IsValidIndex(StepIndex) && StepElapsed >= Steps[StepIndex].Duration;
	if (Steps.IsValidIndex(StepIndex)
		&& Steps[StepIndex].Action == ECollapsePointAutomationAction::WaitBodiesPastX)
	{
		bStepComplete = true;
		for (const FCollapsePointTrackedBody& Track : Results.Last().Tracks)
		{
			const APhysicsObject* Body = Track.Actor.Get();
			bStepComplete &= Body && Body->GetActorLocation().X >= Steps[StepIndex].Value;
		}
		bStepComplete |= StepElapsed >= Steps[StepIndex].Duration;
	}
	if (Steps.IsValidIndex(StepIndex)
		&& Steps[StepIndex].Action == ECollapsePointAutomationAction::WaitSuppressorOpen)
	{
		const ACheckpointVolume* Field =
			FindNearestActor<ACheckpointVolume>(GetWorld(), Steps[StepIndex].Target, 180.f);
		bStepComplete = (Field && !Field->IsSuppressionActive())
			|| StepElapsed >= Steps[StepIndex].Duration;
	}
	if (Steps.IsValidIndex(StepIndex)
		&& Steps[StepIndex].Action == ECollapsePointAutomationAction::WaitBreakableShattered)
	{
		const ABreakablePanel* Panel =
			FindNearestActor<ABreakablePanel>(GetWorld(), Steps[StepIndex].Target, 180.f);
		bStepComplete = (Panel && Panel->IsShattered())
			|| StepElapsed >= Steps[StepIndex].Duration;
	}
	if (Steps.IsValidIndex(StepIndex)
		&& Steps[StepIndex].Action == ECollapsePointAutomationAction::WaitGateOpen)
	{
		const ACollapseGate* Gate =
			FindNearestActor<ACollapseGate>(GetWorld(), Steps[StepIndex].Target, 320.f);
		bStepComplete = (Gate && Gate->IsOpen())
			|| StepElapsed >= Steps[StepIndex].Duration;
	}
	if (Steps.IsValidIndex(StepIndex)
		&& Steps[StepIndex].Action == ECollapsePointAutomationAction::WaitWellDetonated)
	{
		// The over-fed well collapses itself; it is gone / collapsed once it bursts.
		const ASingularity* Well = Character.IsValid()
			? Character->GetAutomationSingularity() : nullptr;
		bStepComplete = (!Well || Well->IsCollapsed())
			|| StepElapsed >= Steps[StepIndex].Duration;
	}

	if (bStepComplete)
	{
		++StepIndex;
		StepElapsed = 0.f;
		if (Steps.IsValidIndex(StepIndex))
		{
			ExecuteCurrentStep();
		}
		else
		{
			CompleteScenario();
		}
	}
}

void UCollapsePointAutomationSubsystem::TryStart()
{
	if (WorldElapsed < 1.f)
	{
		return;
	}

	Character = FindCharacter();
	if (ACollapsePointCharacter* Player = Character.Get())
	{
		Player->SetActorEnableCollision(false);
		Player->SetActorHiddenInGame(true);
		if (UCharacterMovementComponent* Movement = Player->GetCharacterMovement())
		{
			Movement->DisableMovement();
		}
		InterScenarioDelay = 0.f;
	}
}

void UCollapsePointAutomationSubsystem::BeginScenario(const int32 NewScenarioIndex)
{
	ScenarioIndex = NewScenarioIndex;
	ScenarioElapsed = 0.f;
	StepElapsed = 0.f;
	SampleAccumulator = 0.f;
	bEHeavyCapturedBeforeCacheOpened = false;
	bGSawPulseActive = false;
	bGSawPulseInactive = false;
	Steps.Reset();

	static const TCHAR* Names[] = {
		TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D"),
		TEXT("E"), TEXT("F"), TEXT("G"), TEXT("H"),
		TEXT("I"), TEXT("J"), TEXT("K"), TEXT("L"), TEXT("M")
	};
	FCollapsePointScenarioResult& Result = Results.AddDefaulted_GetRef();
	Result.Name = Names[ScenarioIndex];

	bool bFoundTargetChamber = false;
	for (TActorIterator<ATestChamber> It(GetWorld()); It; ++It)
	{
		It->ResetChamber();
		bFoundTargetChamber |= It->ChamberId == FName(Names[ScenarioIndex]);
	}
	if (!bFoundTargetChamber)
	{
		Result.Details += TEXT("Missing chamber actor. ");
	}

	BuildScenarioSteps(ScenarioIndex);
	bScenarioRunning = true;
	StepIndex = 0;
	ExecuteCurrentStep();
	UE_LOG(LogCollapsePoint, Warning, TEXT("[AutoTest.Scenario] %s"), Names[ScenarioIndex]);
}

void UCollapsePointAutomationSubsystem::BuildScenarioSteps(const int32 Index)
{
	switch (Index)
	{
	case 0:
		CharacterAnchor = FVector(500.f, 400.f, 90.f);
		AddTrack(TEXT("CP_A_Light01"), FVector(260.f, -80.f, 55.f));
		AddTrack(TEXT("CP_A_Light02"), FVector(320.f, 90.f, 55.f));
		Steps = {
			Step(ECollapsePointAutomationAction::Wait, 0.25f),
			Step(ECollapsePointAutomationAction::StartWell, 0.50f, FVector(390.f, 0.f, 80.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.50f, FVector(610.f, -210.f, 70.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.85f, FVector(790.f, -250.f, 65.f)),
			Step(ECollapsePointAutomationAction::ReleaseWell, 0.65f, FVector(1.f, 0.f, 0.f))
		};
		break;

	case 1:
		CharacterAnchor = FVector(1550.f, 450.f, 90.f);
		AddTrack(TEXT("CP_B_Light01"), FVector(1210.f, -90.f, 55.f));
		AddTrack(TEXT("CP_B_Light02"), FVector(1250.f, 100.f, 55.f));
		Steps = {
			Step(ECollapsePointAutomationAction::Wait, 0.20f),
			Step(ECollapsePointAutomationAction::StartWell, 0.45f, FVector(1240.f, 0.f, 90.f)),
			Step(ECollapsePointAutomationAction::ResizeOrbit, 0.05f, FVector::ZeroVector, -3.f),
			Step(ECollapsePointAutomationAction::Wait, 0.55f),
			Step(ECollapsePointAutomationAction::MoveWell, 0.30f, FVector(1400.f, 0.f, 110.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.35f, FVector(1530.f, 0.f, 110.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.35f, FVector(1700.f, 0.f, 110.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.20f, FVector(1870.f, 0.f, 65.f)),
			Step(ECollapsePointAutomationAction::ResizeOrbit, 0.05f, FVector::ZeroVector, 8.f),
			Step(ECollapsePointAutomationAction::MoveWell, 1.60f, FVector(1870.f, 0.f, 65.f)),
			Step(ECollapsePointAutomationAction::ReleaseWell, 0.40f, FVector(1.f, 0.f, 0.f))
		};
		break;

	case 2:
		CharacterAnchor = FVector(2750.f, -300.f, 90.f);
		AddTrack(TEXT("CP_C_Debris01"), FVector(2310.f, -180.f, 55.f));
		AddTrack(TEXT("CP_C_Debris02"), FVector(2360.f, 0.f, 55.f));
		AddTrack(TEXT("CP_C_Debris03"), FVector(2310.f, 180.f, 55.f));
		AddTrack(TEXT("CP_C_HeavyAmmo"), FVector(2790.f, 0.f, 95.f));
		Steps = {
			Step(ECollapsePointAutomationAction::Wait, 0.25f),
			Step(ECollapsePointAutomationAction::StartWell, 0.75f, FVector(2360.f, 0.f, 90.f)),
			Step(ECollapsePointAutomationAction::ReleaseWell, 0.85f, FVector(1.f, 0.f, 0.f)),
			Step(ECollapsePointAutomationAction::StartWell, 0.80f, FVector(2790.f, 0.f, 100.f)),
			Step(ECollapsePointAutomationAction::ReleaseWell, 1.30f, FVector(0.57f, 0.82f, 0.f))
		};
		break;

	case 3:
		CharacterAnchor = FVector(3500.f, 450.f, 90.f);
		AddTrack(TEXT("CP_D_Projectile"), FVector(3460.f, 0.f, 55.f));
		Steps = {
			Step(ECollapsePointAutomationAction::Wait, 0.25f),
			Step(ECollapsePointAutomationAction::StartWell, 0.45f, FVector(3460.f, 0.f, 90.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.35f, FVector(3650.f, 0.f, 90.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.45f, FVector(4180.f, 0.f, 70.f)),
			Step(ECollapsePointAutomationAction::ReleaseWell, 1.65f, FVector(1.f, 0.f, 0.f))
		};
		break;

	case 4:
		CharacterAnchor = FVector(5050.f, -450.f, 90.f);
		AddTrack(TEXT("CP_E_Heavy01"), FVector(4520.f, -250.f, 95.f));
		AddTrack(TEXT("CP_E_Heavy02"), FVector(4720.f, 230.f, 95.f));
		Steps = {
			Step(ECollapsePointAutomationAction::Wait, 0.25f),
			Step(ECollapsePointAutomationAction::StartWell, 0.40f, FVector(4300.f, -250.f, 100.f)),
			Step(ECollapsePointAutomationAction::ReleaseWell, 1.20f, FVector(0.45f, 0.89f, 0.02f)),
			Step(ECollapsePointAutomationAction::WaitBreakableShattered, 15.00f, FVector(4600.f, 230.f, 125.f)),
			Step(ECollapsePointAutomationAction::StartWell, 0.65f, FVector(4680.f, 0.f, 100.f)),
			Step(ECollapsePointAutomationAction::ResizeOrbit, 0.10f, FVector::ZeroVector, -3.f),
			Step(ECollapsePointAutomationAction::Wait, 0.45f),
			Step(ECollapsePointAutomationAction::MoveWell, 0.50f, FVector(4920.f, 0.f, 110.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.60f, FVector(5050.f, 0.f, 110.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.80f, FVector(5200.f, 0.f, 110.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.80f, FVector(5500.f, 0.f, 65.f)),
			Step(ECollapsePointAutomationAction::WaitBodiesPastX, 2.00f, FVector::ZeroVector, 5200.f),
			Step(ECollapsePointAutomationAction::ResizeOrbit, 0.05f, FVector::ZeroVector, 8.f),
			Step(ECollapsePointAutomationAction::MoveWell, 2.00f, FVector(5250.f, 0.f, 65.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.25f, FVector(5250.f, 180.f, 65.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.75f, FVector(5250.f, 180.f, 65.f)),
			Step(ECollapsePointAutomationAction::ReleaseWell, 0.45f, FVector(1.f, 0.f, 0.f))
		};
		break;

	case 5:
		CharacterAnchor = FVector(5850.f, 400.f, 90.f);
		AddTrack(TEXT("CP_F_Cargo"), FVector(5900.f, 0.f, 55.f));
		Steps = {
			Step(ECollapsePointAutomationAction::Wait, 0.25f),
			Step(ECollapsePointAutomationAction::StartWell, 0.65f, FVector(5900.f, 0.f, 220.f)),
			Step(ECollapsePointAutomationAction::ResizeOrbit, 0.05f, FVector::ZeroVector, -2.f),
			Step(ECollapsePointAutomationAction::TiltOrbit, 0.05f, FVector::ZeroVector, 6.f),
			Step(ECollapsePointAutomationAction::Wait, 0.50f),
			Step(ECollapsePointAutomationAction::MoveWell, 0.45f, FVector(6100.f, 0.f, 220.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.65f, FVector(6300.f, 0.f, 220.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.70f, FVector(6500.f, 0.f, 220.f)),
			Step(ECollapsePointAutomationAction::TiltOrbit, 0.05f, FVector::ZeroVector, -6.f),
			Step(ECollapsePointAutomationAction::MoveWell, 1.10f, FVector(6650.f, -240.f, 80.f)),
			Step(ECollapsePointAutomationAction::ReleaseWell, 0.40f, FVector(1.f, 0.f, 0.f))
		};
		break;

	case 6:
		CharacterAnchor = FVector(7150.f, 400.f, 90.f);
		AddTrack(TEXT("CP_G_FragileCargo"), FVector(7150.f, 0.f, 55.f));
		Steps = {
			Step(ECollapsePointAutomationAction::Wait, 0.25f),
			Step(ECollapsePointAutomationAction::StartWell, 0.65f, FVector(7150.f, 0.f, 100.f)),
			Step(ECollapsePointAutomationAction::ResizeOrbit, 0.05f, FVector::ZeroVector, -1.f),
			Step(ECollapsePointAutomationAction::WaitSuppressorOpen, 3.00f, FVector(7500.f, 0.f, 160.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.30f, FVector(7350.f, 0.f, 100.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.45f, FVector(7550.f, 0.f, 100.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.50f, FVector(7750.f, 0.f, 100.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 1.80f, FVector(7900.f, -140.f, 80.f)),
			Step(ECollapsePointAutomationAction::ReleaseWell, 0.15f, FVector(0.f, 0.f, 1.f))
		};
		break;

	case 7:
		CharacterAnchor = FVector(8500.f, 350.f, 90.f);
		AddTrack(TEXT("CP_H_HeavyCargo"), FVector(8500.f, -180.f, 95.f));
		AddTrack(TEXT("CP_H_FragileDecoy"), FVector(8400.f, 450.f, 55.f));
		Steps = {
			Step(ECollapsePointAutomationAction::Wait, 0.25f),
			Step(ECollapsePointAutomationAction::StartWell, 0.70f, FVector(8500.f, -180.f, 220.f)),
			Step(ECollapsePointAutomationAction::ResizeOrbit, 0.05f, FVector::ZeroVector, -2.f),
			Step(ECollapsePointAutomationAction::TiltOrbit, 0.05f, FVector::ZeroVector, 6.f),
			Step(ECollapsePointAutomationAction::Wait, 0.45f),
			Step(ECollapsePointAutomationAction::MoveWell, 0.55f, FVector(8750.f, 0.f, 220.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.65f, FVector(8950.f, 0.f, 220.f)),
			Step(ECollapsePointAutomationAction::MoveWell, 0.70f, FVector(9150.f, 0.f, 220.f)),
			Step(ECollapsePointAutomationAction::TiltOrbit, 0.05f, FVector::ZeroVector, -6.f),
			Step(ECollapsePointAutomationAction::MoveWell, 0.65f, FVector(9250.f, 0.f, 150.f)),
			Step(ECollapsePointAutomationAction::ReleaseWell, 1.50f, FVector(1.f, 0.f, 0.f))
		};
		break;

	case 8:
		// Chamber I — "swallow is the key": over-feed the well into a black hole
		// to trip the mass gate. The rule that punished you now opens the door.
		CharacterAnchor = FVector(10400.f, 500.f, 90.f);
		AddTrack(TEXT("CP_I_Feed01"), FVector(10400.f, 0.f, 55.f));
		Steps = {
			Step(ECollapsePointAutomationAction::Wait, 0.30f),
			Step(ECollapsePointAutomationAction::StartWell, 0.60f, FVector(10400.f, 0.f, 110.f)),
			Step(ECollapsePointAutomationAction::ResizeOrbit, 0.05f, FVector::ZeroVector, 4.f),
			Step(ECollapsePointAutomationAction::WaitGateOpen, 7.00f, FVector(10850.f, 0.f, 120.f)),
			Step(ECollapsePointAutomationAction::Wait, 0.30f),
			Step(ECollapsePointAutomationAction::ReleaseWell, 0.40f, FVector(0.f, 0.f, 1.f))
		};
		break;

	case 9:
		// Chamber J — "the field is your only workbench": an inverted containment
		// cell means the well can only live inside it. Fling the cargo out to the pad.
		CharacterAnchor = FVector(11600.f, 650.f, 90.f);
		AddTrack(TEXT("CP_J_Cargo"), FVector(11600.f, 0.f, 55.f));
		Steps = {
			Step(ECollapsePointAutomationAction::Wait, 0.30f),
			Step(ECollapsePointAutomationAction::StartWell, 0.55f, FVector(11600.f, 0.f, 120.f)),
			// Gentle compression so all three shards are reliably captured into a
			// tight, coherent ring before the throw (no lone stragglers).
			Step(ECollapsePointAutomationAction::ResizeOrbit, 0.05f, FVector::ZeroVector, -2.f),
			Step(ECollapsePointAutomationAction::Wait, 1.10f),
			// Attempt to drag out of the cell — containment holds the well inside.
			Step(ECollapsePointAutomationAction::MoveWell, 0.60f, FVector(12100.f, 0.f, 120.f)),
			Step(ECollapsePointAutomationAction::Wait, 0.45f),
			// Fling the cluster down the corridor, aimed slightly up so it arcs
			// clear of the cell wall and into the corridor-wide receiver.
			Step(ECollapsePointAutomationAction::ReleaseWell, 1.80f, FVector(1.f, 0.f, 0.14f))
		};
		break;

	case 10:
		// Chamber K — capstone: confined inside a containment cell, gather scattered
		// mass into a black hole to trip the gate. Both new verbs at once.
		CharacterAnchor = FVector(12900.f, 750.f, 90.f);
		AddTrack(TEXT("CP_K_Feed01"), FVector(12900.f, 0.f, 55.f));
		Steps = {
			Step(ECollapsePointAutomationAction::Wait, 0.30f),
			Step(ECollapsePointAutomationAction::StartWell, 0.60f, FVector(12900.f, 0.f, 120.f)),
			Step(ECollapsePointAutomationAction::ResizeOrbit, 0.05f, FVector::ZeroVector, 6.f),
			Step(ECollapsePointAutomationAction::Wait, 0.60f),
			// Drag toward exit is blocked by the cell; the well stays put and keeps feeding.
			Step(ECollapsePointAutomationAction::MoveWell, 0.40f, FVector(13400.f, 0.f, 120.f)),
			Step(ECollapsePointAutomationAction::WaitGateOpen, 7.00f, FVector(13300.f, 0.f, 120.f)),
			Step(ECollapsePointAutomationAction::Wait, 0.30f),
			Step(ECollapsePointAutomationAction::ReleaseWell, 0.40f, FVector(0.f, 0.f, 1.f))
		};
		break;

	case 11:
		// Chamber L — "the ring is a slingshot": the crosshair-throw only goes down
		// the corridor and hits nothing. A tangential release lets the ring's own
		// spin sling the cargo sideways into one of the two long side-wall receivers.
		CharacterAnchor = FVector(14200.f, 800.f, 90.f);
		AddTrack(TEXT("CP_L_Cargo"), FVector(14200.f, 0.f, 55.f));
		Steps = {
			Step(ECollapsePointAutomationAction::Wait, 0.30f),
			Step(ECollapsePointAutomationAction::StartWell, 0.55f, FVector(14200.f, 0.f, 120.f)),
			// Tight, fast ring => a strong tangential sling.
			Step(ECollapsePointAutomationAction::ResizeOrbit, 0.05f, FVector::ZeroVector, -3.f),
			Step(ECollapsePointAutomationAction::Wait, 1.05f),
			// Tangential launch — aim is only a fallback; the orbit direction throws it.
			Step(ECollapsePointAutomationAction::ReleaseWellTangential, 2.20f, FVector(1.f, 0.f, 0.f))
		};
		break;

	case 12:
		// Chamber M — "cascading collapse": over-feed a central pile past the burst
		// threshold. The well detonates and ejects mass radially, tripping a whole
		// ring of receivers at once — something no single aimed throw could do.
		CharacterAnchor = FVector(15600.f, 900.f, 90.f);
		AddTrack(TEXT("CP_M_Feed01"), FVector(15600.f, 0.f, 55.f));
		Steps = {
			Step(ECollapsePointAutomationAction::Wait, 0.30f),
			Step(ECollapsePointAutomationAction::StartWell, 0.60f, FVector(15600.f, 0.f, 120.f)),
			// Gather the whole pile so mass climbs past the burst threshold.
			Step(ECollapsePointAutomationAction::ResizeOrbit, 0.05f, FVector::ZeroVector, 5.f),
			// Wait for the well to over-feed and detonate on its own.
			Step(ECollapsePointAutomationAction::WaitWellDetonated, 6.00f),
			// Let the radial burst reach and trip the ring of receivers.
			Step(ECollapsePointAutomationAction::Wait, 1.60f)
		};
		break;

	default:
		break;
	}
}

void UCollapsePointAutomationSubsystem::ExecuteCurrentStep()
{
	ACollapsePointCharacter* Player = Character.Get();
	if (!Player || !Steps.IsValidIndex(StepIndex))
	{
		return;
	}

	const FCollapsePointAutomationStep& Current = Steps[StepIndex];
	switch (Current.Action)
	{
	case ECollapsePointAutomationAction::StartWell:
		if (!Player->BeginAutomationSingularityAt(Current.Target))
		{
			Results.Last().Details += FString::Printf(
				TEXT("Failed to start well at (%.0f,%.0f,%.0f). "),
				Current.Target.X, Current.Target.Y, Current.Target.Z);
		}
		break;
	case ECollapsePointAutomationAction::MoveWell:
		Player->SetAutomationSingularityTarget(Current.Target);
		break;
	case ECollapsePointAutomationAction::ResizeOrbit:
		Player->AdjustAutomationOrbitRadius(Current.Value);
		break;
	case ECollapsePointAutomationAction::TiltOrbit:
		Player->AdjustAutomationOrbitTilt(Current.Value);
		break;
	case ECollapsePointAutomationAction::WaitBodiesPastX:
	case ECollapsePointAutomationAction::WaitSuppressorOpen:
	case ECollapsePointAutomationAction::WaitBreakableShattered:
	case ECollapsePointAutomationAction::WaitGateOpen:
	case ECollapsePointAutomationAction::WaitWellDetonated:
		break;
	case ECollapsePointAutomationAction::ReleaseWell:
		Player->ReleaseAutomationSingularity(Current.Target);
		break;
	case ECollapsePointAutomationAction::ReleaseWellTangential:
		Player->ReleaseAutomationSingularity(Current.Target, /*bTangentialSling=*/true);
		break;
	case ECollapsePointAutomationAction::Wait:
	default:
		break;
	}
}

void UCollapsePointAutomationSubsystem::CompleteScenario()
{
	UWorld* World = GetWorld();
	FCollapsePointScenarioResult& Result = Results.Last();
	Result.Duration = ScenarioElapsed;

	switch (ScenarioIndex)
	{
	case 0:
	{
		const ATriggerButton* Button = FindNearestActor<ATriggerButton>(World, FVector(790.f, -400.f, 25.f), 120.f);
		const ASlideDoor* Door = Button ? Button->LinkedDoor.Get() : nullptr;
		Result.bPassed = Button && Button->IsActivated() && Door && Door->IsOpen();
		Result.Details += FString::Printf(TEXT("button=%s door=%s"),
			Button && Button->IsActivated() ? TEXT("active") : TEXT("inactive"),
			Door && Door->IsOpen() ? TEXT("open") : TEXT("closed"));
		break;
	}
	case 1:
	{
		const ADualSwitchShield* Shield = FindNearestActor<ADualSwitchShield>(World, FVector(2070.f, 0.f, 0.f), 150.f);
		Result.bPassed = Shield && Shield->IsShieldDown();
		Result.Details += FString::Printf(TEXT("left=%s right=%s shield=%s"),
			Shield && Shield->HasLeftActivated() ? TEXT("active") : TEXT("inactive"),
			Shield && Shield->HasRightActivated() ? TEXT("active") : TEXT("inactive"),
			Shield && Shield->IsShieldDown() ? TEXT("down") : TEXT("up"));
		break;
	}
	case 2:
	{
		const ABreakablePanel* Cache = FindNearestActor<ABreakablePanel>(World, FVector(2670.f, 0.f, 125.f), 150.f);
		const ATriggerButton* Button = FindNearestActor<ATriggerButton>(World, FVector(3000.f, 300.f, 25.f), 150.f);
		const ASlideDoor* Door = Button ? Button->LinkedDoor.Get() : nullptr;
		Result.bPassed = Cache && Cache->IsShattered()
			&& Button && Button->IsActivated()
			&& Door && Door->IsOpen();
		Result.Details += FString::Printf(TEXT("cache=%s button=%s door=%s"),
			Cache && Cache->IsShattered() ? TEXT("shattered") : TEXT("intact"),
			Button && Button->IsActivated() ? TEXT("active") : TEXT("inactive"),
			Door && Door->IsOpen() ? TEXT("open") : TEXT("closed"));
		break;
	}
	case 3:
	{
		const ATriggerButton* Button = FindNearestActor<ATriggerButton>(World, FVector(4180.f, 0.f, 25.f), 150.f);
		const ASlideDoor* Door = Button ? Button->LinkedDoor.Get() : nullptr;
		const bool bWellStoppedAtField = Result.MaxWellX < 3750.f;
		Result.bPassed = bWellStoppedAtField
			&& Button && Button->IsActivated()
			&& Door && Door->IsOpen();
		Result.Details += FString::Printf(TEXT("wellMaxX=%.0f fieldBlocked=%s button=%s door=%s"),
			Result.MaxWellX,
			bWellStoppedAtField ? TEXT("true") : TEXT("false"),
			Button && Button->IsActivated() ? TEXT("active") : TEXT("inactive"),
			Door && Door->IsOpen() ? TEXT("open") : TEXT("closed"));
		break;
	}
	case 4:
	{
		const ABreakablePanel* Cache = FindNearestActor<ABreakablePanel>(World, FVector(4600.f, 230.f, 125.f), 150.f);
		const ADualSwitchShield* Shield = FindNearestActor<ADualSwitchShield>(World, FVector(5580.f, 0.f, 0.f), 150.f);
		Result.bPassed = Cache && Cache->IsShattered()
			&& !bEHeavyCapturedBeforeCacheOpened
			&& Shield && Shield->IsShieldDown();
		Result.Details += FString::Printf(TEXT("cache=%s earlyHeavyCapture=%s left=%s right=%s shield=%s"),
			Cache && Cache->IsShattered() ? TEXT("shattered") : TEXT("intact"),
			bEHeavyCapturedBeforeCacheOpened ? TEXT("true") : TEXT("false"),
			Shield && Shield->HasLeftActivated() ? TEXT("active") : TEXT("inactive"),
			Shield && Shield->HasRightActivated() ? TEXT("active") : TEXT("inactive"),
			Shield && Shield->IsShieldDown() ? TEXT("down") : TEXT("up"));
		break;
	}
	case 5:
	{
		const ATriggerButton* Button = FindNearestActor<ATriggerButton>(World, FVector(6650.f, -350.f, 25.f), 160.f);
		const ASlideDoor* Door = Button ? Button->LinkedDoor.Get() : nullptr;
		float CargoMaxX = -TNumericLimits<float>::Max();
		if (!Result.Tracks.IsEmpty())
		{
			for (const FCollapsePointTrajectorySample& Sample : Result.Tracks[0].Samples)
			{
				CargoMaxX = FMath::Max(CargoMaxX, Sample.Position.X);
			}
		}
		Result.bPassed = Result.MaxOrbitTilt >= 85.f
			&& CargoMaxX > 6350.f
			&& Button && Button->IsActivated()
			&& Door && Door->IsOpen();
		Result.Details += FString::Printf(
			TEXT("tilt=%.0f cargoMaxX=%.0f button=%s door=%s"),
			Result.MaxOrbitTilt, CargoMaxX,
			Button && Button->IsActivated() ? TEXT("active") : TEXT("inactive"),
			Door && Door->IsOpen() ? TEXT("open") : TEXT("closed"));
		break;
	}
	case 6:
	{
		const APhysicsObject* Cargo = Result.Tracks.IsEmpty() ? nullptr : Result.Tracks[0].Actor.Get();
		const ATriggerButton* Button = FindNearestActor<ATriggerButton>(World, FVector(7900.f, -250.f, 25.f), 160.f);
		const ASlideDoor* Door = Button ? Button->LinkedDoor.Get() : nullptr;
		Result.bPassed = bGSawPulseActive && bGSawPulseInactive
			&& Cargo && !Cargo->IsBroken()
			&& Button && Button->IsActivated()
			&& Door && Door->IsOpen();
		Result.Details += FString::Printf(
			TEXT("pulseActive=%s pulseOpen=%s fragile=%s button=%s door=%s"),
			bGSawPulseActive ? TEXT("seen") : TEXT("missing"),
			bGSawPulseInactive ? TEXT("seen") : TEXT("missing"),
			Cargo && !Cargo->IsBroken() ? TEXT("intact") : TEXT("broken"),
			Button && Button->IsActivated() ? TEXT("active") : TEXT("inactive"),
			Door && Door->IsOpen() ? TEXT("open") : TEXT("closed"));
		break;
	}
	case 7:
	{
		const APhysicsObject* Fragile = Result.Tracks.Num() > 1 ? Result.Tracks[1].Actor.Get() : nullptr;
		const ATriggerButton* Button = FindNearestActor<ATriggerButton>(World, FVector(9700.f, 0.f, 25.f), 180.f);
		const ASlideDoor* Door = Button ? Button->LinkedDoor.Get() : nullptr;
		float HeavyMaxX = -TNumericLimits<float>::Max();
		if (!Result.Tracks.IsEmpty())
		{
			for (const FCollapsePointTrajectorySample& Sample : Result.Tracks[0].Samples)
			{
				HeavyMaxX = FMath::Max(HeavyMaxX, Sample.Position.X);
			}
		}
		Result.bPassed = Result.MaxOrbitTilt >= 85.f
			&& HeavyMaxX > 9400.f
			&& Fragile && !Fragile->IsBroken()
			&& Button && Button->IsActivated()
			&& Door && Door->IsOpen();
		Result.Details += FString::Printf(
			TEXT("tilt=%.0f heavyMaxX=%.0f decoy=%s button=%s door=%s"),
			Result.MaxOrbitTilt, HeavyMaxX,
			Fragile && !Fragile->IsBroken() ? TEXT("intact") : TEXT("broken"),
			Button && Button->IsActivated() ? TEXT("active") : TEXT("inactive"),
			Door && Door->IsOpen() ? TEXT("open") : TEXT("closed"));
		break;
	}
	case 8:
	{
		const ACollapseGate* Gate = FindNearestActor<ACollapseGate>(World, FVector(10850.f, 0.f, 120.f), 320.f);
		const ASlideDoor* Door = Gate ? Gate->LinkedDoor.Get() : nullptr;
		Result.bPassed = Gate && Gate->IsOpen() && Door && Door->IsOpen();
		Result.Details += FString::Printf(TEXT("gate=%s door=%s"),
			Gate && Gate->IsOpen() ? TEXT("open") : TEXT("shut"),
			Door && Door->IsOpen() ? TEXT("open") : TEXT("closed"));
		break;
	}
	case 9:
	{
		const ATriggerButton* Button = FindNearestActor<ATriggerButton>(World, FVector(12400.f, 0.f, 20.f), 260.f);
		const ASlideDoor* Door = Button ? Button->LinkedDoor.Get() : nullptr;
		const bool bWellStayedInCell = Result.MaxWellX < 11980.f;
		Result.bPassed = bWellStayedInCell
			&& Button && Button->IsActivated()
			&& Door && Door->IsOpen();
		Result.Details += FString::Printf(TEXT("wellMaxX=%.0f contained=%s button=%s door=%s"),
			Result.MaxWellX,
			bWellStayedInCell ? TEXT("true") : TEXT("false"),
			Button && Button->IsActivated() ? TEXT("active") : TEXT("inactive"),
			Door && Door->IsOpen() ? TEXT("open") : TEXT("closed"));
		break;
	}
	case 10:
	{
		const ACollapseGate* Gate = FindNearestActor<ACollapseGate>(World, FVector(13300.f, 0.f, 120.f), 360.f);
		const ASlideDoor* Door = Gate ? Gate->LinkedDoor.Get() : nullptr;
		const bool bWellStayedInCell = Result.MaxWellX < 13320.f;
		Result.bPassed = Gate && Gate->IsOpen()
			&& Door && Door->IsOpen()
			&& bWellStayedInCell;
		Result.Details += FString::Printf(TEXT("gate=%s door=%s wellMaxX=%.0f contained=%s"),
			Gate && Gate->IsOpen() ? TEXT("open") : TEXT("shut"),
			Door && Door->IsOpen() ? TEXT("open") : TEXT("closed"),
			Result.MaxWellX,
			bWellStayedInCell ? TEXT("true") : TEXT("false"));
		break;
	}
	case 11:
	{
		// Slingshot: the cargo must reach a side-wall receiver (only a tangential
		// launch throws it laterally; the aimed throw would sail down the corridor).
		const ATriggerButton* Left = FindNearestActor<ATriggerButton>(World, FVector(14200.f, 560.f, 20.f), 340.f);
		const ATriggerButton* Right = FindNearestActor<ATriggerButton>(World, FVector(14200.f, -560.f, 20.f), 340.f);
		const bool bLeft = Left && Left->IsActivated();
		const bool bRight = Right && Right->IsActivated();
		const ASlideDoor* Door = nullptr;
		if (bLeft) { Door = Left->LinkedDoor.Get(); }
		else if (bRight) { Door = Right->LinkedDoor.Get(); }
		Result.bPassed = (bLeft || bRight) && Door && Door->IsOpen();
		Result.Details += FString::Printf(TEXT("left=%s right=%s door=%s"),
			bLeft ? TEXT("active") : TEXT("inactive"),
			bRight ? TEXT("active") : TEXT("inactive"),
			Door && Door->IsOpen() ? TEXT("open") : TEXT("closed"));
		break;
	}
	case 12:
	{
		// Cascading collapse: one over-feed detonation must trip a ring of receivers
		// at once. Require >=3 of the surrounding receivers active — unreachable by
		// any single aimed throw, only by the radial burst.
		int32 RingActive = 0;
		const ASlideDoor* Door = nullptr;
		for (TActorIterator<ATriggerButton> It(World); It; ++It)
		{
			ATriggerButton* Btn = *It;
			if (Btn
				&& FVector::Dist2D(Btn->GetActorLocation(), FVector(15600.f, 0.f, 20.f)) <= 720.f
				&& Btn->IsActivated())
			{
				++RingActive;
				if (!Door && Btn->LinkedDoor.Get())
				{
					Door = Btn->LinkedDoor.Get();
				}
			}
		}
		Result.bPassed = RingActive >= 3 && Door && Door->IsOpen();
		Result.Details += FString::Printf(TEXT("ringActive=%d door=%s"),
			RingActive, Door && Door->IsOpen() ? TEXT("open") : TEXT("closed"));
		break;
	}
	default:
		break;
	}

	UE_LOG(LogCollapsePoint, Warning, TEXT("[AutoTest.Result] %s %s — %s"),
		*Result.Name, Result.bPassed ? TEXT("PASS") : TEXT("FAIL"), *Result.Details);
	bScenarioRunning = false;
	InterScenarioDelay = 0.5f;
}

void UCollapsePointAutomationSubsystem::SampleTrajectories()
{
	if (Results.IsEmpty())
	{
		return;
	}

	for (FCollapsePointTrackedBody& Track : Results.Last().Tracks)
	{
		APhysicsObject* Body = Track.Actor.Get();
		if (!Body || Track.Samples.Num() >= 240)
		{
			continue;
		}

		FCollapsePointTrajectorySample Sample;
		Sample.Time = ScenarioElapsed;
		Sample.Position = Body->GetActorLocation();
		Sample.bBeingSucked = Body->IsBeingSucked();
		if (ScenarioIndex == 4
			&& Track.Label == TEXT("CP_E_Heavy02")
			&& Sample.bBeingSucked)
		{
			const ABreakablePanel* Cache = FindNearestActor<ABreakablePanel>(
				GetWorld(), FVector(4600.f, 230.f, 125.f), 150.f);
			bEHeavyCapturedBeforeCacheOpened |= Cache && !Cache->IsShattered();
		}
		if (const UPrimitiveComponent* Primitive = Body->GetSuckPrimitive())
		{
			Sample.Velocity = Primitive->GetPhysicsLinearVelocity();
		}

		if (!Track.Samples.IsEmpty())
		{
			Track.PathLength += FVector::Distance(Track.Samples.Last().Position, Sample.Position);
		}
		Track.MaxSpeed = FMath::Max(Track.MaxSpeed, Sample.Velocity.Size());
		Track.Samples.Add(Sample);
	}
}

void UCollapsePointAutomationSubsystem::WriteReportAndExit()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	bool bAllPassed = !Results.IsEmpty();
	TArray<TSharedPtr<FJsonValue>> ScenarioValues;
	for (const FCollapsePointScenarioResult& Result : Results)
	{
		bAllPassed &= Result.bPassed;
		TSharedRef<FJsonObject> ScenarioJson = MakeShared<FJsonObject>();
		ScenarioJson->SetStringField(TEXT("name"), Result.Name);
		ScenarioJson->SetBoolField(TEXT("passed"), Result.bPassed);
		ScenarioJson->SetStringField(TEXT("details"), Result.Details);
		ScenarioJson->SetNumberField(TEXT("durationSeconds"), Result.Duration);
		ScenarioJson->SetNumberField(TEXT("maxWellX"), Result.MaxWellX);
		ScenarioJson->SetNumberField(TEXT("maxOrbitTiltDegrees"), Result.MaxOrbitTilt);

		TArray<TSharedPtr<FJsonValue>> TrackValues;
		for (const FCollapsePointTrackedBody& Track : Result.Tracks)
		{
			TSharedRef<FJsonObject> TrackJson = MakeShared<FJsonObject>();
			TrackJson->SetStringField(TEXT("label"), Track.Label);
			TrackJson->SetNumberField(TEXT("pathLengthCm"), Track.PathLength);
			TrackJson->SetNumberField(TEXT("maxSpeedCmPerSecond"), Track.MaxSpeed);

			TArray<TSharedPtr<FJsonValue>> SampleValues;
			for (const FCollapsePointTrajectorySample& Sample : Track.Samples)
			{
				TSharedRef<FJsonObject> SampleJson = MakeShared<FJsonObject>();
				SampleJson->SetNumberField(TEXT("time"), Sample.Time);
				SampleJson->SetBoolField(TEXT("beingSucked"), Sample.bBeingSucked);
				SampleJson->SetStringField(TEXT("position"), Sample.Position.ToCompactString());
				SampleJson->SetStringField(TEXT("velocity"), Sample.Velocity.ToCompactString());
				SampleValues.Add(MakeShared<FJsonValueObject>(SampleJson));
			}
			TrackJson->SetArrayField(TEXT("samples"), SampleValues);
			TrackValues.Add(MakeShared<FJsonValueObject>(TrackJson));
		}
		ScenarioJson->SetArrayField(TEXT("tracks"), TrackValues);
		ScenarioValues.Add(MakeShared<FJsonValueObject>(ScenarioJson));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("allPassed"), bAllPassed);
	Root->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetMapName() : TEXT("None"));
	Root->SetNumberField(TEXT("elapsedSeconds"), WorldElapsed);
	Root->SetArrayField(TEXT("scenarios"), ScenarioValues);

	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	FJsonSerializer::Serialize(Root, Writer);

	FString ReportPath;
	if (!FParse::Value(FCommandLine::Get(), TEXT("CollapsePointAutoReport="), ReportPath))
	{
		ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CollapsePointPlaythrough.json"));
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	const bool bSaved = FFileHelper::SaveStringToFile(JsonText, *ReportPath);
	UE_LOG(LogCollapsePoint, Warning, TEXT("[AutoTest.End] allPassed=%s report=%s saved=%s"),
		bAllPassed ? TEXT("true") : TEXT("false"), *ReportPath, bSaved ? TEXT("true") : TEXT("false"));

	FPlatformMisc::RequestExitWithStatus(true, bAllPassed && bSaved ? 0 : 2, TEXT("CollapsePointAutoTest"));
}

ACollapsePointCharacter* UCollapsePointAutomationSubsystem::FindCharacter() const
{
	for (TActorIterator<ACollapsePointCharacter> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

APhysicsObject* UCollapsePointAutomationSubsystem::FindPhysicsObjectNear(
	const FVector& Location,
	const float MaxDistance) const
{
	return FindNearestActor<APhysicsObject>(GetWorld(), Location, MaxDistance);
}

void UCollapsePointAutomationSubsystem::AddTrack(const TCHAR* Label, const FVector& ExpectedLocation)
{
	FCollapsePointTrackedBody& Track = Results.Last().Tracks.AddDefaulted_GetRef();
	Track.Label = Label;
	Track.Actor = FindPhysicsObjectNear(ExpectedLocation);
	if (!Track.Actor.IsValid())
	{
		Results.Last().Details += FString::Printf(TEXT("Missing tracked body %s. "), Label);
	}
}
