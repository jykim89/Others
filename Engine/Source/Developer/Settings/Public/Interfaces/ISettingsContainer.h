// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ISettingsContainer.h: Declares the ISettingsContainer interface.
=============================================================================*/

#pragma once


/**
 * Delegate type for modified settings categories.
 *
 * The first parameter is the name of the settings category that was modified.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSettingsContainerCategoryModified, const FName&)

/**
 * Delegate type for removed settings sections.
 *
 * The first parameter is the section that was removed.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSettingsContainerSectionRemoved, const ISettingsSectionRef&)


/**
 * Type definition for shared pointers to instances of ISettingsContainer.
 */
typedef TSharedPtr<class ISettingsContainer> ISettingsContainerPtr;

/**
 * Type definition for shared references to instances of ISettingsContainer.
 */
typedef TSharedRef<class ISettingsContainer> ISettingsContainerRef;


/**
 * Interface for setting containers.
 *
 * A settings container is a collection of setting categories.
 * Each category holds a collection of setting sections, which contain
 * the actual settings in the form of UObject properties.
 */
class ISettingsContainer
{
public:

	/**
	 * Updates the details of this settings container.
	 *
	 * @param InDisplayName - The container's localized display name.
	 * @param InDescription - The container's localized description text.
	 * @param InIconName - The name of the container's icon.
	 */
	virtual void Describe( const FText& InDisplayName, const FText& InDescription, const FName& InIconName ) = 0;

	/**
	 * Updates the details of the specified settings category.
	 *
	 * @param CategoryName - The name of the category to update.
	 * @param DisplayName - The category's localized display name.
	 * @param Description - The category's localized description text.
	 * @param IconName - The name of the category's icon.
	 */
	virtual void DescribeCategory( const FName& CategoryName, const FText& DisplayName, const FText& Description, const FName& IconName ) = 0;

	/**
	 * Gets the setting categories.
	 *
	 * @param OutCategories - Will hold the collection of categories.
	 *
	 * @return The number of categories returned.
	 */
	virtual int32 GetCategories( TArray<ISettingsCategoryPtr>& OutCategories ) const = 0;

	/**
	 * Gets the category with the specified name.
	 *
	 * @return The category, or nullptr if it doesn't exist.
	 */
	virtual ISettingsCategoryPtr GetCategory( const FName& CategoryName ) const = 0;

	/**
	 * Gets the container's localized description text.
	 *
	 * @return Description text.
	 */
	virtual const FText& GetDescription( ) const = 0;

	/**
	 * Gets the container's localized display name.
	 *
	 * @return Display name.
	 */
	virtual const FText& GetDisplayName( ) const = 0;

	/**
	 * Gets the name of the container's icon.
	 *
	 * @return Icon image name.
	 */
	virtual const FName& GetIconName( ) const = 0;

	/**
	 * Gets the container's name.
	 *
	 * @return Container name.
	 */
	virtual const FName& GetName( ) const = 0;

public:

	/**
	 * Returns a delegate that is executed when a settings category has been added or modified.
	 *
	 * @return The delegate.
	 */
	virtual FOnSettingsContainerCategoryModified& OnCategoryModified( ) = 0;

	/**
	 * Returns a delegate that is executed when a settings section has been removed.
	 *
	 * @return The delegate.
	 */
	virtual FOnSettingsContainerSectionRemoved& OnSectionRemoved( ) = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~ISettingsContainer( ) { }
};
