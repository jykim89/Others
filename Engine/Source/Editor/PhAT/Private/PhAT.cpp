// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "PhATModule.h"
#include "AssetSelection.h"
#include "ScopedTransaction.h"
#include "ObjectTools.h"
#include "Toolkits/IToolkitHost.h"
#include "PreviewScene.h"
#include "PhATPreviewViewportClient.h"
#include "SPhATPreviewViewport.h"
#include "PhATActions.h"
#include "PhATSharedData.h"
#include "PhATEdSkeletalMeshComponent.h"
#include "PhAT.h"



#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"

#include "WorkflowOrientedApp/SContentReference.h"
#include "AssetData.h"
#include "Developer/MeshUtilities/Public/MeshUtilities.h"
#include "UnrealEd.h"

DEFINE_LOG_CATEGORY(LogPhAT);
#define LOCTEXT_NAMESPACE "PhAT"

namespace PhAT
{
	const float	DefaultPrimSize = 15.0f;
	const float	DuplicateXOffset = 10.0f;
}


class FPhATTreeInfo
{
public:
	FPhATTreeInfo(	FName InName, 
					bool bInBold,
					int32 InParentBoneIdx = INDEX_NONE, 
					int32 InBoneOrConstraintIdx = INDEX_NONE, 
					int32 InBodyIdx = INDEX_NONE, 
					int32 InCollisionIdx = INDEX_NONE, 
					EKCollisionPrimitiveType InCollisionType = KPT_Unknown)
		: Name(InName)
		, bBold(bInBold)
		, ParentBoneIdx(InParentBoneIdx)
		, BoneOrConstraintIdx(InBoneOrConstraintIdx)
		, BodyIdx(InBodyIdx)
		, CollisionIdx(InCollisionIdx)
		, CollisionType(InCollisionType)
	{
	}

	FName Name;
	bool bBold;
	int32 ParentBoneIdx;
	int32 BoneOrConstraintIdx;
	int32 BodyIdx;
	int32 CollisionIdx;
	EKCollisionPrimitiveType CollisionType;
};


static const FName PhATPreviewViewportName("PhAT_PreviewViewport");
static const FName PhATPropertiesName("PhAT_Properties");
static const FName PhATHierarchyName("PhAT_Hierarchy");



void FPhAT::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(TabManager);

	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	TabManager->RegisterTabSpawner( PhATPreviewViewportName, FOnSpawnTab::CreateSP(this, &FPhAT::SpawnTab, PhATPreviewViewportName) )
		.SetDisplayName( LOCTEXT( "ViewportTab", "Viewport" ) );

	TabManager->RegisterTabSpawner( PhATPropertiesName, FOnSpawnTab::CreateSP(this, &FPhAT::SpawnTab, PhATPropertiesName) )
		.SetDisplayName( LOCTEXT( "PropertiesTab", "Details" ) );

	TabManager->RegisterTabSpawner( PhATHierarchyName, FOnSpawnTab::CreateSP(this, &FPhAT::SpawnTab, PhATHierarchyName) )
		.SetDisplayName( LOCTEXT( "HierarchyTab", "Hierarchy" ) );
}

void FPhAT::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(TabManager);

	TabManager->UnregisterTabSpawner(PhATPreviewViewportName);
	TabManager->UnregisterTabSpawner(PhATPropertiesName);
	TabManager->UnregisterTabSpawner(PhATHierarchyName);
}

TSharedRef<SDockTab> FPhAT::SpawnTab( const FSpawnTabArgs& TabSpawnArgs, FName TabIdentifier )
{
	if (TabIdentifier == PhATPreviewViewportName)
	{
		const TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
			.Label(LOCTEXT( "PhATViewportTitle", "Viewport" ) )
			[
				PreviewViewport.ToSharedRef()
			];

		PreviewViewport->ParentTab = SpawnedTab;

		return SpawnedTab;
	}
	else if (TabIdentifier == PhATPropertiesName)
	{
		return SNew(SDockTab)
			.Icon(FEditorStyle::GetBrush("PhAT.Tabs.Properties"))
			.Label(LOCTEXT( "PhATPropertiesTitle", "Details" ) )
			[
				Properties.ToSharedRef()
			];
	}
	else if (TabIdentifier == PhATHierarchyName)
	{
		const TSharedRef<SDockTab> NewTab = SNew(SDockTab)
			.Icon(FEditorStyle::GetBrush("PhAT.Tabs.Hierarchy"))
			.Label(LOCTEXT( "PhATHierarchyTitle", "Hierarchy" ) )
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(5.0f, 0.0f, 0.0f, 5.0f))
				[
					SNew(SHorizontalBox)
					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					[
						SAssignNew(HierarchyFilter, SComboButton)
						.ContentPadding(3)
						.OnGetMenuContent(this, &FPhAT::BuildHierarchyFilterMenu)
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text(this, &FPhAT::GetHierarchyFilter)
						]
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					
				]
				
				+ SVerticalBox::Slot()
				.Padding(FMargin(0.f, 0.f, 0.f, 0.f))
				[
					HierarchyControl.ToSharedRef()
				]
			];

		RefreshHierachyTree();

		return NewTab;
	}
	else
	{
		return SNew(SDockTab);
	}

}

FPhAT::~FPhAT()
{
	GEditor->UnregisterForUndo(this);
}

void FPhAT::InitPhAT(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UPhysicsAsset* ObjectToEdit)
{
	SimulationMode = EPSM_Normal;
	HierarchyFilterMode = PHFM_All;
	SelectedAnimation = NULL;
	SelectedSimulation = false;

	SharedData = MakeShareable(new FPhATSharedData);

	SharedData->SelectionChangedEvent.AddRaw(this, &FPhAT::SetPropertiesSelection);
	SharedData->GroupSelectionChangedEvent.AddRaw(this, &FPhAT::SetPropertiesGroupSelection);
	SharedData->HierarchyChangedEvent.AddRaw(this, &FPhAT::RefreshHierachyTree);
	SharedData->HierarchySelectionChangedEvent.AddRaw(this, &FPhAT::RefreshHierachyTreeSelection);
	SharedData->PreviewChangedEvent.AddRaw(this, &FPhAT::RefreshPreviewViewport);

	SharedData->PhysicsAsset = ObjectToEdit;

	SharedData->Initialize();

	InsideSelChanged = false;

	GEditor->RegisterForUndo(this);

	// Register our commands. This will only register them if not previously registered
	FPhATCommands::Register();

	BindCommands();

	CreateInternalWidgets();

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_PhAT_Layout_v2")
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->SetHideTabWell(true)
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewSplitter()
			->SetSizeCoefficient(0.9f)
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack() 
				->SetSizeCoefficient(0.8f)
				->AddTab(PhATPreviewViewportName, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.2f)
				->Split
				(
					FTabManager::NewStack() ->AddTab(PhATPropertiesName, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack() ->AddTab(PhATHierarchyName, ETabState::OpenedTab)
				)

			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, PhATAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectToEdit);

	IPhATModule* PhATModule = &FModuleManager::LoadModuleChecked<IPhATModule>( "PhAT" );
	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// @todo toolkit world centric editing
	/*if (IsWorldCentricAssetEditor())
	{
		SpawnToolkitTab(GetToolbarTabId(), FString(), EToolkitTabSpot::ToolBar);
		SpawnToolkitTab(FName(TEXT("PhAT_PreviewViewport")), FString(), EToolkitTabSpot::Viewport);
		SpawnToolkitTab(FName(TEXT("PhAT_Properties")), FString(), EToolkitTabSpot::Details);
		SpawnToolkitTab(FName(TEXT("PhAT_Hierarchy")), FString(), EToolkitTabSpot::Details);
	}*/
}

TSharedPtr<FPhATSharedData> FPhAT::GetSharedData() const
{
	return SharedData;
}

void FPhAT::SetPropertiesSelection(UObject* Obj, FPhATSharedData::FSelection * Body)
{
	if (Properties.IsValid())
	{
		TArray<UObject*> Selection;
		Selection.Add(Obj);
		Properties->SetObjects(Selection);
	}

	if (Hierarchy.IsValid())
	{
		if (Body)
		{
			bool bFound = false;
			for (int32 ItemIdx = 0; ItemIdx < TreeElements.Num(); ++ItemIdx)
			{
				FPhATTreeInfo& Info = *TreeElements[ItemIdx];
				if (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
				{
					if ((Info.BodyIdx == Body->Index) && (Info.CollisionType == Body->PrimitiveType) && (Info.CollisionIdx == Body->PrimitiveIndex))
					{
						Hierarchy->ClearSelection();
						Hierarchy->SetItemSelection(TreeElements[ItemIdx], true);
						bFound = true;
						break;
					}
				} 
				else
				{
					if (Info.BoneOrConstraintIdx == Body->Index)
					{
						Hierarchy->ClearSelection();
						Hierarchy->SetItemSelection(TreeElements[ItemIdx], true);
						bFound = true;
						break;
					}
				}
			}

			if (!bFound && (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit))
			{
				int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(SharedData->PhysicsAsset->BodySetup[Body->Index]->BoneName);
				for (int32 ItemIdx = 0; ItemIdx < TreeElements.Num(); ++ItemIdx)
				{
					FPhATTreeInfo& Info = *TreeElements[ItemIdx];
					if (Info.BoneOrConstraintIdx == BoneIndex)
					{
						Hierarchy->ClearSelection();
						Hierarchy->SetItemSelection(TreeElements[ItemIdx], true);
						bFound = true;
						break;
					}
				}
			}
			
			if (!InsideSelChanged && (Hierarchy->GetNumItemsSelected() > 0))
			{
				Hierarchy->RequestScrollIntoView(Hierarchy->GetSelectedItems()[0]);
			}

			// Couldn't find the item in the tree view
			check(bFound);
		}
	}
}


void FPhAT::SetPropertiesGroupSelection(const TArray<UObject*> & Objs)
{
	if (Properties.IsValid())
	{
		Properties->SetObjects(Objs);
	}
}

bool TreeElemSelected(FTreeElemPtr TreeElem, TSharedPtr<FPhATSharedData> SharedData, TSharedPtr< STreeView<FTreeElemPtr> > Hierarchy)
{
	bool bIsExpanded = Hierarchy->IsItemExpanded(TreeElem);

	if(SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		if(TreeElem->BoneOrConstraintIdx != INDEX_NONE && bIsExpanded == false)	//we're selecting a bone so ignore prims, but make sure to only do this if not expanded
		{
			for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
			{
				if (SharedData->PhysicsAsset->BodySetup[SharedData->SelectedBodies[i].Index]->BoneName == (*TreeElem).Name)
				{
					return true;
				}
			}
		}else
		{
			FPhATSharedData::FSelection Selection(TreeElem->BodyIdx, TreeElem->CollisionType, TreeElem->CollisionIdx);
			for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
			{
				if(Selection == SharedData->SelectedBodies[i])
				{
					return true;
				}
			}
		}
	}else
	{
		//for(int32 i=0; i<SharedData->SetSelectedConstraint())
	}

	return false;
}

void FPhAT::RefreshHierachyTreeSelection()
{
	if(InsideSelChanged)	//we only want to update if the change came from viewport
	{
		return;
	}

	if(Hierarchy.Get())
	{
		for(int32 i=0; i<TreeElements.Num(); ++i)
		{
			Hierarchy->SetItemSelection(TreeElements[i], TreeElemSelected(TreeElements[i], SharedData, Hierarchy), ESelectInfo::Direct);
		}
	}
}

bool FPhAT::FilterTreeElement(FTreeElemPtr TreeElem) const
{
	if (HierarchyFilterMode == PHFM_All)
	{
		return true;
	}

	if (HierarchyFilterMode == PHFM_Bodies)
	{
		if (TreeElem->bBold || TreeElem->BodyIdx != INDEX_NONE)
		{
			return true;
		}
	}

	return false;
}

void FPhAT::RefreshHierachyTree()
{
	TreeElements.Empty();
	RootBone.Empty();

	// if next event is selecting a bone to create a new body, Fill up tree with bone names
	if (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		for (int32 i = 0; i < SharedData->PhysicsAsset->BodySetup.Num(); ++i)
		{
			const int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(SharedData->PhysicsAsset->BodySetup[i]->BoneName);
			if (BoneIndex != INDEX_NONE)
			{
				const FKAggregateGeom& AggGeom = SharedData->PhysicsAsset->BodySetup[i]->AggGeom;

				if (AggGeom.SphereElems.Num() + AggGeom.BoxElems.Num() + AggGeom.SphylElems.Num() + AggGeom.ConvexElems.Num() > 0)
				{
					TreeElements.Add(FTreeElemPtr(new FPhATTreeInfo(SharedData->PhysicsAsset->BodySetup[i]->BoneName, true, INDEX_NONE, BoneIndex)));
				}
			}
		}
	}
	else
	{
		// fill tree with constraints
		for (int32 i = 0; i < SharedData->PhysicsAsset->ConstraintSetup.Num(); ++i)
		{
			const UPhysicsConstraintTemplate* Setup = SharedData->PhysicsAsset->ConstraintSetup[i];

			// try to find the joint name in the hierarchy
			const int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(Setup->DefaultInstance.JointName);

			if (BoneIndex == INDEX_NONE)
			{
				continue;
			}

			TreeElements.Add(FTreeElemPtr(new FPhATTreeInfo(SharedData->EditorSkelMesh->RefSkeleton.GetBoneName(BoneIndex), true, INDEX_NONE, i)));
		}
	}

	// Add inert bones
	for (int32 BoneIndex = 0; BoneIndex < SharedData->EditorSkelMesh->RefSkeleton.GetNum(); ++BoneIndex)
	{
		bool bFound = false;
		for (int32 ItemIdx = 0; ItemIdx < TreeElements.Num(); ++ItemIdx)
		{
			const FPhATTreeInfo& Info = *TreeElements[ItemIdx];
			if (SharedData->EditorSkelComp->GetBoneIndex(Info.Name) == BoneIndex)
			{
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			const int32 BoneOrConstraintIdx = (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)? BoneIndex: INDEX_NONE;
			TreeElements.Add(FTreeElemPtr(new FPhATTreeInfo(SharedData->EditorSkelMesh->RefSkeleton.GetBoneName(BoneIndex), false, INDEX_NONE, BoneOrConstraintIdx)));
		}
	}

	struct FCompareBoneIndex
	{
		TSharedPtr<FPhATSharedData> SharedData;

		FCompareBoneIndex(TSharedPtr<FPhATSharedData> InSharedData)
			: SharedData(InSharedData)
		{
		}

		FORCEINLINE bool operator()(const FTreeElemPtr &A, const FTreeElemPtr &B) const
		{
			const int32 ValA = SharedData->EditorSkelComp->GetBoneIndex((*A).Name);
			const int32 ValB = SharedData->EditorSkelComp->GetBoneIndex((*B).Name);
			return ValA < ValB;
		}
	};

	TreeElements.Sort(FCompareBoneIndex(SharedData));

	RootBone.Add(TreeElements[0]);

	if (Hierarchy.IsValid())
	{
		Hierarchy->RequestTreeRefresh();

		for (int32 BoneIndex = 0; BoneIndex < TreeElements.Num(); ++BoneIndex)
		{
			Hierarchy->SetItemExpansion(TreeElements[BoneIndex], true);
		}

		// Force the tree to refresh now instead of next tick
		const FGeometry Stub;
		Hierarchy->Tick(Stub, 0.0, 0.0f);

		if (!InsideSelChanged && (Hierarchy->GetNumItemsSelected() > 0))
		{
			Hierarchy->RequestScrollIntoView(Hierarchy->GetSelectedItems()[0]);
		}
	}
}

void FPhAT::RefreshPreviewViewport()
{
	if (PreviewViewport.IsValid())
	{
		PreviewViewport->RefreshViewport();
	}
}

FName FPhAT::GetToolkitFName() const
{
	return FName("PhAT");
}

FText FPhAT::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "PhAT");
}

FString FPhAT::GetWorldCentricTabPrefix() const
{
	return LOCTEXT( "WorldCentricTabPrefix", "PhAT ").ToString();
}

FLinearColor FPhAT::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

void PopulateLayoutMenu(FMenuBuilder& MenuBuilder, const TSharedRef<SDockTabStack>& DockTabStack)
{

}

void FPhAT::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SharedData->PhysicsAsset);
	Collector.AddReferencedObject(SharedData->EditorSimOptions);

	if (PreviewViewport.IsValid())
	{
		SharedData->PreviewScene.AddReferencedObjects(Collector);
	}

	Collector.AddReferencedObject(SharedData->MouseHandle);
}

void FPhAT::CycleTransformMode()
{
	if ( !SharedData->bRunningSimulation )
	{
		switch( SharedData->WidgetMode )
		{
		case FWidget::WM_Translate:
			SharedData->WidgetMode = FWidget::WM_Rotate;
			break;
		case FWidget::WM_Rotate:
			SharedData->WidgetMode = FWidget::WM_Scale;
			break;
		case FWidget::WM_Scale:
			SharedData->WidgetMode = FWidget::WM_Translate;
			break;	
		default:
			SharedData->WidgetMode = FWidget::WM_Translate;
		}

		RefreshPreviewViewport();
	}
}

void FPhAT::PostUndo(bool bSuccess)
{
	SharedData->SetSelectedBody(NULL);
	SharedData->SetSelectedConstraint(INDEX_NONE);
	RefreshHierachyTree();

	SharedData->RefreshPhysicsAssetChange(SharedData->PhysicsAsset);
}


void FPhAT::PostRedo( bool bSuccess )
{
	for (int32 BodyIdx=0; BodyIdx < SharedData->PhysicsAsset->BodySetup.Num(); ++BodyIdx)
	{
		UBodySetup* Body = SharedData->PhysicsAsset->BodySetup[BodyIdx];
		
		bool bRecreate = false;
		for (int32 ElemIdx=0; ElemIdx < Body->AggGeom.ConvexElems.Num(); ++ElemIdx)
		{
			FKConvexElem& Element = Body->AggGeom.ConvexElems[ElemIdx];

			if (Element.ConvexMesh == NULL)
			{
				bRecreate = true;
				break;
			}
		}

		if (bRecreate)
		{
			Body->InvalidatePhysicsData();
			Body->CreatePhysicsMeshes();
		}

	}

	PostUndo(bSuccess);
}


void FPhAT::CreateInternalWidgets()
{
	PreviewViewport =
	SNew(SPhATPreviewViewport)
	.PhAT(SharedThis(this));

	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	Properties = PropertyModule.CreateDetailView( Args );
	Properties->SetObject(SharedData->EditorSimOptions);


	HierarchyControl = 
	SNew(SBorder)
	.Padding(8)
	[
		SAssignNew(Hierarchy, STreeView<FTreeElemPtr>)
		.SelectionMode(ESelectionMode::Multi)
		.TreeItemsSource(&RootBone)
		.OnGetChildren(this, &FPhAT::OnGetChildrenForTree)
		.OnGenerateRow(this, &FPhAT::OnGenerateRowForTree)
		.OnSelectionChanged(this, &FPhAT::OnTreeSelectionChanged)
		.OnMouseButtonDoubleClick(this, &FPhAT::OnTreeDoubleClick)
		.OnContextMenuOpening(this, &FPhAT::OnTreeRightClick)
		.IsEnabled(this, &FPhAT::IsNotSimulation)
		.HeaderRow
		(
			SNew(SHeaderRow)
			.Visibility(EVisibility::Collapsed)
			+SHeaderRow::Column(FName(TEXT("Hierarchy")))
			.DefaultLabel( LOCTEXT( "Hierarchy", "Hierarchy" ) )
		)
	];
}

FText FPhAT::GetRepeatLastSimulationToolTip() const
{
	if(IsSimulationMode(EPSM_Normal))
	{
		return FPhATCommands::Get().SimulationNormal->GetDescription();
	}else
	{
		return FPhATCommands::Get().SimulationNoGravity->GetDescription();
	}
}

FSlateIcon FPhAT::GetRepeatLastSimulationIcon() const
{
	if(IsSimulationMode(EPSM_Normal))
	{
		return FPhATCommands::Get().SimulationNormal->GetIcon();
	}else
	{
		return FPhATCommands::Get().SimulationNoGravity->GetIcon();
	}
}

FText FPhAT::GetEditModeLabel() const
{
	if(SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		return FPhATCommands::Get().EditingMode_Body->GetLabel();
	}else
	{
		return FPhATCommands::Get().EditingMode_Constraint->GetLabel();
	}
}

FText FPhAT::GetEditModeToolTip() const
{
	if(SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		return FPhATCommands::Get().EditingMode_Body->GetDescription();
	}else
	{
		return FPhATCommands::Get().EditingMode_Constraint->GetDescription();
	}
}

FSlateIcon FPhAT::GetEditModeIcon() const
{
	if(SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		return FPhATCommands::Get().EditingMode_Body->GetIcon();
	}else
	{
		return FPhATCommands::Get().EditingMode_Constraint->GetIcon();
	}
}

void FPhAT::ExtendToolbar()
{
	struct Local
	{
		static TSharedRef< SWidget > FillSimulateOptions(TSharedRef<FUICommandList> InCommandList)
		{
			const bool bShouldCloseWindowAfterMenuSelection = true;
			FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, InCommandList );

			const FPhATCommands& Commands = FPhATCommands::Get();

			MenuBuilder.AddMenuEntry(Commands.SimulationNormal);
			MenuBuilder.AddMenuEntry(Commands.SimulationNoGravity);

			return MenuBuilder.MakeWidget();
		}

		static TSharedRef< SWidget > FillEditMode(TSharedRef<FUICommandList> InCommandList)
		{
			const bool bShouldCloseWindowAfterMenuSelection = true;
			FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, InCommandList );

			const FPhATCommands& Commands = FPhATCommands::Get();

			MenuBuilder.AddMenuEntry(Commands.EditingMode_Body);
			MenuBuilder.AddMenuEntry(Commands.EditingMode_Constraint);

			return MenuBuilder.MakeWidget();
		}

		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, TSharedRef<SWidget> PhATAnimation, FPhATSharedData::EPhATEditingMode InPhATEditingMode, FPhAT * Phat )
		{
			const FPhATCommands& Commands = FPhATCommands::Get();
			TSharedRef<FUICommandList> InCommandList = Phat->GetToolkitCommands();

			ToolbarBuilder.BeginSection("PhATSimulation");
			// Simulate
			ToolbarBuilder.AddToolBarButton( 
				Commands.RepeatLastSimulation, 
				NAME_None, 
				LOCTEXT("RepeatLastSimulation","Simulate"),
				TAttribute< FText >::Create( TAttribute< FText >::FGetter::CreateSP( Phat, &FPhAT::GetRepeatLastSimulationToolTip) ),
				TAttribute< FSlateIcon >::Create( TAttribute< FSlateIcon >::FGetter::CreateSP( Phat, &FPhAT::GetRepeatLastSimulationIcon) )
				);

			//simulate mode combo

			FUIAction SimulationMode;
			SimulationMode.CanExecuteAction = FCanExecuteAction::CreateSP(Phat, &FPhAT::IsNotSimulation);
			{
				ToolbarBuilder.AddComboButton(
					SimulationMode,
					FOnGetContent::CreateStatic( &FillSimulateOptions, InCommandList ),
					LOCTEXT( "SimulateCombo_Label", "Simulate Options" ),
					LOCTEXT( "SimulateComboToolTip", "Options for Simulation" ),
					Commands.RepeatLastSimulation->GetIcon(),
					true
					);
			}
			ToolbarBuilder.EndSection();

			//selected simulation
			ToolbarBuilder.AddToolBarButton(Commands.ToggleSelectedSimulation);

			//phat edit mode combo
			FUIAction PhatMode;
			PhatMode.CanExecuteAction = FCanExecuteAction::CreateSP(Phat, &FPhAT::IsNotSimulation);
			ToolbarBuilder.BeginSection("PhATMode");
			{
				ToolbarBuilder.AddComboButton(
					PhatMode,
					FOnGetContent::CreateStatic( &FillEditMode, InCommandList ),
					TAttribute< FText >::Create( TAttribute< FText >::FGetter::CreateSP( Phat, &FPhAT::GetEditModeLabel) ),
					TAttribute< FText >::Create( TAttribute< FText >::FGetter::CreateSP( Phat, &FPhAT::GetEditModeToolTip) ),
					TAttribute< FSlateIcon >::Create( TAttribute< FSlateIcon >::FGetter::CreateSP( Phat, &FPhAT::GetEditModeIcon) )
					);
			}
			ToolbarBuilder.EndSection();
	
			ToolbarBuilder.BeginSection("PhATTransform");
			{
				ToolbarBuilder.AddToolBarButton(Commands.PhATTranslationMode);
				ToolbarBuilder.AddToolBarButton(Commands.PhATRotationMode);
				ToolbarBuilder.AddToolBarButton(Commands.PhATScaleMode);
			}
			ToolbarBuilder.EndSection();

			if ( InPhATEditingMode == FPhATSharedData::PEM_BodyEdit )
			{
				ToolbarBuilder.BeginSection("PhATCollision");
				{
					ToolbarBuilder.AddToolBarButton(Commands.WeldToBody);
					ToolbarBuilder.AddToolBarButton(Commands.EnableCollision);
					ToolbarBuilder.AddToolBarButton(Commands.DisableCollision);
				}
				ToolbarBuilder.EndSection();
			}
	
			if ( InPhATEditingMode == FPhATSharedData::PEM_ConstraintEdit )
			{
				ToolbarBuilder.BeginSection("PhATConstraint");
				{
					ToolbarBuilder.AddToolBarButton(Commands.ConvertToBallAndSocket);
					ToolbarBuilder.AddToolBarButton(Commands.ConvertToHinge);
					ToolbarBuilder.AddToolBarButton(Commands.ConvertToPrismatic);
					ToolbarBuilder.AddToolBarButton(Commands.ConvertToSkeletal);
					ToolbarBuilder.AddToolBarButton(Commands.SnapConstraint);
				}
				ToolbarBuilder.EndSection();
			}

			ToolbarBuilder.BeginSection("PhATPlayAnimation");
			{
				ToolbarBuilder.AddToolBarButton(Commands.PlayAnimation);
			}
			ToolbarBuilder.EndSection();
	
			ToolbarBuilder.BeginSection("PhATAnimation");
			{
				ToolbarBuilder.AddWidget(PhATAnimation);
			}
			ToolbarBuilder.EndSection();
		}
	};

	// If the ToolbarExtender is valid, remove it before rebuilding it
	if ( ToolbarExtender.IsValid() )
	{
		RemoveToolbarExtender( ToolbarExtender );
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShareable(new FExtender);

	TSharedRef<SWidget> PhATAnimation = SNew(SBox)		
		.WidthOverride(250)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text( LOCTEXT( "PhATToolbarAnimation", "Animation: " ) )
			]
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SContentReference)
				.WidthOverride(80.0f)
				.AllowSelectingNewAsset(true)
				.AssetReference(this, &FPhAT::GetSelectedAnimation)
				.AllowedClass(UAnimSequence::StaticClass())
				.OnShouldFilterAsset(this, &FPhAT::ShouldFilterAssetBasedOnSkeleton)
				.OnSetReference(this, &FPhAT::AnimationSelectionChanged)
				.IsEnabled(this, &FPhAT::IsToggleSimulation)
			]
		];

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic( &Local::FillToolbar, PhATAnimation, SharedData->EditingMode, this)
		);

	AddToolbarExtender(ToolbarExtender);

	IPhATModule* PhATModule = &FModuleManager::LoadModuleChecked<IPhATModule>( "PhAT" );
	AddToolbarExtender(PhATModule->GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FPhAT::ExtendMenu()
{
	struct Local
	{
		static void FillEdit( FMenuBuilder& MenuBarBuilder )
		{
			const FPhATCommands& Commands = FPhATCommands::Get();
			MenuBarBuilder.BeginSection("Selection", LOCTEXT("PhatEditSelection", "Selection"));
			MenuBarBuilder.AddMenuEntry(Commands.SelectAllObjects);
			MenuBarBuilder.EndSection();
		}

		static void FillAsset( FMenuBuilder& MenuBarBuilder )
		{
			const FPhATCommands& Commands = FPhATCommands::Get();
			MenuBarBuilder.BeginSection("Settings", LOCTEXT("PhatAssetSettings", "Settings"));
			MenuBarBuilder.AddMenuEntry(Commands.ChangeDefaultMesh);
			MenuBarBuilder.AddMenuEntry(Commands.ApplyPhysicalMaterial);
			MenuBarBuilder.AddMenuEntry(Commands.ResetEntireAsset);
			MenuBarBuilder.EndSection();
		}
	};
	MenuExtender = MakeShareable(new FExtender);
	MenuExtender->AddMenuExtension(
		"EditHistory",
		EExtensionHook::After,
		GetToolkitCommands(), 
		FMenuExtensionDelegate::CreateStatic( &Local::FillEdit ) );

	MenuExtender->AddMenuExtension(
		"AssetEditorActions",
		EExtensionHook::After,
		GetToolkitCommands(),
		FMenuExtensionDelegate::CreateStatic( &Local::FillAsset )
		);

	AddMenuExtender(MenuExtender);

	IPhATModule* PhATModule = &FModuleManager::LoadModuleChecked<IPhATModule>( "PhAT" );
	AddMenuExtender(PhATModule->GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FPhAT::BindCommands()
{
	const FPhATCommands& Commands = FPhATCommands::Get();

	ToolkitCommands->MapAction(
		Commands.ChangeDefaultMesh,
		FExecuteAction::CreateSP(this, &FPhAT::OnChangeDefaultMesh),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.ResetEntireAsset,
		FExecuteAction::CreateSP(this, &FPhAT::OnResetEntireAsset),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.RestetBoneCollision,
		FExecuteAction::CreateSP(this, &FPhAT::OnResetBoneCollision),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsSelectedEditBodyMode));

	ToolkitCommands->MapAction(
		Commands.ApplyPhysicalMaterial,
		FExecuteAction::CreateSP(this, &FPhAT::OnApplyPhysicalMaterial),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.EditingMode_Body,
		FExecuteAction::CreateSP(this, &FPhAT::OnEditingMode, (int32)FPhATSharedData::PEM_BodyEdit),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation),
		FIsActionChecked::CreateSP(this, &FPhAT::IsEditingMode, (int32)FPhATSharedData::PEM_BodyEdit));

	ToolkitCommands->MapAction(
		Commands.EditingMode_Constraint,
		FExecuteAction::CreateSP(this, &FPhAT::OnEditingMode, (int32)FPhATSharedData::PEM_ConstraintEdit),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation),
		FIsActionChecked::CreateSP(this, &FPhAT::IsEditingMode, (int32)FPhATSharedData::PEM_ConstraintEdit));

	ToolkitCommands->MapAction(
		Commands.MovementSpace_Local,
		FExecuteAction::CreateSP(this, &FPhAT::OnMovementSpace, (int32)COORD_Local),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation),
		FIsActionChecked::CreateSP(this, &FPhAT::IsMovementSpace, (int32)COORD_Local));

	ToolkitCommands->MapAction(
		Commands.MovementSpace_World,
		FExecuteAction::CreateSP(this, &FPhAT::OnMovementSpace, (int32)COORD_World),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation),
		FIsActionChecked::CreateSP(this, &FPhAT::IsMovementSpace, (int32)COORD_World));

	ToolkitCommands->MapAction(
		Commands.PhATTranslationMode,
		FExecuteAction::CreateSP(this, &FPhAT::OnTransformMode, (int32)FWidget::WM_Translate),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation),
		FIsActionChecked::CreateSP(this, &FPhAT::IsTransformMode, (int32)FWidget::WM_Translate));

	ToolkitCommands->MapAction(
		Commands.PhATRotationMode,
		FExecuteAction::CreateSP(this, &FPhAT::OnTransformMode, (int32)FWidget::WM_Rotate),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation),
		FIsActionChecked::CreateSP(this, &FPhAT::IsTransformMode, (int32)FWidget::WM_Rotate));

	ToolkitCommands->MapAction(
		Commands.PhATScaleMode,
		FExecuteAction::CreateSP(this, &FPhAT::OnTransformMode, (int32)FWidget::WM_Scale),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation),
		FIsActionChecked::CreateSP(this, &FPhAT::IsTransformMode, (int32)FWidget::WM_Scale));

	ToolkitCommands->MapAction(
		Commands.CopyProperties,
		FExecuteAction::CreateSP(this, &FPhAT::OnCopyProperties),
		FCanExecuteAction::CreateSP(this, &FPhAT::CanCopyProperties),
		FIsActionChecked::CreateSP(this, &FPhAT::IsCopyProperties));

	ToolkitCommands->MapAction(
		Commands.PasteProperties,
		FExecuteAction::CreateSP(this, &FPhAT::OnPasteProperties),
		FCanExecuteAction::CreateSP(this, &FPhAT::CanPasteProperties));

	ToolkitCommands->MapAction(
		Commands.InstanceProperties,
		FExecuteAction::CreateSP(this, &FPhAT::OnInstanceProperties),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsInstanceProperties));

	ToolkitCommands->MapAction(
		Commands.RepeatLastSimulation,
		FExecuteAction::CreateSP(this, &FPhAT::OnToggleSimulation),
		FCanExecuteAction::CreateSP(this, &FPhAT::CanStartSimulation),
		FIsActionChecked::CreateSP(this, &FPhAT::IsToggleSimulation));

	ToolkitCommands->MapAction(
		Commands.SimulationNormal,
		FExecuteAction::CreateSP(this, &FPhAT::OnSetSimulationMode, EPSM_Normal),
		FCanExecuteAction::CreateSP(this, &FPhAT::CanStartSimulation),
		FIsActionChecked::CreateSP(this, &FPhAT::IsSimulationMode, EPSM_Normal));

	ToolkitCommands->MapAction(
		Commands.SimulationNoGravity,
		FExecuteAction::CreateSP(this, &FPhAT::OnSetSimulationMode, EPSM_Gravity),
		FCanExecuteAction::CreateSP(this, &FPhAT::CanStartSimulation),
		FIsActionChecked::CreateSP(this, &FPhAT::IsSimulationMode, EPSM_Gravity));

	ToolkitCommands->MapAction(
		Commands.ToggleSelectedSimulation,
		FExecuteAction::CreateSP(this, &FPhAT::OnToggleSelectedSimulation),
		FCanExecuteAction::CreateSP(this, &FPhAT::CanStartSimulation),
		FIsActionChecked::CreateSP(this, &FPhAT::IsSelectedSimulation));

	ToolkitCommands->MapAction(
		Commands.MeshRenderingMode_Solid,
		FExecuteAction::CreateSP(this, &FPhAT::OnMeshRenderingMode, FPhATSharedData::PRM_Solid),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsMeshRenderingMode, FPhATSharedData::PRM_Solid));

	ToolkitCommands->MapAction(
		Commands.MeshRenderingMode_Wireframe,
		FExecuteAction::CreateSP(this, &FPhAT::OnMeshRenderingMode, FPhATSharedData::PRM_Wireframe),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsMeshRenderingMode, FPhATSharedData::PRM_Wireframe));

	ToolkitCommands->MapAction(
		Commands.MeshRenderingMode_None,
		FExecuteAction::CreateSP(this, &FPhAT::OnMeshRenderingMode, FPhATSharedData::PRM_None),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsMeshRenderingMode, FPhATSharedData::PRM_None));

	ToolkitCommands->MapAction(
		Commands.CollisionRenderingMode_Solid,
		FExecuteAction::CreateSP(this, &FPhAT::OnCollisionRenderingMode, FPhATSharedData::PRM_Solid),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsCollisionRenderingMode, FPhATSharedData::PRM_Solid));

	ToolkitCommands->MapAction(
		Commands.CollisionRenderingMode_Wireframe,
		FExecuteAction::CreateSP(this, &FPhAT::OnCollisionRenderingMode, FPhATSharedData::PRM_Wireframe),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsCollisionRenderingMode, FPhATSharedData::PRM_Wireframe));

	ToolkitCommands->MapAction(
		Commands.CollisionRenderingMode_None,
		FExecuteAction::CreateSP(this, &FPhAT::OnCollisionRenderingMode, FPhATSharedData::PRM_None),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsCollisionRenderingMode, FPhATSharedData::PRM_None));

	ToolkitCommands->MapAction(
		Commands.ConstraintRenderingMode_None,
		FExecuteAction::CreateSP(this, &FPhAT::OnConstraintRenderingMode, FPhATSharedData::PCV_None),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsConstraintRenderingMode, FPhATSharedData::PCV_None));

	ToolkitCommands->MapAction(
		Commands.ConstraintRenderingMode_AllPositions,
		FExecuteAction::CreateSP(this, &FPhAT::OnConstraintRenderingMode, FPhATSharedData::PCV_AllPositions),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsConstraintRenderingMode, FPhATSharedData::PCV_AllPositions));

	ToolkitCommands->MapAction(
		Commands.ConstraintRenderingMode_AllLimits,
		FExecuteAction::CreateSP(this, &FPhAT::OnConstraintRenderingMode, FPhATSharedData::PCV_AllLimits),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsConstraintRenderingMode, FPhATSharedData::PCV_AllLimits));

	ToolkitCommands->MapAction(
		Commands.ShowKinematicBodies,
		FExecuteAction::CreateSP(this, &FPhAT::OnShowFixedBodies),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsShowFixedBodies));

	ToolkitCommands->MapAction(
		Commands.DrawGroundBox,
		FExecuteAction::CreateSP(this, &FPhAT::OnDrawGroundBox),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsDrawGroundBox));

	ToolkitCommands->MapAction(
		Commands.ToggleGraphicsHierarchy,
		FExecuteAction::CreateSP(this, &FPhAT::OnToggleGraphicsHierarchy),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsToggleGraphicsHierarchy));

	ToolkitCommands->MapAction(
		Commands.ToggleBoneInfuences,
		FExecuteAction::CreateSP(this, &FPhAT::OnToggleBoneInfluences),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsToggleBoneInfluences));

	ToolkitCommands->MapAction(
		Commands.ToggleMassProperties,
		FExecuteAction::CreateSP(this, &FPhAT::OnToggleMassProperties),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsToggleMassProperties));

	ToolkitCommands->MapAction(
		Commands.DisableCollision,
		FExecuteAction::CreateSP(this, &FPhAT::OnSetCollision, false),
		FCanExecuteAction::CreateSP(this, &FPhAT::CanSetCollision));

	ToolkitCommands->MapAction(
		Commands.EnableCollision,
		FExecuteAction::CreateSP(this, &FPhAT::OnSetCollision, true),
		FCanExecuteAction::CreateSP(this, &FPhAT::CanSetCollision));

	ToolkitCommands->MapAction(
		Commands.WeldToBody,
		FExecuteAction::CreateSP(this, &FPhAT::OnWeldToBody),
		FCanExecuteAction::CreateSP(this, &FPhAT::CanWeldToBody));

	ToolkitCommands->MapAction(
		Commands.AddNewBody,
		FExecuteAction::CreateSP(this, &FPhAT::OnAddNewBody),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsEditBodyMode));

	ToolkitCommands->MapAction(
		Commands.AddSphere,
		FExecuteAction::CreateSP(this, &FPhAT::OnAddSphere),
		FCanExecuteAction::CreateSP(this, &FPhAT::CanAddPrimitive));

	ToolkitCommands->MapAction(
		Commands.AddSphyl,
		FExecuteAction::CreateSP(this, &FPhAT::OnAddSphyl),
		FCanExecuteAction::CreateSP(this, &FPhAT::CanAddPrimitive));

	ToolkitCommands->MapAction(
		Commands.AddBox,
		FExecuteAction::CreateSP(this, &FPhAT::OnAddBox),
		FCanExecuteAction::CreateSP(this, &FPhAT::CanAddPrimitive));

	ToolkitCommands->MapAction(
		Commands.DeletePrimitive,
		FExecuteAction::CreateSP(this, &FPhAT::OnDeletePrimitive),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsSelectedEditBodyMode));

	ToolkitCommands->MapAction(
		Commands.DuplicatePrimitive,
		FExecuteAction::CreateSP(this, &FPhAT::OnDuplicatePrimitive),
		FCanExecuteAction::CreateSP(this, &FPhAT::CanDuplicatePrimitive));

	ToolkitCommands->MapAction(
		Commands.ResetConstraint,
		FExecuteAction::CreateSP(this, &FPhAT::OnResetConstraint),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsSelectedEditConstraintMode));

	ToolkitCommands->MapAction(
		Commands.SnapConstraint,
		FExecuteAction::CreateSP(this, &FPhAT::OnSnapConstraint),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsSelectedEditConstraintMode));

	ToolkitCommands->MapAction(
		Commands.ConvertToBallAndSocket,
		FExecuteAction::CreateSP(this, &FPhAT::OnConvertToBallAndSocket),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsSelectedEditConstraintMode));

	ToolkitCommands->MapAction(
		Commands.ConvertToHinge,
		FExecuteAction::CreateSP(this, &FPhAT::OnConvertToHinge),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsSelectedEditConstraintMode));

	ToolkitCommands->MapAction(
		Commands.ConvertToPrismatic,
		FExecuteAction::CreateSP(this, &FPhAT::OnConvertToPrismatic),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsSelectedEditConstraintMode));

	ToolkitCommands->MapAction(
		Commands.ConvertToSkeletal,
		FExecuteAction::CreateSP(this, &FPhAT::OnConvertToSkeletal),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsSelectedEditConstraintMode));

	ToolkitCommands->MapAction(
		Commands.DeleteConstraint,
		FExecuteAction::CreateSP(this, &FPhAT::OnDeleteConstraint),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsSelectedEditConstraintMode));

	ToolkitCommands->MapAction(
		Commands.PlayAnimation,
		FExecuteAction::CreateSP(this, &FPhAT::OnPlayAnimation),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsToggleSimulation),
		FIsActionChecked::CreateSP(this, &FPhAT::IsPlayAnimation));

	ToolkitCommands->MapAction(
		Commands.ShowSkeleton,
		FExecuteAction::CreateSP(this, &FPhAT::OnShowSkeleton),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsShowSkeleton));

	ToolkitCommands->MapAction(
		Commands.MakeBodyKinematic,
		FExecuteAction::CreateSP(this, &FPhAT::OnSetBodyPhysicsType, EPhysicsType::PhysType_Kinematic ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsBodyPhysicsType, EPhysicsType::PhysType_Kinematic ) );

	ToolkitCommands->MapAction(
		Commands.MakeBodySimulated,
		FExecuteAction::CreateSP(this, &FPhAT::OnSetBodyPhysicsType, EPhysicsType::PhysType_Simulated ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsBodyPhysicsType, EPhysicsType::PhysType_Simulated ) );

	ToolkitCommands->MapAction(
		Commands.MakeBodyDefault,
		FExecuteAction::CreateSP(this, &FPhAT::OnSetBodyPhysicsType, EPhysicsType::PhysType_Default ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhAT::IsBodyPhysicsType, EPhysicsType::PhysType_Default ) );

	ToolkitCommands->MapAction(
		Commands.KinematicAllBodiesBelow,
		FExecuteAction::CreateSP(this, &FPhAT::SetBodiesBelowSelectedPhysicsType, EPhysicsType::PhysType_Kinematic) );

	ToolkitCommands->MapAction(
		Commands.SimulatedAllBodiesBelow,
		FExecuteAction::CreateSP(this, &FPhAT::SetBodiesBelowSelectedPhysicsType, EPhysicsType::PhysType_Simulated) );

	ToolkitCommands->MapAction(
		Commands.MakeAllBodiesBelowDefault,
		FExecuteAction::CreateSP(this, &FPhAT::SetBodiesBelowSelectedPhysicsType, EPhysicsType::PhysType_Default) );

	ToolkitCommands->MapAction(
		Commands.DeleteBody,
		FExecuteAction::CreateSP(this, &FPhAT::OnDeleteBody));

	ToolkitCommands->MapAction(
		Commands.DeleteAllBodiesBelow,
		FExecuteAction::CreateSP(this, &FPhAT::OnDeleteAllBodiesBelow));

	ToolkitCommands->MapAction(
		Commands.ToggleMotor,
		FExecuteAction::CreateSP(this, &FPhAT::OnToggleMotor));

	ToolkitCommands->MapAction(
		Commands.EnableMotorsBelow,
		FExecuteAction::CreateSP(this, &FPhAT::OnEnableMotorsBelow));

	ToolkitCommands->MapAction(
		Commands.DisableMotorsBelow,
		FExecuteAction::CreateSP(this, &FPhAT::OnDisableMotorsBelow));

	ToolkitCommands->MapAction(
		Commands.SelectionLock,
		FExecuteAction::CreateSP(this, &FPhAT::OnLockSelection),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.DeleteSelected,
		FExecuteAction::CreateSP(this, &FPhAT::OnDeleteSelection),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.CycleConstraintOrientation,
		FExecuteAction::CreateSP(this, &FPhAT::OnCycleConstraintOrientation),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.CycleConstraintActive,
		FExecuteAction::CreateSP(this, &FPhAT::OnCycleConstraintActive),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.ToggleSwing1,
		FExecuteAction::CreateSP(this, &FPhAT::OnToggleSwing1),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.ToggleSwing2,
		FExecuteAction::CreateSP(this, &FPhAT::OnToggleSwing2),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.ToggleTwist,
		FExecuteAction::CreateSP(this, &FPhAT::OnToggleTwist),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.FocusOnSelection,
		FExecuteAction::CreateSP(this, &FPhAT::OnFocusSelection),
		FCanExecuteAction());
	
	ToolkitCommands->MapAction(
		Commands.CycleTransformMode,
		FExecuteAction::CreateSP(this, &FPhAT::CycleTransformMode),
		FCanExecuteAction::CreateSP(this, &FPhAT::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.SelectAllObjects,
		FExecuteAction::CreateSP(this, &FPhAT::OnSelectAll));

	ToolkitCommands->MapAction(
		Commands.HierarchyFilterAll,
		FExecuteAction::CreateSP(this, &FPhAT::SetHierarchyFilter, PHFM_All));

	ToolkitCommands->MapAction(
		Commands.HierarchyFilterBodies,
		FExecuteAction::CreateSP(this, &FPhAT::SetHierarchyFilter, PHFM_Bodies));
}

TSharedRef<ITableRow> FPhAT::OnGenerateRowForTree(FTreeElemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	class SHoverDetectTableRow : public STableRow<FTreeElemPtr>
	{
	public:
		SLATE_BEGIN_ARGS(SHoverDetectTableRow)
		{}
			SLATE_DEFAULT_SLOT( FArguments, Content )
		SLATE_END_ARGS()

		void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, FPhAT * phat )
		{
			STableRow::Construct(

				STableRow::FArguments()
				[
					InArgs._Content.Widget
				]
				
				,

				InOwnerTableView
			);

			Phat = phat;
		}

		virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE
		{
			Phat->OnTreeHighlightChanged();
		}
	
		virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) OVERRIDE
		{
			Phat->OnTreeHighlightChanged();
		}
	
	private:
		FPhAT * Phat;
	};

	return 
	SNew(SHoverDetectTableRow, OwnerTable, this)
	[
		SNew(STextBlock)
		.Font((*Item).bBold? FEditorStyle::GetFontStyle("BoldFont"): FEditorStyle::GetFontStyle("NormalFont"))
		.Text((*Item).Name.ToString())
	];
}

void FPhAT::OnTreeHighlightChanged()
{
}

void FPhAT::OnGetChildrenForTree(FTreeElemPtr Parent, TArray<FTreeElemPtr>& OutChildren)
{
	if (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		for (int32 i = 0; i < SharedData->PhysicsAsset->BodySetup.Num(); ++i)
		{
			if (SharedData->PhysicsAsset->BodySetup[i]->BoneName == (*Parent).Name)
			{
				int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(SharedData->PhysicsAsset->BodySetup[i]->BoneName);
				FKAggregateGeom& AggGeom = SharedData->PhysicsAsset->BodySetup[i]->AggGeom;

				if (AggGeom.SphereElems.Num() + AggGeom.BoxElems.Num() + AggGeom.SphylElems.Num() + AggGeom.ConvexElems.Num() > 1)
				{
					FTreeElemPtr NewElem;
					for (int32 j = 0; j < AggGeom.SphereElems.Num(); ++j)
					{
						NewElem = FTreeElemPtr(new FPhATTreeInfo(FName(*FString::Printf(TEXT("Sphere %d"), j)), false, BoneIndex, INDEX_NONE, i, j, KPT_Sphere));
						TreeElements.Add(NewElem);
						OutChildren.Add(NewElem);
					}

					for (int32 j = 0; j < AggGeom.BoxElems.Num(); ++j)
					{
						NewElem = FTreeElemPtr(new FPhATTreeInfo(FName(*FString::Printf(TEXT("Box %d"), j)), false, BoneIndex, INDEX_NONE, i, j, KPT_Box));
						TreeElements.Add(NewElem);
						OutChildren.Add(NewElem);
					}

					for (int32 j = 0; j < AggGeom.SphylElems.Num(); ++j)
					{
						NewElem = FTreeElemPtr(new FPhATTreeInfo(FName(*FString::Printf(TEXT("Sphyl %d"), j)), false, BoneIndex, INDEX_NONE, i, j, KPT_Sphyl));
						TreeElements.Add(NewElem);
						OutChildren.Add(NewElem);
					}

					for (int32 j = 0; j < AggGeom.ConvexElems.Num(); ++j)
					{
						NewElem = FTreeElemPtr(new FPhATTreeInfo(FName(*FString::Printf(TEXT("Convex %d"), j)), false, BoneIndex, INDEX_NONE, i, j, KPT_Convex));
						TreeElements.Add(NewElem);
						OutChildren.Add(NewElem);
					}
				}
			}
		}
	}

	int32 ParentIndex = SharedData->EditorSkelComp->GetBoneIndex((*Parent).Name);
	for (int32 BoneIndex = 0; BoneIndex < SharedData->EditorSkelMesh->RefSkeleton.GetNum(); ++BoneIndex)
	{
		const FMeshBoneInfo & Bone = SharedData->EditorSkelMesh->RefSkeleton.GetRefBoneInfo()[BoneIndex];
		if (Bone.ParentIndex != INDEX_NONE)
		{
			const FMeshBoneInfo & ParentBone = SharedData->EditorSkelMesh->RefSkeleton.GetRefBoneInfo()[Bone.ParentIndex];
			if ((BoneIndex != ParentIndex) && (ParentBone.Name == (*Parent).Name))
			{
				for (int32 i = 0; i < TreeElements.Num(); ++i)
				{
					if ((*TreeElements[i]).Name == Bone.Name)
					{
						if (FilterTreeElement(TreeElements[i]))
						{
							//normal element gets added
							OutChildren.Add(TreeElements[i]);
						}
						else
						{
							//we still need to see if any of this element's children get added
							OnGetChildrenForTree(TreeElements[i], OutChildren);
						}
						
					}
				}
			}
		}
	}
}

void FPhAT::OnTreeSelectionChanged(FTreeElemPtr TreeElem, ESelectInfo::Type SelectInfo)
{
	// prevent re-entrancy
	if (InsideSelChanged)
	{
		return;
	}

	if (!TreeElem.IsValid())
	{
		return;
	}

	InsideSelChanged = true;

	TArray<FTreeElemPtr> SelectedElems = Hierarchy->GetSelectedItems();

	//clear selection first
	if(SelectedElems.Num() && SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		SharedData->SetSelectedBody(NULL);	
	}
	else if(SelectedElems.Num() && SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit)
	{
		SharedData->SetSelectedConstraint(INDEX_NONE);
	}

	for(FTreeElemPtr SelectedElem : SelectedElems)
	{
		int32 ObjIndex = (*SelectedElem).BoneOrConstraintIdx;
		if (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
		{
			if (ObjIndex != INDEX_NONE)
			{
				for (int32 i = 0; i < SharedData->PhysicsAsset->BodySetup.Num(); ++i)
				{
					if (SharedData->PhysicsAsset->BodySetup[i]->BoneName == (*SelectedElem).Name)
					{
						FKAggregateGeom& AggGeom = SharedData->PhysicsAsset->BodySetup[i]->AggGeom;

						//select all primitives
						for(int32 j=0; j<AggGeom.BoxElems.Num(); ++j)
						{
							SharedData->HitBone(i, KPT_Box, j, true, false);
						}

						for(int32 j=0; j<AggGeom.SphereElems.Num(); ++j)
						{
							SharedData->HitBone(i, KPT_Sphere, j, true, false);
						}

						for(int32 j=0; j<AggGeom.SphylElems.Num(); ++j)
						{
							SharedData->HitBone(i, KPT_Sphyl, j, true, false);
						}

						for(int32 j=0; j<AggGeom.ConvexElems.Num(); ++j)
						{
							SharedData->HitBone(i, KPT_Convex, j, true, false);
						}

						break;
					}
				}
			}
			else
			{
				FPhATTreeInfo Info = *SelectedElem;
				if (Info.ParentBoneIdx != INDEX_NONE)
				{
					SharedData->HitBone(Info.BodyIdx, Info.CollisionType, Info.CollisionIdx, true, false);
				}
			}
		}
		else
		{
			if (ObjIndex != INDEX_NONE)
			{
				SharedData->HitConstraint(ObjIndex, true);
			}
		}
	}

	

	InsideSelChanged = false;
}

void FPhAT::OnTreeDoubleClick(FTreeElemPtr TreeElem)
{
	if(!TreeElem->bBold && TreeElem->BodyIdx == INDEX_NONE)	//if bone without body add new body
	{
		OnAddNewBody();
	}else if(TreeElem->bBold)
	{
		OnResetBoneCollision();
	}
}

TSharedPtr<SWidget> FPhAT::OnTreeRightClick()
{
	if(SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		if(SharedData->SelectedBodies.Num())	//if we have anything selected just give us context menu of it
		{
			return BuildMenuWidgetBody(true);
		}else
		{
			//otherwise check if we've selected a bone
			TArray<FTreeElemPtr> Elems = Hierarchy->GetSelectedItems();
			for(int32 i=0; i<Elems.Num(); ++i)
			{
				if(Elems[i]->BoneOrConstraintIdx != INDEX_NONE)
				{
					return BuildMenuWidgetBone();
				}
			}
		}
	}else
	{
		if(SharedData->SelectedConstraints.Num())	//if we have anything selected just give us context menu of it
		{
			return BuildMenuWidgetConstraint(true);
		}else
		{

		}
	}

	return NULL;
}

TSharedPtr<SWidget> FPhAT::BuildMenuWidgetBody(bool bHierarchy /*= false*/)
{
	if (!SharedData->GetSelectedBody())
	{
		return NULL;
	}

	const bool bShouldCloseWindowAfterMenuSelection = true;	// Set the menu to automatically close when the user commits to a choice
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, GetToolkitCommands());
	{
		const FPhATCommands& Commands = FPhATCommands::Get();

		struct FLocal
		{
			static void FillPhysicsTypeMenu(FMenuBuilder& InMenuBuilder, bool bInHierarchy)
			{
				const FPhATCommands& PhATCommands = FPhATCommands::Get();

				InMenuBuilder.BeginSection("BodyPhysicsTypeActions", LOCTEXT("BodyPhysicsTypeHeader", "Body Physics Type"));
				InMenuBuilder.AddMenuEntry(PhATCommands.MakeBodyKinematic);
				InMenuBuilder.AddMenuEntry(PhATCommands.MakeBodySimulated);
				InMenuBuilder.AddMenuEntry(PhATCommands.MakeBodyDefault);
				InMenuBuilder.EndSection();
				InMenuBuilder.EndSection();

				if (bInHierarchy)
				{
					InMenuBuilder.BeginSection("BodiesBelowPhysicsTypeActions", LOCTEXT("BodiesBelowPhysicsTypeHeader", "Bodies Below Physics Type"));
					InMenuBuilder.AddMenuEntry(PhATCommands.KinematicAllBodiesBelow);
					InMenuBuilder.AddMenuEntry(PhATCommands.SimulatedAllBodiesBelow);
					InMenuBuilder.AddMenuEntry(PhATCommands.MakeAllBodiesBelowDefault);
					InMenuBuilder.EndSection();
				}
			}
		};

		MenuBuilder.BeginSection( "BoneActions", LOCTEXT( "BoneHeader", "Bone" ) );
		MenuBuilder.AddMenuEntry( Commands.AddBox );
		MenuBuilder.AddMenuEntry( Commands.AddSphere );
		MenuBuilder.AddMenuEntry( Commands.AddSphyl );
		MenuBuilder.AddMenuEntry( Commands.RestetBoneCollision );
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection( "BodyActions", LOCTEXT( "BodyHeader", "Body" ) );
		MenuBuilder.AddSubMenu( LOCTEXT("BodyPhysicsTypeMenu", "Physics Type"), LOCTEXT("BodyPhysicsTypeMenu_ToolTip", "Physics Type"),
			FNewMenuDelegate::CreateStatic( &FLocal::FillPhysicsTypeMenu, bHierarchy ) );	

		MenuBuilder.AddMenuEntry( Commands.CopyProperties );
		MenuBuilder.AddMenuEntry( Commands.PasteProperties );
		MenuBuilder.AddMenuEntry( Commands.WeldToBody );
		MenuBuilder.AddMenuEntry( Commands.EnableCollision );
		MenuBuilder.AddMenuEntry( Commands.DisableCollision );
		MenuBuilder.AddMenuEntry( Commands.DeleteBody );
		if(bHierarchy)
		{
			MenuBuilder.AddMenuEntry( Commands.DeleteAllBodiesBelow );
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection( "PrimitiveActions", LOCTEXT( "PrimitiveHeader", "Primitive" ) );
		MenuBuilder.AddMenuEntry( Commands.DuplicatePrimitive );
		MenuBuilder.AddMenuEntry( Commands.DeletePrimitive );
		MenuBuilder.EndSection();

		if(SharedData->SelectedBodies.Num() > 1)	//different context menu if we have group selection
		{
			
		}else
		{
			
			SAssignNew(PickerComboButton, SComboButton)
				.ContentPadding(3)
				.OnGetMenuContent(this, &FPhAT::BuildStaticMeshAssetPicker)
				.ButtonContent()
				[
					SNew(STextBlock) .Text(LOCTEXT("AddCollisionfromStaticMesh ", "Copy Collision From StaticMesh"))
				];

			MenuBuilder.BeginSection( "Advanced", LOCTEXT( "AdvancedHeading", "Advanced" ) );
			MenuBuilder.AddWidget(PickerComboButton.ToSharedRef(), FText());
			MenuBuilder.EndSection();
		}
	}

	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> FPhAT::BuildMenuWidgetConstraint(bool bHierarchy /*= false*/)
{
	if (!SharedData->GetSelectedConstraint())
	{
		return NULL;
	}

	const bool bShouldCloseWindowAfterMenuSelection = true;	// Set the menu to automatically close when the user commits to a choice
	const FPhATCommands& Commands = FPhATCommands::Get();
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, GetToolkitCommands());
	{
		MenuBuilder.BeginSection( "MotorTypeActions", LOCTEXT("ConstraintMotorTypeHeader", "Motors" ) );
		MenuBuilder.AddMenuEntry(Commands.ToggleMotor);
		if(bHierarchy)
		{
			MenuBuilder.AddMenuEntry(Commands.EnableMotorsBelow);
			MenuBuilder.AddMenuEntry(Commands.DisableMotorsBelow);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection( "EditTypeActions", LOCTEXT("ConstraintEditTypeHeader", "Edit" ) );
		MenuBuilder.AddMenuEntry(Commands.CopyProperties);
		MenuBuilder.AddMenuEntry(Commands.PasteProperties);
		MenuBuilder.AddMenuEntry(Commands.ResetConstraint);
		MenuBuilder.AddMenuEntry(Commands.DeleteConstraint);
		MenuBuilder.EndSection();


	}

	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> FPhAT::BuildMenuWidgetBone()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;	// Set the menu to automatically close when the user commits to a choice
	const FPhATCommands& Commands = FPhATCommands::Get();
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, GetToolkitCommands());
	{
		MenuBuilder.BeginSection( "BodyTypeAction", LOCTEXT("BodyTypeHeader", "New Body" ) );
		MenuBuilder.AddMenuEntry( Commands.AddNewBody );
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

void FPhAT::AnimationSelectionChanged( UObject* Object )
{
	SelectedAnimation = Cast<UAnimationAsset>( Object );
	SharedData->EditorSkelComp->SetAnimation( SelectedAnimation );
}

UObject* FPhAT::GetSelectedAnimation() const
{
	return SelectedAnimation;
}

bool FPhAT::ShouldFilterAssetBasedOnSkeleton( const FAssetData& AssetData )
{
	// @TODO This is a duplicate of FPersona::ShouldFilterAssetBasedOnSkeleton(), but should go away once PhAT is integrated with Persona
	const FString* SkeletonName = AssetData.TagsAndValues.Find(TEXT("Skeleton"));

	if ( SkeletonName )
	{
		USkeleton* Skeleton = SharedData->EditorSkelMesh->Skeleton;

		if ( Skeleton && (*SkeletonName) == FString::Printf(TEXT("%s'%s'"), *Skeleton->GetClass()->GetName(), *Skeleton->GetPathName()) )
		{
			return false;
		}
	}

	return true;
}

void FPhAT::SnapConstraintToBone(int32 ConstraintIndex, const FTransform& ParentFrame)
{
	UPhysicsConstraintTemplate* ConstraintSetup = SharedData->PhysicsAsset->ConstraintSetup[ConstraintIndex];
	ConstraintSetup->Modify();

	// Get child bone transform
	int32 BoneIndex = SharedData->EditorSkelMesh->RefSkeleton.FindBoneIndex(ConstraintSetup->DefaultInstance.ConstraintBone1);
	check(BoneIndex != INDEX_NONE);

	FTransform BoneTM = SharedData->EditorSkelComp->GetBoneTransform(BoneIndex);
	FTransform RelTM = BoneTM.GetRelativeTransform(ParentFrame);

	FTransform Con1Matrix = ConstraintSetup->DefaultInstance.GetRefFrame(EConstraintFrame::Frame2);
	FTransform Con0Matrix = ConstraintSetup->DefaultInstance.GetRefFrame(EConstraintFrame::Frame1);

	ConstraintSetup->DefaultInstance.SetRefFrame(EConstraintFrame::Frame2, Con0Matrix * RelTM * Con1Matrix);
}

void FPhAT::CreateOrConvertConstraint(EPhATConstraintType ConstraintType)
{
	const FScopedTransaction Transaction( LOCTEXT( "CreateConvertConstraint", "Create Or Convert Constraint" ) );

	for(int32 i=0; i<SharedData->SelectedConstraints.Num(); ++i)
	{
		UPhysicsConstraintTemplate* ConstraintSetup = SharedData->PhysicsAsset->ConstraintSetup[SharedData->SelectedConstraints[i].Index];
		ConstraintSetup->Modify();

		if(ConstraintType == EPCT_BSJoint)
		{
			ConstraintSetup->DefaultInstance.ConfigureAsBS();
		}
		else if(ConstraintType == EPCT_Hinge)
		{
			ConstraintSetup->DefaultInstance.ConfigureAsHinge();
		}
		else if(ConstraintType == EPCT_Prismatic)
		{
			ConstraintSetup->DefaultInstance.ConfigureAsPrismatic();
		}
		else if(ConstraintType == EPCT_SkelJoint)
		{
			ConstraintSetup->DefaultInstance.ConfigureAsSkelJoint();
		}	
	}

	RefreshHierachyTree();
	RefreshPreviewViewport();
	
}

void FPhAT::SetConstraintsBelowSelectedMotorised(bool bMotorised)
{
	SharedData->PhysicsAsset->Modify();

	for(int32 i=0; i<SharedData->SelectedConstraints.Num(); ++i)
	{
		// Get the index of this constraint
		UPhysicsConstraintTemplate* BaseSetup = SharedData->PhysicsAsset->ConstraintSetup[SharedData->SelectedConstraints[i].Index];

		TArray<int32> BelowConstraints;
		int32 BaseIndex = SharedData->EditorSkelMesh->RefSkeleton.FindBoneIndex(BaseSetup->DefaultInstance.JointName);

		// Iterate over all other joints, looking for 'children' of this one
		for (int32 j = 0; j < SharedData->PhysicsAsset->ConstraintSetup.Num(); ++j)
		{
			UPhysicsConstraintTemplate* ConstraintSetup = SharedData->PhysicsAsset->ConstraintSetup[j];
			FName TestName = ConstraintSetup->DefaultInstance.JointName;
			int32 TestIndex = SharedData->EditorSkelMesh->RefSkeleton.FindBoneIndex(TestName);

			// We want to return this constraint as well.
			if (TestIndex == BaseIndex || SharedData->EditorSkelMesh->RefSkeleton.BoneIsChildOf(TestIndex, BaseIndex))
			{
				BelowConstraints.Add(j);
			}
		}

		for (int32 j = 0; j < BelowConstraints.Num(); ++j)
		{
			int32 ConIndex = BelowConstraints[j];
			FConstraintInstance* ConstraintInstance = &SharedData->PhysicsAsset->ConstraintSetup[ConIndex]->DefaultInstance;

			ConstraintInstance->bAngularOrientationDrive = bMotorised;
		}

	}
}

void FPhAT::AddNewPrimitive(EKCollisionPrimitiveType InPrimitiveType, bool bCopySelected)
{
	check(!bCopySelected || SharedData->SelectedBodies.Num() == 1);	//we only support this for one selection
	int32 NewPrimIndex = 0;
	TArray<FPhATSharedData::FSelection> NewSelection;
	{
		// Make sure rendering is done - so we are not changing data being used by collision drawing.
		FlushRenderingCommands();

		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "AddNewPrimitive", "Add New Primitive") );

		//first we need to grab all the bodies we're modifying (removes duplicates from multiple primitives)
		for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
		{
			
			NewSelection.AddUnique(FPhATSharedData::FSelection(SharedData->SelectedBodies[i].Index, KPT_Unknown, 0));	//only care about body index for now, we'll later update the primitive index
		}

		for(int32 i=0; i<NewSelection.Num(); ++i)
		{
			UBodySetup* BodySetup = SharedData->PhysicsAsset->BodySetup[NewSelection[i].Index];
			EKCollisionPrimitiveType PrimitiveType;
			if (bCopySelected)
			{
				PrimitiveType = SharedData->GetSelectedBody()->PrimitiveType;
			}
			else
			{
				PrimitiveType = InPrimitiveType;
			}


			BodySetup->Modify();

			if (PrimitiveType == KPT_Sphere)
			{
				NewPrimIndex = BodySetup->AggGeom.SphereElems.AddZeroed();
				NewSelection[i].PrimitiveType = KPT_Sphere;
				NewSelection[i].PrimitiveIndex = NewPrimIndex;
				FKSphereElem* SphereElem = &BodySetup->AggGeom.SphereElems[NewPrimIndex];

				if (!bCopySelected)
				{
					SphereElem->Center = FVector::ZeroVector;

					SphereElem->Radius = PhAT::DefaultPrimSize;
				}
				else
				{
					SphereElem->Center = BodySetup->AggGeom.SphereElems[SharedData->GetSelectedBody()->PrimitiveIndex].Center;
					SphereElem->Center.X += PhAT::DuplicateXOffset;

					SphereElem->Radius = BodySetup->AggGeom.SphereElems[SharedData->GetSelectedBody()->PrimitiveIndex].Radius;
				}
			}
			else if (PrimitiveType == KPT_Box)
			{
				NewPrimIndex = BodySetup->AggGeom.BoxElems.AddZeroed();
				NewSelection[i].PrimitiveType = KPT_Box;
				NewSelection[i].PrimitiveIndex = NewPrimIndex;
				FKBoxElem* BoxElem = &BodySetup->AggGeom.BoxElems[NewPrimIndex];

				if (!bCopySelected)
				{
					BoxElem->SetTransform( FTransform::Identity );

					BoxElem->X = 0.5f * PhAT::DefaultPrimSize;
					BoxElem->Y = 0.5f * PhAT::DefaultPrimSize;
					BoxElem->Z = 0.5f * PhAT::DefaultPrimSize;
				}
				else
				{
					BoxElem->SetTransform( BodySetup->AggGeom.BoxElems[SharedData->GetSelectedBody()->PrimitiveIndex].GetTransform() );
					BoxElem->Center.X += PhAT::DuplicateXOffset;

					BoxElem->X = BodySetup->AggGeom.BoxElems[SharedData->GetSelectedBody()->PrimitiveIndex].X;
					BoxElem->Y = BodySetup->AggGeom.BoxElems[SharedData->GetSelectedBody()->PrimitiveIndex].Y;
					BoxElem->Z = BodySetup->AggGeom.BoxElems[SharedData->GetSelectedBody()->PrimitiveIndex].Z;
				}
			}
			else if (PrimitiveType == KPT_Sphyl)
			{
				NewPrimIndex = BodySetup->AggGeom.SphylElems.AddZeroed();
				NewSelection[i].PrimitiveType = KPT_Sphyl;
				NewSelection[i].PrimitiveIndex = NewPrimIndex;
				FKSphylElem* SphylElem = &BodySetup->AggGeom.SphylElems[NewPrimIndex];

				if (!bCopySelected)
				{
					SphylElem->SetTransform( FTransform::Identity );

					SphylElem->Length = PhAT::DefaultPrimSize;
					SphylElem->Radius = PhAT::DefaultPrimSize;
				}
				else
				{
					SphylElem->SetTransform( BodySetup->AggGeom.SphylElems[SharedData->GetSelectedBody()->PrimitiveIndex].GetTransform() );
					SphylElem->Center.X += PhAT::DuplicateXOffset;

					SphylElem->Length = BodySetup->AggGeom.SphylElems[SharedData->GetSelectedBody()->PrimitiveIndex].Length;
					SphylElem->Radius = BodySetup->AggGeom.SphylElems[SharedData->GetSelectedBody()->PrimitiveIndex].Radius;
				}
			}
		}
	} // ScopedTransaction

	//clear selection
	SharedData->SetSelectedBody(NULL);
	for(int32 i=0; i<NewSelection.Num(); ++i)
	{
		SharedData->SetSelectedBody(&NewSelection[i], true);
	}

	RefreshHierachyTree();
	RefreshPreviewViewport();
}

void FPhAT::SetBodiesBelowSelectedPhysicsType( EPhysicsType InPhysicsType)
{
	TArray<int32> Indices;
	for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
	{
		Indices.Add(SharedData->SelectedBodies[i].Index);
	}

	SetBodiesBelowPhysicsType(InPhysicsType, Indices);
}

void FPhAT::SetBodiesBelowPhysicsType( EPhysicsType InPhysicsType, const TArray<int32> & Indices)
{
	TArray<int32> BelowBodies;
	
	for(int32 i=0; i<Indices.Num(); ++i)
	{
		// Get the index of this body
		UBodySetup* BaseSetup = SharedData->PhysicsAsset->BodySetup[Indices[i]];
		SharedData->PhysicsAsset->GetBodyIndicesBelow(BelowBodies, BaseSetup->BoneName, SharedData->EditorSkelMesh);
	}

	for (int32 i = 0; i < BelowBodies.Num(); ++i)
	{
		int32 BodyIndex = BelowBodies[i];
		UBodySetup* BodySetup = SharedData->PhysicsAsset->BodySetup[BodyIndex];
		BodySetup->Modify();

		BodySetup->PhysicsType = InPhysicsType;
	}
}

bool FPhAT::IsNotSimulation() const
{
	return !SharedData->bRunningSimulation;
}

bool FPhAT::IsEditBodyMode() const
{
	return IsNotSimulation() && (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit);
}

bool FPhAT::IsSelectedEditBodyMode() const
{
	return IsEditBodyMode() && (SharedData->GetSelectedBody());
}

bool FPhAT::IsEditConstraintMode() const
{
	return IsNotSimulation() && (SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit);
}

bool FPhAT::IsSelectedEditConstraintMode() const
{
	return IsEditConstraintMode() && (SharedData->GetSelectedConstraint());
}

bool FPhAT::IsSelectedEditMode() const
{
	return IsSelectedEditBodyMode() || IsSelectedEditConstraintMode();
}

void FPhAT::OnChangeDefaultMesh()
{
	// Get the currently selected SkeletalMesh. Fail if there ain't one.
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	USkeletalMesh* NewSkelMesh = GEditor->GetSelectedObjects()->GetTop<USkeletalMesh>();
	if (!NewSkelMesh)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoSkelMeshSelected", "No SkeletalMesh Selected.\nSelect the SkeletalMesh in the Content Browser that you want to use as the new Default SkeletalMesh for this PhysicsAsset."));
		return;
	}

	// Confirm they want to do this.
	bool bDoChange = EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo,
		FText::Format(NSLOCTEXT("UnrealEd", "SureChangeAssetSkelMesh", "Are you sure you want to change the PhysicsAsset '{0}' to use the SkeletalMesh '{1}'?"),
			FText::FromString(SharedData->PhysicsAsset->GetName()), FText::FromString(NewSkelMesh->GetName()) ));
	if (bDoChange)
	{
		// See if any bones are missing from the skeletal mesh we are trying to use
		// @todo Could do more here - check for bone lengths etc. Maybe modify asset?
		for (int32 i = 0; i <SharedData->PhysicsAsset->BodySetup.Num(); ++i)
		{
			FName BodyName = SharedData->PhysicsAsset->BodySetup[i]->BoneName;
			int32 BoneIndex = NewSkelMesh->RefSkeleton.FindBoneIndex(BodyName);
			if (BoneIndex == INDEX_NONE)
			{
				FMessageDialog::Open(EAppMsgType::Ok,
					FText::Format( NSLOCTEXT("UnrealEd", "BoneMissingFromSkelMesh", "The SkeletalMesh is missing bone '{0}' needed by this PhysicsAsset."), FText::FromName(BodyName) ));
				return;
			}
		}

		// We have all the bones - go ahead and make the change.
		SharedData->PhysicsAsset->PreviewSkeletalMesh = NewSkelMesh;

		// Change preview
		SharedData->EditorSkelMesh = NewSkelMesh;
		SharedData->EditorSkelComp->SetSkeletalMesh(NewSkelMesh);

		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
		// Update various infos based on the mesh
		MeshUtilities.CalcBoneVertInfos(SharedData->EditorSkelMesh, SharedData->DominantWeightBoneInfos, true);
		MeshUtilities.CalcBoneVertInfos(SharedData->EditorSkelMesh, SharedData->AnyWeightBoneInfos, false);
		RefreshHierachyTree();

		// Mark asset's package as dirty as its changed.
		SharedData->PhysicsAsset->MarkPackageDirty();
	}
}

void FPhAT::OnResetEntireAsset()
{
	bool bDoRecalc = EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "Prompt_12", "This will completely replace the current asset.\nAre you sure?"));
	if (!bDoRecalc)
	{
		return;
	}

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	// Then calculate a new one.

	SharedData->OpenNewBodyDlg();

	if (SharedData->NewBodyResponse != EAppReturnType::Cancel)
	{
		// Deselect everything.
		SharedData->SetSelectedBody(NULL);
		SharedData->SetSelectedConstraint(INDEX_NONE);	

		// Empty current asset data.
		SharedData->PhysicsAsset->BodySetup.Empty();
		SharedData->PhysicsAsset->BodySetupIndexMap.Empty();
		SharedData->PhysicsAsset->ConstraintSetup.Empty();

		FText ErrorMessage;
		if (FPhysicsAssetUtils::CreateFromSkeletalMesh(SharedData->PhysicsAsset, SharedData->EditorSkelMesh, SharedData->NewBodyData, ErrorMessage) == false)
		{
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		}

		SharedData->RefreshPhysicsAssetChange(SharedData->PhysicsAsset);

		RefreshHierachyTree();
		RefreshPreviewViewport();
	}
}

void FPhAT::OnResetBoneCollision()
{
	bool bDoRecalc = EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "Prompt_13", "This will completely replace the current bone collision.\nAre you sure?"));
	if (!bDoRecalc)
	{
		return;
	}

	SharedData->OpenNewBodyDlg();

	if (SharedData->NewBodyResponse == EAppReturnType::Cancel)
	{
		return;
	}

	{
		TArray<int32> BodyIndices;
		const FScopedTransaction Transaction( NSLOCTEXT("PhAT", "ResetBoneCollision", "Reset Bone Collision") );
		for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
		{
			UBodySetup* BodySetup = SharedData->PhysicsAsset->BodySetup[SharedData->SelectedBodies[i].Index];
			check(BodySetup);
			BodySetup->Modify();

			int32 BoneIndex = SharedData->EditorSkelMesh->RefSkeleton.FindBoneIndex(BodySetup->BoneName);
			check(BoneIndex != INDEX_NONE);

			FPhysicsAssetUtils::CreateCollisionFromBone(BodySetup, SharedData->EditorSkelMesh, BoneIndex, SharedData->NewBodyData, (SharedData->NewBodyData.VertWeight == EVW_DominantWeight)? SharedData->DominantWeightBoneInfos: SharedData->AnyWeightBoneInfos);
			BodyIndices.AddUnique(SharedData->SelectedBodies[i].Index);
		}

		//deselect first
		SharedData->SetSelectedBody(NULL);
		for(int32 i=0; i<BodyIndices.Num(); ++i)
		{
			SharedData->SetSelectedBodyAnyPrim(BodyIndices[i], true);
		}
	} // scoped transaction

	SharedData->RefreshPhysicsAssetChange(SharedData->PhysicsAsset);	
	RefreshPreviewViewport();
}

void FPhAT::OnApplyPhysicalMaterial()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
	UPhysicalMaterial* SelectedPhysMaterial = GEditor->GetSelectedObjects()->GetTop<UPhysicalMaterial>();

	if (SelectedPhysMaterial)
	{
		for (int32 BodyIdx=0; BodyIdx<SharedData->PhysicsAsset->BodySetup.Num(); BodyIdx++)
		{
			UBodySetup* BodySetup = SharedData->PhysicsAsset->BodySetup[BodyIdx];
			BodySetup->Modify();
			BodySetup->PhysMaterial = SelectedPhysMaterial;
		}
	}
}

void FPhAT::OnEditingMode(int32 Mode)
{
	if (Mode == FPhATSharedData::PEM_BodyEdit)
	{
		SharedData->EditingMode = FPhATSharedData::PEM_BodyEdit;
		RefreshHierachyTree();
		SharedData->SetSelectedBody(NULL, true); // Forces properties panel to update...
	}
	else
	{
		SharedData->EditingMode = FPhATSharedData::PEM_ConstraintEdit;
		RefreshHierachyTree();
		SharedData->SetSelectedConstraint(INDEX_NONE, true);

		// Scale isn't valid for constraints!
		if (SharedData->WidgetMode == FWidget::WM_Scale)
		{
			SharedData->WidgetMode = FWidget::WM_Translate;
		}
	}
	
	RefreshPreviewViewport();

	// Rebuild the toolbar, as the icons shown will have changed
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

bool FPhAT::IsEditingMode(int32 Mode) const
{
	return (FPhATSharedData::EPhATEditingMode)Mode == SharedData->EditingMode;
}

void FPhAT::OnMovementSpace(int32 Mode)
{
	SharedData->MovementSpace = (ECoordSystem)Mode;

	RefreshPreviewViewport();
}

bool FPhAT::IsMovementSpace(int32 Mode) const
{
	return (ECoordSystem)Mode == SharedData->MovementSpace;
}

void FPhAT::OnTransformMode(int32 Mode)
{
	SharedData->WidgetMode = (FWidget::EWidgetMode)Mode;

	RefreshPreviewViewport();
}

bool FPhAT::IsTransformMode(int32 Mode) const
{
	return SharedData->WidgetMode == (FWidget::EWidgetMode)Mode;
}

void FPhAT::OnCopyProperties()
{
	if(SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		SharedData->CopyBody();
	}else
	{
		SharedData->CopyConstraint();
	}
	
	RefreshPreviewViewport();
}

void FPhAT::OnPasteProperties()
{
	if(SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		SharedData->PasteBodyProperties();
	}else
	{
		SharedData->PasteConstraintProperties();
	}
	
}

bool FPhAT::CanCopyProperties() const
{
	if(IsSelectedEditMode())
	{
		if(SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit && SharedData->SelectedBodies.Num() == 1)
		{
			return true;
		}else if(SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit && SharedData->SelectedConstraints.Num() == 1)
		{
			return true;
		}
	}

	return false;
}

bool FPhAT::CanPasteProperties() const
{
	return IsSelectedEditMode() && IsCopyProperties();
}

bool FPhAT::IsCopyProperties() const
{
	return (SharedData->CopiedBodySetup && SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit) || (SharedData->CopiedConstraintTemplate && SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit);
}

void FPhAT::OnInstanceProperties()
{
	SharedData->ToggleInstanceProperties();
}

bool FPhAT::IsInstanceProperties() const
{
	return SharedData->bShowInstanceProps;
}

//We need to save and restore physics states based on the mode we use to simulate
void FPhAT::FixPhysicsState()
{
	UPhysicsAsset * PhysicsAsset = SharedData->PhysicsAsset;
	TArray<UBodySetup*> & BodySetup = PhysicsAsset->BodySetup;

	if(!SharedData->bRunningSimulation)
	{
		PhysicsTypeState.Reset();
		for(int32 i=0; i<SharedData->PhysicsAsset->BodySetup.Num(); ++i)
		{
			PhysicsTypeState.Add(BodySetup[i]->PhysicsType);
		}
	}else
	{
		for(int32 i=0; i<PhysicsTypeState.Num(); ++i)
		{
			BodySetup[i]->Modify();
			BodySetup[i]->PhysicsType = PhysicsTypeState[i];
		}
	}
}

void FPhAT::ImpToggleSimulation()
{
	SharedData->ToggleSimulation();

	if (!PreviewViewport->GetViewportClient()->IsRealtime() && !IsPIERunning())
	{
		PreviewViewport->GetViewportClient()->SetRealtime(true);
	}
}

void FPhAT::OnSetSimulationMode(EPhATSimulationMode Mode)
{
	SimulationMode = Mode;
	OnToggleSimulation();
}

bool FPhAT::IsSimulationMode(EPhATSimulationMode Mode) const
{
	return SimulationMode == Mode;
}

void FPhAT::OnToggleSimulation()
{
	FixPhysicsState();
	if (IsSelectedSimulation())
	{
		OnSelectedSimulation();
	}
	SharedData->bNoGravitySimulation = IsSimulationMode(EPSM_Gravity);
	ImpToggleSimulation();
}

bool FPhAT::IsSelectedSimulation()
{
	return SelectedSimulation;
}

void FPhAT::OnToggleSelectedSimulation()
{
	SelectedSimulation = !SelectedSimulation;
}

void FPhAT::OnSelectedSimulation()
{

	//Before starting we modify the PhysicsType so that selected are unfixed and the rest are fixed
	if(SharedData->bRunningSimulation == false)
	{

		UPhysicsAsset * PhysicsAsset = SharedData->PhysicsAsset;
		TArray<UBodySetup*> & BodySetup = PhysicsAsset->BodySetup;

		//first we fix all the bodies
		for(int32 i=0; i<SharedData->PhysicsAsset->BodySetup.Num(); ++i)
		{
			BodySetup[i]->Modify();
			BodySetup[i]->PhysicsType = PhysType_Kinematic;
		}

		//Find which bodes have been selected
		if(SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
		{
			//Bodies already have a function that does this
			SetBodiesBelowSelectedPhysicsType(PhysType_Simulated);
		}else
		{
			//constraints need some more work
			TArray<int32> BodyIndices;
			TArray<UPhysicsConstraintTemplate*> & ConstraintSetup = PhysicsAsset->ConstraintSetup;
			for(int32 i=0; i<SharedData->SelectedConstraints.Num(); ++i)
			{
				int32 ConstraintIndex = SharedData->SelectedConstraints[i].Index;
				FName ConstraintBone1 = ConstraintSetup[ConstraintIndex]->DefaultInstance.ConstraintBone1;	//we only unfix the child bodies

				for(int32 j=0; j<BodySetup.Num(); ++j)
				{
					if(BodySetup[j]->BoneName == ConstraintBone1)
					{
						BodyIndices.Add(j);
					}
				}
			}

			SetBodiesBelowPhysicsType(PhysType_Simulated, BodyIndices);
		}
	}

}


bool FPhAT::IsToggleSimulation() const
{
	return SharedData->bRunningSimulation;
}

void FPhAT::OnMeshRenderingMode(FPhATSharedData::EPhATRenderMode Mode)
{
	if (SharedData->bRunningSimulation)
	{
		SharedData->Sim_MeshViewMode = Mode;
	}
	else if (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		SharedData->BodyEdit_MeshViewMode = Mode;
	}
	else
	{
		SharedData->ConstraintEdit_MeshViewMode = Mode;
	}

	RefreshPreviewViewport();
}

bool FPhAT::IsMeshRenderingMode(FPhATSharedData::EPhATRenderMode Mode) const
{
	return Mode == SharedData->GetCurrentMeshViewMode();
}

void FPhAT::OnCollisionRenderingMode(FPhATSharedData::EPhATRenderMode Mode)
{
	if (SharedData->bRunningSimulation)
	{
		SharedData->Sim_CollisionViewMode = Mode;
	}
	else if (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		SharedData->BodyEdit_CollisionViewMode = Mode;
	}
	else
	{
		SharedData->ConstraintEdit_CollisionViewMode = Mode;
	}

	RefreshPreviewViewport();
}

bool FPhAT::IsCollisionRenderingMode(FPhATSharedData::EPhATRenderMode Mode) const
{
	return Mode == SharedData->GetCurrentCollisionViewMode();
}

void FPhAT::OnConstraintRenderingMode(FPhATSharedData::EPhATConstraintViewMode Mode)
{
	if (SharedData->bRunningSimulation)
	{
		SharedData->Sim_ConstraintViewMode = Mode;
	}
	else if (SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		SharedData->BodyEdit_ConstraintViewMode = Mode;
	}
	else
	{
		SharedData->ConstraintEdit_ConstraintViewMode = Mode;
	}

	RefreshPreviewViewport();
}

bool FPhAT::IsConstraintRenderingMode(FPhATSharedData::EPhATConstraintViewMode Mode) const
{
	return Mode == SharedData->GetCurrentConstraintViewMode();
}

void FPhAT::OnShowFixedBodies()
{
	SharedData->bShowFixedStatus = !SharedData->bShowFixedStatus;

	RefreshPreviewViewport();
}

bool FPhAT::IsShowFixedBodies() const
{
	return SharedData->bShowFixedStatus;
}

void FPhAT::OnDrawGroundBox()
{
	SharedData->bDrawGround = !SharedData->bDrawGround;

	RefreshPreviewViewport();
}

bool FPhAT::IsDrawGroundBox() const
{
	return SharedData->bDrawGround;
}

void FPhAT::OnToggleGraphicsHierarchy()
{
	SharedData->bShowHierarchy = !SharedData->bShowHierarchy;

	RefreshPreviewViewport();
}

bool FPhAT::IsToggleGraphicsHierarchy() const
{
	return SharedData->bShowHierarchy;
}

void FPhAT::OnToggleBoneInfluences()
{
	SharedData->bShowInfluences = !SharedData->bShowInfluences;

	RefreshPreviewViewport();
}

bool FPhAT::IsToggleBoneInfluences() const
{
	return SharedData->bShowInfluences;
}

void FPhAT::OnToggleMassProperties()
{
	SharedData->bShowCOM = !SharedData->bShowCOM;

	RefreshPreviewViewport();
}

bool FPhAT::IsToggleMassProperties() const
{
	return SharedData->bShowCOM;
}

void FPhAT::OnSetCollision(bool bEnable)
{
	SharedData->SetCollisionBetweenSelected(bEnable);
}

bool FPhAT::CanSetCollision() const
{
	if(IsSelectedEditBodyMode())
	{
		if(SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit && SharedData->SelectedBodies.Num() > 1)
		{
			return true;
		}else if(SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit && SharedData->SelectedConstraints.Num() > 1)
		{
			return true;
		}
	}

	return false;
}


void FPhAT::OnWeldToBody()
{
	SharedData->WeldSelectedBodies();
}

bool FPhAT::CanWeldToBody()
{
	return IsSelectedEditBodyMode() && SharedData->WeldSelectedBodies(false);
}

void FPhAT::OnAddNewBody()
{
	TArray<FTreeElemPtr> Elems = Hierarchy->GetSelectedItems();

	if(Elems.Num())
	{
		SharedData->OpenNewBodyDlg();

		if (SharedData->NewBodyResponse == EAppReturnType::Cancel)
		{
			return;
		}

		const FScopedTransaction Transaction( NSLOCTEXT("PhAT", "AddNewPrimitive", "Add New Body") );

		// Make sure rendering is done - so we are not changing data being used by collision drawing.
		FlushRenderingCommands();

		for(int32 i=0; i<Elems.Num(); ++i)
		{
			int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(Elems[i]->Name);
			if (BoneIndex != INDEX_NONE)
			{
				SharedData->MakeNewBody(BoneIndex);
			}
		}

		RefreshPreviewViewport();
		RefreshHierachyTree();
	}
	
}

void FPhAT::OnAddSphere()
{
	AddNewPrimitive(KPT_Sphere);
}

void FPhAT::OnAddSphyl()
{
	AddNewPrimitive(KPT_Sphyl);
}

void FPhAT::OnAddBox()
{
	AddNewPrimitive(KPT_Box);
}

bool FPhAT::CanAddPrimitive() const
{
	return IsEditBodyMode();
}

void FPhAT::OnDeletePrimitive()
{
	SharedData->DeleteCurrentPrim();
}

void FPhAT::OnDuplicatePrimitive()
{
	AddNewPrimitive(KPT_Unknown, true);
}

bool FPhAT::CanDuplicatePrimitive() const
{
	return IsSelectedEditBodyMode() && SharedData->SelectedBodies.Num() == 1;
}

void FPhAT::OnResetConstraint()
{
	SharedData->SetSelectedConstraintRelTM(FTransform::Identity);
	RefreshPreviewViewport();
}

void FPhAT::OnSnapConstraint()
{
	const FScopedTransaction Transaction( LOCTEXT( "SnapConstraints", "Snap Constraints" ) );

	for(int32 i=0; i<SharedData->SelectedConstraints.Num(); ++i)
	{
		FTransform ParentFrame = SharedData->GetConstraintWorldTM(&SharedData->SelectedConstraints[i], EConstraintFrame::Frame2);
		SnapConstraintToBone(SharedData->SelectedConstraints[i].Index, ParentFrame);
	}
	
	RefreshPreviewViewport();
}

void FPhAT::OnConvertToBallAndSocket()
{
	CreateOrConvertConstraint(EPCT_BSJoint);
}

void FPhAT::OnConvertToHinge()
{
	CreateOrConvertConstraint(EPCT_Hinge);
}

void FPhAT::OnConvertToPrismatic()
{
	CreateOrConvertConstraint(EPCT_Prismatic);
}

void FPhAT::OnConvertToSkeletal()
{
	CreateOrConvertConstraint(EPCT_SkelJoint);
}

void FPhAT::OnDeleteConstraint()
{
	SharedData->DeleteCurrentConstraint();
}

void FPhAT::OnPlayAnimation()
{
	if (!SharedData->EditorSkelComp->IsPlaying() && SharedData->bRunningSimulation)
	{
		SharedData->EditorSkelComp->SetAnimation(SelectedAnimation);
		SharedData->EditorSkelComp->Play(true);
	}
	else
	{
		SharedData->EditorSkelComp->Stop();
	}
}

bool FPhAT::IsPlayAnimation() const
{
	return SharedData->EditorSkelComp->IsPlaying();
}

void FPhAT::OnShowSkeleton()
{
	SharedData->bShowAnimSkel = !SharedData->bShowAnimSkel;
	
	RefreshPreviewViewport();
}

bool FPhAT::IsShowSkeleton() const
{
	return SharedData->bShowAnimSkel;
}

void FPhAT::OnSetBodyPhysicsType( EPhysicsType InPhysicsType )
{
	if (SharedData->GetSelectedBody())
	{
		
		for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
		{
			UBodySetup* BodySetup = SharedData->PhysicsAsset->BodySetup[SharedData->SelectedBodies[i].Index];
			BodySetup->Modify();
			BodySetup->PhysicsType = InPhysicsType;
		}

	}
}

bool FPhAT::IsBodyPhysicsType( EPhysicsType InPhysicsType )
{
	for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
	{
		UBodySetup* BodySetup = SharedData->PhysicsAsset->BodySetup[SharedData->SelectedBodies[i].Index];
		if(BodySetup->PhysicsType == InPhysicsType)
		{
			return true;
		}

	}
	
	return false;
}

void FPhAT::OnDeleteBody()
{
	if(SharedData->SelectedBodies.Num())
	{
		//first build the bodysetup array because deleting bodies modifies the selected array
		TArray<UBodySetup*> BodySetups;
		BodySetups.Reserve(SharedData->SelectedBodies.Num());

		for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
		{
			BodySetups.Add( SharedData->PhysicsAsset->BodySetup[SharedData->SelectedBodies[i].Index] );
		}

		const FScopedTransaction Transaction( LOCTEXT( "DeleteBodies", "Delete Bodies" ) );

		for(int32 i=0; i<BodySetups.Num(); ++i)
		{
			int32 BodyIndex = SharedData->PhysicsAsset->FindBodyIndex(BodySetups[i]->BoneName);
			if(BodyIndex != INDEX_NONE)
			{
				// Use PhAT function to delete action (so undo works etc)
				SharedData->DeleteBody(BodyIndex, false);
			}
		}

		SharedData->RefreshPhysicsAssetChange(SharedData->PhysicsAsset);
	}
}

void FPhAT::OnDeleteAllBodiesBelow()
{
	TArray<UBodySetup*> BodySetups;

	for (FPhATSharedData::FSelection SelectedBody : SharedData->SelectedBodies)
	{
		UBodySetup* BaseSetup = SharedData->PhysicsAsset->BodySetup[SelectedBody.Index];
		
		// Build a list of BodySetups below this one
		TArray<int32> BelowBodies;
		SharedData->PhysicsAsset->GetBodyIndicesBelow(BelowBodies, BaseSetup->BoneName, SharedData->EditorSkelMesh);

		for (const int32 BodyIndex : BelowBodies)
		{
			UBodySetup* BodySetup = SharedData->PhysicsAsset->BodySetup[BodyIndex];
			BodySetups.Add(BodySetup);
		}
	}

	if(BodySetups.Num())
	{
		const FScopedTransaction Transaction( LOCTEXT( "DeleteBodiesBelow", "Delete Bodies Below" ) );

		// Now remove each one
		for (UBodySetup* BodySetup : BodySetups)
		{
			// Use PhAT function to delete action (so undo works etc)
			int32 Index = SharedData->PhysicsAsset->FindBodyIndex(BodySetup->BoneName);
			if(Index != INDEX_NONE)
			{
				SharedData->DeleteBody(Index, false);
			}
		}

		SharedData->RefreshPhysicsAssetChange(SharedData->PhysicsAsset);
	}
	
}

void FPhAT::OnToggleMotor()
{
	for(int32 i=0; i<SharedData->SelectedConstraints.Num(); ++i)
	{
		UPhysicsConstraintTemplate* ConSetup = SharedData->PhysicsAsset->ConstraintSetup[SharedData->SelectedConstraints[i].Index];
		FConstraintInstance* CI = &ConSetup->DefaultInstance;

		CI->bAngularOrientationDrive = !CI->bAngularOrientationDrive;
	}
}

void FPhAT::OnEnableMotorsBelow()
{
	SetConstraintsBelowSelectedMotorised(true);
}

void FPhAT::OnDisableMotorsBelow()
{
	SetConstraintsBelowSelectedMotorised(false);
}

void FPhAT::OnLockSelection()
{
	SharedData->bSelectionLock = !SharedData->bSelectionLock;
}

void FPhAT::OnDeleteSelection()
{
	switch(SharedData->EditingMode)
	{
	case FPhATSharedData::PEM_BodyEdit:
		{
			SharedData->DeleteCurrentPrim();
		}
		break;
	case FPhATSharedData::PEM_ConstraintEdit:
		{
			SharedData->DeleteCurrentConstraint();
		}
		break;
	/** Add new editor modes here */
	default:
		break;
	}
}

void FPhAT::OnCycleConstraintOrientation()
{
	if(SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit && SharedData->GetSelectedConstraint())
	{
		SharedData->CycleCurrentConstraintOrientation();
	}
}

void FPhAT::OnCycleConstraintActive()
{
	if(SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit && SharedData->GetSelectedConstraint())
	{
		SharedData->CycleCurrentConstraintActive();
	}
}

void FPhAT::OnToggleSwing1()
{
	if(SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit && SharedData->GetSelectedConstraint())
	{
		SharedData->ToggleConstraint(FPhATSharedData::PCT_Swing1);
	}
}

void FPhAT::OnToggleSwing2()
{
	if(SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit && SharedData->GetSelectedConstraint())
	{
		SharedData->ToggleConstraint(FPhATSharedData::PCT_Swing2);
	}
}

void FPhAT::OnToggleTwist()
{
	if(SharedData->EditingMode == FPhATSharedData::PEM_ConstraintEdit && SharedData->GetSelectedConstraint())
	{
		SharedData->ToggleConstraint(FPhATSharedData::PCT_Twist);
	}
}

void FPhAT::OnFocusSelection()
{
	switch(SharedData->EditingMode)
	{
	case FPhATSharedData::PEM_BodyEdit:
		{
			if(SharedData->GetSelectedBody())
			{
				int32 BoneIdx = SharedData->EditorSkelComp->GetBoneIndex(SharedData->PhysicsAsset->BodySetup[SharedData->GetSelectedBody()->Index]->BoneName);
				FMatrix BoneTransform = SharedData->EditorSkelComp->GetBoneMatrix(BoneIdx);
				FBoxSphereBounds Bounds(BoneTransform.GetOrigin(), FVector(20), 20);

				PreviewViewport->GetViewportClient()->FocusViewportOnBox(Bounds.GetBox());
			}
		}
		break;
	case FPhATSharedData::PEM_ConstraintEdit:
		{
			if(SharedData->GetSelectedConstraint())
			{
				FTransform ConstraintTransform = SharedData->GetConstraintMatrix(SharedData->GetSelectedConstraint()->Index, EConstraintFrame::Frame2, 1.0f);
				FBoxSphereBounds Bounds(ConstraintTransform.GetTranslation(), FVector(20), 20);

				PreviewViewport->GetViewportClient()->FocusViewportOnBox(Bounds.GetBox());
			}
		}
		break;
		/** Add any new edit modes here */
	default:
		break;
	}
}

TSharedRef<SWidget> FPhAT::BuildStaticMeshAssetPicker()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(UStaticMesh::StaticClass()->GetFName());
	AssetPickerConfig.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateSP(this, &FPhAT::OnAssetSelectedFromStaticMeshAssetPicker);
	AssetPickerConfig.bAllowNullSelection = true;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.ThumbnailScale = 0.0f;
	AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
	AssetPickerConfig.bShowBottomToolbar = false;
	AssetPickerConfig.SelectionMode = ESelectionMode::Single;

	return SNew(SBox)
		.WidthOverride(384)
		.HeightOverride(768)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];
}

TSharedRef<SWidget> FPhAT::BuildHierarchyFilterMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;	// Set the menu to automatically close when the user commits to a choice
	const FPhATCommands& Commands = FPhATCommands::Get();
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, GetToolkitCommands());
	{
		MenuBuilder.AddMenuEntry(Commands.HierarchyFilterAll);
		MenuBuilder.AddMenuEntry(Commands.HierarchyFilterBodies);
	}

	return MenuBuilder.MakeWidget();
}

FString FPhAT::GetHierarchyFilter() const
{

	FText FilterMenuText;

	switch (HierarchyFilterMode)
	{
	case PHFM_All:
		FilterMenuText = FPhATCommands::Get().HierarchyFilterAll->GetLabel();
		break;
	case PHFM_Bodies:
		FilterMenuText = FPhATCommands::Get().HierarchyFilterBodies->GetLabel();
		break;
	default:
		// Unknown mode
		check(0);
		break;
	}

	return FilterMenuText.ToString();

}

void FPhAT::OnAssetSelectedFromStaticMeshAssetPicker( const FAssetData& AssetData )
{
	PickerComboButton->SetIsOpen(false);
	
	const FScopedTransaction Transaction( NSLOCTEXT("PhAT", "Import Convex", "Import Convex") );
	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	if (SharedData->GetSelectedBody())
	{
		SharedData->PhysicsAsset->Modify();

		// Build a list of BodySetups below this one
		UBodySetup* BaseSetup = SharedData->PhysicsAsset->BodySetup[SharedData->GetSelectedBody()->Index];
		BaseSetup->Modify();
	
		UStaticMesh* SM = Cast<UStaticMesh>(AssetData.GetAsset());

		if (SM && SM->BodySetup && SM->BodySetup->AggGeom.ConvexElems.Num() > 0)
		{
			BaseSetup->AddCollisionFrom(SM->BodySetup);

			BaseSetup->InvalidatePhysicsData();
			BaseSetup->CreatePhysicsMeshes();
			SharedData->RefreshPhysicsAssetChange(SharedData->PhysicsAsset);

			RefreshHierachyTree();
		}
		else
		{
			UE_LOG(LogPhysics, Warning, TEXT("Failed to import body from static mesh %s. Mesh probably has no collision setup."), *AssetData.AssetName.ToString());
		}
	}
}

bool FPhAT::CanStartSimulation() const
{
	return !IsPIERunning();
}

bool FPhAT::IsPIERunning()
{
	TArray<FWorldContext> WorldContexts = GEngine->GetWorldContexts();

	for (int32 i=0; i < WorldContexts.Num(); ++i)
	{
		if (WorldContexts[i].World()->IsPlayInEditor())
		{
			return true;
		}
	}
	
	return false;
}

void FPhAT::SetHierarchyFilter(EPhatHierarchyFilterMode Mode)
{
	HierarchyFilterMode = Mode;
	RefreshHierachyTree();
	RefreshHierachyTreeSelection();
}

void FPhAT::OnSelectAll()
{
	UPhysicsAsset * const PhysicsAsset = SharedData->EditorSkelComp->GetPhysicsAsset();
	
	
	if(SharedData->EditingMode == FPhATSharedData::PEM_BodyEdit)
	{
		//Bodies
		//first deselect everything
		SharedData->SetSelectedBody(NULL);
	
		//go through every body and add every geom
		for (int32 i = 0; i <PhysicsAsset->BodySetup.Num(); ++i)
		{
			int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(PhysicsAsset->BodySetup[i]->BoneName);

			// If we found a bone for it, add all geom
			if (BoneIndex != INDEX_NONE)
			{
				FKAggregateGeom* AggGeom = &PhysicsAsset->BodySetup[i]->AggGeom;

				for (int32 j = 0; j <AggGeom->SphereElems.Num(); ++j)
				{
					FPhATSharedData::FSelection Selection(i, KPT_Sphere, j);
					SharedData->SetSelectedBody(&Selection, true);
				}

				for (int32 j = 0; j <AggGeom->BoxElems.Num(); ++j)
				{
					FPhATSharedData::FSelection Selection(i, KPT_Box, j);
					SharedData->SetSelectedBody(&Selection, true);
				}

				for (int32 j = 0; j <AggGeom->SphylElems.Num(); ++j)
				{
					FPhATSharedData::FSelection Selection(i, KPT_Sphyl, j);
					SharedData->SetSelectedBody(&Selection, true);
				}

				for (int32 j = 0; j <AggGeom->ConvexElems.Num(); ++j)
				{
					FPhATSharedData::FSelection Selection(i, KPT_Convex, j);
					SharedData->SetSelectedBody(&Selection, true);
				}

			}
		}
	}else
	{
		//Constraints
		//Deselect everything first
		SharedData->SetSelectedConstraint(INDEX_NONE);

		//go through every constraint and add it
		for (int32 i = 0; i <PhysicsAsset->ConstraintSetup.Num(); ++i)
		{
			int32 BoneIndex1 = SharedData->EditorSkelComp->GetBoneIndex(PhysicsAsset->ConstraintSetup[i]->DefaultInstance.ConstraintBone1);
			int32 BoneIndex2 = SharedData->EditorSkelComp->GetBoneIndex(PhysicsAsset->ConstraintSetup[i]->DefaultInstance.ConstraintBone2);
			// if bone doesn't exist, do not draw it. It crashes in random points when we try to manipulate. 
			if (BoneIndex1 != INDEX_NONE && BoneIndex2 != INDEX_NONE)
			{
				SharedData->SetSelectedConstraint(i, true);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
