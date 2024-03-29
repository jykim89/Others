// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
/**
 *
 */
#include "VehicleAnimInstance.generated.h"

UCLASS(transient)
class ENGINE_API UVehicleAnimInstance : public UAnimInstance
{
	GENERATED_UCLASS_BODY()

	/** Makes a montage jump to the end of a named section. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	class AWheeledVehicle * GetVehicle();
};



