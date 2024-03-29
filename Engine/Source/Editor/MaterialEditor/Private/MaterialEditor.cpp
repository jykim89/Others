// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "MaterialEditorModule.h"
#include "MaterialEditorActions.h"
#include "MaterialExpressionClasses.h"
#include "MaterialCompiler.h"
#include "Toolkits/IToolkitHost.h"
#include "Editor/EditorWidgets/Public/EditorWidgets.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "SMaterialEditorViewport.h"
#include "SMaterialEditorTitleBar.h"
#include "PreviewScene.h"
#include "ScopedTransaction.h"
#include "BusyCursor.h"
#include "STutorialWrapper.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "MaterialEditorDetailCustomization.h"
#include "MaterialInstanceEditor.h"

#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "EditorViewportCommands.h"

#include "GraphEditor.h"
#include "GraphEditorActions.h"
#include "BlueprintEditorUtils.h"
#include "EdGraphUtilities.h"
#include "SNodePanel.h"
#include "MaterialEditorUtilities.h"
#include "SMaterialPalette.h"
#include "FindInMaterial.h"

#include "Developer/MessageLog/Public/MessageLogModule.h"

#define LOCTEXT_NAMESPACE "MaterialEditor"

DEFINE_LOG_CATEGORY_STATIC(LogMaterialEditor, Log, All);

const FName FMaterialEditor::PreviewTabId( TEXT( "MaterialEditor_Preview" ) );
const FName FMaterialEditor::GraphCanvasTabId( TEXT( "MaterialEditor_GraphCanvas" ) );
const FName FMaterialEditor::PropertiesTabId( TEXT( "MaterialEditor_MaterialProperties" ) );
const FName FMaterialEditor::HLSLCodeTabId( TEXT( "MaterialEditor_HLSLCode" ) );
const FName FMaterialEditor::PaletteTabId( TEXT( "MaterialEditor_Palette" ) );
const FName FMaterialEditor::StatsTabId( TEXT( "MaterialEditor_Stats" ) );
const FName FMaterialEditor::FindTabId( TEXT( "MaterialEditor_Find" ) );

///////////////////////////
// FMatExpressionPreview //
///////////////////////////

bool FMatExpressionPreview::ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
{
	if(VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLocalVertexFactory"), FNAME_Find)))
	{
		// we only need the non-light-mapped, base pass, local vertex factory shaders for drawing an opaque Material Tile
		// @todo: Added a FindShaderType by fname or something"

		if(FCString::Stristr(ShaderType->GetName(), TEXT("BasePassVSFNoLightMapPolicy")) ||
			FCString::Stristr(ShaderType->GetName(), TEXT("BasePassHSFNoLightMapPolicy")) ||
			FCString::Stristr(ShaderType->GetName(), TEXT("BasePassDSFNoLightMapPolicy")))
		{
			return true;
		}
		else if(FCString::Stristr(ShaderType->GetName(), TEXT("BasePassPSFNoLightMapPolicy")))
		{
			return true;
		}
	}

	return false;
}

// Material properties.
/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
int32 FMatExpressionPreview::CompileProperty(EMaterialProperty Property,EShaderFrequency InShaderFrequency,FMaterialCompiler* Compiler) const
{
	Compiler->SetMaterialProperty(Property, InShaderFrequency);
	if( Property == MP_EmissiveColor && Expression.IsValid())
	{
		// Hardcoding output 0 as we don't have the UI to specify any other output
		const int32 OutputIndex = 0;
		// Get back into gamma corrected space, as DrawTile does not do this adjustment.
		return Compiler->Power(Compiler->Max(Expression->CompilePreview(Compiler, OutputIndex, -1), Compiler->Constant(0)), Compiler->Constant(1.f/2.2f));
	}
	else if ( Property == MP_WorldPositionOffset)
	{
		//set to 0 to prevent off by 1 pixel errors
		return Compiler->Constant(0.0f);
	}
	else if (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7)
	{
		const int32 TextureCoordinateIndex = Property - MP_CustomizedUVs0;
		return Compiler->TextureCoordinate(TextureCoordinateIndex, false, false);
	}
	else
	{
		return Compiler->Constant(1.0f);
	}
}

void FMatExpressionPreview::NotifyCompilationFinished()
{
	if (Expression.IsValid() && Expression->GraphNode)
	{
		CastChecked<UMaterialGraphNode>(Expression->GraphNode)->bPreviewNeedsUpdate = true;
	}
}

/////////////////////
// FMaterialEditor //
/////////////////////

void FMaterialEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(TabManager);

	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	TabManager->RegisterTabSpawner( PreviewTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_Preview) )
		.SetDisplayName( LOCTEXT("ViewportTab", "Viewport") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );
	
	TabManager->RegisterTabSpawner( GraphCanvasTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_GraphCanvas) )
		.SetDisplayName( LOCTEXT("GraphCanvasTab", "Graph") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );
	
	TabManager->RegisterTabSpawner( PropertiesTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_MaterialProperties) )
		.SetDisplayName( LOCTEXT("DetailsTab", "Details") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );

	TabManager->RegisterTabSpawner( PaletteTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_Palette) )
		.SetDisplayName( LOCTEXT("PaletteTab", "Palette") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );

	TabManager->RegisterTabSpawner( StatsTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_Stats) )
		.SetDisplayName( LOCTEXT("StatsTab", "Stats") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );
	
	TabManager->RegisterTabSpawner(FindTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_Find))
		.SetDisplayName(LOCTEXT("FindTab", "Find Results"))
		.SetGroup(MenuStructure.GetAssetEditorCategory());

	TabManager->RegisterTabSpawner( HLSLCodeTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_HLSLCode) )
		.SetDisplayName( LOCTEXT("HLSLCodeTab", "HLSL Code") )
		.SetGroup( MenuStructure.GetAssetEditorCategory() );
}


void FMaterialEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(TabManager);

	TabManager->UnregisterTabSpawner( PreviewTabId );
	TabManager->UnregisterTabSpawner( GraphCanvasTabId );
	TabManager->UnregisterTabSpawner( PropertiesTabId );
	TabManager->UnregisterTabSpawner( PaletteTabId );
	TabManager->UnregisterTabSpawner( StatsTabId );
	TabManager->UnregisterTabSpawner( FindTabId );
	TabManager->UnregisterTabSpawner( HLSLCodeTabId );
}

void FMaterialEditor::InitEditorForMaterial(UMaterial* InMaterial)
{
	check(InMaterial);

	OriginalMaterial = InMaterial;
	MaterialFunction = NULL;
	OriginalMaterialObject = InMaterial;

	ExpressionPreviewMaterial = NULL;
	
	// Create a copy of the material for preview usage (duplicating to a different class than original!)
	// Propagate all object flags except for RF_Standalone, otherwise the preview material won't GC once
	// the material editor releases the reference.
	Material = (UMaterial*)StaticDuplicateObject(OriginalMaterial, GetTransientPackage(), NULL, ~RF_Standalone, UPreviewMaterial::StaticClass()); 
	
	// Remove NULL entries, so the rest of the material editor can assume all entries of Material->Expressions are valid
	// This can happen if an expression class was removed
	for (int32 ExpressionIndex = Material->Expressions.Num() - 1; ExpressionIndex >= 0; ExpressionIndex--)
	{
		if (!Material->Expressions[ExpressionIndex])
		{
			Material->Expressions.RemoveAt(ExpressionIndex);
		}
	}
}

void FMaterialEditor::InitEditorForMaterialFunction(UMaterialFunction* InMaterialFunction)
{
	check(InMaterialFunction);

	Material = NULL;
	MaterialFunction = InMaterialFunction;
	OriginalMaterialObject = InMaterialFunction;

	ExpressionPreviewMaterial = NULL;

	// Create a temporary material to preview the material function
	Material = (UMaterial*)StaticConstructObject(UMaterial::StaticClass()); 
	{
		FArchive DummyArchive;
		// Hack: serialize the new material with an archive that does nothing so that its material resources are created
		Material->Serialize(DummyArchive);
	}
	Material->SetLightingModel(MLM_Unlit);

	// Propagate all object flags except for RF_Standalone, otherwise the preview material function won't GC once
	// the material editor releases the reference.
	MaterialFunction = (UMaterialFunction*)StaticDuplicateObject(InMaterialFunction, GetTransientPackage(), NULL, ~RF_Standalone, UMaterialFunction::StaticClass()); 
	MaterialFunction->ParentFunction = InMaterialFunction;

	OriginalMaterial = Material;
}

void FMaterialEditor::InitMaterialEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit )
{
	EditorOptions = NULL;
	bMaterialDirty = false;
	bStatsFromPreviewMaterial = false;
	ColorPickerObject = NULL;

	// Support undo/redo
	Material->SetFlags(RF_Transactional);

	GEditor->RegisterForUndo(this);

	if (!Material->MaterialGraph)
	{
		Material->MaterialGraph = CastChecked<UMaterialGraph>(FBlueprintEditorUtils::CreateNewGraph(Material, NAME_None, UMaterialGraph::StaticClass(), UMaterialGraphSchema::StaticClass()));
	}
	Material->MaterialGraph->Material = Material;
	Material->MaterialGraph->MaterialFunction = MaterialFunction;
	Material->MaterialGraph->RealtimeDelegate.BindSP(this, &FMaterialEditor::IsToggleRealTimeExpressionsChecked);
	Material->MaterialGraph->MaterialDirtyDelegate.BindSP(this, &FMaterialEditor::SetMaterialDirty);
	Material->MaterialGraph->ToggleCollapsedDelegate.BindSP(this, &FMaterialEditor::ToggleCollapsed);

	// copy material usage
	for( int32 Usage=0; Usage < MATUSAGE_MAX; Usage++ )
	{
		const EMaterialUsage UsageEnum = (EMaterialUsage)Usage;
		if( OriginalMaterial->GetUsageByFlag(UsageEnum) )
		{
			bool bNeedsRecompile=false;
			Material->SetMaterialUsage(bNeedsRecompile,UsageEnum);
		}
	}
	// Manually copy bUsedAsSpecialEngineMaterial as it is duplicate transient to prevent accidental creation of new special engine materials
	Material->bUsedAsSpecialEngineMaterial = OriginalMaterial->bUsedAsSpecialEngineMaterial;
	
	// Register our commands. This will only register them if not previously registered
	FGraphEditorCommands::Register();
	FMaterialEditorCommands::Register();
	FMaterialEditorSpawnNodeCommands::Register();

	FEditorSupportDelegates::MaterialUsageFlagsChanged.AddRaw(this, &FMaterialEditor::OnMaterialUsageFlagsChanged);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	
	AssetRegistryModule.Get().OnAssetRenamed().AddSP( this, &FMaterialEditor::RenameAssetFromRegistry );

	CreateInternalWidgets();

	// Do setup previously done in SMaterialEditorCanvas
	SetPreviewMaterial(Material);
	Material->bIsPreviewMaterial = true;
	FMaterialEditorUtilities::InitExpressions(Material);

	BindCommands();

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_MaterialEditor_Layout_v5")
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->SetHideTabWell( true )
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal) ->SetSizeCoefficient(0.9f)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation(Orient_Vertical) ->SetSizeCoefficient(0.2f)
				->Split
				(
					FTabManager::NewStack()
					->SetHideTabWell( true )
					->AddTab( PreviewTabId, ETabState::OpenedTab )
				)
				->Split
				(
					FTabManager::NewStack()
					->AddTab( PropertiesTabId, ETabState::OpenedTab )
				)
			)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
				->SetSizeCoefficient(0.80f)
				->Split
				(
					FTabManager::NewStack() 
					->SetSizeCoefficient(0.8f)
					->SetHideTabWell( true )
					->AddTab( GraphCanvasTabId, ETabState::OpenedTab )
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient( 0.20f )
					->AddTab( StatsTabId, ETabState::ClosedTab )
					->AddTab( FindTabId, ETabState::ClosedTab )
				)
				
			)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal) ->SetSizeCoefficient(0.2f)
				->Split
				(
					FTabManager::NewStack()
					->AddTab( PaletteTabId, ETabState::OpenedTab )
				)
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;

	// Add the preview material to the objects being edited, so that we can find this editor from the temporary material graph
	TArray< UObject* > ObjectsToEdit;
	ObjectsToEdit.Add(ObjectToEdit);
	ObjectsToEdit.Add(Material);
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, MaterialEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit, false );
	
	IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
	AddMenuExtender(MaterialEditorModule->GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// @todo toolkit world centric editing
	/*if( IsWorldCentricAssetEditor() )
	{
		SpawnToolkitTab(GetToolbarTabId(), FString(), EToolkitTabSpot::ToolBar);
		SpawnToolkitTab(PreviewTabId, FString(), EToolkitTabSpot::Viewport);
		SpawnToolkitTab(GraphCanvasTabId, FString(), EToolkitTabSpot::Document);
		SpawnToolkitTab(PropertiesTabId, FString(), EToolkitTabSpot::Details);
	}*/
	

	// Load editor settings from disk.
	LoadEditorSettings();
	
	// Set the preview mesh for the material.  This call must occur after the toolbar is initialized.
	if ( !SetPreviewMesh( *Material->PreviewMesh.AssetLongPathname ) )
	{
		// The material preview mesh couldn't be found or isn't loaded.  Default to the one of the primitive types.
		Viewport->SetPreviewMesh( GUnrealEd->GetThumbnailManager()->EditorSphere, NULL );
	}

	// Initialize expression previews.
	if (MaterialFunction)
	{
		Material->Expressions = MaterialFunction->FunctionExpressions;
		Material->EditorComments = MaterialFunction->FunctionEditorComments;

		// Remove NULL entries, so the rest of the material editor can assume all entries of Material->Expressions are valid
		// This can happen if an expression class was removed
		for (int32 ExpressionIndex = Material->Expressions.Num() - 1; ExpressionIndex >= 0; ExpressionIndex--)
		{
			if (!Material->Expressions[ExpressionIndex])
			{
				Material->Expressions.RemoveAt(ExpressionIndex);
			}
		}

		if (Material->Expressions.Num() == 0)
		{
			// If this is an empty functions, create an output by default and start previewing it
			if (GraphEditor.IsValid())
			{
				UMaterialExpression* Expression = CreateNewMaterialExpression(UMaterialExpressionFunctionOutput::StaticClass(), FVector2D(200, 300), false, true);
				SetPreviewExpression(Expression);
			}
		}
		else
		{
			bool bSetPreviewExpression = false;
			UMaterialExpressionFunctionOutput* FirstOutput = NULL;
			for (int32 ExpressionIndex = Material->Expressions.Num() - 1; ExpressionIndex >= 0; ExpressionIndex--)
			{
				UMaterialExpression* Expression = Material->Expressions[ExpressionIndex];

				// Setup the expression to be used with the preview material instead of the function
				Expression->Function = NULL;
				Expression->Material = Material;

				UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Expression);
				if (FunctionOutput)
				{
					FirstOutput = FunctionOutput;
					if (FunctionOutput->bLastPreviewed)
					{
						bSetPreviewExpression = true;

						// Preview the last output previewed
						SetPreviewExpression(FunctionOutput);
					}
				}
			}

			if (!bSetPreviewExpression && FirstOutput)
			{
				SetPreviewExpression(FirstOutput);
			}
		}
	}

	Material->MaterialGraph->RebuildGraph();
	RecenterEditor();
	ForceRefreshExpressionPreviews();
}

FMaterialEditor::FMaterialEditor()
	: bMaterialDirty(false)
	, bStatsFromPreviewMaterial(false)
	, Material(NULL)
	, OriginalMaterial(NULL)
	, ExpressionPreviewMaterial(NULL)
	, EmptyMaterial(NULL)
	, PreviewExpression(NULL)
	, MaterialFunction(NULL)
	, OriginalMaterialObject(NULL)
	, EditorOptions(NULL)
	, ScopedTransaction(NULL)
	, bAlwaysRefreshAllPreviews(false)
	, bHideUnusedConnectors(false)
	, bIsRealtime(false)
	, bShowStats(true)
	, bShowBuiltinStats(false)
	, bShowMobileStats(false)
{
}

FMaterialEditor::~FMaterialEditor()
{
	// Unregister this delegate
	FEditorSupportDelegates::MaterialUsageFlagsChanged.RemoveAll(this);

	// Null out the expression preview material so they can be GC'ed
	ExpressionPreviewMaterial = NULL;

	// Save editor settings to disk.
	SaveEditorSettings();

	MaterialDetailsView.Reset();

	{
		SCOPED_SUSPEND_RENDERING_THREAD(true);
	
		ExpressionPreviews.Empty();
	}
	
	check( !ScopedTransaction );
	
	GEditor->UnregisterForUndo( this );
}

void FMaterialEditor::GetAllMaterialExpressionGroups(TArray<FString>* OutGroups)
{
	for (int32 MaterialExpressionIndex = 0; MaterialExpressionIndex < Material->Expressions.Num(); ++MaterialExpressionIndex)
	{
		UMaterialExpression* MaterialExpression = Material->Expressions[ MaterialExpressionIndex ];
		UMaterialExpressionParameter *Switch = Cast<UMaterialExpressionParameter>(MaterialExpression);
		UMaterialExpressionTextureSampleParameter *TextureS = Cast<UMaterialExpressionTextureSampleParameter>(MaterialExpression);
		UMaterialExpressionFontSampleParameter *FontS = Cast<UMaterialExpressionFontSampleParameter>(MaterialExpression);
		if(Switch)
		{
			OutGroups->AddUnique(Switch->Group.ToString());
		}
		if(TextureS)
		{
			OutGroups->AddUnique(TextureS->Group.ToString());
		}
		if(FontS)
		{
			OutGroups->AddUnique(FontS->Group.ToString());
		}
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FMaterialEditor::CreateInternalWidgets()
{
	Viewport = SNew(SMaterialEditorViewport)
		.MaterialEditor(SharedThis(this));

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

	GraphEditor = CreateGraphEditorWidget();
	// Manually set zoom level to avoid deferred zooming
	GraphEditor->SetViewLocation(FVector2D::ZeroVector, 1);

	const FDetailsViewArgs DetailsViewArgs( false, false, true, false, true, this );
	MaterialDetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );

	FOnGetDetailCustomizationInstance LayoutExpressionParameterDetails = FOnGetDetailCustomizationInstance::CreateStatic(
		&FMaterialExpressionParameterDetails::MakeInstance, FOnCollectParameterGroups::CreateSP(this, &FMaterialEditor::GetAllMaterialExpressionGroups) );

	MaterialDetailsView->RegisterInstancedCustomPropertyLayout( 
		UMaterialExpressionParameter::StaticClass(), 
		LayoutExpressionParameterDetails
		);

	MaterialDetailsView->RegisterInstancedCustomPropertyLayout( 
		UMaterialExpressionFontSampleParameter::StaticClass(), 
		LayoutExpressionParameterDetails
		);

	MaterialDetailsView->RegisterInstancedCustomPropertyLayout( 
		UMaterialExpressionTextureSampleParameter::StaticClass(), 
		LayoutExpressionParameterDetails
		);

	FOnGetDetailCustomizationInstance LayoutCollectionParameterDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FMaterialExpressionCollectionParameterDetails::MakeInstance);

	MaterialDetailsView->RegisterInstancedCustomPropertyLayout( 
		UMaterialExpressionCollectionParameter::StaticClass(), 
		LayoutCollectionParameterDetails
		);

	Palette = SNew(SMaterialPalette, SharedThis(this));

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions LogOptions;
	// Show Pages so that user is never allowed to clear log messages
	LogOptions.bShowPages = true;
	LogOptions.MaxPageCount = 1;
	StatsListing = MessageLogModule.CreateLogListing( "MaterialEditorStats", LogOptions );

	Stats = 
		SNew(SBorder)
		.Padding(0.0f)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			MessageLogModule.CreateLogListingWidget( StatsListing.ToSharedRef() )
		];

	FindResults =
		SNew(SFindInMaterial, SharedThis(this));

	CodeViewUtility =
		SNew(SVerticalBox)
		// Copy Button
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SHorizontalBox )
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f, 0.0f )
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(SButton)
				.Text( LOCTEXT("CopyHLSLButton", "Copy") )
				.ToolTipText( LOCTEXT("CopyHLSLButtonToolTip", "Copies all HLSL code to the clipboard.") )
				.ContentPadding(3)
				.OnClicked(this, &FMaterialEditor::CopyCodeViewTextToClipboard)
			]
		]
		// Separator
		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew(SSeparator)
		];

	CodeView =
		SNew(SScrollBox)
		+SScrollBox::Slot() .Padding(5)
		[
			SNew(STextBlock)
			.Text(this, &FMaterialEditor::GetCodeViewText)
		];

	RegenerateCodeView();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FName FMaterialEditor::GetToolkitFName() const
{
	return FName("MaterialEditor");
}

FText FMaterialEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Material Editor");
}

FText FMaterialEditor::GetToolkitName() const
{
	const UObject* EditingObject = GetEditingObjects()[0];

	const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

	// Overridden to accommodate editing of multiple objects (original and preview materials)
	FFormatNamedArguments Args;
	Args.Add( TEXT("ObjectName"), FText::FromString( EditingObject->GetName() ) );
	Args.Add( TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty() );
	return FText::Format( LOCTEXT("MaterialEditorAppLabel", "{ObjectName}{DirtyState}"), Args );
}

FString FMaterialEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Material ").ToString();
}

FLinearColor FMaterialEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.3f, 0.2f, 0.5f, 0.5f );
}

void FMaterialEditor::Tick( float InDeltaTime )
{
	UpdateMaterialInfoList();
	UpdateGraphNodeStates();
}

TStatId FMaterialEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FMaterialEditor, STATGROUP_Tickables);
}

void FMaterialEditor::UpdateThumbnailInfoPreviewMesh(UMaterialInterface* MatInterface)
{
	if ( MatInterface )
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass( MatInterface->GetClass() );
		if ( AssetTypeActions.IsValid() )
		{
			USceneThumbnailInfoWithPrimitive* OriginalThumbnailInfo = Cast<USceneThumbnailInfoWithPrimitive>(AssetTypeActions.Pin()->GetThumbnailInfo(MatInterface));
			if ( OriginalThumbnailInfo )
			{
				OriginalThumbnailInfo->PreviewMesh = MatInterface->PreviewMesh;
				MatInterface->PostEditChange();
			}
		}
	}
}

void FMaterialEditor::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("Apply");
			{
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().Apply);
			}
			ToolbarBuilder.EndSection();
	
			ToolbarBuilder.BeginSection("Search");
			{
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().FindInMaterial);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("Graph");
			{
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().CameraHome);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().CleanUnusedExpressions);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().ShowHideConnectors);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().ToggleRealtimeExpressions);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().AlwaysRefreshAllPreviews);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().ToggleMaterialStats);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().ToggleReleaseStats);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().ToggleBuiltinStats);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().ToggleMobileStats);
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic( &Local::FillToolbar )
		);
	
	AddToolbarExtender(ToolbarExtender);

	IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
	AddToolbarExtender(MaterialEditorModule->GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}


UMaterialInterface* FMaterialEditor::GetMaterialInterface() const
{
	return Material;
}

bool FMaterialEditor::ApproveSetPreviewMesh(UStaticMesh* InStaticMesh, USkeletalMesh* InSkeletalMesh)
{
	bool bApproved = true;
	// Only permit the use of a skeletal mesh if the material has bUsedWithSkeltalMesh.
	if ( InSkeletalMesh && !Material->bUsedWithSkeletalMesh )
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_MaterialEditor_CantPreviewOnSkelMesh", "Can't preview on the specified skeletal mesh because the material has not been compiled with bUsedWithSkeletalMesh.") );
		bApproved = false;
	}
	return bApproved;
}

void FMaterialEditor::SaveAsset_Execute()
{
	UE_LOG(LogMaterialEditor, Log, TEXT("Saving and Compiling material %s"), *GetEditingObjects()[0]->GetName());
	
	UpdateOriginalMaterial();

	UPackage* Package = OriginalMaterial->GetOutermost();

	if (MaterialFunction != NULL && MaterialFunction->ParentFunction)
	{
		Package = MaterialFunction->ParentFunction->GetOutermost();
	}
	
	if (Package)
	{
		TArray<UPackage*> PackagesToSave;
		PackagesToSave.Add(Package);

		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
	}
}

bool FMaterialEditor::OnRequestClose()
{
	DestroyColorPicker();

	// @todo DB: Store off the viewport camera position/orientation to the material.
	//AnimTree->PreviewCamPos = PreviewVC->ViewLocation;
	//AnimTree->PreviewCamRot = PreviewVC->ViewRotation;

	if (bMaterialDirty)
	{
		// find out the user wants to do with this dirty material
		EAppReturnType::Type YesNoCancelReply = FMessageDialog::Open(EAppMsgType::YesNoCancel,
			FText::Format(
				NSLOCTEXT("UnrealEd", "Prompt_MaterialEditorClose", "Would you like to apply changes to this material to the original material?\n{0}\n(No will lose all changes!)"),
				FText::FromString(OriginalMaterialObject->GetPathName()) ));

		// act on it
		switch (YesNoCancelReply)
		{
		case EAppReturnType::Yes:
			// update material and exit
			UpdateOriginalMaterial();
			break;
				
		case EAppReturnType::No:
			// exit
			break;
				
		case EAppReturnType::Cancel:
			// don't exit
			return false;
		}
	}

	return true;
}


void FMaterialEditor::DrawMaterialInfoStrings(
	FCanvas* Canvas, 
	const UMaterial* Material, 
	const FMaterialResource* MaterialResource, 
	const TArray<FString>& CompileErrors, 
	int32 &DrawPositionY,
	bool bDrawInstructions)
{
	check(Material && MaterialResource);

	ERHIFeatureLevel::Type FeatureLevel = MaterialResource->GetFeatureLevel();
	FString FeatureLevelName;
	GetFeatureLevelName(FeatureLevel,FeatureLevelName);

	// The font to use when displaying info strings
	UFont* FontToUse = GEngine->GetTinyFont();
	const int32 SpacingBetweenLines = 13;

	if (bDrawInstructions)
	{
		// Display any errors and messages in the upper left corner of the viewport.
		TArray<FString> Descriptions;
		TArray<int32> InstructionCounts;
		MaterialResource->GetRepresentativeInstructionCounts(Descriptions, InstructionCounts);

		for (int32 InstructionIndex = 0; InstructionIndex < Descriptions.Num(); InstructionIndex++)
		{
			FString InstructionCountString = FString::Printf(TEXT("%s: %u instructions"),*Descriptions[InstructionIndex],InstructionCounts[InstructionIndex]);
			Canvas->DrawShadowedString(5, DrawPositionY, *InstructionCountString, FontToUse, FLinearColor(1, 1, 0));
			DrawPositionY += SpacingBetweenLines;
		}

		// Display the number of samplers used by the material.
		const int32 SamplersUsed = MaterialResource->GetSamplerUsage();

		if (SamplersUsed >= 0)
		{
			int32 MaxSamplers = GetFeatureLevelMaxTextureSamplers(MaterialResource->GetFeatureLevel());

			Canvas->DrawShadowedString(
				5,
				DrawPositionY,
				*FString::Printf(TEXT("%s samplers: %u/%u"), FeatureLevel == ERHIFeatureLevel::ES2 ? TEXT("Mobile texture") : TEXT("Texture"), SamplersUsed, MaxSamplers),
				FontToUse,
				SamplersUsed > MaxSamplers ? FLinearColor(1,0,0) : FLinearColor(1,1,0)
				);
			DrawPositionY += SpacingBetweenLines;
		}
	}

	for(int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
	{
		Canvas->DrawShadowedString(5, DrawPositionY, *FString::Printf(TEXT("[%s] %s"), *FeatureLevelName, *CompileErrors[ErrorIndex]), FontToUse, FLinearColor(1, 0, 0));
		DrawPositionY += SpacingBetweenLines;
	}
}

void FMaterialEditor::DrawMessages( FViewport* InViewport, FCanvas* Canvas )
{
	if( PreviewExpression != NULL )
	{
		Canvas->PushAbsoluteTransform( FMatrix::Identity );

		// The message to display in the viewport.
		FString Name = FString::Printf( TEXT("Previewing: %s"), *PreviewExpression->GetName() );

		// Size of the tile we are about to draw.  Should extend the length of the view in X.
		const FIntPoint TileSize( InViewport->GetSizeXY().X, 25);

		const FColor PreviewColor( 70,100,200 );
		const FColor FontColor( 255,255,128 );

		UFont* FontToUse = GEditor->EditorFont;

		Canvas->DrawTile(  0.0f, 0.0f, TileSize.X, TileSize.Y, 0.0f, 0.0f, 0.0f, 0.0f, PreviewColor );

		int32 XL, YL;
		StringSize( FontToUse, XL, YL, *Name );
		if( XL > TileSize.X )
		{
			// There isn't enough room to show the preview expression name
			Name = TEXT("Previewing");
			StringSize( FontToUse, XL, YL, *Name );
		}

		// Center the string in the middle of the tile.
		const FIntPoint StringPos( (TileSize.X-XL)/2, ((TileSize.Y-YL)/2)+1 );
		// Draw the preview message
		Canvas->DrawShadowedString(  StringPos.X, StringPos.Y, *Name, FontToUse, FontColor );

		Canvas->PopTransform();
	}
}

void FMaterialEditor::RecenterEditor()
{
	UEdGraphNode* FocusNode = NULL;

	if (MaterialFunction)
	{
		bool bSetPreviewExpression = false;
		UMaterialExpressionFunctionOutput* FirstOutput = NULL;
		for (int32 ExpressionIndex = Material->Expressions.Num() - 1; ExpressionIndex >= 0; ExpressionIndex--)
		{
			UMaterialExpression* Expression = Material->Expressions[ExpressionIndex];

			UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Expression);
			if (FunctionOutput)
			{
				FirstOutput = FunctionOutput;
				if (FunctionOutput->bLastPreviewed)
				{
					bSetPreviewExpression = true;
					FocusNode = FunctionOutput->GraphNode;
				}
			}
		}

		if (!bSetPreviewExpression && FirstOutput)
		{
			FocusNode = FirstOutput->GraphNode;
		}
	}
	else
	{
		FocusNode = Material->MaterialGraph->RootNode;
	}

	if (FocusNode)
	{
		JumpToNode(FocusNode);
	}
	else
	{
		// Get current view location so that we don't change the zoom amount
		FVector2D CurrLocation;
		float CurrZoomLevel;
		GraphEditor->GetViewLocation(CurrLocation, CurrZoomLevel);
		GraphEditor->SetViewLocation(FVector2D::ZeroVector, CurrZoomLevel);
	}
}

bool FMaterialEditor::SetPreviewMesh(UStaticMesh* InStaticMesh, USkeletalMesh* InSkeletalMesh)
{
	if (Viewport.IsValid())
	{
		return Viewport->SetPreviewMesh(InStaticMesh, InSkeletalMesh);
	}
	return false;
}

bool FMaterialEditor::SetPreviewMesh(const TCHAR* InMeshName)
{
	if (Viewport.IsValid())
	{
		return Viewport->SetPreviewMesh(InMeshName);
	}
	return false;
}

void FMaterialEditor::SetPreviewMaterial(UMaterialInterface* InMaterialInterface)
{
	if (Viewport.IsValid())
	{
		Viewport->SetPreviewMaterial(InMaterialInterface);
	}
}

void FMaterialEditor::RefreshPreviewViewport()
{
	if (Viewport.IsValid())
	{
		Viewport->RefreshViewport();
	}
}

void FMaterialEditor::LoadEditorSettings()
{
	EditorOptions = ConstructObject<UMaterialEditorOptions>( UMaterialEditorOptions::StaticClass() );
	
	if (EditorOptions->bHideUnusedConnectors) {OnShowConnectors();}
	if (EditorOptions->bAlwaysRefreshAllPreviews) {OnAlwaysRefreshAllPreviews();}
	if (EditorOptions->bRealtimeExpressionViewport) {ToggleRealTimeExpressions();}

	if ( Viewport.IsValid() )
	{
		if (EditorOptions->bShowGrid) {Viewport->TogglePreviewGrid();}
		if (EditorOptions->bShowBackground) {Viewport->TogglePreviewBackground();}
		if (EditorOptions->bRealtimeMaterialViewport) {Viewport->ToggleRealtime();}

		// Load the preview scene
		Viewport->PreviewScene.LoadSettings(TEXT("MaterialEditor"));
	}

	if (EditorOptions->bShowMobileStats)
	{
		ToggleMobileStats();
	}

	if (EditorOptions->bReleaseStats)
	{
		ToggleReleaseStats();
	}

	if (EditorOptions->bShowBuiltinStats)
	{
		ToggleBuiltinStats();
	}

	// Primitive type
	int32 PrimType;
	if(GConfig->GetInt(TEXT("MaterialEditor"), TEXT("PrimType"), PrimType, GEditorUserSettingsIni))
	{
		Viewport->OnSetPreviewPrimitive((EThumbnailPrimType)PrimType);
	}
}

void FMaterialEditor::SaveEditorSettings()
{
	// Save the preview scene
	check( Viewport.IsValid() );
	Viewport->PreviewScene.SaveSettings(TEXT("MaterialEditor"));

	if ( EditorOptions )
	{
		EditorOptions->bShowGrid					= Viewport->IsTogglePreviewGridChecked();
		EditorOptions->bShowBackground				= Viewport->IsTogglePreviewBackgroundChecked();
		EditorOptions->bRealtimeMaterialViewport	= Viewport->IsRealtime();
		EditorOptions->bShowMobileStats				= bShowMobileStats;
		EditorOptions->bHideUnusedConnectors		= !IsOnShowConnectorsChecked();
		EditorOptions->bAlwaysRefreshAllPreviews	= IsOnAlwaysRefreshAllPreviews();
		EditorOptions->bRealtimeExpressionViewport	= IsToggleRealTimeExpressionsChecked();
		EditorOptions->SaveConfig();
	}

	GConfig->SetInt(TEXT("MaterialEditor"), TEXT("PrimType"), Viewport->PreviewPrimType, GEditorUserSettingsIni);
}

FString FMaterialEditor::GetCodeViewText() const
{
	return HLSLCode;
}

FReply FMaterialEditor::CopyCodeViewTextToClipboard()
{
	FString CodeViewText = GetCodeViewText();
	FPlatformMisc::ClipboardCopy( *CodeViewText );
	return FReply::Handled();
}

void FMaterialEditor::RegenerateCodeView()
{
#define MARKTAG TEXT("/*MARK_")
#define MARKTAGLEN 7

	HLSLCode = TEXT("");
	TMap<FMaterialExpressionKey,int32> ExpressionCodeMap[MP_MAX][SF_NumFrequencies];
	for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; PropertyIndex++)
	{
		for (int32 FrequencyIndex = 0; FrequencyIndex < SF_NumFrequencies; FrequencyIndex++)
		{
			ExpressionCodeMap[PropertyIndex][FrequencyIndex].Empty();
		}
	}

	FString MarkupSource;
	if (Material->GetMaterialResource(GRHIFeatureLevel)->GetMaterialExpressionSource(MarkupSource, ExpressionCodeMap))
	{
		// Remove line-feeds and leave just CRs so the character counts match the selection ranges.
		MarkupSource.ReplaceInline(TEXT("\r"), TEXT(""));

		// Improve formatting: Convert tab to 4 spaces since STextBlock (currently) doesn't show tab characters
		MarkupSource.ReplaceInline(TEXT("\t"), TEXT("    "));

		// Extract highlight ranges from markup tags

		// Make a copy so we can insert null terminators.
		TCHAR* MarkupSourceCopy = new TCHAR[MarkupSource.Len()+1];
		FCString::Strcpy(MarkupSourceCopy, MarkupSource.Len()+1, *MarkupSource);

		TCHAR* Ptr = MarkupSourceCopy;
		while( Ptr && *Ptr != '\0' )
		{
			TCHAR* NextTag = FCString::Strstr( Ptr, MARKTAG );
			if( !NextTag )
			{
				// No more tags, so we're done!
				HLSLCode += Ptr;
				break;
			}

			// Copy the text up to the tag.
			*NextTag = '\0';
			HLSLCode += Ptr;

			// Advance past the markup tag to see what type it is (beginning or end)
			NextTag += MARKTAGLEN;
			int32 TagNumber = FCString::Atoi(NextTag+1);
			Ptr = FCString::Strstr(NextTag, TEXT("*/")) + 2;
		}

		delete[] MarkupSourceCopy;
	}
}

void FMaterialEditor::UpdatePreviewMaterial( )
{
	bStatsFromPreviewMaterial = true;

	if( PreviewExpression && ExpressionPreviewMaterial )
	{
		PreviewExpression->ConnectToPreviewMaterial(ExpressionPreviewMaterial,0);
	}

	if(PreviewExpression)
	{
		// The preview material's expressions array must stay up to date before recompiling 
		// So that RebuildMaterialFunctionInfo will see all the nested material functions that may need to be updated
		ExpressionPreviewMaterial->Expressions = Material->Expressions;

		// If we are previewing an expression, update the expression preview material
		ExpressionPreviewMaterial->PreEditChange( NULL );
		ExpressionPreviewMaterial->PostEditChange();
	}
	else 
	{
		// Update the regular preview material when not previewing an expression.
		Material->PreEditChange( NULL );
		Material->PostEditChange();

		UpdateStatsMaterials();

		// Null out the expression preview material so they can be GC'ed
		ExpressionPreviewMaterial = NULL;
	}


	// Reregister all components that use the preview material, since UMaterial::PEC does not reregister components using a bIsPreviewMaterial=true material
	RefreshPreviewViewport();
}

void FMaterialEditor::RebuildMaterialInstanceEditors(UMaterialInstance * MatInst)
{
	FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
	TArray<UObject*> EditedAssets = AssetEditorManager.GetAllEditedAssets();

	for (int32 AssetIdx = 0; AssetIdx < EditedAssets.Num(); AssetIdx++)
	{
		UObject* EditedAsset = EditedAssets[AssetIdx];

		UMaterialInstance* SourceInstance = Cast<UMaterialInstance>(EditedAsset);

		if(!SourceInstance)
		{
			// Check to see if the EditedAssets are from material instance editor
			UMaterialEditorInstanceConstant* EditorInstance = Cast<UMaterialEditorInstanceConstant>(EditedAsset);
			if(EditorInstance && EditorInstance->SourceInstance)
			{
				SourceInstance = Cast<UMaterialInstance>(EditorInstance->SourceInstance);
			}
		}

		// Ensure the material instance is valid and not a UMaterialInstanceDynamic, as that doesn't use FMaterialInstanceEditor as its editor
		if ( SourceInstance != NULL && !SourceInstance->IsA(UMaterialInstanceDynamic::StaticClass()))
		{
			UMaterial * MICOriginalMaterial = SourceInstance->GetMaterial();
			if (MICOriginalMaterial == OriginalMaterial)
			{
				IAssetEditorInstance* EditorInstance = AssetEditorManager.FindEditorForAsset(EditedAsset, false);
				if ( EditorInstance != NULL )
				{
					FMaterialInstanceEditor* OtherEditor = static_cast<FMaterialInstanceEditor*>(EditorInstance);
					OtherEditor->RebuildMaterialInstanceEditor();
				}
			}
		}
	}
}

void FMaterialEditor::UpdateOriginalMaterial()
{
	// If the Material has compilation errors, warn the user
	for (int32 i = ERHIFeatureLevel::SM5; i >= 0; --i)
	{
		ERHIFeatureLevel::Type FeatureLevel = (ERHIFeatureLevel::Type)i;
		if( Material->GetMaterialResource(FeatureLevel)->GetCompileErrors().Num() > 0 )
		{
			FString FeatureLevelName;
			GetFeatureLevelName(FeatureLevel, FeatureLevelName);
			FSuppressableWarningDialog::FSetupInfo Info(
				FText::Format(NSLOCTEXT("UnrealEd", "Warning_CompileErrorsInMaterial", "The current material has compilation errors, so it will not render correctly in feature level {0}.\nAre you sure you wish to continue?"),FText::FromString(*FeatureLevelName)),
				NSLOCTEXT("UnrealEd", "Warning_CompileErrorsInMaterial_Title", "Warning: Compilation errors in this Material" ), "Warning_CompileErrorsInMaterial");
			Info.ConfirmText = NSLOCTEXT("ModalDialogs", "CompileErrorsInMaterialConfirm", "Continue");
			Info.CancelText = NSLOCTEXT("ModalDialogs", "CompileErrorsInMaterialCancel", "Abort");

			FSuppressableWarningDialog CompileErrorsWarning( Info );
			if( CompileErrorsWarning.ShowModal() == FSuppressableWarningDialog::Cancel )
			{
				return;
			}
		}
	}

	// Make sure any graph position changes that might not have been copied are taken into account
	Material->MaterialGraph->LinkMaterialExpressionsFromGraph();

	//remove any memory copies of shader files, so they will be reloaded from disk
	//this way the material editor can be used for quick shader iteration
	FlushShaderFileCache();

	//recompile and refresh the preview material so it will be updated if there was a shader change
	UpdatePreviewMaterial();

	const FScopedBusyCursor BusyCursor;

	const FText LocalizedMaterialEditorApply = NSLOCTEXT("UnrealEd", "ToolTip_MaterialEditorApply", "Apply changes to original material and its use in the world.");
	GWarn->BeginSlowTask( LocalizedMaterialEditorApply, true );
	GWarn->StatusUpdate( 1, 1, LocalizedMaterialEditorApply );

	// Handle propagation of the material function being edited
	if (MaterialFunction)
	{
		// Copy the expressions back from the preview material
		MaterialFunction->FunctionExpressions = Material->Expressions;
		MaterialFunction->FunctionEditorComments = Material->EditorComments;

		// Preserve the thumbnail info
		UThumbnailInfo* OriginalThumbnailInfo = MaterialFunction->ParentFunction->ThumbnailInfo;
		UThumbnailInfo* ThumbnailInfo = MaterialFunction->ThumbnailInfo;
		MaterialFunction->ParentFunction->ThumbnailInfo = NULL;
		MaterialFunction->ThumbnailInfo = NULL;

		// overwrite the original material function in place by constructing a new one with the same name
		MaterialFunction->ParentFunction = (UMaterialFunction*)StaticDuplicateObject(
			MaterialFunction, 
			MaterialFunction->ParentFunction->GetOuter(), 
			*MaterialFunction->ParentFunction->GetName(), 
			RF_AllFlags, 
			MaterialFunction->ParentFunction->GetClass());

		// Restore the thumbnail info
		MaterialFunction->ParentFunction->ThumbnailInfo = OriginalThumbnailInfo;
		MaterialFunction->ThumbnailInfo = ThumbnailInfo;

		// Restore RF_Standalone on the original material function, as it had been removed from the preview material so that it could be GC'd.
		MaterialFunction->ParentFunction->SetFlags( RF_Standalone );

		for (int32 ExpressionIndex = 0; ExpressionIndex < MaterialFunction->ParentFunction->FunctionExpressions.Num(); ExpressionIndex++)
		{
			UMaterialExpression* CurrentExpression = MaterialFunction->ParentFunction->FunctionExpressions[ExpressionIndex];

			// Link the expressions back to their function
			CurrentExpression->Material = NULL;
			CurrentExpression->Function = MaterialFunction->ParentFunction;
		}
		for (int32 ExpressionIndex = 0; ExpressionIndex < MaterialFunction->ParentFunction->FunctionEditorComments.Num(); ExpressionIndex++)
		{
			UMaterialExpressionComment* CurrentExpression = MaterialFunction->ParentFunction->FunctionEditorComments[ExpressionIndex];

			// Link the expressions back to their function
			CurrentExpression->Material = NULL;
			CurrentExpression->Function = MaterialFunction->ParentFunction;
		}

		// mark the parent function as changed
		MaterialFunction->ParentFunction->PreEditChange(NULL);
		MaterialFunction->ParentFunction->PostEditChange();
		MaterialFunction->ParentFunction->MarkPackageDirty();

		// clear the dirty flag
		bMaterialDirty = false;
		bStatsFromPreviewMaterial = false;

		// Create a material update context so we can safely update materials using this function.
		{
			FMaterialUpdateContext UpdateContext;

			// Go through all materials in memory and recompile them if they use this material function
			for (TObjectIterator<UMaterial> It; It; ++It)
			{
				UMaterial* CurrentMaterial = *It;
				if (CurrentMaterial != Material)
				{
					bool bRecompile = false;

					// Preview materials often use expressions for rendering that are not in their Expressions array, 
					// And therefore their MaterialFunctionInfos are not up to date.
					// However we don't want to trigger this if the Material is a preview material itself. This can now be the case with thumbnail preview materials for material functions.
					if (CurrentMaterial->bIsPreviewMaterial && !Material->bIsPreviewMaterial)
					{
						bRecompile = true;
					}
					else
					{
						for (int32 FunctionIndex = 0; FunctionIndex < CurrentMaterial->MaterialFunctionInfos.Num(); FunctionIndex++)
						{
							if (CurrentMaterial->MaterialFunctionInfos[FunctionIndex].Function == MaterialFunction->ParentFunction)
							{
								bRecompile = true;
								break;
							}
						}
					}

					if (bRecompile)
					{
						UpdateContext.AddMaterial(CurrentMaterial);

						// Propagate the function change to this material
						CurrentMaterial->PreEditChange(NULL);
						CurrentMaterial->PostEditChange();
						CurrentMaterial->MarkPackageDirty();

						if (CurrentMaterial->MaterialGraph)
						{
							CurrentMaterial->MaterialGraph->RebuildGraph();
						}
					}
				}
			}
		}

		// update the world's viewports
		FEditorDelegates::RefreshEditor.Broadcast();
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	}
	// Handle propagation of the material being edited
	else
	{
		// we will unregister and register components to update materials so we have to notify NavigationSystem that this is "fake" operation and we don't have to update NavMesh
		for (auto It=GEditor->GetWorldContexts().CreateConstIterator(); It; ++It)
		{
			if (UWorld* World = It->World())
			{
				if (World->GetNavigationSystem() != NULL)
					World->GetNavigationSystem()->BeginFakeComponentChanges();
			}
		}

		// Create a material update context so we can safely update materials.
		{
			FMaterialUpdateContext UpdateContext;
			UpdateContext.AddMaterial(OriginalMaterial);

			// ensure the original copy of the material is removed from the editor's selection set
			// or it will end up containing a stale, invalid entry
			if ( OriginalMaterial->IsSelected() )
			{
				GEditor->GetSelectedObjects()->Deselect( OriginalMaterial );
			}

			// Preserve the thumbnail info
			UThumbnailInfo* OriginalThumbnailInfo = OriginalMaterial->ThumbnailInfo;
			UThumbnailInfo* ThumbnailInfo = Material->ThumbnailInfo;
			OriginalMaterial->ThumbnailInfo = NULL;
			Material->ThumbnailInfo = NULL;

			// A bit hacky, but disable material compilation in post load when we duplicate the material.
			UMaterial::ForceNoCompilationInPostLoad(true);

			// overwrite the original material in place by constructing a new one with the same name
			OriginalMaterial = (UMaterial*)StaticDuplicateObject( Material, OriginalMaterial->GetOuter(), *OriginalMaterial->GetName(), 
				RF_AllFlags, 
				OriginalMaterial->GetClass());

			// Post load has been called, allow materials to be compiled in PostLoad.
			UMaterial::ForceNoCompilationInPostLoad(false);

			// Restore the thumbnail info
			OriginalMaterial->ThumbnailInfo = OriginalThumbnailInfo;
			Material->ThumbnailInfo = ThumbnailInfo;

			// Change the original material object to the new original material
			OriginalMaterialObject = OriginalMaterial;

			// Restore RF_Standalone on the original material, as it had been removed from the preview material so that it could be GC'd.
			OriginalMaterial->SetFlags( RF_Standalone );

			// Manually copy bUsedAsSpecialEngineMaterial as it is duplicate transient to prevent accidental creation of new special engine materials
			OriginalMaterial->bUsedAsSpecialEngineMaterial = Material->bUsedAsSpecialEngineMaterial;

			// If we are showing stats for mobile materials, compile the full material for ES2 here. That way we can see if permutations
			// not used for preview materials fail to compile.
			if (bShowMobileStats)
			{
				OriginalMaterial->SetFeatureLevelToCompile(ERHIFeatureLevel::ES2,true);
			}

			// let the material update itself if necessary
			OriginalMaterial->PreEditChange(NULL);

			OriginalMaterial->PostEditChange();

			OriginalMaterial->MarkPackageDirty();

			// clear the dirty flag
			bMaterialDirty = false;
			bStatsFromPreviewMaterial = false;

			// update the world's viewports
			FEditorDelegates::RefreshEditor.Broadcast();
			FEditorSupportDelegates::RedrawAllViewports.Broadcast();

			// Force particle components to update their view relevance.
			for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
			{
				It->bIsViewRelevanceDirty = true;
			}

			// Leaving this scope will update all dependent material instances.
		}
		RebuildMaterialInstanceEditors(NULL);
		for (auto It=GEditor->GetWorldContexts().CreateConstIterator(); It; ++It)
		{
			if (UWorld* World = It->World())
			{
				if (World->GetNavigationSystem() != NULL)
					World->GetNavigationSystem()->EndFakeComponentChanges();
			}
		}
	}

	GWarn->EndSlowTask();
}

void FMaterialEditor::UpdateMaterialInfoList(bool bForceDisplay)
{
	TArray< TSharedRef<class FTokenizedMessage> > Messages;

	TArray<TSharedPtr<FMaterialInfo>> TempMaterialInfoList;

	ERHIFeatureLevel::Type FeatureLevelsToDisplay[2];
	int32 NumFeatureLevels = 0;
	// Always show basic features so that errors aren't hidden
	FeatureLevelsToDisplay[NumFeatureLevels++] = GRHIFeatureLevel;
	if (bShowMobileStats)
	{
		FeatureLevelsToDisplay[NumFeatureLevels++] = ERHIFeatureLevel::ES2;
	}

	if (NumFeatureLevels > 0)
	{
		UMaterial* MaterialForStats = bStatsFromPreviewMaterial ? Material : OriginalMaterial;

		for (int32 i = 0; i < NumFeatureLevels; ++i)
		{
			TArray<FString> CompileErrors;
			ERHIFeatureLevel::Type FeatureLevel = FeatureLevelsToDisplay[i];
			const FMaterialResource* MaterialResource = MaterialForStats->GetMaterialResource(FeatureLevel);

			if (MaterialFunction && ExpressionPreviewMaterial)
			{
				// Add a compile error message for functions missing an output
				CompileErrors = ExpressionPreviewMaterial->GetMaterialResource(FeatureLevel)->GetCompileErrors();

				bool bFoundFunctionOutput = false;
				for (int32 ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ExpressionIndex++)
				{
					if (Material->Expressions[ExpressionIndex]->IsA(UMaterialExpressionFunctionOutput::StaticClass()))
					{
						bFoundFunctionOutput = true;
						break;
					}
				}

				if (!bFoundFunctionOutput)
				{
					CompileErrors.Add(TEXT("Missing a function output"));
				}
			}
			else
			{
				CompileErrors = MaterialResource->GetCompileErrors();
			}

			// Only show general info if stats enabled
			if (!MaterialFunction && bShowStats)
			{
				// Display any errors and messages in the upper left corner of the viewport.
				TArray<FString> Descriptions;
				TArray<int32> InstructionCounts;
				TArray<FString> EmptyDescriptions;
				TArray<int32> EmptyInstructionCounts;

				MaterialResource->GetRepresentativeInstructionCounts(Descriptions, InstructionCounts);

				bool bBuiltinStats = false;
				const FMaterialResource* EmptyMaterialResource = EmptyMaterial ? EmptyMaterial->GetMaterialResource(FeatureLevel) : NULL;
				if (bShowBuiltinStats && bStatsFromPreviewMaterial && EmptyMaterialResource && InstructionCounts.Num() > 0)
				{
					EmptyMaterialResource->GetRepresentativeInstructionCounts(EmptyDescriptions, EmptyInstructionCounts);

					if (EmptyInstructionCounts.Num() > 0)
					{
						//The instruction counts should match. If not, the preview material has been changed without the EmptyMaterial being updated to match.
						if (ensure(InstructionCounts.Num() == EmptyInstructionCounts.Num()))
						{
							bBuiltinStats = true;
						}
					}
				}

				for (int32 InstructionIndex = 0; InstructionIndex < Descriptions.Num(); InstructionIndex++)
				{
					FString InstructionCountString = FString::Printf(TEXT("%s: %u instructions"),*Descriptions[InstructionIndex], InstructionCounts[InstructionIndex]);
					if (bBuiltinStats)
					{
						InstructionCountString += FString::Printf(TEXT(" - Built-in instructions: %u"), EmptyInstructionCounts[InstructionIndex]);
					}
					TempMaterialInfoList.Add(MakeShareable(new FMaterialInfo(InstructionCountString, FLinearColor::Yellow)));
					TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create(EMessageSeverity::Info);
					Line->AddToken(FTextToken::Create(FText::FromString(InstructionCountString)));
					Messages.Add(Line);
				}

				// Display the number of samplers used by the material.
				const int32 SamplersUsed = MaterialResource->GetSamplerUsage();

				if (SamplersUsed >= 0)
				{
					int32 MaxSamplers = GetFeatureLevelMaxTextureSamplers(MaterialResource->GetFeatureLevel());
					FString SamplersString = FString::Printf(TEXT("%s samplers: %u/%u"), FeatureLevel == ERHIFeatureLevel::ES2 ? TEXT("Mobile texture") : TEXT("Texture"), SamplersUsed, MaxSamplers);
					TempMaterialInfoList.Add(MakeShareable(new FMaterialInfo(SamplersString, FLinearColor::Yellow)));
					TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create( EMessageSeverity::Info );
					Line->AddToken( FTextToken::Create( FText::FromString( SamplersString ) ) );
					Messages.Add(Line);
				}
			}

			FString FeatureLevelName;
			GetFeatureLevelName(FeatureLevel,FeatureLevelName);
			for(int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
			{
				FString ErrorString = FString::Printf(TEXT("[%s] %s"), *FeatureLevelName, *CompileErrors[ErrorIndex]);
				TempMaterialInfoList.Add(MakeShareable(new FMaterialInfo(ErrorString, FLinearColor::Red)));
				TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create( EMessageSeverity::Error );
				Line->AddToken( FTextToken::Create( FText::FromString( ErrorString ) ) );
				Messages.Add(Line);
				bForceDisplay = true;
			}
		}
	}

	bool bNeedsRefresh = false;
	if (TempMaterialInfoList.Num() != MaterialInfoList.Num())
	{
		bNeedsRefresh = true;
	}

	for (int32 Index = 0; !bNeedsRefresh && Index < TempMaterialInfoList.Num(); ++Index)
	{
		if (TempMaterialInfoList[Index]->Color != MaterialInfoList[Index]->Color)
		{
			bNeedsRefresh = true;
			break;
		}

		if (TempMaterialInfoList[Index]->Text != MaterialInfoList[Index]->Text)
		{
			bNeedsRefresh = true;
			break;
		}
	}

	if (bNeedsRefresh)
	{
		MaterialInfoList = TempMaterialInfoList;
		/*TSharedPtr<SWidget> TitleBar = GraphEditor->GetTitleBar();
		if (TitleBar.IsValid())
		{
			StaticCastSharedPtr<SMaterialEditorTitleBar>(TitleBar)->RequestRefresh();
		}*/

		StatsListing->ClearMessages();
		StatsListing->AddMessages(Messages);

		if (bForceDisplay)
		{
			TabManager->InvokeTab(StatsTabId);
		}
	}
}

void FMaterialEditor::UpdateGraphNodeStates()
{
	const FMaterialResource* ErrorMaterialResource = PreviewExpression ? ExpressionPreviewMaterial->GetMaterialResource(GRHIFeatureLevel) : Material->GetMaterialResource(GRHIFeatureLevel);
	const FMaterialResource* ErrorMaterialResourceES2 = NULL;
	if (bShowMobileStats)
	{
		ErrorMaterialResourceES2 = PreviewExpression ? ExpressionPreviewMaterial->GetMaterialResource(ERHIFeatureLevel::ES2) : Material->GetMaterialResource(ERHIFeatureLevel::ES2);
	}

	bool bUpdatedErrorState = false;

	// Have to loop through everything here as there's no way to be notified when the material resource updates
	for (int32 Index = 0; Index < Material->MaterialGraph->Nodes.Num(); ++Index)
	{
		UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(Material->MaterialGraph->Nodes[Index]);
		if (MaterialNode)
		{
			MaterialNode->bIsPreviewExpression = (PreviewExpression == MaterialNode->MaterialExpression);
			MaterialNode->bIsErrorExpression = (ErrorMaterialResource->GetErrorExpressions().Find(MaterialNode->MaterialExpression) != INDEX_NONE)
												|| (ErrorMaterialResourceES2 && ErrorMaterialResourceES2->GetErrorExpressions().Find(MaterialNode->MaterialExpression) != INDEX_NONE);

			if (MaterialNode->bIsErrorExpression && !MaterialNode->bHasCompilerMessage)
			{
				bUpdatedErrorState = true;
				MaterialNode->bHasCompilerMessage = true;
				MaterialNode->ErrorMsg = MaterialNode->MaterialExpression->LastErrorText;
				MaterialNode->ErrorType = EMessageSeverity::Error;
			}
			else if (!MaterialNode->bIsErrorExpression && MaterialNode->bHasCompilerMessage)
			{
				bUpdatedErrorState = true;
				MaterialNode->bHasCompilerMessage = false;
			}
		}
	}

	if (bUpdatedErrorState)
	{
		// Rebuild the SGraphNodes to display/hide error block
		GraphEditor->NotifyGraphChanged();
	}
}

void FMaterialEditor::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( EditorOptions );
	Collector.AddReferencedObject( Material );
	Collector.AddReferencedObject( OriginalMaterial );
	Collector.AddReferencedObject( MaterialFunction );
	Collector.AddReferencedObject( ExpressionPreviewMaterial );
	Collector.AddReferencedObject( EmptyMaterial );
}

void FMaterialEditor::BindCommands()
{
	const FMaterialEditorCommands& Commands = FMaterialEditorCommands::Get();
	
	ToolkitCommands->MapAction(
		Commands.Apply,
		FExecuteAction::CreateSP( this, &FMaterialEditor::OnApply ),
		FCanExecuteAction::CreateSP( this, &FMaterialEditor::OnApplyEnabled ) );

	ToolkitCommands->MapAction(
		FEditorViewportCommands::Get().ToggleRealTime,
		FExecuteAction::CreateSP( Viewport.ToSharedRef(), &SMaterialEditorViewport::ToggleRealtime ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( Viewport.ToSharedRef(), &SMaterialEditorViewport::IsRealtime ) );

	ToolkitCommands->MapAction(
		FGenericCommands::Get().Undo,
		FExecuteAction::CreateSP(this, &FMaterialEditor::UndoGraphAction));

	ToolkitCommands->MapAction(
		FGenericCommands::Get().Redo,
		FExecuteAction::CreateSP(this, &FMaterialEditor::RedoGraphAction));

	ToolkitCommands->MapAction(
		Commands.CameraHome,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnCameraHome),
		FCanExecuteAction() );

	ToolkitCommands->MapAction(
		Commands.CleanUnusedExpressions,
		FExecuteAction::CreateSP(this, &FMaterialEditor::CleanUnusedExpressions),
		FCanExecuteAction() );

	ToolkitCommands->MapAction(
		Commands.ShowHideConnectors,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnShowConnectors),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialEditor::IsOnShowConnectorsChecked));

	ToolkitCommands->MapAction(
		Commands.ToggleRealtimeExpressions,
		FExecuteAction::CreateSP(this, &FMaterialEditor::ToggleRealTimeExpressions),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialEditor::IsToggleRealTimeExpressionsChecked));

	ToolkitCommands->MapAction(
		Commands.AlwaysRefreshAllPreviews,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnAlwaysRefreshAllPreviews),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialEditor::IsOnAlwaysRefreshAllPreviews));

	ToolkitCommands->MapAction(
		Commands.ToggleMaterialStats,
		FExecuteAction::CreateSP(this, &FMaterialEditor::ToggleStats),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialEditor::IsToggleStatsChecked));

	ToolkitCommands->MapAction(
		Commands.ToggleReleaseStats,
		FExecuteAction::CreateSP(this, &FMaterialEditor::ToggleReleaseStats),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialEditor::IsToggleReleaseStatsChecked));

	ToolkitCommands->MapAction(
		Commands.ToggleBuiltinStats,
		FExecuteAction::CreateSP(this, &FMaterialEditor::ToggleBuiltinStats),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialEditor::IsToggleBuiltinStatsChecked));

	ToolkitCommands->MapAction(
		Commands.ToggleMobileStats,
		FExecuteAction::CreateSP(this, &FMaterialEditor::ToggleMobileStats),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialEditor::IsToggleMobileStatsChecked));

	ToolkitCommands->MapAction(
		Commands.UseCurrentTexture,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnUseCurrentTexture));

	ToolkitCommands->MapAction(
		Commands.ConvertObjects,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertObjects));

	ToolkitCommands->MapAction(
		Commands.ConvertToTextureObjects,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertTextures));

	ToolkitCommands->MapAction(
		Commands.ConvertToTextureSamples,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertTextures));

	ToolkitCommands->MapAction(
		Commands.StopPreviewNode,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnPreviewNode));

	ToolkitCommands->MapAction(
		Commands.StartPreviewNode,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnPreviewNode));

	ToolkitCommands->MapAction(
		Commands.EnableRealtimePreviewNode,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnToggleRealtimePreview));

	ToolkitCommands->MapAction(
		Commands.DisableRealtimePreviewNode,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnToggleRealtimePreview));

	ToolkitCommands->MapAction(
		Commands.SelectDownstreamNodes,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnSelectDownsteamNodes));

	ToolkitCommands->MapAction(
		Commands.SelectUpstreamNodes,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnSelectUpsteamNodes));

	ToolkitCommands->MapAction(
		Commands.RemoveFromFavorites,
		FExecuteAction::CreateSP(this, &FMaterialEditor::RemoveSelectedExpressionFromFavorites));
		
	ToolkitCommands->MapAction(
		Commands.AddToFavorites,
		FExecuteAction::CreateSP(this, &FMaterialEditor::AddSelectedExpressionToFavorites));

	ToolkitCommands->MapAction(
		Commands.ForceRefreshPreviews,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnForceRefreshPreviews));

	ToolkitCommands->MapAction(
		Commands.FindInMaterial,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnFindInMaterial));
}

void FMaterialEditor::OnApply()
{
	UE_LOG(LogMaterialEditor, Log, TEXT("Applying material %s"), *GetEditingObjects()[0]->GetName());

	UpdateOriginalMaterial();
}

bool FMaterialEditor::OnApplyEnabled() const
{
	return bMaterialDirty == true;
}

void FMaterialEditor::OnCameraHome()
{
	RecenterEditor();
}

void FMaterialEditor::OnShowConnectors()
{
	bHideUnusedConnectors = !bHideUnusedConnectors;
	GraphEditor->SetPinVisibility(bHideUnusedConnectors ? SGraphEditor::Pin_HideNoConnection : SGraphEditor::Pin_Show);
}

bool FMaterialEditor::IsOnShowConnectorsChecked() const
{
	return bHideUnusedConnectors == false;
}

void FMaterialEditor::ToggleRealTimeExpressions()
{
	bIsRealtime = !bIsRealtime;
}

bool FMaterialEditor::IsToggleRealTimeExpressionsChecked() const
{
	return bIsRealtime == true;
}

void FMaterialEditor::OnAlwaysRefreshAllPreviews()
{
	bAlwaysRefreshAllPreviews = !bAlwaysRefreshAllPreviews;
	if ( bAlwaysRefreshAllPreviews )
	{
		RefreshExpressionPreviews();
	}
}

bool FMaterialEditor::IsOnAlwaysRefreshAllPreviews() const
{
	return bAlwaysRefreshAllPreviews == true;
}

void FMaterialEditor::ToggleStats()
{
	// Toggle the showing of material stats each time the user presses the show stats button
	bShowStats = !bShowStats;
	UpdateMaterialInfoList(bShowStats);
}

bool FMaterialEditor::IsToggleStatsChecked() const
{
	return bShowStats == true;
}

void FMaterialEditor::ToggleReleaseStats()
{
	Material->bAllowDevelopmentShaderCompile = !Material->bAllowDevelopmentShaderCompile;
	UpdatePreviewMaterial();
}

bool FMaterialEditor::IsToggleReleaseStatsChecked() const
{
	return !Material->bAllowDevelopmentShaderCompile;
}

void FMaterialEditor::ToggleBuiltinStats()
{
	bShowBuiltinStats = !bShowBuiltinStats;

	if (bShowBuiltinStats && !bStatsFromPreviewMaterial)
	{
		//Have to be start using the preview material for stats.
		UpdatePreviewMaterial();
	}

	UpdateStatsMaterials();
}

bool FMaterialEditor::IsToggleBuiltinStatsChecked() const
{
	return bShowBuiltinStats;
}

void FMaterialEditor::ToggleMobileStats()
{
	// Toggle the showing of material stats each time the user presses the show stats button
	bShowMobileStats = !bShowMobileStats;
	UPreviewMaterial* PreviewMaterial = Cast<UPreviewMaterial>(Material);
	if (PreviewMaterial)
	{
		{
			// Sync with the rendering thread but don't reregister components. We will manually do so.
			FMaterialUpdateContext UpdateContext(FMaterialUpdateContext::EOptions::SyncWithRenderingThread);
			UpdateContext.AddMaterial(PreviewMaterial);
			PreviewMaterial->SetFeatureLevelToCompile(ERHIFeatureLevel::ES2,bShowMobileStats);
			PreviewMaterial->ForceRecompileForRendering();
			if (!bStatsFromPreviewMaterial)
			{
				OriginalMaterial->SetFeatureLevelToCompile(ERHIFeatureLevel::ES2,bShowMobileStats);
				OriginalMaterial->ForceRecompileForRendering();
			}
		}
		UpdateStatsMaterials();
		RefreshPreviewViewport();
	}
	UpdateMaterialInfoList(bShowMobileStats);
}

bool FMaterialEditor::IsToggleMobileStatsChecked() const
{
	return bShowMobileStats == true;
}

void FMaterialEditor::OnUseCurrentTexture()
{
	// Set the currently selected texture in the generic browser
	// as the texture to use in all selected texture sample expressions.
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
	UTexture* SelectedTexture = GEditor->GetSelectedObjects()->GetTop<UTexture>();
	if ( SelectedTexture )
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "UseCurrentTexture", "Use Current Texture") );
		const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
			if (GraphNode && GraphNode->MaterialExpression->IsA(UMaterialExpressionTextureBase::StaticClass()) )
			{
				UMaterialExpressionTextureBase* TextureBase = static_cast<UMaterialExpressionTextureBase*>(GraphNode->MaterialExpression);
				TextureBase->Modify();
				TextureBase->Texture = SelectedTexture;
				TextureBase->AutoSetSampleType();
			}
		}

		// Update the current preview material. 
		UpdatePreviewMaterial();
		Material->MarkPackageDirty();
		RegenerateCodeView();
		RefreshExpressionPreviews();
		SetMaterialDirty();
	}
}

void FMaterialEditor::OnConvertObjects()
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	if (SelectedNodes.Num() > 0)
	{
		const FScopedTransaction Transaction( LOCTEXT("MaterialEditorConvert", "Material Editor: Convert to Parameter") );
		Material->Modify();
		Material->MaterialGraph->Modify();
		TArray<class UEdGraphNode*> NodesToDelete;
		TArray<class UEdGraphNode*> NodesToSelect;

		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
			if (GraphNode)
			{
				// Look for the supported classes to convert from
				UMaterialExpression* CurrentSelectedExpression = GraphNode->MaterialExpression;
				UMaterialExpressionConstant* Constant1Expression = Cast<UMaterialExpressionConstant>(CurrentSelectedExpression);
				UMaterialExpressionConstant2Vector* Constant2Expression = Cast<UMaterialExpressionConstant2Vector>(CurrentSelectedExpression);
				UMaterialExpressionConstant3Vector* Constant3Expression = Cast<UMaterialExpressionConstant3Vector>(CurrentSelectedExpression);
				UMaterialExpressionConstant4Vector* Constant4Expression = Cast<UMaterialExpressionConstant4Vector>(CurrentSelectedExpression);
				UMaterialExpressionTextureSample* TextureSampleExpression = Cast<UMaterialExpressionTextureSample>(CurrentSelectedExpression);
				UMaterialExpressionComponentMask* ComponentMaskExpression = Cast<UMaterialExpressionComponentMask>(CurrentSelectedExpression);
				UMaterialExpressionParticleSubUV* ParticleSubUVExpression = Cast<UMaterialExpressionParticleSubUV>(CurrentSelectedExpression);

				// Setup the class to convert to
				UClass* ClassToCreate = NULL;
				if (Constant1Expression)
				{
					ClassToCreate = UMaterialExpressionScalarParameter::StaticClass();
				}
				else if (Constant2Expression || Constant3Expression || Constant4Expression)
				{
					ClassToCreate = UMaterialExpressionVectorParameter::StaticClass();
				}
				else if (ParticleSubUVExpression) // Has to come before the TextureSample comparison...
				{
					ClassToCreate = UMaterialExpressionTextureSampleParameterSubUV::StaticClass();
				}
				else if (TextureSampleExpression && TextureSampleExpression->Texture && TextureSampleExpression->Texture->IsA(UTextureCube::StaticClass()))
				{
					ClassToCreate = UMaterialExpressionTextureSampleParameterCube::StaticClass();
				}	
				else if (TextureSampleExpression)
				{
					ClassToCreate = UMaterialExpressionTextureSampleParameter2D::StaticClass();
				}	
				else if (ComponentMaskExpression)
				{
					ClassToCreate = UMaterialExpressionStaticComponentMaskParameter::StaticClass();
				}

				if (ClassToCreate)
				{
					UMaterialExpression* NewExpression = CreateNewMaterialExpression(ClassToCreate, FVector2D(GraphNode->NodePosX, GraphNode->NodePosY), false, true );
					if (NewExpression)
					{
						UMaterialGraphNode* NewGraphNode = CastChecked<UMaterialGraphNode>(NewExpression->GraphNode);
						NewGraphNode->ReplaceNode(GraphNode);

						bool bNeedsRefresh = false;

						// Copy over expression-specific values
						if (Constant1Expression)
						{
							bNeedsRefresh = true;
							CastChecked<UMaterialExpressionScalarParameter>(NewExpression)->DefaultValue = Constant1Expression->R;
						}
						else if (Constant2Expression)
						{
							bNeedsRefresh = true;
							CastChecked<UMaterialExpressionVectorParameter>(NewExpression)->DefaultValue = FLinearColor(Constant2Expression->R, Constant2Expression->G, 0);
						}
						else if (Constant3Expression)
						{
							bNeedsRefresh = true;
							CastChecked<UMaterialExpressionVectorParameter>(NewExpression)->DefaultValue = Constant3Expression->Constant;
							CastChecked<UMaterialExpressionVectorParameter>(NewExpression)->DefaultValue.A = 1.0f;
						}
						else if (Constant4Expression)
						{
							bNeedsRefresh = true;
							CastChecked<UMaterialExpressionVectorParameter>(NewExpression)->DefaultValue = Constant4Expression->Constant;
						}
						else if (TextureSampleExpression)
						{
							bNeedsRefresh = true;
							UMaterialExpressionTextureSampleParameter* NewTextureExpr = CastChecked<UMaterialExpressionTextureSampleParameter>(NewExpression);
							NewTextureExpr->Texture = TextureSampleExpression->Texture;
							NewTextureExpr->Coordinates = TextureSampleExpression->Coordinates;
							NewTextureExpr->AutoSetSampleType();
							NewTextureExpr->IsDefaultMeshpaintTexture = TextureSampleExpression->IsDefaultMeshpaintTexture;
							NewTextureExpr->TextureObject = TextureSampleExpression->TextureObject;
							NewTextureExpr->MipValue = TextureSampleExpression->MipValue;
							NewTextureExpr->MipValueMode = TextureSampleExpression->MipValueMode;
						}
						else if (ComponentMaskExpression)
						{
							bNeedsRefresh = true;
							UMaterialExpressionStaticComponentMaskParameter* ComponentMask = CastChecked<UMaterialExpressionStaticComponentMaskParameter>(NewExpression);
							ComponentMask->DefaultR = ComponentMaskExpression->R;
							ComponentMask->DefaultG = ComponentMaskExpression->G;
							ComponentMask->DefaultB = ComponentMaskExpression->B;
							ComponentMask->DefaultA = ComponentMaskExpression->A;
						}
						else if (ParticleSubUVExpression)
						{
							bNeedsRefresh = true;
							CastChecked<UMaterialExpressionTextureSampleParameterSubUV>(NewExpression)->Texture = ParticleSubUVExpression->Texture;
						}

						if (bNeedsRefresh)
						{
							// Refresh the expression preview if we changed its properties after it was created
							NewExpression->bNeedToUpdatePreview = true;
							RefreshExpressionPreview( NewExpression, true );
						}

						NodesToDelete.AddUnique(GraphNode);
						NodesToSelect.Add(NewGraphNode);
					}
				}
			}
		}

		// Delete the replaced nodes
		DeleteNodes(NodesToDelete);

		// Select each of the newly converted expressions
		for ( TArray<UEdGraphNode*>::TConstIterator NodeIter(NodesToSelect); NodeIter; ++NodeIter )
		{
			GraphEditor->SetNodeSelection(*NodeIter, true);
		}
	}
}

void FMaterialEditor::OnConvertTextures()
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	if (SelectedNodes.Num() > 0)
	{
		const FScopedTransaction Transaction( LOCTEXT("MaterialEditorConvertTexture", "Material Editor: Convert to Texture") );
		Material->Modify();
		Material->MaterialGraph->Modify();
		TArray<class UEdGraphNode*> NodesToDelete;
		TArray<class UEdGraphNode*> NodesToSelect;

		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
			if (GraphNode)
			{
				// Look for the supported classes to convert from
				UMaterialExpression* CurrentSelectedExpression = GraphNode->MaterialExpression;
				UMaterialExpressionTextureSample* TextureSampleExpression = Cast<UMaterialExpressionTextureSample>(CurrentSelectedExpression);
				UMaterialExpressionTextureObject* TextureObjectExpression = Cast<UMaterialExpressionTextureObject>(CurrentSelectedExpression);

				// Setup the class to convert to
				UClass* ClassToCreate = NULL;
				if (TextureSampleExpression)
				{
					ClassToCreate = UMaterialExpressionTextureObject::StaticClass();
				}
				else if (TextureObjectExpression)
				{
					ClassToCreate = UMaterialExpressionTextureSample::StaticClass();
				}

				if (ClassToCreate)
				{
					UMaterialExpression* NewExpression = CreateNewMaterialExpression(ClassToCreate, FVector2D(GraphNode->NodePosX, GraphNode->NodePosY), false, true);
					if (NewExpression)
					{
						UMaterialGraphNode* NewGraphNode = CastChecked<UMaterialGraphNode>(NewExpression->GraphNode);
						NewGraphNode->ReplaceNode(GraphNode);
						bool bNeedsRefresh = false;

						// Copy over expression-specific values
						if (TextureSampleExpression)
						{
							bNeedsRefresh = true;
							UMaterialExpressionTextureObject* NewTextureExpr = CastChecked<UMaterialExpressionTextureObject>(NewExpression);
							NewTextureExpr->Texture = TextureSampleExpression->Texture;
							NewTextureExpr->AutoSetSampleType();
							NewTextureExpr->IsDefaultMeshpaintTexture = TextureSampleExpression->IsDefaultMeshpaintTexture;
						}
						else if (TextureObjectExpression)
						{
							bNeedsRefresh = true;
							UMaterialExpressionTextureSample* NewTextureExpr = CastChecked<UMaterialExpressionTextureSample>(NewExpression);
							NewTextureExpr->Texture = TextureObjectExpression->Texture;
							NewTextureExpr->AutoSetSampleType();
							NewTextureExpr->IsDefaultMeshpaintTexture = TextureObjectExpression->IsDefaultMeshpaintTexture;
							NewTextureExpr->MipValueMode = TMVM_None;
						}

						if (bNeedsRefresh)
						{
							// Refresh the expression preview if we changed its properties after it was created
							NewExpression->bNeedToUpdatePreview = true;
							RefreshExpressionPreview( NewExpression, true );
						}

						NodesToDelete.AddUnique(GraphNode);
						NodesToSelect.Add(NewGraphNode);
					}
				}
			}
		}

		// Delete the replaced nodes
		DeleteNodes(NodesToDelete);

		// Select each of the newly converted expressions
		for ( TArray<UEdGraphNode*>::TConstIterator NodeIter(NodesToSelect); NodeIter; ++NodeIter )
		{
			GraphEditor->SetNodeSelection(*NodeIter, true);
		}
	}
}

void FMaterialEditor::OnPreviewNode()
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
			if (GraphNode)
			{
				SetPreviewExpression(GraphNode->MaterialExpression);
			}
		}
	}
}

void FMaterialEditor::OnToggleRealtimePreview()
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
			if (GraphNode)
			{
				UMaterialExpression* SelectedExpression = GraphNode->MaterialExpression;
				SelectedExpression->bRealtimePreview = !SelectedExpression->bRealtimePreview;

				if (SelectedExpression->bRealtimePreview)
				{
					SelectedExpression->bCollapsed = false;
				}

				RefreshExpressionPreviews();
				SetMaterialDirty();
			}
		}
	}
}

void FMaterialEditor::OnSelectDownsteamNodes()
{
	TArray<UMaterialGraphNode*> NodesToCheck;
	TArray<UMaterialGraphNode*> CheckedNodes;
	TArray<UMaterialGraphNode*> NodesToSelect;

	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
		if (GraphNode)
		{
			NodesToCheck.Add(GraphNode);
		}
	}

	while (NodesToCheck.Num() > 0)
	{
		UMaterialGraphNode* CurrentNode = NodesToCheck.Last();
		TArray<UEdGraphPin*> OutputPins;
		CurrentNode->GetOutputPins(OutputPins);

		for (int32 Index = 0; Index < OutputPins.Num(); ++Index)
		{
			for (int32 LinkIndex = 0; LinkIndex < OutputPins[Index]->LinkedTo.Num(); ++LinkIndex)
			{
				UMaterialGraphNode* LinkedNode = Cast<UMaterialGraphNode>(OutputPins[Index]->LinkedTo[LinkIndex]->GetOwningNode());
				if (LinkedNode)
				{
					int32 FoundIndex = -1;
					CheckedNodes.Find(LinkedNode, FoundIndex);

					if (FoundIndex < 0)
					{
						NodesToSelect.Add(LinkedNode);
						NodesToCheck.Add(LinkedNode);
					}
				}
			}
		}

		// This graph node has now been examined
		CheckedNodes.Add(CurrentNode);
		NodesToCheck.Remove(CurrentNode);
	}

	for (int32 Index = 0; Index < NodesToSelect.Num(); ++Index)
	{
		GraphEditor->SetNodeSelection(NodesToSelect[Index], true);
	}
}

void FMaterialEditor::OnSelectUpsteamNodes()
{
	TArray<UMaterialGraphNode*> NodesToCheck;
	TArray<UMaterialGraphNode*> CheckedNodes;
	TArray<UMaterialGraphNode*> NodesToSelect;

	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
		if (GraphNode)
		{
			NodesToCheck.Add(GraphNode);
		}
	}

	while (NodesToCheck.Num() > 0)
	{
		UMaterialGraphNode* CurrentNode = NodesToCheck.Last();
		TArray<UEdGraphPin*> InputPins;
		CurrentNode->GetInputPins(InputPins);

		for (int32 Index = 0; Index < InputPins.Num(); ++Index)
		{
			for (int32 LinkIndex = 0; LinkIndex < InputPins[Index]->LinkedTo.Num(); ++LinkIndex)
			{
				UMaterialGraphNode* LinkedNode = Cast<UMaterialGraphNode>(InputPins[Index]->LinkedTo[LinkIndex]->GetOwningNode());
				if (LinkedNode)
				{
					int32 FoundIndex = -1;
					CheckedNodes.Find(LinkedNode, FoundIndex);

					if (FoundIndex < 0)
					{
						NodesToSelect.Add(LinkedNode);
						NodesToCheck.Add(LinkedNode);
					}
				}
			}
		}

		// This graph node has now been examined
		CheckedNodes.Add(CurrentNode);
		NodesToCheck.Remove(CurrentNode);
	}

	for (int32 Index = 0; Index < NodesToSelect.Num(); ++Index)
	{
		GraphEditor->SetNodeSelection(NodesToSelect[Index], true);
	}
}

void FMaterialEditor::OnForceRefreshPreviews()
{
	ForceRefreshExpressionPreviews();
	RefreshPreviewViewport();
}

void FMaterialEditor::OnCreateComment()
{
	CreateNewMaterialExpressionComment(GraphEditor->GetPasteLocation());
}

void FMaterialEditor::OnCreateComponentMaskNode()
{
	CreateNewMaterialExpression(UMaterialExpressionComponentMask::StaticClass(), GraphEditor->GetPasteLocation(), true, false);
}

void FMaterialEditor::OnFindInMaterial()
{
	TabManager->InvokeTab(FindTabId);
	FindResults->FocusForUse();
}

void FMaterialEditor::RenameAssetFromRegistry(const FAssetData& InAddedAssetData, const FString& InNewName)
{
	// Grab the asset class, it will be checked for being a material function.
	UClass* Asset = FindObject<UClass>(ANY_PACKAGE, *InAddedAssetData.AssetClass.ToString());

	if(Asset->IsChildOf(UMaterialFunction::StaticClass()))
	{
		ForceRefreshExpressionPreviews();
	}
}

void FMaterialEditor::OnMaterialUsageFlagsChanged(UMaterial* MaterialThatChanged, int32 FlagThatChanged)
{
	EMaterialUsage Flag = static_cast<EMaterialUsage>(FlagThatChanged);
	if(MaterialThatChanged == OriginalMaterial)
	{
		bool bNeedsRecompile = false;
		Material->SetMaterialUsage(bNeedsRecompile, Flag, MaterialThatChanged->GetUsageByFlag(Flag));
		UpdateStatsMaterials();
	}
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_Preview(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		.Label(LOCTEXT("ViewportTabTitle", "Viewport"))
		[
			Viewport.ToSharedRef()
		];

	Viewport->OnAddedToTab( SpawnedTab );

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_GraphCanvas(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("GraphCanvasTitle", "Graph"));

	if (GraphEditor.IsValid())
	{
		SpawnedTab->SetContent(GraphEditor.ToSharedRef());
	}

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_MaterialProperties(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("LevelEditor.Tabs.Details") )
		.Label( LOCTEXT("MaterialDetailsTitle", "Details") )
		[
			MaterialDetailsView.ToSharedRef()
		];

	if (GraphEditor.IsValid())
	{
		// Since we're initialising, make sure nothing is selected
		GraphEditor->ClearSelectionSet();
	}

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_HLSLCode(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("HLSLCodeTitle", "HLSL Code"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				CodeViewUtility.ToSharedRef()
			]
			+SVerticalBox::Slot()
			.FillHeight(1)
			[
				CodeView.ToSharedRef()
			]
		];

	RegenerateCodeView();

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_Palette(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == PaletteTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("Kismet.Tabs.Palette"))
		.Label(LOCTEXT("MaterialPaletteTitle", "Palette"))
		[
			SNew( STutorialWrapper, TEXT("MaterialPalette") )
			[
				Palette.ToSharedRef()
			]
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_Stats(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == StatsTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("Kismet.Tabs.CompilerResults"))
		.Label(LOCTEXT("MaterialStatsTitle", "Stats"))
		[
			SNew( STutorialWrapper, TEXT("MaterialStats") )
			[
				Stats.ToSharedRef()
			]
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_Find(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == FindTabId);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("Kismet.Tabs.FindResults"))
		.Label(LOCTEXT("MaterialFindTitle", "Find Results"))
		[
			SNew(STutorialWrapper, TEXT("MaterialFind"))
			[
				FindResults.ToSharedRef()
			]
		];

	return SpawnedTab;
}

void FMaterialEditor::SetPreviewExpression(UMaterialExpression* NewPreviewExpression)
{
	UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(NewPreviewExpression);

	if( PreviewExpression == NewPreviewExpression )
	{
		if (FunctionOutput)
		{
			FunctionOutput->bLastPreviewed = false;
		}
		// If we are already previewing the selected expression toggle previewing off
		PreviewExpression = NULL;
		ExpressionPreviewMaterial->Expressions.Empty();
		SetPreviewMaterial( Material );
		// Recompile the preview material to get changes that might have been made during previewing
		UpdatePreviewMaterial();
	}
	else if (NewPreviewExpression)
	{
		if( ExpressionPreviewMaterial == NULL )
		{
			// Create the expression preview material if it hasnt already been created
			ExpressionPreviewMaterial = (UMaterial*)StaticConstructObject( UMaterial::StaticClass(), GetTransientPackage(), NAME_None, RF_Public);
			ExpressionPreviewMaterial->bIsPreviewMaterial = true;
		}

		if (FunctionOutput)
		{
			FunctionOutput->bLastPreviewed = true;
		}
		else
		{
			//Hooking up the output of the break expression doesn't make much sense, preview the expression feeding it instead.
			UMaterialExpressionBreakMaterialAttributes* BreakExpr = Cast<UMaterialExpressionBreakMaterialAttributes>(NewPreviewExpression);
			if( BreakExpr && BreakExpr->GetInput(0) && BreakExpr->GetInput(0)->Expression )
			{
				NewPreviewExpression = BreakExpr->GetInput(0)->Expression;
			}
		}

		// The expression preview material's expressions array must stay up to date before recompiling 
		// So that RebuildMaterialFunctionInfo will see all the nested material functions that may need to be updated
		ExpressionPreviewMaterial->Expressions = Material->Expressions;

		// The preview window should now show the expression preview material
		SetPreviewMaterial( ExpressionPreviewMaterial );

		// Set the preview expression
		PreviewExpression = NewPreviewExpression;

		// Recompile the preview material
		UpdatePreviewMaterial();
	}
}

void FMaterialEditor::JumpToNode(const UEdGraphNode* Node)
{
	GraphEditor->JumpToNode(Node, false);
}

UMaterialExpression* FMaterialEditor::CreateNewMaterialExpression(UClass* NewExpressionClass, const FVector2D& NodePos, bool bAutoSelect, bool bAutoAssignResource)
{
	check( NewExpressionClass->IsChildOf(UMaterialExpression::StaticClass()) );

	if (!IsAllowedExpressionType(NewExpressionClass, MaterialFunction != NULL))
	{
		// Disallowed types should not be visible to the ui to be placed, so we don't need a warning here
		return NULL;
	}

	// Clear the selection
	if ( bAutoSelect )
	{
		GraphEditor->ClearSelectionSet();
	}

	// Create the new expression.
	UMaterialExpression* NewExpression = NULL;
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "MaterialEditorNewExpression", "Material Editor: New Expression") );
		Material->Modify();

		UObject* ExpressionOuter = Material;
		if (MaterialFunction)
		{
			ExpressionOuter = MaterialFunction;
		}

		NewExpression = ConstructObject<UMaterialExpression>( NewExpressionClass, ExpressionOuter, NAME_None, RF_Transactional );
		Material->Expressions.Add( NewExpression );
		NewExpression->Material = Material;

		if (MaterialFunction != NULL)
		{
			// Parameters currently not supported in material functions
			check(!NewExpression->bIsParameterExpression);
		}

		// If the new expression is created connected to an input tab, offset it by this amount.
		int32 NewConnectionOffset = 0;

		// Set the expression location.
		NewExpression->MaterialExpressionEditorX = NodePos.X + NewConnectionOffset;
		NewExpression->MaterialExpressionEditorY = NodePos.Y + NewConnectionOffset;

		if (bAutoAssignResource)
		{
			// If the user is adding a texture, automatically assign the currently selected texture to it.
			UMaterialExpressionTextureBase* METextureBase = Cast<UMaterialExpressionTextureBase>( NewExpression );
			if( METextureBase )
			{
				FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
				METextureBase->Texture = GEditor->GetSelectedObjects()->GetTop<UTexture>();
				METextureBase->AutoSetSampleType();
			}

			UMaterialExpressionMaterialFunctionCall* MEMaterialFunction = Cast<UMaterialExpressionMaterialFunctionCall>( NewExpression );
			if( MEMaterialFunction )
			{
				FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
				MEMaterialFunction->SetMaterialFunction(MaterialFunction, NULL, GEditor->GetSelectedObjects()->GetTop<UMaterialFunction>());
			}

			UMaterialExpressionCollectionParameter* MECollectionParameter = Cast<UMaterialExpressionCollectionParameter>( NewExpression );
			if( MECollectionParameter )
			{
				FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
				MECollectionParameter->Collection = GEditor->GetSelectedObjects()->GetTop<UMaterialParameterCollection>();
			}
		}

		UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>( NewExpression );
		if( FunctionInput )
		{
			FunctionInput->ConditionallyGenerateId(true);
			FunctionInput->ValidateName();
		}

		UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>( NewExpression );
		if( FunctionOutput )
		{
			FunctionOutput->ConditionallyGenerateId(true);
			FunctionOutput->ValidateName();
		}

		NewExpression->UpdateParameterGuid(true, true);

		UMaterialExpressionTextureSampleParameter* TextureParameterExpression = Cast<UMaterialExpressionTextureSampleParameter>( NewExpression );
		if( TextureParameterExpression )
		{
			// Change the parameter's name on creation to mirror the object's name; this avoids issues of having colliding parameter
			// names and having the name left as "None"
			TextureParameterExpression->ParameterName = TextureParameterExpression->GetFName();
		}

		UMaterialExpressionComponentMask* ComponentMaskExpression = Cast<UMaterialExpressionComponentMask>( NewExpression );
		// Setup defaults for the most likely use case
		// Can't change default properties as that will affect existing content
		if( ComponentMaskExpression )
		{
			ComponentMaskExpression->R = true;
			ComponentMaskExpression->G = true;
		}

		UMaterialExpressionStaticComponentMaskParameter* StaticComponentMaskExpression = Cast<UMaterialExpressionStaticComponentMaskParameter>( NewExpression );
		// Setup defaults for the most likely use case
		// Can't change default properties as that will affect existing content
		if( StaticComponentMaskExpression )
		{
			StaticComponentMaskExpression->DefaultR = true;
		}

		UMaterialExpressionRotateAboutAxis* RotateAboutAxisExpression = Cast<UMaterialExpressionRotateAboutAxis>( NewExpression );
		if( RotateAboutAxisExpression )
		{
			// Create a default expression for the Position input
			UMaterialExpressionWorldPosition* WorldPositionExpression = ConstructObject<UMaterialExpressionWorldPosition>( UMaterialExpressionWorldPosition::StaticClass(), ExpressionOuter, NAME_None, RF_Transactional );
			Material->Expressions.Add( WorldPositionExpression );
			WorldPositionExpression->Material = Material;
			RotateAboutAxisExpression->Position.Expression = WorldPositionExpression;
			WorldPositionExpression->MaterialExpressionEditorX = RotateAboutAxisExpression->MaterialExpressionEditorX + 250;
			WorldPositionExpression->MaterialExpressionEditorY = RotateAboutAxisExpression->MaterialExpressionEditorY + 73;
			Material->MaterialGraph->AddExpression(WorldPositionExpression);
			if ( bAutoSelect )
			{
				GraphEditor->SetNodeSelection(WorldPositionExpression->GraphNode, true);
			}
		}

		// Setup defaults for the most likely use case
		// Can't change default properties as that will affect existing content
		UMaterialExpressionTransformPosition* PositionTransform = Cast<UMaterialExpressionTransformPosition>(NewExpression);
		if (PositionTransform)
		{
			PositionTransform->TransformSourceType = TRANSFORMPOSSOURCE_Local;
			PositionTransform->TransformType = TRANSFORMPOSSOURCE_World;
		}

		Material->AddExpressionParameter(NewExpression);

		if (NewExpression)
		{
			Material->MaterialGraph->AddExpression(NewExpression);

			// Select the new node.
			if ( bAutoSelect )
			{
				GraphEditor->SetNodeSelection(NewExpression->GraphNode, true);
			}
		}
	}

	RegenerateCodeView();

	// Update the current preview material.
	UpdatePreviewMaterial();
	Material->MarkPackageDirty();

	RefreshExpressionPreviews();
	GraphEditor->NotifyGraphChanged();
	SetMaterialDirty();
	return NewExpression;
}

UMaterialExpressionComment* FMaterialEditor::CreateNewMaterialExpressionComment(const FVector2D& NodePos)
{
	UMaterialExpressionComment* NewComment = NULL;
	{
		Material->Modify();

		UObject* ExpressionOuter = Material;
		if (MaterialFunction)
		{
			ExpressionOuter = MaterialFunction;
		}

		NewComment = ConstructObject<UMaterialExpressionComment>( UMaterialExpressionComment::StaticClass(), ExpressionOuter, NAME_None, RF_Transactional );

		// Add to the list of comments associated with this material.
		Material->EditorComments.Add( NewComment );

		FSlateRect Bounds;
		if (GraphEditor->GetBoundsForSelectedNodes(Bounds, 50.0f))
		{
			NewComment->MaterialExpressionEditorX = Bounds.Left;
			NewComment->MaterialExpressionEditorY = Bounds.Top;

			FVector2D Size = Bounds.GetSize();
			NewComment->SizeX = Size.X;
			NewComment->SizeY = Size.Y;
		}
		else
		{

			NewComment->MaterialExpressionEditorX = NodePos.X;
			NewComment->MaterialExpressionEditorY = NodePos.Y;
			NewComment->SizeX = 400;
			NewComment->SizeY = 100;
		}

		NewComment->Text = NSLOCTEXT("K2Node", "CommentBlock_NewEmptyComment", "Comment").ToString();
	}

	if (NewComment)
	{
		Material->MaterialGraph->AddComment(NewComment);

		// Select the new comment.
		GraphEditor->ClearSelectionSet();
		GraphEditor->SetNodeSelection(NewComment->GraphNode, true);
	}

	Material->MarkPackageDirty();
	GraphEditor->NotifyGraphChanged();
	SetMaterialDirty();
	return NewComment;
}

void FMaterialEditor::ForceRefreshExpressionPreviews()
{
	// Initialize expression previews.
	const bool bOldAlwaysRefreshAllPreviews = bAlwaysRefreshAllPreviews;
	bAlwaysRefreshAllPreviews = true;
	RefreshExpressionPreviews();
	bAlwaysRefreshAllPreviews = bOldAlwaysRefreshAllPreviews;
}

void FMaterialEditor::AddToSelection(UMaterialExpression* Expression)
{
	GraphEditor->SetNodeSelection(Expression->GraphNode, true);
}

void FMaterialEditor::SelectAllNodes()
{
	GraphEditor->SelectAllNodes();
}

bool FMaterialEditor::CanSelectAllNodes() const
{
	return GraphEditor.IsValid();
}

void FMaterialEditor::DeleteSelectedNodes()
{
	TArray<UEdGraphNode*> NodesToDelete;
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		NodesToDelete.Add(CastChecked<UEdGraphNode>(*NodeIt));
	}

	DeleteNodes(NodesToDelete);
}

void FMaterialEditor::DeleteNodes(const TArray<UEdGraphNode*>& NodesToDelete)
{
	if (NodesToDelete.Num() > 0)
	{
		if (!CheckExpressionRemovalWarnings(NodesToDelete))
		{
			return;
		}

		// If we are previewing an expression and the expression being previewed was deleted
		bool bHaveExpressionsToDelete			= false;
		bool bPreviewExpressionDeleted			= false;

		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "MaterialEditorDelete", "Material Editor: Delete") );
			Material->Modify();

			for (int32 Index = 0; Index < NodesToDelete.Num(); ++Index)
			{
				if (NodesToDelete[Index]->CanUserDeleteNode())
				{
					// Break all node links first so that we don't update the material before deleting
					NodesToDelete[Index]->BreakAllNodeLinks();

					FBlueprintEditorUtils::RemoveNode(NULL, NodesToDelete[Index], true);

					if (UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(NodesToDelete[Index]))
					{
						UMaterialExpression* MaterialExpression = GraphNode->MaterialExpression;

						bHaveExpressionsToDelete = true;

						DestroyColorPicker();

						if( PreviewExpression == MaterialExpression )
						{
							// The expression being previewed is also being deleted
							bPreviewExpressionDeleted = true;
						}

						MaterialExpression->Modify();
						Material->Expressions.Remove( MaterialExpression );
						Material->RemoveExpressionParameter(MaterialExpression);
						// Make sure the deleted expression is caught by gc
						MaterialExpression->MarkPendingKill();
					}
					else if (UMaterialGraphNode_Comment* CommentNode = Cast<UMaterialGraphNode_Comment>(NodesToDelete[Index]))
					{
						CommentNode->MaterialExpressionComment->Modify();
						Material->EditorComments.Remove( CommentNode->MaterialExpressionComment );
					}
				}
			}

			Material->MaterialGraph->LinkMaterialExpressionsFromGraph();
		} // ScopedTransaction

		// Deselect all expressions and comments.
		GraphEditor->ClearSelectionSet();
		GraphEditor->NotifyGraphChanged();

		if ( bHaveExpressionsToDelete )
		{
			if( bPreviewExpressionDeleted )
			{
				// The preview expression was deleted.  Null out our reference to it and reset to the normal preview material
				PreviewExpression = NULL;
				SetPreviewMaterial( Material );
			}
			RegenerateCodeView();
		}
		UpdatePreviewMaterial();
		Material->MarkPackageDirty();
		SetMaterialDirty();

		if ( bHaveExpressionsToDelete )
		{
			RefreshExpressionPreviews();
		}
	}
}

bool FMaterialEditor::CanDeleteNodes() const
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			if (Cast<UMaterialGraphNode_Root>(*NodeIt))
			{
				// Return false if only root node is selected, as it can't be deleted
				return false;
			}
		}
	}

	return SelectedNodes.Num() > 0;
}

void FMaterialEditor::DeleteSelectedDuplicatableNodes()
{
	// Cache off the old selection
	const FGraphPanelSelectionSet OldSelectedNodes = GraphEditor->GetSelectedNodes();

	// Clear the selection and only select the nodes that can be duplicated
	FGraphPanelSelectionSet RemainingNodes;
	GraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if ((Node != NULL) && Node->CanDuplicateNode())
		{
			GraphEditor->SetNodeSelection(Node, true);
		}
		else
		{
			RemainingNodes.Add(Node);
		}
	}

	// Delete the duplicatable nodes
	DeleteSelectedNodes();

	// Reselect whatever's left from the original selection after the deletion
	GraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(RemainingNodes); SelectedIter; ++SelectedIter)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
			GraphEditor->SetNodeSelection(Node, true);
		}
	}
}

void FMaterialEditor::CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	FString ExportedText;

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		if(UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
			Node->PrepareForCopying();
		}
	}

	FEdGraphUtilities::ExportNodesToText(SelectedNodes, /*out*/ ExportedText);
	FPlatformMisc::ClipboardCopy(*ExportedText);

	// Make sure Material remains the owner of the copied nodes
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		if (UMaterialGraphNode* Node = Cast<UMaterialGraphNode>(*SelectedIter))
		{
			Node->PostCopyNode();
		}
		else if (UMaterialGraphNode_Comment* Comment = Cast<UMaterialGraphNode_Comment>(*SelectedIter))
		{
			Comment->PostCopyNode();
		}
	}
}

bool FMaterialEditor::CanCopyNodes() const
{
	// If any of the nodes can be duplicated then we should allow copying
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if ((Node != NULL) && Node->CanDuplicateNode())
		{
			return true;
		}
	}
	return false;
}

void FMaterialEditor::PasteNodes()
{
	PasteNodesHere(GraphEditor->GetPasteLocation());
}

void FMaterialEditor::PasteNodesHere(const FVector2D& Location)
{
	// Undo/Redo support
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "MaterialEditorPaste", "Material Editor: Paste") );
	Material->MaterialGraph->Modify();
	Material->Modify();

	// Clear the selection set (newly pasted stuff will be selected)
	GraphEditor->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(Material->MaterialGraph, TextToImport, /*out*/ PastedNodes);

	//Average position of nodes so we can move them while still maintaining relative distances to each other
	FVector2D AvgNodePosition(0.0f,0.0f);

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* Node = *It;
		AvgNodePosition.X += Node->NodePosX;
		AvgNodePosition.Y += Node->NodePosY;
	}

	if ( PastedNodes.Num() > 0 )
	{
		float InvNumNodes = 1.0f/float(PastedNodes.Num());
		AvgNodePosition.X *= InvNumNodes;
		AvgNodePosition.Y *= InvNumNodes;
	}

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* Node = *It;
		if (UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(Node))
		{
			// These are not copied and we must account for expressions pasted between different materials anyway
			GraphNode->RealtimeDelegate = Material->MaterialGraph->RealtimeDelegate;
			GraphNode->MaterialDirtyDelegate = Material->MaterialGraph->MaterialDirtyDelegate;
			GraphNode->bPreviewNeedsUpdate = false;

			UMaterialExpression* NewExpression = GraphNode->MaterialExpression;
			NewExpression->Material = Material;
			NewExpression->Function = NULL;
			Material->Expressions.Add(NewExpression);

			// There can be only one default mesh paint texture.
			UMaterialExpressionTextureBase* TextureSample = Cast<UMaterialExpressionTextureBase>( NewExpression );
			if( TextureSample )
			{
				TextureSample->IsDefaultMeshpaintTexture = false;
			}

			NewExpression->UpdateParameterGuid(true, true);

			UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>( NewExpression );
			if( FunctionInput )
			{
				FunctionInput->ConditionallyGenerateId(true);
				FunctionInput->ValidateName();
			}

			UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>( NewExpression );
			if( FunctionOutput )
			{
				FunctionOutput->ConditionallyGenerateId(true);
				FunctionOutput->ValidateName();
			}
		}
		else if (UMaterialGraphNode_Comment* CommentNode = Cast<UMaterialGraphNode_Comment>(Node))
		{
			CommentNode->MaterialDirtyDelegate = Material->MaterialGraph->MaterialDirtyDelegate;
			CommentNode->MaterialExpressionComment->Material = Material;
			Material->EditorComments.Add(CommentNode->MaterialExpressionComment);
		}

		// Select the newly pasted stuff
		GraphEditor->SetNodeSelection(Node, true);

		Node->NodePosX = (Node->NodePosX - AvgNodePosition.X) + Location.X ;
		Node->NodePosY = (Node->NodePosY - AvgNodePosition.Y) + Location.Y ;

		Node->SnapToGrid(SNodePanel::GetSnapGridSize());

		// Give new node a different Guid from the old one
		Node->CreateNewGuid();
	}

	// Force new pasted Material Expressions to have same connections as graph nodes
	Material->MaterialGraph->LinkMaterialExpressionsFromGraph();

	// Update UI
	GraphEditor->NotifyGraphChanged();

	Material->PostEditChange();
	Material->MarkPackageDirty();
}

bool FMaterialEditor::CanPasteNodes() const
{
	FString ClipboardContent;
	FPlatformMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(Material->MaterialGraph, ClipboardContent);
}

void FMaterialEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	// Cut should only delete nodes that can be duplicated
	DeleteSelectedDuplicatableNodes();
}

bool FMaterialEditor::CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FMaterialEditor::DuplicateNodes()
{
	// Copy and paste current selection
	CopySelectedNodes();
	PasteNodes();
}

bool FMaterialEditor::CanDuplicateNodes() const
{
	return CanCopyNodes();
}

FString FMaterialEditor::GetOriginalObjectName() const
{
	return GetEditingObjects()[0]->GetName();
}

void FMaterialEditor::UpdateMaterialAfterGraphChange()
{
	Material->MaterialGraph->LinkMaterialExpressionsFromGraph();

	// Update the current preview material.
	UpdatePreviewMaterial();

	Material->MarkPackageDirty();
	RegenerateCodeView();
	RefreshExpressionPreviews();
	SetMaterialDirty();
}

int32 FMaterialEditor::GetNumberOfSelectedNodes() const
{
	return GraphEditor->GetSelectedNodes().Num();
}

FMaterialRenderProxy* FMaterialEditor::GetExpressionPreview(UMaterialExpression* InExpression)
{
	bool bNewlyCreated;
	return GetExpressionPreview(InExpression, bNewlyCreated);
}

void FMaterialEditor::UndoGraphAction()
{
	int32 NumExpressions = Material->Expressions.Num();
	GEditor->UndoTransaction();

	if(NumExpressions != Material->Expressions.Num())
	{
		Material->BuildEditorParameterList();
	}

	// Update the current preview material.
	UpdatePreviewMaterial();

	RefreshExpressionPreviews();
	GraphEditor->NotifyGraphChanged();
	SetMaterialDirty();
}

void FMaterialEditor::RedoGraphAction()
{
	// Clear selection, to avoid holding refs to nodes that go away
	GraphEditor->ClearSelectionSet();

	int32 NumExpressions = Material->Expressions.Num();
	GEditor->RedoTransaction();

	if(NumExpressions != Material->Expressions.Num())
	{
		Material->BuildEditorParameterList();
	}

	// Update the current preview material.
	UpdatePreviewMaterial();

	RefreshExpressionPreviews();
	GraphEditor->NotifyGraphChanged();
	SetMaterialDirty();
}

void FMaterialEditor::PostUndo(bool bSuccess)
{
	GraphEditor->ClearSelectionSet();

	Material->BuildEditorParameterList();

	// Update the current preview material.
	UpdatePreviewMaterial();

	RefreshExpressionPreviews();
	GraphEditor->NotifyGraphChanged();
	SetMaterialDirty();
}

void FMaterialEditor::NotifyPreChange(UProperty* PropertyAboutToChange)
{
	check( !ScopedTransaction );
	ScopedTransaction = new FScopedTransaction( NSLOCTEXT("UnrealEd", "MaterialEditorEditProperties", "Material Editor: Edit Properties") );
	FlushRenderingCommands();
}

void FMaterialEditor::NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	check( ScopedTransaction );

	if ( PropertyThatChanged )
	{
		const FName NameOfPropertyThatChanged( *PropertyThatChanged->GetName() );
		if ( NameOfPropertyThatChanged == FName(TEXT("PreviewMesh")) ||
			NameOfPropertyThatChanged == FName(TEXT("bUsedWithSkeletalMesh")) )
		{
			// SetPreviewMesh will return false if the material has bUsedWithSkeletalMesh and
			// a skeleton was requested, in which case revert to a sphere static mesh.
			if ( !SetPreviewMesh( *Material->PreviewMesh.AssetLongPathname ) )
			{
				SetPreviewMesh( GUnrealEd->GetThumbnailManager()->EditorSphere, NULL );
			}
		}

		FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UMaterialGraphNode* SelectedNode = Cast<UMaterialGraphNode>(*NodeIt);

			if (SelectedNode && SelectedNode->MaterialExpression)
			{
				if(NameOfPropertyThatChanged == FName(TEXT("ParameterName")))
				{
					Material->UpdateExpressionParameterName(SelectedNode->MaterialExpression);
				}
				else if (NameOfPropertyThatChanged == FName(TEXT("ParamNames")))
				{
					Material->UpdateExpressionDynamicParameterNames(SelectedNode->MaterialExpression);
				}
				else
				{
					Material->PropagateExpressionParameterChanges(SelectedNode->MaterialExpression);
				}
			}
		}
	}

	// Prevent constant recompilation of materials while properties are being interacted with
	if( PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive )
	{
		// Also prevent recompilation when properties have no effect on material output
		const FName PropertyName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
		if (PropertyName != GET_MEMBER_NAME_CHECKED(UMaterialExpressionComment, Text)
		&& PropertyName != GET_MEMBER_NAME_CHECKED(UMaterialExpressionComment, CommentColor)
		&& PropertyName != GET_MEMBER_NAME_CHECKED(UMaterialExpression, Desc))
		{
			// Update the current preview material.
			UpdatePreviewMaterial();
			RefreshExpressionPreviews();
			RegenerateCodeView();
		}
	}

	delete ScopedTransaction;
	ScopedTransaction = NULL;

	Material->MarkPackageDirty();
	SetMaterialDirty();
}

void FMaterialEditor::ToggleCollapsed(UMaterialExpression* MaterialExpression)
{
	check( MaterialExpression );
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "MaterialEditorToggleCollapsed", "Material Editor: Toggle Collapsed") );
		MaterialExpression->Modify();
		MaterialExpression->bCollapsed = !MaterialExpression->bCollapsed;
	}
	MaterialExpression->PreEditChange( NULL );
	MaterialExpression->PostEditChange();
	MaterialExpression->MarkPackageDirty();
	SetMaterialDirty();

	// Update the preview.
	RefreshExpressionPreview( MaterialExpression, true );
	RefreshPreviewViewport();
}

void FMaterialEditor::RefreshExpressionPreviews()
{
	const FScopedBusyCursor BusyCursor;

	if ( bAlwaysRefreshAllPreviews )
	{
		// we need to make sure the rendering thread isn't drawing these tiles
		SCOPED_SUSPEND_RENDERING_THREAD(true);

		// Refresh all expression previews.
		ExpressionPreviews.Empty();
	}
	else
	{
		// Only refresh expressions that are marked for realtime update.
		for ( int32 ExpressionIndex = 0 ; ExpressionIndex < Material->Expressions.Num() ; ++ExpressionIndex )
		{
			UMaterialExpression* MaterialExpression = Material->Expressions[ ExpressionIndex ];
			RefreshExpressionPreview( MaterialExpression, false );
		}
	}

	TArray<FMatExpressionPreview*> ExpressionPreviewsBeingCompiled;
	ExpressionPreviewsBeingCompiled.Empty(50);

	// Go through all expression previews and create new ones as needed, and maintain a list of previews that are being compiled
	for( int32 ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ++ExpressionIndex )
	{
		UMaterialExpression* MaterialExpression = Material->Expressions[ ExpressionIndex ];
		if (MaterialExpression && !MaterialExpression->IsA(UMaterialExpressionComment::StaticClass()) )
		{
			bool bNewlyCreated;
			FMatExpressionPreview* Preview = GetExpressionPreview( MaterialExpression, bNewlyCreated );
			if (bNewlyCreated && Preview)
			{
				ExpressionPreviewsBeingCompiled.Add(Preview);
			}
		}
	}
}

void FMaterialEditor::RefreshExpressionPreview(UMaterialExpression* MaterialExpression, bool bRecompile)
{
	if ( (MaterialExpression->bRealtimePreview || MaterialExpression->bNeedToUpdatePreview) && !MaterialExpression->bCollapsed )
	{
		for( int32 PreviewIndex = 0 ; PreviewIndex < ExpressionPreviews.Num() ; ++PreviewIndex )
		{
			FMatExpressionPreview& ExpressionPreview = ExpressionPreviews[PreviewIndex];
			if( ExpressionPreview.GetExpression() == MaterialExpression )
			{
				// we need to make sure the rendering thread isn't drawing this tile
				SCOPED_SUSPEND_RENDERING_THREAD(true);
				ExpressionPreviews.RemoveAt( PreviewIndex );
				MaterialExpression->bNeedToUpdatePreview = false;
				if (bRecompile)
				{
					bool bNewlyCreated;
					GetExpressionPreview(MaterialExpression, bNewlyCreated);
				}
				break;
			}
		}
	}
}

FMatExpressionPreview* FMaterialEditor::GetExpressionPreview(UMaterialExpression* MaterialExpression, bool& bNewlyCreated)
{
	bNewlyCreated = false;
	if (!MaterialExpression->bHidePreviewWindow && !MaterialExpression->bCollapsed)
	{
		FMatExpressionPreview*	Preview = NULL;
		for( int32 PreviewIndex = 0 ; PreviewIndex < ExpressionPreviews.Num() ; ++PreviewIndex )
		{
			FMatExpressionPreview& ExpressionPreview = ExpressionPreviews[PreviewIndex];
			if( ExpressionPreview.GetExpression() == MaterialExpression )
			{
				Preview = &ExpressionPreviews[PreviewIndex];
				break;
			}
		}

		if( !Preview )
		{
			bNewlyCreated = true;
			Preview = new(ExpressionPreviews) FMatExpressionPreview(MaterialExpression);
			Preview->CacheShaders(GRHIShaderPlatform, true);
		}
		return Preview;
	}

	return NULL;
}

void FMaterialEditor::PreColorPickerCommit(FLinearColor LinearColor)
{
	// Begin a property edit transaction.
	if ( GEditor )
	{
		GEditor->BeginTransaction( LOCTEXT("ModifyColorPicker", "Modify Color Picker Value") );
	}

	NotifyPreChange(NULL);

	UObject* Object = ColorPickerObject.Get(false);
	if( Object )
	{
		Object->PreEditChange(NULL);
	}
}

void FMaterialEditor::OnColorPickerCommitted(FLinearColor LinearColor)
{	
	UObject* Object = ColorPickerObject.Get(false);
	if( Object )
	{
		Object->MarkPackageDirty();
		Object->PostEditChange();
	}

	NotifyPostChange(NULL,NULL);

	if ( GEditor )
	{
		GEditor->EndTransaction();
	}

	RefreshExpressionPreviews();
}

TSharedRef<SGraphEditor> FMaterialEditor::CreateGraphEditorWidget()
{
	GraphEditorCommands = MakeShareable( new FUICommandList );
	{
		// Editing commands
		GraphEditorCommands->MapAction( FGenericCommands::Get().SelectAll,
			FExecuteAction::CreateSP( this, &FMaterialEditor::SelectAllNodes ),
			FCanExecuteAction::CreateSP( this, &FMaterialEditor::CanSelectAllNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Delete,
			FExecuteAction::CreateSP( this, &FMaterialEditor::DeleteSelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FMaterialEditor::CanDeleteNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Copy,
			FExecuteAction::CreateSP( this, &FMaterialEditor::CopySelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FMaterialEditor::CanCopyNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Paste,
			FExecuteAction::CreateSP( this, &FMaterialEditor::PasteNodes ),
			FCanExecuteAction::CreateSP( this, &FMaterialEditor::CanPasteNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Cut,
			FExecuteAction::CreateSP( this, &FMaterialEditor::CutSelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FMaterialEditor::CanCutNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Duplicate,
			FExecuteAction::CreateSP( this, &FMaterialEditor::DuplicateNodes ),
			FCanExecuteAction::CreateSP( this, &FMaterialEditor::CanDuplicateNodes )
			);

		// Graph Editor Commands
		GraphEditorCommands->MapAction( FGraphEditorCommands::Get().CreateComment,
			FExecuteAction::CreateSP( this, &FMaterialEditor::OnCreateComment )
			);

		// Material specific commands
		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().UseCurrentTexture,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnUseCurrentTexture)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().ConvertObjects,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertObjects)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().ConvertToTextureObjects,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertTextures)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().ConvertToTextureSamples,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertTextures)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().StopPreviewNode,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnPreviewNode)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().StartPreviewNode,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnPreviewNode)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().EnableRealtimePreviewNode,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnToggleRealtimePreview)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().DisableRealtimePreviewNode,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnToggleRealtimePreview)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().SelectDownstreamNodes,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnSelectDownsteamNodes)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().SelectUpstreamNodes,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnSelectUpsteamNodes)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().RemoveFromFavorites,
			FExecuteAction::CreateSP(this, &FMaterialEditor::RemoveSelectedExpressionFromFavorites)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().AddToFavorites,
			FExecuteAction::CreateSP(this, &FMaterialEditor::AddSelectedExpressionToFavorites)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().ForceRefreshPreviews,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnForceRefreshPreviews)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().CreateComponentMaskNode,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnCreateComponentMaskNode)
			);
	}

	FGraphAppearanceInfo AppearanceInfo;
	
	if (MaterialFunction)
	{
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_MaterialFunction", "MATERIAL FUNCTION").ToString();
	}
	else
	{
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Material", "MATERIAL").ToString();
	}

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FMaterialEditor::OnSelectedNodesChanged);
	InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FMaterialEditor::OnNodeDoubleClicked);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FMaterialEditor::OnNodeTitleCommitted);
	InEvents.OnSpawnNodeByShortcut = SGraphEditor::FOnSpawnNodeByShortcut::CreateSP(this, &FMaterialEditor::OnSpawnGraphNodeByShortcut, CastChecked<UEdGraph>(Material->MaterialGraph));

	// Create the title bar widget
	TSharedPtr<SWidget> TitleBarWidget = SNew(SMaterialEditorTitleBar)
		.TitleText(this, &FMaterialEditor::GetOriginalObjectName);
		//.MaterialInfoList(&MaterialInfoList);

	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(true)
		.TitleBar(TitleBarWidget)
		.Appearance(AppearanceInfo)
		.GraphToEdit(Material->MaterialGraph)
		.GraphEvents(InEvents)
		.ShowPIENotification(false);
}

void FMaterialEditor::CleanUnusedExpressions()
{
	TArray<UEdGraphNode*> UnusedNodes;

	Material->MaterialGraph->GetUnusedExpressions(UnusedNodes);

	if (UnusedNodes.Num() > 0 && CheckExpressionRemovalWarnings(UnusedNodes))
	{
		{
			// Kill off expressions referenced by the material that aren't reachable.
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "MaterialEditorCleanUnusedExpressions", "Material Editor: Clean Unused Expressions") );
				
			Material->Modify();
			Material->MaterialGraph->Modify();

			for (int32 Index = 0; Index < UnusedNodes.Num(); ++Index)
			{
				UMaterialGraphNode* GraphNode = CastChecked<UMaterialGraphNode>(UnusedNodes[Index]);
				UMaterialExpression* MaterialExpression = GraphNode->MaterialExpression;

				FBlueprintEditorUtils::RemoveNode(NULL, GraphNode, true);

				MaterialExpression->Modify();
				Material->Expressions.Remove(MaterialExpression);
				Material->RemoveExpressionParameter(MaterialExpression);
				// Make sure the deleted expression is caught by gc
				MaterialExpression->MarkPendingKill();
			}

			Material->MaterialGraph->LinkMaterialExpressionsFromGraph();
		} // ScopedTransaction

		GraphEditor->ClearSelectionSet();
		GraphEditor->NotifyGraphChanged();

		SetMaterialDirty();
	}
}

bool FMaterialEditor::CheckExpressionRemovalWarnings(const TArray<UEdGraphNode*>& NodesToRemove)
{
	FString FunctionWarningString;
	bool bFirstExpression = true;
	for (int32 Index = 0; Index < NodesToRemove.Num(); ++Index)
	{
		if (UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(NodesToRemove[Index]))
		{
			UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(GraphNode->MaterialExpression);
			UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(GraphNode->MaterialExpression);

			if (FunctionInput)
			{
				if (!bFirstExpression)
				{
					FunctionWarningString += TEXT(", ");
				}
				bFirstExpression = false;
				FunctionWarningString += FunctionInput->InputName;
			}

			if (FunctionOutput)
			{
				if (!bFirstExpression)
				{
					FunctionWarningString += TEXT(", ");
				}
				bFirstExpression = false;
				FunctionWarningString += FunctionOutput->OutputName;
			}
		}
	}

	if (FunctionWarningString.Len() > 0)
	{
		if (EAppReturnType::Yes != FMessageDialog::Open( EAppMsgType::YesNo,
				FText::Format(
					NSLOCTEXT("UnrealEd", "Prompt_MaterialEditorDeleteFunctionInputs", "Delete function inputs or outputs \"{0}\"?\nAny materials which use this function will lose their connections to these function inputs or outputs once deleted."),
					FText::FromString(FunctionWarningString) )))
		{
			// User said don't delete
			return false;
		}
	}

	return true;
}

void FMaterialEditor::RemoveSelectedExpressionFromFavorites()
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			if (UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt))
			{
				MaterialExpressionClasses::Get()->RemoveMaterialExpressionFromFavorites(GraphNode->MaterialExpression->GetClass());
				EditorOptions->FavoriteExpressions.Remove(GraphNode->MaterialExpression->GetClass()->GetName());
				EditorOptions->SaveConfig();
			}
		}
	}
}

void FMaterialEditor::AddSelectedExpressionToFavorites()
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			if (UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt))
			{
				MaterialExpressionClasses::Get()->AddMaterialExpressionToFavorites(GraphNode->MaterialExpression->GetClass());
				EditorOptions->FavoriteExpressions.AddUnique(GraphNode->MaterialExpression->GetClass()->GetName());
				EditorOptions->SaveConfig();
			}
		}
	}
}

void FMaterialEditor::OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection)
{
	TArray<UObject*> SelectedObjects;

	UObject* EditObject = Material;
	if (MaterialFunction)
	{
		EditObject = MaterialFunction;
	}

	if( NewSelection.Num() == 0 )
	{
		SelectedObjects.Add(EditObject);
	}
	else
	{
		for(TSet<class UObject*>::TConstIterator SetIt(NewSelection);SetIt;++SetIt)
		{
			if (UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*SetIt))
			{
				SelectedObjects.Add(GraphNode->MaterialExpression);
			}
			else if (UMaterialGraphNode_Comment* CommentNode = Cast<UMaterialGraphNode_Comment>(*SetIt))
			{
				SelectedObjects.Add(CommentNode->MaterialExpressionComment);
			}
			else
			{
				SelectedObjects.Add(EditObject);
			}
		}
	}

	GetDetailView()->SetObjects( SelectedObjects, true );
}

void FMaterialEditor::OnNodeDoubleClicked(class UEdGraphNode* Node)
{
	UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(Node);

	if (GraphNode)
	{
		UMaterialExpressionConstant3Vector* Constant3Expression = Cast<UMaterialExpressionConstant3Vector>(GraphNode->MaterialExpression);
		UMaterialExpressionConstant4Vector* Constant4Expression = Cast<UMaterialExpressionConstant4Vector>(GraphNode->MaterialExpression);
		UMaterialExpressionFunctionInput* InputExpression = Cast<UMaterialExpressionFunctionInput>(GraphNode->MaterialExpression);
		UMaterialExpressionVectorParameter* VectorExpression = Cast<UMaterialExpressionVectorParameter>(GraphNode->MaterialExpression);

		FColorChannels ChannelEditStruct;

		if( Constant3Expression )
		{
			ChannelEditStruct.Red = &Constant3Expression->Constant.R;
			ChannelEditStruct.Green = &Constant3Expression->Constant.G;
			ChannelEditStruct.Blue = &Constant3Expression->Constant.B;
		}
		else if( Constant4Expression )
		{
			ChannelEditStruct.Red = &Constant4Expression->Constant.R;
			ChannelEditStruct.Green = &Constant4Expression->Constant.G;
			ChannelEditStruct.Blue = &Constant4Expression->Constant.B;
			ChannelEditStruct.Alpha = &Constant4Expression->Constant.A;
		}
		else if (InputExpression)
		{
			ChannelEditStruct.Red = &InputExpression->PreviewValue.X;
			ChannelEditStruct.Green = &InputExpression->PreviewValue.Y;
			ChannelEditStruct.Blue = &InputExpression->PreviewValue.Z;
			ChannelEditStruct.Alpha = &InputExpression->PreviewValue.W;
		}
		else if (VectorExpression)
		{
			ChannelEditStruct.Red = &VectorExpression->DefaultValue.R;
			ChannelEditStruct.Green = &VectorExpression->DefaultValue.G;
			ChannelEditStruct.Blue = &VectorExpression->DefaultValue.B;
			ChannelEditStruct.Alpha = &VectorExpression->DefaultValue.A;
		}

		if (ChannelEditStruct.Red || ChannelEditStruct.Green || ChannelEditStruct.Blue || ChannelEditStruct.Alpha)
		{
			TArray<FColorChannels> Channels;
			Channels.Add(ChannelEditStruct);

			ColorPickerObject = GraphNode->MaterialExpression;

			// Open a color picker that only sends updates when Ok is clicked, 
			// Since it is too slow to recompile preview expressions as the user is picking different colors
			FColorPickerArgs PickerArgs;
			PickerArgs.ParentWidget = GraphEditor;//AsShared();
			PickerArgs.bUseAlpha = Constant4Expression != NULL || VectorExpression != NULL;
			PickerArgs.bOnlyRefreshOnOk = true;
			PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
			PickerArgs.ColorChannelsArray = &Channels;
			PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &FMaterialEditor::OnColorPickerCommitted);
			PickerArgs.PreColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &FMaterialEditor::PreColorPickerCommit);

			OpenColorPicker(PickerArgs);
		}

		UMaterialExpressionTextureSample* TextureExpression = Cast<UMaterialExpressionTextureSample>(GraphNode->MaterialExpression);
		UMaterialExpressionTextureSampleParameter* TextureParameterExpression = Cast<UMaterialExpressionTextureSampleParameter>(GraphNode->MaterialExpression);
		UMaterialExpressionMaterialFunctionCall* FunctionExpression = Cast<UMaterialExpressionMaterialFunctionCall>(GraphNode->MaterialExpression);
		UMaterialExpressionCollectionParameter* CollectionParameter = Cast<UMaterialExpressionCollectionParameter>(GraphNode->MaterialExpression);

		TArray<UObject*> ObjectsToView;
		UObject* ObjectToEdit = NULL;

		if (TextureExpression && TextureExpression->Texture)
		{
			ObjectsToView.Add(TextureExpression->Texture);
		}
		else if (TextureParameterExpression && TextureParameterExpression->Texture)
		{
			ObjectsToView.Add(TextureParameterExpression->Texture);
		}
		else if (FunctionExpression && FunctionExpression->MaterialFunction)
		{
			ObjectToEdit = FunctionExpression->MaterialFunction;
		}
		else if (CollectionParameter && CollectionParameter->Collection)
		{
			ObjectToEdit = CollectionParameter->Collection;
		}

		if (ObjectsToView.Num() > 0)
		{
			GEditor->SyncBrowserToObjects(ObjectsToView);
		}
		if (ObjectToEdit)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(ObjectToEdit);
		}
	}
}

void FMaterialEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (NodeBeingChanged)
	{
		const FScopedTransaction Transaction( LOCTEXT( "RenameNode", "Rename Node" ) );
		NodeBeingChanged->Modify();
		NodeBeingChanged->OnRenameNode(NewText.ToString());
	}
}

FReply FMaterialEditor::OnSpawnGraphNodeByShortcut(FInputGesture InGesture, const FVector2D& InPosition, UEdGraph* InGraph)
{
	UEdGraph* Graph = InGraph;

	TSharedPtr< FEdGraphSchemaAction > Action = FMaterialEditorSpawnNodeCommands::Get().GetGraphActionByGesture(InGesture, InGraph);

	if(Action.IsValid())
	{
		TArray<UEdGraphPin*> DummyPins;
		Action->PerformAction(Graph, DummyPins, InPosition);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void FMaterialEditor::UpdateStatsMaterials()
{
	if (bShowBuiltinStats && bStatsFromPreviewMaterial)
	{	
		UMaterial* StatsMaterial = Material;
		FString EmptyMaterialName = FString(TEXT("MEStatsMaterial_Empty_")) + Material->GetName();
		EmptyMaterial = (UMaterial*)StaticDuplicateObject(Material, GetTransientPackage(), *EmptyMaterialName, ~RF_Standalone, UPreviewMaterial::StaticClass());

		EmptyMaterial->SetFeatureLevelToCompile(ERHIFeatureLevel::ES2, bShowMobileStats);

		EmptyMaterial->Expressions.Empty();

		//Disconnect all properties from the expressions
		for (int32 PropIdx = 0; PropIdx < MP_MAX; ++PropIdx)
		{
			FExpressionInput* ExpInput = EmptyMaterial->GetExpressionInputForProperty((EMaterialProperty)PropIdx);
			ExpInput->Expression = NULL;
		}
		EmptyMaterial->bAllowDevelopmentShaderCompile = Material->bAllowDevelopmentShaderCompile;
		EmptyMaterial->PreEditChange(NULL);
		EmptyMaterial->PostEditChange();
	}
}

#undef LOCTEXT_NAMESPACE
