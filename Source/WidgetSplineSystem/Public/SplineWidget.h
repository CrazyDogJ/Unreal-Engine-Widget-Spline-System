// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Slate/SSpline.h"
#include "SplineWidget.generated.h"

UCLASS()
class WIDGETSPLINESYSTEM_API USplineWidget : public UWidget
{
	GENERATED_BODY()

public:
	USplineWidget(const FObjectInitializer& ObjectInitializer);

	virtual void Serialize(FArchive& Ar) override;
	virtual void SynchronizeProperties() override;

	/** The entire set of designer paths, in paint order. */
	UPROPERTY(EditAnywhere, BlueprintSetter=SetSplines, Category="Spline Widget")
	TArray<FSlateSpline> Splines = { FSlateSpline() };

	/** Shared brush for imported paths. ImageSize.X is the line width in Slate units at MappingScale 1. */
	UPROPERTY(EditAnywhere, BlueprintSetter=SetImportedSplineBrush, Category="Spline Widget")
	FSlateBrush ImportedSplineBrush;

	/** Requested layout size in Slate units. Zero axes are automatic. Two positive axes define a box to fit. */
	UPROPERTY(EditAnywhere, BlueprintSetter=SetPreferredSize, Category="Spline Widget|Layout", meta=(ClampMin="0"))
	FVector2D PreferredSize = FVector2D::ZeroVector;

	/** Zoom about the center of the fitted paths. 1 fits the set, 2 doubles its displayed size. */
	UPROPERTY(EditAnywhere, BlueprintSetter=SetMappingScale, Category="Spline Widget|Layout", meta=(ClampMin="0", UIMin="0.1", UIMax="4"))
	float MappingScale = 1.0f;

	UFUNCTION(BlueprintSetter, Category=Spline)
	void SetSplines(const TArray<FSlateSpline>& InSplines);

	UFUNCTION(BlueprintSetter, Category=Spline)
	void SetImportedSplineBrush(const FSlateBrush& InBrush);

	UFUNCTION(BlueprintSetter, Category="Spline Widget|Layout")
	void SetPreferredSize(FVector2D InSize);

	UFUNCTION(BlueprintSetter, Category="Spline Widget|Layout")
	void SetMappingScale(float InScale);

	/** Resume using Splines and rebuild geometry, distance tables and layout bounds. */
	UFUNCTION(BlueprintCallable, Category=Spline)
	virtual void UpdateSpline();

	void SetSplineCurves(FSlateSplineCurves InCurves);
	void SetSplineCurves(TArray<FSlateSplineCurves> InSplines);

	UFUNCTION(BlueprintCallable, Category=Spline)
	bool SetSplineFromProvider(UObject* Provider, int32 ReparamStepsPerSegment = 10);

	UFUNCTION(BlueprintPure, Category=Spline)
	int32 GetSplineCount() const;

	/** Combined bounds in source coordinates, including the stroke. An empty set returns an invalid box. */
	UFUNCTION(BlueprintPure, Category="Spline Widget|Layout")
	FBox2D GetSplineBounds() const;

	FSlateSplineFit GetSplineFit() const;
	const FSlateSplineCurves* FindSplineCurves(int32 SplineIndex) const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override
	{
		return NSLOCTEXT("Spline", "Spline", "Spline");
	}
#endif

	TSharedPtr<SSpline> SlateSpline;

private:
	UPROPERTY(Transient)
	TArray<FSlateSplineCurves> SplineCurves;

	UPROPERTY(Transient)
	bool bUsingExternalSplines = false;

#if WITH_EDITORONLY_DATA
	// Load-only property tags for assets saved before all designer paths lived in Splines.
	UPROPERTY()
	FSlateSpline SplineData_DEPRECATED;
	UPROPERTY()
	TArray<FSlateSpline> AdditionalSplines_DEPRECATED;
#endif
};
