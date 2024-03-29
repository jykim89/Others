// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

APlayerStart::APlayerStart(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	CapsuleComponent->InitCapsuleSize(40.0f, 92.0f);

#if WITH_EDITORONLY_DATA
	ArrowComponent = PCIP.CreateEditorOnlyDefaultSubobject<UArrowComponent>(this, TEXT("Arrow"));

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> PlayerStartTextureObject;
			FName ID_PlayerStart;
			FText NAME_PlayerStart;
			FName ID_Navigation;
			FText NAME_Navigation;
			FConstructorStatics()
				: PlayerStartTextureObject(TEXT("/Engine/EditorResources/S_Player"))
				, ID_PlayerStart(TEXT("PlayerStart"))
				, NAME_PlayerStart(NSLOCTEXT("SpriteCategory", "PlayerStart", "Player Start"))
				, ID_Navigation(TEXT("Navigation"))
				, NAME_Navigation(NSLOCTEXT("SpriteCategory", "Navigation", "Navigation"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (GoodSprite)
		{
			GoodSprite->Sprite = ConstructorStatics.PlayerStartTextureObject.Get();
			GoodSprite->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
			GoodSprite->SpriteInfo.Category = ConstructorStatics.ID_PlayerStart;
			GoodSprite->SpriteInfo.DisplayName = ConstructorStatics.NAME_PlayerStart;
		}
		if (BadSprite)
		{
			BadSprite->SetVisibility(false);
		}

		if (ArrowComponent)
		{
			ArrowComponent->ArrowColor = FColor(150, 200, 255);

			ArrowComponent->ArrowSize = 1.0f;
			ArrowComponent->bTreatAsASprite = true;
			ArrowComponent->SpriteInfo.Category = ConstructorStatics.ID_Navigation;
			ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Navigation;
			ArrowComponent->AttachParent = CapsuleComponent;
			ArrowComponent->bIsScreenSizeScaled = true;
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void APlayerStart::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if ( !IsPendingKill()  && GetWorld()->GetAuthGameMode() )
	{
		GetWorld()->GetAuthGameMode()->AddPlayerStart(this);
	}
}

void APlayerStart::PostUnregisterAllComponents()
{
	Super::PostUnregisterAllComponents();

	UWorld* ActorWorld = GetWorld();
	if ( ActorWorld && ActorWorld->GetAuthGameMode() )
	{
		ActorWorld->GetAuthGameMode()->RemovePlayerStart(this);
	}
}

