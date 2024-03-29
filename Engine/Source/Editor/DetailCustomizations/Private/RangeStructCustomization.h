// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RangeStructCustomization.h: Declares the FRangeStructCustomization<> class.
=============================================================================*/

#pragma once

/**
* Implements a details panel customization for FFloatRange structures.
*/
template <typename NumericType>
class FRangeStructCustomization : public IStructCustomization
{
public:

	FRangeStructCustomization()
		: bIsUsingSlider(false) {}

	// Begin IStructCustomization interface

	virtual void CustomizeStructHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IStructCustomizationUtils& StructCustomizationUtils) OVERRIDE;

	virtual void CustomizeStructChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IStructCustomizationUtils& StructCustomizationUtils) OVERRIDE;

	// End IStructCustomization interface

public:

	/**
	* Creates a new instance.
	*
	* @return A new struct customization for Guids.
	*/
	static TSharedRef<IStructCustomization> MakeInstance();

protected:

	/**
	 * Gets the value for the provided property handle
	 *
	 * @param	ValueWeakPtr	Handle to the property to get the value from
	 * @param	TypeWeakPtr		Handle to the property to get the type from
	 *
	 * @return	The value or unset if it could not be accessed
	 */
	TOptional<NumericType> OnGetValue(TWeakPtr<IPropertyHandle> ValueWeakPtr, TWeakPtr<IPropertyHandle> TypeWeakPtr) const;

	/**
	 * Called when the value is committed from the property editor
	 *
	 * @param	NewValue		The new value of the property
	 * @param	CommitType		How the value was committed (unused)
	 * @param	HandleWeakPtr	Handle to the property that the new value is for
	 */
	void OnValueCommitted(NumericType NewValue, ETextCommit::Type CommitType, TWeakPtr<IPropertyHandle> HandleWeakPtr);

	/**
	 * Called when the value is changed in the property editor
	 *
	 * @param	NewValue		The new value of the property
	 * @param	HandleWeakPtr	Handle to the property that the new value is for
	 */
	void OnValueChanged(NumericType NewValue, TWeakPtr<IPropertyHandle> HandleWeakPtr);

	/**
	 * Called when a value starts to be changed by a slider
	 */
	void OnBeginSliderMovement();

	/**
	 * Called when a value stops being changed by a slider
	 *
	 * @param	NewValue		The new value of the property
	 */
	void OnEndSliderMovement(NumericType NewValue);

	/**
	 * Determines if the value is valid from the handle of the range type passed in
	 *
	 * @param	HandleWeakPtr	Handle to the property to get the type from
	 *
	 * @return	Whether the value is valid or not.
	 */
	bool OnQueryIfEnabled(TWeakPtr<IPropertyHandle> HandleWeakPtr) const;

	/**
	 * Determines if the spinbox is enabled on a numeric value widget
	 *
	 * @return Whether the spinbox should be enabled.
	 */
	bool ShouldAllowSpin() const;

	/**
	 * Generates a row of the combo widget
	 *
	 * @param	InComboString	Item string to be displayed in the combo item
	 *
	 * @return	An SWidget representing the combo row.
	 */
	TSharedRef<SWidget> OnGenerateComboWidget(TSharedPtr<FString> InComboString);

	/**
	 * Called when an item is selected in the combo box
	 *
	 * @param	InSelectedItem	String of the item selected
	 * @param	SelectInfo		Type of selection
	 * @param	HandleWeakPtr	Handle to the type property which will be changed by the combo selection
	 */
	void OnComboSelectionChanged(TSharedPtr<FString> InSelectedItem, ESelectInfo::Type SelectInfo, TWeakPtr<IPropertyHandle> HandleWeakPtr);

	// Cached shared pointers to properties that we are managing
	TSharedPtr<IPropertyHandle> LowerBoundStructHandle;
	TSharedPtr<IPropertyHandle> UpperBoundStructHandle;
	TSharedPtr<IPropertyHandle> LowerBoundValueHandle;
	TSharedPtr<IPropertyHandle> LowerBoundTypeHandle;
	TSharedPtr<IPropertyHandle> UpperBoundValueHandle;
	TSharedPtr<IPropertyHandle> UpperBoundTypeHandle;

	// Min and max allowed values from the metadata
	TOptional<NumericType> MinAllowedValue;
	TOptional<NumericType> MaxAllowedValue;

	// Arrays of combo box data: list items, and tooltips
	TArray< TSharedPtr<FString> > ComboBoxList;
	TArray< TSharedPtr<FString> > ComboBoxToolTips;

	// Flags whether the slider is being moved at the moment on any of our widgets
	bool bIsUsingSlider;
};
