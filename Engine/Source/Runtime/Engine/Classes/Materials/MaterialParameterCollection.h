// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 * MaterialParameterCollection.h - defines an asset that has a list of parameters, which can be referenced by any material and updated efficiently at runtime
 */

#pragma once
#include "MaterialParameterCollection.generated.h"

/** Base struct for collection parameters */
USTRUCT()
struct FCollectionParameterBase
{
	GENERATED_USTRUCT_BODY()
	
	FCollectionParameterBase()
	{
		FPlatformMisc::CreateGuid(Id);
	}

	/** The name of the parameter.  Changing this name will break any blueprints that reference the parameter. */
	UPROPERTY(EditAnywhere, Category=Parameter)
	FName ParameterName;

	/** Uniquely identifies the parameter, used for fixing up materials that reference this parameter when renaming. */
	UPROPERTY()
	FGuid Id;
};

/** A scalar parameter */
USTRUCT()
struct FCollectionScalarParameter : public FCollectionParameterBase
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category=Parameter)
	float DefaultValue;
};

/** A vector parameter */
USTRUCT()
struct FCollectionVectorParameter : public FCollectionParameterBase
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category=Parameter)
	FLinearColor DefaultValue;
};

/** 
 * Asset class that contains a list of parameter names and their default values. 
 * Any number of materials can reference these parameters and get new values when the parameter values are changed.
 */
UCLASS(hidecategories=object, MinimalAPI)
class UMaterialParameterCollection : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Used by materials using this collection to know when to recompile. */
	UPROPERTY(duplicatetransient)
	FGuid StateId;

	UPROPERTY(EditAnywhere, Category=Material)
	TArray<FCollectionScalarParameter> ScalarParameters;

	UPROPERTY(EditAnywhere, Category=Material)
	TArray<FCollectionVectorParameter> VectorParameters;

	// Begin UObject interface.
#if WITH_EDITOR
	virtual void PreEditChange(class FEditPropertyChain& PropertyAboutToChange) OVERRIDE;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	virtual void PostLoad() OVERRIDE;
	// End UObject interface.

	/** Finds a parameter name given an Id, returns NAME_None if the parameter was not found. */
	FName GetParameterName(const FGuid& Id) const;

	/** Finds a parameter id given a name, returns the default guid if the parameter was not found. */
	FGuid GetParameterId(FName ParameterName) const;

	/** Gets a vector and component index for the given parameter, used when compiling materials, to know where to access a certain parameter. */
	void GetParameterIndex(const FGuid& Id, int32& OutIndex, int32& OutComponentIndex) const;

	/** Populates an array with either scalar or vector parameter names. */
	ENGINE_API void GetParameterNames(TArray<FName>& OutParameterNames, bool bVectorParameters) const;

	/** Utility to find a scalar parameter struct given a parameter name.  Returns NULL if not found. */
	const FCollectionScalarParameter* GetScalarParameterByName(FName ParameterName) const;

	/** Utility to find a vector parameter struct given a parameter name.  Returns NULL if not found. */
	const FCollectionVectorParameter* GetVectorParameterByName(FName ParameterName) const;

	/** Accessor for the uniform buffer layout description. */
	const FUniformBufferStruct& GetUniformBufferStruct() const
	{
		check(UniformBufferStruct);
		return *UniformBufferStruct;
	}

private:

	TScopedPointer<FUniformBufferStruct> UniformBufferStruct;

	void CreateBufferStruct();
};



