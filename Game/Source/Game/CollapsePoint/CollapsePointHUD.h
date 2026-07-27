// Collapse Point — simple center crosshair HUD

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CollapsePointHUD.generated.h"

UCLASS()
class GAME_API ACollapsePointHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float CrosshairHalfSize = 8.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float CrosshairThickness = 2.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	float CrosshairGap = 4.f;

	UPROPERTY(EditAnywhere, Category = "CollapsePoint")
	FLinearColor CrosshairColor = FLinearColor(1.f, 1.f, 1.f, 0.85f);
};
