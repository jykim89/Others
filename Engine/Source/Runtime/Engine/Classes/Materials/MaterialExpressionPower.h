// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "MaterialExpressionPower.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionPower : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput Base;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstExponent' if not specified"))
	FExpressionInput Exponent;

	/** only used if Exponent is not hooked up */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionPower, meta=(OverridingInputProperty = "Exponent"))
	float ConstExponent;

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) OVERRIDE;
	virtual void GetCaption(TArray<FString>& OutCaptions) const OVERRIDE;
	// End UMaterialExpression Interface

};



