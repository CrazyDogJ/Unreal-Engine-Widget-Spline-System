// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Slate/SSpline.h"
#include "Slate/SplineBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetSplineBrushTest, "WidgetSplineSystem.Brush.LocalWidthAndUV",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWidgetSplineBrushTest::RunTest(const FString&)
{
	const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(300, 80), FSlateLayoutTransform());
	FSlateWindowElementList Elements(nullptr);
	const FSlatePaintContext Context(Elements, Geometry, 0, ESlateDrawEffect::None, FColor::White);
	for (const FVector2D Size : { FVector2D(4, 1), FVector2D(12, 3) })
	{
		FSplineBuilder Strip(Size, Context);
		Strip.BuildBezierGeometry({ FVector2D(0, 40), FVector2D::ZeroVector }, { FVector2D(200, 40), FVector2D::ZeroVector }, true);
		Strip.Finish(false);
		const auto& Vertices = Strip.GetVertexArray();
		if (!TestEqual(TEXT("Straight strip vertex count"), Vertices.Num(), 4))
		{
			return false;
		}
		TestEqual(TEXT("Brush X sets the complete strip width"), FVector2f::Distance(Vertices[0].Position, Vertices[1].Position), float(Size.X));
		TestEqual(TEXT("Texture covers the width once"), Vertices[0].TexCoords[0] * Vertices[0].TexCoords[2], 1.0f);
		TestEqual(TEXT("Brush Y sets V for two hundred local units"), Vertices.Last().TexCoords[1], float(2.0 * Size.Y));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetSplineClickTest, "WidgetSplineSystem.Interaction.ClickIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWidgetSplineClickTest::RunTest(const FString&)
{
	FSlateSpline Path;
	Path.bIsLinear = true;
	Path.Points = { { FVector2D(1000, 4000), FVector2D::ZeroVector }, { FVector2D(11000, 4000), FVector2D::ZeroVector } };
	Path.Brush.SetImageSize(FVector2D(10, 1));
	TArray<int32> Clicks;
	const TSharedRef<SSpline> Widget = SNew(SSpline).OnSplineClicked_Lambda([&](int32 Index) { Clicks.Add(Index); });
	const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(200, 80), FSlateLayoutTransform(1.5f))
		.MakeChild(FSlateRenderTransform(FScale2D(2.0f, 0.5f), FVector2f(60, 40)), FVector2f::ZeroVector);
	const auto Screen = [&](double X, double Y) { return FVector2D(Geometry.LocalToAbsolute(FVector2D(X, Y))); };
	Widget->SetSplines(TArray<FSlateSpline>{ Path, Path });
	TestEqual(TEXT("Top designer path wins after layout and render transforms"), Widget->FindSplineAt(Geometry, Screen(100, 44)), 1);
	TestEqual(TEXT("Outside a ten-unit stroke"), Widget->FindSplineAt(Geometry, Screen(100, 46)), INDEX_NONE);

	Widget->SetImportedSplineBrush(Path.Brush);
	FSlateSplineCurves Line;
	Line.Position = FSlateSplineCurves::MakePositionCurve(Path);
	FSlateSplineCurves Gap = Line;
	Gap.Position.Points[0].InterpMode = CIM_Constant;
	Widget->SetSplineCurves(TArray<FSlateSplineCurves>{ {}, Line, Gap });
	TestEqual(TEXT("Provider order includes empty paths and a constant gap does not intercept"), Widget->FindSplineAt(Geometry, Screen(100, 40)), 1);

	const TSharedRef<SWindow> Window = SNew(SWindow)[Widget];
	FArrangedChildren Widgets(EVisibility::Visible);
	Widgets.AddWidget(FArrangedWidget(Window, Geometry));
	Widgets.AddWidget(FArrangedWidget(Widget, Geometry));
	const FWidgetPath EventPath(Window, Widgets);
	auto User = FSlateApplication::Get().GetCursorUser();
	constexpr uint32 TestPointer = 207;
	const TSet<FKey> Held = { EKeys::LeftMouseButton };
	const TSet<FKey> Released;
	auto Button = [&](FVector2D Point, const TSet<FKey>& Buttons)
	{
		return FPointerEvent(User->GetUserIndex(), TestPointer, Point, Point, Buttons, EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
	};
	const FPointerEvent Down = Button(Screen(100, 40), Held);
	TestFalse(TEXT("Blank space leaves mouse-down unhandled"), Widget->OnMouseButtonDown(Geometry, Button(Screen(100, 60), Held)).IsEventHandled());
	auto Click = [&](const FPointerEvent& Up)
	{
		const FReply Press = Widget->OnMouseButtonDown(Geometry, Down);
		if (TestTrue(TEXT("Line requests capture"), Press.GetMouseCaptor() == Widget)
			&& User->SetPointerCaptor(TestPointer, Widget, EventPath))
		{
			Widget->OnMouseButtonUp(Geometry, Up);
			User->ReleaseCapture(TestPointer);
		}
	};
	Click(Button(Screen(100, 60), Released));
	TestEqual(TEXT("Releasing off the stroke cancels the click"), Clicks.Num(), 0);
	Click(Button(Screen(100, 40), Released));
	TestTrue(TEXT("A complete click reports the source index once"), Clicks == TArray<int32>{ 1 });

	Path.bIsClosedLoop = true;
	Path.Points.Add({ FVector2D(11000, 14000), FVector2D::ZeroVector });
	Widget->SetSplines(TArray<FSlateSpline>{ Path });
	auto Fit = Widget->GetSplineFit(Geometry.GetLocalSize());
	const auto ClosingMidpoint = Geometry.LocalToAbsolute(Fit.TransformPosition(FVector2D(6000, 9000)));
	TestEqual(TEXT("Closing segment can be clicked"), Widget->FindSplineAt(Geometry, ClosingMidpoint), 0);

	Line.Position.Points[0].InterpMode = CIM_CurveUser;
	Line.Position.Points[0].LeaveTangent = FVector2D(0, 16000);
	Line.Position.Points[1].ArriveTangent = FVector2D(0, -16000);
	Widget->SetSplineCurves(Line);
	Fit = Widget->GetSplineFit(Geometry.GetLocalSize());
	const auto Arc = Geometry.LocalToAbsolute(Fit.TransformPosition(Line.Position.Eval(0.5f, FVector2D::ZeroVector)));
	TestEqual(TEXT("Cubic stroke can be clicked away from its chord"), Widget->FindSplineAt(Geometry, Arc), 0);
	return true;
}

#endif
