// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Implementation for a box that only accepts a numeric value or that can display an undetermined value via a string
 * Supports an optional spin box for manipulating a value by dragging with the mouse
 * Supports an optional label inset in the text box
 */ 
template<typename NumericType>
class SNumericEntryBox : public SCompoundWidget
{
public: 
	static const FLinearColor RedLabelBackgroundColor;
	static const FLinearColor GreenLabelBackgroundColor;
	static const FLinearColor BlueLabelBackgroundColor;

	/** Notification for numeric value change */
	DECLARE_DELEGATE_OneParam( FOnValueChanged, NumericType );

	/** Notification for numeric value committed */
	DECLARE_DELEGATE_TwoParams( FOnValueCommitted, NumericType, ETextCommit::Type);

public:
	SLATE_BEGIN_ARGS( SNumericEntryBox<NumericType> )
		: _EditableTextBoxStyle( &FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox") )
		, _Label()
		, _LabelVAlign( VAlign_Fill )
		, _LabelPadding( FMargin(3,0) )
		, _BorderForegroundColor(FCoreStyle::Get().GetSlateColor("InvertedForeground"))
		, _BorderBackgroundColor(FLinearColor::White)
		, _UndeterminedString( TEXT("---") )
		, _Font( FCoreStyle::Get().GetFontStyle( TEXT("NormalFont") ) )
		, _AllowSpin(false)
		, _Delta(0)
		, _MinValue(TNumericLimits<NumericType>::Lowest())
		, _MaxValue(TNumericLimits<NumericType>::Max())
		, _MinSliderValue(0)				
		, _MaxSliderValue(100)
		, _SliderExponent(1.f)
	{}		

		/** Style to use for the editable text box within this widget */
		SLATE_STYLE_ARGUMENT( FEditableTextBoxStyle, EditableTextBoxStyle )

		/** Slot for this button's content (optional) */
		SLATE_NAMED_SLOT( FArguments, Label )
		/** Vertical alignment of the label content */
		SLATE_ARGUMENT( EVerticalAlignment, LabelVAlign )
		/** Padding around the label content */
		SLATE_ARGUMENT( FMargin, LabelPadding )
		/** Border Foreground Color */
		SLATE_ARGUMENT( FSlateColor, BorderForegroundColor )
		/** Border Background Color */
		SLATE_ARGUMENT( FSlateColor, BorderBackgroundColor )
		/** The value that should be displayed.  This value is optional in the case where a value cannot be determined */
		SLATE_ATTRIBUTE( TOptional<NumericType>, Value )
		/** The string to display if the value cannot be determined */
		SLATE_TEXT_ARGUMENT( UndeterminedString )
		/** Font color and opacity */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )
		/** Whether or not the user should be able to change the value by dragging with the mouse cursor */
		SLATE_ARGUMENT( bool, AllowSpin )
		/** Delta to increment the value as the slider moves.  If not specified will determine automatically */
		SLATE_ARGUMENT( NumericType, Delta )
		/** The minimum value that can be entered into the text edit box */
		SLATE_ATTRIBUTE( TOptional< NumericType >, MinValue )
		/** The maximum value that can be entered into the text edit box */
		SLATE_ATTRIBUTE( TOptional< NumericType >, MaxValue )
		/** The minimum value that can be specified by using the slider */
		SLATE_ATTRIBUTE( TOptional< NumericType >, MinSliderValue )
		/** The maximum value that can be specified by using the slider */
		SLATE_ATTRIBUTE( TOptional< NumericType >, MaxSliderValue )
		/** Use exponential scale for the slider */
		SLATE_ATTRIBUTE( float, SliderExponent )
		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT( FOnValueChanged, OnValueChanged )
		/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
		SLATE_EVENT( FOnValueCommitted, OnValueCommitted )
		/** Called right before the slider begins to move */
		SLATE_EVENT( FSimpleDelegate, OnBeginSliderMovement )
		/** Called right after the slider handle is released by the user */
		SLATE_EVENT( FOnValueChanged, OnEndSliderMovement )		
		/** Menu extender for right-click context menu */
		SLATE_EVENT( FMenuExtensionDelegate, ContextMenuExtender )		

	SLATE_END_ARGS()
	SNumericEntryBox()
	{
	}

	void Construct( const FArguments& InArgs )
	{
		check(InArgs._EditableTextBoxStyle);

		const bool bAllowSpin = InArgs._AllowSpin;
		OnValueChanged = InArgs._OnValueChanged;
		OnValueCommitted = InArgs._OnValueCommitted;
		ValueAttribute = InArgs._Value;
		UndeterminedString = InArgs._UndeterminedString;
		
		BorderImageNormal = &InArgs._EditableTextBoxStyle->BackgroundImageNormal;
		BorderImageHovered = &InArgs._EditableTextBoxStyle->BackgroundImageHovered;
		BorderImageFocused = &InArgs._EditableTextBoxStyle->BackgroundImageFocused;
		const FMargin& TextMargin = InArgs._EditableTextBoxStyle->Padding;
		
		TSharedPtr<SWidget> FinalWidget;
		if( bAllowSpin )
		{
			SAssignNew( SpinBox, SSpinBox<NumericType> )
				.Style( FCoreStyle::Get(), "NumericEntrySpinBox" )
				.Font( InArgs._Font )
				.ContentPadding( TextMargin )
				.Value( this, &SNumericEntryBox<NumericType>::OnGetValueForSpinBox )
				.Delta( InArgs._Delta )
				.OnValueChanged( OnValueChanged )
				.OnValueCommitted( OnValueCommitted )
				.MinSliderValue(InArgs._MinSliderValue)
				.MaxSliderValue(InArgs._MaxSliderValue)
				.MaxValue(InArgs._MaxValue)
				.MinValue(InArgs._MinValue)
				.SliderExponent(InArgs._SliderExponent)
				.OnBeginSliderMovement(InArgs._OnBeginSliderMovement)
				.OnEndSliderMovement(InArgs._OnEndSliderMovement);
		}

		// Always create an editable text box.  In the case of an undetermined value being passed in, we cant use the spinbox.
		SAssignNew( EditableText, SEditableText )
			.Text( this, &SNumericEntryBox<NumericType>::OnGetValueForTextBox )
			.Visibility( bAllowSpin ? EVisibility::Collapsed : EVisibility::Visible )
			.Font( InArgs._Font )
			.SelectAllTextWhenFocused( true )
			.ClearKeyboardFocusOnCommit( false )
			.OnTextChanged( this, &SNumericEntryBox<NumericType>::OnTextChanged  )
			.OnTextCommitted( this, &SNumericEntryBox<NumericType>::OnTextCommitted )
			.SelectAllTextOnCommit( true )
			.ContextMenuExtender( InArgs._ContextMenuExtender );

		TSharedRef<SHorizontalBox> HorizontalBox = SNew( SHorizontalBox );
		if( InArgs._Label.Widget != SNullWidget::NullWidget )
		{
			HorizontalBox->AddSlot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(InArgs._LabelVAlign)
				.Padding(InArgs._LabelPadding)
				[
					InArgs._Label.Widget
				];
		}

		// Add the spin box if we have one
		if( bAllowSpin )
		{
			HorizontalBox->AddSlot()
				.HAlign(HAlign_Fill) 
				.VAlign(VAlign_Center) 
				.FillWidth(1)
				[
					SpinBox.ToSharedRef()
				];
		}

		HorizontalBox->AddSlot()
			.HAlign(HAlign_Fill) 
			.VAlign(VAlign_Center)
			.Padding(TextMargin)
			.FillWidth(1)
			[
				EditableText.ToSharedRef()
			];


		ChildSlot
			[
				SNew( SBorder )
				.BorderImage( this, &SNumericEntryBox<NumericType>::GetBorderImage )
				.BorderBackgroundColor( InArgs._BorderBackgroundColor )
				.ForegroundColor( InArgs._BorderForegroundColor )
				.Padding(0)
				[
					HorizontalBox
				]
			];
	}


	/** Build a generic label with specified text, foreground color and background color */
	static TSharedRef<SWidget> BuildLabel( const FText& LabelText, const FSlateColor& ForegroundColor, const FSlateColor& BackgroundColor )
	{
		return
			SNew(SBorder)
			.BorderImage( FCoreStyle::Get().GetBrush("NumericEntrySpinBox.Decorator") )
			.BorderBackgroundColor( BackgroundColor )
			.ForegroundColor( ForegroundColor )
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding( FMargin(1, 0, 6, 0) )
			[
				SNew( STextBlock )
				.Text( LabelText )
			];
	}

private:

	// Begin SWidget Interface
	virtual bool SupportsKeyboardFocus() const OVERRIDE
	{
		return StaticCastSharedPtr<SWidget>(EditableText)->SupportsKeyboardFocus();
	}

	virtual FReply OnKeyboardFocusReceived( const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent ) OVERRIDE
	{
		FReply Reply = FReply::Handled();

		// The widget to forward focus to changes depending on whether we have a SpinBox or not.
		TSharedPtr<SWidget> FocusWidget;
		if (SpinBox.IsValid() && SpinBox->GetVisibility() == EVisibility::Visible) 
		{
			FocusWidget = SpinBox;
		}
		else
		{
			FocusWidget = EditableText;
		}

		if ( InKeyboardFocusEvent.GetCause() != EKeyboardFocusCause::Cleared )
		{
			// Forward keyboard focus to our chosen widget
			Reply.SetKeyboardFocus( FocusWidget.ToSharedRef(), InKeyboardFocusEvent.GetCause() );
		}

		return Reply;
	}

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent ) OVERRIDE
	{
		FKey Key = InKeyboardEvent.GetKey();

		if( Key == EKeys::Escape && EditableText->HasKeyboardFocus() )
		{
			return FReply::Handled().SetKeyboardFocus( SharedThis( this ), EKeyboardFocusCause::Cleared );
		}

		return FReply::Unhandled();
	}
	// End SWidget Interface

	/**
	 * @return the Label that should be displayed                   
	 */
	FString GetLabel() const
	{
		// Should always be set if this is being called
		return LabelAttribute.Get().GetValue();
	}

	/**
	 * Called to get the value for the spin box                   
	 */
	NumericType OnGetValueForSpinBox() const
	{
		const auto& Value = ValueAttribute.Get();

		// Get the value or 0 if its not set
		if( Value.IsSet() == true )
		{
			return Value.GetValue();
		}
		else
		{
			return 0;
		}
	}

	/**
	 * Called to get the value for the text box as FText                 
	 */
	FText OnGetValueForTextBox() const
	{
		FString NewString = TEXT("");

		if( EditableText->GetVisibility() == EVisibility::Visible )
		{
			const auto& Value = ValueAttribute.Get();

			// If the value was set convert it to a string, otherwise the value cannot be determined
			if( Value.IsSet() == true )
			{
				auto CurrentValue = Value.GetValue();
				NewString = TTypeToString<NumericType>::ToSanitizedString(CurrentValue);
			}
			else
			{
				NewString = UndeterminedString;
			}
		}

		// The box isnt visible, just return an empty string
		return FText::FromString(NewString);
	}


	/**
	 * Called when the text changes in the text box                   
	 */
	void OnTextChanged( const FText& NewValue )
	{
		const auto& Value = ValueAttribute.Get();

		// Do not sent change events if the current value cannot be determined or else next tick the spin box could be swapped in if the value becomes determined
		// while a user is typing in the box.  This causes keyboard focus switch which is bad
		if( Value.IsSet() == true )
		{
			SendChangesFromText( NewValue, false, ETextCommit::Default );
		}
	}

	/**
	 * Called when the text is committed from the text box                   
	 */
	void OnTextCommitted( const FText& NewValue, ETextCommit::Type CommitInfo )
	{
		SendChangesFromText( NewValue, true, CommitInfo );
	}

	/**
	 * Called to get the border image of the box                   
	 */
	const FSlateBrush* GetBorderImage() const
	{
		TSharedPtr<const SWidget> EditingWidget;
		if (SpinBox.IsValid() && SpinBox->GetVisibility() == EVisibility::Visible) 
		{
			EditingWidget = SpinBox;
		}
		else
		{
			EditingWidget = EditableText;
		}

		if ( EditingWidget->HasKeyboardFocus() )
		{
			return BorderImageFocused;
		}
		else
		{
			if ( EditingWidget->IsHovered() )
			{
				return BorderImageHovered;
			}
			else
			{
				return BorderImageNormal;
			}
		}
	}

	/**
	 * Calls the value commit or changed delegate set for this box when the value is set from a string
	 *
	 * @param NewValue	The new value as a string
	 * @param bCommit	Whether or not to call the commit or changed delegate
	 */
	void SendChangesFromText( const FText& NewValue, bool bCommit, ETextCommit::Type CommitInfo )
	{
		// Only call the delegates if we have a valid numeric value
		if( !NewValue.IsEmpty()  )
		{
			if( bCommit )
			{
				bool bEvalResult = false;
				NumericType NumericValue = 0;

				// Convert string to an underlying type in case text is not math equation
				// Try to parse equation otherwise
				if ( NewValue.IsNumeric() )
				{
					TTypeFromString<NumericType>::FromString( NumericValue, *NewValue.ToString() );
					bEvalResult = true;
				}
				else
				{
					// Only evaluate equations on commit or else they could fail because the equation is still being typed
					float Value = 0.f;
					bEvalResult = FMath::Eval( NewValue.ToString(), Value );
					NumericValue = NumericType(Value);
				}
				
				if( bEvalResult )
				{
					OnValueCommitted.ExecuteIfBound(NumericValue, CommitInfo );
				}
			}
			else if( NewValue.IsNumeric() )
			{
				NumericType Value = 0;
				TTypeFromString<NumericType>::FromString( Value, *NewValue.ToString() );
				OnValueChanged.ExecuteIfBound( Value );
			}
		}
	}

	/**
	 * Caches the value and performs widget visibility maintenance
	 */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) OVERRIDE
	{
		// Visibility toggle only matters if the spinbox is used
		if( SpinBox.IsValid() )
		{
			const auto& Value = ValueAttribute.Get();

			if( Value.IsSet() == true )
			{
				if( SpinBox->GetVisibility() != EVisibility::Visible )
				{
					// Set the visibility of the spinbox to visible if we have a valid value
					SpinBox->SetVisibility( EVisibility::Visible );
					// The text box should be invisible
					EditableText->SetVisibility( EVisibility::Collapsed );
				}
			}
			else
			{
				// The value isn't set so the spinbox should be hidden and the text box shown
				SpinBox->SetVisibility(EVisibility::Collapsed);
				EditableText->SetVisibility(EVisibility::Visible);
			}
		}
	}

private:
	/** Attribute for getting the label */
	TAttribute< TOptional<FString > > LabelAttribute;
	/** Attribute for getting the value.  If the value is not set we display the undetermined string */
	TAttribute< TOptional<NumericType> > ValueAttribute;
	/** Spinbox widget */
	TSharedPtr<SWidget> SpinBox;
	/** Editable widget */
	TSharedPtr<SEditableText> EditableText;
	/** Delegate to call when the value changes */
	FOnValueChanged OnValueChanged;
	/** Delegate to call when the value is committed */
	FOnValueCommitted OnValueCommitted;
	/** The undetermined string to display when needed */
	FString UndeterminedString;
	/** Styling: border image to draw when not hovered or focused */
	const FSlateBrush* BorderImageNormal;
	/** Styling: border image to draw when hovered */
	const FSlateBrush* BorderImageHovered;
	/** Styling: border image to draw when focused */
	const FSlateBrush* BorderImageFocused;
};


template <typename NumericType>
const FLinearColor SNumericEntryBox<NumericType>::RedLabelBackgroundColor(0.594f,0.0197f,0.0f);

template <typename NumericType>
const FLinearColor SNumericEntryBox<NumericType>::GreenLabelBackgroundColor(0.1349f,0.3959f,0.0f);

template <typename NumericType>
const FLinearColor SNumericEntryBox<NumericType>::BlueLabelBackgroundColor(0.0251f,0.207f,0.85f);
