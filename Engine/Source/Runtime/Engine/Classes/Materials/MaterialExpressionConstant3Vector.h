// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "MaterialExpressionConstant3Vector.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionConstant3Vector : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	float R_DEPRECATED;

	UPROPERTY()
	float G_DEPRECATED;

	UPROPERTY()
	float B_DEPRECATED;

 	UPROPERTY(EditAnywhere, Category=MaterialExpressionConstant3Vector, meta=(HideAlphaChannel))
	FLinearColor Constant;

	// Begin UObject interface.
	virtual void PostLoad() OVERRIDE;
	// End UObject interface.

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) OVERRIDE;
	virtual void GetCaption(TArray<FString>& OutCaptions) const OVERRIDE;
#if WITH_EDITOR
	virtual FString GetDescription() const OVERRIDE;
	virtual uint32 GetOutputType(int32 OutputIndex) OVERRIDE {return MCT_Float3;}
#endif // WITH_EDITOR
	// End UMaterialExpression Interface
};



