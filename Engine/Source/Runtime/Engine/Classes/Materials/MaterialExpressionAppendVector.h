// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "MaterialExpressionAppendVector.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionAppendVector : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput A;

	UPROPERTY()
	FExpressionInput B;


	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) OVERRIDE;
	virtual void GetCaption(TArray<FString>& OutCaptions) const OVERRIDE;
	// End UMaterialExpression Interface

};



