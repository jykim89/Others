// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SlateBoxBrush.h: Declares the FSlateBoxBrush structure.
=============================================================================*/

#pragma once


/**
 * A 3x3 box where the sides stretch horizontally and vertically and the middle stretches to fill.
 * The corners will not be stretched. The size of the sides and corners is determined by the
 * Margin as follows:
 *
 *                 _____________________
 *                |  | Margin.Top    |  |
 *                |__|_______________|__|   Margin.Right
 *                |  |               |  |  /
 *              +--> |               | <--+
 *             /  |__|_______________|__|
 *  Margin.Left   |  | Margin.Bottom |  |
 *                |__|_______________|__|
 *
 */
struct SLATECORE_API FSlateBoxBrush
	: public FSlateBrush
{
	/**
	 * Make a 3x3 box that stretches the texture.
	 *
	 * @param InImageName		The name of image to make into a box
	 * @param InMargin			The size of corners and sides in normalized texture UV space.
	 * @param InColorAndOpacity	Color and opacity scale.
	 */
	FORCENOINLINE FSlateBoxBrush( const FName& InImageName, const FMargin& InMargin, const FLinearColor& InColorAndOpacity = FLinearColor(1,1,1,1), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, FVector2D::ZeroVector, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const FString& InImageName, const FMargin& InMargin, const FLinearColor& InColorAndOpacity = FLinearColor(1,1,1,1), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, *InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, FVector2D::ZeroVector, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const ANSICHAR* InImageName, const FMargin& InMargin, const FLinearColor& InColorAndOpacity = FLinearColor(1,1,1,1), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, FVector2D::ZeroVector, InColorAndOpacity)
	{ } 

	FORCENOINLINE FSlateBoxBrush( const WIDECHAR* InImageName, const FMargin& InMargin, const FLinearColor& InColorAndOpacity = FLinearColor(1,1,1,1), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, FVector2D::ZeroVector, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const FName& InImageName, const FMargin& InMargin, const TSharedRef< FLinearColor >& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, FVector2D::ZeroVector, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const FString& InImageName, const FMargin& InMargin, const TSharedRef< FLinearColor >& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, *InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, FVector2D::ZeroVector, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const ANSICHAR* InImageName, const FMargin& InMargin, const TSharedRef< FLinearColor >& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, FVector2D::ZeroVector, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const TCHAR* InImageName, const FMargin& InMargin, const TSharedRef< FLinearColor >& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, FVector2D::ZeroVector, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const FName& InImageName, const FMargin& InMargin, const FSlateColor& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, FVector2D::ZeroVector, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const FString& InImageName, const FMargin& InMargin, const FSlateColor& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, *InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, FVector2D::ZeroVector, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const ANSICHAR* InImageName, const FMargin& InMargin, const FSlateColor& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, FVector2D::ZeroVector, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const TCHAR* InImageName, const FMargin& InMargin, const FSlateColor& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, FVector2D::ZeroVector, InColorAndOpacity)
	{ }

	/**
	 * Make a 3x3 box that stretches the texture.
	 *
	 * @param InImageName       The name of image to make into a box
	 * @param ImageSize         The size of the resource as we want it to appear in slate units.
	 * @param InMargin          The size of corners and sides in texture space.
	 * @param InColorAndOpacity	Color and opacity scale. Note of the image type is ImageType_TintMask, this value should be in HSV
	 */
	FORCENOINLINE FSlateBoxBrush( const FName& InImageName, const FVector2D& ImageSize, const FMargin& InMargin, const FLinearColor& InColorAndOpacity = FLinearColor(1,1,1,1), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, ImageSize, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const FString& InImageName, const FVector2D& ImageSize, const FMargin& InMargin, const FLinearColor& InColorAndOpacity = FLinearColor(1,1,1,1), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, *InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, ImageSize, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const ANSICHAR* InImageName, const FVector2D& ImageSize, const FMargin& InMargin, const FLinearColor& InColorAndOpacity = FLinearColor(1,1,1,1), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, ImageSize, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const WIDECHAR* InImageName, const FVector2D& ImageSize, const FMargin& InMargin, const FLinearColor& InColorAndOpacity = FLinearColor(1,1,1,1), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, ImageSize, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const FName& InImageName, const FVector2D& ImageSize, const FMargin& InMargin, const TSharedRef< FLinearColor >& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, ImageSize, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const FString& InImageName, const FVector2D& ImageSize, const FMargin& InMargin, const TSharedRef< FLinearColor >& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, *InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, ImageSize, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const ANSICHAR* InImageName, const FVector2D& ImageSize, const FMargin& InMargin, const TSharedRef< FLinearColor >& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, ImageSize, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const TCHAR* InImageName, const FVector2D& ImageSize, const FMargin& InMargin, const TSharedRef< FLinearColor >& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, ImageSize, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const FName& InImageName, const FVector2D& ImageSize, const FMargin& InMargin, const FSlateColor& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, ImageSize, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const FString& InImageName, const FVector2D& ImageSize, const FMargin& InMargin, const FSlateColor& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, *InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, ImageSize, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const ANSICHAR* InImageName, const FVector2D& ImageSize, const FMargin& InMargin, const FSlateColor& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, ImageSize, InColorAndOpacity)
	{ }

	FORCENOINLINE FSlateBoxBrush( const TCHAR* InImageName, const FVector2D& ImageSize, const FMargin& InMargin, const FSlateColor& InColorAndOpacity, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor )
		: FSlateBrush(ESlateBrushDrawType::Box, InImageName, InMargin, ESlateBrushTileType::NoTile, InImageType, ImageSize, InColorAndOpacity)
	{ }
};
