// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "ProfilerPrivatePCH.h"

#define LOCTEXT_NAMESPACE "SDataGraph"

/**
 * @return hash value for the specified linear color
 */
FORCEINLINE uint32 GetTypeHash( const FLinearColor& LinearColor )
{
	return LinearColor.ToFColor(true).DWColor();
}

/*-----------------------------------------------------------------------------
	STrackedStatSummary/SDataGraphSummary
-----------------------------------------------------------------------------*/

/** Widget used to represent summary of the specified tracked stat. */
class SDataGraphSummary : public SCompoundWidget
{
public:
	/** Default constructor. */
	SDataGraphSummary()
	{}

	/** Virtual destructor. */
	virtual ~SDataGraphSummary()
	{}

	SLATE_BEGIN_ARGS( SDataGraphSummary )
		: _ParentWidget()
		, _GraphDescription()
		{}

		SLATE_ARGUMENT( TSharedPtr<SDataGraph>, ParentWidget )
		SLATE_ARGUMENT( FGraphDescription, GraphDescription )
		SLATE_EVENT( FGetHoveredFrameIndexDelegate, OnGetMouseFrameIndex )
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& InArgs )
	{
		ParentWidget = InArgs._ParentWidget;
		GraphDescription = InArgs._GraphDescription;
		OnGetMouseFrameIndex = InArgs._OnGetMouseFrameIndex;

		const FSlateColor TextColor( GraphDescription.ColorAverage );
		const FName CloseButtonStyle = TEXT("Docking.MajorTab.CloseButton");
		FFormatNamedArguments Args;
		Args.Add( TEXT("StatName"), FText::FromString( GraphDescription.CombinedGraphDataSource->GetStatName() ) );
		const FText ToolTipText = FText::Format( LOCTEXT("DataGraphSummary_CloseButton_TT", "Click to stop tracking '{StatName}' stat"), Args );

		ChildSlot
		[
			SNew(SHorizontalBox)

			// Close button.
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding( 1.0f )
			[
				SNew(SButton)
				.ButtonStyle( FEditorStyle::Get(), CloseButtonStyle )
				.OnClicked( this, &SDataGraphSummary::CloseButton_OnClicked )
				.ContentPadding( 0 )
				.ToolTipText( ToolTipText )
				[
					SNew(SSpacer)
					.Size( FEditorStyle::GetBrush(CloseButtonStyle, ".Normal" )->ImageSize )
				]
			]

			// Stat group name.
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding( 1.0f, 0.0, 1.0f, 0.0f )
			[
				SNew(STextBlock)
				.ColorAndOpacity( TextColor )
				.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
				.Text( this, &SDataGraphSummary::SummaryInformation_GetGroupName )
			]

			// Stat name.
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding( 1.0f, 0.0, 1.0f, 0.0f )
			[
				SNew(STextBlock)
				.ColorAndOpacity( TextColor )
				.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
				.Text( this, &SDataGraphSummary::SummaryInformation_GetStatName )
			]

			// Summary information. 
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding( 1.0f, 0.0, 1.0f, 0.0f )
			[
				SNew(STextBlock)
				.ColorAndOpacity( TextColor )
				.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
				.Text( this, &SDataGraphSummary::SummaryInformation_GetSummary )
			]
		];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

protected:
	/** Stops tracking the associated stat and removes it from the data graph. */
	FReply CloseButton_OnClicked()
	{
		FProfilerManager::Get()->UntrackStat( GraphDescription.CombinedGraphDataSource->GetStatID() );	
		return FReply::Handled();
	}

	FString SummaryInformation_GetSummary() const
	{
		FString SummaryText = LOCTEXT("DataGraphSummary_Warning", "Not implemented yet").ToString();
		const bool bCanDisplayData = GraphDescription.CombinedGraphDataSource->CanBeDisplayedAsIndexBased() && ParentWidget->GetViewMode() == EDataGraphViewModes::Index;

		const uint32 FrameIndex = OnGetMouseFrameIndex.IsBound() ? (uint32)OnGetMouseFrameIndex.Execute() : 0;
		const FGraphDataSourceRefConst* GraphDataSource = GraphDescription.CombinedGraphDataSource->GetFirstSource();

		if( bCanDisplayData && GraphDataSource && FrameIndex < (*GraphDataSource)->GetNumFrames() )
		{
			const double SampleValue = (*GraphDataSource)->GetValueFromIndex( FrameIndex );
			const EProfilerSampleTypes::Type UnitType = (*GraphDataSource)->GetSampleType();
			const FProfilerAggregatedStat& Aggregated = *(*GraphDataSource)->GetAggregatedStat();

			SummaryText = FString::Printf( TEXT("%4.2f - "), SampleValue );
			SummaryText+= Aggregated.ToString();
		}

		return SummaryText;
	}

	FString SummaryInformation_GetGroupName() const
	{
		return FString::Printf( TEXT("(%s)"), *GraphDescription.CombinedGraphDataSource->GetGroupName() );
	}

	FString SummaryInformation_GetStatName() const
	{
		return FProfilerHelper::ShortenName( GraphDescription.CombinedGraphDataSource->GetStatName(), 32 );
	}

private:
	FGraphDescription GraphDescription;

	/** A shared pointer to the parent widget. */
	TSharedPtr<SDataGraph> ParentWidget;

	/** The delegate to be invoked when the data graph summary widget wants to know the frame index pointed by the mouse. */
	FGetHoveredFrameIndexDelegate OnGetMouseFrameIndex;
};

/*-----------------------------------------------------------------------------
	SDataGraph
-----------------------------------------------------------------------------*/

const float GraphMarkerWidth = 4.0f;
const float HalfGraphMarkerWidth = GraphMarkerWidth * 0.5f;

SDataGraph::SDataGraph()
	: MousePosition( FVector2D( 0.0f, 0.0f ) )
	, MouseWheelAcc( 6.0f )
	, bIsRMB_Scrolling( false )
	, bIsLMB_SelectionDragging( false )
	, bIsLMB_Pressed( false )
	, bIsRMB_Pressed( false )

	, ViewMode( EDataGraphViewModes::Index )
	, MultiMode( EDataGraphMultiModes::OneLinePerDataSource )
	, TimeBasedAccuracy( FTimeAccuracy::FPS060 )
	, DistanceBetweenPoints( 4 )

	, NumDataPoints( 0 )
	, NumVisiblePoints( 0 )
	, GraphOffset( 0 )
	, RealGraphOffset( 0.0f )
	, HoveredFrameIndex( 0 )

	, DataTotalTimeMS( 0.0f )
	, VisibleTimeMS( 0.0f )
	, GraphOffsetMS( 0.0f )
	, HoveredFrameStartTimeMS( 0.0f )
{
	ScaleY = FMath::Pow( 2.0f, MouseWheelAcc );

	FMemory::MemZero( FrameIndices );
	FMemory::MemZero( FrameTimesMS );
}

SDataGraph::~SDataGraph()
{
}

void SDataGraph::Construct( const FArguments& InArgs )
{
	OnGraphOffsetChanged = InArgs._OnGraphOffsetChanged;
	OnViewModeChanged = InArgs._OnViewModeChanged;

	ChildSlot
	[
		SNew(SOverlay)
		.Visibility( EVisibility::SelfHitTestInvisible )

		+SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding( FMargin( 48.0f, 16.0f, 48.0f, 16.0f ) )	// Make some space for graph labels
		[
			SAssignNew(GraphDescriptionsVBox,SVerticalBox)
		]
	];

	BindCommands();
}

void SDataGraph::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SCompoundWidget::Tick(AllottedGeometry,InCurrentTime,InDeltaTime);
	ThisGeometry = AllottedGeometry;

	UpdateState();
}

void SDataGraph::UpdateState()
{
	const FGraphDescription* GraphDesc = GetFirstGraph();
	if( GraphDesc )
	{
		// Check if we need to force time based view mode.
		const bool bCanBeDisplayedAsMulti = GraphDesc->CombinedGraphDataSource->CanBeDisplayedAsMulti();
		if( bCanBeDisplayedAsMulti )
		{
			ViewMode = EDataGraphViewModes::Time;
		}

		// If the view mode is index based, use the first source for reading the number of frames.
		if( ViewMode == EDataGraphViewModes::Index )
		{
			const FGraphDataSourceRefConst* GraphDataSource = GraphDesc->CombinedGraphDataSource->GetFirstSource();
			NumDataPoints = GraphDataSource ? (int32)(*GraphDataSource)->GetNumFrames() : 0;
		}
		else
		{
			NumDataPoints = (int32)GraphDesc->CombinedGraphDataSource->GetNumFrames();
		}

		NumVisiblePoints = FMath::Max( 0, FMath::TruncToInt(ThisGeometry.Size.X) / DistanceBetweenPoints );
		// GraphOffset - Updated by OnMouseMove or by ScrollTo
		GraphOffset = FMath::Clamp( GraphOffset, 0, FMath::Max(NumDataPoints-NumVisiblePoints,0) );
		
		DataTotalTimeMS = GraphDesc->CombinedGraphDataSource->GetTotalTimeMS();
		VisibleTimeMS = NumVisiblePoints * FTimeAccuracy::AsFrameTime( TimeBasedAccuracy );
		GraphOffsetMS = GraphOffset * FTimeAccuracy::AsFrameTime( TimeBasedAccuracy );
	}
	else
	{
		NumDataPoints = 0;
		NumVisiblePoints = 0;
		GraphOffset = 0;

		DataTotalTimeMS = 0.0f;
		VisibleTimeMS = 0.0f;
		GraphOffsetMS = 0.0f;
	}
}

int32 SDataGraph::OnPaint
( 
	const FGeometry& AllottedGeometry, 
	const FSlateRect& MyClippingRect, 
	FSlateWindowElementList& OutDrawElements, 
	int32 LayerId, 
	const FWidgetStyle& InWidgetStyle, 
	bool bParentEnabled 
) const
{
	static double TotalTime = 0.0f;
	static uint32 NumCalls = 0;
	const double StartTime = FPlatformTime::Seconds();

	const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	// Rendering info.
	const bool bEnabled  = ShouldBeEnabled( bParentEnabled );
	ESlateDrawEffect::Type DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FSlateBrush* TimelineAreaBrush = FEditorStyle::GetBrush("Profiler.LineGraphArea");
	const FSlateBrush* WhiteBrush = FEditorStyle::GetBrush("WhiteTexture");

	/** Width of the alloted geometry that is used to draw a data graph. */
	const float AreaX0 = 0.0f;
	const float AreaX1 = AllottedGeometry.Size.X;

	// Draw background.
	FSlateDrawElement::MakeBox
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry( FVector2D(0,0), FVector2D(AreaX1,AllottedGeometry.Size.Y) ),
		TimelineAreaBrush,
		MyClippingRect,
		DrawEffects,
		TimelineAreaBrush->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint()
	);
	LayerId++;
		
	const float CounterToTimeScale = 1.0f / 8.0f;

	TArray<FVector2D> GraphPoints;
	GraphPoints.Empty( NumVisiblePoints );

	TArray<FVector2D> GraphPoints2;
	GraphPoints2.Empty( NumVisiblePoints );

	TArray<FVector2D> GraphPoints3;
	GraphPoints3.Empty( NumVisiblePoints );

	// Draw all graphs.
	for( auto It = StatIDToGraphDescriptionMapping.CreateConstIterator(); It; ++It )
	{
		SCOPE_CYCLE_COUNTER(STAT_DG_OnPaint);

		const FGraphDescription& GraphDescription = It.Value();
		const float GraphYScale = AllottedGeometry.Size.Y/ScaleY;
		
		const float UnitTypeScale = GraphDescription.CombinedGraphDataSource->GetSampleType() == EProfilerSampleTypes::HierarchicalTime ? 1.0f : CounterToTimeScale;
		const float TimeAccuracyMS = FTimeAccuracy::AsFrameTime( TimeBasedAccuracy );

		if( ViewMode == EDataGraphViewModes::Time )
		{
			const float GraphRangeEndMS = FMath::Min( GraphOffsetMS+VisibleTimeMS, DataTotalTimeMS ) - TimeAccuracyMS;

			if( MultiMode == EDataGraphMultiModes::Combined && GraphDescription.CombinedGraphDataSource->GetSourcesNum() > 0 )
			{
				// Draw combined line graph where X=Min, Y=Max, Z=Avg
				for( float GraphStartTimeMS = GraphOffsetMS; GraphStartTimeMS < GraphRangeEndMS; GraphStartTimeMS += TimeAccuracyMS )
				{
					const FVector Value = GraphDescription.CombinedGraphDataSource->GetValueFromTimeRange( GraphStartTimeMS, GraphStartTimeMS+TimeAccuracyMS );
					const float XPos = DistanceBetweenPoints*GraphPoints.Num();

					// X=Min
					{
						const float YPos = FMath::Clamp( AllottedGeometry.Size.Y - GraphYScale*Value.X*UnitTypeScale, 0.0f, AllottedGeometry.Size.Y );
						new (GraphPoints) FVector2D(XPos,YPos);
					}

					// Y=Max
					{
						const float YPos = FMath::Clamp( AllottedGeometry.Size.Y - GraphYScale*Value.Y*UnitTypeScale, 0.0f, AllottedGeometry.Size.Y );
						new (GraphPoints2) FVector2D(XPos,YPos);
					}

					// Z=Avg
					{
						const float YPos = FMath::Clamp( AllottedGeometry.Size.Y - GraphYScale*Value.Z*UnitTypeScale, 0.0f, AllottedGeometry.Size.Y );
						new (GraphPoints3) FVector2D(XPos,YPos);
					}
				}

				// Min
				FSlateDrawElement::MakeLines
				(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(),
					GraphPoints,
					MyClippingRect,
					DrawEffects,
					InWidgetStyle.GetColorAndOpacityTint() * GraphDescription.ColorBackground,
					false
				);
				GraphPoints.Empty( NumVisiblePoints );

				// Max
				FSlateDrawElement::MakeLines
				(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(),
					GraphPoints2,
					MyClippingRect,
					DrawEffects,
					InWidgetStyle.GetColorAndOpacityTint() * GraphDescription.ColorExtremes,
					false
				);
				GraphPoints2.Empty( NumVisiblePoints );

				// Avg
				FSlateDrawElement::MakeLines
				(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(),
					GraphPoints3,
					MyClippingRect,
					DrawEffects,
					InWidgetStyle.GetColorAndOpacityTint() * GraphDescription.ColorAverage,
					false
				);
				GraphPoints3.Empty( NumVisiblePoints );
				
				LayerId++;
			}
			else if( MultiMode == EDataGraphMultiModes::OneLinePerDataSource )
			{
				// Draw line graph for each graph data source.
				for( auto GraphSourceIt = GraphDescription.CombinedGraphDataSource->GetSourcesIterator(); GraphSourceIt; ++GraphSourceIt )
				{
					const FGraphDataSourceRefConst& GraphDataSource = GraphSourceIt.Value();
 
					for( float GraphStartTimeMS = GraphOffsetMS; GraphStartTimeMS < GraphRangeEndMS; GraphStartTimeMS += TimeAccuracyMS )
					{
						const float Value = GraphDataSource->GetValueFromTimeRange( GraphStartTimeMS, GraphStartTimeMS+TimeAccuracyMS );
						const float XPos = DistanceBetweenPoints*GraphPoints.Num();
						const float YPos = FMath::Clamp( AllottedGeometry.Size.Y - GraphYScale*Value*UnitTypeScale, 0.0f, AllottedGeometry.Size.Y );
						new (GraphPoints) FVector2D(XPos,YPos);
					}
 
					FSlateDrawElement::MakeLines
					(
						OutDrawElements,
						LayerId,
						AllottedGeometry.ToPaintGeometry(),
						GraphPoints,
						MyClippingRect,
						DrawEffects,
						InWidgetStyle.GetColorAndOpacityTint() * GraphDescription.ColorAverage,
						false
					);
					GraphPoints.Empty( NumVisiblePoints );
				}
				LayerId++;
			}
		}
		else if( ViewMode == EDataGraphViewModes::Index )
		{
			if( MultiMode == EDataGraphMultiModes::OneLinePerDataSource )
			{
				for( auto GraphSourceIt = GraphDescription.CombinedGraphDataSource->GetSourcesIterator(); GraphSourceIt; ++GraphSourceIt )
				{			
					const FGraphDataSourceRefConst& GraphDataSource = GraphSourceIt.Value();
					const int32 GraphRangeEndIndex = FMath::Min( GraphOffset+NumVisiblePoints+1, NumDataPoints );
 
					for( uint32 GraphStartIndex = (uint32)GraphOffset; GraphStartIndex < (uint32)GraphRangeEndIndex; GraphStartIndex++ )
					{
						const float Value = GraphDataSource->GetValueFromIndex( GraphStartIndex );
						const float XPos = DistanceBetweenPoints*(float)GraphPoints.Num();
						const float YPos = FMath::Clamp( AllottedGeometry.Size.Y - GraphYScale*Value*UnitTypeScale, 0.0f, AllottedGeometry.Size.Y );
						GraphPoints.Add( FVector2D(XPos,YPos) );
					}
 
					FSlateDrawElement::MakeLines
					(
						OutDrawElements,
						LayerId,
						AllottedGeometry.ToPaintGeometry(),
						GraphPoints,
						MyClippingRect,
						DrawEffects,
						InWidgetStyle.GetColorAndOpacityTint() * GraphDescription.ColorAverage,
						false
					);
					GraphPoints.Empty( NumVisiblePoints );
				}
 
				LayerId++;
			}
		}
	}


	FSlateFontInfo SummaryFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 8 );
	const float MaxFontCharHeight = FontMeasureService->Measure( TEXT("!"), SummaryFont ).Y;

	// Draw graph annotations.
	//const IDataProviderPtr DataProvider = StatIDToGraphDescriptionMapping.Num() > 0 ? StatIDToGraphDescriptionMapping.CreateConstIterator().Key().DataSource->GetDataProvider() : NULL;
	// Bottom	-	Frame numbers, starting from 0
	// Top		-	Time, normalized to the beginning of the capture process
	// Left		-	Values in ms, for the cycle counters
	// Right	-	Values in human readable number, for the counters

	
	//-----------------------------------------------------------------------------
	// Data provider is needed for time line markers.

	static const FLinearColor GridColor = FLinearColor(0.0f,0.0f,0.0f, 0.25f);
	static const FLinearColor GridTextColor = FLinearColor(1.0f,1.0f,1.0f, 0.25f);
	TArray<FVector2D> LinePoints;
	const float LabelSize = MaxFontCharHeight * 7.0f;

	const FGraphDescription* GraphDesc = GetFirstGraph();
	if( GraphDesc && GraphDesc->CombinedGraphDataSource->GetFirstSource() )
	{
		const FGraphDataSourceRefConst* GraphDataSource = GraphDesc->CombinedGraphDataSource->GetFirstSource();

		if( ViewMode == EDataGraphViewModes::Index )
		{
			// Draw a vertical line every 60 frames.
			const int32 AvgFrameRate = 60;
			const int32 FrameStartIndex = GraphOffset + AvgFrameRate - (GraphOffset % AvgFrameRate);
			const int32 FrameEndIndex = FMath::Min( GraphOffset + NumVisiblePoints, NumDataPoints );
			const IDataProviderRef DataProvider = (*GraphDataSource)->GetDataProvider();

			for( int32 FrameIndex = FrameStartIndex; FrameIndex < FrameEndIndex; FrameIndex += AvgFrameRate )
			{
				const float MarkerPosX = (FrameIndex - GraphOffset) * DistanceBetweenPoints;
				const float ElapsedFrameTimeMS = DataProvider->GetElapsedFrameTimeMS( FrameIndex );

				LinePoints.Add( FVector2D(MarkerPosX, 0.0) );
				LinePoints.Add( FVector2D(MarkerPosX, AllottedGeometry.Size.Y) );
				FSlateDrawElement::MakeLines
				(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(),
					LinePoints,
					MyClippingRect,
					DrawEffects,
					GridColor
				);
				LinePoints.Empty();

				// Don't draw label if too close to the time values.
				if( MarkerPosX < LabelSize || MarkerPosX > AreaX1-LabelSize )
				{
					continue;
				}

				// Bottom - Frame numbers, starting from 0.
				const FString AccumulatedFrameNumberStr = FString::Printf(TEXT("%i"), FrameIndex);
				FSlateDrawElement::MakeText
				(
					OutDrawElements, 
					LayerId, 
					AllottedGeometry.ToOffsetPaintGeometry( FVector2D(MarkerPosX,2.0f) ),
					AccumulatedFrameNumberStr,
					SummaryFont, 
					MyClippingRect, 
					DrawEffects, 
					FLinearColor::White
				);

				// Top - Time, normalized to the beginning of the capture process.
				const FString ElapseTimeStr = FString::Printf(TEXT("%.1fs"), ElapsedFrameTimeMS * 0.001f );
				FSlateDrawElement::MakeText
				(
					OutDrawElements, 
					LayerId, 
					AllottedGeometry.ToOffsetPaintGeometry( FVector2D(MarkerPosX,AllottedGeometry.Size.Y-2.0f-MaxFontCharHeight) ),
					ElapseTimeStr,
					SummaryFont, 
					MyClippingRect, 
					DrawEffects, 
					FLinearColor::White
				);
			}
		}
		else if( ViewMode == EDataGraphViewModes::Time )
		{
			// Draw a vertical line every one second.
			const int32 AvgFrameRate = FTimeAccuracy::AsFPSCounter( TimeBasedAccuracy );
			const int32 FrameStartIndex = GraphOffset + AvgFrameRate - (GraphOffset % AvgFrameRate);
			const int32 FrameEndIndex = FMath::Min( GraphOffset + NumVisiblePoints, NumDataPoints );

			const bool bCanBeDisplayedAsMulti = GraphDesc->CombinedGraphDataSource->CanBeDisplayedAsMulti();
			const IDataProviderRef DataProvider = (*GraphDataSource)->GetDataProvider();

			for( int32 FrameIndex = FrameStartIndex; FrameIndex < FrameEndIndex; FrameIndex += AvgFrameRate )
			{
				const float MarkerPosX = (FrameIndex - GraphOffset) * DistanceBetweenPoints;
				const float ElapsedFrameTimeMS = FrameIndex * FTimeAccuracy::AsFrameTime( TimeBasedAccuracy );
				const int32 ElapsedFrameTime = FMath::Max( FMath::RoundToInt( ElapsedFrameTimeMS * 0.001f ) - 1, 0 );
				const int32 AccumulatedFrameCounter = bCanBeDisplayedAsMulti ? FrameIndex : DataProvider->GetAccumulatedFrameCounter(ElapsedFrameTime);

				LinePoints.Add( FVector2D(MarkerPosX, 0.0) );
				LinePoints.Add( FVector2D(MarkerPosX, AllottedGeometry.Size.Y) );
				FSlateDrawElement::MakeLines
				(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(),
					LinePoints,
					MyClippingRect,
					DrawEffects,
					GridColor
				);
				LinePoints.Empty();

				// Don't draw label if too close to the time values.
				if( MarkerPosX < LabelSize || MarkerPosX > AreaX1-LabelSize )
				{
					continue;
				}

				// Bottom - Time, normalized to the beginning of the capture process.
				const FString ElapsedTimeStr = FString::Printf(TEXT("%.1fs"), ElapsedFrameTimeMS * 0.001f);
				FSlateDrawElement::MakeText
				(
					OutDrawElements, 
					LayerId, 
					AllottedGeometry.ToOffsetPaintGeometry( FVector2D(MarkerPosX,2.0f) ),
					ElapsedTimeStr,
					SummaryFont, 
					MyClippingRect, 
					DrawEffects, 
					FLinearColor::White
				);

				// Top - Frame numbers, starting from 0, for single session current frame counter.
				const FString AccumulatedFrameNumberStr = FString::Printf(TEXT("%i"), AccumulatedFrameCounter);
				FSlateDrawElement::MakeText
				(
					OutDrawElements, 
					LayerId, 
					AllottedGeometry.ToOffsetPaintGeometry( FVector2D(MarkerPosX,AllottedGeometry.Size.Y-2.0f-MaxFontCharHeight) ),
					AccumulatedFrameNumberStr,
					SummaryFont, 
					MyClippingRect, 
					DrawEffects, 
					FLinearColor::White
				);
			}
		}

		LayerId++;
	}

	//-----------------------------------------------------------------------------

	// Draw horizontal i vertical line, where mouse is located with some tooltips.
	// Dashed line aka focuses widget.
// 	FSlateDrawElement::MakeBox
// 	(
// 		WindowElementList,
// 		InLayerId++,
// 		FPaintGeometry(
// 		FocusedWidgetGeomertry.Geometry.AbsolutePosition - FocusPath.GetWindow()->GetPositionInScreen(),
// 		FocusedWidgetGeomertry.Geometry.Size * FocusedWidgetGeomertry.Geometry.Scale,
// 		FocusedWidgetGeomertry.Geometry.Scale ),
// 		FEditorStyle::GetBrush("FocusRectangle"),
// 		FocusPath.GetWindow()->GetClippingRectangleInWindow(),
// 		ESlateDrawEffect::None,
// 		FColor(255,255,255,128)
// 	);

	const int32 MaxGridPixelSpacing = 160.0f;

	// Draw a horizontal lines every 150 pixels and draw a few basic lines like 5ms, 10ms, 16ms, 33ms
	static const TArray<float> DefaultTimeValueHints = TArrayBuilder<float>()
		.Add( 5.0f )
		.Add( 10.0f )
		.Add( 16.6f )
		.Add( 33.0f );

	const FLinearColor HintColor05(0.0f,1.0f,1.0f, 0.5f);
	const FLinearColor HintColor33(0.5f,1.0f,0.0f, 0.5f);

	static TMap<float,FLinearColor> DefaultTimeValueHintColors = TMapBuilder<float,FLinearColor>()
		.Add( 5.0f, FMath::Lerp(HintColor05,HintColor33,0.0f) )
		.Add( 10.0f, FMath::Lerp(HintColor05,HintColor33,0.33f) )
		.Add( 16.6f, FMath::Lerp(HintColor05,HintColor33,0.66f) )
		.Add( 33.0f, FMath::Lerp(HintColor05,HintColor33,1.0f) );


	// Time value hints based on the graph height and maximum value the can be displayed on this graph.
	TArray<float> TimeValueHints = DefaultTimeValueHints;
	const int SecondaryIndicators = (int)AllottedGeometry.Size.Y/MaxGridPixelSpacing + 1;
	//const float RealTimeGridSpacing = AllottedGeometry.Size.Y / (float)SecondaryIndicators;

	const float MinTimeValue = 0.0f;
	const float MaxTimeValue = ScaleY;
	const float TimeValueGraphScale = MaxTimeValue / SecondaryIndicators;
	const float TimeValueToGraph = AllottedGeometry.Size.Y/MaxTimeValue;

	for( int32 SecondaryIndex = 1; SecondaryIndex <= SecondaryIndicators; SecondaryIndex++ )
	{
		TimeValueHints.AddUnique( (float)SecondaryIndex*TimeValueGraphScale );
	}
	
	// Generate the list of hints with value scaled to the graph height.
	TArray<float> TimeValueHintsGraph;
	for( int32 Index = 0; Index < TimeValueHints.Num(); Index++ )
	{
		TimeValueHintsGraph.Add( TimeValueHints[Index]*TimeValueToGraph );
	}

	// First pass, hide hints which are outside this graph bounds or if basic lines are placed too tight.
	for( int32 HintIndex = 0; HintIndex < TimeValueHintsGraph.Num(); HintIndex++ )
	{
		float& CurrentHintY = TimeValueHintsGraph[HintIndex];
		if( CurrentHintY < MaxGridPixelSpacing*0.5f && HintIndex!=TimeValueHintsGraph.Num()-1 )
		{
			// Mark as hidden.
			CurrentHintY = -1.0f;
			TimeValueHints[HintIndex] = -1.0f;
		}
		else if( CurrentHintY > AllottedGeometry.Size.Y )
		{
			// Mark as hidden.
			CurrentHintY = -1.0f;
			TimeValueHints[HintIndex] = -1.0f;
		}
	}

	// Zero is always visible.
	TimeValueHints.Add( 0.0f );
	TimeValueHintsGraph.Add( 0.0f );

	TimeValueHints.Sort();
	TimeValueHintsGraph.Sort();

	// Second pass, remove hints that are too close to each other, but promote hints from the default list.
	// First needs to be always visible.
	const float MinGridSpacing = MaxFontCharHeight * 3.0f;
	const int FirstHintIndex = TimeValueHints.Find( 0.0f );
	int32 LastVisibleHintIndex = FirstHintIndex+1;
	for( int32 CurrentHintIndex = LastVisibleHintIndex+1; CurrentHintIndex < TimeValueHintsGraph.Num()-1; CurrentHintIndex++, LastVisibleHintIndex++ )
	{
		const float LastVisibleHintY = TimeValueHintsGraph[LastVisibleHintIndex];
		const float CurrentHintY = TimeValueHintsGraph[CurrentHintIndex];

		if( LastVisibleHintY < 0.0f )
		{
			continue;
		}

		if( CurrentHintY < 0.0f )
		{
			continue;
		}

		if( CurrentHintY - LastVisibleHintY < MinGridSpacing )
		{
			// This hints should be hidden, but check if this hints is a basic one.
			const bool bLastIsBasic = DefaultTimeValueHints.Contains( TimeValueHints[LastVisibleHintIndex] );
			const bool bCurrentIsBasic = DefaultTimeValueHints.Contains( TimeValueHints[CurrentHintIndex] );

			// Mark as hidden.
			if( bLastIsBasic && !bCurrentIsBasic )
			{
				TimeValueHintsGraph[CurrentHintIndex] = -1.0f;
				TimeValueHints[CurrentHintIndex] = -1.0f;
			}
			else if( !bLastIsBasic && bCurrentIsBasic )
			{
				TimeValueHintsGraph[LastVisibleHintIndex] = -1.0f;
				TimeValueHints[LastVisibleHintIndex] = -1.0f;
			}
			else
			{
				int32 k=0;k++;
			}

			LastVisibleHintIndex += 1;
			CurrentHintIndex += 1;
			continue;
		}
	}

	for( int32 IndicatorIndex = 0; IndicatorIndex < TimeValueHints.Num(); ++IndicatorIndex )
	{
		const float TimeValue = TimeValueHints[IndicatorIndex];
		
		if( TimeValue < 0.0f )
		{
			// Ignore hidden hints.
			continue;
		}

		const float MarkerPosY = AllottedGeometry.Size.Y - TimeValue*TimeValueToGraph;

		// Check if this hint should be drawn as the basic hint.
		const FLinearColor* BasicHintColor = DefaultTimeValueHintColors.Find( TimeValue );

		LinePoints.Add( FVector2D(0, MarkerPosY) );
		LinePoints.Add( FVector2D(AreaX1, MarkerPosY) );
		FSlateDrawElement::MakeLines
		(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			MyClippingRect,
			DrawEffects,
			BasicHintColor ? *BasicHintColor : GridColor
		);
		LinePoints.Empty();

		float HintOffsetY = 2.0f/*-MaxFontCharHeight * 0.5f*/;
		if( IndicatorIndex == FirstHintIndex )
		{
			HintOffsetY = -MaxFontCharHeight;
		}
		else if( IndicatorIndex==TimeValueHints.Num()-1 )
		{
			HintOffsetY = 2.0f;
		}

		FString TimeValueStr;
		if( BasicHintColor )
		{
			TimeValueStr = FString::Printf( TEXT("%.1fms (%iFPS)"), TimeValue, int32(1000.0f/TimeValue) );
		}
		else
		{
			TimeValueStr = FString::Printf( TEXT("%.1fms "), TimeValue, TimeValue*TimeValueToGraph );
		}

		// Left		-	Values in ms, for the hierarchical samples
		FSlateDrawElement::MakeText
		(
			OutDrawElements, 
			LayerId, 
			AllottedGeometry.ToOffsetPaintGeometry( FVector2D(2.0f,MarkerPosY+HintOffsetY) ),
			TimeValueStr,
			SummaryFont, 
			MyClippingRect, 
			DrawEffects, 
			BasicHintColor ? *BasicHintColor : FLinearColor::White
		);

		// Right	-	Values in human readable string, for the non-hierarchical samples
		const FString CounterValueStr = FString::Printf( TEXT("%.1f KB"), TimeValue/CounterToTimeScale );
		const float RightValueSizeX = FontMeasureService->Measure( CounterValueStr, SummaryFont ).X;
		FSlateDrawElement::MakeText
		(
			OutDrawElements, 
			LayerId, 
			AllottedGeometry.ToOffsetPaintGeometry( FVector2D(AreaX1-RightValueSizeX,MarkerPosY+HintOffsetY) ),
			CounterValueStr,
			SummaryFont, 
			MyClippingRect, 
			DrawEffects, 
			FLinearColor::White
		);
	}

	// Draw selected frames markers.
	{
		LayerId++;
		const float LocalGraphOffset = GraphOffset * DistanceBetweenPoints;
		const float LocalGraphSelectionX0 = FrameIndices[0]*DistanceBetweenPoints - LocalGraphOffset;
		const float LocalGraphSelectionX1 = FrameIndices[1]*DistanceBetweenPoints - LocalGraphOffset;
		const float LocalGraphSelectionX[2] = { LocalGraphSelectionX0, LocalGraphSelectionX1 };

		const uint32 NumVisibleFrameMarkers = ( FrameIndices[0]==FrameIndices[1] ) ? 1 : 2;

		for( uint32 Nx = 0; Nx < NumVisibleFrameMarkers; ++Nx )
		{
			if( LocalGraphSelectionX[Nx]+HalfGraphMarkerWidth > 0.0f && LocalGraphSelectionX[Nx]-HalfGraphMarkerWidth < AreaX1 )
			{
				FSlateDrawElement::MakeBox
				(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry( FVector2D(LocalGraphSelectionX[Nx]-HalfGraphMarkerWidth,0.0f), FVector2D(GraphMarkerWidth, AllottedGeometry.Size.Y) ),
					FEditorStyle::GetBrush("ProgressBar.Background"),
					MyClippingRect,
					DrawEffects,
					FColor(64,64,255,128)
				);
			}
		}

		if( NumVisibleFrameMarkers == 2 )
		{
			const bool bIsSelectionVisible = LocalGraphSelectionX1<AreaX0 || LocalGraphSelectionX0>AreaX1 ? false : true;
			if( bIsSelectionVisible )
			{
				// Highlight selected area, clamp the box to the visible area.
				const float GraphSelectionX0 = FMath::Max( LocalGraphSelectionX0, AreaX0 );
				const float GraphSelectionX1 = FMath::Min( LocalGraphSelectionX1, AreaX1 );
				const float GraphSelectionW = GraphSelectionX1-GraphSelectionX0;

				FSlateDrawElement::MakeBox
				(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry( FVector2D(GraphSelectionX0/*+HalfGraphMarkerWidth*/,0.0f), FVector2D(GraphSelectionW/*-GraphMarkerWidth*/, AllottedGeometry.Size.Y) ),
					FEditorStyle::GetBrush("ProgressBar.Background"),
					MyClippingRect,
					DrawEffects,
					FColor(64,64,255,32)
				);
			}	
		}
	}

	// Draw current mouse position.
	{
		LayerId++;

		const int32 LocalPosition = HoveredFrameIndex - GraphOffset;
		const float LocalPositionGraphX = LocalPosition * DistanceBetweenPoints;

		FSlateDrawElement::MakeBox
		(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry( FVector2D(LocalPositionGraphX-HalfGraphMarkerWidth,0.0f), FVector2D(GraphMarkerWidth, AllottedGeometry.Size.Y) ),
			FEditorStyle::GetBrush("ProgressBar.Background"),
			MyClippingRect,
			DrawEffects,
			FColor(255,128,128,128)
		);
	}

	// Draw all graphs descriptions.
	float GraphDescPosY = 100.0f;

#if DEBUG_PROFILER_PERFORMANCE
	// Debug text.
	FSlateDrawElement::MakeText
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToOffsetPaintGeometry( FVector2D(16.0f,GraphDescPosY) ),
		FString::Printf( TEXT("ScaleY: %f MPos: %s Hovered: %i (%.1f)"), ScaleY, *MousePosition.ToString(), HoveredFrameIndex, HoveredFrameStartTimeMS ),
		SummaryFont,
		MyClippingRect,
		DrawEffects,
		FLinearColor::White
	);
	GraphDescPosY += MaxFontCharHeight + 1.0f;

	// Debug text.
	FSlateDrawElement::MakeText
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToOffsetPaintGeometry( FVector2D(16.0f,GraphDescPosY) ),
		FString::Printf( TEXT("Offset: %4i (%.1f) Num: %4i (%.1f) NumVis: %4i (%.1f)"), GraphOffset, GraphOffsetMS, NumDataPoints, DataTotalTimeMS, NumVisiblePoints, VisibleTimeMS ),
		SummaryFont,
		MyClippingRect,
		DrawEffects,
		FLinearColor::White
	);
	GraphDescPosY += MaxFontCharHeight + 1.0f;

	// Debug text.
	FSlateDrawElement::MakeText
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToOffsetPaintGeometry( FVector2D(16.0f,GraphDescPosY) ),
		FString::Printf( TEXT("SelFr: %i-%i (%.1f-%.1f)"), FrameIndices[0], FrameIndices[1], FrameTimesMS[0], FrameTimesMS[1] ),
		SummaryFont,
		MyClippingRect,
		DrawEffects,
		FLinearColor::White
	);
	GraphDescPosY += MaxFontCharHeight + 1.0f;

	const double CurrentTime = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
	if( CurrentTime > 1.0f )
	{
		TotalTime += CurrentTime;
		NumCalls ++;
		UE_LOG( Profiler, Log, TEXT("%4.2f, %4.2f, %5u"), CurrentTime, TotalTime/(double)NumCalls, NumCalls );
	}
#endif // DEBUG_PROFILER_PERFORMANCE

	return SCompoundWidget::OnPaint(AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled && IsEnabled() );
}



void SDataGraph::AddInnerGraph
(
	const uint32 InStatID, 	
	const FLinearColor InColorAverage,
	const FLinearColor InColorExtremes,
	const FLinearColor InColorBackground, 
	const FCombinedGraphDataSourceRef& CombinedGraphDataSource  
)
{
	FGraphDescription GraphDescriptionLine( CombinedGraphDataSource, InColorAverage, InColorExtremes, InColorBackground );

	TSharedPtr<SWidget> GraphSummary;
	GraphDescriptionsVBox->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	.Padding( 1.0f )
	[
		SAssignNew(GraphSummary,SDataGraphSummary)
		.ParentWidget( SharedThis(this) )
		.GraphDescription( GraphDescriptionLine )
		.OnGetMouseFrameIndex( this, &SDataGraph::DataGraphSummary_GetHoveredFrameIndex )
	];

	StatIDToGraphDescriptionMapping.Add( InStatID, GraphDescriptionLine );
	StatIDToWidgetMapping.Add( InStatID, GraphSummary.ToSharedRef() );

	UpdateState();
}

void SDataGraph::RemoveInnerGraph( const uint32 InStatID )
{
	FGraphDescription* GraphDescription = StatIDToGraphDescriptionMapping.Find( InStatID );
	if( GraphDescription )
	{
		TSharedRef<SWidget> DataGraphSummary = StatIDToWidgetMapping.FindChecked( InStatID );

		GraphDescriptionsVBox->RemoveSlot( DataGraphSummary );
		
		StatIDToWidgetMapping.Remove( InStatID );
		StatIDToGraphDescriptionMapping.Remove( InStatID );
	}
}

FReply SDataGraph::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FReply::Unhandled();
	MousePositionOnButtonDown = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );

	if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsLMB_Pressed = true;
		if( NumDataPoints > 0 )
		{
			// Capture mouse, so we can move outside this widget.
			FrameIndices[0]=FrameIndices[1]= HoveredFrameIndex;
			FrameTimesMS[0]=FrameTimesMS[1]= HoveredFrameStartTimeMS;
			Reply = FReply::Handled().CaptureMouse(SharedThis(this));
		}
	}
	else if( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{
		bIsRMB_Pressed = true;
		if( NumDataPoints > 0 )
		{
			// Capture mouse, so we can scroll outside this widget.
			RealGraphOffset = GraphOffset;
			Reply = FReply::Handled().CaptureMouse(SharedThis(this));
		}
	}

	return Reply;
}

FReply SDataGraph::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FReply::Unhandled();
	MousePositionOnButtonUp = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );

	const bool bIsValidForMouseClick = MousePositionOnButtonUp.Equals( MousePositionOnButtonDown, 2.0f );

	if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton  )
	{
		if( bIsLMB_Pressed )
		{
			FrameIndices[1] = HoveredFrameIndex;
			FrameTimesMS[1] = HoveredFrameStartTimeMS;

			if( FrameIndices[0] > FrameIndices[1] )
			{
				Exchange( FrameIndices[0], FrameIndices[1] );
				Exchange( FrameTimesMS[0], FrameTimesMS[1] );
			}

			if( ViewMode == EDataGraphViewModes::Index )
			{
				SelectionChangedForIndexEvent.Broadcast( FrameIndices[0], FrameIndices[1] );
			}
			else if( ViewMode == EDataGraphViewModes::Time )
			{
				OnSelectionChangedForTime.ExecuteIfBound( FrameTimesMS[0], FrameTimesMS[1] );
			}

			// Release mouse as we no longer drag
			bIsLMB_SelectionDragging = false;
			Reply = FReply::Handled().ReleaseMouseCapture();
		}

		bIsLMB_Pressed = false;
	}
	else if( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{
		if( bIsRMB_Pressed )
		{
			if( !bIsRMB_Scrolling && bIsValidForMouseClick )
			{
				ShowContextMenu( MouseEvent.GetScreenSpacePosition() );
				Reply = FReply::Handled();
			}
			else
			if( bIsRMB_Scrolling )
			{
				// Release mouse as we no longer scroll
				bIsRMB_Scrolling = false;
				Reply = FReply::Handled().ReleaseMouseCapture();
			}
		}

		bIsRMB_Pressed = false;
	}

	return Reply;
}

FReply SDataGraph::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FReply::Unhandled();
	MousePosition = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
	HoveredFrameIndex = CalculateFrameIndex( MousePosition );
	HoveredFrameStartTimeMS = HoveredFrameIndex * FTimeAccuracy::AsFrameTime( TimeBasedAccuracy );

	if( MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) )
	{
		if( HasMouseCapture() && !MouseEvent.GetCursorDelta().IsZero() )
		{
			bIsLMB_SelectionDragging = true;

			FrameIndices[1] = HoveredFrameIndex;
			FrameTimesMS[1] = HoveredFrameStartTimeMS;

			Reply = FReply::Handled();
		}
	}
	else if( MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) )
	{
		if( HasMouseCapture() && !MouseEvent.GetCursorDelta().IsZero() )
		{
			bIsRMB_Scrolling = true;
			const float ScrollByAmount = -MouseEvent.GetCursorDelta().X * (1.0f/DistanceBetweenPoints);
			RealGraphOffset += ScrollByAmount;

			GraphOffset = FMath::Clamp( FMath::TruncToInt( RealGraphOffset ), 0, FMath::Max(NumDataPoints-NumVisiblePoints,0) );
			OnGraphOffsetChanged.ExecuteIfBound( GraphOffset );

			Reply = FReply::Handled();
		}
	}

	return Reply; // SAssetViewItem::CreateToolTipWidget
}

const int32 SDataGraph::CalculateFrameIndex( const FVector2D InMousePosition ) const
{
	const float ScaleX = 1.0f/DistanceBetweenPoints;
	const int32 MousePositionOffset = FMath::TruncToInt( (InMousePosition.X+HalfGraphMarkerWidth) * ScaleX );
	return FMath::Clamp( GraphOffset+MousePositionOffset, 0, NumDataPoints-1 );
}

void SDataGraph::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{

}

void SDataGraph::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	if( !HasMouseCapture() )
	{
		// No longer scrolling (unless we have mouse capture).
		bIsRMB_Scrolling = false;
		bIsLMB_SelectionDragging = false;

		bIsLMB_Pressed = false;
		bIsRMB_Pressed = false;
	}
}

FReply SDataGraph::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// 2^ 3=8
	// 2^11=2048
	MouseWheelAcc += MouseEvent.GetWheelDelta() * 0.25f;
	MouseWheelAcc = FMath::Clamp( MouseWheelAcc, 3.0f, 11.0f );

	ScaleY = FMath::Pow( 2.0f, MouseWheelAcc );

	return FReply::Handled();
}

const FEventGraphDataHandlerRef SDataGraph::PrepareEventGraphDataHandler( const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition )
{
	const FGraphDescription* GraphDescription = GetFirstGraph();

	if( GraphDescription )
	{
		if( ViewMode == EDataGraphViewModes::Time )
		{
			TMap<FGuid,uint32> StartIndices;
			const float TimeAccuracyMS = FTimeAccuracy::AsFrameTime( TimeBasedAccuracy );
			GraphDescription->CombinedGraphDataSource->GetStartIndicesFromTimeRange( FrameTimesMS[0], FrameTimesMS[1], StartIndices );

			const FEventGraphDataHandlerRef Params = MakeShareable( new FEventGraphDataHandler( /*StartIndices*/ ) ); 
			return Params;
		}
		else if( ViewMode == EDataGraphViewModes::Index )
		{
			const FGraphDataSourceRefConst* GraphDataSource = GraphDescription->CombinedGraphDataSource->GetFirstSource();

			if( GraphDataSource )
			{
				const FEventGraphDataHandlerRef Params = MakeShareable( new FEventGraphDataHandler( (*GraphDataSource)->GetSessionInstanceID(), FrameIndices[0], FrameIndices[1], EEventGraphTypes::Maximum ) ); 
				return Params;
			}
		}
	}

	return MakeShareable( new FEventGraphDataHandler() );
}

FReply SDataGraph::OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Unhandled();
}

void SDataGraph::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	SCompoundWidget::OnDragEnter(MyGeometry, DragDropEvent);

	TSharedPtr<FStatIDDragDropOp> Operation = DragDropEvent.GetOperationAs<FStatIDDragDropOp>();
	if (Operation.IsValid())
	{
		Operation->ShowOK();
	}
}

void SDataGraph::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	SCompoundWidget::OnDragLeave(DragDropEvent);

	TSharedPtr<FStatIDDragDropOp> Operation = DragDropEvent.GetOperationAs<FStatIDDragDropOp>();
	if (Operation.IsValid())
	{
		Operation->ShowError();
	}
}

FReply SDataGraph::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	return SCompoundWidget::OnDragOver(MyGeometry,DragDropEvent);
}

FReply SDataGraph::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FStatIDDragDropOp> Operation = DragDropEvent.GetOperationAs<FStatIDDragDropOp>();

	if(Operation.IsValid())
	{
		if( Operation->IsSingleStatID() )
		{
			FProfilerManager::Get()->TrackStat( Operation->GetSingleStatID() );
		}
		else
		{
			const TArray<int32>& StatIDs = Operation->GetStatIDs();
			const int32 NumStatIDs = StatIDs.Num();
			for( int32 Nx = 0; Nx < NumStatIDs; ++Nx )
			{
				FProfilerManager::Get()->TrackStat( StatIDs[Nx] );
			}
		}
		return FReply::Handled();
	}
	return SCompoundWidget::OnDrop(MyGeometry,DragDropEvent);
}

FCursorReply SDataGraph::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const 
{
	if( bIsRMB_Scrolling )
	{
		return FCursorReply::Cursor( EMouseCursor::GrabHand );
	}
	else if( bIsLMB_SelectionDragging )
	{
		return FCursorReply::Cursor( EMouseCursor::GrabHandClosed );
	}

	return FCursorReply::Unhandled();
}

void SDataGraph::ShowContextMenu( const FVector2D& ScreenSpacePosition )
{
	TSharedPtr<FUICommandList> ProfilerCommandList = FProfilerManager::Get()->GetCommandList();
	const FProfilerCommands& ProfilerCommands = FProfilerManager::GetCommands();
	const FProfilerActionManager& ProfilerActionManager = FProfilerManager::GetActionManager();

	// Build data required for opening event graph(s).
	const FEventGraphDataHandlerRef EventGraphDataHandler = PrepareEventGraphDataHandler( ThisGeometry, ScreenSpacePosition );

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, ProfilerCommandList );

	if( !FProfilerManager::GetSettings().bSingleInstanceMode )
	{
		MenuBuilder.BeginSection( "ProfilerInstances", LOCTEXT("ContextMenu_ProfilerInstances", "Profiler Instances") );
		{
			// @TODO: Add to FProfilerMenuBuilder
			struct FProfilerSessionMenu
			{
				static void Build( FMenuBuilder& InMenuBuilder, const FGuid SessionInstanceID, const FEventGraphDataHandlerRef EventGraphDataHandler )
				{
					TSharedPtr<FUICommandList> ProfilerCommandList = FProfilerManager::Get()->GetCommandList();
					const FProfilerCommands& Commands = FProfilerManager::GetCommands();
					const FProfilerActionManager& ProfilerActionMgr = FProfilerManager::GetActionManager();

					FProfilerMenuBuilder::AddMenuEntry
					( 
						InMenuBuilder, 
						Commands.ToggleDataPreview, 
						ProfilerActionMgr.ToggleDataPreview_Custom(SessionInstanceID) 
					);

					FProfilerMenuBuilder::AddMenuEntry
					( 
						InMenuBuilder, 
						Commands.ToggleDataCapture, 
						ProfilerActionMgr.ToggleDataCapture_Custom(SessionInstanceID) 
					);

					if( SessionInstanceID.IsValid() )
					{
						FProfilerMenuBuilder::AddMenuEntry
						( 
							InMenuBuilder, 
							Commands.ToggleShowDataGraph, 
							ProfilerActionMgr.ToggleShowDataGraph_Custom(SessionInstanceID) 
						);
					}
				}
			};

			if( FProfilerManager::Get()->GetProfilerInstancesNum() > 1 )
			{
				MenuBuilder.AddSubMenu
				( 
					LOCTEXT("ContextMenu_AllProfilerInstances", "AllInstances"), 
					LOCTEXT("ContextMenu_AllProfilerInstances_TT", "All profiler instances options"), 
					FNewMenuDelegate::CreateStatic( &FProfilerSessionMenu::Build, FGuid(), EventGraphDataHandler )
				);
			}

			MenuBuilder.AddMenuSeparator();

			for( auto It = FProfilerManager::Get()->GetProfilerInstancesIterator(); It; ++It )
			{
				const FProfilerSessionRef& ProfilerSession = It.Value();
				const FGuid SessionInstanceID = ProfilerSession->GetInstanceID();

				MenuBuilder.AddSubMenu
				( 
					FText::FromString( ProfilerSession->GetShortName() ), 
					LOCTEXT("ContextMenu_InstancesList_TT", "Profiler instance options"), 
					FNewMenuDelegate::CreateStatic( &FProfilerSessionMenu::Build, SessionInstanceID, EventGraphDataHandler )
				);	
			}
		}
		MenuBuilder.EndSection();
	}
	
	MenuBuilder.BeginSection( "ViewMode", LOCTEXT("ContextMenu_ViewMode", "View Mode") );
	{
		MenuBuilder.AddMenuEntry( ProfilerCommands.DataGraph_ViewMode_SetIndexBased );
		// @TODO: Disabled for now.
		//MenuBuilder.AddMenuEntry( ProfilerCommands.DataGraph_ViewMode_SetTimeBased );
	}
	MenuBuilder.EndSection();

	if( !FProfilerManager::GetSettings().bSingleInstanceMode )
	{
		MenuBuilder.BeginSection( "MultiMode", LOCTEXT("ContextMenu_MultiMode", "Multi Mode") );
		{
			MenuBuilder.AddMenuEntry( ProfilerCommands.DataGraph_MultiMode_SetCombined );
			MenuBuilder.AddMenuEntry( ProfilerCommands.DataGraph_MultiMode_SetOneLinePerDataSource );
		}
		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection( TEXT("Misc"), LOCTEXT("Miscellaneous", "Miscellaneous") );
	{
		MenuBuilder.AddMenuEntry( FProfilerManager::GetCommands().EventGraph_SelectAllFrames );
		MenuBuilder.AddMenuEntry( FProfilerManager::GetCommands().ProfilerManager_ToggleLivePreview );
	}
	MenuBuilder.EndSection();

	TSharedRef<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	FSlateApplication::Get().PushMenu( SharedThis( this ), MenuWidget, ScreenSpacePosition, FPopupTransitionEffect::ContextMenu );
}

void SDataGraph::BindCommands()
{
	TSharedPtr<FUICommandList> ProfilerCommandList = FProfilerManager::Get()->GetCommandList();
	const FProfilerCommands& ProfilerCommands = FProfilerManager::GetCommands();

	// DataGraph_ViewMode_SetIndexBased
	ProfilerCommandList->MapAction
	( 
		ProfilerCommands.DataGraph_ViewMode_SetIndexBased,
		FExecuteAction::CreateSP( this, &SDataGraph::ViewMode_SetIndexBased_Execute ),
		FCanExecuteAction::CreateSP( this, &SDataGraph::ViewMode_SetIndexBased_CanExecute ),
		FIsActionChecked::CreateSP( this, &SDataGraph::ViewMode_SetIndexBased_IsChecked )
	);
 
	// DataGraph_ViewMode_SetTimeBased
	ProfilerCommandList->MapAction
	( 
		ProfilerCommands.DataGraph_ViewMode_SetTimeBased,
		FExecuteAction::CreateSP( this, &SDataGraph::ViewMode_SetTimeBased_Execute ),
		FCanExecuteAction::CreateSP( this, &SDataGraph::ViewMode_SetTimeBased_CanExecute ),
		FIsActionChecked::CreateSP( this, &SDataGraph::ViewMode_SetTimeBased_IsChecked )
	);
 
	// DataGraph_MultiMode_SetCombined
	ProfilerCommandList->MapAction
	( 
		ProfilerCommands.DataGraph_MultiMode_SetCombined,
		FExecuteAction::CreateSP( this, &SDataGraph::MultiMode_SetCombined_Execute ),
		FCanExecuteAction::CreateSP( this, &SDataGraph::MultiMode_SetCombined_CanExecute ),
		FIsActionChecked::CreateSP( this, &SDataGraph::MultiMode_SetCombined_IsChecked )
	);
 
	// DataGraph_MultiMode_SetCombined
	ProfilerCommandList->MapAction
	( 
		ProfilerCommands.DataGraph_MultiMode_SetOneLinePerDataSource,
		FExecuteAction::CreateSP( this, &SDataGraph::MultiMode_SetOneLinePerDataSource_Execute ),
		FCanExecuteAction::CreateSP( this, &SDataGraph::MultiMode_SetOneLinePerDataSource_CanExecute ),
		FIsActionChecked::CreateSP( this, &SDataGraph::MultiMode_SetOneLinePerDataSource_IsChecked )
	);
}

/*-----------------------------------------------------------------------------
	ViewMode_SetIndexBased
-----------------------------------------------------------------------------*/

void SDataGraph::ViewMode_SetIndexBased_Execute()
{
	ViewMode = EDataGraphViewModes::Index;
	UpdateState();
	OnViewModeChanged.ExecuteIfBound( ViewMode );
}

bool SDataGraph::ViewMode_SetIndexBased_CanExecute() const
{
	const bool bCanBeDisplayedAsIndexBased = GetFirstGraph() ? GetFirstGraph()->CombinedGraphDataSource->CanBeDisplayedAsIndexBased() : false;
	return ViewMode != EDataGraphViewModes::Index && bCanBeDisplayedAsIndexBased;
}

bool SDataGraph::ViewMode_SetIndexBased_IsChecked() const
{
	return ViewMode == EDataGraphViewModes::Index;
}

/*-----------------------------------------------------------------------------
	ViewMode_SetTimeBased
-----------------------------------------------------------------------------*/

void SDataGraph::ViewMode_SetTimeBased_Execute()
{
	ViewMode = EDataGraphViewModes::Time;
	UpdateState();
	OnViewModeChanged.ExecuteIfBound( ViewMode );
}

bool SDataGraph::ViewMode_SetTimeBased_CanExecute() const
{
	const bool bCanBeDisplayedAsTimeBased = GetFirstGraph() ? GetFirstGraph()->CombinedGraphDataSource->CanBeDisplayedAsTimeBased() : false;
	return ViewMode != EDataGraphViewModes::Time && bCanBeDisplayedAsTimeBased;
}

bool SDataGraph::ViewMode_SetTimeBased_IsChecked() const
{
	return ViewMode == EDataGraphViewModes::Time;
}

/*-----------------------------------------------------------------------------
	MultiMode_SetCombined
-----------------------------------------------------------------------------*/

void SDataGraph::MultiMode_SetCombined_Execute()
{
	MultiMode = EDataGraphMultiModes::Combined;
	UpdateState();
}

bool SDataGraph::MultiMode_SetCombined_CanExecute() const
{
	const bool bCanBeDisplayedAsMulti = GetFirstGraph() ? GetFirstGraph()->CombinedGraphDataSource->CanBeDisplayedAsMulti() : false;
	return MultiMode != EDataGraphMultiModes::Combined && bCanBeDisplayedAsMulti && ViewMode == EDataGraphViewModes::Time;
}

bool SDataGraph::MultiMode_SetCombined_IsChecked() const
{
	return MultiMode == EDataGraphMultiModes::Combined;
}

/*-----------------------------------------------------------------------------
	MultiMode_SetOneLinePerDataSource
-----------------------------------------------------------------------------*/

void SDataGraph::MultiMode_SetOneLinePerDataSource_Execute()
{
	MultiMode = EDataGraphMultiModes::OneLinePerDataSource;
	UpdateState();
}

bool SDataGraph::MultiMode_SetOneLinePerDataSource_CanExecute() const
{
	return MultiMode != EDataGraphMultiModes::OneLinePerDataSource;
}

bool SDataGraph::MultiMode_SetOneLinePerDataSource_IsChecked() const
{
	return MultiMode == EDataGraphMultiModes::OneLinePerDataSource;
}

void SDataGraph::EventGraph_OnRestoredFromHistory( uint32 FrameStartIndex, uint32 FrameEndIndex )
{
	UpdateState();
	// Mark the specified frames as selection and center.
	FrameIndices[0] = FrameStartIndex;
	FrameIndices[1] = FrameEndIndex-1;
	bIsLMB_SelectionDragging = false;

	const int32 FramesRange = FrameEndIndex - FrameStartIndex;
	const int32 SelectionShift = FramesRange == NumDataPoints ? 0 : (NumVisiblePoints - FramesRange) / 2;

	ScrollTo( FrameStartIndex-SelectionShift );
	OnGraphOffsetChanged.ExecuteIfBound( GraphOffset );
}

#undef LOCTEXT_NAMESPACE