// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Net/UnrealNetwork.h"
#include "ConfigCacheIni.h"
#include "SoundDefinitions.h"
#include "OnlineSubsystemUtils.h"
#include "IHeadMountedDisplay.h"
#include "IForceFeedbackSystem.h"
#include "Slate.h"
#include "GameFramework/TouchInterface.h"
#include "DisplayDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlayerController, Log, All);

const float RetryClientRestartThrottleTime = 0.5f;
const float RetryServerAcknowledgeThrottleTime = 0.25f;
const float RetryServerCheckSpectatorThrottleTime = 0.25f;

//////////////////////////////////////////////////////////////////////////
// APlayerController

APlayerController::APlayerController(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	NetPriority = 3.0f;
	CheatClass = UCheatManager::StaticClass();
	ClientCap = 0;
	LocalPlayerCachedLODDistanceFactor = 1.0f;
	bIsUsingStreamingVolumes = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bShouldPerformFullTickWhenPaused = false;
	LastRetryPlayerTime = 0.f;
	DefaultMouseCursor = EMouseCursor::Default;
	DefaultClickTraceChannel = ECollisionChannel::ECC_Visibility;

	bCinemaDisableInputMove = false;
	bCinemaDisableInputLook = false;

	bInputEnabled = true;

	bAutoManageActiveCameraTarget = true;

	if (RootComponent)
	{
		// We want to drive rotation with ControlRotation regardless of attachment state.
		RootComponent->bAbsoluteRotation = true;
	}
}

float APlayerController::GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, APlayerController* Viewer, UActorChannel* InChannel, float Time, bool bLowBandwidth)
{
	if ( Viewer == this )
	{
		Time *= 4.f;
	}
	return NetPriority * Time;
}

UPlayer* APlayerController::GetNetOwningPlayer() 
{
	return Player;
}

UNetConnection* APlayerController::GetNetConnection()
{
	// A controller without a player has no "owner"
	return (Player != NULL) ? NetConnection : NULL;
}

bool APlayerController::IsLocalController() const
{
	if (Player == NULL)
	{
		UE_LOG(LogPlayerController, Warning, TEXT("Calling IsLocalController() while Player is NULL is undefined!"));
	}

	ENetMode NetMode = GetNetMode();
	if (NetMode == NM_DedicatedServer)
	{
		return false;
	}

	if (Super::IsLocalController())
	{
		return true;
	}

	if (NetMode == NM_Client)
	{
		// Clients only receive their own PC. We are not ROLE_AutonomousProxy until after PostInitializeComponents so we can't check that.
		return true;
	}

	return false;
}

bool APlayerController::IsLocalPlayerController() const
{
	// We automatically pass the "IsPlayer" part because we are a PlayerController...
	return IsLocalController();
}

void APlayerController::FailedToSpawnPawn()
{
	Super::FailedToSpawnPawn();
	ChangeState(NAME_Inactive);
	ClientGotoState(NAME_Inactive);
}


void APlayerController::ClientUpdateLevelStreamingStatus_Implementation(FName PackageName, bool bNewShouldBeLoaded, bool bNewShouldBeVisible, bool bNewShouldBlockOnLoad, int32 LODIndex )
{
	// For PIE Networking: remap the packagename to our local PIE packagename
	FString PackageNameStr = PackageName.ToString();
	if (GEngine->NetworkRemapPath(GetWorld(), PackageNameStr, true))
	{
		PackageName = FName(*PackageNameStr);
	}


	//GEngine->NetworkRemapPath(GetWorld(), RealName, false);

	// if we're about to commit a map change, we assume that the streaming update is based on the to be loaded map and so defer it until that is complete
	if (GEngine->ShouldCommitPendingMapChange(GetWorld()))
	{
		GEngine->AddNewPendingStreamingLevel(GetWorld(), PackageName, bNewShouldBeLoaded, bNewShouldBeVisible, LODIndex);		
	}
	else
	{
		// search for the level object by name
		ULevelStreaming* LevelStreamingObject = NULL;
		if (PackageName != NAME_None)
		{
			for (int32 LevelIndex=0; LevelIndex < GetWorld()->StreamingLevels.Num(); LevelIndex++)
			{
				ULevelStreaming* CurrentLevelStreamingObject = GetWorld()->StreamingLevels[LevelIndex];
				if (CurrentLevelStreamingObject != NULL && CurrentLevelStreamingObject->PackageName == PackageName)
				{
					LevelStreamingObject = CurrentLevelStreamingObject;
					if (LevelStreamingObject != NULL)
					{
						// If we're unloading any levels, we need to request a one frame delay of garbage collection to make sure it happens after the level is actually unloaded
						if (LevelStreamingObject->bShouldBeLoaded && !bNewShouldBeLoaded)
						{
							GetWorld()->DelayGarbageCollection();
						}

						LevelStreamingObject->bShouldBeLoaded		= bNewShouldBeLoaded;
						LevelStreamingObject->bShouldBeVisible		= bNewShouldBeVisible;
						LevelStreamingObject->bShouldBlockOnLoad	= bNewShouldBlockOnLoad;
						LevelStreamingObject->SetLODIndex(GetWorld(), LODIndex);
					}
					else
					{
						UE_LOG(LogStreaming, Log, TEXT("Unable to handle streaming object %s"),*LevelStreamingObject->GetName() );
					}

					// break out of object iterator if we found a match
					break;
				}
			}
		}

		if (LevelStreamingObject == NULL)
		{
			UE_LOG(LogStreaming, Log, TEXT("Unable to find streaming object %s"), *PackageName.ToString() );
		}
	}
}

void APlayerController::ClientFlushLevelStreaming_Implementation()
{
	// if we're already doing a map change, requesting another blocking load is just wasting time	
	if (GEngine->ShouldCommitPendingMapChange(GetWorld()))
	{
		// request level streaming be flushed next frame
		GetWorld()->UpdateLevelStreaming(NULL);
		GetWorld()->bRequestedBlockOnAsyncLoading = true;
		// request GC as soon as possible to remove any unloaded levels from memory
		GetWorld()->ForceGarbageCollection();
	}
}


void APlayerController::ServerUpdateLevelVisibility_Implementation(FName PackageName, bool bIsVisible)
{
	UNetConnection* Connection = Cast<UNetConnection>(Player);
	if (Connection != NULL)
	{
		// add or remove the level package name from the list, as requested
		if (bIsVisible)
		{
			// verify that we were passed a valid level name
			FString Filename;
			UPackage* TempPkg = FindPackage(NULL, *PackageName.ToString());
			ULinkerLoad* Linker = ULinkerLoad::FindExistingLinkerForPackage(TempPkg);

			// If we have a linker we know it has been loaded off disk successfully
			// If we have a file it is fine too
			// If its in our own streaming level list, its good

			struct Local
			{
				static bool IsInLevelList(UWorld *World, FName InPackageName)
				{
					for (int32 i=0; i < World->StreamingLevels.Num(); ++i)
					{
						if ( World->StreamingLevels[i] && (World->StreamingLevels[i]->PackageName == InPackageName ))
						{
							return true;
						}
					}
					return false;
				}
			};

			if ( Linker || FPackageName::DoesPackageExist(PackageName.ToString(), NULL, &Filename ) || Local::IsInLevelList(GetWorld(), PackageName ) )
			{
				Connection->ClientVisibleLevelNames.AddUnique(PackageName);
				UE_LOG( LogPlayerController, Log, TEXT("ServerUpdateLevelVisibility() Added '%s'"), *PackageName.ToString() );
			}
			else
			{
				UE_LOG( LogPlayerController, Warning, TEXT("ServerUpdateLevelVisibility() ignored non-existant package '%s'"), *PackageName.ToString() );
				Connection->Close();
			}
		}
		else
		{
			Connection->ClientVisibleLevelNames.Remove(PackageName);
			UE_LOG( LogPlayerController, Log, TEXT("ServerUpdateLevelVisibility() Removed '%s'"), *PackageName.ToString() );
			
			// Close any channels now that have actors that were apart of the level the client just unloaded
			for ( auto It = Connection->ActorChannels.CreateIterator(); It; ++It )
			{
				UActorChannel * Channel	= It.Value();					

				check( Channel->OpenedLocally );

				if ( Channel->Actor != NULL && Channel->Actor->GetLevel()->GetOutermost()->GetFName() == PackageName )
				{
					Channel->Close();
				}
			}
		}
	}
}

bool APlayerController::ServerUpdateLevelVisibility_Validate(FName PackageName, bool bIsVisible)
{
	RPC_VALIDATE( PackageName.IsValid() );

	FText Reason;

	if ( !FPackageName::IsValidLongPackageName( PackageName.ToString(), true, &Reason ) )
	{
		UE_LOG( LogPlayerController, Warning, TEXT( "ServerUpdateLevelVisibility() Invalid package name: %s (%s)" ), *PackageName.ToString(), *Reason.ToString() );
		return false;
	}

	return true;
}

void APlayerController::ClientAddTextureStreamingLoc_Implementation(FVector InLoc, float Duration, bool bOverrideLocation )
{
	if (!IStreamingManager::HasShutdown())
	{
		IStreamingManager::Get().AddViewSlaveLocation(InLoc, 1.0f, bOverrideLocation, Duration);
	}
}

void APlayerController::SetNetSpeed(int32 NewSpeed)
{
	UNetDriver* Driver = GetWorld()->GetNetDriver();
	if (Player != NULL && Driver != NULL)
	{
		Player->CurrentNetSpeed = FMath::Clamp(NewSpeed, 1800, Driver->MaxClientRate);
		if (Driver->ServerConnection != NULL)
		{
			Driver->ServerConnection->CurrentNetSpeed = Player->CurrentNetSpeed;
		}
	}
}

FString APlayerController::ConsoleCommand(const FString& Cmd,bool bWriteToLog)
{
	if (Player != NULL)
	{
		UConsole* ViewportConsole = (GEngine->GameViewport != NULL) ? GEngine->GameViewport->ViewportConsole : NULL;
		FConsoleOutputDevice StrOut(ViewportConsole);

		const int32 CmdLen = Cmd.Len();
		TCHAR* CommandBuffer = (TCHAR*)FMemory::Malloc((CmdLen+1)*sizeof(TCHAR));
		TCHAR* Line = (TCHAR*)FMemory::Malloc((CmdLen+1)*sizeof(TCHAR));

		const TCHAR* Command = CommandBuffer;
		// copy the command into a modifiable buffer
		FCString::Strcpy(CommandBuffer, (CmdLen+1), *Cmd.Left(CmdLen)); 
		
		// iterate over the line, breaking up on |'s
		while (FParse::Line(&Command, Line, CmdLen+1))	// The FParse::Line function expects the full array size, including the NULL character.
		{
			if (Player)
			{
				if(!Player->Exec( GetWorld(), Line, StrOut))
				{
					StrOut.Logf(TEXT("Command not recognized: %s"), Line);
				}
			}
		}

		// Free temp arrays
		FMemory::Free(CommandBuffer);
		CommandBuffer=NULL;

		FMemory::Free(Line);
		Line=NULL;

		if (!bWriteToLog)
		{
			return *StrOut;
		}
	}

	return TEXT("");
}

void APlayerController::CleanUpAudioComponents()
{
	TArray<UAudioComponent*> Components;
	GetComponents(Components);

	for(int32 CompIndex = 0; CompIndex < Components.Num(); CompIndex++)
	{
		UAudioComponent* AComp = Components[CompIndex];
		if (AComp->Sound == NULL)
		{
			AComp->DestroyComponent();
		}
	}
}

AActor* APlayerController::GetViewTarget() const
{
	return PlayerCameraManager ? PlayerCameraManager->GetViewTarget() : NULL;
}

void APlayerController::SetViewTarget(class AActor* NewViewTarget, struct FViewTargetTransitionParams TransitionParams)
{
	// if we're being controlled by a director track, update it with the new viewtarget 
	// so it returns to the proper viewtarget when it finishes.
	UInterpTrackInstDirector* const Director = GetControllingDirector();
	if (Director)
	{
		Director->OldViewTarget = NewViewTarget;
	}

	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetViewTarget(NewViewTarget, TransitionParams);
	}
}


void APlayerController::SetControllingDirector(UInterpTrackInstDirector* NewControllingDirector, bool bClientSimulatingViewTarget)
{
	ControllingDirTrackInst = NewControllingDirector;

	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->bClientSimulatingViewTarget = (NewControllingDirector != NULL) ? bClientSimulatingViewTarget : false;
	}
}


UInterpTrackInstDirector* APlayerController::GetControllingDirector()
{
	return ControllingDirTrackInst;
}

bool APlayerController::ServerNotifyLoadedWorld_Validate(FName WorldPackageName)
{
	RPC_VALIDATE( WorldPackageName.IsValid() );
	return true;
}

void APlayerController::ServerNotifyLoadedWorld_Implementation(FName WorldPackageName)
{
	UE_LOG(LogPlayerController, Log, TEXT("APlayerController::ServerNotifyLoadedWorld_Implementation: Client loaded %s"), *WorldPackageName.ToString());

	UWorld* CurWorld = GetWorld();

	// Only valid for calling, for PC's in the process of seamless traveling
	// NOTE: SeamlessTravelCount tracks client seamless travel, through the serverside gameplay code; this should not be replaced.
	if (CurWorld != NULL && CurWorld->IsServer() && SeamlessTravelCount > 0 && LastCompletedSeamlessTravelCount < SeamlessTravelCount)
	{
		// Update our info on what world the client is in
		UNetConnection* const Connection = Cast<UNetConnection>(Player);

		if (Connection != NULL)
		{
			Connection->ClientWorldPackageName = WorldPackageName;
			Connection->PackageMap->SetLocked(false);
		}

		// if both the server and this client have completed the transition, handle it
		FSeamlessTravelHandler& SeamlessTravelHandler = GEngine->SeamlessTravelHandlerForWorld(CurWorld);
		AGameMode* CurGameMode = CurWorld->GetAuthGameMode();

		if (!SeamlessTravelHandler.IsInTransition() && WorldPackageName == CurWorld->GetOutermost()->GetFName() && CurGameMode != NULL)
		{
			AController* TravelPlayer = this;
			CurGameMode->HandleSeamlessTravelPlayer(TravelPlayer);
		}
	}
}

bool APlayerController::HasClientLoadedCurrentWorld()
{
	UNetConnection* Connection = Cast<UNetConnection>(Player);
	if (Connection == NULL && UNetConnection::GNetConnectionBeingCleanedUp != NULL && UNetConnection::GNetConnectionBeingCleanedUp->PlayerController == this)
	{
		Connection = UNetConnection::GNetConnectionBeingCleanedUp;
	}
	if (Connection != NULL)
	{
		// NOTE: To prevent exploits, child connections must not use the parent connections ClientWorldPackageName value at all.

		return (Connection->ClientWorldPackageName == GetWorld()->GetOutermost()->GetFName());
	}
	else
	{
		// if we have no client connection, we're local, so we always have the current world
		return true;
	}
}

void APlayerController::ForceSingleNetUpdateFor(AActor* Target)
{
	if (Target == NULL)
	{
		UE_LOG(LogPlayerController, Warning, TEXT("PlayerController::ForceSingleNetUpdateFor(): No Target specified"));
	}
	else if (GetNetMode() == NM_Client)
	{
		UE_LOG(LogPlayerController, Warning, TEXT("PlayerController::ForceSingleNetUpdateFor(): Only valid on server"));
	}
	else
	{
		UNetConnection* Conn = Cast<UNetConnection>(Player);
		if (Conn != NULL)
		{
			if (Conn->GetUChildConnection() != NULL)
			{
				Conn = ((UChildConnection*)Conn)->Parent;
				checkSlow(Conn != NULL);
			}
			UActorChannel* Channel = Conn->ActorChannels.FindRef(Target);
			if (Channel != NULL)
			{
				Target->bPendingNetUpdate = true; // will cause some other clients to do lesser checks too, but that's unavoidable with the current functionality
			}
		}
	}
}

/** worker function for APlayerController::SmoothTargetViewRotation() */
int32 BlendRot(float DeltaTime, float BlendC, float NewC)
{
	if ( FMath::Abs(BlendC - NewC) > 180.f )
	{
		if ( BlendC > NewC )
			NewC += 360.f;
		else
			BlendC += 360.f;
	}
	if ( FMath::Abs(BlendC - NewC) > 22.57 )
		BlendC = NewC;
	else
		BlendC = BlendC + (NewC - BlendC) * FMath::Min(1.f,24.f * DeltaTime);

	return FRotator::ClampAxis(BlendC);
}


void APlayerController::SmoothTargetViewRotation(APawn* TargetPawn, float DeltaSeconds)
{
	BlendedTargetViewRotation.Pitch = BlendRot(DeltaSeconds, BlendedTargetViewRotation.Pitch, FRotator::ClampAxis(TargetViewRotation.Pitch));
	BlendedTargetViewRotation.Yaw = BlendRot(DeltaSeconds, BlendedTargetViewRotation.Yaw, FRotator::ClampAxis(TargetViewRotation.Yaw));
	BlendedTargetViewRotation.Roll = BlendRot(DeltaSeconds, BlendedTargetViewRotation.Roll, FRotator::ClampAxis(TargetViewRotation.Roll));
}


void APlayerController::InitInputSystem()
{
	if (PlayerInput == NULL)
	{
		PlayerInput = ConstructObject<UPlayerInput>(UPlayerInput::StaticClass(), this);
	}

	// initialize input stack
	CurrentInputStack.Empty();

	SetupInputComponent();

	CurrentMouseCursor = DefaultMouseCursor;
	CurrentClickTraceChannel = DefaultClickTraceChannel;

	UWorld* World = GetWorld();
	check(World);
	World->PersistentLevel->PushPendingAutoReceiveInput(this);

	// add the player to any matinees running so that it gets in on any cinematics already running, etc
	// (already done on server in PostLogin())
	if (Role < ROLE_Authority)
	{
		TArray<AMatineeActor*> AllMatineeActors;
		World->GetMatineeActors(AllMatineeActors);

		// tell them all to add this PC to any running Director tracks
		for (int32 i = 0; i < AllMatineeActors.Num(); i++)
		{
			AllMatineeActors[i]->AddPlayerToDirectorTracks(this);
		}
	}

	// setup optional touchscreen interface
	CreateTouchInterface();
}

void APlayerController::SafeRetryClientRestart()
{
	if (AcknowledgedPawn != GetPawn())
	{
		UWorld* World = GetWorld();
		check(World);

		if (World->TimeSince(LastRetryPlayerTime) > RetryClientRestartThrottleTime)
		{
			ClientRetryClientRestart(GetPawn());
			LastRetryPlayerTime = World->TimeSeconds;
		}
	}
}


/** Avoid calling ClientRestart if we have already accepted this pawn */
void APlayerController::ClientRetryClientRestart_Implementation(APawn* NewPawn)
{
	if (NewPawn == NULL)
	{
		return;
	}

	UE_LOG(LogPlayerController, Log, TEXT("ClientRetryClientRestart_Implementation %s, AcknowledgedPawn: %s"), *GetNameSafe(NewPawn), *GetNameSafe(AcknowledgedPawn));

	// Avoid calling ClientRestart if we have already accepted this pawn
	if( (GetPawn() != NewPawn) || (NewPawn->Controller != this) || (NewPawn != AcknowledgedPawn) )
	{
		SetPawn(NewPawn);
		NewPawn->Controller = this;
		ClientRestart(GetPawn());
	}
}

void APlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
	UE_LOG(LogPlayerController, Log, TEXT("ClientRestart_Implementation %s"), *GetNameSafe(NewPawn));

	ResetIgnoreInputFlags();
	AcknowledgedPawn = NULL;

	SetPawn(NewPawn);
	if ( (GetPawn() != NULL) && GetPawn()->bTearOff )
	{
		UnPossess();
		SetPawn(NULL);
		AcknowledgePossession(GetPawn());
		return;
	}

	if ( GetPawn() == NULL )
	{
		return;
	}

	// Only acknowledge non-null Pawns here. ClientRestart is only ever called by the Server for valid pawns,
	// but we may receive the function call before Pawn is replicated over, so it will resolve to NULL.
	AcknowledgePossession(GetPawn());

	GetPawn()->Controller = this;
	GetPawn()->PawnClientRestart();
	
	if (Role < ROLE_Authority)
	{
		if (bAutoManageActiveCameraTarget)
		{
			SetViewTarget(GetPawn());
			ResetCameraMode();
		}
		
		ChangeState( NAME_Playing );
	}
}

void APlayerController::Possess(APawn* PawnToPossess)
{
	if ( PawnToPossess != NULL && 
		(PlayerState == NULL || !PlayerState->bOnlySpectator) )
	{
		if (GetPawn() && GetPawn() != PawnToPossess)
		{
			UnPossess();
		}

		if (PawnToPossess->Controller != NULL)
		{
			PawnToPossess->Controller->UnPossess();
		}

		PawnToPossess->PossessedBy(this);

		// update rotation to match possessed pawn's rotation
		SetControlRotation( PawnToPossess->GetActorRotation() );

		SetPawn(PawnToPossess);
		check(GetPawn() != NULL);

		GetPawn()->SetActorTickEnabled(true);
		GetControlledPawn()->Restart();

		INetworkPredictionInterface* NetworkPredictionInterface = GetPawn() ? InterfaceCast<INetworkPredictionInterface>(GetPawn()->GetMovementComponent()) : NULL;
		if (NetworkPredictionInterface)
		{
			NetworkPredictionInterface->ResetPredictionData_Server();
		}

		ChangeState( NAME_Playing );
		AcknowledgedPawn = NULL;
		ClientRestart(GetPawn());
		if (bAutoManageActiveCameraTarget)
		{
			SetViewTarget(GetPawn());
			ResetCameraMode();
		}
		UpdateNavigationComponents();
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!IsPendingKill() && GetNetMode() != NM_DedicatedServer)
	{
		if (!DebuggingController)
		{
			DebuggingController = ConstructObject<UGameplayDebuggingControllerComponent>(UGameplayDebuggingControllerComponent::StaticClass(), this);
			DebuggingController->RegisterComponent();
			DebuggingController->InitBasicFuncionality();
		}
	}
#endif
}

void APlayerController::AcknowledgePossession(APawn* P)
{
	if (Cast<ULocalPlayer>(Player) != NULL)
	{
		AcknowledgedPawn = P;
		if (P != NULL)
		{
			P->RecalculateBaseEyeHeight();
		}
		ServerAcknowledgePossession(P);
	}
}

void APlayerController::ReceivedPlayer()
{
	if (IsInState(NAME_Spectating))
	{
		if (GetSpectatorPawn() == NULL)
		{
			BeginSpectatingState();
		}
	}
}

FVector APlayerController::GetFocalLocation() const
{
	if (GetPawnOrSpectator())
	{
		return GetPawnOrSpectator()->GetActorLocation();
	}
	else
	{
		return GetSpawnLocation();
	}
}

void APlayerController::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerUE4Version() < VER_UE4_SPLIT_TOUCH_AND_CLICK_ENABLES)
	{
		bEnableTouchEvents = bEnableClickEvents;
	}
}

void APlayerController::GetActorEyesViewPoint( FVector& out_Location, FRotator& out_Rotation ) const
{
	// If we have a Pawn, this is our view point.
	if (GetPawnOrSpectator() != NULL)
	{
		GetPawnOrSpectator()->GetActorEyesViewPoint(out_Location, out_Rotation);
	}
	else
	{
		out_Location = PlayerCameraManager ? PlayerCameraManager->GetCameraLocation() : GetSpawnLocation();
		out_Rotation = GetControlRotation();
	}
}

void APlayerController::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	OutResult.Location = GetFocalLocation();
	OutResult.Rotation = GetControlRotation();
}


void APlayerController::GetPlayerViewPoint( FVector& out_Location, FRotator& out_Rotation ) const
{
	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->GetCameraViewPoint(out_Location, out_Rotation);
	}
	else
	{
		AActor* TheViewTarget = GetViewTarget();

		if( TheViewTarget != NULL )
		{
			out_Location = TheViewTarget->GetActorLocation();
			out_Rotation = TheViewTarget->GetActorRotation();
		}
		else
		{
			Super::GetPlayerViewPoint(out_Location,out_Rotation);
		}
	}
}

void APlayerController::UpdateRotation( float DeltaTime )
{
	// Calculate Delta to be applied on ViewRotation
	FRotator DeltaRot(RotationInput);

	FRotator ViewRotation = GetControlRotation();

	if (PlayerCameraManager)
	{
		PlayerCameraManager->ProcessViewRotation(DeltaTime, ViewRotation, DeltaRot);
	}

	if (!PlayerCameraManager || !PlayerCameraManager->bFollowHmdOrientation)
	{
		if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed())
		{
			GEngine->HMDDevice->ApplyHmdRotation(this, ViewRotation);
		}
	}

	SetControlRotation(ViewRotation);

	APawn* const P = GetPawnOrSpectator();
	if (P)
	{
		P->FaceRotation(ViewRotation, DeltaTime);
	}
}

void APlayerController::FellOutOfWorld(const UDamageType& dmgType) {}

void APlayerController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if ( !IsPendingKill() && (GetNetMode() != NM_Client) )
	{
		// create a new player replication info
		InitPlayerState();
	}

	SpawnPlayerCameraManager();
	ResetCameraMode(); 

	if ( GetNetMode() == NM_Client )
	{
		SpawnDefaultHUD();
	}

	AddCheats();

	bPlayerIsWaiting = true;
	StateName = NAME_Spectating; // Don't use ChangeState, because we want to defer spawning the SpectatorPawn until the Player is received

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!IsPendingKill() && DebuggingController == NULL && GetNetMode() != NM_DedicatedServer)
	{
		DebuggingController = ConstructObject<UGameplayDebuggingControllerComponent>(UGameplayDebuggingControllerComponent::StaticClass(), this);
		DebuggingController->RegisterComponent();
		DebuggingController->InitializeComponent();
	}
#endif

}

bool APlayerController::ServerShortTimeout_Validate()
{
	return true;
}

void APlayerController::ServerShortTimeout_Implementation()
{
	if (!bShortConnectTimeOut)
	{
		UWorld* World = GetWorld();
		check(World);

		bShortConnectTimeOut = true;

		// quick update of pickups and gameobjectives since this player is now relevant
		if (GetWorldSettings()->Pauser != NULL)
		{
			// update everything immediately, as TimeSeconds won't get advanced while paused
			// so otherwise it won't happen at all until the game is unpaused
			// this floods the network, but we're paused, so no gameplay is going on that would care much
			for (FActorIterator It(World); It; ++It)
			{
				AActor* A = *It;
				if (A && !A->IsPendingKill())
				{
					if (!A->bOnlyRelevantToOwner)
					{
						A->ForceNetUpdate();
					}
				}
			}
		}
		else 
		{
			float NetUpdateTimeOffset = (World->GetAuthGameMode()->NumPlayers < 8) ? 0.2f : 0.5f;
			for (FActorIterator It(World); It; ++It)
			{
				AActor* A = *It;
				if (A && !A->IsPendingKill())
				{
					if ( (A->NetUpdateFrequency < 1) && !A->bOnlyRelevantToOwner )
					{
						A->SetNetUpdateTime(FMath::Min(A->NetUpdateTime, World->TimeSeconds + NetUpdateTimeOffset * FMath::FRand()));
					}
				}
			}
		}
	}
}

void APlayerController::AddCheats(bool bForce)
{
	UWorld* World = GetWorld();
	check(World);

	// Assuming that this never gets called for NM_Client without bForce=true
	if ( ((CheatManager == NULL) && (World->GetAuthGameMode() != NULL) && World->GetAuthGameMode()->AllowCheats(this)) || bForce)
	{
		CheatManager = Cast<UCheatManager>(StaticConstructObject(CheatClass, this));
		CheatManager->InitCheatManager();
	}
}

void APlayerController::EnableCheats()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	AddCheats(true);
#else
	AddCheats();
#endif
}


void APlayerController::SpawnDefaultHUD()
{
	if ( Cast<ULocalPlayer>(Player) == NULL )
	{
		return;
	}

	UE_LOG(LogPlayerController, Log, TEXT("SpawnDefaultHUD"));
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.Instigator = Instigator;
	MyHUD = GetWorld()->SpawnActor<AHUD>( SpawnInfo );
}

void APlayerController::CreateTouchInterface()
{
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);

	// do we want to show virtual joysticks?
	if (LocalPlayer && LocalPlayer->ViewportClient && SVirtualJoystick::ShouldDisplayTouchInterface())
	{
		// load what the game wants to show at startup
		FStringAssetReference DefaultTouchInterfaceName = GetDefault<UInputSettings>()->DefaultTouchInterface;

		if (DefaultTouchInterfaceName.IsValid())
		{
			// create the joystick 
			VirtualJoystick = SNew(SVirtualJoystick);

			// add it to the player's viewport
			LocalPlayer->ViewportClient->AddViewportWidgetContent(VirtualJoystick.ToSharedRef());

			// activate this interface if we have it
			UTouchInterface* DefaultTouchInterface = LoadObject<UTouchInterface>(NULL, *DefaultTouchInterfaceName.ToString());
			if (DefaultTouchInterface != NULL)
			{
				DefaultTouchInterface->Activate(VirtualJoystick);
			}
		}
	}
}

void APlayerController::CleanupGameViewport()
{
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);

	if (LocalPlayer && LocalPlayer->ViewportClient && VirtualJoystick.IsValid())
	{
		LocalPlayer->ViewportClient->RemoveViewportWidgetContent(VirtualJoystick.ToSharedRef());
		VirtualJoystick = NULL;
	}
}

AHUD* APlayerController::GetHUD() const
{
	return MyHUD;
}

void APlayerController::GetViewportSize(int32& SizeX, int32& SizeY) const
{
	SizeX = 0;
	SizeY = 0;

	ULocalPlayer* LocPlayer = Cast<ULocalPlayer>(Player);
	if( LocPlayer && LocPlayer->ViewportClient )
	{
		FVector2D ViewportSize;
		LocPlayer->ViewportClient->GetViewportSize(ViewportSize);

		SizeX = ViewportSize.X;
		SizeY = ViewportSize.Y;
	}
}

void APlayerController::Reset()
{
	if ( GetPawn() != NULL )
	{
		PawnPendingDestroy( GetPawn() );
		UnPossess();
	}

	Super::Reset();

	SetViewTarget(this);
	ResetCameraMode();

	bPlayerIsWaiting = !PlayerState->bOnlySpectator;
	ChangeState(NAME_Spectating);
}

void APlayerController::ClientReset_Implementation()
{
	ResetCameraMode();
	SetViewTarget(this);

	bPlayerIsWaiting = !PlayerState->bOnlySpectator;
	ChangeState(NAME_Spectating);
}

void APlayerController::ClientGotoState_Implementation(FName NewState)
{
	ChangeState(NewState);
}

void APlayerController::UnFreeze() {}

bool APlayerController::IsFrozen()
{
	return GetWorldTimerManager().IsTimerActive(this, &APlayerController::UnFreeze);
}

void APlayerController::ServerAcknowledgePossession_Implementation(APawn* P)
{
	UE_LOG(LogPlayerController, Log, TEXT("ServerAcknowledgePossession_Implementation %s"), *GetNameSafe(P));
	AcknowledgedPawn = P;
}

bool APlayerController::ServerAcknowledgePossession_Validate(APawn* P)
{
	if (P)
	{
		// Valid to acknowledge no possessed pawn
		RPC_VALIDATE( !P->HasAnyFlags(RF_ClassDefaultObject) );
	}
	return true;
}

void APlayerController::UnPossess()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (DebuggingController != NULL)
	{
		DebuggingController->UnregisterComponent();
		DebuggingController = NULL;
	}
#endif

	if (GetPawn() != NULL)
	{
		if (Role == ROLE_Authority)
		{
			GetPawn()->SetReplicates(true);
		}
		GetPawn()->UnPossessed();

		if (GetViewTarget() == GetPawn())
		{
			SetViewTarget(this);
		}
	}
	SetPawn(NULL);
}

void APlayerController::ClientSetHUD_Implementation(TSubclassOf<AHUD> NewHUDClass)
{
	if ( MyHUD != NULL )
	{
		MyHUD->Destroy();
		MyHUD = NULL;
	}
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.Instigator = Instigator;
	MyHUD = GetWorld()->SpawnActor<AHUD>(NewHUDClass, SpawnInfo );
}

void APlayerController::CleanupPlayerState()
{
	AGameMode* const GameMode = GetWorld()->GetAuthGameMode();
	if (GameMode)
	{
		GameMode->AddInactivePlayer(PlayerState, this);
	}

	PlayerState = NULL;
}

void APlayerController::OnActorChannelOpen(FInBunch& InBunch, UNetConnection* Connection)
{
	// Attempt to match the player controller to a local viewport (client side)
	InBunch << NetPlayerIndex;
	if (Connection->Driver != NULL && Connection == Connection->Driver->ServerConnection)
	{
		if (NetPlayerIndex == 0)
		{
			// main connection PlayerController
			Connection->HandleClientPlayer(this, Connection); 
		}
		else
		{
			int32 ChildIndex = int32(NetPlayerIndex) - 1;
			if (Connection->Children.IsValidIndex(ChildIndex))
			{
				// received a new PlayerController for an already existing child
				Connection->Children[ChildIndex]->HandleClientPlayer(this, Connection);
			}
			else
			{
				// create a split connection on the client
				UChildConnection* Child = Connection->Driver->CreateChild(Connection); 
				Child->HandleClientPlayer(this, Connection);
				UE_LOG(LogNet, Log, TEXT("Client received PlayerController=%s. Num child connections=%i."), *GetName(), Connection->Children.Num());
			}
		}
	}
}

void APlayerController::OnSerializeNewActor(FOutBunch& OutBunch)
{
	// serialize PlayerIndex as part of the initial bunch for PlayerControllers so they can be matched to the correct client-side viewport
	OutBunch << NetPlayerIndex;
}

void APlayerController::OnNetCleanup(UNetConnection* Connection)
{
	UWorld* World = GetWorld();
	// destroy the PC that was waiting for a swap, if it exists
	if (World != NULL)
	{
		World->DestroySwappedPC(Connection);
	}

	check(UNetConnection::GNetConnectionBeingCleanedUp == NULL);
	UNetConnection::GNetConnectionBeingCleanedUp = Connection;
	//@note: if we ever implement support for splitscreen players leaving a match without the primary player leaving, we'll need to insert
	// a call to ClearOnlineDelegates() here so that PlayerController.ClearOnlineDelegates can use the correct ControllerId (which lives
	// in ULocalPlayer)
	Player = NULL;
	NetConnection = NULL;
	Destroy( true );		
	UNetConnection::GNetConnectionBeingCleanedUp = NULL;
}

void APlayerController::ClientReceiveLocalizedMessage_Implementation( TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject )
{
	// Wait for player to be up to date with replication when joining a server, before stacking up messages
	if ( GetNetMode() == NM_DedicatedServer || GetWorld()->GameState == NULL )
	{
		return;
	}

	FClientReceiveData ClientData;
	ClientData.LocalPC = this;
	ClientData.MessageIndex = Switch;
	ClientData.RelatedPlayerState_1 = RelatedPlayerState_1;
	ClientData.RelatedPlayerState_2 = RelatedPlayerState_2;
	ClientData.OptionalObject = OptionalObject;

	Message->GetDefaultObject<ULocalMessage>()->ClientReceive( ClientData );
}

void APlayerController::ClientPlaySound_Implementation(USoundBase* Sound, float VolumeMultiplier /* = 1.f */, float PitchMultiplier /* = 1.f */)
{
	FVector AudioPosition = GetFocalLocation();
	UGameplayStatics::PlaySoundAtLocation( this, Sound, AudioPosition, VolumeMultiplier, PitchMultiplier );
}

void APlayerController::ClientPlaySoundAtLocation_Implementation(USoundBase* Sound, FVector Location, float VolumeMultiplier /* = 1.f */, float PitchMultiplier /* = 1.f */)
{
	UGameplayStatics::PlaySoundAtLocation( this, Sound, Location, VolumeMultiplier, PitchMultiplier );
}

void APlayerController::ClientMessage_Implementation( const FString& S, FName Type, float MsgLifeTime )
{
	if ( GetNetMode() == NM_DedicatedServer || GetWorld()->GameState == NULL )
	{
		return;
	}

	if (Type == NAME_None)
	{
		Type = FName(TEXT("Event"));
	}

	ClientTeamMessage(PlayerState, S, Type, MsgLifeTime);
}

void APlayerController::ClientTeamMessage_Implementation( APlayerState* SenderPlayerState, const FString& S, FName Type, float MsgLifeTime  )
{
	FString SMod = S;
	static FName NAME_Say = FName(TEXT("Say"));
	if( (Type == NAME_Say) && ( SenderPlayerState != NULL ) )
	{
		SMod = FString::Printf(TEXT("%s: %s"), *SenderPlayerState->PlayerName, *SMod);
	}

	// since this is on the client, we can assume that if Player exists, it is a LocalPlayer
	if( Player != NULL && CastChecked<ULocalPlayer>(Player)->ViewportClient->ViewportConsole )
	{
		CastChecked<ULocalPlayer>(Player)->ViewportClient->ViewportConsole->OutputText( SMod );
	}
}

bool APlayerController::ServerToggleAILogging_Validate()
{
	return true;
}

void APlayerController::ServerToggleAILogging_Implementation()
{
	if (CheatManager)
	{
		CheatManager->ServerToggleAILogging();
	}
}

bool APlayerController::ServerReplicateMessageToAIDebugView_Validate(class APawn* InPawn, uint32 InMessage, uint32 DataView)
{
	return true;
}

void APlayerController::ServerReplicateMessageToAIDebugView_Implementation(class APawn* InPawn, uint32 InMessage, uint32 DataView)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UGameplayDebuggingComponent* DebuggingComponent = InPawn ? InPawn->GetDebugComponent(true) : NULL;
	if(DebuggingComponent)
	{
		DebuggingComponent->ServerReplicateData((EDebugComponentMessage::Type)InMessage, (EAIDebugDrawDataView::Type)DataView);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void APlayerController::PawnLeavingGame()
{
	if (GetPawn() != NULL)
	{
		GetPawn()->Destroy();
		SetPawn(NULL);
	}
}


void APlayerController::Destroyed()
{
	if (GetPawn() != NULL)
	{
		// Handle players leaving the game
		if (Player == NULL && Role == ROLE_Authority)
		{
			PawnLeavingGame();
		}
		else
		{
			UnPossess();
		}
	}

	if (GetSpectatorPawn() != NULL)
	{
		DestroySpectatorPawn();
	}
	if ( MyHUD != NULL )
	{
		MyHUD->Destroy();
		MyHUD = NULL;
	}

	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->Destroy();
		PlayerCameraManager = NULL;
	}

	// Tells the game info to forcibly remove this player's CanUnpause delegates from its list of Pausers.
	// Prevents the game from being stuck in a paused state when a PC that paused the game is destroyed before the game is unpaused.
	AGameMode* const GameMode = GetWorld()->GetAuthGameMode();
	if (GameMode)
	{
		GameMode->ForceClearUnpauseDelegates(this);
	}

	PlayerInput = NULL;
	CheatManager = NULL;

	if (VirtualJoystick.IsValid())
	{
		Cast<ULocalPlayer>(Player)->ViewportClient->RemoveViewportWidgetContent(VirtualJoystick.ToSharedRef());
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (DebuggingController != NULL)
	{
		DebuggingController->UnregisterComponent();
		DebuggingController = NULL;
	}
#endif

	Super::Destroyed();
}

void APlayerController::FOV(float F)
{
	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->SetFOV(F);
	}
}

void APlayerController::PreClientTravel( const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel )
{
}

void APlayerController::Camera( FName NewMode )
{
	ServerCamera(NewMode);
}

void APlayerController::ServerCamera_Implementation( FName NewMode )
{
	SetCameraMode(NewMode);
}

bool APlayerController::ServerCamera_Validate( FName NewMode )
{
	RPC_VALIDATE( NewMode.IsValid() );
	return true;
}

void APlayerController::ClientSetCameraMode_Implementation( FName NewCamMode )
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->CameraStyle = NewCamMode;
	}
}


void APlayerController::SetCameraMode( FName NewCamMode )
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->CameraStyle = NewCamMode;
	}
	
	if ( GetNetMode() == NM_DedicatedServer )
	{
		ClientSetCameraMode( NewCamMode );
	}
}

void APlayerController::ResetCameraMode()
{
	FName DefaultMode = NAME_Default;
	if (PlayerCameraManager)
	{
		DefaultMode = PlayerCameraManager->CameraStyle;
	}

	SetCameraMode(DefaultMode);
}

void APlayerController::ClientSetCameraFade_Implementation(bool bEnableFading, FColor FadeColor, FVector2D FadeAlpha, float FadeTime, bool bFadeAudio)
{
	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->bEnableFading = bEnableFading;
		if (PlayerCameraManager->bEnableFading)
		{
			PlayerCameraManager->FadeColor = FadeColor;
			PlayerCameraManager->FadeAlpha = FadeAlpha;
			PlayerCameraManager->FadeTime = FadeTime;
			PlayerCameraManager->FadeTimeRemaining = FadeTime;
			PlayerCameraManager->bFadeAudio = bFadeAudio;
		}
 		else
 		{
 			// Make sure FadeAmount finishes at the correct value
 			PlayerCameraManager->FadeAmount = PlayerCameraManager->FadeAlpha.Y;
 		}
	}
}


void APlayerController::SendClientAdjustment()
{
	if (AcknowledgedPawn != GetPawn() && !GetSpectatorPawn())
	{
		return;
	}

	// Server sends updates.
	// Note: we do this for both the pawn and spectator in case an implementation has a networked spectator.
	APawn* RemotePawn = GetPawnOrSpectator();
	if (RemotePawn && (GetNetMode() < NM_Client) && (RemotePawn->GetRemoteRole() == ROLE_AutonomousProxy))
	{
		INetworkPredictionInterface* NetworkPredictionInterface = InterfaceCast<INetworkPredictionInterface>(RemotePawn->GetMovementComponent());
		if (NetworkPredictionInterface)
		{
			NetworkPredictionInterface->SendClientAdjustment();
		}
	}
}


void APlayerController::ClientCapBandwidth_Implementation(int32 Cap)
{
	ClientCap = Cap;
	if( (Player != NULL) && (Player->CurrentNetSpeed > Cap) )
	{
		SetNetSpeed(Cap);
	}
}


void APlayerController::UpdatePing(float InPing)
{
	if (PlayerState != NULL)
	{
		PlayerState->UpdatePing(InPing);
	}
}


void APlayerController::SetSpawnLocation(const FVector& NewLocation)
{
	SpawnLocation = NewLocation;
}


void APlayerController::SetInitialLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation)
{
	Super::SetInitialLocationAndRotation(NewLocation, NewRotation);
	SetSpawnLocation(NewLocation);
	if (GetSpectatorPawn())
	{
		GetSpectatorPawn()->TeleportTo(NewLocation, NewRotation, false, true);
	}
}


bool APlayerController::ServerUpdateCamera_Validate(FVector_NetQuantize CamLoc, int32 CamPitchAndYaw)
{
	return true;
}

void APlayerController::ServerUpdateCamera_Implementation(FVector_NetQuantize CamLoc, int32 CamPitchAndYaw)
{
	FPOV NewPOV;
	NewPOV.Location = CamLoc;
	
	NewPOV.Rotation.Yaw = FRotator::DecompressAxisFromShort( (CamPitchAndYaw >> 16) & 65535 );
	NewPOV.Rotation.Pitch = FRotator::DecompressAxisFromShort(CamPitchAndYaw & 65535);

	if (PlayerCameraManager)
	{
		if ( PlayerCameraManager->bDebugClientSideCamera )
		{
			// show differences (on server) between local and replicated camera
			const FVector PlayerCameraLoc = PlayerCameraManager->GetCameraLocation();

			DrawDebugSphere(GetWorld(), PlayerCameraLoc, 10, 10, FColor::Green );
			DrawDebugSphere(GetWorld(), NewPOV.Location, 10, 10, FColor::Yellow );
			DrawDebugLine(GetWorld(), PlayerCameraLoc, PlayerCameraLoc + 100*PlayerCameraManager->GetCameraRotation().Vector(), FColor::Green);
			DrawDebugLine(GetWorld(), NewPOV.Location, NewPOV.Location + 100*NewPOV.Rotation.Vector(), FColor::Yellow);
		}
		else
		{
			//@TODO: CAMERA: Fat pipe
			FMinimalViewInfo NewInfo = PlayerCameraManager->CameraCache.POV;
			NewInfo.Location = NewPOV.Location;
			NewInfo.Rotation = NewPOV.Rotation;
			PlayerCameraManager->FillCameraCache(NewInfo);
		}
	}
}

void APlayerController::RestartLevel()
{
	if( GetNetMode()==NM_Standalone )
	{
		ClientTravel( TEXT("?restart"), TRAVEL_Relative );
	}
}

void APlayerController::LocalTravel( const FString& FURL )
{
	if( GetNetMode()==NM_Standalone )
	{
		ClientTravel( FURL, TRAVEL_Relative );
	}
}

void APlayerController::ClientReturnToMainMenu_Implementation(const FString& ReturnReason)
{
	UWorld* World = GetWorld();
	if (Player)
	{
		Player->HandleDisconnect(World, World->GetNetDriver());
	}
	else
	{
		GEngine->HandleDisconnect(World, World->GetNetDriver());
	}
}


bool APlayerController::SetPause( bool bPause, FCanUnpause CanUnpauseDelegate)
{
	bool bResult = false;
	if (GetNetMode() != NM_Client)
	{
		AGameMode* const GameMode = GetWorld()->GetAuthGameMode();
		if (bPause)
		{
			// Pause gamepad rumbling too if needed
			bResult = GameMode->SetPause(this, CanUnpauseDelegate);
		}
		else
		{
			GameMode->ClearPause();
		}
	}
	return bResult;
}

bool APlayerController::IsPaused()
{
	return GetWorldSettings()->Pauser != NULL;
}

void APlayerController::Pause()
{
	ServerPause();
}

bool APlayerController::ServerPause_Validate()
{
#if UE_BUILD_SHIPPING
	// Don't let clients remotely pause the game in shipping builds.
	return IsLocalController();
#else
	return true;
#endif
}

void APlayerController::ServerPause_Implementation()
{
	SetPause(!IsPaused());
}

void APlayerController::SetName(const FString& S)
{
	if (!S.IsEmpty())
	{
		// Games can override this to persist name on the client if desired
		ServerChangeName(S);
	}
}

void APlayerController::ServerChangeName_Implementation( const FString& S )
{
	if (!S.IsEmpty())
	{
		GetWorld()->GetAuthGameMode()->ChangeName( this, S, true );
	}
}

bool APlayerController::ServerChangeName_Validate( const FString& S )
{
	RPC_VALIDATE( !S.IsEmpty() );
	return true;
}

void APlayerController::SwitchLevel(const FString& FURL)
{
	const ENetMode NetMode = GetNetMode();
	if (NetMode == NM_Standalone || NetMode == NM_ListenServer)
	{
		GetWorld()->ServerTravel(FURL);
	}
}

void APlayerController::NotifyLoadedWorld(FName WorldPackageName, bool bFinalDest)
{
	// place the camera at the first playerstart we can find
	SetViewTarget(this);
	
	for( FActorIterator It(GetWorld()); It; ++It )
	{
		APlayerStart* P = Cast<APlayerStart>(*It);
		if (P)
		{
			FRotator SpawnRotation(ForceInit);
			SpawnRotation.Yaw = P->GetActorRotation().Yaw;
			SetInitialLocationAndRotation(P->GetActorLocation(), SpawnRotation);
			break;
		}
	}
}

void APlayerController::GameHasEnded(AActor* EndGameFocus, bool bIsWinner)
{
	// and transition to the game ended state
	SetViewTarget(EndGameFocus);
	ClientGameEnded(EndGameFocus, bIsWinner);
}


void APlayerController::ClientGameEnded_Implementation(AActor* EndGameFocus, bool bIsWinner)
{
	SetViewTarget(EndGameFocus);
}

bool APlayerController::GetHitResultUnderCursor(ECollisionChannel TraceChannel, bool bTraceComplex, FHitResult& HitResult) const
{
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);
	bool bHit = false;
	if (LocalPlayer)
	{
		bHit = GetHitResultAtScreenPosition(LocalPlayer->ViewportClient->GetMousePosition(), TraceChannel, bTraceComplex, HitResult);
	}

	if(!bHit)	//If there was no hit we reset the results. This is redundent but helps Blueprint users
	{
		HitResult = FHitResult();
	}

	return bHit;
}

bool APlayerController::GetHitResultUnderCursorByChannel(ETraceTypeQuery TraceChannel, bool bTraceComplex, FHitResult& HitResult) const
{
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);
	bool bHit = false;
	if (LocalPlayer)
	{
		bHit = GetHitResultAtScreenPosition(LocalPlayer->ViewportClient->GetMousePosition(), TraceChannel, bTraceComplex, HitResult);
	}

	if(!bHit)	//If there was no hit we reset the results. This is redundent but helps Blueprint users
	{
		HitResult = FHitResult();
	}

	return bHit;
}

bool APlayerController::GetHitResultUnderCursorForObjects(const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, FHitResult& HitResult) const
{
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);
	bool bHit = false;
	if (LocalPlayer)
	{
		bHit = GetHitResultAtScreenPosition(LocalPlayer->ViewportClient->GetMousePosition(), ObjectTypes, bTraceComplex, HitResult);
	}

	if(!bHit)	//If there was no hit we reset the results. This is redundent but helps Blueprint users
	{
		HitResult = FHitResult();
	}

	return bHit;
}

bool APlayerController::GetHitResultUnderFinger(ETouchIndex::Type FingerIndex, ECollisionChannel TraceChannel, bool bTraceComplex, FHitResult& HitResult) const
{
	bool bHit = false;
	if (PlayerInput)
	{
		FVector2D TouchPosition;
		bool bIsPressed = false;
		GetInputTouchState(FingerIndex, TouchPosition.X, TouchPosition.Y, bIsPressed);
		if (bIsPressed)
		{
			bHit = GetHitResultAtScreenPosition(TouchPosition, TraceChannel, bTraceComplex, HitResult);
		}
	}

	if(!bHit)	//If there was no hit we reset the results. This is redundent but helps Blueprint users
	{
		HitResult = FHitResult();
	}

	return bHit;
}

bool APlayerController::GetHitResultUnderFingerByChannel(ETouchIndex::Type FingerIndex, ETraceTypeQuery TraceChannel, bool bTraceComplex, FHitResult& HitResult) const
{
	bool bHit = false;
	if (PlayerInput)
	{
		FVector2D TouchPosition;
		bool bIsPressed = false;
		GetInputTouchState(FingerIndex, TouchPosition.X, TouchPosition.Y, bIsPressed);
		if (bIsPressed)
		{
			bHit = GetHitResultAtScreenPosition(TouchPosition, TraceChannel, bTraceComplex, HitResult);
		}
	}

	if(!bHit)	//If there was no hit we reset the results. This is redundent but helps Blueprint users
	{
		HitResult = FHitResult();
	}

	return bHit;
}

bool APlayerController::GetHitResultUnderFingerForObjects(ETouchIndex::Type FingerIndex, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, FHitResult& HitResult) const
{
	bool bHit = false;
	if (PlayerInput)
	{
		FVector2D TouchPosition;
		bool bIsPressed = false;
		GetInputTouchState(FingerIndex, TouchPosition.X, TouchPosition.Y, bIsPressed);
		if (bIsPressed)
		{
			bHit = GetHitResultAtScreenPosition(TouchPosition, ObjectTypes, bTraceComplex, HitResult);
		}
	}

	if(!bHit)	//If there was no hit we reset the results. This is redundent but helps Blueprint users
	{
		HitResult = FHitResult();
	}

	return bHit;
}

void APlayerController::DeprojectMousePositionToWorld(FVector & WorldLocation, FVector & WorldDirection) const
{
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);
	if (LocalPlayer != NULL && LocalPlayer->ViewportClient != NULL)
	{
		FVector2D ScreenPosition = LocalPlayer->ViewportClient->GetMousePosition();
		DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, WorldLocation, WorldDirection);
	}
}

void APlayerController::DeprojectScreenPositionToWorld(float ScreenX, float ScreenY, FVector & WorldLocation, FVector & WorldDirection) const
{
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);

	if (LocalPlayer != NULL && LocalPlayer->ViewportClient != NULL && LocalPlayer->ViewportClient->Viewport != NULL)
	{
		// Create a view family for the game viewport
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			LocalPlayer->ViewportClient->Viewport,
			GetWorld()->Scene,
			LocalPlayer->ViewportClient->EngineShowFlags)
			.SetRealtimeUpdate(true));

		// Calculate a view where the player is to update the streaming from the players start location
		FVector ViewLocation;
		FRotator ViewRotation;
		FSceneView* SceneView = LocalPlayer->CalcSceneView(&ViewFamily, /*out*/ ViewLocation, /*out*/ ViewRotation, LocalPlayer->ViewportClient->Viewport);

		if (SceneView)
		{
			const FVector2D ScreenPosition(ScreenX, ScreenY);
			SceneView->DeprojectFVector2D(ScreenPosition, WorldLocation, WorldDirection);
		}
	}
}


bool APlayerController::GetHitResultAtScreenPosition(const FVector2D ScreenPosition, const ECollisionChannel TraceChannel, bool bTraceComplex, FHitResult& HitResult) const
{
	// Early out if we clicked on a HUD hitbox
	if( GetHUD() != NULL && GetHUD()->GetHitBoxAtCoordinates(ScreenPosition, true) )
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);

	if (LocalPlayer != NULL && LocalPlayer->ViewportClient != NULL && LocalPlayer->ViewportClient->Viewport != NULL)
	{
		// Create a view family for the game viewport
		FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues(
			LocalPlayer->ViewportClient->Viewport,
			GetWorld()->Scene,
			LocalPlayer->ViewportClient->EngineShowFlags )
			.SetRealtimeUpdate(true) );


		// Calculate a view where the player is to update the streaming from the players start location
		FVector ViewLocation;
		FRotator ViewRotation;
		FSceneView* SceneView = LocalPlayer->CalcSceneView( &ViewFamily, /*out*/ ViewLocation, /*out*/ ViewRotation, LocalPlayer->ViewportClient->Viewport );

		if (SceneView)
		{
			FVector WorldOrigin;
			FVector WorldDirection;
			SceneView->DeprojectFVector2D(ScreenPosition, WorldOrigin, WorldDirection);

			return GetWorld()->LineTraceSingle(HitResult, WorldOrigin, WorldOrigin + WorldDirection * 100000.f, TraceChannel, FCollisionQueryParams("ClickableTrace", bTraceComplex));
		}
	}

	return false;
}

bool APlayerController::GetHitResultAtScreenPosition(const FVector2D ScreenPosition, const ETraceTypeQuery TraceChannel, bool bTraceComplex, FHitResult& HitResult) const
{
	// Early out if we clicked on a HUD hitbox
	if (GetHUD() != NULL && GetHUD()->GetHitBoxAtCoordinates(ScreenPosition, true))
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);

	if (LocalPlayer != NULL && LocalPlayer->ViewportClient != NULL && LocalPlayer->ViewportClient->Viewport != NULL)
	{
		// Create a view family for the game viewport
		FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues(
			LocalPlayer->ViewportClient->Viewport,
			GetWorld()->Scene,
			LocalPlayer->ViewportClient->EngineShowFlags )
			.SetRealtimeUpdate(true) );


		// Calculate a view where the player is to update the streaming from the players start location
		FVector ViewLocation;
		FRotator ViewRotation;
		FSceneView* SceneView = LocalPlayer->CalcSceneView( &ViewFamily, /*out*/ ViewLocation, /*out*/ ViewRotation, LocalPlayer->ViewportClient->Viewport );

		if (SceneView)
		{
			FVector WorldOrigin;
			FVector WorldDirection;
			SceneView->DeprojectFVector2D(ScreenPosition, WorldOrigin, WorldDirection);

			return GetWorld()->LineTraceSingle(HitResult, WorldOrigin, WorldOrigin + WorldDirection * 100000.f, UEngineTypes::ConvertToCollisionChannel(TraceChannel), FCollisionQueryParams("ClickableTrace", bTraceComplex));
		}
	}

	return false;
}

bool APlayerController::GetHitResultAtScreenPosition(const FVector2D ScreenPosition, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, FHitResult& HitResult) const
{
	// Early out if we clicked on a HUD hitbox
	if (GetHUD() != NULL && GetHUD()->GetHitBoxAtCoordinates(ScreenPosition, true))
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);

	if (LocalPlayer != NULL && LocalPlayer->ViewportClient != NULL && LocalPlayer->ViewportClient->Viewport != NULL)
	{
		// Create a view family for the game viewport
		FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues(
			LocalPlayer->ViewportClient->Viewport,
			GetWorld()->Scene,
			LocalPlayer->ViewportClient->EngineShowFlags )
			.SetRealtimeUpdate(true) );


		// Calculate a view where the player is to update the streaming from the players start location
		FVector ViewLocation;
		FRotator ViewRotation;
		FSceneView* SceneView = LocalPlayer->CalcSceneView( &ViewFamily, /*out*/ ViewLocation, /*out*/ ViewRotation, LocalPlayer->ViewportClient->Viewport );

		if (SceneView)
		{
			FVector WorldOrigin;
			FVector WorldDirection;
			SceneView->DeprojectFVector2D(ScreenPosition, WorldOrigin, WorldDirection);

			FCollisionObjectQueryParams ObjParam(ObjectTypes);
			
			return GetWorld()->LineTraceSingle(HitResult, WorldOrigin, WorldOrigin + WorldDirection * 100000.f, FCollisionQueryParams("ClickableTrace", bTraceComplex), ObjParam);
		}
	}

	return false;
}

/* PlayerTick is only called if the PlayerController has a PlayerInput object.  Therefore, it will not be called on servers for non-locally controlled playercontrollers. */
void APlayerController::PlayerTick( float DeltaTime )
{
	if (!bShortConnectTimeOut)
	{
		bShortConnectTimeOut = true;
		ServerShortTimeout();
	}

	if (ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player))
	{
		if (bEnableMouseOverEvents)
		{
			FHitResult HitResult;
			const bool bHit = GetHitResultAtScreenPosition(LocalPlayer->ViewportClient->GetMousePosition(), CurrentClickTraceChannel, true, /*out*/ HitResult);

			UPrimitiveComponent* PreviousComponent = CurrentClickablePrimitive.Get();
			UPrimitiveComponent* CurrentComponent = (bHit ? HitResult.Component.Get() : NULL);

			UPrimitiveComponent::DispatchMouseOverEvents(PreviousComponent, CurrentComponent);

			CurrentClickablePrimitive = CurrentComponent;
		}

		if (bEnableTouchOverEvents)
		{
			for (int32 TouchIndexInt = 0; TouchIndexInt < EKeys::NUM_TOUCH_KEYS; ++TouchIndexInt)
			{
				const ETouchIndex::Type FingerIndex = ETouchIndex::Type(TouchIndexInt);

				FHitResult HitResult;
				const bool bHit = GetHitResultUnderFinger(FingerIndex, CurrentClickTraceChannel, true, /*out*/ HitResult);

				UPrimitiveComponent* PreviousComponent = CurrentTouchablePrimitives[TouchIndexInt].Get();
				UPrimitiveComponent* CurrentComponent = (bHit ? HitResult.Component.Get() : NULL);

				UPrimitiveComponent::DispatchTouchOverEvents(FingerIndex, PreviousComponent, CurrentComponent);

				CurrentTouchablePrimitives[TouchIndexInt] = CurrentComponent;
			}
		}
	}

	ProcessPlayerInput(DeltaTime, DeltaTime == 0.f);
	ProcessForceFeedback(DeltaTime, DeltaTime == 0.f);


	if ((Player != NULL) && (Player->PlayerController == this))
	{
		// Validate current state
		bool bUpdateRotation = false;
		if (IsInState(NAME_Playing))
		{
			if( GetPawn() == NULL )
			{
				ChangeState(NAME_Inactive);
			}
			else if (Player && GetPawn() && GetPawn() == AcknowledgedPawn)
			{
				bUpdateRotation = true;
			}
		}
		
		if ( IsInState(NAME_Inactive) )
		{
			if (Role < ROLE_Authority)
			{
				SafeServerCheckClientPossession();
			}

			bUpdateRotation = !IsFrozen();
		}
		else if ( IsInState(NAME_Spectating) )
		{
			if (Role < ROLE_Authority)
			{
				SafeServerUpdateSpectatorState();
			}

			bUpdateRotation = true;
		}

		// Update rotation
		if (bUpdateRotation)
		{
			UpdateRotation(DeltaTime);
		}
	}
}

void APlayerController::FlushPressedKeys()
{
	if (PlayerInput)
	{
		PlayerInput->FlushPressedKeys();
	}
}

bool APlayerController::InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	bool bResult = false;

	if (PlayerInput)
	{
		bResult = PlayerInput->InputKey(Key, EventType, AmountDepressed, bGamepad);

		// TODO: Allow click key(s?) to be defined
		if (bEnableClickEvents && Key == EKeys::LeftMouseButton)
		{
			const FVector2D MousePosition = CastChecked<ULocalPlayer>(Player)->ViewportClient->GetMousePosition();
			UPrimitiveComponent* ClickedPrimitive = NULL;
			if (bEnableMouseOverEvents)
			{
				ClickedPrimitive = CurrentClickablePrimitive.Get();
			}
			else
			{
				FHitResult HitResult;
				const bool bHit = GetHitResultAtScreenPosition(MousePosition, CurrentClickTraceChannel, true, HitResult);
				if (bHit)
				{
					ClickedPrimitive = HitResult.Component.Get();
				}
			}
			if( GetHUD() )
			{
				if (GetHUD()->UpdateAndDispatchHitBoxClickEvents(MousePosition, EventType, false))
				{
					ClickedPrimitive = NULL;
				}
			}

			if (ClickedPrimitive)
			{
				switch(EventType)
				{
				case IE_Pressed:
				case IE_DoubleClick:
					ClickedPrimitive->DispatchOnClicked();
					break;

				case IE_Released:
					ClickedPrimitive->DispatchOnReleased();
					break;
				}
			}

			bResult = true;
		}
	}

	return bResult;
}

bool APlayerController::InputAxis(FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	bool bResult = false;
	
	if (PlayerInput)
	{
		bResult = PlayerInput->InputAxis(Key, Delta, DeltaTime, NumSamples, bGamepad);
	}

	return bResult;
}

bool APlayerController::InputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex)
{
	bool bResult = false;

	if (PlayerInput)
	{
		bResult = PlayerInput->InputTouch(Handle, Type, TouchLocation, DeviceTimestamp, TouchpadIndex);

		if (bEnableTouchEvents || bEnableTouchOverEvents)
		{
			const ETouchIndex::Type FingerIndex = ETouchIndex::Type(Handle);

			FHitResult HitResult;
			const bool bHit = GetHitResultAtScreenPosition(TouchLocation, CurrentClickTraceChannel, true, HitResult);

			UPrimitiveComponent* PreviousComponent = CurrentTouchablePrimitives[Handle].Get();
			UPrimitiveComponent* CurrentComponent = (bHit ? HitResult.Component.Get() : NULL);

			if (GetHUD())
			{
				if (Type == ETouchType::Began || Type == ETouchType::Ended)
				{
					if (GetHUD()->UpdateAndDispatchHitBoxClickEvents(TouchLocation, (Type == ETouchType::Began ? EInputEvent::IE_Pressed : EInputEvent::IE_Released), true))
					{
						CurrentComponent = NULL;
					}
				}
			}

			switch(Type)
			{
			case ETouchType::Began:
				// Give it a begin touch
				if (bEnableTouchEvents && (CurrentComponent != NULL))
				{
					CurrentComponent->DispatchOnInputTouchBegin(FingerIndex);
				}

				// Give a touch enter event
				if (bEnableTouchOverEvents)
				{
					UPrimitiveComponent::DispatchTouchOverEvents(FingerIndex, PreviousComponent, CurrentComponent);
					CurrentTouchablePrimitives[Handle] = CurrentComponent;
				}
				break;
			case ETouchType::Ended:
				// Give it a touch exit
				if (bEnableTouchEvents && (CurrentComponent != NULL))
				{
					CurrentComponent->DispatchOnInputTouchEnd(FingerIndex);
				}

				// Give it a end touch
				if (bEnableTouchOverEvents)
				{
					// Handle the case where the finger moved faster than tick, and is being released over a different component than it was last dragged over
					if ((PreviousComponent != CurrentComponent) && (PreviousComponent != NULL))
					{
						// First notify the old component that the touch left it to go to the current component
						UPrimitiveComponent::DispatchTouchOverEvents(FingerIndex, PreviousComponent, CurrentComponent);
					}

					// Now notify that the current component is being released and thus the touch is leaving it
					PreviousComponent = CurrentComponent;
					CurrentComponent = NULL;
					UPrimitiveComponent::DispatchTouchOverEvents(FingerIndex, PreviousComponent, CurrentComponent);
					CurrentTouchablePrimitives[Handle] = CurrentComponent;
				}
				break;
			default:
				break;
			};
		}
	}

	return bResult;
}

bool APlayerController::InputMotion(const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration)
{
	bool bResult = false;

	if (PlayerInput)
	{
		bResult = PlayerInput->InputMotion(Tilt, RotationRate, Gravity, Acceleration);
	}

	return bResult;
}

bool APlayerController::ShouldShowMouseCursor() const
{
	return bShowMouseCursor;
}

EMouseCursor::Type APlayerController::GetMouseCursor() const
{
	if (ShouldShowMouseCursor())
	{
		return CurrentMouseCursor;
	}

	return EMouseCursor::None;
}

void APlayerController::SetupInputComponent()
{
	// A subclass could create a different InputComponent class but still want the default bindings
	if (InputComponent == NULL)
	{
		InputComponent = ConstructObject<UInputComponent>(UInputComponent::StaticClass(), this, TEXT("PC_InputComponent0"));
		InputComponent->RegisterComponent();
	}

	// Only do this if this actor is of a blueprint class
	UBlueprintGeneratedClass* BGClass = Cast<UBlueprintGeneratedClass>(GetClass());
	if(BGClass != NULL)
	{
		InputComponent->bBlockInput = bBlockInput;
		UInputDelegateBinding::BindInputDelegates(BGClass, InputComponent);
	}

	if (GetNetMode() != NM_DedicatedServer && InputComponent && DebuggingController)
	{
		DebuggingController->BindActivationKeys();
	}
}


void APlayerController::BuildInputStack(TArray<UInputComponent*>& InputStack)
{
	// Controlled pawn gets last dibs on the input stack
	APawn* ControlledPawn = GetPawnOrSpectator();
	if (ControlledPawn)
	{
		if (ControlledPawn->InputEnabled())
		{
			// Get the explicit input component that is created upon Pawn possession. This one gets last dibs.
			if (ControlledPawn->InputComponent)
			{
				InputStack.Push(ControlledPawn->InputComponent);
			}

			// See if there is another InputComponent that was added to the Pawn's components array (possibly by script).
			TArray<UInputComponent*> Components;
			ControlledPawn->GetComponents(Components);

			for (int32 i=0; i < Components.Num(); i++)
			{
				UInputComponent* PawnInputComponent = Components[i];
				if (PawnInputComponent != ControlledPawn->InputComponent)
				{
					InputStack.Push(PawnInputComponent);
				}
			}
		}
	}

	// LevelScriptActors are put on the stack next
	for (ULevel* Level : GetWorld()->GetLevels())
	{
		ALevelScriptActor* ScriptActor = Level->GetLevelScriptActor();
		if (ScriptActor)
		{
			if (ScriptActor->InputEnabled() && ScriptActor->InputComponent)
			{
				InputStack.Push(ScriptActor->InputComponent);
			}
		}
	}

	if (InputEnabled())
	{
		InputStack.Push(InputComponent);
	}

	// Components pushed on to the stack get priority
	for (int32 Idx=0; Idx<CurrentInputStack.Num(); ++Idx)
	{
		UInputComponent* IC = CurrentInputStack[Idx].Get();
		if (IC)
		{
			InputStack.Push(IC);
		}
	}
}

void APlayerController::ProcessPlayerInput(const float DeltaTime, const bool bGamePaused)
{
	// process all input components in the stack, top down
	TArray<UInputComponent*> InputStack;

	BuildInputStack(InputStack);

	// process the desired components
	PlayerInput->ProcessInputStack(InputStack, DeltaTime, bGamePaused);
}

void APlayerController::PreProcessInput(const float DeltaTime, const bool bGamePaused)
{
}

void APlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if( IgnoreLookInput )
	{
		// zero look inputs
		RotationInput = FRotator::ZeroRotator;
	}
}

void APlayerController::ResetIgnoreInputFlags()
{
	IgnoreMoveInput = GetDefault<APlayerController>()->IgnoreMoveInput;
	IgnoreLookInput = GetDefault<APlayerController>()->IgnoreLookInput;
}

void APlayerController::SetCinematicMode( bool bInCinematicMode, bool bAffectsMovement, bool bAffectsTurning)
{
	if ( bAffectsMovement && (bInCinematicMode != bCinemaDisableInputMove) )
	{
		SetIgnoreMoveInput(bInCinematicMode);
		bCinemaDisableInputMove = bInCinematicMode;
	}
	if ( bAffectsTurning && (bInCinematicMode != bCinemaDisableInputLook) )
	{
		SetIgnoreLookInput(bInCinematicMode);
		bCinemaDisableInputLook = bInCinematicMode;
	}
}


void APlayerController::SetIgnoreMoveInput( bool bNewMoveInput )
{
	IgnoreMoveInput = FMath::Max( IgnoreMoveInput + (bNewMoveInput ? +1 : -1), 0 );
	//`Log("IgnoreMove: " $ IgnoreMoveInput);
}

bool APlayerController::IsMoveInputIgnored() const
{
	return (IgnoreMoveInput > 0);
}


void APlayerController::SetIgnoreLookInput( bool bNewLookInput )
{
	IgnoreLookInput = FMath::Max( IgnoreLookInput + (bNewLookInput ? +1 : -1), 0 );
	//`Log("IgnoreLook: " $ IgnoreLookInput);
}

bool APlayerController::IsLookInputIgnored() const
{
	return (IgnoreLookInput > 0);
}


/** returns whether this Controller is a locally controlled PlayerController
 * @note not valid until the Controller is completely spawned (i.e, unusable in Pre/PostInitializeComponents())
 */
void APlayerController::SetViewTargetWithBlend(AActor* NewViewTarget, float BlendTime, EViewTargetBlendFunction BlendFunc, float BlendExp, bool bLockOutgoing)
{
	FViewTargetTransitionParams TransitionParams;
	TransitionParams.BlendTime = BlendTime;
	TransitionParams.BlendFunction = BlendFunc;
	TransitionParams.BlendExp = BlendExp;
	TransitionParams.bLockOutgoing = bLockOutgoing;

	SetViewTarget(NewViewTarget, TransitionParams);
}

void APlayerController::ClientSetViewTarget_Implementation( AActor* A, FViewTargetTransitionParams TransitionParams )
{
	if (PlayerCameraManager && !PlayerCameraManager->bClientSimulatingViewTarget)
	{
		if( A == NULL )
		{
			ServerVerifyViewTarget();
			return;
		}
		// don't force view to self while unpossessed (since server may be doing it having destroyed the pawn)
		if ( IsInState(NAME_Inactive) && A == this )
		{
			return;
		}
		SetViewTarget(A, TransitionParams);
	}
}

bool APlayerController::ServerVerifyViewTarget_Validate()
{
	return true;
}

void APlayerController::ServerVerifyViewTarget_Implementation()
{
	AActor* TheViewTarget = GetViewTarget();
	if( TheViewTarget == this )
	{
		return;
	}
	ClientSetViewTarget( TheViewTarget );
}

void APlayerController::SpawnPlayerCameraManager()
{
	// servers and owning clients get cameras
	// If no archetype specified, spawn an Engine.PlayerCameraManager.  NOTE all games should specify an archetype.
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.Instigator = Instigator;
	if (PlayerCameraManagerClass != NULL)
	{
		PlayerCameraManager = GetWorld()->SpawnActor<APlayerCameraManager>(PlayerCameraManagerClass, SpawnInfo);
	}
	else
	{
		PlayerCameraManager = GetWorld()->SpawnActor<APlayerCameraManager>(SpawnInfo);
	}

	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->InitializeFor(this);
	}
	else
	{
		UE_LOG(LogPlayerController, Log,  TEXT("Couldn't Spawn PlayerCameraManager for Player!!") );
	}
}

void APlayerController::GetAudioListenerPosition(FVector& OutLocation, FVector& OutFrontDir, FVector& OutRightDir)
{
	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FRotationTranslationMatrix ViewRotationMatrix(ViewRotation, ViewLocation);

	OutLocation = ViewLocation;
	OutFrontDir = ViewRotationMatrix.GetUnitAxis( EAxis::X );
	OutRightDir = ViewRotationMatrix.GetUnitAxis( EAxis::Y );
}

bool APlayerController::ServerCheckClientPossession_Validate()
{
	return true;
}

void APlayerController::ServerCheckClientPossession_Implementation()
{
	if (AcknowledgedPawn != GetPawn())
	{
		// Client already throttles their call to this function, so respond immediately by resetting LastRetryClientTime
		LastRetryPlayerTime = 0.f;
		SafeRetryClientRestart();			
	}
}

void APlayerController::SafeServerCheckClientPossession()
{
	if (GetPawn() && AcknowledgedPawn != GetPawn())
	{
		if (GetWorld()->TimeSince(LastRetryPlayerTime) > RetryServerAcknowledgeThrottleTime)
		{
			ServerCheckClientPossession();
			LastRetryPlayerTime = GetWorld()->TimeSeconds;
		}
	}
}

void APlayerController::SafeServerUpdateSpectatorState()
{
	if (IsInState(NAME_Spectating))
	{
		if (GetWorld()->TimeSince(LastSpectatorStateSynchTime) > RetryServerCheckSpectatorThrottleTime)
		{
			ServerSetSpectatorLocation(GetFocalLocation());
			LastSpectatorStateSynchTime = GetWorld()->TimeSeconds;
		}
	}
}

bool APlayerController::ServerSetSpectatorLocation_Validate(FVector NewLoc)
{
	return true;
}

void APlayerController::ServerSetSpectatorLocation_Implementation(FVector NewLoc)
{
	if ( IsInState(NAME_Spectating) )
	{
		if ( GetWorld()->TimeSeconds - LastSpectatorStateSynchTime > 2.f )
		{
			ClientGotoState(GetStateName());
			LastSpectatorStateSynchTime = GetWorld()->TimeSeconds;
		}
	}
	// if we receive this with !bIsSpectating, the client is in the wrong state; tell it what state it should be in
	else if (GetWorld()->TimeSeconds != LastSpectatorStateSynchTime)
	{
		if (AcknowledgedPawn != GetPawn())
		{
			SafeRetryClientRestart();			
		}
		else
		{
			ClientGotoState(GetStateName());
			ClientSetViewTarget(GetViewTarget());
		}
		
		LastSpectatorStateSynchTime = GetWorld()->TimeSeconds;
	}
}

bool APlayerController::ServerViewNextPlayer_Validate()
{
	return true;
}

void APlayerController::ServerViewNextPlayer_Implementation()
{
	if (IsInState(NAME_Spectating))
	{
		ViewAPlayer(+1);
	}
}

bool APlayerController::ServerViewPrevPlayer_Validate()
{
	return true;
}

void APlayerController::ServerViewPrevPlayer_Implementation()
{
	if (IsInState(NAME_Spectating))
	{
		ViewAPlayer(-1);
	}
}


APlayerState* APlayerController::GetNextViewablePlayer(int32 dir)
{
	int32 CurrentIndex = -1;
	if (PlayerCameraManager->ViewTarget.PlayerState )
	{
		// Find index of current viewtarget's PlayerState
		for ( int32 i=0; i<GetWorld()->GameState->PlayerArray.Num(); i++ )
		{
			if (PlayerCameraManager->ViewTarget.PlayerState == GetWorld()->GameState->PlayerArray[i])
			{
				CurrentIndex = i;
				break;
			}
		}
	}

	// Find next valid viewtarget in appropriate direction
	int32 NewIndex;
	for ( NewIndex=CurrentIndex+dir; (NewIndex>=0)&&(NewIndex<GetWorld()->GameState->PlayerArray.Num()); NewIndex=NewIndex+dir )
	{
		APlayerState* const PlayerState = GetWorld()->GameState->PlayerArray[NewIndex];
		if ( (PlayerState != NULL) && (Cast<AController>(PlayerState->GetOwner()) != NULL) && (Cast<AController>(PlayerState->GetOwner())->GetPawn() != NULL)
			&& GetWorld()->GetAuthGameMode()->CanSpectate(this, PlayerState) )
		{
			return PlayerState;
		}
	}

	// wrap around
	CurrentIndex = (NewIndex < 0) ? GetWorld()->GameState->PlayerArray.Num() : -1;
	for ( NewIndex=CurrentIndex+dir; (NewIndex>=0)&&(NewIndex<GetWorld()->GameState->PlayerArray.Num()); NewIndex=NewIndex+dir )
	{
		APlayerState* const PlayerState = GetWorld()->GameState->PlayerArray[NewIndex];
		if ( (PlayerState != NULL) && (Cast<AController>(PlayerState->GetOwner()) != NULL) && (Cast<AController>(PlayerState->GetOwner())->GetPawn() != NULL) &&
			GetWorld()->GetAuthGameMode()->CanSpectate(this, PlayerState) )
		{
			return PlayerState;
		}
	}

	return NULL;
}


void APlayerController::ViewAPlayer(int32 dir)
{
	APlayerState* const PlayerState = GetNextViewablePlayer(dir);

	if ( PlayerState != NULL )
	{
		SetViewTarget(PlayerState);
	}
}

bool APlayerController::ServerViewSelf_Validate(FViewTargetTransitionParams TransitionParams)
{
	return true;
}

void APlayerController::ServerViewSelf_Implementation(FViewTargetTransitionParams TransitionParams)
{
	if (IsInState(NAME_Spectating))
	{
		ResetCameraMode();
		SetViewTarget( this, TransitionParams );
		ClientSetViewTarget( this, TransitionParams );
	}
}

void APlayerController::StartFire( uint8 FireModeNum ) 
{
	if ( ((IsInState(NAME_Spectating) && bPlayerIsWaiting) || IsInState(NAME_Inactive)) && !IsFrozen() )
	{
		ServerRestartPlayer();
	}
	else if ( IsInState(NAME_Spectating) )
	{
		ServerViewNextPlayer();
	}
	else if ( GetPawn() && !bCinematicMode && !GetWorld()->bPlayersOnly )
	{
		GetPawn()->PawnStartFire( FireModeNum );
	}
}


bool APlayerController::NotifyServerReceivedClientData(APawn* InPawn, float TimeStamp)
{
	if (GetPawn() != InPawn || (GetNetMode() == NM_Client))
	{
		return false;
	}

	if (AcknowledgedPawn != GetPawn())
	{
		SafeRetryClientRestart();
		return false;
	}

	return true;
}

bool APlayerController::ServerRestartPlayer_Validate()
{
	return true;
}

void APlayerController::ServerRestartPlayer_Implementation()
{
	UE_LOG(LogPlayerController, Log, TEXT("SERVER RESTART PLAYER"));
	if ( GetNetMode() == NM_Client )
	{
		return;
	}

	if ( IsInState(NAME_Inactive) || (IsInState(NAME_Spectating) && bPlayerIsWaiting) )
	{
		AGameMode* const GameMode = GetWorld()->GetAuthGameMode();
		if ( !GetWorld()->GetAuthGameMode()->PlayerCanRestart(this) )
		{
			return;
		}

		// If we're still attached to a Pawn, leave it
		if ( GetPawn() != NULL )
		{
			UnPossess();
		}

		GameMode->RestartPlayer( this );
	}
	else if ( GetPawn() != NULL )
	{
		ClientRetryClientRestart(GetPawn());
	}
}

bool APlayerController::CanRestartPlayer()
{
	return PlayerState && !PlayerState->bOnlySpectator && HasClientLoadedCurrentWorld() && PendingSwapConnection == NULL;
}

void APlayerController::ClientIgnoreMoveInput_Implementation(bool bIgnore)
{
	SetIgnoreMoveInput(bIgnore);
}

void APlayerController::ClientIgnoreLookInput_Implementation(bool bIgnore)
{
	SetIgnoreLookInput(bIgnore);
}


void APlayerController::DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	Canvas->SetDrawColor(255,255,0);
	UFont* RenderFont = GEngine->GetSmallFont();
	Canvas->DrawText(RenderFont, FString::Printf(TEXT("STATE %s"), *GetStateName().ToString()), 4.0f, YPos );
	YPos += YL;

	if (DebugDisplay.IsDisplayOn(NAME_Camera))
	{
		if (PlayerCameraManager != NULL)
		{
			Canvas->DrawText(RenderFont, "<<<< CAMERA >>>>", 4.0f, YPos );
			YPos += YL;
			PlayerCameraManager->DisplayDebug( Canvas, DebugDisplay, YL, YPos );
		}
		else
		{
			Canvas->SetDrawColor(255,0,0);
			Canvas->DrawText(RenderFont, "<<<< NO CAMERA >>>>", 4.0f, YPos );
			YPos += YL;
		}
	}
	if ( DebugDisplay.IsDisplayOn(NAME_Input) )
	{
		TArray<UInputComponent*> InputStack;
		BuildInputStack(InputStack);

		Canvas->SetDrawColor(255,255,255);
		Canvas->DrawText(RenderFont, TEXT("<<<< INPUT STACK >>>"), 4.0f, YPos);
		YPos += YL;

		for(int32 i=InputStack.Num() - 1; i >= 0; --i)
		{
			AActor* Owner = InputStack[i]->GetOwner();
			Canvas->SetDrawColor(255,255,255);
			if (Owner)
			{
				Canvas->DrawText(RenderFont, FString::Printf(TEXT(" %s.%s"), *Owner->GetName(), *InputStack[i]->GetName()), 4.0f, YPos);
			}
			else
			{
				Canvas->DrawText(RenderFont, FString::Printf(TEXT(" %s"), *InputStack[i]->GetName()), 4.0f, YPos);
			}
			YPos += YL;
		}

		if (PlayerInput)
		{
			PlayerInput->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
		}
		else
		{
			Canvas->SetDrawColor(255,0,0);
			Canvas->DrawText(RenderFont, "NO INPUT", 4.0f, YPos);
			YPos += YL;
		}
	}
	if ( DebugDisplay.IsDisplayOn("ForceFeedback"))
	{
		Canvas->SetDrawColor(255, 255, 255);
		Canvas->DrawText(RenderFont, FString::Printf(TEXT("Force Feedback - LL: %.2f LS: %.2f RL: %.2f RS: %.2f"), ForceFeedbackValues.LeftLarge, ForceFeedbackValues.LeftSmall, ForceFeedbackValues.RightLarge, ForceFeedbackValues.RightSmall), 4.0f, YPos);
		YPos += YL;
	}
}

void APlayerController::SetCinematicMode(bool bInCinematicMode, bool bHidePlayer, bool bAffectsHUD, bool bAffectsMovement, bool bAffectsTurning)
{
	bCinematicMode = bInCinematicMode;

	// If we have a pawn we need to determine if we should show/hide the player
	if (GetPawn() != NULL)
	{
		// Only hide the pawn if in cinematic mode and we want to
		if (bCinematicMode && bHidePlayer)
		{
			GetPawn()->SetActorHiddenInGame(true);
		}
		// Always safe to show the pawn when not in cinematic mode
		else if (!bCinematicMode)
		{
			GetPawn()->SetActorHiddenInGame(false);
		}
	}

	// Let the input system know about cinematic mode
	SetCinematicMode(bCinematicMode, bAffectsMovement, bAffectsTurning);

	// Replicate to the client
	ClientSetCinematicMode(bCinematicMode, bAffectsMovement, bAffectsTurning, bAffectsHUD);
}

void APlayerController::ClientSetCinematicMode_Implementation(bool bInCinematicMode, bool bAffectsMovement, bool bAffectsTurning, bool bAffectsHUD)
{
	bCinematicMode = bInCinematicMode;

	// If there's a HUD, set whether it should be shown or not
	if (MyHUD && bAffectsHUD)
	{
		MyHUD->bShowHUD = !bCinematicMode;
		ULocalPlayer* LocPlayer = Cast<ULocalPlayer>(Player);
		if (VirtualJoystick.IsValid())
		{
			VirtualJoystick->SetVisibility(MyHUD->bShowHUD, true);
		}
	}

	// Let the input system know about cinematic mode
	SetCinematicMode(bCinematicMode, bAffectsMovement, bAffectsTurning);
}

void APlayerController::ClientForceGarbageCollection_Implementation()
{
	GetWorld()->ForceGarbageCollection();
}

void APlayerController::LevelStreamingStatusChanged(ULevelStreaming* LevelObject, bool bNewShouldBeLoaded, bool bNewShouldBeVisible, bool bNewShouldBlockOnLoad, int32 LODIndex )
{
	//`log( "LevelStreamingStatusChanged: " @ LevelObject @ bNewShouldBeLoaded @ bNewShouldBeVisible @ bNewShouldBeVisible );
	ClientUpdateLevelStreamingStatus(LevelObject->PackageName,bNewShouldBeLoaded,bNewShouldBeVisible,bNewShouldBlockOnLoad,LODIndex);
}

void APlayerController::ClientPrepareMapChange_Implementation(FName LevelName, bool bFirst, bool bLast)
{
	// Only call on the first local player controller to handle it being called on multiple PCs for splitscreen.
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if( PlayerController != this )
	{
		return;
	}

	if (bFirst)
	{
		PendingMapChangeLevelNames.Empty();
		GetWorldTimerManager().ClearTimer( this, &APlayerController::DelayedPrepareMapChange );
	}
	PendingMapChangeLevelNames.Add(LevelName);
	if (bLast)
	{
		DelayedPrepareMapChange();
	}
}

void APlayerController::DelayedPrepareMapChange()
{
	if (GetWorld()->IsPreparingMapChange())
	{
		// we must wait for the previous one to complete
		GetWorldTimerManager().SetTimer( this, &APlayerController::DelayedPrepareMapChange, 0.01f );
	}
	else
	{
		GetWorld()->PrepareMapChange(PendingMapChangeLevelNames);
	}
}


void APlayerController::ClientCommitMapChange_Implementation()
{
	if (GetWorldTimerManager().IsTimerActive(this, &APlayerController::DelayedPrepareMapChange))
	{
		GetWorldTimerManager().SetTimer(this, &APlayerController::ClientCommitMapChange, 0.01f);
	}
	else
	{
		if (bAutoManageActiveCameraTarget)
		{
			if (GetPawnOrSpectator() != NULL)
			{
				SetViewTarget(GetPawnOrSpectator());
			}
			else
			{
				SetViewTarget(this);
			}
		}
		GetWorld()->CommitMapChange();
	}
}

void APlayerController::ClientCancelPendingMapChange_Implementation()
{
	GetWorld()->CancelPendingMapChange();
}


void APlayerController::ClientSetBlockOnAsyncLoading_Implementation()
{
	GetWorld()->bRequestedBlockOnAsyncLoading = true;
}


void APlayerController::GetSeamlessTravelActorList(bool bToEntry, TArray<AActor*>& ActorList)
{
	if (MyHUD != NULL)
	{
		ActorList.Add(MyHUD);
	}

	// Should player camera persist or just be recreated?  (clients have to recreate on host)
	ActorList.Add(PlayerCameraManager);
}


void APlayerController::SeamlessTravelTo(APlayerController* NewPC) {}


void APlayerController::SeamlessTravelFrom(APlayerController* OldPC)
{
	// copy PlayerState data
	if (OldPC->PlayerState)
	{
		OldPC->PlayerState->Reset();
		OldPC->PlayerState->SeamlessTravelTo(PlayerState);

		//@fixme: need a way to replace PlayerStates that doesn't cause incorrect "player left the game"/"player entered the game" messages
		OldPC->PlayerState->Destroy();
		OldPC->PlayerState = NULL;
	}
}

void APlayerController::ClientEnableNetworkVoice_Implementation(bool bEnable)
{
	ToggleSpeaking(bEnable);
}

void APlayerController::StartTalking()
{
	ToggleSpeaking(true);
}

void APlayerController::StopTalking()
{
	ToggleSpeaking(false);
}

void APlayerController::ToggleSpeaking(bool bSpeaking)
{
	ULocalPlayer* LP = Cast<ULocalPlayer>(Player);
	if (LP != NULL)
	{
		UWorld* World = GetWorld();
		IOnlineVoicePtr VoiceInt = Online::GetVoiceInterface(World);
		if (VoiceInt.IsValid())
		{
			if (bSpeaking)
			{
				VoiceInt->StartNetworkedVoice(LP->ControllerId);
			}
			else
			{
				VoiceInt->StopNetworkedVoice(LP->ControllerId);
			}
		}
	}
}

void APlayerController::ClientVoiceHandshakeComplete_Implementation()
{
	MuteList.bHasVoiceHandshakeCompleted = true;
}

void APlayerController::GameplayMutePlayer(const FUniqueNetIdRepl& PlayerNetId)
{
	if (PlayerNetId.IsValid())
	{
		MuteList.GameplayMutePlayer(this, PlayerNetId);
	}
}

void APlayerController::GameplayUnmutePlayer(const FUniqueNetIdRepl& PlayerNetId)
{
	if (PlayerNetId.IsValid())
	{
		MuteList.GameplayUnmutePlayer(this, PlayerNetId);
	}
}

void APlayerController::ServerMutePlayer_Implementation(FUniqueNetIdRepl PlayerId)
{
	MuteList.ServerMutePlayer(this, PlayerId);
}

bool APlayerController::ServerMutePlayer_Validate(FUniqueNetIdRepl PlayerId)
{
	if (!PlayerId.IsValid())
	{
		return false;
	}

	return true;
}

void APlayerController::ServerUnmutePlayer_Implementation(FUniqueNetIdRepl PlayerId)
{
	MuteList.ServerUnmutePlayer(this, PlayerId);
}

bool APlayerController::ServerUnmutePlayer_Validate(FUniqueNetIdRepl PlayerId)
{
	if (!PlayerId.IsValid())
	{
		return false;
	}

	return true;
}

void APlayerController::ClientMutePlayer_Implementation(FUniqueNetIdRepl PlayerId)
{
	MuteList.ClientMutePlayer(this, PlayerId);
}

void APlayerController::ClientUnmutePlayer_Implementation(FUniqueNetIdRepl PlayerId)
{
	MuteList.ClientUnmutePlayer(this, PlayerId);
}

bool APlayerController::IsPlayerMuted(const FUniqueNetId& PlayerId)
{
	return MuteList.IsPlayerMuted(PlayerId);
}

void APlayerController::NotifyDirectorControl(bool bNowControlling, AMatineeActor* CurrentMatinee)
{
	// matinee is done, make sure client syncs up viewtargets, since we were ignoring
	// ClientSetViewTarget during the matinee.
	if (!bNowControlling && (GetNetMode() == NM_Client) && PlayerCameraManager && PlayerCameraManager->bClientSimulatingViewTarget)
	{
		ServerVerifyViewTarget();
	}
}

void APlayerController::ClientWasKicked_Implementation(const FText& KickReason)
{
}

void APlayerController::ConsoleKey(FKey Key)
{
#if !UE_BUILD_SHIPPING
	if (ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player))
	{
		if (LocalPlayer->ViewportClient->ViewportConsole)
		{
			LocalPlayer->ViewportClient->ViewportConsole->InputKey(0, Key, IE_Pressed);
		}
	}
#endif // !UE_BUILD_SHIPPING
}
void APlayerController::SendToConsole(const FString& Command)
{
#if !UE_BUILD_SHIPPING
	if (ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player))
	{
		if (LocalPlayer->ViewportClient->ViewportConsole)
		{
			LocalPlayer->ViewportClient->ViewportConsole->ConsoleCommand(Command);
		}
	}
#endif // !UE_BUILD_SHIPPING
}


bool APlayerController::IsPrimaryPlayer() const
{
	int32 SSIndex;
	return !IsSplitscreenPlayer(&SSIndex) || SSIndex == 0;
}

bool APlayerController::IsSplitscreenPlayer(int32* OutSplitscreenPlayerIndex) const
{
	bool bResult = 0;

	if (OutSplitscreenPlayerIndex)
	{
		*OutSplitscreenPlayerIndex = NetPlayerIndex;
	}

	if (Player != NULL)
	{
		ULocalPlayer* const LP = Cast<ULocalPlayer>(Player);
		if (LP != NULL)
		{
			const TArray<ULocalPlayer*>& GamePlayers = LP->GetOuterUEngine()->GetGamePlayers(GetWorld());
			if (GamePlayers.Num() > 1)
			{
				if (OutSplitscreenPlayerIndex)
				{
					*OutSplitscreenPlayerIndex = GamePlayers.Find(LP);
				}
				bResult = true;
			}
		}
		else
		{
			UNetConnection* RemoteConnection = Cast<UNetConnection>(Player);
			if (RemoteConnection->Children.Num() > 0)
			{
				if (OutSplitscreenPlayerIndex)
				{
					*OutSplitscreenPlayerIndex = 0;
				}
				bResult = true;
			}
			else
			{
				UChildConnection* const ChildRemoteConnection = Cast<UChildConnection>(RemoteConnection);
				if (ChildRemoteConnection)
				{
					if (OutSplitscreenPlayerIndex && ChildRemoteConnection->Parent)
					{
						*OutSplitscreenPlayerIndex = ChildRemoteConnection->Parent->Children.Find(ChildRemoteConnection) + 1;
					}
					bResult = true;
				}
			}
		}
	}

	return bResult;
}


APlayerState* APlayerController::GetSplitscreenPlayerByIndex(int32 PlayerIndex) const
{
	APlayerState* Result = NULL;
	if ( Player != NULL )
	{
		if ( IsSplitscreenPlayer() )
		{
			ULocalPlayer* LP = Cast<ULocalPlayer>(Player);
			UNetConnection* RemoteConnection = Cast<UNetConnection>(Player);
			if ( LP != NULL )
			{
				const TArray<ULocalPlayer*>& GamePlayers = LP->ViewportClient->GetOuterUEngine()->GetGamePlayers(GetWorld());
				// this PC is a local player
				if ( PlayerIndex >= 0 && PlayerIndex < GamePlayers.Num() )
				{
					ULocalPlayer* SplitPlayer = GamePlayers[PlayerIndex];
					Result = SplitPlayer->PlayerController->PlayerState;
				}
				else
				{
					UE_LOG(LogPlayerController, Warning, TEXT("(%s) APlayerController::%s:GetSplitscreenPlayerByIndex: requested player at invalid index! PlayerIndex:%i NumLocalPlayers:%i"),
						*GetFName().ToString(), *GetStateName().ToString(), PlayerIndex, GamePlayers.Num());
				}
			}
			else if ( RemoteConnection != NULL )
			{
				if ( GetNetMode() == NM_Client )
				{
					//THIS SHOULD NEVER HAPPEN - IF HAVE A REMOTECONNECTION, WE SHOULDN'T BE A CLIENT
					// this player is a client
					UE_LOG(LogPlayerController, Warning, TEXT("(%s) APlayerController::%s:GetSplitscreenPlayerByIndex: CALLED ON CLIENT WITH VALID REMOTE NETCONNECTION!"),
						*GetFName().ToString(), *GetStateName().ToString());
				}
				else
				{
					UChildConnection* ChildRemoteConnection = Cast<UChildConnection>(RemoteConnection);
					if ( ChildRemoteConnection != NULL )
					{
						// this player controller is not the primary player in the splitscreen layout
						UNetConnection* MasterConnection = ChildRemoteConnection->Parent;
						if ( PlayerIndex == 0 )
						{
							Result = MasterConnection->PlayerController->PlayerState;
						}
						else
						{
							PlayerIndex--;
							if ( PlayerIndex >= 0 && PlayerIndex < MasterConnection->Children.Num() )
							{
								ChildRemoteConnection = MasterConnection->Children[PlayerIndex];
								Result = ChildRemoteConnection->PlayerController->PlayerState;
							}
						}
					}
					else if ( RemoteConnection->Children.Num() > 0 )
					{
						// this PC is the primary splitscreen player
						if ( PlayerIndex == 0 )
						{
							// they want this player controller's PlayerState
							Result = PlayerState;
						}
						else
						{
							// our split-screen's PlayerState is being requested.
							PlayerIndex--;
							if ( PlayerIndex >= 0 && PlayerIndex < RemoteConnection->Children.Num() )
							{
								ChildRemoteConnection = RemoteConnection->Children[PlayerIndex];
								Result = ChildRemoteConnection->PlayerController->PlayerState;
							}
						}
					}
					else
					{
						UE_LOG(LogPlayerController, Log, TEXT("(%s) APlayerController::%s:%s: %s IS NOT THE PRIMARY CONNECTION AND HAS NO CHILD CONNECTIONS!"), *GetFName().ToString(), *GetStateName().ToString(), Player);
					}
				}
			}
			else
			{
				UE_LOG(LogPlayerController, Log, TEXT("(%s) APlayerController::%s:%s: %s IS NOT A ULocalPlayer* AND NOT A RemoteConnection! (No valid UPlayer* reference)"), *GetFName().ToString(), *GetStateName().ToString(), Player);
			}
		}
	}
	else
	{
		UE_LOG(LogPlayerController, Log, TEXT("(%s) APlayerController::%s:%s: %s"), *GetFName().ToString(), *GetStateName().ToString(),  "NULL value for Player!");
	}

	return Result;
}


int32 APlayerController::GetSplitscreenPlayerCount() const
{
	int32 Result = 0;

	if ( IsSplitscreenPlayer() )
	{
		if ( Player != NULL )
		{
			ULocalPlayer* const LP = Cast<ULocalPlayer>(Player);
			UNetConnection* RemoteConnection = Cast<UNetConnection>(Player);
			if ( LP != NULL )
			{
				Result = LP->ViewportClient->GetOuterUEngine()->GetNumGamePlayers(GetWorld());
			}
			else if ( RemoteConnection != NULL )
			{
				if ( Cast<UChildConnection>(RemoteConnection) != NULL )
				{
					// we're the secondary (or otherwise) player in the split - we need to move up to the primary connection
					RemoteConnection = Cast<UChildConnection>(RemoteConnection)->Parent;
				}

				// add one for the primary player
				Result = RemoteConnection->Children.Num() + 1;
			}
			else
			{
				UE_LOG(LogPlayerController, Log, TEXT("(%s) APlayerController::%s:%s NOT A ULocalPlayer* AND NOT A RemoteConnection!"), *GetFName().ToString(), *GetStateName().ToString());
			}
		}
		else
		{
			UE_LOG(LogPlayerController, Log, TEXT("(%s) APlayerController::%s:%s called without a valid UPlayer* value!"), *GetFName().ToString(), *GetStateName().ToString());
		}
	}

	return Result;
}


void APlayerController::ClientSetForceMipLevelsToBeResident_Implementation( UMaterialInterface* Material, float ForceDuration, int32 CinematicTextureGroups )
{
	if ( Material != NULL && IsPrimaryPlayer() )
	{
		Material->SetForceMipLevelsToBeResident( false, false, ForceDuration, CinematicTextureGroups );
	}
}


void APlayerController::ClientPrestreamTextures_Implementation( AActor* ForcedActor, float ForceDuration, bool bEnableStreaming, int32 CinematicTextureGroups)
{
	if ( ForcedActor != NULL && IsPrimaryPlayer() )
	{
		ForcedActor->PrestreamTextures( ForceDuration, bEnableStreaming, CinematicTextureGroups );
	}
}

void APlayerController::ClientPlayForceFeedback_Implementation( UForceFeedbackEffect* ForceFeedbackEffect, bool bLooping, FName Tag)
{
	if (ForceFeedbackEffect)
	{
		if (Tag != NAME_None)
		{
			for (int32 Index = ActiveForceFeedbackEffects.Num() - 1; Index >= 0; --Index)
			{
				if (ActiveForceFeedbackEffects[Index].Tag == Tag)
				{
					ActiveForceFeedbackEffects.RemoveAtSwap(Index);
				}
			}
		}

		FActiveForceFeedbackEffect ActiveEffect(ForceFeedbackEffect, bLooping, Tag);
		ActiveForceFeedbackEffects.Add(ActiveEffect);
	}
}

void APlayerController::ClientStopForceFeedback_Implementation( UForceFeedbackEffect* ForceFeedbackEffect, FName Tag)
{
	if (ForceFeedbackEffect == NULL && Tag == NAME_None)
	{
		ActiveForceFeedbackEffects.Empty();
	}
	else
	{
		for (int32 Index = ActiveForceFeedbackEffects.Num() - 1; Index >= 0; --Index)
		{
			if (    (ForceFeedbackEffect == NULL || ActiveForceFeedbackEffects[Index].ForceFeedbackEffect == ForceFeedbackEffect)
				 && (Tag == NAME_None || ActiveForceFeedbackEffects[Index].Tag == Tag) )
			{
				ActiveForceFeedbackEffects.RemoveAtSwap(Index);
			}
		}
	}
}

void APlayerController::ProcessForceFeedback(const float DeltaTime, const bool bGamePaused)
{
	if (Player == NULL)
	{
		return;
	}

	ForceFeedbackValues.LeftLarge = ForceFeedbackValues.LeftSmall = ForceFeedbackValues.RightLarge = ForceFeedbackValues.RightSmall = 0.f;

	if (!bGamePaused)
	{
		for (int32 Index = ActiveForceFeedbackEffects.Num() - 1; Index >= 0; --Index)
		{
			if (!ActiveForceFeedbackEffects[Index].Update(DeltaTime, ForceFeedbackValues))
			{
				ActiveForceFeedbackEffects.RemoveAtSwap(Index);
			}
		}
	}

	// Get the IForceFeedbackSystem pointer from the global application, returning if NULL
	IForceFeedbackSystem* ForceFeedbackSystem = FSlateApplication::Get().GetForceFeedbackSystem();
	if (ForceFeedbackSystem)
	{
		ForceFeedbackSystem->SetChannelValues(CastChecked<ULocalPlayer>(Player)->ControllerId, ForceFeedbackValues);
	}
}

void APlayerController::ClientPlayCameraShake_Implementation( TSubclassOf<class UCameraShake> Shake, float Scale, ECameraAnimPlaySpace::Type PlaySpace, FRotator UserPlaySpaceRot )
{
	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->PlayCameraShake(Shake, Scale, PlaySpace, UserPlaySpaceRot);
	}
}

void APlayerController::ClientStopCameraShake_Implementation( TSubclassOf<class UCameraShake> Shake )
{
	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->StopCameraShake(Shake);
	}
}

void APlayerController::ClientPlayCameraAnim_Implementation( UCameraAnim* AnimToPlay, float Scale, float Rate,
						float BlendInTime, float BlendOutTime, bool bLoop,
						bool bRandomStartTime, ECameraAnimPlaySpace::Type Space, FRotator CustomPlaySpace )
{
	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->PlayCameraAnim(AnimToPlay, Rate, Scale, BlendInTime, BlendOutTime, bLoop, bRandomStartTime, 0.f, Space, CustomPlaySpace);
	}
}

void APlayerController::ClientStopCameraAnim_Implementation(UCameraAnim* AnimToStop)
{
	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->StopAllInstancesOfCameraAnim(AnimToStop);
	}
}


void APlayerController::ClientSpawnCameraLensEffect_Implementation( TSubclassOf<AEmitterCameraLensEffectBase> LensEffectEmitterClass )
{
	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->AddCameraLensEffect(LensEffectEmitterClass);
	}
}

void APlayerController::ReceivedGameModeClass(TSubclassOf<AGameMode> GameModeClass)
{
}

void APlayerController::ReceivedSpectatorClass(TSubclassOf<AGameMode> SpectatorClass)
{
	if (IsInState(NAME_Spectating))
	{
		if (GetSpectatorPawn() == NULL)
		{
			BeginSpectatingState();
		}
	}
}

void APlayerController::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	// These used to only replicate if PlayerCameraManager->GetViewTargetPawn() != GetPawn()
	// But, since they also don't update unless that condition is true, these values won't change, thus won't send
	// This is a little less efficient, but fits into the new condition system well, and shouldn't really add much overhead
	DOREPLIFETIME_CONDITION( APlayerController, TargetViewRotation,		COND_OwnerOnly );
}

void APlayerController::SetPawn(APawn* InPawn)
{
	if (InPawn == NULL)
	{
		// Attempt to move the PC to the current camera location if no pawn was specified
		const FVector NewLocation = (PlayerCameraManager != NULL) ? PlayerCameraManager->GetCameraLocation() : GetSpawnLocation();
		SetSpawnLocation(NewLocation);

		if (bAutoManageActiveCameraTarget)
		{
			SetViewTarget(this);
		}
	}

	Super::SetPawn(InPawn);
}


void APlayerController::SetPlayer( UPlayer* InPlayer )
{
	check(InPlayer!=NULL);

	// Detach old player.
	if( InPlayer->PlayerController )
		InPlayer->PlayerController->Player = NULL;

	// Set the viewport.
	Player = InPlayer;
	InPlayer->PlayerController = this;

	// cap outgoing rate to max set by server
	UNetDriver* Driver = GetWorld()->GetNetDriver();
	if( (ClientCap>=2600) && Driver && Driver->ServerConnection )
	{
		Player->CurrentNetSpeed = Driver->ServerConnection->CurrentNetSpeed = FMath::Clamp( ClientCap, 1800, Driver->MaxClientRate );
	}

	// initializations only for local players
	ULocalPlayer *LP = Cast<ULocalPlayer>(InPlayer);
	if (LP != NULL)
	{
		LP->InitOnlineSession();
		InitInputSystem();
	}
	else
	{
		NetConnection = Cast<UNetConnection>(InPlayer);
		if (NetConnection)
		{
			NetConnection->OwningActor = this;
		}
	}

	UpdateStateInputComponents();

	// notify script that we've been assigned a valid player
	ReceivedPlayer();
}


void APlayerController::TickActor( float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction )
{
	if (TickType == LEVELTICK_PauseTick && !ShouldPerformFullTickWhenPaused())
	{
		if (PlayerInput != NULL)
		{
			ProcessPlayerInput(DeltaSeconds, true);
			ProcessForceFeedback(DeltaSeconds, true);
		}

		// Clear axis inputs from previous frame.
		RotationInput = FRotator::ZeroRotator;

		return; //root of tick hierarchy
	}

	//root of tick hierarchy

	if ((GetNetMode() < NM_Client) && (GetRemoteRole() == ROLE_AutonomousProxy) && !IsLocalPlayerController())
	{
		// force physics update for clients that aren't sending movement updates in a timely manner 
		// this prevents cheats associated with artificially induced ping spikes
		// skip updates if pawn lost autonomous proxy role (e.g. TurnOff() call)
		if (GetPawn() && !GetPawn()->IsPendingKill() && GetPawn()->GetRemoteRole() == ROLE_AutonomousProxy)
		{
			INetworkPredictionInterface* NetworkPredictionInterface = InterfaceCast<INetworkPredictionInterface>(GetPawn()->GetMovementComponent());
			if (NetworkPredictionInterface)
			{
				FNetworkPredictionData_Server* ServerData = NetworkPredictionInterface->GetPredictionData_Server();
				const float TimeSinceUpdate = ServerData ? GetWorld()->GetTimeSeconds() - ServerData->ServerTimeStamp : 0.f;
				if (TimeSinceUpdate > FMath::Max<float>(DeltaSeconds+0.06f,AGameNetworkManager::StaticClass()->GetDefaultObject<AGameNetworkManager>()->MAXCLIENTUPDATEINTERVAL))
				{
					const USkeletalMeshComponent* PawnMesh = GetPawn()->FindComponentByClass<USkeletalMeshComponent>();
					if (!PawnMesh || !PawnMesh->IsSimulatingPhysics())
					{
						NetworkPredictionInterface->ForcePositionUpdate(TimeSinceUpdate);
						ServerData->ServerTimeStamp = GetWorld()->TimeSeconds;
					}					
				}
			}
		}

		// update viewtarget replicated info
		if (PlayerCameraManager != NULL)
		{
			APawn* TargetPawn = PlayerCameraManager->GetViewTargetPawn();
			
			if ((TargetPawn != GetPawn()) && (TargetPawn != NULL))
			{
				TargetViewRotation = TargetPawn->GetViewRotation();
			}
		}
	}
	else if (Role > ROLE_SimulatedProxy)
	{
		// Process PlayerTick with input.
		if (!PlayerInput)
		{
			InitInputSystem();
		}

		if (PlayerInput)
		{
			PlayerInput->Tick(DeltaSeconds);
			PlayerTick(DeltaSeconds);
		}

		if (IsPendingKill())
		{
			return;
		}

		// update viewtarget replicated info
		if (PlayerCameraManager != NULL)
		{
			APawn* TargetPawn = PlayerCameraManager->GetViewTargetPawn();
			if ((TargetPawn != GetPawn()) && (TargetPawn != NULL))
			{
				SmoothTargetViewRotation(TargetPawn, DeltaSeconds);
			}
		}
	}

	if (!IsPendingKill())
	{
		Tick(DeltaSeconds);	// perform any tick functions unique to an actor subclass
	}

	// Clear old axis inputs since we are done with them. 
	RotationInput = FRotator::ZeroRotator;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CheatManager != NULL)
	{
		CheatManager->TickCollisionDebug();
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

bool APlayerController::IsNetRelevantFor(APlayerController* RealViewer, AActor* Viewer, const FVector& SrcLocation)
{
	return ( this==RealViewer );
}

void APlayerController::ClientTravel(const FString& URL, ETravelType TravelType, bool bSeamless, FGuid MapPackageGuid)
{
	// Keep track of seamless travel serverside
	if (bSeamless && TravelType == TRAVEL_Relative)
	{
		SeamlessTravelCount++;
	}

	// Now pass on to the RPC
	ClientTravelInternal(URL, TravelType, bSeamless, MapPackageGuid);
}

void APlayerController::ClientTravelInternal_Implementation(const FString& URL, ETravelType TravelType, bool bSeamless, FGuid MapPackageGuid)
{
	UWorld* World = GetWorld();

	// Warn the client.
	PreClientTravel(URL, TravelType, bSeamless);

	if (bSeamless && TravelType == TRAVEL_Relative)
	{
		World->SeamlessTravel(URL);
	}
	else
	{
		if (bSeamless)
		{
			UE_LOG(LogPlayerController, Warning, TEXT("Unable to perform seamless travel because TravelType was %i, not TRAVEL_Relative"), int32(TravelType));
		}
		// Do the travel.
		GEngine->SetClientTravel(World, *URL, (ETravelType)TravelType);
	}
}

FString APlayerController::GetPlayerNetworkAddress()
{
	if( Player && Player->IsA(UNetConnection::StaticClass()) )
		return Cast<UNetConnection>(Player)->LowLevelGetRemoteAddress();
	else
		return TEXT("");
}

FString APlayerController::GetServerNetworkAddress()
{
	UNetDriver* NetDriver = NULL;
	if (GetWorld())
	{
		NetDriver = GetWorld()->GetNetDriver();
	}

	if( NetDriver && NetDriver->ServerConnection )
	{
		return NetDriver->ServerConnection->LowLevelGetRemoteAddress();
	}

	return TEXT("");
}

bool APlayerController::DefaultCanUnpause()
{
	return GetWorldSettings() != NULL && GetWorldSettings()->Pauser == PlayerState;
}

void APlayerController::StartSpectatingOnly()
{
	ChangeState(NAME_Spectating);
	PlayerState->bIsSpectator = true;
	PlayerState->bOnlySpectator = true;
	bPlayerIsWaiting = false; // Can't spawn, we are only allowed to be a spectator.
}

void APlayerController::EndPlayingState()
{
	if ( GetPawn() != NULL )
	{
		GetPawn()->SetRemoteViewPitch( 0.f );
	}
}


void APlayerController::BeginSpectatingState()
{
	if ( GetPawn() != NULL )
	{
		UnPossess();
	}

	DestroySpectatorPawn();
	SetSpectatorPawn(SpawnSpectatorPawn());
}


void APlayerController::SetSpectatorPawn(class ASpectatorPawn* NewSpectatorPawn)
{
	if (IsInState(NAME_Spectating))
	{
		RemovePawnTickDependency(SpectatorPawn);
		SpectatorPawn = NewSpectatorPawn;
		AttachToPawn(SpectatorPawn);
		AddPawnTickDependency(SpectatorPawn);
	}
}


ASpectatorPawn* APlayerController::SpawnSpectatorPawn()
{
	ASpectatorPawn* SpawnedSpectator = NULL;

	// Only spawned for the local player
	if (GetSpectatorPawn() == NULL && IsLocalController())
	{
		AGameState const* const GameState = GetWorld()->GameState;
		if (GameState)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.bNoCollisionFail = true;
			SpawnedSpectator = GetWorld()->SpawnActor<ASpectatorPawn>(GameState->SpectatorClass, GetSpawnLocation(), GetControlRotation(), SpawnParams);
			if (SpawnedSpectator)
			{
				SpawnedSpectator->PossessedBy(this);
				SpawnedSpectator->PawnClientRestart();
				SpawnedSpectator->SetActorTickEnabled(true);

				UE_LOG(LogPlayerController, Log, TEXT("Spawned spectator %s [server:%d]"), *GetNameSafe(SpawnedSpectator), GetNetMode() < NM_Client);
			}
			else
			{
				UE_LOG(LogPlayerController, Warning, TEXT("Failed to spawn spectator with class %s"), GameState->SpectatorClass ? *GameState->SpectatorClass->GetName() : TEXT("NULL"));
			}
		}
		else
		{
			// This normally happens on clients if the Player is replicated but the GameState has not yet.
			UE_LOG(LogPlayerController, Log, TEXT("NULL GameState when trying to spawn spectator!"));
		}
	}

	return SpawnedSpectator;
}


void APlayerController::DestroySpectatorPawn()
{
	if (GetSpectatorPawn())
	{
		if (GetViewTarget() == GetSpectatorPawn())
		{
			SetViewTarget(this);
		}

		GetSpectatorPawn()->UnPossessed();
		GetWorld()->DestroyActor(GetSpectatorPawn());
		SetSpectatorPawn(NULL);
	}
}


APawn* APlayerController::GetPawnOrSpectator() const
{
	return GetPawn() ? GetPawn() : GetSpectatorPawn();
}


void APlayerController::UpdateStateInputComponents()
{
	// update Inactive state component
	if (StateName == NAME_Inactive && IsLocalController())
	{
		if (InactiveStateInputComponent == NULL)
		{
			static const FName InactiveStateInputComponentName(TEXT("PC_InactiveStateInputComponent0"));
			InactiveStateInputComponent = ConstructObject<UInputComponent>(UInputComponent::StaticClass(), this, InactiveStateInputComponentName);
			SetupInactiveStateInputComponent(InactiveStateInputComponent);
			InactiveStateInputComponent->RegisterComponent();
			PushInputComponent(InactiveStateInputComponent);
		}
	}
	else if (InactiveStateInputComponent)
	{
		PopInputComponent(InactiveStateInputComponent);
		InactiveStateInputComponent->DestroyComponent();
		InactiveStateInputComponent = NULL;
	}
}

void APlayerController::ChangeState(FName NewState)
{
	if(NewState != StateName)
	{
		// end current state
		if(StateName == NAME_Spectating)
		{
			EndSpectatingState();
		}
		else if(StateName == NAME_Playing)
		{
			EndPlayingState();
		}

		Super::ChangeState(NewState); // Will set StateName, also handles EndInactiveState/BeginInactiveState

		// start new state
		if(StateName == NAME_Playing)
		{
			BeginPlayingState();
		}
		else if (StateName == NAME_Spectating)
		{
			BeginSpectatingState();
		}

		UpdateStateInputComponents();
	}
}

void APlayerController::BeginPlayingState()
{
	if (GetPawn() != NULL)
	{
		if (GetCharacter() != NULL)
		{
			GetCharacter()->UnCrouch(false);
			UCharacterMovementComponent* CharacterMovement = GetCharacter()->CharacterMovement;
			if (CharacterMovement && !CharacterMovement->IsFalling() && GetCharacter()->GetRootComponent() && !GetCharacter()->GetRootComponent()->IsSimulatingPhysics()) // FIXME HACK!!!
			{
				CharacterMovement->SetMovementMode(MOVE_Walking);
			}
		}		
	}
}

void APlayerController::EndSpectatingState()
{
	if ( PlayerState != NULL )
	{
		if ( PlayerState->bOnlySpectator )
		{
			UE_LOG(LogPlayerController, Log, TEXT("WARNING - Spectator only UPlayer* leaving spectating state"));
		}
		PlayerState->bIsSpectator = false;
	}

	bPlayerIsWaiting = false;

	DestroySpectatorPawn();
}

void APlayerController::BeginInactiveState()
{
	if ( (GetPawn() != NULL) && (GetPawn()->Controller == this) )
	{
		GetPawn()->Controller = NULL;
	}
	SetPawn(NULL);

	AGameState const* const GameState = GetWorld()->GameState;

	float const MinRespawnDelay = ((GameState != NULL) && (GameState->GameModeClass != NULL)) ? GetDefault<AGameMode>(GameState->GameModeClass)->MinRespawnDelay : 1.0f;
	GetWorldTimerManager().SetTimer(this, &APlayerController::UnFreeze, MinRespawnDelay);
}

void APlayerController::EndInactiveState()
{
}

void APlayerController::SetupInactiveStateInputComponent(UInputComponent* InComponent)
{
	check(InComponent);

	InComponent->BindAxis("Spectator_Turn", this, &APlayerController::AddYawInput);
	InComponent->BindAxis("Spectator_LookUp", this, &APlayerController::AddPitchInput);
}


void APlayerController::PushInputComponent(UInputComponent* InputComponent)
{
	if (InputComponent)
	{
		if (InputComponent->HasBindings())
		{
			CurrentInputStack.Push(InputComponent);
		}
		else
		{
			UE_LOG(LogPlayerController, Warning, TEXT("InputComponent '%s' with no bindings pushed on the stack"), *InputComponent->GetFullName());
		}
	}
}

bool APlayerController::PopInputComponent(UInputComponent* InputComponent)
{
	if (InputComponent)
	{
		if (CurrentInputStack.RemoveSingle(InputComponent) > 0)
		{
			for (FInputAxisBinding& AxisBinding : InputComponent->AxisBindings)
			{
				AxisBinding.AxisValue = 0.f;
			}
			for (FInputAxisKeyBinding& AxisKeyBinding : InputComponent->AxisKeyBindings)
			{
				AxisKeyBinding.AxisValue = 0.f;
			}
			for (FInputVectorAxisBinding& VectorAxisBinding : InputComponent->VectorAxisBindings)
			{
				VectorAxisBinding.AxisValue = FVector::ZeroVector;
			}

			return true;
		}
	}
	return false;
}

void APlayerController::AddPitchInput(float Val)
{
	RotationInput.Pitch += !IsLookInputIgnored() ? Val * InputPitchScale : 0.f;
}

void APlayerController::AddYawInput(float Val)
{
	RotationInput.Yaw += !IsLookInputIgnored() ? Val * InputYawScale : 0.f;
}

void APlayerController::AddRollInput(float Val)
{
	RotationInput.Roll += !IsLookInputIgnored() ? Val * InputRollScale : 0.f;
}

bool APlayerController::IsInputKeyDown(const FKey Key) const
{
	return (PlayerInput ? PlayerInput->IsPressed(Key) : false);
}

bool APlayerController::WasInputKeyJustPressed(const FKey Key) const
{
	return (PlayerInput ? PlayerInput->WasJustPressed(Key) : false);
}

bool APlayerController::WasInputKeyJustReleased(const FKey Key) const
{
	return (PlayerInput ? PlayerInput->WasJustReleased(Key) : false);
}

float APlayerController::GetInputAnalogKeyState(const FKey Key) const
{
	return (PlayerInput ? PlayerInput->GetKeyValue(Key) : 0.f);
}

FVector APlayerController::GetInputVectorKeyState(const FKey Key) const
{
	return (PlayerInput ? PlayerInput->GetVectorKeyValue(Key) : FVector());
}

void APlayerController::GetInputTouchState(ETouchIndex::Type FingerIndex, float& LocationX, float& LocationY, bool& bIsCurrentlyPressed) const
{
	if (PlayerInput)
	{
		if (FingerIndex < EKeys::NUM_TOUCH_KEYS)
		{
			LocationX = PlayerInput->Touches[FingerIndex].X;
			LocationY = PlayerInput->Touches[FingerIndex].Y;
			bIsCurrentlyPressed = PlayerInput->Touches[FingerIndex].Z != 0 ? true : false;
		}
		else
		{
			bIsCurrentlyPressed = false;
			UE_LOG(LogPlayerController, Warning, TEXT("Requesting information for invalid finger index."));
		}
	}
	else
	{
		LocationX = LocationY = 0.f;
		bIsCurrentlyPressed = false;
	}
}

void APlayerController::GetInputMotionState(float& Tilt, float& RotationRate, float& Gravity, float& Acceleration) const
{
	Tilt = GetInputAnalogKeyState(EKeys::Tilt);
	RotationRate = GetInputAnalogKeyState(EKeys::RotationRate);
	Gravity = GetInputAnalogKeyState(EKeys::Gravity);
	Acceleration = GetInputAnalogKeyState(EKeys::Acceleration);
}

float APlayerController::GetInputKeyTimeDown(const FKey Key) const
{
	return (PlayerInput ? PlayerInput->GetTimeDown(Key) : 0.f);
}

void APlayerController::GetMousePosition(float& LocationX, float& LocationY) const
{
	const FVector2D MousePosition = CastChecked<ULocalPlayer>(Player)->ViewportClient->GetMousePosition();
	LocationX = MousePosition.X;
	LocationY = MousePosition.Y;
}

void APlayerController::GetInputMouseDelta(float& DeltaX, float& DeltaY) const
{
	if (PlayerInput)
	{
		DeltaX = PlayerInput->GetKeyValue(EKeys::MouseX);
		DeltaY = PlayerInput->GetKeyValue(EKeys::MouseY);
	}
	else
	{
		DeltaX = DeltaY = 0.f;
	}
}

void APlayerController::GetInputAnalogStickState(EControllerAnalogStick::Type WhichStick, float& StickX, float& StickY) const
{
	if (PlayerInput)
	{
		switch (WhichStick)
		{
		case EControllerAnalogStick::CAS_LeftStick:
			StickX = PlayerInput->GetKeyValue(EKeys::Gamepad_LeftX);
			StickY = PlayerInput->GetKeyValue(EKeys::Gamepad_LeftY);
			break;

		case EControllerAnalogStick::CAS_RightStick:
			StickX = PlayerInput->GetKeyValue(EKeys::Gamepad_RightX);
			StickY = PlayerInput->GetKeyValue(EKeys::Gamepad_RightY);
			break;

		default:
			StickX = 0.f;
			StickY = 0.f;
		}
	}
	else
	{
		StickX = StickY = 0.f;
	}
}

void APlayerController::EnableInput(class APlayerController* PlayerController)
{
	if (PlayerController == this || PlayerController == NULL)
	{
		bInputEnabled = true;
	}
	else
	{
		UE_LOG(LogPlayerController, Error, TEXT("EnableInput can only be specified on a PlayerController for itself"));
	}
}

void APlayerController::DisableInput(class APlayerController* PlayerController)
{
	if (PlayerController == this || PlayerController == NULL)
	{
		bInputEnabled = false;
	}
	else
	{
		UE_LOG(LogPlayerController, Error, TEXT("DisableInput can only be specified on a PlayerController for itself"));
	}
}


void APlayerController::ActivateTouchInterface(UTouchInterface* NewTouchInterface)
{
	if(NewTouchInterface)
	{
		NewTouchInterface->Activate(VirtualJoystick);
	}
}

void APlayerController::UpdateCameraManager(float DeltaSeconds)
{
	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->UpdateCamera(DeltaSeconds);
	}
}

void APlayerController::BuildHiddenComponentList(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents)
{
	// Translate the hidden actors list to a hidden primitive list.
	UpdateHiddenActors(ViewLocation);

	for (int32 ActorIndex = 0; ActorIndex < HiddenActors.Num(); ++ActorIndex)
	{
		AActor* HiddenActor = HiddenActors[ActorIndex];
		if (HiddenActor != NULL)
		{
			TArray<UPrimitiveComponent*> Components;
			HiddenActor->GetComponents(Components);

			for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
			{
				UPrimitiveComponent* PrimitiveComponent = Components[ComponentIndex];
				if (PrimitiveComponent->IsRegistered())
				{
					HiddenComponents.Add(PrimitiveComponent->ComponentId);

					for (int32 AttachChildrenIndex = 0; AttachChildrenIndex < PrimitiveComponent->AttachChildren.Num(); AttachChildrenIndex++)
					{						
						UPrimitiveComponent* AttachChildPC = Cast<UPrimitiveComponent>(PrimitiveComponent->AttachChildren[AttachChildrenIndex]);
						if (AttachChildPC && AttachChildPC->IsRegistered())
						{
							HiddenComponents.Add(AttachChildPC->ComponentId);
						}
					}
				}
			}
		}
		else
		{
			HiddenActors.RemoveAt(ActorIndex);
			ActorIndex--;
		}
	}

	// Allow a chance to operate on a per primitive basis
	UpdateHiddenComponents(ViewLocation, HiddenComponents);
}

void APlayerController::ClientRepObjRef_Implementation(UObject *Object)
{
	UE_LOG(LogPlayerController, Warning, TEXT("APlayerController::ClientRepObjRef repped: %s"), Object ? *Object->GetName() : TEXT("NULL") );
}
