// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SColorWheel.cpp: Implements the SColorWheel class.
=============================================================================*/

#include "SlatePrivatePCH.h"


/* SColorWheel methods
 *****************************************************************************/

void SColorWheel::Construct( const FArguments& InArgs )
{
	Image = FCoreStyle::Get().GetBrush("ColorWheel.HueValueCircle");
	SelectorImage = FCoreStyle::Get().GetBrush("ColorWheel.Selector");
	SelectedColor = InArgs._SelectedColor;

	OnMouseCaptureBegin = InArgs._OnMouseCaptureBegin;
	OnMouseCaptureEnd = InArgs._OnMouseCaptureEnd;
	OnValueChanged = InArgs._OnValueChanged;
}


/* SWidget overrides
 *****************************************************************************/

FVector2D SColorWheel::ComputeDesiredSize( ) const
{
	return Image->ImageSize + SelectorImage->ImageSize;
}


FReply SColorWheel::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FVector2D LocalMouseCoordinate = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition());
		FVector2D Location = LocalMouseCoordinate / (MyGeometry.Size * 0.5f) - FVector2D(1.0f, 1.0f);
		float Radius = Location.Size();
	
		if (Radius > 1.f)
		{
			return FReply::Unhandled();
		}

		OnMouseCaptureBegin.ExecuteIfBound();

		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	else
	{
		return FReply::Unhandled();
	}
}


FReply SColorWheel::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HasMouseCapture())
	{
		OnMouseCaptureEnd.ExecuteIfBound();

		return FReply::Handled().ReleaseMouseCapture();
	}
	else
	{
		return FReply::Unhandled();
	}
}


FReply SColorWheel::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (!HasMouseCapture())
	{
		return FReply::Unhandled();
	}

	FVector2D LocalMouseCoordinate = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	FVector2D Location = LocalMouseCoordinate / (MyGeometry.Size * 0.5f) - FVector2D(1.0f, 1.0f);
	float Angle = FMath::Atan2(Location.Y, Location.X);
	float Radius = FMath::Min(Location.Size(), 1.0f);
	
	if (Angle < 0.0f)
	{
		Angle += 2.0f * PI;
	}

	FLinearColor NewColor = SelectedColor.Get();
	NewColor.R = Angle * 180.0f * INV_PI;
	NewColor.G = Radius;

	OnValueChanged.ExecuteIfBound(NewColor);

	return FReply::Handled();
}


int32 SColorWheel::OnPaint( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
	const uint32 DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		Image,
		MyClippingRect,
		DrawEffects,
		InWidgetStyle.GetColorAndOpacityTint() * Image->GetTint(InWidgetStyle));

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(CalcRelativeSelectedPosition() * AllottedGeometry.Size * 0.5f - SelectorImage->ImageSize * 0.5f, SelectorImage->ImageSize),
		SelectorImage,
		MyClippingRect,
		DrawEffects,
		InWidgetStyle.GetColorAndOpacityTint() * SelectorImage->GetTint(InWidgetStyle));

	return LayerId + 1;
}


/* SColorWheel implementation
 *****************************************************************************/

FVector2D SColorWheel::CalcRelativeSelectedPosition( ) const
{
	float Hue = SelectedColor.Get().R;
	float Saturation = SelectedColor.Get().G;
	float Angle = Hue / 180.0f * PI;
	float Radius = Saturation;

	return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius + FVector2D(1.0f, 1.0f);
}
