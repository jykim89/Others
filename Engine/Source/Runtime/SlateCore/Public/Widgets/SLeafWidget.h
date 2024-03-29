// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SLeafWidget.h: Declares the SLeafWidget class.
=============================================================================*/

#pragma once


/**
 * Implements a leaf widget.
 *
 * A LeafWidget is a Widget that has no slots for children.
 * LeafWidgets are usually intended as building blocks for aggregate widgets.
 */
class SLATECORE_API SLeafWidget
	: public SWidget
{
public:

	// Begin SWidget overrides

	/**
	 * Overwritten from SWidget.
	 *
	 * LeafWidgets provide a visual representation of themselves. They do so by adding DrawElement(s)
	 * to the OutDrawElements. DrawElements should have their positions set to absolute coordinates in
	 * Window space; for this purpose the Slate system provides the AllottedGeometry parameter.
	 * AllottedGeometry describes the space allocated for the visualization of this widget.
	 *
	 * Whenever possible, LeafWidgets should avoid dealing with layout properties. See TextBlock for an example.
	 */
	virtual int32 OnPaint( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const = 0;
	
	/**
	 * Overwritten from SWidget.
	 *
	 * LeafWidgets should compute their DesiredSize based solely on their visual representation. There is no need to
	 * take child widgets into account as LeafWidgets have none by definition. For example, the TextBlock widget simply
	 * measures the area necessary to display its text with the given font and font size.
	 */
	virtual FVector2D ComputeDesiredSize( ) const = 0;
	
	/**
	 * Overwritten from SWidget.
	 *
	 * Leaf widgets never have children.
	 */
	virtual FChildren* GetChildren( );

	virtual void ArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const;

private:

	// Shared instance of FNoChildren for all widgets with no children.
	static FNoChildren NoChildrenInstance;
};
