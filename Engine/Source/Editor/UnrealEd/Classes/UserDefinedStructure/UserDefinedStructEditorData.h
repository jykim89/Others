// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UserDefinedStructEditorData.generated.h"

USTRUCT()
struct FStructVariableDescription
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName VarName;

	UPROPERTY()
	FGuid VarGuid;

	UPROPERTY()
	FString FriendlyName;

	UPROPERTY()
	FString DefaultValue;

	// TYPE DATA
	UPROPERTY()
	FString Category;

	UPROPERTY()
	FString SubCategory;

	UPROPERTY()
	TAssetPtr<UObject> SubCategoryObject;

	UPROPERTY()
	bool bIsArray;

	UPROPERTY(Transient)
	bool bInvalidMember;

	// CurrentDefaultValue stores the actual default value, after the DefaultValue was changed, and before the struct was recompiled
	UPROPERTY()
	FString CurrentDefaultValue;

	UPROPERTY()
	FString ToolTip;

	UNREALED_API bool SetPinType(const struct FEdGraphPinType& VarType);

	UNREALED_API FEdGraphPinType ToPinType() const;
};

UCLASS()
class UNREALED_API UUserDefinedStructEditorData : public UObject
{
	GENERATED_UCLASS_BODY()

private:
	// the property is used to generate an uniqe name id for member variable
	UPROPERTY(NonTransactional) 
	uint32 UniqueNameId;

public:
	UPROPERTY()
	TArray<FStructVariableDescription> VariablesDescriptions;

	UPROPERTY()
	FString ToolTip;

	// UObject interface.
	virtual void PostEditUndo() OVERRIDE;
	virtual void PostLoadSubobjects(struct FObjectInstancingGraph* OuterInstanceGraph) OVERRIDE;
	// End of UObject interface.

	uint32 GenerateUniqueNameIdForMemberVariable();
	class UUserDefinedStruct* GetOwnerStruct() const;
};