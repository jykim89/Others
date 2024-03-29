// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	KismetCompiler.cpp
=============================================================================*/

#include "KismetCompilerPrivatePCH.h"
#include "KismetCompilerBackend.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetDebugUtilities.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetReinstanceUtilities.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Editor/UnrealEd/Public/ScriptDisassembler.h"
#include "K2Node_PlayMovieScene.h"
#include "RuntimeMovieScenePlayer.h"
#include "MovieSceneBindings.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "UserDefinedStructureCompilerUtils.h"

static bool bDebugPropertyPropagation = false;


#define LOCTEXT_NAMESPACE "KismetCompiler"

//////////////////////////////////////////////////////////////////////////
// Stats for this module


DEFINE_STAT(EKismetCompilerStats_CompileTime);
DEFINE_STAT(EKismetCompilerStats_CreateSchema);
DEFINE_STAT(EKismetCompilerStats_ReplaceGraphRefsToGeneratedClass);
DEFINE_STAT(EKismetCompilerStats_CreateFunctionList);
DEFINE_STAT(EKismetCompilerStats_Expansion);
DEFINE_STAT(EKismetCompilerStats_ProcessUbergraph);
DEFINE_STAT(EKismetCompilerStats_ProcessFunctionGraph);
DEFINE_STAT(EKismetCompilerStats_PrecompileFunction);
DEFINE_STAT(EKismetCompilerStats_CompileFunction);
DEFINE_STAT(EKismetCompilerStats_PostcompileFunction);
DEFINE_STAT(EKismetCompilerStats_FinalizationWork);
DEFINE_STAT(EKismetCompilerStats_CodeGenerationTime);
DEFINE_STAT(EKismetCompilerStats_UpdateBlueprintGeneratedClass);
		
//////////////////////////////////////////////////////////////////////////
// FKismetCompilerContext

FKismetCompilerContext::FKismetCompilerContext(UBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions, TArray<UObject*>* InObjLoaded)
	: FGraphCompilerContext(InMessageLog)
	, Schema(NULL)
	, Blueprint(SourceSketch)
	, NewClass(NULL)
	, ConsolidatedEventGraph(NULL)
	, UbergraphContext(NULL)
	, CompileOptions(InCompilerOptions)
	, ObjLoaded(InObjLoaded)
{
	MacroRowMaxHeight = 0;

	MinimumSpawnX = -2000;
	MaximumSpawnX = 2000;

	AverageNodeWidth = 200;
	AverageNodeHeight = 150;

	HorizontalSectionPadding = 250;
	VerticalSectionPadding = 250;
	HorizontalNodePadding = 40;

	MacroSpawnX = MinimumSpawnX;
	MacroSpawnY = -2000;

	VectorStruct = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("Vector"));
	RotatorStruct = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("Rotator"));
	TransformStruct = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("Transform"));
	LinearColorStruct = FindObjectChecked<UScriptStruct>(UObject::StaticClass(), TEXT("LinearColor"));
}

FKismetCompilerContext::~FKismetCompilerContext()
{
	for (TMap< TSubclassOf<UEdGraphNode>, FNodeHandlingFunctor*>::TIterator It(NodeHandlers); It; ++It)
	{
		FNodeHandlingFunctor* FPtr = It.Value();
		delete FPtr;
	}
	NodeHandlers.Empty();
	DefaultPropertyValueMap.Empty();
}

UEdGraphSchema_K2* FKismetCompilerContext::CreateSchema()
{
	return NewObject<UEdGraphSchema_K2>();
}

void FKismetCompilerContext::EnsureProperGeneratedClass(UClass*& TargetUClass)
{
	if( TargetUClass && !((UObject*)TargetUClass)->IsA(UBlueprintGeneratedClass::StaticClass()) )
	{
		FKismetCompilerUtilities::ConsignToOblivion(TargetUClass, Blueprint->bIsRegeneratingOnLoad);
		TargetUClass = NULL;
	}
}

void FKismetCompilerContext::SpawnNewClass(const FString& NewClassName)
{
	// First, attempt to find the class, in case it hasn't been serialized in yet
	NewClass = FindObject<UBlueprintGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);
	if (NewClass == NULL)
	{
		// If the class hasn't been found, then spawn a new one
		NewClass = ConstructObject<UBlueprintGeneratedClass>(UBlueprintGeneratedClass::StaticClass(), Blueprint->GetOutermost(), FName(*NewClassName), RF_Public|RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked in the Blueprint yet due to load ordering issues
		FBlueprintCompileReinstancer GeneratedClassReinstancer(NewClass);
	}
}

struct FSubobjectCollection
{ 
private:
	TSet<const UObject*> Collection;

public:
	void AddObject(const UObject* const InObject)
	{
		if (InObject)
		{
			Collection.Add(InObject);
			TArray<UObject*> Subobjects;
			GetObjectsWithOuter(InObject, Subobjects, true);
			for (auto SubObject : Subobjects)
			{
				Collection.Add(SubObject);
			}
		}
	}

	template<typename TOBJ>
	void AddObjects(const TArray<TOBJ*>& InObjects)
	{
		for (const auto ObjPtr : InObjects)
		{
			AddObject(ObjPtr);
		}
	}

	bool operator()(const UObject* const RemovalCandidate) const
	{
		return (NULL != Collection.Find(RemovalCandidate));
	}
};

void FKismetCompilerContext::CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& OldCDO)
{
	const bool bRecompilingOnLoad = Blueprint->bIsRegeneratingOnLoad;
	FString TransientClassString = FString::Printf(TEXT("TRASHCLASS_%s"), *Blueprint->GetName());
	FName TransientClassName = MakeUniqueObjectName(GetTransientPackage(), UBlueprintGeneratedClass::StaticClass(), FName(*TransientClassString));
	UClass* TransientClass = ConstructObject<UBlueprintGeneratedClass>(UBlueprintGeneratedClass::StaticClass(), GetTransientPackage(), TransientClassName, RF_Public|RF_Transient);
	
	UClass* ParentClass = Blueprint->ParentClass;
	if( ParentClass == NULL )
	{
		ParentClass = UObject::StaticClass();
	}
	TransientClass->ClassAddReferencedObjects = ParentClass->AddReferencedObjects;
	
	NewClass = ClassToClean;
	OldCDO = ClassToClean->ClassDefaultObject; // we don't need to create the CDO at this point
	
	const ERenameFlags RenFlags = REN_DontCreateRedirectors | (bRecompilingOnLoad ? REN_ForceNoResetLoaders : 0);

	if( OldCDO )
	{
		OldCDO->Rename(NULL, GetTransientPackage(), RenFlags);
		ULinkerLoad::InvalidateExport(OldCDO);
	}

	// Purge all subobjects (properties, functions, params) of the class, as they will be regenerated
	TArray<UObject*> ClassSubObjects;
	GetObjectsWithOuter(ClassToClean, ClassSubObjects, true);

	{	// Save subobjects, that won't be regenerated.
		FSubobjectCollection SubObjectsToSave;
		SubObjectsToSave.AddObjects(Blueprint->ComponentTemplates);
		SubObjectsToSave.AddObjects(Blueprint->Timelines);
		if (Blueprint->SimpleConstructionScript)
		{
			SubObjectsToSave.AddObject(Blueprint->SimpleConstructionScript);
			if (const USCS_Node* DefaultScene = Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode())
			{
				SubObjectsToSave.AddObject(DefaultScene->ComponentTemplate);
			}

			TArray<USCS_Node*> SCSNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
			for (auto SCSNode : SCSNodes)
			{
				SubObjectsToSave.AddObject(SCSNode->ComponentTemplate);
			}
		}
		ClassSubObjects.RemoveAllSwap(SubObjectsToSave);
	}

	for( auto SubObjIt = ClassSubObjects.CreateIterator(); SubObjIt; ++SubObjIt )
	{
		UObject* CurrSubObj = *SubObjIt;
		CurrSubObj->Rename(NULL, TransientClass, RenFlags);
		if( UProperty* Prop = Cast<UProperty>(CurrSubObj) )
		{
			FKismetCompilerUtilities::InvalidatePropertyExport(Prop);
		}
		else
		{
			ULinkerLoad::InvalidateExport(CurrSubObj);
		}
	}

	// Purge the class to get it back to a "base" state
	ClassToClean->PurgeClass(bRecompilingOnLoad);

	// Set properties we need to regenerate the class with
	ClassToClean->PropertyLink = ParentClass->PropertyLink;
	ClassToClean->ClassWithin = ParentClass;
	ClassToClean->ClassConfigName = ClassToClean->HasAnyFlags(RF_Native) ? FName(ClassToClean->StaticConfigName()) : FName(*ParentClass->GetConfigName());
	ClassToClean->DebugData = FBlueprintDebugData();
}

void FKismetCompilerContext::PostCreateSchema()
{
	NodeHandlers.Add(UEdGraphNode_Comment::StaticClass(), new FNodeHandlingFunctor(*this));

	TArray<UClass*> ClassesOfUK2Node;
	GetDerivedClasses(UK2Node::StaticClass(), ClassesOfUK2Node, true);
	for ( auto ClassIt = ClassesOfUK2Node.CreateConstIterator(); ClassIt; ++ClassIt )
	{
		if( !(*ClassIt)->HasAnyClassFlags(CLASS_Abstract) )
		{
			FNodeHandlingFunctor* HandlingFunctor = (*ClassIt)->GetDefaultObject<UK2Node>()->CreateNodeHandler(*this);
			if (HandlingFunctor)
			{
				NodeHandlers.Add(*ClassIt, HandlingFunctor);
			}
		}
	}
}	


/** Validates that the interconnection between two pins is schema compatible */
void FKismetCompilerContext::ValidateLink(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	Super::ValidateLink(PinA, PinB);

	// At this point we can assume the pins are linked, and as such the connection response should not be to disallow
	// @todo: Potentially revisit this later.
	// This API is intended to describe how to handle a potentially new connection to a pin that may already have a connection.
	// However it also checks all necessary constraints for a valid connection to exist. We rely on the fact that the "disallow"
	// response will be returned if the pins are not compatible; any other response here then means that the connection is valid.
	if (Schema->CanCreateConnection(PinA, PinB).Response == CONNECT_RESPONSE_DISALLOW)
	{
		MessageLog.Warning(*LOCTEXT("PinTypeMismatch_Error", "Type mismatch between pins @@ and @@").ToString(), PinA, PinB); 
	}
}

/** Validate that the wiring for a single pin is schema compatible */
void FKismetCompilerContext::ValidatePin(const UEdGraphPin* Pin) const
{
	Super::ValidatePin(Pin);

	// Fixing up references to the skel or the generated classes to be PSC_Self pins
	if ((Pin->PinType.PinCategory == Schema->PC_Object) || (Pin->PinType.PinCategory == Schema->PC_Interface))
	{
		//@todo:  This is modifying the model, but is acceptable to save another prepass on the pins
		UEdGraphPin* MutablePin = const_cast<UEdGraphPin*>(Pin);

		if (NewClass && MutablePin->PinType.PinSubCategoryObject.Get() == NewClass)
		{
			MutablePin->PinType.PinSubCategory = Schema->PSC_Self;
			MutablePin->PinType.PinSubCategoryObject = NULL;
		}
	}

	if (Pin->PinType.PinCategory == Schema->PC_Wildcard)
	{
		// Wildcard pins should never be seen by the compiler; they should always be forced into a particular type by wiring.
		MessageLog.Error(*LOCTEXT("UndeterminedPinType_Error", "The type of @@ is undetermined.  Connect something to @@ to imply a specific type.").ToString(), Pin, Pin->GetOwningNode()); 
	}

	if (Pin->LinkedTo.Num() > 1)
	{
		if (Pin->Direction == EGPD_Output)
		{
			if (Schema->IsExecPin(*Pin))
			{
				// Multiple outputs are not OK, since they don't have a clear defined order of execution
				MessageLog.Error(*LOCTEXT("TooManyOutputPinConnections_Error", "Exec output pin @@ cannot have more than one connection").ToString(), Pin);
			}
		}
		else if (Pin->Direction == EGPD_Input)
		{
			if (Schema->IsExecPin(*Pin))
			{
				// Multiple inputs to an execution wire are ok, it means we get executed from more than one path
			}
			else if( Schema->IsSelfPin(*Pin) )
			{
				// Pure functions and latent functions cannot have more than one self connection
				UK2Node_CallFunction* OwningNode = Cast<UK2Node_CallFunction>(Pin->GetOwningNode());
				if( OwningNode )
				{
					if( OwningNode->IsNodePure() )
					{
						MessageLog.Error(*LOCTEXT("PureFunction_OneSelfPin_Error", "Pure function call node @@ cannot have more than one self pin connection").ToString(), OwningNode);
					}
					else if( OwningNode->IsLatentFunction() )
					{
						MessageLog.Error(*LOCTEXT("LatentFunction_OneSelfPin_Error", "Latent function call node @@ cannot have more than one self pin connection").ToString(), OwningNode);
					}
				}
			}
			else
			{
				MessageLog.Error(*LOCTEXT("InputPin_OneConnection_Error", "Input pin @@ cannot have more than one connection").ToString(), Pin);
			}
		}
		else
		{
			MessageLog.Error(*LOCTEXT("UnexpectedPiNDirection_Error", "Unexpected pin direction encountered on @@").ToString(), Pin);
		}
	}

	//function return node exec pin should be connected to something
	if(Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() == 0 && Schema->IsExecPin(*Pin) )
	{
		if(UK2Node_FunctionResult* OwningNode = Cast<UK2Node_FunctionResult>(Pin->GetOwningNode()))
		{
			MessageLog.Warning(*LOCTEXT("ReturnNodeExecPinUnconnected", "ReturnNode Exec pin has no connections on @@").ToString(), Pin);
		}
	}
}

/** Validates that the node is schema compatible */
void FKismetCompilerContext::ValidateNode(const UEdGraphNode* Node) const
{
	//@TODO: Validate the node type is a known one
	Super::ValidateNode(Node);
}

/** Creates a class variable */
UProperty* FKismetCompilerContext::CreateVariable(const FName VarName, const FEdGraphPinType& VarType)
{
	UProperty* NewProperty = FKismetCompilerUtilities::CreatePropertyOnScope(NewClass, VarName, VarType, NewClass, 0, Schema, MessageLog);
	if (NewProperty != NULL)
	{
		FKismetCompilerUtilities::LinkAddedProperty(NewClass, NewProperty);
	}
	else
	{
		MessageLog.Error(*FString::Printf(*LOCTEXT("VariableInvalidType_Error", "The variable %s declared in @@ has an invalid type %s").ToString(),
			*VarName.ToString(), *UEdGraphSchema_K2::TypeToString(VarType)), Blueprint);
	}

	return NewProperty;
}

/** Determines if a node is pure */
bool FKismetCompilerContext::IsNodePure(const UEdGraphNode* Node) const
{
	if (const UK2Node* K2Node = Cast<const UK2Node>(Node))
	{
		return K2Node->IsNodePure();
	}
	// Only non K2Nodes are comments, which are pure
	ensure(Node->IsA(UEdGraphNode_Comment::StaticClass()));
	return true;
}

void FKismetCompilerContext::ValidateVariableNames()
{
	TSharedPtr<FKismetNameValidator> ParentBPNameValidator;
	if( Blueprint->ParentClass != NULL )
	{
		UBlueprint* ParentBP = Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy);
		if( ParentBP != NULL )
		{
			ParentBPNameValidator = MakeShareable(new FKismetNameValidator(ParentBP));
		}
	}

	if(ParentBPNameValidator.IsValid())
	{
		for (int32 VariableIndex=0; VariableIndex < Blueprint->NewVariables.Num(); ++VariableIndex)
		{
			FBPVariableDescription& Variable = Blueprint->NewVariables[VariableIndex];
			if( ParentBPNameValidator->IsValid(Variable.VarName.ToString()) != EValidatorResult::Ok )
			{
				FName NewVariableName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, Variable.VarName.ToString());
				MessageLog.Warning(*FString::Printf(*LOCTEXT("MemberVariableConflictWarning", "Found a member variable with a conflicting name (%s) - changed to %s.").ToString(), *Variable.VarName.ToString(), *NewVariableName.ToString()));
				FBlueprintEditorUtils::RenameMemberVariable(Blueprint, Variable.VarName, NewVariableName);
			}
		}
	}
}

void FKismetCompilerContext::ValidateTimelineNames()
{
	TSharedPtr<FKismetNameValidator> ParentBPNameValidator;
	if( Blueprint->ParentClass != NULL )
	{
		UBlueprint* ParentBP = Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy);
		if( ParentBP != NULL )
		{
			ParentBPNameValidator = MakeShareable(new FKismetNameValidator(ParentBP));
		}
	}

	for (int32 TimelineIndex=0; TimelineIndex < Blueprint->Timelines.Num(); ++TimelineIndex)
	{
		UTimelineTemplate* TimelineTemplate = Blueprint->Timelines[TimelineIndex];
		if( TimelineTemplate )
		{
			if( ParentBPNameValidator.IsValid() && ParentBPNameValidator->IsValid(TimelineTemplate->GetName()) != EValidatorResult::Ok )
			{
				// Use the viewer displayed Timeline name (without the _Template suffix) because it will be added later for appropriate checks.
				FString TimelineName = UTimelineTemplate::TimelineTemplateNameToVariableName(TimelineTemplate->GetFName());

				FName NewName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, TimelineName);
				MessageLog.Warning(*FString::Printf(*LOCTEXT("TimelineConflictWarning", "Found a timeline with a conflicting name (%s) - changed to %s.").ToString(), *TimelineTemplate->GetName(), *NewName.ToString()));
				FBlueprintEditorUtils::RenameTimeline(Blueprint, FName(*TimelineName), NewName);
			}
		}
	}
}

void FKismetCompilerContext::CreateClassVariablesFromBlueprint()
{
	// Ensure that member variable names are valid and that there are no collisions with a parent class
	ValidateVariableNames();

	// Grab the blueprint variables
	NewClass->NumReplicatedProperties = 0;	// Keep track of how many replicated variables this blueprint adds
	for (int32 i = 0; i < Blueprint->NewVariables.Num(); ++i)
	{
		FBPVariableDescription& Variable = Blueprint->NewVariables[Blueprint->NewVariables.Num() - (i + 1)];

		UProperty* NewProperty = CreateVariable(Variable.VarName, Variable.VarType);
		if (NewProperty != NULL)
		{
			NewProperty->SetPropertyFlags(Variable.PropertyFlags);
			NewProperty->SetMetaData(TEXT("DisplayName"), *Variable.FriendlyName);
			NewProperty->SetMetaData(TEXT("Category"), *Variable.Category.ToString());
			NewProperty->RepNotifyFunc = Variable.RepNotifyFunc;

			if(!Variable.DefaultValue.IsEmpty())
			{
				SetPropertyDefaultValue(NewProperty, Variable.DefaultValue);
			}

			if (NewProperty->HasAnyPropertyFlags(CPF_Net))
			{
				NewClass->NumReplicatedProperties++;
			}

			// Set metadata on property
			for (FBPVariableMetaDataEntry& Entry : Variable.MetaDataArray)
			{
				NewProperty->SetMetaData(Entry.DataKey, *Entry.DataValue);
				if (Entry.DataKey == FBlueprintMetadata::MD_ExposeOnSpawn)
				{
					NewProperty->SetPropertyFlags(CPF_ExposeOnSpawn);
					if (NewProperty->HasAnyPropertyFlags(CPF_DisableEditOnInstance))
					{
						MessageLog.Warning(*FString::Printf(*LOCTEXT("ExposeToSpawnButPrivateWarning", "Variable %s is marked as 'Expose on Spawn' but not marked as 'Editable'; please make it 'Editable'").ToString(), *NewProperty->GetName()));
					}
				}
			}
		}
	}

	// Ensure that timeline names are valid and that there are no collisions with a parent class
	ValidateTimelineNames();

	// Create a class property for each timeline instance contained in the blueprint
	for (int32 TimelineIndex = 0; TimelineIndex < Blueprint->Timelines.Num(); ++TimelineIndex)
	{
		UTimelineTemplate* Timeline = Blueprint->Timelines[TimelineIndex];
		FEdGraphPinType TimelinePinType(Schema->PC_Object, TEXT(""), UTimelineComponent::StaticClass(), false, false);

		// Previously UTimelineComponent object has exactly the same name as UTimelineTemplate object (that obj was in blueprint)
		const FString TimelineVariableName = UTimelineTemplate::TimelineTemplateNameToVariableName(Timeline->GetFName());
		UProperty* TimelineProperty = CreateVariable(*TimelineVariableName, TimelinePinType);
		if (TimelineProperty != NULL)
		{
			TimelineProperty->SetMetaData( TEXT("Category"), *Blueprint->GetName() );
			TimelineProperty->SetPropertyFlags(CPF_BlueprintVisible);

			TimelineToMemberVariableMap.Add(Timeline, TimelineProperty);
		}

		FEdGraphPinType DirectionPinType(Schema->PC_Byte, TEXT(""), FTimeline::GetTimelineDirectionEnum(), false, false);
		CreateVariable(Timeline->GetDirectionPropertyName(), DirectionPinType);

		FEdGraphPinType FloatPinType(Schema->PC_Float, TEXT(""), NULL, false, false);
		for(int32 i=0; i<Timeline->FloatTracks.Num(); i++)
		{
			CreateVariable(Timeline->GetTrackPropertyName(Timeline->FloatTracks[i].TrackName), FloatPinType);
		}

		FEdGraphPinType VectorPinType(Schema->PC_Struct, TEXT(""), VectorStruct, false, false);
		for(int32 i=0; i<Timeline->VectorTracks.Num(); i++)
		{
			CreateVariable(Timeline->GetTrackPropertyName(Timeline->VectorTracks[i].TrackName), VectorPinType);
		}

		FEdGraphPinType LinearColorPinType(Schema->PC_Struct, TEXT(""), LinearColorStruct, false, false);
		for(int32 i=0; i<Timeline->LinearColorTracks.Num(); i++)
		{
			CreateVariable(Timeline->GetTrackPropertyName(Timeline->LinearColorTracks[i].TrackName), LinearColorPinType);
		}
	}

	// Create a class property for any simple-construction-script created components that should be exposed
	if (Blueprint->SimpleConstructionScript != NULL)
	{
		// Ensure that variable names are valid and that there are no collisions with a parent class
		Blueprint->SimpleConstructionScript->ValidateNodeVariableNames(MessageLog);

		TArray<USCS_Node*> AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
		for (int32 NodeIdx=0; NodeIdx<AllNodes.Num(); NodeIdx++)
		{
			USCS_Node* Node = AllNodes[NodeIdx];
			check(Node != NULL);

			FName VarName = Node->GetVariableName();
			if ((VarName != NAME_None) && (Node->ComponentTemplate != NULL))
			{
				FEdGraphPinType Type(Schema->PC_Object, TEXT(""), Node->ComponentTemplate->GetClass(), false, false);
				UProperty* NewProperty = CreateVariable(VarName, Type);
				if (NewProperty != NULL)
				{
					const FString CategoryName = Node->CategoryName != NAME_None ? Node->CategoryName.ToString() : Blueprint->GetName();
					
					NewProperty->SetMetaData(TEXT("Category"), *CategoryName);
					NewProperty->SetPropertyFlags(CPF_BlueprintVisible);
				}
			}
		}
	}
}

void FKismetCompilerContext::CreatePropertiesFromList(UStruct* Scope, UField**& PropertyStorageLocation, TIndirectArray<FBPTerminal>& Terms, uint64 PropertyFlags, bool bPropertiesAreLocal, bool bPropertiesAreParameters)
{
	for (int32 i = 0; i < Terms.Num(); ++i)
	{
		FBPTerminal& Term = Terms[i];

		if(NULL != Term.AssociatedVarProperty)
		{	
			const bool bIsStructMember = (Term.Context && Term.Context->bIsStructContext);
			if(bIsStructMember)
			{
				continue;
			}
			MessageLog.Warning(*FString::Printf(*LOCTEXT("AssociatedVarProperty_Error", "AssociatedVarProperty property overriden %s from @@ type (%s)").ToString(), *Term.Name, *UEdGraphSchema_K2::TypeToString(Term.Type)), Term.Source);
		}

		UProperty* NewProperty = FKismetCompilerUtilities::CreatePropertyOnScope(Scope, FName(*Term.Name), Term.Type, NewClass, PropertyFlags, Schema, MessageLog);
		if (NewProperty != NULL)
		{
			NewProperty->PropertyFlags |= PropertyFlags;

			if (Term.bPassedByReference)
			{
				if (!NewProperty->HasAnyPropertyFlags(CPF_OutParm))
				{
					NewProperty->SetPropertyFlags(CPF_OutParm | CPF_ReferenceParm);
				}
			}

			if (Term.bIsSavePersistent)
			{
				NewProperty->SetPropertyFlags(CPF_SaveGame);
			}

			// Imply read only for input object pointer parameters to a const class
			//@TODO: UCREMOVAL: This should really happen much sooner, and isn't working here
			if (bPropertiesAreParameters && ((PropertyFlags & CPF_OutParm) == 0))
			{
				if (UObjectProperty* ObjProp = Cast<UObjectProperty>(NewProperty))
				{
					UClass* EffectiveClass = NULL;
					if (ObjProp->PropertyClass != NULL)
					{
						EffectiveClass = ObjProp->PropertyClass;
					}
					else if (UClassProperty* ClassProp = Cast<UClassProperty>(ObjProp))
					{
						EffectiveClass = ClassProp->MetaClass;
					}


					if ((EffectiveClass != NULL) && (EffectiveClass->HasAnyClassFlags(CLASS_Const)))
					{
						NewProperty->PropertyFlags |= CPF_ConstParm;
					}
				}
				else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(NewProperty))
				{
					NewProperty->PropertyFlags |= CPF_ReferenceParm;

					// ALWAYS pass array parameters as out params, so they're set up as passed by ref
					if( (PropertyFlags & CPF_Parm) != 0 )
					{
						NewProperty->PropertyFlags |= CPF_OutParm;
					}
				}
			}
			
			// Link this object to the tail of the list (so properties remain in the desired order)
			*PropertyStorageLocation = NewProperty;
			PropertyStorageLocation = &(NewProperty->Next);

			Term.AssociatedVarProperty = NewProperty;
			Term.bIsLocal = bPropertiesAreLocal;

			// Record in the debugging information
			//@TODO: Rename RegisterClassPropertyAssociation, etc..., to better match that indicate it works with locals
			{
				UObject* TrueSourceObject = MessageLog.FindSourceObject(Term.Source);
				NewClass->GetDebugData().RegisterClassPropertyAssociation(TrueSourceObject, NewProperty);
			}

			// Record the desired default value for this, if specified by the term
			if (!Term.PropertyDefault.IsEmpty())
			{
				if (bPropertiesAreParameters)
				{
					const bool bInputParameter = (0 == (PropertyFlags & CPF_OutParm)) && (0 != (PropertyFlags & CPF_Parm));
					if (bInputParameter)
					{
						Scope->SetMetaData(NewProperty->GetFName(), *Term.PropertyDefault);
					}
					else
					{
						MessageLog.Warning(
							*FString::Printf(*LOCTEXT("UnusedDefaultValue_Warn", "Default value for '%s' cannot be used.").ToString(), *NewProperty->GetName()), 
							Term.Source);
					}
				}
				else
				{
					SetPropertyDefaultValue(NewProperty, Term.PropertyDefault);
				}
			}
		}
		else
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("FailedCreateProperty_Error", "Failed to create property %s from @@ due to a bad or unknown type (%s)").ToString(), *Term.Name, *UEdGraphSchema_K2::TypeToString(Term.Type)),
				Term.Source);
		}

	}
}

void FKismetCompilerContext::CreateLocalVariablesForFunction(FKismetFunctionContext& Context)
{
	// Local stack frame (or maybe class for the ubergraph)
	{
		const bool bArePropertiesLocal = true;

		// Pull the local properties generated out of the function, they will be put at the end of the list
		UField* LocalProperties = Context.Function->Children;

		UField** PropertyStorageLocation = &Context.Function->Children;
		CreatePropertiesFromList(Context.Function, PropertyStorageLocation, Context.Parameters, CPF_Parm, bArePropertiesLocal, /*bPropertiesAreParameters=*/ true);
		CreatePropertiesFromList(Context.Function, PropertyStorageLocation, Context.Results, CPF_Parm | CPF_OutParm, bArePropertiesLocal, /*bPropertiesAreParameters=*/ true);
		CreatePropertiesFromList(Context.Function, PropertyStorageLocation, Context.Locals, 0, bArePropertiesLocal, /*bPropertiesAreParameters=*/ true);

		// If there were local properties, place them at the end of the property storage location
		if(LocalProperties)
		{
			*PropertyStorageLocation = LocalProperties;
		}

		// Create debug data for variable reads/writes
		if (Context.bCreateDebugData)
		{
			for (int32 VarAccessIndex = 0; VarAccessIndex < Context.VariableReferences.Num(); ++VarAccessIndex)
			{
				FBPTerminal& Term = Context.VariableReferences[VarAccessIndex];

				if (Term.AssociatedVarProperty != NULL)
				{
					UObject* TrueSourceObject = MessageLog.FindSourceObject(Term.Source);
					NewClass->GetDebugData().RegisterClassPropertyAssociation(TrueSourceObject, Term.AssociatedVarProperty);
				}
			}
		}

		// Fix up the return value
		//@todo:  Is there a better way of doing this without mangling code?
		const FName RetValName = FName(TEXT("ReturnValue"));
		for (TFieldIterator<UProperty> It(Context.Function); It && (It->PropertyFlags & CPF_Parm); ++It)
		{
			UProperty* Property = *It;
			if ((Property->GetFName() == RetValName) && Property->HasAnyPropertyFlags(CPF_OutParm))
			{
				Property->SetPropertyFlags(CPF_ReturnParm);
			}
		}
	}

	// Class
	{
		const bool bArePropertiesLocal = false;
		
		int32 PropertySafetyCounter = 100000;
		UField** PropertyStorageLocation = &(NewClass->Children);
		while (*PropertyStorageLocation != NULL)
		{
			if (--PropertySafetyCounter == 0)
			{
				checkf(false, TEXT("Property chain is corrupted;  The most likely causes are multiple properties with the same name.") );
			}

			PropertyStorageLocation = &((*PropertyStorageLocation)->Next);
		}

		const uint64 UbergraphHiddenVarFlags = CPF_Transient | CPF_DuplicateTransient;
		CreatePropertiesFromList(NewClass, PropertyStorageLocation, Context.EventGraphLocals, UbergraphHiddenVarFlags, bArePropertiesLocal);

		// Handle level actor references
		const uint64 LevelActorReferenceVarFlags = 0/*CPF_Edit*/;
		CreatePropertiesFromList(NewClass, PropertyStorageLocation, Context.LevelActorReferences, LevelActorReferenceVarFlags, false);
	}
}

void FKismetCompilerContext::CreateUserDefinedLocalVariablesForFunction(FKismetFunctionContext& Context, UField**& PropertyStorageLocation)
{
	check(Context.Function->Children == NULL);

	// Create local variables from the Context entry point
	for (int32 i = 0; i < Context.EntryPoint->LocalVariables.Num(); ++i)
	{
		FBPVariableDescription& Variable = Context.EntryPoint->LocalVariables[Context.EntryPoint->LocalVariables.Num() - (i + 1)];

		// Create the property based on the variable description, scoped to the function
		UProperty* NewProperty = FKismetCompilerUtilities::CreatePropertyOnScope(Context.Function, Variable.VarName, Variable.VarType, NewClass, 0, Schema, MessageLog);
		if (NewProperty != NULL)
		{
			// Link this object to the tail of the list (so properties remain in the desired order)
			*PropertyStorageLocation = NewProperty;
			PropertyStorageLocation = &(NewProperty->Next);
		}

		if (NewProperty != NULL)
		{
			NewProperty->SetPropertyFlags(Variable.PropertyFlags);
			NewProperty->SetMetaData(TEXT("FriendlyName"), *Variable.FriendlyName);
			NewProperty->SetMetaData(TEXT("Category"), *Variable.Category.ToString());
			NewProperty->RepNotifyFunc = Variable.RepNotifyFunc;

			if(!Variable.DefaultValue.IsEmpty())
			{
				SetPropertyDefaultValue(NewProperty, Variable.DefaultValue);
			}
		}
	}
}

void FKismetCompilerContext::SetPropertyDefaultValue(const UProperty* PropertyToSet, FString& Value)
{
	DefaultPropertyValueMap.Add(PropertyToSet->GetFName(), Value);
}

/** Copies default values cached for the terms in the DefaultPropertyValueMap to the final CDO */
void FKismetCompilerContext::CopyTermDefaultsToDefaultObject(UObject* DefaultObject)
{
	// Assign all default object values from the map to the new CDO
	for( TMap<FName, FString>::TIterator PropIt(DefaultPropertyValueMap); PropIt; ++PropIt )
	{
		FName TargetPropName = PropIt.Key();
		FString Value = PropIt.Value();

		for (TFieldIterator<UProperty> It(DefaultObject->GetClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			UProperty* Property = *It;
			if (Property->GetFName() == TargetPropName)
			{
				const bool bParseSuccedded = FBlueprintEditorUtils::PropertyValueFromString(Property, Value, reinterpret_cast<uint8*>(DefaultObject));
				if(!bParseSuccedded)
				{
					const FString ErrorMessage = *FString::Printf(*LOCTEXT("ParseDefaultValueError", "Can't parse default value '%s' for @@. Property: %s.").ToString(), *Value, *Property->GetName());
					UObject* InstigatorObject = NewClass->GetDebugData().FindObjectThatCreatedProperty(Property);
					MessageLog.Warning(*ErrorMessage, InstigatorObject);
				}

				break;
			}
		}			
	}
}

void FKismetCompilerContext::PrintVerboseInfoStruct(UStruct* Struct) const
{
	for (TFieldIterator<UProperty> PropIt(Struct); PropIt; ++PropIt)
	{
		UProperty* Prop = *PropIt;

		MessageLog.Note(*FString::Printf(*LOCTEXT("StructInfo_Note", "  %s named %s at offset %d with size %d [dim = %d] and flags %x").ToString(),
			*Prop->GetClass()->GetDescription(),
			*Prop->GetName(),
			Prop->GetOffset_ForDebug(),
			Prop->ElementSize,
			Prop->ArrayDim,
			Prop->PropertyFlags));
	}
}

void FKismetCompilerContext::PrintVerboseInformation(UClass* Class) const
{
	MessageLog.Note(*FString::Printf(*LOCTEXT("ClassHasMembers_Note", "Class %s has members:").ToString(), *Class->GetName()));
	PrintVerboseInfoStruct(Class);

	for (int32 i = 0; i < FunctionList.Num(); ++i)
	{
		const FKismetFunctionContext& Context = FunctionList[i];

		if (Context.IsValid())
		{
			MessageLog.Note(*FString::Printf(*LOCTEXT("FunctionHasMembers_Note", "Function %s has members:").ToString(), *Context.Function->GetName()));
			PrintVerboseInfoStruct(Context.Function);
		}
		else
		{
			MessageLog.Note(*FString::Printf(*LOCTEXT("FunctionCompileFailed_Note", "Function #%d failed to compile and is not valid.").ToString(), i));
		}
	}
}

void FKismetCompilerContext::CheckConnectionResponse(const FPinConnectionResponse &Response, const UEdGraphNode *Node)
{
	if (!Response.CanSafeConnect())
	{
		MessageLog.Error(*FString::Printf(*LOCTEXT("FailedBuildingConnection_Error", "COMPILER ERROR: failed building connection with '%s' at @@").ToString(), *Response.Message), Node);
	}
}

/**
 * Performs transformations on specific nodes that require it according to the schema
 */
void FKismetCompilerContext::TransformNodes(FKismetFunctionContext& Context)
{
	// Give every node a chance to transform itself
	for (int32 NodeIndex = 0; NodeIndex < Context.SourceGraph->Nodes.Num(); ++NodeIndex)
	{
		UEdGraphNode* Node = Context.SourceGraph->Nodes[NodeIndex];

		if (FNodeHandlingFunctor* Handler = NodeHandlers.FindRef(Node->GetClass()))
		{
			Handler->Transform(Context, Node);
		}
		else
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("UnexpectedNodeType_Error", "Unexpected node type %s encountered at @@").ToString(), *(Node->GetClass()->GetName())), Node);
		}
	}
}


struct FNodeVisitorDownExecWires
{
	TSet<UEdGraphNode*> VisitedNodes;
	UEdGraphSchema_K2* Schema;

	void TouchNode(UEdGraphNode* Node)
	{
	}

	void TraverseNodes(UEdGraphNode* Node)
	{
		VisitedNodes.Add(Node);
		TouchNode(Node);

		// Follow every exec output pin
		for (int32 i = 0; i < Node->Pins.Num(); ++i)
		{
			UEdGraphPin* MyPin = Node->Pins[i];

			if ((MyPin->Direction == EGPD_Output) && (Schema->IsExecPin(*MyPin)))
			{
				for (int32 j = 0; j < MyPin->LinkedTo.Num(); ++j)
				{
					UEdGraphPin* OtherPin = MyPin->LinkedTo[j];
					if( OtherPin )
					{
						UEdGraphNode* OtherNode = OtherPin->GetOwningNode();
						if (!VisitedNodes.Contains(OtherNode))
						{
							TraverseNodes(OtherNode);
						}
					}
				}
			}
		}
	}
};

bool FKismetCompilerContext::CanIgnoreNode(const UEdGraphNode* Node) const
{
	if (const UK2Node* K2Node = Cast<const UK2Node>(Node))
	{
		return K2Node->IsNodeSafeToIgnore();
	}
	return false;
}

bool FKismetCompilerContext::ShouldForceKeepNode(const UEdGraphNode* Node) const
{
	if (Node->IsA(UEdGraphNode_Comment::StaticClass()) && CompileOptions.bSaveIntermediateProducts)
	{
		// Preserve comment nodes when debugging the compiler
		return true;
	}
	else
	{
		return false;
	}
}

/** Prunes any nodes that weren't visited from the graph, printing out a warning */
void FKismetCompilerContext::PruneIsolatedNodes(const TArray<UEdGraphNode*>& RootSet, TArray<UEdGraphNode*>& GraphNodes)
{
	//@TODO: This function crawls the graph twice (once here and once in Super, could potentially combine them, with a bitflag for flows reached via exec wires)

	// Prune the impure nodes that aren't reachable via any (even impossible, e.g., a branch never taken) execution flow
	FNodeVisitorDownExecWires Visitor;
	Visitor.Schema = Schema;

	for (TArray<UEdGraphNode*>::TConstIterator It(RootSet); It; ++It)
	{
		UEdGraphNode* RootNode = *It;
		Visitor.TraverseNodes(RootNode);
	}

	for (int32 NodeIndex = 0; NodeIndex < GraphNodes.Num(); ++NodeIndex)
	{
		UEdGraphNode* Node = GraphNodes[NodeIndex];
		if (!Visitor.VisitedNodes.Contains(Node) && !IsNodePure(Node))
		{
			if (!CanIgnoreNode(Node))
			{
				// Disabled this warning, because having orphaned chains is standard workflow for LDs
				//MessageLog.Warning(TEXT("Node @@ will never be executed and is being pruned"), Node);
			}

			if (!ShouldForceKeepNode(Node))
			{
				Node->BreakAllNodeLinks();
				GraphNodes.RemoveAtSwap(NodeIndex);
				--NodeIndex;
			}
		}
	}

	// Prune the nodes that aren't even reachable via data dependencies
	Super::PruneIsolatedNodes(RootSet, GraphNodes);
}

/**
 *	Checks if self pins are connected.
 */
void FKismetCompilerContext::ValidateSelfPinsInGraph(const UEdGraph* SourceGraph)
{
	check(NULL != Schema);
	for (int32 NodeIndex = 0; NodeIndex < SourceGraph->Nodes.Num(); ++NodeIndex)
	{
		if(const UEdGraphNode* Node = SourceGraph->Nodes[NodeIndex])
		{
			for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
			{
				if (const UEdGraphPin* Pin = Node->Pins[PinIndex])
				{
					if (Schema->IsSelfPin(*Pin) && (Pin->LinkedTo.Num() == 0))
					{
						FEdGraphPinType SelfType;
						SelfType.PinCategory    = Schema->PC_Object;
						SelfType.PinSubCategory = Schema->PSC_Self;

						if (!Schema->ArePinTypesCompatible(SelfType, Pin->PinType, NewClass))
						{
							if (Pin->DefaultObject == NULL)
							{
								FString PinType = Pin->PinType.PinCategory;
								if ((Pin->PinType.PinCategory == Schema->PC_Object)    || 
									(Pin->PinType.PinCategory == Schema->PC_Interface) ||
									(Pin->PinType.PinCategory == Schema->PC_Class))
								{
									if (Pin->PinType.PinSubCategoryObject.IsValid())
									{
										PinType = Pin->PinType.PinSubCategoryObject->GetName();
									}
									else
									{
										PinType = TEXT("");
									}
								}

								FString ErrorMsg;
								if(PinType.IsEmpty())
								{
									ErrorMsg = FString::Printf(*LOCTEXT("PinMustHaveConnection_NoType_Error", "'@@' must have a connection").ToString());
								}
								else
								{
									ErrorMsg = FString::Printf(*LOCTEXT("PinMustHaveConnection_Error", "This blueprint (self) is not a %s, therefore '@@' must have a connection").ToString(), *PinType);	
								}

								MessageLog.Error(*ErrorMsg, Pin);
							}
						}
					}
				}
			}
		}
	}
}

/**
 * First phase of compiling a function graph
 *   - Prunes the 'graph' to only included the connected portion that contains the function entry point 
 *   - Schedules execution of each node based on data dependencies
 *   - Creates a UFunction object containing parameters and local variables (but no script code yet)
 */
void FKismetCompilerContext::PrecompileFunction(FKismetFunctionContext& Context)
{
	SCOPE_CYCLE_COUNTER(EKismetCompilerStats_PrecompileFunction);

	// Find the root node, which will drive everything else
	check(Context.RootSet.Num() == 0);
	FindNodesByClass(Context.SourceGraph, UK2Node_FunctionEntry::StaticClass(), Context.RootSet);

	if (Context.RootSet.Num())
	{
		Context.EntryPoint = CastChecked<UK2Node_FunctionEntry>(Context.RootSet[0]);

		// Make sure there was only one function entry node
		for (int32 i = 1; i < Context.RootSet.Num(); ++i)
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("ExpectedOneFunctionEntry_Error", "Expected only one function entry node in graph @@, but found both @@ and @@").ToString()),
				Context.SourceGraph,
				Context.EntryPoint,
				Context.RootSet[i]);
		}

		// Find any other entry points caused by special nodes
		FindNodesByClass(Context.SourceGraph, UK2Node_Event::StaticClass(), Context.RootSet);
		FindNodesByClass(Context.SourceGraph, UK2Node_Timeline::StaticClass(), Context.RootSet);

		// Find the connected subgraph starting at the root node and prune out unused nodes
		PruneIsolatedNodes(Context.RootSet, Context.SourceGraph->Nodes);

		// Check if self pins are connected after PruneIsolatedNodes, to avoid errors from isolated nodes.
		ValidateSelfPinsInGraph(Context.SourceGraph);

		// Transforms
		TransformNodes(Context);

		//Now we can safely remove automatically added WorldContext pin from static function.
		Context.EntryPoint->RemoveUnnecessaryAutoWorldContext();

		// Create the function stub
		FName NewFunctionName = (Context.EntryPoint->CustomGeneratedFunctionName != NAME_None) ? Context.EntryPoint->CustomGeneratedFunctionName : Context.EntryPoint->SignatureName;
		if(Context.IsDelegateSignature())
		{
			FString Name = NewFunctionName.ToString();
			Name += HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX;
			NewFunctionName = FName(*Name);
		}

		// Determine if this is a new function or if it overrides a parent function
		//@TODO: Does not support multiple overloads for a parent virtual function
		UFunction* ParentFunction = Blueprint->ParentClass->FindFunctionByName(NewFunctionName);

		const FString NewFunctionNameString = NewFunctionName.ToString();
		if (CreatedFunctionNames.Contains(NewFunctionNameString))
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("DuplicateFunctionName_Error", "Found more than one function with the same name %s; second occurance at @@").ToString(), *NewFunctionNameString), Context.EntryPoint);
			return;
		}
		else if(NULL != FindField<UProperty>(NewClass, NewFunctionName))
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("DuplicateFieldName_Error", "Name collision - function and property have the same name - '%s'. @@").ToString(), *NewFunctionNameString), Context.EntryPoint);
			return;
		}
		else
		{
			CreatedFunctionNames.Add(NewFunctionNameString);
		}

		Context.Function = NewNamedObject<UFunction>(NewClass, NewFunctionName, RF_Public);

#if USE_TRANSIENT_SKELETON
		// Propagate down transient settings from the class
		if (NewClass->HasAnyFlags(RF_Transient))
		{
			Context.Function->SetFlags(RF_Transient);
		}
#endif

		Context.Function->SetSuperStruct( ParentFunction );
		Context.Function->RepOffset = MAX_uint16;
		Context.Function->ReturnValueOffset = MAX_uint16;
		Context.Function->FirstPropertyToInit = NULL;

		// Set up the function category
		FKismetUserDeclaredFunctionMetadata& FunctionMetaData = Context.EntryPoint->MetaData;
		if( !FunctionMetaData.Category.IsEmpty() )
		{
			Context.Function->SetMetaData(FBlueprintMetadata::MD_FunctionCategory, *FunctionMetaData.Category);
		}
		
		// Link it
		//@TODO: should this be in regular or reverse order?
		Context.Function->Next = Context.NewClass->Children;
		Context.NewClass->Children = Context.Function;

		// Add the function to it's owner class function name -> function map
		Context.NewClass->AddFunctionToFunctionMap(Context.Function);

		//@TODO: Prune pure functions that don't have any consumers

		// Find the execution path (and make sure it has no cycles)
		CreateExecutionSchedule(Context.SourceGraph->Nodes, Context.LinearExecutionList);

		// Create any user defined variables, this must occur before registering nets so that the properties are in place
		UField** PropertyStorageLocation = &(Context.Function->Children);
		CreateUserDefinedLocalVariablesForFunction(Context, PropertyStorageLocation);

		bool bIsPureFunction = true;
		for (int32 NodeIndex = 0; NodeIndex < Context.LinearExecutionList.Num(); ++NodeIndex)
		{
			UEdGraphNode* Node = Context.LinearExecutionList[NodeIndex];

			// Register nets in the schedule
			if (FNodeHandlingFunctor* Handler = NodeHandlers.FindRef(Node->GetClass()))
			{
				Handler->RegisterNets(Context, Node);
			}
			else
			{
				MessageLog.Error(*FString::Printf(*LOCTEXT("UnexpectedNodeType_Error", "Unexpected node type %s encountered at @@").ToString(), *(Node->GetClass()->GetName())), Node);
			}
		}
	
		// Create variable declarations
		CreateLocalVariablesForFunction(Context);

		//Validate AccessSpecifier
		const uint32 AccessSpecifierFlag = FUNC_AccessSpecifiers & Context.EntryPoint->ExtraFlags;
		const bool bAcceptedAccessSpecifier = 
			(0 == AccessSpecifierFlag) || (FUNC_Public == AccessSpecifierFlag) || (FUNC_Protected == AccessSpecifierFlag) || (FUNC_Private == AccessSpecifierFlag);
		if(!bAcceptedAccessSpecifier)
		{
			MessageLog.Warning(*LOCTEXT("WrongAccessSpecifier_Error", "Wrong access specifier @@").ToString(), Context.EntryPoint);
		}

		Context.Function->FunctionFlags |= Context.GetNetFlags();

		// Make sure the function signature is valid if this is an override
		if (ParentFunction)
		{
			// Verify the signature
			if (!ParentFunction->IsSignatureCompatibleWith(Context.Function))
			{
				FString SignatureClassName("");
				if (Context.EntryPoint && Context.EntryPoint->SignatureClass)
				{
					SignatureClassName = Context.EntryPoint->SignatureClass->GetName();
				}
				MessageLog.Error(*FString::Printf(*LOCTEXT("OverrideFunctionDifferentSignature_Error", "Cannot override '%s::%s' at @@ which was declared in a parent with a different signature").ToString(), *SignatureClassName, *NewFunctionNameString), Context.EntryPoint);
			}
			const bool bEmptyCase = (0 == AccessSpecifierFlag);
			const bool bDifferentAccessSpecifiers = AccessSpecifierFlag != (ParentFunction->FunctionFlags & FUNC_AccessSpecifiers);
			if( !bEmptyCase && bDifferentAccessSpecifiers )
			{
				MessageLog.Warning(*LOCTEXT("IncompatibleAccessSpecifier_Error", "Access specifier is not compatible the parent function @@").ToString(), Context.EntryPoint);
			}

			uint32 const ParentNetFlags = (ParentFunction->FunctionFlags & FUNC_NetFuncFlags);
			if (ParentNetFlags != Context.GetNetFlags())
			{
				MessageLog.Error(*LOCTEXT("MismatchedNetFlags_Error", "@@ function's net flags don't match parent function's flags").ToString(), Context.EntryPoint);
				
				// clear the existing net flags
				Context.Function->FunctionFlags &= ~(FUNC_NetFuncFlags);
				// have to replace with the parent's net flags, or this will   
				// trigger an assert in Link()
				Context.Function->FunctionFlags |= ParentNetFlags;
			}
		}

		////////////////////////////////////////

		if(Context.IsDelegateSignature())
		{
			Context.Function->FunctionFlags |= FUNC_Delegate;
			UMulticastDelegateProperty* Property = FindObject<UMulticastDelegateProperty>(Context.NewClass, *Context.DelegateSignatureName.ToString());
			if(Property)
			{
				Property->SignatureFunction = Context.Function;
			}
			else
			{
				MessageLog.Warning(*LOCTEXT("NoDelegateProperty_Error", "No delegate property found for '%s'").ToString(), *Context.SourceGraph->GetName());
			}
		}

	}
	else
	{
		MessageLog.Error(*LOCTEXT("NoRootNodeFound_Error", "Could not find a root node for the graph @@").ToString(), Context.SourceGraph);
	}
}

/** Inserts a new item into an array in a sorted position; using an externally stored sort index map */
template<typename DataType, typename SortKeyType>
void OrderedInsertIntoArray(TArray<DataType>& Array, const TMap<DataType, SortKeyType>& SortKeyMap, const DataType& NewItem)
{
	const SortKeyType NewItemKey = SortKeyMap.FindChecked(NewItem);

	for (int32 i = 0; i < Array.Num(); ++i)
	{
		DataType& TestItem = Array[i];
		const SortKeyType TestItemKey = SortKeyMap.FindChecked(TestItem);

		if (TestItemKey > NewItemKey)
		{
			Array.Insert(NewItem, i);
			return;
		}
	}

	Array.Add(NewItem);
}

/**
 * Second phase of compiling a function graph
 *   - Generates executable code and performs final validation
 */
void FKismetCompilerContext::CompileFunction(FKismetFunctionContext& Context)
{
	SCOPE_CYCLE_COUNTER(EKismetCompilerStats_CompileFunction);
	check(Context.IsValid());

	// Generate statements for each node in the linear execution order (which should roughly correspond to the final execution order)
	TMap<UEdGraphNode*, int32> SortKeyMap;
	int32 NumNodesAtStart = Context.LinearExecutionList.Num();
	for (int32 i = 0; i < Context.LinearExecutionList.Num(); ++i)
	{
		UEdGraphNode* Node = Context.LinearExecutionList[i];
		SortKeyMap.Add(Node, i);

		const FString NodeComment = Node->NodeComment.IsEmpty() ? Node->GetName() : Node->NodeComment;

		// Debug comments
		if (KismetCompilerDebugOptions::EmitNodeComments)
		{
			FBlueprintCompiledStatement& Statement = Context.AppendStatementForNode(Node);
			Statement.Type = KCST_Comment;
			Statement.Comment = NodeComment;
		}

		// Debug opcode insertion point
		if (Context.bCreateDebugData && !IsNodePure(Node))
		{
			bool bEmitDebuggingSite = true;

			if (Context.IsEventGraph() && (Node->IsA(UK2Node_FunctionEntry::StaticClass())))
			{
				// The entry point in the ubergraph is a non-visual construct, and will lead to some
				// other 'fake' entry point such as an event or latent action.  Therefore, don't create
				// debug data for the behind-the-scenes entry point, only for the user-visible ones.
				bEmitDebuggingSite = false;
			}

			if (bEmitDebuggingSite)
			{
				FBlueprintCompiledStatement& Statement = Context.AppendStatementForNode(Node);
				Statement.Type = KCST_DebugSite;
				Statement.Comment = NodeComment;
			}
		}

		// Let the node handlers try to compile it
		if (FNodeHandlingFunctor* Handler = NodeHandlers.FindRef(Node->GetClass()))
		{
			Handler->Compile(Context, Node);
		}
		else
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("UnexpectedNodeTypeWhenCompilingFunc_Error", "Unexpected node type %s encountered in execution chain at @@").ToString(), *(Node->GetClass()->GetName())), Node);
		}
	}
	
	// The LinearExecutionList should be immutable at this point
	check(Context.LinearExecutionList.Num() == NumNodesAtStart);

	// Now pull out pure chains and inline their generated code into the nodes that need it
	TMap< UEdGraphNode*, TSet<UEdGraphNode*> > PureNodesNeeded;
	
	for (int32 TestIndex = 0; TestIndex < Context.LinearExecutionList.Num(); )
	{
		UEdGraphNode* Node = Context.LinearExecutionList[TestIndex];

		// List of pure nodes this node depends on.
		bool bHasAntecedentPureNodes = PureNodesNeeded.Contains(Node);

		if (IsNodePure(Node))
		{
			// Push this node to the requirements list of any other nodes using it's outputs, if this node had any real impact
			if (Context.DidNodeGenerateCode(Node) || bHasAntecedentPureNodes)
			{
				for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
				{
					UEdGraphPin* Pin = Node->Pins[PinIndex];
					if (Pin->Direction == EGPD_Output)
					{
						for (int32 OutputIndex = 0; OutputIndex < Pin->LinkedTo.Num(); ++OutputIndex)
						{
							UEdGraphNode* NodeUsingOutput = Pin->LinkedTo[OutputIndex]->GetOwningNode();
							if (NodeUsingOutput != NULL)
							{
								// Add this node, as well as other nodes this node depends on
								TSet<UEdGraphNode*>& TargetNodesRequired = PureNodesNeeded.FindOrAdd(NodeUsingOutput);
								TargetNodesRequired.Add(Node);
								if (bHasAntecedentPureNodes)
								{
									TargetNodesRequired.Append(PureNodesNeeded.FindChecked(Node));
								}
							}
						}
					}
				}
			}

			// Remove it from the linear execution list; the dependent nodes will inline the code when necessary
			Context.LinearExecutionList.RemoveAt(TestIndex);
		}
		else
		{
			if (bHasAntecedentPureNodes)
			{
				// This node requires the output of one or more pure nodes, so that pure code needs to execute at this node

				// Sort the nodes by execution order index
				TSet<UEdGraphNode*>& AntecedentPureNodes = PureNodesNeeded.FindChecked(Node);
				TArray<UEdGraphNode*> SortedPureNodes;
				for (TSet<UEdGraphNode*>::TIterator It(AntecedentPureNodes); It; ++It)
				{
					OrderedInsertIntoArray(SortedPureNodes, SortKeyMap, *It);
				}

				// Inline their code
				for (int32 i = 0; i < SortedPureNodes.Num(); ++i)
				{
					UEdGraphNode* NodeToInline = SortedPureNodes[SortedPureNodes.Num() - 1 - i];

					Context.CopyAndPrependStatements(Node, NodeToInline);
				}
			}

			// Proceed to the next node
			++TestIndex;
		}
	}
}

/**
 * Final phase of compiling a function graph; called after all functions have had CompileFunction called
 *   - Patches up cross-references, etc..., and performs final validation
 */
void FKismetCompilerContext::PostcompileFunction(FKismetFunctionContext& Context)
{
	SCOPE_CYCLE_COUNTER(EKismetCompilerStats_PostcompileFunction);

	// Sort the 'linear execution list' again by likely execution order.
	Context.FinalSortLinearExecList();

	// Resolve goto links
	Context.ResolveGotoFixups();

	//@TODO: Code generation (should probably call backend here, not later)

	// Seal the function, it's done!
	FinishCompilingFunction(Context);
}

/**
 * Handles final post-compilation setup, flags, creates cached values that would normally be set during deserialization, etc...
 */
void FKismetCompilerContext::FinishCompilingFunction(FKismetFunctionContext& Context)
{
	UFunction* Function = Context.Function;
	Function->Bind();
	Function->StaticLink(true);

	// Set the required function flags
	if (Context.CanBeCalledByKismet())
	{
		Function->FunctionFlags |= FUNC_BlueprintCallable;
	}

	if (Context.IsInterfaceStub())
	{
		Function->FunctionFlags |= FUNC_BlueprintEvent;
	}

	// Inherit extra flags from the entry node
	if (Context.EntryPoint)
	{
		Function->FunctionFlags |= Context.EntryPoint->ExtraFlags;
	}

	// First try to get the overriden function from the super class
	UFunction* OverridenFunction = Function->GetSuperFunction();
	// If we couldn't find it, see if we can find an interface class in our inheritance to get it from
	if (!OverridenFunction && Context.Blueprint)
	{
		bool bInvalidInterface = false;
		OverridenFunction = FBlueprintEditorUtils::FindFunctionInImplementedInterfaces( Context.Blueprint, Function->GetFName(), &bInvalidInterface );
		if(bInvalidInterface)
			{
				MessageLog.Warning(TEXT("Blueprint tried to implement invalid interface."));
			}
		}

	// Inherit flags and validate against overridden function if it exists
	if (OverridenFunction)
	{
		Function->FunctionFlags |= (OverridenFunction->FunctionFlags & (FUNC_FuncInherit | FUNC_Public | FUNC_Protected | FUNC_Private));

		if( (Function->FunctionFlags & FUNC_AccessSpecifiers) != (OverridenFunction->FunctionFlags & FUNC_AccessSpecifiers) )
		{
			MessageLog.Error(*LOCTEXT("IncompatibleAccessSpecifier_Error", "Access specifier is not compatible the parent function @@").ToString(), Context.EntryPoint);
		}

		ensure((Function->FunctionFlags & FUNC_FuncOverrideMatch) == (OverridenFunction->FunctionFlags & FUNC_FuncOverrideMatch));

		// Copy metadata from parent function as well
		UMetaData::CopyMetadata(OverridenFunction, Function);
	}
	else
	{
		Function->FunctionFlags |= FUNC_Exec;

		// If this is the root of a blueprint-defined function or event, and if it's public, make it overrideable
		if( !Context.IsEventGraph() && !Function->HasAnyFunctionFlags(FUNC_Private) )
		{
			Function->FunctionFlags |= FUNC_BlueprintEvent;
		}
	}

	// Set function flags and calculate cached values so the class can be used immediately
	Function->ParmsSize = 0;
	Function->NumParms = 0;
	Function->ReturnValueOffset = MAX_uint16;

	for (TFieldIterator<UProperty> PropIt(Function, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
	{
		UProperty* Property = *PropIt;
		if (Property->HasAnyPropertyFlags(CPF_Parm))
		{
			++Function->NumParms;
			Function->ParmsSize = Property->GetOffset_ForUFunction() + Property->GetSize();

			if (Property->HasAnyPropertyFlags(CPF_OutParm))
			{
				Function->FunctionFlags |= FUNC_HasOutParms;
			}

			if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				Function->ReturnValueOffset = Property->GetOffset_ForUFunction();
			}
		}
		else
		{
			if (!Property->HasAnyPropertyFlags(CPF_ZeroConstructor))
			{
				Function->FirstPropertyToInit = Property;
				Function->FunctionFlags |= FUNC_HasDefaults;
				break;
			}
		}
	}

	// Add in any extra user-defined metadata, like tooltip
	UK2Node_FunctionEntry* EntryNode = CastChecked<UK2Node_FunctionEntry>(Context.EntryPoint);
	if (!EntryNode->MetaData.ToolTip.IsEmpty())
	{
		Function->SetMetaData(FBlueprintMetadata::MD_Tooltip, *EntryNode->MetaData.ToolTip);
	}

	if (auto WorldContextPin = EntryNode->GetAutoWorldContextPin())
	{
		Function->SetMetaData(FBlueprintMetadata::MD_DefaultToSelf, *WorldContextPin->PinName); 
		Function->SetMetaData(TEXT("HidePin"), *WorldContextPin->PinName);
	}

	for (int32 EntryPinIndex = 0; EntryPinIndex < EntryNode->Pins.Num(); ++EntryPinIndex)
	{
		UEdGraphPin* EntryPin = EntryNode->Pins[EntryPinIndex];
		// No defaults for object/class pins
		if(	!Schema->IsMetaPin(*EntryPin) && 
			(EntryPin->PinType.PinCategory != Schema->PC_Object) && 
			(EntryPin->PinType.PinCategory != Schema->PC_Class)  && 
			(EntryPin->PinType.PinCategory != Schema->PC_Interface) && 
			!EntryPin->DefaultValue.IsEmpty() )
		{
			Function->SetMetaData(*EntryPin->PinName, *EntryPin->DefaultValue);
		}
	}
}

/**
 * Handles adding the implemented interface information to the class
 */
void FKismetCompilerContext::AddInterfacesFromBlueprint(UClass* Class)
{
	// Make sure we actually have some interfaces to implement
	if( Blueprint->ImplementedInterfaces.Num() == 0 )
	{
		return;
	}

	// Iterate over all implemented interfaces, and add them to the class
	for(int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); i++)
	{
		UClass* Interface = Cast<UClass>(Blueprint->ImplementedInterfaces[i].Interface);
		if( Interface )
		{
			// Make sure it's a valid interface
			check(Interface->HasAnyClassFlags(CLASS_Interface));

			//propogate the inheritable ClassFlags
			Class->ClassFlags |= (Interface->ClassFlags) & CLASS_ScriptInherit;

			new (Class->Interfaces) FImplementedInterface(Interface, 0, true);
		}
	}
}

/**
 * Handles final post-compilation setup, flags, creates cached values that would normally be set during deserialization, etc...
 */
void FKismetCompilerContext::FinishCompilingClass(UClass* Class)
{
	UClass* ParentClass = Class->GetSuperClass();

	if (ParentClass != NULL)
	{
		// Propagate the new parent's inheritable class flags
		Class->ReferenceTokenStream.Empty();
		Class->ClassFlags &= ~CLASS_RecompilerClear;
		Class->ClassFlags |= (ParentClass->ClassFlags & CLASS_ScriptInherit);//@TODO: ChangeParentClass had this, but I don't think I want it: | UClass::StaticClassFlags;  // will end up with CLASS_Intrinsic
		Class->ClassCastFlags |= ParentClass->ClassCastFlags;
		Class->ClassWithin = ParentClass->ClassWithin ? ParentClass->ClassWithin : UObject::StaticClass();
		Class->ClassConfigName = ParentClass->ClassConfigName;

		// Copy the category info from the parent class
#if WITH_EDITORONLY_DATA
		if (ParentClass->HasMetaData(TEXT("HideCategories")))
		{
			Class->SetMetaData(TEXT("HideCategories"), *ParentClass->GetMetaData("HideCategories"));
		}
		if (ParentClass->HasMetaData(TEXT("ShowCategories")))
		{
			Class->SetMetaData(TEXT("ShowCategories"), *ParentClass->GetMetaData("ShowCategories"));
		}
		if (ParentClass->HasMetaData(TEXT("HideFunctions")))
		{
			Class->SetMetaData(TEXT("HideFunctions"), *ParentClass->GetMetaData("HideFunctions"));
		}
		if (ParentClass->HasMetaData(TEXT("AutoExpandCategories")))
		{
			Class->SetMetaData(TEXT("AutoExpandCategories"), *ParentClass->GetMetaData("AutoExpandCategories"));
		}
		if (ParentClass->HasMetaData(TEXT("AutoCollapseCategories")))
		{
			Class->SetMetaData(TEXT("AutoCollapseCategories"), *ParentClass->GetMetaData("AutoCollapseCategories"));
		}
#endif

		// Add in additional flags implied by the blueprint
		switch (Blueprint->BlueprintType)
		{
			case BPTYPE_MacroLibrary:
				Class->ClassFlags |= CLASS_Abstract | CLASS_NotPlaceable;
				break;
			case BPTYPE_Const:
				Class->ClassFlags |= CLASS_Const;
				break;
		}

		//@TODO: Might want to be able to specify some of these here too
	}

	// Add in any other needed flags
	Class->ClassFlags |= (CLASS_Parsed | CLASS_CompiledFromBlueprint);

	// Look for OnRep 
	for( TFieldIterator<UProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		UProperty *Property = *It;
		if (Property->HasAnyPropertyFlags(CPF_Net))
		{
			// Verify rep notifies are valid, if not, clear them
			if (Property->HasAnyPropertyFlags(CPF_RepNotify))
			{
				UFunction * OnRepFunc = Class->FindFunctionByName(Property->RepNotifyFunc);
				if( OnRepFunc != NULL && OnRepFunc->NumParms == 0 && OnRepFunc->GetReturnProperty() == NULL )
				{
					// This function is good so just continue
					continue;
				}
				// Invalid function for RepNotify! clear the flag
				Property->RepNotifyFunc = NAME_None;
			}
		}
	}

	// Set class metadata as needed
	if (FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint))
	{
		NewClass->ClassFlags |= CLASS_Interface;
	}

	{
		UBlueprintGeneratedClass* BPGClass = Cast<UBlueprintGeneratedClass>(Class);
		check(BPGClass);

		BPGClass->ComponentTemplates.Empty();
		BPGClass->Timelines.Empty();
		BPGClass->SimpleConstructionScript = NULL;

		BPGClass->ComponentTemplates = Blueprint->ComponentTemplates;
		BPGClass->Timelines = Blueprint->Timelines;
		BPGClass->SimpleConstructionScript = Blueprint->SimpleConstructionScript;
	}

	//@TODO: Not sure if doing this again is actually necessary
	// It will be if locals get promoted to class scope during function compilation, but that should ideally happen during Precompile or similar
	Class->Bind();
	Class->StaticLink(true);

	// Create the default object for this class
	FKismetCompilerUtilities::CompileDefaultProperties(Class);
}

void FKismetCompilerContext::BuildDynamicBindingObjects(UBlueprintGeneratedClass* Class)
{
	Class->DynamicBindingObjects.Empty();

	for (int NodeIndex = 0; NodeIndex < ConsolidatedEventGraph->Nodes.Num(); ++NodeIndex)
	{
		UK2Node* Node = Cast<UK2Node>(ConsolidatedEventGraph->Nodes[NodeIndex]);

		if (Node)
		{
			UClass* DynamicBindingClass = Node->GetDynamicBindingClass();

			if (DynamicBindingClass)
			{
				UDynamicBlueprintBinding* DynamicBindingObject = Class->GetDynamicBindingObject(DynamicBindingClass);
				if (DynamicBindingObject == NULL)
				{
					DynamicBindingObject = ConstructObject<UDynamicBlueprintBinding>(DynamicBindingClass, Class);
					Class->DynamicBindingObjects.Add(DynamicBindingObject);
				}
				Node->RegisterDynamicBinding(DynamicBindingObject);
			}
		}
	}
}

/**
 * Helper function to create event node for a given pin on a timeline node.
 * @param TimelineNode	The timeline node to create the node event for
 * @param SourceGraph	The source graph to create the event node in
 * @param FunctionName	The function to use as the custom function for the event node
 * @param PinName		The pin name to redirect output from, into the pin of the node event
 * @param ExecFuncName	The event signature name that the event node implements
 */ 
void FKismetCompilerContext::CreatePinEventNodeForTimelineFunction(UK2Node_Timeline* TimelineNode, UEdGraph* SourceGraph, FName FunctionName, const FString& PinName, FName ExecFuncName)
{
	UK2Node_Event* TimelineEventNode = SpawnIntermediateNode<UK2Node_Event>(TimelineNode, SourceGraph);
	TimelineEventNode->EventSignatureName = ExecFuncName;
	TimelineEventNode->EventSignatureClass = UTimelineComponent::StaticClass();
	TimelineEventNode->CustomFunctionName = FunctionName; // Make sure we name this function the thing we are expecting
	TimelineEventNode->bInternalEvent = true;
	TimelineEventNode->AllocateDefaultPins();

	// Move any links from 'update' pin to the 'update event' node
	UEdGraphPin* UpdatePin = TimelineNode->FindPin(PinName);
	check(UpdatePin);

	UEdGraphPin* UpdateOutput = Schema->FindExecutionPin(*TimelineEventNode, EGPD_Output);

	if(UpdatePin != NULL && UpdateOutput != NULL)
	{
		MovePinLinksToIntermediate(*UpdatePin, *UpdateOutput);
	}
}

UK2Node_CallFunction* FKismetCompilerContext::CreateCallTimelineFunction(UK2Node_Timeline* TimelineNode, UEdGraph* SourceGraph, FName FunctionName, UEdGraphPin* TimelineVarPin, UEdGraphPin* TimelineFunctionPin)
{
	// Create 'call play' node
	UK2Node_CallFunction* CallNode = SpawnIntermediateNode<UK2Node_CallFunction>(TimelineNode, SourceGraph);
	CallNode->FunctionReference.SetExternalMember(FunctionName, UTimelineComponent::StaticClass());
	CallNode->AllocateDefaultPins();

	// Wire 'get timeline' to 'self' pin of function call
	UEdGraphPin* CallSelfPin = CallNode->FindPinChecked(Schema->PN_Self);
	TimelineVarPin->MakeLinkTo(CallSelfPin);

	// Move any exec links from 'play' pin to the 'call play' node
	UEdGraphPin* CallExecInput = Schema->FindExecutionPin(*CallNode, EGPD_Input);
	MovePinLinksToIntermediate(*TimelineFunctionPin, *CallExecInput);
	return CallNode;
}

/** Expand timeline nodes into necessary nodes */
void FKismetCompilerContext::ExpandTimelineNodes(UEdGraph* SourceGraph)
{
	for (int32 ChildIndex = 0; ChildIndex < SourceGraph->Nodes.Num(); ++ChildIndex)
	{
		UK2Node_Timeline* TimelineNode = Cast<UK2Node_Timeline>( SourceGraph->Nodes[ChildIndex] );
		if (TimelineNode != NULL)
		{
			UTimelineTemplate* Timeline = Blueprint->FindTimelineTemplateByVariableName(TimelineNode->TimelineName);
			if (Timeline != NULL)
			{
				FString TimelineNameString = TimelineNode->TimelineName.ToString();

				UEdGraphPin* PlayPin = TimelineNode->GetPlayPin();
				bool bPlayPinConnected = (PlayPin->LinkedTo.Num() > 0);

				UEdGraphPin* PlayFromStartPin = TimelineNode->GetPlayFromStartPin();
				bool bPlayFromStartPinConnected = (PlayFromStartPin->LinkedTo.Num() > 0);

				UEdGraphPin* StopPin = TimelineNode->GetStopPin();
				bool bStopPinConnected = (StopPin->LinkedTo.Num() > 0);

				UEdGraphPin* ReversePin = TimelineNode->GetReversePin();
				bool bReversePinConnected = (ReversePin->LinkedTo.Num() > 0);

				UEdGraphPin* ReverseFromEndPin = TimelineNode->GetReverseFromEndPin();
				bool bReverseFromEndPinConnected = (ReverseFromEndPin->LinkedTo.Num() > 0);

				UEdGraphPin* SetTimePin = TimelineNode->GetSetNewTimePin();
				bool bSetNewTimePinConnected = (SetTimePin->LinkedTo.Num() > 0);

				// Only create nodes for play/stop if they are actually connected - otherwise we get a 'unused node being pruned' warning
				if(bPlayPinConnected || bPlayFromStartPinConnected || bStopPinConnected || bReversePinConnected || bReverseFromEndPinConnected || bSetNewTimePinConnected)
				{
					// First create 'get var' node to get the timeline object
					UK2Node_VariableGet* GetTimelineNode = SpawnIntermediateNode<UK2Node_VariableGet>(TimelineNode, SourceGraph);
					GetTimelineNode->VariableReference.SetSelfMember(TimelineNode->TimelineName);
					GetTimelineNode->AllocateDefaultPins();

					// Debug data: Associate the timeline node instance with the property that was created earlier
					UProperty* AssociatedTimelineInstanceProperty = TimelineToMemberVariableMap.FindChecked(Timeline);
					if (AssociatedTimelineInstanceProperty != NULL)
					{
						UObject* TrueSourceObject = MessageLog.FindSourceObject(TimelineNode);
						NewClass->GetDebugData().RegisterClassPropertyAssociation(TrueSourceObject, AssociatedTimelineInstanceProperty);
					}

					// Get the variable output pin
					UEdGraphPin* TimelineVarPin = GetTimelineNode->FindPin(TimelineNameString);

					// This might fail if this is the first compile after adding the timeline (property doesn't exist yet) - in that case, manually add the output pin
					if(TimelineVarPin == NULL)
					{
						TimelineVarPin = GetTimelineNode->CreatePin(EGPD_Output, Schema->PC_Object, TEXT(""), UTimelineComponent::StaticClass(), false, false, TimelineNode->TimelineName.ToString());
					}

					if(bPlayPinConnected)
					{
						static FName PlayName(GET_FUNCTION_NAME_CHECKED(UTimelineComponent, Play));
						CreateCallTimelineFunction(TimelineNode, SourceGraph, PlayName, TimelineVarPin, PlayPin);
					}

					if(bPlayFromStartPinConnected)
					{
						static FName PlayFromStartName(GET_FUNCTION_NAME_CHECKED(UTimelineComponent, PlayFromStart));
						CreateCallTimelineFunction(TimelineNode, SourceGraph, PlayFromStartName, TimelineVarPin, PlayFromStartPin);
					}

					if(bStopPinConnected)
					{
						static FName StopName(GET_FUNCTION_NAME_CHECKED(UTimelineComponent, Stop));
						CreateCallTimelineFunction(TimelineNode, SourceGraph, StopName, TimelineVarPin, StopPin);
					}

					if (bReversePinConnected)
					{
						static FName ReverseName(GET_FUNCTION_NAME_CHECKED(UTimelineComponent, Reverse));
						CreateCallTimelineFunction(TimelineNode, SourceGraph, ReverseName, TimelineVarPin, ReversePin);
					}

					if (bReverseFromEndPinConnected)
					{
						static FName ReverseFromEndName(GET_FUNCTION_NAME_CHECKED(UTimelineComponent, ReverseFromEnd));
						CreateCallTimelineFunction(TimelineNode, SourceGraph, ReverseFromEndName, TimelineVarPin, ReverseFromEndPin);
					}

					if (bSetNewTimePinConnected)
					{
						UEdGraphPin* NewTimePin = TimelineNode->GetNewTimePin();

						static FName SetNewTimeName(GET_FUNCTION_NAME_CHECKED(UTimelineComponent, SetNewTime));
						UK2Node_CallFunction* CallNode = CreateCallTimelineFunction(TimelineNode, SourceGraph, SetNewTimeName, TimelineVarPin, SetTimePin);

						if (CallNode && NewTimePin)
						{
							UEdGraphPin* InputPin = CallNode->FindPinChecked(TEXT("NewTime"));
							MovePinLinksToIntermediate(*NewTimePin, *InputPin);
						}
					}
				}

				// Create event to call on each update
				UFunction* EventSigFunc = UTimelineComponent::GetTimelineEventSignature();

				// Create event nodes for any event tracks
				for(int32 EventTrackIdx=0; EventTrackIdx<Timeline->EventTracks.Num(); EventTrackIdx++)
				{
					FName EventTrackName =  Timeline->EventTracks[EventTrackIdx].TrackName;
					CreatePinEventNodeForTimelineFunction(TimelineNode, SourceGraph, Timeline->GetEventTrackFunctionName(EventTrackIdx), EventTrackName.ToString(), EventSigFunc->GetFName());
				}

				// Generate Update Pin Event Node
				CreatePinEventNodeForTimelineFunction(TimelineNode, SourceGraph, Timeline->GetUpdateFunctionName(), TEXT("Update"), EventSigFunc->GetFName());

				// Generate Finished Pin Event Node
				CreatePinEventNodeForTimelineFunction(TimelineNode, SourceGraph, Timeline->GetFinishedFunctionName(), TEXT("Finished"), EventSigFunc->GetFName());
			}			
		}
	}
}

UEdGraphPin* FKismetCompilerContext::ExpandNodesToAllocateRuntimeMovieScenePlayer( UEdGraph* SourceGraph, UK2Node_PlayMovieScene* PlayMovieSceneNode, ULevel* Level, UK2Node_TemporaryVariable*& OutPlayerVariableNode )
{
	// Call URuntimeMovieScenePlayer::CreateRuntimeMovieScenePlayer() to create a new RuntimeMovieScenePlayer instance
	UK2Node_CallFunction* CreatePlayerCallNode = SpawnIntermediateNode<UK2Node_CallFunction>( PlayMovieSceneNode, SourceGraph );
	{
		CreatePlayerCallNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(URuntimeMovieScenePlayer, CreateRuntimeMovieScenePlayer), URuntimeMovieScenePlayer::StaticClass());
		CreatePlayerCallNode->AllocateDefaultPins();
	}

	// The return value of URuntimeMovieScenePlayer::CreateRuntimeMovieScenePlayer() is the actual MovieScenePlayer we'll be operating with
	UEdGraphPin* CreatePlayerReturnValuePin = CreatePlayerCallNode->GetReturnValuePin();

	// Make a literal for the level and bind it to our function call as a parameter
	UK2Node_Literal* LevelLiteralNode = SpawnIntermediateNode<UK2Node_Literal>( PlayMovieSceneNode, SourceGraph );
	LevelLiteralNode->AllocateDefaultPins();

	// Make a literal for the MovieSceneBindings object and bind it to our function call as a parameter
	UK2Node_Literal* MovieSceneBindingsLiteralNode = SpawnIntermediateNode<UK2Node_Literal>( PlayMovieSceneNode, SourceGraph );
	MovieSceneBindingsLiteralNode->AllocateDefaultPins();


	// Create a local variable to store the URuntimeMovieScenePlayer object instance in
	UK2Node_TemporaryVariable* PlayerVariableNode;
	{
		const bool bIsArray = false;
		PlayerVariableNode = SpawnInternalVariable(
			PlayMovieSceneNode,
			CreatePlayerReturnValuePin->PinType.PinCategory, 
			CreatePlayerReturnValuePin->PinType.PinSubCategory, 
			CreatePlayerReturnValuePin->PinType.PinSubCategoryObject.Get(),
			bIsArray );
	}
	UEdGraphPin* PlayerVariablePin = PlayerVariableNode->GetVariablePin();

	UK2Node_AssignmentStatement* AssignResultToPlayerVariableNode = SpawnIntermediateNode<UK2Node_AssignmentStatement>( PlayMovieSceneNode, SourceGraph );
	AssignResultToPlayerVariableNode->AllocateDefaultPins();

	// Create a node that checks to see if our variable that contains the RuntimeMovieScenePlayer instance is null.
	// If it's null, we'll allocate it now and store it in the variable.
	UK2Node_CallFunction* ComparisonNode = SpawnIntermediateNode<UK2Node_CallFunction>( PlayMovieSceneNode, SourceGraph );
	{
		ComparisonNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_ObjectObject), UKismetMathLibrary::StaticClass());
		ComparisonNode->AllocateDefaultPins();
	}

	UK2Node_IfThenElse* IfVariableNullNode = SpawnIntermediateNode<UK2Node_IfThenElse>( PlayMovieSceneNode, SourceGraph );
	{
		IfVariableNullNode->AllocateDefaultPins();
	}


	// OK, all of our nodes are created.  Now wire everything together!
	{
		// The first thing we'll do is check to see if we've allocated a player yet.  If not, we need to
		// do that now.  So we'll use an "if-then" node to check.

		// Hook the "if-then" node to our comparison function, that simply checks for null
		IfVariableNullNode->GetConditionPin()->MakeLinkTo( ComparisonNode->GetReturnValuePin() );

		// We'll compare the player variable ('A')...
		ComparisonNode->FindPinChecked(TEXT("A"))->MakeLinkTo( PlayerVariablePin );

		// ...against a NULL value ('B')
		ComparisonNode->FindPinChecked(TEXT("B"))->DefaultObject = NULL;

		// If the comparison returns true (variable is null), then we need to call our function to
		// create the player object
		IfVariableNullNode->GetThenPin()->MakeLinkTo( CreatePlayerCallNode->GetExecPin() );

		// Setup function params for "URuntimeMovieScenePlayer::CreateRuntimeMovieScenePlayer()"
		{
			// Our level literal just points to the level object
			LevelLiteralNode->SetObjectRef( Level );

			{
				// Duplicate the bindings and store a copy into the level.  We want the bindings to be
				// outered to the level so they'll be duplicated when the level is duplicated (e.g. for PIE)
				UMovieSceneBindings* NodeMovieSceneBindings = PlayMovieSceneNode->GetMovieSceneBindings();
				UMovieSceneBindings* LevelMovieSceneBindings = NULL;
				if( NodeMovieSceneBindings != NULL && Level != NULL )
				{
					LevelMovieSceneBindings = DuplicateObject( NodeMovieSceneBindings, Level );
					check( LevelMovieSceneBindings != NULL );

					// Tell the Level about the new bindings object.
					Level->AddMovieSceneBindings( LevelMovieSceneBindings );
				}

				MovieSceneBindingsLiteralNode->SetObjectRef( LevelMovieSceneBindings );
			}

			CreatePlayerCallNode->FindPinChecked( TEXT( "Level" ) )->MakeLinkTo( LevelLiteralNode->GetValuePin() );
			CreatePlayerCallNode->FindPinChecked( TEXT( "MovieSceneBindings" ) )->MakeLinkTo( MovieSceneBindingsLiteralNode->GetValuePin() );
		}


		// Our function that creates the player returns the newly-created player object.  We'll
		// store that in our variable
		CreatePlayerCallNode->GetThenPin()->MakeLinkTo( AssignResultToPlayerVariableNode->GetExecPin() );
		AssignResultToPlayerVariableNode->GetVariablePin()->MakeLinkTo( PlayerVariablePin );
		AssignResultToPlayerVariableNode->PinConnectionListChanged( AssignResultToPlayerVariableNode->GetVariablePin() );
		CreatePlayerReturnValuePin->MakeLinkTo( AssignResultToPlayerVariableNode->GetValuePin() );
		AssignResultToPlayerVariableNode->PinConnectionListChanged( AssignResultToPlayerVariableNode->GetValuePin() );
	}

	OutPlayerVariableNode = PlayerVariableNode;
	return IfVariableNullNode->GetExecPin();
}


void FKismetCompilerContext::ExpandPlayMovieSceneNodes( UEdGraph* SourceGraph )
{
	ULevel* Level = NULL;
	if( Blueprint->IsA( ULevelScriptBlueprint::StaticClass() ) )
	{
		ULevelScriptBlueprint* LSB = CastChecked< ULevelScriptBlueprint >( Blueprint );
		Level = LSB->GetLevel();
	}

	// Wipe old MovieSceneBindings on the Level.  We'll recreate them all now.
	if( Level != NULL )
	{
		Level->ClearMovieSceneBindings();
	}

	for (int32 ChildIndex = 0; ChildIndex < SourceGraph->Nodes.Num(); ++ChildIndex)
	{
		UK2Node_PlayMovieScene* PlayMovieSceneNode = Cast<UK2Node_PlayMovieScene>( SourceGraph->Nodes[ChildIndex] );
		if( PlayMovieSceneNode != NULL )
		{
			UEdGraphPin* PlayPin = PlayMovieSceneNode->GetPlayPin();
			const bool bPlayPinConnected = PlayPin->LinkedTo.Num() > 0;
			UEdGraphPin* PausePin = PlayMovieSceneNode->GetPausePin();
			const bool bPausePinConnected = PausePin->LinkedTo.Num() > 0;

			// Do we need to create a MovieScenePlayer?
			const bool bNeedMovieScenePlayer = bPlayPinConnected || bPausePinConnected;
			if( bNeedMovieScenePlayer )
			{
				// Generate a node network to allocate a MovieScenePlayer on demand.  All of the various input exec pins
				// will first be routed through this network, to make sure that we have a movie scene player to work with!
				UK2Node_TemporaryVariable* PlayerVariableNode = NULL;
				UEdGraphPin* AllocateRuntimeMovieScenePlayerExecPin = 
					ExpandNodesToAllocateRuntimeMovieScenePlayer( SourceGraph, PlayMovieSceneNode, Level, PlayerVariableNode );
				UEdGraphPin* PlayerVariablePin = PlayerVariableNode->GetVariablePin();


				// Create a call function node to call 'Play' on the RuntimeMovieScenePlayer object
				if( bPlayPinConnected )
				{
					UK2Node_CallFunction* PlayCallNode = SpawnIntermediateNode<UK2Node_CallFunction>( PlayMovieSceneNode, SourceGraph );
					{
						PlayCallNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(URuntimeMovieScenePlayer, Play), URuntimeMovieScenePlayer::StaticClass());
						PlayCallNode->AllocateDefaultPins();
					}
					
					UK2Node_ExecutionSequence* SequenceNode = SpawnIntermediateNode<UK2Node_ExecutionSequence>( PlayMovieSceneNode, SourceGraph );
					SequenceNode->AllocateDefaultPins();
				
					// Move input links from 'Play' to the exec pin on the Sequence node
					MovePinLinksToIntermediate( *PlayPin, *SequenceNode->GetExecPin() );

					SequenceNode->GetThenPinGivenIndex( 0 )->MakeLinkTo( AllocateRuntimeMovieScenePlayerExecPin );

					// Tell the 'Play' node which player object it's calling the function on
					UEdGraphPin* PlaySelfPin = Schema->FindSelfPin( *PlayCallNode, EGPD_Input );
					PlaySelfPin->MakeLinkTo( PlayerVariablePin );

					// Hook our sequence up to call the function
					UEdGraphPin* PlayExecPin = Schema->FindExecutionPin( *PlayCallNode, EGPD_Input );
					SequenceNode->GetThenPinGivenIndex( 1 )->MakeLinkTo( PlayExecPin );
				}
					
					
				// Create a call function node to call 'Pause' on the RuntimeMovieScenePlayer object
				if( bPausePinConnected )
				{
					UK2Node_CallFunction* PauseCallNode = SpawnIntermediateNode<UK2Node_CallFunction>( PlayMovieSceneNode, SourceGraph );
					{
						PauseCallNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(URuntimeMovieScenePlayer, Pause), URuntimeMovieScenePlayer::StaticClass());
						PauseCallNode->AllocateDefaultPins();
					}

					UK2Node_ExecutionSequence* SequenceNode = SpawnIntermediateNode<UK2Node_ExecutionSequence>( PlayMovieSceneNode, SourceGraph );
					SequenceNode->AllocateDefaultPins();
				
					// Move input links from 'Pause' to the exec pin on the Sequence node
					MovePinLinksToIntermediate( *PausePin, *SequenceNode->GetExecPin() );

					SequenceNode->GetThenPinGivenIndex( 0 )->MakeLinkTo( AllocateRuntimeMovieScenePlayerExecPin );

					// Tell the 'Pause' node which player object it's calling the function on
					UEdGraphPin* PauseSelfPin = Schema->FindSelfPin( *PauseCallNode, EGPD_Input );
					PauseSelfPin->MakeLinkTo( PlayerVariablePin );

					// Hook our sequence up to call the function
					UEdGraphPin* PauseExecPin = Schema->FindExecutionPin( *PauseCallNode, EGPD_Input );
					SequenceNode->GetThenPinGivenIndex( 1 )->MakeLinkTo( PauseExecPin );
				}
			}
		}
	}
}

FPinConnectionResponse FKismetCompilerContext::MovePinLinksToIntermediate(UEdGraphPin& SourcePin, UEdGraphPin& IntermediatePin)
{
	 UEdGraphSchema_K2 const* K2Schema = GetSchema();
	 FPinConnectionResponse ConnectionResult = K2Schema->MovePinLinks(SourcePin, IntermediatePin, true);

	 CheckConnectionResponse(ConnectionResult, SourcePin.GetOwningNode());
	 MessageLog.NotifyIntermediateObjectCreation(&IntermediatePin, &SourcePin);

	 return ConnectionResult;
}

FPinConnectionResponse FKismetCompilerContext::CopyPinLinksToIntermediate(UEdGraphPin& SourcePin, UEdGraphPin& IntermediatePin)
{
	UEdGraphSchema_K2 const* K2Schema = GetSchema();
	FPinConnectionResponse ConnectionResult = K2Schema->CopyPinLinks(SourcePin, IntermediatePin, true);

	CheckConnectionResponse(ConnectionResult, SourcePin.GetOwningNode());
	MessageLog.NotifyIntermediateObjectCreation(&IntermediatePin, &SourcePin);

	return ConnectionResult;
}

UK2Node_TemporaryVariable* FKismetCompilerContext::SpawnInternalVariable(UEdGraphNode* SourceNode, FString Category, FString SubCategory, UObject* SubcategoryObject, bool bIsArray)
{
	UK2Node_TemporaryVariable* Result = SpawnIntermediateNode<UK2Node_TemporaryVariable>(SourceNode);

	Result->VariableType = FEdGraphPinType(Category, SubCategory, SubcategoryObject, bIsArray, false);

	Result->AllocateDefaultPins();

	// Assign the variable source information to the source object as well
	MessageLog.NotifyIntermediateObjectCreation(Result->GetVariablePin(), SourceNode);

	return Result;
}

FName FKismetCompilerContext::GetEventStubFunctionName(UK2Node_Event* SrcEventNode)
{
	FName EventNodeName;

	// If we are overriding a function, we use the exact name for the event node
	if (SrcEventNode->bOverrideFunction)
	{
		EventNodeName = SrcEventNode->EventSignatureName;
	}
	else
	{
		// If not, create a new name
		if (SrcEventNode->CustomFunctionName != NAME_None)
		{
			EventNodeName = SrcEventNode->CustomFunctionName;
		}
		else
		{
			FString EventNodeString = ClassScopeNetNameMap.MakeValidName<UEdGraphNode>(SrcEventNode);
			EventNodeName = FName(*EventNodeString);
		}
	}

	return EventNodeName;
}

void FKismetCompilerContext::CreateFunctionStubForEvent(UK2Node_Event* SrcEventNode, UObject* OwnerOfTemporaries)
{
	FName EventNodeName = GetEventStubFunctionName(SrcEventNode);

	// Create the stub graph and add it to the list of functions to compile

	UEdGraph* ChildStubGraph = NewNamedObject<UEdGraph>(OwnerOfTemporaries, EventNodeName);
	Blueprint->EventGraphs.Add(ChildStubGraph);
	ChildStubGraph->Schema = UEdGraphSchema_K2::StaticClass();
	ChildStubGraph->SetFlags(RF_Transient);
	MessageLog.NotifyIntermediateObjectCreation(ChildStubGraph, SrcEventNode);

	FKismetFunctionContext& StubContext = *new (FunctionList) FKismetFunctionContext(MessageLog, Schema, NewClass, Blueprint);
	StubContext.SourceGraph = ChildStubGraph;

	// A stub graph has no visual representation and is thus not suited to be debugged via the debugger
	StubContext.bCreateDebugData = false;

	StubContext.SourceEventFromStubGraph = SrcEventNode;

	if (SrcEventNode->bOverrideFunction || SrcEventNode->bInternalEvent)
	{
		StubContext.MarkAsInternalOrCppUseOnly();
	}

	if ((SrcEventNode->FunctionFlags & FUNC_Net) > 0)
	{
		StubContext.MarkAsNetFunction(SrcEventNode->FunctionFlags);
	}

	// Create an entry point
	UK2Node_FunctionEntry* EntryNode = SpawnIntermediateNode<UK2Node_FunctionEntry>(SrcEventNode, ChildStubGraph);
	EntryNode->NodePosX = -200;
	EntryNode->SignatureClass = SrcEventNode->EventSignatureClass;
	EntryNode->SignatureName = SrcEventNode->EventSignatureName;
	EntryNode->CustomGeneratedFunctionName = EventNodeName;

	if(!SrcEventNode->bOverrideFunction && SrcEventNode->IsUsedByAuthorityOnlyDelegate())
	{
		EntryNode->ExtraFlags |= FUNC_BlueprintAuthorityOnly;
	}

	// If this is a customizable event, make sure to copy over the user defined pins
	if (UK2Node_CustomEvent const* SrcCustomEventNode = Cast<UK2Node_CustomEvent const>(SrcEventNode))
	{
		EntryNode->UserDefinedPins = SrcCustomEventNode->UserDefinedPins;
		// CustomEvents may inherit net flags (so let's use their GetNetFlags() incase this is an override)
		StubContext.MarkAsNetFunction(SrcCustomEventNode->GetNetFlags());
	}
	EntryNode->AllocateDefaultPins();

	// Confirm that the event node matches the latest function signature, which the newly created EntryNode should have
	if( !SrcEventNode->IsFunctionEntryCompatible(EntryNode) )
	{
		// There is no match, so the function parameters must have changed.  Throw an error, and force them to refresh
		MessageLog.Error(*FString::Printf(*LOCTEXT("EventNodeOutOfDate_Error", "Event node @@ is out-of-date.  Please refresh it.").ToString()), SrcEventNode);
		return;
	}

	// Copy each event parameter to the assignment node, if there are any inputs
	UK2Node_VariableSet* AssignmentNode = NULL;
	for (int32 PinIndex = 0; PinIndex < EntryNode->Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* SourcePin = EntryNode->Pins[PinIndex];
		if (!Schema->IsMetaPin(*SourcePin) && (SourcePin->Direction == EGPD_Output))
		{
			if (AssignmentNode == NULL)
			{
				// Create a variable write node to store the parameters into the ubergraph frame storage
				AssignmentNode = SpawnIntermediateNode<UK2Node_VariableSet>(SrcEventNode, ChildStubGraph);
				AssignmentNode->VariableReference.SetSelfMember(NAME_None);
				AssignmentNode->AllocateDefaultPins();
			}

			// Determine what the member variable name is for this pin
			UEdGraphPin* UGSourcePin = SrcEventNode->FindPin(SourcePin->PinName);
			const FString MemberVariableName = ClassScopeNetNameMap.MakeValidName(UGSourcePin);

			UEdGraphPin* DestPin = AssignmentNode->CreatePin(
				EGPD_Input, 
				SourcePin->PinType.PinCategory, 
				SourcePin->PinType.PinSubCategory, 
				SourcePin->PinType.PinSubCategoryObject.Get(), 
				SourcePin->PinType.bIsArray, 
				SourcePin->PinType.bIsReference, 
				MemberVariableName);
			MessageLog.NotifyIntermediateObjectCreation(DestPin, SourcePin);
			DestPin->MakeLinkTo(SourcePin);
		}
	}

	if (AssignmentNode == NULL)
	{
		// The event took no parameters, store it as a direct-access call
		StubContext.bIsSimpleStubGraphWithNoParams = true;
	}

	// Create a call into the ubergraph
	UK2Node_CallFunction* CallIntoUbergraph = SpawnIntermediateNode<UK2Node_CallFunction>(SrcEventNode, ChildStubGraph);
	CallIntoUbergraph->NodePosX = 300;

	// Use the ExecuteUbergraph base function to generate the pins...
	CallIntoUbergraph->FunctionReference.SetExternalMember(Schema->FN_ExecuteUbergraphBase, UObject::StaticClass());
	CallIntoUbergraph->AllocateDefaultPins();
	
	// ...then swap to the generated version for this level
	CallIntoUbergraph->FunctionReference.SetSelfMember(GetUbergraphCallName());
	UEdGraphPin* CallIntoUbergraphSelf = Schema->FindSelfPin(*CallIntoUbergraph, EGPD_Input);
	CallIntoUbergraphSelf->PinType.PinSubCategory = Schema->PSC_Self;
	CallIntoUbergraphSelf->PinType.PinSubCategoryObject = *Blueprint->SkeletonGeneratedClass;

	UEdGraphPin* EntryPointPin = CallIntoUbergraph->FindPin(Schema->PN_EntryPoint);
	if (EntryPointPin)
	{
		EntryPointPin->DefaultValue = TEXT("0");
	}

	// Schedule a patchup on the event entry address
	CallsIntoUbergraph.Add(CallIntoUbergraph, SrcEventNode);

	// Wire up the node execution wires
	UEdGraphPin* ExecEntryOut = Schema->FindExecutionPin(*EntryNode, EGPD_Output);
	UEdGraphPin* ExecCallIn = Schema->FindExecutionPin(*CallIntoUbergraph, EGPD_Input);

	if (AssignmentNode != NULL)
	{
		UEdGraphPin* ExecVariablesIn = Schema->FindExecutionPin(*AssignmentNode, EGPD_Input);
		UEdGraphPin* ExecVariablesOut = Schema->FindExecutionPin(*AssignmentNode, EGPD_Output);

		ExecEntryOut->MakeLinkTo(ExecVariablesIn);
		ExecVariablesOut->MakeLinkTo(ExecCallIn);
	}
	else
	{
		ExecEntryOut->MakeLinkTo(ExecCallIn);
	}
}

void FKismetCompilerContext::MergeUbergraphPagesIn(UEdGraph* Ubergraph)
{
	for (TArray<UEdGraph*>::TIterator It(Blueprint->UbergraphPages); It; ++It)
	{
		UEdGraph* SourceGraph = *It;

		if (CompileOptions.bSaveIntermediateProducts)
		{
			TArray<UEdGraphNode*> ClonedNodeList;
			FEdGraphUtilities::CloneAndMergeGraphIn(Ubergraph, SourceGraph, MessageLog, /*bRequireSchemaMatch=*/ true, &ClonedNodeList);

			// Create a comment block around the ubergrapgh contents before anything else got started
			int32 OffsetX;
			int32 OffsetY;
			CreateCommentBlockAroundNodes(ClonedNodeList, SourceGraph, Ubergraph, SourceGraph->GetName(), FLinearColor(1.0f, 0.7f, 0.7f), /*out*/ OffsetX, /*out*/ OffsetY);
			
			// Reposition the nodes, so nothing ever overlaps
			for (TArray<UEdGraphNode*>::TIterator NodeIt(ClonedNodeList); NodeIt; ++NodeIt)
			{
				UEdGraphNode* ClonedNode = *NodeIt;

				ClonedNode->NodePosX += OffsetX;
				ClonedNode->NodePosY += OffsetY;
			}
		}
		else
		{
			FEdGraphUtilities::CloneAndMergeGraphIn(Ubergraph, SourceGraph, MessageLog, /*bRequireSchemaMatch=*/ true);
		}
	}
}

// Expands out nodes that need it
void FKismetCompilerContext::ExpansionStep(UEdGraph* Graph, bool bAllowUbergraphExpansions)
{
	if (bIsFullCompile)
	{
		SCOPE_CYCLE_COUNTER(EKismetCompilerStats_Expansion);

		// Collapse any remaining tunnels or macros
		ExpandTunnelsAndMacros(Graph);

		for (int32 NodeIndex = 0; NodeIndex < Graph->Nodes.Num(); ++NodeIndex)
		{
			UK2Node* Node = Cast<UK2Node>(Graph->Nodes[NodeIndex]);
			if (Node)
			{
				Node->ExpandNode(*this, Graph);
			}
		}

		if (bAllowUbergraphExpansions)
		{
			// Expand timeline nodes
			ExpandTimelineNodes(Graph);

			// Expand PlayMovieScene nodes
			ExpandPlayMovieSceneNodes(Graph);
		}
	}
}

void FKismetCompilerContext::VerifyValidOverrideEvent(const UEdGraph* Graph)
{
	check(NULL != Graph);
	check(NULL != Blueprint);

	TArray<const UK2Node_Event*> EntryPoints;
	Graph->GetNodesOfClass(EntryPoints);

	for (TFieldIterator<UFunction> FunctionIt(Blueprint->ParentClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		const UFunction* Function = *FunctionIt;
		if(!UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function))
		{
			const UClass* FuncClass = CastChecked<UClass>(Function->GetOuter());
			const FName FuncName = Function->GetFName();
			for(int32 EntryPointsIdx = 0; EntryPointsIdx < EntryPoints.Num(); EntryPointsIdx++)
			{
				const UK2Node_Event* EventNode = EntryPoints[EntryPointsIdx];
				if( EventNode && EventNode->bOverrideFunction &&
					(EventNode->EventSignatureClass == FuncClass) &&
					(EventNode->EventSignatureName == FuncName))
				{
					if (EventNode->IsDeprecated())
					{
						MessageLog.Warning(*EventNode->GetDeprecationMessage(), EventNode);
					}
					else
					{
						MessageLog.Error(TEXT("The function in node @@ cannot be overridden and/or placed as event"), EventNode);
					}
				}
			}
		}
	}
}

void FKismetCompilerContext::VerifyValidOverrideFunction(const UEdGraph* Graph)
{
	check(NULL != Graph);
	check(NULL != Blueprint);

	TArray<const UK2Node_FunctionEntry*> EntryPoints;
	Graph->GetNodesOfClass(EntryPoints);

	for(int32 EntryPointsIdx = 0; EntryPointsIdx < EntryPoints.Num(); EntryPointsIdx++)
	{
		const UK2Node_FunctionEntry* EventNode = EntryPoints[EntryPointsIdx];
		check(NULL != EventNode);

		const UClass* FuncClass = *EventNode->SignatureClass;
		if( FuncClass )
		{
			const UFunction* Function = FuncClass->FindFunctionByName(EventNode->SignatureName);
			if( Function )
			{
				const bool bCanBeOverridden = Function->HasAllFunctionFlags(FUNC_BlueprintEvent);
				if(!bCanBeOverridden)
				{
					MessageLog.Error(TEXT("The function in node @@ cannot be overridden"), EventNode);
				}
			}
		}
		else
		{
			// check if the function name is unique
			for (TFieldIterator<UFunction> FunctionIt(Blueprint->ParentClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
			{
				const UFunction* Function = *FunctionIt;
				if( Function && (Function->GetFName() == EventNode->SignatureName) )
				{
					MessageLog.Error(TEXT("The function name in node @@ is already used"), EventNode);
				}
			}
		}
	}
}


// Merges pages and creates function stubs, etc... from the ubergraph entry points
void FKismetCompilerContext::CreateAndProcessUbergraph()
{
	SCOPE_CYCLE_COUNTER(EKismetCompilerStats_ProcessUbergraph);

	ConsolidatedEventGraph = NewNamedObject<UEdGraph>(Blueprint, GetUbergraphCallName());
	ConsolidatedEventGraph->Schema = UEdGraphSchema_K2::StaticClass();
	ConsolidatedEventGraph->SetFlags(RF_Transient);

	// Merge all of the top-level pages
	MergeUbergraphPagesIn(ConsolidatedEventGraph);

	// Add a dummy entry point to the uber graph, to get the function signature correct
	{
		UK2Node_FunctionEntry* EntryNode = SpawnIntermediateNode<UK2Node_FunctionEntry>(NULL, ConsolidatedEventGraph);
		EntryNode->SignatureClass = UObject::StaticClass();
		EntryNode->SignatureName = Schema->FN_ExecuteUbergraphBase;
		EntryNode->CustomGeneratedFunctionName = ConsolidatedEventGraph->GetFName();
		EntryNode->AllocateDefaultPins();
	}

	// Loop over implemented interfaces, and add dummy event entry points for events that aren't explicitly handled by the user
	TArray<UK2Node_Event*> EntryPoints;
	ConsolidatedEventGraph->GetNodesOfClass(EntryPoints);

	for (int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); i++)
	{
		const FBPInterfaceDescription& InterfaceDesc = Blueprint->ImplementedInterfaces[i];
		for (TFieldIterator<UFunction> FunctionIt(InterfaceDesc.Interface, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
		{
			const UFunction* Function = *FunctionIt;
			const FName FunctionName = Function->GetFName();

			// If this is an event, check the merged ubergraph to make sure that it has an event handler, and if not, add one
			if (UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function) && UEdGraphSchema_K2::CanKismetOverrideFunction(Function))
			{
				bool bFoundEntry = false;
				// Search the cached entry points to see if we have a match
				for (int32 EntryIndex = 0; EntryIndex < EntryPoints.Num(); ++EntryIndex)
				{
					const UK2Node_Event* EventNode = EntryPoints[EntryIndex];
					if( EventNode && (EventNode->EventSignatureName == FunctionName) )
					{
						bFoundEntry = true;
						break;
					}
				}

				if (!bFoundEntry)
				{
					// Create an entry node stub, so that we have a entry point for interfaces to call to
					UK2Node_Event* EventNode = SpawnIntermediateNode<UK2Node_Event>(NULL, ConsolidatedEventGraph);
					EventNode->EventSignatureName = FunctionName;
					EventNode->EventSignatureClass = InterfaceDesc.Interface;
					EventNode->bOverrideFunction = true;
					EventNode->AllocateDefaultPins();
				}
			}
		}
	}

	// Expand out nodes that need it
	ExpansionStep(ConsolidatedEventGraph, true);

	// If a function in the graph cannot be overridden/placed as event make sure that it is not.
	VerifyValidOverrideEvent(ConsolidatedEventGraph);

	// Do some cursory validation (pin types match, inputs to outputs, pins never point to their parent node, etc...)
	{
		UbergraphContext = new (FunctionList) FKismetFunctionContext(MessageLog, Schema, NewClass, Blueprint);
		UbergraphContext->SourceGraph = ConsolidatedEventGraph;
		UbergraphContext->MarkAsEventGraph();
		UbergraphContext->MarkAsInternalOrCppUseOnly();
		UbergraphContext->SetExternalNetNameMap(&ClassScopeNetNameMap);

		Blueprint->EventGraphs.Empty();

		// Validate all the nodes in the graph
		for (int32 ChildIndex = 0; ChildIndex < ConsolidatedEventGraph->Nodes.Num(); ++ChildIndex)
		{
			const UEdGraphNode* Node = ConsolidatedEventGraph->Nodes[ChildIndex];	
			const int32 SavedErrorCount = MessageLog.NumErrors;
			ValidateNode(Node);

			// If the node didn't generate any errors then generate function stubs for event entry nodes etc.
			if (SavedErrorCount == MessageLog.NumErrors)
			{
				if (UK2Node_Event* SrcEventNode = Cast<UK2Node_Event>(ConsolidatedEventGraph->Nodes[ChildIndex]))
				{
					CreateFunctionStubForEvent(SrcEventNode, Blueprint);
				}
			}
		}
	}

}

void FKismetCompilerContext::AutoAssignNodePosition(UEdGraphNode* Node)
{
	int32 Width = FMath::Max<int32>(Node->NodeWidth, AverageNodeWidth);
	int32 Height = FMath::Max<int32>(Node->NodeHeight, AverageNodeHeight);

	Node->NodePosX = MacroSpawnX;
	Node->NodePosY = MacroSpawnY;

	MacroSpawnX += Width + HorizontalNodePadding;
	MacroRowMaxHeight = FMath::Max<int32>(MacroRowMaxHeight, Height);

	// Advance the spawn position
	if (MacroSpawnX >= MaximumSpawnX)
	{
		MacroSpawnX = MinimumSpawnX;
		MacroSpawnY += MacroRowMaxHeight + VerticalSectionPadding;

		MacroRowMaxHeight = 0;
	}
}

void FKismetCompilerContext::AdvanceMacroPlacement(int32 Width, int32 Height)
{
	MacroSpawnX += Width + HorizontalSectionPadding;
	MacroRowMaxHeight = FMath::Max<int32>(MacroRowMaxHeight, Height);

	if (MacroSpawnX > MaximumSpawnX)
	{
		MacroSpawnX = MinimumSpawnX;
		MacroSpawnY += MacroRowMaxHeight + VerticalSectionPadding;

		MacroRowMaxHeight = 0;
	}
}

void FKismetCompilerContext::CreateCommentBlockAroundNodes(const TArray<UEdGraphNode*>& Nodes, UObject* SourceObject, UEdGraph* TargetGraph, FString CommentText, FLinearColor CommentColor, int32& Out_OffsetX, int32& Out_OffsetY)
{
	FIntRect Bounds = FEdGraphUtilities::CalculateApproximateNodeBoundaries(Nodes);

	// Figure out how to offset the expanded nodes to fit into our tile
	Out_OffsetX = MacroSpawnX - Bounds.Min.X;
	Out_OffsetY = MacroSpawnY - Bounds.Min.Y;

	// Create a comment node around the expanded nodes, using the name
	const int32 Padding = 60;

	UEdGraphNode_Comment* CommentNode = SpawnIntermediateNode<UEdGraphNode_Comment>(Cast<UEdGraphNode>(SourceObject), TargetGraph);
	CommentNode->CommentColor = CommentColor;
	CommentNode->NodePosX = MacroSpawnX - Padding;
	CommentNode->NodePosY = MacroSpawnY - Padding;
	CommentNode->NodeWidth = Bounds.Width() + 2*Padding;
	CommentNode->NodeHeight = Bounds.Height() + 2*Padding;
	CommentNode->NodeComment = CommentText;
	CommentNode->AllocateDefaultPins();

	// Advance the macro expansion tile to the next open slot
	AdvanceMacroPlacement(Bounds.Width(), Bounds.Height());
}

void FKismetCompilerContext::ExpandTunnelsAndMacros(UEdGraph* SourceGraph)
{
	// Determine if we are regenerating a blueprint on load
	const bool bIsLoading = Blueprint ? Blueprint->bIsRegeneratingOnLoad : false;

	// Collapse any remaining tunnels
	for (TArray<UEdGraphNode*>::TIterator NodeIt(SourceGraph->Nodes); NodeIt; ++NodeIt)
	{
		if (UK2Node_MacroInstance* MacroInstanceNode = Cast<UK2Node_MacroInstance>(*NodeIt))
		{
			UEdGraph* MacroGraph = MacroInstanceNode->GetMacroGraph();
			// Verify that this macro can actually be expanded
			if( MacroGraph == NULL )
			{
				MessageLog.Error(TEXT("Macro node @@ is pointing at an invalid macro graph."), MacroInstanceNode);
				continue;
			}

			// Clone the macro graph, then move all of its children, keeping a list of nodes from the macro
			UEdGraph* ClonedGraph = FEdGraphUtilities::CloneGraph(MacroGraph, NULL, &MessageLog, true);

			TArray<UEdGraphNode*> MacroNodes(ClonedGraph->Nodes);

			// resolve any wildcard pins in the nodes cloned from the macro
			if (!MacroInstanceNode->ResolvedWildcardType.PinCategory.IsEmpty())
			{
				for (auto ClonedNodeIt = ClonedGraph->Nodes.CreateConstIterator(); ClonedNodeIt; ++ClonedNodeIt)
				{
					UEdGraphNode* const ClonedNode = *ClonedNodeIt;
					if (ClonedNode)
					{
						for (auto ClonedPinIt = ClonedNode->Pins.CreateConstIterator(); ClonedPinIt; ++ClonedPinIt)
						{
							UEdGraphPin* const ClonedPin = *ClonedPinIt;
							if ( ClonedPin && (ClonedPin->PinType.PinCategory == Schema->PC_Wildcard) )
							{
								// copy only type info, so array or ref status is preserved
								ClonedPin->PinType.PinCategory = MacroInstanceNode->ResolvedWildcardType.PinCategory;
								ClonedPin->PinType.PinSubCategory = MacroInstanceNode->ResolvedWildcardType.PinSubCategory;
								ClonedPin->PinType.PinSubCategoryObject = MacroInstanceNode->ResolvedWildcardType.PinSubCategoryObject;
							}
						}
					}
				}
			}

			// Handle any nodes that need to inherit their macro instance's NodeGUID
			for( auto ClonedNode : MacroNodes)
			{
				UK2Node_TemporaryVariable* TempVarNode = Cast<UK2Node_TemporaryVariable>(ClonedNode);
				if(TempVarNode && TempVarNode->bIsPersistent)
				{
					TempVarNode->NodeGuid = MacroInstanceNode->NodeGuid;
				}
			}

			// Since we don't support array literals, drop a make array node on any unconnected array pins, which will allow macro expansion to succeed even if disconnected
			for (auto PinIt = MacroInstanceNode->Pins.CreateIterator(); PinIt; ++PinIt)
			{
				UEdGraphPin* Pin = *PinIt;
				if (Pin 
					&& Pin->PinType.bIsArray
					&& (Pin->Direction == EGPD_Input)
					&& (Pin->LinkedTo.Num() == 0))
				{
					UK2Node_MakeArray* MakeArrayNode = SpawnIntermediateNode<UK2Node_MakeArray>(MacroInstanceNode, SourceGraph);
					MakeArrayNode->AllocateDefaultPins();
					UEdGraphPin* MakeArrayOut = MakeArrayNode->GetOutputPin();
					check(MakeArrayOut);
					MakeArrayOut->MakeLinkTo(Pin);
					MakeArrayNode->PinConnectionListChanged(MakeArrayOut);
				}
			}

			ClonedGraph->MoveNodesToAnotherGraph(SourceGraph, GIsAsyncLoading || bIsLoading);
			FEdGraphUtilities::MergeChildrenGraphsIn(SourceGraph, ClonedGraph, /*bRequireSchemaMatch=*/ true);

			// When emitting intermediate products; make an effort to make them readable by preventing overlaps and adding informative comments
			int32 NodeOffsetX = 0;
			int32 NodeOffsetY = 0;
			if (CompileOptions.bSaveIntermediateProducts)
			{
				CreateCommentBlockAroundNodes(
					MacroNodes,
					MacroInstanceNode,
					SourceGraph, 
					FString::Printf(*LOCTEXT("ExpandedMacroComment", "Macro %s").ToString(), *(MacroGraph->GetName())),
					MacroInstanceNode->MetaData.InstanceTitleColor,
					/*out*/ NodeOffsetX,
					/*out*/ NodeOffsetY);
			}

			// Record intermediate object creation nodes, offset the nodes, and handle tunnels
			for (TArray<UEdGraphNode*>::TIterator MacroNodeIt(MacroNodes); MacroNodeIt; ++MacroNodeIt)
			{
				UEdGraphNode* DuplicatedNode = *MacroNodeIt;

				if( DuplicatedNode != NULL )
				{
					// Record the source node mapping for the intermediate node first, as it's going to be overwritten through the MessageLog below
					UEdGraphNode* MacroSourceNode = Cast<UEdGraphNode>(MessageLog.FindSourceObject(DuplicatedNode));
					if (MacroSourceNode)
					{
						FinalNodeBackToMacroSourceMap.NotifyIntermediateObjectCreation(DuplicatedNode, MacroSourceNode);

						// Also record mappings from final macro source node to intermediate macro instance nodes (there may be more than one)
						UEdGraphNode* MacroInstanceSourceNode = Cast<UEdGraphNode>(FinalNodeBackToMacroSourceMap.FindSourceObject(MacroInstanceNode));
						if (MacroInstanceSourceNode && MacroInstanceSourceNode != MacroInstanceNode)
						{
							MacroSourceToMacroInstanceNodeMap.Add(MacroSourceNode, MacroInstanceSourceNode);
						}
					}

					MessageLog.NotifyIntermediateObjectCreation(DuplicatedNode, MacroInstanceNode);

					DuplicatedNode->NodePosY += NodeOffsetY;
					DuplicatedNode->NodePosX += NodeOffsetX;

					// Fix up the tunnel nodes to point correctly
					if (UK2Node_Tunnel* TunnelNode = Cast<UK2Node_Tunnel>(DuplicatedNode))
					{
						if (TunnelNode->bCanHaveInputs)
						{
							TunnelNode->InputSinkNode = MacroInstanceNode;
							MacroInstanceNode->OutputSourceNode = TunnelNode;
						}
						else if (TunnelNode->bCanHaveOutputs)
						{
							TunnelNode->OutputSourceNode = MacroInstanceNode;
							MacroInstanceNode->InputSinkNode = TunnelNode;
						}
					}
				}
			}
		}
		else if (UK2Node_Tunnel* TunnelNode = Cast<UK2Node_Tunnel>(*NodeIt))
		{
			UEdGraphNode* InputSink = TunnelNode->GetInputSink();
			for (UEdGraphPin const* TunnelPin : TunnelNode->Pins)
			{
				if ((TunnelPin->Direction != EGPD_Input) || (TunnelPin->PinType.PinCategory != Schema->PC_Exec))
				{
					continue;
				}
				check(InputSink != NULL);

				UEdGraphPin* SinkPin = InputSink->FindPin(TunnelPin->PinName);
				if (SinkPin == NULL)
				{
					continue;
				}
				check(SinkPin->Direction == EGPD_Output);

				for (UEdGraphPin* TunnelLinkedPin : TunnelPin->LinkedTo)
				{
					MessageLog.NotifyIntermediateObjectCreation(TunnelLinkedPin, SinkPin);
				}
			}

			bool bSuccess = Schema->CollapseGatewayNode(TunnelNode, InputSink, TunnelNode->GetOutputSource());
			if (!bSuccess)
			{
				MessageLog.Error(*LOCTEXT("CollapseTunnel_Error", "Failed to collapse tunnel @@").ToString(), TunnelNode);
			}
		}
	}
}

void FKismetCompilerContext::ResetErrorFlags(UEdGraph* Graph) const
{
	if (Graph != NULL)
	{
		for (int32 NodeIndex = 0; NodeIndex < Graph->Nodes.Num(); ++NodeIndex)
		{
			if (UEdGraphNode* GraphNode = Graph->Nodes[NodeIndex])
			{
				GraphNode->ClearCompilerMessage();
			}
		}
	}
}

/**
 * Merges macros/subgraphs into the graph and validates it, creating a function list entry if it's reasonable.
 */
void FKismetCompilerContext::ProcessOneFunctionGraph(UEdGraph* SourceGraph)
{
	SCOPE_CYCLE_COUNTER(EKismetCompilerStats_ProcessFunctionGraph);

	// Clone the source graph so we can modify it as needed; merging in the child graphs
	UEdGraph* FunctionGraph = FEdGraphUtilities::CloneGraph(SourceGraph, Blueprint, &MessageLog, true); 
	FEdGraphUtilities::MergeChildrenGraphsIn(FunctionGraph, FunctionGraph, /*bRequireSchemaMatch=*/ true);

	ExpansionStep(FunctionGraph, false);

	// If a function in the graph cannot be overridden/placed as event make sure that it is not.
	VerifyValidOverrideFunction(FunctionGraph);

	// First do some cursory validation (pin types match, inputs to outputs, pins never point to their parent node, etc...)
	// If this fails we don't proceed any further to avoid crashes or infinite loops
	if (ValidateGraphIsWellFormed(FunctionGraph))
	{
		FKismetFunctionContext& Context = *new (FunctionList) FKismetFunctionContext(MessageLog, Schema, NewClass, Blueprint);
		Context.SourceGraph = FunctionGraph;

		if(FBlueprintEditorUtils::IsDelegateSignatureGraph(SourceGraph))
		{
			Context.SetDelegateSignatureName(SourceGraph->GetFName());
		}

		// If this is an interface blueprint, mark the function contexts as stubs
		if (FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint))
		{
			Context.MarkAsInterfaceStub();
		}

		if (FBlueprintEditorUtils::IsBlueprintConst(Blueprint))
		{
			Context.MarkAsConstFunction();
		}
	}
}

void FKismetCompilerContext::ValidateFunctionGraphNames()
{
	TSharedPtr<FKismetNameValidator> ParentBPNameValidator;
	if( Blueprint->ParentClass != NULL )
	{
		UBlueprint* ParentBP = Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy);
		if( ParentBP != NULL )
		{
			ParentBPNameValidator = MakeShareable(new FKismetNameValidator(ParentBP));
		}
	}

	if(ParentBPNameValidator.IsValid())
	{
		for (int32 FunctionIndex=0; FunctionIndex < Blueprint->FunctionGraphs.Num(); ++FunctionIndex)
		{
			UEdGraph* FunctionGraph = Blueprint->FunctionGraphs[FunctionIndex];
			if(FunctionGraph->GetFName() != Schema->FN_UserConstructionScript)
			{
				if( ParentBPNameValidator->IsValid(FunctionGraph->GetName()) != EValidatorResult::Ok )
				{
					FName NewFunctionName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, FunctionGraph->GetName());
					MessageLog.Warning(*FString::Printf(*LOCTEXT("FunctionGraphConflictWarning", "Found a function graph with a conflicting name (%s) - changed to %s.").ToString(), *FunctionGraph->GetName(), *NewFunctionName.ToString()));
					FBlueprintEditorUtils::RenameGraph(FunctionGraph, NewFunctionName.ToString());
				}
			}
		}
	}
}

// Performs initial validation that the graph is at least well formed enough to be processed further
// Merge separate pages of the ubergraph together into one ubergraph
// Creates a copy of the graph to allow further transformations to occur
void FKismetCompilerContext::CreateFunctionList()
{
	// Process the ubergraph if one should be present
	if (FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint))
	{
		CreateAndProcessUbergraph();
	}

	if (Blueprint->BlueprintType != BPTYPE_MacroLibrary)
	{
		// Ensure that function graph names are valid and that there are no collisions with a parent class
		//ValidateFunctionGraphNames();

		// Run thru the individual function graphs
		for (int32 i = 0; i < Blueprint->FunctionGraphs.Num(); ++i)
		{
			ProcessOneFunctionGraph(Blueprint->FunctionGraphs[i]);
		}

		for (int32 i = 0; i < Blueprint->DelegateSignatureGraphs.Num(); ++i)
		{
			// change function names to unique

			ProcessOneFunctionGraph(Blueprint->DelegateSignatureGraphs[i]);
		}

		// Run through all the implemented interface member functions
		for (int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); ++i)
		{
			for(int32 j = 0; j < Blueprint->ImplementedInterfaces[i].Graphs.Num(); ++j)
			{
				UEdGraph* SourceGraph = Blueprint->ImplementedInterfaces[i].Graphs[j];
				ProcessOneFunctionGraph(SourceGraph);
			}
		}
	}
}

FKismetFunctionContext* FKismetCompilerContext::CreateFunctionContext()
{
	return new (FunctionList)FKismetFunctionContext(MessageLog, Schema, NewClass, Blueprint);
}

/** Compile a blueprint into a class and a set of functions */
void FKismetCompilerContext::Compile()
{
	SCOPE_CYCLE_COUNTER(EKismetCompilerStats_CompileTime);

	// Interfaces only need function signatures, so we only need to perform the first phase of compilation for them
	bIsFullCompile = CompileOptions.DoesRequireBytecodeGeneration() && (Blueprint->BlueprintType != BPTYPE_Interface);

	CallsIntoUbergraph.Empty();
	if (bIsFullCompile)
	{
		Blueprint->IntermediateGeneratedGraphs.Empty();
	}

	// This flag tries to ensure that component instances will use their template name (since that's how old->new instance mapping is done here)
	//@TODO: This approach will break if and when we multithread compiling, should be an inc-dec pair instead
	TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);

	if (Schema == NULL)
	{
		SCOPE_CYCLE_COUNTER(EKismetCompilerStats_CreateSchema);
		Schema = CreateSchema();
		PostCreateSchema();
	}

	// Make sure the parent class exists and can be used
	check(Blueprint->ParentClass && Blueprint->ParentClass->GetPropertiesSize());

	const bool bIsSkeletonOnly = (CompileOptions.CompileType == EKismetCompileType::SkeletonOnly);
	UClass* TargetUClass = bIsSkeletonOnly ? Blueprint->SkeletonGeneratedClass : Blueprint->GeneratedClass;

	// >>> Backwards Compatibility:  Make sure this is an actual UBlueprintGeneratedClass / UAnimBlueprintGeneratedClass, as opposed to the old UClass
	EnsureProperGeneratedClass(TargetUClass);
	// <<< End Backwards Compatibility

	UBlueprintGeneratedClass* TargetClass = Cast<UBlueprintGeneratedClass>(TargetUClass);

	// >>> Backwards Compatibility: Make sure that skeleton generated classes have the proper "SKEL_" naming convention
	const FString SkeletonPrefix(TEXT("SKEL_"));
	if( bIsSkeletonOnly && TargetClass && !TargetClass->GetName().StartsWith(SkeletonPrefix) )
	{
		FString NewName = SkeletonPrefix + TargetClass->GetName();
 
		// Ensure we have a free name for this class
		UClass* AnyClassWithGoodName = (UClass*)StaticFindObject(UClass::StaticClass(), Blueprint->GetOutermost(), *NewName, false);
		if( AnyClassWithGoodName )
		{
			// Special Case:  If the CDO of the class has become dissociated from its actual CDO, attempt to find the proper named CDO, and get rid of it.
			if( AnyClassWithGoodName->ClassDefaultObject == TargetClass->ClassDefaultObject )
			{
				AnyClassWithGoodName->ClassDefaultObject = NULL;
				FString DefaultObjectName = FString(DEFAULT_OBJECT_PREFIX) + NewName;
				AnyClassWithGoodName->ClassDefaultObject = (UObject*)StaticFindObject(UObject::StaticClass(), Blueprint->GetOutermost(), *DefaultObjectName, false);
			}
 
			// Get rid of the old class to make room for renaming our class to the final SKEL name
			FKismetCompilerUtilities::ConsignToOblivion(AnyClassWithGoodName, Blueprint->bIsRegeneratingOnLoad);

			// Update the refs to the old SKC
			TMap<UObject*, UObject*> ClassReplacementMap;
			ClassReplacementMap.Add(AnyClassWithGoodName, TargetClass);
			TArray<UEdGraph*> AllGraphs;
			Blueprint->GetAllGraphs(AllGraphs);
			for (int32 i = 0; i < AllGraphs.Num(); ++i)
			{
				FArchiveReplaceObjectRef<UObject> ReplaceInBlueprintAr(AllGraphs[i], ClassReplacementMap, /*bNullPrivateRefs=*/ false, /*bIgnoreOuterRef=*/ false, /*bIgnoreArchetypeRef=*/ false);
			}
		}
 
		ERenameFlags RenameFlags = (REN_DontCreateRedirectors|REN_NonTransactional|(Blueprint->bIsRegeneratingOnLoad ? REN_ForceNoResetLoaders : 0));
		TargetClass->Rename(*NewName, NULL, RenameFlags);
	}
	// <<< End Backwards Compatibility

	// >>> Backwards compatibility:  If SkeletonGeneratedClass == GeneratedClass, we need to make a new generated class the first time we need it
	if( !bIsSkeletonOnly && (Blueprint->SkeletonGeneratedClass == Blueprint->GeneratedClass) )
	{
		Blueprint->GeneratedClass = NULL;
		TargetClass = NULL;
	}
	// <<< End Backwards Compatibility

	if( !TargetClass )
	{
		FName NewSkelClassName, NewGenClassName;
		Blueprint->GetBlueprintClassNames(NewGenClassName, NewSkelClassName);
		SpawnNewClass( bIsSkeletonOnly ? NewSkelClassName.ToString() : NewGenClassName.ToString() );
		check(NewClass);

		TargetClass = NewClass;

		// Fix up the reference in the blueprint to the new class
		if( bIsSkeletonOnly )
		{
			Blueprint->SkeletonGeneratedClass = TargetClass;
		}
		else
		{
			Blueprint->GeneratedClass = TargetClass;
		}
	}

	// Early validation
	if (CompileOptions.CompileType == EKismetCompileType::Full)
	{
		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);
		for(auto GraphIter = AllGraphs.CreateIterator(); GraphIter; ++GraphIter)
		{
			if(UEdGraph* Graph = *GraphIter)
			{
				TArray<UK2Node*> AllNodes;
				Graph->GetNodesOfClass(AllNodes);
				for(auto NodeIter = AllNodes.CreateIterator(); NodeIter; ++NodeIter)
				{
					if(UK2Node* Node = *NodeIter)
					{
						Node->EarlyValidation(MessageLog);
					}
				}
			}
		}
	}

	UObject* OldCDO = NULL;

	int32 OldSkelLinkerIdx = INDEX_NONE;
	int32 OldGenLinkerIdx = INDEX_NONE;
	ULinkerLoad* OldLinker = Blueprint->GetLinker();

	if (OldLinker)
	{
		// Cache linker addresses so we can fixup linker for old CDO
		FName GeneratedName, SkeletonName;
		Blueprint->GetBlueprintCDONames(GeneratedName, SkeletonName);

		for( int32 i = 0; i < OldLinker->ExportMap.Num(); i++ )
		{
			FObjectExport& ThisExport = OldLinker->ExportMap[i];
			if( ThisExport.ObjectName == SkeletonName )
			{
				OldSkelLinkerIdx = i;
			}
			else if( ThisExport.ObjectName == GeneratedName )
			{
				OldGenLinkerIdx = i;
			}

			if( OldSkelLinkerIdx != INDEX_NONE && OldGenLinkerIdx != INDEX_NONE )
			{
				break;
			}
		}
	}

	CleanAndSanitizeClass(TargetClass, OldCDO);

	FKismetCompilerVMBackend Backend_VM(Blueprint, Schema, *this);

	NewClass->ClassGeneratedBy = Blueprint;

	NewClass->SetSuperStruct(Blueprint->ParentClass);
	NewClass->ClassFlags |= (Blueprint->ParentClass->ClassFlags & CLASS_Inherit);
	NewClass->ClassCastFlags |= Blueprint->ParentClass->ClassCastFlags;
	
	if(Blueprint->bGenerateConstClass)
	{
		NewClass->ClassFlags |= CLASS_Const;
	}

	// Make sure that this blueprint is up-to-date with regards to its parent functions
	FBlueprintEditorUtils::ConformCallsToParentFunctions(Blueprint);

	// Conform implemented events here, to ensure we generate custom events if necessary after reparenting
	FBlueprintEditorUtils::ConformImplementedEvents(Blueprint);

	// Conform implemented interfaces here, to ensure we generate all functions required by the interface as stubs
	FBlueprintEditorUtils::ConformImplementedInterfaces(Blueprint);

	if (CompileOptions.DoesRequireBytecodeGeneration())
	{
		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);
		for (int32 i = 0; i < AllGraphs.Num(); i++)
		{
			//Reset error flags associated with nodes in each graph
			ResetErrorFlags(AllGraphs[i]);
		}
	}

	// Run thru the class defined variables first, get them registered
	CreateClassVariablesFromBlueprint();

	// Construct a context for each function, doing validation and building the function interface
	{	
		SCOPE_CYCLE_COUNTER(EKismetCompilerStats_CreateFunctionList);
		CreateFunctionList();
	}

	// Precompile the functions
	// Handle delegates signatures first, because they are needed by other functions
	for (int32 i = 0; i < FunctionList.Num(); ++i)
	{
		if(FunctionList[i].IsDelegateSignature())
		{
			PrecompileFunction(FunctionList[i]);
		}
	}

	for (int32 i = 0; i < FunctionList.Num(); ++i)
	{
		if(!FunctionList[i].IsDelegateSignature())
		{
			PrecompileFunction(FunctionList[i]);
		}
	}

	// Relink the class
	NewClass->Bind();
	NewClass->StaticLink(true);

	if (bIsFullCompile && !MessageLog.NumErrors)
	{
		// Generate code for each function (done in a second pass to allow functions to reference each other)
		for (int32 i = 0; i < FunctionList.Num(); ++i)
		{
			if (FunctionList[i].IsValid())
			{
				CompileFunction(FunctionList[i]);
			}
		}

		// Finalize all functions (done last to allow cross-function patchups)
		for (int32 i = 0; i < FunctionList.Num(); ++i)
		{
			if (FunctionList[i].IsValid())
			{
				PostcompileFunction(FunctionList[i]);
			}
		}

		// Save off intermediate build products if requested
		if (CompileOptions.bSaveIntermediateProducts && !Blueprint->bIsRegeneratingOnLoad)
		{
			// Generate code for each function (done in a second pass to allow functions to reference each other)
			for (int32 i = 0; i < FunctionList.Num(); ++i)
			{
				FKismetFunctionContext& ContextFunction = FunctionList[i];
				if (FunctionList[i].SourceGraph != NULL)
				{
					// Record this graph as an intermediate product
					ContextFunction.SourceGraph->Schema = UEdGraphSchema_K2::StaticClass();
					Blueprint->IntermediateGeneratedGraphs.Add(ContextFunction.SourceGraph);
					ContextFunction.SourceGraph->SetFlags(RF_Transient);
				}
			}
		}

		for (TFieldIterator<UMulticastDelegateProperty> PropertyIt(NewClass); PropertyIt; ++PropertyIt)
		{
			if(const UMulticastDelegateProperty* MCDelegateProp = *PropertyIt)
			{
				if(NULL == MCDelegateProp->SignatureFunction)
				{
					MessageLog.Warning(TEXT("No SignatureFunction in MulticastDelegateProperty '%s'"), *MCDelegateProp->GetName());
				}
			}
		}
	}
	else
	{
		// Still need to set flags on the functions even for a skeleton class
		for (int32 i = 0; i < FunctionList.Num(); ++i)
		{
			FKismetFunctionContext& Function = FunctionList[i];
			if (Function.IsValid())
			{
				SCOPE_CYCLE_COUNTER(EKismetCompilerStats_PostcompileFunction);
				FinishCompilingFunction(Function);
			}
		}
	}

	// Late validation for Delegates.
	{
		TSet<UEdGraph*> AllGraphs;
		AllGraphs.Add(UbergraphContext ? UbergraphContext->SourceGraph : NULL);
		for(auto FunctionContextIter = FunctionList.CreateIterator(); FunctionContextIter; ++FunctionContextIter)
		{
			AllGraphs.Add((*FunctionContextIter).SourceGraph);
		}
		for(auto GraphIter = AllGraphs.CreateIterator(); GraphIter; ++GraphIter)
		{
			if(UEdGraph* Graph = *GraphIter)
			{
				TArray<UK2Node_CreateDelegate*> AllNodes;
				Graph->GetNodesOfClass(AllNodes);
				for(auto NodeIter = AllNodes.CreateIterator(); NodeIter; ++NodeIter)
				{
					if(UK2Node_CreateDelegate* Node = *NodeIter)
					{
						Node->ValidationAfterFunctionsAreCreated(MessageLog, (0 != bIsFullCompile));
					}
				}
			}
		}
	}

	{
		SCOPE_CYCLE_COUNTER(EKismetCompilerStats_FinalizationWork);

		// Add any interfaces that the blueprint implements to the class
		AddInterfacesFromBlueprint(NewClass);

		// Set any final flags and seal the class, build a CDO, etc...
		FinishCompilingClass(NewClass);

		// Build delegate binding maps if we have a graph
		if (ConsolidatedEventGraph)
		{
			// Build any dynamic binding information for this class
			BuildDynamicBindingObjects(NewClass);
		}

		UObject* NewCDO = NewClass->GetDefaultObject();

		FUserDefinedStructureCompilerUtils::DefaultUserDefinedStructs(NewCDO, MessageLog);

		// Copy over the CDO properties if we're not already regenerating on load.  In that case, the copy will be done after compile on load is complete
		FBlueprintEditorUtils::PropagateParentBlueprintDefaults(NewClass);

		if (Blueprint->HasAnyFlags(RF_BeingRegenerated))
		{
			if (CompileOptions.CompileType == EKismetCompileType::Full)
			{
				check(Blueprint->PRIVATE_InnermostPreviousCDO == NULL);
				Blueprint->PRIVATE_InnermostPreviousCDO = OldCDO;
			}
		}
		else
		{
			if( NewCDO )
			{
				// Propagate the old CDO's properties to the new
				if( OldCDO )
				{
					if (ObjLoaded)
					{
						if (OldLinker && OldGenLinkerIdx != INDEX_NONE)
						{
							// If we have a list of objects that are loading, patch our export table. This also fixes up load flags
							FBlueprintEditorUtils::PatchNewCDOIntoLinker(Blueprint->GeneratedClass->GetDefaultObject(), OldLinker, OldGenLinkerIdx, *ObjLoaded);
						}
						else
						{
							UE_LOG(LogK2Compiler, Warning, TEXT("Failed to patch linker table for blueprint CDO %s"), *NewCDO->GetName());
						}
					}

					UEditorEngine::CopyPropertiesForUnrelatedObjects(OldCDO, NewCDO);
				}

				// >>> Backwards Compatibility: Propagate data from the skel CDO to the gen CDO if we haven't already done so for this blueprint
				if( !bIsSkeletonOnly && !Blueprint->IsGeneratedClassAuthoritative() )
				{
					UEditorEngine::FCopyPropertiesForUnrelatedObjectsParams CopyDetails;
					CopyDetails.bAggressiveDefaultSubobjectReplacement = false;
					CopyDetails.bDoDelta = false;
					UEditorEngine::CopyPropertiesForUnrelatedObjects(Blueprint->SkeletonGeneratedClass->GetDefaultObject(), NewCDO, CopyDetails);
					Blueprint->SetLegacyGeneratedClassIsAuthoritative();
				}
				// <<< End Backwards Compatibility
			}
		}

		CopyTermDefaultsToDefaultObject(NewCDO);
		SetCanEverTickForActor();
		FKismetCompilerUtilities::ValidateEnumProperties(NewCDO, MessageLog);
	}

	// Fill out the function bodies, either with function bodies, or simple stubs if this is skeleton generation
	{
		// Should we display debug information about the backend outputs?
		bool bDisplayCpp = false;
		bool bDisplayBytecode = false;

		if (!Blueprint->bIsRegeneratingOnLoad)
		{
			GConfig->GetBool(TEXT("Kismet"), TEXT("CompileDisplaysTextBackend"), /*out*/ bDisplayCpp, GEngineIni);
			GConfig->GetBool(TEXT("Kismet"), TEXT("CompileDisplaysBinaryBackend"), /*out*/ bDisplayBytecode, GEngineIni);
		}

		// Generate code thru the backend(s)
		if (bDisplayCpp && bIsFullCompile)
		{
			FKismetCppBackend Backend_CPP(Schema, *this);

			// The C++ backend is currently only for debugging, so it's only run if the output will be visible
			Backend_CPP.GenerateCodeFromClass(NewClass, FunctionList, !bIsFullCompile);
		
			// need to break it down per line to prevent the log from failing to emit it
			TArray<FString> Lines;
			FString TotalString = FString::Printf(TEXT("\n\n\n[header]\n\n\n%s[body]\n%s"), *Backend_CPP.Header, *Backend_CPP.Body);

			TotalString.ParseIntoArray(&Lines, TEXT("\n"), true);
			for (int32 I=0; I<Lines.Num(); ++I)
			{
				FString Line = Lines[I];
				UE_LOG(LogK2Compiler, Log, TEXT("%s"), *Line);
			}
		}

		// Always run the VM backend, it's needed for more than just debug printing
		{
			SCOPE_CYCLE_COUNTER(EKismetCompilerStats_CodeGenerationTime);
			Backend_VM.GenerateCodeFromClass(NewClass, FunctionList, !bIsFullCompile);
		}

		if (bDisplayBytecode && bIsFullCompile)
		{
			TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

			FKismetBytecodeDisassembler Disasm(*GLog);

			// Disassemble script code
			for (int32 i = 0; i < FunctionList.Num(); ++i)
			{
				FKismetFunctionContext& Function = FunctionList[i];
				if (Function.IsValid())
				{
					UE_LOG(LogK2Compiler, Log, TEXT("\n\n[function %s]:\n"), *(Function.Function->GetName()));
					Disasm.DisassembleStructure(Function.Function);
				}
			}
		}
	}

	// If this was a skeleton compile, make sure everything is RF_Transient
	if (bIsSkeletonOnly)
	{
		TArray<UObject*> Subobjects;
		GetObjectsWithOuter(NewClass, Subobjects);

		for (auto SubObjIt = Subobjects.CreateIterator(); SubObjIt; ++SubObjIt)
		{
			UObject* CurrObj = *SubObjIt;
			CurrObj->SetFlags(RF_Transient);
		}

		NewClass->SetFlags(RF_Transient);
	}

	// For full compiles, find other blueprints that may need refreshing, and mark them dirty, in case they try to run
	if( bIsFullCompile && !Blueprint->bIsRegeneratingOnLoad )
	{
		TArray<UObject*> AllBlueprints;
		GetObjectsOfClass(UBlueprint::StaticClass(), AllBlueprints, true);
  
		// Mark any blueprints that implement this interface as dirty
		for( auto CurrentObj = AllBlueprints.CreateIterator(); CurrentObj; ++CurrentObj )
		{
			UBlueprint* CurrentBP = Cast<UBlueprint>(*CurrentObj);
  
			if( FBlueprintEditorUtils::IsBlueprintDependentOn(CurrentBP, Blueprint) )
			{
				CurrentBP->Status = BS_Dirty;
				FBlueprintEditorUtils::RefreshExternalBlueprintDependencyNodes(CurrentBP);
				CurrentBP->BroadcastChanged();
			}
		}
	}

	// Clear out pseudo-local members that are only valid within a Compile call
	UbergraphContext = NULL;
	CallsIntoUbergraph.Empty();
	TimelineToMemberVariableMap.Empty();


	check(NewClass->PropertiesSize >= UObject::StaticClass()->PropertiesSize);
	check(NewClass->ClassDefaultObject != NULL);

	PostCompileDiagnostics();

	if (bIsFullCompile && !Blueprint->bIsRegeneratingOnLoad)
	{
		bool Result = ValidateGeneratedClass(NewClass);
		// TODO What do we do if validation fails?
	}
}

bool FKismetCompilerContext::ValidateGeneratedClass(UBlueprintGeneratedClass* Class)
{
	return UBlueprint::ValidateGeneratedClass(Class);
}

const UK2Node_FunctionEntry* FKismetCompilerContext::FindLocalEntryPoint(const UFunction* Function) const
{
	for (int32 i = 0; i < FunctionList.Num(); ++i)
	{
		const FKismetFunctionContext& FunctionContext = FunctionList[i];
		if (FunctionContext.IsValid() && FunctionContext.Function == Function)
		{
			return FunctionContext.EntryPoint;
		}
	}
	return NULL;
}

void FKismetCompilerContext::SetCanEverTickForActor()
{
	AActor* const CDActor = NewClass ? Cast<AActor>(NewClass->GetDefaultObject()) : NULL;
	if(NULL == CDActor)
	{
		return;
	}

	const bool bOldFlag = CDActor->PrimaryActorTick.bCanEverTick;
	// RESET FLAG 
	{
		UClass* ParentClass = NewClass->GetSuperClass();
		const AActor* ParentCDO = ParentClass ? Cast<AActor>(ParentClass->GetDefaultObject()) : NULL;
		check(NULL != ParentCDO);
		// Clear to handle case, when an event (that forced a flag) was removed, or class was re-parented
		CDActor->PrimaryActorTick.bCanEverTick = ParentCDO->PrimaryActorTick.bCanEverTick;
	}
	
	// RECEIVE TICK
	static FName ReceiveTickName(GET_FUNCTION_NAME_CHECKED(AActor, ReceiveTick));
	const UFunction * ReciveTickEvent = FKismetCompilerUtilities::FindOverriddenImplementableEvent(ReceiveTickName, NewClass);
	if (ReciveTickEvent)
	{
		static const FName ChildCanTickName = TEXT("ChildCanTick");
		const UClass* FirstNativeClass = FBlueprintEditorUtils::FindFirstNativeClass(NewClass);
		const bool bOverrideFlags = (AActor::StaticClass() == FirstNativeClass) || (FirstNativeClass && FirstNativeClass->HasMetaData(ChildCanTickName));
		if(bOverrideFlags)
		{
			CDActor->PrimaryActorTick.bCanEverTick = true;
		}
		else if(!CDActor->PrimaryActorTick.bCanEverTick)
		{
			const FString ReceivTickEventWarning = FString::Printf( 
				*LOCTEXT("ReceiveTick_CanNeverTick", "Blueprint %s has the ReceiveTick @@ event, but it can never tick").ToString(), *NewClass->GetName());
			MessageLog.Warning( *ReceivTickEventWarning, FindLocalEntryPoint(ReciveTickEvent) );
		}
	}

	if(CDActor->PrimaryActorTick.bCanEverTick != bOldFlag)
	{
		UE_LOG(LogK2Compiler, Verbose, TEXT("Overridden flags for Actor class '%s': CanEverTick %s "), *NewClass->GetName(),
			CDActor->PrimaryActorTick.bCanEverTick ? *(GTrue.ToString()) : *(GFalse.ToString()) );
	}
}

#undef LOCTEXT_NAMESPACE
//////////////////////////////////////////////////////////////////////////
