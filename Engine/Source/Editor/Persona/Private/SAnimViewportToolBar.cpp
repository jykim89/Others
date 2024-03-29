// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "SAnimViewportToolBar.h"
#include "SAnimationEditorViewport.h"
#include "EditorViewportCommands.h"
#include "AnimViewportMenuCommands.h"
#include "AnimViewportShowCommands.h"
#include "AnimViewportLODCommands.h"
#include "AnimViewportPlaybackCommands.h"
#include "SAnimPlusMinusSlider.h"
#include "AnimationEditorViewportClient.h"
#include "Editor/UnrealEd/Public/SEditorViewportToolBarMenu.h"
#include "Editor/UnrealEd/Public/STransformViewportToolbar.h"
#include "SEditorViewportViewMenu.h"

#define LOCTEXT_NAMESPACE "AnimViewportToolBar"

//Class definition which represents widget to modify viewport's background color.
class SBackgroundColorSettings : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SBackgroundColorSettings)
		{}
		SLATE_ARGUMENT( TWeakPtr<SAnimationEditorViewportTabBody>, AnimEditorViewport )
	SLATE_END_ARGS()

	/** Constructs this widget from its declaration */
	void Construct(const FArguments& InArgs )
	{
		AnimViewportPtr = InArgs._AnimEditorViewport;

		TSharedPtr<SWidget> ExtraWidget = 
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("FilledBorder"))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(1)
				[
					SNew( SColorBlock )
					.Color(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetViewportBackgroundColor)
					.IgnoreAlpha(true)
					.ToolTipText(LOCTEXT("ColorBlock_ToolTip", "Select background color"))
					.OnMouseButtonDown(this, &SBackgroundColorSettings::OnColorBoxClicked)
				]
			];

		this->ChildSlot
		[
			SNew(SAnimPlusMinusSlider)
				.Label( LOCTEXT("BrightNess", "Brightness:")  )
				.IsEnabled( this, &SBackgroundColorSettings::IsBrightnessSliderEnabled )
				.OnMinusClicked( this, &SBackgroundColorSettings::OnDecreaseBrightness )
				.MinusTooltip( LOCTEXT("DecreaseBrightness_ToolTip", "Decrease brightness") )
				.SliderValue(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetBackgroundBrightness)
				.OnSliderValueChanged(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::SetBackgroundBrightness)
				.SliderTooltip( LOCTEXT("BackgroundBrightness_ToolTip", "Change background brightness") )
				.OnPlusClicked( this, &SBackgroundColorSettings::OnIncreaseBrightness )
				.PlusTooltip( LOCTEXT("IncreaseBrightness_ToolTip", "Increase brightness") )
				.ExtraWidget( ExtraWidget )
		];
	}

protected:

	/** Function to open color picker window when selected from context menu */
	FReply OnColorBoxClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}

		FSlateApplication::Get().DismissAllMenus();

		FLinearColor BackgroundColor = AnimViewportPtr.Pin()->GetViewportBackgroundColor();
		TArray<FLinearColor*> LinearColorArray;
		LinearColorArray.Add(&BackgroundColor);

		FColorPickerArgs PickerArgs;
		PickerArgs.bIsModal = true;
		PickerArgs.ParentWidget = AnimViewportPtr.Pin();
		PickerArgs.bOnlyRefreshOnOk = true;
		PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
		PickerArgs.LinearColorArray = &LinearColorArray;
		PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::SetViewportBackgroundColor);

		if (OpenColorPicker(PickerArgs))
		{
			AnimViewportPtr.Pin()->RefreshViewport();
		}

		return FReply::Handled();
	}

	/** Callback function for decreasing color brightness */
	FReply OnDecreaseBrightness( )
	{
		const float DeltaValue = 0.05f;
		AnimViewportPtr.Pin()->SetBackgroundBrightness( AnimViewportPtr.Pin()->GetBackgroundBrightness() - DeltaValue );
		return FReply::Handled();
	}

	/** Callback function for increasing color brightness */
	FReply OnIncreaseBrightness( )
	{
		const float DeltaValue = 0.05f;
		AnimViewportPtr.Pin()->SetBackgroundBrightness( AnimViewportPtr.Pin()->GetBackgroundBrightness() + DeltaValue );
		return FReply::Handled();
	}

	/** Callback function which determines whether this widget is enabled */
	bool IsBrightnessSliderEnabled() const
	{
		// Only enable the brightness control when we can see the background (ie, no sky or floor)
		TSharedPtr<SAnimationEditorViewportTabBody> AnimViewportPinnedPtr = AnimViewportPtr.Pin();
		return !(AnimViewportPinnedPtr->IsShowingSky() || AnimViewportPinnedPtr->IsShowingFloor());
	}

protected:

	/** The viewport hosting this widget */
	TWeakPtr<SAnimationEditorViewportTabBody> AnimViewportPtr;
};

//Class definition which represents widget to modify strength of wind for clothing
class SClothWindSettings : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SClothWindSettings)
	{}

		SLATE_ARGUMENT(TWeakPtr<SAnimationEditorViewportTabBody>, AnimEditorViewport)
	SLATE_END_ARGS()

	/** Constructs this widget from its declaration */
	void Construct(const FArguments& InArgs )
	{
		AnimViewportPtr = InArgs._AnimEditorViewport;

		TSharedPtr<SWidget> ExtraWidget = SNew( STextBlock )
			.Text(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetWindStrengthLabel)
			.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) );

		this->ChildSlot
			[
				SNew(SAnimPlusMinusSlider)
				.IsEnabled( this, &SClothWindSettings::IsWindEnabled ) 
				.Label( LOCTEXT("WindStrength", "Wind Strength:") )
				.OnMinusClicked( this, &SClothWindSettings::OnDecreaseWindStrength )
				.MinusTooltip( LOCTEXT("DecreaseWindStrength_ToolTip", "Decrease Wind Strength") )
				.SliderValue(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetWindStrengthSliderValue)
				.OnSliderValueChanged(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::SetWindStrength)
				.SliderTooltip( LOCTEXT("WindStrength_ToolTip", "Change wind strength") )
				.OnPlusClicked( this, &SClothWindSettings::OnIncreaseWindStrength )
				.PlusTooltip( LOCTEXT("IncreasetWindStrength_ToolTip", "Increase Wind Strength") )
				.ExtraWidget( ExtraWidget )
			];
	}

protected:
	/** Callback function for decreasing size */
	FReply OnDecreaseWindStrength()
	{
		const float DeltaValue = 0.1f;
		AnimViewportPtr.Pin()->SetWindStrength( AnimViewportPtr.Pin()->GetWindStrengthSliderValue() - DeltaValue );
		return FReply::Handled();
	}

	/** Callback function for increasing size */
	FReply OnIncreaseWindStrength()
	{
		const float DeltaValue = 0.1f;
		AnimViewportPtr.Pin()->SetWindStrength( AnimViewportPtr.Pin()->GetWindStrengthSliderValue() + DeltaValue );
		return FReply::Handled();
	}

	/** Callback function which determines whether this widget is enabled */
	bool IsWindEnabled() const
	{
		return AnimViewportPtr.Pin()->IsApplyingClothWind();
	}

protected:
	/** The viewport hosting this widget */
	TWeakPtr<SAnimationEditorViewportTabBody> AnimViewportPtr;
};

//Class definition which represents widget to modify gravity for preview
class SGravitySettings : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SGravitySettings)
	{}

		SLATE_ARGUMENT(TWeakPtr<SAnimationEditorViewportTabBody>, AnimEditorViewport)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs )
	{
		AnimViewportPtr = InArgs._AnimEditorViewport;

		TSharedPtr<SWidget> ExtraWidget = SNew( STextBlock )
			.Text(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetGravityScaleLabel)
			.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) );

		this->ChildSlot
			[
				SNew(SAnimPlusMinusSlider)
				.Label( LOCTEXT("Gravity Scale", "Gravity Scale Preview:")  )
				.OnMinusClicked( this, &SGravitySettings::OnDecreaseGravityScale )
				.MinusTooltip( LOCTEXT("DecreaseGravitySize_ToolTip", "Decrease Gravity Scale") )
				.SliderValue(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetGravityScaleSliderValue)
				.OnSliderValueChanged(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::SetGravityScale)
				.SliderTooltip( LOCTEXT("GravityScale_ToolTip", "Change Gravity Scale") )
				.OnPlusClicked( this, &SGravitySettings::OnIncreaseGravityScale )
				.PlusTooltip( LOCTEXT("IncreaseGravityScale_ToolTip", "Increase Gravity Scale") )
				.ExtraWidget( ExtraWidget )
			];
	}

protected:
	FReply OnDecreaseGravityScale()
	{
		const float DeltaValue = 0.025f;
		AnimViewportPtr.Pin()->SetGravityScale( AnimViewportPtr.Pin()->GetGravityScaleSliderValue() - DeltaValue );
		return FReply::Handled();
	}

	FReply OnIncreaseGravityScale()
	{
		const float DeltaValue = 0.025f;
		AnimViewportPtr.Pin()->SetGravityScale( AnimViewportPtr.Pin()->GetGravityScaleSliderValue() + DeltaValue );
		return FReply::Handled();
	}

protected:
	TWeakPtr<SAnimationEditorViewportTabBody> AnimViewportPtr;
};

///////////////////////////////////////////////////////////
// SAnimViewportToolBar

void SAnimViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<class SAnimationEditorViewportTabBody> InViewport, TSharedPtr<class SEditorViewport> InRealViewport)
{
	Viewport = InViewport;

	TSharedRef<SAnimationEditorViewportTabBody> ViewportRef = Viewport.Pin().ToSharedRef();

	TSharedRef<SHorizontalBox> LeftToolbar = SNew(SHorizontalBox)
			// Generic viewport options
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f, 2.0f )
			[
				//Menu
				SNew( SEditorViewportToolbarMenu )
				.ParentToolBar( SharedThis( this ) )
				.Image("EditorViewportToolBar.MenuDropdown")
//					.Label( this, &SAnimViewportToolBar::GetViewMenuLabel )
				.OnGetMenuContent( this, &SAnimViewportToolBar::GenerateViewMenu ) 
			]

			// Camera Type (Perspective/Top/etc...)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f, 2.0f )
			[
				SNew( SEditorViewportToolbarMenu )
				.ParentToolBar( SharedThis( this ) )
				.Label(this, &SAnimViewportToolBar::GetCameraMenuLabel)
				.LabelIcon(this, &SAnimViewportToolBar::GetCameraMenuLabelIcon)
				.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateViewportTypeMenu)
			]

			// View menu (lit, unlit, etc...)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 2.0f)
			[
				SNew( SEditorViewportViewMenu, InRealViewport.ToSharedRef(), SharedThis(this) )
			]

			// Show flags menu
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f, 2.0f )
			[
				SNew( SEditorViewportToolbarMenu )
				.ParentToolBar( SharedThis( this ) )
				.Label( LOCTEXT("ShowMenu", "Show") )
				.OnGetMenuContent( this, &SAnimViewportToolBar::GenerateShowMenu ) 
			]

			// LOD menu
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f, 2.0f )
			[
				//LOD
				SNew( SEditorViewportToolbarMenu )
				.ParentToolBar( SharedThis( this ) )
				.Label( this, &SAnimViewportToolBar::GetLODMenuLabel )
				.OnGetMenuContent( this, &SAnimViewportToolBar::GenerateLODMenu ) 
			]

			// Playback speed menu
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f, 2.0f )
			[
				SNew( SEditorViewportToolbarMenu )
				.ParentToolBar( SharedThis( this ) )
				.Label( this, &SAnimViewportToolBar::GetPlaybackMenuLabel )
				.OnGetMenuContent( this, &SAnimViewportToolBar::GeneratePlaybackMenu ) 
			]
				
 			+SHorizontalBox::Slot()
			.Padding(3.0f, 1.0f)
			.HAlign(HAlign_Right)
			[
				SNew(STransformViewportToolBar)
				.Viewport(InRealViewport)
				.CommandList(InRealViewport->GetCommandList())
				.Visibility(this, &SAnimViewportToolBar::GetTransformToolbarVisibility)
			];
	//@TODO: Need clipping horizontal box: LeftToolbar->AddWrapButton();


	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( FEditorStyle::GetBrush("NoBorder") )
		// Color and opacity is changed based on whether or not the mouse cursor is hovering over the toolbar area
		.ColorAndOpacity( this, &SViewportToolBar::OnGetColorAndOpacity )
		.ForegroundColor( FEditorStyle::GetSlateColor("DefaultForeground") )
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.AutoHeight()
			[

				LeftToolbar
			]
			+SVerticalBox::Slot()
			.Padding(FMargin(4.0f, 3.0f, 0.0f, 0.0f))
			[
				// Display text (e.g., item being previewed)
				SNew(STextBlock)
				.Text(ViewportRef, &SAnimationEditorViewportTabBody::GetDisplayString)
				.Font(FEditorStyle::GetFontStyle(TEXT("AnimViewport.MessageFont")))
				.ShadowOffset(FVector2D(0.5f, 0.5f))
				.ShadowColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.3f))
				.ColorAndOpacity(this, &SAnimViewportToolBar::GetFontColor)
			]
		]
	];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

EVisibility SAnimViewportToolBar::GetTransformToolbarVisibility() const
{
	return Viewport.Pin()->CanUseGizmos() ? EVisibility::Visible : EVisibility::Hidden;
}

FText SAnimViewportToolBar::GetViewMenuLabel() const
{
	FText Label = LOCTEXT("ViewMenu_AutoLabel", "Menu");
	if (Viewport.IsValid())
	{
		// lock mode on
		if (Viewport.Pin()->IsPreviewModeOn(1))
		{
			Label = LOCTEXT("ViewMenu_LockLabel", "Lock");
		}
	}

	return Label;
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateViewMenu() const
{
	const FAnimViewportMenuCommands& Actions = FAnimViewportMenuCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ViewMenuBuilder( bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList() );
	{
		// View modes
		{
			ViewMenuBuilder.BeginSection("AnimViewportPreviewMode", LOCTEXT("ViewMenu_PreviewModeLabel", "Preview Mode") );
			{
				ViewMenuBuilder.AddMenuEntry( Actions.Auto );
				ViewMenuBuilder.AddMenuEntry( Actions.Lock );
			}
			ViewMenuBuilder.EndSection();

			ViewMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().CameraFollow);

			ViewMenuBuilder.BeginSection("AnimViewportPreview", LOCTEXT("ViewMenu_PreviewLabel", "Preview") );
			{
				ViewMenuBuilder.AddMenuEntry( Actions.UseInGameBound );
			}
			ViewMenuBuilder.EndSection();
		}
	}

	return ViewMenuBuilder.MakeWidget();

}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateShowMenu() const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());
	{
			ShowMenuBuilder.BeginSection("AnimViewportFOV", LOCTEXT("Viewport_FOVLabel", "Field Of View"));
			{
				const float FOVMin = 5.f;
				const float FOVMax = 170.f;

				TSharedPtr<SWidget> FOVWidget = SNew(SSpinBox<float>)
					.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
					.MinValue(FOVMin)
					.MaxValue(FOVMax)
					.Value(this, &SAnimViewportToolBar::OnGetFOVValue)
					.OnValueChanged(this, &SAnimViewportToolBar::OnFOVValueChanged)
					.OnValueCommitted(this, &SAnimViewportToolBar::OnFOVValueCommitted);

				ShowMenuBuilder.AddWidget(FOVWidget.ToSharedRef(), FText());
			}
			ShowMenuBuilder.EndSection();

			ShowMenuBuilder.BeginSection("AnimViewportAudio", LOCTEXT("Viewport_AudioLabel", "Audio"));
			{
				ShowMenuBuilder.AddMenuEntry(Actions.MuteAudio);
			}
			ShowMenuBuilder.EndSection();

			ShowMenuBuilder.BeginSection("AnimViewportMesh", LOCTEXT("ShowMenu_Actions_Mesh", "Mesh"));
			{
				ShowMenuBuilder.AddMenuEntry( Actions.ShowReferencePose );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowBound );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowPreviewMesh );
			}
			ShowMenuBuilder.EndSection();

			ShowMenuBuilder.BeginSection("AnimViewportAnimation", LOCTEXT("ShowMenu_Actions_Asset", "Asset"));
			{
				ShowMenuBuilder.AddMenuEntry( Actions.ShowRawAnimation );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowNonRetargetedAnimation );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowAdditiveBaseBones );
			}
			ShowMenuBuilder.EndSection();

			ShowMenuBuilder.BeginSection("AnimViewportPreviewBones", LOCTEXT("ShowMenu_Actions_Bones", "Hierarchy") );
			{
				ShowMenuBuilder.AddMenuEntry( Actions.ShowSockets );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowBones );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowBoneNames );
				ShowMenuBuilder.AddMenuEntry( Actions.ShowBoneWeight );
			}
			ShowMenuBuilder.EndSection();

			ShowMenuBuilder.BeginSection("AnimviewportInfo", LOCTEXT("ShowInfo_Actions_Info", "Info") );
			{
				ShowMenuBuilder.AddMenuEntry( Actions.ShowDisplayInfo );
			}
			ShowMenuBuilder.EndSection();

#if WITH_APEX_CLOTHING
			UDebugSkelMeshComponent* PreviewComp = Viewport.Pin()->GetPersona().Pin()->PreviewComponent;

			if( PreviewComp && PreviewComp->HasValidClothingActors() )
			{
				ShowMenuBuilder.AddMenuSeparator();
				ShowMenuBuilder.AddSubMenu(
					LOCTEXT("AnimViewportClothingSubMenu", "Clothing"),
					LOCTEXT("AnimViewportClothingSubMenuToolTip", "Options relating to clothing"),
					FNewMenuDelegate::CreateRaw(this, &SAnimViewportToolBar::FillShowClothingMenu));
			}
			else // if skeletal mesh has clothing assets without mapping yet or assets have only collision volumes without clothing sections, then need to show collision volumes which assets include 
			if( PreviewComp && PreviewComp->SkeletalMesh && PreviewComp->SkeletalMesh->ClothingAssets.Num() > 0)
			{				
				ShowMenuBuilder.BeginSection("AnimViewportClothingOptions", LOCTEXT("ShowMenu_Actions_Clothing", "Clothing") );
				{
					ShowMenuBuilder.AddMenuEntry( Actions.ShowClothCollisionVolumes );
				}
				ShowMenuBuilder.EndSection();
			}
#endif // #if WITH_APEX_CLOTHING

		ShowMenuBuilder.AddMenuSeparator();

		ShowMenuBuilder.AddSubMenu(
			LOCTEXT("AnimViewportSceneSubMenu", "Scene Setup"),
			LOCTEXT("AnimViewportSceneSubMenuToolTip", "Options relating to the preview scene"),
			FNewMenuDelegate::CreateRaw( this, &SAnimViewportToolBar::FillShowSceneMenu ) );

		ShowMenuBuilder.AddSubMenu(
			LOCTEXT("AnimViewportAdvancedSubMenu", "Advanced"),
			LOCTEXT("AnimViewportAdvancedSubMenuToolTip", "Advanced options"),
			FNewMenuDelegate::CreateRaw( this, &SAnimViewportToolBar::FillShowAdvancedMenu ) );
	}

	return ShowMenuBuilder.MakeWidget();

}

void SAnimViewportToolBar::FillShowSceneMenu(FMenuBuilder& MenuBuilder) const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	MenuBuilder.BeginSection("AnimViewportAccessory", LOCTEXT("Viewport_AccessoryLabel", "Accessory"));
	{
		MenuBuilder.AddMenuEntry(Actions.ToggleFloor);
		MenuBuilder.AddMenuEntry(Actions.ToggleSky);
		}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimViewportFloorOffset", LOCTEXT("Viewport_FloorOffsetLabel", "Floor Height Offset"));
	{
		TSharedPtr<SWidget> FloorOffsetWidget = SNew(SNumericEntryBox<float>)
			.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
			.Value(this, &SAnimViewportToolBar::OnGetFloorOffset)
			.OnValueChanged(this, &SAnimViewportToolBar::OnFloorOffsetChanged)
			.ToolTipText(LOCTEXT("FloorOffsetToolTip", "Height offset for the floor mesh (stored per-mesh)"));

		MenuBuilder.AddWidget(FloorOffsetWidget.ToSharedRef(), FText());
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimViewportGrid", LOCTEXT("Viewport_GridLabel", "Grid"));
	{
		MenuBuilder.AddMenuEntry(Actions.ToggleGrid);
		MenuBuilder.AddMenuEntry(Actions.HighlightOrigin);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimViewportBackground", LOCTEXT("Viewport_BackgroundLabel", "Background"));
	{
		TSharedPtr<SWidget> BackgroundColorWidget = SNew(SBackgroundColorSettings).AnimEditorViewport(Viewport);
		MenuBuilder.AddWidget(BackgroundColorWidget.ToSharedRef(), FText());
	}
	MenuBuilder.EndSection();
}

void SAnimViewportToolBar::FillShowAdvancedMenu(FMenuBuilder& MenuBuilder) const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	// Draw UVs
	MenuBuilder.BeginSection("UVVisualization", LOCTEXT("UVVisualization_Label", "UV Visualization"));
	{
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().AnimSetDrawUVs);
		MenuBuilder.AddWidget(Viewport.Pin()->UVChannelCombo.ToSharedRef(), FText());
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ShowVertex", LOCTEXT("ShowVertex_Label", "Vertex Normal Visualization"));
	{
		// Vertex debug flags
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowNormals);
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowTangents);
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowBinormals);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimViewportPreviewHierarchyLocalAxes", LOCTEXT("ShowMenu_Actions_HierarchyAxes", "Hierarchy Local Axes") );
	{
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesAll );
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesSelected );
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesNone );
	}
	MenuBuilder.EndSection();
}

void SAnimViewportToolBar::FillShowClothingMenu(FMenuBuilder& MenuBuilder) const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	MenuBuilder.BeginSection("ClothPreview", LOCTEXT("ClothPreview_Label", "Preview"));
	{
		MenuBuilder.AddMenuEntry(Actions.DisableClothSimulation);
		MenuBuilder.AddMenuEntry(Actions.ApplyClothWind);
		TSharedPtr<SWidget> WindWidget = SNew(SClothWindSettings).AnimEditorViewport(Viewport);
		MenuBuilder.AddWidget(WindWidget.ToSharedRef(), FText());
		TSharedPtr<SWidget> GravityWidget = SNew(SGravitySettings).AnimEditorViewport(Viewport);
		MenuBuilder.AddWidget(GravityWidget.ToSharedRef(), FText());
		MenuBuilder.AddMenuEntry(Actions.EnableCollisionWithAttachedClothChildren);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ClothNormalVisualization", LOCTEXT("ClothNormalVisualization_Label", "Normal Visualization"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowClothSimulationNormals);
		MenuBuilder.AddMenuEntry(Actions.ShowClothGraphicalTangents);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ClothConstraintsVisualization", LOCTEXT("ClothConstraintsVisualization_Label", "Constraints Visualization"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowClothCollisionVolumes);
		MenuBuilder.AddMenuEntry(Actions.ShowClothPhysicalMeshWire);
		MenuBuilder.AddMenuEntry(Actions.ShowClothMaxDistances);
		MenuBuilder.AddMenuEntry(Actions.ShowClothBackstop);
		MenuBuilder.AddMenuEntry(Actions.ShowClothFixedVertices);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ClothAdditionalVisualization", LOCTEXT("ClothAdditionalVisualization_Label", "Sections Display Mode"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowAllSections);
		MenuBuilder.AddMenuEntry(Actions.ShowOnlyClothSections);
		MenuBuilder.AddMenuEntry(Actions.HideOnlyClothSections);
	}
}

FText SAnimViewportToolBar::GetLODMenuLabel() const
{
	FText Label = LOCTEXT("LODMenu_AutoLabel", "LOD Auto");
	if (Viewport.IsValid())
	{
		// LOD 0
		if (Viewport.Pin()->IsLODModelSelected(SAnimationEditorViewportTabBody::LOD_0))
		{
			Label = LOCTEXT("LODMenu_LOD0Label", "LOD 0");
		}
		// LOD 1
		else if (Viewport.Pin()->IsLODModelSelected(SAnimationEditorViewportTabBody::LOD_1))
		{
			Label = LOCTEXT("LODMenu_LOD1Label", "LOD 1");
		}
		// LOD 2
		else if (Viewport.Pin()->IsLODModelSelected(SAnimationEditorViewportTabBody::LOD_2))
		{
			Label = LOCTEXT("LODMenu_LOD2Label", "LOD 2");
		}
		// LOD 3
		else if (Viewport.Pin()->IsLODModelSelected(SAnimationEditorViewportTabBody::LOD_3))
		{
			Label = LOCTEXT("LODMenu_LOD3Label", "LOD 3");
		}
	}
	return Label;
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateLODMenu() const
{
	const FAnimViewportLODCommands& Actions = FAnimViewportLODCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder( bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList() );
	{
		// LOD Models
		ShowMenuBuilder.BeginSection("AnimViewportPreviewLODs", LOCTEXT("ShowLOD_PreviewLabel", "Preview LODs") );
		{
			ShowMenuBuilder.AddMenuEntry( Actions.LODAuto );
			ShowMenuBuilder.AddMenuEntry( Actions.LOD0 );

			if( Viewport.Pin()->GetLODModelCount() > 1 )
			{
				ShowMenuBuilder.AddMenuEntry( Actions.LOD1 );
			}

			if( Viewport.Pin()->GetLODModelCount() > 2 )
			{
				ShowMenuBuilder.AddMenuEntry( Actions.LOD2 );
			}

			if( Viewport.Pin()->GetLODModelCount() > 3 )
			{
				ShowMenuBuilder.AddMenuEntry( Actions.LOD3 );
			}
		}
		ShowMenuBuilder.EndSection();

		// Commands
		ShowMenuBuilder.BeginSection("AnimViewportLODSettings");
		{
			ShowMenuBuilder.AddMenuEntry( Actions.ShowLevelOfDetailSettings);
		}
		ShowMenuBuilder.EndSection();
	}

	return ShowMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateViewportTypeMenu() const
{
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder CameraMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetViewportWidget()->GetCommandList());

	// Camera types
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Perspective);

	CameraMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", LOCTEXT("CameraTypeHeader_Ortho", "Othographic"));
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Side);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
	CameraMenuBuilder.EndSection();

	return CameraMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GeneratePlaybackMenu() const
{
	const FAnimViewportPlaybackCommands& Actions = FAnimViewportPlaybackCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;

	FMenuBuilder PlaybackMenuBuilder( bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList() );
	{
		// View modes
		{
			PlaybackMenuBuilder.BeginSection("AnimViewportPlaybackSpeed", LOCTEXT("PlaybackMenu_SpeedLabel", "Playback Speed") );
			{
				for(int i = 0; i < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++i)
				{
					PlaybackMenuBuilder.AddMenuEntry( Actions.PlaybackSpeedCommands[i] );
				}
			}
			PlaybackMenuBuilder.EndSection();
		}
	}

	return PlaybackMenuBuilder.MakeWidget();

}

FSlateColor SAnimViewportToolBar::GetFontColor() const
{
	FLinearColor FontColor;
	if (Viewport.Pin()->IsShowingSky())
	{
		FontColor = FLinearColor::Black;
	}
	else
	{
		FLinearColor BackgroundColorInHSV = Viewport.Pin()->GetViewportBackgroundColor().LinearRGBToHSV();

		// see if it's dark, if V is less than 0.2
		if ( BackgroundColorInHSV.B < 0.3f )
		{
			FontColor = FLinearColor::White;
		}
		else
		{
			FontColor = FLinearColor::Black;
		}
	}

	return FontColor;
}

FString SAnimViewportToolBar::GetPlaybackMenuLabel() const
{
	FString Label = TEXT("Error");
	if (Viewport.IsValid())
	{
		for(int i = 0; i < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++i)
		{
			if (Viewport.Pin()->IsPlaybackSpeedSelected(i))
			{
				Label = FString::Printf( (i == EAnimationPlaybackSpeeds::Quarter) ? 
										 TEXT("x%.2f") : TEXT("x%.1f"), 
										 EAnimationPlaybackSpeeds::Values[i]);
				break;
			}
		}
	}
	return Label;
}

FText SAnimViewportToolBar::GetCameraMenuLabel() const
{
	FText Label = LOCTEXT("Viewport_Default", "Camera");
	TSharedPtr< SAnimationEditorViewportTabBody > PinnedViewport(Viewport.Pin());
	if( PinnedViewport.IsValid() )
	{
		switch( PinnedViewport->GetLevelViewportClient().ViewportType )
		{
		case LVT_Perspective:
			Label = LOCTEXT("CameraMenuTitle_Perspective", "Perspective");
			break;

		case LVT_OrthoXY:
			Label = LOCTEXT("CameraMenuTitle_Top", "Top");
			break;

		case LVT_OrthoYZ:
			Label = LOCTEXT("CameraMenuTitle_Side", "Side");
			break;

		case LVT_OrthoXZ:
			Label = LOCTEXT("CameraMenuTitle_Front", "Front");
			break;
		}
	}

	return Label;
}

const FSlateBrush* SAnimViewportToolBar::GetCameraMenuLabelIcon() const
{
	FName Icon = NAME_None;
	TSharedPtr< SAnimationEditorViewportTabBody > PinnedViewport(Viewport.Pin());
	if (PinnedViewport.IsValid())
	{
		switch (PinnedViewport->GetLevelViewportClient().ViewportType)
		{
		case LVT_Perspective:
			Icon = FName("EditorViewport.Perspective");
			break;

		case LVT_OrthoXY:
			Icon = FName("EditorViewport.Top");
			break;

		case LVT_OrthoYZ:
			Icon = FName("EditorViewport.Side");
			break;

		case LVT_OrthoXZ:
			Icon = FName("EditorViewport.Front");
			break;
		}
	}

	return FEditorStyle::GetBrush(Icon);
}

float SAnimViewportToolBar::OnGetFOVValue( ) const
{
	return Viewport.Pin()->GetLevelViewportClient().ViewFOV;
}

void SAnimViewportToolBar::OnFOVValueChanged( float NewValue )
{
	bool bUpdateStoredFOV = true;
	FEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();

	// @todo Viewport Cleanup
/*
	if (ViewportClient.ActorLockedToCamera.IsValid())
	{
		ACameraActor* CameraActor = Cast< ACameraActor >( ViewportClient.ActorLockedToCamera.Get() );
		if( CameraActor != NULL )
		{
			CameraActor->CameraComponent->FieldOfView = NewValue;
			bUpdateStoredFOV = false;
		}
	}*/

	if ( bUpdateStoredFOV )
	{
		ViewportClient.FOVAngle = NewValue;
		// @TODO cleanup - this interface should be in FNewAnimationViewrpotClient in the future
		// update config
		FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)(ViewportClient);
		AnimViewportClient.ConfigOption->SetViewFOV(NewValue);
	}

	ViewportClient.ViewFOV = NewValue;
	ViewportClient.Invalidate();
}

void SAnimViewportToolBar::OnFOVValueCommitted( float NewValue, ETextCommit::Type CommitInfo )
{
	//OnFOVValueChanged will be called... nothing needed here.
}

TOptional<float> SAnimViewportToolBar::OnGetFloorOffset() const
{
	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)Viewport.Pin()->GetLevelViewportClient();

	return AnimViewportClient.GetFloorOffset();
}

void SAnimViewportToolBar::OnFloorOffsetChanged( float NewValue )
{
	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)Viewport.Pin()->GetLevelViewportClient();

	AnimViewportClient.SetFloorOffset( NewValue );
}

#undef LOCTEXT_NAMESPACE
