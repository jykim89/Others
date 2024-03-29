// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SSessionLauncherDeployRepositorySettings.cpp: Implements the SSessionLauncherDeployRepositorySettings class.
=============================================================================*/

#include "SessionLauncherPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SSessionLauncherDeployRepositorySettings"


/* SSessionLauncherDeployRepositorySettings interface
 *****************************************************************************/

void SSessionLauncherDeployRepositorySettings::Construct( const FArguments& InArgs, const FSessionLauncherModelRef& InModel )
{
	Model = InModel;

	ChildSlot
	[
		SNew(SVerticalBox)


		+ SVerticalBox::Slot()
		.FillHeight(1.0)
		[
			SNew(SBorder)
			.Padding(8.0)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RepositoryPathLabel", "Repository Path:"))
				]

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0, 4.0, 0.0, 0.0)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0)
						.Padding(0.0, 0.0, 0.0, 3.0)
						[
							// repository path text box
							SAssignNew(RepositoryPathTextBox, SEditableTextBox)
							.OnTextCommitted(this, &SSessionLauncherDeployRepositorySettings::OnTextCommitted)
							.OnTextChanged(this, &SSessionLauncherDeployRepositorySettings::OnTextChanged)
						]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Right)
							.Padding(4.0, 0.0, 0.0, 0.0)
							[
								// browse button
								SNew(SButton)
								.ContentPadding(FMargin(6.0, 2.0))
								.IsEnabled(true)
								.Text(LOCTEXT("BrowseButtonText", "Browse..."))
								.ToolTipText(LOCTEXT("BrowseButtonToolTip", "Browse for the repository"))
								.OnClicked(this, &SSessionLauncherDeployRepositorySettings::HandleBrowseButtonClicked)
							]
					]
			]
		]

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
					.Padding(8.0f)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					[
						// deploy targets area
						SNew(SSessionLauncherDeployTargets, InModel)
					]
			]
	];
}


/* SSessionLauncherDeployRepositorySettings callbacks
 *****************************************************************************/

FReply SSessionLauncherDeployRepositorySettings::HandleBrowseButtonClicked( )
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if ( DesktopPlatform )
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

		FString FolderName;
		const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
			ParentWindowHandle,
			LOCTEXT("RepositoryBrowseTitle", "Choose a repository location").ToString(),
			RepositoryPathTextBox->GetText().ToString(),
			FolderName
			);

		if ( bFolderSelected )
		{
			if ( !FolderName.EndsWith(TEXT("/")) )
			{
				FolderName += TEXT("/");
			}

			RepositoryPathTextBox->SetText(FText::FromString(FolderName));
			ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

			if(SelectedProfile.IsValid())
			{
				SelectedProfile->SetPackageDirectory(FolderName);
			}
		}
	}

	return FReply::Handled();
}

void SSessionLauncherDeployRepositorySettings::OnTextChanged(const FText& InText)
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if(SelectedProfile.IsValid())
	{
		SelectedProfile->SetPackageDirectory(InText.ToString());
	}
}

void SSessionLauncherDeployRepositorySettings::OnTextCommitted( const FText& InText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

		if(SelectedProfile.IsValid())
		{
			SelectedProfile->SetPackageDirectory(InText.ToString());
		}
	}
}

#undef LOCTEXT_NAMESPACE
