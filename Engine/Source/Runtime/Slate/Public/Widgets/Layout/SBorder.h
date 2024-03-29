// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A border is an widget that can be used to contain other widgets.
 * It has a BorderImage property, which allows it to take on different appearances.
 * Border also has a Content() slot as well as some parameters controlling the
 * arrangement of said content.
 */
class SLATE_API SBorder : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SBorder)
		: _Content()
		, _HAlign( HAlign_Fill )
		, _VAlign( VAlign_Fill )
		, _Padding( FMargin(2.0f) )
		, _OnMouseButtonDown()
		, _OnMouseButtonUp()
		, _OnMouseMove()
		, _OnMouseDoubleClick()
		, _BorderImage( FCoreStyle::Get().GetBrush( "Border" ) )
		, _ContentScale( FVector2D(1,1) )
		, _DesiredSizeScale( FVector2D(1,1) )
		, _ColorAndOpacity( FLinearColor(1,1,1,1) )
		, _BorderBackgroundColor( FLinearColor::White )
		, _ForegroundColor( FSlateColor::UseForeground() )
		, _ShowEffectWhenDisabled( true )
		{}

		SLATE_DEFAULT_SLOT( FArguments, Content )

		SLATE_ARGUMENT( EHorizontalAlignment, HAlign )
		SLATE_ARGUMENT( EVerticalAlignment, VAlign )
		SLATE_ATTRIBUTE( FMargin, Padding )
		
		SLATE_EVENT( FPointerEventHandler, OnMouseButtonDown )
		SLATE_EVENT( FPointerEventHandler, OnMouseButtonUp )
		SLATE_EVENT( FPointerEventHandler, OnMouseMove )
		SLATE_EVENT( FPointerEventHandler, OnMouseDoubleClick )

		SLATE_ATTRIBUTE( const FSlateBrush*, BorderImage )

		SLATE_ATTRIBUTE( FVector2D, ContentScale )

		SLATE_ATTRIBUTE( FVector2D, DesiredSizeScale )

		/** ColorAndOpacity is the color and opacity of content in the border */
		SLATE_ATTRIBUTE( FLinearColor, ColorAndOpacity )
		/** BorderBackgroundColor refers to the actual color and opacity of the supplied border image.*/
		SLATE_ATTRIBUTE( FSlateColor, BorderBackgroundColor )
		/** The foreground color of text and some glyphs that appear as the border's content. */
		SLATE_ATTRIBUTE( FSlateColor, ForegroundColor )
		/** Whether or not to show the disabled effect when this border is disabled */
		SLATE_ATTRIBUTE( bool, ShowEffectWhenDisabled )
	SLATE_END_ARGS()

	/**
	 * Default constructor.
	 */
	SBorder();

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Sets the content for this border
	 *
	 * @param	InContent	The widget to use as content for the border
	 */
	void SetContent( const TSharedRef< SWidget >& InContent );

	/**
	 * Gets the content for this border
	 *
	 * @return The widget used as content for the border
	 */
	const TSharedRef< SWidget >& GetContent() const;

	/** Clears out the content for the border */
	void ClearContent();

	/** Gets the color and opacity of the background image of this border. */
	FSlateColor GetBorderBackgroundColor()
	{
		return BorderBackgroundColor.Get();
	}

	/** Sets the color and opacity of the background image of this border. */
	void SetBorderBackgroundColor( const TAttribute<FSlateColor>& InColorAndOpacity )
	{
		BorderBackgroundColor = InColorAndOpacity;
	}

	/** Set the desired size scale multiplier */
	FVector2D GetDesiredSizeScale()
	{
		return DesiredSizeScale.Get();
	}

	/** Set the desired size scale multiplier */
	void SetDesiredSizeScale( const TAttribute<FVector2D>& InDesiredSizeScale )
	{
		DesiredSizeScale = InDesiredSizeScale;
	}

public:
	// SWidget interface
	virtual int32 OnPaint( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const OVERRIDE;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;
	virtual FVector2D ComputeDesiredSize() const OVERRIDE;
	// End of SWidget interface

 protected:
	
	TAttribute<const FSlateBrush*> BorderImage;
	TAttribute<FSlateColor> BorderBackgroundColor;
	TAttribute<FVector2D> DesiredSizeScale;
	/** Whether or not to show the disabled effect when this border is disabled */
	TAttribute<bool> ShowDisabledEffect;
	FPointerEventHandler MouseButtonDownHandler;
	FPointerEventHandler MouseButtonUpHandler;
	FPointerEventHandler MouseMoveHandler;
	FPointerEventHandler MouseDoubleClickHandler;
 };
