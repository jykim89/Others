// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TranslationUnit.generated.h"

USTRUCT()
struct FTranslationChange
{
	GENERATED_USTRUCT_BODY()

public:

	/** The changelist of this change */
	UPROPERTY(Category=Translation, VisibleAnywhere)
	FString Version;

	/** Date of this change */
	UPROPERTY(Category=Translation, VisibleAnywhere)//, meta=(DisplayName = "Date & Time"))
	FDateTime DateAndTime;

	/** Source at time of this change */
	UPROPERTY(Category=Translation, VisibleAnywhere)
	FString Source;

	/** Translation at time of this change */
	UPROPERTY(Category=Translation, VisibleAnywhere)
	FString Translation;
};

USTRUCT()
struct FTranslationContextInfo
{
	GENERATED_USTRUCT_BODY()

public:

	/** The key specified in LOCTEXT */
	UPROPERTY(Category=Translation, VisibleAnywhere)
	FString Key;

	/** What file and line this translation is from */
	UPROPERTY(Category=Translation, VisibleAnywhere)
	FString Context;

	/** List of previous versions of the source text for this context */
	UPROPERTY(Category=Translation, VisibleAnywhere)
	TArray<FTranslationChange> Changes;
};

UCLASS(hidecategories = Object, MinimalAPI)
class UTranslationUnit : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** The localization namespace for this translation */
	UPROPERTY(Category=Translation, VisibleAnywhere)
	FString Namespace;

	/** Original text from the source language */
	UPROPERTY(Category=Translation, VisibleAnywhere)
	FString Source;

	/** Translations */
	UPROPERTY(Category=Translation, EditAnywhere)
	FString Translation;

	/** Contexts the source was found in */
	UPROPERTY(Category=Translation, VisibleAnywhere)
	TArray<FTranslationContextInfo> Contexts;

	/** Whether the changes have been reviewed */
	UPROPERTY(Category=Translation, EditAnywhere)
	bool HasBeenReviewed;

	/** If this Translation Unit had a different translation before import, it will be stored here */
	UPROPERTY(Category=Translation, EditAnywhere)
	FString TranslationBeforeImport;

	/**
	* Returns an event delegate that is executed when a property has changed.
	*
	* @return The delegate.
	*/
	DECLARE_EVENT_OneParam(UTranslationUnit, FTranslationUnitPropertyChangedEvent, FName /*PropertyName*/);
	FTranslationUnitPropertyChangedEvent& OnPropertyChanged() { return TranslationUnitPropertyChangedEvent; }

protected:
	/**
	* Called when a property on this object has been modified externally
	*
	* @param PropertyThatChanged the property that was modified
	*/
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;

private:

	// Holds an event delegate that is executed when a property has changed.
	FTranslationUnitPropertyChangedEvent TranslationUnitPropertyChangedEvent;
};