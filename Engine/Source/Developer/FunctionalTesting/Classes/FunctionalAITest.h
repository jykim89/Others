// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AI/AITypes.h"
#include "FunctionalAITest.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFunctionalTestAISpawned, AAIController*, Controller, APawn*, Pawn);

USTRUCT()
struct FAITestSpawnInfo
{
	GENERATED_USTRUCT_BODY()

	/** Determines AI to be spawned */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	TSubclassOf<class APawn>  PawnClass;
	
	/** class to override default pawn's controller class. If None the default will be used*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	TSubclassOf<class AAIController>  ControllerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	FGenericTeamId TeamID;

	/** if set will be applied to spawned AI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	UBehaviorTree* BehaviorTree;

	/** Where should AI be spawned */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	AActor* SpawnLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn, meta=(UIMin=1, ClampMin=1))
	int32 NumberToSpawn;

	FAITestSpawnInfo() : NumberToSpawn(1)
	{}

	FORCEINLINE bool IsValid() const { return PawnClass != NULL && SpawnLocation != NULL; }
};

USTRUCT()
struct FAITestSpawnSet
{
	GENERATED_USTRUCT_BODY()

	/** what to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	TArray<FAITestSpawnInfo> SpawnInfoContainer;

	/** give the set a name to help identify it if need be */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	uint32 bEnabled:1;

	/** location used for spawning if spawn info doesn't define one */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AISpawn)
	AActor* FallbackSpawnLocation;

	FAITestSpawnSet() : bEnabled(true)
	{}
};

UCLASS(Blueprintable, MinimalAPI)
class AFunctionalAITest : public AFunctionalTest
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AITest)
	TArray<FAITestSpawnSet> SpawnSets;

	UPROPERTY(BlueprintReadOnly, Category=AITest)
	TArray<APawn*> SpawnedPawns;

	int32 CurrentSpawnSetIndex;
	FString CurrentSpawnSetName;

	/** Called when a single AI finished spawning */
	UPROPERTY(BlueprintAssignable)
	FFunctionalTestAISpawned OnAISpawned;

	/** Called when a all AI finished spawning */
	UPROPERTY(BlueprintAssignable)
	FFunctionalTestEventSignature OnAllAISPawned;
		
	UFUNCTION(BlueprintCallable, Category = "Development")
	virtual bool IsOneOfSpawnedPawns(AActor* Actor);

	virtual void BeginPlay() OVERRIDE;

	virtual bool StartTest() OVERRIDE;
	virtual void FinishTest(TEnumAsByte<EFunctionalTestResult::Type> TestResult, const FString& Message) OVERRIDE;
	virtual bool WantsToRunAgain() const OVERRIDE;
	virtual void CleanUp() OVERRIDE;
	virtual FString GetAdditionalTestFinishedMessage(EFunctionalTestResult::Type TestResult) const OVERRIDE;

protected:

	void KillOffSpawnedPawns();
};