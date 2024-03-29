// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"

#include "CompilerResultsLog.h"
#include "KismetCompiler.h"
#include "EventEntryHandler.h"

const FString UK2Node_Event::DelegateOutputName(TEXT("OutputDelegate"));

bool UK2Node_Event::IsCosmeticTickEvent() const
{
	// Special case for EventTick/ReceiveTick that is conditionally executed by a separate bool rather than function flag.
	static const FName EventTickName(TEXT("ReceiveTick"));
	if (EventSignatureName == EventTickName)
	{
		const UBlueprint* Blueprint = GetBlueprint();
		if (Blueprint)
		{
			UClass* BPClass = Blueprint->GeneratedClass;
			const AActor* DefaultActor = BPClass ? Cast<const AActor>(BPClass->GetDefaultObject()) : NULL;
			if (DefaultActor && !DefaultActor->AllowReceiveTickEventOnDedicatedServer())
			{
				return true;
			}
		}
	}

	return false;
}


#define LOCTEXT_NAMESPACE "K2Node_Event"

UK2Node_Event::UK2Node_Event(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	FunctionFlags = 0;
}

FNodeHandlingFunctor* UK2Node_Event::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_EventEntry(CompilerContext);
}


FLinearColor UK2Node_Event::GetNodeTitleColor() const
{
	return GEditor->AccessEditorUserSettings().EventNodeTitleColor;
}

FText UK2Node_Event::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (bOverrideFunction || (CustomFunctionName == NAME_None))
	{
		FString FunctionName = EventSignatureName.ToString(); // If we fail to find the function, still want to write something on the node.

		if (UFunction* Function = FindField<UFunction>(EventSignatureClass, EventSignatureName))
		{
			FunctionName = UEdGraphSchema_K2::GetFriendlySignitureName(Function);
		}

		FFormatNamedArguments Args;
		Args.Add(TEXT("FunctionName"), FText::FromString(FunctionName));
		FText Title = FText::Format(NSLOCTEXT("K2Node", "Event_Name", "Event {FunctionName}"), Args);

		if(TitleType == ENodeTitleType::FullTitle && EventSignatureClass != NULL && EventSignatureClass->IsChildOf(UInterface::StaticClass()))
		{
			FString SourceString = EventSignatureClass->GetName();

			// @todo: This action won't be necessary once the new name convention is used.
			if(SourceString.EndsWith(TEXT("_C")))
			{
				SourceString = SourceString.LeftChop(2);
			}

			FFormatNamedArguments FullTitleArgs;
			FullTitleArgs.Add(TEXT("Title"), Title);
			FullTitleArgs.Add(TEXT("InterfaceClass"), FText::FromString(SourceString));

			Title = FText::Format(LOCTEXT("EventFromInterface", "{Title}\nFrom {InterfaceClass}"), FullTitleArgs);
		}

		return Title;
	}
	else
	{
		return FText::FromName(CustomFunctionName);
	}
}

FString UK2Node_Event::GetNodeNativeTitle(ENodeTitleType::Type TitleType) const
{
	// Do not setup this function for localization, intentionally left unlocalized!
	if (bOverrideFunction || (CustomFunctionName == NAME_None))
	{
		FString FunctionName = EventSignatureName.ToString(); // If we fail to find the function, still want to write something on the node.

		if (UFunction* Function = FindField<UFunction>(EventSignatureClass, EventSignatureName))
		{
			FunctionName = UEdGraphSchema_K2::GetFriendlySignitureName(Function);
		}

		FString TitleString = FString(TEXT("Event ")) + FunctionName;

		if(TitleType == ENodeTitleType::FullTitle && EventSignatureClass != NULL && EventSignatureClass->IsChildOf(UInterface::StaticClass()))
		{
			FString SourceString = EventSignatureClass->GetName();

			// @todo: This action won't be necessary once the new name convention is used.
			if(SourceString.EndsWith(TEXT("_C")))
			{
				SourceString = SourceString.LeftChop(2);
			}

			TitleString += FString(TEXT("\n")) + FString::Printf(TEXT("From %s"), *SourceString);
		}

		return TitleString;
	}
	else
	{
		return CustomFunctionName.ToString();
	}
}

FString UK2Node_Event::GetTooltip() const
{
	FString Tooltip;

	UFunction* Function = FindField<UFunction>(EventSignatureClass, EventSignatureName);
	if (Function != NULL)
	{
		Tooltip = Function->GetToolTipText().ToString();

		if (bOverrideFunction || (CustomFunctionName == NAME_None))
		{
			FString ClientString;

			//@TODO: KISMETREPLICATION: Should do this for events with a custom function name, if it's a newly introduced replicating thingy
			if (Function->HasAllFunctionFlags(FUNC_BlueprintCosmetic) || IsCosmeticTickEvent())
			{
				ClientString = NSLOCTEXT("K2Node", "ClientEvent", "\n\nCosmetic. This event is only for cosmetic, non-gameplay actions.").ToString();
			} 
			else if(Function->HasAllFunctionFlags(FUNC_BlueprintAuthorityOnly))
			{
				ClientString = NSLOCTEXT("K2Node", "ServerEvent", "\n\nAuthority Only. This event only fires on the server.").ToString();
			}

			Tooltip += ClientString;
		}
	}

	return Tooltip;
}

FString UK2Node_Event::GetKeywords() const
{
	FString Keywords;

	UFunction* Function = FindField<UFunction>(EventSignatureClass, EventSignatureName);
	if (Function != NULL)
	{
		Keywords = UK2Node_CallFunction::GetKeywordsForFunction( Function );
	}

	return Keywords;
}

FString UK2Node_Event::GetDocumentationLink() const
{
	if ( EventSignatureClass != NULL )
	{
		return FString::Printf(TEXT("Shared/Types/%s%s"), EventSignatureClass->GetPrefixCPP(), *EventSignatureClass->GetName());
	}

	return TEXT("");
}

FString UK2Node_Event::GetDocumentationExcerptName() const
{
	return EventSignatureName.ToString();
}

void UK2Node_Event::PostReconstructNode()
{
	UpdateDelegatePin();
}

void UK2Node_Event::UpdateDelegatePin()
{
	UEdGraphPin* Pin = FindPinChecked(DelegateOutputName);
	checkSlow(EGPD_Output == Pin->Direction);
	const UObject* OldSignature = Pin->PinType.PinSubCategoryObject.Get();

	if(bOverrideFunction)
	{
		Pin->PinType.PinSubCategoryObject = EventSignatureClass->FindFunctionByName(EventSignatureName);
	}
	else if(UBlueprint* Blueprint = GetBlueprint())
	{
		Pin->PinType.PinSubCategoryObject = Blueprint->SkeletonGeneratedClass 
			? Blueprint->SkeletonGeneratedClass->FindFunctionByName(CustomFunctionName)
			: NULL;
	}
	else
	{
		Pin->PinType.PinSubCategoryObject = NULL;
	}

	if(OldSignature != Pin->PinType.PinSubCategoryObject.Get())
	{
		PinTypeChanged(Pin);
	}
}

void UK2Node_Event::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if(Pin == FindPin(DelegateOutputName)) 
	{
		UpdateDelegatePin();
	}

	Super::PinConnectionListChanged(Pin);
}

FName UK2Node_Event::GetFunctionName() const
{
	return bOverrideFunction ? EventSignatureName : CustomFunctionName;
}

UFunction* UK2Node_Event::FindEventSignatureFunction()
{
	UFunction* Function = FindField<UFunction>(EventSignatureClass, EventSignatureName);

	// First try remap table
	if ((Function == NULL) && (EventSignatureClass != NULL))
	{
		Function = Cast<UFunction>(FindRemappedField(EventSignatureClass, EventSignatureName));
		if( Function )
		{
			// Found a remapped property, update the node
			EventSignatureName = Function->GetFName();
			EventSignatureClass = Cast<UClass>(Function->GetOuter());
		}
	}

	return Function;
}

void UK2Node_Event::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	CreatePin(EGPD_Output, K2Schema->PC_Delegate, TEXT(""), NULL, false, false, DelegateOutputName);
	CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, K2Schema->PN_Then);

	const UFunction* Function = FindEventSignatureFunction();
	if (Function != NULL)
	{
		CreatePinsForFunctionEntryExit(Function, /*bIsFunctionEntry=*/ true);
	}

	Super::AllocateDefaultPins();
}

void UK2Node_Event::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	// If we are overriding a function, but we can;t find the function we are overriding, that is a compile error
	if(bOverrideFunction && FindField<UFunction>(EventSignatureClass, EventSignatureName) == NULL)
	{
		MessageLog.Error(*FString::Printf(*NSLOCTEXT("KismetCompiler", "MissingEventSig_Error", "Missing Event '%s' for @@").ToString(), *EventSignatureName.ToString()), this);
	}
}

void UK2Node_Event::GetRedirectPinNames(const UEdGraphPin& Pin, TArray<FString>& RedirectPinNames) const
{
	Super::GetRedirectPinNames(Pin, RedirectPinNames);

	if ( RedirectPinNames.Num() > 0 )
	{
		const FString & OldPinName = RedirectPinNames[0];

		// first add functionname.param
		RedirectPinNames.Add(FString::Printf(TEXT("%s.%s"), *EventSignatureName.ToString(), *OldPinName));
		// if there is class, also add an option for class.functionname.param
		if ( EventSignatureClass!=NULL )
		{
			RedirectPinNames.Add(FString::Printf(TEXT("%s.%s.%s"), *EventSignatureClass->GetName(), *EventSignatureName.ToString(), *OldPinName));
		}
	}
}

bool UK2Node_Event::IsFunctionEntryCompatible(const UK2Node_FunctionEntry* EntryNode) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Copy a set of the pin references for both nodes, so we can pare down lists
	TArray<UEdGraphPin*> EventPins = Pins;
	TArray<UEdGraphPin*> EntryPins = EntryNode->Pins;

	// Prune the exec wires and inputs (delegate binding) from both sets
	for(int32 i = 0; i < EventPins.Num(); i++)
	{
		const UEdGraphPin* CurPin = EventPins[i];
		if( CurPin->PinType.PinCategory == K2Schema->PC_Exec 
			|| CurPin->PinType.PinSubCategory == K2Schema->PSC_Self
			|| CurPin->PinName == DelegateOutputName
			|| CurPin->Direction == EGPD_Input )
		{
			EventPins.RemoveAt(i, 1);
			i--;
		}
	}

	for(int32 i = 0; i < EntryPins.Num(); i++)
	{
		const UEdGraphPin* CurPin = EntryPins[i];
		if( CurPin->PinType.PinCategory == K2Schema->PC_Exec 
			|| CurPin->PinType.PinSubCategory == K2Schema->PSC_Self
			|| CurPin->Direction == EGPD_Input)
		{
			EntryPins.RemoveAt(i, 1);
			i--;
		}
	}

	// Early out:  we don't have the same number of parameters
	if( EventPins.Num() != EntryPins.Num() )
	{
		return false;
	}

	// Now check through the event's pins, and check for compatible pins, removing them if we find a match.
	for( int32 i = 0; i < EventPins.Num(); i++ )
	{
		const UEdGraphPin* CurEventPin = EventPins[i];

		bool bMatchFound = false;
		for( int32 j = 0; j < EntryPins.Num(); j++ )
		{
			const UEdGraphPin* CurEntryPin = EntryPins[j];
			if( CurEntryPin->PinName == CurEventPin->PinName )
			{
				// Check to make sure pins are of the same type
				if( K2Schema->ArePinTypesCompatible(CurEntryPin->PinType, CurEventPin->PinType) )
				{
					// Found a match, remove it from the list
					bMatchFound = true;
					EntryPins.RemoveAt(j, 1);
					break;
				}
				else
				{
					// Found a pin, but the type has changed, bail.
					bMatchFound = false;
					break;
				}
			}
		}

		if( bMatchFound )
		{
			// Found a match, remove it from the event array
			EventPins.RemoveAt(i, 1);
			i--;
		}
		else
		{
			// Didn't find a match...bail!
			return false;
		}
	}

	// Checked for matches, if any pins remain in either array, they were unmatched.
	return (EventPins.Num() == 0) && (EntryPins.Num() == 0);
}

bool UK2Node_Event::CanPasteHere(const UEdGraph* TargetGraph, const UEdGraphSchema* Schema) const
{
	// By default, to be safe, we don't allow events to be pasted, except under special circumstances (see below)
	bool bDisallowPaste = true;

	// Ensure that we can be instanced under the specified schema
	if(CanCreateUnderSpecifiedSchema(Schema))
	{
		// Can only place events in ubergraphs
		if(Schema->GetGraphType(TargetGraph) == EGraphType::GT_Ubergraph)
		{
			// Find the Blueprint that owns the target graph
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
			if(Blueprint && Blueprint->SkeletonGeneratedClass)
			{
				TArray<FName> ExistingNamesInUse;
				TArray<FString> ExcludedEventNames;
				TArray<UK2Node_Event*> ExistingEventNodes;
				TArray<UClass*> ImplementedInterfaceClasses;

				// Gather all names in use by the Blueprint class
				FBlueprintEditorUtils::GetFunctionNameList(Blueprint, ExistingNamesInUse);
				FBlueprintEditorUtils::GetClassVariableList(Blueprint, ExistingNamesInUse);

				// Gather all existing event nodes
				FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(Blueprint, ExistingEventNodes);

				// Gather any event names excluded by the Blueprint class
				const FString ExclusionListKeyName = TEXT("KismetHideOverrides");
				if(Blueprint->ParentClass->HasMetaData(*ExclusionListKeyName))
				{
					const FString ExcludedEventNameString = Blueprint->ParentClass->GetMetaData(*ExclusionListKeyName);
					ExcludedEventNameString.ParseIntoArray(&ExcludedEventNames, TEXT(","), true);
				}

				// Gather all interfaces implemented by the Blueprint class
				FBlueprintEditorUtils::FindImplementedInterfaces(Blueprint, true, ImplementedInterfaceClasses);

				// If this is an internal event, don't paste this event
				if(!bInternalEvent)
				{
					// If this is a function override
					if(bOverrideFunction)
					{
						// If the function name is hidden by the parent class, don't paste this event
						bDisallowPaste = EventSignatureClass == Blueprint->ParentClass
							&& ExcludedEventNames.Contains(EventSignatureName.ToString());
						if(!bDisallowPaste)
						{
							// If the event function is already handled in this Blueprint, don't paste this event
							for(int32 i = 0; i < ExistingEventNodes.Num() && !bDisallowPaste; ++i)
							{
								bDisallowPaste = ExistingEventNodes[i]->bOverrideFunction
									&& ExistingEventNodes[i]->EventSignatureName == EventSignatureName
									&& ExistingEventNodes[i]->EventSignatureClass == EventSignatureClass;
							}

							if(!bDisallowPaste)
							{
								// If the signature class is not implemented by the Blueprint parent class or an interface, don't paste this event
								bDisallowPaste = !Blueprint->ParentClass->IsChildOf(EventSignatureClass)
									&& !ImplementedInterfaceClasses.Contains(EventSignatureClass);
								if(bDisallowPaste)
								{
									UE_LOG(LogBlueprint, Log, TEXT("Cannot paste event node (%s) directly because the event signature class (%s) is incompatible with this Blueprint."), *GetFName().ToString(), EventSignatureClass ? *EventSignatureClass->GetFName().ToString() : TEXT("NONE"));
								}
							}
							else
							{
								UE_LOG(LogBlueprint, Log, TEXT("Cannot paste event node (%s) directly because the event function (%s) is already handled."), *GetFName().ToString(), *EventSignatureName.ToString());
							}
						}
						else
						{
							UE_LOG(LogBlueprint, Log, TEXT("Cannot paste event node (%s) directly because the event function (%s) is hidden by the Blueprint parent class (%s)."), *GetFName().ToString(), *EventSignatureName.ToString(), EventSignatureClass ? *EventSignatureClass->GetFName().ToString() : TEXT("NONE"));
						}
					}
					else if(CustomFunctionName != NAME_None)
					{
						// If this name is already in use, we can't paste this event
						bDisallowPaste = ExistingNamesInUse.Contains(CustomFunctionName);

						if(!bDisallowPaste)
						{
							// Handle events that have a custom function name with an actual signature name/class that is not an override (e.g. AnimNotify events)
							if(EventSignatureName != NAME_None)
							{
								// If the signature class is not implemented by the Blueprint parent class or an interface, don't paste this event
								bDisallowPaste = !Blueprint->ParentClass->IsChildOf(EventSignatureClass)
									&& !ImplementedInterfaceClasses.Contains(EventSignatureClass);
								if(bDisallowPaste)
								{
									UE_LOG(LogBlueprint, Log, TEXT("Cannot paste event node (%s) directly because the custom event function (%s) with event signature name (%s) has an event signature class (%s) that is incompatible with this Blueprint."), *GetFName().ToString(), *CustomFunctionName.ToString(), *EventSignatureName.ToString(), EventSignatureClass ? *EventSignatureClass->GetFName().ToString() : TEXT("NONE"));
								}
							}
						}
						else
						{
							UE_LOG(LogBlueprint, Log, TEXT("Cannot paste event node (%s) directly because the custom event function (%s) is already handled."), *GetFName().ToString(), *CustomFunctionName.ToString());
						}
					}
					else
					{
						UE_LOG(LogBlueprint, Log, TEXT("Cannot paste event node (%s) directly because the event configuration is not specifically handled (EventSignatureName=%s, EventSignatureClass=%s)."), *GetFName().ToString(), *EventSignatureName.ToString(), EventSignatureClass ? *EventSignatureClass->GetFName().ToString() : TEXT("NONE"));
					}
				}
				else
				{
					UE_LOG(LogBlueprint, Log, TEXT("Cannot paste event node (%s) directly because it is flagged as an internal event."), *GetFName().ToString());
				}
			}
		}
	}
	else
	{
		UE_LOG(LogBlueprint, Log, TEXT("Cannot paste event node (%s) directly because it cannot be created under the specified schema."), *GetFName().ToString());
	}

	return !bDisallowPaste;
}

FString UK2Node_Event::GetLocalizedNetString(uint32 FunctionFlags, bool Calling)
{
	FString RPCString;
	if (FunctionFlags & FUNC_Net)
	{
		RPCString = FString(TEXT("\n"));

		if (FunctionFlags & FUNC_NetReliable)
		{
			RPCString += NSLOCTEXT("K2Node", "CustomEvent_ReplicatedReliable", "RELIABLE ").ToString();
		}

		if (FunctionFlags & FUNC_NetMulticast)
		{
			if (Calling)
			{
				RPCString += NSLOCTEXT("K2Node", "CustomEvent_ReplicatedMulticast", "Replicated To All (if server)").ToString();
			}
			else
			{
				RPCString += NSLOCTEXT("K2Node", "CustomEvent_ReplicatedMulticastFrom", "Replicated From Server\nExecutes On All").ToString();
			}

		}
		else if (FunctionFlags & FUNC_NetServer)
		{
			if (Calling)
			{
				RPCString += NSLOCTEXT("K2Node", "CustomEvent_ReplicatedServer", "Replicated To Server (if owning client)").ToString();
			}
			else
			{
				RPCString += NSLOCTEXT("K2Node", "CustomEvent_ReplicatedServerFrom", "Replicated From Client\nExecutes On Server").ToString();
			}
		}
		else if (FunctionFlags & FUNC_NetClient)
		{
			if (Calling)
			{
				RPCString += NSLOCTEXT("K2Node", "CustomEvent_ReplicatedClient", "Replicated To Owning Client (if server)").ToString();
			}
			else
			{
				RPCString += NSLOCTEXT("K2Node", "CustomEvent_ReplicatedClientFrom", "Replicated From Server\nExecutes on Owning Client").ToString();
			}
		}
	}
	return RPCString;
}

void UK2Node_Event::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (CompilerContext.bIsFullCompile)
	{
		UEdGraphPin* OrgDelegatePin = FindPin(UK2Node_Event::DelegateOutputName);
		if (OrgDelegatePin && OrgDelegatePin->LinkedTo.Num() > 0)
		{
			const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

			const FName FunctionName = GetFunctionName();
			if(FunctionName == NAME_None)
			{
				CompilerContext.MessageLog.Error(*FString::Printf(*LOCTEXT("EventDelegateName_Error", "Event node @@ has no name of function.").ToString()), this);
			}

			UK2Node_Self* SelfNode = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(this, SourceGraph);
			SelfNode->AllocateDefaultPins();

			UK2Node_CreateDelegate* CreateDelegateNode = CompilerContext.SpawnIntermediateNode<UK2Node_CreateDelegate>(this, SourceGraph);
			CreateDelegateNode->AllocateDefaultPins();
			CompilerContext.MovePinLinksToIntermediate(*OrgDelegatePin, *CreateDelegateNode->GetDelegateOutPin());
			Schema->TryCreateConnection(SelfNode->FindPinChecked(Schema->PN_Self), CreateDelegateNode->GetObjectInPin());
			CreateDelegateNode->SetFunction(FunctionName);
			CreateDelegateNode->HandleAnyChangeWithoutNotifying();
			if (CreateDelegateNode->GetFunctionName() != FunctionName)
			{
				CreateDelegateNode->SetFunction(FunctionName);
				CompilerContext.MessageLog.Warning(*FString::Printf(*LOCTEXT("EventDelegateError", "Invalid delegate connection @@. Try recompile.").ToString()), OrgDelegatePin);
			}
		}
	}
}

FName UK2Node_Event::GetCornerIcon() const
{
	if (UFunction* Function = FindField<UFunction>(EventSignatureClass, EventSignatureName))
	{
		if (bOverrideFunction || (CustomFunctionName == NAME_None))
		{
			//@TODO: KISMETREPLICATION: Should do this for events with a custom function name, if it's a newly introduced replicating thingy
			if (Function->HasAllFunctionFlags(FUNC_BlueprintCosmetic) || IsCosmeticTickEvent())
			{
				return TEXT("Graph.Replication.ClientEvent");
			} 
			else if(Function->HasAllFunctionFlags(FUNC_BlueprintAuthorityOnly))
			{
				return TEXT("Graph.Replication.AuthorityOnly");
			}
		}
	}

	if(IsUsedByAuthorityOnlyDelegate())
	{
		return TEXT("Graph.Replication.AuthorityOnly");
	}

	if(EventSignatureClass != NULL && EventSignatureClass->IsChildOf(UInterface::StaticClass()))
	{
		return TEXT("Graph.Event.InterfaceEventIcon");
	}

	return Super::GetCornerIcon();
}

FText UK2Node_Event::GetToolTipHeading() const
{
	FText Heading = Super::GetToolTipHeading();

	FText EventHeading = FText::GetEmpty();
	if (UFunction* Function = FindField<UFunction>(EventSignatureClass, EventSignatureName))
	{
		if (bOverrideFunction || (CustomFunctionName == NAME_None))
		{
			if (Function->HasAllFunctionFlags(FUNC_BlueprintCosmetic) || IsCosmeticTickEvent())
			{
				EventHeading = LOCTEXT("ClinetOnlyEvent", "Client Only");
			}
			else if(Function->HasAllFunctionFlags(FUNC_BlueprintAuthorityOnly))
			{
				EventHeading = LOCTEXT("ServerOnlyEvent", "Server Only");
			}
		}
	}

	if (EventHeading.IsEmpty() && IsUsedByAuthorityOnlyDelegate())
	{
		EventHeading = LOCTEXT("ServerOnlyEvent", "Server Only");
	}
	else if(EventHeading.IsEmpty() && (EventSignatureClass != NULL) && EventSignatureClass->IsChildOf(UInterface::StaticClass()))
	{
		EventHeading = LOCTEXT("InterfaceEvent", "Interface Event");
	}

	FText CompleteHeading = Super::GetToolTipHeading();
	if (!CompleteHeading.IsEmpty() && !EventHeading.IsEmpty())
	{
		CompleteHeading = FText::Format(FText::FromString("{0}\n{1}"), EventHeading, CompleteHeading);
	}
	else if (!EventHeading.IsEmpty())
	{
		CompleteHeading = EventHeading;
	}
	return CompleteHeading;
}

bool UK2Node_Event::IsDeprecated() const
{
	if (UFunction* Function = FindField<UFunction>(EventSignatureClass, EventSignatureName))
	{
		return Function->HasMetaData(FBlueprintMetadata::MD_DeprecatedFunction);
	}

	return false;
}

FString UK2Node_Event::GetDeprecationMessage() const
{
	if (UFunction* Function = FindField<UFunction>(EventSignatureClass, EventSignatureName))
	{
		if (Function->HasMetaData(FBlueprintMetadata::MD_DeprecationMessage))
		{
			return FString::Printf(TEXT("%s %s"), *LOCTEXT("EventDeprecated_Warning", "@@ is deprecated;").ToString(), *Function->GetMetaData(FBlueprintMetadata::MD_DeprecationMessage));
		}
	}

	return Super::GetDeprecationMessage();
}

UObject* UK2Node_Event::GetJumpTargetForDoubleClick() const
{
	if(EventSignatureClass != NULL && EventSignatureClass->ClassGeneratedBy != NULL && EventSignatureClass->ClassGeneratedBy->IsA(UBlueprint::StaticClass()))
	{
		UBlueprint* Blueprint = CastChecked<UBlueprint>(EventSignatureClass->ClassGeneratedBy);
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for(auto It(Graphs.CreateConstIterator()); It; It++)
		{
			if((*It)->GetFName() == EventSignatureName)
			{
				return *It;
			}
		}
	}

	return NULL;
}

#undef LOCTEXT_NAMESPACE