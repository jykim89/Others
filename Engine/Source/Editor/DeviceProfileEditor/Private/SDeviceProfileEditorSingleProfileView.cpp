// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SDeviceProfileEditorSingleProfileView.cpp: Implements the SDeviceProfileEditorSingleProfileView class.
=============================================================================*/


#include "DeviceProfileEditorPCH.h"
#include "PropertyEditorModule.h"


#define LOCTEXT_NAMESPACE "DeviceProfileEditorSingleProfileView"


void  SDeviceProfileEditorSingleProfileView::Construct(const FArguments& InArgs, TWeakObjectPtr< UDeviceProfile > InDeviceProfileToView)
{
	EditingProfile = InDeviceProfileToView;

	// initialize settings view
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bSearchInitialKeyFocus = false;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bObjectsUseNameArea = false;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bShowOptions = false;
	}

	SettingsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	SettingsView->SetObject(EditingProfile.Get());

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("NoBorder"))
		[
			SettingsView.ToSharedRef()
		]
	];
}


#undef LOCTEXT_NAMESPACE