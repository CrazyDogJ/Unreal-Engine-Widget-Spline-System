// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#include "Slate/SSpline.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Slate/SplineBuilder.h"

namespace
{
// Both painters use widget-local positions and Hermite tangents per unit segment.
template <typename VisitorType>
void VisitSplineSegments(const FInterpCurveVector2D& Curve, const FSlateSplineFit& Fit, VisitorType&& Visit)
{
	const auto& Points = Curve.Points;
	const int32 Count = Curve.bIsLooped ? Points.Num() : Points.Num() - 1;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const bool bClosing = Index + 1 == Points.Num();
		const auto& Start = Points[Index];
		const auto& End = Points[bClosing ? 0 : Index + 1];
		const double TangentScale = Fit.Scale * (bClosing ? Curve.LoopKeyOffset : End.InVal - Start.InVal);
		Visit(FSlateSplinePoint(Fit.TransformPosition(Start.OutVal), Start.LeaveTangent * TangentScale),
			FSlateSplinePoint(Fit.TransformPosition(End.OutVal), End.ArriveTangent * TangentScale), Start.InterpMode);
	}
}

bool HitsStroke(const FInterpCurveVector2D& Curve, const FSlateSplineFit& Fit, FVector2D Pointer, double Radius)
{
	bool bHit = false;
	VisitSplineSegments(Curve, Fit, [&](const FSlateSplinePoint& Start, const FSlateSplinePoint& End, EInterpCurveMode Mode)
	{
		if (bHit || Mode == CIM_Constant)
		{
			return;
		}
		FBox2D SegmentBounds(Start.Location, Start.Location);
		SegmentBounds += End.Location;
		if (Mode != CIM_Linear)
		{
			SegmentBounds += Start.Location + Start.Direction / 3.0;
			SegmentBounds += End.Location - End.Direction / 3.0;
		}
		if (!SegmentBounds.ExpandBy(Radius).IsInsideOrOn(Pointer))
		{
			return;
		}

		FVector2D Previous = Start.Location;
		FSplineBuilder::VisitSegmentPoints(Start, End, Mode == CIM_Linear, [&](const FVector2D& Point)
		{
			if (!bHit && Point != Previous)
			{
				const FVector2D Closest = FMath::ClosestPointOnSegment2D(Pointer, Previous, Point);
				bHit = FVector2D::DistSquared(Closest, Pointer) <= FMath::Square(Radius);
			}
			Previous = Point;
		});
	});
	return bHit;
}
}

void SSpline::Construct(const FArguments& InArguments)
{
	OnSplineClicked = InArguments._OnSplineClicked;
	IsSplineClickEnabled = InArguments._IsSplineClickEnabled;
	ImportedSplineBrush = InArguments._ImportedSplineBrush;
	SetSplines(InArguments._Splines);
}

void SSpline::SetSplines(TConstArrayView<FSlateSpline> InSplines)
{
	DesignerSplines.Reset(InSplines.Num());
	for (const FSlateSpline& Spline : InSplines)
	{
		DesignerSplines.Add({ FSlateSplineCurves::MakePositionCurve(Spline), Spline.Brush });
	}
	ClearSplineCurves();
}

void SSpline::SetSplineCurves(const FSlateSplineCurves& InCurves)
{
	SetSplineCurves(MakeArrayView(&InCurves, 1));
}

void SSpline::SetSplineCurves(TConstArrayView<FSlateSplineCurves> InCurves)
{
	PressedSplineIndex = INDEX_NONE;
	auto& Positions = ImportedPositions.Emplace();
	Positions.Reserve(InCurves.Num());
	for (const FSlateSplineCurves& Curves : InCurves)
	{
		Positions.Add(Curves.Position);
	}
	RefreshBounds();
}

bool SSpline::SetSplineFromProvider(const UObject* Provider, int32 ReparamStepsPerSegment)
{
	TArray<FSlateSplineCurves> Curves;
	if (!FSlateSplineCurves::TryFromProvider(Provider, Curves, ReparamStepsPerSegment))
	{
		return false;
	}
	SetSplineCurves(Curves);
	return true;
}

void SSpline::ClearSplineCurves()
{
	PressedSplineIndex = INDEX_NONE;
	ImportedPositions.Reset();
	RefreshBounds();
}

void SSpline::SetImportedSplineBrush(const FSlateBrush& InBrush)
{
	ImportedSplineBrush = InBrush;
	if (ImportedPositions.IsSet())
	{
		RefreshBounds();
	}
}

void SSpline::VisitSplines(TFunctionRef<void(const FInterpCurveVector2D&, const FSlateBrush&)> Visit) const
{
	if (ImportedPositions.IsSet())
	{
		for (const FInterpCurveVector2D& Position : ImportedPositions.GetValue())
		{
			Visit(Position, ImportedSplineBrush);
		}
		return;
	}
	for (const FPaintSpline& Spline : DesignerSplines)
	{
		Visit(Spline.Position, Spline.Brush);
	}
}

void SSpline::RefreshBounds()
{
	Bounds = FSlateSplineBounds();
	VisitSplines([this](const FInterpCurveVector2D& Position, const FSlateBrush& Brush)
	{
		Bounds.Add(Position, Brush);
	});
	Invalidate(EInvalidateWidgetReason::Layout);
}

FVector2D SSpline::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return Bounds.GetDesiredSize();
}

FSlateSplineFit SSpline::GetSplineFit(const FVector2D& AllottedSize) const
{
	return FSlateSplineFit(Bounds, AllottedSize);
}

int32 SSpline::FindSplineAt(const FGeometry& Geometry, const FVector2D& ScreenPosition) const
{
	const FSlateSplineFit Fit = GetSplineFit(Geometry.GetLocalSize());
	if (Fit.Scale <= 0.0 || !Geometry.IsUnderLocation(ScreenPosition))
	{
		return INDEX_NONE;
	}
	const FVector2D Pointer = Geometry.AbsoluteToLocal(ScreenPosition);
	const bool bImported = ImportedPositions.IsSet();
	const int32 Count = bImported ? ImportedPositions->Num() : DesignerSplines.Num();
	for (int32 Index = Count - 1; Index >= 0; --Index)
	{
		const FSlateBrush& Brush = bImported ? ImportedSplineBrush : DesignerSplines[Index].Brush;
		const double Width = Brush.GetImageSize().X;
		if (Width > 0.0)
		{
			const FInterpCurveVector2D& Path = bImported ? (*ImportedPositions)[Index] : DesignerSplines[Index].Position;
			if (HitsStroke(Path, Fit, Pointer, Width * 0.5))
			{
				return Index;
			}
		}
	}
	return INDEX_NONE;
}

FReply SSpline::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !HasMouseCapture()
		&& OnSplineClicked.IsBound() && IsSplineClickEnabled.Get())
	{
		PressedSplineIndex = FindSplineAt(MyGeometry, MouseEvent.GetScreenSpacePosition());
		if (PressedSplineIndex != INDEX_NONE)
		{
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
	}
	return FReply::Unhandled();
}

FReply SSpline::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SSpline::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !HasMouseCaptureByUser(MouseEvent.GetUserIndex(), MouseEvent.GetPointerIndex()))
	{
		return FReply::Unhandled();
	}
	const int32 Index = PressedSplineIndex;
	PressedSplineIndex = INDEX_NONE;
	if (Index != INDEX_NONE && IsSplineClickEnabled.Get() && FindSplineAt(MyGeometry, MouseEvent.GetScreenSpacePosition()) == Index)
	{
		OnSplineClicked.ExecuteIfBound(Index);
	}
	return FReply::Handled().ReleaseMouseCapture();
}

void SSpline::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	PressedSplineIndex = INDEX_NONE;
	SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
}

int32 SSpline::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateSplineFit Fit = GetSplineFit(AllottedGeometry.GetLocalSize());
	if (Fit.Scale <= 0.0)
	{
		return LayerId;
	}
	const ESlateDrawEffect Effect = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));
	VisitSplines([&](const FInterpCurveVector2D& Position, const FSlateBrush& Brush)
	{
		if (Brush.GetImageSize().X <= 0.0f || Position.Points.Num() < (Position.bIsLooped ? 1 : 2))
		{
			return;
		}
		FSlatePaintContext Context(OutDrawElements, AllottedGeometry, ++LayerId, Effect, Brush.TintColor.GetColor(InWidgetStyle).ToFColorSRGB());
		Context.Brush = &Brush;
		Context.Position = &Position;
		Context.Fit = Fit;
		if (IsValid(Brush.GetResourceObject()))
		{
			PaintSplineBrush(Context);
		}
		else
		{
			PaintSplineSimple(Context);
		}
	});
	OutDrawElements.PopClip();
	return LayerId;
}

void SSpline::PaintSplineSimple(const FSlatePaintContext& Context) const
{
	// MakeSpline takes its stroke width in screen pixels.
	const float Thickness = FMath::Max(0.0f, static_cast<float>(Context.Brush->GetImageSize().X))
		* Context.AllotedGeometry.GetAccumulatedLayoutTransform().GetScale();
	VisitSplineSegments(*Context.Position, Context.Fit, [&](const FSlateSplinePoint& Start, const FSlateSplinePoint& End, EInterpCurveMode Mode)
	{
		if (Mode != CIM_Constant)
		{
			FSlateDrawElement::MakeSpline(Context.OutDrawElements, Context.LayerId, Context.PaintGeometry,
				Start.Location, Mode == CIM_Linear ? FVector2D::ZeroVector : Start.Direction,
				End.Location, Mode == CIM_Linear ? FVector2D::ZeroVector : End.Direction,
				Thickness, Context.DrawEffect, Context.TintColor);
		}
	});
}

void SSpline::PaintSplineBrush(const FSlatePaintContext& Context) const
{
	const FSlateResourceHandle Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Context.Brush);
	TOptional<FSplineBuilder> Strip;
	auto FinishStrip = [&](bool bCloseLoop)
	{
		if (Strip.IsSet())
		{
			Strip->Finish(bCloseLoop);
			FSlateDrawElement::MakeCustomVerts(Context.OutDrawElements, Context.LayerId, Resource,
				Strip->GetVertexArray(), Strip->GetIndexArray(), nullptr, 0, 0, Context.DrawEffect, ESlateBatchDrawFlag::TileV);
			Strip.Reset();
		}
	};

	bool bCloseStrip = Context.Position->bIsLooped;
	VisitSplineSegments(*Context.Position, Context.Fit, [&](const FSlateSplinePoint& Start, const FSlateSplinePoint& End, EInterpCurveMode Mode)
	{
		if (Mode == CIM_Constant)
		{
			FinishStrip(false);
			bCloseStrip = false;
			return;
		}
		if (!Strip.IsSet())
		{
			Strip.Emplace(Context.Brush->GetImageSize(), Context);
		}
		Strip->BuildBezierGeometry(Start, End, Mode == CIM_Linear);
	});
	FinishStrip(bCloseStrip);
}
