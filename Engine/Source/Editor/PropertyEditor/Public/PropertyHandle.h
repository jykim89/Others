// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyEditorModule.h"

namespace EPropertyValueSetFlags
{
	typedef uint32 Type;

	/** Normal way to call set value (make a transaction, call posteditchange) */
	const Type DefaultFlags = 0;
	/** No transaction will be created when setting this value (no undo/redo) */
	const Type NotTransactable = 1 << 0;
	/** When PostEditChange is called mark the change as interactive (e.g, user is spinning a value in a spin box) */
	const Type InteractiveChange = 1 << 1;

};

/**
 * A handle to a property which is used to read and write the value without needing to handle Pre/PostEditChange, transactions, package modification
 * A handle also is used to identify the property in detail customization interfaces
 */
class IPropertyHandle
{
public:
	virtual ~IPropertyHandle(){}

	/**
	 * @return Whether or not the handle is valid
	 */
	virtual bool IsValidHandle() const = 0;
	
	/**
	 * @return Whether or not the property is edit const (can't be changed)
	 */
	virtual bool IsEditConst() const = 0;

	/**
	 * Gets the class of the property being edited
	 */
	virtual const UClass* GetPropertyClass() const = 0;

	/**
	 * Gets the property being edited
	 */
	virtual UProperty* GetProperty() const = 0;

	/**
	 * Gets the property tool tip text.
	 */
	virtual FString GetToolTipText() const = 0;
	
	/**
	 * Sets the tooltip shown for this property
	 *
	 * @param ToolTip	The tool tip to show
	 */
	virtual void SetToolTipText(const FString& ToolTip) = 0;

	/**
	 * Gets the value formatted as a string.
	 *
	 * @param OutValue	String where the value is stored.  Remains unchanged if the value could not be set
	 * @return The result of attempting to get the value
	 */
	virtual FPropertyAccess::Result GetValueAsFormattedString( FString& OutValue ) const = 0;

	/**
	 * Gets the value formatted as a string, possibly using an alternate form more suitable for display in the UI
	 *
	 * @param OutValue	String where the value is stored.  Remains unchanged if the value could not be set
	 * @return The result of attempting to get the value
	 */
	virtual FPropertyAccess::Result GetValueAsDisplayString( FString& OutValue ) const = 0;

	/**
	 * Gets the value formatted as a string, as Text.
	 *
	 * @param OutValue	Text where the value is stored.  Remains unchanged if the value could not be set
	 * @return The result of attempting to get the value
	 */
	virtual FPropertyAccess::Result GetValueAsFormattedText( FText& OutValue ) const = 0;

	/**
	 * Gets the value formatted as a string, as Text, possibly using an alternate form more suitable for display in the UI
	 *
	 * @param OutValue	Text where the value is stored.  Remains unchanged if the value could not be set
	 * @return The result of attempting to get the value
	 */
	virtual FPropertyAccess::Result GetValueAsDisplayText( FText& OutValue ) const = 0;

	/**
	 * Sets the value formatted as a string.
	 *
	 * @param OutValue	String where the value is stored.  Is unchanged if the value could not be set
	 * @return The result of attempting to set the value
	 */
	virtual FPropertyAccess::Result SetValueFromFormattedString( const FString& InValue,  EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;
	
	/**
	 * Sets a delegate to call when the value of the property is changed
	 *
	 * @param InOnPropertyValueChanged	The delegate to call
	 */
	virtual void SetOnPropertyValueChanged( const FSimpleDelegate& InOnPropertyValueChanged ) = 0;

	/**
	 * Gets the typed value of a property.  
	 * If the property does not support the value type FPropertyAccess::Fail is returned
	 *
	 * @param OutValue	The value that will be set if successful
	 * @return The result of attempting to get the value
	 */
	virtual FPropertyAccess::Result GetValue( int32& OutValue ) const = 0;
	virtual FPropertyAccess::Result GetValue( float& OutValue ) const = 0;
	virtual FPropertyAccess::Result GetValue( bool& OutValue ) const = 0;
	virtual FPropertyAccess::Result GetValue( uint8& OutValue ) const = 0;
	virtual FPropertyAccess::Result GetValue( FString& OutValue ) const = 0;
	virtual FPropertyAccess::Result GetValue( FName& OutValue ) const = 0;
	virtual FPropertyAccess::Result GetValue( FVector& OutValue ) const = 0;
	virtual FPropertyAccess::Result GetValue( FVector2D& OutValue ) const = 0;
	virtual FPropertyAccess::Result GetValue( FVector4& OutValue ) const = 0;
	virtual FPropertyAccess::Result GetValue( FQuat& OutValue ) const = 0;
	virtual FPropertyAccess::Result GetValue( FRotator& OutValue ) const = 0;
	virtual FPropertyAccess::Result GetValue( UObject*& OutValue ) const = 0;

	/**
	 * Sets the typed value of a property.  
	 * If the property does not support the value type FPropertyAccess::Fail is returned
	 *
	 * @param InValue	The value to set
	 * @return The result of attempting to set the value
	 */
	virtual FPropertyAccess::Result SetValue( const int32& InValue, EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;
	virtual FPropertyAccess::Result SetValue( const float& InValue, EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;
	virtual FPropertyAccess::Result SetValue( const bool& InValue,  EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;
	virtual FPropertyAccess::Result SetValue( const uint8& InValue,  EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;
	virtual FPropertyAccess::Result SetValue( const FString& InValue,  EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;
	virtual FPropertyAccess::Result SetValue( const FName& InValue, EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;
	virtual FPropertyAccess::Result SetValue( const FVector& InValue,  EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;
	virtual FPropertyAccess::Result SetValue( const FVector2D& InValue,  EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;
	virtual FPropertyAccess::Result SetValue( const FVector4& InValue,  EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;
	virtual FPropertyAccess::Result SetValue( const FQuat& InValue,  EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;
	virtual FPropertyAccess::Result SetValue( const FRotator& InValue,  EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;
	virtual FPropertyAccess::Result SetValue( const UObject*& InValue,  EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;
	
	/**
	 * Called to manually notify root objects that this property is about to change
	 * This does not need to be called when SetValue functions are used since it will be called automatically
	 */
	virtual void NotifyPreChange() = 0;

	/**
	 * Called to manually notify root objects that this property has changed
	 * This does not need to be called when SetValue functions are used since it will be called automatically
	 */
	virtual void NotifyPostChange() = 0;

	/**
	 * Sets the object value from the current editor selection
	 * Will fail if this handle isn't an object property
	 */
	virtual FPropertyAccess::Result SetObjectValueFromSelection() = 0;

	/**
	 * Sets a unique value for each object this handle is editing
	 *
	 * @param PerObjectValues	The per object values as a formatted string.  There must be one entry per object or the return value is FPropertyAccess::Fail
	 */
	virtual FPropertyAccess::Result SetPerObjectValues( const TArray<FString>& PerObjectValues, EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags ) = 0;

	/**
	 * Gets a unique value for each object this handle is editing
	 */
	virtual FPropertyAccess::Result GetPerObjectValues( TArray<FString>& OutPerObjectValues ) = 0;

	/**
	 * @return The index of this element in an array if it is in one.  INDEX_NONE otherwise                                                              
	 */
	virtual int32 GetIndexInArray() const = 0;

	/**
	 * Gets a child handle of this handle.  Useful for accessing properties in structs.  
	 * Array elements cannot be accessed in this way  
	 *
	 * @param The name of the child
	 * @return The property handle for the child if it exists
	 */
	virtual TSharedPtr<IPropertyHandle> GetChildHandle( FName ChildName ) const = 0;

	/**
	 * Gets a child handle of this handle.  Useful for accessing properties in structs.  
	 *
	 * @param The index of the child
	 * @return The property handle for the child if it exists
	 */
	virtual TSharedPtr<IPropertyHandle> GetChildHandle( uint32 Index ) const = 0;
	
	/**
	 * @return a handle to the parent array if this handle is an array element
	 */
	virtual TSharedPtr<IPropertyHandle> GetParentHandle() const = 0;

	/**
	 * @return The number of children the property handle has
	 */
	virtual FPropertyAccess::Result GetNumChildren( uint32& OutNumChildren ) const = 0;

	/**
	 * @return Get the number objects that contain this property and are being observed in the property editor
	 */
	virtual uint32 GetNumOuterObjects() const = 0;
	
	/**
	 * Get the objects that contain this property 
	 *
	 * @param OuterObjects	An array that will be populated with the outer objects 
	 */
	virtual void GetOuterObjects( TArray<UObject*>& OuterObjects ) = 0;

	/**
	 * Accesses the raw data of this property.  (Each pointer can be cast to the property data type)
	 *
	 * @param RawData	An array of raw data.  The elements in this array are the raw data for this property on each of the objects in the  property editor
	 */ 
	virtual void AccessRawData( TArray<void*>& RawData ) = 0;

	/**
	 * Returns this handle as an array if possible
	 *
	 * @return the handle as an array if it is an array (static or dynamic)
	 */
	virtual TSharedPtr<class IPropertyHandleArray> AsArray() = 0;

	/**
	 * @return The display name of the property
	 */
	virtual FString GetPropertyDisplayName() const = 0;
	
	/**
	 * Resets the value to its default
	 */
	virtual void ResetToDefault() = 0;

	/**
	 * @return Whether or not the value differs from its default                   
	 */
	virtual bool DiffersFromDefault() const = 0;

	/**
	 * @return A label suitable for displaying the reset to default value
	 */
	virtual FText GetResetToDefaultLabel() const = 0;

	/**
	 * Generates a list of possible enum/class options for the property
	 */
	virtual bool GeneratePossibleValues(TArray< TSharedPtr<FString> >& OutOptionStrings, TArray< TSharedPtr<FString> >& OutToolTips, TArray<bool>& OutRestrictedItems) = 0;

	/**
	 * Marks this property has hidden by customizaton (will not show up in the default place)
	 */
	virtual void MarkHiddenByCustomization() = 0;

	/**
	 * @return True if this property is customized                                                              
	 */
	virtual bool IsCustomized() const = 0;

	/**
	 * Creates a name widget for this property
	 * @param NameOverride			The name override to use instead of the property name
	 * @param bDisplayResetToDefault	Whether or not to display the reset to default button
	 * @param bDisplayText				Whether or not to display the text name of the property
	 * @param bDisplayThumbnail			Whether or not to display the thumbnail for the property (if any)
	 * @return the name widget for this property
	 */
	virtual TSharedRef<SWidget> CreatePropertyNameWidget( const FString& NameOverride = TEXT(""), bool bDisplayResetToDefault = false, bool bDisplayText = true, bool bDisplayThumbnail = true ) const = 0;

	/**
	 * Creates a value widget for this property

	 * @return the value widget for this property
	 */
	virtual TSharedRef<SWidget> CreatePropertyValueWidget() const = 0;

	/**
	 * Adds a restriction to the possible values for this property.
	 * @param Restriction	The restriction being added to this property.
	 */
	virtual void AddRestriction( TSharedRef<const class FPropertyRestriction> Restriction ) = 0;

	/**
	 * Tests if a value is restricted for this property
	 * @param Value			The value to test for restriction.
	 * @return				True if this value is restricted.
	 */
	virtual bool IsRestricted(const FString& Value) const = 0;
	
	/**
	 * Tests if a value is restricted for this property.
	 * @param Value			The value to test for restriction.
	 * @param OutReasons	Outputs an array of the reasons why this value is restricted.
	 * @return				True if this value is restricted.
	 */
	virtual bool IsRestricted(const FString& Value, TArray<FText>& OutReasons) const = 0;

	/**
	 * Generates a consistent tooltip describing this restriction for use in the editor.
	 * @param Value			The value to test for restriction and generate the tooltip from.
	 * @param OutTooltip	The tooltip describing why this value is restricted.
	 * @return				True if this value is restricted.
	 */
	virtual bool GenerateRestrictionToolTip(const FString& Value, FText& OutTooltip)const = 0;
};

/**
 * A handle to an array property which allows you to manipulate the array                                                              
 */
class IPropertyHandleArray
{
public:
	virtual ~IPropertyHandleArray(){}
	
	/**
	 * Adds an item to the end of the array
	 * @return Whether or not this was successful
	 */
	virtual FPropertyAccess::Result AddItem() = 0;

	/**
	 * Empty the array
	 * @return Whether or not this was successful
	 */
	virtual FPropertyAccess::Result EmptyArray() = 0;

	/**
	 * Inserts an item into the array at the specified index
	 * @return Whether or not this was successful
	 */
	virtual FPropertyAccess::Result Insert( int32 Index ) = 0;

	/**
	 * Duplicates the item at the specified index in the array.
	 * @return Whether or not this was successful
	 */
	virtual FPropertyAccess::Result DuplicateItem( int32 Index ) = 0;

	/**
	 * Deletes the item at the specified index of the array
	 * @return Whether or not this was successful
	 */
	virtual FPropertyAccess::Result DeleteItem( int32 Index ) = 0;

	/**
	 * @return The number of elements in the array
	 */
	virtual FPropertyAccess::Result GetNumElements( uint32& OutNumItems ) const = 0;

	/**
	 * @return a handle to the element at the specified index                                                              
	 */
	virtual TSharedRef<IPropertyHandle> GetElement( int32 Index ) const = 0;

	/**
	 * Sets a delegate to call when the number of elements changes                                                  
	 */
	virtual void SetOnNumElementsChanged( FSimpleDelegate& InOnNumElementsChanged ) = 0;
};
