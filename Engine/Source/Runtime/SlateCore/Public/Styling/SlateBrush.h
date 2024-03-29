// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SlateBrush.h: Declares the FSlateBrush structure.
=============================================================================*/

#pragma once

#include "Margin.h"
#include "SlateColor.h"

#include "SlateBrush.generated.h"


/**
 * Enumerates ways in which an image can be drawn.
 */
UENUM()
namespace ESlateBrushDrawType
{
	enum Type
	{
		/** Don't do anything */
		NoDrawType,

		/** Draw a 3x3 box, where the sides and the middle stretch based on the Margin */
		Box,

		/** Draw a 3x3 border where the sides tile and the middle is empty */
		Border,

		/** Draw an image; margin is ignored */
		Image
	};
}


/**
 * Enumerates tiling options for image drawing.
 */
UENUM()
namespace ESlateBrushTileType
{
	enum Type
	{
		/** Just stretch */
		NoTile,

		/** Tile the image horizontally */
		Horizontal,

		/** Tile the image vertically */
		Vertical,

		/** Tile in both directions */
		Both
	};
}


/*
 * Enumerates brush image types.
 */
UENUM()
namespace ESlateBrushImageType
{
	enum Type
	{
		/** No image is loaded.  Color only brushes, transparent brushes etc */
		NoImage,

		/** The image to be loaded is in full color */
		FullColor,

		/** The image is a special texture in linear space (usually a rendering resource such as a lookup table)*/
		Linear,
	};
}


/**
 * An brush which contains information about how to draw a Slate element
 */
USTRUCT()
struct SLATECORE_API FSlateBrush
{
	GENERATED_USTRUCT_BODY()

	/** Size of the resource in Slate Units */
	UPROPERTY(EditAnywhere, Category=Brush)
	FVector2D ImageSize;

	/** How to draw the image */
	UPROPERTY(EditAnywhere, Category=Brush)
	TEnumAsByte<enum ESlateBrushDrawType::Type > DrawAs;

	/** The margin to use in Box and Border modes */
	UPROPERTY(EditAnywhere, Category=Brush, meta=(UVSpace="true"))
	FMargin Margin;

	/** Tinting applied to the image. */
	UPROPERTY()
	FLinearColor Tint_DEPRECATED;

	/** Tinting applied to the image. */
	UPROPERTY(EditAnywhere, Category=Brush, meta=(DisplayName="Tint"))
	FSlateColor TintColor;

	/** How to tile the image in Image mode */
	UPROPERTY(EditAnywhere, Category=Brush)
	TEnumAsByte<enum ESlateBrushTileType::Type> Tiling;

	/** The type of image */
	UPROPERTY()
	TEnumAsByte<enum ESlateBrushImageType::Type> ImageType;

public:

	/**
	 * Default constructor.
	 */
	FSlateBrush()
		: ImageSize(32.0f, 32.0f)
		, DrawAs(ESlateBrushDrawType::Image)
		, Margin(0.0f)
		, Tint_DEPRECATED(FLinearColor::White)
		, TintColor(FLinearColor::White)
		, Tiling(ESlateBrushTileType::NoTile)
		, ImageType(ESlateBrushImageType::NoImage)
		, ResourceObject(nullptr)
		, ResourceName(NAME_None)
		, bIsDynamicallyLoaded(false)
		, bHasUObject_DEPRECATED(false)
	{ }

public:

	/**
	 * Gets the name of the resource object, if any.
	 *
	 * @return Resource name, or NAME_None if the resource object is not set.
	 */
	const FName GetResourceName( ) const
	{
		return ((ResourceName != NAME_None) || (ResourceObject == nullptr))
			? ResourceName
			: ResourceObject->GetFName();
	}

	/**
	 * Gets the UObject that represents the brush resource, if any.
	 *
	 * Ensure that we only access the TextureObject as a UTexture2D.
	 *
	 * @return The resource object, or nullptr if it is not set.
	 */
	class UObject* GetResourceObject( ) const
	{
		return ResourceObject;
	}

	/**
	 * Gets the brush's tint color.
	 *
	 * @param InWidgetStyle The widget style to get the tint for.
	 *
	 * @return Tint color.
	 */
	FLinearColor GetTint( const FWidgetStyle& InWidgetStyle ) const
	{
		return TintColor.GetColor(InWidgetStyle);
	}

	/**
	 * Checks whether this brush has a UTexture object
	 *
	 * @return true if it has a UTexture object, false otherwise.
	 */
	bool HasUObject( ) const
	{
		return (ResourceObject != nullptr) || (bHasUObject_DEPRECATED);
	}

	/**
	 * Checks whether the brush resource is loaded dynamically.
	 *
	 * @return true if loaded dynamically, false otherwise.
	 */
	bool IsDynamicallyLoaded( ) const
	{
		return bIsDynamicallyLoaded;
	}

public:

	/**
	 * Gets the identifier for UObject based texture paths.
	 *
	 * @return Texture identifier string.
	 */
	static const FString UTextureIdentifier( );

protected:

	/** Pointer to the UTexture2D. Holding onto it as a UObject because UTexture2D is not available in Slate core. */
	UPROPERTY(EditAnywhere, Category=Brush, meta=(DisplayThumbnail="true", DisplayName="Texture"))
	UObject* ResourceObject;

	/** The name of the rendering resource to use */
	UPROPERTY()
	FName ResourceName;

	/** Whether or not the brush path is a path to a UObject */
	bool bIsDynamicallyLoaded;

	/** Whether or not the brush has a UTexture resource */
	bool bHasUObject_DEPRECATED;

protected:

	/** 
	 * This constructor is protected; use one of the deriving classes instead.
	 *
	 * @param InDrawType      How to draw the texture
	 * @param InResourceName  The name of the resource
	 * @param InMargin        Margin to use in border and box modes
	 * @param InTiling        Tile horizontally/vertically or both? (only in image mode)
	 * @param InImageType	  The type of image
	 * @param InTint		  Tint to apply to the element.
	 */
	 FORCENOINLINE FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const FLinearColor& InTint = FLinearColor::White, UObject* InObjectResource = nullptr, bool bInDynamicallyLoaded = false );

	 FORCENOINLINE FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const TSharedRef< FLinearColor >& InTint, UObject* InObjectResource = nullptr, bool bInDynamicallyLoaded = false );

	 FORCENOINLINE FSlateBrush( ESlateBrushDrawType::Type InDrawType, const FName InResourceName, const FMargin& InMargin, ESlateBrushTileType::Type InTiling, ESlateBrushImageType::Type InImageType, const FVector2D& InImageSize, const FSlateColor& InTint, UObject* InObjectResource = nullptr, bool bInDynamicallyLoaded = false );
};
