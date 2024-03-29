// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "LightComponentBase.generated.h"

UCLASS(abstract, HideCategories=(Trigger,Activation,"Components|Activation",Physics), ShowCategories=(Mobility))
class ENGINE_API ULightComponentBase : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/**
	 * GUID used to associate a light component with precomputed shadowing information across levels.
	 * The GUID changes whenever the light position changes.
	 */
	UPROPERTY()
	FGuid LightGuid;

	/**  */
	UPROPERTY()
	float Brightness_DEPRECATED;

	/** 
	 * Total energy that the light emits.  
	 * For point/spot lights with inverse squared falloff, this is in units of lumens.  1700 lumens corresponds to a 100W lightbulb. 
	 * For other lights, this is just a brightness multiplier. 
	 */
	UPROPERTY(BlueprintReadOnly, interp, Category=Light, meta=(DisplayName = "Intensity", UIMin = "0.0", UIMax = "20.0"))
	float Intensity;

	/** 
	 * Filter color of the light.
	 * Note that this can change the light's effective intensity.
	 */
	UPROPERTY(BlueprintReadOnly, interp, Category=Light, meta=(HideAlphaChannel))
	FColor LightColor;

	/** 
	 * Whether the light can affect the world, or whether it is disabled.
	 * A disabled light will not contribute to the scene in any way.  This setting cannot be changed at runtime and unbuilds lighting when changed.
	 * Setting this to false has the same effect as deleting the light, so it is useful for non-destructive experiments.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light)
	uint32 bAffectsWorld:1;

	/**
	 * Whether the light should cast any shadows.
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light)
	uint32 CastShadows:1;

	/**
	 * Whether the light should cast shadows from static objects.  Also requires Cast Shadows to be set to True.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, AdvancedDisplay)
	uint32 CastStaticShadows:1;

	/**
	 * Whether the light should cast shadows from dynamic objects.  Also requires Cast Shadows to be set to True.
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, AdvancedDisplay)
	uint32 CastDynamicShadows:1;

	/** Whether the light affects translucency or not.  Disabling this can save GPU time when there are many small lights. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, AdvancedDisplay)
	uint32 bAffectTranslucentLighting:1;

	/** The precomputed lighting for that light source is valid. It might become invalid if some properties change (e.g. position, brightness). */
	UPROPERTY()
	uint32 bPrecomputedLightingIsValid:1;

#if WITH_EDITORONLY_DATA
	/** Sprite for static light in the editor. */
	UPROPERTY(transient)
	UTexture2D* StaticEditorTexture;

	/** Sprite scaling for static light in the editor. */
	UPROPERTY(transient)
	float StaticEditorTextureScale;

	/** Sprite for dynamic light in the editor. */
	UPROPERTY(transient)
	UTexture2D* DynamicEditorTexture;

	/** Sprite scaling for dynamic light in the editor. */
	UPROPERTY(transient)
	float DynamicEditorTextureScale;
#endif

	virtual void Serialize(FArchive& Ar) OVERRIDE;

	/**
	 * Called after duplication & serialization and before PostLoad. Used to e.g. make sure GUIDs remains globally unique.
	 */
	virtual void PostDuplicate(bool bDuplicateForPIE);

#if WITH_EDITOR
	/** UObject interface */
	virtual void PostEditImport() OVERRIDE;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;

	/**
	* @return Path to the editor sprite for the light component class
	*/
	UTexture2D* GetEditorSprite() const
	{
		return (Mobility == EComponentMobility::Movable) ? DynamicEditorTexture : StaticEditorTexture;
	}

	/**
	* @return Uniform scaling factor for the sprite for the light component class
	*/
	float GetEditorSpriteScale() const
	{
		return (Mobility == EComponentMobility::Movable) ? DynamicEditorTextureScale : StaticEditorTextureScale;
	}

	/** Update the texture used on the editor sprite */
	virtual void UpdateLightSpriteTexture();
#endif

	/**
	 * Validate light GUIDs and resets as appropriate.
	 */
	void ValidateLightGUIDs();

	/**
	 * Update/reset light GUIDs.
	 */
	virtual void UpdateLightGUIDs();

	/**
	 * Return True if a light's parameters as well as its position is static during gameplay, and can thus use static lighting.
	 * A light with HasStaticLighting() == true will always have HasStaticShadowing() == true as well.
	 */
	bool HasStaticLighting() const;

	/** 
	 * Whether the light has static direct shadowing.  
	 * The light may still have dynamic brightness and color. 
	 * The light may or may not also have static lighting.
	 */
	bool HasStaticShadowing() const;

	/** UActorComponent Interface */
	virtual void OnRegister() OVERRIDE;

	/** We return a small bounds to allow us to non-interpenetrates when placing lights in the level. */
	virtual bool ShouldCollideWhenPlacing() const OVERRIDE;

	/** Get the extent used when placing this component in the editor, used for 'pulling back' hit. */
	virtual FBoxSphereBounds GetPlacementExtent() const OVERRIDE;

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	class UBillboardComponent* SpriteComponent;
#endif

};



