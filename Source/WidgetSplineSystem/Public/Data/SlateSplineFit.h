// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#pragma once

#include "Data/SlateSplineCurves.h"

struct WIDGETSPLINESYSTEM_API FSlateSplineBounds
{
	FBox2D Centerlines = FBox2D(ForceInit);
	FBox2D Painted = FBox2D(ForceInit);
	double MaxRadius = 0.0;

	void Add(const FInterpCurveVector2D& Position, const FSlateBrush& Brush)
	{
		const FBox2D Path = FSlateSplineCurves::GetPositionBounds(Position);
		if (Path.bIsValid)
		{
			const double Radius = FSlateSplineCurves::GetStrokeRadius(Brush);
			Centerlines += Path;
			Painted += Path.ExpandBy(Radius);
			MaxRadius = FMath::Max(MaxRadius, Radius);
		}
	}

	FVector2D GetDesiredSize() const
	{
		return Centerlines.bIsValid ? Centerlines.GetSize() + FVector2D(2.0 * MaxRadius) : FVector2D::ZeroVector;
	}
};

/** Fits all centerlines uniformly. Brush dimensions stay in widget-local units. */
struct WIDGETSPLINESYSTEM_API FSlateSplineFit
{
	FVector2D SourceMin = FVector2D::ZeroVector;
	FVector2D Offset = FVector2D::ZeroVector;
	double Scale = 0.0;

	FSlateSplineFit() = default;
	FSlateSplineFit(const FSlateSplineBounds& Bounds, const FVector2D& AllottedSize)
	{
		if (!Bounds.Centerlines.bIsValid || AllottedSize.X <= 0.0 || AllottedSize.Y <= 0.0)
		{
			return;
		}

		const FVector2D Extent = Bounds.Centerlines.GetSize();
		// An oversized brush is clipped by the slot. Keep its width even when there is very little room for the path.
		const FVector2D Available = (AllottedSize - FVector2D(2.0 * Bounds.MaxRadius))
			.ComponentMax(FVector2D(1.0)).ComponentMin(AllottedSize);
		Scale = Extent.X > 0.0 ? Available.X / Extent.X : 1.0;
		if (Extent.Y > 0.0)
		{
			const double ScaleY = Available.Y / Extent.Y;
			Scale = Extent.X > 0.0 ? FMath::Min(Scale, ScaleY) : ScaleY;
		}
		SourceMin = Bounds.Centerlines.Min;
		Offset = (AllottedSize - Extent * Scale) * 0.5;
	}

	FVector2D TransformPosition(const FVector2D& Position) const
	{
		return (Position - SourceMin) * Scale + Offset;
	}
};
