// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#pragma once

#include "Components/SplineComponent.h"
#include "WidgetSplineSnapshot.generated.h"

/** One baked spline component. All snapshots in a map must use the same world coordinate frame. */
USTRUCT()
struct WIDGETSPLINESYSTEM_API FWidgetSplineSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	FSplineCurves LocalCurves;

	/** The component's GetComponentTransform() at bake time, including its parent transforms. */
	UPROPERTY()
	FTransform ComponentToWorld = FTransform::Identity;
};
