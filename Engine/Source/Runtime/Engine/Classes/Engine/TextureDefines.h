// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TextureDefines.generated.h"

/**
 * @warning: if this is changed:
 *     update BaseEngine.ini [SystemSettings]
 *     you might have to update the update Game's DefaultEngine.ini [SystemSettings]
 *     order and actual name can never change (order is important!)
 *
 * TEXTUREGROUP_Cinematic: should be used for Cinematics which will be baked out
 *                         and want to have the highest settings
 */
UENUM()
enum TextureGroup
{
	TEXTUREGROUP_World UMETA(DisplayName="World"),
	TEXTUREGROUP_WorldNormalMap UMETA(DisplayName="WorldNormalMap"),
	TEXTUREGROUP_WorldSpecular UMETA(DisplayName="WorldSpecular"),
	TEXTUREGROUP_Character UMETA(DisplayName="Character"),
	TEXTUREGROUP_CharacterNormalMap UMETA(DisplayName="CharacterNormalMap"),
	TEXTUREGROUP_CharacterSpecular UMETA(DisplayName="CharacterSpecular"),
	TEXTUREGROUP_Weapon UMETA(DisplayName="Weapon"),
	TEXTUREGROUP_WeaponNormalMap UMETA(DisplayName="WeaponNormalMap"),
	TEXTUREGROUP_WeaponSpecular UMETA(DisplayName="WeaponSpecular"),
	TEXTUREGROUP_Vehicle UMETA(DisplayName="Vehicle"),
	TEXTUREGROUP_VehicleNormalMap UMETA(DisplayName="VehicleNormalMap"),
	TEXTUREGROUP_VehicleSpecular UMETA(DisplayName="VehicleSpecular"),
	TEXTUREGROUP_Cinematic UMETA(DisplayName="Cinematic"),
	TEXTUREGROUP_Effects UMETA(DisplayName="Effects"),
	TEXTUREGROUP_EffectsNotFiltered UMETA(DisplayName="EffectsNotFiltered"),
	TEXTUREGROUP_Skybox UMETA(DisplayName="Skybox"),
	TEXTUREGROUP_UI UMETA(DisplayName="UI"),
	TEXTUREGROUP_Lightmap UMETA(DisplayName="Lightmap"),
	TEXTUREGROUP_RenderTarget UMETA(DisplayName="RenderTarget"),
	TEXTUREGROUP_MobileFlattened UMETA(DisplayName="MobileFlattened"),
	//obsolete - kept for back-compat
	TEXTUREGROUP_ProcBuilding_Face UMETA(DisplayName="ProcBuilding_Face"),
	//obsolete - kept for back-compat
	TEXTUREGROUP_ProcBuilding_LightMap UMETA(DisplayName="ProcBuilding_LightMap"),
	TEXTUREGROUP_Shadowmap UMETA(DisplayName="Shadowmap"),
	// no compression, no mips
	TEXTUREGROUP_ColorLookupTable UMETA(DisplayName="ColorLookupTable"),
	TEXTUREGROUP_Terrain_Heightmap UMETA(DisplayName="Terrain_Heightmap"),
	TEXTUREGROUP_Terrain_Weightmap UMETA(DisplayName="Terrain_Weightmap"),
	// using this TextureGroup triggers special mip map generation code only useful for the BokehDOF post process
	TEXTUREGROUP_Bokeh UMETA(DisplayName="Bokeh"),
	// no compression, created on import of a .IES file
	TEXTUREGROUP_IESLightProfile UMETA(DisplayName="IESLightProfile"),
	TEXTUREGROUP_MAX,
};

UENUM()
enum TextureMipGenSettings
{
	// default for the "texture"
	TMGS_FromTextureGroup UMETA(DisplayName="FromTextureGroup"),
	// 2x2 average, default for the "texture group"
	TMGS_SimpleAverage UMETA(DisplayName="SimpleAverage"),
	// 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1..little, 5=medium, 10=extreme
	TMGS_Sharpen0 UMETA(DisplayName="Sharpen0"),
	TMGS_Sharpen1 UMETA(DisplayName="Sharpen1"),
	TMGS_Sharpen2 UMETA(DisplayName="Sharpen2"),
	TMGS_Sharpen3 UMETA(DisplayName="Sharpen3"),
	TMGS_Sharpen4 UMETA(DisplayName="Sharpen4"),
	TMGS_Sharpen5 UMETA(DisplayName="Sharpen5"),
	TMGS_Sharpen6 UMETA(DisplayName="Sharpen6"),
	TMGS_Sharpen7 UMETA(DisplayName="Sharpen7"),
	TMGS_Sharpen8 UMETA(DisplayName="Sharpen8"),
	TMGS_Sharpen9 UMETA(DisplayName="Sharpen9"),
	TMGS_Sharpen10 UMETA(DisplayName="Sharpen10"),
	TMGS_NoMipmaps UMETA(DisplayName="NoMipmaps"),
	// Do not touch existing mip chain as it contains generated data
	TMGS_LeaveExistingMips UMETA(DisplayName="LeaveExistingMips"),
	// blur further (useful for image based reflections)
	TMGS_Blur1 UMETA(DisplayName="Blur1"),
	TMGS_Blur2 UMETA(DisplayName="Blur2"),
	TMGS_Blur3 UMETA(DisplayName="Blur3"),
	TMGS_Blur4 UMETA(DisplayName="Blur4"),
	TMGS_Blur5 UMETA(DisplayName="Blur5"),
	TMGS_MAX,
};
