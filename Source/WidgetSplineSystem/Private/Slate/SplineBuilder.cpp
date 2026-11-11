// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#include "Slate/SplineBuilder.h"
#include "Materials/MaterialInterface.h"

FSplineBuilder::FSplineBuilder(const FVector2D& InSize, const FSlatePaintContext& PaintContext)
	: RenderTransform(PaintContext.GetRenderTransform())
	, SingleColor(PaintContext.TintColor)
	// Materials can read the original width in UV1. The stock texture shader multiplies UV0 by UV1.
	, TextureCoord2(PaintContext.Brush && Cast<UMaterialInterface>(PaintContext.Brush->GetResourceObject()) ? InSize.X : 1.0f, 1.0f)
	, CoordScaleV(InSize.Y * 0.01)
{
	HalfLineThickness = 0.5f * FMath::Max(0.0f, static_cast<float>(InSize.X));
}

void FSplineBuilder::AddVertex(const FSlateRenderTransform& Transform, FVector2f Position, float U)
{
	const FVector4f UVs(U, CurrentCoordV, TextureCoord2.X, TextureCoord2.Y);
	FSlateVertex Vertex = FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, Position, UVs,
		FVector2f(U, CurrentCoordV), SingleColor);
	Vertex.PixelSize[0] = Vertex.PixelSize[1] = 0;
	Vertices.Add(Vertex);
}

void FSplineBuilder::BuildBezierGeometry(FSlateSplinePoint SegmentStart, FSlateSplinePoint SegmentEnd, const bool bIsLinear)
{
	if (NumPointsAdded == 0)
	{
		AppendPoint(SegmentStart.Location);
	}

	VisitSegmentPoints(SegmentStart, SegmentEnd, bIsLinear, [this](const FVector2D& Point) { AppendPoint(Point); });
}

void FSplineBuilder::VisitSegmentPoints(const FSlateSplinePoint& Start, const FSlateSplinePoint& End, bool bLinear,
	TFunctionRef<void(const FVector2D&)> Visit)
{
	if (bLinear)
	{
		Visit(End.Location);
		return;
	}

	// This is the same value that Unreal inside FSlateSplinePayload::SetHermiteSpline
	constexpr float BezierControlPointScale = 3.0f;
	const FVector2D P1 = Start.Location + Start.Direction / BezierControlPointScale;
	const FVector2D P2 = End.Location - End.Direction / BezierControlPointScale;
		
	Subdivide(Start.Location, P1, P2, End.Location, Visit);
}

void FSplineBuilder::Finish(const bool bCloseLoop)
{
	if (NumPointsAdded < 2)
	{
		return;
	}
	if (NumPointsAdded == 2)
	{
		// Line builder needs at least two line segments (3 points) to
		// complete building its geometry.
		// This will only happen in the case when we have a straight line.
		AppendPoint(LastPointAdded[0]);
	}
	else
	{
		// We have added the last point, but the line builder only builds
		// geometry for the previous line segment. Build geometry for the
		// last line segment.
		const FVector2D LastUp = LastNormal * HalfLineThickness;

		CurrentLength += FVector2D::Distance(LastPointAdded[1], LastPointAdded[0]);
		CurrentCoordV = CurrentLength * CoordScaleV;

		if (bCloseLoop)
		{
			const FSlateRenderTransform& TempRenderTransform = FSlateRenderTransform(1.0f);
			AddVertex(TempRenderTransform, Vertices[0].Position, 1.0f);
			AddVertex(TempRenderTransform, Vertices[1].Position, 0.0f);
		}
		else
		{
			AddVertex(RenderTransform, FVector2f(LastPointAdded[0] + LastUp), 1.0f);
			AddVertex(RenderTransform, FVector2f(LastPointAdded[0] - LastUp), 0.0f);
		}

		const int32 NumVerts = Vertices.Num();

		// Counterclockwise winding on triangles
		Indices.Add(NumVerts - 3);
		Indices.Add(NumVerts - 4);
		Indices.Add(NumVerts - 2);

		Indices.Add(NumVerts - 3);
		Indices.Add(NumVerts - 2);
		Indices.Add(NumVerts - 1);
	}
}

void FSplineBuilder::AppendPoint(const FVector2D NewPoint)
	{
		if (NumPointsAdded == 0)
		{
			LastPointAdded[0] = LastPointAdded[1] = NewPoint;
			NumPointsAdded++;
			return;
		}
		
		// We only add vertexes for the previous line segment.
		// This is because we want to average the previous and new normals
		// In order to prevent overlapping line segments on the spline.
		// These occur especially when curvature is high.

		const FVector2D NewNormal = FVector2D(LastPointAdded[0].Y - NewPoint.Y, NewPoint.X - LastPointAdded[0].X).GetSafeNormal();

		if (NumPointsAdded == 2)
		{
			// Once we have two points, we have a normal, so we can generate the first bit of geometry.
			const FVector2D LastUp = LastNormal*HalfLineThickness;

			AddVertex(RenderTransform, FVector2f(LastPointAdded[1] + LastUp), 1.0f);
			AddVertex(RenderTransform, FVector2f(LastPointAdded[1] - LastUp), 0.0f);
		}

		if (NumPointsAdded >= 2)
		{
			const FVector2D AveragedUp = (0.5f*(NewNormal + LastNormal)).GetSafeNormal()*HalfLineThickness;

			CurrentLength += FVector2D::Distance(LastPointAdded[1], LastPointAdded[0]);
			CurrentCoordV = CurrentLength * CoordScaleV;

			AddVertex(RenderTransform, FVector2f(LastPointAdded[0] + AveragedUp), 1.0f);
			AddVertex(RenderTransform, FVector2f(LastPointAdded[0] - AveragedUp), 0.0f);

			const int32 NumVerts = Vertices.Num();

			// Counterclockwise winding on triangles
			Indices.Add(NumVerts - 3);
			Indices.Add(NumVerts - 4);
			Indices.Add(NumVerts - 2);

			Indices.Add(NumVerts - 3);
			Indices.Add(NumVerts - 2);
			Indices.Add(NumVerts - 1);
		}

		LastPointAdded[1] = LastPointAdded[0];
		LastPointAdded[0] = NewPoint;
		LastNormal = NewNormal;

		++NumPointsAdded;
	}

float FSplineBuilder::ComputeCurviness(const FVector2D P0, const FVector2D P1, const FVector2D P2, const FVector2D P3)
{
	const FVector2D TwoP1Deviations = P0 + P2 - 2 * P1;
	const FVector2D TwoP2Deviations = P1 + P3 - 2 * P2;
	return FMath::Abs(TwoP1Deviations.X) + FMath::Abs(TwoP1Deviations.Y) + FMath::Abs(TwoP2Deviations.X) + FMath::Abs(TwoP2Deviations.Y);
}

void FSplineBuilder::deCasteljauSplit(const FVector2D P0, const FVector2D P1, const FVector2D P2, const FVector2D P3,
	FVector2D OutCurveParams[7])
{
	const FVector2D L1 = (P0 + P1) * 0.5f;
	const FVector2D M = (P1 + P2) * 0.5f;
	const FVector2D R2 = (P2 + P3) * 0.5f;

	const FVector2D L2 = (L1 + M) * 0.5f;
	const FVector2D R1 = (M + R2) * 0.5f;

	const FVector2D L3R0 = (L2 + R1) * 0.5f;

	OutCurveParams[0] = P0;
	OutCurveParams[1] = L1;
	OutCurveParams[2] = L2;
	OutCurveParams[3] = L3R0;
	OutCurveParams[4] = R1;
	OutCurveParams[5] = R2;
	OutCurveParams[6] = P3;
}

void FSplineBuilder::Subdivide(const FVector2D P0, const FVector2D P1, const FVector2D P2, const FVector2D P3,
	TFunctionRef<void(const FVector2D&)> Visit)
{
	const float Curviness = ComputeCurviness(P0, P1, P2, P3);
	if (Curviness > 1.0f)
	{
		// Split the Bezier into two curves.
		FVector2D TwoCurves[7];
		deCasteljauSplit(P0, P1, P2, P3, TwoCurves);
		// Subdivide left, then right
		Subdivide(TwoCurves[0], TwoCurves[1], TwoCurves[2], TwoCurves[3], Visit);
		Subdivide(TwoCurves[3], TwoCurves[4], TwoCurves[5], TwoCurves[6], Visit);
	}
	else
	{
		Visit(P3);
	}
}
