// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvQueryGenerator_SimpleGrid.generated.h"

/**
 *  Simple grid, generates points in 2D square around context
 */

UCLASS()
class UEnvQueryGenerator_SimpleGrid : public UEnvQueryGenerator_ProjectedPoints
{
	GENERATED_UCLASS_BODY()

	/** square's extent */
	UPROPERTY(EditDefaultsOnly, Category=Generator)
	FEnvFloatParam Radius;

	/** generation density */
	UPROPERTY(EditDefaultsOnly, Category=Generator)
	FEnvFloatParam Density;

	/** context */
	UPROPERTY(EditDefaultsOnly, Category=Generator)
	TSubclassOf<class UEnvQueryContext> GenerateAround;

	void GenerateItems(struct FEnvQueryInstance& QueryInstance); 

	virtual FText GetDescriptionTitle() const OVERRIDE;
	virtual FText GetDescriptionDetails() const OVERRIDE;
};
