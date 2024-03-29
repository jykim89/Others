// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once


class SWindow;


/** Matches widgets against InWidget */
struct FWidgetMatcher
{
	FWidgetMatcher( const TSharedRef<const SWidget> InWidget )
		: WidgetToFind( InWidget )
	{}

	bool IsMatch( const TSharedRef<const SWidget>& InWidget ) const
	{
		return WidgetToFind == InWidget;
	}

	TSharedRef<const SWidget> WidgetToFind;
};

/**
 * A widget path is a vertical slice through the tree.
 * The canonical form for widget paths is "leafmost last". The top-level window always resides at index 0.
 * A widget path also contains a reference to a top-level SWindow that contains all the widgets in the path.
 * The window is needed for its ability to determine its own geometry, from which the geometries of the rest
 * of the widget can be determined.
 */
class FWidgetPath
{
public:
	/** Constructor */
	FWidgetPath()
	: Widgets( EVisibility::Visible )
	{
	}

	FWidgetPath( TSharedPtr<SWindow> InTopLevelWindow, const FArrangedChildren& InWidgetPath )
	: Widgets( InWidgetPath )
	, TopLevelWindow(InTopLevelWindow)
	{
	
	}

	/**
	 * @param MarkerWidget Copy the path up to and including this widget 
	 *
	 * @return a copy of the widget path down to and including the MarkerWidget. If the MarkerWidget is not found in the path, return an invalid path.
	 */
	FWidgetPath GetPathDownTo( TSharedRef<const SWidget> MarkerWidget ) const
	{
		FArrangedChildren ClippedPath(EVisibility::Visible);
		bool bCopiedMarker = false;
		for( int32 WidgetIndex = 0; !bCopiedMarker && WidgetIndex < Widgets.Num(); ++WidgetIndex )
		{
			ClippedPath.AddWidget( Widgets(WidgetIndex) );
			bCopiedMarker = (Widgets(WidgetIndex).Widget == MarkerWidget);
		}
		
		if ( bCopiedMarker )
		{
			// We found the MarkerWidget and copied the path down to (and including) it.
			return FWidgetPath( TopLevelWindow, ClippedPath );
		}
		else
		{
			// The MarkerWidget was not in the widget path. We failed.
			return FWidgetPath( nullptr, FArrangedChildren(EVisibility::Visible) );		
		}	
	}
	
	/** @return true if the WidgetToFind is in this WidgetPath, false otherwise. */
	bool ContainsWidget( TSharedRef<const SWidget> WidgetToFind ) const
	{
		for(int32 WidgetIndex = 0; WidgetIndex < Widgets.Num(); ++WidgetIndex)
		{
			if ( Widgets(WidgetIndex).Widget == WidgetToFind )
			{
				return true;
			}
		}
		
		return false;
	}

	SLATECORE_API FArrangedWidget FindArrangedWidget( TSharedRef<const SWidget> WidgetToFind ) const;
	
	/**
	 * Get the first (top-most) widget in this path, which is always a window; assumes path is valid
	 *
	 * @return Window at the top of this path
	 */
	TSharedRef<SWindow> GetWindow()
	{
		check(IsValid());

		TSharedRef<SWindow> FirstWidgetWindow = StaticCastSharedRef<SWindow>(Widgets(0).Widget);
		return FirstWidgetWindow;
	}

	/**
	 * Get the first (top-most) widget in this path, which is always a window; assumes path is valid
	 *
	 * @return Window at the top of this path
	 */
	TSharedRef<SWindow> GetWindow() const
	{
		check(IsValid());

		TSharedRef<SWindow> FirstWidgetWindow = StaticCastSharedRef<SWindow>(Widgets(0).Widget);
		return FirstWidgetWindow;
	}
	
	/** A valid path has at least one widget in it */
	bool IsValid() const { return Widgets.Num() > 0; }
	
	FString ToString() const
	{
		FString StringBuffer;
		for( int32 WidgetIndex = Widgets.Num()-1; WidgetIndex >= 0; --WidgetIndex )
		{
			StringBuffer += Widgets(WidgetIndex).ToString();
			StringBuffer += TEXT("\n");
		}
		return StringBuffer;
	}

	/**
	 * Extend the current path such that it reaches some widget that qualifies as a Match
	 * The widget to match must be a descendant of the last widget currently in the path.
	 *
	 * @param Matcher         Some struct that has a "bool IsMatch( const TSharedRef<const SWidget>& InWidget ) const" method
	 * @param  VisibilityFilter	Widgets must have this type of visibility to be included the path
	 *
	 * @return true if successful; false otherwise.
	 */
	template<typename MatcherType>
	bool ExtendPathTo( const MatcherType& Matcher, EVisibility VisibilityFilter = EVisibility::Visible )
	{
		const FArrangedWidget& LastWidget = Widgets.Last();
		
		FArrangedChildren Extension = GeneratePathToWidget( Matcher, LastWidget, EFocusMoveDirection::Next, VisibilityFilter );

		for( int32 WidgetIndex=0; WidgetIndex < Extension.Num(); ++WidgetIndex )
		{
			this->Widgets.AddWidget( Extension(WidgetIndex) );
		}

		return Extension.Num() > 0;
	}

	/**
	 * Generate a path from FromWidget to WidgetToFind. The path will not include FromWidget.
	 *
	 * @param Matcher         Some struct that has a "bool IsMatch( const TSharedRef<const SWidget>& InWidget ) const" method
	 * @param FromWidget      Widget from which we a building a path.AddItem*
	 * @param  VisibilityFilter	Widgets must have this type of visibility to be included the path
	 * 
	 * @return A path from FromWidget to WidgetToFind; will not include FromWidget.
	 */
	template<typename MatcherType>
	FArrangedChildren GeneratePathToWidget( const MatcherType& Matcher, const FArrangedWidget& FromWidget, EFocusMoveDirection::Type Direction = EFocusMoveDirection::Next, EVisibility VisibilityFilter = EVisibility::Visible )
	{
		FArrangedChildren PathResult(VisibilityFilter);

		if ( Direction == EFocusMoveDirection::Next )
		{
			SearchForWidgetRecursively( Matcher, FromWidget, PathResult, VisibilityFilter );
		}
		else
		{
			SearchForWidgetRecursively_Reverse( Matcher, FromWidget, PathResult, VisibilityFilter );
		}
		

		// Reverse the list of widgets we found; canonical form is leafmost last.
		PathResult.Reverse();

		return PathResult;
	}
	
	/**
	 * Move focus either forward on backward in the path level specified by PathLevel.
	 * That is, this movement of focus will modify the subtree under Widgets(PathLevel).
	 *
	 * @param PathLevel       The level in this WidgetPath whose focus to move.
	 * @param MoveDirectin    Move focus forward or backward?
	 *
	 * @return true if the focus moved successfully, false if we were unable to move focus
	 */
	bool MoveFocus(int32 PathLevel, EFocusMoveDirection::Type MoveDirection);


	FArrangedChildren Widgets;
	TSharedPtr< SWindow > TopLevelWindow;

private:

	/**
	 * Utility function to search recursively through a widget hierarchy for a specific widget
	 *
	 * @param Matcher         Some struct that has a "bool IsMatch( const TSharedRef<const SWidget>& InWidget ) const" method
	 * @param  InCandidate      The current widget-geometry pair we're testing
	 * @param  OutReversedPath  The resulting path in reversed order (canonical order is Windows @ index 0, Leafmost widget is last.)
	 * @param  VisibilityFilter	Widgets must have this type of visibility to be included the path
	 *
	 * @return  true if the child widget was found; false otherwise
	 */
	template<typename MatchRuleType>
	static bool SearchForWidgetRecursively( const MatchRuleType& MatchRule, const FArrangedWidget& InCandidate, FArrangedChildren& OutReversedPath, EVisibility VisibilityFilter = EVisibility::Visible );

	/** Identical to SearchForWidgetRecursively, but iterates in reverse order */
	template<typename MatchRuleType>
	static bool SearchForWidgetRecursively_Reverse( const MatchRuleType& MatchRule, const FArrangedWidget& InCandidate, FArrangedChildren& OutReversedPath, EVisibility VisibilityFilter = EVisibility::Visible );

};


/**
 * Just like a WidgetPath, but uses weak pointers and does not store geometry.
 */
class SLATECORE_API FWeakWidgetPath
{
public:
	/** Construct a weak widget path from a widget path. Defaults to an invalid path. */
	FWeakWidgetPath( const FWidgetPath& InWidgetPath = FWidgetPath( TSharedPtr<SWindow>(nullptr), FArrangedChildren(EVisibility::Visible) ) );

	/** Should interrupted paths truncate or return an invalid path? */
	struct EInterruptedPathHandling
	{
		enum Type
		{
			Truncate,
			ReturnInvalid
		};
	};

	/**
	 * Make a non-weak WidgetPath out of this WeakWidgetPath. Do this by computing all the relevant geometries and converting the weak pointers to TSharedPtr.
	 *
	 * @param InterruptedPathHandling  Should interrupted paths result in a truncated path or an invalid path
	 */
	FWidgetPath ToWidgetPath( EInterruptedPathHandling::Type InterruptedPathHandling = EInterruptedPathHandling::Truncate ) const;

	struct EPathResolutionResult
	{
		enum Result
		{
			Live,
			Truncated
		};
	};
	/**
	 * Make a non-weak WidgetPath out of this WeakWidgetPath. Do this by computing all the relevant geometries and converting the weak pointers to TSharedPtr.
	 *
	 * @param WidgetPath				The non-weak path is returned via this.
	 * @param InterruptedPathHandling	Should interrupted paths result in a truncated path or an invalid path.
	 * @return Whether the path is truncated or live - a live path refers to a widget that is currently active and visible, a widget with a truncated path is not.
	 */
	EPathResolutionResult::Result ToWidgetPath( FWidgetPath& WidgetPath, EInterruptedPathHandling::Type InterruptedPathHandling = EInterruptedPathHandling::Truncate ) const;

	bool ContainsWidget( const TSharedRef< const SWidget >& SomeWidget ) const;

	/**
	 * @param MoveDirection      Direction in which to move the focus.
	 * 
	 * @return The new focus path.
	 */
	FWidgetPath ToNextFocusedPath(EFocusMoveDirection::Type MoveDirection);
	
	/** Get the last (leaf-most) widget in this path; assumes path is valid */
	TWeakPtr< SWidget > GetLastWidget() const
	{
		check( IsValid() );
		return Widgets[Widgets.Num()-1];
	}
	
	/** A valid path has at least one widget in it */
	bool IsValid() const { return Widgets.Num() > 0; }

	TArray< TWeakPtr<SWidget> > Widgets;
	TWeakPtr< SWindow > Window;
};
