// Collapse Point — ISuckable

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuckableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class USuckable : public UInterface
{
	GENERATED_BODY()
};

class GAME_API ISuckable
{
	GENERATED_BODY()

public:
	virtual bool CanBeSucked() const = 0;
	virtual float GetSuckMass() const = 0;
	virtual UPrimitiveComponent* GetSuckPrimitive() const = 0;
	virtual void OnSuckedTick(const FVector& Force) {}
	virtual bool IsBeingSucked() const { return false; }
};
