// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#include "SplineWidgetFunctionLibrary.h"

#include "SplineWidget.h"
#include "Algo/BinarySearch.h"
#include "Blueprint/SlateBlueprintLibrary.h"

namespace
{
const FSlateSplineCurves* FindCurves(const USplineWidget* Widget, int32 SplineIndex)
{
	return Widget ? Widget->FindSplineCurves(SplineIndex) : nullptr;
}

const FInterpCurvePointVector2D* FindPositionPoint(const FSlateSplineCurves& Curves, int32 PointIndex)
{
	const auto& Points = Curves.Position.Points;
	if (Points.IsEmpty())
	{
		return nullptr;
	}
	const int32 Index = Curves.Position.bIsLooped && PointIndex >= Points.Num() ? 0 : FMath::Clamp(PointIndex, 0, Points.Num() - 1);
	return &Points[Index];
}

FVector2D TransformLocation(const USplineWidget& Widget, const FVector2D& Location, ESlateSplineCoordinateSpace Space)
{
	if (Space == ESlateSplineCoordinateSpace::Source)
	{
		return Location;
	}
	const FVector2D Local = Widget.GetSplineFit().TransformPosition(Location);
	if (Space == ESlateSplineCoordinateSpace::Screen)
	{
		return Widget.GetCachedGeometry().LocalToAbsolute(Local);
	}
	if (Space == ESlateSplineCoordinateSpace::Viewport)
	{
		FVector2D PixelPosition, ViewportPosition;
		USlateBlueprintLibrary::LocalToViewport(Widget.GetWorld(), Widget.GetCachedGeometry(), Local, PixelPosition, ViewportPosition);
		return ViewportPosition;
	}
	return Local;
}

FVector2D TransformVectorToSpace(const USplineWidget& Widget, const FVector2D& Vector, ESlateSplineCoordinateSpace Space)
{
	if (Space == ESlateSplineCoordinateSpace::Source)
	{
		return Vector;
	}
	const FVector2D Local = Vector * Widget.GetSplineFit().Scale;
	if (Space == ESlateSplineCoordinateSpace::Viewport)
	{
		FVector2D PixelPosition, Origin, End;
		USlateBlueprintLibrary::LocalToViewport(Widget.GetWorld(), Widget.GetCachedGeometry(), FVector2D::ZeroVector, PixelPosition, Origin);
		USlateBlueprintLibrary::LocalToViewport(Widget.GetWorld(), Widget.GetCachedGeometry(), Local, PixelPosition, End);
		return End - Origin;
	}
	if (Space == ESlateSplineCoordinateSpace::Screen)
	{
		return Widget.GetCachedGeometry().GetAccumulatedRenderTransform().TransformVector(Local);
	}
	return Local;
}
}

float USplineWidgetFunctionLibrary::GetSplineLength(const USplineWidget* InSplineWidget, int32 SplineIndex)
{
	const FSlateSplineCurves* Curves = FindCurves(InSplineWidget, SplineIndex);
	return Curves ? Curves->GetSplineLength() : 0.0f;
}

float USplineWidgetFunctionLibrary::GetDistanceAlongSplineAtSplineInputKey(const USplineWidget* InSplineWidget, float InKey, int32 SplineIndex)
{
	const FSlateSplineCurves* Curves = FindCurves(InSplineWidget, SplineIndex);
	if (!Curves || Curves->Position.Points.IsEmpty() || Curves->ReparamTable.Points.IsEmpty() || FMath::IsNaN(InKey))
	{
		return 0.0f;
	}
	const FInterpCurveVector2D& Position = Curves->Position;
	if (InKey <= Position.Points[0].InVal)
	{
		return 0.0f;
	}
	const float EndKey = Position.Points.Last().InVal + (Position.bIsLooped ? Position.LoopKeyOffset : 0.0f);
	if (InKey >= EndKey)
	{
		return Curves->GetSplineLength();
	}
	const int32 Index = Algo::UpperBoundBy(Position.Points, InKey, &FInterpCurvePointVector2D::InVal) - 1;
	const float StartDistance = Curves->ReparamTable.Points[Index * Curves->ReparamStepsPerSegment].InVal;
	const float Alpha = (InKey - Position.Points[Index].InVal) / Curves->GetSegmentKeySpan(Index);
	return StartDistance + Curves->GetSegmentLength(Index, FMath::Clamp(Alpha, 0.0f, 1.0f), Position.bIsLooped);
}

FVector2D USplineWidgetFunctionLibrary::GetLocationAtSplineInputKey(const USplineWidget* InSplineWidget, float InKey, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex)
{
	const FSlateSplineCurves* Curves = FindCurves(InSplineWidget, SplineIndex);
	return Curves && !Curves->Position.Points.IsEmpty()
		? TransformLocation(*InSplineWidget, Curves->Position.Eval(InKey, FVector2D::ZeroVector), CoordinateSpace) : FVector2D::ZeroVector;
}

FVector2D USplineWidgetFunctionLibrary::GetTangentAtSplineInputKey(const USplineWidget* InSplineWidget, float InKey, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex)
{
	const FSlateSplineCurves* Curves = FindCurves(InSplineWidget, SplineIndex);
	return Curves ? TransformVectorToSpace(*InSplineWidget, Curves->Position.EvalDerivative(InKey, FVector2D::ZeroVector), CoordinateSpace) : FVector2D::ZeroVector;
}

FVector2D USplineWidgetFunctionLibrary::GetDirectionAtSplineInputKey(const USplineWidget* InSplineWidget, float InKey, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex)
{
	return GetTangentAtSplineInputKey(InSplineWidget, InKey, CoordinateSpace, SplineIndex).GetSafeNormal();
}

float USplineWidgetFunctionLibrary::GetRotationAngleAtSplineInputKey(const USplineWidget* InSplineWidget, float InKey, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex)
{
	const FVector2D Direction = GetDirectionAtSplineInputKey(InSplineWidget, InKey, CoordinateSpace, SplineIndex);
	const float AngleInRadians = Direction.IsNearlyZero() ? 0.0f : FMath::Atan2(Direction.Y, Direction.X);
	return FMath::RadiansToDegrees(AngleInRadians);
}

FVector2D USplineWidgetFunctionLibrary::GetLocationAtSplinePoint(const USplineWidget* InSplineWidget, int32 PointIndex, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex)
{
	const FSlateSplineCurves* Curves = FindCurves(InSplineWidget, SplineIndex);
	if (!Curves)
	{
		return FVector2D::ZeroVector;
	}
	const FInterpCurvePointVector2D* Point = FindPositionPoint(*Curves, PointIndex);
	return Point ? TransformLocation(*InSplineWidget, Point->OutVal, CoordinateSpace) : FVector2D::ZeroVector;
}

FVector2D USplineWidgetFunctionLibrary::GetDirectionAtSplinePoint(const USplineWidget* InSplineWidget, int32 PointIndex, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex)
{
	return GetTangentAtSplinePoint(InSplineWidget, PointIndex, CoordinateSpace, SplineIndex).GetSafeNormal();
}

FVector2D USplineWidgetFunctionLibrary::GetTangentAtSplinePoint(const USplineWidget* InSplineWidget, int32 PointIndex, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex)
{
	const FSlateSplineCurves* Curves = FindCurves(InSplineWidget, SplineIndex);
	const FInterpCurvePointVector2D* Point = Curves ? FindPositionPoint(*Curves, PointIndex) : nullptr;
	return Point ? TransformVectorToSpace(*InSplineWidget, Point->LeaveTangent, CoordinateSpace) : FVector2D::ZeroVector;
}

float USplineWidgetFunctionLibrary::GetInputKeyAtDistanceAlongSpline(const USplineWidget* InSplineWidget, float Distance, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex)
{
	const FSlateSplineCurves* Curves = FindCurves(InSplineWidget, SplineIndex);
	return Curves ? Curves->ReparamTable.Eval(Distance, 0.0f) : 0.0f;
}

FVector2D USplineWidgetFunctionLibrary::GetLocationAtDistanceAlongSpline(const USplineWidget* InSplineWidget, float Distance, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex)
{
	const float Key = GetInputKeyAtDistanceAlongSpline(InSplineWidget, Distance, CoordinateSpace, SplineIndex);
	return GetLocationAtSplineInputKey(InSplineWidget, Key, CoordinateSpace, SplineIndex);
}

FVector2D USplineWidgetFunctionLibrary::GetDirectionAtDistanceAlongSpline(const USplineWidget* InSplineWidget, float Distance, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex)
{
	const float Key = GetInputKeyAtDistanceAlongSpline(InSplineWidget, Distance, CoordinateSpace, SplineIndex);
	return GetDirectionAtSplineInputKey(InSplineWidget, Key, CoordinateSpace, SplineIndex);
}

FVector2D USplineWidgetFunctionLibrary::GetTangentAtDistanceAlongSpline(const USplineWidget* InSplineWidget, float Distance, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex)
{
	const float Key = GetInputKeyAtDistanceAlongSpline(InSplineWidget, Distance, CoordinateSpace, SplineIndex);
	return GetTangentAtSplineInputKey(InSplineWidget, Key, CoordinateSpace, SplineIndex);
}

float USplineWidgetFunctionLibrary::GetRotationAngleAtDistanceAlongSpline(const USplineWidget* InSplineWidget, float Distance, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex)
{
	const float Key = GetInputKeyAtDistanceAlongSpline(InSplineWidget, Distance, CoordinateSpace, SplineIndex);
	return GetRotationAngleAtSplineInputKey(InSplineWidget, Key, CoordinateSpace, SplineIndex);
}
