// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 * MaterialParameterCollectionInstance.h 
 */

#pragma once
#include "MaterialParameterCollectionInstance.generated.h"

/** 
 * Class that stores per-world instance parameter data for a given UMaterialParameterCollection resource. 
 * Instances of this class are always transient.
 */
UCLASS(hidecategories=object, MinimalAPI)
class UMaterialParameterCollectionInstance : public UObject
{
	GENERATED_UCLASS_BODY()

	// Begin UObject interface.
	ENGINE_API virtual void PostInitProperties();	
	ENGINE_API virtual void FinishDestroy() OVERRIDE;
	// End UObject interface.

	/** Initializes the instance with the collection it is based off of and the world it is owned by. */
	void SetCollection(UMaterialParameterCollection* InCollection, UWorld* InWorld);

	/** Sets parameter value overrides, returns false if the parameter was not found. */
	bool SetScalarParameterValue(FName ParameterName, float ParameterValue);
	bool SetVectorParameterValue(FName ParameterName, const FLinearColor& ParameterValue);

	/** Gets parameter values, returns false if the parameter was not found. */
	bool GetScalarParameterValue(FName ParameterName, float& OutParameterValue) const;
	bool GetVectorParameterValue(FName ParameterName, FLinearColor& OutParameterValue) const;

	class FMaterialParameterCollectionInstanceResource* GetResource()
	{
		return Resource;
	}

	const UMaterialParameterCollection* GetCollection() const
	{
		return Collection;
	}

	void UpdateRenderState();

	/** Tracks whether this instance has ever issued a missing parameter warning, to reduce log spam. */
	bool bLoggedMissingParameterWarning;

protected:

	/** Collection resource this instance is based off of. */
	UPROPERTY()
	UMaterialParameterCollection* Collection;

	/** World that owns this instance. */
	UPROPERTY()
	UWorld* World;

	/** Overrides for scalar parameter values. */
	TMap<FName, float> ScalarParameterValues;

	/** Overrides for vector parameter values. */
	TMap<FName, FLinearColor> VectorParameterValues;

	/** Instance resource which stores the rendering thread representation of this instance. */
	FMaterialParameterCollectionInstanceResource* Resource;

	/** Boils down the instance overrides and default values into data to be set on the uniform buffer. */
	void GetParameterData(TArray<FVector4>& ParameterData) const;
};



