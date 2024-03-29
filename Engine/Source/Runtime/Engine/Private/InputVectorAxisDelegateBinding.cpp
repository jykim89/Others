// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "BlueprintUtilities.h"


UInputVectorAxisDelegateBinding::UInputVectorAxisDelegateBinding(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UInputVectorAxisDelegateBinding::BindToInputComponent(UInputComponent* InputComponent) const
{
	TArray<FInputVectorAxisBinding> BindsToAdd;

	for (int32 BindIndex=0; BindIndex<InputAxisKeyDelegateBindings.Num(); ++BindIndex)
	{
		const FBlueprintInputAxisKeyDelegateBinding& Binding = InputAxisKeyDelegateBindings[BindIndex];

		FInputVectorAxisBinding VAB( Binding.AxisKey );
		VAB.bConsumeInput = Binding.bConsumeInput;
		VAB.bExecuteWhenPaused = Binding.bExecuteWhenPaused;
		VAB.AxisDelegate.BindDelegate(InputComponent->GetOwner(), Binding.FunctionNameToBind);

		if (Binding.bOverrideParentBinding)
		{
			for (int32 ExistingIndex = InputComponent->VectorAxisBindings.Num() - 1; ExistingIndex >= 0; --ExistingIndex)
			{
				const FInputVectorAxisBinding& ExistingBind = InputComponent->VectorAxisBindings[ExistingIndex];
				if (ExistingBind.AxisKey == VAB.AxisKey)
				{
					InputComponent->AxisKeyBindings.RemoveAt(ExistingIndex);
				}
			}
		}

		// To avoid binds in the same layer being removed by the parent override temporarily put them in this array and add later
		BindsToAdd.Add(VAB);
	}

	for (int32 Index=0; Index < BindsToAdd.Num(); ++Index)
	{
		InputComponent->VectorAxisBindings.Add(BindsToAdd[Index]);
	}
}