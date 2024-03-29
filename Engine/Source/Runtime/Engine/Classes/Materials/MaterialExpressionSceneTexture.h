// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "MaterialExpressionSceneTexture.generated.h"

/** like EPassInputId but can expose more e.g. GBuffer */
UENUM()
enum ESceneTextureId
{
	/** Scene color, normal post process passes should use PostProcessInput0 */
	PPI_SceneColor UMETA(DisplayName="SceneColor"),
	/** Scene depth, single channel, contains the linear depth of the opaque objects */
	PPI_SceneDepth UMETA(DisplayName="SceneDepth"),
	/** Material diffuse, RGB color (GBuffer) */
	PPI_DiffuseColor UMETA(DisplayName="DiffuseColor"),
	/** Material specular, RGB color (GBuffer) */
	PPI_SpecularColor UMETA(DisplayName="SpecularColor"),
	/** Material subsurface, RGB color (GBuffer) */
	PPI_SubsurfaceColor UMETA(DisplayName="SubsurfaceColor"),
	/** Material base, RGB color (GBuffer) */
	PPI_BaseColor UMETA(DisplayName="BaseColor"),
	/** Material specular, single channel (GBuffer) */
	PPI_Specular UMETA(DisplayName="Specular"),
	/** Material metallic, single channel (GBuffer) */
	PPI_Metallic UMETA(DisplayName="Metallic"),
	/** Normal, RGB in -1..1 range, not normalized (GBuffer) */
	PPI_WorldNormal UMETA(DisplayName="WorldNormal"),
	/** Not yet supported */
	PPI_SeparateTranslucency UMETA(DisplayName="SeparateTranslucency"),
	/** Material opacity, single channel (GBuffer) */
	PPI_Opacity UMETA(DisplayName="Opacity"),
	/** Material roughness, single channel (GBuffer) */
	PPI_Roughness UMETA(DisplayName="Roughness"),
	/** Material ambient occlusion, single channel (GBuffer) */
	PPI_MaterialAO UMETA(DisplayName="MaterialAO"),
	/** Scene depth, single channel, contains the linear depth of the opaque objects rendered with CustomDepth (mesh property) */
	PPI_CustomDepth UMETA(DisplayName="CustomDepth"),
	/** Input #0 of this postprocess pass, usually the only one hooked up */
	PPI_PostProcessInput0 UMETA(DisplayName="PostProcessInput0"),
	/** Input #1 of this postprocess pass, usually not used */
	PPI_PostProcessInput1 UMETA(DisplayName="PostProcessInput1"),
	/** Input #2 of this postprocess pass, usually not used */
	PPI_PostProcessInput2 UMETA(DisplayName="PostProcessInput2"),
	/** Input #3 of this postprocess pass, usually not used */
	PPI_PostProcessInput3 UMETA(DisplayName="PostProcessInput3"),
	/** Input #4 of this postprocess pass, usually not used */
	PPI_PostProcessInput4 UMETA(DisplayName="PostProcessInput4"),
	/** Input #5 of this postprocess pass, usually not used */
	PPI_PostProcessInput5 UMETA(DisplayName="PostProcessInput5"),
	/** Input #6 of this postprocess pass, usually not used */
	PPI_PostProcessInput6 UMETA(DisplayName="PostProcessInput6"),
	/** Decal Mask, single bit */
	PPI_DecalMask UMETA(DisplayName="Decal Mask"),
	/** Lighting model, two bits */
	PPI_LightingModel UMETA(DisplayName="Lighting Model"),
	/** Ambient Occlusion, single channel */
	PPI_AmbientOcclusion UMETA(DisplayName="Ambient Occlusion"),
};

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionSceneTexture : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** UV in 0..1 range */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Ignored if not specified"))
	FExpressionInput Coordinates;

	/** Which scene texture (screen aligned texture) we want to make a lookup into */
	UPROPERTY(EditAnywhere, Category=UMaterialExpressionSceneTexture, meta=(DisplayName = "Scene Texture Id"))
	TEnumAsByte<ESceneTextureId> SceneTextureId;
	
	/** Clamps texture coordinates to the range 0 to 1. Incurs a performance cost. */
	UPROPERTY(EditAnywhere, Category=UMaterialExpressionSceneTexture, meta=(DisplayName = "Clamp UVs"))
	bool bClampUVs;

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) OVERRIDE;
	virtual void GetCaption(TArray<FString>& OutCaptions) const OVERRIDE;
	// End UMaterialExpression Interface
};

