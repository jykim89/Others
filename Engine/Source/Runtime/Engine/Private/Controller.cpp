// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Controller.cpp: 

=============================================================================*/
#include "EnginePrivate.h"
#include "Net/UnrealNetwork.h"
#include "ConfigCacheIni.h"
#include "NavigationPathBuilder.h"

DEFINE_LOG_CATEGORY(LogPath);


AController::AController(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	PrimaryActorTick.bCanEverTick = true;
	bHidden = true;
#if WITH_EDITORONLY_DATA
	bHiddenEd = true;
#endif // WITH_EDITORONLY_DATA
	bOnlyRelevantToOwner = true;

	TransformComponent = PCIP.CreateDefaultSubobject<USceneComponent>(this, TEXT("TransformComponent0"));
	RootComponent = TransformComponent;

	bCanBeDamaged = false;
	bAttachToPawn = false;

	if (RootComponent)
	{
		// We attach the RootComponent to the pawn for location updates,
		// but we want to drive rotation with ControlRotation regardless of attachment state.
		RootComponent->bAbsoluteRotation = true;
	}
}

void AController::K2_DestroyActor()
{
	// do nothing, disallow destroying controller from Blueprints
}

bool AController::IsLocalPlayerController() const
{
	return false;
}

bool AController::IsLocalController() const
{
	const ENetMode NetMode = GetNetMode();

	if (NetMode == NM_Standalone)
	{
		// Not networked.
		return true;
	}
	
	if (NetMode == NM_Client && Role == ROLE_AutonomousProxy)
	{
		// Networked client in control.
		return true;
	}

	if (GetRemoteRole() != ROLE_AutonomousProxy && Role == ROLE_Authority)
	{
		// Local authority in control.
		return true;
	}

	return false;
}

void AController::FailedToSpawnPawn()
{
	ChangeState(NAME_Inactive);
}

void AController::SetInitialLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation)
{
	SetActorLocationAndRotation(NewLocation, NewRotation);
	SetControlRotation(NewRotation);
}

FRotator AController::GetControlRotation() const
{
	return ControlRotation;
}

void AController::SetControlRotation(const FRotator& NewRotation)
{
	ControlRotation = NewRotation;

	if (RootComponent && RootComponent->bAbsoluteRotation)
	{
		RootComponent->SetWorldRotation(GetControlRotation());
	}
}


void AController::AttachToPawn(APawn* InPawn)
{
	if (bAttachToPawn && RootComponent)
	{
		if (InPawn)
		{
			// Only attach if not already attached.
			if (InPawn->GetRootComponent() && RootComponent->GetAttachParent() != InPawn->GetRootComponent())
			{
				RootComponent->DetachFromParent(false);
				RootComponent->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
				RootComponent->AttachTo(InPawn->GetRootComponent());
			}
		}
		else
		{
			DetachFromPawn();
		}
	}
}

void AController::DetachFromPawn()
{
	if (bAttachToPawn && RootComponent && RootComponent->GetAttachParent() && Cast<APawn>(RootComponent->GetAttachmentRootActor()))
	{
		RootComponent->DetachFromParent(true);
	}
}


AActor* AController::GetViewTarget() const
{
	if (Pawn)
	{
		return Pawn;
	}

	return const_cast<AController*>(this);
}

void AController::GetPlayerViewPoint( FVector& out_Location, FRotator& out_Rotation ) const
{
	GetActorEyesViewPoint( out_Location, out_Rotation);
}

bool AController::LineOfSightTo(const AActor* Other, FVector ViewPoint, bool bAlternateChecks)
{
	if( !Other )
	{
		return false;
	}

	if ( ViewPoint.IsZero() )
	{
		AActor*	ViewTarg = GetViewTarget();
		ViewPoint = ViewTarg->GetActorLocation();
		if( ViewTarg == Pawn )
		{
			ViewPoint.Z += Pawn->BaseEyeHeight; //look from eyes
		}
	}

	static FName NAME_LineOfSight = FName(TEXT("LineOfSight"));
	FCollisionQueryParams CollisionParms(NAME_LineOfSight, true, Other);
	CollisionParms.AddIgnoredActor(this->GetPawn());
	FVector TargetLocation = Other->GetTargetLocation(Pawn);
	bool bHit = GetWorld()->LineTraceTest(ViewPoint, TargetLocation, ECC_Visibility, CollisionParms);
	if( !bHit )
	{
		return true;
	}

	// if other isn't using a cylinder for collision and isn't a Pawn (which already requires an accurate cylinder for AI)
	// then don't go any further as it likely will not be tracing to the correct location
	if (!Cast<const APawn>(Other) && Cast<UCapsuleComponent>(Other->GetRootComponent()) == NULL)
	{
		return false;
	}
	float distSq = (Other->GetActorLocation() - ViewPoint).SizeSquared();
	if ( distSq > FARSIGHTTHRESHOLDSQUARED )
	{
		return false;
	}
	if ( !Cast<const APawn>(Other) && (distSq > NEARSIGHTTHRESHOLDSQUARED) ) 
	{
		return false;
	}

	float OtherRadius, OtherHeight;
	Other->GetSimpleCollisionCylinder(OtherRadius, OtherHeight);
	
	//try viewpoint to head
	bHit = GetWorld()->LineTraceTest(ViewPoint,  Other->GetActorLocation() + FVector(0.f,0.f,OtherHeight), ECC_Visibility, CollisionParms);
	return !bHit;
}

void AController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if ( !IsPendingKill() )
	{
		GetWorld()->AddController( this );
	}
}

void AController::Possess(APawn* InPawn)
{
	REDIRECT_ACTOR_TO_VLOG(InPawn, this);

	if (InPawn != NULL)
	{
		if (GetPawn() && GetPawn() != InPawn)
		{
			UnPossess();
		}

		if (InPawn->Controller != NULL)
		{
			InPawn->Controller->UnPossess();
		}

		InPawn->PossessedBy(this);
		SetPawn(InPawn);

		// update rotation to match possessed pawn's rotation
		SetControlRotation( Pawn->GetActorRotation() );

		Pawn->Restart();
	}
}

void AController::UnPossess()
{
	if ( Pawn != NULL )
	{
		Pawn->UnPossessed();
		SetPawn(NULL);
	}
}


void AController::PawnPendingDestroy(APawn* inPawn)
{
	if ( IsInState(NAME_Inactive) )
	{
		UE_LOG(LogPath, Log, TEXT("PawnPendingDestroy while inactive %s"), *GetName());
	}

	if ( inPawn != Pawn )
	{
		return;
	}

	UnPossess();
	ChangeState(NAME_Inactive);

	if (PlayerState == NULL)
	{
		Destroy();
	}
}

void AController::Reset()
{
	Super::Reset();
	StartSpot = NULL;
}

void AController::ClientSetLocation_Implementation( FVector NewLocation, FRotator NewRotation )
{
	ClientSetRotation(NewRotation);
	if (Pawn != NULL)
	{
		Pawn->TeleportTo(NewLocation, Pawn->GetActorRotation());
	}
}

void AController::ClientSetRotation_Implementation( FRotator NewRotation, bool bResetCamera )
{
	SetControlRotation(NewRotation);
	if (Pawn != NULL)
	{
		Pawn->FaceRotation( NewRotation, 0.f );
	}
}

void AController::RemovePawnTickDependency(APawn* InOldPawn)
{
	if (InOldPawn != NULL)
	{
		UPawnMovementComponent* PawnMovement = InOldPawn->GetMovementComponent();
		if (PawnMovement)
		{
			PawnMovement->PrimaryComponentTick.RemovePrerequisite(this, this->PrimaryActorTick);
		}
		else
		{
			InOldPawn->PrimaryActorTick.RemovePrerequisite(this, this->PrimaryActorTick);
		}
	}
}


void AController::AddPawnTickDependency(APawn* NewPawn)
{
	if (NewPawn != NULL)
	{
		UPawnMovementComponent* PawnMovement = NewPawn->GetMovementComponent();
		if (PawnMovement)
		{
			PawnMovement->PrimaryComponentTick.AddPrerequisite(this, this->PrimaryActorTick);
		}
		else
		{
			NewPawn->PrimaryActorTick.AddPrerequisite(this, this->PrimaryActorTick);
		}
	}
}


void AController::SetPawn(APawn* InPawn)
{
	RemovePawnTickDependency(Pawn);

	Pawn = InPawn;
	Character = (Pawn ? Cast<ACharacter>(Pawn) : NULL);

	AttachToPawn(Pawn);

	AddPawnTickDependency(Pawn);
}

void AController::SetPawnFromRep(APawn* InPawn)
{
	// This function is needed to ensure OnRep_Pawn is called in the case we need to set AController::Pawn
	// due to APawn::Controller being replicated first. See additional notes in APawn::OnRep_Controller.
	RemovePawnTickDependency(Pawn);
	Pawn = InPawn;
	OnRep_Pawn();
}

void AController::OnRep_Pawn()
{
	// Detect when pawn changes, so we can NULL out the controller on the old pawn
	if ( OldPawn != NULL && Pawn != OldPawn.Get() && OldPawn->Controller == this )
	{
		// Set the old controller to NULL, since we are no longer the owner, and can't rely on it replicating to us anymore
		OldPawn->Controller = NULL;
	}

	OldPawn = Pawn;

	SetPawn(Pawn);
}

void AController::OnRep_PlayerState()
{
	if (PlayerState != NULL)
	{
		PlayerState->ClientInitialize(this);
	}
}

void AController::Destroyed()
{
	if (Role == ROLE_Authority && PlayerState != NULL)
	{
		// if we are a player, log out
		AGameMode* const GameMode = GetWorld()->GetAuthGameMode();
		if (GameMode)
		{
			GameMode->Logout(this);
		}

		CleanupPlayerState();
	}

	UnPossess();
	GetWorld()->RemoveController( this );
	Super::Destroyed();
}


void AController::CleanupPlayerState()
{
	PlayerState->Destroy();
	PlayerState = NULL;
}

void AController::InstigatedAnyDamage(float Damage, const class UDamageType* DamageType, class AActor* DamagedActor, class AActor* DamageCauser)
{
	ReceiveInstigatedAnyDamage(Damage, DamageType, DamagedActor, DamageCauser);
	OnInstigatedAnyDamage.Broadcast(Damage, DamageType, DamagedActor, DamageCauser);
}

void AController::InitPlayerState()
{
	if ( GetNetMode() != NM_Client )
	{
		UWorld* const World = GetWorld();
		AGameMode* const GameMode = World ? World->GetAuthGameMode() : NULL;
		if (GameMode != NULL)
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Owner = this;
			SpawnInfo.Instigator = Instigator;
			SpawnInfo.bNoCollisionFail = true;
			PlayerState = World->SpawnActor<APlayerState>(GameMode->PlayerStateClass, SpawnInfo );
	
			// force a default player name if necessary
			if (PlayerState && PlayerState->PlayerName.IsEmpty())
			{
				// don't call SetPlayerName() as that will broadcast entry messages but the GameMode hasn't had a chance
				// to potentially apply a player/bot name yet
				PlayerState->PlayerName = GetDefault<AGameMode>()->DefaultPlayerName;
			}
		}
	}
}


void AController::GameHasEnded(AActor* EndGameFocus, bool bIsWinner)
{
}


FRotator AController::GetDesiredRotation() const
{
	return GetControlRotation();
}


void AController::GetActorEyesViewPoint( FVector& out_Location, FRotator& out_Rotation ) const
{
	// If we have a Pawn, this is our view point.
	if ( Pawn != NULL )
	{
		Pawn->GetActorEyesViewPoint( out_Location, out_Rotation );
	}
	// otherwise, controllers don't have a physical location
}


void AController::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	UFont* RenderFont = GEngine->GetSmallFont();
	if ( Pawn == NULL )
	{
		if (PlayerState == NULL)
		{
			Canvas->DrawText(RenderFont, TEXT("NO PlayerState"), 4.0f, YPos );
		}
		else
		{
			PlayerState->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
		}
		YPos += YL;

		Super::DisplayDebug(Canvas, DebugDisplay, YL,YPos);
		return;
	}

	Canvas->SetDrawColor(255,0,0);
	Canvas->DrawText(RenderFont, FString::Printf(TEXT("CONTROLLER %s Pawn %s"), *GetName(), *Pawn->GetName()), 4.0f, YPos );
	YPos += YL;
}

FString AController::GetHumanReadableName() const
{
	return PlayerState ? PlayerState->PlayerName : *GetName();
}

void AController::CurrentLevelUnloaded() {}

void AController::ChangeState(FName NewState)
{
	if(NewState != StateName)
	{
		// end current state
		if(StateName == NAME_Inactive)
		{
			EndInactiveState();
		}

		// Set new state name
		StateName = NewState;

		// start new state
		if(StateName == NAME_Inactive)
		{
			BeginInactiveState();
		}
	}
}

FName AController::GetStateName() const
{
	return StateName;
}

bool AController::IsInState(FName InStateName)
{
	return (StateName == InStateName);
}

void AController::BeginInactiveState() {}

void AController::EndInactiveState() {}


APlayerController* AController::CastToPlayerController()
{
	return Cast<APlayerController>(this);
}

APawn* AController::GetControlledPawn() const
{
	return Pawn;
}

const FNavAgentProperties* AController::GetNavAgentProperties() const
{
	return Pawn && Pawn->GetMovementComponent() ? Pawn->GetMovementComponent()->GetNavAgentProperties() : NULL;
}

FVector AController::GetNavAgentLocation() const
{
	return Pawn ? Pawn->GetNavAgentLocation() : FVector::ZeroVector;
}

void AController::GetMoveGoalReachTest(class AActor* MovingActor, const FVector& MoveOffset, FVector& GoalOffset, float& GoalRadius, float& GoalHalfHeight) const 
{
	if (Pawn)
	{
		Pawn->GetMoveGoalReachTest(MovingActor, MoveOffset, GoalOffset, GoalRadius, GoalHalfHeight); 
	}
}

void AController::UpdateNavigationComponents()
{
	UNavigationComponent* PathFindingComp = FindComponentByClass<UNavigationComponent>();
	if (PathFindingComp != NULL)
	{
		PathFindingComp->OnNavAgentChanged();
		PathFindingComp->UpdateCachedComponents();
	}

	UPathFollowingComponent* PathFollowingComp = FindComponentByClass<UPathFollowingComponent>();
	if (PathFollowingComp != NULL)
	{
		PathFollowingComp->UpdateCachedComponents();
	}

	// initialize movement mode in characters
	ACharacter* MyCharacter = Cast<ACharacter>(GetPawn());
	if (MyCharacter && MyCharacter->CharacterMovement)
	{
		MyCharacter->CharacterMovement->SetDefaultMovementMode();
	}
}

void AController::InitNavigationControl(UNavigationComponent*& PathFindingComp, UPathFollowingComponent*& PathFollowingComp)
{
	bool bInitPathFinding = false;
	PathFindingComp = FindComponentByClass<UNavigationComponent>();
	if (PathFindingComp == NULL)
	{
		PathFindingComp = NewObject<UNavigationComponent>(this);
		PathFindingComp->RegisterComponentWithWorld(GetWorld());
		bInitPathFinding = true;
	}

	bool bInitPathFollowing = false;
	PathFollowingComp = FindComponentByClass<UPathFollowingComponent>();
	if (PathFollowingComp == NULL)
	{
		PathFollowingComp = NewObject<UPathFollowingComponent>(this);
		PathFollowingComp->RegisterComponentWithWorld(GetWorld());
		bInitPathFollowing = true;
	}

	if (bInitPathFinding)
	{
		PathFindingComp->InitializeComponent();
	}

	if (bInitPathFollowing)
	{
		PathFollowingComp->InitializeComponent();
	}
}

void AController::StopMovement()
{
	UE_VLOG(this, LogNavigation, Log, TEXT("AController::StopMovement: %s STOP MOVEMENT"), *GetNameSafe(GetPawn()));

	UPathFollowingComponent* PathFollowingComp = FindComponentByClass<UPathFollowingComponent>();
	if (PathFollowingComp != NULL)
	{
		PathFollowingComp->AbortMove(TEXT("StopMovement"));
	}
}

void AController::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( AController, PlayerState );
	DOREPLIFETIME( AController, Pawn );
}