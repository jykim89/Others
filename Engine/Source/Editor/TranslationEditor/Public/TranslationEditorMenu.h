// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Translation Editor menu
 */
class TRANSLATIONEDITOR_API FTranslationEditorMenu
{
public:
	static void SetupTranslationEditorMenu( TSharedPtr< FExtender > Extender, FTranslationEditor& TranslationEditor);
	static void SetupTranslationEditorToolbar( TSharedPtr< FExtender > Extender, FTranslationEditor& TranslationEditor);
	
protected:

	static void FillTranslationMenu( FMenuBuilder& MenuBuilder/*, FTranslationEditor& TranslationEditor*/ );
};


class FTranslationEditorCommands : public TCommands<FTranslationEditorCommands>
{
public:
	/** Constructor */
	FTranslationEditorCommands() 
		: TCommands<FTranslationEditorCommands>("TranslationEditor", NSLOCTEXT("Contexts", "TranslationEditor", "Translation Editor"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	/** Switch fonts */
	TSharedPtr<FUICommandInfo> ChangeSourceFont;
	TSharedPtr<FUICommandInfo> ChangeTranslationTargetFont;

	/** Save the translations to file */
	TSharedPtr<FUICommandInfo> SaveTranslations;

	/** Save the translations to file */
	TSharedPtr<FUICommandInfo> PreviewAllTranslationsInEditor;

	/** Export to PortableObject format (.po) */
	TSharedPtr<FUICommandInfo> ExportToPortableObjectFormat;

	/** Import from PortableObject format (.po) */
	TSharedPtr<FUICommandInfo> ImportFromPortableObjectFormat;

	/** Open the tab for searching */
	TSharedPtr<FUICommandInfo> OpenSearchTab;

	/** Initialize commands */
	virtual void RegisterCommands() OVERRIDE;
};

