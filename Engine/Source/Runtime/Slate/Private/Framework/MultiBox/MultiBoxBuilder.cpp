// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "MultiBox.h"
#include "SHeadingBlock.h"
#include "SMenuEntryBlock.h"
#include "SMenuSeparatorBlock.h"
#include "SToolBarSeparatorBlock.h"
#include "SToolBarButtonBlock.h"
#include "SToolBarComboButtonBlock.h"
#include "SEditableTextBlock.h"
#include "SButtonRowBlock.h"
#include "SWidgetBlock.h"
#include "SGroupMarkerBlock.h"
#include "STutorialWrapper.h"

FMultiBoxBuilder::FMultiBoxBuilder( const EMultiBoxType::Type InType, FMultiBoxCustomization InCustomization, const bool bInShouldCloseWindowAfterMenuSelection, const TSharedPtr< const FUICommandList >& InCommandList, TSharedPtr<FExtender> InExtender )
	: MultiBox( FMultiBox::Create( InType, InCustomization, bInShouldCloseWindowAfterMenuSelection ) )
	, CommandListStack()
{
	CommandListStack.Push( InCommandList );
	ExtenderStack.Push(InExtender);
}

void FMultiBoxBuilder::AddEditableText( const FText& InLabel, const FText& InToolTip, const FSlateIcon& InIcon, const TAttribute< FText >& InTextAttribute, const FOnTextCommitted& InOnTextCommitted, const FOnTextChanged& InOnTextChanged, bool bInReadOnly )
{
	MultiBox->AddMultiBlock( MakeShareable( new FEditableTextBlock( InLabel, InToolTip, InIcon, InTextAttribute, bInReadOnly, InOnTextCommitted, InOnTextChanged ) ) );
}

void FMultiBoxBuilder::PushCommandList( const TSharedRef< const FUICommandList > CommandList )
{
	CommandListStack.Push( CommandList );
}

void FMultiBoxBuilder::PopCommandList()
{
	// Never allowed to pop the last command-list!  This command-list was set when the multibox was first created and is canonical.
	if( ensure( CommandListStack.Num() > 1 ) )
	{
		CommandListStack.Pop();
	}
}

TSharedPtr<const FUICommandList> FMultiBoxBuilder::GetTopCommandList()
{
	return (CommandListStack.Num() > 0) ? CommandListStack.Top() : TSharedPtr<const FUICommandList>(NULL);
}

void FMultiBoxBuilder::PushExtender( TSharedRef< FExtender > InExtender )
{
	ExtenderStack.Push( InExtender );
}

void FMultiBoxBuilder::PopExtender()
{
	// Never allowed to pop the last extender! This extender was set when the multibox was first created and is canonical.
	if( ensure( ExtenderStack.Num() > 1 ) )
	{
		ExtenderStack.Pop();
	}
}

const ISlateStyle* FMultiBoxBuilder::GetStyleSet() const 
{ 
	return MultiBox->GetStyleSet();
}

const FName& FMultiBoxBuilder::GetStyleName() const 
{ 
	return MultiBox->GetStyleName();
}

void FMultiBoxBuilder::SetStyle( const ISlateStyle* InStyleSet, const FName& InStyleName ) 
{ 
	MultiBox->SetStyle( InStyleSet, InStyleName ); 
}

FMultiBoxCustomization FMultiBoxBuilder::GetCustomization() const
{
	return FMultiBoxCustomization( MultiBox->GetCustomizationName() ); 
}

TSharedRef< class SWidget > FMultiBoxBuilder::MakeWidget()
{
	return MultiBox->MakeWidget();
}


FBaseMenuBuilder::FBaseMenuBuilder( const EMultiBoxType::Type InType, const bool bInShouldCloseWindowAfterMenuSelection, TSharedPtr< const FUICommandList > InCommandList, bool bInCloseSelfOnly, TSharedPtr<FExtender> InExtender, const ISlateStyle* InStyleSet )
	: FMultiBoxBuilder( InType, FMultiBoxCustomization::None, bInShouldCloseWindowAfterMenuSelection, InCommandList, InExtender )
	, bCloseSelfOnly( bInCloseSelfOnly )
{
	MultiBox->SetStyle(InStyleSet, "Menu");
}

void FBaseMenuBuilder::AddMenuEntry( const TSharedPtr< const FUICommandInfo > InCommand, FName InExtensionHook, const TAttribute<FText>& InLabelOverride, const TAttribute<FText>& InToolTipOverride, const FSlateIcon& InIconOverride )
{
	ApplySectionBeginning();

	ApplyHook(InExtensionHook, EExtensionHook::Before);
	
	// The command must be valid
	check( InCommand.IsValid() );
	TSharedRef< FMenuEntryBlock > NewMenuEntryBlock( new FMenuEntryBlock( InExtensionHook, InCommand, CommandListStack.Last(), InLabelOverride, InToolTipOverride, InIconOverride, bCloseSelfOnly ) );
	MultiBox->AddMultiBlock( NewMenuEntryBlock );
	
	ApplyHook(InExtensionHook, EExtensionHook::After);
}

void FBaseMenuBuilder::AddMenuEntry( const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const FSlateIcon& InIcon, const FUIAction& InAction, FName InExtensionHook, const EUserInterfaceActionType::Type UserInterfaceActionType )
{
	ApplySectionBeginning();

	ApplyHook(InExtensionHook, EExtensionHook::Before);
	
	TSharedRef< FMenuEntryBlock > NewMenuEntryBlock( new FMenuEntryBlock( InExtensionHook, InLabel, InToolTip, InIcon, InAction, UserInterfaceActionType, bCloseSelfOnly ) );
	MultiBox->AddMultiBlock( NewMenuEntryBlock );
	
	ApplyHook(InExtensionHook, EExtensionHook::After);
}

void FBaseMenuBuilder::AddMenuEntry( const FUIAction& UIAction, const TSharedRef< SWidget > Contents, const FName& InExtensionHook, const EUserInterfaceActionType::Type UserInterfaceActionType )
{
	ApplySectionBeginning();

	ApplyHook(InExtensionHook, EExtensionHook::Before);

	TSharedRef< FMenuEntryBlock > NewMenuEntryBlock( new FMenuEntryBlock( InExtensionHook, UIAction, Contents, UserInterfaceActionType, bCloseSelfOnly ) );
	MultiBox->AddMultiBlock( NewMenuEntryBlock );

	ApplyHook(InExtensionHook, EExtensionHook::After);
}

void FMenuBuilder::BeginSection( FName InExtensionHook, const TAttribute< FText >& InHeadingText )
{
	check(CurrentSectionExtensionHook == NAME_None && !bSectionNeedsToBeApplied);

	ApplyHook(InExtensionHook, EExtensionHook::Before);
	
	// Do not actually apply the section header, because if this section is ended immediately
	// then nothing ever gets created, preventing empty sections from ever appearing
	bSectionNeedsToBeApplied = true;
	CurrentSectionExtensionHook = InExtensionHook;
	CurrentSectionHeadingText = InHeadingText.Get();

	// Do apply the section beginning if we are in developer "show me all the hooks" mode
	if (FMultiBoxSettings::DisplayMultiboxHooks.Get())
	{
		ApplySectionBeginning();
	}
	
	ApplyHook(InExtensionHook, EExtensionHook::First);
}

void FMenuBuilder::EndSection()
{
	FName SectionExtensionHook = CurrentSectionExtensionHook;
	CurrentSectionExtensionHook = NAME_None;
	bSectionNeedsToBeApplied = false;
	CurrentSectionHeadingText = FText::GetEmpty();

	ApplyHook(SectionExtensionHook, EExtensionHook::After);
}

void FMenuBuilder::AddMenuSeparator(FName InExtensionHook)
{
	ApplySectionBeginning();

	ApplyHook(InExtensionHook, EExtensionHook::Before);

	// Never add a menu separate as the first item, even if we were asked to
	if( MultiBox->GetBlocks().Num() > 0 || FMultiBoxSettings::DisplayMultiboxHooks.Get() )
	{
		TSharedRef< FMenuSeparatorBlock > NewMenuSeparatorBlock( new FMenuSeparatorBlock(InExtensionHook) );
		MultiBox->AddMultiBlock( NewMenuSeparatorBlock );
	}

	ApplyHook(InExtensionHook, EExtensionHook::After);
}

void FMenuBuilder::AddSubMenu( const FText& InMenuLabel, const FText& InToolTip, const FNewMenuDelegate& InSubMenu, const FUIAction& InUIAction, FName InExtensionHook, const EUserInterfaceActionType::Type InUserInterfaceActionType, const bool bInOpenSubMenuOnClick, const FSlateIcon& InIcon )
{
	ApplySectionBeginning();

	const bool bIsSubMenu = true;
	TSharedRef< FMenuEntryBlock > NewMenuEntryBlock( new FMenuEntryBlock( InExtensionHook, InMenuLabel, InToolTip, InSubMenu, ExtenderStack.Top(), bIsSubMenu, bInOpenSubMenuOnClick, InIcon, InUIAction, InUserInterfaceActionType, bCloseSelfOnly ) );

	MultiBox->AddMultiBlock( NewMenuEntryBlock );
}

void FMenuBuilder::AddSubMenu( const FText& InMenuLabel, const FText& InToolTip, const FNewMenuDelegate& InSubMenu, const bool bInOpenSubMenuOnClick /*= false*/, const FSlateIcon& InIcon /*= FSlateIcon()*/ )
{
	ApplySectionBeginning();

	const bool bIsSubMenu = true;
	TSharedRef< FMenuEntryBlock > NewMenuEntryBlock( new FMenuEntryBlock( NAME_None, InMenuLabel, InToolTip, InSubMenu, ExtenderStack.Top(), bIsSubMenu, bInOpenSubMenuOnClick, CommandListStack.Last(), bCloseSelfOnly, InIcon ) );

	MultiBox->AddMultiBlock( NewMenuEntryBlock );
}

void FMenuBuilder::AddSubMenu( const TSharedRef< SWidget > Contents, const FNewMenuDelegate& InSubMenu, const bool bInOpenSubMenuOnClick /*= false*/ )
{
	ApplySectionBeginning();

	const bool bIsSubMenu = true;
	TSharedRef< FMenuEntryBlock > NewMenuEntryBlock( new FMenuEntryBlock( NAME_None, Contents, InSubMenu, ExtenderStack.Top(), bIsSubMenu, bInOpenSubMenuOnClick, CommandListStack.Last(), bCloseSelfOnly ) );

	MultiBox->AddMultiBlock( NewMenuEntryBlock );
}

void FMenuBuilder::AddSubMenu( const FUIAction& UIAction, const TSharedRef< SWidget > Contents, const FNewMenuDelegate& InSubMenu )
{
	ApplySectionBeginning();

	const bool bIsSubMenu = true;
	TSharedRef< FMenuEntryBlock > NewMenuEntryBlock( new FMenuEntryBlock( NAME_None, UIAction, Contents, InSubMenu, ExtenderStack.Top(), bIsSubMenu, CommandListStack.Last(), bCloseSelfOnly ) );

	MultiBox->AddMultiBlock( NewMenuEntryBlock );
}

void FMenuBuilder::AddWrapperSubMenu( const FText& InMenuLabel, const FText& InToolTip, const FOnGetContent& InSubMenu, const FSlateIcon& InIcon )
{
	ApplySectionBeginning();

	const bool bIsSubMenu = true;
	TSharedRef< FMenuEntryBlock > NewMenuEntryBlock( new FMenuEntryBlock( NAME_None, InMenuLabel, InToolTip, InSubMenu, ExtenderStack.Top(), bIsSubMenu, false, CommandListStack.Last(), bCloseSelfOnly, InIcon ) );

	MultiBox->AddMultiBlock( NewMenuEntryBlock );
}

void FMenuBuilder::AddWrapperSubMenu( const FText& InMenuLabel, const FText& InToolTip, const TSharedPtr<SWidget>& InSubMenu, const FSlateIcon& InIcon )
{
	ApplySectionBeginning();

	const bool bIsSubMenu = true;
	TSharedRef< FMenuEntryBlock > NewMenuEntryBlock( new FMenuEntryBlock( NAME_None, InMenuLabel, InToolTip, InSubMenu, ExtenderStack.Top(), bIsSubMenu, false, CommandListStack.Last(), bCloseSelfOnly, InIcon ) );

	MultiBox->AddMultiBlock( NewMenuEntryBlock );
}

void FMenuBuilder::AddWidget( TSharedRef<SWidget> InWidget, const FText& Label, bool bNoIndent )
{
	ApplySectionBeginning();

	TSharedRef< FWidgetBlock > NewWidgetBlock( new FWidgetBlock( InWidget, Label, bNoIndent ) );
	MultiBox->AddMultiBlock( NewWidgetBlock );
}

void FMenuBuilder::ApplyHook(FName InExtensionHook, EExtensionHook::Position HookPosition)
{
	// this is a virtual function to get a properly typed "this" pointer
	auto& Extender = ExtenderStack.Top();
	if (InExtensionHook != NAME_None && Extender.IsValid())
	{
		Extender->Apply(InExtensionHook, HookPosition, *this);
	}
}

void FMenuBuilder::ApplySectionBeginning()
{
	if (bSectionNeedsToBeApplied)
	{
		if( MultiBox->GetBlocks().Num() > 0 || FMultiBoxSettings::DisplayMultiboxHooks.Get() )
		{
			MultiBox->AddMultiBlock( MakeShareable( new FMenuSeparatorBlock(CurrentSectionExtensionHook) ) );
		}
		if (!CurrentSectionHeadingText.IsEmpty())
		{
			MultiBox->AddMultiBlock( MakeShareable( new FHeadingBlock(CurrentSectionExtensionHook, CurrentSectionHeadingText) ) );
		}
		bSectionNeedsToBeApplied = false;
		CurrentSectionHeadingText = FText::GetEmpty();
	}
}



void FMenuBarBuilder::AddPullDownMenu( const FText& InMenuLabel, const FText& InToolTip, const FNewMenuDelegate& InPullDownMenu, FName InExtensionHook, FName InTutorialHighlightName )
{
	ApplySectionBeginning();

	ApplyHook(InExtensionHook, EExtensionHook::Before);

	const bool bIsSubMenu = false;
	const bool bOpenSubMenuOnClick = false;
	// Pulldown menus always close all menus not just themselves
	const bool bCloseSelfOnly = false;
	TSharedRef< FMenuEntryBlock > NewMenuEntryBlock( new FMenuEntryBlock( InExtensionHook, InMenuLabel, InToolTip, InPullDownMenu, ExtenderStack.Top(), bIsSubMenu, bOpenSubMenuOnClick, CommandListStack.Last(), bCloseSelfOnly ) );
	NewMenuEntryBlock->SetTutorialHightlightName(InTutorialHighlightName);
	MultiBox->AddMultiBlock( NewMenuEntryBlock );
	
	ApplyHook(InExtensionHook, EExtensionHook::After);
}

void FMenuBarBuilder::ApplyHook(FName InExtensionHook, EExtensionHook::Position HookPosition)
{
	// this is a virtual function to get a properly typed "this" pointer
	auto& Extender = ExtenderStack.Top();
	if (InExtensionHook != NAME_None && Extender.IsValid())
	{
		Extender->Apply(InExtensionHook, HookPosition, *this);
	}
}

void FToolBarBuilder::AddToolBarButton( const TSharedPtr< const FUICommandInfo > InCommand, FName InExtensionHook, const TAttribute<FText>& InLabelOverride, const TAttribute<FText>& InToolTipOverride, const TAttribute<FSlateIcon>& InIconOverride, FName TutorialHighlightName)
{
	ApplySectionBeginning();

	ApplyHook(InExtensionHook, EExtensionHook::Before);

	TSharedRef< FToolBarButtonBlock > NewToolBarButtonBlock( new FToolBarButtonBlock( InCommand.ToSharedRef(), CommandListStack.Last(), InLabelOverride, InToolTipOverride, InIconOverride ) );

	if ( LabelVisibility.IsSet() )
	{
		NewToolBarButtonBlock->SetLabelVisibility( LabelVisibility.GetValue() );
	}

	NewToolBarButtonBlock->SetIsFocusable(bIsFocusable);
	NewToolBarButtonBlock->SetForceSmallIcons(bForceSmallIcons);
	NewToolBarButtonBlock->SetTutorialHightlightName(TutorialHighlightName);

	MultiBox->AddMultiBlock( NewToolBarButtonBlock );

	ApplyHook(InExtensionHook, EExtensionHook::After);
}

void FToolBarBuilder::AddToolBarButton( const FUIAction& InAction, FName InExtensionHook, const TAttribute<FText>& InLabelOverride, const TAttribute<FText>& InToolTipOverride, const TAttribute<FSlateIcon>& InIconOverride, const EUserInterfaceActionType::Type UserInterfaceActionType, FName TutorialHighlightName )
{
	ApplySectionBeginning();

	ApplyHook(InExtensionHook, EExtensionHook::Before);

	TSharedRef< FToolBarButtonBlock > NewToolBarButtonBlock( new FToolBarButtonBlock( InLabelOverride, InToolTipOverride, InIconOverride, InAction, UserInterfaceActionType ) );

	if ( LabelVisibility.IsSet() )
	{
		NewToolBarButtonBlock->SetLabelVisibility( LabelVisibility.GetValue() );
	}

	NewToolBarButtonBlock->SetIsFocusable(bIsFocusable);
	NewToolBarButtonBlock->SetForceSmallIcons(bForceSmallIcons);
	NewToolBarButtonBlock->SetTutorialHightlightName(TutorialHighlightName);

	MultiBox->AddMultiBlock( NewToolBarButtonBlock );

	ApplyHook(InExtensionHook, EExtensionHook::After);
}

void FToolBarBuilder::AddComboButton( const FUIAction& InAction, const FOnGetContent& InMenuContentGenerator, const TAttribute<FText>& InLabelOverride, const TAttribute<FText>& InToolTipOverride, const TAttribute<FSlateIcon>& InIconOverride, bool bInSimpleComboBox )
{
	ApplySectionBeginning();

	TSharedRef< FToolBarComboButtonBlock > NewToolBarComboButtonBlock( new FToolBarComboButtonBlock( InAction, InMenuContentGenerator, InLabelOverride, InToolTipOverride, InIconOverride, bInSimpleComboBox ) );

	if ( LabelVisibility.IsSet() )
	{
		NewToolBarComboButtonBlock->SetLabelVisibility( LabelVisibility.GetValue() );
	}

	NewToolBarComboButtonBlock->SetForceSmallIcons(bForceSmallIcons);

	MultiBox->AddMultiBlock( NewToolBarComboButtonBlock );
}

void FToolBarBuilder::AddWidget( TSharedRef<SWidget> InWidget, FName TutorialHighlightName )
{
	ApplySectionBeginning();

	// If tutorial name specified, wrap in tutorial wrapper
	if (TutorialHighlightName != NAME_None)
	{
		TSharedRef<SWidget> ChildWidget = InWidget;

		InWidget = 
			SNew( STutorialWrapper, TutorialHighlightName )
			[
				ChildWidget
			];
	}
	
	TSharedRef< FWidgetBlock > NewWidgetBlock( new FWidgetBlock( InWidget, FText::GetEmpty(), true ) );
	MultiBox->AddMultiBlock( NewWidgetBlock );
}

void FToolBarBuilder::AddSeparator(FName InExtensionHook)
{
	ApplySectionBeginning();

	ApplyHook(InExtensionHook, EExtensionHook::Before);

	// Never add a menu separate as the first item, even if we were asked to
	if( MultiBox->GetBlocks().Num() > 0 || FMultiBoxSettings::DisplayMultiboxHooks.Get() )
	{
		MultiBox->AddMultiBlock( MakeShareable( new FToolBarSeparatorBlock(InExtensionHook) ) );
	}

	ApplyHook(InExtensionHook, EExtensionHook::After);
}

void FToolBarBuilder::BeginSection( FName InExtensionHook )
{
	checkf(CurrentSectionExtensionHook == NAME_None && !bSectionNeedsToBeApplied, TEXT("Did you forget to call EndSection()?"));

	ApplyHook(InExtensionHook, EExtensionHook::Before);
	
	// Do not actually apply the section header, because if this section is ended immediately
	// then nothing ever gets created, preventing empty sections from ever appearing
	bSectionNeedsToBeApplied = true;
	CurrentSectionExtensionHook = InExtensionHook;
	
	// Do apply the section beginning if we are in developer "show me all the hooks" mode
	if (FMultiBoxSettings::DisplayMultiboxHooks.Get())
	{
		ApplySectionBeginning();
	}
	
	ApplyHook(InExtensionHook, EExtensionHook::First);
}

void FToolBarBuilder::EndSection()
{
	FName SectionExtensionHook = CurrentSectionExtensionHook;
	CurrentSectionExtensionHook = NAME_None;
	bSectionNeedsToBeApplied = false;

	ApplyHook(SectionExtensionHook, EExtensionHook::After);
}

void FToolBarBuilder::ApplyHook(FName InExtensionHook, EExtensionHook::Position HookPosition)
{
	// this is a virtual function to get a properly typed "this" pointer
	auto& Extender = ExtenderStack.Top();
	if (InExtensionHook != NAME_None && Extender.IsValid())
	{
		Extender->Apply(InExtensionHook, HookPosition, *this);
	}
}

void FToolBarBuilder::ApplySectionBeginning()
{
	if (bSectionNeedsToBeApplied)
	{
		if( MultiBox->GetBlocks().Num() > 0 || FMultiBoxSettings::DisplayMultiboxHooks.Get() )
		{
			MultiBox->AddMultiBlock( MakeShareable( new FToolBarSeparatorBlock(CurrentSectionExtensionHook) ) );
		}
		bSectionNeedsToBeApplied = false;
	}
}

void FToolBarBuilder::EndBlockGroup()
{
	ApplySectionBeginning();

	TSharedRef< FGroupEndBlock > NewGroupEndBlock( new FGroupEndBlock() );

	MultiBox->AddMultiBlock( NewGroupEndBlock );
}

void FToolBarBuilder::BeginBlockGroup()
{
	ApplySectionBeginning();

	TSharedRef< FGroupStartBlock > NewGroupStartBlock( new FGroupStartBlock() );

	MultiBox->AddMultiBlock( NewGroupStartBlock );
}

void FButtonRowBuilder::AddButton( const TSharedPtr< const FUICommandInfo > InCommand, const TAttribute<FText>& InLabelOverride, const TAttribute<FText>& InToolTipOverride, const FSlateIcon& InIconOverride )
{
	ApplySectionBeginning();

	TSharedRef< FButtonRowBlock > NewButtonRowBlock( new FButtonRowBlock( InCommand.ToSharedRef(), CommandListStack.Last(), InLabelOverride, InToolTipOverride, InIconOverride ) );

	MultiBox->AddMultiBlock( NewButtonRowBlock );
}

void FButtonRowBuilder::AddButton( const FText& InLabel, const FText& InToolTip, const FUIAction& UIAction, const FSlateIcon& InIcon, const EUserInterfaceActionType::Type UserInterfaceActionType )
{
	ApplySectionBeginning();

	TSharedRef< FButtonRowBlock > NewButtonRowBlock( new FButtonRowBlock( InLabel, InToolTip, InIcon, UIAction, UserInterfaceActionType ) );

	MultiBox->AddMultiBlock( NewButtonRowBlock );
}
