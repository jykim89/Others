// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "InputAxisKeyDelegateBinding.generated.h"

USTRUCT()
struct ENGINE_API FBlueprintInputAxisKeyDelegateBinding : public FBlueprintInputDelegateBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FKey AxisKey;

	UPROPERTY()
	FName FunctionNameToBind;

	FBlueprintInputAxisKeyDelegateBinding()
		: FBlueprintInputDelegateBinding()
		, FunctionNameToBind(NAME_None)
	{
	}
};

UCLASS()
class ENGINE_API UInputAxisKeyDelegateBinding : public UInputDelegateBinding
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FBlueprintInputAxisKeyDelegateBinding> InputAxisKeyDelegateBindings;

	// Begin UInputDelegateBinding interface
	virtual void BindToInputComponent(UInputComponent* InputComponent) const OVERRIDE;
	// End UInputDelegateBinding interface
};
