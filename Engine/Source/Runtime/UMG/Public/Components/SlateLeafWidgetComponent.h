// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateLeafWidgetComponent.generated.h"

/** Leaf widget that cannot have any children */
UCLASS(Abstract)
class UMG_API USlateLeafWidgetComponent: public USlateWrapperComponent
{
	GENERATED_UCLASS_BODY()

	// USceneComponent interface
	virtual bool CanAttachAsChild(USceneComponent* ChildComponent, FName SocketName) const;
	// End of USceneComponent interface
};
