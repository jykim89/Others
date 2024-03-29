// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "MultiBox.h"
#include "SHeadingBlock.h"


/**
 * Constructor
 *
 * @param	InHeadingText	Heading text
 */
FHeadingBlock::FHeadingBlock( const FName& InExtensionHook, const TAttribute< FText >& InHeadingText )
	: FMultiBlock( NULL, NULL, InExtensionHook )
	, HeadingText( InHeadingText )
{
}


/**
 * Allocates a widget for this type of MultiBlock.  Override this in derived classes.
 *
 * @return  MultiBlock widget object
 */
TSharedRef< class IMultiBlockBaseWidget > FHeadingBlock::ConstructWidget() const
{
	return SNew( SHeadingBlock );
}


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SHeadingBlock::Construct( const FArguments& InArgs )
{
}



/**
 * Builds this MultiBlock widget up from the MultiBlock associated with it
 */
void SHeadingBlock::BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName)
{
	TSharedRef< const FHeadingBlock > HeadingBlock = StaticCastSharedRef< const FHeadingBlock >( MultiBlock.ToSharedRef() );

	ChildSlot
		.Padding( 2.0f )
		[
			SNew( STextBlock )
				.Text( HeadingBlock->HeadingText )
				.TextStyle( StyleSet, ISlateStyle::Join( StyleName, ".Heading" ) )
		];
}

