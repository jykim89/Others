// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "InputAxisDelegateBinding.generated.h"

USTRUCT()
struct ENGINE_API FBlueprintInputAxisDelegateBinding : public FBlueprintInputDelegateBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName InputAxisName;

	UPROPERTY()
	FName FunctionNameToBind;

	FBlueprintInputAxisDelegateBinding()
		: FBlueprintInputDelegateBinding()
		, InputAxisName(NAME_None)
		, FunctionNameToBind(NAME_None)
	{
	}
};

UCLASS()
class ENGINE_API UInputAxisDelegateBinding : public UInputDelegateBinding
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FBlueprintInputAxisDelegateBinding> InputAxisDelegateBindings;

	// Begin UInputDelegateBinding interface
	virtual void BindToInputComponent(UInputComponent* InputComponent) const OVERRIDE;
	// End UInputDelegateBinding interface
};
