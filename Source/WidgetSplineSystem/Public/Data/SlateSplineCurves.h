// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#pragma once

#include "Data/SlateSpline.h"
#include "Math/InterpCurve.h"
#include "SlateSplineCurves.generated.h"

struct FSplineCurves;
struct FWidgetSplineSnapshot;

// Based on FSplineCurves from SplineComponent.h

USTRUCT(BlueprintType)
struct WIDGETSPLINESYSTEM_API FSlateSplineCurves
{
	GENERATED_BODY()

	/** Spline built from position data. */
	UPROPERTY()
	FInterpCurveVector2D Position;

	/** Input: distance along curve, output: parameter that puts you there. */
	UPROPERTY()
	FInterpCurveFloat ReparamTable;

	UPROPERTY()
	int32 ReparamStepsPerSegment = 0;

	UPROPERTY(transient)
	uint32 Version = 0xffffffff;

	bool operator==(const FSlateSplineCurves& Other) const
	{
		return Position == Other.Position;
	}

	bool operator!=(const FSlateSplineCurves& Other) const
	{
		return !(*this == Other);
	}

	/** Project local XY positions and tangents to widget units and rebuild the 2D distance table. */
	static FSlateSplineCurves FromSplineCurves(const FSplineCurves& Source, int32 InReparamStepsPerSegment = 10);
	static TArray<FSlateSplineCurves> FromSplineCurves(const TArray<FSplineCurves>& Sources, int32 InReparamStepsPerSegment = 10);

	/** Transform the 3D geometry first, then project onto world XY and measure the projected curve. */
	static FSlateSplineCurves FromSplineCurves(const FSplineCurves& LocalCurves, const FTransform& ComponentToWorld, int32 InReparamStepsPerSegment = 10);
	static TArray<FSlateSplineCurves> FromSplineSnapshots(const TArray<FWidgetSplineSnapshot>& Snapshots, int32 InReparamStepsPerSegment = 10);

	/** Bake a 3D copy with world positions, transformed tangents, composed rotation/scale keys and a world-distance table. */
	static FSplineCurves ToWorldSplineCurves(const FSplineCurves& LocalCurves, const FTransform& ComponentToWorld, int32 InReparamStepsPerSegment = 10);

	static FInterpCurveVector2D MakePositionCurve(const FSlateSpline& Spline, bool bOverrideLoopKey = false, float LoopKey = 0.0f);
	/** Bounds in source coordinates, including curve extrema, the closing segment and stroke footprint. */
	static FBox2D GetPaintBounds(const FInterpCurveVector2D& InPosition, const FSlateBrush& Brush);
	static float GetStrokeRadius(const FSlateBrush& Brush);

	/** Read the first spline. Returns false for an unavailable source or an empty set. */
	static bool TryFromProvider(const UObject* Provider, FSlateSplineCurves& OutCurves, int32 InReparamStepsPerSegment = 10);
	/** Read all splines in source order. OutCurves is only replaced on success. */
	static bool TryFromProvider(const UObject* Provider, TArray<FSlateSplineCurves>& OutCurves, int32 InReparamStepsPerSegment = 10);

	/** 
	 * Update the spline's internal data according to the passed-in params 
	 * @param	InSplineRef				The spline data
	 * @param	bClosedLoop				Whether the spline is to be considered as a closed loop.
	 * @param	bStationaryEndpoints	Whether the endpoints of the spline are considered stationary when traversing the spline at non-constant velocity.  Essentially this sets the endpoints' tangents to zero vectors.
	 * @param	InReparamStepsPerSegment	Number of steps per spline segment to place in the reparameterization table
	 * @param	bLoopPositionOverride	Whether to override the loop position with LoopPosition
	 * @param	LoopPosition			The loop position to use instead of the last key
	 * @param	Scale2D					The world scale to override
	 */
	void UpdateSpline(const FSlateSpline& InSplineRef, int32 InReparamStepsPerSegment = 10, bool bLoopPositionOverride = false, float
	                  LoopPosition = 0.0f, const FVector2D& Scale2D = FVector2D(1.0f));

	/** Returns the length of the specified spline segment up to the parametric value given */
	float GetSegmentLength(const int32 Index, const float Param, bool bClosedLoop = false, const FVector2D& Scale2D = FVector2D(1.0f)) const;

	/** Returns total length along this spline */
	float GetSplineLength() const;

	/** Input-key duration of a segment, including the closing segment of a loop. */
	float GetSegmentKeySpan(int32 Index) const;

private:
	void RebuildReparamTable(int32 InReparamStepsPerSegment, const FVector2D& Scale2D);
};
