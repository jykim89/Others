// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SSessionBrowserInstanceListRow.h: Declares the SSessionBrowserInstanceListRow class.
=============================================================================*/

#pragma once


#define LOCTEXT_NAMESPACE "SSessionBrowserInstanceListRow"


/**
 * Delegate type for changed instance check box state changes.
 *
 * The first parameter is the engine instance that was checked or unchecked.
 * The second parameter is the new checked state.
 */
DECLARE_DELEGATE_TwoParams(FOnSessionInstanceCheckStateChanged, const ISessionInstanceInfoPtr&, ESlateCheckBoxState::Type)


/**
 * Implements a row widget for the session instance list.
 */
class SSessionBrowserInstanceListRow
	: public SMultiColumnTableRow<ISessionInstanceInfoPtr>
{
public:

	SLATE_BEGIN_ARGS(SSessionBrowserInstanceListRow) { }
		SLATE_ARGUMENT(ISessionInstanceInfoPtr, InstanceInfo)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs - The construction arguments.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		InstanceInfo = InArgs._InstanceInfo;

		SMultiColumnTableRow<ISessionInstanceInfoPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

public:

	/**
	 * Generates the widget for the specified column.
	 *
	 * @param ColumnName - The name of the column to generate the widget for.
	 *
	 * @return The widget.
	 */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) OVERRIDE
	{
		if (ColumnName == "Device")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SSessionBrowserInstanceListRow::HandleTextColorAndOpacity)
						.Text(this, &SSessionBrowserInstanceListRow::HandleDeviceColumnText)
				];
		}
		else if (ColumnName == "Level")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0, 0.0))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SSessionBrowserInstanceListRow::HandleTextColorAndOpacity)
						.Text(this, &SSessionBrowserInstanceListRow::HandleLevelColumnText)
				];
		}
		else if (ColumnName == "Name")
		{
			return SNew(SBox)
				.Padding(FMargin(1.0f, 1.0f, 4.0f, 0.0f))
				.HAlign(HAlign_Left)
				[
					SNew(SBorder)
						.BorderBackgroundColor(this, &SSessionBrowserInstanceListRow::HandleInstanceBorderBackgroundColor)
						.BorderImage(this, &SSessionBrowserInstanceListRow::HandleInstanceBorderBrush)
						.ColorAndOpacity(FLinearColor(0.25f, 0.25f, 0.25f))
						.Padding(FMargin(6.0f, 4.0f))
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Font(FEditorStyle::GetFontStyle("BoldFont"))
								.Text(InstanceInfo->GetInstanceName())							
						]
				];
		}
		else if (ColumnName == "Platform")
		{
			return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
						.Image(FEditorStyle::GetBrush(*FString::Printf(TEXT("Launcher.Platform_%s"), *InstanceInfo->GetPlatformName())))
				]

			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::FromString(InstanceInfo->GetPlatformName()))
				];
		}
		else if (ColumnName == "Status")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SSessionBrowserInstanceListRow::HandleTextColorAndOpacity)
						.Text(this, &SSessionBrowserInstanceListRow::HandleStatusColumnText)
				];
		}
		else if (ColumnName == "Type")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::FromString(InstanceInfo->GetInstanceType()))
				];
		}

		return SNullWidget::NullWidget;
	}

private:

	// Callback for getting the text in the 'Device' column.
	FText HandleDeviceColumnText( ) const
	{
		return FText::FromString(InstanceInfo->GetDeviceName());
	}

	// Callback for getting the border color for this row.
	FSlateColor HandleInstanceBorderBackgroundColor( ) const
	{
		return FLinearColor((GetTypeHash(InstanceInfo->GetInstanceId()) & 0xff) * 360.0 / 256.0, 0.8, 0.3, 1.0).HSVToLinearRGB();
	}

	// Callback for getting the border brush for this row.
	const FSlateBrush* HandleInstanceBorderBrush( ) const
	{
		if (FDateTime::UtcNow() - InstanceInfo->GetLastUpdateTime() < FTimespan::FromSeconds(10.0))
		{
			return FEditorStyle::GetBrush("ErrorReporting.Box");
		}

		return FEditorStyle::GetBrush("ErrorReporting.EmptyBox");
	}

	// Callback for getting the instance's current level.
	FString HandleLevelColumnText( ) const
	{
		return InstanceInfo->GetCurrentLevel();
	}

	// Callback for getting the text in the 'Status' column.
	FText HandleStatusColumnText( ) const
	{
		if (FDateTime::UtcNow() - InstanceInfo->GetLastUpdateTime() < FTimespan::FromSeconds(10.0))
		{
			return LOCTEXT("StatusRunning", "Running");
		}

		return LOCTEXT("StatusTimedOut", "Timed Out");
	}

	// Callback for getting the foreground text color.
	FSlateColor HandleTextColorAndOpacity( ) const
	{
		if (FDateTime::UtcNow() - InstanceInfo->GetLastUpdateTime() < FTimespan::FromSeconds(10.0))
		{
			return FSlateColor::UseForeground();
		}

		return FSlateColor::UseSubduedForeground();
	}

private:

	// Holds a reference to the instance info that is displayed in this row.
	ISessionInstanceInfoPtr InstanceInfo;

	// Holds a delegate to be invoked when the check box state changed.
	FOnCheckStateChanged OnCheckStateChanged;
};


#undef LOCTEXT_NAMESPACE
