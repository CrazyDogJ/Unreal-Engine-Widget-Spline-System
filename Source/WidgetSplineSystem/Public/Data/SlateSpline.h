// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#pragma once

#include "SlateSplinePoint.h"
#include "Styling/SlateBrush.h"
#include "SlateSpline.generated.h"

USTRUCT(BlueprintType)
struct WIDGETSPLINESYSTEM_API FSlateSpline
{
	GENERATED_BODY()

	FSlateSpline()
	{
		Brush.SetImageSize(FVector2D::UnitVector);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spline Widget")
	bool bIsLinear = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spline Widget")
	bool bIsClosedLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spline Widget")
	TArray<FSlateSplinePoint> Points = {
		FSlateSplinePoint(FVector2D(0.0f, 0.0f), FVector2D(200.0f, 0.0f)),
		FSlateSplinePoint(FVector2D(200.0f, 200.0f), FVector2D(200.0f, 0.0f))
	};

	/** ImageSize.X is the local stroke width. ImageSize.Y is the number of texture repeats per 100 local units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spline Widget")
	FSlateBrush Brush = FSlateBrush();
};
