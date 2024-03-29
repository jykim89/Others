// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Slate.h"
#include "EditorStyle.h"
#include "SRealtimeProfilerLineGraph.h"
#include "SRealtimeProfilerVisualizer.h"
#include "RealtimeProfiler.h"

//////////////////////////////////////////////////////////////////////////
// SRealtimeProfilerLineGraph

void SRealtimeProfilerLineGraph::Construct(const FArguments& InArgs)
{
	MaxValue = InArgs._MaxValue;
	MaxFrames = InArgs._MaxFrames;
	OnGeometryChanged = InArgs._OnGeometryChanged;
	Zoom = 1.0f;
	Offset = 0.0f;
	bIsProfiling = false;
	Visualizer = InArgs._Visualizer;
	bDisplayFPSChart = false;

	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Top)
	[
		SNew(SHorizontalBox)

		//START
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(StartButton,SButton)
			.ToolTipText(NSLOCTEXT("RealtimeProfileLineGraph", "StartProfilingButton", "Start"))
			.OnClicked(this, &SRealtimeProfilerLineGraph::OnStartButtonDown)
			.ContentPadding(1)
			.Visibility(this, &SRealtimeProfilerLineGraph::GetStartButtonVisibility)
			[
				SNew(SImage) 
				.Image( FEditorStyle::GetBrush("Profiler.Start") ) 
			]
		]

		//PAUSE
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(PauseButton,SButton)
			.ToolTipText(NSLOCTEXT("RealtimeProfileLineGraph", "PauseProfilingButton", "Pause"))
			.OnClicked(this, &SRealtimeProfilerLineGraph::OnPauseButtonDown)
			.ContentPadding(1)
			.Visibility(this, &SRealtimeProfilerLineGraph::GetPauseButtonVisibility)
			[
				SNew(SImage) 
				.Image( FEditorStyle::GetBrush("Profiler.Pause") ) 
			]
		]

		//STOP
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(NSLOCTEXT("RealtimeProfileLineGraph", "StopProfilingButton", "Stop"))
			.OnClicked(this, &SRealtimeProfilerLineGraph::OnStopButtonDown)
			.ContentPadding(1)
			[
				SNew(SImage) 
				.Image( FEditorStyle::GetBrush("Profiler.Stop") ) 
			]
		]

		//SWITCH GRAPH VIEW
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(NSLOCTEXT("RealtimeProfileLineGraph", "SwitchProfilingViewButton", "Switch View"))
			.OnClicked(this, &SRealtimeProfilerLineGraph::OnSwitchViewButtonDown)
			.ContentPadding(1)
			[
				SNew(SImage) 
				.Image( FEditorStyle::GetBrush("Profiler.SwitchView") ) 
			]
		]

	];

	
}

FVector2D SRealtimeProfilerLineGraph::ComputeDesiredSize() const
{
	return FVector2D(128,64);
}


//input X,Y are 0.0 to 1.0 local values of the geometry,
//outputs x,y in screen space

FVector2D SRealtimeProfilerLineGraph::GetWidgetPosition(float X, float Y, const FGeometry& Geom) const
{
	return FVector2D( (X*Geom.Size.X) , (Geom.Size.Y-1) - (Y*Geom.Size.Y) );
}


int32 SRealtimeProfilerLineGraph::OnPaint( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// Rendering info
	bool bEnabled = ShouldBeEnabled( bParentEnabled );
	ESlateDrawEffect::Type DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FSlateBrush* TimelineAreaBrush = FEditorStyle::GetBrush("Profiler.LineGraphArea");
	const FSlateBrush* WhiteBrush = FEditorStyle::GetBrush("WhiteTexture");


	// Draw timeline background

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry( FVector2D(0,0), FVector2D(AllottedGeometry.Size.X,AllottedGeometry.Size.Y) ),
		TimelineAreaBrush,
		MyClippingRect,
		DrawEffects,
		TimelineAreaBrush->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint()
	);

	LayerId++;


	//Draw axies:

	TArray<FVector2D> AxisPoints;
	AxisPoints.Add(GetWidgetPosition(0, 1, AllottedGeometry));
	AxisPoints.Add(GetWidgetPosition(0, 0, AllottedGeometry));
	AxisPoints.Add(GetWidgetPosition(1, 0, AllottedGeometry));

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		AxisPoints,
		MyClippingRect,
		DrawEffects,
		WhiteBrush->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint()
		);

	LayerId++;




	//Draw line graph:

	TArray<FVector2D> LinePoints;

	//FPS Chart points:
	TArray<FVector2D> UnitFramePoints;
	TArray<FVector2D> UnitRenderPoints;
	TArray<FVector2D> UnitGamePoints;
	TArray<FVector2D> UnitGPUPoints;

	float PixelDistanceBetweenPoints = ((float)AllottedGeometry.Size.X/MaxFrames.Get())*Zoom;
	int32 NumPointsToDraw = (AllottedGeometry.Size.X/PixelDistanceBetweenPoints)+2;

	//Convert Offset to FrameOffset
	float FrameOffset = (-Offset/Zoom)*MaxFrames.Get();

	int32 StartPointIndex = FMath::FloorToInt( FrameOffset );

	if(StartPointIndex <= 0)
		StartPointIndex = 0;

	int32 EndPointIndex = StartPointIndex + NumPointsToDraw - 1;

	for(int i=StartPointIndex; i<=EndPointIndex; ++i)
	{
		if(i>=ProfileDataArray.Num())
			break;

		float XPos = i*PixelDistanceBetweenPoints - FrameOffset*PixelDistanceBetweenPoints;
		float YPos;

		if(!bDisplayFPSChart)
		{
			YPos = ProfileDataArray.GetTypedData()[i]->DurationMs/MaxValue.Get();
			YPos = (AllottedGeometry.Size.Y-1) - (YPos*AllottedGeometry.Size.Y);
			LinePoints.Add(FVector2D((int32)XPos,(int32)YPos));
		}
		else
		{
			YPos = FPSChartDataArray.GetTypedData()[i].UnitFrame/MaxValue.Get();
			YPos = (AllottedGeometry.Size.Y-1) - (YPos*AllottedGeometry.Size.Y);
			UnitFramePoints.Add(FVector2D((int32)XPos,(int32)YPos));

			YPos = FPSChartDataArray.GetTypedData()[i].UnitRender/MaxValue.Get();
			YPos = (AllottedGeometry.Size.Y-1) - (YPos*AllottedGeometry.Size.Y);
			UnitRenderPoints.Add(FVector2D((int32)XPos,(int32)YPos));

			YPos = FPSChartDataArray.GetTypedData()[i].UnitGame/MaxValue.Get();
			YPos = (AllottedGeometry.Size.Y-1) - (YPos*AllottedGeometry.Size.Y);
			UnitGamePoints.Add(FVector2D((int32)XPos,(int32)YPos));

			YPos = FPSChartDataArray.GetTypedData()[i].UnitGPU/MaxValue.Get();
			YPos = (AllottedGeometry.Size.Y-1) - (YPos*AllottedGeometry.Size.Y);
			UnitGPUPoints.Add(FVector2D((int32)XPos,(int32)YPos));
		}

	}

	if(!bDisplayFPSChart)
	{
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			MyClippingRect,
			DrawEffects,
			InWidgetStyle.GetColorAndOpacityTint() * FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)
			);

		LayerId++;
	}
	else
	{
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			UnitFramePoints,
			MyClippingRect,
			DrawEffects,
			InWidgetStyle.GetColorAndOpacityTint() * FLinearColor(0.0f, 1.0f, 0.0f, 1.0f)
			);

		LayerId++;

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			UnitRenderPoints,
			MyClippingRect,
			DrawEffects,
			InWidgetStyle.GetColorAndOpacityTint() * FLinearColor(0.0f, 0.0f, 1.0f, 1.0f)
			);

		LayerId++;

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			UnitGamePoints,
			MyClippingRect,
			DrawEffects,
			InWidgetStyle.GetColorAndOpacityTint() * FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)
			);

		LayerId++;

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			UnitGPUPoints,
			MyClippingRect,
			DrawEffects,
			InWidgetStyle.GetColorAndOpacityTint() * FLinearColor(1.0f, 1.0f, 0.0f, 1.0f)
			);

		LayerId++;
	}





	//Draw 30FPS line:
	TArray<FVector2D> FPS30LinePoints;
	FPS30LinePoints.Add(GetWidgetPosition(0, 33.3333/MaxValue.Get(), AllottedGeometry));
	FPS30LinePoints.Add(GetWidgetPosition(1, 33.3333/MaxValue.Get(), AllottedGeometry));

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		FPS30LinePoints,
		MyClippingRect,
		DrawEffects,
		WhiteBrush->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint()
		);

	LayerId++;

	//Draw 60FPS line:
	TArray<FVector2D> FPS60LinePoints;
	FPS60LinePoints.Add(GetWidgetPosition(0, 16.6666/MaxValue.Get(), AllottedGeometry));
	FPS60LinePoints.Add(GetWidgetPosition(1, 16.6666/MaxValue.Get(), AllottedGeometry));

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		FPS60LinePoints,
		MyClippingRect,
		DrawEffects,
		WhiteBrush->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint()
		);

	LayerId++;

	//Draw mouse cursor:
	TArray<FVector2D> MouseCursorPoints;
	MouseCursorPoints.Add(FVector2D(MousePosition.X, 0));
	MouseCursorPoints.Add(FVector2D(MousePosition.X, AllottedGeometry.Size.Y));

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		MouseCursorPoints,
		MyClippingRect,
		DrawEffects,
		WhiteBrush->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint()
		);

	LayerId++;

	// Paint children
	SCompoundWidget::OnPaint(AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	return LayerId;
}



void SRealtimeProfilerLineGraph::AppendData(TSharedPtr< FVisualizerEvent > ProfileData, FRealtimeProfilerFPSChartFrame * InFPSChartFrame)
{
	if(bIsProfiling)
	{
		if(ProfileDataArray.Num() > MaxFrames.Get())
		{
			ProfileDataArray.RemoveAt(0);
			FPSChartDataArray.RemoveAt(0);
		}

		ProfileDataArray.Add(ProfileData);
		FPSChartDataArray.Add(FRealtimeProfilerFPSChartFrame(InFPSChartFrame));
	}
}

int32 SRealtimeProfilerLineGraph::GetMaxFrames() const
{
	return MaxFrames.Get();
}


void SRealtimeProfilerLineGraph::DisplayFrameDetailAtMouse(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	FVector2D pressedLocation = InMyGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

	float PixelDistanceBetweenPoints = ((float)InMyGeometry.Size.X/MaxFrames.Get())*Zoom;
	float FrameOffset = (-Offset/Zoom)*MaxFrames.Get();
	int32 FrameIndex = (pressedLocation.X/PixelDistanceBetweenPoints)+FrameOffset;

	if(0 <= FrameIndex && FrameIndex < ProfileDataArray.Num())
	{
		TSharedPtr< FVisualizerEvent > SelectedData = ProfileDataArray.GetTypedData()[FrameIndex];
		Visualizer.Get()->DisplayFrameDetails(SelectedData);
	}
}

FReply SRealtimeProfilerLineGraph::OnMouseButtonDown( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	bIsLeftMouseButtonDown = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;

	if(bIsLeftMouseButtonDown)
	{		
		DisplayFrameDetailAtMouse(InMyGeometry,InMouseEvent);
	}

	return FReply::Handled();
}

FReply SRealtimeProfilerLineGraph::OnMouseButtonUp( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	bIsLeftMouseButtonDown = !(InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton);

	return FReply::Handled();
}

FReply SRealtimeProfilerLineGraph::OnMouseMove( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{	
	MousePosition = InMyGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

	if(bIsLeftMouseButtonDown)
	{		
		DisplayFrameDetailAtMouse(InMyGeometry,InMouseEvent);
	}
	return FReply::Handled();
}


void SRealtimeProfilerLineGraph::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if( OnGeometryChanged.IsBound() )
	{
		if( AllottedGeometry != this->LastGeometry )
		{
			OnGeometryChanged.ExecuteIfBound( AllottedGeometry );
			LastGeometry = AllottedGeometry;
		}
	}

	SWidget::Tick( AllottedGeometry, InCurrentTime, InDeltaTime );
}

FReply SRealtimeProfilerLineGraph::OnStartButtonDown()
{
	bIsProfiling = true;
	return FReply::Handled();
}

FReply SRealtimeProfilerLineGraph::OnPauseButtonDown()
{
	bIsProfiling = false;
	return FReply::Handled();
}

FReply SRealtimeProfilerLineGraph::OnStopButtonDown()
{
	ProfileDataArray.Reset(MaxFrames.Get());
	FPSChartDataArray.Reset(MaxFrames.Get());
	bIsProfiling = false;
	return FReply::Handled();
}

FReply SRealtimeProfilerLineGraph::OnSwitchViewButtonDown()
{
	bDisplayFPSChart = !bDisplayFPSChart;
	return FReply::Handled();
}

EVisibility SRealtimeProfilerLineGraph::GetStartButtonVisibility() const
{
	return (bIsProfiling) ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SRealtimeProfilerLineGraph::GetPauseButtonVisibility() const
{
	return (bIsProfiling) ? EVisibility::Visible : EVisibility::Collapsed;
}