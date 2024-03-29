// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/*-----------------------------------------------------------------------------
	Basic structures
-----------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
	Declarations
-----------------------------------------------------------------------------*/

/** Widget used to present thread data in the mini-view. */
class SProfilerThreadView : public SCompoundWidget
{
	enum 
	{
		/** Minimum width of the one rendered sample, if less cycles counter will be combined. */
		MIN_NUM_PIXELS_PER_SAMPLE = 32,

		/**
		 *	Number of milliseconds that can be renderer at once in the window.
		 *	For the default zoom value.
		 */
		NUM_MILLISECONDS_PER_WINDOW = 33,

		/** Number of pixels needed to render one row of cycle counter. */
		NUM_PIXELS_PER_ROW = 16,

		/** Number of pixels. */
		MOUSE_SNAP_DISTANCE = 4,

		/** Wait time in milliseconds before we display a tooltip. */
		TOOLTIP_DELAY = 500,

		/** Width of the thread description windows. */
		WIDTH_THREAD_DESC = 128,

		/**
		 *	Displayed data will be partitioned into smaller batches to avoid long processing times.
		 *	This should help in situation when the user scroll the thread-view, so we don't need to wait for the whole data.
		 *	At the same it adds some overhead to the processing in favor of using massive parallel processing,
		 *	so overall it will be faster and much more responsive.
		 *	One partition must have a least one frame.
		 */
		NUM_DATA_PARTITIONS = 16,
		// @TODO yrx 2014-04-25 Dynamic data partitioning

		/**
		 *	Maximum zoom value for time axis.
		 *	Default value mean that one 33ms frame can be rendered at once in the window.
		 *	Maximum zoom allows to see individual cycles.
		 */
		INV_MIN_VISIBLE_RANGE_X = 10000,
		MAX_VISIBLE_RANGE_X = 250,

		/** Number of pixels between each time line. */
		NUM_PIXELS_BETWEEN_TIMELINE = 96,
	};

	struct EThreadViewCursor
	{
		enum Type
		{
			Default,
			Arrow,
			Hand,
		};
	};

	/** Holds current state provided by OnPaint function, used to simplify drawing. */
	struct FSlateOnPaintState : public FNoncopyable
	{
		FSlateOnPaintState( const FGeometry& InAllottedGeometry, const FSlateRect& InMyClippingRect, FSlateWindowElementList& InOutDrawElements, int32& InLayerId, const FWidgetStyle& InWidgetStyle, ESlateDrawEffect::Type InDrawEffects )
			: AllottedGeometry( InAllottedGeometry )
			, AbsoluteClippingRect( InMyClippingRect )
			, LocalClippingRect( FVector2D::ZeroVector, InAllottedGeometry.Size )
			, WidgetStyle( InWidgetStyle )
			, OutDrawElements( InOutDrawElements )
			, LayerId( InLayerId )
			, DrawEffects( InDrawEffects )
			, FontMeasureService( FSlateApplication::Get().GetRenderer()->GetFontMeasureService() )
			, SummaryFont8( FPaths::EngineContentDir() / TEXT( "Slate/Fonts/Roboto-Regular.ttf" ), 8 )
			, SummaryFont8Height( FontMeasureService->Measure( TEXT( "!" ), SummaryFont8 ).Y )
		{}

		const FVector2D& Size2D() const
		{
			return AllottedGeometry.Size;
		}

		/** Accessors. */
		const FGeometry& AllottedGeometry; 
		const FSlateRect& AbsoluteClippingRect;
		const FSlateRect LocalClippingRect;
		const FWidgetStyle& WidgetStyle;
		 
		FSlateWindowElementList& OutDrawElements;
		int32& LayerId;
		const ESlateDrawEffect::Type DrawEffects;

		const TSharedRef< FSlateFontMeasure > FontMeasureService;

		const FSlateFontInfo SummaryFont8;
		const float SummaryFont8Height;
	};

public:
	SProfilerThreadView();
	~SProfilerThreadView();

	SLATE_BEGIN_ARGS( SProfilerThreadView )
		{}
	SLATE_END_ARGS()

	/**
	* Construct this widget
	*
	* @param	InArgs	The declaration data for this widget
	*/
	void Construct( const FArguments& InArgs );

	/** Resets internal widget's data to the default one. */
	void Reset();

	/**
	* Ticks this widget.  Override in derived classes, but always call the parent implementation.
	*
	* @param  AllottedGeometry The space allotted for this widget
	* @param  InCurrentTime  Current absolute real time
	* @param  InDeltaTime  Real time passed since last tick
	*/
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) OVERRIDE;

	/**
	* The widget should respond by populating the OutDrawElements array with FDrawElements
	* that represent it and any of its children.
	*
	* @param AllottedGeometry  The FGeometry that describes an area in which the widget should appear.
	* @param MyClippingRect    The clipping rectangle allocated for this widget and its children.
	* @param OutDrawElements   A list of FDrawElements to populate with the output.
	* @param LayerId           The Layer onto which this widget should be rendered.
	* @param InColorAndOpacity ColorAverage and Opacity to be applied to all the descendants of the widget being painted
	* @param bParentEnabled	True if the parent of this widget is enabled.
	*
	* @return The maximum layer ID attained by this widget or any of its children.
	*/
	virtual int32 OnPaint( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const OVERRIDE;

	/**
	* The system calls this method to notify the widget that a mouse button was pressed within it. This event is bubbled.
	*
	* @param MyGeometry The Geometry of the widget receiving the event
	* @param MouseEvent Information about the input event
	*
	* @return Whether the event was handled along with possible requests for the system to take action.
	*/
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;

	/**
	* The system calls this method to notify the widget that a mouse button was release within it. This event is bubbled.
	*
	* @param MyGeometry The Geometry of the widget receiving the event
	* @param MouseEvent Information about the input event
	*
	* @return Whether the event was handled along with possible requests for the system to take action.
	*/
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;

	/**
	* The system calls this method to notify the widget that a mouse moved within it. This event is bubbled.
	*
	* @param MyGeometry The Geometry of the widget receiving the event
	* @param MouseEvent Information about the input event
	*
	* @return Whether the event was handled along with possible requests for the system to take action.
	*/
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;

	/**
	* The system will use this event to notify a widget that the cursor has entered it. This event is NOT bubbled.
	*
	* @param MyGeometry The Geometry of the widget receiving the event
	* @param MouseEvent Information about the input event
	*/
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;

	/**
	* The system will use this event to notify a widget that the cursor has left it. This event is NOT bubbled.
	*
	* @param MouseEvent Information about the input event
	*/
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) OVERRIDE;

	/**
	* Called when the mouse wheel is spun. This event is bubbled.
	*
	* @param  MouseEvent  Mouse event
	*
	* @return  Returns whether the event was handled, along with other possible actions
	*/
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;

	/**
	* Called when a mouse button is double clicked.  Override this in derived classes.
	*
	* @param  InMyGeometry  Widget geometry
	* @param  InMouseEvent  Mouse button event
	*
	* @return  Returns whether the event was handled, along with other possible actions
	*/
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;

	/**
	* Called when the system wants to know which cursor to display for this Widget.  This event is bubbled.
	*
	* @return  The cursor requested (can be None.)
	*/
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const OVERRIDE;

	void ShowContextMenu( const FVector2D& ScreenSpacePosition );

	/**	Binds UI commands to delegates. */
	void BindCommands();

	void DrawText( const FString& Text, const FSlateFontInfo& FontInfo, FVector2D Position, const FColor& TextColor, const FColor& ShadowColor, FVector2D ShadowOffset, const FSlateRect* ClippingRect = nullptr ) const;

	void DrawUIStackNodes_Recursively( const FProfilerUIStackNode& UIStackNode ) const;

	void DrawFramesBackgroundAndTimelines() const;
	void DrawUIStackNodes() const;
	void DrawFrameMarkers() const;

public:

	/**
	 *	Changes the position-x of the thread view.
	 *	Called by the horizontal scroll bar.
	 */
	void SetPositionXToByScrollBar( double ScrollOffset ) 
	{
		SetPositionX( ScrollOffset*TotalRangeXMS );
	}

	void SetPositionX( double NewPositionXMS )
	{
		const double ClampedPositionXMS = FMath::Clamp( NewPositionXMS, 0.0, TotalRangeXMS - RangeXMS );
		SetTimeRange( ClampedPositionXMS, ClampedPositionXMS + RangeXMS, true );
	}

	/**
	 *	Changes the position-y of the thread view.
	 *	Called by the external code.
	 */
	void SetPositonYTo( double ScrollOffset )
	{

	}

	/** Changes the position-x and range-x of the thread view. */
	void SetTimeRange( double StartTimeMS, double EndTimeMS, bool bBroadcast = true )
	{
		check( EndTimeMS > StartTimeMS );

		PositionXMS = StartTimeMS;
		RangeXMS = EndTimeMS - StartTimeMS;
		FramesIndices = ProfilerStream->GetFramesIndicesForTimeRange( StartTimeMS, EndTimeMS );

		bUpdateData = true;
		
		//UE_LOG( LogTemp, Log, TEXT( "StartTimeMS=%f, EndTimeMS=%f, bBroadcast=%1i FramesIndices=%3i,%3i" ), StartTimeMS, EndTimeMS, (int)bBroadcast, FramesIndices.X, FramesIndices.Y );
		if( bBroadcast )
		{
			ViewPositionXChangedEvent.Broadcast( StartTimeMS, EndTimeMS, TotalRangeXMS, FramesIndices.X, FramesIndices.Y );
		}
	}

	/**
	 *	Changes the position-x and range-x of the thread view. 
	 *	Called by the mini-view.
	 */
	void SetFrameRange( int32 FrameStart, int32 FrameEnd )
	{
		const double EndTimeMS = ProfilerStream->GetElapsedFrameTimeMS( FrameEnd );
		const double StartTimeMS = ProfilerStream->GetElapsedFrameTimeMS( FrameStart ) - ProfilerStream->GetFrameTimeMS( FrameStart );
		SetTimeRange( StartTimeMS, EndTimeMS, true );
	}

	/** Attaches profiler stream to the thread-view widgets and displays the first frame of data. */
	void AttachProfilerStream( const FProfilerStream& InProfilerStream )
	{
		ProfilerStream = &InProfilerStream;

		TotalRangeXMS = ProfilerStream->GetElapsedTime();
		TotalRangeY = ProfilerStream->GetNumThreads()*FProfilerUIStream::DEFAULT_VISIBLE_THREAD_DEPTH;

		// Display the first frame.
		const FProfilerFrame* ProfilerFrame = ProfilerStream->GetProfilerFrame( 0 );
		SetTimeRange( ProfilerFrame->Root->CycleCounterStartTimeMS, ProfilerFrame->Root->CycleCounterEndTimeMS );
	}

public:
	/** The event to execute when the position-x of the thread view has been changed. */
	DECLARE_EVENT_FiveParams( SProfilerThreadView, FViewPositionXChangedEvent, double /*StartTimeMS*/, double /*EndTimeMS*/, double /*MaxEndTimeMS*/, int32 /*FrameStart*/, int32 /*FrameEnd*/ );
	FViewPositionXChangedEvent& OnViewPositionXChanged()
	{
		return ViewPositionXChangedEvent;
	}
	
protected:
	/** The event to execute when the position-x of the thread view has been changed. */
	FViewPositionXChangedEvent ViewPositionXChangedEvent;

public:
	/** The event to execute when the position-y of the thread view has been changed. */
	DECLARE_EVENT_ThreeParams( SProfilerThreadView, FViewPositionYChangedEvent, double /*PosYStart*/, double /*PosYEnd*/, double /*MaxPosY*/ );
	FViewPositionYChangedEvent& OnViewPositionYChanged()
	{
		return ViewPositionYChangedEvent;
	}

protected:
	/** The event to execute when the position-y of the thread view has been changed. */
	FViewPositionYChangedEvent ViewPositionYChangedEvent;


protected:
	void UpdateInternalConstants()
	{
		ZoomFactorX = (double)NUM_MILLISECONDS_PER_WINDOW / RangeXMS;
		RangeY = FMath::RoundToFloat( ThisGeometry.Size.Y / (double)NUM_PIXELS_PER_ROW );
		
		const double Aspect = ThisGeometry.Size.X / NUM_MILLISECONDS_PER_WINDOW * ZoomFactorX;
		NumMillisecondsPerWindow = (double)ThisGeometry.Size.X / Aspect;
		NumPixelsPerMillisecond = (double)ThisGeometry.Size.X / NumMillisecondsPerWindow;
		NumMillisecondsPerSample = NumMillisecondsPerWindow / (double)ThisGeometry.Size.X * (double)MIN_NUM_PIXELS_PER_SAMPLE;	
	}
	
	void ProcessData();

	/**
	 * @return True, if the widget is ready to use, also means that contains at least one frame of the thread data.
	 */
	bool IsReady() const
	{
		return ProfilerStream && ProfilerStream->GetNumFrames() > 0;
	}

	bool ShouldUpdateData()
	{
		return bUpdateData;
	}

protected:

	/*-----------------------------------------------------------------------------
		Data variables
	-----------------------------------------------------------------------------*/

	/** Profiler UI stream, contains data optimized for displaying in this widget. */
	FProfilerUIStream ProfilerUIStream;

	/** Pointer to the profiler stream, used as a source for the UI stream. */
	const FProfilerStream* ProfilerStream;

	/*-----------------------------------------------------------------------------
		UI variables
	-----------------------------------------------------------------------------*/

	FGeometry ThisGeometry;

	/** Current Slate OnPaint state. */
	uint8 PaintStateMemory[sizeof(FSlateOnPaintState)];
	mutable FSlateOnPaintState*	PaintState;

	/** The current mouse position. */
	FVector2D MousePosition;

	/** The last mouse position. */
	FVector2D LastMousePosition;

	/** Mouse position during the call on mouse button down. */
	FVector2D MousePositionOnButtonDown;

	/** Position-X of the thread view, in milliseconds. */
	double PositionXMS;

	/** Position-Y of the thread view, where 1.0 means one row of the data. */
	double PositionY;

	/** Range of the visible data for the current zoom, in milliseconds. */
	double RangeXMS;

	/** Range of the visible data. */
	double RangeY;

	/** Range of the all collected data, in milliseconds. */
	double TotalRangeXMS;

	/** Range of the all collected data. */
	double TotalRangeY;

	/** Current zoom value for X. */
	double ZoomFactorX;

	/** Number of milliseconds that can be renderer at once in the window. */
	double NumMillisecondsPerWindow;

	/** Number of pixels needed to render one millisecond cycle counter. */
	double NumPixelsPerMillisecond;

	/** Number of milliseconds that can be displayed as one cycle counter. */
	double NumMillisecondsPerSample;

	/** Index of the frame currently being hovered by the mouse. */
	int32 HoveredFrameIndex;

	/** Thread ID currently being hovered by the mouse. */
	int32 HoveredThreadID;

	/** Position-Y of the thread view currently being hovered by the mouse, in milliseconds. */
	double HoveredPositionX;

	/** Position-Y of the thread view currently being hovered by the mouse. */
	double HoveredPositionY;

	/** Distance dragged. */
	double DistanceDragged;

	/** Frame indices of the currently visible data. X= FrameStart, Y=FrameEnd+1 */
	FIntPoint FramesIndices;

	bool bIsLeftMousePressed;
	bool bIsRightMousePressed;

	/** Whether to updated data. */
	bool bUpdateData;

	/** Cursor type. */
	EThreadViewCursor::Type CursorType;
};