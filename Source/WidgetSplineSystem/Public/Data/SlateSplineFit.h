// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** A uniform mapping from the combined spline bounds into an allotted widget rectangle. */
struct WIDGETSPLINESYSTEM_API FSlateSplineFit
{
	FVector2D SourceMin = FVector2D::ZeroVector;
	FVector2D Offset = FVector2D::ZeroVector;
	double Scale = 0.0;
	double StrokeScale = 0.0;

	FSlateSplineFit() = default;

	static float ClampMappingScale(float Value)
	{
		return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : 1.0f;
	}

	static FVector2D GetDesiredSize(const FBox2D& Bounds, const FVector2D& PreferredSize, double DisplayStrokeRadius = 0.0)
	{
		if (!Bounds.bIsValid)
		{
			return FVector2D::ZeroVector;
		}
		const FVector2D Size = Bounds.GetSize();
		if (PreferredSize.X <= 0.0 && PreferredSize.Y <= 0.0)
		{
			return Size;
		}
		FSlateSplineFit Fit;
		Fit.FitToSize(Size, PreferredSize, DisplayStrokeRadius);
		const FVector2D Padding(2.0 * DisplayStrokeRadius);
		const FVector2D Extent = (Size - Padding).ComponentMax(FVector2D::ZeroVector) * Fit.Scale + Padding * Fit.StrokeScale;
		// A single specified axis also sets the slot size for horizontal or vertical paths.
		return FVector2D(PreferredSize.X > 0.0 && PreferredSize.Y <= 0.0 ? PreferredSize.X : Extent.X,
			PreferredSize.Y > 0.0 && PreferredSize.X <= 0.0 ? PreferredSize.Y : Extent.Y);
	}

	FSlateSplineFit(const FBox2D& Bounds, const FVector2D& AllottedSize, float MappingScale = 1.0f, double DisplayStrokeRadius = 0.0)
	{
		if (!Bounds.bIsValid || AllottedSize.X <= 0.0 || AllottedSize.Y <= 0.0)
		{
			return;
		}
		const FVector2D Size = Bounds.GetSize();
		if (Size.X <= 0.0 || Size.Y <= 0.0)
		{
			return;
		}
		SourceMin = Bounds.Min;
		FitToSize(Size, AllottedSize, DisplayStrokeRadius);
		const double Zoom = ClampMappingScale(MappingScale);
		Scale *= Zoom;
		StrokeScale *= Zoom;
		Offset = (AllottedSize - Size * Scale) * 0.5;
	}

	FVector2D TransformPosition(const FVector2D& Position) const
	{
		return (Position - SourceMin) * Scale + Offset;
	}

private:
	void FitToSize(const FVector2D& Size, const FVector2D& Target, double DisplayStrokeRadius)
	{
		// Imported bounds include one brush radius on either side of the centerlines.
		const double Diameter = 2.0 * DisplayStrokeRadius;
		const FVector2D CenterlineSize = (Size - FVector2D(Diameter)).ComponentMax(FVector2D::ZeroVector);
		StrokeScale = 1.0;
		if (Diameter > 0.0)
		{
			// Only shrink the pen in tiny slots. Leave room for the centerline on axes where
			// it has an extent; a horizontal line can use the full slot height for its stroke.
			const FVector2D PenAndPath = FVector2D(Diameter) + CenterlineSize.ComponentMin(FVector2D(Diameter));
			if (Target.X > 0.0 && Target.X <= Diameter)
			{
				StrokeScale = FMath::Min(StrokeScale, Target.X / PenAndPath.X);
			}
			if (Target.Y > 0.0 && Target.Y <= Diameter)
			{
				StrokeScale = FMath::Min(StrokeScale, Target.Y / PenAndPath.Y);
			}
		}
		const double Pad = Diameter * StrokeScale;
		const bool bFitX = Target.X > 0.0 && CenterlineSize.X > 0.0;
		const bool bFitY = Target.Y > 0.0 && CenterlineSize.Y > 0.0;
		Scale = bFitX ? FMath::Max(0.0, Target.X - Pad) / CenterlineSize.X : 1.0;
		if (bFitY)
		{
			const double ScaleY = FMath::Max(0.0, Target.Y - Pad) / CenterlineSize.Y;
			Scale = bFitX ? FMath::Min(Scale, ScaleY) : ScaleY;
		}
		if (Diameter <= 0.0)
		{
			StrokeScale = Scale;
		}
	}
};
