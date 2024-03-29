// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

#include "Components/ApplicationLifecycleComponent.h"
#include "AI/BehaviorTree/BlackboardComponent.h"
#include "AI/BrainComponent.h"
#include "AI/BehaviorTree/BehaviorTreeComponent.h"
#include "Debug/GameplayDebuggingControllerComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/MovementComponent.h"
#include "Components/SceneComponent.h"
#include "Components/LightComponentBase.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/NavMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpectatorPawnMovement.h"
#include "Vehicles/WheeledVehicleMovementComponent.h"
#include "Vehicles/WheeledVehicleMovementComponent4W.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "AI/Navigation/NavigationComponent.h"
#include "AI/Navigation/PathFollowingComponent.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "Components/PawnSensingComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Atmosphere/AtmosphericFogComponent.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "AI/Navigation/NavigationGraphNodeComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/PhysicsThrusterComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/BrushComponent.h"
#include "Components/DrawFrustumComponent.h"
#include "AI/EnvironmentQuery/EQSRenderingComponent.h"
#include "Debug/GameplayDebuggingComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/DestructibleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/InteractiveFoliageComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/ModelComponent.h"
#include "AI/Navigation/NavLinkRenderingComponent.h"
#include "AI/Navigation/NavMeshRenderingComponent.h"
#include "AI/Navigation/NavTestRenderingComponent.h"
#include "Components/NiagaraComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/DrawSphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/VectorFieldComponent.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "Components/TimelineComponent.h"
#include "Slate.h"
#include "SlateReflector.h"
#include "AI/NavDataGenerator.h"
#include "OnlineSubsystemUtils.h"

#if WITH_EDITOR
#include "UnrealEd.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogCheatManager, Log, All);

#define LOCTEXT_NAMESPACE "CheatManager"	

UCheatManager::UCheatManager(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	DebugCameraControllerClass = ADebugCameraController::StaticClass();
	DebugCapsuleHalfHeight = 23.0f;
	DebugCapsuleRadius = 21.0f;
	DebugTraceDistance = 10000.0f;
	DebugTraceDrawNormalLength = 30.0f;
	DebugTraceChannel = ECC_Pawn;
	bDebugCapsuleTraceComplex = false;
}

void UCheatManager::FreezeFrame(float delay)
{
	FCanUnpause DefaultCanUnpause;
	DefaultCanUnpause.BindUObject( GetOuterAPlayerController(), &APlayerController::DefaultCanUnpause );
	GetWorld()->GetAuthGameMode()->SetPause(GetOuterAPlayerController(),DefaultCanUnpause);
	GetWorld()->PauseDelay = GetWorld()->TimeSeconds + delay;
}

void UCheatManager::Teleport()
{	
	FVector	ViewLocation;
	FRotator ViewRotation;
	check(GetOuterAPlayerController() != NULL);
	GetOuterAPlayerController()->GetPlayerViewPoint( ViewLocation, ViewRotation );

	FHitResult Hit;

	APawn* AssociatedPawn = GetOuterAPlayerController()->GetPawn();
	static FName NAME_TeleportTrace = FName(TEXT("TeleportTrace"));
	FCollisionQueryParams TraceParams(NAME_TeleportTrace, true, AssociatedPawn);

	bool bHit = GetWorld()->LineTraceSingle(Hit, ViewLocation, ViewLocation + 1000000.f * ViewRotation.Vector(), ECC_Pawn, TraceParams);
	if ( bHit )
	{
		Hit.Location += Hit.Normal * 4.0f;
	}

	if (AssociatedPawn != NULL)
	{
		AssociatedPawn->TeleportTo( Hit.Location, AssociatedPawn->GetActorRotation() );
	}
	else
	{
		ADebugCameraController* const DCC = Cast<ADebugCameraController>(GetOuter());
		if ((DCC != NULL) && (DCC->OriginalControllerRef != NULL))
		{
			APawn* OriginalControllerPawn = DCC->OriginalControllerRef->GetPawn();
			if (OriginalControllerPawn != NULL)
			{
				OriginalControllerPawn->TeleportTo(Hit.Location, OriginalControllerPawn->GetActorRotation());
			}
		}
	}
}

void UCheatManager::ChangeSize( float F )
{
	APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	
	// Note: only works on characters
	ACharacter *Character =  Cast<ACharacter>(Pawn);
	if (Character)
	{
		ACharacter* DefaultCharacter = Character->GetClass()->GetDefaultObject<ACharacter>();
		Character->CapsuleComponent->SetCapsuleSize( DefaultCharacter->CapsuleComponent->GetUnscaledCapsuleRadius() * F, DefaultCharacter->CapsuleComponent->GetUnscaledCapsuleHalfHeight() * F );

		if (Character->Mesh.IsValid())
		{
			Character->Mesh->SetRelativeScale3D( FVector(F) );
		}
		Character->TeleportTo(Character->GetActorLocation(), Character->GetActorRotation());
	}
}


void UCheatManager::Fly()
{
	APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if (Pawn != NULL)
	{
		GetOuterAPlayerController()->ClientMessage(TEXT("You feel much lighter"));
		
		ACharacter* Character =  Cast<ACharacter>(Pawn);
		if (Character)
		{
			Character->ClientCheatFly();
			if (!Character->IsLocallyControlled())
			{
				Character->ClientCheatFly_Implementation();
			}
		}
	}
}

void UCheatManager::Walk()
{
	APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if (Pawn != NULL)
	{
		ACharacter* Character =  Cast<ACharacter>(Pawn);
		if (Character)
		{
			Character->ClientCheatWalk();
			if (!Character->IsLocallyControlled())
			{
				Character->ClientCheatWalk_Implementation();
			}
		}
	}
}

void UCheatManager::Ghost()
{
	APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if (Pawn != NULL)
	{
		GetOuterAPlayerController()->ClientMessage(TEXT("You feel ethereal"));
		
		ACharacter* Character =  Cast<ACharacter>(Pawn);
		if (Character)
		{
			Character->ClientCheatGhost();
			if (!Character->IsLocallyControlled())
			{
				Character->ClientCheatGhost_Implementation();
			}
		}
	}	
}

void UCheatManager::God()
{
	APawn* Pawn = GetOuterAPlayerController()->GetPawn();
	if ( Pawn != NULL )
	{
		if ( Pawn->bCanBeDamaged )
		{
			Pawn->bCanBeDamaged = false;
			GetOuterAPlayerController()->ClientMessage(TEXT("God mode on"));
		}
		else
		{
			Pawn->bCanBeDamaged = true;
			GetOuterAPlayerController()->ClientMessage(TEXT("God Mode off"));
		}
	}
	else
	{
		GetOuterAPlayerController()->ClientMessage(TEXT("No APawn* possessed"));
	}
}

void UCheatManager::Slomo( float T )
{
	GetOuterAPlayerController()->GetWorldSettings()->TimeDilation = FMath::Clamp(T, 0.0001f, 20.0f);
}

void UCheatManager::DamageTarget(float DamageAmount)
{
	APlayerController* const MyPC = GetOuterAPlayerController();
	if ((MyPC == NULL) || (MyPC->PlayerCameraManager == NULL))
	{
		return;
	}

	check(GetWorld() != NULL);
	FVector const CamLoc = MyPC->PlayerCameraManager->GetCameraLocation();
	FRotator const CamRot = MyPC->PlayerCameraManager->GetCameraRotation();

	FCollisionQueryParams TraceParams(NAME_None, true, MyPC->GetPawn());
	FHitResult Hit;
	bool bHit = GetWorld()->LineTraceSingle(Hit, CamLoc, CamRot.Vector() * 100000.f + CamLoc, ECC_Pawn, TraceParams);
	if (bHit)
	{
		check(Hit.GetActor() != NULL);
		FVector ActorForward, ActorSide, ActorUp;
		FRotationMatrix(Hit.GetActor()->GetActorRotation()).GetScaledAxes(ActorForward, ActorSide, ActorUp);

		FPointDamageEvent DamageEvent(DamageAmount, Hit, -ActorForward, UDamageType::StaticClass());
		Hit.GetActor()->TakeDamage(DamageAmount, DamageEvent, MyPC, MyPC->GetPawn());
	}
}

void UCheatManager::DestroyTarget()
{
	APlayerController* const MyPC = GetOuterAPlayerController();
	if ((MyPC == NULL) || (MyPC->PlayerCameraManager == NULL))
	{
		return;
	}

	check(GetWorld() != NULL);
	FVector const CamLoc = MyPC->PlayerCameraManager->GetCameraLocation();
	FRotator const CamRot = MyPC->PlayerCameraManager->GetCameraRotation();

	FCollisionQueryParams TraceParams(NAME_None, true, MyPC->GetPawn());
	FHitResult Hit;
	bool bHit = GetWorld()->LineTraceSingle(Hit, CamLoc, CamRot.Vector() * 100000.f + CamLoc, ECC_Pawn, TraceParams);
	if (bHit)
	{
		check(Hit.GetActor() != NULL);
		APawn* Pawn = Cast<APawn>(Hit.GetActor());
		if (Pawn != NULL)
		{
			if ((Pawn->Controller != NULL) && (Cast<APlayerController>(Pawn->Controller) == NULL))
			{
				// Destroy any associated controller as long as it's not a player controller.
				Pawn->Controller->Destroy();
			}
		}

		Hit.GetActor()->Destroy();
	}
}

void UCheatManager::DestroyAll(TSubclassOf<AActor> aClass)
{
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (A && A->IsA(aClass) && !A->IsPendingKill())
		{
			APawn* Pawn = Cast<APawn>(A);
			if (Pawn != NULL)
			{
				if ((Pawn->Controller != NULL) && (Cast<APlayerController>(Pawn->Controller) == NULL))
				{
					// Destroy any associated controller as long as it's not a player controller.
					Pawn->Controller->Destroy();
				}
			}
			A->Destroy();
		}
	}
}

void UCheatManager::DestroyPawns(TSubclassOf<APawn> aClass)
{
	if ( aClass == NULL )
	{
		 aClass = APawn::StaticClass();
	}
	for( FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator )
	{
		APawn* Pawn = *Iterator;
		if ( Pawn->IsA(aClass) && Cast<APlayerController>(Pawn->Controller) == NULL )
		{
			if ( Pawn->Controller != NULL )
			{
				Pawn->Controller->Destroy();
			}
			Pawn->Destroy();
		}
	}
	
}

void UCheatManager::Summon( const FString& ClassName )
{
	UE_LOG(LogCheatManager, Log,  TEXT("Fabricate %s"), *ClassName );

	bool bIsValidClassName = true;
	FString FailureReason;
	if ( ClassName.Contains(TEXT(" ")) )
	{
		FailureReason = FString::Printf(TEXT("ClassName contains a space."));
		bIsValidClassName = false;
	}
	else if ( !FPackageName::IsShortPackageName(ClassName) )
	{
		if ( ClassName.Contains(TEXT(".")) )
		{
			FString PackageName;
			FString ObjectName;
			ClassName.Split(TEXT("."), &PackageName, &ObjectName);

			const bool bIncludeReadOnlyRoots = true;
			FText Reason;
			if ( !FPackageName::IsValidLongPackageName(PackageName, bIncludeReadOnlyRoots, &Reason) )
			{
				FailureReason = Reason.ToString();
				bIsValidClassName = false;
			}
		}
		else
		{
			FailureReason = TEXT("Class names with a path must contain a dot. (i.e /Script/Engine.StaticMeshActor)");
			bIsValidClassName = false;
		}
	}

	bool bSpawnedActor = false;
	if ( bIsValidClassName )
	{
		UClass* NewClass = NULL;
		if ( FPackageName::IsShortPackageName(ClassName) )
		{
			NewClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
		}
		else
		{
			NewClass = FindObject<UClass>(NULL, *ClassName);
		}

		if( NewClass )
		{
			if ( NewClass->IsChildOf(AActor::StaticClass()) )
			{
				APlayerController* const MyPlayerController = GetOuterAPlayerController();
				if(MyPlayerController)
				{
					FRotator const SpawnRot = MyPlayerController->GetControlRotation();
					FVector SpawnLoc = MyPlayerController->GetFocalLocation();

					SpawnLoc += 72.f * SpawnRot.Vector() + FVector(0.f, 0.f, 1.f) * 15.f;
					FActorSpawnParameters SpawnInfo;
					SpawnInfo.Instigator = MyPlayerController->Instigator;
					AActor* Actor = GetWorld()->SpawnActor(NewClass, &SpawnLoc, &SpawnRot, SpawnInfo );
					if ( Actor )
					{
						bSpawnedActor = true;
					}
					else
					{
						FailureReason = TEXT("SpawnActor failed.");
						bSpawnedActor = false;
					}
				}
			}
			else
			{
				FailureReason = TEXT("Class is not derived from Actor.");
				bSpawnedActor = false;
			}
		}
		else
		{
			FailureReason = TEXT("Failed to find class.");
			bSpawnedActor = false;
		}
	}
	
	if ( !bSpawnedActor )
	{
		UE_LOG(LogCheatManager, Warning, TEXT("Failed to summon %s. Reason: %s"), *ClassName, *FailureReason);
	}
}

void UCheatManager::AIIgnorePlayers()
{
	AAIController::ToggleAIIgnorePlayers();
}

void UCheatManager::PlayersOnly()
{
	check( GetWorld() );
	if (GetWorld()->bPlayersOnly || GetWorld()->bPlayersOnlyPending)
	{
		GetWorld()->bPlayersOnly = false;
		GetWorld()->bPlayersOnlyPending = false;
	}
	else
	{
		GetWorld()->bPlayersOnlyPending = !GetWorld()->bPlayersOnlyPending;
		// World.bPlayersOnly is set after next tick of UWorld::Tick
	}	
}

void UCheatManager::ViewSelf()
{
	GetOuterAPlayerController()->ResetCameraMode();
	if ( GetOuterAPlayerController()->GetPawn() != NULL )
	{
		GetOuterAPlayerController()->SetViewTarget(GetOuterAPlayerController()->GetPawn());
	}
	else
	{
		GetOuterAPlayerController()->SetViewTarget(GetOuterAPlayerController());
	}
	GetOuterAPlayerController()->ClientMessage(LOCTEXT("OwnCamera", "Viewing from own camera").ToString(), TEXT("Event"));
}

void UCheatManager::ViewPlayer( const FString& S )
{
	AController* Controller = NULL;
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		Controller = *Iterator;
		if ( Controller->PlayerState && (FCString::Stricmp(*Controller->PlayerState->PlayerName, *S) == 0 ) )
		{
			break;
		}
	}

	if ( Controller && Controller->GetPawn() != NULL )
	{
		GetOuterAPlayerController()->ClientMessage(FText::Format(LOCTEXT("ViewPlayer", "Viewing from {0}"), FText::FromString(Controller->PlayerState->PlayerName)).ToString(), TEXT("Event"));
		GetOuterAPlayerController()->SetViewTarget(Controller->GetPawn());
	}
}

void UCheatManager::ViewActor( FName ActorName)
{
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (A && !A ->IsPendingKill())
		{
			if ( A->GetFName() == ActorName )
			{
				GetOuterAPlayerController()->SetViewTarget(A);
				static FName NAME_ThirdPerson = FName(TEXT("ThirdPerson"));
				GetOuterAPlayerController()->SetCameraMode(NAME_ThirdPerson);
				return;
			}
		}
	}
}

void UCheatManager::ViewClass( TSubclassOf<AActor> DesiredClass )
{
	bool bFound = false;
	AActor* First = NULL;
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AActor* TestActor = *It;
		if (TestActor && !TestActor->IsPendingKill() && (*TestActor->GetClass()).IsChildOf(*DesiredClass) )
		{
			AActor* Other = TestActor;
			if (bFound || (First == NULL))
			{
				First = Other;
				if (bFound)
				{
					break;
				}
			}

			if (Other == GetOuterAPlayerController()->PlayerCameraManager->GetViewTarget())
			{
				bFound = true;
			}
		}
	}

	if (First != NULL)
	{
		GetOuterAPlayerController()->ClientMessage(FText::Format(LOCTEXT("ViewPlayer", "Viewing from {0}"), FText::FromString(First->GetHumanReadableName())).ToString(), TEXT("Event"));
		GetOuterAPlayerController()->SetViewTarget(First);
	}
	else
	{
		ViewSelf();
	}
}

void UCheatManager::SetLevelStreamingStatus(FName PackageName, bool bShouldBeLoaded, bool bShouldBeVisible)
{
	if (PackageName != NAME_All)
	{
		for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
		{
			(*Iterator)->ClientUpdateLevelStreamingStatus(PackageName, bShouldBeLoaded, bShouldBeVisible, false, INDEX_NONE );
		}
	}
	else
	{
		for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
		{
			for (int32 i = 0; i < GetWorld()->StreamingLevels.Num(); i++)
			{
				(*Iterator)->ClientUpdateLevelStreamingStatus(GetWorld()->StreamingLevels[i]->PackageName, bShouldBeLoaded, bShouldBeVisible, false, INDEX_NONE );
			}
		}
	}
}

void UCheatManager::StreamLevelIn(FName PackageName)
{
	SetLevelStreamingStatus(PackageName, true, true);
}

void UCheatManager::OnlyLoadLevel(FName PackageName)
{
	SetLevelStreamingStatus(PackageName, true, false);
}

void UCheatManager::StreamLevelOut(FName PackageName)
{
	SetLevelStreamingStatus(PackageName, false, false);
}

void UCheatManager::ToggleDebugCamera()
{
	ADebugCameraController* const DCC = Cast<ADebugCameraController>(GetOuter());
	if (DCC)
	{
		DisableDebugCamera();
	}
	else
	{
		EnableDebugCamera();
	}
}

void UCheatManager::EnableDebugCamera()
{
	APlayerController* const PC = GetOuterAPlayerController();
	if (PC && PC->Player && PC->IsLocalPlayerController())
	{
		if (DebugCameraControllerRef == NULL)
		{
			// spawn if necessary
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Instigator = PC->Instigator;
			DebugCameraControllerRef = GetWorld()->SpawnActor<ADebugCameraController>(DebugCameraControllerClass, SpawnInfo );
		}
		if (DebugCameraControllerRef)
		{
			// set up new controller
			DebugCameraControllerRef->OnActivate(PC);

			// then switch to it
			PC->Player->SwitchController( DebugCameraControllerRef );
		}
	}
}

void UCheatManager::DisableDebugCamera()
{
	ADebugCameraController* const DCC = Cast<ADebugCameraController>(GetOuter());
	if (DCC)
	{
		DCC->OriginalPlayer->SwitchController(DCC->OriginalControllerRef);
		DCC->OnDeactivate(DCC->OriginalControllerRef);
	}
}

void UCheatManager::InitCheatManager() {}

bool UCheatManager::ServerToggleAILogging_Validate()
{
	return true;
}

void UCheatManager::ServerToggleAILogging_Implementation()
{
#if ENABLE_VISUAL_LOG
	FVisualLog* VisLog = FVisualLog::Get();
	if (!VisLog)
	{
		return;
	}

	const bool bWasRecording = VisLog->IsRecording();
	VisLog->SetIsRecording(!bWasRecording);
	if (bWasRecording)
	{
		VisLog->DumpRecordedLogs();
	}

	GetOuterAPlayerController()->ClientMessage(FString::Printf(TEXT("OK! VisLog recording is now %s"), VisLog->IsRecording() ? TEXT("Enabled") : TEXT("Disabled")));
#endif
}

void UCheatManager::ToggleAILogging()
{
#if ENABLE_VISUAL_LOG
	APlayerController* PC = GetOuterAPlayerController();
	if (!PC)
	{
		return;
	}

	if (GetWorld()->GetNetMode() == NM_Client)
	{
		FVisualLog* VisLog = FVisualLog::Get();
		if (VisLog)
		{
			VisLog->SetIsRecordingOnServer(!VisLog->IsRecordingOnServer());
			GetOuterAPlayerController()->ClientMessage(FString::Printf(TEXT("OK! VisLog recording is now %s"), VisLog->IsRecordingOnServer() ? TEXT("Enabled") : TEXT("Disabled")));
		}
		PC->ServerToggleAILogging();
	}
	else
	{
		ServerToggleAILogging();
	}
#endif
}

void UCheatManager::AILoggingVerbose()
{
	APlayerController* PC = GetOuterAPlayerController();
	if (PC)
	{
		PC->ConsoleCommand(TEXT("log lognavigation verbose | log logpathfollowing verbose | log LogCharacter verbose | log LogBehaviorTree verbose | log LogPawnAction verbose|"));
	}
}


#define SAFE_TRACEINDEX_DECREASE(x) ((--x)<0)? 9:(x)

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UCheatManager::TickCollisionDebug()
{
	// If we are debugging capsule tracing
	if(bDebugCapsuleSweep)
	{
		APlayerController* PC = GetOuterAPlayerController();
		if(PC)
		{
			// Get view location to act as start point
			FVector ViewLoc;
			FRotator ViewRot;
			PC->GetPlayerViewPoint(ViewLoc, ViewRot);
			FVector ViewDir = ViewRot.Vector();
			FVector End = ViewLoc + (DebugTraceDistance * ViewDir);

			// Fill in params and do trace
			static const FName TickCollisionDebugName(TEXT("TickCollisionDebug"));
			FCollisionQueryParams CapsuleParams(TickCollisionDebugName, false, PC->GetPawn());
			CapsuleParams.bTraceComplex = bDebugCapsuleTraceComplex;

			if (bDebugCapsuleSweep)
			{
				// If we get a hit, draw the capsule
				FHitResult Result;
				bool bHit = GetWorld()->SweepSingle(Result, ViewLoc, End, FQuat::Identity, DebugTraceChannel, FCollisionShape::MakeCapsule(DebugCapsuleRadius, DebugCapsuleHalfHeight), CapsuleParams);
				if(bHit)
				{
					AddCapsuleSweepDebugInfo(ViewLoc, End, Result.ImpactPoint, Result.Normal, Result.ImpactNormal, Result.Location, DebugCapsuleHalfHeight, DebugCapsuleRadius, false, (Result.bStartPenetrating && Result.bBlockingHit)? true: false);
					UE_LOG(LogCollision, Log, TEXT("Collision component (%s) : Actor (%s)"), *GetNameSafe(Result.Component.Get()), *GetNameSafe(Result.GetActor()));
				}
			}
		}
	}

	// draw
	for (int32 TraceIdx=0; TraceIdx < DebugTraceInfoList.Num(); ++TraceIdx)
	{
		FDebugTraceInfo & TraceInfo = DebugTraceInfoList[TraceIdx];
		DrawDebugDirectionalArrow(GetWorld(), TraceInfo.LineTraceStart, TraceInfo.LineTraceEnd, 10.f, FColor(255,255,255), SDPG_World);
		// if it's current trace index, use highlight color
		if (CurrentTraceIndex == TraceIdx)
		{
			if (TraceInfo.bInsideOfObject)
			{
				DrawDebugCapsule(GetWorld(), TraceInfo.HitLocation, TraceInfo.CapsuleHalfHeight, TraceInfo.CapsuleRadius, FQuat::Identity, FColor(255,100,64));
			}
			else
			{
				DrawDebugCapsule(GetWorld(), TraceInfo.HitLocation, TraceInfo.CapsuleHalfHeight, TraceInfo.CapsuleRadius, FQuat::Identity, FColor(255,200,128));
			}
		}
		else
		{
			if (TraceInfo.bInsideOfObject)
			{
				DrawDebugCapsule(GetWorld(), TraceInfo.HitLocation, TraceInfo.CapsuleHalfHeight, TraceInfo.CapsuleRadius, FQuat::Identity, FColor(64,100,255));
			}
			else
			{
				DrawDebugCapsule(GetWorld(), TraceInfo.HitLocation, TraceInfo.CapsuleHalfHeight, TraceInfo.CapsuleRadius, FQuat::Identity, FColor(128,200,255));
			}
		}

		DrawDebugDirectionalArrow(GetWorld(), TraceInfo.HitNormalStart, TraceInfo.HitNormalEnd, 5, FColor(255,64,64), SDPG_World);

		DrawDebugDirectionalArrow(GetWorld(), TraceInfo.HitNormalStart, TraceInfo.HitImpactNormalEnd, 5, FColor(64,64,255), SDPG_World);
	}

	FLinearColor CurrentColor(255.f/255.f,96.f/255.f,96/255.f);
	FLinearColor DeltaColor = (FLinearColor(1.0f, 1.0f, 1.0f, 1.0f) - CurrentColor)*0.1f;
	int32 TotalCount=0;

	if ( DebugTracePawnInfoList.Num() > 0 )
	{
		// the latest will draw very red-ish to whiter color as it gets older. 
		for (int32 TraceIdx=CurrentTracePawnIndex; TotalCount<10; TraceIdx=SAFE_TRACEINDEX_DECREASE(TraceIdx), CurrentColor+=DeltaColor, ++TotalCount)
		{
			FDebugTraceInfo & TraceInfo = DebugTracePawnInfoList[TraceIdx];
			DrawDebugDirectionalArrow(GetWorld(), TraceInfo.LineTraceStart, TraceInfo.LineTraceEnd, 10.f, FColor(200,200,100), SDPG_World);

			if (TraceInfo.bInsideOfObject)
			{
				DrawDebugCapsule(GetWorld(), TraceInfo.HitLocation, TraceInfo.CapsuleHalfHeight, TraceInfo.CapsuleRadius, FQuat::Identity, FColor(64, 64, 255));
			}
			else
			{
				DrawDebugCapsule(GetWorld(), TraceInfo.HitLocation, TraceInfo.CapsuleHalfHeight, TraceInfo.CapsuleRadius, FQuat::Identity, CurrentColor.Quantize());
			}
			DrawDebugDirectionalArrow(GetWorld(), TraceInfo.HitNormalStart, TraceInfo.HitNormalEnd, 5.f, FColor(255,64,64), SDPG_World);
		}
	}
}

void UCheatManager::AddCapsuleSweepDebugInfo(
	const FVector & LineTraceStart, 
	const FVector & LineTraceEnd, 
	const FVector & HitImpactLocation, 
	const FVector & HitNormal, 
	const FVector & HitImpactNormal, 
	const FVector & HitLocation, 
	float CapsuleHalfheight, 
	float CapsuleRadius, 
	bool bTracePawn, 
	bool bInsideOfObject  )
{
	if (bTracePawn)
	{
		// to keep the last index to be the one added. We increase index first
		// this gets initialized to be -1, so it should be 0 when it starts. Max is 10
		if (++CurrentTracePawnIndex>9)
		{
			CurrentTracePawnIndex =0;
		}
	}

	FDebugTraceInfo & TraceInfo = (bTracePawn)?DebugTracePawnInfoList[CurrentTracePawnIndex]:DebugTraceInfoList[CurrentTraceIndex];

	TraceInfo.LineTraceStart = LineTraceStart;
	TraceInfo.LineTraceEnd = LineTraceEnd;
	TraceInfo.CapsuleHalfHeight = CapsuleHalfheight;
	TraceInfo.CapsuleRadius = CapsuleRadius;
	TraceInfo.HitLocation = HitLocation;

	TraceInfo.HitNormalStart = HitImpactLocation;
	TraceInfo.HitNormalEnd = HitImpactLocation + (HitNormal*DebugTraceDrawNormalLength);
	TraceInfo.HitImpactNormalEnd = HitImpactLocation + (HitImpactNormal*DebugTraceDrawNormalLength);

	TraceInfo.bInsideOfObject = bInsideOfObject;
}
#endif	//#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

void UCheatManager::DebugCapsuleSweep()
{
	bDebugCapsuleSweep = !bDebugCapsuleSweep;
	if (bDebugCapsuleSweep)
	{
		CurrentTraceIndex = DebugTraceInfoList.Num();
		DebugTraceInfoList.AddUninitialized(1);
	}
	else
	{
		DebugTraceInfoList.RemoveAt(CurrentTraceIndex);
	}
}

void UCheatManager::DebugCapsuleSweepSize(float HalfHeight,float Radius)
{
	DebugCapsuleHalfHeight	= HalfHeight;
	DebugCapsuleRadius		= Radius;
}

void UCheatManager::DebugCapsuleSweepChannel(enum ECollisionChannel Channel)
{
	DebugTraceChannel = Channel;
}

void UCheatManager::DebugCapsuleSweepComplex(bool bTraceComplex)
{
	bDebugCapsuleTraceComplex = bTraceComplex;
}

void UCheatManager::DebugCapsuleSweepCapture()
{
	++CurrentTraceIndex;
	DebugTraceInfoList.AddUninitialized(1);
}

void UCheatManager::DebugCapsuleSweepPawn()
{
	bDebugCapsuleSweepPawn = !bDebugCapsuleSweepPawn;
	if (bDebugCapsuleSweepPawn)
	{
		CurrentTracePawnIndex = 0;
		// only last 10 is the one saving for Pawn
		if (DebugTracePawnInfoList.Num() == 0)
		{
			DebugTracePawnInfoList.AddUninitialized(10);
		}
	}
}

void UCheatManager::DebugCapsuleSweepClear()
{
	CurrentTraceIndex = 0;
	DebugTraceInfoList.Empty();
	DebugTracePawnInfoList.Empty();
	if (bDebugCapsuleSweep)
	{
		DebugTraceInfoList.AddUninitialized(1);
	}

	if (bDebugCapsuleSweepPawn)
	{
		CurrentTracePawnIndex = 0;
		DebugTracePawnInfoList.AddUninitialized(10);
	}
}

void UCheatManager::TestCollisionDistance()
{
	APlayerController* PC = GetOuterAPlayerController();
	if(PC)
	{
		// Get view location to act as start point
		FVector ViewLoc;
		FRotator ViewRot;
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);

		FlushPersistentDebugLines( GetOuterAPlayerController()->GetWorld() );//change the GetWorld

		// calculate from viewloc
		for (FObjectIterator Iter(AVolume::StaticClass()); Iter; ++Iter)
		{
			AVolume * Volume = Cast<AVolume>(*Iter);

			if (Volume->GetClass()->GetDefaultObject() != Volume)
			{
				FVector ClosestPoint(0,0,0);
				float Distance = Volume->BrushComponent->GetDistanceToCollision(ViewLoc, ClosestPoint);
				float NormalizedDistance = FMath::Clamp<float>(Distance, 0.f, 1000.f)/1000.f;
				FColor DrawColor(255*NormalizedDistance, 255*(1-NormalizedDistance), 0);
				DrawDebugLine(GetWorld(), ViewLoc, ClosestPoint, DrawColor, true);

				UE_LOG(LogCheatManager, Log, TEXT("Distance to (%s) is %0.2f"), *Volume->GetName(), Distance);
			}
		}
	}
}

void UCheatManager::WidgetReflector()
{
	static TWeakPtr<SWindow> WidgetReflectorWindow;
	
	// Only allow one instance open at a time
	if( !WidgetReflectorWindow.IsValid() )
	{
		const TSharedRef<SWindow> ReflectorWindow = SNew(SWindow)
			.AutoCenter(EAutoCenter::PrimaryWorkArea)
			.ClientSize(FVector2D(600,400))
			[
				FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector").GetWidgetReflector()
			];
		
		WidgetReflectorWindow = ReflectorWindow;
		
		FSlateApplication::Get().AddWindow( ReflectorWindow );
	}
}

void UCheatManager::RebuildNavigation()
{
#if WITH_NAVIGATION_GENERATOR
	if (GetWorld() && GetWorld()->GetNavigationSystem())
	{
		GetWorld()->GetNavigationSystem()->Build();
	}
#endif // WITH_NAVIGATION_GENERATOR
}

void UCheatManager::SetNavDrawDistance(float DrawDistance)
{
	if (GIsEditor)
	{
		APlayerController* PC = GetOuterAPlayerController();
		if (PC != NULL)
		{
			PC->ClientMessage(TEXT("Setting Nav Rendering Draw Distance is not supported while in Edior"));
		}
	}
	ARecastNavMesh::SetDrawDistance(DrawDistance);
}

void UCheatManager::DumpOnlineSessionState()
{
	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(GetWorld());
	if (SessionInt.IsValid())
	{
		SessionInt->DumpSessionState();
	}
}

void UCheatManager::DumpVoiceMutingState()
{
	TSharedPtr<FUniqueNetId> NetId;
	
	UE_LOG(LogCheatManager, Display, TEXT(""));
	UE_LOG(LogCheatManager, Display, TEXT("-------------------------------------------------------------"));
	UE_LOG(LogCheatManager, Display, TEXT(""));

	// Log the online view of the voice state
	IOnlineVoicePtr VoiceInt = Online::GetVoiceInterface(GetWorld());
	if (VoiceInt.IsValid())
	{
		UE_LOG(LogCheatManager, Display, TEXT("\n%s"), *VoiceInt->GetVoiceDebugState());
	}

	// For each player list their gameplay mutes and system wide mutes
	UE_LOG(LogCheatManager, Display, TEXT("\n%s"), *DumpMutelistState(GetWorld()));
}

UWorld* UCheatManager::GetWorld() const
{
	return GetOuterAPlayerController()->GetWorld();
}

void UCheatManager::BugItGo( float X, float Y, float Z, float Pitch, float Yaw, float Roll )
{
	FVector TheLocation(X, Y, Z);
	FRotator TheRotation(Pitch, Yaw, Roll);
	BugItWorker( TheLocation, TheRotation );
}


void UCheatManager::BugItGoString(const FString& TheLocation, const FString& TheRotation)
{
	const TCHAR* Stream = *TheLocation;
	FVector Vect(ForceInit);
	Vect.X = FCString::Atof(Stream);
	Stream = FCString::Strstr(Stream,TEXT(","));
	if( Stream )
	{
		Vect.Y = FCString::Atof(++Stream);
		Stream = FCString::Strstr(Stream,TEXT(","));
		if( Stream )
			Vect.Z = FCString::Atof(++Stream);
	}

	const TCHAR* RotStream = *TheRotation;
	FRotator Rotation(ForceInit);
	Rotation.Pitch = FCString::Atof(RotStream);
	RotStream = FCString::Strstr(RotStream,TEXT(","));
	if( RotStream )
	{
		Rotation.Yaw = FCString::Atof(++RotStream);
		RotStream = FCString::Strstr(RotStream,TEXT(","));
		if( RotStream )
			Rotation.Roll = FCString::Atof(++RotStream);
	}

	BugItWorker(Vect, Rotation);
}


void UCheatManager::BugItWorker( FVector TheLocation, FRotator TheRotation )
{
	UE_LOG(LogCheatManager, Log,  TEXT("BugItGo to: %s %s"), *TheLocation.ToString(), *TheRotation.ToString() );

	Ghost();

	APlayerController* const MyPlayerController = GetOuterAPlayerController();
	if (MyPlayerController->GetPawn())
	{
		MyPlayerController->GetPawn()->TeleportTo( TheLocation, TheRotation );
		MyPlayerController->GetPawn()->FaceRotation( TheRotation, 0.0f );
	}
	MyPlayerController->SetControlRotation(TheRotation);
}


void UCheatManager::BugIt( const FString& ScreenShotDescription )
{
	APlayerController* const MyPlayerController = GetOuterAPlayerController();

	MyPlayerController->ConsoleCommand( FString::Printf(TEXT("BUGSCREENSHOTWITHHUDINFO %s"), *ScreenShotDescription ));

	FVector ViewLocation;
	FRotator ViewRotation;
	MyPlayerController->GetPlayerViewPoint( ViewLocation, ViewRotation );

	if( MyPlayerController->GetPawn() != NULL )
	{
		ViewLocation = MyPlayerController->GetPawn()->GetActorLocation();
	}

	FString GoString, LocString;
	BugItStringCreator( ViewLocation, ViewRotation, GoString, LocString );

	LogOutBugItGoToLogFile( ScreenShotDescription, GoString, LocString );
}

void UCheatManager::BugItStringCreator( FVector ViewLocation, FRotator ViewRotation, FString& GoString, FString& LocString )
{
	GoString = FString::Printf(TEXT("BugItGo %f %f %f %f %f %f"), ViewLocation.X, ViewLocation.Y, ViewLocation.Z, ViewRotation.Pitch, ViewRotation.Yaw, ViewRotation.Roll);
	UE_LOG(LogCheatManager, Log, TEXT("%s"), *GoString);

	LocString = FString::Printf(TEXT("?BugLoc=%s?BugRot=%s"), *ViewLocation.ToString(), *ViewRotation.ToString());
	UE_LOG(LogCheatManager, Log, TEXT("%s"), *LocString);
}

void UCheatManager::FlushLog()
{
	GLog->FlushThreadedLogs();
	GLog->Flush();
}

void UCheatManager::LogLoc()
{
	APlayerController* const MyPlayerController = GetOuterAPlayerController();

	FVector ViewLocation;
	FRotator ViewRotation;
	MyPlayerController->GetPlayerViewPoint( ViewLocation, ViewRotation );
	if( MyPlayerController->GetPawn() != NULL )
	{
		ViewLocation = MyPlayerController->GetPawn()->GetActorLocation();
	}
	FString GoString, LocString;
	BugItStringCreator( ViewLocation, ViewRotation, GoString, LocString );
}

void UCheatManager::SetWorldOrigin()
{
	UWorld* World = GetWorld();
	check(World);

	APlayerController* const MyPlayerController = GetOuterAPlayerController();

	FVector ViewLocation;
	FRotator ViewRotation;
	MyPlayerController->GetPlayerViewPoint( ViewLocation, ViewRotation );
	if( MyPlayerController->GetPawn() != NULL )
	{
		ViewLocation = MyPlayerController->GetPawn()->GetActorLocation();
	}
	
	FIntPoint NewOrigin = FIntPoint(ViewLocation.X, ViewLocation.Y) + World->GlobalOriginOffset;
	World->RequestNewWorldOrigin(NewOrigin);
}

void UCheatManager::ToggleGameplayDebugView(const FString& InViewName)
{
	static TArray<FString> ViewNames;
	if (ViewNames.Num() == 0)
	{
		const UEnum* ViewlEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EAIDebugDrawDataView"), true);
		ViewNames.AddZeroed(EAIDebugDrawDataView::MAX);
		for (int32 Index = 0; Index < EAIDebugDrawDataView::MAX; ++Index)
		{
			ViewNames[Index] = ViewlEnum->GetEnumName(Index);
		}
	}

	int32 ViewIndex = ViewNames.Find(InViewName);
	if (ViewIndex != INDEX_NONE)
	{
		const bool bIsEnabled = UGameplayDebuggingControllerComponent::ToggleStaticView(EAIDebugDrawDataView::Type(ViewIndex));
		GetOuterAPlayerController()->ClientMessage(FString::Printf(TEXT("View %s %s")
			, *InViewName
			, bIsEnabled ? TEXT("enabled") : TEXT("disabled")));
	}
	else
	{
		GetOuterAPlayerController()->ClientMessage(TEXT("Unknown debug view name. Valid options are:"));
		for (int32 Index = 0; Index < EAIDebugDrawDataView::MAX; ++Index)
		{
			GetOuterAPlayerController()->ClientMessage(*ViewNames[Index]);
		}
	}
}

void UCheatManager::RunEQS(const FString& QueryName)
{
	APlayerController* MyPC = GetOuterAPlayerController();
	UEnvQueryManager* EQS = GetWorld()->GetEnvironmentQueryManager();
	if (MyPC && EQS)
	{
		AActor* Target = NULL;
		UGameplayDebuggingControllerComponent* DebugComp = MyPC->FindComponentByClass<UGameplayDebuggingControllerComponent>();
		if (DebugComp)
		{
			Target = DebugComp->GetCurrentDebugTarget();
		}

#if WITH_EDITOR
		if (Target == NULL && GEditor != NULL)
		{
			Target = GEditor->GetSelectedObjects()->GetTop<AActor>();

			// this part should not be needed, but is due to gameplay debugging messed up design
			if (Target == NULL)
			{
				for (FObjectIterator It(UGameplayDebuggingComponent::StaticClass()); It && (Target == NULL); ++It)
				{
					UGameplayDebuggingComponent* Comp = ((UGameplayDebuggingComponent*)*It);
					if (Comp->IsSelected())
					{
						Target = Comp->GetOwner();
					}
				}
			}
		}
#endif // WITH_EDITOR

		if (Target)
		{
			UEnvQuery* QueryTemplate = EQS->FindQueryTemplate(QueryName);
			
			if (QueryTemplate)
			{
				EQS->RunInstantQuery(FEnvQueryRequest(QueryTemplate, Target), EEnvQueryRunMode::AllMatching);
			}
			else
			{
				GetOuterAPlayerController()->ClientMessage(FString::Printf(TEXT("Unable to fing query template \'%s\'"), *QueryName));
			}
		}
		else
		{
			GetOuterAPlayerController()->ClientMessage(TEXT("No debugging target"));
		}
	}
}

void UCheatManager::LogOutBugItGoToLogFile( const FString& InScreenShotDesc, const FString& InGoString, const FString& InLocString )
{
#if ALLOW_DEBUG_FILES
	// Create folder if not already there

	const FString OutputDir = FPaths::BugItDir() + InScreenShotDesc + TEXT("/");

	IFileManager::Get().MakeDirectory( *OutputDir );
	// Create archive for log data.
	// we have to +1 on the GScreenshotBitmapIndex as it will be incremented by the bugitscreenshot which is processed next tick

	const FString DescPlusExtension = FString::Printf( TEXT("%s%i.txt"), *InScreenShotDesc, GScreenshotBitmapIndex );
	const FString TxtFileName = CreateProfileFilename( DescPlusExtension, false );

	//FString::Printf( TEXT("BugIt%s-%s%05i"), *GEngineVersion.ToString(), *InScreenShotDesc, GScreenshotBitmapIndex+1 ) + TEXT( ".txt" );
	const FString FullFileName = OutputDir + TxtFileName;

	FOutputDeviceFile OutputFile(*FullFileName);
	//FArchive* OutputFile = IFileManager::Get().CreateDebugFileWriter( *(FullFileName), FILEWRITE_Append );


	OutputFile.Logf( TEXT("Dumping BugIt data chart at %s using build %s built from changelist %i"), *FDateTime::Now().ToString(), *GEngineVersion.ToString(), GetChangeListNumberForPerfTesting() );

	const FString MapNameStr = GetWorld()->GetMapName();

	OutputFile.Logf( TEXT("MapName: %s"), *MapNameStr );

	OutputFile.Logf( TEXT("Description: %s"), *InScreenShotDesc );
	OutputFile.Logf( TEXT("%s"), *InGoString );
	OutputFile.Logf( TEXT("%s"), *InLocString );

	OutputFile.Logf( TEXT(" ---=== GameSpecificData ===--- ") );
	DoGameSpecificBugItLog(OutputFile);


	// Flush, close and delete.
	//delete OutputFile;
	OutputFile.TearDown();

	// so here we want to send this bad boy back to the PC
	SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!BUGIT:"), *(FullFileName) );
#endif // ALLOW_DEBUG_FILES
}

#undef LOCTEXT_NAMESPACE
