// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateWidgetStyleContainerBase.h"
#include "ButtonWidgetStyle.generated.h"

/**
 */
UCLASS(hidecategories=Object, MinimalAPI)
class UButtonWidgetStyle : public USlateWidgetStyleContainerBase
{
	GENERATED_UCLASS_BODY()

public:
	/** The actual data describing the button's appearance. */
	UPROPERTY(Category=Appearance, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FButtonStyle ButtonStyle;

	virtual const struct FSlateWidgetStyle* const GetStyle() const OVERRIDE
	{
		return static_cast< const struct FSlateWidgetStyle* >( &ButtonStyle );
	}
};
