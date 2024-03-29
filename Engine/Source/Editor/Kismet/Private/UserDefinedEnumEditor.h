// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEditorModule.h"

#include "Editor/PropertyEditor/Public/PropertyEditing.h"
#include "Editor/UnrealEd/Public/Kismet2/EnumEditorUtils.h"

class KISMET_API FUserDefinedEnumEditor : public IUserDefinedEnumEditor
{
	/** App Identifier.*/
	static const FName UserDefinedEnumEditorAppIdentifier;

	/**	The tab ids for all the tabs used */
	static const FName EnumeratorsTabId;
	
	/** Property viewing widget */
	TSharedPtr<class IDetailsView> PropertyView;

public:
	/**
	 * Edits the specified enum
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	EnumToEdit				The user defined enum to edit
	 */
	void InitEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UUserDefinedEnum* EnumToEdit);

	/** Destructor */
	virtual ~FUserDefinedEnumEditor();

	/** IToolkit interface */
	virtual FName GetToolkitFName() const OVERRIDE;
	virtual FText GetBaseToolkitName() const OVERRIDE;
	virtual FText GetToolkitName() const OVERRIDE;
	virtual FString GetWorldCentricTabPrefix() const OVERRIDE;
	virtual FLinearColor GetWorldCentricTabColorScale() const OVERRIDE;

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) OVERRIDE;

protected:
	TSharedRef<SDockTab> SpawnEnumeratorsTab(const FSpawnTabArgs& Args);
};

/** Details customization for functions and graphs selected in the MyBlueprint panel */
class FEnumDetails : public IDetailCustomization, FEnumEditorUtils::INotifyOnEnumChanged
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FEnumDetails);
	}

	FEnumDetails()
		: TargetEnum(NULL)
	{
	}

	~FEnumDetails();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) OVERRIDE;

	/** Forces a refresh on the details customization */
	void OnForceRefresh();

	/** FEnumEditorUtils::INotifyOnEnumChanged */
	virtual void OnChanged(const class UUserDefinedEnum* Enum) OVERRIDE;

private:
	/** Handles new enum element request */
	FReply OnAddNewEnumerator();

private:
	TSharedPtr<class FUserDefinedEnumLayout> Layout;
	/** The target node that this argument is on */
	TWeakObjectPtr<UUserDefinedEnum> TargetEnum;
};

/** Custom struct for each group of arguments in the function editing details */
class FUserDefinedEnumLayout : public IDetailCustomNodeBuilder, public TSharedFromThis<FUserDefinedEnumLayout>
{
public:
	FUserDefinedEnumLayout(UUserDefinedEnum* InTargetEnum)
		: TargetEnum(InTargetEnum)
	{}

	void Refresh()
	{
		OnRebuildChildren.ExecuteIfBound();
	}

	bool CausedChange() const;

private:
	/** IDetailCustomNodeBuilder Interface*/
	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) OVERRIDE { OnRebuildChildren = InOnRegenerateChildren; }
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) OVERRIDE {}
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) OVERRIDE;
	virtual void Tick( float DeltaTime ) OVERRIDE {}
	virtual bool RequiresTick() const OVERRIDE { return false; }
	virtual FName GetName() const OVERRIDE { return NAME_None; }
	virtual bool InitiallyCollapsed() const OVERRIDE { return false; }

private:
	FSimpleDelegate OnRebuildChildren;
	/** The target node that this argument is on */
	TWeakObjectPtr<UUserDefinedEnum> TargetEnum;

	TArray<TWeakPtr<class FUserDefinedEnumIndexLayout>> Children;
};

/** Custom struct for each group of arguments in the function editing details */
class FUserDefinedEnumIndexLayout : public IDetailCustomNodeBuilder, public TSharedFromThis<FUserDefinedEnumIndexLayout>
{
public:
	FUserDefinedEnumIndexLayout(UUserDefinedEnum* InTargetEnum, int32 InEnumeratorIndex)
		: TargetEnum(InTargetEnum)
		, EnumeratorIndex(InEnumeratorIndex)
		, bCausedChange(false)
	{}

	bool CausedChange() const { return bCausedChange; }

private:
	/** IDetailCustomNodeBuilder Interface*/
	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) OVERRIDE {};
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) OVERRIDE;
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) OVERRIDE {};
	virtual void Tick( float DeltaTime ) OVERRIDE {}
	virtual bool RequiresTick() const OVERRIDE { return false; }
	virtual FName GetName() const OVERRIDE { return NAME_None; }
	virtual bool InitiallyCollapsed() const OVERRIDE { return false; }

private:
	/** Retrieves the enumerator's name */
	FText GetEnumeratorName() const;

	/** Moves the enumerator up in the list */
	FReply OnMoveEnumeratorUp();
	
	/** Moves the enumerator down in the list */
	FReply OnMoveEnumeratorDown();

	/** Deletes the enumerator */
	void OnEnumeratorRemove();

	/** Callback when the enumerator's name is committed */
	void OnEnumeratorNameCommitted(const FText& NewText, ETextCommit::Type InTextCommit);
	
	/** Callback when changing the enumerator's name to verify the name */
	void OnEnumeratorNameChanged(const FText& NewText);

	/** 
	 * Helper function to validate the enumerator's display name
	 *
	 * @param NewText		The name to validate
	 *
	 * @return				Returns true if the name is valid
	 */
	bool IsValidEnumeratorDisplayName(const FText& NewText) const;

private:
	/** The target node that this argument is on */
	TWeakObjectPtr<UUserDefinedEnum> TargetEnum;

	/** Index of enumerator */
	int32 EnumeratorIndex;

	/** The enumerator's name widget, used for setting a enumerator's name */
	TWeakPtr< SEditableTextBox > EnumeratorNameWidget;

	bool bCausedChange;
};