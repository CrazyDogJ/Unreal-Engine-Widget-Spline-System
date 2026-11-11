// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#pragma once

#include "Data/WidgetSplineSnapshot.h"
#include "UObject/Interface.h"
#include "WidgetSplineCurvesProvider.generated.h"

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class WIDGETSPLINESYSTEM_API UWidgetSplineCurvesProvider : public UInterface
{
	GENERATED_BODY()
};

/** Supply an ordered set of baked components or curves already in a shared coordinate frame. */
class WIDGETSPLINESYSTEM_API IWidgetSplineCurvesProvider
{
	GENERATED_BODY()

public:
	/** Override for baked components. The default wraps GetWidgetSplines results in identity transforms. */
	virtual bool GetWidgetSplineSnapshots(TArray<FWidgetSplineSnapshot>& OutSnapshots) const;

	/** Return true to replace the displayed set (an empty array clears it), or false if unavailable. */
	virtual bool GetWidgetSplines(TArray<FSplineCurves>& OutSplines) const;

	/** Single-spline providers can continue to override this instead of GetWidgetSplines. */
	virtual bool GetWidgetSplineCurves(FSplineCurves& OutCurves) const { return false; }
};
