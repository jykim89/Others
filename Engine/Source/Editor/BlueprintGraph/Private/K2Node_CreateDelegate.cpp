// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "BlueprintGraphPrivatePCH.h"

#include "K2Node_CreateDelegate.h"
#include "DelegateNodeHandlers.h"
#include "K2Node_BaseMCDelegate.h"

struct FK2Node_CreateDelegate_Helper
{
	static FString ObjectInputName;
	static FString DelegateOutputName;
};
FString FK2Node_CreateDelegate_Helper::ObjectInputName(TEXT("InputObject"));
FString FK2Node_CreateDelegate_Helper::DelegateOutputName(TEXT("OutputDelegate"));

UK2Node_CreateDelegate::UK2Node_CreateDelegate(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UK2Node_CreateDelegate::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	if(UEdGraphPin* ObjPin = CreatePin(EGPD_Input, K2Schema->PC_Object, TEXT(""), UObject::StaticClass(), false, false, FK2Node_CreateDelegate_Helper::ObjectInputName))
	{
		ObjPin->PinFriendlyName = NSLOCTEXT("K2Node", "CreateDelegate_ObjectInputName", "Object");
	}

	if(UEdGraphPin* DelegatePin = CreatePin(EGPD_Output, K2Schema->PC_Delegate, TEXT(""), NULL, false, false, FK2Node_CreateDelegate_Helper::DelegateOutputName))
	{
		DelegatePin->PinFriendlyName = NSLOCTEXT("K2Node", "CreateDelegate_DelegateOutName", "Event");
	}

	Super::AllocateDefaultPins();
}

bool UK2Node_CreateDelegate::IsValid(FString* OutMsg, bool bDontUseSkeletalClassForSelf) const
{

	if (GetFunctionName() == NAME_None)
	{
		return false;
	}

	const UEdGraphPin* DelegatePin = GetDelegateOutPin();
	if(!DelegatePin)
	{
		return false;
	}

	const UFunction* Signature = GetDelegateSignature();
	if(!Signature)
	{
		return false;
	}

	for(int PinIter = 1; PinIter < DelegatePin->LinkedTo.Num(); PinIter++)
	{
		const UEdGraphPin* OtherPin = DelegatePin->LinkedTo[PinIter];
		const UFunction* OtherSignature = OtherPin ? 
			FMemberReference::ResolveSimpleMemberReference<UFunction>(OtherPin->PinType.PinSubCategoryMemberReference) : NULL;
		if(!OtherSignature || !Signature->IsSignatureCompatibleWith(OtherSignature))
		{
			return false;
		}
	}

	UClass* ScopeClass = GetScopeClass(bDontUseSkeletalClassForSelf);
	if(!ScopeClass)
	{
		return false;
	}

	FMemberReference MemeberReference;
	MemeberReference.SetDirect(SelectedFunctionName, SelectedFunctionGuid, ScopeClass, false);
	const UFunction* FoundFunction = MemeberReference.ResolveMember<UFunction>((UClass*) NULL);
	if (!FoundFunction || 
		!Signature->IsSignatureCompatibleWith(FoundFunction) || 
		!UEdGraphSchema_K2::FunctionCanBeUsedInDelegate(FoundFunction))
	{
		return false;
	}

	if(!FoundFunction->HasAllFunctionFlags(FUNC_BlueprintAuthorityOnly))
	{
		for(int PinIter = 0; PinIter < DelegatePin->LinkedTo.Num(); PinIter++)
		{
			const UEdGraphPin* OtherPin = DelegatePin->LinkedTo[PinIter];
			const UK2Node_BaseMCDelegate* Node = OtherPin ? Cast<const UK2Node_BaseMCDelegate>(OtherPin->GetOwningNode()) : NULL;
			if (Node && Node->IsAuthorityOnly())
			{
				if(OutMsg)
				{
					*OutMsg = NSLOCTEXT("K2Node", "WrongDelegateAuthorityOnly", "No AuthorityOnly flag").ToString();
				}
				return false;
			}
		}
	}

	return true;
}

void UK2Node_CreateDelegate::ValidationAfterFunctionsAreCreated(class FCompilerResultsLog& MessageLog, bool bFullCompile) const
{
	FString Msg;
	if(!IsValid(&Msg, bFullCompile))
	{
		MessageLog.Error(*FString::Printf( TEXT("%s %s"), *NSLOCTEXT("K2Node", "WrongDelegate", "Events signatures don't match.").ToString(), *Msg));
	}
}

void UK2Node_CreateDelegate::HandleAnyChangeWithoutNotifying()
{
	const auto Blueprint = GetBlueprint();
	const auto SelfScopeClass = Blueprint ? Blueprint->SkeletonGeneratedClass : NULL;
	const auto ParentClass = GetScopeClass();
	const bool bIsSelfScope = SelfScopeClass && ParentClass && ((SelfScopeClass->IsChildOf(ParentClass)) || (SelfScopeClass->ClassGeneratedBy == ParentClass->ClassGeneratedBy));

	FMemberReference FunctionReference;
	FunctionReference.SetDirect(SelectedFunctionName, SelectedFunctionGuid, GetScopeClass(), bIsSelfScope);

	if (FunctionReference.ResolveMember<UFunction>(SelfScopeClass))
	{
		SelectedFunctionName = FunctionReference.GetMemberName();
		SelectedFunctionGuid = FunctionReference.GetMemberGuid();

		if (!SelectedFunctionGuid.IsValid())
		{
			UBlueprint::GetGuidFromClassByFieldName<UFunction>(ParentClass, SelectedFunctionName, SelectedFunctionGuid);
		}
	}

	if(!IsValid())
	{
		SelectedFunctionName = NAME_None;
		SelectedFunctionGuid.Invalidate();
	}
}

void UK2Node_CreateDelegate::HandleAnyChange(UEdGraph* & OutGraph, UBlueprint* & OutBlueprint)
{
	const FName OldSelectedFunctionName = GetFunctionName();
	HandleAnyChangeWithoutNotifying();
	if (OldSelectedFunctionName != GetFunctionName())
	{
		OutGraph = GetGraph();
		OutBlueprint = GetBlueprint();
	}
	else
	{
		OutGraph = NULL;
		OutBlueprint = NULL;
	}
}

void UK2Node_CreateDelegate::HandleAnyChange(bool bForceModify)
{
	const FName OldSelectedFunctionName = GetFunctionName();
	HandleAnyChangeWithoutNotifying();
	if (bForceModify || (OldSelectedFunctionName != GetFunctionName()))
	{
		if(UEdGraph* Graph = GetGraph())
		{
			Graph->NotifyGraphChanged();
		}

		UBlueprint* Blueprint = GetBlueprint();
		if(Blueprint && !Blueprint->bBeingCompiled)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			Blueprint->BroadcastChanged();
		}
	}
	else if (GetFunctionName() == NAME_None)
	{
		if(UEdGraph* Graph = GetGraph())
		{
			Graph->NotifyGraphChanged();
		}
	}
}

void UK2Node_CreateDelegate::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);
	UBlueprint* Blueprint = GetBlueprint();
	if(Blueprint && !Blueprint->bBeingCompiled)
	{
		HandleAnyChange();
	}
	else
	{
		HandleAnyChangeWithoutNotifying();
	}
}

void UK2Node_CreateDelegate::PinTypeChanged(UEdGraphPin* Pin)
{
	Super::PinTypeChanged(Pin);

	HandleAnyChangeWithoutNotifying();
}

void UK2Node_CreateDelegate::NodeConnectionListChanged()
{
	Super::NodeConnectionListChanged();
	UBlueprint* Blueprint = GetBlueprint();
	if(Blueprint && !Blueprint->bBeingCompiled)
	{
		HandleAnyChange();
	}
	else
	{
		HandleAnyChangeWithoutNotifying();
	}
}

void UK2Node_CreateDelegate::PostReconstructNode()
{
	Super::PostReconstructNode();
	HandleAnyChange();
}

UFunction* UK2Node_CreateDelegate::GetDelegateSignature() const
{
	UEdGraphPin* Pin = GetDelegateOutPin();
	check(Pin != NULL);
	if(Pin->LinkedTo.Num())
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if(UEdGraphPin* ResultPin = Pin->LinkedTo[0])
		{
			ensure(K2Schema->PC_Delegate == ResultPin->PinType.PinCategory);
			return FMemberReference::ResolveSimpleMemberReference<UFunction>(ResultPin->PinType.PinSubCategoryMemberReference);
		}
	}
	return NULL;
}

UClass* UK2Node_CreateDelegate::GetScopeClass(bool bDontUseSkeletalClassForSelf/* = false*/) const
{
	UEdGraphPin* Pin = FindPin(FK2Node_CreateDelegate_Helper::ObjectInputName);
	check(Pin != NULL);
	check(Pin->LinkedTo.Num() <= 1);
	if(Pin->LinkedTo.Num())
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if(UEdGraphPin* ResultPin = Pin->LinkedTo[0])
		{
			ensure(K2Schema->PC_Object == ResultPin->PinType.PinCategory);
			if(UClass* TrueScopeClass = Cast<UClass>(ResultPin->PinType.PinSubCategoryObject.Get()))
			{
				if(UBlueprint* ScopeClassBlueprint = Cast<UBlueprint>(TrueScopeClass->ClassGeneratedBy))
				{
					if(ScopeClassBlueprint->SkeletonGeneratedClass)
					{
						return ScopeClassBlueprint->SkeletonGeneratedClass;
					}
				}
				return TrueScopeClass;
			}
			if (K2Schema->PN_Self == ResultPin->PinType.PinSubCategory)
			{
				if (UBlueprint* ScopeClassBlueprint = GetBlueprint())
				{
					return bDontUseSkeletalClassForSelf ? ScopeClassBlueprint->GeneratedClass : ScopeClassBlueprint->SkeletonGeneratedClass;
				}
			}
		}
	}
	return NULL;
}

FName UK2Node_CreateDelegate::GetFunctionName() const
{
	return SelectedFunctionName;
}

UEdGraphPin* UK2Node_CreateDelegate::GetDelegateOutPin() const
{
	return FindPin(FK2Node_CreateDelegate_Helper::DelegateOutputName);
}

UEdGraphPin* UK2Node_CreateDelegate::GetObjectInPin() const
{
	return FindPinChecked(FK2Node_CreateDelegate_Helper::ObjectInputName);
}

FText UK2Node_CreateDelegate::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "CreateDelegate", "Create Event");
}

UObject* UK2Node_CreateDelegate::GetJumpTargetForDoubleClick() const
{
	UBlueprint* ScopeClassBlueprint = NULL;

	UEdGraphPin* Pin = FindPinChecked(FK2Node_CreateDelegate_Helper::ObjectInputName);
	UEdGraphPin* ResultPin = Pin->LinkedTo.Num() ? Pin->LinkedTo[0] : NULL;
	if (ResultPin)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		ensure(K2Schema->PC_Object == ResultPin->PinType.PinCategory);
		if (UClass* TrueScopeClass = Cast<UClass>(ResultPin->PinType.PinSubCategoryObject.Get()))
		{
			ScopeClassBlueprint = Cast<UBlueprint>(TrueScopeClass->ClassGeneratedBy);
		}
		else if (K2Schema->PN_Self == ResultPin->PinType.PinSubCategory)
		{
			ScopeClassBlueprint = GetBlueprint();
		}
	}

	if (ScopeClassBlueprint)
	{
		if (UEdGraph* FoundGraph = FindObject<UEdGraph>(ScopeClassBlueprint, *GetFunctionName().ToString()))
		{
			if(!FBlueprintEditorUtils::IsGraphIntermediate(FoundGraph))
			{
				return FoundGraph;
			}
		}
		for (auto UbergraphIt = ScopeClassBlueprint->UbergraphPages.CreateIterator(); UbergraphIt; ++UbergraphIt)
		{
			UEdGraph* Graph = (*UbergraphIt);
			if(!FBlueprintEditorUtils::IsGraphIntermediate(Graph))
			{
				TArray<UK2Node_Event*> EventNodes;
				Graph->GetNodesOfClass(EventNodes);
				for (auto EventIt = EventNodes.CreateIterator(); EventIt; ++EventIt)
				{
					if(GetFunctionName() == (*EventIt)->GetFunctionName())
					{
						return *EventIt;
					}
				}
			}
		}
	}
	return NULL;
}

FNodeHandlingFunctor* UK2Node_CreateDelegate::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_CreateDelegate(CompilerContext);
}

void UK2Node_CreateDelegate::SetFunction(FName Name)
{
	SelectedFunctionName = Name;
	SelectedFunctionGuid.Invalidate();
}