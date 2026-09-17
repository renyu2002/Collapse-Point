// Collapse Point — shatterable glass / fragile bridge (whitebox)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakablePanel.generated.h"

class UStaticMeshComponent;

UCLASS()
class GAME_API ABreakablePanel : public AActor
{
	GENERATED_BODY()

public:
	ABreakablePanel();

	virtual void BeginPlay() override;

	bool IsShattered() const { return bShattered; }
	bool BlocksAttractionPath(const FVector& Start, const FVector& End) const;
	void ApplyWellPull(float DeltaTime, float DistToWell);
	void Shatter(AActor* Causer);
	void Restore();

protected:
	UFUNCTION()
	void OnPanelHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnPanelOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void TryBreakFromActor(AActor* OtherActor, UPrimitiveComponent* OtherComp, float ExtraScore);

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float ImpactBreakScore = 8000.f;

	/** Seconds of well-pull inside range required to crack. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float WellPullBreakTime = 0.55f;

	/** Optional for special fragile props; puzzle caches require a carried-body hit. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	bool bAllowWellPullBreak = false;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	FLinearColor IntactColor = FLinearColor(0.45f, 0.75f, 0.95f, 0.6f);

	float WellPullAccum = 0.f;
	bool bShattered = false;
	FTransform SpawnTransform;
};
