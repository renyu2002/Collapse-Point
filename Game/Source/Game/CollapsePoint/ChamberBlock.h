// Collapse Point — static whitebox block (walls / floors / alcoves)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChamberBlock.generated.h"

class UStaticMeshComponent;

UCLASS()
class GAME_API AChamberBlock : public AActor
{
	GENERATED_BODY()

public:
	AChamberBlock();

	virtual void BeginPlay() override;

	void SetBlockColor(FLinearColor Color);

protected:
	UPROPERTY(VisibleAnywhere, Category = "CollapsePoint")
	TObjectPtr<UStaticMeshComponent> Mesh;
};
