// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 * CubeBuilder: Builds a 3D cube brush.
 */

#pragma once
#include "CubeBuilder.generated.h"

UCLASS(MinimalAPI, autoexpandcategories=BrushSettings, EditInlineNew, meta=(DisplayName="Box"))
class UCubeBuilder : public UEditorBrushBuilder
{
	GENERATED_UCLASS_BODY()

	/** The size of the cube in the X dimension */
	UPROPERTY(EditAnywhere, Category=BrushSettings, meta=(ClampMin = "0.000001"))
	float X;

	/** The size of the cube in the Y dimension */
	UPROPERTY(EditAnywhere, Category=BrushSettings, meta=(ClampMin = "0.000001"))
	float Y;

	/** The size of the cube in the Z dimension */
	UPROPERTY(EditAnywhere, Category=BrushSettings, meta=(ClampMin = "0.000001"))
	float Z;

	/** The thickness of the cube wall when hollow */
	UPROPERTY(EditAnywhere, Category=BrushSettings, meta=(EditCondition="Hollow"))
	float WallThickness;

	UPROPERTY()
	FName GroupName;

	/** Whether this is a hollow or solid cube */
	UPROPERTY(EditAnywhere, Category=BrushSettings)
	uint32 Hollow:1;

	/** Whether extra internal faces should be generated for each cube face */
	UPROPERTY(EditAnywhere, Category=BrushSettings)
	uint32 Tessellated:1;


	// Begin UBrushBuilder Interface
	virtual bool Build( UWorld* InWorld, ABrush* InBrush = NULL ) OVERRIDE;
	// End UBrushBuilder Interface

	// @todo document
	virtual void BuildCube( int32 Direction, float dx, float dy, float dz, bool _tessellated );
};



