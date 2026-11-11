// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SplineWidgetFunctionLibrary.generated.h"

class USplineWidget;

UENUM(BlueprintType)
enum class ESlateSplineCoordinateSpace : uint8
{
	Local,
	Viewport,
	Screen,
	/** Coordinates before the widget fits the spline bounds into its allotted size. */
	Source
};

/** SplineIndex follows display order. Distances and lengths are in source spline units. */
UCLASS()
class WIDGETSPLINESYSTEM_API USplineWidgetFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns total length along this spline */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static float GetSplineLength(const USplineWidget* InSplineWidget, int32 SplineIndex = 0);

	/** Get distance along the spline at the provided input key value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static float GetDistanceAlongSplineAtSplineInputKey(const USplineWidget* InSplineWidget, float InKey, int32 SplineIndex = 0);

	/** Get location along spline at the provided input key value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static FVector2D GetLocationAtSplineInputKey(const USplineWidget* InSplineWidget, float InKey, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex = 0);

	/** Get tangent along spline at the provided input key value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static FVector2D GetTangentAtSplineInputKey(const USplineWidget* InSplineWidget, float InKey, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex = 0);

	/** Get unit direction along spline at the provided input key value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static FVector2D GetDirectionAtSplineInputKey(const USplineWidget* InSplineWidget, float InKey, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex = 0);

	/** Get rotator corresponding to rotation along spline at the provided input key value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static float GetRotationAngleAtSplineInputKey(const USplineWidget* InSplineWidget, float InKey, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex = 0);

	/** Get the location at spline point */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static FVector2D GetLocationAtSplinePoint(const USplineWidget* InSplineWidget, int32 PointIndex, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex = 0);

	/** Get the direction at spline point */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static FVector2D GetDirectionAtSplinePoint(const USplineWidget* InSplineWidget, int32 PointIndex, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex = 0);

	/** Get the tangent at spline point. This fetches the Leave tangent of the point. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static FVector2D GetTangentAtSplinePoint(const USplineWidget* InSplineWidget, int32 PointIndex, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex = 0);

	/** Given a distance along the length of this spline, return the corresponding input key at that point */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static float GetInputKeyAtDistanceAlongSpline(const USplineWidget* InSplineWidget, float Distance, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex = 0);

	/** Given a distance along the length of this spline, return the point in space where this puts you */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static FVector2D GetLocationAtDistanceAlongSpline(const USplineWidget* InSplineWidget, float Distance, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex = 0);

	/** Given a distance along the length of this spline, return a unit direction vector of the spline tangent there. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static FVector2D GetDirectionAtDistanceAlongSpline(const USplineWidget* InSplineWidget, float Distance, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex = 0);

	/** Given a distance along the length of this spline, return the tangent vector of the spline there. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static FVector2D GetTangentAtDistanceAlongSpline(const USplineWidget* InSplineWidget, float Distance, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex = 0);

	/** Given a distance along the length of this spline, return a rotation corresponding to the spline's rotation there. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Spline)
	static float GetRotationAngleAtDistanceAlongSpline(const USplineWidget* InSplineWidget, float Distance, ESlateSplineCoordinateSpace CoordinateSpace, int32 SplineIndex = 0);

};
