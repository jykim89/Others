// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


/**
 * Allows the artists to quickly set up a Fresnel term. Returns:
 *
 *		pow(1 - max(Normal dot Camera,0),Exponent)
 */

#pragma once
#include "MaterialExpressionFresnel.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionFresnel : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'Exponent' if not specified"))
	FExpressionInput ExponentIn;

	/** The exponent to pass into the pow() function */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFresnel, meta=(OverridingInputProperty = "ExponentIn"))
	float Exponent;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'BaseReflectFraction' if not specified"))
	FExpressionInput BaseReflectFractionIn;

	/** 
	 * Specifies the fraction of specular reflection when the surfaces is viewed from straight on.
	 * A value of 1 effectively disables Fresnel.
	 */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFresnel, meta=(OverridingInputProperty = "BaseReflectFractionIn"))
	float BaseReflectFraction;

	/** The normal to dot with the camera FVector */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to Pixel World Normal if not specified"))
	FExpressionInput Normal;


	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) OVERRIDE;
	virtual void GetCaption(TArray<FString>& OutCaptions) const OVERRIDE
	{
		OutCaptions.Add(FString(TEXT("Fresnel")));
	}
	// End UMaterialExpression Interface
};



