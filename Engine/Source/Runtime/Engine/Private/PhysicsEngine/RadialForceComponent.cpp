// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Net/UnrealNetwork.h"

//////////////////////////////////////////////////////////////////////////
// RADIALFORCECOMPONENT
URadialForceComponent::URadialForceComponent(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	Radius = 200.0f;
	Falloff = RIF_Constant;
	ImpulseStrength = 1000.0f;
	ForceStrength = 10.0f;
	bAutoActivate = true;

	// by default we affect all 'dynamic' objects that can currently be affected by forces
	AddCollisionChannelToAffect(ECC_Pawn);
	AddCollisionChannelToAffect(ECC_PhysicsBody);
	AddCollisionChannelToAffect(ECC_Vehicle);
	AddCollisionChannelToAffect(ECC_Destructible);

	UpdateCollisionObjectQueryParams();
}

void URadialForceComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(bIsActive)
	{
		const FVector Origin = GetComponentLocation();

		// Find objects within the sphere
		static FName AddForceOverlapName = FName(TEXT("AddForceOverlap"));
		TArray<FOverlapResult> Overlaps;

		FCollisionQueryParams Params(AddForceOverlapName, false);
		Params.bTraceAsyncScene = true; // want to hurt stuff in async scene
		GetWorld()->OverlapMulti(Overlaps, Origin, FQuat::Identity, FCollisionShape::MakeSphere(Radius), Params, CollisionObjectQueryParams);

		// Iterate over each and apply force
		for ( int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); OverlapIdx++ )
		{
			UPrimitiveComponent* PokeComp = Overlaps[OverlapIdx].Component.Get();
			if(PokeComp)
			{
				PokeComp->AddRadialForce( Origin, Radius, ForceStrength, Falloff );

				// see if this is a target for a movement component
				TArray<UMovementComponent*> MovementComponents;
				PokeComp->GetOwner()->GetComponents<UMovementComponent>(MovementComponents);
				for(const auto& MovementComponent : MovementComponents)
				{
					if(MovementComponent->UpdatedComponent == PokeComp)
					{
						MovementComponent->AddRadialForce( Origin, Radius, ForceStrength, Falloff );
						break;
					}
				}
			}
		}
	}
}

void URadialForceComponent::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerUE4Version() < VER_UE4_CONFORM_COMPONENT_ACTIVATE_FLAG)
	{
		bAutoActivate = bForceEnabled_DEPRECATED;
	}

	UpdateCollisionObjectQueryParams();
}

void URadialForceComponent::FireImpulse()
{
	const FVector Origin = GetComponentLocation();

	// Find objects within the sphere
	static FName FireImpulseOverlapName = FName(TEXT("FireImpulseOverlap"));
	TArray<FOverlapResult> Overlaps;

	FCollisionQueryParams Params(FireImpulseOverlapName, false);
	Params.bTraceAsyncScene = true; // want to hurt stuff in async scene
	GetWorld()->OverlapMulti(Overlaps, Origin, FQuat::Identity, FCollisionShape::MakeSphere(Radius), Params, CollisionObjectQueryParams);

	// Iterate over each and apply an impulse
	for ( int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); OverlapIdx++ )
	{
		UPrimitiveComponent* PokeComp = Overlaps[OverlapIdx].Component.Get();
		if(PokeComp)
		{
			// If DestructibleDamage is non-zero, see if this is a destructible, and do damage if so.
			if(DestructibleDamage > SMALL_NUMBER)
			{
				UDestructibleComponent* DestructibleComp = Cast<UDestructibleComponent>(PokeComp);
				if(DestructibleComp != NULL)
				{
					DestructibleComp->ApplyRadiusDamage(DestructibleDamage, Origin, Radius, ImpulseStrength, (Falloff == RIF_Constant));
				}
			}

			// Do impulse after
			PokeComp->AddRadialImpulse( Origin, Radius, ImpulseStrength, Falloff, bImpulseVelChange );

			// see if this is a target for a movement component
			TArray<UMovementComponent*> MovementComponents;
			PokeComp->GetOwner()->GetComponents<UMovementComponent>(MovementComponents);
			for(const auto& MovementComponent : MovementComponents)
			{
				if(MovementComponent->UpdatedComponent == PokeComp)
				{
					MovementComponent->AddRadialImpulse( Origin, Radius, ImpulseStrength, Falloff, bImpulseVelChange );
					break;
				}
			}
		}
	}
}

void URadialForceComponent::AddCollisionChannelToAffect(enum ECollisionChannel CollisionChannel)
{
	EObjectTypeQuery ObjectType = UEngineTypes::ConvertToObjectType(CollisionChannel);
	if(ObjectType != ObjectTypeQuery_MAX)
	{
		AddObjectTypeToAffect(ObjectType);
	}
}

void URadialForceComponent::AddObjectTypeToAffect(TEnumAsByte<enum EObjectTypeQuery> ObjectType)
{
	ObjectTypesToAffect.AddUnique(ObjectType);
	UpdateCollisionObjectQueryParams();
}

void URadialForceComponent::RemoveObjectTypeToAffect(TEnumAsByte<enum EObjectTypeQuery> ObjectType)
{
	ObjectTypesToAffect.Remove(ObjectType);
	UpdateCollisionObjectQueryParams();
}

#if WITH_EDITOR

void URadialForceComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// If we have edited the object types to effect, update our bitfield.
	if(PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == TEXT("ObjectTypesToAffect"))
	{
		UpdateCollisionObjectQueryParams();
	}
}

#endif

void URadialForceComponent::UpdateCollisionObjectQueryParams()
{
	CollisionObjectQueryParams = FCollisionObjectQueryParams(ObjectTypesToAffect);
}


//////////////////////////////////////////////////////////////////////////
// ARB_RADIALFORCEACTOR
ARadialForceActor::ARadialForceActor(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	ForceComponent = PCIP.CreateDefaultSubobject<URadialForceComponent>(this, TEXT("ForceComponent0"));

#if WITH_EDITOR
	SpriteComponent = PCIP.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("Sprite"));
	if (SpriteComponent)
	{
		// Structure to hold one-time initialization
		if (!IsRunningCommandlet())
		{
			struct FConstructorStatics
			{
				ConstructorHelpers::FObjectFinderOptional<UTexture2D> RadialForceTexture;
				FName ID_Physics;
				FText NAME_Physics;
				FConstructorStatics()
					: RadialForceTexture(TEXT("/Engine/EditorResources/S_RadForce.S_RadForce"))
					, ID_Physics(TEXT("Physics"))
					, NAME_Physics(NSLOCTEXT( "SpriteCategory", "Physics", "Physics" ))
				{
				}
			};
			static FConstructorStatics ConstructorStatics;

			SpriteComponent->Sprite = ConstructorStatics.RadialForceTexture.Get();

#if WITH_EDITORONLY_DATA
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Physics;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Physics;
#endif // WITH_EDITORONLY_DATA
		}

		SpriteComponent->RelativeScale3D.X = 0.5f;
		SpriteComponent->RelativeScale3D.Y = 0.5f;
		SpriteComponent->RelativeScale3D.Z = 0.5f;
		SpriteComponent->AttachParent = ForceComponent;
		SpriteComponent->bIsScreenSizeScaled = true;
	}
#endif

	RootComponent = ForceComponent;
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	NetUpdateFrequency = 0.1f;
}

#if WITH_EDITOR
void ARadialForceActor::EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	FVector ModifiedScale = DeltaScale * ( AActor::bUsePercentageBasedScaling ? 500.0f : 5.0f );

	const float Multiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Y > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;
	if(ForceComponent.IsValid())
	{
		ForceComponent->Radius += Multiplier * ModifiedScale.Size();
		ForceComponent->Radius = FMath::Max( 0.f, ForceComponent->Radius );
	}
}
#endif

void ARadialForceActor::FireImpulse()
{
	if(ForceComponent.IsValid())
	{
		ForceComponent->FireImpulse();
	}
}

void ARadialForceActor::EnableForce()
{
	if(ForceComponent.IsValid())
	{
		ForceComponent->Activate();
	}
}

void ARadialForceActor::DisableForce()
{
	if(ForceComponent.IsValid())
	{
		ForceComponent->Deactivate();
	}
}

void ARadialForceActor::ToggleForce()
{
	if(ForceComponent.IsValid())
	{
		ForceComponent->ToggleActive();
	}
}

