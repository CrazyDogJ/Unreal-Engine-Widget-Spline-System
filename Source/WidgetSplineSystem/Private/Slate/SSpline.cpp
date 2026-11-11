// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#include "Slate/SSpline.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Slate/SplineBuilder.h"

namespace
{
// Both painters use widget-local positions and Hermite tangents per unit segment.
template <typename VisitorType>
void VisitSplineSegments(const FSlatePaintContext& Context, VisitorType&& Visit)
{
	const FInterpCurveVector2D& Curve = *Context.Position;
	const auto& Points = Curve.Points;
	const int32 Count = Curve.bIsLooped ? Points.Num() : Points.Num() - 1;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const bool bClosing = Index + 1 == Points.Num();
		const auto& Start = Points[Index];
		const auto& End = Points[bClosing ? 0 : Index + 1];
		const double TangentScale = Context.Fit.Scale * (bClosing ? Curve.LoopKeyOffset : End.InVal - Start.InVal);
		Visit(FSlateSplinePoint(Context.Fit.TransformPosition(Start.OutVal), Start.LeaveTangent * TangentScale),
			FSlateSplinePoint(Context.Fit.TransformPosition(End.OutVal), End.ArriveTangent * TangentScale), Start.InterpMode);
	}
}
}

void SSpline::Construct(const FArguments& InArguments)
{
	PreferredSize = InArguments._PreferredSize;
	MappingScale = FSlateSplineFit::ClampMappingScale(InArguments._MappingScale);
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

void SSpline::SetPreferredSize(FVector2D InSize)
{
	if (PreferredSize != InSize)
	{
		PreferredSize = InSize;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void SSpline::SetMappingScale(float InScale)
{
	const float NewScale = FSlateSplineFit::ClampMappingScale(InScale);
	if (MappingScale != NewScale)
	{
		MappingScale = NewScale;
		Invalidate(EInvalidateWidgetReason::Paint);
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
	Bounds = FBox2D(ForceInit);
	VisitSplines([this](const FInterpCurveVector2D& Position, const FSlateBrush& Brush)
	{
		Bounds += FSlateSplineCurves::GetPaintBounds(Position, Brush);
	});
	Invalidate(EInvalidateWidgetReason::Layout);
}

FVector2D SSpline::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FSlateSplineFit::GetDesiredSize(Bounds, PreferredSize, GetDisplayStrokeRadius());
}

double SSpline::GetDisplayStrokeRadius() const
{
	return ImportedPositions.IsSet() ? FSlateSplineCurves::GetStrokeRadius(ImportedSplineBrush) : 0.0;
}

FSlateSplineFit SSpline::GetSplineFit(const FVector2D& AllottedSize) const
{
	return FSlateSplineFit(Bounds, AllottedSize, MappingScale, GetDisplayStrokeRadius());
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
		if (Position.Points.Num() < (Position.bIsLooped ? 1 : 2))
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
		* Context.Fit.StrokeScale * Context.AllotedGeometry.GetAccumulatedLayoutTransform().GetScale();
	VisitSplineSegments(Context, [&](const FSlateSplinePoint& Start, const FSlateSplinePoint& End, EInterpCurveMode Mode)
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
				Strip->GetVertexArray(), Strip->GetIndexArray(), nullptr, 0, 0, Context.DrawEffect);
			Strip.Reset();
		}
	};

	bool bCloseStrip = Context.Position->bIsLooped;
	VisitSplineSegments(Context, [&](const FSlateSplinePoint& Start, const FSlateSplinePoint& End, EInterpCurveMode Mode)
	{
		if (Mode == CIM_Constant)
		{
			FinishStrip(false);
			bCloseStrip = false;
			return;
		}
		if (!Strip.IsSet())
		{
			Strip.Emplace(Context.Brush->GetImageSize(), Context, Context.Fit.Scale, Context.Fit.StrokeScale);
		}
		Strip->BuildBezierGeometry(Start, End, Mode == CIM_Linear);
	});
	FinishStrip(bCloseStrip);
}
