// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#pragma once

#include "Data/SlatePaintContext.h"
#include "Data/SlateSplineCurves.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

class WIDGETSPLINESYSTEM_API SSpline : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSpline) : _PreferredSize(FVector2D::ZeroVector), _MappingScale(1.0f)
	{
		_ImportedSplineBrush.SetImageSize(FVector2D::UnitVector);
	}
		SLATE_ARGUMENT(TArray<FSlateSpline>, Splines)
		SLATE_ARGUMENT(FSlateBrush, ImportedSplineBrush)
		SLATE_ARGUMENT(FVector2D, PreferredSize)
		SLATE_ARGUMENT(float, MappingScale)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArguments);
	void SetSplines(TConstArrayView<FSlateSpline> InSplines);
	void SetSplineCurves(const FSlateSplineCurves& InCurves);
	void SetSplineCurves(TConstArrayView<FSlateSplineCurves> InCurves);
	bool SetSplineFromProvider(const UObject* Provider, int32 ReparamStepsPerSegment = 10);
	void ClearSplineCurves();
	void SetImportedSplineBrush(const FSlateBrush& InBrush);
	void SetPreferredSize(FVector2D InSize);
	void SetMappingScale(float InScale);
	FSlateSplineFit GetSplineFit(const FVector2D& AllottedSize) const;
	const FBox2D& GetSplineBounds() const { return Bounds; }

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

protected:
	virtual void PaintSplineSimple(const FSlatePaintContext& InPaintContext) const;
	virtual void PaintSplineBrush(const FSlatePaintContext& InPaintContext) const;

private:
	struct FPaintSpline
	{
		FInterpCurveVector2D Position;
		FSlateBrush Brush;
	};

	void VisitSplines(TFunctionRef<void(const FInterpCurveVector2D&, const FSlateBrush&)> Visit) const;
	void RefreshBounds();
	double GetDisplayStrokeRadius() const;

	TArray<FPaintSpline> DesignerSplines;
	TOptional<TArray<FInterpCurveVector2D>> ImportedPositions;
	FSlateBrush ImportedSplineBrush;
	FBox2D Bounds = FBox2D(ForceInit);
	FVector2D PreferredSize = FVector2D::ZeroVector;
	float MappingScale = 1.0f;
};
