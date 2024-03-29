// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 * MaterialExpressionCollectionParameter.h - a node that references a single parameter in a MaterialParameterCollection
 */

#pragma once
#include "MaterialExpressionCollectionParameter.generated.h"

UCLASS(hidecategories=object, MinimalAPI)
class UMaterialExpressionCollectionParameter : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** The Parameter Collection to use. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionCollectionParameter)
	class UMaterialParameterCollection* Collection;

	/** Name of the parameter being referenced. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionCollectionParameter)
	FName ParameterName;

	/** Id that is set from the name, and used to handle renaming of collection parameters. */
	UPROPERTY()
	FGuid ParameterId;

	// Begin UObject interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	virtual void PostLoad() OVERRIDE;
	// End UObject interface.

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) OVERRIDE;
	virtual void GetCaption(TArray<FString>& OutCaptions) const OVERRIDE;
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery ) OVERRIDE;
	// End UMaterialExpression Interface
};



