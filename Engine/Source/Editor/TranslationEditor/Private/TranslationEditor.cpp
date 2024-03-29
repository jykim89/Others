// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "TranslationEditorPrivatePCH.h"

#include "TranslationEditor.h"
#include "Toolkits/IToolkitHost.h"
#include "WorkspaceMenuStructureModule.h"
#include "MessageLog.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IPropertyTable.h"
#include "Editor/PropertyEditor/Public/IPropertyTableColumn.h"
#include "Editor/PropertyEditor/Public/IPropertyTableRow.h"
#include "Editor/PropertyEditor/Public/IPropertyTableCell.h"
#include "Editor/PropertyEditor/Public/PropertyHandle.h"
#include "Editor/PropertyEditor/Public/PropertyPath.h"
#include "CustomFontColumn.h"
#include "DesktopPlatformModule.h"
#include "IPropertyTableWidgetHandle.h"

#include "TranslationUnit.h"
#include "SSearchBox.h"
#include "InternationalizationExportSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LocalizationExport, Log, All);

#define LOCTEXT_NAMESPACE "TranslationEditor"

const FName FTranslationEditor::UntranslatedTabId( TEXT( "TranslationEditor_Untranslated" ) );
const FName FTranslationEditor::ReviewTabId( TEXT( "TranslationEditor_Review" ) );
const FName FTranslationEditor::CompletedTabId( TEXT( "TranslationEditor_Completed" ) );
const FName FTranslationEditor::PreviewTabId( TEXT( "TranslationEditor_Preview" ) );
const FName FTranslationEditor::ContextTabId( TEXT( "TranslationEditor_Context" ) );
const FName FTranslationEditor::HistoryTabId( TEXT( "TranslationEditor_History" ) );
const FName FTranslationEditor::SearchTabId( TEXT( "TranslationEditor_Search" ) );
const FName FTranslationEditor::ChangedOnImportTabId( TEXT( "TranslationEditor_ChangedOnImport" ) );

void FTranslationEditor::Initialize()
{
	// Set up delegate functions for the buttons/spinboxes in the custom font columns' headers
	SourceColumn->SetOnChangeFontButtonClicked(FOnClicked::CreateSP(this, &FTranslationEditor::ChangeSourceFont_FReply));
	SourceColumn->SetOnFontSizeValueCommitted(FOnInt32ValueCommitted::CreateSP(this, &FTranslationEditor::OnSourceFontSizeCommitt));
	TranslationColumn->SetOnChangeFontButtonClicked(FOnClicked::CreateSP(this, &FTranslationEditor::ChangeTranslationTargetFont_FReply));
	TranslationColumn->SetOnFontSizeValueCommitted(FOnInt32ValueCommitted::CreateSP(this, &FTranslationEditor::OnTranslationTargetFontSizeCommitt));
}

void FTranslationEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(TabManager);

	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	TabManager->RegisterTabSpawner( UntranslatedTabId, FOnSpawnTab::CreateSP(this, &FTranslationEditor::SpawnTab_Untranslated) )
		.SetDisplayName( LOCTEXT("UntranslatedTab", "Untranslated") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );

	TabManager->RegisterTabSpawner( ReviewTabId, FOnSpawnTab::CreateSP(this, &FTranslationEditor::SpawnTab_Review) )
		.SetDisplayName( LOCTEXT("ReviewTab", "Needs Review") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );

	TabManager->RegisterTabSpawner( CompletedTabId, FOnSpawnTab::CreateSP(this, &FTranslationEditor::SpawnTab_Completed) )
		.SetDisplayName( LOCTEXT("CompletedTab", "Completed") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );

	TabManager->RegisterTabSpawner( PreviewTabId, FOnSpawnTab::CreateSP(this, &FTranslationEditor::SpawnTab_Preview) )
		.SetDisplayName( LOCTEXT("PreviewTab", "Preview") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );

	TabManager->RegisterTabSpawner( ContextTabId, FOnSpawnTab::CreateSP(this, &FTranslationEditor::SpawnTab_Context) )
		.SetDisplayName( LOCTEXT("ContextTab", "Context") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );

	TabManager->RegisterTabSpawner( HistoryTabId, FOnSpawnTab::CreateSP(this, &FTranslationEditor::SpawnTab_History) )
		.SetDisplayName( LOCTEXT("HistoryTab", "History") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );

	TabManager->RegisterTabSpawner( SearchTabId, FOnSpawnTab::CreateSP(this, &FTranslationEditor::SpawnTab_Search) )
		.SetDisplayName(LOCTEXT("SearchTab", "Search"))
		.SetGroup(MenuStructure.GetAssetEditorCategory());

	TabManager->RegisterTabSpawner( ChangedOnImportTabId, FOnSpawnTab::CreateSP(this, &FTranslationEditor::SpawnTab_ChangedOnImport) )
		.SetDisplayName(LOCTEXT("ChangedOnImportTab", "ChangedOnImport"))
		.SetGroup(MenuStructure.GetAssetEditorCategory());
}

void FTranslationEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	TabManager->UnregisterTabSpawner( UntranslatedTabId );
	TabManager->UnregisterTabSpawner( ReviewTabId );
	TabManager->UnregisterTabSpawner( CompletedTabId );
	TabManager->UnregisterTabSpawner( PreviewTabId );
	TabManager->UnregisterTabSpawner( ContextTabId );
	TabManager->UnregisterTabSpawner(HistoryTabId);
	TabManager->UnregisterTabSpawner(SearchTabId);
	TabManager->UnregisterTabSpawner(ChangedOnImportTabId);
}

void FTranslationEditor::InitTranslationEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost )
{	
	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_TranslationEditor_Layout" )
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->SetHideTabWell( true )
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.5)
			->SetHideTabWell( false )
			->AddTab( UntranslatedTabId, ETabState::OpenedTab )
			->AddTab( ReviewTabId,  ETabState::OpenedTab )
			->AddTab( CompletedTabId,  ETabState::OpenedTab )
			->AddTab( SearchTabId, ETabState::ClosedTab )
			->AddTab( ChangedOnImportTabId, ETabState::ClosedTab )
		)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.5)
			->SetHideTabWell(false)
			->AddTab(PreviewTabId, ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewSplitter()
			->Split
			(
				FTabManager::NewStack()
				->SetHideTabWell(false)
				->AddTab(ContextTabId, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetHideTabWell(false)
				->AddTab(HistoryTabId, ETabState::OpenedTab)
			)
		)
	);

	// Register the UI COMMANDS and map them to our functions
	MapActions();

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	// Need editing object to not be null
	UTranslationUnit* EditingObject;
	if (DataManager->GetAllTranslationsArray().Num() > 0 && DataManager->GetAllTranslationsArray()[0] != NULL)
	{
		EditingObject = DataManager->GetAllTranslationsArray()[0];
	}
	else
	{
		EditingObject = NewObject<UTranslationUnit>();
	}
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, FTranslationEditorModule::TranslationEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, EditingObject);
	
	FTranslationEditorModule& TranslationEditorModule = FModuleManager::LoadModuleChecked<FTranslationEditorModule>( "TranslationEditor" );
	AddMenuExtender(TranslationEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
	TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender);
	FTranslationEditorMenu::SetupTranslationEditorMenu( MenuExtender, *this );
	AddMenuExtender(MenuExtender);

	AddToolbarExtender(TranslationEditorModule.GetToolbarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	FTranslationEditorMenu::SetupTranslationEditorToolbar( ToolbarExtender, *this );
	AddToolbarExtender(ToolbarExtender);

	RegenerateMenusAndToolbars();

	// @todo toolkit world centric editing
	/*// Setup our tool's layout
	if( IsWorldCentricAssetEditor() )
	{
		const FString TabInitializationPayload(TEXT(""));		// NOTE: Payload not currently used for table properties
		SpawnToolkitTab( UntranslatedTabId, TabInitializationPayload, EToolkitTabSpot::Details );
	}*/

	// NOTE: Could fill in asset editor commands here!
}

FName FTranslationEditor::GetToolkitFName() const
{
	return FName("TranslationEditor");
}

FText FTranslationEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "Translation Editor" );
}

FText FTranslationEditor::GetToolkitName() const
{
	const UObject* EditingObject = GetEditingObject();

	check (EditingObject != NULL);

	// This doesn't correctly indicate dirty status for Translation Editor currently...
	const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

	FFormatNamedArguments Args;
	Args.Add(TEXT("Language"), FText::FromString(FPaths::GetBaseFilename(FPaths::GetPath(ArchiveFilePath))));
	Args.Add(TEXT("ProjectName"), FText::FromString(FPaths::GetBaseFilename(ManifestFilePath)));
	Args.Add( TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty() );
	Args.Add( TEXT("ToolkitName"), GetBaseToolkitName() );
	return FText::Format( LOCTEXT("TranslationEditorAppLabel", "{Language}{DirtyState} - {ProjectName} - {ToolkitName}"), Args );
}

FString FTranslationEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Translation ").ToString();
}

FLinearColor FTranslationEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

TSharedRef<SDockTab> FTranslationEditor::SpawnTab_Untranslated( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == UntranslatedTabId );

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

	UProperty* SourceProperty = FindField<UProperty>( UTranslationUnit::StaticClass(), "Source");
	UProperty* TranslationProperty = FindField<UProperty>( UTranslationUnit::StaticClass(), "Translation");

	// create empty property table
	UntranslatedPropertyTable = PropertyEditorModule.CreatePropertyTable();
	UntranslatedPropertyTable->SetIsUserAllowedToChangeRoot( false );
	UntranslatedPropertyTable->SetOrientation( EPropertyTableOrientation::AlignPropertiesInColumns );
	UntranslatedPropertyTable->SetShowRowHeader( true );
	UntranslatedPropertyTable->SetShowObjectName( false );
	UntranslatedPropertyTable->OnSelectionChanged()->AddSP( this, &FTranslationEditor::UpdateUntranslatedSelection );

	// we want to customize some columns
	TArray< TSharedRef<class IPropertyTableCustomColumn>> CustomColumns;
	SourceColumn->AddSupportedProperty(SourceProperty);
	TranslationColumn->AddSupportedProperty(TranslationProperty);
	CustomColumns.Add( SourceColumn );
	CustomColumns.Add(TranslationColumn);

	UntranslatedPropertyTable->SetObjects((TArray<UObject*>&)DataManager->GetUntranslatedArray());

	// Add the columns we want to display
	UntranslatedPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)SourceProperty);
	UntranslatedPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)TranslationProperty);

	// Freeze columns, don't want user to remove them
	TArray<TSharedRef<IPropertyTableColumn>> Columns = UntranslatedPropertyTable->GetColumns();
	for (TSharedRef<IPropertyTableColumn> Column : Columns)
	{
		Column->SetFrozen(true);
	}

	UntranslatedPropertyTableWidgetHandle = PropertyEditorModule.CreatePropertyTableWidgetHandle( UntranslatedPropertyTable.ToSharedRef(), CustomColumns);
	TSharedRef<SWidget> PropertyTableWidget = UntranslatedPropertyTableWidgetHandle->GetWidget();

	TSharedRef<SDockTab> NewDockTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("TranslationEditor.Tabs.Properties") )
		.Label( LOCTEXT("UntranslatedTabTitle", "Untranslated") )
		.TabColorScale( GetTabColorScale() )
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			.Padding(0.0f)
			[
				PropertyTableWidget
			]
		];

	UntranslatedTab = NewDockTab;

	return NewDockTab;
}

TSharedRef<SDockTab> FTranslationEditor::SpawnTab_Review( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == ReviewTabId );

	UProperty* SourceProperty = FindField<UProperty>( UTranslationUnit::StaticClass(), "Source");
	UProperty* TranslationProperty = FindField<UProperty>( UTranslationUnit::StaticClass(), "Translation");

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

	// create empty property table
	ReviewPropertyTable = PropertyEditorModule.CreatePropertyTable();
	ReviewPropertyTable->SetIsUserAllowedToChangeRoot( false );
	ReviewPropertyTable->SetOrientation( EPropertyTableOrientation::AlignPropertiesInColumns );
	ReviewPropertyTable->SetShowRowHeader( true );
	ReviewPropertyTable->SetShowObjectName( false );
	ReviewPropertyTable->OnSelectionChanged()->AddSP( this, &FTranslationEditor::UpdateNeedsReviewSelection );

	// we want to customize some columns
	TArray< TSharedRef< class IPropertyTableCustomColumn > > CustomColumns;
	SourceColumn->AddSupportedProperty(SourceProperty);
	TranslationColumn->AddSupportedProperty(TranslationProperty);
	CustomColumns.Add( SourceColumn );
	CustomColumns.Add( TranslationColumn );

	ReviewPropertyTable->SetObjects((TArray<UObject*>&)DataManager->GetReviewArray());

	// Add the columns we want to display
	ReviewPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>( UTranslationUnit::StaticClass(), "Source"));
	ReviewPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>( UTranslationUnit::StaticClass(), "Translation"));
	ReviewPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>( UTranslationUnit::StaticClass(), "HasBeenReviewed"));

	TArray<TSharedRef<IPropertyTableColumn>> Columns = ReviewPropertyTable->GetColumns();
	for (TSharedRef<IPropertyTableColumn> Column : Columns)
	{
		FString ColumnId = Column->GetId().ToString();
		if (ColumnId == "HasBeenReviewed")
		{
			Column->SetWidth(120);
			Column->SetSizeMode(EPropertyTableColumnSizeMode::Fixed);
		}
		// Freeze columns, don't want user to remove them
		Column->SetFrozen(true);
	}

	ReviewPropertyTableWidgetHandle = PropertyEditorModule.CreatePropertyTableWidgetHandle( ReviewPropertyTable.ToSharedRef(), CustomColumns);
	TSharedRef<SWidget> PropertyTableWidget = ReviewPropertyTableWidgetHandle->GetWidget();

	TSharedRef<SDockTab> NewDockTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("TranslationEditor.Tabs.Properties") )
		.Label( LOCTEXT("ReviewTabTitle", "Needs Review") )
		.TabColorScale( GetTabColorScale() )
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			.Padding(0.0f)
			[
				PropertyTableWidget
			]
		];

	ReviewTab = NewDockTab;

	return NewDockTab;
}

TSharedRef<SDockTab> FTranslationEditor::SpawnTab_Completed( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == CompletedTabId );

	UProperty* SourceProperty = FindField<UProperty>( UTranslationUnit::StaticClass(), "Source");
	UProperty* TranslationProperty = FindField<UProperty>( UTranslationUnit::StaticClass(), "Translation");

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

	// create empty property table
	CompletedPropertyTable = PropertyEditorModule.CreatePropertyTable();
	CompletedPropertyTable->SetIsUserAllowedToChangeRoot( false );
	CompletedPropertyTable->SetOrientation( EPropertyTableOrientation::AlignPropertiesInColumns );
	CompletedPropertyTable->SetShowRowHeader( true );
	CompletedPropertyTable->SetShowObjectName( false );
	CompletedPropertyTable->OnSelectionChanged()->AddSP( this, &FTranslationEditor::UpdateCompletedSelection );

	// we want to customize some columns
	TArray< TSharedRef< class IPropertyTableCustomColumn > > CustomColumns;
	SourceColumn->AddSupportedProperty(SourceProperty);
	TranslationColumn->AddSupportedProperty(TranslationProperty);
	CustomColumns.Add( SourceColumn );
	CustomColumns.Add( TranslationColumn );

	CompletedPropertyTable->SetObjects((TArray<UObject*>&)DataManager->GetCompleteArray());

	// Add the columns we want to display
	CompletedPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>( UTranslationUnit::StaticClass(), "Source"));
	CompletedPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>( UTranslationUnit::StaticClass(), "Translation"));

	// Freeze columns, don't want user to remove them
	TArray<TSharedRef<IPropertyTableColumn>> Columns = CompletedPropertyTable->GetColumns();
	for (TSharedRef<IPropertyTableColumn> Column : Columns)
	{
		Column->SetFrozen(true);
	}

	CompletedPropertyTableWidgetHandle = PropertyEditorModule.CreatePropertyTableWidgetHandle( CompletedPropertyTable.ToSharedRef(), CustomColumns);
	TSharedRef<SWidget> PropertyTableWidget = CompletedPropertyTableWidgetHandle->GetWidget();

	TSharedRef<SDockTab> NewDockTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("TranslationEditor.Tabs.Properties") )
		.Label( LOCTEXT("CompletedTabTitle", "Completed") )
		.TabColorScale( GetTabColorScale() )
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			.Padding(0.0f)
			[
				PropertyTableWidget
			]
		];

	CompletedTab = NewDockTab;

	return NewDockTab;
}

TSharedRef<SDockTab> FTranslationEditor::SpawnTab_Search(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == SearchTabId);

	UProperty* SourceProperty = FindField<UProperty>(UTranslationUnit::StaticClass(), "Source");
	UProperty* TranslationProperty = FindField<UProperty>(UTranslationUnit::StaticClass(), "Translation");

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// create empty property table
	SearchPropertyTable = PropertyEditorModule.CreatePropertyTable();
	SearchPropertyTable->SetIsUserAllowedToChangeRoot(false);
	SearchPropertyTable->SetOrientation(EPropertyTableOrientation::AlignPropertiesInColumns);
	SearchPropertyTable->SetShowRowHeader(true);
	SearchPropertyTable->SetShowObjectName(false);
	SearchPropertyTable->OnSelectionChanged()->AddSP(this, &FTranslationEditor::UpdateSearchSelection);

	// we want to customize some columns
	TArray< TSharedRef< class IPropertyTableCustomColumn > > CustomColumns;
	SourceColumn->AddSupportedProperty(SourceProperty);
	TranslationColumn->AddSupportedProperty(TranslationProperty);
	CustomColumns.Add(SourceColumn);
	CustomColumns.Add(TranslationColumn);

	SearchPropertyTable->SetObjects((TArray<UObject*>&)DataManager->GetSearchResultsArray());

	// Add the columns we want to display
	SearchPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(UTranslationUnit::StaticClass(), "Source"));
	SearchPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(UTranslationUnit::StaticClass(), "Translation"));

	// Freeze columns, don't want user to remove them
	TArray<TSharedRef<IPropertyTableColumn>> Columns = SearchPropertyTable->GetColumns();
	for (TSharedRef<IPropertyTableColumn> Column : Columns)
	{
		Column->SetFrozen(true);
	}

	SearchPropertyTableWidgetHandle = PropertyEditorModule.CreatePropertyTableWidgetHandle(SearchPropertyTable.ToSharedRef(), CustomColumns);
	TSharedRef<SWidget> PropertyTableWidget = SearchPropertyTableWidgetHandle->GetWidget();

	TSharedRef<SDockTab> NewDockTab = SNew(SDockTab)
		//.Icon(FEditorStyle::GetBrush("TranslationEditor.Tabs.Properties"))
		.Label(LOCTEXT("SearchTabTitle", "Search"))
		.TabColorScale(GetTabColorScale())
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SAssignNew(SearchBox, SSearchBox)
				.HintText(LOCTEXT("FilterSearch", "Search..."))
				.ToolTipText(LOCTEXT("FilterSearchHint", "Type here to search").ToString())
				.OnTextChanged(this, &FTranslationEditor::OnFilterTextChanged)
				.OnTextCommitted(this, &FTranslationEditor::OnFilterTextCommitted)
			]
			+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				.FillHeight(10.f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(0.0f)
				.VAlign(VAlign_Top)
				[
					PropertyTableWidget
				]
			]
		];

	SearchTab = NewDockTab;

	return NewDockTab;
}

TSharedRef<SDockTab> FTranslationEditor::SpawnTab_ChangedOnImport(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == ChangedOnImportTabId);

	UProperty* SourceProperty = FindField<UProperty>(UTranslationUnit::StaticClass(), "Source");
	UProperty* TranslationBeforeImportProperty = FindField<UProperty>(UTranslationUnit::StaticClass(), "TranslationBeforeImport");
	UProperty* TranslationProperty = FindField<UProperty>(UTranslationUnit::StaticClass(), "Translation");

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// create empty property table
	ChangedOnImportPropertyTable = PropertyEditorModule.CreatePropertyTable();
	ChangedOnImportPropertyTable->SetIsUserAllowedToChangeRoot(false);
	ChangedOnImportPropertyTable->SetOrientation(EPropertyTableOrientation::AlignPropertiesInColumns);
	ChangedOnImportPropertyTable->SetShowRowHeader(true);
	ChangedOnImportPropertyTable->SetShowObjectName(false);
	ChangedOnImportPropertyTable->OnSelectionChanged()->AddSP(this, &FTranslationEditor::UpdateSearchSelection);

	// we want to customize some columns
	TArray< TSharedRef< class IPropertyTableCustomColumn > > CustomColumns;
	SourceColumn->AddSupportedProperty(SourceProperty);
	TranslationColumn->AddSupportedProperty(TranslationProperty);
	CustomColumns.Add(SourceColumn);
	CustomColumns.Add(TranslationColumn);

	ChangedOnImportPropertyTable->SetObjects((TArray<UObject*>&)DataManager->GetSearchResultsArray());

	// Add the columns we want to display
	ChangedOnImportPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(UTranslationUnit::StaticClass(), "Source"));
	ChangedOnImportPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(UTranslationUnit::StaticClass(), "TranslationBeforeImport"));
	ChangedOnImportPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(UTranslationUnit::StaticClass(), "Translation"));

	// Freeze columns, don't want user to remove them
	TArray<TSharedRef<IPropertyTableColumn>> Columns = ChangedOnImportPropertyTable->GetColumns();
	for (TSharedRef<IPropertyTableColumn> Column : Columns)
	{
		Column->SetFrozen(true);
	}

	SearchPropertyTableWidgetHandle = PropertyEditorModule.CreatePropertyTableWidgetHandle(ChangedOnImportPropertyTable.ToSharedRef(), CustomColumns);
	TSharedRef<SWidget> PropertyTableWidget = SearchPropertyTableWidgetHandle->GetWidget();

	TSharedRef<SDockTab> NewDockTab = SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("TranslationEditor.Tabs.Properties"))
		.Label(LOCTEXT("ChangedOnImportTabTitle", "Changed on Import"))
		.TabColorScale(GetTabColorScale())
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(0.0f)
			[
				PropertyTableWidget
			]
		];

	ChangedOnImportTab = NewDockTab;

	return NewDockTab;
}

TSharedRef<SDockTab> FTranslationEditor::SpawnTab_Preview( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == PreviewTabId );

	TSharedRef<SDockTab> NewDockTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("TranslationEditor.Tabs.Properties") )
		.Label( LOCTEXT("PreviewTabTitle", "Preview") )
		.TabColorScale( GetTabColorScale() )
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			.Padding(0.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					PreviewTextBlock
				]
			]
		];
	
	return NewDockTab;
}

TSharedRef<SDockTab> FTranslationEditor::SpawnTab_Context( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == ContextTabId );

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

	// create empty property table
	ContextPropertyTable = PropertyEditorModule.CreatePropertyTable();
	ContextPropertyTable->SetIsUserAllowedToChangeRoot( false );
	ContextPropertyTable->SetOrientation( EPropertyTableOrientation::AlignPropertiesInColumns );
	ContextPropertyTable->SetShowRowHeader( true );
	ContextPropertyTable->SetShowObjectName( false );
	ContextPropertyTable->OnSelectionChanged()->AddSP( this, &FTranslationEditor::UpdateContextSelection );

	if (DataManager->GetAllTranslationsArray().Num() > 0)
	{
		TArray<UObject*> Objects;
		Objects.Add(DataManager->GetAllTranslationsArray()[0]);
		ContextPropertyTable->SetObjects(Objects);
	}

	// Build the Path to the data we want to show
	UProperty* ContextProp = FindField<UProperty>( UTranslationUnit::StaticClass(), "Contexts" );
	FPropertyInfo ContextPropInfo;
	ContextPropInfo.Property = ContextProp;
	ContextPropInfo.ArrayIndex = INDEX_NONE;
	TSharedRef<FPropertyPath> Path = FPropertyPath::CreateEmpty();
	Path = Path->ExtendPath(ContextPropInfo);
	ContextPropertyTable->SetRootPath(Path);

	// Add the columns we want to display
	ContextPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>( FTranslationContextInfo::StaticStruct(), "Key"));
	ContextPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>( FTranslationContextInfo::StaticStruct(), "Context"));

	// Freeze columns, don't want user to remove them
	TArray<TSharedRef<IPropertyTableColumn>> Columns = ContextPropertyTable->GetColumns();
	for (TSharedRef<IPropertyTableColumn> Column : Columns)
	{
		Column->SetFrozen(true);
	}

	ContextPropertyTableWidgetHandle = PropertyEditorModule.CreatePropertyTableWidgetHandle( ContextPropertyTable.ToSharedRef() );
	TSharedRef<SWidget> PropertyTableWidget = ContextPropertyTableWidgetHandle->GetWidget();

	TSharedRef<SDockTab> NewDockTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("TranslationEditor.Tabs.Properties") )
		.Label( LOCTEXT("ContextTabTitle", "Context") )
		.TabColorScale( GetTabColorScale() )
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			.Padding(0.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.FillHeight(0.1)
				[
					NamespaceTextBlock
				]
				+SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					PropertyTableWidget
				]
			]
		];
	
	return NewDockTab;
}

TSharedRef<SDockTab> FTranslationEditor::SpawnTab_History( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == HistoryTabId );

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

	UProperty* SourceProperty = FindField<UProperty>( FTranslationChange::StaticStruct(), "Source");
	UProperty* TranslationProperty = FindField<UProperty>( FTranslationChange::StaticStruct(), "Translation");

	// create empty property table
	HistoryPropertyTable = PropertyEditorModule.CreatePropertyTable();
	HistoryPropertyTable->SetIsUserAllowedToChangeRoot( false );
	HistoryPropertyTable->SetOrientation( EPropertyTableOrientation::AlignPropertiesInColumns );
	HistoryPropertyTable->SetShowRowHeader( true );
	HistoryPropertyTable->SetShowObjectName( false );

	// we want to customize some columns
	TArray< TSharedRef<class IPropertyTableCustomColumn>> CustomColumns;
	SourceColumn->AddSupportedProperty(SourceProperty);
	TranslationColumn->AddSupportedProperty(TranslationProperty);
	CustomColumns.Add( SourceColumn );
	CustomColumns.Add( TranslationColumn );

	if (DataManager->GetAllTranslationsArray().Num() > 0)
	{
		TArray<UObject*> Objects;
		Objects.Add(DataManager->GetAllTranslationsArray()[0]);
		HistoryPropertyTable->SetObjects(Objects);
	}

	// Build the Path to the data we want to show
	TSharedRef<FPropertyPath> Path = FPropertyPath::CreateEmpty();
	UArrayProperty* ContextsProp = FindField<UArrayProperty>( UTranslationUnit::StaticClass(), "Contexts" );
	Path = Path->ExtendPath(FPropertyPath::Create(ContextsProp));
	FPropertyInfo ContextsPropInfo;
	ContextsPropInfo.Property = ContextsProp->Inner;
	ContextsPropInfo.ArrayIndex = 0;
	Path = Path->ExtendPath(ContextsPropInfo);

	UProperty* ChangesProp = FindField<UProperty>( FTranslationContextInfo::StaticStruct(), "Changes" );
	FPropertyInfo ChangesPropInfo;
	ChangesPropInfo.Property = ChangesProp;
	ChangesPropInfo.ArrayIndex = INDEX_NONE;
	Path = Path->ExtendPath(ChangesPropInfo);
	HistoryPropertyTable->SetRootPath(Path);

	// Add the columns we want to display
	HistoryPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>( FTranslationChange::StaticStruct(), "Version"));
	HistoryPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>( FTranslationChange::StaticStruct(), "DateAndTime"));
	HistoryPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)SourceProperty);
	HistoryPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)TranslationProperty);

	// Freeze columns, don't want user to remove them
	TArray<TSharedRef<IPropertyTableColumn>> Columns = HistoryPropertyTable->GetColumns();
	for (TSharedRef<IPropertyTableColumn> Column : Columns)
	{
		Column->SetFrozen(true);
	}

	HistoryPropertyTableWidgetHandle = PropertyEditorModule.CreatePropertyTableWidgetHandle( HistoryPropertyTable.ToSharedRef(), CustomColumns );
	TSharedRef<SWidget> PropertyTableWidget = HistoryPropertyTableWidgetHandle->GetWidget();

	TSharedRef<SDockTab> NewDockTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("TranslationEditor.Tabs.Properties") )
		.Label( LOCTEXT("HistoryTabTitle", "History") )
		.TabColorScale( GetTabColorScale() )
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			.Padding(0.0f)
			[
				PropertyTableWidget
			]
		];
	
	return NewDockTab;
}

void FTranslationEditor::MapActions()
{
	FTranslationEditorCommands::Register();

	ToolkitCommands->MapAction( FTranslationEditorCommands::Get().ChangeSourceFont,
		FExecuteAction::CreateSP(this, &FTranslationEditor::ChangeSourceFont),
		FCanExecuteAction());

	ToolkitCommands->MapAction( FTranslationEditorCommands::Get().ChangeTranslationTargetFont,
		FExecuteAction::CreateSP(this, &FTranslationEditor::ChangeTranslationTargetFont),
		FCanExecuteAction());

	ToolkitCommands->MapAction( FTranslationEditorCommands::Get().SaveTranslations,
		FExecuteAction::CreateSP(this, &FTranslationEditor::SaveAsset_Execute),
		FCanExecuteAction());

	ToolkitCommands->MapAction(FTranslationEditorCommands::Get().PreviewAllTranslationsInEditor,
		FExecuteAction::CreateSP(this, &FTranslationEditor::PreviewAllTranslationsInEditor_Execute),
		FCanExecuteAction());

	ToolkitCommands->MapAction(FTranslationEditorCommands::Get().ExportToPortableObjectFormat,
		FExecuteAction::CreateSP(this, &FTranslationEditor::ExportToPortableObjectFormat_Execute),
		FCanExecuteAction());

	ToolkitCommands->MapAction(FTranslationEditorCommands::Get().ImportFromPortableObjectFormat,
		FExecuteAction::CreateSP(this, &FTranslationEditor::ImportFromPortableObjectFormat_Execute),
		FCanExecuteAction());

	ToolkitCommands->MapAction(FTranslationEditorCommands::Get().OpenSearchTab,
		FExecuteAction::CreateSP(this, &FTranslationEditor::OpenSearchTab_Execute),
		FCanExecuteAction());
}

void FTranslationEditor::ChangeSourceFont()
{
	// Use path from current font
	FString DefaultFile(SourceFont.FontName.ToString());

	FString NewFontFilename;
	bool bOpened = OpenFontPicker(DefaultFile, NewFontFilename);

	if ( bOpened && NewFontFilename != "")
	{
		SourceFont = FSlateFontInfo(NewFontFilename, SourceFont.Size);
		RefreshUI();
	}
}

void FTranslationEditor::ChangeTranslationTargetFont()
{
	// Use path from current font
	FString DefaultFile(TranslationTargetFont.FontName.ToString());

	FString NewFontFilename;
	bool bOpened = OpenFontPicker(DefaultFile, NewFontFilename);

	if ( bOpened && NewFontFilename != "")
	{
		TranslationTargetFont = FSlateFontInfo(NewFontFilename, TranslationTargetFont.Size);
		RefreshUI();
	}
}

void FTranslationEditor::RefreshUI()
{
	// Set the fonts in our custom font columns and text block
	SourceColumn->SetFont(SourceFont);
	TranslationColumn->SetFont(TranslationTargetFont);
	PreviewTextBlock->SetFont(TranslationTargetFont);

	// Refresh our widget displays
	if (UntranslatedPropertyTableWidgetHandle.IsValid())
	{
		UntranslatedPropertyTableWidgetHandle->RequestRefresh();
	}
	if (ReviewPropertyTableWidgetHandle.IsValid())	
	{
		ReviewPropertyTableWidgetHandle->RequestRefresh();
	}
	if (CompletedPropertyTableWidgetHandle.IsValid())
	{
		CompletedPropertyTableWidgetHandle->RequestRefresh();
	}
	if (ContextPropertyTableWidgetHandle.IsValid())
	{
		ContextPropertyTableWidgetHandle->RequestRefresh();
	}
	if (HistoryPropertyTableWidgetHandle.IsValid())
	{
		HistoryPropertyTableWidgetHandle->RequestRefresh();
	}
	if (SearchPropertyTableWidgetHandle.IsValid())
	{
		SearchPropertyTableWidgetHandle->RequestRefresh();
	}
	if (ChangedOnImportPropertyTableWidgetHandle.IsValid())
	{
		ChangedOnImportPropertyTableWidgetHandle->RequestRefresh();
	}
}

bool FTranslationEditor::OpenFontPicker( const FString DefaultFile, FString& OutFile )
{
	const FString FontFileDescription = LOCTEXT( "FontFileDescription", "Font File" ).ToString();
	const FString FontFileExtension = TEXT("*.ttf;*.otf");
	const FString FileTypes = FString::Printf( TEXT("%s (%s)|%s"), *FontFileDescription, *FontFileExtension, *FontFileExtension );

	// Prompt the user for the filenames
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	if ( DesktopPlatform )
	{
		void* ParentWindowWindowHandle = NULL;

		const TSharedPtr<SWindow>& ParentWindow = FSlateApplication::Get().FindWidgetWindow(PreviewTextBlock);
		if ( ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid() )
		{
			ParentWindowWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowWindowHandle,
			LOCTEXT("ChooseFontWindowTitle", "Choose Font").ToString(),
			FPaths::GetPath(DefaultFile),
			TEXT(""),
			FileTypes,
			EFileDialogFlags::None,
			OpenFilenames
			);
	}

	if ( bOpened && OpenFilenames.Num() > 0 )
	{
		OutFile = OpenFilenames[0];
	} 
	else
	{
		OutFile = "";
	}

	return bOpened;
}

void FTranslationEditor::UpdateUntranslatedSelection()
{
	TSharedPtr<SDockTab> UntranslatedTabSharedPtr = UntranslatedTab.Pin();
	if (UntranslatedTabSharedPtr.IsValid() && UntranslatedTabSharedPtr->IsForeground() && UntranslatedPropertyTable.IsValid())
	{
		TSet<TSharedRef<IPropertyTableRow>> SelectedRows = UntranslatedPropertyTable->GetSelectedRows();
		UpdateTranslationUnitSelection(SelectedRows);
	}
}

void FTranslationEditor::UpdateNeedsReviewSelection()
{
	TSharedPtr<SDockTab> ReviewTabSharedPtr = ReviewTab.Pin();
	if (ReviewTabSharedPtr.IsValid() && ReviewTabSharedPtr->IsForeground() && ReviewPropertyTable.IsValid())
	{
		TSet<TSharedRef<IPropertyTableRow>> SelectedRows = ReviewPropertyTable->GetSelectedRows();
		UpdateTranslationUnitSelection(SelectedRows);
	}
}

void FTranslationEditor::UpdateCompletedSelection()
{
	TSharedPtr<SDockTab> CompletedTabSharedPtr = CompletedTab.Pin();
	if (CompletedTabSharedPtr.IsValid() && CompletedTabSharedPtr->IsForeground() && CompletedPropertyTable.IsValid())
	{
		TSet<TSharedRef<IPropertyTableRow>> SelectedRows = CompletedPropertyTable->GetSelectedRows();
		UpdateTranslationUnitSelection(SelectedRows);
	}
}

void FTranslationEditor::UpdateSearchSelection()
{
	TSharedPtr<SDockTab> SearchTabSharedPtr = SearchTab.Pin();
	if (SearchTabSharedPtr.IsValid() && SearchTabSharedPtr->IsForeground() && SearchPropertyTable.IsValid())
	{
		TSet<TSharedRef<IPropertyTableRow>> SelectedRows = SearchPropertyTable->GetSelectedRows();
		UpdateTranslationUnitSelection(SelectedRows);
	}
}

void FTranslationEditor::UpdateChangedOnImportSelection()
{
	TSharedPtr<SDockTab> ChangedOnImportTabSharedPtr = SearchTab.Pin();
	if (ChangedOnImportTabSharedPtr.IsValid() && ChangedOnImportTabSharedPtr->IsForeground() && ChangedOnImportPropertyTable.IsValid())
	{
		TSet<TSharedRef<IPropertyTableRow>> SelectedRows = ChangedOnImportPropertyTable->GetSelectedRows();
		UpdateTranslationUnitSelection(SelectedRows);
	}
}

void FTranslationEditor::UpdateTranslationUnitSelection(TSet<TSharedRef<IPropertyTableRow>>& SelectedRows)
{
	// Can only really handle single selection
	if (SelectedRows.Num() == 1)
	{
		TSharedRef<IPropertyTableRow> SelectedRow = *(SelectedRows.CreateConstIterator());
		TSharedRef<FPropertyPath> PartialPath = SelectedRow->GetPartialPath();

		TWeakObjectPtr<UObject> UObjectWeakPtr = SelectedRow->GetDataSource()->AsUObject();
		if (UObjectWeakPtr.IsValid())
		{
			UObject* UObjectPtr = UObjectWeakPtr.Get();
			if (UObjectPtr != NULL)
			{
				UTranslationUnit* SelectedTranslationUnit = (UTranslationUnit*)UObjectPtr;
				if (SelectedTranslationUnit != NULL)
				{
					PreviewTextBlock->SetText(SelectedTranslationUnit->Translation);
					NamespaceTextBlock->SetText(FText::Format(LOCTEXT("TranslationNamespace", "Namespace: {0}"), FText::FromString(SelectedTranslationUnit->Namespace)));

					// Add the ContextPropertyTable-specific path
					UArrayProperty* ContextArrayProp = FindField<UArrayProperty>(UTranslationUnit::StaticClass(), "Contexts");
					FPropertyInfo ContextArrayPropInfo;
					ContextArrayPropInfo.Property = ContextArrayProp;
					ContextArrayPropInfo.ArrayIndex = INDEX_NONE;
					TSharedRef<FPropertyPath> ContextPath = FPropertyPath::CreateEmpty();
					ContextPath = ContextPath->ExtendPath(PartialPath);
					ContextPath = ContextPath->ExtendPath(ContextArrayPropInfo);

					if (ContextPropertyTable.IsValid())
					{
						TArray<UObject*> ObjectArray;
						ObjectArray.Add(SelectedTranslationUnit);
						ContextPropertyTable->SetObjects(ObjectArray);
						ContextPropertyTable->SetRootPath(ContextPath);

						// Need to re-add the columns we want to display
						ContextPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(FTranslationContextInfo::StaticStruct(), "Key"));
						ContextPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(FTranslationContextInfo::StaticStruct(), "Context"));

						TArray<TSharedRef<IPropertyTableColumn>> Columns = ContextPropertyTable->GetColumns();
						for (TSharedRef<IPropertyTableColumn> Column : Columns)
						{
							Column->SetFrozen(true);
						}

						TSharedPtr<IPropertyTableCell> ContextToSelectPtr = ContextPropertyTable->GetFirstCellInTable();

						if (ContextToSelectPtr.IsValid())
						{
							TSet<TSharedRef<IPropertyTableCell>> CellsToSelect;
							CellsToSelect.Add(ContextToSelectPtr.ToSharedRef());
							ContextPropertyTable->SetSelectedCells(CellsToSelect);
						}
					}
				}
			}
		}
	}
}

void FTranslationEditor::SaveAsset_Execute()
{
	// Doesn't call parent SaveAsset_Execute, only need to tell data manager to write data
	DataManager->WriteTranslationData();
}

void FTranslationEditor::UpdateContextSelection()
{
	if (ContextPropertyTable.IsValid())
	{
		TSet<TSharedRef<IPropertyTableRow>> SelectedRows = ContextPropertyTable->GetSelectedRows();
		TSharedRef<FPropertyPath> InitialPath = ContextPropertyTable->GetRootPath();
		UProperty* PropertyToFind = InitialPath->GetRootProperty().Property.Get();

		// Can only really handle single selection
		if (SelectedRows.Num() == 1)
		{
			TSharedRef<IPropertyTableRow> SelectedRow = *(SelectedRows.CreateConstIterator());
			TSharedRef<FPropertyPath> PartialPath = SelectedRow->GetPartialPath();

			TWeakObjectPtr<UObject> UObjectWeakPtr = SelectedRow->GetDataSource()->AsUObject();
			if (UObjectWeakPtr.IsValid())
			{
				UObject* UObjectPtr = UObjectWeakPtr.Get();
				if (UObjectPtr != NULL)
				{
					UTranslationUnit* SelectedTranslationUnit = (UTranslationUnit*)UObjectPtr;
					if (SelectedTranslationUnit != NULL)
					{
						// Index of the leaf most property is the context info index we need
						FTranslationContextInfo& SelectedContextInfo = SelectedTranslationUnit->Contexts[PartialPath->GetLeafMostProperty().ArrayIndex];

						// If this is a translation unit from the review tab and they select a context, possibly update the selected translation with one from that context
						// Only change the suggested translation if they haven't yet reviewed it
						if (SelectedTranslationUnit->HasBeenReviewed == false)
						{
							for (int32 ChangeIndex = 0; ChangeIndex < SelectedContextInfo.Changes.Num(); ++ChangeIndex)
							{
								// Find most recent, non-empty translation
								if (!SelectedContextInfo.Changes[ChangeIndex].Translation.IsEmpty() && SelectedTranslationUnit->Translation != SelectedContextInfo.Changes[ChangeIndex].Translation)
								{
									SelectedTranslationUnit->Modify();
									SelectedTranslationUnit->Translation = SelectedContextInfo.Changes[ChangeIndex].Translation;
									SelectedTranslationUnit->PostEditChange();
								}
							}
						}


						// Add the HistoryPropertyTable-specific path
						TSharedRef<FPropertyPath> HistoryPath = ContextPropertyTable->GetRootPath();
						UArrayProperty* ContextArrayProp = FindField<UArrayProperty>(UTranslationUnit::StaticClass(), "Contexts");
						FPropertyInfo ContextPropInfo;
						ContextPropInfo.Property = ContextArrayProp->Inner;
						ContextPropInfo.ArrayIndex = PartialPath->GetLeafMostProperty().ArrayIndex;
						HistoryPath = HistoryPath->ExtendPath(ContextPropInfo);
						UArrayProperty* ChangesProp = FindField<UArrayProperty>(FTranslationContextInfo::StaticStruct(), "Changes");
						FPropertyInfo ChangesPropInfo;
						ChangesPropInfo.Property = ChangesProp;
						ChangesPropInfo.ArrayIndex = INDEX_NONE;
						HistoryPath = HistoryPath->ExtendPath(ChangesPropInfo);
						if (HistoryPropertyTable.IsValid())
						{
							TArray<UObject*> ObjectArray;
							ObjectArray.Add(SelectedTranslationUnit);
							HistoryPropertyTable->SetObjects(ObjectArray);
							HistoryPropertyTable->SetRootPath(HistoryPath);

							// Need to re-add the columns we want to display
							HistoryPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(FTranslationChange::StaticStruct(), "Version"));
							HistoryPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(FTranslationChange::StaticStruct(), "DateAndTime"));
							HistoryPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(FTranslationChange::StaticStruct(), "Source"));
							HistoryPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(FTranslationChange::StaticStruct(), "Translation"));

							TArray<TSharedRef<IPropertyTableColumn>> Columns = HistoryPropertyTable->GetColumns();
							for (TSharedRef<IPropertyTableColumn> Column : Columns)
							{
								Column->SetFrozen(true);
							}
						}
					}
				}
			}
		}
	}
}

void FTranslationEditor::PreviewAllTranslationsInEditor_Execute()
{
	DataManager->PreviewAllTranslationsInEditor();
}

void FTranslationEditor::ExportToPortableObjectFormat_Execute()
{
	const FString PortableObjectFileDescription = LOCTEXT("PortableObjectFileDescription", "Portable Object File").ToString();
	const FString PortableObjectFileExtension = TEXT("*.po");
	const FString FileTypes = FString::Printf(TEXT("%s (%s)|%s"), *PortableObjectFileDescription, *PortableObjectFileExtension, *PortableObjectFileExtension);
	const FString DefaultFilename = FPaths::GetBaseFilename(ManifestFilePath) + "-" + FPaths::GetBaseFilename(FPaths::GetPath(ArchiveFilePath)) + ".po";
	FString DefaultPath = FPaths::GameSavedDir();
	if (LastExportFilePath != "")
	{
		DefaultPath = LastExportFilePath;
	}
	TArray<FString> SaveFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bSelected = false;
	bool bHadError = false;
	
	// Prompt the user for the filename
	if (DesktopPlatform)
	{
		void* ParentWindowWindowHandle = NULL;

		const TSharedPtr<SWindow>& ParentWindow = FSlateApplication::Get().FindWidgetWindow(PreviewTextBlock);
		if (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		bSelected = DesktopPlatform->SaveFileDialog(
			ParentWindowWindowHandle,
			LOCTEXT("ChooseExportLocationWindowTitle", "Choose Export Location").ToString(),
			LastExportFilePath,
			DefaultFilename,
			FileTypes,
			EFileDialogFlags::None,
			SaveFilenames
			);
	}

	if (bSelected)
	{
		GWarn->BeginSlowTask(LOCTEXT("ExportingInternationalization", "Exporting Internationalization Data..."), true);

		// Write translation data first to ensure all changes are exported
		DataManager->WriteTranslationData();

		UInternationalizationExportSettings* ExportSettings = NewObject<UInternationalizationExportSettings>();
		ExportSettings->CulturesToGenerate.Empty();
		ExportSettings->CulturesToGenerate.Add(FPaths::GetBaseFilename(FPaths::GetPath(ArchiveFilePath)));
		ExportSettings->CommandletClass = "InternationalizationExport";
		ExportSettings->SourcePath = FPaths::GetPath(ManifestFilePath);
		ExportSettings->ManifestName = FPaths::GetBaseFilename(ManifestFilePath) + ".manifest";
		ExportSettings->ArchiveName = FPaths::GetBaseFilename(ManifestFilePath) + ".archive";
		ExportSettings->bExportLoc = true;
		ExportSettings->bImportLoc = false;

		ExportSettings->DestinationPath = DefaultPath / DefaultFilename;

		if (SaveFilenames.Num() > 0)
		{
			ExportSettings->DestinationPath = FPaths::GetPath(SaveFilenames[0]);
			ExportSettings->PortableObjectName = FPaths::GetCleanFilename(SaveFilenames[0]);
			LastExportFilePath = FPaths::GetPath(SaveFilenames[0]);
		}

		// Write these settings to a temporary config file that the Internationalization Export Commandlet will read
		FString TempConfigFilepath = FPaths::GameSavedDir() / "Config" / "InternationalizationExport.ini";
		ExportSettings->SaveConfig(CPF_Config, *TempConfigFilepath);

		// Using .ini config saving means these settings will be saved in the GetClass()->GetPathName() section
		TArray<FString> ConfigSections;
		ConfigSections.Add(ExportSettings->GetClass()->GetPathName());
		FMessageLog TranslationEditorMessageLog("TranslationEditor");

		for (FString& ConfigSection : ConfigSections)
		{
			// Spawn the LocalizationExport commandlet, and run its log output back into ours
			FString AppURL = FPlatformProcess::ExecutableName(true);
			FString Parameters = FString("-run=InternationalizationExport -config=") + TempConfigFilepath + " -section=" + ConfigSection;

			void* WritePipe;
			void* ReadPipe;
			FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
			FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*AppURL, *Parameters, false, false, false, NULL, 0, NULL, WritePipe);

			while (FPlatformProcess::IsProcRunning(ProcessHandle))
			{
				FString NewLine = FPlatformProcess::ReadPipe(ReadPipe);
				if (NewLine.Len() > 0)
				{
					UE_LOG(LocalizationExport, Log, TEXT("%s"), *NewLine);
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("LogMessage"), FText::FromString(NewLine));
					TranslationEditorMessageLog.Info(FText::Format(LOCTEXT("LocalizationExportLog", "Localization Export Log: {LogMessage}"), Arguments));
				}

				FPlatformProcess::Sleep(0.25);
			}
			FString NewLine = FPlatformProcess::ReadPipe(ReadPipe);
			if (NewLine.Len() > 0)
			{
				UE_LOG(LocalizationExport, Log, TEXT("%s"), *NewLine);
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("LogMessage"), FText::FromString(NewLine));
				TranslationEditorMessageLog.Info(FText::Format(LOCTEXT("LocalizationExportLog", "Localization Export Log: {LogMessage}"), Arguments));
			}

			FPlatformProcess::Sleep(0.25);
			FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

			int32 ReturnCode;
			if (!FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode))
			{
				bHadError = true;
			}
			else if (ReturnCode != 0)
			{
				bHadError = true;
			}
		}

		GWarn->EndSlowTask();

		if (bHadError)
		{
			TranslationEditorMessageLog.Error(LOCTEXT("FailedToExportLocalization", "Failed to export localization!"));
			TranslationEditorMessageLog.Notify(LOCTEXT("FailedToExportLocalization", "Failed to export localization!"));
			TranslationEditorMessageLog.Open(EMessageSeverity::Error);
		}
	}
}

void FTranslationEditor::ImportFromPortableObjectFormat_Execute()
{
	const FString PortableObjectFileDescription = LOCTEXT("PortableObjectFileDescription", "Portable Object File").ToString();
	const FString PortableObjectFileExtension = TEXT("*.po");
	const FString FileTypes = FString::Printf(TEXT("%s (%s)|%s"), *PortableObjectFileDescription, *PortableObjectFileExtension, *PortableObjectFileExtension);
	FString DefaultPath = FPaths::GameSavedDir();
	if (LastImportFilePath != "")
	{
		DefaultPath = LastImportFilePath;
	}
	TArray<FString> OpenFilenames;
	bool bHadError = false;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	bool bOpened = false;
	if (DesktopPlatform)
	{
		void* ParentWindowWindowHandle = NULL;

		const TSharedPtr<SWindow>& ParentWindow = FSlateApplication::Get().FindWidgetWindow(PreviewTextBlock);
		if (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
		{
			ParentWindowWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();
		}

		bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowWindowHandle,
			LOCTEXT("ChooseImportLocationWindowTitle", "Choose File to Import").ToString(),
			DefaultPath,
			TEXT(""),
			FileTypes,
			EFileDialogFlags::None,
			OpenFilenames
			);
	}

	if (bOpened)
	{
		UInternationalizationExportSettings* ImportSettings = NewObject<UInternationalizationExportSettings>();
		ImportSettings->CulturesToGenerate.Empty();
		ImportSettings->CulturesToGenerate.Add(FPaths::GetBaseFilename(FPaths::GetPath(ArchiveFilePath)));
		ImportSettings->CommandletClass = "InternationalizationExport";
		ImportSettings->DestinationPath = FPaths::GetPath(ManifestFilePath);
		ImportSettings->ManifestName = FPaths::GetBaseFilename(ManifestFilePath) + ".manifest";
		ImportSettings->ArchiveName = FPaths::GetBaseFilename(ManifestFilePath) + ".archive";
		ImportSettings->bExportLoc = false;
		ImportSettings->bImportLoc = true;

		ImportSettings->SourcePath = DefaultPath / FPaths::GetBaseFilename(ManifestFilePath);

		if (OpenFilenames.Num() > 0)
		{
			ImportSettings->SourcePath = FPaths::GetPath(OpenFilenames[0]);
			ImportSettings->PortableObjectName = FPaths::GetCleanFilename(OpenFilenames[0]);
			LastImportFilePath = FPaths::GetPath(OpenFilenames[0]);
		}

		// Write translation data first to ensure all changes are exported
		bHadError = !(DataManager->WriteTranslationData(true));

		if (!bHadError)
		{
			GWarn->BeginSlowTask(LOCTEXT("ImportingInternationalization", "Importing Internationalization Data..."), true);

			// Write these settings to a temporary config file that the Internationalization Export Commandlet will read
			FString TempConfigFilepath = FPaths::GameSavedDir() / "Config" / "InternationalizationExport.ini";
			ImportSettings->SaveConfig(CPF_Config, *TempConfigFilepath);

			// Using .ini config saving means these settings will be saved in the GetClass()->GetPathName() section
			TArray<FString> ConfigSections;
			ConfigSections.Add(ImportSettings->GetClass()->GetPathName());
			FMessageLog TranslationEditorMessageLog("TranslationEditor");

			for (FString& ConfigSection : ConfigSections)
			{
				// Spawn the LocalizationExport commandlet, and run its log output back into ours
				FString AppURL = FPlatformProcess::ExecutableName(true);
				FString Parameters = FString("-run=InternationalizationExport -config=") + TempConfigFilepath + " -section=" + ConfigSection;

				void* WritePipe;
				void* ReadPipe;
				FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
				FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*AppURL, *Parameters, false, false, false, NULL, 0, NULL, WritePipe);

				while (FPlatformProcess::IsProcRunning(ProcessHandle))
				{
					FString NewLine = FPlatformProcess::ReadPipe(ReadPipe);
					if (NewLine.Len() > 0)
					{
						UE_LOG(LocalizationExport, Log, TEXT("%s"), *NewLine);
						FFormatNamedArguments Arguments;
						Arguments.Add(TEXT("LogMessage"), FText::FromString(NewLine));
						TranslationEditorMessageLog.Info(FText::Format(LOCTEXT("LocalizationImportLog", "Localization Import Log: {LogMessage}"), Arguments));
					}

					FPlatformProcess::Sleep(0.25);
				}
				FString NewLine = FPlatformProcess::ReadPipe(ReadPipe);
				if (NewLine.Len() > 0)
				{
					UE_LOG(LocalizationExport, Log, TEXT("%s"), *NewLine);
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("LogMessage"), FText::FromString(NewLine));
					TranslationEditorMessageLog.Info(FText::Format(LOCTEXT("LocalizationImportLog", "Localization Import Log: {LogMessage}"), Arguments));
				}

				FPlatformProcess::Sleep(0.25);
				FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

				int32 ReturnCode;
				if (!FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode))
				{
					bHadError = true;
				}
				else if (ReturnCode != 0)
				{
					bHadError = true;
				}
			}

			GWarn->EndSlowTask();

			if (bHadError)
			{
				TranslationEditorMessageLog.Error(LOCTEXT("FailedToExportLocalization", "Failed to export localization!"));
				TranslationEditorMessageLog.Notify(LOCTEXT("FailedToExportLocalization", "Failed to export localization!"), EMessageSeverity::Info, true);
				TranslationEditorMessageLog.Open(EMessageSeverity::Error);
			}
			else
			{
				DataManager->LoadFromArchive(DataManager->GetAllTranslationsArray(), true, true);

				TabManager->InvokeTab(ChangedOnImportTabId);
				ChangedOnImportPropertyTable->SetObjects((TArray<UObject*>&)DataManager->GetChangedOnImportArray());
				// Need to re-add the columns we want to display
				ChangedOnImportPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(UTranslationUnit::StaticClass(), "Source"));
				ChangedOnImportPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(UTranslationUnit::StaticClass(), "TranslationBeforeImport"));
				ChangedOnImportPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(UTranslationUnit::StaticClass(), "Translation"));
			}
		}
	}
}

void FTranslationEditor::OnFilterTextChanged(const FText& InFilterText)
{

}

void FTranslationEditor::OnFilterTextCommitted(const FText& InFilterText, ETextCommit::Type CommitInfo)
{
	const FString InFilterString = InFilterText.ToString();

	if (CommitInfo == ETextCommit::OnEnter)
	{
		if (InFilterString != CurrentSearchFilter)
		{
			CurrentSearchFilter = InFilterString;

			DataManager->PopulateSearchResultsUsingFilter(InFilterString);

			if (SearchPropertyTable.IsValid())
			{
				SearchPropertyTable->SetObjects((TArray<UObject*>&)DataManager->GetSearchResultsArray());

				// Need to re-add the columns we want to display
				SearchPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(UTranslationUnit::StaticClass(), "Source"));
				SearchPropertyTable->AddColumn((TWeakObjectPtr<UProperty>)FindField<UProperty>(UTranslationUnit::StaticClass(), "Translation"));

				TArray<TSharedRef<IPropertyTableColumn>> Columns = SearchPropertyTable->GetColumns();
				for (TSharedRef<IPropertyTableColumn> Column : Columns)
				{
					Column->SetFrozen(true);
				}
			}
		}
	}
}

void FTranslationEditor::OpenSearchTab_Execute()
{
	TabManager->InvokeTab(SearchTabId);
}

#undef LOCTEXT_NAMESPACE
