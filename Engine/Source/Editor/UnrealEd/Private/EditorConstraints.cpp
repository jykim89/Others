// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorConstraints.cpp: Editor movement constraints.
=============================================================================*/

#include "UnrealEd.h"


float UEditorEngine::GetGridSize()
{
	const TArray<float> PosGridSizes = GetCurrentPositionGridArray();
	const int32 CurrentPosGridSize = GetDefault<ULevelEditorViewportSettings>()->CurrentPosGridSize;
	float PosVal = 0.0001f;
	if ( PosGridSizes.IsValidIndex(CurrentPosGridSize) )
	{
		PosVal = PosGridSizes[CurrentPosGridSize];
	}
	return PosVal;
}

bool UEditorEngine::IsGridSizePowerOfTwo() const
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();

	return ViewportSettings->bUsePowerOf2SnapSize;
}

void UEditorEngine::SetGridSize( int32 InIndex )
{
	FinishAllSnaps();

	ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	ViewportSettings->CurrentPosGridSize = FMath::Clamp<int32>( InIndex, 0, GetCurrentPositionGridArray().Num() - 1 );
	ViewportSettings->PostEditChange();

	FEditorDelegates::OnGridSnappingChanged.Broadcast(GetDefault<ULevelEditorViewportSettings>()->GridEnabled, GetGridSize());
	
	RedrawLevelEditingViewports();
	FEditorSupportDelegates::UpdateUI.Broadcast();
}

void UEditorEngine::GridSizeIncrement( )
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	SetGridSize(ViewportSettings->CurrentPosGridSize + 1);
}

void UEditorEngine::GridSizeDecrement( )
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	SetGridSize(ViewportSettings->CurrentPosGridSize - 1);
}

const TArray<float>& UEditorEngine::GetCurrentPositionGridArray() const
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();

	return (ViewportSettings->bUsePowerOf2SnapSize) ?
			ViewportSettings->Pow2GridSizes :
			ViewportSettings->DecimalGridSizes;
}

FRotator UEditorEngine::GetRotGridSize()
{
	const TArray<float> RotGridSizes = GetCurrentRotationGridArray();
	const int32 CurrentRotGridSize = GetDefault<ULevelEditorViewportSettings>()->CurrentRotGridSize;
	float RotVal = 0.0001f;
	if ( RotGridSizes.IsValidIndex(CurrentRotGridSize) )
	{
		RotVal = RotGridSizes[CurrentRotGridSize];
	}
	return FRotator(RotVal, RotVal, RotVal);
}

void UEditorEngine::SetRotGridSize( int32 InIndex, ERotationGridMode InGridMode )
{
	FinishAllSnaps();

	const TArray<float> RotGridSizes = GetCurrentRotationGridArray();

	ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	ViewportSettings->CurrentRotGridMode = InGridMode;
	ViewportSettings->CurrentRotGridSize = FMath::Clamp<int32>( InIndex, 0, RotGridSizes.Num() - 1 );
	ViewportSettings->PostEditChange();
	
	RedrawLevelEditingViewports();
	FEditorSupportDelegates::UpdateUI.Broadcast();
}

void UEditorEngine::RotGridSizeIncrement( )
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	SetRotGridSize(ViewportSettings->CurrentRotGridSize + 1, ViewportSettings->CurrentRotGridMode );
}

void UEditorEngine::RotGridSizeDecrement( )
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	SetRotGridSize(ViewportSettings->CurrentRotGridSize - 1, ViewportSettings->CurrentRotGridMode );
}

const TArray<float>& UEditorEngine::GetCurrentRotationGridArray() const
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();

	return (ViewportSettings->CurrentRotGridMode == GridMode_Common) ?
			ViewportSettings->CommonRotGridSizes :
			ViewportSettings->DivisionsOf360RotGridSizes;
}

float UEditorEngine::GetScaleGridSize()
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	float ScaleVal = 0.0001f;
	if ( ViewportSettings->ScalingGridSizes.IsValidIndex(ViewportSettings->CurrentScalingGridSize) )
	{
		ScaleVal = ViewportSettings->ScalingGridSizes[ViewportSettings->CurrentScalingGridSize];
	}
	return ScaleVal;
}

void UEditorEngine::SetScaleGridSize( int32 InIndex )
{
	FinishAllSnaps();

	ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	ViewportSettings->CurrentScalingGridSize = FMath::Clamp<int32>( InIndex, 0, ViewportSettings->ScalingGridSizes.Num() - 1 );
	ViewportSettings->PostEditChange();

	RedrawLevelEditingViewports();
	FEditorSupportDelegates::UpdateUI.Broadcast();
}

float UEditorEngine::GetGridInterval()
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	const TArray<float>& GridIntervals = ViewportSettings->bUsePowerOf2SnapSize ? ViewportSettings->Pow2GridIntervals : ViewportSettings->DecimalGridIntervals;

	int32 CurrentIndex = (ViewportSettings->CurrentPosGridSize < GridIntervals.Num()) ? ViewportSettings->CurrentPosGridSize : GridIntervals.Num() - 1;

	return GridIntervals[CurrentIndex];
}


