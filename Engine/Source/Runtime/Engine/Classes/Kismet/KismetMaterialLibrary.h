// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KismetMaterialLibrary.generated.h"

UCLASS(MinimalAPI)
class UKismetMaterialLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Sets a scalar parameter value on the material collection instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Material", meta=(Keywords="SetFloatParameterValue", HidePin="WorldContextObject", DefaultToSelf="WorldContextObject", MaterialParameterCollectionFunction = "true"))
	static ENGINE_API void SetScalarParameterValue(UObject* WorldContextObject, UMaterialParameterCollection* Collection, FName ParameterName, float ParameterValue);

	/** Sets a vector parameter value on the material collection instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Material", meta=(Keywords="SetColorParameterValue", HidePin="WorldContextObject", DefaultToSelf="WorldContextObject", MaterialParameterCollectionFunction = "true"))
	static ENGINE_API void SetVectorParameterValue(UObject* WorldContextObject, UMaterialParameterCollection* Collection, FName ParameterName, const FLinearColor& ParameterValue);

	/** Gets a scalar parameter value from the material collection instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Material", meta=(Keywords="GetFloatParameterValue", HidePin="WorldContextObject", DefaultToSelf="WorldContextObject", MaterialParameterCollectionFunction = "true"))
	static ENGINE_API float GetScalarParameterValue(UObject* WorldContextObject, UMaterialParameterCollection* Collection, FName ParameterName);

	/** Gets a vector parameter value from the material collection instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Material", meta=(Keywords="GetColorParameterValue", HidePin="WorldContextObject", DefaultToSelf="WorldContextObject", MaterialParameterCollectionFunction = "true"))
	static ENGINE_API FLinearColor GetVectorParameterValue(UObject* WorldContextObject, UMaterialParameterCollection* Collection, FName ParameterName);

	/** Creates a Dynamic Material Instance which you can modify during gameplay. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Material", meta=(HidePin="WorldContextObject", DefaultToSelf="WorldContextObject", MaterialParameterCollectionFunction = "true"))
	static ENGINE_API class UMaterialInstanceDynamic* CreateDynamicMaterialInstance(UObject* WorldContextObject, class UMaterialInterface* Parent);
};
