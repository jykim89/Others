// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BlackboardKeyType_Float.generated.h"

UCLASS(EditInlineNew, MinimalAPI)
class UBlackboardKeyType_Float : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	static float GetValue(const uint8* RawData);
	static bool SetValue(uint8* RawData, float Value);

	virtual FString DescribeValue(const uint8* RawData) const OVERRIDE;
	virtual EBlackboardCompare::Type Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const OVERRIDE;
	virtual bool TestArithmeticOperation(const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const OVERRIDE;
	virtual FString DescribeArithmeticParam(int32 IntValue, float FloatValue) const OVERRIDE;
};
