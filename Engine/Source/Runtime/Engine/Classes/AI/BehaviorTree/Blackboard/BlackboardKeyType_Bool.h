// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BlackboardKeyType_Bool.generated.h"

UCLASS(EditInlineNew, MinimalAPI)
class UBlackboardKeyType_Bool : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	static bool GetValue(const uint8* RawData);
	static bool SetValue(uint8* RawData, bool bValue);

	virtual FString DescribeValue(const uint8* RawData) const OVERRIDE;
	virtual EBlackboardCompare::Type Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const OVERRIDE;
	virtual bool TestBasicOperation(const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const OVERRIDE;
};
