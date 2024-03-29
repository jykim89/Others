// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlModule.h"

namespace ELoginConnectionState
{
	enum Type
	{
		Disconnected,
		Connecting,
		Connected,
	};
}

#if SOURCE_CONTROL_WITH_SLATE

class SSourceControlLogin : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SSourceControlLogin) {}
	
	/** A reference to the parent window */
	SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)

	/** Callback to be called when the "Disable Source Control" button is pressed. */
	SLATE_ARGUMENT(FSourceControlLoginClosed, OnSourceControlLoginClosed)

	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs);

	/** SWidget interface */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) OVERRIDE;

	/**
	 * Refresh the displayed settings. Usually called when a provider is changed.
	 */
	void RefreshSettings();

private:

	/** Delegate called when the user clicks the 'Accept Settings' button */
	FReply OnAcceptSettings();

	/** Delegate called when the user clicks the 'Disable Source Control' button */
	FReply OnDisableSourceControl();

	/** Called when a connection attempt fails */
	void DisplayConnectionError(const FText& InErrorText = FText()) const;

	/** Called when a connection attempt succeeds */
	void DisplayConnectionSuccess() const;

	/** Delegate called form the source control system when a login attempt has completed */
	void SourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);

	/** Delegate to determine control enabled state */
	bool AreControlsEnabled() const;

	/** Delegate to determine 'accept settings' button enabled state */
	bool IsAcceptSettingsEnabled() const;

	/** Delegate to determine visibility of the throbber */
	EVisibility GetThrobberVisibility() const;

	/** Delegate to determine visibility of the settings widget */
	EVisibility GetSettingsVisibility() const;

	/** Delegate to determine visibility of the disabled text widget */
	EVisibility GetDisabledTextVisibility() const;

private:

	/** The parent window of this widget */
	TWeakPtr<SWindow> ParentWindowPtr;

	/** Holds the details view. */
	TSharedPtr<class IDetailsView> DetailsView;

	/** Current connection state */
	ELoginConnectionState::Type ConnectionState;

	/** Delegate called when the window is closed */
	FSourceControlLoginClosed SourceControlLoginClosed;

	/** The currently displayed settings widget container */
	TSharedPtr<class SBorder> SettingsBorder;
};

#endif // SOURCE_CONTROL_WITH_SLATE
