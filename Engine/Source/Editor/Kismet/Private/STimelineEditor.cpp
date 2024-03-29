// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "BlueprintEditorPrivatePCH.h"
#include "ScopedTransaction.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "BlueprintUtilities.h"

#include "SCurveEditor.h"

#include "STimelineEditor.h"
#include "PackageTools.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DlgPickAssetPath.h"

#define LOCTEXT_NAMESPACE "STimelineEditor"

namespace TimelineEditorHelpers
{
	FName GetTrackNameFromTimeline(UTimelineTemplate* InTimeline, TSharedPtr<FTimelineEdTrack> InTrack)
	{
		if(InTrack->TrackType == FTimelineEdTrack::TT_Event)
		{
			return InTimeline->EventTracks[InTrack->TrackIndex].TrackName;
		}
		else if(InTrack->TrackType == FTimelineEdTrack::TT_FloatInterp)
		{
			return InTimeline->FloatTracks[InTrack->TrackIndex].TrackName;
		}
		else if(InTrack->TrackType == FTimelineEdTrack::TT_VectorInterp)
		{
			return InTimeline->VectorTracks[InTrack->TrackIndex].TrackName;
		}
		else if(InTrack->TrackType == FTimelineEdTrack::TT_LinearColorInterp)
		{
			return InTimeline->LinearColorTracks[InTrack->TrackIndex].TrackName;
		}
		return NAME_None;
	}
}

//////////////////////////////////////////////////////////////////////////
// STimelineEdTrack

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STimelineEdTrack::Construct(const FArguments& InArgs, TSharedPtr<FTimelineEdTrack> InTrack, TSharedPtr<STimelineEditor> InTimelineEd)
{
	Track = InTrack;
	TimelineEdPtr = InTimelineEd;

	ResetExternalCurveInfo();

	// Get the timeline we are editing
	TSharedPtr<STimelineEditor> TimelineEd = TimelineEdPtr.Pin();
	check(TimelineEd.IsValid());
	UTimelineTemplate* TimelineObj = TimelineEd->GetTimeline();
	check(TimelineObj); // We shouldn't have any tracks if there is no track object!

	// Get a pointer to the track this widget is for
	CurveBasePtr = NULL;
	FTTTrackBase* TrackBase = NULL;
	bool bDrawCurve = true;
	if(Track->TrackType == FTimelineEdTrack::TT_Event)
	{
		check(Track->TrackIndex < TimelineObj->EventTracks.Num());
		FTTEventTrack* EventTrack = &(TimelineObj->EventTracks[Track->TrackIndex]);
		CurveBasePtr = EventTrack->CurveKeys;
		TrackBase = EventTrack;
		bDrawCurve = false;
	}
	else if(Track->TrackType == FTimelineEdTrack::TT_FloatInterp)
	{
		check(Track->TrackIndex < TimelineObj->FloatTracks.Num());
		FTTFloatTrack* FloatTrack = &(TimelineObj->FloatTracks[Track->TrackIndex]);
		CurveBasePtr = FloatTrack->CurveFloat;
		TrackBase = FloatTrack;
	}
	else if(Track->TrackType == FTimelineEdTrack::TT_VectorInterp)
	{
		check(Track->TrackIndex < TimelineObj->VectorTracks.Num());
		FTTVectorTrack* VectorTrack = &(TimelineObj->VectorTracks[Track->TrackIndex]);
		CurveBasePtr = VectorTrack->CurveVector;
		TrackBase = VectorTrack;
	}
	else if(Track->TrackType == FTimelineEdTrack::TT_LinearColorInterp)
	{
		check(Track->TrackIndex < TimelineObj->LinearColorTracks.Num());
		FTTLinearColorTrack* LinearColorTrack = &(TimelineObj->LinearColorTracks[Track->TrackIndex]);
		CurveBasePtr = LinearColorTrack->CurveLinearColor;
		TrackBase = LinearColorTrack;
	}

	if( TrackBase && TrackBase->bIsExternalCurve )
	{
		//Update track with external curve info
		UseExternalCurve( CurveBasePtr );
	}

	TSharedRef<STimelineEditor> TimelineRef = TimelineEd.ToSharedRef();
	TSharedPtr<SInlineEditableTextBlock> InlineTextBlock;

	this->ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0,0,0,0)
			[
				SNew(SBorder)
				.VAlign(VAlign_Fill)
				[
					SNew(SBox)
					.WidthOverride(128)
					.HeightOverride(32)
					[
						// Name of track
						SAssignNew(InlineTextBlock, SInlineEditableTextBlock)
							.Text(FText::FromName(TrackBase->TrackName))
							.ToolTipText(FString(TEXT("Enter track name")))
							.OnVerifyTextChanged(TimelineRef, &STimelineEditor::OnVerifyTrackNameCommit,TrackBase, this)
							.OnTextCommitted(TimelineRef, &STimelineEditor::OnTrackNameCommitted,TrackBase, this)
					]
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0,16,0,0)
			[
				SNew(SBorder)
				.VAlign(VAlign_Fill)
				[
					SNew(SBox)
					.WidthOverride(128)
					.HeightOverride(36)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(1,0)
						.MaxWidth(80)
						[
							//External curve name display box
							SNew(SEditableTextBox)
							.Text( this, &STimelineEdTrack::GetExternalCurveName )
							.ForegroundColor( FLinearColor::Black )
							.IsReadOnly(true)
							.ToolTipText(this, &STimelineEdTrack::GetExternalCurvePath )
							.MinDesiredWidth( 80 )
							.BackgroundColor( FLinearColor::White )
						]

						// Use external curve button
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(1,0)
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
							.OnClicked(this, &STimelineEdTrack::OnClickUse)
							.ContentPadding(1.f)
							.ToolTipText(NSLOCTEXT("TimelineEdTrack", "TimelineEdTrack_Use", "Use External Curve"))
							[
								SNew(SImage)
								.Image( FEditorStyle::GetBrush(TEXT("PropertyWindow.Button_Use")) )
							]
						]
						// Browse external curve button
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(1,0)
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
							.OnClicked(this, &STimelineEdTrack::OnClickBrowse)
							.ContentPadding(0)
							.ToolTipText(NSLOCTEXT("TimelineEdTrack", "TimelineEdTrack_Browse", "Browse External Curve"))
							[
								SNew(SImage)
								.Image( FEditorStyle::GetBrush(TEXT("PropertyWindow.Button_Browse")) )
							]
						]
						// Convert to internal curve button
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(1,0)
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
							.OnClicked(this, &STimelineEdTrack::OnClickClear)
							.ContentPadding(1.f)
							.ToolTipText(NSLOCTEXT("TimelineEdTrack", "TimelineEdTrack_Clear", "Convert to Internal Curve"))
							[
								SNew(SImage)
								.Image( FEditorStyle::GetBrush(TEXT("PropertyWindow.Button_Clear")) )
							]
						]
					]
				]
			]
		]
		+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(SBorder)
				.VAlign(VAlign_Fill)
				[
					SAssignNew(TrackWidget, SCurveEditor)
					.ViewMinInput(TimelineRef, &STimelineEditor::GetViewMinInput)
					.ViewMaxInput(TimelineRef, &STimelineEditor::GetViewMaxInput)
					.TimelineLength(TimelineRef, &STimelineEditor::GetTimelineLength)
					.OnSetInputViewRange(TimelineRef, &STimelineEditor::SetInputViewRange)
					.DesiredSize(TimelineRef, &STimelineEditor::GetTimelineDesiredSize)
					.DrawCurve(bDrawCurve)
					.HideUI(false)
					.OnCreateAsset(this, &STimelineEdTrack::OnCreateExternalCurve )
				]
			]
	];

	if( TrackBase )
	{
		bool bZoomToFit = false;
		if( TimelineRef->GetViewMaxInput() == 0 && TimelineRef->GetViewMinInput() == 0 )
		{
			// If the input range has not been set, zoom to fit to set it
			bZoomToFit = true;
		}

		//Inform track widget about the curve and whether it is editable or not.
		TrackWidget->SetZoomToFit(bZoomToFit);
		TrackWidget->SetCurveOwner(CurveBasePtr, !TrackBase->bIsExternalCurve);
	}

	InTrack->OnRenameRequest.BindSP(InlineTextBlock.Get(), &SInlineEditableTextBlock::EnterEditingMode);

}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FString STimelineEdTrack::CreateUniqueCurveAssetPathName()
{
	//Default path
	FString BasePath = FString(TEXT( "/Game/Unsorted" ));

	TSharedRef<STimelineEditor> TimelineRef = TimelineEdPtr.Pin().ToSharedRef();

	//Get curve name from editable text box
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	// Create a unique asset name so the user can instantly hit OK if they want to create the new asset
	FString AssetName = TimelineEditorHelpers::GetTrackNameFromTimeline(TimelineEdPtr.Pin()->GetTimeline(), Track).ToString();
	FString PackageName;
	BasePath = BasePath + TEXT("/") + AssetName;
	AssetToolsModule.Get().CreateUniqueAssetName(BasePath, TEXT(""), PackageName, AssetName);

	return PackageName;
}

void STimelineEdTrack::OnCloseCreateCurveWindow()
{
	if(AssetCreationWindow.IsValid())
	{
		//Destroy asset creation dialog
		TSharedPtr<SWindow> ParentWindow = AssetCreationWindow->GetParentWindow();
		AssetCreationWindow->RequestDestroyWindow();
		AssetCreationWindow.Reset();
	}
}

void STimelineEdTrack::OnCreateExternalCurve()
{
	UCurveBase* NewCurveAsset = CreateCurveAsset();
	if( NewCurveAsset )
	{
		//Switch internal to external curve
		SwitchToExternalCurve(NewCurveAsset);
	}
	//Close dialog once switching is complete
	OnCloseCreateCurveWindow();
}

void STimelineEdTrack::SwitchToExternalCurve(UCurveBase* AssetCurvePtr)
{
	if( AssetCurvePtr )
	{
		// Get the timeline we are editing
		TSharedPtr<STimelineEditor> TimelineEd = TimelineEdPtr.Pin();
		check(TimelineEd.IsValid());
		UTimelineTemplate* TimelineObj = TimelineEd->GetTimeline();
		check(TimelineObj); // We shouldn't have any tracks if there is no track object!

		FTTTrackBase* TrackBase = NULL;
		if(Track->TrackType == FTimelineEdTrack::TT_Event)
		{
			if(AssetCurvePtr->IsA(UCurveFloat::StaticClass()))
			{
				FTTEventTrack& NewTrack = TimelineObj->EventTracks[ Track->TrackIndex ];
				NewTrack.CurveKeys = Cast<UCurveFloat>(AssetCurvePtr);
				TrackBase = &NewTrack;
			}
		}
		else if(Track->TrackType == FTimelineEdTrack::TT_FloatInterp)
		{
			if(AssetCurvePtr->IsA(UCurveFloat::StaticClass()))
			{
				FTTFloatTrack& NewTrack = TimelineObj->FloatTracks[ Track->TrackIndex ];
				NewTrack.CurveFloat = Cast<UCurveFloat>(AssetCurvePtr);
				TrackBase = &NewTrack;
			}
		}
		else if(Track->TrackType == FTimelineEdTrack::TT_VectorInterp)
		{
			if(AssetCurvePtr->IsA(UCurveVector::StaticClass()))
			{
				FTTVectorTrack& NewTrack = TimelineObj->VectorTracks[ Track->TrackIndex ];
				NewTrack.CurveVector = Cast<UCurveVector>(AssetCurvePtr);
				TrackBase = &NewTrack;
			}
		}
		else if(Track->TrackType == FTimelineEdTrack::TT_LinearColorInterp)
		{
			if(AssetCurvePtr->IsA(UCurveLinearColor::StaticClass()))
			{
				FTTLinearColorTrack& NewTrack = TimelineObj->LinearColorTracks[ Track->TrackIndex ];
				NewTrack.CurveLinearColor = Cast<UCurveLinearColor>(AssetCurvePtr);
				TrackBase = &NewTrack;
			}
		}

		if( TrackBase )
		{
			//Flag it as using external curve
			TrackBase->bIsExternalCurve = true;
			TrackWidget->SetCurveOwner( AssetCurvePtr, false );
			CurveBasePtr = AssetCurvePtr;

			UseExternalCurve(CurveBasePtr);
		}
	}
}

void STimelineEdTrack::UseExternalCurve( UObject* AssetObj )
{
	ResetExternalCurveInfo();

	if( AssetObj )
	{
		ExternalCurveName = AssetObj->GetName();
		ExternalCurvePath =  AssetObj->GetFullName();

		int32 StringLen = ExternalCurveName.Len();

		//If string is too long, then truncate (eg. "abcdefgijklmnopq" is converted as "abcd...nopq")
		const int32 MaxAllowedLength = 12;
		if( StringLen > MaxAllowedLength )
		{
			//Take first 4 characters
			FString TruncatedStr(ExternalCurveName.Left(4));
			TruncatedStr += FString( TEXT("..."));

			//Take last 4 characters
			TruncatedStr += ExternalCurveName.Right(4);
			ExternalCurveName = TruncatedStr;
		}
	}
}


void STimelineEdTrack::UseInternalCurve( )
{
	if( CurveBasePtr )
	{
		TSharedPtr<STimelineEditor> TimelineEd = TimelineEdPtr.Pin();
		check(TimelineEd.IsValid());
		UTimelineTemplate* TimelineObj = TimelineEd->GetTimeline();
		check(TimelineObj); // We shouldn't have any tracks if there is no track object!

		FTTTrackBase* TrackBase = NULL;
		UCurveBase* CurveBase = NULL;

		if(Track->TrackType == FTimelineEdTrack::TT_Event)
		{
			FTTEventTrack& NewTrack = TimelineObj->EventTracks[ Track->TrackIndex ];

			if(NewTrack.bIsExternalCurve )
			{
				UCurveFloat* SrcCurve = NewTrack.CurveKeys;
				UCurveFloat* DestCurve = Cast<UCurveFloat>(TimelineEd->CreateNewCurve( Track->TrackType) );
				if( SrcCurve && DestCurve )
				{
					//Copy external event curve data to internal curve
					CopyCurveData( &SrcCurve->FloatCurve, &DestCurve->FloatCurve );
					NewTrack.CurveKeys = DestCurve;
					CurveBase = DestCurve;
				}
			}
			TrackBase = &NewTrack;
		}
		else if(Track->TrackType == FTimelineEdTrack::TT_FloatInterp)
		{
			FTTFloatTrack& NewTrack = TimelineObj->FloatTracks[ Track->TrackIndex ];
			if(NewTrack.bIsExternalCurve)
			{
				UCurveFloat* SrcCurve = NewTrack.CurveFloat;
				UCurveFloat* DestCurve = Cast<UCurveFloat>(TimelineEd->CreateNewCurve( Track->TrackType) );
				if( SrcCurve && DestCurve )
				{
					//Copy external float curve data to internal curve
					CopyCurveData( &SrcCurve->FloatCurve, &DestCurve->FloatCurve );
					NewTrack.CurveFloat = DestCurve;
					CurveBase = DestCurve;
				}
			}
			TrackBase = &NewTrack;
		}
		else if(Track->TrackType == FTimelineEdTrack::TT_VectorInterp)
		{
			FTTVectorTrack& NewTrack = TimelineObj->VectorTracks[ Track->TrackIndex ];
			if(NewTrack.bIsExternalCurve )
			{
				UCurveVector* SrcCurve = NewTrack.CurveVector;
				UCurveVector* DestCurve = Cast<UCurveVector>(TimelineEd->CreateNewCurve( Track->TrackType) );
				if( SrcCurve && DestCurve )
				{
					for( int32 i=0; i<3; i++ )
					{
						//Copy external vector curve data to internal curve
						CopyCurveData( &SrcCurve->FloatCurves[i], &DestCurve->FloatCurves[i] );
					}
					NewTrack.CurveVector = DestCurve;
					CurveBase = DestCurve;
				}
			}
			TrackBase = &NewTrack;
		}
		else if(Track->TrackType == FTimelineEdTrack::TT_LinearColorInterp)
		{
			FTTLinearColorTrack& NewTrack = TimelineObj->LinearColorTracks[ Track->TrackIndex ];
			if(NewTrack.bIsExternalCurve )
			{
				UCurveLinearColor* SrcCurve = NewTrack.CurveLinearColor;
				UCurveLinearColor* DestCurve = Cast<UCurveLinearColor>(TimelineEd->CreateNewCurve( Track->TrackType) );
				if( SrcCurve && DestCurve )
				{
					for( int32 i=0; i<4; i++ )
					{
						//Copy external vector curve data to internal curve
						CopyCurveData( &SrcCurve->FloatCurves[i], &DestCurve->FloatCurves[i] );
					}
					NewTrack.CurveLinearColor = DestCurve;
					CurveBase = DestCurve;
				}
			}
			TrackBase = &NewTrack;
		}

		if( TrackBase && CurveBase )
		{
			//Reset flag
			TrackBase->bIsExternalCurve = false;

			TrackWidget->SetCurveOwner( CurveBase );
			CurveBasePtr = CurveBase;

			ResetExternalCurveInfo();
		}
	}
}

FReply STimelineEdTrack::OnClickClear()
{
	UseInternalCurve();
	return FReply::Handled();
}

FReply STimelineEdTrack::OnClickUse()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	UCurveBase* SelectedObj = GEditor->GetSelectedObjects()->GetTop<UCurveBase>();
	if( SelectedObj )
	{
		SwitchToExternalCurve(SelectedObj);
	}
	return FReply::Handled();
}

FReply STimelineEdTrack::OnClickBrowse()
{
	if(CurveBasePtr)
	{
		TArray<UObject*> Objects;
		Objects.Add(CurveBasePtr);
		GEditor->SyncBrowserToObjects(Objects);
	}
	return FReply::Handled();
}

FText STimelineEdTrack::GetExternalCurveName( ) const
{
	return FText::FromString(ExternalCurveName);
}

FString STimelineEdTrack::GetExternalCurvePath( ) const
{
	return ExternalCurvePath;
}

UCurveBase* STimelineEdTrack::CreateCurveAsset()
{
	UCurveBase* AssetCurve = NULL;

	if( TrackWidget.IsValid() )
	{
		TSharedRef<SDlgPickAssetPath> NewLayerDlg = 
			SNew(SDlgPickAssetPath)
			.Title(LOCTEXT("CreateExternalCurve", "Create External Curve"))
			.DefaultAssetPath(FText::FromString(CreateUniqueCurveAssetPathName()));

		if (NewLayerDlg->ShowModal() != EAppReturnType::Cancel)
		{
			FString PackageName = NewLayerDlg->GetFullAssetPath().ToString();
			FName AssetName = FName(*NewLayerDlg->GetAssetName().ToString());

			UPackage* Package = CreatePackage(NULL, *PackageName);
			
			//Get the curve class type
			TSubclassOf<UCurveBase> CurveType;
			if( Track->TrackType == FTimelineEdTrack::TT_Event || Track->TrackType == FTimelineEdTrack::TT_FloatInterp )
			{
				CurveType = UCurveFloat::StaticClass();
			}
			else if( Track->TrackType == FTimelineEdTrack::TT_LinearColorInterp )
			{
				CurveType = UCurveLinearColor::StaticClass();
			}
			else 
			{
				CurveType = UCurveVector::StaticClass();
			}

			//Create curve object
			UObject* NewObj = TrackWidget->CreateCurveObject( CurveType, Package, AssetName );
			if( NewObj )
			{
				//Copy curve data from current curve to newly create curve
				if(  Track->TrackType == FTimelineEdTrack::TT_Event || Track->TrackType == FTimelineEdTrack::TT_FloatInterp )
				{
					UCurveFloat* DestCurve = CastChecked<UCurveFloat>(NewObj);

					AssetCurve = DestCurve;
					UCurveFloat* SourceCurve = CastChecked<UCurveFloat>(CurveBasePtr);

					if( SourceCurve && DestCurve )
					{
						CopyCurveData( &SourceCurve->FloatCurve, &DestCurve->FloatCurve );
					}

					DestCurve->bIsEventCurve = ( Track->TrackType == FTimelineEdTrack::TT_Event ) ? true : false;
				}
				else if( Track->TrackType == FTimelineEdTrack::TT_VectorInterp)
				{
					UCurveVector* DestCurve = Cast<UCurveVector>(NewObj);

					AssetCurve = DestCurve;
					UCurveVector* SrcCurve = CastChecked<UCurveVector>(CurveBasePtr);

					if( SrcCurve && DestCurve )
					{
						for( int32 i=0; i<3; i++ )
						{
							CopyCurveData( &SrcCurve->FloatCurves[i], &DestCurve->FloatCurves[i] );
						}
					}
				}
				else if( Track->TrackType == FTimelineEdTrack::TT_LinearColorInterp)
				{
					UCurveLinearColor* DestCurve = Cast<UCurveLinearColor>(NewObj);

					AssetCurve = DestCurve;
					UCurveLinearColor* SrcCurve = CastChecked<UCurveLinearColor>(CurveBasePtr);

					if( SrcCurve && DestCurve )
					{
						for( int32 i=0; i<4; i++ )
						{
							CopyCurveData( &SrcCurve->FloatCurves[i], &DestCurve->FloatCurves[i] );
						}
					}
				}

				// Set the new objects as the sole selection.
				USelection* SelectionSet = GEditor->GetSelectedObjects();
				SelectionSet->DeselectAll();
				SelectionSet->Select( NewObj );

				// Notify the asset registry
				FAssetRegistryModule::AssetCreated(NewObj);

				// Mark the package dirty...
				Package->GetOutermost()->MarkPackageDirty();
				return AssetCurve;
			}
		}
	}

	return NULL;
}


void STimelineEdTrack::CopyCurveData( const FRichCurve* SrcCurve, FRichCurve* DestCurve )
{
	if( SrcCurve && DestCurve )
	{
		for (auto It(SrcCurve->GetKeyIterator()); It; ++It)
		{
			const FRichCurveKey& Key = *It;
			FKeyHandle KeyHandle = DestCurve->AddKey(Key.Time, Key.Value);
			DestCurve->GetKey(KeyHandle) = Key;
		}
	}
}

void STimelineEdTrack::ResetExternalCurveInfo( )
{
	ExternalCurveName = FString( TEXT( "None" ) );
	ExternalCurvePath = FString( TEXT( "None" ) );
}

//////////////////////////////////////////////////////////////////////////
// STimelineEditor

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void STimelineEditor::Construct(const FArguments& InArgs, TSharedPtr<FBlueprintEditor> InKismet2, UTimelineTemplate* InTimelineObj)
{
	NewTrackPendingRename = NAME_None;

	Kismet2Ptr = InKismet2;
	TimelineObj = NULL;

	NominalTimelineDesiredHeight = 300.0f;
	TimelineDesiredSize = FVector2D(128.0f, NominalTimelineDesiredHeight);

	// Leave these uninitialized at first.  We'll zoom to fit the tracks which will set the correct values
	ViewMinInput = 0.f;
	ViewMaxInput = 0.f;

	CommandList = MakeShareable( new FUICommandList );

	CommandList->MapAction( FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &STimelineEditor::OnRequestTrackRename),
		FCanExecuteAction::CreateSP(this, &STimelineEditor::CanRenameSelectedTrack) );

	CommandList->MapAction( FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &STimelineEditor::OnDeleteSelectedTracks),
		FCanExecuteAction::CreateSP(this, &STimelineEditor::CanDeleteSelectedTracks) );

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			// Header, shows name of timeline we are editing
			SNew(SBorder)
			. BorderImage( FEditorStyle::GetBrush( TEXT("Graph.TitleBackground") ) )
			. HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding( 10,0 )
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image( FEditorStyle::GetBrush(TEXT("GraphEditor.TimelineGlyph")) )
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				. VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font( FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14 ) )
					.ColorAndOpacity( FLinearColor(1,1,1,0.5) )
					.Text( this, &STimelineEditor::GetTimelineName )
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			// Box for holding buttons
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			[
				// Add float track button
				SNew(SButton)
				.ContentPadding(FMargin(2,0))
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("TimelineEditor.AddFloatTrack"))
				]
				.ToolTipText( LOCTEXT( "AddFloatTrack", "Add Float Track" ) )
				.OnClicked( this, &STimelineEditor::CreateNewTrack, FTimelineEdTrack::TT_FloatInterp )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			[
				// Add vector track button
				SNew(SButton)
				.ContentPadding(FMargin(2,0))
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("TimelineEditor.AddVectorTrack"))
				]
				.ToolTipText( LOCTEXT( "AddVectorTrack", "Add Vector Track" ) )
				.OnClicked( this, &STimelineEditor::CreateNewTrack, FTimelineEdTrack::TT_VectorInterp )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			[
				// Add event track button
				SNew(SButton)
				.ContentPadding(FMargin(2,0))
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("TimelineEditor.AddEventTrack"))
				]
				.ToolTipText( LOCTEXT( "AddEventTrack", "Add Event Track" ) )
				.OnClicked( this, &STimelineEditor::CreateNewTrack, FTimelineEdTrack::TT_Event )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			[
				// Add color track button
				SNew(SButton)
				.ContentPadding(FMargin(2,0))
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("TimelineEditor.AddColorTrack"))
				]
				.ToolTipText( LOCTEXT( "AddColorTrack", "Add Color Track" ) )
				.OnClicked( this, &STimelineEditor::CreateNewTrack, FTimelineEdTrack::TT_LinearColorInterp )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			[
				// Add external curve asset button
				SNew(SButton)
				.ContentPadding(FMargin(2,0))
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("TimelineEditor.AddCurveAssetTrack"))
				]
				.ToolTipText( LOCTEXT( "AddExternalAsset", "Add Selected Curve Asset" ) )
				.IsEnabled( this, &STimelineEditor::IsCurveAssetSelected )
				.OnClicked( this, &STimelineEditor::CreateNewTrackFromAsset )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			.VAlign(VAlign_Center)
			[
				// Length label
				SNew(STextBlock)
				.Text( LOCTEXT( "Length", "Length" ) )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6.f)
			[
				// Length edit box
				SAssignNew(TimelineLengthEdit, SEditableTextBox)
				.Text( this, &STimelineEditor::GetLengthString )
				.OnTextCommitted( this, &STimelineEditor::OnLengthStringChanged )
				.SelectAllTextWhenFocused(true)
				.MinDesiredWidth(64)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			[
				// Use last keyframe as length check box
				SAssignNew(UseLastKeyframeCheckBox, SCheckBox)
				.IsChecked( this, &STimelineEditor::IsUseLastKeyframeChecked )
				.OnCheckStateChanged( this, &STimelineEditor::OnUseLastKeyframeChanged )
				[
					SNew(STextBlock) .Text( LOCTEXT( "UseLastKeyframe", "Use Last Keyframe?" ) )
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			[
				// Play check box
				SAssignNew(PlayCheckBox, SCheckBox)
				.IsChecked( this, &STimelineEditor::IsAutoPlayChecked )
				.OnCheckStateChanged( this, &STimelineEditor::OnAutoPlayChanged )
				[
					SNew(STextBlock) .Text( LOCTEXT( "AutoPlay", "AutoPlay" ) )
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			[
				// Loop check box
				SAssignNew(LoopCheckBox, SCheckBox)
				.IsChecked( this, &STimelineEditor::IsLoopChecked )
				.OnCheckStateChanged( this, &STimelineEditor::OnLoopChanged )
				[
					SNew(STextBlock) .Text( LOCTEXT( "Loop", "Loop" ) )
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			[
				// Replicated check box
				SAssignNew(ReplicatedCheckBox, SCheckBox)
				.IsChecked( this, &STimelineEditor::IsReplicatedChecked )
				.OnCheckStateChanged( this, &STimelineEditor::OnReplicatedChanged )
				[
					SNew(STextBlock) .Text( LOCTEXT( "Replicated", "Replicated" ) )
				]
			]
		]
		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			// The list of tracks
			SAssignNew( TrackListView, STimelineEdTrackListType )
			.ListItemsSource( &TrackList )
			.OnGenerateRow( this, &STimelineEditor::MakeTrackWidget )
			.ItemHeight( 96 )
			.OnItemScrolledIntoView(this, &STimelineEditor::OnItemScrolledIntoView)
			.OnContextMenuOpening(this, &STimelineEditor::MakeContextMenu)
			.SelectionMode(ESelectionMode::SingleToggle)
		]
	];

	TimelineObj = InTimelineObj;
	check(TimelineObj);

	// Initial call to get list built
	OnTimelineChanged();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText STimelineEditor::GetTimelineName() const
{
	if(TimelineObj != NULL)
	{
		return FText::FromString(UTimelineTemplate::TimelineTemplateNameToVariableName(TimelineObj->GetFName()));
	}
	else
	{
		return LOCTEXT( "NoTimeline", "No Timeline" );
	}
}

float STimelineEditor::GetViewMaxInput() const
{
	return ViewMaxInput;
}

float STimelineEditor::GetViewMinInput() const
{
	return ViewMinInput;
}

float STimelineEditor::GetTimelineLength() const
{
	return (TimelineObj != NULL) ? TimelineObj->TimelineLength : 0.f;
}

void STimelineEditor::SetInputViewRange(float InViewMinInput, float InViewMaxInput)
{
	ViewMaxInput = InViewMaxInput;
	ViewMinInput = InViewMinInput;
}

TSharedRef<ITableRow> STimelineEditor::MakeTrackWidget( TSharedPtr<FTimelineEdTrack> Track, const TSharedRef<STableViewBase>& OwnerTable )
{
	check( Track.IsValid() );

	return
	SNew(STableRow< TSharedPtr<FTimelineEdTrack> >, OwnerTable )
	[
		SNew(STimelineEdTrack, Track, SharedThis(this))
	];
}

FReply STimelineEditor::CreateNewTrack(FTimelineEdTrack::ETrackType Type)
{
	FName TrackName = MakeUniqueObjectName(TimelineObj, UTimelineTemplate::StaticClass(), FName(*(LOCTEXT("NewTrack_DefaultName", "NewTrack").ToString())));
		
	TSharedPtr<FBlueprintEditor> Kismet2 = Kismet2Ptr.Pin();
	UBlueprint* Blueprint = Kismet2->GetBlueprintObj();
	UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, TimelineObj);

	FText ErrorMessage;

	if(TimelineObj->IsNewTrackNameValid(TrackName))
	{
		if (TimelineNode)
		{
			const FScopedTransaction Transaction( LOCTEXT( "TimelineEditor_AddNewTrack", "Add new track" ) );

			TimelineNode->Modify();
			TimelineObj->Modify();

			NewTrackPendingRename = TrackName;
			if(Type == FTimelineEdTrack::TT_Event)
			{
				FTTEventTrack NewTrack;
				NewTrack.TrackName = TrackName;
				NewTrack.CurveKeys = NewNamedObject<UCurveFloat>(Blueprint, NAME_None, RF_Public); // Needs to be marked public so that it can be referenced from timeline instances in the level
				NewTrack.CurveKeys->bIsEventCurve = true;
				TimelineObj->EventTracks.Add(NewTrack);
			}
			else if(Type == FTimelineEdTrack::TT_FloatInterp)
			{
				FTTFloatTrack NewTrack;
				NewTrack.TrackName = TrackName;
				// @hack for using existing curve assets.  need something better!
				NewTrack.CurveFloat = FindObject<UCurveFloat>(ANY_PACKAGE, *TrackName.ToString() );
				if (NewTrack.CurveFloat == NULL)
				{
					NewTrack.CurveFloat = NewNamedObject<UCurveFloat>(Blueprint, NAME_None, RF_Public); 
				}
				TimelineObj->FloatTracks.Add(NewTrack);
			}
			else if(Type == FTimelineEdTrack::TT_VectorInterp)
			{
				FTTVectorTrack NewTrack;
				NewTrack.TrackName = TrackName;
				NewTrack.CurveVector = NewNamedObject<UCurveVector>(Blueprint, NAME_None, RF_Public); 
				TimelineObj->VectorTracks.Add(NewTrack);
			}
			else if(Type == FTimelineEdTrack::TT_LinearColorInterp)
			{
				FTTLinearColorTrack NewTrack;
				NewTrack.TrackName = TrackName;
				NewTrack.CurveLinearColor = NewNamedObject<UCurveLinearColor>(Blueprint, NAME_None, RF_Public); 
				TimelineObj->LinearColorTracks.Add(NewTrack);
			}

			// Refresh the node that owns this timeline template to get new pin
			TimelineNode->ReconstructNode();
			Kismet2->RefreshEditors();
		}
		else
		{
			// invalid node for timeline
			ErrorMessage = LOCTEXT( "InvalidTimelineNodeCreate","Failed to create track. Timeline node is invalid. Please remove timeline node." );
		}
	}
	else
	{
		//name is in use 
		FFormatNamedArguments Args;
		Args.Add( TEXT("TrackName"), FText::FromName( TrackName ) );
		ErrorMessage = FText::Format( LOCTEXT( "DupTrackName","Failed to create track. Duplicate Track name entered. \n\"{TrackName}\" is already in use" ), Args );
	}

	if (!ErrorMessage.IsEmpty())
	{
		FNotificationInfo Info(ErrorMessage);
		Info.ExpireDuration = 3.0f;
		Info.bUseLargeFont = false;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if ( Notification.IsValid() )
		{
			Notification->SetCompletionState( SNotificationItem::CS_Fail );
		}
	}

	return FReply::Handled();
}

UCurveBase* STimelineEditor::CreateNewCurve( FTimelineEdTrack::ETrackType Type )
{
	TSharedPtr<FBlueprintEditor> Kismet2 = Kismet2Ptr.Pin();
	UBlueprint* Blueprint = Kismet2->GetBlueprintObj();

	UCurveBase* NewCurve = NULL;
	if(Type == FTimelineEdTrack::TT_Event)
	{
		NewCurve = NewNamedObject<UCurveFloat>(Blueprint, NAME_None, RF_Public); 
	}
	else if(Type == FTimelineEdTrack::TT_FloatInterp)
	{
		NewCurve = NewNamedObject<UCurveFloat>(Blueprint, NAME_None, RF_Public); 
	}
	else if(Type == FTimelineEdTrack::TT_VectorInterp)
	{
		NewCurve = NewNamedObject<UCurveVector>(Blueprint, NAME_None, RF_Public);
	}
	else if(Type == FTimelineEdTrack::TT_LinearColorInterp)
	{
		NewCurve = NewNamedObject<UCurveLinearColor>(Blueprint, NAME_None, RF_Public);
	}


	return NewCurve;
}

bool STimelineEditor::CanDeleteSelectedTracks() const
{
	int32 SelectedItems = TrackListView->GetNumItemsSelected();
	return (SelectedItems == 1);
}

void STimelineEditor::OnDeleteSelectedTracks()
{
	if(TimelineObj != NULL)
	{
		TSharedPtr<FBlueprintEditor> Kismet2 = Kismet2Ptr.Pin();
		UBlueprint* Blueprint = Kismet2->GetBlueprintObj();
		UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, TimelineObj);

		TArray< TSharedPtr<FTimelineEdTrack> > SelTracks = TrackListView->GetSelectedItems();
		if(SelTracks.Num() == 1)
		{
			if (TimelineNode)
			{
				const FScopedTransaction Transaction( LOCTEXT( "TimelineEditor_DeleteTrack", "Delete track" ) );

				TimelineNode->Modify();
				TimelineObj->Modify();

				TSharedPtr<FTimelineEdTrack> SelTrack = SelTracks[0];
				if(SelTrack->TrackType == FTimelineEdTrack::TT_Event)
				{
					TimelineObj->EventTracks.RemoveAt(SelTrack->TrackIndex);
				}
				else if(SelTrack->TrackType == FTimelineEdTrack::TT_FloatInterp)
				{
					TimelineObj->FloatTracks.RemoveAt(SelTrack->TrackIndex);
				}
				else if(SelTrack->TrackType == FTimelineEdTrack::TT_VectorInterp)
				{
					TimelineObj->VectorTracks.RemoveAt(SelTrack->TrackIndex);
				}
				else if(SelTrack->TrackType == FTimelineEdTrack::TT_LinearColorInterp)
				{
					TimelineObj->LinearColorTracks.RemoveAt(SelTrack->TrackIndex);
				}

				// Refresh the node that owns this timeline template to remove pin
				TimelineNode->ReconstructNode();
				Kismet2->RefreshEditors();
			}
			else
			{
				FNotificationInfo Info( LOCTEXT( "InvalidTimelineNodeDestroy","Failed to destroy track. Timeline node is invalid. Please remove timeline node." ) );
				Info.ExpireDuration = 3.0f;
				Info.bUseLargeFont = false;
				TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
				if ( Notification.IsValid() )
				{
					Notification->SetCompletionState( SNotificationItem::CS_Fail );
				}
			}
		}
	}
}

UTimelineTemplate* STimelineEditor::GetTimeline()
{
	return TimelineObj;
}

void STimelineEditor::OnTimelineChanged()
{
	TrackList.Empty();

	TSharedPtr<FTimelineEdTrack> NewlyCreatedTrack;

	// If we have a timeline,
	if(TimelineObj != NULL)
	{
		// Iterate over tracks and create entries in the array that drives the list widget

		for(int32 i=0; i<TimelineObj->EventTracks.Num(); i++)
		{
			TSharedRef<FTimelineEdTrack> Track = FTimelineEdTrack::Make(FTimelineEdTrack::TT_Event, i);

			if(TimelineObj->EventTracks[i].TrackName == NewTrackPendingRename)
			{
				NewlyCreatedTrack = Track;
			}
			TrackList.Add(Track);
		}

		for(int32 i=0; i<TimelineObj->FloatTracks.Num(); i++)
		{
			TSharedRef<FTimelineEdTrack> Track = FTimelineEdTrack::Make(FTimelineEdTrack::TT_FloatInterp, i);
			
			if(TimelineObj->FloatTracks[i].TrackName == NewTrackPendingRename)
			{
				NewlyCreatedTrack = Track;
			}
			TrackList.Add(Track);
		}

		for(int32 i=0; i<TimelineObj->VectorTracks.Num(); i++)
		{
			TSharedRef<FTimelineEdTrack> Track = FTimelineEdTrack::Make(FTimelineEdTrack::TT_VectorInterp, i);
			
			if(TimelineObj->VectorTracks[i].TrackName == NewTrackPendingRename)
			{
				NewlyCreatedTrack = Track;
			}
			TrackList.Add(Track);
		}

		for(int32 i=0; i<TimelineObj->LinearColorTracks.Num(); i++)
		{
			TSharedRef<FTimelineEdTrack> Track = FTimelineEdTrack::Make(FTimelineEdTrack::TT_LinearColorInterp, i);
			
			if(TimelineObj->LinearColorTracks[i].TrackName == NewTrackPendingRename)
			{
				NewlyCreatedTrack = Track;
			}
			TrackList.Add(Track);
		}

	}

	TrackListView->RequestListRefresh();

	TrackListView->RequestScrollIntoView(NewlyCreatedTrack);
}

void STimelineEditor::OnItemScrolledIntoView( TSharedPtr<FTimelineEdTrack> InTrackNode, const TSharedPtr<ITableRow>& InWidget )
{
	if(NewTrackPendingRename != NAME_None)
	{
		InTrackNode->OnRenameRequest.ExecuteIfBound();
		NewTrackPendingRename = NAME_None;
	}
}

ESlateCheckBoxState::Type STimelineEditor::IsAutoPlayChecked() const
{
	return (TimelineObj && TimelineObj->bAutoPlay) ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

void STimelineEditor::OnAutoPlayChanged(ESlateCheckBoxState::Type NewType)
{
	if(TimelineObj)
	{
		TimelineObj->bAutoPlay = (NewType == ESlateCheckBoxState::Checked) ? true : false;

		// Refresh the node that owns this timeline template to cache play status
		TSharedPtr<FBlueprintEditor> Kismet2 = Kismet2Ptr.Pin();
		UBlueprint* Blueprint = Kismet2->GetBlueprintObj();
		UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, TimelineObj);
		if (TimelineNode)
		{
			TimelineNode->bAutoPlay = TimelineObj->bAutoPlay;
		}
	}
}


ESlateCheckBoxState::Type STimelineEditor::IsLoopChecked() const
{
	return (TimelineObj && TimelineObj->bLoop) ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

void STimelineEditor::OnLoopChanged(ESlateCheckBoxState::Type NewType)
{
	if(TimelineObj)
	{
		TimelineObj->bLoop = (NewType == ESlateCheckBoxState::Checked) ? true : false;

		// Refresh the node that owns this timeline template to cache play status
		TSharedPtr<FBlueprintEditor> Kismet2 = Kismet2Ptr.Pin();
		UBlueprint* Blueprint = Kismet2->GetBlueprintObj();
		UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, TimelineObj);
		if (TimelineNode)
		{
			TimelineNode->bLoop = TimelineObj->bLoop;
		}
	}
}

ESlateCheckBoxState::Type STimelineEditor::IsReplicatedChecked() const
{
	return (TimelineObj && TimelineObj->bReplicated) ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

void STimelineEditor::OnReplicatedChanged(ESlateCheckBoxState::Type NewType)
{
	if(TimelineObj)
	{
		TimelineObj->bReplicated = (NewType == ESlateCheckBoxState::Checked) ? true : false;

		// Refresh the node that owns this timeline template to cache play status
		TSharedPtr<FBlueprintEditor> Kismet2 = Kismet2Ptr.Pin();
		UBlueprint* Blueprint = Kismet2->GetBlueprintObj();
		UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, TimelineObj);
		if (TimelineNode)
		{
			TimelineNode->bReplicated = TimelineObj->bReplicated;
		}
	}
}

ESlateCheckBoxState::Type STimelineEditor::IsUseLastKeyframeChecked() const
{
	return (TimelineObj && TimelineObj->LengthMode == ETimelineLengthMode::TL_LastKeyFrame) ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

void STimelineEditor::OnUseLastKeyframeChanged(ESlateCheckBoxState::Type NewType)
{
	if(TimelineObj)
	{
		TimelineObj->LengthMode = (NewType == ESlateCheckBoxState::Checked) ? ETimelineLengthMode::TL_LastKeyFrame : ETimelineLengthMode::TL_TimelineLength;
	}
}

FText STimelineEditor::GetLengthString() const
{
	FString LengthString(TEXT("0.0"));
	if(TimelineObj != NULL)
	{
		LengthString = FString::Printf(TEXT("%.2f"), TimelineObj->TimelineLength);
	}
	return FText::FromString(LengthString);
}

void STimelineEditor::OnLengthStringChanged(const FText& NewString, ETextCommit::Type CommitInfo)
{
	bool bCommitted = (CommitInfo == ETextCommit::OnEnter) || (CommitInfo == ETextCommit::OnUserMovedFocus);
	if(TimelineObj != NULL && bCommitted)
	{
		float NewLength = FCString::Atof( *NewString.ToString() );
		if(NewLength > KINDA_SMALL_NUMBER)
		{
			TimelineObj->TimelineLength = NewLength;
		}
	}
}

bool STimelineEditor::OnVerifyTrackNameCommit(const FText& TrackName, FText& OutErrorMessage, FTTTrackBase* TrackBase, STimelineEdTrack* Track )
{
	FName RequestedName(  *TrackName.ToString() );
	bool bValid(true);

	if(TrackName.IsEmpty())
	{
		OutErrorMessage = LOCTEXT( "NameMissing_Error", "You must provide a name." );
		bValid = false;
	}
	else if(TrackBase->TrackName != RequestedName && 
		false == TimelineObj->IsNewTrackNameValid(RequestedName))
	{
		OutErrorMessage = FText::FromString(FString(TEXT("\"")) + *TrackName.ToString() + LOCTEXT( "AlreadyInUse", "\" is already in use." ).ToString());
		bValid = false;
	}

	return bValid;
}

void STimelineEditor::OnTrackNameCommitted( const FText& StringName, ETextCommit::Type /*CommitInfo*/, FTTTrackBase* TrackBase, STimelineEdTrack* Track )
{
	FName RequestedName( *StringName.ToString() );
	if( TimelineObj->IsNewTrackNameValid(RequestedName))
	{	
		TimelineObj->Modify();
		TSharedPtr<FBlueprintEditor> Kismet2 = Kismet2Ptr.Pin();
		UBlueprint* Blueprint = Kismet2->GetBlueprintObj();

		UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, TimelineObj);
		
		if (TimelineNode)
		{
			//rename pin directly to avoid breaking links
			for(TArray<UEdGraphPin*>::TIterator PinIt(TimelineNode->Pins);PinIt;++PinIt)
			{
				UEdGraphPin* Pin = *PinIt;
			
				if (Pin->PinName == TrackBase->TrackName.ToString())
				{
					Pin->Modify();
					Pin->PinName = StringName.ToString();
					break;
				}
			}

			TrackBase->TrackName = RequestedName;

			Kismet2->RefreshEditors();
			OnTimelineChanged();
		}
	}
}

bool STimelineEditor::IsCurveAssetSelected() const
{
	// Note: Cannot call GetContentBrowserSelections() during serialization and GC due to its use of FindObject()
	if(!GIsSavingPackage && !GIsGarbageCollecting)
	{
		TArray<UClass*> SelectionList;
		GEditor->GetContentBrowserSelections(SelectionList);

		for( int i=0; i<SelectionList.Num(); i++ )
		{
			UClass* Item = SelectionList[i];
			if( Item->IsChildOf(UCurveBase::StaticClass()))
			{
				return true;
			}
		}
	}
	
	return false;
}


FReply STimelineEditor::CreateNewTrackFromAsset()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
	UCurveBase* SelectedObj = GEditor->GetSelectedObjects()->GetTop<UCurveBase>();

	TSharedPtr<FBlueprintEditor> Kismet2 = Kismet2Ptr.Pin();
	UBlueprint* Blueprint = Kismet2->GetBlueprintObj();
	UK2Node_Timeline* TimelineNode = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, TimelineObj);
		
	if( SelectedObj && TimelineNode )
	{
		const FScopedTransaction Transaction( LOCTEXT( "TimelineEditor_CreateFromAsset", "Add new track from asset" ) );

		TimelineNode->Modify();
		TimelineObj->Modify();

		FString TrackName = SelectedObj->GetName();

		if(SelectedObj->IsA( UCurveFloat::StaticClass() ) )
		{
			UCurveFloat* FloatCurveObj = CastChecked<UCurveFloat>(SelectedObj);
			if( FloatCurveObj->bIsEventCurve )
			{
				FTTEventTrack NewEventTrack;
				NewEventTrack.TrackName = FName( *TrackName );
				NewEventTrack.CurveKeys = CastChecked<UCurveFloat>(SelectedObj);
				NewEventTrack.bIsExternalCurve = true;

				TimelineObj->EventTracks.Add(NewEventTrack);
			}
			else
			{
				FTTFloatTrack NewFloatTrack;
				NewFloatTrack.TrackName = FName( *TrackName );
				NewFloatTrack.CurveFloat = CastChecked<UCurveFloat>(SelectedObj);
				NewFloatTrack.bIsExternalCurve = true;

				TimelineObj->FloatTracks.Add(NewFloatTrack);
			}
		}
		else if(SelectedObj->IsA( UCurveVector::StaticClass() ))
		{
			FTTVectorTrack NewTrack;
			NewTrack.TrackName = FName( *TrackName );
			NewTrack.CurveVector = CastChecked<UCurveVector>(SelectedObj);
			NewTrack.bIsExternalCurve = true;
			TimelineObj->VectorTracks.Add(NewTrack);
		}
		else if(SelectedObj->IsA( UCurveLinearColor::StaticClass() ))
		{
			FTTLinearColorTrack NewTrack;
			NewTrack.TrackName = FName( *TrackName );
			NewTrack.CurveLinearColor = CastChecked<UCurveLinearColor>(SelectedObj);
			NewTrack.bIsExternalCurve = true;
			TimelineObj->LinearColorTracks.Add(NewTrack);
		}

		// Refresh the node that owns this timeline template to get new pin
		TimelineNode->ReconstructNode();
		Kismet2->RefreshEditors();
	}
	return FReply::Handled();
}

bool STimelineEditor::CanRenameSelectedTrack() const
{
	return TrackListView->GetNumItemsSelected() == 1;
}

void STimelineEditor::OnRequestTrackRename() const
{
	check(TrackListView->GetNumItemsSelected() == 1);

	TrackListView->GetSelectedItems()[0]->OnRenameRequest.Execute();
}

FReply STimelineEditor::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	if(CommandList->ProcessCommandBindings(InKeyboardEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

TSharedPtr< SWidget > STimelineEditor::MakeContextMenu() const
{
	// Build up the menu
	FMenuBuilder MenuBuilder( true, CommandList );
	{
		MenuBuilder.AddMenuEntry( FGenericCommands::Get().Rename );
		MenuBuilder.AddMenuEntry( FGenericCommands::Get().Delete );
	}

	{
		TSharedRef<SWidget> SizeSlider = SNew(SSlider)
			.Value(this, &STimelineEditor::GetSizeScaleValue)
			.OnValueChanged(this, &STimelineEditor::SetSizeScaleValue);

		MenuBuilder.AddWidget(SizeSlider, LOCTEXT("TimelineEditorVerticalSize", "Height"));
	}

	return MenuBuilder.MakeWidget();
}

FVector2D STimelineEditor::GetTimelineDesiredSize() const
{
	return TimelineDesiredSize;
}

void STimelineEditor::SetSizeScaleValue(float NewValue)
{
	TimelineDesiredSize.Y = NominalTimelineDesiredHeight * (1.0f + NewValue * 5.0f);
	TrackListView->RequestListRefresh();
}

float STimelineEditor::GetSizeScaleValue() const
{
	return ((TimelineDesiredSize.Y / NominalTimelineDesiredHeight) - 1.0f) / 5.0f;
}

#undef LOCTEXT_NAMESPACE
