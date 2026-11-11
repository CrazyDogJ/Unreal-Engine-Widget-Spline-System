// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#include "WidgetSplineCurvesProvider.h"

bool IWidgetSplineCurvesProvider::GetWidgetSplineSnapshots(TArray<FWidgetSplineSnapshot>& OutSnapshots) const
{
	TArray<FSplineCurves> Curves;
	if (!GetWidgetSplines(Curves))
	{
		return false;
	}
	OutSnapshots.Reset(Curves.Num());
	for (FSplineCurves& Curve : Curves)
	{
		OutSnapshots.AddDefaulted_GetRef().LocalCurves = MoveTemp(Curve);
	}
	return true;
}

bool IWidgetSplineCurvesProvider::GetWidgetSplines(TArray<FSplineCurves>& OutSplines) const
{
	FSplineCurves Curve;
	if (!GetWidgetSplineCurves(Curve))
	{
		return false;
	}
	OutSplines.Reset(1);
	OutSplines.Add(MoveTemp(Curve));
	return true;
}
