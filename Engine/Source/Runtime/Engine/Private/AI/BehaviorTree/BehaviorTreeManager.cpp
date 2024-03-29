// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

DEFINE_STAT(STAT_AI_BehaviorTree_Tick);
DEFINE_STAT(STAT_AI_BehaviorTree_LoadTime);
DEFINE_STAT(STAT_AI_BehaviorTree_SearchTime);
DEFINE_STAT(STAT_AI_BehaviorTree_ExecutionTime);
DEFINE_STAT(STAT_AI_BehaviorTree_AuxUpdateTime);
DEFINE_STAT(STAT_AI_BehaviorTree_NumTemplates);
DEFINE_STAT(STAT_AI_BehaviorTree_NumInstances);
DEFINE_STAT(STAT_AI_BehaviorTree_InstanceMemory);

UBehaviorTreeManager::UBehaviorTreeManager(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	MaxDebuggerSteps = 100;
}

void UBehaviorTreeManager::FinishDestroy()
{
	SET_DWORD_STAT(STAT_AI_BehaviorTree_NumTemplates, 0);

	Super::FinishDestroy();
}

bool UBehaviorTreeManager::IsBehaviorTreeUsageEnabled()
{
	struct FCheckMeStaticly
	{
		bool bBehaviorTreeEditorEnabled;
		bool bBehaviorTreeUsageEnabled;
		FCheckMeStaticly() : bBehaviorTreeEditorEnabled(false), bBehaviorTreeUsageEnabled(true)
		{
#if WITH_EDITOR
			bool bBehaviorTreeEditorEnabledFromUserSettings = false;
			GConfig->GetBool(TEXT("/Script/UnrealEd.EditorExperimentalSettings"), TEXT("bBehaviorTreeEditor"), bBehaviorTreeEditorEnabledFromUserSettings, GEditorUserSettingsIni);

			GConfig->GetBool(TEXT("BehaviorTreesEd"), TEXT("BehaviorTreeEditorEnabled"), bBehaviorTreeEditorEnabled, GEngineIni);
			bBehaviorTreeEditorEnabled = bBehaviorTreeEditorEnabled || bBehaviorTreeEditorEnabledFromUserSettings;
			if (bBehaviorTreeUsageEnabled && bBehaviorTreeEditorEnabled && !FModuleManager::Get().IsModuleLoaded(TEXT("BehaviorTreeEditor")))
			{
				// load this module earlier, before any access to BehaviorTree assets
				FModuleManager::Get().LoadModule(TEXT("BehaviorTreeEditor"));
			}
#endif
		}
	};

	static FCheckMeStaticly CheckMeStaticly;
	return CheckMeStaticly.bBehaviorTreeUsageEnabled;
}

int32 UBehaviorTreeManager::GetAlignedDataSize(int32 Size)
{
	// round to 4 bytes
	return ((Size + 3) & ~3);
}

struct FNodeInitializationData
{
	UBTNode* Node;
	UBTCompositeNode* ParentNode;
	uint16 ExecutionIndex;
	uint16 DataSize;
	uint16 SpecialDataSize;
	uint8 TreeDepth;

	FNodeInitializationData() {}
	FNodeInitializationData(UBTNode* InNode, UBTCompositeNode* InParentNode,
		uint16 InExecutionIndex, uint8 InTreeDepth, uint16 NodeMemory, uint16 SpecialNodeMemory = 0)
		: Node(InNode), ParentNode(InParentNode), ExecutionIndex(InExecutionIndex), TreeDepth(InTreeDepth)
	{
		SpecialDataSize = UBehaviorTreeManager::GetAlignedDataSize(SpecialNodeMemory);

		const uint16 NodeMemorySize = NodeMemory + SpecialDataSize;
		DataSize = (NodeMemorySize <= 2) ? NodeMemorySize : UBehaviorTreeManager::GetAlignedDataSize(NodeMemorySize);
	}

	struct FMemorySort
	{
		FORCEINLINE bool operator()(const FNodeInitializationData& A, const FNodeInitializationData& B) const
		{
			return A.DataSize > B.DataSize;
		}
	};
};

static void InitializeNodeHelper(UBTCompositeNode* ParentNode, UBTNode* NodeOb,
	uint8 TreeDepth, uint16& ExecutionIndex, TArray<FNodeInitializationData>& InitList,
	class UBehaviorTree* TreeAsset, UObject* NodeOuter)
{
	// special case: subtrees
	UBTTask_RunBehavior* SubtreeTask = Cast<UBTTask_RunBehavior>(NodeOb);
	if (SubtreeTask)
	{
		ExecutionIndex += SubtreeTask->GetInjectedNodesCount();
	}

	InitList.Add(FNodeInitializationData(NodeOb, ParentNode, ExecutionIndex, TreeDepth, NodeOb->GetInstanceMemorySize(), NodeOb->GetSpecialMemorySize()));
	NodeOb->InitializeFromAsset(TreeAsset);
	ExecutionIndex++;

	UBTCompositeNode* CompositeOb = Cast<UBTCompositeNode>(NodeOb);
	if (CompositeOb)
	{
		for (int32 i = 0; i < CompositeOb->Services.Num(); i++)
		{
			UBTService* Service = Cast<UBTService>(StaticDuplicateObject(CompositeOb->Services[i], NodeOuter, TEXT("None")));;
			CompositeOb->Services[i] = Service;

			InitList.Add(FNodeInitializationData(Service, CompositeOb, ExecutionIndex, TreeDepth,
				Service->GetInstanceMemorySize(), Service->GetSpecialMemorySize()));

			Service->InitializeFromAsset(TreeAsset);
			ExecutionIndex++;
		}

		for (int32 i = 0; i < CompositeOb->Children.Num(); i++)
		{
			FBTCompositeChild& ChildInfo = CompositeOb->Children[i];
			for (int32 j = 0; j < ChildInfo.Decorators.Num(); j++)
			{
				UBTDecorator* Decorator = Cast<UBTDecorator>(StaticDuplicateObject(ChildInfo.Decorators[j], NodeOuter, TEXT("None")));
				ChildInfo.Decorators[j] = Decorator;

				InitList.Add(FNodeInitializationData(Decorator, CompositeOb, ExecutionIndex, TreeDepth,
					Decorator->GetInstanceMemorySize(), Decorator->GetSpecialMemorySize()));

				Decorator->InitializeFromAsset(TreeAsset);
				Decorator->InitializeDecorator(i);
				ExecutionIndex++;
			}

			UBTNode* ChildNode = NULL;
			
			if (ChildInfo.ChildComposite)
			{
				ChildInfo.ChildComposite = Cast<UBTCompositeNode>(StaticDuplicateObject(ChildInfo.ChildComposite, NodeOuter, TEXT("None")));
				ChildNode = ChildInfo.ChildComposite;
			}
			else if (ChildInfo.ChildTask)
			{
				ChildInfo.ChildTask = Cast<UBTTaskNode>(StaticDuplicateObject(ChildInfo.ChildTask, NodeOuter, TEXT("None")));
				ChildNode = ChildInfo.ChildTask;
			}

			if (ChildNode)
			{
				InitializeNodeHelper(CompositeOb, ChildNode, TreeDepth + 1, ExecutionIndex, InitList, TreeAsset, NodeOuter);
			}
		}

		CompositeOb->InitializeComposite(InitList.Num() - 1);
	}
}

bool UBehaviorTreeManager::LoadTree(class UBehaviorTree* Asset, UBTCompositeNode*& Root, uint16& InstanceMemorySize)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_BehaviorTree_LoadTime);

	for (int32 i = 0; i < LoadedTemplates.Num(); i++)
	{
		FBehaviorTreeTemplateInfo& TemplateInfo = LoadedTemplates[i];
		if (TemplateInfo.Asset == Asset)
		{
			Root = TemplateInfo.Template;
			InstanceMemorySize = TemplateInfo.InstanceMemorySize;
			return true;
		}
	}

	if (Asset->RootNode)
	{
		FBehaviorTreeTemplateInfo TemplateInfo;
		TemplateInfo.Asset = Asset;
		TemplateInfo.Template = Cast<UBTCompositeNode>(StaticDuplicateObject(Asset->RootNode, this, TEXT("None")));

		TArray<FNodeInitializationData> InitList;
		uint16 ExecutionIndex = 0;
		InitializeNodeHelper(NULL, TemplateInfo.Template, 0, ExecutionIndex, InitList, Asset, this);

#if USE_BEHAVIORTREE_DEBUGGER
		// fill in information about next nodes in execution index, before sorting memory offsets
		for (int32 i = 0; i < InitList.Num() - 1; i++)
		{
			InitList[i].Node->InitializeExecutionOrder(InitList[i + 1].Node);
		}
#endif

		// sort nodes by memory size, so they can be packed better
		// it still won't protect against structures, that are internally misaligned (-> uint8, uint32)
		// but since all Engine level nodes are good... 
		InitList.Sort(FNodeInitializationData::FMemorySort());
		uint16 MemoryOffset = 0;
		for (int32 i = 0; i < InitList.Num(); i++)
		{
			InitList[i].Node->InitializeNode(InitList[i].ParentNode, InitList[i].ExecutionIndex, InitList[i].SpecialDataSize + MemoryOffset, InitList[i].TreeDepth);
			MemoryOffset += InitList[i].DataSize;
		}
		
		TemplateInfo.InstanceMemorySize = MemoryOffset;

		INC_DWORD_STAT(STAT_AI_BehaviorTree_NumTemplates);
		LoadedTemplates.Add(TemplateInfo);
		Root = TemplateInfo.Template;
		InstanceMemorySize = TemplateInfo.InstanceMemorySize;
		return true;
	}

	return false;
}

void UBehaviorTreeManager::InitializeMemoryHelper(const TArray<UBTDecorator*>& Nodes, TArray<uint16>& MemoryOffsets, int32& MemorySize)
{
	TArray<FNodeInitializationData> InitList;
	for (int32 i = 0; i < Nodes.Num(); i++)
	{
		InitList.Add(FNodeInitializationData(Nodes[i], NULL, 0, 0, Nodes[i]->GetInstanceMemorySize(), Nodes[i]->GetSpecialMemorySize()));
	}

	InitList.Sort(FNodeInitializationData::FMemorySort());

	uint16 MemoryOffset = 0;
	MemoryOffsets.AddZeroed(Nodes.Num());

	for (int32 i = 0; i < InitList.Num(); i++)
	{
		MemoryOffsets[i] = InitList[i].SpecialDataSize + MemoryOffset;
		MemoryOffset += InitList[i].DataSize;
	}

	MemorySize = MemoryOffset;
}
