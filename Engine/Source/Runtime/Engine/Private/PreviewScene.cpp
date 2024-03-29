// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PreviewScene.cpp: Preview scene implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "SoundDefinitions.h"
#include "PreviewScene.h"

FPreviewScene::FPreviewScene(FPreviewScene::ConstructionValues CVS)
	: PreviewWorld(NULL)
	, bForceAllUsedMipsResident(CVS.bForceMipsResident)
{
	PreviewWorld = new UWorld(FPostConstructInitializeProperties(),FURL(NULL));
	PreviewWorld->WorldType = EWorldType::Preview;
	if (!CVS.bTransactional)
	{
		PreviewWorld->ClearFlags(RF_Transactional);
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Preview);
	WorldContext.SetCurrentWorld(PreviewWorld);

	PreviewWorld->InitializeNewWorld(UWorld::InitializationValues()
										.AllowAudioPlayback(CVS.bAllowAudioPlayback)
										.CreatePhysicsScene(CVS.bCreatePhysicsScene)
										.RequiresHitProxies(false)
										.ShouldSimulatePhysics(CVS.bShouldSimulatePhysics)
										.SetTransactional(CVS.bTransactional));
	PreviewWorld->InitializeActorsForPlay(FURL());

	GetScene()->UpdateDynamicSkyLight(FLinearColor::White * CVS.SkyBrightness, FLinearColor::Black);

	DirectionalLight = ConstructObject<UDirectionalLightComponent>(UDirectionalLightComponent::StaticClass());
	DirectionalLight->Intensity = CVS.LightBrightness;
	DirectionalLight->LightColor = FColor(255,255,255);
	AddComponent(DirectionalLight, FTransform(CVS.LightRotation));

	LineBatcher = ConstructObject<ULineBatchComponent>(ULineBatchComponent::StaticClass());
	AddComponent(LineBatcher, FTransform::Identity);
}

FPreviewScene::~FPreviewScene()
{
	// Stop any audio components playing in this scene
	if( GEngine && GEngine->GetAudioDevice() )
	{
		GEngine->GetAudioDevice()->Flush( GetWorld(), false );
	}

	// Remove all the attached components
	for( int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++ )
	{
		UActorComponent* Component = Components[ ComponentIndex ];

		if (bForceAllUsedMipsResident)
		{
			// Remove the mip streaming override on the mesh to be removed
			UMeshComponent* pMesh = Cast<UMeshComponent>(Component);
			if (pMesh != NULL)
			{
				pMesh->SetTextureForceResidentFlag(false);
			}
		}

		Component->UnregisterComponent();
	}
	
	PreviewWorld->CleanupWorld();
	GEngine->DestroyWorldContext(GetWorld());
}

void FPreviewScene::AddComponent(UActorComponent* Component,const FTransform& LocalToWorld)
{
	Components.AddUnique(Component);

	USceneComponent* SceneComp = Cast<USceneComponent>(Component);
	if(SceneComp && SceneComp->GetAttachParent() == NULL)
	{
		SceneComp->SetRelativeTransform(LocalToWorld);
	}

	Component->RegisterComponentWithWorld(GetWorld());

	if (bForceAllUsedMipsResident)
	{
		// Add a mip streaming override to the new mesh
		UMeshComponent* pMesh = Cast<UMeshComponent>(Component);
		if (pMesh != NULL)
		{
			pMesh->SetTextureForceResidentFlag(true);
		}
	}

	GetScene()->UpdateSpeedTreeWind(0.0);
}

void FPreviewScene::RemoveComponent(UActorComponent* Component)
{
	Component->UnregisterComponent();
	Components.Remove(Component);

	if (bForceAllUsedMipsResident)
	{
		// Remove the mip streaming override on the old mesh
		UMeshComponent* pMesh = Cast<UMeshComponent>(Component);
		if (pMesh != NULL)
		{
			pMesh->SetTextureForceResidentFlag(false);
		}
	}
}

void FPreviewScene::AddReferencedObjects( FReferenceCollector& Collector )
{
	for( int32 Index = 0; Index < Components.Num(); Index++ )
	{
		Collector.AddReferencedObject( Components[ Index ] );
	}
	Collector.AddReferencedObject( DirectionalLight );
	Collector.AddReferencedObject( PreviewWorld );
}

void FPreviewScene::ClearLineBatcher()
{
	if (LineBatcher != NULL)
	{
		LineBatcher->Flush();
	}
}

/** Accessor for finding the current direction of the preview scene's DirectionalLight. */
FRotator FPreviewScene::GetLightDirection()
{
	return DirectionalLight->ComponentToWorld.GetUnitAxis( EAxis::X ).Rotation();
}

/** Function for modifying the current direction of the preview scene's DirectionalLight. */
void FPreviewScene::SetLightDirection(const FRotator& InLightDir)
{
#if WITH_EDITOR
	DirectionalLight->PreEditChange(NULL);
#endif // WITH_EDITOR
	DirectionalLight->SetAbsolute(true, true, true);
	DirectionalLight->SetRelativeRotation(InLightDir);
#if WITH_EDITOR
	DirectionalLight->PostEditChange();
#endif // WITH_EDITOR
}

void FPreviewScene::SetLightBrightness(float LightBrightness)
{
#if WITH_EDITOR
	DirectionalLight->PreEditChange(NULL);
#endif // WITH_EDITOR
	DirectionalLight->Intensity = LightBrightness;
#if WITH_EDITOR
	DirectionalLight->PostEditChange();
#endif // WITH_EDITOR
}

void FPreviewScene::SetLightColor(const FColor& LightColor)
{
#if WITH_EDITOR
	DirectionalLight->PreEditChange(NULL);
#endif // WITH_EDITOR
	DirectionalLight->LightColor = LightColor;
#if WITH_EDITOR
	DirectionalLight->PostEditChange();
#endif // WITH_EDITOR
}

void FPreviewScene::SetSkyBrightness(float SkyBrightness)
{
	GetScene()->UpdateDynamicSkyLight(FLinearColor::White * SkyBrightness, FLinearColor::Black);
}

void FPreviewScene::LoadSettings(const TCHAR* Section)
{
	FRotator LightDir;
	if ( GConfig->GetRotator( Section, TEXT("LightDir"), LightDir, GEditorUserSettingsIni ) )
	{
		SetLightDirection( LightDir );
	}
}

void FPreviewScene::SaveSettings(const TCHAR* Section)
{
	GConfig->SetRotator( Section, TEXT("LightDir"), GetLightDirection(), GEditorUserSettingsIni );
}
