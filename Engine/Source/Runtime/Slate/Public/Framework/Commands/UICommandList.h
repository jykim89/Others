// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UICommandList.h: Declares the FUICommandList class.
=============================================================================*/

#pragma once


class SLATE_API FUICommandList
	: public TSharedFromThis<FUICommandList>
{
public:

	/** Determines if this UICommandList is capable of producing an action for the supplied command */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FCanProduceActionForCommand, const TSharedRef<const FUICommandInfo>& /*Command*/);

	/**
	 * Maps a command info to a series of delegates that are executed by a multibox or mouse/keyboard input
	 *
	 * @param InUICommandInfo	The command info to map
	 * @param ExecuteAction		The delegate to call when the command should be executed
	 */
	void MapAction( const TSharedPtr< const FUICommandInfo > InUICommandInfo, FExecuteAction ExecuteAction );

	/**
	 * Maps a command info to a series of delegates that are executed by a multibox or mouse/keyboard input
	 *
	 * @param InUICommandInfo	The command info to map
	 * @param ExecuteAction		The delegate to call when the command should be executed
	 * @param CanExecuteAction	The delegate to call to see if the command can be executed
	 */
	void MapAction( const TSharedPtr< const FUICommandInfo > InUICommandInfo, FExecuteAction ExecuteAction, FCanExecuteAction CanExecuteAction );

	/**
	 * Maps a command info to a series of delegates that are executed by a multibox or mouse/keyboard input
	 *
	 * @param InUICommandInfo	The command info to map
	 * @param ExecuteAction		The delegate to call when the command should be executed
	 * @param CanExecuteAction	The delegate to call to see if the command can be executed
	 * @param IsCheckedDelegate	The delegate to call to see if the command should appear checked when visualized in a multibox
	 */
	void MapAction( const TSharedPtr< const FUICommandInfo > InUICommandInfo, FExecuteAction ExecuteAction, FCanExecuteAction CanExecuteAction, FIsActionChecked IsCheckedDelegate );

	/**
	 * Maps a command info to a series of delegates that are executed by a multibox or mouse/keyboard input
	 *
	 * @param InUICommandInfo	The command info to map
	 * @param ExecuteAction		The delegate to call when the command should be executed
	 * @param CanExecuteAction	The delegate to call to see if the command can be executed
	 * @param IsCheckedDelegate	The delegate to call to see if the command should appear checked when visualized in a multibox
	 * @param IsVisibleDelegate	The delegate to call to see if the command should appear or be hidden when visualized in a multibox
	 */
	void MapAction( const TSharedPtr< const FUICommandInfo > InUICommandInfo, FExecuteAction ExecuteAction, FCanExecuteAction CanExecuteAction, FIsActionChecked IsCheckedDelegate, FIsActionButtonVisible IsVisibleDelegate );

	/**
	 * Maps a command info to a series of delegates that are executed by a multibox or mouse/keyboard input
	 *
	 * @param InUICommandInfo	The command info to map
	 * @param InUIAction		Action to map to this command
	 */
	void MapAction( const TSharedPtr< const FUICommandInfo > InUICommandInfo, const FUIAction& InUIAction );

	/**
	 * Append commands in InCommandsToAppend to this command list.
	 */
	void Append( const TSharedRef<FUICommandList>& InCommandsToAppend );

	/**
	 * Executes the action associated with the provided command info
	 * Note: It is assumed at this point that CanExecuteAction was already checked
	 *
	 * @param InUICommandInfo	The command info execute
	 */
	bool ExecuteAction( const TSharedRef< const FUICommandInfo > InUICommandInfo ) const;

	/**
	 * Calls the CanExecuteAction associated with the provided command info to see if ExecuteAction can be called
	 *
	 * @param InUICommandInfo	The command info execute
	 */
	bool CanExecuteAction( const TSharedRef< const FUICommandInfo > InUICommandInfo ) const;

	/**
	 * Attempts to execute the action associated with the provided command info
	 * Note: This will check if the action can be executed before finally executing the action
	 *
	 * @param InUICommandInfo	The command info execute
	 */
	bool TryExecuteAction( const TSharedRef< const FUICommandInfo > InUICommandInfo ) const;

	/**
	 * Calls the IsVisible delegate associated with the provided command info to see if the command should be visible in a toolbar
	 *
	 * @param InUICommandInfo	The command info execute
	 */
	EVisibility GetVisibility( const TSharedRef< const FUICommandInfo > InUICommandInfo ) const;

	/**
	 * Calls the IsChecked delegate to see if the visualization of this command in a multibox should appear checked
	 *
	 * @param InUICommandInfo	The command info execute
	 */
	bool IsChecked( const TSharedRef< const FUICommandInfo > InUICommandInfo ) const;

	/**
	 * Processes any UI commands which are activated by the specified keyboard event
	 *
	 * @param InKeyboardEvent	The keyboard event to check
	 *
	 * @return true if an action was processed
	 */
	bool ProcessCommandBindings( const FKeyboardEvent& InKeyboardEvent ) const;

	/**
	 * Processes any UI commands which are activated by the specified mouse event
	 *
	 * @param InKeyboardEvent	The mouse event to check
	 *
	 * @return true if an action was processed
	 */
	bool ProcessCommandBindings( const FPointerEvent& InMouseEvent ) const;

	/** Sets the delegate that determines if this UICommandList is capable of producing an action for the supplied command */
	void SetCanProduceActionForCommand( const FCanProduceActionForCommand& NewCanProduceActionForCommand ) { CanProduceActionForCommand = NewCanProduceActionForCommand; }

	/** 
	  * Attempts to find an action for the specified command in the current UICommandList. This is a wrapper for GetActionForCommandRecursively.
	  *
	  * @param Command				The UI command for which you are discovering an action
	  */
	const FUIAction* GetActionForCommand(TSharedPtr<const FUICommandInfo> Command) const;

protected:

	/**
	 * Helper function to execute delegate or exec command associated with a command (if valid)
	 *
	 * @param Key		The current key that is pressed
	 * @param bCtrl		True if control is pressed
	 * @param bAlt		True if alt is pressed
	 * @param bShift	True if shift is pressed
	 * @param bRepeat	True if command is repeating (held)
	 * @return True if a command was executed, False otherwise
	 */
	bool ConditionalProcessCommandBindings( const FKey Key, bool bCtrl, bool bAlt, bool bShift, bool bRepeat ) const;

	/** 
	  * Attempts to find an action for the specified command in the current UICommandList. If it is not found, the action for the
	  * specified command is discovered in the children recursively then the parents recursively.
	  *
	  * @param Command				The UI command for which you are discovering an action
	  * @param bIncludeChildren		If true, children of this command list will be searched in the event that the action is not found
	  * @param bIncludeParents		If true, parents of this command list will be searched in the event that the action is not found
	  * @param OutVisitedLists		The set of visited lists during recursion. This is used to prevent cycles.
	  */
	const FUIAction* GetActionForCommandRecursively(const TSharedRef<const FUICommandInfo>& Command, bool bIncludeChildren, bool bIncludeParents, TSet<TSharedRef<const FUICommandList>>& InOutVisitedLists) const;

	/** Returns all contexts associated with this list. This is a wrapper for GatherContextsForListRecursively */
	void GatherContextsForList(TSet<FName>& OutAllContexts) const;

	/** Returns all contexts associated with this list. */
	void GatherContextsForListRecursively(TSet<FName>& OutAllContexts, TSet<TSharedRef<const FUICommandList>>& InOutVisitedLists) const;

private:

	typedef TMap< const TSharedPtr< const FUICommandInfo >, FUIAction > FUIBindingMap;
	
	/** Known contexts in this list.  Each context must be known so we can quickly look up commands from bindings */
	TSet<FName> ContextsInList;

	/** Mapping of command to action */
	FUIBindingMap UICommandBindingMap;

	/** The list of parent and children UICommandLists */
	TArray<TWeakPtr<FUICommandList>> ParentUICommandLists;
	TArray<TWeakPtr<FUICommandList>> ChildUICommandLists;

	/** Determines if this UICommandList is capable of producing an action for the supplied command */
	FCanProduceActionForCommand CanProduceActionForCommand;
};
