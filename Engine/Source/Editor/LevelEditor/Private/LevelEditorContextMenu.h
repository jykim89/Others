// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#ifndef __LevelViewportContextMenu_h__
#define __LevelViewportContextMenu_h__

#pragma once

/** Enum to describe what a menu should be built for */
enum class LevelEditorMenuContext
{
	/** This context menu is applicable to a viewport */
	Viewport,
	/** This context menu is applicable to an external UI or dialog (disables click-position-based menu items) */
	NonViewport,
};

/**
 * Context menu construction class 
 */
class FLevelEditorContextMenu
{

public:

	/**
	 * Summons the level viewport context menu
	 * @param	LevelEditor		The level editor using this menu.
	 * @param	ContextType		The context we should use to specialize this menu
	 */
	static void SummonMenu( const TSharedRef< class SLevelEditor >& LevelEditor, LevelEditorMenuContext ContextType );

	/**
	 * Creates a widget for the context menu that can be inserted into a pop-up window
	 *
	 * @param	LevelEditor		The level editor using this menu.
	 * @param	ContextType		The context we should use to specialize this menu
	 * @param	Extender		Allows extension of this menu based on context.
	 * @return	Widget for this context menu
	 */
	static TSharedPtr< SWidget > BuildMenuWidget(TWeakPtr< SLevelEditor > LevelEditor, LevelEditorMenuContext ContextType, TSharedPtr<FExtender> Extender = TSharedPtr<FExtender>());

	/**
	 * Populates the specified menu builder for the context menu that can be inserted into a pop-up window
	 *
	 * @param	MenuBuilder		The menu builder to fill the menu with
	 * @param	LevelEditor		The level editor using this menu.
	 * @param	ContextType		The context we should use to specialize this menu
	 * @param	Extender		Allows extension of this menu based on context.
	 */
	static void FillMenu(FMenuBuilder& MenuBuilder, TWeakPtr< SLevelEditor > LevelEditor, LevelEditorMenuContext ContextType, TSharedPtr<FExtender> Extender);

private:

	/**
	 * Builds the actor group menu
	 *
	 * @param MenuBuilder		The menu builder to add items to.
	 * @param SelectedActorInfo	Information about the selected actors.
	 */
	static void BuildGroupMenu( FMenuBuilder& MenuBuilder, const struct FSelectedActorInfo& SelectedActorInfo );
};

#endif		// __LevelViewportContextMenu_h__