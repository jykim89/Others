// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SColorSpectrum.cpp: Implements the SColorSpectrum class.
=============================================================================*/

#include "SlatePrivatePCH.h"


/* SColorSpectrum methods
 *****************************************************************************/

void SColorSpectrum::Construct( const FArguments& InArgs )
{
	Image = FCoreStyle::Get().GetBrush("ColorSpectrum.Spectrum");
	SelectorImage = FCoreStyle::Get().GetBrush("ColorSpectrum.Selector");
	SelectedColor = InArgs._SelectedColor;

	OnMouseCaptureBegin = InArgs._OnMouseCaptureBegin;
	OnMouseCaptureEnd = InArgs._OnMouseCaptureEnd;
	OnValueChanged = InArgs._OnValueChanged;
}


/* SWidget overrides
 *****************************************************************************/

FVector2D SColorSpectrum::ComputeDesiredSize( ) const
{
	return Image->ImageSize;
}


FReply SColorSpectrum::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnMouseCaptureBegin.ExecuteIfBound();

		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}


FReply SColorSpectrum::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && HasMouseCapture())
	{
		OnMouseCaptureEnd.ExecuteIfBound();

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}


FReply SColorSpectrum::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (!HasMouseCapture())
	{
		return FReply::Unhandled();
	}

	FVector2D NormalizedMousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()) / MyGeometry.Size;
	NormalizedMousePosition = NormalizedMousePosition.ClampAxes(0.0f, 1.0f);

	FLinearColor NewColor = SelectedColor.Get();
	NewColor.R = 360.0f * NormalizedMousePosition.X;

	if (NormalizedMousePosition.Y > 0.5f)
	{
		NewColor.G = 1.0f;
		NewColor.B = 2.0f * (1.0f - NormalizedMousePosition.Y);
	}
	else
	{
		NewColor.G = 2.0f * NormalizedMousePosition.Y;
		NewColor.B = 1.0f;
	}

	OnValueChanged.ExecuteIfBound(NewColor);

	return FReply::Handled();
}


int32 SColorSpectrum::OnPaint( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
	const uint32 DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	
	// draw gradient
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		Image,
		MyClippingRect,
		DrawEffects,
		InWidgetStyle.GetColorAndOpacityTint() * Image->GetTint(InWidgetStyle));

	// ignore colors that can't be represented in spectrum
	const FLinearColor& Color = SelectedColor.Get();

	if ((Color.G < 1.0f) && (Color.B < 1.0f))
	{
		return LayerId;
	}

	// draw cursor
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(CalcRelativeSelectedPosition() * AllottedGeometry.Size - SelectorImage->ImageSize * 0.5f, SelectorImage->ImageSize),
		SelectorImage,
		MyClippingRect,
		DrawEffects,
		InWidgetStyle.GetColorAndOpacityTint() * SelectorImage->GetTint(InWidgetStyle));

	return LayerId + 1;
}


/* SColorSpectrum implementation
 *****************************************************************************/

FVector2D SColorSpectrum::CalcRelativeSelectedPosition( ) const
{
	const FLinearColor& Color = SelectedColor.Get();

	if (Color.G == 1.0f)
	{
		return FVector2D(Color.R / 360.0f, 1.0f - 0.5f * Color.B);
	}

	return FVector2D(Color.R / 360.0f, 0.5f * Color.G);
}
