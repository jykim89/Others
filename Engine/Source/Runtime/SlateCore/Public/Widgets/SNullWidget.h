// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SNullWidget.h: Declares the SNullWidget class.
=============================================================================*/

#pragma once


/**
 * Implements a widget that can be used as a placeholder.
 *
 * Widgets that support slots, such as SOverlay and SHorizontalBox should initialize
 * their slots' child widgets to SNullWidget if no user defined widget was provided.
 */
class SLATECORE_API SNullWidget
{
public:

	/**
	 * Returns a placeholder widget.
	 *
	 * @return The widget.
	 */
	static TSharedRef<class SWidget> NullWidget;
};
