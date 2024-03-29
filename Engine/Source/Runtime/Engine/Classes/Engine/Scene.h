// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// Scene - script exposed scene enums
//=============================================================================

#pragma once
#include "Scene.generated.h"

/** used by FPostProcessSettings Depth of Field */
UENUM()
enum EDepthOfFieldMethod
{
	DOFM_BokehDOF UMETA(DisplayName="BokehDOF"),
	DOFM_Gaussian UMETA(DisplayName="Gaussian"),
	DOFM_MAX,
};

/** used by FPostProcessSettings Anti-aliasing */
UENUM()
enum EAntiAliasingMethod
{
	AAM_None UMETA(DisplayName="None"),
	AAM_FXAA UMETA(DisplayName="FXAA"),
	AAM_TemporalAA UMETA(DisplayName="TemporalAA"),
	AAM_MAX,
};

/** To be able to use struct PostProcessSettings. */
// Each property consists of a bool to enable it (by default off),
// the variable declaration and further down the default value for it.
// The comment should include the meaning and usable range.
USTRUCT(BlueprintType)
struct FPostProcessSettings
{
	GENERATED_USTRUCT_BODY()

	// first all bOverride_... as they get grouped together into bitfields

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_FilmWhitePoint:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_FilmSaturation:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_FilmChannelMixerRed:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_FilmChannelMixerGreen:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_FilmChannelMixerBlue:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_FilmContrast:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_FilmDynamicRange:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_FilmHealAmount:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_FilmToeAmount:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_FilmShadowTint:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_FilmShadowTintBlend:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_FilmShadowTintAmount:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_SceneColorTint:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_SceneFringeIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_SceneFringeSaturation:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientCubemapTint:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientCubemapIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_BloomIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_BloomThreshold:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_Bloom1Tint:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_Bloom1Size:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_Bloom2Size:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_Bloom2Tint:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_Bloom3Tint:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_Bloom3Size:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_Bloom4Tint:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_Bloom4Size:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_Bloom5Tint:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_Bloom5Size:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_BloomDirtMaskIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_BloomDirtMaskTint:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_BloomDirtMask:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AutoExposureLowPercent:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AutoExposureHighPercent:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AutoExposureMinBrightness:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AutoExposureMaxBrightness:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AutoExposureSpeedUp:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AutoExposureSpeedDown:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AutoExposureBias:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_HistogramLogMin:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_HistogramLogMax:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LensFlareIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LensFlareTint:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LensFlareTints:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LensFlareBokehSize:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LensFlareBokehShape:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LensFlareThreshold:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_VignetteIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_VignetteColor:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_GrainIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_GrainJitter:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientOcclusionIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientOcclusionStaticFraction:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientOcclusionRadius:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientOcclusionFadeDistance:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientOcclusionFadeRadius:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientOcclusionDistance:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientOcclusionRadiusInWS:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientOcclusionPower:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientOcclusionBias:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientOcclusionQuality:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientOcclusionMipBlend:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientOcclusionMipScale:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AmbientOcclusionMipThreshold:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LPVIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LPVWarpIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LPVSize:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LPVSecondaryOcclusionIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LPVSecondaryBounceIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LPVGeometryVolumeBias:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LPVVplInjectionBias:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LPVEmissiveInjectionIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_LPVTransmissionIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_IndirectLightingColor:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_IndirectLightingIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_ColorGradingIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_ColorGradingLUT:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldFocalDistance:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldFocalRegion:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldNearTransitionRegion:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldFarTransitionRegion:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldScale:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldMaxBokehSize:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldNearBlurSize:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldFarBlurSize:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldMethod:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldBokehShape:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldOcclusion:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldColorThreshold:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldSizeThreshold:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_DepthOfFieldSkyFocusDistance:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_MotionBlurAmount:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_MotionBlurMax:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_MotionBlurPerObjectSize:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_ScreenPercentage:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_AntiAliasingMethod:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_ScreenSpaceReflectionIntensity:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_ScreenSpaceReflectionQuality:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_ScreenSpaceReflectionMaxRoughness:1;

	UPROPERTY(BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint32 bOverride_ScreenSpaceReflectionRoughnessScale:1;

	// -----------------------------------------------------------------------


	UPROPERTY(interp, Category=Film, AdvancedDisplay, meta=(editcondition = "bOverride_FilmWhitePoint", DisplayName = "Tint"))
	FLinearColor FilmWhitePoint;
	UPROPERTY(interp, Category=Film, meta=(editcondition = "bOverride_FilmShadowTint", DisplayName = "Tint Shadow"))
	FLinearColor FilmShadowTint;
	UPROPERTY(interp, Category=Film, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmShadowTintBlend", DisplayName = "Tint Shadow Blend"))
	float FilmShadowTintBlend;
	UPROPERTY(interp, Category=Film, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmShadowTintAmount", DisplayName = "Tint Shadow Amount"))
	float FilmShadowTintAmount;

	UPROPERTY(interp, Category=Film, meta=(UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_FilmSaturation", DisplayName = "Saturation"))
	float FilmSaturation;
	UPROPERTY(interp, Category=Film, AdvancedDisplay, meta=(editcondition = "bOverride_FilmChannelMixerRed", DisplayName = "Channel Mixer Red"))
	FLinearColor FilmChannelMixerRed;
	UPROPERTY(interp, Category=Film, AdvancedDisplay, meta=(editcondition = "bOverride_FilmChannelMixerGreen", DisplayName = "Channel Mixer Green"))
	FLinearColor FilmChannelMixerGreen;
	UPROPERTY(interp, Category=Film, AdvancedDisplay, meta=(editcondition = "bOverride_FilmChannelMixerBlue", DisplayName = " Channel Mixer Blue"))
	FLinearColor FilmChannelMixerBlue;

	UPROPERTY(interp, Category=Film, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmContrast", DisplayName = "Contrast"))
	float FilmContrast;
	UPROPERTY(interp, Category=Film, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmToeAmount", DisplayName = "Crush Shadows"))
	float FilmToeAmount;
	UPROPERTY(interp, Category=Film, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmHealAmount", DisplayName = "Crush Highlights"))
	float FilmHealAmount;
	UPROPERTY(interp, Category=Film, AdvancedDisplay, meta=(UIMin = "1.0", UIMax = "4.0", editcondition = "bOverride_FilmDynamicRange", DisplayName = "Dynamic Range"))
	float FilmDynamicRange;



	/** Scene tint color */
	UPROPERTY(interp, Category=SceneColor, AdvancedDisplay, meta=(editcondition = "bOverride_SceneColorTint", HideAlphaChannel))
	FLinearColor SceneColorTint;
	
	/** in percent, Scene chromatic aberration / color fringe (camera imperfection) to simulate an artifact that happens in real-world lens, mostly visible in the image corners. */
	UPROPERTY(interp, Category=SceneColor, meta=(UIMin = "0.0", UIMax = "5.0", editcondition = "bOverride_SceneFringeIntensity", DisplayName = "Fringe Intensity"))
	float SceneFringeIntensity;

	/** 0..1, Scene chromatic aberration / color fringe (camera imperfection) to simulate an artifact that happens in real-world lens, mostly visible in the image corners. */
	UPROPERTY(interp, Category=SceneColor, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "1.0", editcondition = "bOverride_SceneFringeSaturation", DisplayName = "Fringe Saturation"))
	float SceneFringeSaturation;

	/** Multiplier for all bloom contributions >=0: off, 1(default), >1 brighter */
	UPROPERTY(interp, Category=Bloom, meta=(ClampMin = "0.0", UIMax = "8.0", editcondition = "bOverride_BloomIntensity", DisplayName = "Intensity"))
	float BloomIntensity;

	/**
	 * minimum brightness the bloom starts having effect
	 * -1:all pixels affect bloom equally (dream effect), 0:all pixels affect bloom brights more, 1(default), >1 brighter
	 */
	UPROPERTY(interp, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "-1.0", UIMin = "0.0", UIMax = "8.0", editcondition = "bOverride_BloomThreshold", DisplayName = "Threshold"))
	float BloomThreshold;

	/**
	 * Diameter size for the Bloom1 in percent of the screen width
	 * (is done in 1/2 resolution, larger values cost more performance, good for high frequency details)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "4.0", editcondition = "bOverride_Bloom1Size", DisplayName = "#1 Size"))
	float Bloom1Size;
	/**
	 * Diameter size for Bloom2 in percent of the screen width
	 * (is done in 1/4 resolution, larger values cost more performance)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "8.0", editcondition = "bOverride_Bloom2Size", DisplayName = "#2 Size"))
	float Bloom2Size;
	/**
	 * Diameter size for Bloom3 in percent of the screen width
	 * (is done in 1/8 resolution, larger values cost more performance)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "16.0", editcondition = "bOverride_Bloom3Size", DisplayName = "#3 Size"))
	float Bloom3Size;
	/**
	 * Diameter size for Bloom4 in percent of the screen width
	 * (is done in 1/16 resolution, larger values cost more performance, best for wide contributions)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "32.0", editcondition = "bOverride_Bloom4Size", DisplayName = "#4 Size"))
	float Bloom4Size;
	/**
	 * Diameter size for Bloom5 in percent of the screen width
	 * (is done in 1/32 resolution, larger values cost more performance, best for wide contributions)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, Category=Bloom, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "64.0", editcondition = "bOverride_Bloom5Size", DisplayName = "#5 Size"))
	float Bloom5Size;

	/** Bloom1 tint color */
	UPROPERTY(interp, Category=Bloom, AdvancedDisplay, meta=(editcondition = "bOverride_Bloom1Tint", DisplayName = "#1 Tint", HideAlphaChannel))
	FLinearColor Bloom1Tint;
	/** Bloom2 tint color */
	UPROPERTY(interp, Category=Bloom, AdvancedDisplay, meta=(editcondition = "bOverride_Bloom2Tint", DisplayName = "#2 Tint", HideAlphaChannel))
	FLinearColor Bloom2Tint;
	/** Bloom3 tint color */
	UPROPERTY(interp, Category=Bloom, AdvancedDisplay, meta=(editcondition = "bOverride_Bloom3Tint", DisplayName = "#3 Tint", HideAlphaChannel))
	FLinearColor Bloom3Tint;
	/** Bloom4 tint color */
	UPROPERTY(interp, Category=Bloom, AdvancedDisplay, meta=(editcondition = "bOverride_Bloom4Tint", DisplayName = "#4 Tint", HideAlphaChannel))
	FLinearColor Bloom4Tint;
	/** Bloom5 tint color */
	UPROPERTY(interp, Category=Bloom, AdvancedDisplay, meta=(editcondition = "bOverride_Bloom5Tint", DisplayName = "#5 Tint", HideAlphaChannel))
	FLinearColor Bloom5Tint;

	/** BloomDirtMask intensity */
	UPROPERTY(interp, Category=Bloom, meta=(ClampMin = "0.0", UIMax = "8.0", editcondition = "bOverride_BloomDirtMaskIntensity", DisplayName = "Dirt Mask Intensity"))
	float BloomDirtMaskIntensity;

	/** BloomDirtMask tint color */
	UPROPERTY(interp, Category=Bloom, AdvancedDisplay, meta=(editcondition = "bOverride_BloomDirtMaskTint", DisplayName = "Dirt Mask Tint"))
	FLinearColor BloomDirtMaskTint;

	/**
	 * Texture that defines the dirt on the camera lens where the light of very bright objects is scattered.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Bloom, meta=(editcondition = "bOverride_BloomDirtMask", DisplayName = "Dirt Mask"))
	class UTexture* BloomDirtMask;	// The plan is to replace this texture with many small textures quads for better performance, more control and to animate the effect.

	/** How strong the dynamic GI from the LPV should be. 0.0 is off, 1.0 is the "normal" value, but higher values can be used to boost the effect*/
	UPROPERTY(interp, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVIntensity", UIMin = "0", UIMax = "20", DisplayName = "Intensity") )
	float LPVIntensity;

	/** CURRENTLY DISABLED - The strength of the warp offset for reducing light bleeding. 0.0 is off, 1.0 is the "normal" value, but higher values can be used to boost the effect*/
	UPROPERTY(interp, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVWarpIntensity", UIMin = "0", UIMax = "1", DisplayName = "(DISABLED) Grid Warp Intensity") )
	float LPVWarpIntensity;

	/** Bias applied to light injected into the LPV in cell units. Increase to reduce bleeding through thin walls*/
	UPROPERTY(interp, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVVplInjectionBias", UIMin = "0", UIMax = "2", DisplayName = "Light Injection Bias") )
	float LPVVplInjectionBias;

	/** The size of the LPV volume, in Unreal units*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVSize", UIMin = "100", UIMax = "20000", DisplayName = "Size") )
	float LPVSize;

	/** Secondary occlusion strength (bounce light shadows). Set to 0 to disable*/
	UPROPERTY(interp, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVSecondaryOcclusionIntensity", UIMin = "0", UIMax = "1", DisplayName = "Secondary Occlusion Intensity") )
	float LPVSecondaryOcclusionIntensity;

	/** Secondary bounce light strength (bounce light shadows). Set to 0 to disable*/
	UPROPERTY(interp, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVSecondaryBounceIntensity", UIMin = "0", UIMax = "1", DisplayName = "Secondary Bounce Intensity") )
	float LPVSecondaryBounceIntensity;

	/** Bias applied to the geometry volume in cell units. Increase to reduce darkening due to secondary occlusion */
	UPROPERTY(interp, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVGeometryVolumeBias", UIMin = "0", UIMax = "2", DisplayName = "Geometry Volume Bias") )
	float LPVGeometryVolumeBias;

	UPROPERTY(interp, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVEmissiveInjectionIntensity", UIMin = "0", UIMax = "20", DisplayName = "Emissive Injection Intensity") )
	float LPVEmissiveInjectionIntensity;

	/** How strong light transmission from the LPV should be. 0.0 is off, 1.0 is the "normal" value, but higher values can be used to boost the effect*/
	UPROPERTY(interp, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVTransmissionIntensity", UIMin = "0", UIMax = "2", DisplayName = "Transmission Intensity") )
	float LPVTransmissionIntensity;

	/** AmbientCubemap tint color */
	UPROPERTY(interp, Category=AmbientCubemap, AdvancedDisplay, meta=(editcondition = "bOverride_AmbientCubemapTint", DisplayName = "Tint"))
	FLinearColor AmbientCubemapTint;
	/**
	 * To scale the Ambient cubemap brightness
	 * >=0: off, 1(default), >1 brighter
	 */
	UPROPERTY(interp, Category=AmbientCubemap, meta=(ClampMin = "0.0", UIMax = "4.0", editcondition = "bOverride_AmbientCubemapIntensity", DisplayName = "Intensity"))
	float AmbientCubemapIntensity;
	/** The Ambient cubemap (Affects diffuse and specular shading), blends additively which if different from all other settings here */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AmbientCubemap, meta=(DisplayName = "Cubemap Texture"))
	class UTextureCube* AmbientCubemap;

	/**
	 * The eye adaptation will adapt to a value extracted from the luminance histogram of the scene color.
	 * The value is defined as having x percent below this brightness. Higher values give bright spots on the screen more priority
	 * but can lead to less stable results. Lower values give the medium and darker values more priority but might cause burn out of
	 * bright spots.
	 * >0, <100, good values are in the range 70 .. 80
	 */
	UPROPERTY(interp, Category=AutoExposure, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "100.0", editcondition = "bOverride_AutoExposureLowPercent", DisplayName = "Low Percent"))
	float AutoExposureLowPercent;

	/**
	 * The eye adaptation will adapt to a value extracted from the luminance histogram of the scene color.
	 * The value is defined as having x percent below this brightness. Higher values give bright spots on the screen more priority
	 * but can lead to less stable results. Lower values give the medium and darker values more priority but might cause burn out of
	 * bright spots.
	 * >0, <100, good values are in the range 80 .. 95
	 */
	UPROPERTY(interp, Category=AutoExposure, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "100.0", editcondition = "bOverride_AutoExposureHighPercent", DisplayName = "High Percent"))
	float AutoExposureHighPercent;

	/**
	 * A good value should be positive near 0. This is the minimum brightness the auto exposure can adapt to.
	 * It should be tweaked in a dark lighting situation (too small: image appears too bright, too large: image appears too dark).
	 * Note: Tweaking emissive materials and lights or tweaking auto exposure can look the same. Tweaking auto exposure has global
	 * effect and defined the HDR range - you don't want to change that late in the project development.
	 * Eye Adaptation is disabled if MinBrightness = MaxBrightness
	 */
	UPROPERTY(interp, Category=AutoExposure, meta=(ClampMin = "0.0", UIMax = "10.0", editcondition = "bOverride_AutoExposureMinBrightness", DisplayName = "Min Brightness"))
	float AutoExposureMinBrightness;

	/**
	 * A good value should be positive (2 is a good value). This is the maximum brightness the auto exposure can adapt to.
	 * It should be tweaked in a bright lighting situation (too small: image appears too bright, too large: image appears too dark).
	 * Note: Tweaking emissive materials and lights or tweaking auto exposure can look the same. Tweaking auto exposure has global
	 * effect and defined the HDR range - you don't want to change that late in the project development.
	 * Eye Adaptation is disabled if MinBrightness = MaxBrightness
	 */
	UPROPERTY(interp, Category=AutoExposure, meta=(ClampMin = "0.0", UIMax = "10.0", editcondition = "bOverride_AutoExposureMaxBrightness", DisplayName = "Max Brightness"))
	float AutoExposureMaxBrightness;

	/** >0 */
	UPROPERTY(interp, Category=AutoExposure, AdvancedDisplay, meta=(ClampMin = "0.02", UIMax = "20.0", editcondition = "bOverride_AutoExposureSpeedUp", DisplayName = "Speed Up"))
	float AutoExposureSpeedUp;

	/** >0 */
	UPROPERTY(interp, Category=AutoExposure, AdvancedDisplay, meta=(ClampMin = "0.02", UIMax = "20.0", editcondition = "bOverride_AutoExposureSpeedDown", DisplayName = "Speed Down"))
	float AutoExposureSpeedDown;

	/**
	 * Logarithmic adjustment for the exposure. Only used if a tonemapper is specified.
	 * 0: no adjustment, -1:2x darker, -2:4x darker, 1:2x brighter, 2:4x brighter, ...
	 */
	UPROPERTY(interp, Category=AutoExposure, meta=(UIMin = "-8.0", UIMax = "8.0", editcondition = "bOverride_AutoExposureBias"))
	float AutoExposureBias;

	/** temporary exposed until we found good values, -8: 1/256, -10: 1/1024 */
	UPROPERTY(interp, Category=AutoExposure, AdvancedDisplay, meta=(UIMin = "-16", UIMax = "0.0", editcondition = "bOverride_HistogramLogMin"))
	float HistogramLogMin;

	/** temporary exposed until we found good values 4: 16, 8: 256 */
	UPROPERTY(interp, Category=AutoExposure, AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "16.0", editcondition = "bOverride_HistogramLogMax"))
	float HistogramLogMax;

	/** Brightness scale of the image cased lens flares (linear) */
	UPROPERTY(interp, Category=LensFlares, meta=(UIMin = "0.0", UIMax = "16.0", editcondition = "bOverride_LensFlareIntensity", DisplayName = "Intensity"))
	float LensFlareIntensity;

	/** Tint color for the image based lens flares. */
	UPROPERTY(interp, Category=LensFlares, AdvancedDisplay, meta=(editcondition = "bOverride_LensFlareTint", DisplayName = "Tint"))
	FLinearColor LensFlareTint;

	/** Size of the Lens Blur (in percent of the view width) that is done with the Bokeh texture (note: performance cost is radius*radius) */
	UPROPERTY(interp, Category=LensFlares, meta=(UIMin = "0.0", UIMax = "32.0", editcondition = "bOverride_LensFlareBokehSize", DisplayName = "BokehSize"))
	float LensFlareBokehSize;

	/** Minimum brightness the lens flare starts having effect (this should be as high as possible to avoid the performance cost of blurring content that is too dark too see) */
	UPROPERTY(interp, Category=LensFlares, AdvancedDisplay, meta=(UIMin = "0.1", UIMax = "32.0", editcondition = "bOverride_LensFlareThreshold", DisplayName = "Threshold"))
	float LensFlareThreshold;

	/** Defines the shape of the Bokeh when the image base lens flares are blurred, cannot be blended */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LensFlares, meta=(editcondition = "bOverride_LensFlareBokehShape", DisplayName = "BokehShape"))
	class UTexture* LensFlareBokehShape;

	/** RGB defines the lens flare color, A it's position. This is a temporary solution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LensFlares, meta=(editcondition = "bOverride_LensFlareTints", DisplayName = "Tints"))
	FLinearColor LensFlareTints[8];

	/** 0..1 0=off/no vignette .. 1=strong vignette */
	UPROPERTY(interp, Category=SceneColor, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_VignetteIntensity"))
	float VignetteIntensity;

	/** Vignette color. */
	UPROPERTY(interp, Category=SceneColor, meta=(editcondition = "bOverride_VignetteColor", DisplayName = "Vignette Color"))
	FLinearColor VignetteColor;

	/** 0..1 grain jitter */
	UPROPERTY(interp, Category=SceneColor, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_GrainJitter"))
	float GrainJitter;

	/** 0..1 grain intensity */
	UPROPERTY(interp, Category=SceneColor, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_GrainIntensity"))
	float GrainIntensity;

	/** 0..1 0=off/no ambient occlusion .. 1=strong ambient occlusion, defines how much it affects the non direct lighting after base pass */
	UPROPERTY(interp, Category=AmbientOcclusion, meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_AmbientOcclusionIntensity", DisplayName = "Intensity"))
	float AmbientOcclusionIntensity;

	/** 0..1 0=no effect on static lighting .. 1=AO affects the stat lighting, 0 is free meaning no extra rendering pass */
	UPROPERTY(interp, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_AmbientOcclusionStaticFraction", DisplayName = "Static Fraction"))
	float AmbientOcclusionStaticFraction;

	/** >0, in unreal units, bigger values means even distant surfaces affect the ambient occlusion */
	UPROPERTY(interp, Category=AmbientOcclusion, meta=(ClampMin = "0.1", UIMax = "200.0", editcondition = "bOverride_AmbientOcclusionRadius", DisplayName = "Radius"))
	float AmbientOcclusionRadius;

	/** true: AO radius is in world space units, false: AO radius is locked the view space in 400 units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AmbientOcclusion, AdvancedDisplay, meta=(editcondition = "bOverride_AmbientOcclusionRadiusInWS", DisplayName = "Radius in WorldSpace"))
	uint32 AmbientOcclusionRadiusInWS:1;

	/** >0, in unreal units, at what distance the AO effect disppears in the distance (avoding artifacts and AO effects on huge object) */
	UPROPERTY(interp, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "20000.0", editcondition = "bOverride_AmbientOcclusionFadeDistance", DisplayName = "Fade Out Distance"))
	float AmbientOcclusionFadeDistance;
	
	/** >0, in unreal units, how many units before AmbientOcclusionFadeOutDistance it starts fading out */
	UPROPERTY(interp, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "20000.0", editcondition = "bOverride_AmbientOcclusionFadeRadius", DisplayName = "Fade Out Radius"))
	float AmbientOcclusionFadeRadius;

	/** >0, in unreal units, how wide the ambient occlusion effect should affect the geometry (in depth) */
	UPROPERTY(interp, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "2000.0", editcondition = "bOverride_AmbientOcclusionDistance", DisplayName = "Occlusion Distance"))
	float AmbientOcclusionDistance;

	/** >0, in unreal units, bigger values means even distant surfaces affect the ambient occlusion */
	UPROPERTY(interp, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.1", UIMax = "4.0", editcondition = "bOverride_AmbientOcclusionPower", DisplayName = "Power"))
	float AmbientOcclusionPower;

	/** >0, in unreal units, default (3.0) works well for flat surfaces but can reduce details */
	UPROPERTY(interp, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "10.0", editcondition = "bOverride_AmbientOcclusionBias", DisplayName = "Bias"))
	float AmbientOcclusionBias;

	/** 0=lowest quality..100=maximum quality, only a few quality levels are implemented, no soft transition */
	UPROPERTY(interp, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "100.0", editcondition = "bOverride_AmbientOcclusionQuality", DisplayName = "Quality"))
	float AmbientOcclusionQuality;

	/** Affects the blend over the multiple mips (lower resolution versions) , 0:fully use full resolution, 1::fully use low resolution, around 0.6 seems to be a good value */
	UPROPERTY(interp, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.1", UIMax = "1.0", editcondition = "bOverride_AmbientOcclusionMipBlend", DisplayName = "Mip Blend"))
	float AmbientOcclusionMipBlend;

	/** Affects the radius AO radius scale over the multiple mips (lower resolution versions) */
	UPROPERTY(interp, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.5", UIMax = "4.0", editcondition = "bOverride_AmbientOcclusionMipScale", DisplayName = "Mip Scale"))
	float AmbientOcclusionMipScale;

	/** to tweak the bilateral upsampling when using multiple mips (lower resolution versions) */
	UPROPERTY(interp, Category=AmbientOcclusion, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "0.1", editcondition = "bOverride_AmbientOcclusionMipThreshold", DisplayName = "Mip Threshold"))
	float AmbientOcclusionMipThreshold;

	/** Adjusts indirect lighting color. (1,1,1) is default. (0,0,0) to disable GI. The show flag 'Global Illumination' must be enabled to use this property. */
	UPROPERTY(interp, Category=GlobalIllumination, AdvancedDisplay, meta=(editcondition = "bOverride_IndirectLightingColor", DisplayName = "Indirect Lighting Color"))
	FLinearColor IndirectLightingColor;

	/** Scales the indirect lighting contribution. A value of 0 disables GI. Default is 1. The show flag 'Global Illumination' must be enabled to use this property. */
	UPROPERTY(interp, Category=GlobalIllumination, AdvancedDisplay, meta=(ClampMin = "0", UIMax = "4.0", editcondition = "bOverride_IndirectLightingIntensity", DisplayName = "Indirect Lighting Intensity"))
	float IndirectLightingIntensity;

	/** 0..1=full intensity */
	UPROPERTY(interp, Category=SceneColor, meta=(ClampMin = "0", ClampMax = "1.0", editcondition = "bOverride_ColorGradingIntensity", DisplayName = "Color Grading Intensity"))
	float ColorGradingIntensity;

	/** Name of the LUT texture e.g. MyPackage01.LUTNeutral, empty if not used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneColor, meta=(editcondition = "bOverride_ColorGradingLUT", DisplayName = "Color Grading"))
	class UTexture* ColorGradingLUT;

	/** BokehDOF, Simple gaussian, ... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DepthOfField, meta=(editcondition = "bOverride_DepthOfFieldMethod", DisplayName = "Method"))
	TEnumAsByte<enum EDepthOfFieldMethod> DepthOfFieldMethod;

	/** Distance in which the Depth of Field effect should be sharp, in unreal units (cm) */
	UPROPERTY(interp, Category=DepthOfField, meta=(UIMin = "0.0", UIMax = "10000.0", editcondition = "bOverride_DepthOfFieldFocalDistance", DisplayName = "Focal Distance"))
	float DepthOfFieldFocalDistance;

	/** Artificial region where all content is in focus, starting after DepthOfFieldFocalDistance, in unreal units  (cm) */
	UPROPERTY(interp, Category=DepthOfField, meta=(UIMin = "0.0", UIMax = "10000.0", editcondition = "bOverride_DepthOfFieldFocalRegion", DisplayName = "Focal Region"))
	float DepthOfFieldFocalRegion;

	/** To define the width of the transition region next to the focal region on the near side (cm) */
	UPROPERTY(interp, Category=DepthOfField, meta=(UIMin = "0.0", UIMax = "10000.0", editcondition = "bOverride_DepthOfFieldNearTransitionRegion", DisplayName = "Near Transition Region"))
	float DepthOfFieldNearTransitionRegion;

	/** To define the width of the transition region next to the focal region on the near side (cm) */
	UPROPERTY(interp, Category=DepthOfField, meta=(UIMin = "0.0", UIMax = "10000.0", editcondition = "bOverride_DepthOfFieldFarTransitionRegion", DisplayName = "Far Transition Region"))
	float DepthOfFieldFarTransitionRegion;

	/** BokehDOF only: To amplify the depth of field effect (like aperture)  0=off */
	UPROPERTY(interp, Category=DepthOfField, meta=(ClampMin = "0.0", ClampMax = "2.0", editcondition = "bOverride_DepthOfFieldScale", DisplayName = "Scale"))
	float DepthOfFieldScale;

	/** BokehDOF only: Maximum size of the Depth of Field blur (in percent of the view width) (note: performance cost scales with size*size) */
	UPROPERTY(interp, Category=DepthOfField, meta=(UIMin = "0.0", UIMax = "32.0", editcondition = "bOverride_DepthOfFieldMaxBokehSize", DisplayName = "Max Bokeh Size"))
	float DepthOfFieldMaxBokehSize;

	/** Gaussian only: Maximum size of the Depth of Field blur (in percent of the view width) (note: performance cost scales with size) */
	UPROPERTY(interp, Category=DepthOfField, meta=(UIMin = "0.0", UIMax = "32.0", editcondition = "bOverride_DepthOfFieldNearBlurSize", DisplayName = "Near Blur Size"))
	float DepthOfFieldNearBlurSize;

	/** Gaussian only: Maximum size of the Depth of Field blur (in percent of the view width) (note: performance cost scales with size) */
	UPROPERTY(interp, Category=DepthOfField, meta=(UIMin = "0.0", UIMax = "32.0", editcondition = "bOverride_DepthOfFieldFarBlurSize", DisplayName = "Far Blur Size"))
	float DepthOfFieldFarBlurSize;

	/** Defines the shape of the Bokeh when object get out of focus, cannot be blended */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=DepthOfField, meta=(editcondition = "bOverride_DepthOfFieldBokehShape", DisplayName = "Shape"))
	class UTexture* DepthOfFieldBokehShape;

	/** Occlusion tweak factor 1 (0.18 to get natural occlusion, 0.4 to solve layer color leaking issues) */
	UPROPERTY(interp, Category=DepthOfField, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_DepthOfFieldOcclusion", DisplayName = "Occlusion"))
	float DepthOfFieldOcclusion;
	
	/** Color threshold to do full quality DOF */
	UPROPERTY(interp, Category=DepthOfField, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "10.0", editcondition = "bOverride_DepthOfFieldColorThreshold", DisplayName = "Color Threshold"))
	float DepthOfFieldColorThreshold;

	/** Size threshold to do full quality DOF */
	UPROPERTY(interp, Category=DepthOfField, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_DepthOfFieldSizeThreshold", DisplayName = "Size Threshold"))
	float DepthOfFieldSizeThreshold;
	
	/** Artificial distance to allow the skybox to be in focus (e.g. 200000), <=0 to switch the feature off, only for GaussianDOF, can cost performance */
	UPROPERTY(interp, Category=DepthOfField, AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "200000.0", editcondition = "bOverride_DepthOfFieldSkyFocusDistance", DisplayName = "Sky Distance"))
	float DepthOfFieldSkyFocusDistance;
	
	/** Strength of motion blur, 0:off, should be renamed to intensity */
	UPROPERTY(interp, Category=MotionBlur, meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_MotionBlurAmount", DisplayName = "Amount"))
	float MotionBlurAmount;
	/** max distortion caused by motion blur, in percent of the screen width, 0:off */
	UPROPERTY(interp, Category=MotionBlur, meta=(ClampMin = "0.0", ClampMax = "100.0", editcondition = "bOverride_MotionBlurMax", DisplayName = "Max"))
	float MotionBlurMax;
	/** The minimum projected screen radius for a primitive to be drawn in the velocity pass, percentage of screen width. smaller numbers cause more draw calls, default: 4% */
	UPROPERTY(interp, Category=MotionBlur, AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "100.0", editcondition = "bOverride_MotionBlurPerObjectSize", DisplayName = "Per Object Size"))
	float MotionBlurPerObjectSize;

	/** to render with lower resolution and upscale, controlled by console variable, 100:off, needs to be <99 to see effect, only applied in game  */
	UPROPERTY(interp, Category=Misc, meta=(ClampMin = "0.0", ClampMax = "400.0", editcondition = "bOverride_ScreenPercentage"))
	float ScreenPercentage;

	/** TemporalAA, FXAA, ... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=Misc, meta=(editcondition = "bOverride_AntiAliasingMethod", DisplayName = "AA Method"))
	TEnumAsByte<enum EAntiAliasingMethod> AntiAliasingMethod;

	/** Enable/Fade/disable the Screen Space Reflection feature, in percent, avoid numbers between 0 and 1 fo consistency */
	UPROPERTY(interp, Category=ScreenSpaceReflections, meta=(ClampMin = "0.0", ClampMax = "100.0", editcondition = "bOverride_ScreenSpaceReflectionIntensity", DisplayName = "Intensity"))
	float ScreenSpaceReflectionIntensity;

	/** 0=lowest quality..100=maximum quality, only a few quality levels are implemented, no soft transition, 50 is the default for better performance. */
	UPROPERTY(interp, Category=ScreenSpaceReflections, meta=(ClampMin = "0.0", UIMax = "100.0", editcondition = "bOverride_ScreenSpaceReflectionQuality", DisplayName = "Quality"))
	float ScreenSpaceReflectionQuality;

	/** Until what roughness we fade the screen space reflections, 0.8 works well, smaller can run faster */
	UPROPERTY(interp, Category=ScreenSpaceReflections, meta=(ClampMin = "0.01", ClampMax = "1.0", editcondition = "bOverride_ScreenSpaceReflectionMaxRoughness", DisplayName = "Max Roughness"))
	float ScreenSpaceReflectionMaxRoughness;

	// Note: Adding properties before this line require also changes to the OverridePostProcessSettings() function and 
	// FPostProcessSettings constructor and possibly the SetBaseValues() method.
	// -----------------------------------------------------------------------
	
	/** Allows custom post process materials to be defined,		using a MaterialInstance with the same Material as its parent to allow blending. Make sure you use the "PostProcess" domain type */
	UPROPERTY(EditAnywhere, Category=Misc)
	TArray<UObject*> Blendables;

	// good start values for a new volume, by default no value is overriding
	FPostProcessSettings()
	{
		// to set all bOverride_.. by default to false
		FMemory::Memzero(this, sizeof(FPostProcessSettings));

		// default values:
		FilmWhitePoint = FLinearColor(1.0f,1.0f,1.0f);
		FilmSaturation = 1.0f;
		FilmChannelMixerRed = FLinearColor(1.0f,0.0f,0.0f);
		FilmChannelMixerGreen = FLinearColor(0.0f,1.0f,0.0f);
		FilmChannelMixerBlue = FLinearColor(0.0f,0.0f,1.0f);
		FilmContrast = 0.03f;
		FilmDynamicRange = 4.0f;
		FilmHealAmount = 0.18f;
		FilmToeAmount = 1.0f;
		FilmShadowTint = FLinearColor(1.0f,1.0f,1.0f);
		FilmShadowTintBlend = 0.5;
		FilmShadowTintAmount = 0.0;

		SceneColorTint = FLinearColor(1, 1, 1);
		SceneFringeIntensity = 0.0f;
		SceneFringeSaturation = 0.5f;
		BloomIntensity = 1.0f;
		BloomThreshold = 1.0f;
		Bloom1Tint = FLinearColor(0.5f, 0.5f, 0.5f);
		Bloom1Size = 1.0f;
		Bloom2Tint = FLinearColor(0.5f, 0.5f, 0.5f);
		Bloom2Size = 4.0f;
		Bloom3Tint = FLinearColor(0.5f, 0.5f, 0.5f);
		Bloom3Size = 16.0f;
		Bloom4Tint = FLinearColor(0.5f, 0.5f, 0.5f);
		Bloom4Size = 32.0f;
		Bloom5Tint = FLinearColor(0.5f, 0.5f, 0.5f);
		Bloom5Size = 100.0f;
		BloomDirtMaskIntensity = 1.0f;
		BloomDirtMaskTint = FLinearColor(0.5f, 0.5f, 0.5f);
		AmbientCubemapIntensity = 1.0f;
		AmbientCubemapTint = FLinearColor(1, 1, 1);
		LPVIntensity = 1.0f;
		LPVWarpIntensity = 0.0f;
		LPVSize = 5312.0f;
		LPVSecondaryOcclusionIntensity = 0.0f;
		LPVSecondaryBounceIntensity = 0.0f;
		LPVVplInjectionBias = 0.64f;
		LPVGeometryVolumeBias = 0.384f;
		LPVEmissiveInjectionIntensity = 1.0f;
		LPVTransmissionIntensity = 1.0f;
		AutoExposureLowPercent = 80.0f;
		AutoExposureHighPercent = 98.3f;
		AutoExposureMinBrightness = 0.03f;
		AutoExposureMaxBrightness = 2.0f;
		AutoExposureBias = 0.0f;
		AutoExposureSpeedUp = 3.0f;
		AutoExposureSpeedDown = 1.0f;
		HistogramLogMin = -8.0f;
		HistogramLogMax = 4.0f;
		LensFlareIntensity = 1.0f;
		LensFlareTint = FLinearColor(1.0f, 1.0f, 1.0f);
		LensFlareBokehSize = 3.0f;
		LensFlareThreshold = 8.0f;
		VignetteIntensity = 0.0f;
		VignetteColor = FLinearColor(0.0f, 0.0f, 0.0f);
		GrainIntensity = 0.0f;
		GrainJitter = 0.0f;
		AmbientOcclusionIntensity = .5f;
		AmbientOcclusionStaticFraction = 1.0f;
		AmbientOcclusionRadius = 40.0f;
		AmbientOcclusionDistance = 80.0f;
		AmbientOcclusionFadeDistance = 8000.0f;
		AmbientOcclusionFadeRadius = 5000.0f;
		AmbientOcclusionPower = 2.0f;
		AmbientOcclusionBias = 3.0f;
		AmbientOcclusionQuality = 50.0f;
		AmbientOcclusionMipBlend = 0.6f;
		AmbientOcclusionMipScale = 1.7f;
		AmbientOcclusionMipThreshold = 0.01f;
		AmbientOcclusionRadiusInWS = false;
		IndirectLightingColor = FLinearColor(1.0f, 1.0f, 1.0f);
		IndirectLightingIntensity = 1.0f;
		ColorGradingIntensity = 1.0f;
		DepthOfFieldFocalDistance = 1000.0f;
		DepthOfFieldFocalRegion = 0.0f;
		DepthOfFieldNearTransitionRegion = 300.0f;
		DepthOfFieldFarTransitionRegion = 500.0f;
		DepthOfFieldScale = 0.0f;
		DepthOfFieldMaxBokehSize = 15.0f;
		DepthOfFieldNearBlurSize = 15.0f;
		DepthOfFieldFarBlurSize = 15.0f;
		DepthOfFieldOcclusion = 0.4f;
		DepthOfFieldColorThreshold = 1.0f;
		DepthOfFieldSizeThreshold = 0.08f;
		DepthOfFieldSkyFocusDistance = 0.0f;
		LensFlareTints[0] = FLinearColor(1.0f, 0.8f, 0.4f, 0.6f);
		LensFlareTints[1] = FLinearColor(1.0f, 1.0f, 0.6f, 0.53f);
		LensFlareTints[2] = FLinearColor(0.8f, 0.8f, 1.0f, 0.46f);
		LensFlareTints[3] = FLinearColor(0.5f, 1.0f, 0.4f, 0.39f);
		LensFlareTints[4] = FLinearColor(0.5f, 0.8f, 1.0f, 0.31f);
		LensFlareTints[5] = FLinearColor(0.9f, 1.0f, 0.8f, 0.27f);
		LensFlareTints[6] = FLinearColor(1.0f, 0.8f, 0.4f, 0.22f);
		LensFlareTints[7] = FLinearColor(0.9f, 0.7f, 0.7f, 0.15f);
		MotionBlurAmount = 0.5f;
		MotionBlurMax = 5.0f;
		MotionBlurPerObjectSize = 0.5f;
		ScreenPercentage = 100.0f;
		AntiAliasingMethod = AAM_TemporalAA;
		ScreenSpaceReflectionIntensity = 100.0f;
		ScreenSpaceReflectionQuality = 50.0f;
		ScreenSpaceReflectionMaxRoughness = 0.6f;
	}

	/**
		* Used to define the values before any override happens.
		* Should be as neutral as possible.
		*/		
	void SetBaseValues()
	{
		*this = FPostProcessSettings();

		AmbientCubemapIntensity = 0.0f;
		ColorGradingIntensity = 0.0f;
	}
};

UCLASS()
class UScene : public UObject
{
	GENERATED_UCLASS_BODY()


	/** bits needed to store DPG value */
	#define SDPG_NumBits	3
};