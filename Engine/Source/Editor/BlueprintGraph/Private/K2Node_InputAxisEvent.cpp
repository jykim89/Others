// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"

#include "CompilerResultsLog.h"

UK2Node_InputAxisEvent::UK2Node_InputAxisEvent(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bConsumeInput = true;
	bOverrideParentBinding = true;
	bInternalEvent = true;

	EventSignatureName = TEXT("InputAxisHandlerDynamicSignature__DelegateSignature");
	EventSignatureClass = UInputComponent::StaticClass();
}

void UK2Node_InputAxisEvent::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerUE4Version() < VER_UE4_BLUEPRINT_INPUT_BINDING_OVERRIDES)
	{
		// Don't change existing behaviors
		bOverrideParentBinding = false;
	}
}

void UK2Node_InputAxisEvent::Initialize(const FName AxisName)
{
	InputAxisName = AxisName;
	CustomFunctionName = FName( *FString::Printf(TEXT("InpAxisEvt_%s_%s"), *InputAxisName.ToString(), *GetName()));
}

FText UK2Node_InputAxisEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("InputAxisName"), FText::FromName(InputAxisName));
	return FText::Format(NSLOCTEXT("K2Node", "InputAxis_Name", "InputAxis {InputAxisName}"), Args);
}

FString UK2Node_InputAxisEvent::GetNodeNativeTitle(ENodeTitleType::Type TitleType) const
{
	// Do not setup this function for localization, intentionally left unlocalized!
	return FString::Printf(TEXT("InputAxis %s"), *InputAxisName.ToString());
}

FString UK2Node_InputAxisEvent::GetTooltip() const
{
	return FString::Printf(*NSLOCTEXT("K2Node", "InputAxis_Tooltip", "Event that provides the current value of the %s axis once per frame when input is enabled for the containing actor.").ToString(), *InputAxisName.ToString());
}

void UK2Node_InputAxisEvent::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	TArray<FName> AxisNames;
	GetDefault<UInputSettings>()->GetAxisNames(AxisNames);
	if (!AxisNames.Contains(InputAxisName))
	{
		MessageLog.Warning(*FString::Printf(*NSLOCTEXT("KismetCompiler", "MissingInputAxisEvent_Warning", "Input Axis Event references unknown Axis '%s' for @@").ToString(), *InputAxisName.ToString()), this);
	}
}

UClass* UK2Node_InputAxisEvent::GetDynamicBindingClass() const
{
	return UInputAxisDelegateBinding::StaticClass();
}

void UK2Node_InputAxisEvent::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	UInputAxisDelegateBinding* InputAxisBindingObject = CastChecked<UInputAxisDelegateBinding>(BindingObject);

	FBlueprintInputAxisDelegateBinding Binding;
	Binding.InputAxisName = InputAxisName;
	Binding.bConsumeInput = bConsumeInput;
	Binding.bExecuteWhenPaused = bExecuteWhenPaused;
	Binding.bOverrideParentBinding = bOverrideParentBinding;
	Binding.FunctionNameToBind = CustomFunctionName;

	InputAxisBindingObject->InputAxisDelegateBindings.Add(Binding);
}

bool UK2Node_InputAxisEvent::CanPasteHere(const UEdGraph* TargetGraph, const UEdGraphSchema* Schema) const
{
	// By default, to be safe, we don't allow events to be pasted, except under special circumstances (see below)
	bool bAllowPaste = false;

	// Ensure that we can be instanced under the specified schema
	if (CanCreateUnderSpecifiedSchema(Schema))
	{
		// Can only place events in ubergraphs
		if (Schema->GetGraphType(TargetGraph) == EGraphType::GT_Ubergraph)
		{
			// Find the Blueprint that owns the target graph
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
			if (Blueprint && Blueprint->SkeletonGeneratedClass)
			{
				bAllowPaste = Blueprint->ParentClass->IsChildOf(AActor::StaticClass());
				if (!bAllowPaste)
				{
					UE_LOG(LogBlueprint, Log, TEXT("Cannot paste event node (%s) directly because the graph does not belong to an Actor."), *GetFName().ToString());
				}
			}
		}
	}
	else
	{
		UE_LOG(LogBlueprint, Log, TEXT("Cannot paste event node (%s) directly because it cannot be created under the specified schema."), *GetFName().ToString());
	}

	return bAllowPaste;
}
