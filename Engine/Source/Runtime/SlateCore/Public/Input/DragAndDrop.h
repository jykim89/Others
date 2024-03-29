// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once


class SWindow;


/**
 * A base class for a DragAndDrop operation which supports reflection.
 * Drag and Drop is inherently quite stateful.
 * Implementing a custom DragDropOperation allows for dedicated handling of
 * a drag drop operation which keeps a decorator window (optionally)
 * Implement derived types with DRAG_DROP_OPERATOR_TYPE (see below)
 */
class SLATECORE_API FDragDropOperation
	: public TSharedFromThis<FDragDropOperation>
{
public:

	/**
	 * Default constructor.
	 */
	FDragDropOperation( ) { }

	/**
	 * Destructor.
	 */
	virtual ~FDragDropOperation( );

	/** Check if this drag and drop operation can cast safely to the specified template type */
	template<class TType> bool IsOfType() const 
	{
		return IsOfTypeImpl(TType::GetTypeId());
	}

	/**
	 * Invoked when the drag and drop operation has ended.
	 * 
	 * @param bDropWasHandled   true when the drop was handled by some widget; false otherwise
	 * @param MouseEvent        The mouse event which caused the on drop to be called.
	 */
	virtual void OnDrop( bool bDropWasHandled, const FPointerEvent& MouseEvent );

	/** 
	 * Called when the mouse was moved during a drag and drop operation
	 *
	 * @param DragDropEvent    The event that describes this drag drop operation.
	 */
	virtual void OnDragged( const class FDragDropEvent& DragDropEvent );
	
	/** Allows drag/drop operations to override the current cursor */
	virtual FCursorReply OnCursorQuery();

	/** Gets the widget that will serve as the decorator unless overridden. If you do not override, you will have no decorator */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const { return TSharedPtr<SWidget>(); }
	
	/** Alters the visibility of the window */
	void SetDecoratorVisibility(bool bVisible);

	/** Is this drag Drop operation going to interact with applications outside of Slate */
	virtual bool IsExternalOperation() const { return false; }

	/** 
	 * Sets the cursor to override the drag drop operations cursor with so that a 
	 * control can give temporary feedback, for example - a slashed circle telling 
	 * the user 'this can't be dropped here'.
	 */
	void SetCursorOverride( TOptional<EMouseCursor::Type> CursorType );

protected:

	/**
	 * Constructs the window and widget if applicable.
	 */
	virtual void Construct();

	/**
	 * Destroys the cursor decorator.
	 */
	void DestroyCursorDecoratorWindow( );

	/**
	 * Checks whether this drag and drop operation can cast safely to the specified type.
	 */
	virtual bool IsOfTypeImpl( const FString& Type ) const
	{
		return false;
	}

protected:

	/** The window that owns the decorator widget */
	TSharedPtr<SWindow> CursorDecoratorWindow;

	/** Mouse cursor used by the drag drop operation */
	TOptional<EMouseCursor::Type> MouseCursor;

	/** Mouse cursor used to temporarily replace the drag drops cursor */
	TOptional<EMouseCursor::Type> MouseCursorOverride;
};


/** Like a mouse event but with content */
class FDragDropEvent : public FPointerEvent
{
public:
	 /**
	  * Construct a DragDropEvent.
	  *
	  * @param InMouseEvent  The mouse event that is causing this drag and drop event to occur.
	  * @param InContent     The content being dragged.
	  */
	 FDragDropEvent( const FPointerEvent& InMouseEvent, const TSharedPtr<FDragDropOperation> InContent )
		: FPointerEvent( InMouseEvent )
		, Content( InContent )
	{
	}

	/** @return the content being dragged */
	TSharedPtr<FDragDropOperation> GetOperation() const
	{
		return Content;
	}

	/** @return the content being dragged if it matched the 'OperationType'; invalid Ptr otherwise. */
	template<typename OperationType>
	TSharedPtr<OperationType> GetOperationAs() const;

private:
	/** the content being dragged */
	TSharedPtr<FDragDropOperation> Content;
};


/**
 * Invoked when a drag and drop is finished.
 * This allows the widget that started the drag/drop to respond to the end of the operation.
 *
 * @param bWasDropHandled   True when the drag and drop operation was handled by some widget;
 *                          False when no widget handled the drop.
 * @param DragDropEvent     The Drop event that terminated the whole DragDrop operation
 */
DECLARE_DELEGATE_TwoParams( FOnDragDropEnded,
	bool /* bWasDropHandled */,
	const FDragDropEvent& /* DragDropEvent */
)

/**
 * A delegate invoked on the initiator of the DragDrop operation.
 * This delegate is invoked periodically during the DragDrop, and
 * gives the initiator an opportunity to respond to the current state of
 * the process. e.g. Create and update a custom cursor.
 */
DECLARE_DELEGATE_OneParam( FOnDragDropUpdate,
	const FDragDropEvent& /* DragDropEvent */
)



/**
 * All drag drop operations that require type checking must include this macro.
 * Example Usage:
 *	class FMyDragDropOp : public FDragDropOperation
 *	{
 *	public:
 *		DRAG_DROP_OPERATOR_TYPE(FMyDragDropOp, FDragDropOperation)
 *		...
 *	};
 */

#define DRAG_DROP_OPERATOR_TYPE(TYPE, BASE) \
	static const FString& GetTypeId() { static FString Type = TEXT(#TYPE); return Type; } \
	virtual bool IsOfTypeImpl(const FString& Type) const OVERRIDE { return GetTypeId() == Type || BASE::IsOfTypeImpl(Type); }


/**
 * An external drag and drop operation that originates outside of slate.
 * E.g. an OLE drag and drop.
 */
class SLATECORE_API FExternalDragOperation : public FDragDropOperation
{
private:
	/** A private constructor to ensure that the appropriate "New" factory method is used below */
	FExternalDragOperation(){}

	virtual bool IsExternalOperation() const OVERRIDE { return true; }

public:
	DRAG_DROP_OPERATOR_TYPE(FExternalDragOperation, FDragDropOperation)

	/** Creates a new external text drag operation */
	static TSharedRef<FExternalDragOperation> NewText( const FString& InText );
	/** Creates a new external file drag operation */
	static TSharedRef<FExternalDragOperation> NewFiles( const TArray<FString>& InFileNames );

	/** @return true if this is a text drag operation */
	bool HasText() const {return DragType == DragText;}
	/** @return true if this is a file drag operation */
	bool HasFiles() const {return DragType == DragFiles;}
	
	/** @return the text from this drag operation */
	const FString& GetText() const { ensure(HasText()); return DraggedText;}
	/** @return the filenames from this drag operation */
	const TArray<FString>& GetFiles() const { ensure(HasFiles()); return DraggedFileNames;}

private:
	FString DraggedText;
	TArray<FString> DraggedFileNames;

	enum ExternalDragType
	{
		DragText,
		DragFiles
	} DragType;
};


#include "DragAndDrop.inl"
