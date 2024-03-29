// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

class SPropertyEditorInteractiveActorPicker : public SButton
{
public:
	SLATE_BEGIN_ARGS( SPropertyEditorInteractiveActorPicker ) {}

	/** Delegate used to filter allowed actors */
	SLATE_EVENT( FOnGetAllowedClasses, OnGetAllowedClasses )

	/** Delegate used to filter allowed actors */
	SLATE_EVENT( FOnShouldFilterActor, OnShouldFilterActor )

	/** Delegate called when an actor is selected */
	SLATE_EVENT( FOnActorSelected, OnActorSelected )

	SLATE_END_ARGS()

	~SPropertyEditorInteractiveActorPicker();

	void Construct( const FArguments& InArgs );

	/** Begin SWidget interface */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent ) OVERRIDE;
	virtual bool SupportsKeyboardFocus() const OVERRIDE;
	/** End SWidget interface */

private:
	/** Delegate for when the button is clicked */
	FReply OnClicked();

	/** Delegate used to filter allowed actors */
	FOnGetAllowedClasses OnGetAllowedClasses;

	/** Delegate used to filter allowed actors */
	FOnShouldFilterActor OnShouldFilterActor;

	/** Delegate called when an actor is selected */
	FOnActorSelected OnActorSelected;
};