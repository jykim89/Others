// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "EnvQueryItemType_Direction.generated.h"

UCLASS()
class ENGINE_API UEnvQueryItemType_Direction : public UEnvQueryItemType_VectorBase
{
	GENERATED_UCLASS_BODY()

	static FVector GetValue(const uint8* RawData);
	static void SetValue(uint8* RawData, const FVector& Value);

	static FRotator GetValueRot(const uint8* RawData);
	static void SetValueRot(uint8* RawData, const FRotator& Value);

	static void SetContextHelper(struct FEnvQueryContextData& ContextData, const FVector& SingleDirection);
	static void SetContextHelper(struct FEnvQueryContextData& ContextData, const FRotator& SingleRotation);
	static void SetContextHelper(struct FEnvQueryContextData& ContextData, const TArray<FVector>& MultipleDirections);
	static void SetContextHelper(struct FEnvQueryContextData& ContextData, const TArray<FRotator>& MultipleRotations);

	virtual FRotator GetRotation(const uint8* RawData) const OVERRIDE;
	virtual FString GetDescription(const uint8* RawData) const OVERRIDE;
};
