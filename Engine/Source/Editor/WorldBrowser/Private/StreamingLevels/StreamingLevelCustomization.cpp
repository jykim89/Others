// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "WorldBrowserPrivatePCH.h"

#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "Editor/PropertyEditor/Public/PropertyEditing.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "SVectorInputBox.h"
#include "SRotatorInputBox.h"

#include "SWorldDetailsView.h"
#include "StreamingLevelCollectionModel.h"
#include "StreamingLevelCustomization.h"
#include "StreamingLevelEdMode.h"

#define LOCTEXT_NAMESPACE "WorldBrowser"

/////////////////////////////////////////////////////
// FWorldTileDetails 
FStreamingLevelCustomization::FStreamingLevelCustomization()
	: bSliderMovement(false)
{
}

TSharedRef<IDetailCustomization> FStreamingLevelCustomization::MakeInstance(TSharedRef<FStreamingLevelCollectionModel> InWorldModel)
{
	TSharedRef<FStreamingLevelCustomization> Instance = MakeShareable(new FStreamingLevelCustomization());
	Instance->WorldModel = InWorldModel;
	return Instance;
}

void FStreamingLevelCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	IDetailCategoryBuilder& LevelStreamingCategory = DetailLayoutBuilder.EditCategory("LevelStreaming");

	// Hide level transform
	TSharedPtr<IPropertyHandle> LevelTransformProperty = DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULevelStreaming, LevelTransform));
	DetailLayoutBuilder.HideProperty(LevelTransformProperty);
	LevelPositionProperty = LevelTransformProperty->GetChildHandle("Translation");
	LevelRotationProperty = LevelTransformProperty->GetChildHandle("Rotation");

	// Add Position property
	LevelStreamingCategory.AddCustomRow(LOCTEXT("Position", "Position").ToString())
		.NameContent()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("Position", "Position"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
			.MinDesiredWidth(500)
		[
			SNew(SVectorInputBox)
			.IsEnabled(this, &FStreamingLevelCustomization::LevelEditTextTransformAllowed)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.bColorAxisLabels(true)
			.X(this, &FStreamingLevelCustomization::OnGetLevelPosition, 0)
			.Y(this, &FStreamingLevelCustomization::OnGetLevelPosition, 1)
			.Z(this, &FStreamingLevelCustomization::OnGetLevelPosition, 2)
			.OnXCommitted(this, &FStreamingLevelCustomization::OnSetLevelPosition, 0)
			.OnYCommitted(this, &FStreamingLevelCustomization::OnSetLevelPosition, 1)
			.OnZCommitted(this, &FStreamingLevelCustomization::OnSetLevelPosition, 2)
		];

	
	// Add Yaw Rotation property
	LevelStreamingCategory.AddCustomRow(LOCTEXT("Rotation", "Rotation").ToString())
		.NameContent()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("Rotation", "Rotation"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
			.MinDesiredWidth(500)
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SNumericEntryBox<int32>)

				.IsEnabled(this, &FStreamingLevelCustomization::LevelEditTextTransformAllowed)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.UndeterminedString(LOCTEXT("MultipleValues", "Multiple Values"))
				//.Delta(90)
				.AllowSpin(true)
				.MinValue(0)
				.MaxValue(360 - 1)
				.MinSliderValue(0)
				.MaxSliderValue(360 - 1)
				.Value(this, &FStreamingLevelCustomization::GetLevelRotation)
				.OnValueChanged(this, &FStreamingLevelCustomization::OnSetLevelRotation)
				.OnBeginSliderMovement(this, &FStreamingLevelCustomization::OnBeginLevelRotatonSlider)
				.OnEndSliderMovement(this, &FStreamingLevelCustomization::OnEndLevelRotatonSlider)
				.LabelPadding(0)					
				.Label()
				[
					SNumericEntryBox<float>::BuildLabel(LOCTEXT("LevelRotation_Label", "Yaw"), FLinearColor::White, SNumericEntryBox<int32>::BlueLabelBackgroundColor)
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SButton )
				.Text(LOCTEXT("EditLevelTransform", "Viewport Edit"))
				.ToolTipText(LOCTEXT("EditLevelToolTip", "Edit level transform in viewport."))
				.OnClicked(this, &FStreamingLevelCustomization::OnEditLevelClicked)
				.IsEnabled(this, &FStreamingLevelCustomization::LevelViewportTransformAllowed)
			]
		];
}

void FStreamingLevelCustomization::OnSetLevelPosition( float NewValue, ETextCommit::Type CommitInfo, int32 Axis )
{
	TSharedPtr<FStreamingLevelCollectionModel> CollectionModel = WorldModel.Pin();
	if (CollectionModel.IsValid())
	{
		FLevelModelList SelectedLevels = CollectionModel->GetSelectedLevels();
		for (auto It = SelectedLevels.CreateIterator(); It; ++It)
		{
			TSharedPtr<FStreamingLevelModel> LevelModel = StaticCastSharedPtr<FStreamingLevelModel>(*It);
			if (LevelModel->IsEditable() && LevelModel->GetLevelStreaming().IsValid())
			{
				ULevelStreaming* LevelStreaming = LevelModel->GetLevelStreaming().Get();
				// Create transform with new translation
				FTransform LevelTransform = LevelStreaming->LevelTransform;
				FVector LevelTraslation = LevelTransform.GetTranslation();
				LevelTraslation[Axis] = NewValue;
				LevelTransform.SetTranslation(LevelTraslation);
				// Transform level
				FLevelUtils::SetEditorTransform(LevelStreaming, LevelTransform);
			}
		}
	}
}

TOptional<float> FStreamingLevelCustomization::OnGetLevelPosition( int32 Axis ) const
{
	float AxisVal = 0.f;
	if (LevelPositionProperty->GetChildHandle(Axis)->GetValue(AxisVal) == FPropertyAccess::Success)
	{
		return AxisVal;
	}
	
	return TOptional<float>();
}

void FStreamingLevelCustomization::OnSetLevelRotation( int32 NewValue )
{
	CachedYawValue = NewValue;
	if (bSliderMovement)
	{
		return;
	}
	
	// Apply rotation when user stops slider drag
	TSharedPtr<FStreamingLevelCollectionModel> CollectionModel = WorldModel.Pin();
	if (CollectionModel.IsValid())
	{
		FQuat NewRotaion = FRotator(0.f, (float)CachedYawValue.GetValue(), 0.f).Quaternion();
		
		FLevelModelList SelectedLevels = CollectionModel->GetSelectedLevels();
		for (auto It = SelectedLevels.CreateIterator(); It; ++It)
		{
			TSharedPtr<FStreamingLevelModel> LevelModel = StaticCastSharedPtr<FStreamingLevelModel>(*It);
			if (LevelModel->IsEditable() && LevelModel->GetLevelStreaming().IsValid())
			{
				ULevelStreaming* LevelStreaming = LevelModel->GetLevelStreaming().Get();
				FTransform LevelTransform = LevelStreaming->LevelTransform;
				if (LevelTransform.GetRotation() != NewRotaion)
				{
					LevelTransform.SetRotation(NewRotaion);
					FLevelUtils::SetEditorTransform(LevelStreaming, LevelTransform);
				}
			}
		}
	}
}

void FStreamingLevelCustomization::OnBeginLevelRotatonSlider()
{		
	CachedYawValue = GetLevelRotation();
	bSliderMovement = true;
}

void FStreamingLevelCustomization::OnEndLevelRotatonSlider( int32 NewValue )
{
	bSliderMovement = false;
	OnSetLevelRotation(NewValue);
}

TOptional<int32> FStreamingLevelCustomization::GetLevelRotation() const
{
	if (bSliderMovement)
	{
		return CachedYawValue;
	}

	// If were not using the spin box use the actual transform instead of cached as it may have changed with the view port widget
	FQuat RotQ;
	if (LevelRotationProperty->GetValue(RotQ) == FPropertyAccess::Success)
	{
		int32 YawValue = FMath::RoundToInt(RotQ.Rotator().Yaw);
		return YawValue < 0 ? (YawValue + 360) : YawValue;
	}

	return TOptional<int32>();
}

bool FStreamingLevelCustomization::LevelViewportTransformAllowed() const
{
	TSharedPtr<FStreamingLevelCollectionModel> CollectionModel = WorldModel.Pin();
	if (CollectionModel.IsValid() && CollectionModel->IsOneLevelSelected())
	{
		auto SelectedLevel = CollectionModel->GetSelectedLevels()[0];
		return SelectedLevel->IsEditable() && SelectedLevel->IsVisible();
	}

	return false;
}

bool FStreamingLevelCustomization::LevelEditTextTransformAllowed() const
{
	TSharedPtr<FStreamingLevelCollectionModel> CollectionModel = WorldModel.Pin();
	if (!CollectionModel.IsValid() || !CollectionModel->AreAnySelectedLevelsEditable())
	{
		return false;
	}
	
	auto LevelModel = StaticCastSharedPtr<FStreamingLevelModel>(CollectionModel->GetSelectedLevels()[0]);
	ULevelStreaming* LevelStreaming = LevelModel->GetLevelStreaming().Get();
	
	auto* ActiveMode = static_cast<FStreamingLevelEdMode*>(
		GEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_StreamingLevel)
		);		
	
	if (ActiveMode && ActiveMode->IsEditing(LevelStreaming) == true)
	{
		return false;
	}
	
	return CollectionModel->AreAnySelectedLevelsEditable();
}

FReply FStreamingLevelCustomization::OnEditLevelClicked() 
{
	TSharedPtr<FStreamingLevelCollectionModel> CollectionModel = WorldModel.Pin();
	if (!CollectionModel.IsValid() || !CollectionModel->AreAnySelectedLevelsEditable())
	{
		return FReply::Handled();
	}

	auto LevelModel = StaticCastSharedPtr<FStreamingLevelModel>(CollectionModel->GetSelectedLevels()[0]);
	ULevelStreaming* LevelStreaming = LevelModel->GetLevelStreaming().Get();
	
	if (!LevelStreaming)
	{
		return FReply::Handled();
	}		

	if (!GEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_StreamingLevel))
	{
		// Activate Level Mode if it was not active
		GEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_StreamingLevel);
	}
			
	auto* ActiveMode = static_cast<FStreamingLevelEdMode*>(
		GEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_StreamingLevel)
		);		
	check(ActiveMode != NULL);

	if (ActiveMode->IsEditing(LevelStreaming) == true)
	{
		// Toggle this mode off if already editing this level
		GEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_StreamingLevel);
	}
	else
	{
		// Set the level we now want to edit
		ActiveMode->SetLevel(LevelStreaming);
	}		

	return FReply::Handled();
}



#undef LOCTEXT_NAMESPACE