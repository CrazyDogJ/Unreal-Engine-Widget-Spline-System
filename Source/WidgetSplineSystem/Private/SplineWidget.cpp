// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#include "SplineWidget.h"
#include "Serialization/CustomVersion.h"

namespace
{
const FGuid SplineWidgetVersionGuid(0x384D6227, 0xCEE14169, 0x8583C457, 0x7150E9C0);
constexpr int32 ArraySplineDataVersion = 1;
const FCustomVersionRegistration SplineWidgetVersion(SplineWidgetVersionGuid, ArraySplineDataVersion, TEXT("WidgetSplineSystem"));
}

USplineWidget::USplineWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ImportedSplineBrush.SetImageSize(FVector2D::UnitVector);
#if WITH_EDITORONLY_DATA
	SplineData_DEPRECATED.Points.Empty();
#endif
}

void USplineWidget::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(SplineWidgetVersionGuid);
#if WITH_EDITORONLY_DATA
	const bool bLoadLegacy = Ar.IsLoading() && Ar.IsPersistent() && !Ar.IsFilterEditorOnly()
		&& Ar.CustomVer(SplineWidgetVersionGuid) < ArraySplineDataVersion;
	if (bLoadLegacy)
	{
		// Old assets can serialize only the differences from their archetype.
		SplineData_DEPRECATED = Splines.IsEmpty() ? FSlateSpline() : Splines[0];
		AdditionalSplines_DEPRECATED.Reset();
		if (Splines.Num() > 1)
		{
			AdditionalSplines_DEPRECATED.Append(Splines.GetData() + 1, Splines.Num() - 1);
		}
	}
#endif
	Super::Serialize(Ar);
#if WITH_EDITORONLY_DATA
	if (bLoadLegacy)
	{
		ImportedSplineBrush = SplineData_DEPRECATED.Brush;
		Splines.Reset(1 + AdditionalSplines_DEPRECATED.Num());
		Splines.Add(MoveTemp(SplineData_DEPRECATED));
		Splines.Append(MoveTemp(AdditionalSplines_DEPRECATED));
		SplineData_DEPRECATED.Brush = FSlateBrush();
	}
#endif
}

TSharedRef<SWidget> USplineWidget::RebuildWidget()
{
	SlateSpline = SNew(SSpline)
		.IsSplineClickEnabled_UObject(this, &USplineWidget::HasSplineClickListeners)
		.OnSplineClicked_UObject(this, &USplineWidget::HandleSplineClicked);
	return SlateSpline.ToSharedRef();
}

void USplineWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	SlateSpline.Reset();
}

void USplineWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (SlateSpline.IsValid())
	{
		SlateSpline->SetImportedSplineBrush(ImportedSplineBrush);
	}
	if (!bUsingExternalSplines)
	{
		UpdateSpline();
	}
	else if (SlateSpline.IsValid())
	{
		SlateSpline->SetSplineCurves(SplineCurves);
	}
}

void USplineWidget::SetSplines(const TArray<FSlateSpline>& InSplines)
{
	Splines = InSplines;
	UpdateSpline();
}

void USplineWidget::SetImportedSplineBrush(const FSlateBrush& InBrush)
{
	ImportedSplineBrush = InBrush;
	if (SlateSpline.IsValid())
	{
		SlateSpline->SetImportedSplineBrush(InBrush);
	}
}

bool USplineWidget::HasSplineClickListeners() const
{
	return OnSplineClicked.IsBound();
}

void USplineWidget::HandleSplineClicked(int32 SplineIndex)
{
	OnSplineClicked.Broadcast(SplineIndex);
}

void USplineWidget::UpdateSpline()
{
	bUsingExternalSplines = false;
	SplineCurves.SetNum(Splines.Num());
	for (int32 Index = 0; Index < Splines.Num(); ++Index)
	{
		SplineCurves[Index].UpdateSpline(Splines[Index]);
	}
	if (SlateSpline.IsValid())
	{
		SlateSpline->SetSplines(Splines);
	}
}

void USplineWidget::SetSplineCurves(FSlateSplineCurves InCurves)
{
	TArray<FSlateSplineCurves> Curves;
	Curves.Add(MoveTemp(InCurves));
	SetSplineCurves(MoveTemp(Curves));
}

void USplineWidget::SetSplineCurves(TArray<FSlateSplineCurves> InSplines)
{
	for (int32 Index = 0; Index < InSplines.Num(); ++Index)
	{
		InSplines[Index].Version = SplineCurves.IsValidIndex(Index) ? SplineCurves[Index].Version + 1 : 0;
	}
	SplineCurves = MoveTemp(InSplines);
	bUsingExternalSplines = true;
	if (SlateSpline.IsValid())
	{
		SlateSpline->SetSplineCurves(SplineCurves);
	}
}

bool USplineWidget::SetSplineFromProvider(UObject* Provider, int32 ReparamStepsPerSegment)
{
	TArray<FSlateSplineCurves> Curves;
	if (!FSlateSplineCurves::TryFromProvider(Provider, Curves, ReparamStepsPerSegment))
	{
		return false;
	}
	SetSplineCurves(MoveTemp(Curves));
	return true;
}

int32 USplineWidget::GetSplineCount() const
{
	return bUsingExternalSplines ? SplineCurves.Num() : Splines.Num();
}

const FSlateSplineCurves* USplineWidget::FindSplineCurves(int32 SplineIndex) const
{
	return SplineCurves.IsValidIndex(SplineIndex) ? &SplineCurves[SplineIndex] : nullptr;
}

FBox2D USplineWidget::GetSplineBounds() const
{
	return SlateSpline.IsValid() ? SlateSpline->GetSplineBounds() : CalculateSplineBounds().Painted;
}

FSlateSplineBounds USplineWidget::CalculateSplineBounds() const
{
	FSlateSplineBounds Bounds;
	if (bUsingExternalSplines)
	{
		for (const FSlateSplineCurves& Curve : SplineCurves)
		{
			Bounds.Add(Curve.Position, ImportedSplineBrush);
		}
	}
	else
	{
		for (const FSlateSpline& Spline : Splines)
		{
			Bounds.Add(FSlateSplineCurves::MakePositionCurve(Spline), Spline.Brush);
		}
	}
	return Bounds;
}

FSlateSplineFit USplineWidget::GetSplineFit() const
{
	if (SlateSpline.IsValid())
	{
		return SlateSpline->GetSplineFit(SlateSpline->GetCachedGeometry().GetLocalSize());
	}
	const FSlateSplineBounds Bounds = CalculateSplineBounds();
	return FSlateSplineFit(Bounds, Bounds.GetDesiredSize());
}
