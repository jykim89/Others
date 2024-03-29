// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "PropertyEditorToolkit.h"
#include "IPropertyTable.h"
#include "IPropertyTableColumn.h"
#include "PropertyPath.h"
#include "IPropertyTreeRow.h"
#include "IPropertyTableRow.h"
#include "SPropertyTreeViewImpl.h"

#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "PropertyEditorToolkit"

const FName FPropertyEditorToolkit::ApplicationId( TEXT( "PropertyEditorToolkitApp" ) );
const FName FPropertyEditorToolkit::TreeTabId( TEXT( "PropertyEditorToolkit_PropertyTree" ) );
const FName FPropertyEditorToolkit::GridTabId( TEXT( "PropertyEditorToolkit_PropertyTable" ) );
const FName FPropertyEditorToolkit::TreePinAsColumnHeaderId( TEXT( "PropertyEditorToolkit_PinAsColumnHeader" ) );

void FPropertyEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	TabManager->RegisterTabSpawner( GridTabId, FOnSpawnTab::CreateSP(this, &FPropertyEditorToolkit::SpawnTab_PropertyTable) )
		.SetDisplayName( LOCTEXT("PropertyTableTab", "Grid") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );

	TabManager->RegisterTabSpawner( TreeTabId, FOnSpawnTab::CreateSP(this, &FPropertyEditorToolkit::SpawnTab_PropertyTree) )
		.SetDisplayName( LOCTEXT("PropertiesTab", "Details") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );
}

void FPropertyEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	TabManager->UnregisterTabSpawner( GridTabId );
	TabManager->UnregisterTabSpawner( TreeTabId );
}

FPropertyEditorToolkit::FPropertyEditorToolkit()
	: PropertyTree()
	, PropertyTable()
	, PathToRoot()
{
	PinSequence.AddCurve( 0, 1.0f, ECurveEaseFunction::QuadIn );
}


TSharedRef<FPropertyEditorToolkit> FPropertyEditorToolkit::CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit )
{
	TSharedRef< FPropertyEditorToolkit > NewEditor( new FPropertyEditorToolkit() );

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add( ObjectToEdit );
	NewEditor->Initialize( Mode, InitToolkitHost, ObjectsToEdit );

	return NewEditor;
}


TSharedRef<FPropertyEditorToolkit> FPropertyEditorToolkit::CreateEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit )
{
	TSharedRef< FPropertyEditorToolkit > NewEditor( new FPropertyEditorToolkit() );
	NewEditor->Initialize( Mode, InitToolkitHost, ObjectsToEdit );

	return NewEditor;
}


void FPropertyEditorToolkit::Initialize( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, const TArray<UObject*>& ObjectsToEdit )
{
	CreatePropertyTree();
	CreatePropertyTable();

	TArray< UObject* > AdjustedObjectsToEdit;
	for( auto ObjectIter = ObjectsToEdit.CreateConstIterator(); ObjectIter; ++ObjectIter )
	{
		//@todo Remove this and instead extend the blueprints Edit Defaults editor to use a Property Table as well [12/6/2012 Justin.Sargent]
		UObject* Object = *ObjectIter;
		if ( Object->IsA( UBlueprint::StaticClass() ) )
		{
			UBlueprint* Blueprint = Cast<UBlueprint>( Object );

			// Make sure that the generated class is valid, in case the super has been removed, and this class can't be loaded.
			if( Blueprint->GeneratedClass != NULL )
			{
				AdjustedObjectsToEdit.Add( Blueprint->GeneratedClass->GetDefaultObject() );
			}
		}
		else
		{
			AdjustedObjectsToEdit.Add( Object );
		}
	}

	PropertyTable->SetObjects( AdjustedObjectsToEdit );
	TableColumnsChanged();

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_PropertyEditorToolkit_Layout" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Horizontal)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.8f)
			->AddTab(GridTabId, ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.2f)
			->SetHideTabWell( true )
			->AddTab(TreeTabId, ETabState::OpenedTab)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = false;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, ApplicationId, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, AdjustedObjectsToEdit );

	TArray< TWeakObjectPtr<UObject> > AdjustedObjectsToEditWeak;
	for( auto ObjectIter = AdjustedObjectsToEdit.CreateConstIterator(); ObjectIter; ++ObjectIter )
	{
		AdjustedObjectsToEditWeak.Add(*ObjectIter);
	}
	PropertyTree->SetObjectArray( AdjustedObjectsToEditWeak );

	PinColor = FSlateColor( FLinearColor( 1, 1, 1, 0 ) );
	TickPinColorDelegate.BindSP( this, &FPropertyEditorToolkit::TickPinColorAndOpacity );
	GEditor->GetTimerManager()->SetTimer( TickPinColorDelegate, 0.1f, true );
}


TSharedRef<SDockTab> FPropertyEditorToolkit::SpawnTab_PropertyTree( const FSpawnTabArgs& Args ) 
{
	check( Args.GetTabId() == TreeTabId );

	TSharedRef<SDockTab> TreeToolkitTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("PropertyEditor.Properties.TabIcon") )
		.Label( LOCTEXT("GenericDetailsTitle", "Details") )
		.TabColorScale( GetTabColorScale() )
		.Content()
		[
			SNew(SBorder)
			.Padding(4)
			.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
			.Content()
			[
				PropertyTree.ToSharedRef()
			]
		];	
	
	return TreeToolkitTab;
}


TSharedRef<SDockTab> FPropertyEditorToolkit::SpawnTab_PropertyTable( const FSpawnTabArgs& Args ) 
{
	check( Args.GetTabId() == GridTabId );

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	TSharedRef<SDockTab> GridToolkitTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("PropertyEditor.Grid.TabIcon") )
		.Label( LOCTEXT("GenericGridTitle", "Grid") )
		.TabColorScale( GetTabColorScale() )
		.Content()
		[
			SNew( SOverlay )
			+SOverlay::Slot()
			[
				PropertyEditorModule.CreatePropertyTableWidget( PropertyTable.ToSharedRef() )
			]
			+SOverlay::Slot()
			.HAlign( HAlign_Right )
			.VAlign( VAlign_Top )
			.Padding( FMargin( 0, 3, 0, 0 ) )
			[
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign( HAlign_Center )
				.VAlign( VAlign_Center )
				[
					SNew( SImage )
					.Image( FEditorStyle::GetBrush( "PropertyEditor.AddColumnOverlay" ) )
					.Visibility( this, &FPropertyEditorToolkit::GetAddColumnInstructionsOverlayVisibility )
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign( HAlign_Center )
				.VAlign( VAlign_Center )
				[
					SNew( SImage )
					.Image( FEditorStyle::GetBrush( "PropertyEditor.RemoveColumn" ) )
					.Visibility( this, &FPropertyEditorToolkit::GetAddColumnInstructionsOverlayVisibility )
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign( HAlign_Center )
				.VAlign( VAlign_Center )
				.Padding( FMargin( 0, 0, 3, 0 ) )
				[
					SNew( STextBlock )
					.Font( FEditorStyle::GetFontStyle( "PropertyEditor.AddColumnMessage.Font" ) )
					.Text( LOCTEXT("GenericPropertiesTitle", "Pin Properties to Add Columns") )
					.Visibility( this, &FPropertyEditorToolkit::GetAddColumnInstructionsOverlayVisibility )
					.ColorAndOpacity( FEditorStyle::GetColor( "PropertyEditor.AddColumnMessage.ColorAndOpacity" ) )
				]
			]
		];	

	return GridToolkitTab;
}


void FPropertyEditorToolkit::CreatePropertyTree()
{
	PropertyTree = SNew( SPropertyTreeViewImpl )
		.AllowFavorites( false )
		.ShowTopLevelNodes(false)
		.OnPropertyMiddleClicked( this, &FPropertyEditorToolkit::ToggleColumnForProperty )
		.ConstructExternalColumnHeaders( this, &FPropertyEditorToolkit::ConstructTreeColumns )
		.ConstructExternalColumnCell( this, &FPropertyEditorToolkit::ConstructTreeCell )
		.NameColumnWidth( 0.5f );
}


void FPropertyEditorToolkit::CreatePropertyTable()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	PropertyTable = PropertyEditorModule.CreatePropertyTable();

	PropertyTable->OnSelectionChanged()->AddSP( this, &FPropertyEditorToolkit::GridSelectionChanged );
	PropertyTable->OnColumnsChanged()->AddSP( this, &FPropertyEditorToolkit::TableColumnsChanged );
	PropertyTable->OnRootPathChanged()->AddSP( this, &FPropertyEditorToolkit::GridRootPathChanged );
}


void FPropertyEditorToolkit::ConstructTreeColumns( const TSharedRef< class SHeaderRow >& HeaderRow )
{
	SHeaderRow::FColumn::FArguments ColumnArgs;
	ColumnArgs
	.ColumnId( TreePinAsColumnHeaderId )
	.FixedWidth(24)
	[
		SNew(SBorder)
		.Padding( 0 )
		.BorderImage( FEditorStyle::GetBrush( "NoBorder" ) )
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Center )
		.ToolTipText( LOCTEXT("AddColumnLabel", "Push Pins to Add Columns") )
		[
			SNew( SImage )
			.Image( FEditorStyle::GetBrush(TEXT("PropertyEditor.RemoveColumn")) )
		]
	];

	HeaderRow->InsertColumn( ColumnArgs, 0 );
}


TSharedRef< SWidget > FPropertyEditorToolkit::ConstructTreeCell( const FName& ColumnName, const TSharedRef< IPropertyTreeRow >& Row )
{
	if ( ColumnName == TreePinAsColumnHeaderId )
	{
		const TWeakPtr<IPropertyTreeRow> RowPtr = Row;
		PinRows.Add( Row );

		return SNew( SBorder )
			.Padding( 0 )
			.BorderImage( &FEditorStyle::GetWidgetStyle<FHeaderRowStyle>("PropertyTable.HeaderRow").ColumnStyle.NormalBrush )
			[
				SNew(SButton)
				.IsFocusable(false)
				.ToolTipText(NSLOCTEXT("PropertyEditor", "ToggleColumnButtonToolTip", "Toggle Column"))
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.ContentPadding(0) 
				.OnClicked( this, &FPropertyEditorToolkit::OnToggleColumnClicked, RowPtr )
				.HAlign( HAlign_Center )
				.VAlign( VAlign_Center )
				[
					SNew(SImage)
					.Image( this, &FPropertyEditorToolkit::GetToggleColumnButtonImageBrush, RowPtr )
					.ColorAndOpacity( this, &FPropertyEditorToolkit::GetPinColorAndOpacity, RowPtr )
				]
			];
	}

	return SNullWidget::NullWidget;
}


EVisibility FPropertyEditorToolkit::GetAddColumnInstructionsOverlayVisibility() const
{
	return TableHasCustomColumns() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}


void FPropertyEditorToolkit::ToggleColumnForProperty( const TSharedPtr< FPropertyPath >& PropertyPath )
{
	if ( !PropertyPath.IsValid() )
	{
		return;
	}

	TSharedRef< FPropertyPath > NewPath = PropertyPath->TrimRoot( PropertyTable->GetRootPath()->GetNumProperties() );
	const TSet< TSharedRef< IPropertyTableRow > > SelectedRows = PropertyTable->GetSelectedRows();
	
	for( auto RowIter = SelectedRows.CreateConstIterator(); RowIter; ++RowIter )
	{
		NewPath = NewPath->TrimRoot( (*RowIter)->GetPartialPath()->GetNumProperties() );
		break;
	}

	if ( NewPath->GetNumProperties() == 0 )
	{
		return;
	}

	TSharedPtr< IPropertyTableColumn > ExistingColumn;
	for( auto ColumnIter = PropertyTable->GetColumns().CreateConstIterator(); ColumnIter; ++ColumnIter )
	{
		TSharedRef< IPropertyTableColumn > Column = *ColumnIter;
		const TSharedPtr< FPropertyPath > Path = Column->GetDataSource()->AsPropertyPath();

		if ( Path.IsValid() && FPropertyPath::AreEqual( Path.ToSharedRef(), NewPath ) )
		{
			ExistingColumn = Column;
		}
	}

	if ( ExistingColumn.IsValid() )
	{
		PropertyTable->RemoveColumn( ExistingColumn.ToSharedRef() );
		const TSharedRef< FPropertyPath > ColumnPath = ExistingColumn->GetDataSource()->AsPropertyPath().ToSharedRef();
		for (int Index = PropertyPathsAddedAsColumns.Num() - 1; Index >= 0 ; Index--)
		{
			if ( FPropertyPath::AreEqual( ColumnPath, PropertyPathsAddedAsColumns[ Index ] ) )
			{
				PropertyPathsAddedAsColumns.RemoveAt( Index );
			}
		}
	}
	else
	{
		PropertyTable->AddColumn( NewPath );
		PropertyPathsAddedAsColumns.Add( NewPath );
	}
}


bool FPropertyEditorToolkit::TableHasCustomColumns() const
{
	return PropertyPathsAddedAsColumns.Num() > 0;
}

bool FPropertyEditorToolkit::CloseWindow()
{
	GEditor->GetTimerManager()->ClearTimer( TickPinColorDelegate );
	return FAssetEditorToolkit::CloseWindow();
}


bool FPropertyEditorToolkit::IsExposedAsColumn( const TWeakPtr< IPropertyTreeRow >& Row ) const
{
	bool Result = false;

	if (Row.IsValid())
	{
		const TSharedPtr< FPropertyPath > RowPathPtr = Row.Pin()->GetPropertyPath();
		if ( RowPathPtr.IsValid() )
		{
			TSharedRef< FPropertyPath > TrimmedPath = RowPathPtr->TrimRoot( PropertyTable->GetRootPath()->GetNumProperties() );
			const TSet< TSharedRef< IPropertyTableRow > > SelectedRows = PropertyTable->GetSelectedRows();

			for( auto RowIter = SelectedRows.CreateConstIterator(); RowIter; ++RowIter )
			{
				TrimmedPath = TrimmedPath->TrimRoot( (*RowIter)->GetPartialPath()->GetNumProperties() );
				break;
			}

			for (int Index = 0; Index < PropertyPathsAddedAsColumns.Num(); Index++)
			{
				if ( FPropertyPath::AreEqual( TrimmedPath, PropertyPathsAddedAsColumns[ Index ] ) )
				{
					Result = true;
					break;
				}
			}
		}
	}

	return Result;
}

void FPropertyEditorToolkit::TableColumnsChanged()
{
	PropertyPathsAddedAsColumns.Empty();

	for( auto ColumnIter = PropertyTable->GetColumns().CreateConstIterator(); ColumnIter; ++ColumnIter )
	{
		TSharedRef< IPropertyTableColumn > Column = *ColumnIter;
		TSharedPtr< FPropertyPath > ColumnPath = Column->GetDataSource()->AsPropertyPath();

		if ( ColumnPath.IsValid() && ColumnPath->GetNumProperties() > 0 )
		{
			PropertyPathsAddedAsColumns.Add( ColumnPath.ToSharedRef() );
		}
	}
}


void FPropertyEditorToolkit::GridSelectionChanged()
{
	TArray< TWeakObjectPtr< UObject > > SelectedObjects;
	PropertyTable->GetSelectedObjects( SelectedObjects );
	PropertyTree->SetObjectArray( SelectedObjects );

	const TSet< TSharedRef< IPropertyTableRow > > SelectedRows = PropertyTable->GetSelectedRows();

	if ( SelectedRows.Num() == 1 )
	{
		for( auto RowIter = SelectedRows.CreateConstIterator(); RowIter; ++RowIter )
		{
			PropertyTree->SetRootPath( PropertyTable->GetRootPath()->ExtendPath( (*RowIter)->GetPartialPath() ) );
			break;
		}
	}
	else if ( !FPropertyPath::AreEqual( PropertyTree->GetRootPath(), PropertyTable->GetRootPath() ) )
	{
		PropertyTree->SetRootPath( PropertyTable->GetRootPath() );
	}
}


void FPropertyEditorToolkit::GridRootPathChanged()
{
	GridSelectionChanged();
	PropertyTree->SetRootPath( PropertyTable->GetRootPath() );
}

FName FPropertyEditorToolkit::GetToolkitFName() const
{
	return FName("PropertyEditor");
}

FText FPropertyEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Property Editor");
}

FText FPropertyEditorToolkit::GetToolkitName() const
{
	const auto EditingObjects = GetEditingObjects();

	check( EditingObjects.Num() > 0 );

	if( EditingObjects.Num() == 1 )
	{
		const UObject* EditingObject = EditingObjects[ 0 ];

		const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

		FFormatNamedArguments Args;
		Args.Add( TEXT("ObjectName"), FText::FromString( EditingObject->GetName() ) );
		Args.Add( TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty() );
		Args.Add( TEXT("ToolkitName"), GetBaseToolkitName() );
		return FText::Format( LOCTEXT("ToolkitName_SingleObject", "{ObjectName}{DirtyState} - {ToolkitName}"), Args );
	}
	else
	{
		bool bDirtyState = false;
		UClass* SharedBaseClass = NULL;
		for( int32 x = 0; x < EditingObjects.Num(); ++x )
		{
			UObject* Obj = EditingObjects[ x ];
			check( Obj );

			UClass* ObjClass = Cast<UClass>(Obj);
			if (ObjClass == NULL)
			{
				ObjClass = Obj->GetClass();
			}
			check( ObjClass );

			// Initialize with the class of the first object we encounter.
			if( SharedBaseClass == NULL )
			{
				SharedBaseClass = ObjClass;
			}

			// If we've encountered an object that's not a subclass of the current best baseclass,
			// climb up a step in the class hierarchy.
			while( !ObjClass->IsChildOf( SharedBaseClass ) )
			{
				SharedBaseClass = SharedBaseClass->GetSuperClass();
			}

			// If any of the objects are dirty, flag the label
			bDirtyState |= Obj->GetOutermost()->IsDirty();
		}

		FFormatNamedArguments Args;
		Args.Add( TEXT("NumberOfObjects"), EditingObjects.Num() );
		Args.Add( TEXT("ClassName"), FText::FromString( SharedBaseClass->GetName() ) );
		Args.Add( TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty() );
		return FText::Format( LOCTEXT("ToolkitName_MultiObject", "{NumberOfObjects} {ClassName}{DirtyState} Objects - Property Matrix Editor"), Args );
	}
}


FReply FPropertyEditorToolkit::OnToggleColumnClicked( const TWeakPtr< IPropertyTreeRow > Row )
{
	if (Row.IsValid())
	{
		ToggleColumnForProperty( Row.Pin()->GetPropertyPath() );
	}

	return FReply::Handled();
}


const FSlateBrush* FPropertyEditorToolkit::GetToggleColumnButtonImageBrush( const TWeakPtr< IPropertyTreeRow > Row ) const
{
	if ( IsExposedAsColumn( Row ) )
	{
		return FEditorStyle::GetBrush("PropertyEditor.RemoveColumn");
	}

	return FEditorStyle::GetBrush("PropertyEditor.AddColumn");
}

void FPropertyEditorToolkit::TickPinColorAndOpacity()
{
	bool IsRowBeingHoveredOver = false;
	for (int Index = PinRows.Num() - 1; Index >= 0 ; Index--)
	{
		TSharedPtr< IPropertyTreeRow > Row = PinRows[ Index ].Pin();
		if ( Row.IsValid() )
		{
			IsRowBeingHoveredOver |= Row->IsCursorHovering();

			if ( IsRowBeingHoveredOver )
			{
				break;
			}
		}
		else
		{
			PinRows.RemoveAt( Index );
		}
	}

	if ( IsRowBeingHoveredOver )
	{
		PinSequence.JumpToStart();
	}

	float Opacity = 0.0f;
	if ( !TableHasCustomColumns() )
	{
		Opacity = PinSequence.GetLerp();
	}

	if ( !PinSequence.IsPlaying() )
	{
		if ( PinSequence.IsAtStart() )
		{
			PinSequence.Play();
		}
		else
		{
			PinSequence.PlayReverse();
		}
	}

	PinColor = FSlateColor( FColor( 255, 255, 255, FMath::Lerp( 0, 200, Opacity ) ).ReinterpretAsLinear() );
}

FSlateColor FPropertyEditorToolkit::GetPinColorAndOpacity( const TWeakPtr< IPropertyTreeRow > Row ) const
{
	if ( Row.IsValid() && ( Row.Pin()->IsCursorHovering() || IsExposedAsColumn( Row ) ) )
	{
		return FSlateColor( FLinearColor::White );
	}

	return PinColor;
}


FString FPropertyEditorToolkit::GetWorldCentricTabPrefix() const
{
	check(0);
	return TEXT("");
}


FLinearColor FPropertyEditorToolkit::GetWorldCentricTabColorScale() const
{
	check(0);
	return FLinearColor();
}

#undef LOCTEXT_NAMESPACE
