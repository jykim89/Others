// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
   DebugCameraInput.cpp: Native implementation for the debug camera
=============================================================================*/

#include "EnginePrivate.h"

// ------------------
// Externals
// ------------------

ADebugCameraHUD::ADebugCameraHUD(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bHidden = false;
}

bool ADebugCameraHUD::DisplayMaterials( float X, float& Y, float DY, UMeshComponent* MeshComp )
{
	bool bDisplayedMaterial = false;
	if ( MeshComp != NULL )
	{
		FFontRenderInfo FontRenderInfo = Canvas->CreateFontRenderInfo(false, true);

		UFont* Font = GEngine->GetSmallFont();
		for ( int32 MaterialIndex = 0; MaterialIndex < MeshComp->GetNumMaterials(); ++MaterialIndex )
		{
			UMaterialInterface* Material = MeshComp->GetMaterial(MaterialIndex);
			if ( Material != NULL )
			{
				Y += DY;
				Canvas->DrawText(Font, FString::Printf(TEXT("Material: '%s'"), *Material->GetFName().ToString()), X + DY, Y, 1.f, 1.f, FontRenderInfo );
				bDisplayedMaterial = true;
			}
		}
	}
	return bDisplayedMaterial;
}

void ADebugCameraHUD::PostRender()
{
	Super::PostRender();

	if (bShowHUD)
	{
		ADebugCameraController* DCC = Cast<ADebugCameraController>( PlayerOwner );
		UFont* RenderFont = GEngine->GetSmallFont();
		if( DCC != NULL )
		{
			FFontRenderInfo FontRenderInfo = Canvas->CreateFontRenderInfo(false, true);

			Canvas->SetDrawColor(64, 64, 255, 255);
			FString MyText = TEXT("Debug Camera");
			float xl, yl;
			Canvas->StrLen(RenderFont, MyText, xl, yl);
			float X = Canvas->SizeX * 0.05f;
			float Y = yl;//*1.67;
			yl += 2*Y;
			Canvas->DrawText(RenderFont, MyText, X, yl, 1.f, 1.f, FontRenderInfo);

			Canvas->SetDrawColor(200, 200, 128, 255);

			FVector const CamLoc = DCC->PlayerCameraManager->GetCameraLocation();
			FRotator const CamRot = DCC->PlayerCameraManager->GetCameraRotation();
			float const CamFOV = DCC->PlayerCameraManager->GetFOVAngle();

			yl += Y;
			
			FString const LocRotString = FString::Printf(TEXT("Loc=(%.1f, %.1f, %.1f) Rot=(%.1f, %.1f, %.1f)"), CamLoc.X, CamLoc.Y, CamLoc.Z, CamRot.Pitch, CamRot.Yaw, CamRot.Roll);
			Canvas->DrawText(RenderFont, LocRotString, X, yl, 1.f, 1.f, FontRenderInfo);
			yl += Y;
			
			FString const FOVString = FString::Printf(TEXT("HFOV=%.1f"), CamFOV);
			Canvas->DrawText(RenderFont, FOVString, X, yl, 1.f, 1.f, FontRenderInfo);
			yl += Y;
			
			FString const SpeedScaleString = FString::Printf(TEXT("SpeedScale=%.2fx"), DCC->SpeedScale);
			Canvas->DrawText(RenderFont, SpeedScaleString, X, yl, 1.f, 1.f, FontRenderInfo);
			yl += Y;
			
			FString const SpeedString = FString::Printf(TEXT("MaxSpeed=%.1f"), DCC->GetSpectatorPawn() && DCC->GetSpectatorPawn()->GetMovementComponent() ? DCC->GetSpectatorPawn()->GetMovementComponent()->GetMaxSpeed() : 0.f);
			Canvas->DrawText(RenderFont, SpeedString, X, yl, 1.f, 1.f, FontRenderInfo);
			yl += Y;

			//Canvas->DrawText(FString::Printf(TEXT("CamLoc:%s CamRot:%s"), *CamLoc.ToString(), *CamRot.ToString() ));

			const TCHAR* CVarComplexName = TEXT("g.DebugCameraTraceComplex");
			static const auto CVarComplex = IConsoleManager::Get().FindTConsoleVariableDataInt(CVarComplexName);
			const bool bTraceComplex = (CVarComplex ? (CVarComplex->GetValueOnGameThread() != 0) : true);

			FCollisionQueryParams TraceParams(NAME_None, bTraceComplex, this);
			FHitResult Hit;
			bool bHit = GetWorld()->LineTraceSingle(Hit, CamLoc, CamRot.Vector() * 100000.f + CamLoc, ECC_Pawn, TraceParams);

			yl += Y;
			Canvas->DrawText(RenderFont, FString::Printf(TEXT("Trace info (%s = %d):"), CVarComplexName, bTraceComplex ? 1 : 0), X, yl, 1.f, 1.f, FontRenderInfo);

			if( bHit )
			{
				AActor* HitActor = Hit.GetActor();
				yl += Y;
				Canvas->DrawText(RenderFont, FString::Printf(TEXT("HitLoc:%s HitNorm:%s"), *Hit.Location.ToString(), *Hit.Normal.ToString() ), X, yl, 1.f, 1.f, FontRenderInfo);
				yl += Y;
				Canvas->DrawText(RenderFont, FString::Printf(TEXT("HitDist: %f"), (CamLoc - Hit.Location).Size()), X, yl, 1.f, 1.f, FontRenderInfo);
				yl += Y;
				Canvas->DrawText(RenderFont, FString::Printf(TEXT("HitActor: '%s'"), HitActor ? *HitActor->GetFName().ToString() : TEXT("<NULL>")), X, yl, 1.f, 1.f, FontRenderInfo);
				yl += Y;
				Canvas->DrawText(RenderFont, FString::Printf(TEXT("HitComponent: '%s'"), Hit.Component.Get() ? *Hit.Component.Get()->GetFName().ToString() : TEXT("<NULL>")), X, yl, 1.f, 1.f, FontRenderInfo);
				yl += Y;
				Canvas->DrawText(RenderFont, FString::Printf(TEXT("HitActor Class: '%s'"), HitActor && HitActor->GetClass() ? *HitActor->GetClass()->GetName() : TEXT("<Not Found>") ), X, yl, 1.f, 1.f, FontRenderInfo);
				yl += Y;
				Canvas->DrawText(RenderFont, FString::Printf(TEXT("HitActorPath: '%s'"), HitActor ? *HitActor->GetPathName() : TEXT("<Not Found>")), X, yl, 1.f, 1.f, FontRenderInfo);
				yl += Y;

				bool bFoundMaterial = false;
				if ( Hit.Component != NULL )
				{
					bFoundMaterial = DisplayMaterials( X, yl, Y, Cast<UMeshComponent>(Hit.Component.Get()) );
				}
				else
				{
					TArray<UMeshComponent*> Components;
					GetComponents(Components);

					for ( int32 i=0; i<Components.Num(); i++ )
					{
						UMeshComponent* MeshComp = Components[i];
						if ( MeshComp->IsRegistered() )
						{
							bFoundMaterial = bFoundMaterial || DisplayMaterials( X, yl, Y, MeshComp );	
						}
					}
				}
				if ( bFoundMaterial == false )
				{
					yl += Y;
					Canvas->DrawText(RenderFont, "Material: NULL", X + Y, yl, 1.f, 1.f, FontRenderInfo );
				}
				DrawDebugLine( GetWorld(), Hit.Location, Hit.Location+Hit.Normal*30.f, FColor::White );
			}
			else
			{
				yl += Y;
				Canvas->DrawText( RenderFont, TEXT("No trace Hit"), X, yl, 1.f, 1.f, FontRenderInfo);
			}

			if ( DCC->bShowSelectedInfo && DCC->SelectedActor != NULL )
			{
				yl += Y;
				Canvas->DrawText(RenderFont,  FString::Printf(TEXT("Selected actor: '%s'"), *DCC->SelectedActor->GetFName().ToString()), X, yl, 1.f, 1.f, FontRenderInfo);
				DisplayMaterials( X, yl, Y, Cast<UMeshComponent>(DCC->SelectedComponent) );
			}


			// controls display
			yl += Y*15;
			
			Canvas->SetDrawColor(64, 64, 255, 255);
			Canvas->DrawText(RenderFont, TEXT("Controls"), X, yl, 1.f, 1.f, FontRenderInfo);
			yl += Y;
			
			Canvas->SetDrawColor(200, 200, 128, 255);
			Canvas->DrawText(RenderFont, TEXT("FOV +/-: ,/. or DPad Up/Down"), X, yl, 1.f, 1.f, FontRenderInfo);
			yl += Y;
			
			Canvas->DrawText(RenderFont, TEXT("Speed +/-: MouseWheel or +/- or LB/RB"), X, yl, 1.f, 1.f, FontRenderInfo);
			yl += Y;
			
			Canvas->DrawText(RenderFont, TEXT("Freeze Rendering: F or YButton"), X, yl, 1.f, 1.f, FontRenderInfo);
			yl += Y;			
			
			Canvas->DrawText(RenderFont, TEXT("Toggle Display: BackSpace or XButton"), X, yl, 1.f, 1.f, FontRenderInfo);
			yl += Y;
		}
	}
}
