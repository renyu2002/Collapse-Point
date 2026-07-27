#include "CollapsePoint/CollapsePointHUD.h"
#include "Engine/Canvas.h"

void ACollapsePointHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float Half = CrosshairHalfSize;
	const float Gap = CrosshairGap;
	const float T = CrosshairThickness;
	const FLinearColor Color = CrosshairColor;

	DrawRect(Color, CX - Half - Gap, CY - T * 0.5f, Half, T);
	DrawRect(Color, CX + Gap, CY - T * 0.5f, Half, T);
	DrawRect(Color, CX - T * 0.5f, CY - Half - Gap, T, Half);
	DrawRect(Color, CX - T * 0.5f, CY + Gap, T, Half);
	DrawRect(Color, CX - 1.f, CY - 1.f, 2.f, 2.f);
}
