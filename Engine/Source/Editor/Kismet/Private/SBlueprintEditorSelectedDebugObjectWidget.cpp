// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "BlueprintEditorCommands.h"
#include "BlueprintEditor.h"
#include "SBlueprintEditorToolbar.h"
#include "Editor/UnrealEd/Public/Kismet2/DebuggerCommands.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "SSCSEditor.h"
#include "SSCSEditorViewport.h"
#include "GraphEditorActions.h"
#include "ISourceControlModule.h"
#include "ISourceControlRevision.h"
#include "AssetToolsModule.h"
#include "BlueprintEditorModes.h"
#include "WorkflowOrientedApp/SModeWidget.h"
#include "Editor/PropertyEditor/Public/PropertyEditing.h"
#include "PropertyCustomizationHelpers.h"
#include "IDocumentation.h"
#include "SLevelOfDetailBranchNode.h"
#include "STutorialWrapper.h"
#include "SBlueprintEditorSelectedDebugObjectWidget.h"

#define LOCTEXT_NAMESPACE "KismetToolbar"

//////////////////////////////////////////////////////////////////////////
// SBlueprintEditorSelectedDebugObjectWidget

void SBlueprintEditorSelectedDebugObjectWidget::Construct(const FArguments& InArgs, TSharedPtr<FBlueprintEditor> InBlueprintEditor)
{
	BlueprintEditor = InBlueprintEditor;

	const TSharedRef<SWidget> BrowseButton = PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &SBlueprintEditorSelectedDebugObjectWidget::SelectedDebugObject_OnClicked));
	BrowseButton->SetVisibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SBlueprintEditorSelectedDebugObjectWidget::IsSelectDebugObjectButtonVisible)));
	BrowseButton->SetToolTipText(LOCTEXT("DebugSelectActor", "Select this Actor in level"));

	GenerateDebugWorldNames(false);
	GenerateDebugObjectNames(false);
	LastObjectObserved = DebugObjects[0];

	DebugWorldsComboBox = SNew(STextComboBox)
		.ToolTip(IDocumentation::Get()->CreateToolTip(
		LOCTEXT("BlueprintDebugWorldTooltip", "Select a world to debug"),
		NULL,
		TEXT("Shared/Editors/BlueprintEditor/BlueprintDebugger"),
		TEXT("DebugWorld")))
		.OptionsSource(&DebugWorldNames)
		.InitiallySelectedItem(GetDebugWorldName())
		.Visibility(this, &SBlueprintEditorSelectedDebugObjectWidget::IsDebugWorldComboVisible)
		.OnComboBoxOpening(this, &SBlueprintEditorSelectedDebugObjectWidget::GenerateDebugWorldNames, true)
		.OnSelectionChanged(this, &SBlueprintEditorSelectedDebugObjectWidget::DebugWorldSelectionChanged);

	DebugObjectsComboBox = SNew(STextComboBox)
		.ToolTip(IDocumentation::Get()->CreateToolTip(
		LOCTEXT("BlueprintDebugObjectTooltip", "Select an object to debug"),
		NULL,
		TEXT("Shared/Editors/BlueprintEditor/BlueprintDebugger"),
		TEXT("DebugObject")))
		.OptionsSource(&DebugObjectNames)
		.InitiallySelectedItem(GetDebugObjectName())
		.OnComboBoxOpening(this, &SBlueprintEditorSelectedDebugObjectWidget::GenerateDebugObjectNames, true)
		.OnSelectionChanged(this, &SBlueprintEditorSelectedDebugObjectWidget::DebugObjectSelectionChanged);

	TSharedRef<SWidget> DebugObjectSelectionWidget =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			DebugObjectsComboBox.ToSharedRef()
		]
	+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(2.0f)
		[
			BrowseButton
		];





	ChildSlot
		[
			SNew(SLevelOfDetailBranchNode)
			.UseLowDetailSlot(FMultiBoxSettings::UseSmallToolBarIcons)
			.LowDetail()
			[
				// Horizontal Layout when using small icons
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					DebugWorldsComboBox.ToSharedRef()
				]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						DebugObjectSelectionWidget
					]
			]
			.HighDetail()
				[
					SNew(SVerticalBox)
					.Visibility(this, &SBlueprintEditorSelectedDebugObjectWidget::ShouldShowDebugObjectPicker)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Bottom)
					[
						// Vertical Layout when using normal size icons
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							DebugWorldsComboBox.ToSharedRef()
						]

						+ SVerticalBox::Slot()
							.AutoHeight()
							[
								DebugObjectSelectionWidget
							]
					]
					+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.Padding(2.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("DebugSelectTitle", "Debug Filter"))
						]
				]
		];
}

void SBlueprintEditorSelectedDebugObjectWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (GetBlueprintObj())
	{
		if (UObject* Object = GetBlueprintObj()->GetObjectBeingDebugged())
		{
			if (Object != LastObjectObserved.Get())
			{
				// bRestoreSelection attempts to restore the selection by name, 
				// this ensures that if the last object we had selected was 
				// regenerated (spawning a new object), then we select that  
				// again, even if it is technically a different object
				GenerateDebugObjectNames(/*bRestoreSelection =*/true);

				TSharedPtr<FString> NewSelection = DebugObjectsComboBox->GetSelectedItem();
				// just in case that object we want to select is actually in 
				// there (and wasn't caught by bRestoreSelection), then let's 
				// favor that over whatever was picked
				for (int32 Index = 0; Index < DebugObjects.Num(); ++Index)
				{
					if (DebugObjects[Index] == Object)
					{
						NewSelection = DebugObjectNames[Index];
						break;
					}
				}

				if (!NewSelection.IsValid())
				{
					NewSelection = DebugObjectNames[0];
				}

				DebugObjectsComboBox->SetSelectedItem(NewSelection);
				LastObjectObserved = Object;
			}
		}
		else
		{
			LastObjectObserved = NULL;

			// If the object name is a name (rather than the 'No debug selected' string then regenerate the names (which will reset the combo box) as the object is invalid.
			TSharedPtr<FString> CurrentString = DebugObjectsComboBox->GetSelectedItem();
			if (*CurrentString != GetNoDebugString())
			{
				GenerateDebugObjectNames(false);
			}
		}
	}
}

const FString& SBlueprintEditorSelectedDebugObjectWidget::GetNoDebugString() const
{
	return NSLOCTEXT("BlueprintEditor", "DebugObjectNothingSelected", "No debug object selected").ToString();
}

const FString& SBlueprintEditorSelectedDebugObjectWidget::GetDebugAllWorldsString() const
{
	return NSLOCTEXT("BlueprintEditor", "DebugWorldNothingSelected", "All Worlds").ToString();
}

void SBlueprintEditorSelectedDebugObjectWidget::OnRefresh()
{
	if (GetBlueprintObj())
	{
		GenerateDebugWorldNames(false);
		if (UObject* Object = GetBlueprintObj()->GetObjectBeingDebugged())
		{
			GenerateDebugObjectNames(false);
			if (AActor* Actor = Cast<AActor>(Object))
			{
				DebugObjectsComboBox->SetSelectedItem(MakeShareable(new FString(Actor->GetActorLabel())));
			}
			else
			{
				DebugObjectsComboBox->SetSelectedItem(MakeShareable(new FString(Object->GetName())));
			}
		}
		else
		{
			// If we didnt find and object its probably a good idea to regenerate the names now - this will also ensure the combo box has a valid selection.
			GenerateDebugObjectNames(false);
		}
	}
}

void SBlueprintEditorSelectedDebugObjectWidget::GenerateDebugWorldNames(bool bRestoreSelection)
{
	TSharedPtr<FString> OldSelection;

	// Store off the old selection
	if (bRestoreSelection && DebugWorldsComboBox.IsValid())
	{
		OldSelection = DebugWorldsComboBox->GetSelectedItem();
	}

	DebugWorldNames.Empty();
	DebugWorlds.Empty();

	DebugWorlds.Add(NULL);
	DebugWorldNames.Add(MakeShareable(new FString(GetDebugAllWorldsString())));

	UWorld* PreviewWorld = NULL;
	TSharedPtr<SSCSEditorViewport> PreviewViewportPtr = BlueprintEditor.Pin()->GetSCSViewport();
	if (PreviewViewportPtr.IsValid())
	{
		PreviewWorld = PreviewViewportPtr->GetPreviewScene().GetWorld();
	}

	for (TObjectIterator<UWorld> It; It; ++It)
	{
		UWorld *TestWorld = *It;
		if (!TestWorld || TestWorld->WorldType != EWorldType::PIE)
		{
			continue;
		}

		DebugWorlds.Add(TestWorld);
		ENetMode NetMode = TestWorld->GetNetMode();

		FString WorldName;

		switch (NetMode)
		{
		case NM_Standalone:
			WorldName = NSLOCTEXT("BlueprintEditor", "DebugWorldStandalone", "Standalone").ToString();
			break;

		case NM_ListenServer:
			WorldName = NSLOCTEXT("BlueprintEditor", "DebugWorldListenServer", "Listen Server").ToString();
			break;

		case NM_DedicatedServer:
			WorldName = NSLOCTEXT("BlueprintEditor", "DebugWorldDedicatedServer", "Dedicated Server").ToString();
			break;

		case NM_Client:
			FWorldContext &PieContext = GEngine->GetWorldContextFromWorldChecked(TestWorld);
			WorldName = FString::Printf(TEXT("%s %d"), *NSLOCTEXT("BlueprintEditor", "DebugWorldClient", "Client").ToString(), PieContext.PIEInstance - 1);
			break;
		};

		DebugWorldNames.Add(MakeShareable(new FString(WorldName)));
	}

	// Attempt to restore the old selection
	if (bRestoreSelection && DebugWorldsComboBox.IsValid())
	{
		bool bMatchFound = false;
		for (int32 WorldIdx = 0; WorldIdx < DebugWorldNames.Num(); ++WorldIdx)
		{
			if (*DebugWorldNames[WorldIdx] == *OldSelection)
			{
				DebugWorldsComboBox->SetSelectedItem(DebugWorldNames[WorldIdx]);
				bMatchFound = true;
				break;
			}
		}

		// No match found, use the default option
		if (!bMatchFound)
		{
			DebugWorldsComboBox->SetSelectedItem(DebugWorldNames[0]);
		}
	}


	// Finally ensure we have a valid selection
	if (DebugWorldsComboBox.IsValid())
	{
		TSharedPtr<FString> CurrentSelection = DebugWorldsComboBox->GetSelectedItem();
		if (DebugWorldNames.Find(CurrentSelection) == INDEX_NONE)
		{
			if (DebugWorldNames.Num() > 0)
			{
				DebugWorldsComboBox->SetSelectedItem(DebugWorldNames[0]);
			}
			else
			{
				DebugWorldsComboBox->ClearSelection();
			}
		}
	}
}

void SBlueprintEditorSelectedDebugObjectWidget::GenerateDebugObjectNames(bool bRestoreSelection)
{
	TSharedPtr<FString> OldSelection;

	// Store off the old selection
	if (bRestoreSelection && DebugObjectsComboBox.IsValid())
	{
		OldSelection = DebugObjectsComboBox->GetSelectedItem();
	}

	// Empty the lists of actors and regenerate them
	DebugObjects.Empty();
	DebugObjectNames.Empty();
	DebugObjects.Add(NULL);
	DebugObjectNames.Add(MakeShareable(new FString(GetNoDebugString())));

	// Grab custom objects that should always be visible, regardless of the world
	TArray<FCustomDebugObject> CustomDebugObjects;
	BlueprintEditor.Pin()->GetCustomDebugObjects(/*inout*/ CustomDebugObjects);

	for (const FCustomDebugObject& Entry : CustomDebugObjects)
	{
		if (Entry.NameOverride.IsEmpty())
		{
			AddDebugObject(Entry.Object);
		}
		else
		{
			AddDebugObjectWithName(Entry.Object, Entry.NameOverride);
		}
	}

	// Check for a specific debug world. If DebugWorld=NULL we take that as "any PIE world"
	UWorld* DebugWorld = NULL;
	if (DebugWorldsComboBox.IsValid())
	{
		TSharedPtr<FString> CurrentWorldSelection = DebugWorldsComboBox->GetSelectedItem();
		int32 SelectedIndex = DebugWorldNames.Find(CurrentWorldSelection);
		if (SelectedIndex > 0 && DebugWorldNames.IsValidIndex(SelectedIndex))
		{
			DebugWorld = DebugWorlds[SelectedIndex].Get();
		}
	}

	UWorld* PreviewWorld = NULL;
	TSharedPtr<SSCSEditorViewport> PreviewViewportPtr = BlueprintEditor.Pin()->GetSCSViewport();
	if (PreviewViewportPtr.IsValid())
	{
		PreviewWorld = PreviewViewportPtr->GetPreviewScene().GetWorld();
	}

	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* TestObject = *It;

		// Skip Blueprint preview objects (don't allow them to be selected for debugging)
		if (PreviewWorld != NULL && TestObject->IsIn(PreviewWorld))
		{
			continue;
		}

		const bool bPassesFlags = !TestObject->HasAnyFlags(RF_PendingKill | RF_ClassDefaultObject);
		const bool bGeneratedByAnyBlueprint = TestObject->GetClass()->ClassGeneratedBy != nullptr;
		const bool bGeneratedByThisBlueprint = bGeneratedByAnyBlueprint && TestObject->IsA(GetBlueprintObj()->GeneratedClass);

		if (bPassesFlags && bGeneratedByThisBlueprint)
		{
			UObject *ObjOuter = TestObject;
			UWorld *ObjWorld = NULL;
			while (ObjWorld == NULL && ObjOuter != NULL)
			{
				ObjOuter = ObjOuter->GetOuter();
				ObjWorld = Cast<UWorld>(ObjOuter);
			}

			// Object not in any world
			if (!ObjWorld)
			{
				continue;
			}

			// Make check on owning level (not streaming level)
			if (ObjWorld->PersistentLevel && ObjWorld->PersistentLevel->OwningWorld)
			{
				ObjWorld = ObjWorld->PersistentLevel->OwningWorld;
			}

			// We have a specific debug world and the object isnt in it
			if (DebugWorld && ObjWorld != DebugWorld)
			{
				continue;
			}

			// We don't have a specific debug world, but the object isnt in a PIE world
			if (ObjWorld->WorldType != EWorldType::PIE)
			{
				continue;
			}

			AddDebugObject(TestObject);
		}
	}

	// Attempt to restore the old selection
	if (bRestoreSelection && DebugObjectsComboBox.IsValid())
	{
		bool bMatchFound = false;
		for (int32 ObjectIndex = 0; ObjectIndex < DebugObjectNames.Num(); ++ObjectIndex)
		{
			if (*DebugObjectNames[ObjectIndex] == *OldSelection)
			{
				DebugObjectsComboBox->SetSelectedItem(DebugObjectNames[ObjectIndex]);
				bMatchFound = true;
				break;
			}
		}

		// No match found, use the default option
		if (!bMatchFound)
		{
			DebugObjectsComboBox->SetSelectedItem(DebugObjectNames[0]);
		}
	}

	// Finally ensure we have a valid selection
	if (DebugObjectsComboBox.IsValid())
	{
		TSharedPtr<FString> CurrentSelection = DebugObjectsComboBox->GetSelectedItem();
		if (DebugObjectNames.Find(CurrentSelection) == INDEX_NONE)
		{
			if (DebugObjectNames.Num() > 0)
			{
				DebugObjectsComboBox->SetSelectedItem(DebugObjectNames[0]);
			}
			else
			{
				DebugObjectsComboBox->ClearSelection();
			}
		}

		DebugObjectsComboBox->RefreshOptions();
	}
}

EVisibility SBlueprintEditorSelectedDebugObjectWidget::ShouldShowDebugObjectPicker() const
{
	check(GetBlueprintObj());
	return FBlueprintEditorUtils::IsLevelScriptBlueprint(GetBlueprintObj()) ? EVisibility::Collapsed : EVisibility::Visible;
}

TSharedPtr<FString> SBlueprintEditorSelectedDebugObjectWidget::GetDebugObjectName() const
{
	check(GetBlueprintObj());
	check(DebugObjects.Num() == DebugObjectNames.Num());
	if (UObject* DebugObj = GetBlueprintObj()->GetObjectBeingDebugged())
	{
		for (int32 ObjectIndex = 0; ObjectIndex < DebugObjects.Num(); ++ObjectIndex)
		{
			if (DebugObjects[ObjectIndex].IsValid() && (DebugObjects[ObjectIndex].Get() == DebugObj))
			{
				return DebugObjectNames[ObjectIndex];
			}
		}
	}

	return DebugObjectNames[0];
}

TSharedPtr<FString> SBlueprintEditorSelectedDebugObjectWidget::GetDebugWorldName() const
{
	check(GetBlueprintObj());
	check(DebugWorlds.Num() == DebugWorldNames.Num());
	if (UObject* DebugObj = GetBlueprintObj()->GetObjectBeingDebugged())
	{
		for (int32 WorldIdx = 0; WorldIdx < DebugWorlds.Num(); ++WorldIdx)
		{
			if (DebugObj->IsIn(DebugWorlds[WorldIdx].Get()))
			{
				return DebugWorldNames[WorldIdx];
			}
		}
	}
	return DebugWorldNames[0];

}

void SBlueprintEditorSelectedDebugObjectWidget::DebugWorldSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!NewSelection.IsValid())
	{
		return;
	}

	check(DebugObjects.Num() == DebugObjectNames.Num());
	for (int32 WorldIdx = 0; WorldIdx < DebugWorldNames.Num(); ++WorldIdx)
	{
		if (*DebugWorldNames[WorldIdx] == *NewSelection)
		{
			GetBlueprintObj()->SetWorldBeingDebugged(DebugWorlds[WorldIdx].Get());
			GenerateDebugObjectNames(false);
		}
	}
}

void SBlueprintEditorSelectedDebugObjectWidget::DebugObjectSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	check(DebugObjects.Num() == DebugObjectNames.Num());
	for (int32 ObjectIndex = 0; ObjectIndex < DebugObjectNames.Num(); ++ObjectIndex)
	{
		if (*DebugObjectNames[ObjectIndex] == *NewSelection)
		{
			UObject* DebugObj = DebugObjects[ObjectIndex].IsValid() ? DebugObjects[ObjectIndex].Get() : NULL;
			GetBlueprintObj()->SetObjectBeingDebugged(DebugObj);
		}
	}
}

EVisibility SBlueprintEditorSelectedDebugObjectWidget::IsSelectDebugObjectButtonVisible() const
{
	check(GetBlueprintObj());
	if (UObject* DebugObj = GetBlueprintObj()->GetObjectBeingDebugged())
	{
		if (AActor* Actor = Cast<AActor>(DebugObj))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

void SBlueprintEditorSelectedDebugObjectWidget::SelectedDebugObject_OnClicked()
{
	if (UObject* DebugObj = GetBlueprintObj()->GetObjectBeingDebugged())
	{
		if (AActor* Actor = Cast<AActor>(DebugObj))
		{
			GEditor->SelectNone(false, true, false);
			GEditor->SelectActor(Actor, true, true, true);
			GUnrealEd->Exec(Actor->GetWorld(), TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY"));
		}
	}
}

EVisibility SBlueprintEditorSelectedDebugObjectWidget::IsDebugWorldComboVisible() const
{
	if (GEditor->PlayWorld != NULL)
	{
		const TArray<FWorldContext> &WorldContexts = GEngine->GetWorldContexts();
		int32 LocalWorldCount = 0;
		for (int32 i = 0; i < WorldContexts.Num() && LocalWorldCount <= 1; ++i)
		{
			if (WorldContexts[i].WorldType == EWorldType::PIE && WorldContexts[i].World() != NULL)
			{
				LocalWorldCount++;
			}
		}

		if (LocalWorldCount > 1)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

void SBlueprintEditorSelectedDebugObjectWidget::AddDebugObject(UObject* TestObject)
{
	FString Label;
	if (AActor* Actor = Cast<AActor>(TestObject))
	{
		Label = Actor->GetActorLabel();
	}
	else
	{
		if (AActor* ParentActor = TestObject->GetTypedOuter<AActor>())
		{
			// This gives the most precision, but is pretty long for the combo box
			//const FString RelativePath = TestObject->GetPathName(/*StopOuter=*/ ParentActor);
			const FString RelativePath = TestObject->GetName();
			Label = FString::Printf(TEXT("%s in %s"), *RelativePath, *ParentActor->GetActorLabel());
		}
		else
		{
			Label = TestObject->GetName();
		}
	}

	AddDebugObjectWithName(TestObject, Label);
}

void SBlueprintEditorSelectedDebugObjectWidget::AddDebugObjectWithName(UObject* TestObject, const FString& TestObjectName)
{
	DebugObjects.Add(TestObject);
	DebugObjectNames.Add(MakeShareable(new FString(TestObjectName)));
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
