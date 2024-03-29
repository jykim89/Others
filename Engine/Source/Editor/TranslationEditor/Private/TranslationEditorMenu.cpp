// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "TranslationEditorPrivatePCH.h"
#include "TranslationEditor.h"
#include "GraphEditorActions.h"
#include "Editor/PropertyEditor/Public/PropertyEditing.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "KismetToolbar"

void FTranslationEditorMenu::FillTranslationMenu( FMenuBuilder& MenuBuilder/*, FTranslationEditor& TranslationEditor*/ )
{
	MenuBuilder.BeginSection("Font", LOCTEXT("Translation_FontHeading", "Font"));
	{
		MenuBuilder.AddMenuEntry( FTranslationEditorCommands::Get().ChangeSourceFont );
		MenuBuilder.AddMenuEntry( FTranslationEditorCommands::Get().ChangeTranslationTargetFont );
		MenuBuilder.AddMenuEntry( FTranslationEditorCommands::Get().PreviewAllTranslationsInEditor );
		MenuBuilder.AddMenuEntry( FTranslationEditorCommands::Get().ExportToPortableObjectFormat );
		MenuBuilder.AddMenuEntry( FTranslationEditorCommands::Get().ImportFromPortableObjectFormat );
		MenuBuilder.AddMenuEntry( FTranslationEditorCommands::Get().OpenSearchTab );
	}
	MenuBuilder.EndSection();
}

void FTranslationEditorMenu::SetupTranslationEditorMenu( TSharedPtr< FExtender > Extender, FTranslationEditor& TranslationEditor)
{
	// Add additional editor menu
	{
		struct Local
		{
			static void AddSaveMenuOption( FMenuBuilder& MenuBuilder )
			{
				MenuBuilder.AddMenuEntry( FTranslationEditorCommands::Get().SaveTranslations, "SaveTranslations", TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "AssetEditor.SaveAsset.Greyscale") );
			}

			static void AddTranslationEditorMenu( FMenuBarBuilder& MenuBarBuilder )
			{
				// View
				MenuBarBuilder.AddPullDownMenu( 
					LOCTEXT("TranslationMenu", "Translation"),
					LOCTEXT("TranslationMenu_ToolTip", "Open the Translation menu"),
					FNewMenuDelegate::CreateStatic( &FTranslationEditorMenu::FillTranslationMenu ),
					"View");
			}
		};

		Extender->AddMenuExtension(
			"FileLoadAndSave",
			EExtensionHook::First,
			TranslationEditor.GetToolkitCommands(),
			FMenuExtensionDelegate::CreateStatic( &Local::AddSaveMenuOption ) );

		Extender->AddMenuBarExtension(
			"Edit",
			EExtensionHook::After,
			TranslationEditor.GetToolkitCommands(), 
			FMenuBarExtensionDelegate::CreateStatic( &Local::AddTranslationEditorMenu ) );
	}
}

void FTranslationEditorMenu::SetupTranslationEditorToolbar( TSharedPtr< FExtender > Extender, FTranslationEditor& TranslationEditor )
{
	struct Local
	{
		static void AddToolbarButtons( FToolBarBuilder& ToolbarBuilder )
		{
			ToolbarBuilder.AddToolBarButton(
				FTranslationEditorCommands::Get().SaveTranslations, "SaveTranslations", TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "AssetEditor.SaveAsset"));
			ToolbarBuilder.AddToolBarButton(
				FTranslationEditorCommands::Get().PreviewAllTranslationsInEditor, "PreviewTranslationsInEditor", TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "AssetEditor.ReimportAsset")); 
			ToolbarBuilder.AddToolBarButton(
				FTranslationEditorCommands::Get().ExportToPortableObjectFormat, "ExportToPortableObjectFormat", TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "FontEditor.Export"));
			ToolbarBuilder.AddToolBarButton(
				FTranslationEditorCommands::Get().ImportFromPortableObjectFormat, "ImportFromPortableObjectFormat", TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "TranslationEditor.Import"));
			ToolbarBuilder.AddToolBarButton(
				FTranslationEditorCommands::Get().OpenSearchTab, "OpenSearchTab", TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "BlueprintEditor.FindInBlueprint"));
		}
	};

	Extender->AddToolBarExtension(
		"Asset",
		EExtensionHook::First,
		TranslationEditor.GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic( &Local::AddToolbarButtons ) );
}


//////////////////////////////////////////////////////////////////////////
// FTranslationEditorCommands

void FTranslationEditorCommands::RegisterCommands() 
{
	UI_COMMAND( ChangeSourceFont, "Change Source Font", "Change the Font for the Source Lanugage", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( ChangeTranslationTargetFont, "Change Translation Font", "Change the Translation Target Language Font", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( SaveTranslations, "Save", "Saves the translations to file", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( PreviewAllTranslationsInEditor, "Preview in Editor", "Preview All Translations in the Editor UI", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( ExportToPortableObjectFormat, "Export to .PO", "Export to Portable Object Format", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( ImportFromPortableObjectFormat, "Import from .PO", "Import from Portable Object Format", EUserInterfaceActionType::Button, FInputGesture() );
	UI_COMMAND( OpenSearchTab, "Search", "Search Source and Translation Strings", EUserInterfaceActionType::Button, FInputGesture() );
}


#undef LOCTEXT_NAMESPACE
