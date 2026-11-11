// Copyright to Kat Code Labs, SRL. All Rights Reserved.

#include "SplineWidgetDetailsCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "SplineWidget.h"
#include "SplineWidgetEditPanel.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SplineWidgetDetails"

TSharedRef<IDetailCustomization> FSplineWidgetDetailCustomization::MakeInstance()
{
	return MakeShareable(new FSplineWidgetDetailCustomization);
}

void FSplineWidgetDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();
	if (SelectedObjects.Num() != 1)
	{
		return;
	}
	
	USplineWidget* SplineWidget = nullptr;
	for (const TWeakObjectPtr<UObject> Object : SelectedObjects)
	{
		if (USplineWidget* TestSplineWidget = Cast<USplineWidget>(Object))
		{
			SplineWidget = TestSplineWidget;
			break;
		}
	}

	if (!SplineWidget)
	{
		return;
	}

	const TSharedPtr<IPropertyHandleArray> SplineArray = DetailLayout.GetProperty(
		GET_MEMBER_NAME_CHECKED(USplineWidget, Splines), USplineWidget::StaticClass())->AsArray();

	const TWeakObjectPtr<USplineWidget> Widget = SplineWidget;
	const TSharedRef<int32> EditingIndex = MakeShared<int32>(0);
	auto GetEditingIndex = [Widget, EditingIndex]()
	{
		return Widget.IsValid() && !Widget->Splines.IsEmpty() ? FMath::Clamp(*EditingIndex, 0, Widget->Splines.Num() - 1) : INDEX_NONE;
	};
	const TSharedRef<SSplineWidgetEditPanel> Panel = SNew(SSplineWidgetEditPanel)
		.SplineData_Lambda([Widget, GetEditingIndex]()
		{
			const int32 Index = GetEditingIndex();
			if (Index == INDEX_NONE)
			{
				FSlateSpline Empty;
				Empty.Points.Reset();
				return Empty;
			}
			return Widget->Splines[Index];
		})
		.Clipping(EWidgetClipping::ClipToBounds)
		.Visibility_Lambda([Widget]() { return Widget.IsValid() && !Widget->Splines.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
		.OnSplineDataChanged_Lambda([GetEditingIndex, SplineArray](const FSlateSpline& NewSplineInfo)
		{
			const int32 Index = GetEditingIndex();
			if (Index == INDEX_NONE || !SplineArray.IsValid())
			{
				return;
			}
			const TSharedPtr<IPropertyHandle> Handle = SplineArray->GetElement(Index);
			if (Handle.IsValid() && Handle->IsValidHandle())
			{
				FString TextValue;
				FSlateSpline::StaticStruct()->ExportText(TextValue, &NewSplineInfo, nullptr, nullptr, PPF_None, nullptr);
				Handle->SetValueFromFormattedString(TextValue, EPropertyValueSetFlags::NotTransactable);
			}
		});

	// Make sure the EditSpline category is right below the Appearance category
	IDetailCategoryBuilder& EditSplineCategory = DetailLayout.EditCategory("SplineWidget", FText::GetEmpty(), ECategoryPriority::TypeSpecific);

	EditSplineCategory.AddCustomRow(LOCTEXT("SplineEditPanel", "Spline Edit Panel"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
					SNew(SHorizontalBox)
					.Visibility_Lambda([Widget]() { return Widget.IsValid() && Widget->Splines.Num() > 1 ? EVisibility::Visible : EVisibility::Collapsed; })
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
					[
						SNew(STextBlock).Text(LOCTEXT("EditSplineIndex", "Edit spline"))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SSpinBox<int32>)
						.MinValue(0)
						.MaxValue_Lambda([Widget]() -> TOptional<int32> { return Widget.IsValid() ? FMath::Max(Widget->Splines.Num() - 1, 0) : 0; })
						.Value_Lambda(GetEditingIndex)
						.OnValueChanged_Lambda([EditingIndex, Panel](int32 Index)
						{
							*EditingIndex = Index;
							Panel->ResetForSplineSelection();
						})
					]
			]
				+ SVerticalBox::Slot().AutoHeight()
				[
					Panel
				]
		];
}

#undef LOCTEXT_NAMESPACE
