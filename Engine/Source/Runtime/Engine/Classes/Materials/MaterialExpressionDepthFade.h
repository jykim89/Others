// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 * Scales Opacity by a Linear fade based on SceneDepth, from 0 at PixelDepth to 1 at FadeDistance
 */

#pragma once
#include "MaterialExpressionDepthFade.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionDepthFade : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Input opacity which will be scaled by the result of the fade. */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'OpacityDefault' if not specified"))
	FExpressionInput InOpacity;

	/** World space distance over which the fade should take place. */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'FadeDistanceDefault' if not specified"))
	FExpressionInput FadeDistance;

	/** Opacity which will be scaled by the result of the fade.  This is used when InOpacity is unconnected. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionDepthFade, meta=(OverridingInputProperty = "InOpacity"))
	float OpacityDefault;

	/** World space distance over which the fade should take place.  This is used when FadeDistance is unconnected. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionDepthFade, meta=(OverridingInputProperty = "FadeDistance"))
	float FadeDistanceDefault;


	// Begin UMaterialExpression Interface
	virtual FString GetInputName(int32 InputIndex) const OVERRIDE
	{
		if (InputIndex == 0)
		{
			return TEXT("Opacity");
		}
		else
		{
			return Super::GetInputName(InputIndex);
		}
	}
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) OVERRIDE;
	virtual void GetCaption(TArray<FString>& OutCaptions) const OVERRIDE
	{
		OutCaptions.Add(TEXT("Depth Fade"));
	}
	// End UMaterialExpression Interface#endif
};



