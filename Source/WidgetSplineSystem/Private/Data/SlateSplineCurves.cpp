// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#include "Data/SlateSplineCurves.h"
#include "Components/SplineComponent.h"
#include "WidgetSplineCurvesProvider.h"

namespace
{
void TransformPositionPoint(FInterpCurvePointVector& Point, const FTransform& ComponentToWorld)
{
	Point.OutVal = ComponentToWorld.TransformPosition(Point.OutVal);
	Point.ArriveTangent = ComponentToWorld.TransformVector(Point.ArriveTangent);
	Point.LeaveTangent = ComponentToWorld.TransformVector(Point.LeaveTangent);
}

template <typename VectorType>
float SegmentKeySpan(const FInterpCurve<VectorType>& Position, int32 Index)
{
	return Index + 1 < Position.Points.Num()
		? Position.Points[Index + 1].InVal - Position.Points[Index].InVal
		: Position.LoopKeyOffset;
}

template <typename VectorType>
float MeasureSegment(const FInterpCurve<VectorType>& Position, int32 Index, float Param, bool bClosedLoop, const VectorType& Scale)
{
	const int32 NumPoints = Position.Points.Num();
	const int32 LastPoint = NumPoints - 1;

	check(Index >= 0 && ((bClosedLoop && Index < NumPoints) || (!bClosedLoop && Index < LastPoint)));
	check(Param >= 0.0f && Param <= 1.0f);

	// Evaluate the length of a Hermite spline segment.
	// This calculates the integral of |dP/dt| dt, where P(t) is the spline equation with components (x(t), y(t), z(t)).
	// This isn't solvable analytically, so we use a numerical method (Legendre-Gauss quadrature) which performs very well
	// with functions of this type, even with very few samples.  In this case, just 5 samples is sufficient to yield a
	// reasonable result.

	struct FLegendreGaussCoefficient
	{
		float Abscissa;
		float Weight;
	};

	static const FLegendreGaussCoefficient LegendreGaussCoefficients[] =
	{
		{ 0.0f, 0.5688889f },
		{ -0.5384693f, 0.47862867f },
		{ 0.5384693f, 0.47862867f },
		{ -0.90617985f, 0.23692688f },
		{ 0.90617985f, 0.23692688f }
	};

	const auto& StartPoint = Position.Points[Index];
	const auto& EndPoint = Position.Points[Index == LastPoint ? 0 : Index + 1];

	const auto& P0 = StartPoint.OutVal;
	const auto& P1 = EndPoint.OutVal;

	// Special cases for linear or constant segments
	if (StartPoint.InterpMode == CIM_Linear)
	{
		return ((P1 - P0) * Scale).Size() * Param;
	}
	else if (StartPoint.InterpMode == CIM_Constant)
	{
		return Param == 1.0f ? ((P1 - P0) * Scale).Size() : 0.0f;
	}

	// InterpCurve tangents are derivatives per input key; the integral uses a unit segment.
	const float KeySpan = SegmentKeySpan(Position, Index);
	const VectorType T0 = StartPoint.LeaveTangent * KeySpan;
	const VectorType T1 = EndPoint.ArriveTangent * KeySpan;

	// Cache the coefficients to be fed into the function to calculate the spline derivative at each sample point as they are constant.
	const VectorType Coeff1 = ((P0 - P1) * 2.0f + T0 + T1) * 3.0f;
	const VectorType Coeff2 = (P1 - P0) * 6.0f - T0 * 4.0f - T1 * 2.0f;
	const VectorType Coeff3 = T0;

	const float HalfParam = Param * 0.5f;

	float Length = 0.0f;
	for (const auto& LegendreGaussCoefficient : LegendreGaussCoefficients)
	{
		// Calculate derivative at each Legendre-Gauss sample, and perform a weighted sum
		const float Alpha = HalfParam * (1.0f + LegendreGaussCoefficient.Abscissa);
		const VectorType Derivative = ((Coeff1 * Alpha + Coeff2) * Alpha + Coeff3) * Scale;
		Length += Derivative.Size() * LegendreGaussCoefficient.Weight;
	}
	Length *= HalfParam;

	return Length;
}

template <typename VectorType>
int32 BuildDistanceTable(const FInterpCurve<VectorType>& Position, FInterpCurveFloat& ReparamTable, int32 RequestedSteps, const VectorType& Scale)
{
	const int32 NumPoints = Position.Points.Num();
	const int32 NumSegments = Position.bIsLooped ? NumPoints : FMath::Max(NumPoints - 1, 0);
	const int32 Steps = FMath::Clamp(RequestedSteps, 1, (MAX_int32 - 1) / FMath::Max(NumSegments, 1));
	ReparamTable.ClearLoopKey();
	ReparamTable.Points.Reset(NumSegments * Steps + 1);
	if (NumPoints == 0)
	{
		return Steps;
	}

	float Distance = 0.0f;
	for (int32 Index = 0; Index < NumSegments; ++Index)
	{
		const float StartKey = Position.Points[Index].InVal;
		const float KeySpan = SegmentKeySpan(Position, Index);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float Alpha = static_cast<float>(Step) / Steps;
			const float PartialLength = Step == 0 ? 0.0f : MeasureSegment(Position, Index, Alpha, Position.bIsLooped, Scale);
			ReparamTable.Points.Emplace(Distance + PartialLength, StartKey + Alpha * KeySpan, 0.0f, 0.0f, CIM_Linear);
		}
		Distance += MeasureSegment(Position, Index, 1.0f, Position.bIsLooped, Scale);
	}
	const float EndKey = Position.Points.Last().InVal + (Position.bIsLooped ? Position.LoopKeyOffset : 0.0f);
	ReparamTable.Points.Emplace(Distance, EndKey, 0.0f, 0.0f, CIM_Linear);
	return Steps;
}
}

FSlateSplineCurves FSlateSplineCurves::FromSplineCurves(const FSplineCurves& Source, int32 InReparamStepsPerSegment)
{
	return FromSplineCurves(Source, FTransform::Identity, InReparamStepsPerSegment);
}

FSlateSplineCurves FSlateSplineCurves::FromSplineCurves(const FSplineCurves& LocalCurves, const FTransform& ComponentToWorld, int32 InReparamStepsPerSegment)
{
	FSlateSplineCurves Result;
	Result.Position.Points.Reserve(LocalCurves.Position.Points.Num());
	for (FInterpCurvePointVector Point : LocalCurves.Position.Points)
	{
		TransformPositionPoint(Point, ComponentToWorld);
		Result.Position.Points.Emplace(Point.InVal, FVector2D(Point.OutVal.X, Point.OutVal.Y),
			FVector2D(Point.ArriveTangent.X, Point.ArriveTangent.Y),
			FVector2D(Point.LeaveTangent.X, Point.LeaveTangent.Y), Point.InterpMode);
	}
	Result.Position.bIsLooped = LocalCurves.Position.bIsLooped;
	Result.Position.LoopKeyOffset = LocalCurves.Position.LoopKeyOffset;
	Result.RebuildReparamTable(InReparamStepsPerSegment, FVector2D(1.0f));
	return Result;
}

TArray<FSlateSplineCurves> FSlateSplineCurves::FromSplineCurves(const TArray<FSplineCurves>& Sources, int32 InReparamStepsPerSegment)
{
	TArray<FSlateSplineCurves> Result;
	Result.Reserve(Sources.Num());
	for (const FSplineCurves& Source : Sources)
	{
		Result.Add(FromSplineCurves(Source, InReparamStepsPerSegment));
	}
	return Result;
}

TArray<FSlateSplineCurves> FSlateSplineCurves::FromSplineSnapshots(const TArray<FWidgetSplineSnapshot>& Snapshots, int32 InReparamStepsPerSegment)
{
	TArray<FSlateSplineCurves> Result;
	Result.Reserve(Snapshots.Num());
	for (const FWidgetSplineSnapshot& Snapshot : Snapshots)
	{
		Result.Add(FromSplineCurves(Snapshot.LocalCurves, Snapshot.ComponentToWorld, InReparamStepsPerSegment));
	}
	return Result;
}

FSplineCurves FSlateSplineCurves::ToWorldSplineCurves(const FSplineCurves& LocalCurves, const FTransform& ComponentToWorld, int32 InReparamStepsPerSegment)
{
	FSplineCurves World = LocalCurves;
	for (FInterpCurvePointVector& Point : World.Position.Points)
	{
		TransformPositionPoint(Point, ComponentToWorld);
	}

	const FQuat Rotation = ComponentToWorld.GetRotation();
	for (FInterpCurvePointQuat& Point : World.Rotation.Points)
	{
		// UE's quaternion curve tangents are control orientations.
		Point.OutVal = Rotation * Point.OutVal;
		Point.ArriveTangent = Rotation * Point.ArriveTangent;
		Point.LeaveTangent = Rotation * Point.LeaveTangent;
	}
	const FVector Scale = ComponentToWorld.GetScale3D();
	for (FInterpCurvePointVector& Point : World.Scale.Points)
	{
		Point.OutVal *= Scale;
		Point.ArriveTangent *= Scale;
		Point.LeaveTangent *= Scale;
	}

	// Keep baked auto/break tangents and custom keys. Rebuild only the derived distance table.
	BuildDistanceTable(World.Position, World.ReparamTable, InReparamStepsPerSegment, FVector::OneVector);
	++World.Version;
	return World;
}

static bool ReadProviderSplines(const UObject* Provider, TArray<FWidgetSplineSnapshot>& OutSplines)
{
	const IWidgetSplineCurvesProvider* Source = IsValid(Provider) ? Cast<IWidgetSplineCurvesProvider>(Provider) : nullptr;
	return Source && Source->GetWidgetSplineSnapshots(OutSplines);
}

bool FSlateSplineCurves::TryFromProvider(const UObject* Provider, FSlateSplineCurves& OutCurves, int32 InReparamStepsPerSegment)
{
	TArray<FWidgetSplineSnapshot> Splines;
	if (!ReadProviderSplines(Provider, Splines) || Splines.IsEmpty())
	{
		return false;
	}
	OutCurves = FromSplineCurves(Splines[0].LocalCurves, Splines[0].ComponentToWorld, InReparamStepsPerSegment);
	return true;
}

bool FSlateSplineCurves::TryFromProvider(const UObject* Provider, TArray<FSlateSplineCurves>& OutCurves, int32 InReparamStepsPerSegment)
{
	TArray<FWidgetSplineSnapshot> Splines;
	if (!ReadProviderSplines(Provider, Splines))
	{
		return false;
	}
	OutCurves = FromSplineSnapshots(Splines, InReparamStepsPerSegment);
	return true;
}

void FSlateSplineCurves::UpdateSpline(const FSlateSpline& InSplineRef, int32 InReparamStepsPerSegment,
	bool bLoopPositionOverride, float LoopPosition, const FVector2D& Scale2D)
{
	Position = MakePositionCurve(InSplineRef, bLoopPositionOverride, LoopPosition);
	RebuildReparamTable(InReparamStepsPerSegment, Scale2D);
}

FInterpCurveVector2D FSlateSplineCurves::MakePositionCurve(const FSlateSpline& Spline, bool bOverrideLoopKey, float LoopKey)
{
	FInterpCurveVector2D Result;
	Result.Points.Reserve(Spline.Points.Num());
	const EInterpCurveMode Mode = Spline.bIsLinear ? CIM_Linear : CIM_CurveUser;
	for (int32 Index = 0; Index < Spline.Points.Num(); ++Index)
	{
		const FSlateSplinePoint& Point = Spline.Points[Index];
		Result.Points.Emplace(Index, Point.Location, Point.Direction, Point.Direction, Mode);
	}
	if (Spline.bIsClosedLoop && !Result.Points.IsEmpty())
	{
		Result.SetLoopKey(bOverrideLoopKey ? LoopKey : Result.Points.Last().InVal + 1.0f);
	}
	return Result;
}

FBox2D FSlateSplineCurves::GetPositionBounds(const FInterpCurveVector2D& InPosition)
{
	const auto& Points = InPosition.Points;
	if (Points.IsEmpty())
	{
		return FBox2D(ForceInit);
	}
	FBox2D Bounds(Points[0].OutVal, Points[0].OutVal);
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		CurveVector2DFindIntervalBounds(Points[Index - 1], Points[Index], Bounds.Min, Bounds.Max);
	}
	if (InPosition.bIsLooped)
	{
		FInterpCurvePointVector2D End = Points[0];
		End.InVal = Points.Last().InVal + InPosition.LoopKeyOffset;
		CurveVector2DFindIntervalBounds(Points.Last(), End, Bounds.Min, Bounds.Max);
	}
	return Bounds;
}

FBox2D FSlateSplineCurves::GetPaintBounds(const FInterpCurveVector2D& InPosition, const FSlateBrush& Brush)
{
	const FBox2D Bounds = GetPositionBounds(InPosition);
	return Bounds.bIsValid ? Bounds.ExpandBy(GetStrokeRadius(Brush)) : Bounds;
}

float FSlateSplineCurves::GetStrokeRadius(const FSlateBrush& Brush)
{
	const float Width = FMath::Max(0.0f, static_cast<float>(Brush.GetImageSize().X));
	return 0.5f * Width + (Width > 0.0f && !IsValid(Brush.GetResourceObject()) ? 1.0f : 0.0f);
}

float FSlateSplineCurves::GetSegmentKeySpan(int32 Index) const
{
	return SegmentKeySpan(Position, Index);
}

void FSlateSplineCurves::RebuildReparamTable(int32 InReparamStepsPerSegment, const FVector2D& Scale2D)
{
	ReparamStepsPerSegment = BuildDistanceTable(Position, ReparamTable, InReparamStepsPerSegment, Scale2D);
	++Version;
}

float FSlateSplineCurves::GetSegmentLength(int32 Index, float Param, bool bClosedLoop, const FVector2D& Scale2D) const
{
	return MeasureSegment(Position, Index, Param, bClosedLoop, Scale2D);
}

float FSlateSplineCurves::GetSplineLength() const
{
	return (ReparamTable.Points.Num() > 0) ? ReparamTable.Points.Last().InVal : 0.0f; 
}
