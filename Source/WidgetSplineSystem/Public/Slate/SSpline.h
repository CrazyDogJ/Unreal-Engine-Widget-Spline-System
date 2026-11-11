// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#pragma once

#include "Data/SlatePaintContext.h"
#include "Data/SlateSplineCurves.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

class WIDGETSPLINESYSTEM_API SSpline : public SLeafWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSplineClicked, int32);

	SLATE_BEGIN_ARGS(SSpline) : _IsSplineClickEnabled(true)
	{
		_ImportedSplineBrush.SetImageSize(FVector2D::UnitVector);
	}
		SLATE_ARGUMENT(TArray<FSlateSpline>, Splines)
		SLATE_ARGUMENT(FSlateBrush, ImportedSplineBrush)
		SLATE_EVENT(FOnSplineClicked, OnSplineClicked)
		SLATE_ATTRIBUTE(bool, IsSplineClickEnabled)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArguments);
	void SetSplines(TConstArrayView<FSlateSpline> InSplines);
	void SetSplineCurves(const FSlateSplineCurves& InCurves);
	void SetSplineCurves(TConstArrayView<FSlateSplineCurves> InCurves);
	bool SetSplineFromProvider(const UObject* Provider, int32 ReparamStepsPerSegment = 10);
	void ClearSplineCurves();
	void SetImportedSplineBrush(const FSlateBrush& InBrush);
	FSlateSplineFit GetSplineFit(const FVector2D& AllottedSize) const;
	const FBox2D& GetSplineBounds() const { return Bounds.Painted; }
	/** Topmost painted path under a screen-space pointer, or INDEX_NONE. */
	int32 FindSplineAt(const FGeometry& Geometry, const FVector2D& ScreenPosition) const;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
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

	TArray<FPaintSpline> DesignerSplines;
	TOptional<TArray<FInterpCurveVector2D>> ImportedPositions;
	FSlateBrush ImportedSplineBrush;
	FSlateSplineBounds Bounds;
	FOnSplineClicked OnSplineClicked;
	TAttribute<bool> IsSplineClickEnabled;
	int32 PressedSplineIndex = INDEX_NONE;
};
