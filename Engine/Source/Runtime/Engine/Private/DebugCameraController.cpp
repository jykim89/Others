// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
   DebugCameraController.cpp: Native implementation for the debug camera

=============================================================================*/

#include "EnginePrivate.h"

/** The currently selected actor. */
AActor* GDebugSelectedActor = NULL;
/** The currently selected component in the actor. */
ENGINE_API UPrimitiveComponent* GDebugSelectedComponent = NULL;
/** The lightmap used by the currently selected component, if it's a static mesh component. */
ENGINE_API FLightMap2D* GDebugSelectedLightmap = NULL;

extern bool UntrackTexture( const FString& TextureName );
extern bool TrackTexture( const FString& TextureName );

static const float SPEED_SCALE_ADJUSTMENT = 0.5f;


ADebugCameraController::ADebugCameraController(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SelectedActor = NULL;
	SelectedComponent = NULL;
	OriginalControllerRef = NULL;
	OriginalPlayer = NULL;

	SpeedScale = 1.f;
	InitialMaxSpeed = 0.f;
	InitialAccel = 0.f;
	InitialDecel = 0.f;

	bIsFrozenRendering = false;
	DrawFrustum = NULL;
	bHidden = false;
#if WITH_EDITORONLY_DATA
	bHiddenEd = false;
#endif // WITH_EDITORONLY_DATA
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bShouldPerformFullTickWhenPaused = true;
}

void InitializeDebugCameraInputBindings()
{
	static bool bBindingsAdded = false;
	if (!bBindingsAdded)
	{
		bBindingsAdded = true;

		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_Select", EKeys::LeftMouseButton));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_Unselect", EKeys::Escape));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_IncreaseSpeed", EKeys::Add));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_IncreaseSpeed", EKeys::MouseScrollUp));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_DecreaseSpeed", EKeys::Subtract));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_DecreaseSpeed", EKeys::MouseScrollDown));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_IncreaseFOV", EKeys::Comma));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_DecreaseFOV", EKeys::Period));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_ToggleDisplay", EKeys::BackSpace));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_FreezeRendering", EKeys::F));

		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_Select", EKeys::Gamepad_RightTrigger));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_IncreaseSpeed", EKeys::Gamepad_RightShoulder));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_DecreaseSpeed", EKeys::Gamepad_LeftShoulder));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_IncreaseFOV", EKeys::Gamepad_DPad_Up));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_DecreaseFOV", EKeys::Gamepad_DPad_Down));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_ToggleDisplay", EKeys::Gamepad_FaceButton_Left));
		UPlayerInput::AddEngineDefinedActionMapping(FInputActionKeyMapping("DebugCamera_FreezeRendering", EKeys::Gamepad_FaceButton_Top));
	}
}

void ADebugCameraController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InitializeDebugCameraInputBindings();
	InputComponent->BindAction("DebugCamera_Select", IE_Pressed, this, &ADebugCameraController::SelectTargetedObject);
	InputComponent->BindAction("DebugCamera_Unselect", IE_Pressed, this, &ADebugCameraController::Unselect);

	InputComponent->BindAction("DebugCamera_IncreaseSpeed", IE_Pressed, this, &ADebugCameraController::IncreaseCameraSpeed);
	InputComponent->BindAction("DebugCamera_DecreaseSpeed", IE_Pressed, this, &ADebugCameraController::DecreaseCameraSpeed);

	InputComponent->BindAction("DebugCamera_IncreaseFOV", IE_Pressed, this, &ADebugCameraController::IncreaseFOV);
	InputComponent->BindAction("DebugCamera_DecreaseFOV", IE_Pressed, this, &ADebugCameraController::DecreaseFOV);

	InputComponent->BindAction("DebugCamera_ToggleDisplay", IE_Pressed, this, &ADebugCameraController::ToggleDisplay);
	InputComponent->BindAction("DebugCamera_FreezeRendering", IE_Pressed, this, &ADebugCameraController::ToggleFreezeRendering);
}

void ADebugCameraController::Select( FHitResult const& Hit )
{
	// First untrack the currently tracked lightmap.
	UTexture2D* Texture2D = GDebugSelectedLightmap ? GDebugSelectedLightmap->GetTexture(0) : NULL;
	if ( Texture2D )
	{
		UntrackTexture( Texture2D->GetName() );
	}

	// store selection

	SelectedActor = Hit.GetActor();
	SelectedComponent = Hit.Component.Get();
	GDebugSelectedActor = SelectedActor;
	GDebugSelectedComponent = SelectedComponent;
	
	// figure out lightmap
	GDebugSelectedLightmap = NULL;
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>( SelectedComponent );
	if ( StaticMeshComponent && StaticMeshComponent->LODData.Num() > 0 )
	{
		const FStaticMeshComponentLODInfo& LODInfo = StaticMeshComponent->LODData[0];
		if ( LODInfo.LightMap )
		{
			GDebugSelectedLightmap = LODInfo.LightMap->GetLightMap2D();
			Texture2D = GDebugSelectedLightmap ? GDebugSelectedLightmap->GetTexture(0) : NULL;
			if ( Texture2D )
			{
				extern bool TrackTexture( const FString& TextureName );
				TrackTexture( Texture2D->GetName() );
			}
		}
	}
}


void ADebugCameraController::Unselect()
{
	UTexture2D* Texture2D = GDebugSelectedLightmap ? GDebugSelectedLightmap->GetTexture(0) : NULL;
	if ( Texture2D )
	{
		extern bool UntrackTexture( const FString& TextureName );
		UntrackTexture( Texture2D->GetName() );
	}
	
	SelectedActor = NULL;
	SelectedComponent = NULL;
	
	GDebugSelectedActor = NULL;
	GDebugSelectedComponent = NULL;
	GDebugSelectedLightmap = NULL;
}




FString ADebugCameraController::ConsoleCommand(const FString& Cmd,bool bWriteToLog)
{
	/**
	 * This is the same as PlayerController::ConsoleCommand(), except with some extra code to 
	 * give our regular PC a crack at handling the command.
	 */
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
			if (Player->Exec( GetWorld(), Line, StrOut) == false)
			{
				Player->PlayerController = OriginalControllerRef;
				Player->Exec( GetWorld(), Line, StrOut);
				Player->PlayerController = this;
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

void ADebugCameraController::UpdateHiddenComponents(const FVector& ViewLocation,TSet<FPrimitiveComponentId>& HiddenComponents)
{
	if (OriginalControllerRef != NULL)
	{
		OriginalControllerRef->UpdateHiddenComponents(ViewLocation,HiddenComponents);
	}
}


void ADebugCameraController::SetSpectatorPawn(ASpectatorPawn* NewSpectatorPawn)
{
	Super::SetSpectatorPawn(NewSpectatorPawn);
	if (GetSpectatorPawn())
	{
		GetSpectatorPawn()->SetActorEnableCollision(false);
		GetSpectatorPawn()->PrimaryActorTick.bTickEvenWhenPaused = bShouldPerformFullTickWhenPaused;
		USpectatorPawnMovement* SpectatorMovement = Cast<USpectatorPawnMovement>(GetSpectatorPawn()->GetMovementComponent());
		if (SpectatorMovement)
		{
			SpectatorMovement->bIgnoreTimeDilation = true;
			SpectatorMovement->PrimaryComponentTick.bTickEvenWhenPaused = bShouldPerformFullTickWhenPaused;
			InitialMaxSpeed = SpectatorMovement->MaxSpeed;
			InitialAccel = SpectatorMovement->Acceleration;
			InitialDecel = SpectatorMovement->Deceleration;
			ApplySpeedScale();
		}
	}
}

void ADebugCameraController::EndSpectatingState()
{
	DestroySpectatorPawn();
}

void ADebugCameraController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// if hud is existing, delete it and create new hud for debug camera
	if ( MyHUD != NULL )
	{
		MyHUD->Destroy();
	}
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.Instigator = Instigator;
	SpawnInfo.bNoCollisionFail = true;
	MyHUD = GetWorld()->SpawnActor<ADebugCameraHUD>( SpawnInfo );

	ChangeState(NAME_Spectating);
}

void ADebugCameraController::OnActivate( APlayerController* OriginalPC )
{
	// keep these around
	OriginalPlayer = OriginalPC->Player;
	OriginalControllerRef = OriginalPC;
	
	FVector OrigCamLoc;
	FRotator OrigCamRot;
	OriginalPC->GetPlayerViewPoint(OrigCamLoc, OrigCamRot);
	float const OrigCamFOV = OriginalPC->PlayerCameraManager->GetFOVAngle();

	ChangeState(NAME_Spectating);

	// start debug camera at original camera pos
	SetInitialLocationAndRotation(OrigCamLoc, OrigCamRot);

	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetFOV( OrigCamFOV );
		PlayerCameraManager->UpdateCamera(0.0f);
	}

	// draw frustum of original camera (where you detached)
	if (DrawFrustum == NULL)
	{
		DrawFrustum = NewObject<UDrawFrustumComponent>(OriginalPC->PlayerCameraManager);
	}
	if (DrawFrustum)
	{
		DrawFrustum->SetVisibility(true);
		OriginalPC->SetActorHiddenInGame(false);
		OriginalPC->PlayerCameraManager->SetActorHiddenInGame(false);

		DrawFrustum->FrustumAngle = OrigCamFOV;
		DrawFrustum->SetAbsolute(true, true, false);
		DrawFrustum->SetRelativeLocation(OrigCamLoc);
		DrawFrustum->SetRelativeRotation(OrigCamRot);
		DrawFrustum->RegisterComponent();

		ConsoleCommand(TEXT("show camfrustums")); //called to render camera frustums from original player camera
	}

	GetWorld()->AddController(this);
}


void ADebugCameraController::AddCheats(bool bForce)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Super::AddCheats(true);
#else
	Super::AddCheats(bForce);
#endif
}



void ADebugCameraController::OnDeactivate( APlayerController* RestoredPC )
{
	// restore FreezeRendering command state
	if (bIsFrozenRendering) 
	{
		ConsoleCommand(TEXT("FreezeRendering"));
		bIsFrozenRendering = false;
	}

	DrawFrustum->SetVisibility(false);
	ConsoleCommand(TEXT("show camfrustums"));
	DrawFrustum->UnregisterComponent();
	RestoredPC->SetActorHiddenInGame(true);
	RestoredPC->PlayerCameraManager->SetActorHiddenInGame(true);

	OriginalControllerRef = NULL;
	OriginalPlayer = NULL;

	ChangeState(NAME_Inactive);
	GetWorld()->RemoveController(this);
}


void ADebugCameraController::ToggleFreezeRendering()
{
	ConsoleCommand(TEXT("FreezeRendering"));
	bIsFrozenRendering = !bIsFrozenRendering;
}

void ADebugCameraController::SelectTargetedObject()
{
	FVector CamLoc;
	FRotator CamRot;
	GetPlayerViewPoint(CamLoc, CamRot);

	FHitResult Hit;
	FCollisionQueryParams TraceParams(NAME_None, true, this);
	bool const bHit = GetWorld()->LineTraceSingle(Hit, CamLoc, CamRot.Vector() * 5000.f * 20.f + CamLoc, ECC_Pawn, TraceParams);
	if( bHit)
	{
		Select(Hit);
	}
}

void ADebugCameraController::ShowDebugSelectedInfo()
{
	bShowSelectedInfo = !bShowSelectedInfo;
}

void ADebugCameraController::IncreaseCameraSpeed()
{
	SpeedScale += SPEED_SCALE_ADJUSTMENT;
	ApplySpeedScale();
}

void ADebugCameraController::DecreaseCameraSpeed()
{
	SpeedScale -= SPEED_SCALE_ADJUSTMENT;
	SpeedScale = FMath::Max(SPEED_SCALE_ADJUSTMENT, SpeedScale);
	ApplySpeedScale();
}

void ADebugCameraController::ApplySpeedScale()
{
	ASpectatorPawn* Spectator = GetSpectatorPawn();
	if (Spectator)
	{
		USpectatorPawnMovement* SpectatorMovement = Cast<USpectatorPawnMovement>(Spectator->GetMovementComponent());
		if (SpectatorMovement)
		{
			SpectatorMovement->MaxSpeed = InitialMaxSpeed * SpeedScale;
			SpectatorMovement->Acceleration = InitialAccel * SpeedScale;
			SpectatorMovement->Deceleration = InitialDecel * SpeedScale;
		}
	}
}

void ADebugCameraController::IncreaseFOV()
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetFOV( PlayerCameraManager->GetFOVAngle() + 1.f );
	}
}
void ADebugCameraController::DecreaseFOV()
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->SetFOV( PlayerCameraManager->GetFOVAngle() - 1.f );
	}
}

void ADebugCameraController::ToggleDisplay()
{
	if (MyHUD)
	{
		MyHUD->ShowHUD();
	}
}


