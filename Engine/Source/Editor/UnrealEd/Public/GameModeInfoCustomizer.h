// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDocumentation.h"
#include "KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "FGameModeInfoCustomizer"

static FString GameModeCategory(LOCTEXT("GameModeCategory", "GameMode").ToString());

/** Class to help customize a GameMode class picker, to show settings 'withing' GameMode. */
class FGameModeInfoCustomizer : public TSharedFromThis<FGameModeInfoCustomizer>
{
public:
	FGameModeInfoCustomizer(UObject* InOwningObject, FName InGameModePropertyName)
	{
		OwningObject = InOwningObject;
		GameModePropertyName = InGameModePropertyName;
	}

	/** Create widget for the name of a default class property */
	TSharedRef<SWidget> CreateGameModePropertyLabelWidget(FName PropertyName)
	{
		UProperty* Prop = FindFieldChecked<UProperty>(AGameMode::StaticClass(), PropertyName);

		FString DisplayName = Prop->GetDisplayNameText().ToString();
		if (DisplayName.Len() == 0)
		{
			DisplayName = Prop->GetName();
		}
		DisplayName = FName::NameToDisplayString(DisplayName, false);

		return
			SNew(STextBlock)
			.Text(FText::FromString(DisplayName))
			.ToolTip(IDocumentation::Get()->CreateToolTip(Prop->GetToolTipText(), NULL, TEXT("Shared/Types/AGameMode"), Prop->GetName()))
			.Font(IDetailLayoutBuilder::GetDetailFont());
	}

	/** Create widget fo modifying a default class within the current GameMode */
	void CustomizeGameModeDefaultClass(IDetailGroup& Group, FName DefaultClassPropertyName)
	{
		// Find the metaclass of this property
		UClass* MetaClass = UObject::StaticClass();
		const UClass* GameModeClass = GetCurrentGameModeClass();
		bool bAllowNone = false;
		if (GameModeClass != NULL)
		{
			UClassProperty* ClassProp = FindFieldChecked<UClassProperty>(GameModeClass, DefaultClassPropertyName);
			MetaClass = ClassProp->MetaClass;
			bAllowNone = !(ClassProp->PropertyFlags & CPF_NoClear);
		}

		TAttribute<bool> CanBrowseAtrribute = TAttribute<bool>::Create( TAttribute<bool>::FGetter::CreateSP( this, &FGameModeInfoCustomizer::CanBrowseDefaultClass, DefaultClassPropertyName) ) ;

		// Add a row for choosing a new default class
		Group.AddWidgetRow()
		.NameContent()
		[
			CreateGameModePropertyLabelWidget(DefaultClassPropertyName)
		]
		.ValueContent()
		.MaxDesiredWidth(0)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SClassPropertyEntryBox)
				.AllowNone(bAllowNone)
				.MetaClass(MetaClass)
				.IsEnabled(this, &FGameModeInfoCustomizer::AllowModifyGameMode)
				.SelectedClass(this, &FGameModeInfoCustomizer::OnGetDefaultClass, DefaultClassPropertyName)
				.OnSetClass(FOnSetClass::CreateSP(this, &FGameModeInfoCustomizer::OnSetDefaultClass, DefaultClassPropertyName))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FGameModeInfoCustomizer::OnBrowseDefaultClassClicked, DefaultClassPropertyName), FText(), CanBrowseAtrribute)
			]
		];
	}

	/** Add special customization for the GameMode setting */
	void CustomizeGameModeSetting(IDetailLayoutBuilder& LayoutBuilder, IDetailCategoryBuilder& CategoryBuilder)
	{
		// Add GameMode picker widget
		TSharedPtr<IPropertyHandle> DefaultGameModeHandle = LayoutBuilder.GetProperty(GameModePropertyName);

		UProperty* DefaultGameProperty = DefaultGameModeHandle->GetProperty();
		// See if its a FStringClassReference property
		if (UStructProperty* GameModeStructProp = Cast<UStructProperty>(DefaultGameProperty))
		{
			check(GameModeStructProp->Struct->GetName() == TEXT("StringClassReference"));
			DefaultGameModeClassHandle = DefaultGameModeHandle->GetChildHandle("ClassName");
			check(DefaultGameModeClassHandle.IsValid());
		}
		// Look for a regular Class property
		else if (UClassProperty* GameModeClassProp = Cast<UClassProperty>(DefaultGameProperty))
		{
			DefaultGameModeClassHandle = DefaultGameModeHandle;
		}

		IDetailPropertyRow& DefaultGameModeRow = CategoryBuilder.AddProperty(DefaultGameModeHandle);
		// SEe if we are allowed to choose 'no' GameMode
		const bool bAllowNone = !(DefaultGameModeHandle->GetProperty()->PropertyFlags & CPF_NoClear);

		TAttribute<bool> CanBrowseAtrribute = TAttribute<bool>(this, &FGameModeInfoCustomizer::CanBrowseGameMode);

		DefaultGameModeRow
		.ShowPropertyButtons(false)
		.CustomWidget()
		.NameContent()
		[
			DefaultGameModeHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(0)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SClassPropertyEntryBox)
				.AllowNone(bAllowNone)
				.MetaClass(AGameMode::StaticClass())
				.SelectedClass(this, &FGameModeInfoCustomizer::GetCurrentGameModeClass)
				.OnSetClass(FOnSetClass::CreateSP(this, &FGameModeInfoCustomizer::SetCurrentGameModeClass))
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f, 1.0f, 2.0f, 0.0f )
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(FMargin(2.0f,0.0f))
				.OnClicked(this, &FGameModeInfoCustomizer::OnClickNewGameMode)
				.ToolTipText(LOCTEXT("NewGameMode_ToolTip", "Create a new Game Mode"))
				[
					SNew( STextBlock )
					.Text(LOCTEXT("NewGameMode", "New.."))
					.Font( IDetailLayoutBuilder::GetDetailFont() )
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FGameModeInfoCustomizer::OnBrowseGameModeClicked), FText(), CanBrowseAtrribute)
			]
		];

		static FName SelectedGameModeDetailsName(TEXT("SelectedGameModeDetails"));		
		IDetailGroup& Group = CategoryBuilder.AddGroup(SelectedGameModeDetailsName, LOCTEXT("SelectedGameModeDetails", "Selected GameMode").ToString());


		// Then add rows to show key properties and let you edit them
		CustomizeGameModeDefaultClass(Group, GET_MEMBER_NAME_CHECKED(AGameMode, DefaultPawnClass));
		CustomizeGameModeDefaultClass(Group, GET_MEMBER_NAME_CHECKED(AGameMode, HUDClass));
		CustomizeGameModeDefaultClass(Group, GET_MEMBER_NAME_CHECKED(AGameMode, PlayerControllerClass));
		CustomizeGameModeDefaultClass(Group, GET_MEMBER_NAME_CHECKED(AGameMode, GameStateClass));
	}

	/** Get the currently set GameMode class */
	const UClass* GetCurrentGameModeClass() const
	{
		FString ClassName;
		DefaultGameModeClassHandle->GetValueAsFormattedString(ClassName);

		if (ClassName.IsEmpty() || ClassName == "None")
		{
			return NULL;
		}

		FString StrippedClassName = ClassName;
		ConstructorHelpers::StripObjectClass(StrippedClassName);

		const UClass* GameModeClass = FindObject<UClass>(ANY_PACKAGE, *StrippedClassName);
		if (!GameModeClass)
		{
			GameModeClass = LoadObject<UClass>(NULL, *ClassName);
		}

		return GameModeClass;
	}

	void SetCurrentGameModeClass(const UClass* NewGameModeClass)
	{
		DefaultGameModeClassHandle->SetValueFromFormattedString((NewGameModeClass) ? NewGameModeClass->GetPathName() : "None");
	}

	/** Get the CDO from the currently set GameMode class */
	AGameMode* GetCurrentGameModeCDO() const
	{
		UClass* GameModeClass = const_cast<UClass*>( GetCurrentGameModeClass() );
		if (GameModeClass != NULL)
		{
			return GameModeClass->GetDefaultObject<AGameMode>();
		}
		else
		{
			return NULL;
		}
	}

	/** Find the current default class by property name */
	const UClass* OnGetDefaultClass(FName ClassPropertyName) const
	{
		UClass* CurrentDefaultClass = NULL;
		const UClass* GameModeClass = GetCurrentGameModeClass();
		if (GameModeClass != NULL)
		{
			UClassProperty* ClassProp = FindFieldChecked<UClassProperty>(GameModeClass, ClassPropertyName);
			CurrentDefaultClass = (UClass*)ClassProp->GetObjectPropertyValue(ClassProp->ContainerPtrToValuePtr<void>(GetCurrentGameModeCDO()));
		}
		return CurrentDefaultClass;
	}

	/** Set a new default class by property name */
	void OnSetDefaultClass(const UClass* NewDefaultClass, FName ClassPropertyName)
	{
		const UClass* GameModeClass = GetCurrentGameModeClass();
		if (GameModeClass != NULL && AllowModifyGameMode())
		{
			UClassProperty* ClassProp = FindFieldChecked<UClassProperty>(GameModeClass, ClassPropertyName);
			const UClass** DefaultClassPtr = ClassProp->ContainerPtrToValuePtr<const UClass*>(GetCurrentGameModeCDO());
			*DefaultClassPtr = NewDefaultClass;

			// Indicate that the BP has changed and would need to be saved.
			GameModeClass->MarkPackageDirty();
		}
	}

	bool CanBrowseDefaultClass(FName ClassPropertyName) const
	{
		return CanSyncToClass(OnGetDefaultClass(ClassPropertyName));
	}

	void OnBrowseDefaultClassClicked(FName ClassPropertyName)
	{
		SyncBrowserToClass(OnGetDefaultClass(ClassPropertyName));
	}

	bool CanBrowseGameMode() const
	{
		return CanSyncToClass(GetCurrentGameModeClass());
	}

	void OnBrowseGameModeClicked()
	{
		SyncBrowserToClass(GetCurrentGameModeClass());
	}

	bool CanSyncToClass(const UClass* Class) const
	{
		return (Class != NULL && Class->ClassGeneratedBy != NULL);
	}

	void SyncBrowserToClass(const UClass* Class)
	{
		if (CanSyncToClass(Class))
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy);
			if (ensure(Blueprint != NULL))
			{
				TArray<UObject*> SyncObjects;
				SyncObjects.Add(Blueprint);
				GEditor->SyncBrowserToObjects(SyncObjects);
			}
		}
	}

	FReply OnClickNewGameMode()
	{
		// Create a new GameMode BP
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromClass(LOCTEXT("CreateNewGameMode", "Create New GameMode"), AGameMode::StaticClass(), TEXT("NewGameMode"));
		// if that worked, assign it
		if(Blueprint != NULL && Blueprint->GeneratedClass)
		{
			DefaultGameModeClassHandle->SetValueFromFormattedString(Blueprint->GeneratedClass->GetPathName());
		}

		return FReply::Handled();
	}

	/** Are we allowed to modify the currently selected GameMode */
	bool AllowModifyGameMode() const
	{
		// Only allow editing GameMode BP, not native class!
		const UBlueprintGeneratedClass* GameModeBPClass = Cast<UBlueprintGeneratedClass>(GetCurrentGameModeClass());
		return (GameModeBPClass != NULL);
	}

private:
	/** Object that owns the pointer to the GameMode we want to customize */
	TWeakObjectPtr<UObject>	OwningObject;
	/** Name of GameMode property inside OwningObject */
	FName GameModePropertyName;
	/** Handle to the DefaultGameMode property */
	TSharedPtr<IPropertyHandle> DefaultGameModeClassHandle;
};

#undef LOCTEXT_NAMESPACE
