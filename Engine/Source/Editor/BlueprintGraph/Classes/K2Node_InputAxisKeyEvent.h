// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "K2Node_InputAxisKeyEvent.generated.h"

UCLASS(MinimalAPI)
class UK2Node_InputAxisKeyEvent : public UK2Node_Event
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FKey AxisKey;

	// Prevents actors with lower priority from handling this input
	UPROPERTY(EditAnywhere, Category = "Input")
	uint32 bConsumeInput : 1;

	// Should the binding execute even when the game is paused
	UPROPERTY(EditAnywhere, Category = "Input")
	uint32 bExecuteWhenPaused : 1;

	// Should any bindings to this event in parent classes be removed
	UPROPERTY(EditAnywhere, Category = "Input")
	uint32 bOverrideParentBinding : 1;

	// Begin EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const OVERRIDE;
	virtual FString GetNodeNativeTitle(ENodeTitleType::Type TitleType) const OVERRIDE;
	virtual FString GetTooltip() const OVERRIDE;
	virtual FName GetPaletteIcon(FLinearColor& OutColor) const OVERRIDE;
	virtual bool CanPasteHere(const UEdGraph* TargetGraph, const UEdGraphSchema* Schema) const OVERRIDE;
	// End EdGraphNode interface

	// Begin UK2Node interface
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const OVERRIDE;
	virtual bool ShouldShowNodeProperties() const OVERRIDE{ return true; }
	virtual UClass* GetDynamicBindingClass() const OVERRIDE;
	virtual void RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const OVERRIDE;
	// End UK2Node interface

	void Initialize(const FKey AxisKey);
};