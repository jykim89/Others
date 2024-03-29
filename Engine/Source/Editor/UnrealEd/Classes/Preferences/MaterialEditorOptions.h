// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 *
 * A configuration class used by the UMaterial Editor to save editor
 * settings across sessions.
 */

#pragma once
#include "MaterialEditorOptions.generated.h"

UCLASS(hidecategories=Object, config=EditorUserSettings)
class UNREALED_API UMaterialEditorOptions : public UObject
{
	GENERATED_UCLASS_BODY()

	/** If true, render grid the preview scene. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bShowGrid:1;

	/** If true, render background object in the preview scene. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bShowBackground:1;

	/** If true, don't render connectors that are not connected to anything. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bHideUnusedConnectors:1;

	/** If true, draw connections with splines.  If false, use straight lines. */
	UPROPERTY(config)
	uint32 bDrawCurves_DEPRECATED:1;

	/** If true, the 3D material preview viewport updates in realtime. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bRealtimeMaterialViewport:1;

	/** If true, the linked object viewport updates in realtime. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bRealtimeExpressionViewport:1;

	/** If true, always refresh all expression previews. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bAlwaysRefreshAllPreviews:1;

	/** If true, use expression categorized menus. */
	UPROPERTY(config)
	uint32 bUseSortedMenus_DEPRECATED:1;

	/** If false, use expression categorized menus. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bUseUnsortedMenus:1;

	/** If true, show mobile statis and errors. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bShowMobileStats:1;

	/** If true, stats use release shaders. */
	UPROPERTY(EditAnywhere, config, Category = Options)
	uint32 bReleaseStats : 1;

	/** If true, show stats for an blank copy (no graph) of the preview material. Helps identify cost of material graph. Incurs slight cost to build/iteration times. */
	UPROPERTY(EditAnywhere, config, Category = Options)
	uint32 bShowBuiltinStats : 1;

	/** The users favorite material expressions. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	TArray<FString> FavoriteExpressions;

};

