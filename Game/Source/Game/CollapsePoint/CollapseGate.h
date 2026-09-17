// Collapse Point — mass gate: opens only when a nearby well collapses into a black hole

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CollapseGate.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class ASlideDoor;

/**
 * Reframes the "over-feeding is punishment" rule: the only way to open this gate
 * is to deliberately grow a gravity well past its black-hole mass threshold nearby.
 */
UCLASS()
class GAME_API ACollapseGate : public AActor
{
	GENERATED_BODY()

public:
	ACollapseGate();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	bool IsOpen() const { return bOpen; }
	void ResetGate();

	/** Door driven open when the gate trips. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CollapsePoint")
	TObjectPtr<ASlideDoor> LinkedDoor;

	/** A black-hole-stage well within this radius trips the gate. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float TriggerRadius = 700.f;

	/**
	 * If set, a black-hole well in range is force-detonated (radial burst) instead
	 * of just opening a door — this is the "cascading collapse" trigger for M-style
	 * chambers. Ordinary gates leave this false and simply open LinkedDoor.
	 */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	bool bDetonateWell = false;

	/** For detonators: wait until the well reaches this mass so the burst is full. */
	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float DetonateMassThreshold = 12.f;

protected:
	void OpenGate();
	void UpdateVisual();

	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MeshMID;

	bool bOpen = false;
};
