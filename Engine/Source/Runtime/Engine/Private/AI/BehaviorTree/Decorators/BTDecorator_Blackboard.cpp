// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

UBTDecorator_Blackboard::UBTDecorator_Blackboard(const class FPostConstructInitializeProperties& PCIP) : Super(PCIP)
{
	NodeName = "Blackboard";
}

bool UBTDecorator_Blackboard::CalculateRawConditionValue(class UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) const
{
	const UBlackboardComponent* BlackboardComp = OwnerComp->GetBlackboardComponent();
	return EvaluateOnBlackboard(BlackboardComp);
}

bool UBTDecorator_Blackboard::EvaluateOnBlackboard(const UBlackboardComponent* BlackboardComp) const
{
	bool bResult = false;
	if (BlackboardComp && BlackboardKey.SelectedKeyType)
	{
		UBlackboardKeyType* KeyCDO = BlackboardKey.SelectedKeyType->GetDefaultObject<UBlackboardKeyType>();
		const uint8* KeyMemory = BlackboardComp->GetKeyRawData(BlackboardKey.GetSelectedKeyID());

		const EBlackboardKeyOperation::Type Op = KeyCDO->GetTestOperation();
		switch (Op)
		{
		case EBlackboardKeyOperation::Basic:		bResult = KeyCDO->TestBasicOperation(KeyMemory, (EBasicKeyOperation::Type)OperationType); break;
		case EBlackboardKeyOperation::Arithmetic:	bResult = KeyCDO->TestArithmeticOperation(KeyMemory, (EArithmeticKeyOperation::Type)OperationType, IntValue, FloatValue); break;
		case EBlackboardKeyOperation::Text:			bResult = KeyCDO->TestTextOperation(KeyMemory, (ETextKeyOperation::Type)OperationType, StringValue); break;
		default: break;
		}
	}

	return bResult;
}

void UBTDecorator_Blackboard::OnBlackboardChange(const class UBlackboardComponent* Blackboard, uint8 ChangedKeyID)
{
	UBehaviorTreeComponent* BehaviorComp = Blackboard ? (UBehaviorTreeComponent*)Blackboard->GetBrainComponent() : NULL;
	if (BlackboardKey.GetSelectedKeyID() == ChangedKeyID && BehaviorComp)
	{
		const bool bIsExecutingBranch = BehaviorComp->IsExecutingBranch(this, GetChildIndex());
		const bool bPass = EvaluateOnBlackboard(Blackboard);

		UE_VLOG(BehaviorComp->GetOwner(), LogBehaviorTree, Verbose, TEXT("%s, OnBlackboardChange[%s] pass:%d executing:%d => %s"),
			*UBehaviorTreeTypes::DescribeNodeHelper(this),
			*Blackboard->GetKeyName(ChangedKeyID).ToString(), bPass, bIsExecutingBranch,
			(bIsExecutingBranch && !bPass) || (!bIsExecutingBranch && bPass) ? TEXT("restart") : TEXT("skip"));

		if ((bIsExecutingBranch && !bPass) || 
			(!bIsExecutingBranch && bPass))
		{
			BehaviorComp->RequestExecution(this);
		}
	}
}

void UBTDecorator_Blackboard::DescribeRuntimeValues(const class UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);

	const UBlackboardComponent* BlackboardComp = OwnerComp->GetBlackboardComponent();
	FString DescKeyValue;

	if (BlackboardComp)
	{
		DescKeyValue = BlackboardComp->DescribeKeyValue(BlackboardKey.GetSelectedKeyID(), EBlackboardDescription::OnlyValue);
	}

	const bool bResult = EvaluateOnBlackboard(BlackboardComp);
	Values.Add(FString::Printf(TEXT("value: %s (%s)"), *DescKeyValue, bResult ? TEXT("pass") : TEXT("fail")));
}

FString UBTDecorator_Blackboard::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: %s"), *Super::GetStaticDescription(), *CachedDescription);
}

#if WITH_EDITOR
static UEnum* BasicOpEnum = NULL;
static UEnum* ArithmeticOpEnum = NULL;
static UEnum* TextOpEnum = NULL;

static void CacheOperationEnums()
{
	if (BasicOpEnum == NULL)
	{
		BasicOpEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EBasicKeyOperation"));
		check(BasicOpEnum);
	}

	if (ArithmeticOpEnum == NULL)
	{
		ArithmeticOpEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EArithmeticKeyOperation"));
		check(ArithmeticOpEnum);
	}

	if (TextOpEnum == NULL)
	{
		TextOpEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ETextKeyOperation"));
		check(TextOpEnum);
	}
}

void UBTDecorator_Blackboard::BuildDescription()
{
	UBlackboardData* BlackboardAsset = GetBlackboardAsset();
	const FBlackboardEntry* EntryInfo = BlackboardAsset ? BlackboardAsset->GetKey(BlackboardKey.GetSelectedKeyID()) : NULL;

	FString BlackboardDesc = "invalid";
	if (EntryInfo)
	{
		// safety feature to not crash when changing couple of properties on a bunch
		// while "post edit property" triggers for every each of them
		if (EntryInfo->KeyType->GetClass() == BlackboardKey.SelectedKeyType)
		{
			const FString KeyName = EntryInfo->EntryName.ToString();
			CacheOperationEnums();		

			const EBlackboardKeyOperation::Type Op = EntryInfo->KeyType->GetTestOperation();
			switch (Op)
			{
			case EBlackboardKeyOperation::Basic:
				BlackboardDesc = FString::Printf(TEXT("%s is %s"), *KeyName, *BasicOpEnum->GetEnumName(OperationType));
				break;

			case EBlackboardKeyOperation::Arithmetic:
				BlackboardDesc = FString::Printf(TEXT("%s %s %s"), *KeyName, *ArithmeticOpEnum->GetDisplayNameText(OperationType).ToString(),
					*EntryInfo->KeyType->DescribeArithmeticParam(IntValue, FloatValue));
				break;

			case EBlackboardKeyOperation::Text:
				BlackboardDesc = FString::Printf(TEXT("%s %s [%s]"), *KeyName, *TextOpEnum->GetEnumName(OperationType), *StringValue);
				break;

			default: break;
			}
		}
	}

	CachedDescription = BlackboardDesc;
}

void UBTDecorator_Blackboard::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property == NULL)
	{
		return;
	}

	const FName ChangedPropName = PropertyChangedEvent.Property->GetFName();
	if (ChangedPropName == GET_MEMBER_NAME_CHECKED(UBTDecorator_Blackboard, BlackboardKey.SelectedKeyName))
	{
		if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Enum::StaticClass() ||
			BlackboardKey.SelectedKeyType == UBlackboardKeyType_NativeEnum::StaticClass())
		{
			IntValue = 0;
		}
	}

#if WITH_EDITORONLY_DATA

	UBlackboardKeyType* KeyCDO = BlackboardKey.SelectedKeyType ? BlackboardKey.SelectedKeyType->GetDefaultObject<UBlackboardKeyType>() : NULL;
	if (ChangedPropName == GET_MEMBER_NAME_CHECKED(UBTDecorator_Blackboard, BasicOperation))
	{
		if (KeyCDO && KeyCDO->GetTestOperation() == EBlackboardKeyOperation::Basic)
		{
			OperationType = BasicOperation;
		}
	}
	else if (ChangedPropName == GET_MEMBER_NAME_CHECKED(UBTDecorator_Blackboard, ArithmeticOperation))
	{
		if (KeyCDO && KeyCDO->GetTestOperation() == EBlackboardKeyOperation::Arithmetic)
		{
			OperationType = ArithmeticOperation;
		}
	}
	else if (ChangedPropName == GET_MEMBER_NAME_CHECKED(UBTDecorator_Blackboard, TextOperation))
	{
		if (KeyCDO && KeyCDO->GetTestOperation() == EBlackboardKeyOperation::Text)
		{
			OperationType = TextOperation;
		}
	}

#endif // WITH_EDITORONLY_DATA

	BuildDescription();
}

void UBTDecorator_Blackboard::InitializeFromAsset(class UBehaviorTree* Asset)
{
	Super::InitializeFromAsset(Asset);
	BuildDescription();
}

#endif	// WITH_EDITOR
