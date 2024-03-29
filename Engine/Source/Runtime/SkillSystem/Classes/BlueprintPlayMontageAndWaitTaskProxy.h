// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BlueprintPlayMontageAndWaitTaskProxy.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMontageWaitSimpleDelegate);

UCLASS(MinimalAPI)
class UBlueprintPlayMontageAndWaitTaskProxy : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FMontageWaitSimpleDelegate	OnComplete;

	UPROPERTY(BlueprintAssignable)
	FMontageWaitSimpleDelegate	OnInterrupted;

	UFUNCTION()
	void	OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

public:
	
};