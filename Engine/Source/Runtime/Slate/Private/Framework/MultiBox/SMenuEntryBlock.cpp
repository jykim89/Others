// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "MultiBox.h"
#include "SMenuEntryBlock.h"


FMenuEntryBlock::FMenuEntryBlock( const FName& InExtensionHook, const TSharedPtr< const FUICommandInfo > InCommand, TSharedPtr< const FUICommandList > InCommandList, const TAttribute<FText>& InLabelOverride, const TAttribute<FText>& InToolTipOverride, const FSlateIcon& InIconOverride, bool bInCloseSelfOnly )
	: FMultiBlock( InCommand, InCommandList, InExtensionHook )
	, LabelOverride( InLabelOverride )
	, ToolTipOverride( InToolTipOverride )
	, IconOverride( InIconOverride )
	, bIsSubMenu( false )
	, bOpenSubMenuOnClick( false )
	, UserInterfaceActionType( EUserInterfaceActionType::Button )
	, bCloseSelfOnly( bInCloseSelfOnly )
{
}


FMenuEntryBlock::FMenuEntryBlock( const FName& InExtensionHook, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const FNewMenuDelegate& InEntryBuilder, TSharedPtr<FExtender> InExtender, bool bInSubMenu, bool bInSubMenuOnClick, const FSlateIcon& InIcon, const FUIAction& InUIAction, const EUserInterfaceActionType::Type InUserInterfaceActionType, bool bInCloseSelfOnly )
	: FMultiBlock( InUIAction, InExtensionHook )
	, LabelOverride( InLabel )
	, ToolTipOverride( InToolTip )
	, IconOverride( InIcon )
	, EntryBuilder( InEntryBuilder )
	, bIsSubMenu( bInSubMenu )
	, bOpenSubMenuOnClick( bInSubMenuOnClick )
	, UserInterfaceActionType( InUserInterfaceActionType )
	, bCloseSelfOnly( bInCloseSelfOnly )
	, Extender( InExtender )	
{
}


FMenuEntryBlock::FMenuEntryBlock( const FName& InExtensionHook, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const FSlateIcon& InIcon, const FUIAction& UIAction, const EUserInterfaceActionType::Type InUserInterfaceActionType, bool bInCloseSelfOnly )
	: FMultiBlock( UIAction, InExtensionHook )
	, LabelOverride( InLabel )
	, ToolTipOverride( InToolTip )
	, IconOverride( InIcon )
	, bIsSubMenu( false )
	, bOpenSubMenuOnClick( false )
	, UserInterfaceActionType( InUserInterfaceActionType )
	, bCloseSelfOnly( bInCloseSelfOnly )
{
}


FMenuEntryBlock::FMenuEntryBlock( const FName& InExtensionHook, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const FNewMenuDelegate& InEntryBuilder, TSharedPtr<FExtender> InExtender, bool bInSubMenu, bool bInSubMenuOnClick, TSharedPtr< const FUICommandList > InCommandList, bool bInCloseSelfOnly, const FSlateIcon& InIcon )
	: FMultiBlock( NULL, InCommandList, InExtensionHook )
	, LabelOverride( InLabel )
	, ToolTipOverride( InToolTip )
	, IconOverride( InIcon )
	, EntryBuilder( InEntryBuilder )
	, bIsSubMenu( bInSubMenu )
	, bOpenSubMenuOnClick( bInSubMenuOnClick )
	, UserInterfaceActionType( EUserInterfaceActionType::Button )
	, bCloseSelfOnly( bInCloseSelfOnly )
	, Extender( InExtender )
{
}


FMenuEntryBlock::FMenuEntryBlock( const FName& InExtensionHook, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const FOnGetContent& InMenuBuilder, TSharedPtr<FExtender> InExtender, bool bInSubMenu, bool bInSubMenuOnClick, TSharedPtr< const FUICommandList > InCommandList, bool bInCloseSelfOnly, const FSlateIcon& InIcon )
	: FMultiBlock( NULL, InCommandList, InExtensionHook )
	, LabelOverride( InLabel )
	, ToolTipOverride( InToolTip )
	, IconOverride( InIcon )
	, MenuBuilder( InMenuBuilder )
	, bIsSubMenu( bInSubMenu )
	, bOpenSubMenuOnClick( bInSubMenuOnClick )
	, UserInterfaceActionType( EUserInterfaceActionType::Button )
	, bCloseSelfOnly( bInCloseSelfOnly )
	, Extender( InExtender )
{
}


FMenuEntryBlock::FMenuEntryBlock( const FName& InExtensionHook, const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const TSharedPtr<SWidget>& InEntryWidget, TSharedPtr<FExtender> InExtender, bool bInSubMenu, bool bInSubMenuOnClick, TSharedPtr< const FUICommandList > InCommandList, bool bInCloseSelfOnly, const FSlateIcon& InIcon )
	: FMultiBlock( NULL, InCommandList, InExtensionHook )
	, LabelOverride( InLabel )
	, ToolTipOverride( InToolTip )
	, IconOverride( InIcon )
	, EntryWidget( InEntryWidget )
	, bIsSubMenu( bInSubMenu )
	, bOpenSubMenuOnClick( bInSubMenuOnClick )
	, UserInterfaceActionType( EUserInterfaceActionType::Button )
	, bCloseSelfOnly( bInCloseSelfOnly )
	, Extender( InExtender )
{
}

FMenuEntryBlock::FMenuEntryBlock( const FName& InExtensionHook, const FUIAction& UIAction, const TSharedRef< SWidget > Contents, const EUserInterfaceActionType::Type InUserInterfaceActionType, bool bInCloseSelfOnly )
	: FMultiBlock( UIAction, InExtensionHook )
	, EntryWidget( Contents )
	, bIsSubMenu( false )
	, bOpenSubMenuOnClick( false )
	, UserInterfaceActionType( InUserInterfaceActionType )
	, bCloseSelfOnly( bInCloseSelfOnly )
{
}


FMenuEntryBlock::FMenuEntryBlock( const FName& InExtensionHook, const TSharedRef< SWidget > Contents, const FNewMenuDelegate& InEntryBuilder, TSharedPtr<FExtender> InExtender, bool bInSubMenu, bool bInSubMenuOnClick, TSharedPtr< const FUICommandList > InCommandList, bool bInCloseSelfOnly )
	: FMultiBlock( NULL, InCommandList, InExtensionHook )
	, EntryBuilder( InEntryBuilder )
	, EntryWidget( Contents )
	, bIsSubMenu( bInSubMenu )
	, bOpenSubMenuOnClick( bInSubMenuOnClick )
	, UserInterfaceActionType( EUserInterfaceActionType::Button )
	, bCloseSelfOnly( bInCloseSelfOnly )
	, Extender( InExtender )
{
}


FMenuEntryBlock::FMenuEntryBlock( const FName& InExtensionHook, const FUIAction& UIAction, const TSharedRef< SWidget > Contents, const FNewMenuDelegate& InEntryBuilder, TSharedPtr<FExtender> InExtender, bool bInSubMenu, TSharedPtr< const FUICommandList > InCommandList, bool bInCloseSelfOnly )
: FMultiBlock( UIAction, InExtensionHook )
	, EntryBuilder( InEntryBuilder )
	, EntryWidget( Contents )
	, bIsSubMenu( bInSubMenu )
	, bOpenSubMenuOnClick( false )
	, UserInterfaceActionType( EUserInterfaceActionType::Button )
	, bCloseSelfOnly( bInCloseSelfOnly )
	, Extender( InExtender )
{
}


void FMenuEntryBlock::CreateMenuEntry(FMenuBuilder& InMenuBuilder) const
{
	InMenuBuilder.AddSubMenu(LabelOverride.Get(), ToolTipOverride.Get(), EntryBuilder, false, IconOverride);	
}


/**
 * Allocates a widget for this type of MultiBlock.  Override this in derived classes.
 *
 * @return  MultiBlock widget object
 */
TSharedRef< class IMultiBlockBaseWidget > FMenuEntryBlock::ConstructWidget() const
{
	return SNew( SMenuEntryBlock );
}




/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SMenuEntryBlock::Construct( const FArguments& InArgs )
{
	// No initial sub-menus should be opened
	TimeToSubMenuOpen = 0.0f;
	SubMenuRequestState = Idle;

	// No images by default
	CheckedImage = NULL;
	UncheckedImage = NULL;

	this->SetForegroundColor( TAttribute<FSlateColor>::Create( TAttribute<FSlateColor>::FGetter::CreateRaw( this, &SMenuEntryBlock::InvertOnHover ) ) );
}


TSharedRef< SWidget>  SMenuEntryBlock::BuildMenuBarWidget( const FMenuEntryBuildParams& InBuildParams )
{
	const TAttribute<FText>& Label = InBuildParams.Label;
	const TAttribute<FText>& ToolTip = InBuildParams.ToolTip;

	check( OwnerMultiBoxWidget.IsValid() );

	const ISlateStyle* const StyleSet = InBuildParams.StyleSet;
	const FName& StyleName = InBuildParams.StyleName;

	/* Style for menu bar button with sub menu opened */
	MenuBarButtonBorderSubmenuOpen = StyleSet->GetBrush(StyleName, ".Button.SubMenuOpen");
	/* Style for menu bar button with no sub menu opened */
	MenuBarButtonBorderSubmenuClosed = FCoreStyle::Get().GetBrush("NoBorder");

	TSharedPtr< SMenuAnchor > NewMenuAnchor;


	// Create a menu bar button within a pop-up anchor
	TSharedRef<SWidget> Widget =
		SAssignNew( NewMenuAnchor, SMenuAnchor )
		.Placement( MenuPlacement_BelowAnchor )

		// When the menu is summoned, this callback will fire to generate content for the menu window
		.OnGetMenuContent( this, &SMenuEntryBlock::MakeNewMenuWidget )

		[
			SNew( SBorder )

			.BorderImage( this, &SMenuEntryBlock::GetMenuBarButtonBorder )

			.Padding(0)

			[
				// Create a button
				SNew( SButton )

				// Use the menu bar item style for this button
				.ButtonStyle( StyleSet, ISlateStyle::Join( StyleName, ".Button" ) )

				// Pull-down menu bar items always activate on mouse-down, not mouse-up
				.ClickMethod( EButtonClickMethod::MouseDown )

				// Pass along the block's tool-tip string
				.ToolTipText( this, &SMenuEntryBlock::GetFilteredToolTipText, ToolTip )

				// Add horizontal padding between the edge of the button and the content.  Also add a bit of vertical
				// padding to push the text down from the top of the menu bar a bit.
				.ContentPadding( FMargin( 10.0f, 2.0f ) )

				.ForegroundColor( FSlateColor::UseForeground() )

				.VAlign( VAlign_Center )

				[
					SNew( STextBlock )
					.TextStyle( StyleSet, ISlateStyle::Join( StyleName, ".Label" ) )
					.Text( Label )
				]

				// Bind the button's "on clicked" event to our object's method for this
				.OnClicked( this, &SMenuEntryBlock::OnMenuItemButtonClicked )
			]
		]
	;

	MenuAnchor = NewMenuAnchor;

	return Widget;
}


FText SMenuEntryBlock::GetFilteredToolTipText( TAttribute<FText> ToolTipText ) const
{
	// If we're part of a menu bar that has a currently open menu, then we suppress our own tool-tip
	// as it will just get in the way
	if( OwnerMultiBoxWidget.Pin()->GetOpenMenu().IsValid() )
	{
		return FText::GetEmpty();
	}

	return ToolTipText.Get();
}

EVisibility SMenuEntryBlock::GetVisibility() const
{
	TSharedPtr< const FUICommandList > ActionList = MultiBlock->GetActionList();
	TSharedPtr< const FUICommandInfo > Action = MultiBlock->GetAction();
	const FUIAction& DirectActions = MultiBlock->GetDirectActions();

	if (ActionList.IsValid() && Action.IsValid())
	{
		return ActionList->GetVisibility(Action.ToSharedRef());
	}

	// There is no action list or action associated with this block via a UI command.  Execute any direct action we have
	return DirectActions.IsVisible();
}

TSharedRef< SWidget > SMenuEntryBlock::BuildMenuEntryWidget( const FMenuEntryBuildParams& InBuildParams )
{
	const TAttribute<FText>& Label = InBuildParams.Label;
	const TAttribute<FText>& ToolTip = InBuildParams.ToolTip;
	const TSharedPtr< const FMenuEntryBlock > MenuEntryBlock = InBuildParams.MenuEntryBlock;
	const TSharedPtr< const FMultiBox > MultiBox = InBuildParams.MultiBox;
	const TSharedPtr< const FUICommandInfo >& UICommand = InBuildParams.UICommand;

	// See if the action is valid and if so we will use the actions icon if we dont override it later
	const FSlateIcon ActionIcon = UICommand.IsValid() ? UICommand->GetIcon() : FSlateIcon();

	// Allow the block to override the tool bar icon, too
	const FSlateIcon& ActualIcon = !MenuEntryBlock->IconOverride.IsSet() ? ActionIcon : MenuEntryBlock->IconOverride;

	check( OwnerMultiBoxWidget.IsValid() );

	const ISlateStyle* const StyleSet = InBuildParams.StyleSet;
	const FName& StyleName = InBuildParams.StyleName;

	// Grab the friendly text name for this action's input binding
	FText InputBindingText = UICommand.IsValid() ? UICommand->GetInputText() : FText::GetEmpty();

	// Allow menu item buttons to be triggered on mouse-up events if the menu is configured to be
	// dismissed automatically after clicking.  This preserves the behavior people expect for context
	// menus and pull-down menus
	const EButtonClickMethod::Type ButtonClickMethod =
		MultiBox->ShouldCloseWindowAfterMenuSelection() ?
		EButtonClickMethod::MouseUp : EButtonClickMethod::DownAndUp;

	// If we were supplied an image than go ahead and use that, otherwise we use a null widget
	TSharedRef< SWidget > IconWidget = SNullWidget::NullWidget;
	if( ActualIcon.IsSet() )
	{
		const FSlateBrush* IconBrush = ActualIcon.GetIcon();
		if( IconBrush->GetResourceName() != NAME_None )
		{
			IconWidget =
				SNew( SImage )
				.Image( IconBrush );
		}
	}

	// What type of UI should we create for this block?
	EUserInterfaceActionType::Type UserInterfaceType = MenuEntryBlock->UserInterfaceActionType;
	if ( UICommand.IsValid() )
	{
		// If we have a UICommand, then this is specified in the command.
		UserInterfaceType = UICommand->GetUserInterfaceType();
	}
	
	EVisibility CheckBoxVisibility =
		( UserInterfaceType == EUserInterfaceActionType::ToggleButton ||
			UserInterfaceType == EUserInterfaceActionType::RadioButton ||
			UserInterfaceType == EUserInterfaceActionType::Check ) ?
				EVisibility::Visible :
				EVisibility::Hidden;

	TAttribute<FSlateColor> CheckBoxForegroundColor = FSlateColor::UseForeground();
	FName CheckBoxStyle = ISlateStyle::Join( StyleName, ".CheckBox" );
	if (UserInterfaceType == EUserInterfaceActionType::Check)
	{
		CheckBoxStyle = ISlateStyle::Join( StyleName, ".Check" );
	}
	else if (UserInterfaceType == EUserInterfaceActionType::RadioButton)
	{
		CheckBoxStyle = ISlateStyle::Join( StyleName, ".RadioButton" );
		CheckBoxForegroundColor = TAttribute<FSlateColor>::Create( TAttribute<FSlateColor>::FGetter::CreateRaw( this, &SMenuEntryBlock::TintOnHover ) );
	}

	TSharedPtr< SWidget > ButtonContent = MenuEntryBlock->EntryWidget;
	if ( !ButtonContent.IsValid() )
	{
		// Create the content for our button
		ButtonContent = SNew( SHorizontalBox )
		// Whatever we have in the icon area goes first
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.Visibility(CheckBoxVisibility)
			.WidthOverride( MultiBoxConstants::MenuCheckBoxSize )
			.HeightOverride( MultiBoxConstants::MenuCheckBoxSize )
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew( SCheckBox )
				.ForegroundColor( CheckBoxForegroundColor )
				.IsChecked( this, &SMenuEntryBlock::IsChecked )
				.Style( StyleSet, CheckBoxStyle )
				.OnCheckStateChanged( this, &SMenuEntryBlock::OnCheckStateChanged )
				.ReadOnly( UserInterfaceType == EUserInterfaceActionType::Check )
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(2, 0, 2, 0))
		[
			SNew( SBox )
			.Visibility(IconWidget != SNullWidget::NullWidget ? EVisibility::Visible : EVisibility::Collapsed)
			.WidthOverride( MultiBoxConstants::MenuIconSize + 2 )
			.HeightOverride( MultiBoxConstants::MenuIconSize )
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew( SBox )
				.WidthOverride( MultiBoxConstants::MenuIconSize )
				.HeightOverride( MultiBoxConstants::MenuIconSize )
				[
					IconWidget
				]
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth( 1.0f )
		.Padding(FMargin(2, 0, 6, 0))
		.VAlign( VAlign_Center )
		[
			SNew( STextBlock )
			.TextStyle( StyleSet, ISlateStyle::Join( StyleName, ".Label" ) )
			.Text( Label )
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign( VAlign_Center )
		.HAlign( HAlign_Right )
		[
			SNew( SBox )
			.Visibility(InputBindingText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
			.Padding(FMargin(16,0,4,0))
			[
				SNew( STextBlock )
				.TextStyle( StyleSet, ISlateStyle::Join( StyleName, ".Keybinding" ) )
				.ColorAndOpacity( FSlateColor::UseSubduedForeground() )
				.Text( InputBindingText )
			]
		];
	}

	// Create a menu item button
	TSharedPtr<SWidget> MenuEntryWidget = 
		SNew( SButton )

		// Use the menu item style for this button
		.ButtonStyle( StyleSet, ISlateStyle::Join( StyleName, ".Button" ) )

		// Set our click method for this menu item.  It will be different for pull-down/context menus.
		.ClickMethod( ButtonClickMethod )

		// Pass along the block's tool-tip string
		.ToolTip( FMultiBoxSettings::ToolTipConstructor.Execute( ToolTip, NULL, UICommand ) )

		.ContentPadding(FMargin(0, 2))

		.ForegroundColor( FSlateColor::UseForeground() )
		[
			ButtonContent.ToSharedRef()
		]

		// Bind the button's "on clicked" event to our object's method for this
		.OnClicked( this, &SMenuEntryBlock::OnMenuItemButtonClicked );


	return MenuEntryWidget.ToSharedRef();
}




/**
 * A button for a sub-menu entry that shows its hovered state when the sub-menu is open
 */
class SSubMenuButton : public SButton
{
public:
	SLATE_BEGIN_ARGS( SSubMenuButton )
		: _ShouldAppearHovered( false )
		, _ButtonStyle( nullptr )
		{}
		/** The label to display on the button */
		SLATE_ATTRIBUTE( FText, Label )
		/** Called when the button is clicked */
		SLATE_EVENT( FOnClicked, OnClicked )
		/** Content to put in the button */
		SLATE_DEFAULT_SLOT( FArguments, Content )
		/** Whether or not the button should appear in the hovered state */
		SLATE_ATTRIBUTE( bool, ShouldAppearHovered )
		/** The style to use */
		SLATE_STYLE_ARGUMENT( FButtonStyle, ButtonStyle )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs )
	{
		ShouldAppearHovered = InArgs._ShouldAppearHovered;

		SButton::FArguments ButtonArgs;
		ButtonArgs.Text(InArgs._Label);
		ButtonArgs.ForegroundColor( this, &SSubMenuButton::InvertOnHover );
		ButtonArgs.HAlign(HAlign_Fill);
		ButtonArgs.VAlign(VAlign_Fill);
		ButtonArgs.ContentPadding(FMargin(0,2));

		if ( InArgs._ButtonStyle )
		{
			ButtonArgs.ButtonStyle( InArgs._ButtonStyle );
		}

		ButtonArgs.OnClicked(InArgs._OnClicked);
		ButtonArgs.ClickMethod(EButtonClickMethod::MouseDown)
		[
			InArgs._Content.Widget
		];

		SButton::Construct( ButtonArgs );

		// Replace SButtons delegate for getting the border image with our own
		// so we can control hovered state
		BorderImage.Bind( this, &SSubMenuButton::GetBorderImage );
	}

private:
	const FSlateBrush* GetBorderImage() const
	{
		// If the button should appear hovered always show the hovered image
		// Otherwise let SButton decide
		if( ShouldAppearHovered.Get() )
		{
			return HoverImage;
		}
		else
		{
			return SButton::GetBorder();
		}
	}

	FSlateColor InvertOnHover() const
	{
		if ( this->IsHovered() || ShouldAppearHovered.Get() )
		{
			return FLinearColor::Black;
		}
		else
		{
			return FSlateColor::UseForeground();
		}
	}

private:
	/** Attribute to indicate if the sub-menu is open or not */
	TAttribute<bool> ShouldAppearHovered;
};


TSharedRef< SWidget> SMenuEntryBlock::BuildSubMenuWidget( const FMenuEntryBuildParams& InBuildParams )
{
	const TAttribute<FText>& Label = InBuildParams.Label;
	const TAttribute<FText>& ToolTip = InBuildParams.ToolTip;

	const TSharedPtr< const FMenuEntryBlock > MenuEntryBlock = InBuildParams.MenuEntryBlock;
	const TSharedPtr< const FMultiBox > MultiBox = InBuildParams.MultiBox;
	const TSharedPtr< const FUICommandInfo >& UICommand = InBuildParams.UICommand;
	
	// See if the action is valid and if so we will use the actions icon if we dont override it later
	const FSlateIcon ActionIcon = UICommand.IsValid() ? UICommand->GetIcon() : FSlateIcon();
	
	// Allow the block to override the tool bar icon, too
	const FSlateIcon& ActualIcon = !MenuEntryBlock->IconOverride.IsSet() ? ActionIcon : MenuEntryBlock->IconOverride;

	check( OwnerMultiBoxWidget.IsValid() );

	const ISlateStyle* const StyleSet = InBuildParams.StyleSet;
	const FName& StyleName = InBuildParams.StyleName;

	// Allow menu item buttons to be triggered on mouse-up events if the menu is configured to be
	// dismissed automatically after clicking.  This preserves the behavior people expect for context
	// menus and pull-down menus
	const EButtonClickMethod::Type ButtonClickMethod =
		MultiBox->ShouldCloseWindowAfterMenuSelection() ?
		EButtonClickMethod::MouseUp : EButtonClickMethod::DownAndUp;

	// If we were supplied an image than go ahead and use that, otherwise we use a null widget
	TSharedRef< SWidget > IconWidget = SNullWidget::NullWidget;
	if( ActualIcon.IsSet() )
	{
		const FSlateBrush* IconBrush = ActualIcon.GetIcon();
		if( IconBrush->GetResourceName() != NAME_None )
		{
			IconWidget =
				SNew( SImage )
				.Image( IconBrush );
		}
	}

	// What type of UI should we create for this block?
	EUserInterfaceActionType::Type UserInterfaceType = MenuEntryBlock->UserInterfaceActionType;
	if ( UICommand.IsValid() )
	{
		// If we have a UICommand, then this is specified in the command.
		UserInterfaceType = UICommand->GetUserInterfaceType();
	}
	
	EVisibility CheckBoxVisibility =
		( UserInterfaceType == EUserInterfaceActionType::ToggleButton ||
			UserInterfaceType == EUserInterfaceActionType::RadioButton ||
			UserInterfaceType == EUserInterfaceActionType::Check ) ?
				EVisibility::Visible :
				EVisibility::Hidden;

	TAttribute<FSlateColor> CheckBoxForegroundColor = FSlateColor::UseForeground();
	FName CheckBoxStyle = ISlateStyle::Join( StyleName, ".CheckBox" );
	if (UserInterfaceType == EUserInterfaceActionType::Check)
	{
		CheckBoxStyle = ISlateStyle::Join( StyleName, ".Check" );
	}
	else if (UserInterfaceType == EUserInterfaceActionType::RadioButton)
	{
		CheckBoxStyle = ISlateStyle::Join( StyleName, ".RadioButton" );
		CheckBoxForegroundColor = TAttribute<FSlateColor>::Create( TAttribute<FSlateColor>::FGetter::CreateRaw( this, &SMenuEntryBlock::TintOnHover ) );
	}

	TSharedPtr< SWidget > ButtonContent = MenuEntryBlock->EntryWidget;
	if ( !ButtonContent.IsValid() )
	{
		// Create the content for our button
		ButtonContent = SNew( SHorizontalBox )
		// Whatever we have in the icon area goes first
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.Visibility(CheckBoxVisibility)
			.WidthOverride( MultiBoxConstants::MenuCheckBoxSize )
			.HeightOverride( MultiBoxConstants::MenuCheckBoxSize )
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew( SCheckBox )
				.ForegroundColor( CheckBoxForegroundColor )
				.IsChecked( this, &SMenuEntryBlock::IsChecked )
				.Style( StyleSet, CheckBoxStyle )
				.OnCheckStateChanged( this, &SMenuEntryBlock::OnCheckStateChanged )
				.ReadOnly( UserInterfaceType == EUserInterfaceActionType::Check )
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(2, 0, 2, 0))
		[
			SNew( SBox )
			.Visibility(IconWidget != SNullWidget::NullWidget ? EVisibility::Visible : EVisibility::Collapsed)
			.WidthOverride( MultiBoxConstants::MenuIconSize + 2 )
			.HeightOverride( MultiBoxConstants::MenuIconSize )
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew( SBox )
				.WidthOverride( MultiBoxConstants::MenuIconSize )
				.HeightOverride( MultiBoxConstants::MenuIconSize )
				[
					IconWidget
				]
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth( 1.0f )
		.Padding(FMargin(2, 0, 6, 0))
		.VAlign( VAlign_Center )
		[
			SNew( STextBlock )
			.TextStyle( StyleSet, ISlateStyle::Join( StyleName, ".Label" ) )
			.Text( Label )
		];
	}

	TSharedPtr< SMenuAnchor > NewMenuAnchorPtr;
	TSharedRef< SWidget > Widget = 
	SAssignNew( NewMenuAnchorPtr, SMenuAnchor )
		.Placement( MenuPlacement_MenuRight )
		// When the menu is summoned, this callback will fire to generate content for the menu window
		.OnGetMenuContent( this, &SMenuEntryBlock::MakeNewMenuWidget )
		[
			// Create a button
			SNew( SSubMenuButton )
			// Pass along the block's tool-tip string
			.ToolTipText( ToolTip )
			// Style to use
			.ButtonStyle( &StyleSet->GetWidgetStyle<FButtonStyle>( ISlateStyle::Join( StyleName, ".Button" ) ) )
			// Allow the button to change its state depending on the state of the submenu
			.ShouldAppearHovered( this, &SMenuEntryBlock::ShouldSubMenuAppearHovered )
			[
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				.FillWidth( 1.0f )
				[
					ButtonContent.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign( VAlign_Center )
				.HAlign( HAlign_Right )
				[
					SNew( SBox )
					.Padding(FMargin(7,0,0,0))
					[
						SNew( SImage )
						.Image( StyleSet->GetBrush( StyleName, ".SubMenuIndicator" ) )
					]
				]
			]

			// Bind the button's "on clicked" event to our object's method for this
			.OnClicked( this, &SMenuEntryBlock::OnMenuItemButtonClicked )
		];

	MenuAnchor = NewMenuAnchorPtr;

	return Widget;
}

/**
 * Builds this MultiBlock widget up from the MultiBlock associated with it
 */
void SMenuEntryBlock::BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName)
{
	FMenuEntryBuildParams BuildParams;
	TSharedPtr< const FMultiBox > MultiBox = OwnerMultiBoxWidget.Pin()->GetMultiBox();
	TSharedPtr< const FMenuEntryBlock > MenuEntryBlock = StaticCastSharedRef< const FMenuEntryBlock >( MultiBlock.ToSharedRef() );
	BuildParams.MultiBox = MultiBox;
	BuildParams.MenuEntryBlock = MenuEntryBlock;
	BuildParams.UICommand = BuildParams.MenuEntryBlock->GetAction();
	BuildParams.StyleSet = StyleSet;
	BuildParams.StyleName = StyleName;
	if (MenuEntryBlock->LabelOverride.IsSet())
	{
		BuildParams.Label = MenuEntryBlock->LabelOverride;
	}
	else
	{
		BuildParams.Label = BuildParams.UICommand.IsValid() ? BuildParams.UICommand->GetLabel() : FText::GetEmpty();
	}

	// Tool tips are optional so if the tool tip override is empty and there is no UI command just use the empty tool tip.
	if (MenuEntryBlock->ToolTipOverride.IsSet())
	{
		BuildParams.ToolTip = MenuEntryBlock->ToolTipOverride;
	}
	else
	{
		BuildParams.ToolTip = BuildParams.UICommand.IsValid() ? BuildParams.UICommand->GetDescription() : FText::GetEmpty();
	}	

	if( MultiBox->GetType() == EMultiBoxType::Menu )
	{
		if( MenuEntryBlock->bIsSubMenu )
		{
			// This menu entry is actually a submenu that opens a new menu to the right
			ChildSlot.Widget = BuildSubMenuWidget( BuildParams );
		}
		else
		{
			// Standard menu entry 
			ChildSlot.Widget = BuildMenuEntryWidget( BuildParams );
		}
	}
	else if( ensure( MultiBox->GetType() == EMultiBoxType::MenuBar ) )
	{
		// Menu bar items cannot be submenus
		check( !MenuEntryBlock->bIsSubMenu );
		
		ChildSlot.Widget = BuildMenuBarWidget( BuildParams );
	}

	// Insert named STutorialWrapper if desired
	FName TutorialName = MenuEntryBlock->GetTutorialHighlightName();
	if(TutorialName != NAME_None)
	{
		TSharedRef<SWidget> ChildWidget = ChildSlot.Widget;
		ChildSlot.Widget = 
			SNew( STutorialWrapper, TutorialName )
			[
				ChildWidget
			];
	}

	// Bind our widget's enabled state to whether or not our action can execute
	SetEnabled( TAttribute<bool>( this, &SMenuEntryBlock::IsEnabled ) );

	// Bind our widget's visible state to whether or not the action should be visible
	SetVisibility( TAttribute<EVisibility>(this, &SMenuEntryBlock::GetVisibility) );
}

void SMenuEntryBlock::RequestSubMenuToggle( bool bOpenMenu, const bool bClobber )
{
	// Reset the time before the menu opens
	TimeToSubMenuOpen = bClobber ? MultiBoxConstants::SubMenuClobberTime : MultiBoxConstants::SubMenuOpenTime;
	SubMenuRequestState = bOpenMenu ? WantOpen : WantClose;
	UpdateSubMenuState();
}

void SMenuEntryBlock::CancelPendingSubMenu()
{
	// Reset any pending sub-menu openings
	TimeToSubMenuOpen = 0.0f;
	SubMenuRequestState = Idle;
	UpdateSubMenuState();
}

bool SMenuEntryBlock::ShouldSubMenuAppearHovered() const
{
	// The sub-menu entry should appear hovered if the sub-menu is open.  Except in the case that the user is actually interacting with this menu.  
	// In that case we need to show what the user is selecting
	return MenuAnchor.IsValid() && MenuAnchor.Pin()->IsOpen() && !OwnerMultiBoxWidget.Pin()->IsHovered();
}

FReply SMenuEntryBlock::OnMenuItemButtonClicked()
{
	// The button itself was clicked
	const bool bCheckBoxClicked = false;
	OnClicked( bCheckBoxClicked );
	return FReply::Handled();
}

/**
 * Called by Slate when this menu entry's button is clicked
 */
void SMenuEntryBlock::OnClicked( bool bCheckBoxClicked )
{
	// Button was clicked, so trigger the action!
	TSharedRef< const FMenuEntryBlock > MenuEntryBlock = StaticCastSharedRef< const FMenuEntryBlock >( MultiBlock.ToSharedRef() );
	
	TSharedPtr< const FUICommandList > ActionList = MultiBlock->GetActionList();

	TSharedRef< const FMultiBox > MultiBox( OwnerMultiBoxWidget.Pin()->GetMultiBox() );

	// If this is a context menu, then we'll also dismiss the window after the user clicked on the item
	// NOTE: We dismiss the menu stack BEFORE executing the action to allow cases where the action actually starts a new menu stack
	// If we dismiss it before after the action, we would also dismiss the new menu
	const bool ClosingMenu = MultiBox->ShouldCloseWindowAfterMenuSelection() && ( !MenuEntryBlock->bIsSubMenu || ( MenuEntryBlock->bIsSubMenu && MenuEntryBlock->GetDirectActions().IsBound() ) );
	
	// Do not close the menu if we clicked a checkbox
	if( !bCheckBoxClicked )
	{
		if( ClosingMenu )
		{
			if( MenuEntryBlock->bCloseSelfOnly )
			{
				// Close only this menu and its children
				TSharedRef<SWindow> ParentContextMenuWindow = FSlateApplication::Get().FindWidgetWindow( AsShared() ).ToSharedRef();
				FSlateApplication::Get().DismissMenu( ParentContextMenuWindow );
			}
			else
			{
				// Dismiss the entire menu stack when a button is clicked to close all sub-menus
				FSlateApplication::Get().DismissAllMenus();
			}
		}
	}

	if( ActionList.IsValid() && MultiBlock->GetAction().IsValid() )
	{
		ActionList->ExecuteAction( MultiBlock->GetAction().ToSharedRef() );
	}
	else
	{
		// There is no action list or action associated with this block via a UI command.  Execute any direct action we have
		MenuEntryBlock->GetDirectActions().Execute();
	}

	// If we have a pull-down or sub-menu to summon, then go ahead and do that now
	if( !ClosingMenu && ( MenuEntryBlock->EntryBuilder.IsBound() || MenuEntryBlock->MenuBuilder.IsBound() || MenuEntryBlock->EntryWidget.IsValid() ) )
	{
		// Summon the menu!
		TSharedPtr< SMenuAnchor > PinnedMenuAnchor( MenuAnchor.Pin() );

		// Do not close the menu if its already open
		if( PinnedMenuAnchor.IsValid() && PinnedMenuAnchor->ShouldOpenDueToClick() )
		{
			FWidgetPath WidgetPath;
			FSlateApplication::Get().GeneratePathToWidgetChecked( PinnedMenuAnchor->AsShared(), WidgetPath );

			// Don't process clicks that attempt to open sub-menus when the parent is queued for destruction.
			if (!FSlateApplication::Get().IsWindowInDestroyQueue( WidgetPath.GetWindow() ))
			{
				// Close other open pull-down menus from this menu bar
				OwnerMultiBoxWidget.Pin()->CloseSummonedMenus();

				PinnedMenuAnchor->SetIsOpen( true );

				// Also tell the multibox about this open pull-down menu, so it can be closed later if we need to
				OwnerMultiBoxWidget.Pin()->SetSummonedMenu( PinnedMenuAnchor.ToSharedRef() );
			}
		}
	}

	// When a menu item is clicked we open the sub-menu instantly or close the entire menu in the case this is an actual menu item.
	CancelPendingSubMenu();
}


/**
 * Called by Slate to determine if this menu entry is enabled
 * 
 * @return True if the menu entry is enabled, false otherwise
 */
bool SMenuEntryBlock::IsEnabled() const
{
	TSharedPtr< const FUICommandList > ActionList = MultiBlock->GetActionList();
	TSharedPtr< const FUICommandInfo > Action = MultiBlock->GetAction();
	const FUIAction& DirectActions = MultiBlock->GetDirectActions();

	bool bEnabled = true;
	if( ActionList.IsValid() && Action.IsValid() )
	{
		bEnabled = ActionList->CanExecuteAction( Action.ToSharedRef() );
	}
	else
	{
		// There is no action list or action associated with this block via a UI command.  Execute any direct action we have
		bEnabled = DirectActions.CanExecute();
	}

	return bEnabled;
}


/**
 * Called by Slate when this menu entry check box button is toggled
 */
void SMenuEntryBlock::OnCheckStateChanged( const ESlateCheckBoxState::Type NewCheckedState )
{
	// The check box was clicked
	const bool bCheckBoxClicked = true;
	OnClicked( bCheckBoxClicked );
}

/**
 * Called by slate to determine if this menu entry should appear checked
 *
 * @return ESlateCheckBoxState::Checked if it should be checked, ESlateCheckBoxState::Unchecked if not.
 */
ESlateCheckBoxState::Type SMenuEntryBlock::IsChecked() const
{
	TSharedPtr< const FUICommandList > ActionList = MultiBlock->GetActionList();
	TSharedPtr< const FUICommandInfo > Action = MultiBlock->GetAction();
	const FUIAction& DirectActions = MultiBlock->GetDirectActions();

	bool bIsChecked = true;
	if( ActionList.IsValid() && Action.IsValid() )
	{
		bIsChecked = ActionList->IsChecked( Action.ToSharedRef() );
	}
	else
	{
		// There is no action list or action associated with this block via a UI command.  Execute any direct action we have
		bIsChecked = DirectActions.IsChecked();
	}

	return bIsChecked ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

const FSlateBrush* SMenuEntryBlock::OnGetCheckImage() const
{
	return IsChecked() == ESlateCheckBoxState::Checked ? CheckedImage : UncheckedImage;
}

/**
 * The system will use this event to notify a widget that the cursor has entered it. This event is NOT bubbled.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 */
void SMenuEntryBlock::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{

	SMultiBlockBaseWidget::OnMouseEnter( MyGeometry, MouseEvent );

	// Button was clicked, so trigger the action!
	TSharedRef< const FMenuEntryBlock > MenuEntryBlock = StaticCastSharedRef< const FMenuEntryBlock >( MultiBlock.ToSharedRef() );
	
	TSharedPtr< SMultiBoxWidget > PinnedOwnerMultiBoxWidget( OwnerMultiBoxWidget.Pin() );
	check( PinnedOwnerMultiBoxWidget.IsValid() );

	// Never dismiss another entry's submenu while the cursor is potentially moving toward that menu.  It's
	// not fun to try to keep the mouse in the menu entry bounds while moving towards the actual menu!
	const TSharedPtr< const SMenuAnchor > OpenedMenuAnchor( PinnedOwnerMultiBoxWidget->GetOpenMenu() );
	const bool bSubMenuAlreadyOpen = ( OpenedMenuAnchor.IsValid() && OpenedMenuAnchor->IsOpen() );
	bool bMouseEnteredTowardSubMenu = false;
	{
		if( bSubMenuAlreadyOpen )
		{
			const FVector2D& SubMenuPosition = OpenedMenuAnchor->GetMenuPosition();
			const bool bIsMenuTowardRight = MouseEvent.GetScreenSpacePosition().X < SubMenuPosition.X;
			const bool bDidMouseEnterTowardRight = MouseEvent.GetCursorDelta().X >= 0.0f;	// NOTE: Intentionally inclusive of zero here.
			bMouseEnteredTowardSubMenu = ( bIsMenuTowardRight == bDidMouseEnterTowardRight );
		}
	}

	// For menu bar entries, we also need to handle mouse enter/leave events, so we can show and hide
	// the pull-down menu appropriately
	if( MenuEntryBlock->EntryBuilder.IsBound() || MenuEntryBlock->MenuBuilder.IsBound() || MenuEntryBlock->EntryWidget.IsValid() )
	{
		// Do we have a different pull-down menu open?
		TSharedPtr< SMenuAnchor > PinnedMenuAnchor( MenuAnchor.Pin() );
		if( MenuEntryBlock->bIsSubMenu )
		{
			if ( MenuEntryBlock->bOpenSubMenuOnClick == false )
			{
				if( PinnedOwnerMultiBoxWidget->GetOpenMenu() != PinnedMenuAnchor )
				{
					const bool bClobber = bSubMenuAlreadyOpen && bMouseEnteredTowardSubMenu;
					RequestSubMenuToggle( true, bClobber );
				}
			}
		}
		else if( bSubMenuAlreadyOpen && OpenedMenuAnchor != PinnedMenuAnchor )
		{
			// Close other open pull-down menus from this menu bar
			PinnedOwnerMultiBoxWidget->CloseSummonedMenus();

			// Summon the new pull-down menu!
			if( PinnedMenuAnchor.IsValid() )
			{
				PinnedMenuAnchor->SetIsOpen( true );

				// Also tell the multibox about this open pull-down menu, so it can be closed later if we need to
				PinnedOwnerMultiBoxWidget->SetSummonedMenu( PinnedMenuAnchor.ToSharedRef() );
			}
		}
		
	}
	else if( bSubMenuAlreadyOpen )
	{
		// Hovering over a menu item that is not a sub-menu, we need to close any sub-menus that are open
		const bool bClobber = bSubMenuAlreadyOpen && bMouseEnteredTowardSubMenu;
		RequestSubMenuToggle( false, bClobber );
	}
	
}

void SMenuEntryBlock::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	SMultiBlockBaseWidget::OnMouseLeave( MouseEvent );

	// Reset any pending sub-menus that may be opening when we stop hovering over it
	CancelPendingSubMenu();
}

FReply SMenuEntryBlock::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& KeyboardEvent )
{
	SMultiBlockBaseWidget::OnKeyDown(MyGeometry, KeyboardEvent);

	// allow use of up and down keys to transfer focus
	if(KeyboardEvent.GetKey() == EKeys::Up || KeyboardEvent.GetKey() == EKeys::Down)
	{
		// find the next widget to focus
		EFocusMoveDirection::Type MoveDirection = ( KeyboardEvent.GetKey() == EKeys::Up )
			? EFocusMoveDirection::Previous
			: EFocusMoveDirection::Next;
		 
		return SMultiBoxWidget::FocusNextWidget(MoveDirection);
	}

	return FReply::Unhandled();
}

void SMenuEntryBlock::UpdateSubMenuState()
{
	// Check to see if there is a pending sub-menu request
	if( SubMenuRequestState != Idle )
	{
		// Reduce the time until the new menu opens
		TimeToSubMenuOpen -= FSlateApplication::Get().GetDeltaTime();
		if( TimeToSubMenuOpen <= 0.0f )
		{
			const bool bSubMenuNeedsToOpen = ( SubMenuRequestState == WantOpen );
			SubMenuRequestState = Idle;

			// The menu should be opened now as our timer is up
			TSharedRef< const FMenuEntryBlock > MenuEntryBlock = StaticCastSharedRef< const FMenuEntryBlock >( MultiBlock.ToSharedRef() );

			TSharedPtr< SMultiBoxWidget > PinnedOwnerMultiBoxWidget( OwnerMultiBoxWidget.Pin() );
			check( PinnedOwnerMultiBoxWidget.IsValid() );

			if( bSubMenuNeedsToOpen )
			{
				// For menu bar entries, we also need to handle mouse enter/leave events, so we can show and hide
				// the pull-down menu appropriately
				check( MenuEntryBlock->EntryBuilder.IsBound() || MenuEntryBlock->MenuBuilder.IsBound() || MenuEntryBlock->EntryWidget.IsValid() );

				// Close other open pull-down menus from this menu bar
				// Do we have a different pull-down menu open?
				TSharedPtr< SMenuAnchor > PinnedMenuAnchor( MenuAnchor.Pin() );
				if( PinnedOwnerMultiBoxWidget->GetOpenMenu() != PinnedMenuAnchor )
				{
					PinnedOwnerMultiBoxWidget->CloseSummonedMenus();

					// Summon the new pull-down menu!
					if( PinnedMenuAnchor.IsValid() )
					{
						PinnedMenuAnchor->SetIsOpen( true );
					}

					// Also tell the multibox about this open pull-down menu, so it can be closed later if we need to
					PinnedOwnerMultiBoxWidget->SetSummonedMenu( PinnedMenuAnchor.ToSharedRef() );
				}
			}
			else
			{
				PinnedOwnerMultiBoxWidget->CloseSummonedMenus();
			}
		}

	}
}


void SMenuEntryBlock::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	UpdateSubMenuState();
}

/**
 * Called to create content for a pull-down or sub-menu window when it's summoned by the user
 *
 * @return	The widget content for the new menu
 */
TSharedRef< SWidget > SMenuEntryBlock::MakeNewMenuWidget() const
{
	TSharedRef< const FMenuEntryBlock > MenuEntryBlock = StaticCastSharedRef< const FMenuEntryBlock >( MultiBlock.ToSharedRef() );

	check( OwnerMultiBoxWidget.IsValid() );

	TSharedPtr<SMultiBoxWidget> MultiBoxWidget = OwnerMultiBoxWidget.Pin();
	const ISlateStyle* const StyleSet = MultiBoxWidget->GetStyleSet();

	// Check each of the menu entry creation methods to see which one's been set, then use it to create the entry

	if (MenuEntryBlock->EntryBuilder.IsBound())
	{
		const bool bShouldCloseWindowAfterMenuSelection = true;
		const bool bCloseSelfOnly = false;
		FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, MultiBlock->GetActionList(), MenuEntryBlock->Extender, bCloseSelfOnly, StyleSet );
		{
			MenuEntryBlock->EntryBuilder.Execute( MenuBuilder );
		}

		return MenuBuilder.MakeWidget();
	}
	else if (MenuEntryBlock->MenuBuilder.IsBound())
	{
		return MenuEntryBlock->MenuBuilder.Execute();
	}
	else if (MenuEntryBlock->EntryWidget.IsValid())
	{
		const bool bShouldCloseWindowAfterMenuSelection = true;
		const bool bCloseSelfOnly = false;
		FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, NULL, TSharedPtr<FExtender>(), bCloseSelfOnly, StyleSet );
		{
			MenuBuilder.AddWidget( MenuEntryBlock->EntryWidget.ToSharedRef(), FText::GetEmpty() );
		}

		return MenuBuilder.MakeWidget();
	}
	else
	{
		// No entry creation method was initialized
		check(false);
		return SNullWidget::NullWidget;
	}
}

/**
 * Called to get the appropriate border for Buttons on Menu Bars based on whether or not submenu is open
 *
 * @return	The appropriate border to use
 */
const FSlateBrush* SMenuEntryBlock::GetMenuBarButtonBorder( ) const
{
	TSharedPtr<SMenuAnchor> MenuAnchorSharedPtr = MenuAnchor.Pin();

	if (MenuAnchorSharedPtr.IsValid() && MenuAnchorSharedPtr->IsOpen())
	{
		return MenuBarButtonBorderSubmenuOpen;
	}

	return MenuBarButtonBorderSubmenuClosed;
}

FSlateColor SMenuEntryBlock::TintOnHover() const
{
	if ( this->IsHovered() )
	{
		check( OwnerMultiBoxWidget.IsValid() );

		TSharedPtr<SMultiBoxWidget> MultiBoxWidget = OwnerMultiBoxWidget.Pin();
		const ISlateStyle* const StyleSet = MultiBoxWidget->GetStyleSet();

		return StyleSet->GetSlateColor("SelectionColor");
	}
	else
	{
		return FSlateColor::UseForeground();
	}
}

FSlateColor SMenuEntryBlock::InvertOnHover() const
{
	if ( this->IsHovered() )
	{
		return FLinearColor::Black;
	}
	else
	{
		return FSlateColor::UseForeground();
	}
}
