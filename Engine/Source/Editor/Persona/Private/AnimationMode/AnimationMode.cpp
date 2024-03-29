// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"

#include "Persona.h"
#include "Editor/Kismet/Public/BlueprintEditorTabs.h"
#include "Editor/KismetWidgets/Public/SSingleObjectDetailsPanel.h"

#include "AnimationMode.h"
#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "PersonaAnimationMode"

/////////////////////////////////////////////////////
// SAnimAssetPropertiesTabBody

class SAnimAssetPropertiesTabBody : public SSingleObjectDetailsPanel
{
public:
	SLATE_BEGIN_ARGS(SAnimAssetPropertiesTabBody) {}
	SLATE_END_ARGS()

private:
	// Pointer back to owning Persona instance (the keeper of state)
	TWeakPtr<class FPersona> PersonaPtr;
public:
	void Construct(const FArguments& InArgs, TSharedPtr<FPersona> InPersona)
	{
		PersonaPtr = InPersona;

		SSingleObjectDetailsPanel::Construct(SSingleObjectDetailsPanel::FArguments());
	}

	virtual EVisibility GetAssetDisplayNameVisibility() const
	{
		return (GetObjectToObserve() != NULL) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	virtual FString GetAssetDisplayName() const
	{
		if (UObject* Object = GetObjectToObserve())
		{
			return Object->GetName();
		}
		else
		{
			return FString();
		}
	}

	// SSingleObjectDetailsPanel interface
	virtual UObject* GetObjectToObserve() const OVERRIDE
	{
		return PersonaPtr.Pin()->GetAnimationAssetBeingEdited();
	}

	virtual TSharedRef<SWidget> PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget) OVERRIDE
	{
		return SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				// Header, shows name of the blend space we are editing
				SNew(SBorder)
				.BorderImage( FEditorStyle::GetBrush( TEXT("Graph.TitleBackground") ) )
				.HAlign(HAlign_Center)
				.Visibility(this, &SAnimAssetPropertiesTabBody::GetAssetDisplayNameVisibility)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font( FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14 ) )
						.ColorAndOpacity( FLinearColor(1,1,1,0.5) )
						.Text( this, &SAnimAssetPropertiesTabBody::GetAssetDisplayName)
					]
				]
			]

			+SVerticalBox::Slot()
			.FillHeight(1)
			[
				PropertyEditorWidget
			];
	}
	// End of SSingleObjectDetailsPanel interface
};

/////////////////////////////////////////////////////
// FAnimAssetPropertiesSummoner

struct FAnimAssetPropertiesSummoner : public FWorkflowTabFactory
{
	FAnimAssetPropertiesSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	// FWorkflowTabFactory interface
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const OVERRIDE;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const OVERRIDE
	{
		return  IDocumentation::Get()->CreateToolTip(LOCTEXT("AnimAssetPropertiesTooltip", "The Anim Asset Details tab lets you edit properties of the selection animation asset (animation, blend space etc)."), NULL, TEXT("Shared/Editors/Persona"), TEXT("AnimationAssetDetail_Window"));
	}
	// FWorkflowTabFactory interface
};

FAnimAssetPropertiesSummoner::FAnimAssetPropertiesSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FPersonaTabs::AnimAssetPropertiesID, InHostingApp)
{
	TabLabel = LOCTEXT("AnimAssetProperties_TabTitle", "Anim Asset Details");
	//TabIcon = FEditorStyle::GetBrush("LevelEditor.Tabs.ContentBrowser");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("AnimAssetProperties_MenuTitle", "Anim Asset Details");
	ViewMenuTooltip = LOCTEXT("AnimAssetProperties_MenuToolTip", "Shows the animation asset properties");
}

TSharedRef<SWidget> FAnimAssetPropertiesSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FPersona> PersonaApp = StaticCastSharedPtr<FPersona>(HostingApp.Pin());

	return SNew(SOverlay)
		+SOverlay::Slot()
		[
			SNew(SAnimAssetPropertiesTabBody, PersonaApp)
		]
		+SOverlay::Slot()
		[
			SNew(SAnimDifferentAssetBeingPreviewedWarning, PersonaApp)
		];
}

/////////////////////////////////////////////////////
// FAnimEditAppMode

FAnimEditAppMode::FAnimEditAppMode(TSharedPtr<FPersona> InPersona)
	: FPersonaAppMode(InPersona, FPersonaModes::AnimationEditMode)
{
	PersonaTabFactories.RegisterFactory(MakeShareable(new FSelectionDetailsSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FAnimAssetPropertiesSummoner(InPersona)));

	TabLayout = FTabManager::NewLayout("Persona_AnimEditMode_Layout_v6")
		->AddArea
		(
			FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
			->Split
			(
				// Top toolbar area
				FTabManager::NewStack()
				->SetSizeCoefficient(0.186721f)
				->SetHideTabWell(true)
				->AddTab( InPersona->GetToolbarTabId(), ETabState::OpenedTab )
			)
			->Split
			(
				// Rest of screen
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					// Left 1/3rd - Skeleton and Anim properties
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->AddTab( FPersonaTabs::SkeletonTreeViewID, ETabState::OpenedTab )
					)
					->Split
					(
						FTabManager::NewStack()
						->AddTab( FPersonaTabs::AnimAssetPropertiesID, ETabState::OpenedTab )
					)
				)
				->Split
				(
					// Middle 1/3rd - Viewport and anim document area
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.75f)
						->SetHideTabWell(true)
						->AddTab( FPersonaTabs::PreviewViewportID, ETabState::OpenedTab )
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.25f)
						->AddTab( "Document", ETabState::ClosedTab )
					)
				)
				->Split
				(
					// Right 1/3rd - Details panel and quick browser
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->AddTab( FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab )
					)
					->Split
					(
						FTabManager::NewStack()
						->AddTab( FPersonaTabs::AssetBrowserID, ETabState::OpenedTab )
					)
				)
			)
		);
}

/////////////////////////////////////////////////////
// SAnimDifferentAssetBeingPreviewedWarning

void SAnimDifferentAssetBeingPreviewedWarning::Construct(const FArguments& InArgs, TSharedPtr<FPersona> InPersona)
{
	PersonaPtr = InPersona;

	FSlateFontInfo BoldFont = FEditorStyle::GetFontStyle(TEXT("NotificationList.FontBold"));

	Visibility = EVisibility::HitTestInvisible;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(20.0f)
		.Visibility(this, &SAnimDifferentAssetBeingPreviewedWarning::GetVisibility)
		.BorderImage(FEditorStyle::GetBrush("NotificationList.ItemBackground"))
		.BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.62f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(BoldFont)
			.Text(LOCTEXT("AssetPreviewMismatch", "Currently previewing\nanother asset"))
		]
	];
}

EVisibility SAnimDifferentAssetBeingPreviewedWarning::GetVisibility() const
{
	UObject* AssetBeingEdited = PersonaPtr.Pin()->GetAnimationAssetBeingEdited();
	UObject* AssetBeingPreviewed = PersonaPtr.Pin()->GetPreviewAnimationAsset();

	if (AssetBeingEdited != NULL)
	{
		if ((AssetBeingPreviewed != NULL) && (AssetBeingPreviewed != AssetBeingEdited))
		{
			return EVisibility::HitTestInvisible;
		}
	}
	
	return EVisibility::Collapsed;
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE