// Collapse Point — unattended, command-line gameplay playthrough

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CollapsePointAutomationSubsystem.generated.h"

class ACollapsePointCharacter;
class APhysicsObject;

enum class ECollapsePointAutomationAction : uint8
{
	Wait,
	StartWell,
	MoveWell,
	ResizeOrbit,
	TiltOrbit,
	WaitBodiesPastX,
	WaitSuppressorOpen,
	WaitBreakableShattered,
	WaitGateOpen,
	WaitWellDetonated,
	ReleaseWell,
	ReleaseWellTangential
};

struct FCollapsePointAutomationStep
{
	ECollapsePointAutomationAction Action = ECollapsePointAutomationAction::Wait;
	float Duration = 0.f;
	FVector Target = FVector::ZeroVector;
	float Value = 0.f;
};

struct FCollapsePointTrajectorySample
{
	float Time = 0.f;
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	bool bBeingSucked = false;
};

struct FCollapsePointTrackedBody
{
	FString Label;
	TWeakObjectPtr<APhysicsObject> Actor;
	TArray<FCollapsePointTrajectorySample> Samples;
	float PathLength = 0.f;
	float MaxSpeed = 0.f;
};

struct FCollapsePointScenarioResult
{
	FString Name;
	bool bPassed = false;
	FString Details;
	float Duration = 0.f;
	float MaxWellX = -TNumericLimits<float>::Max();
	float MaxOrbitTilt = 0.f;
	TArray<FCollapsePointTrackedBody> Tracks;
};

/**
 * Enabled only by -CollapsePointAutoTest. Runs the eight chamber scenarios in a
 * headless game world, writes a JSON trajectory report, then exits the process.
 */
UCLASS()
class GAME_API UCollapsePointAutomationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	void TryStart();
	void BeginScenario(int32 NewScenarioIndex);
	void BuildScenarioSteps(int32 Index);
	void ExecuteCurrentStep();
	void CompleteScenario();
	void SampleTrajectories();
	void WriteReportAndExit();

	ACollapsePointCharacter* FindCharacter() const;
	APhysicsObject* FindPhysicsObjectNear(const FVector& Location, float MaxDistance = 180.f) const;
	void AddTrack(const TCHAR* Label, const FVector& ExpectedLocation);

	TWeakObjectPtr<ACollapsePointCharacter> Character;
	FVector CharacterAnchor = FVector::ZeroVector;
	TArray<FCollapsePointAutomationStep> Steps;
	TArray<FCollapsePointScenarioResult> Results;

	int32 ScenarioIndex = INDEX_NONE;
	int32 StepIndex = INDEX_NONE;
	float WorldElapsed = 0.f;
	float ScenarioElapsed = 0.f;
	float StepElapsed = 0.f;
	float SampleAccumulator = 0.f;
	float InterScenarioDelay = 0.f;
	bool bWorldReady = false;
	bool bScenarioRunning = false;
	bool bEHeavyCapturedBeforeCacheOpened = false;
	bool bGSawPulseActive = false;
	bool bGSawPulseInactive = false;
	bool bFinished = false;
};
