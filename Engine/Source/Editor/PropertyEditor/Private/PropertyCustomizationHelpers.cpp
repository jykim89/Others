// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "PropertyEditing.h"
#include "PropertyCustomizationHelpers.h"
#include "AssetThumbnail.h"
#include "AssetRegistryModule.h"
#include "SPropertyAssetPicker.h"
#include "SPropertyMenuAssetPicker.h"
#include "SPropertySceneOutliner.h"
#include "SPropertyMenuActorPicker.h"
#include "ScopedTransaction.h"
#include "SoundDefinitions.h"
#include "SAssetDropTarget.h"
#include "SPropertyEditorAsset.h"
#include "SPropertyEditorClass.h"
#include "SPropertyEditorInteractiveActorPicker.h"

#define LOCTEXT_NAMESPACE "PropertyCustomizationHelpers"

namespace PropertyCustomizationHelpers
{
	class SPropertyEditorButton : public SButton
	{
	public:

		SLATE_BEGIN_ARGS( SPropertyEditorButton ) 
			: _Text( )
			, _Image( FEditorStyle::GetBrush("Default") )
			, _IsFocusable( true )
		{}
			SLATE_TEXT_ARGUMENT( Text )
			SLATE_ARGUMENT( const FSlateBrush*, Image )
			SLATE_EVENT( FSimpleDelegate, OnClickAction )

			/** Sometimes a button should only be mouse-clickable and never keyboard focusable. */
			SLATE_ARGUMENT( bool, IsFocusable )
		SLATE_END_ARGS()

		void Construct( const FArguments& InArgs )
		{
			OnClickAction = InArgs._OnClickAction;

			SButton::FArguments ButtonArgs = SButton::FArguments()
				.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
				.OnClicked( this, &SPropertyEditorButton::OnClick )
				.ToolTipText( InArgs._Text )
				.ContentPadding( 4.0f )
				.ForegroundColor( FSlateColor::UseForeground() )
				.IsFocusable(InArgs._IsFocusable)
				[ 
					SNew( SImage )
					.Image( InArgs._Image )
					.ColorAndOpacity( FSlateColor::UseForeground() )
				]; 

			SButton::Construct( ButtonArgs );
		}


	private:
		FReply OnClick()
		{
			OnClickAction.ExecuteIfBound();
			return FReply::Handled();
		}
	private:
		FSimpleDelegate OnClickAction;
	};

	TSharedRef<SWidget> MakeAddButton( FSimpleDelegate OnAddClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return	
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "AddButtonLabel", "Add" ) )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "AddButtonToolTipText", "Adds Element") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_AddToArray") )
			.OnClickAction( OnAddClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeRemoveButton( FSimpleDelegate OnRemoveClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return	
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "RemoveButtonLabel", "Remove" ) )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "RemoveButtonToolTipText", "Removes Element") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_RemoveFromArray") )
			.OnClickAction( OnRemoveClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeEmptyButton( FSimpleDelegate OnEmptyClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "EmptyButtonLabel", "Empty" ) )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "EmptyButtonToolTipText", "Removes All Elements") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_EmptyArray") )
			.OnClickAction( OnEmptyClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeUseSelectedButton( FSimpleDelegate OnUseSelectedClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "UseButtonLabel", "Use") )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "UseButtonToolTipText", "Use Selected Asset from Content Browser") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_Use") )
			.OnClickAction( OnUseSelectedClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeDeleteButton( FSimpleDelegate OnDeleteClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "DeleteButtonLabel", "Delete") )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "DeleteButtonToolTipText", "Delete") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_Delete") )
			.OnClickAction( OnDeleteClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeClearButton( FSimpleDelegate OnClearClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "ClearButtonLabel", "Clear") )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "ClearButtonToolTipText", "Clear Path") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_Clear") )
			.OnClickAction( OnClearClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeBrowseButton( FSimpleDelegate OnFindClicked, TAttribute<FText> OptionalToolTipText, TAttribute<bool> IsEnabled )
	{
		return
			SNew( SPropertyEditorButton )
			.Text( LOCTEXT( "BrowseButtonLabel", "Browse") )
			.ToolTipText( OptionalToolTipText.Get().IsEmpty() ? LOCTEXT( "BrowseButtonToolTipText", "Browse to Asset in Content Browser") : OptionalToolTipText )
			.Image( FEditorStyle::GetBrush("PropertyWindow.Button_Browse") )
			.OnClickAction( OnFindClicked )
			.IsEnabled(IsEnabled)
			.IsFocusable( false );
	}

	TSharedRef<SWidget> MakeInsertDeleteDuplicateButton( FExecuteAction OnInsertClicked, FExecuteAction OnDeleteClicked, FExecuteAction OnDuplicateClicked )
	{	
		FMenuBuilder MenuContentBuilder( true, NULL );
		{
			FUIAction InsertAction( OnInsertClicked );
			MenuContentBuilder.AddMenuEntry( LOCTEXT( "InsertButtonLabel", "Insert"), FText::GetEmpty(), FSlateIcon(), InsertAction );

			FUIAction DeleteAction( OnDeleteClicked );
			MenuContentBuilder.AddMenuEntry( LOCTEXT( "DeleteButtonLabel", "Delete"), FText::GetEmpty(), FSlateIcon(), DeleteAction );

			FUIAction DuplicateAction( OnDuplicateClicked );
			MenuContentBuilder.AddMenuEntry( LOCTEXT( "DuplicateButtonLabel", "Duplicate"), FText::GetEmpty(), FSlateIcon(), DuplicateAction );
		}

		return
			SNew(SComboButton)
			.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
			.ContentPadding(2)
			.ForegroundColor( FSlateColor::UseForeground() )
			.HasDownArrow(true)
			.IsFocusable(false)
			.MenuContent()
			[
				MenuContentBuilder.MakeWidget()
			];
	}

	TSharedRef<SWidget> MakeAssetPickerAnchorButton( FOnGetAllowedClasses OnGetAllowedClasses, FOnAssetSelected OnAssetSelectedFromPicker )
	{
		return 
			SNew( SPropertyAssetPicker )
			.OnGetAllowedClasses( OnGetAllowedClasses )
			.OnAssetSelected( OnAssetSelectedFromPicker );
	}

	TSharedRef<SWidget> MakeAssetPickerWithMenu( UObject* const InitialObject, const bool AllowClear, const TArray<const UClass*>* const AllowedClasses, FOnShouldFilterAsset OnShouldFilterAsset, FOnAssetSelected OnSet, FSimpleDelegate OnClose )
	{
		return 
			SNew( SPropertyMenuAssetPicker )
			.InitialObject(InitialObject)
			.AllowClear(AllowClear)
			.AllowedClasses(AllowedClasses)
			.OnShouldFilterAsset(OnShouldFilterAsset)
			.OnSet(OnSet)
			.OnClose(OnClose);
	}

	TSharedRef<SWidget> MakeActorPickerAnchorButton( FOnGetActorFilters OnGetActorFilters, FOnActorSelected OnActorSelectedFromPicker )
	{
		return 
			SNew( SPropertySceneOutliner )
			.OnGetActorFilters( OnGetActorFilters )
			.OnActorSelected( OnActorSelectedFromPicker );
	}

	TSharedRef<SWidget> MakeActorPickerWithMenu( AActor* const InitialActor, const bool AllowClear, const TSharedPtr< SceneOutliner::FOutlinerFilters >& ActorFilters, FOnActorSelected OnSet, FSimpleDelegate OnClose, FSimpleDelegate OnUseSelected )
	{
		return 
			SNew( SPropertyMenuActorPicker )
			.InitialActor(InitialActor)
			.AllowClear(AllowClear)
			.ActorFilters(ActorFilters)
			.OnSet(OnSet)
			.OnClose(OnClose)
			.OnUseSelected(OnUseSelected);
	}

	TSharedRef<SWidget> MakeInteractiveActorPicker( FOnGetAllowedClasses OnGetAllowedClasses, FOnShouldFilterActor OnShouldFilterActor, FOnActorSelected OnActorSelectedFromPicker )
	{
		return 
			SNew( SPropertyEditorInteractiveActorPicker )
			.ToolTipText( LOCTEXT( "PickButtonLabel", "Pick Actor from scene").ToString() )
			.OnGetAllowedClasses( OnGetAllowedClasses )
			.OnShouldFilterActor( OnShouldFilterActor )
			.OnActorSelected( OnActorSelectedFromPicker );
	}
}

void SObjectPropertyEntryBox::Construct( const FArguments& InArgs )
{
	ObjectPath = InArgs._ObjectPath;
	OnObjectChanged = InArgs._OnObjectChanged;

	bool bDisplayThumbnail = false;

	if( InArgs._PropertyHandle.IsValid() && InArgs._PropertyHandle->IsValidHandle() )
	{
		PropertyHandle = InArgs._PropertyHandle;

		// check if the property metadata wants us to display a thumbnail
		FString DisplayThumbnailString = PropertyHandle->GetProperty()->GetMetaData(TEXT("DisplayThumbnail"));
		if(DisplayThumbnailString.Len() > 0)
		{
			bDisplayThumbnail = DisplayThumbnailString == TEXT("true");
		}

		// if being used with an object property, check the allowed class is valid for the property
		UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(PropertyHandle->GetProperty());
		if (ObjectProperty != NULL)
		{
			checkSlow(InArgs._AllowedClass->IsChildOf(ObjectProperty->PropertyClass));
		}
	}

	ChildSlot
	[	
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1)
		.VAlign(VAlign_Center)	
		[
			SAssignNew(PropertyEditorAsset, SPropertyEditorAsset)
				.ObjectPath( this, &SObjectPropertyEntryBox::OnGetObjectPath )
				.Class( InArgs._AllowedClass )
				.OnSetObject(this, &SObjectPropertyEntryBox::OnSetObject)
				.ThumbnailPool(InArgs._ThumbnailPool)
				.DisplayThumbnail(bDisplayThumbnail)
				.OnShouldFilterAsset(InArgs._OnShouldFilterAsset)
				.AllowClear(InArgs._AllowClear)
				.PropertyHandle(PropertyHandle)
		]
	];
}

FString SObjectPropertyEntryBox::OnGetObjectPath() const
{
	FString StringReference;
	if( PropertyHandle.IsValid() )
	{
		PropertyHandle->GetValueAsFormattedString( StringReference );
	}
	else
	{
		StringReference = ObjectPath.Get();
	}
	
	return StringReference;
}

void SObjectPropertyEntryBox::OnSetObject(const UObject* InObject)
{
	if( PropertyHandle.IsValid() && PropertyHandle->IsValidHandle() )
	{
		FString ObjectPathName = TEXT("None");
		if(InObject)
		{
			ObjectPathName = InObject->GetPathName();
		}

		PropertyHandle->SetValueFromFormattedString(ObjectPathName);
	}

	OnObjectChanged.ExecuteIfBound(InObject);
}

void SClassPropertyEntryBox::Construct(const FArguments& InArgs)
{
	ChildSlot
	[	
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SAssignNew(PropertyEditorClass, SPropertyEditorClass)
				.MetaClass(InArgs._MetaClass)
				.RequiredInterface(InArgs._RequiredInterface)
				.AllowAbstract(InArgs._AllowAbstract)
				.IsBlueprintBaseOnly(InArgs._IsBlueprintBaseOnly)
				.AllowNone(InArgs._AllowNone)
				.SelectedClass(InArgs._SelectedClass)
				.OnSetClass(InArgs._OnSetClass)
		]
	];
}

void SProperty::Construct( const FArguments& InArgs, TSharedPtr<IPropertyHandle> InPropertyHandle )
{
	TSharedPtr<SWidget> ChildSlotContent;

	const FString& DisplayName = InArgs._DisplayName.Get();

	PropertyHandle = InPropertyHandle;

	if( PropertyHandle->IsValidHandle() )
	{
		InPropertyHandle->MarkHiddenByCustomization();

		if( InArgs._CustomWidget.Widget != SNullWidget::NullWidget )
		{
			TSharedRef<SWidget> CustomWidget = InArgs._CustomWidget.Widget;

			// If the name should be displayed create it now
			if( InArgs._ShouldDisplayName )
			{
				CustomWidget = 
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.Padding( 4.0f, 0.0f )
					.FillWidth(1.0f)
					[
						InPropertyHandle->CreatePropertyNameWidget( DisplayName )
					]
					+ SHorizontalBox::Slot()
					.Padding( 0.0f, 0.0f )
					.VAlign(VAlign_Center)
					.FillWidth(1.0f)
					[
						CustomWidget
					];
			}

			ChildSlotContent = CustomWidget;
		}
		else
		{
			if( InArgs._ShouldDisplayName )
			{
				ChildSlotContent = 
					SNew( SHorizontalBox )
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.Padding( 3.0f, 0.0f )
					.FillWidth(1.0f)
					[
						InPropertyHandle->CreatePropertyNameWidget( DisplayName )
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.FillWidth(1.0f)
					[
						InPropertyHandle->CreatePropertyValueWidget()
					];
			}
			else
			{
				ChildSlotContent = InPropertyHandle->CreatePropertyValueWidget();
			}
		}
	}
	else
	{
		// The property was not found, just filter out this widget completely
		// Note a spacer widget is used instead of setting the visibility of this widget in the case that a user overrides the visibility of this widget
		ChildSlotContent = 
			SNew( SSpacer )
			.Visibility( EVisibility::Collapsed );
	}
	
	ChildSlot
	[
		ChildSlotContent.ToSharedRef()
	];
}

void SProperty::ResetToDefault()
{
	if( PropertyHandle->IsValidHandle() )
	{
		PropertyHandle->ResetToDefault();
	}
}

FText SProperty::GetResetToDefaultLabel() const
{
	if( PropertyHandle->IsValidHandle() )
	{
		return PropertyHandle->GetResetToDefaultLabel();
	}

	return FText();
}

bool SProperty::ShouldShowResetToDefault() const
{
	return PropertyHandle->IsValidHandle() && !PropertyHandle->IsEditConst() && PropertyHandle->DiffersFromDefault();
}

bool SProperty::IsValidProperty() const
{
	return PropertyHandle.IsValid() && PropertyHandle->IsValidHandle();
}

/**
 * Builds up a list of unique materials while creating some information about the materials
 */
class FMaterialListBuilder : public IMaterialListBuilder
{
	friend class FMaterialList;
public:

	/** 
	 * Adds a new material to the list
	 * 
	 * @param SlotIndex		The slot (usually mesh element index) where the material is located on the component
	 * @param Material		The material being used
	 * @param bCanBeReplced	Whether or not the material can be replaced by a user
	 */
	virtual void AddMaterial( uint32 SlotIndex, UMaterialInterface* Material, bool bCanBeReplaced ) OVERRIDE
	{
		int32 NumMaterials = MaterialSlots.Num();

		FMaterialListItem MaterialItem( Material, SlotIndex, bCanBeReplaced ); 
		if( !UniqueMaterials.Contains( MaterialItem ) ) 
		{
			MaterialSlots.Add( MaterialItem );
			UniqueMaterials.Add( MaterialItem );
		}

		// Did we actually add material?  If we did then we need to increment the number of materials in the element
		if( MaterialSlots.Num() > NumMaterials )
		{
			// Resize the array to support the slot if needed
			if( !MaterialCount.IsValidIndex(SlotIndex) )
			{
				int32 NumToAdd = (SlotIndex - MaterialCount.Num()) + 1;
				if( NumToAdd > 0 )
				{
					MaterialCount.AddZeroed( NumToAdd );
				}
			}

			++MaterialCount[SlotIndex];
		}
	}

	/** Empties the list */
	void Empty()
	{
		UniqueMaterials.Empty();
		MaterialSlots.Reset();
		MaterialCount.Reset();
	}

	/** Sorts the list by slot index */
	void Sort()
	{
		struct FSortByIndex
		{
			bool operator()( const FMaterialListItem& A, const FMaterialListItem& B ) const
			{
				return A.SlotIndex < B.SlotIndex;
			}
		};

		MaterialSlots.Sort( FSortByIndex() );
	}

	/** @return The number of materials in the list */
	uint32 GetNumMaterials() const { return MaterialSlots.Num(); }

	/** @return The number of materials in the list at a given slot */
	uint32 GetNumMaterialsInSlot( uint32 Index ) const { return MaterialCount[Index]; }
private:
	/** All unique materials */
	TSet<FMaterialListItem> UniqueMaterials;
	/** All material items in the list */
	TArray<FMaterialListItem> MaterialSlots;
	/** Material counts for each slot.  The slot is the index and the value at that index is the count */
	TArray<uint32> MaterialCount;
};

/**
 * A view of a single item in an FMaterialList
 */
class FMaterialItemView : public TSharedFromThis<FMaterialItemView>
{
public:
	/**
	 * Creates a new instance of this class
	 *
	 * @param Material				The material to view
	 * @param InOnMaterialChanged	Delegate for when the material changes
	 */
	static TSharedRef<FMaterialItemView> Create(
		const FMaterialListItem& Material, 
		FOnMaterialChanged InOnMaterialChanged,
		FOnGenerateWidgetsForMaterial InOnGenerateNameWidgetsForMaterial, 
		FOnGenerateWidgetsForMaterial InOnGenerateWidgetsForMaterial, 
		FOnResetMaterialToDefaultClicked InOnResetToDefaultClicked,
		int32 InMultipleMaterialCount )
	{
		return MakeShareable( new FMaterialItemView( Material, InOnMaterialChanged, InOnGenerateNameWidgetsForMaterial, InOnGenerateWidgetsForMaterial, InOnResetToDefaultClicked, InMultipleMaterialCount ) );
	}

	TSharedRef<SWidget> CreateNameContent()
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ElementIndex"), MaterialItem.SlotIndex);

		return 
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew( STextBlock )
				.Font( IDetailLayoutBuilder::GetDetailFont() )
				.Text( FText::Format(LOCTEXT("ElementIndex", "Element {ElementIndex}"), Arguments ) )
			]
			+SVerticalBox::Slot()
			.Padding(0.0f,4.0f)
			.AutoHeight()
			[
				OnGenerateCustomNameWidgets.IsBound() ? OnGenerateCustomNameWidgets.Execute( MaterialItem.Material.Get(), MaterialItem.SlotIndex ) : StaticCastSharedRef<SWidget>( SNullWidget::NullWidget )
			];
	}

	TSharedRef<SWidget> CreateValueContent( const TSharedPtr<FAssetThumbnailPool>& ThumbnailPool )
	{
		return
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 0.0f )
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew( SPropertyEditorAsset )
					.ObjectPath(MaterialItem.Material->GetPathName())
					.Class(UMaterialInterface::StaticClass())
					.OnSetObject(this, &FMaterialItemView::OnSetObject)
					.DisplayThumbnail(true)
					.ThumbnailPool(ThumbnailPool)
					.CustomContentSlot()
					[
						SNew( SBox )
						.HAlign(HAlign_Left)
						[
							// Add a menu for displaying all textures 
							SNew( SComboButton )
							.OnGetMenuContent( this, &FMaterialItemView::OnGetTexturesMenuForMaterial )
							.VAlign( VAlign_Center )
							.ContentPadding(2)
							.IsEnabled( this, &FMaterialItemView::IsTexturesMenuEnabled )
							.ButtonContent()
							[
								SNew( STextBlock )
								.Font( IDetailLayoutBuilder::GetDetailFont() )
								.ToolTipText( LOCTEXT("ViewTexturesToolTip", "View the textures used by this material" ) )
								.Text( LOCTEXT("ViewTextures","Textures") )
							]
						]
					]
					.ResetToDefaultSlot()
					[
							// Add a button to reset the material to the base material
						SNew(SButton)
						.ToolTipText(LOCTEXT( "ResetToBase", "Reset to base material" ))
						.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
						.ContentPadding(0) 
						.Visibility( this, &FMaterialItemView::GetReplaceVisibility )
						.OnClicked( this, &FMaterialItemView::OnResetToBaseClicked )
						[
							SNew(SImage)
							.Image( FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault") )
						]
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			.VAlign( VAlign_Center )
			[
				OnGenerateCustomMaterialWidgets.IsBound() ? OnGenerateCustomMaterialWidgets.Execute( MaterialItem.Material.Get(), MaterialItem.SlotIndex ) : StaticCastSharedRef<SWidget>( SNullWidget::NullWidget )
			];
	}

private:

	FMaterialItemView(	const FMaterialListItem& InMaterial, 
						FOnMaterialChanged& InOnMaterialChanged, 
						FOnGenerateWidgetsForMaterial& InOnGenerateNameWidgets, 
						FOnGenerateWidgetsForMaterial& InOnGenerateMaterialWidgets, 
						FOnResetMaterialToDefaultClicked& InOnResetToDefaultClicked,
						int32 InMultipleMaterialCount )
		: MaterialItem( InMaterial )
		, OnMaterialChanged( InOnMaterialChanged )
		, OnGenerateCustomNameWidgets( InOnGenerateNameWidgets )
		, OnGenerateCustomMaterialWidgets( InOnGenerateMaterialWidgets )
		, OnResetToDefaultClicked( InOnResetToDefaultClicked )
		, MultipleMaterialCount( InMultipleMaterialCount )
	{

	}

	void ReplaceMaterial( UMaterialInterface* NewMaterial, bool bReplaceAll = false )
	{
		UMaterialInterface* PrevMaterial = NULL;
		if( MaterialItem.Material.IsValid() )
		{
			PrevMaterial = MaterialItem.Material.Get();
		}

		if( NewMaterial != PrevMaterial )
		{
			// Replace the material
			OnMaterialChanged.ExecuteIfBound( NewMaterial, PrevMaterial, MaterialItem.SlotIndex, bReplaceAll );
		}
	}

	void OnSetObject( const UObject* InAsset )
	{
		const bool bReplaceAll = false;

		UMaterialInterface* NewMaterial = Cast<UMaterialInterface>(const_cast<UObject*>(InAsset));
		ReplaceMaterial( NewMaterial, bReplaceAll );
	}

	/**
	 * @return Whether or not the textures menu is enabled
	 */
	bool IsTexturesMenuEnabled() const
	{
		return MaterialItem.Material.Get() != NULL;
	}

	TSharedRef<SWidget> OnGetTexturesMenuForMaterial()
	{
		FMenuBuilder MenuBuilder( true, NULL );

		if( MaterialItem.Material.IsValid() )
		{
			UMaterialInterface* Material = MaterialItem.Material.Get();

			TArray< UTexture* > Textures;
			Material->GetUsedTextures( Textures, EMaterialQualityLevel::Num, false );

			// Add a menu item for each texture.  Clicking on the texture will display it in the content browser
			for( int32 TextureIndex = 0; TextureIndex < Textures.Num(); ++TextureIndex )
			{
				// UObject for delegate compatibility
				UObject* Texture = Textures[TextureIndex];

				FUIAction Action( FExecuteAction::CreateSP( this, &FMaterialItemView::GoToAssetInContentBrowser, TWeakObjectPtr<UObject>(Texture) ) );

				MenuBuilder.AddMenuEntry( FText::FromString( Texture->GetName() ), LOCTEXT( "BrowseTexture_ToolTip", "Find this texture in the content browser" ), FSlateIcon(), Action );
			}
		}

		return MenuBuilder.MakeWidget();
	}

	/**
	 * Finds the asset in the content browser
	 */
	void GoToAssetInContentBrowser( TWeakObjectPtr<UObject> Object )
	{
		if( Object.IsValid() )
		{
			TArray< UObject* > Objects;
			Objects.Add( Object.Get() );
			GEditor->SyncBrowserToObjects( Objects );
		}
	}

	/**
	 * Called to get the visibility of the replace button
	 */
	EVisibility GetReplaceVisibility() const
	{
		// Only show the replace button if the current material can be replaced
		if( OnMaterialChanged.IsBound() && MaterialItem.bCanBeReplaced )
		{
			return EVisibility::Visible;
		}

		return EVisibility::Collapsed;
	}

	/**
	 * Called when reset to base is clicked
	 */
	FReply OnResetToBaseClicked()
	{
		// Only allow reset to base if the current material can be replaced
		if( MaterialItem.Material.IsValid() && MaterialItem.bCanBeReplaced )
		{
			bool bReplaceAll = false;
			ReplaceMaterial( NULL, bReplaceAll );
			OnResetToDefaultClicked.ExecuteIfBound( MaterialItem.Material.Get(), MaterialItem.SlotIndex );
		}
		return FReply::Handled();
	}

private:
	FMaterialListItem MaterialItem;
	FOnMaterialChanged OnMaterialChanged;
	FOnGenerateWidgetsForMaterial OnGenerateCustomNameWidgets;
	FOnGenerateWidgetsForMaterial OnGenerateCustomMaterialWidgets;
	FOnResetMaterialToDefaultClicked OnResetToDefaultClicked;
	int32 MultipleMaterialCount;
};


FMaterialList::FMaterialList( IDetailLayoutBuilder& InDetailLayoutBuilder, FMaterialListDelegates& InMaterialListDelegates )
	: MaterialListDelegates( InMaterialListDelegates )
	, DetailLayoutBuilder( InDetailLayoutBuilder )
	, MaterialListBuilder( new FMaterialListBuilder )
{
}

void FMaterialList::OnDisplayMaterialsForElement( int32 SlotIndex )
{
	// We now want to display all the materials in the element
	ExpandedSlots.Add( SlotIndex );

	MaterialListBuilder->Empty();
	MaterialListDelegates.OnGetMaterials.ExecuteIfBound( *MaterialListBuilder );

	OnRebuildChildren.ExecuteIfBound();
}

void FMaterialList::OnHideMaterialsForElement( int32 SlotIndex )
{
	// No longer want to expand the element
	ExpandedSlots.Remove( SlotIndex );

	// regenerate the materials
	MaterialListBuilder->Empty();
	MaterialListDelegates.OnGetMaterials.ExecuteIfBound( *MaterialListBuilder );
	
	OnRebuildChildren.ExecuteIfBound();
}


void FMaterialList::Tick( float DeltaTime )
{
	// Check each material to see if its still valid.  This allows the material list to stay up to date when materials are changed out from under us
	if( MaterialListDelegates.OnGetMaterials.IsBound() )
	{
		// Whether or not to refresh the material list
		bool bRefrestMaterialList = false;

		// Get the current list of materials from the user
		MaterialListBuilder->Empty();
		MaterialListDelegates.OnGetMaterials.ExecuteIfBound( *MaterialListBuilder );

		if( MaterialListBuilder->GetNumMaterials() != DisplayedMaterials.Num() )
		{
			// The array sizes differ so we need to refresh the list
			bRefrestMaterialList = true;
		}
		else
		{
			// Compare the new list against the currently displayed list
			for( int32 MaterialIndex = 0; MaterialIndex < MaterialListBuilder->MaterialSlots.Num(); ++MaterialIndex )
			{
				const FMaterialListItem& Item = MaterialListBuilder->MaterialSlots[MaterialIndex];

				// The displayed materials is out of date if there isn't a 1:1 mapping between the material sets
				if( !DisplayedMaterials.IsValidIndex( MaterialIndex ) || DisplayedMaterials[ MaterialIndex ] != Item )
				{
					bRefrestMaterialList = true;
					break;
				}
			}
		}

		if( bRefrestMaterialList )
		{
			OnRebuildChildren.ExecuteIfBound();
		}
	}
}

void FMaterialList::GenerateHeaderRowContent( FDetailWidgetRow& NodeRow )
{
}

void FMaterialList::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	ViewedMaterials.Empty();
	DisplayedMaterials.Empty();
	if( MaterialListBuilder->GetNumMaterials() > 0 )
	{
		DisplayedMaterials = MaterialListBuilder->MaterialSlots;

		MaterialListBuilder->Sort();
		TArray<FMaterialListItem>& MaterialSlots = MaterialListBuilder->MaterialSlots;

		int32 CurrentSlot = INDEX_NONE;
		bool bDisplayAllMaterialsInSlot = true;
		for( auto It = MaterialSlots.CreateConstIterator(); It; ++It )
		{
			const FMaterialListItem& Material = *It;

			if( CurrentSlot != Material.SlotIndex )
			{
				// We've encountered a new slot.  Make a widget to display that
				CurrentSlot = Material.SlotIndex;

				uint32 NumMaterials = MaterialListBuilder->GetNumMaterialsInSlot(CurrentSlot);

				// If an element is expanded we want to display all its materials
				bool bWantToDisplayAllMaterials = NumMaterials > 1 && ExpandedSlots.Contains(CurrentSlot);

				// If we are currently displaying an expanded set of materials for an element add a link to collapse all of them
				if( bWantToDisplayAllMaterials )
				{
					FDetailWidgetRow& ChildRow = ChildrenBuilder.AddChildContent( LOCTEXT( "HideAllMaterialSearchString", "Hide All Materials").ToString() );

					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("ElementSlot"), CurrentSlot);
					ChildRow
					.ValueContent()
					.MaxDesiredWidth(0.0f)// No Max Width
					[
						SNew( SBox )
							.HAlign( HAlign_Center )
							[
								SNew( SHyperlink )
									.TextStyle( FEditorStyle::Get(), "MaterialList.HyperlinkStyle" )
									.Text( FText::Format(LOCTEXT("HideAllMaterialLinkText", "Hide All Materials on Element {ElementSlot}"), Arguments ) )
									.OnNavigate( this, &FMaterialList::OnHideMaterialsForElement, CurrentSlot )
							]
					];
				}	

				if( NumMaterials > 1 && !bWantToDisplayAllMaterials )
				{
					// The current slot has multiple elements to view
					bDisplayAllMaterialsInSlot = false;

					FDetailWidgetRow& ChildRow = ChildrenBuilder.AddChildContent( TEXT("") );

					AddMaterialItem( ChildRow, CurrentSlot, FMaterialListItem( NULL, CurrentSlot, true ), !bDisplayAllMaterialsInSlot );
				}
				else
				{
					bDisplayAllMaterialsInSlot = true;
				}

			}

			// Display each thumbnail element unless we shouldn't display multiple materials for one slot
			if( bDisplayAllMaterialsInSlot )
			{
				FDetailWidgetRow& ChildRow = ChildrenBuilder.AddChildContent( Material.Material.IsValid()? Material.Material->GetName() : TEXT("") );

				AddMaterialItem( ChildRow, CurrentSlot, Material, !bDisplayAllMaterialsInSlot );
			}
		}
	}
	else
	{
		FDetailWidgetRow& ChildRow = ChildrenBuilder.AddChildContent( LOCTEXT("NoMaterials", "No Materials").ToString() );

		ChildRow
		[
			SNew( SBox )
			.HAlign( HAlign_Center )
			[
				SNew( STextBlock )
				.Text( LOCTEXT("NoMaterials", "No Materials") ) 
				.Font( IDetailLayoutBuilder::GetDetailFont() )
			]
		];
	}		
}
			
void FMaterialList::AddMaterialItem( FDetailWidgetRow& Row, int32 CurrentSlot, const FMaterialListItem& Item, bool bDisplayLink )
{
	uint32 NumMaterials = MaterialListBuilder->GetNumMaterialsInSlot(CurrentSlot);

	TSharedRef<FMaterialItemView> NewView = FMaterialItemView::Create( Item, MaterialListDelegates.OnMaterialChanged, MaterialListDelegates.OnGenerateCustomNameWidgets, MaterialListDelegates.OnGenerateCustomMaterialWidgets, MaterialListDelegates.OnResetMaterialToDefaultClicked, NumMaterials );

	TSharedPtr<SWidget> RightSideContent;
	if( bDisplayLink )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("NumMaterials"), NumMaterials);

		RightSideContent = 
			SNew( SBox )
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				[
					SNew( SHyperlink )
					.TextStyle( FEditorStyle::Get(), "MaterialList.HyperlinkStyle" )
					.Text( FText::Format(LOCTEXT("DisplayAllMaterialLinkText", "Display {NumMaterials} materials"), Arguments) )
					.ToolTipText( LOCTEXT("DisplayAllMaterialLink_ToolTip","Display all materials. Drag and drop a material here to replace all materials.") )
					.OnNavigate( this, &FMaterialList::OnDisplayMaterialsForElement, CurrentSlot )
				];
	}
	else
	{
		RightSideContent = NewView->CreateValueContent( DetailLayoutBuilder.GetThumbnailPool() );
		ViewedMaterials.Add( NewView );
	}

	Row.NameContent()
	[
		NewView->CreateNameContent()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0.0f) // no maximum
	[
		RightSideContent.ToSharedRef()
	];
}

#undef LOCTEXT_NAMESPACE

