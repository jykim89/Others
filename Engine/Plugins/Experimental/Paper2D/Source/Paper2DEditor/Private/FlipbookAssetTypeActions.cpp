// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"

#include "FlipbookAssetTypeActions.h"
#include "FlipbookEditor/FlipbookEditor.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

//////////////////////////////////////////////////////////////////////////
// FFlipbookAssetTypeActions

FText FFlipbookAssetTypeActions::GetName() const
{
	return LOCTEXT("FFlipbookAssetTypeActionsName", "Sprite Flipbook");
}

FColor FFlipbookAssetTypeActions::GetTypeColor() const
{
	return FColor(0, 255, 255);
}

UClass* FFlipbookAssetTypeActions::GetSupportedClass() const
{
	return UPaperFlipbook::StaticClass();
}

void FFlipbookAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(*ObjIt))
		{
			TSharedRef<FFlipbookEditor> NewFlipbookEditor(new FFlipbookEditor());
			NewFlipbookEditor->InitFlipbookEditor(Mode, EditWithinLevelEditor, Flipbook);
		}
	}
}

uint32 FFlipbookAssetTypeActions::GetCategories()
{
	return EAssetTypeCategories::Animation;
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE