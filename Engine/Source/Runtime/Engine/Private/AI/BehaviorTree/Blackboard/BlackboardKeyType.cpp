// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

UBlackboardKeyType::UBlackboardKeyType(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	ValueSize = 0;
	SupportedOp = EBlackboardKeyOperation::Basic;
}

FString UBlackboardKeyType::DescribeValue(const uint8* RawData) const
{
	FString DescBytes;
	for (int32 i = 0; i < ValueSize; i++)
	{
		DescBytes += FString::Printf(TEXT("%X"), RawData[i]);
	}

	return DescBytes.Len() ? (FString("0x") + DescBytes) : FString("empty");
}

FString UBlackboardKeyType::DescribeSelf() const
{
	return FString();
};

void UBlackboardKeyType::Initialize(uint8* MemoryBlock) const
{
	// empty in base class
}

bool UBlackboardKeyType::IsAllowedByFilter(UBlackboardKeyType* FilterOb) const
{
	return GetClass() == (FilterOb ? FilterOb->GetClass() : NULL);
}

bool UBlackboardKeyType::GetLocation(const uint8* RawData, FVector& Location) const
{
	return false;
}

bool UBlackboardKeyType::GetRotation(const uint8* MemoryBlock, FRotator& Rotation) const
{
	return false;
}

EBlackboardCompare::Type UBlackboardKeyType::Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const
{ 
	return MemoryBlockA == MemoryBlockB ? EBlackboardCompare::Equal : EBlackboardCompare::NotEqual;
}

bool UBlackboardKeyType::TestBasicOperation(const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const
{
	return false;
}

bool UBlackboardKeyType::TestArithmeticOperation(const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const
{
	return false;
}

bool UBlackboardKeyType::TestTextOperation(const uint8* MemoryBlock, ETextKeyOperation::Type Op, const FString& OtherString) const
{
	return false;
}

FString UBlackboardKeyType::DescribeArithmeticParam(int32 IntValue, float FloatValue) const
{
	return FString();
}
