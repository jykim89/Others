// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "CurveStructCustomization.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "PackageTools.h"
#include "MiniCurveEditor.h"
#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "CurveStructCustomization"

const FVector2D FCurveStructCustomization::DEFAULT_WINDOW_SIZE = FVector2D(800, 500);

TSharedRef<IStructCustomization> FCurveStructCustomization::MakeInstance() 
{
	return MakeShareable( new FCurveStructCustomization );
}

FCurveStructCustomization::~FCurveStructCustomization()
{
	DestroyPopOutWindow();
}

FCurveStructCustomization::FCurveStructCustomization()
	: ViewMinInput(0.0f)
	, ViewMaxInput(5.0f)
	, RuntimeCurve(NULL)
	, Owner(NULL)
{
}

void FCurveStructCustomization::CustomizeStructHeader( TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IStructCustomizationUtils& StructCustomizationUtils )
{
	this->StructPropertyHandle = InStructPropertyHandle;

	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);

	TArray<void*> StructPtrs;
	StructPropertyHandle->AccessRawData( StructPtrs );
	check(StructPtrs.Num()!=0);

	if (StructPtrs.Num() == 1)
	{
		RuntimeCurve = reinterpret_cast<FRuntimeFloatCurve*>(StructPtrs[0]);

		if (OuterObjects.Num() == 1)
		{
			Owner = OuterObjects[0];
		}

		HeaderRow
			.NameContent()
			[
				InStructPropertyHandle->CreatePropertyNameWidget( TEXT( "" ), false )
			]
			.ValueContent()
			.MinDesiredWidth(0.f)
			.MaxDesiredWidth(0.f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.VAlign(VAlign_Fill)
					.OnMouseDoubleClick(this, &FCurveStructCustomization::OnCurvePreviewDoubleClick)
					[
						SAssignNew(CurveWidget, SCurveEditor)
						.ViewMinInput(this, &FCurveStructCustomization::GetViewMinInput)
						.ViewMaxInput(this, &FCurveStructCustomization::GetViewMaxInput)
						.TimelineLength(this, &FCurveStructCustomization::GetTimelineLength)
						.OnSetInputViewRange(this, &FCurveStructCustomization::SetInputViewRange)
						.HideUI(false)
						.DesiredSize(FVector2D(128, 128))
					]
				]
			];

		check(CurveWidget.IsValid());
		if (RuntimeCurve->ExternalCurve)
		{
			CurveWidget->SetCurveOwner(RuntimeCurve->ExternalCurve, false);
		}
		else
		{
			CurveWidget->SetCurveOwner(this);
		}
	}
	else
	{
		HeaderRow
			.NameContent()
			[
				InStructPropertyHandle->CreatePropertyNameWidget( TEXT( "" ), false )
			]
			.ValueContent()
			[
				SNew(SBorder)
				.VAlign(VAlign_Fill)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MultipleCurves", "Multiple Curves - unable to modify"))
				]
			];
	}
}

void FCurveStructCustomization::CustomizeStructChildren( TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IStructCustomizationUtils& StructCustomizationUtils )
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
	{
		TSharedPtr<IPropertyHandle> Child = StructPropertyHandle->GetChildHandle( ChildIndex );

		if( Child->GetProperty()->GetName() == TEXT("ExternalCurve") )
		{
			ExternalCurveHandle = Child;

			FSimpleDelegate OnCurveChangedDelegate = FSimpleDelegate::CreateSP( this, &FCurveStructCustomization::OnExternalCurveChanged );
			Child->SetOnPropertyValueChanged(OnCurveChangedDelegate);

			StructBuilder.AddChildContent(TEXT("ExternalCurve"))
				.NameContent()
				[
					Child->CreatePropertyNameWidget()
				]
				.ValueContent()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						[
							Child->CreatePropertyValueWidget()
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(1,0)
						[
							SNew(SButton)
							.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
							.ContentPadding(1.f)
							.ToolTipText(LOCTEXT( "ConvertInternalCurveTooltip", "Convert to Internal Curve"))
							//.ToolTipText(LOCTEXT( "ConvertInternalCurveTooltip", "Copy the external CurveFloat asset to this curve") )
							.OnClicked(this, &FCurveStructCustomization::OnConvertButtonClicked)
							.IsEnabled(this, &FCurveStructCustomization::IsConvertButtonEnabled)
							[
								SNew(SImage)
								.Image( FEditorStyle::GetBrush(TEXT("PropertyWindow.Button_Clear")) )
							]
						]
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.Text( LOCTEXT( "CreateAssetButton", "Create External Curve" ) )
							.ToolTipText(LOCTEXT( "CreateAssetTooltip", "Create a new CurveFloat asset from this curve") )
							.OnClicked(this, &FCurveStructCustomization::OnCreateButtonClicked)
							.IsEnabled(this, &FCurveStructCustomization::IsCreateButtonEnabled)
						]
						/*+SHorizontalBox::Slot()
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.Text( LOCTEXT( "ConvertAssetButton", "Convert to Internal Curve" ) )
							.ToolTipText(LOCTEXT( "CopyExternalCurveTooltip", "Copy the external CurveFloat asset to this curve") )
							.OnClicked(this, &FCurveStructCustomization::OnConvertButtonClicked)
							.IsEnabled(this, &FCurveStructCustomization::IsConvertButtonEnabled)
						]*/
					]
				];
		}
		else
		{
			StructBuilder.AddChildProperty(Child.ToSharedRef());
		}
	}
}

TArray<FRichCurveEditInfoConst> FCurveStructCustomization::GetCurves() const
{
	TArray<FRichCurveEditInfoConst> Curves;
	Curves.Add(FRichCurveEditInfoConst(&RuntimeCurve->EditorCurveData));
	return Curves;
}

TArray<FRichCurveEditInfo> FCurveStructCustomization::GetCurves()
{
	TArray<FRichCurveEditInfo> Curves;
	Curves.Add(FRichCurveEditInfo(&RuntimeCurve->EditorCurveData));
	return Curves;
}

UObject* FCurveStructCustomization::GetOwner()
{
	return Owner;
}

void FCurveStructCustomization::ModifyOwner()
{
	if (Owner)
	{
		Owner->Modify(true);
	}
}

void FCurveStructCustomization::MakeTransactional()
{
	if (Owner)
	{
		Owner->SetFlags(Owner->GetFlags() | RF_Transactional);
	}
}

float FCurveStructCustomization::GetTimelineLength() const
{
	return 0.f;
}

void FCurveStructCustomization::SetInputViewRange(float InViewMinInput, float InViewMaxInput)
{
	ViewMaxInput = InViewMaxInput;
	ViewMinInput = InViewMinInput;
}

void FCurveStructCustomization::OnExternalCurveChanged()
{
	if (RuntimeCurve)
	{
		if (RuntimeCurve->ExternalCurve)
		{
			CurveWidget->SetCurveOwner(RuntimeCurve->ExternalCurve, false);
		}
		else
		{
			CurveWidget->SetCurveOwner(this);
		}
	}
}

FReply FCurveStructCustomization::OnCreateButtonClicked()
{
	if (CurveWidget.IsValid())
	{
		FString DefaultAsset = FPackageName::GetLongPackagePath(Owner->GetOutermost()->GetName()) + TEXT("/") + Owner->GetName() + TEXT("_ExternalCurve");

		TSharedRef<SDlgPickAssetPath> NewCurveDlg = 
			SNew(SDlgPickAssetPath)
			.Title(LOCTEXT("NewCurveDialogTitle", "Choose Location for External Curve Asset"))
			.DefaultAssetPath(FText::FromString(DefaultAsset));

		if (NewCurveDlg->ShowModal() != EAppReturnType::Cancel)
		{
			FString Package(NewCurveDlg->GetFullAssetPath().ToString());
			FString Name(NewCurveDlg->GetAssetName().ToString());
			FString Group(TEXT(""));

			// Find (or create!) the desired package for this object
			UPackage* Pkg = CreatePackage(NULL, *Package);
			UPackage* OutermostPkg = Pkg->GetOutermost();

			TArray<UPackage*> TopLevelPackages;
			TopLevelPackages.Add( OutermostPkg );
			if (!PackageTools::HandleFullyLoadingPackages(TopLevelPackages, LOCTEXT("CreateANewObject", "Create a new object")))
			{
				// User aborted.
				return FReply::Handled();
			}

			if (!PromptUserIfExistingObject(Name, Package, Group, Pkg))
			{
				return FReply::Handled();
			}
			
			// PromptUserIfExistingObject may have GCed and recreated our outermost package - re-acquire it here.
			OutermostPkg = Pkg->GetOutermost();

			// Create a new asset and set it as the external curve
			FName AssetName = *Name;
			UCurveFloat* NewCurve = Cast<UCurveFloat>(CurveWidget->CreateCurveObject(UCurveFloat::StaticClass(), Pkg, AssetName));
			if( NewCurve )
			{
				// run through points of editor data and add to external curve
				CopyCurveData(&RuntimeCurve->EditorCurveData, &NewCurve->FloatCurve);

				// Set the new object as the sole selection.
				USelection* SelectionSet = GEditor->GetSelectedObjects();
				SelectionSet->DeselectAll();
				SelectionSet->Select( NewCurve );

				// Notify the asset registry
				FAssetRegistryModule::AssetCreated(NewCurve);

				// Mark the package dirty...
				OutermostPkg->MarkPackageDirty();

				// Make sure expected type of pointer passed to SetValue, so that it's not interpreted as a bool
				const UObject* NewObject = NewCurve;
				ExternalCurveHandle->SetValue(NewObject);
			}
		}
	}
	return FReply::Handled();
}

bool FCurveStructCustomization::IsCreateButtonEnabled() const
{
	return CurveWidget.IsValid() && RuntimeCurve != NULL && RuntimeCurve->ExternalCurve == NULL;
}

FReply FCurveStructCustomization::OnConvertButtonClicked()
{
	if (RuntimeCurve && RuntimeCurve->ExternalCurve)
	{
		// clear points of editor data
		RuntimeCurve->EditorCurveData.Reset();

		// run through points of external curve and add to editor data
		CopyCurveData(&RuntimeCurve->ExternalCurve->FloatCurve, &RuntimeCurve->EditorCurveData);

		// null out external curve
		const UObject* NullObject = NULL;
		ExternalCurveHandle->SetValue(NullObject);
	}
	return FReply::Handled();
}

bool FCurveStructCustomization::IsConvertButtonEnabled() const
{
	return RuntimeCurve != NULL && RuntimeCurve->ExternalCurve != NULL;
}

FReply FCurveStructCustomization::OnCurvePreviewDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (RuntimeCurve->ExternalCurve)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(RuntimeCurve->ExternalCurve);
		}
		else
		{
			DestroyPopOutWindow();

			// Determine the position of the window so that it will spawn near the mouse, but not go off the screen.
			const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
			FSlateRect Anchor(CursorPos.X, CursorPos.Y, CursorPos.X, CursorPos.Y);

			FVector2D AdjustedSummonLocation = FSlateApplication::Get().CalculatePopupWindowPosition( Anchor, FCurveStructCustomization::DEFAULT_WINDOW_SIZE, Orient_Horizontal );

			TSharedPtr<SWindow> Window = SNew(SWindow)
				.Title( FText::Format( LOCTEXT("WindowHeader", "{0} - Internal Curve Editor"), FText::FromString(StructPropertyHandle->GetPropertyDisplayName())) )
				.ClientSize( FCurveStructCustomization::DEFAULT_WINDOW_SIZE )
				.ScreenPosition(AdjustedSummonLocation)
				.AutoCenter(EAutoCenter::None)
				.SupportsMaximize(false)
				.SupportsMinimize(false)
				.SizingRule( ESizingRule::FixedSize );

			// init the mini curve editor widget
			TSharedRef<SMiniCurveEditor> MiniCurveEditor = 
				SNew(SMiniCurveEditor)
				.CurveOwner(this)
				.ParentWindow(Window);

			Window->SetContent( MiniCurveEditor );

			// Find the window of the parent widget
			FWidgetPath WidgetPath;
			FSlateApplication::Get().GeneratePathToWidgetChecked( CurveWidget.ToSharedRef(), WidgetPath );
			Window = FSlateApplication::Get().AddWindowAsNativeChild( Window.ToSharedRef(), WidgetPath.GetWindow() );

			//hold on to the window created for external use...
			CurveEditorWindow = Window;
		}
	}
	return FReply::Handled();
}

void FCurveStructCustomization::CopyCurveData( const FRichCurve* SrcCurve, FRichCurve* DestCurve )
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

void FCurveStructCustomization::DestroyPopOutWindow()
{
	if (CurveEditorWindow.IsValid())
	{
		CurveEditorWindow.Pin()->RequestDestroyWindow();
		CurveEditorWindow.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
