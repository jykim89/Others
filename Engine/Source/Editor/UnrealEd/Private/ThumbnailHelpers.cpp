// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "ThumbnailHelpers.h"
#include "FXSystem.h"

/*
***************************************************************
  FThumbnailPreviewScene
***************************************************************
*/

FThumbnailPreviewScene::FThumbnailPreviewScene()
	: FPreviewScene( ConstructionValues()
						.SetLightRotation( FRotator(304.736, 39.84, 0) )
						.SetCreatePhysicsScene(false)
						.SetTransactional(false))
{
	// A background sky sphere
 	UStaticMeshComponent* BackgroundComponent = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());
 	BackgroundComponent->SetStaticMesh( GUnrealEd->GetThumbnailManager()->EditorSkySphere );
 	const float SkySphereScale = 2000.0f;
 	const FTransform BackgroundTransform(FRotator(0,0,0), FVector(0,0,0), FVector(SkySphereScale));
 	FPreviewScene::AddComponent(BackgroundComponent, BackgroundTransform);

	// Adjust the default light
	DirectionalLight->Intensity = 0.2f;

	// Add additional lights
	UDirectionalLightComponent* DirectionalLight2 = ConstructObject<UDirectionalLightComponent>(UDirectionalLightComponent::StaticClass());
	DirectionalLight->Intensity = 5.0f;
	AddComponent(DirectionalLight2, FTransform( FRotator(-40,-144.678, 0) ));

	UDirectionalLightComponent* DirectionalLight3 = ConstructObject<UDirectionalLightComponent>(UDirectionalLightComponent::StaticClass());
	DirectionalLight->Intensity = 1.0f;
	AddComponent(DirectionalLight3, FTransform( FRotator(299.235,144.993, 0) ));

	// Add an infinite plane
	const float FloorPlaneScale = 10000.f;
	const FTransform FloorPlaneTransform(FRotator(-90.f,0,0), FVector::ZeroVector, FVector(FloorPlaneScale));
	UStaticMeshComponent* FloorPlaneComponent = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());
	FloorPlaneComponent->SetStaticMesh( GUnrealEd->GetThumbnailManager()->EditorPlane );
	FloorPlaneComponent->SetMaterial( 0, GUnrealEd->GetThumbnailManager()->FloorPlaneMaterial );
	FPreviewScene::AddComponent(FloorPlaneComponent, FloorPlaneTransform);
}

void FThumbnailPreviewScene::GetView(FSceneViewFamily* ViewFamily, int32 X, int32 Y, uint32 SizeX, uint32 SizeY) const
{
	check(ViewFamily);

	FIntRect ViewRect(
		FMath::Max<int32>(X,0),
		FMath::Max<int32>(Y,0),
		FMath::Max<int32>(X+SizeX,0),
		FMath::Max<int32>(Y+SizeY,0));

	if (ViewRect.Width() > 0 && ViewRect.Height() > 0)
	{
		const float FOVDegrees = 30.f;
		const float HalfFOVRadians = FMath::DegreesToRadians<float>(FOVDegrees) * 0.5f;
		const float NearPlane = 1.0f;
		FMatrix ProjectionMatrix = FReversedZPerspectiveMatrix(
			HalfFOVRadians,
			1.0f,
			1.0f,
			NearPlane
			);

		FVector Origin(0);
		float OrbitPitch = 0;
		float OrbitYaw = 0;
		float OrbitZoom = 0;
		GetViewMatrixParameters(FOVDegrees, Origin, OrbitPitch, OrbitYaw, OrbitZoom);

		// Ensure a minimum camera distance to prevent problems with really small objects
		const float MinCameraDistance = 48;
		OrbitZoom = FMath::Max<float>(MinCameraDistance, OrbitZoom);

		const FRotator RotationOffsetToViewCenter(0.f, 90.f, 0.f);
		FMatrix ViewMatrix = FTranslationMatrix( Origin ) *
			FRotationMatrix( FRotator(0, OrbitYaw, 0) ) * 
			FRotationMatrix( FRotator(0, 0, OrbitPitch) ) *
			FTranslationMatrix( FVector(0, OrbitZoom, 0) ) *
			FInverseRotationMatrix( RotationOffsetToViewCenter );

		ViewMatrix = ViewMatrix * FMatrix(
			FPlane(0,	0,	1,	0),
			FPlane(1,	0,	0,	0),
			FPlane(0,	1,	0,	0),
			FPlane(0,	0,	0,	1));

		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.ViewFamily = ViewFamily;
		ViewInitOptions.SetViewRectangle(ViewRect);
		ViewInitOptions.ViewMatrix = ViewMatrix;
		ViewInitOptions.ProjectionMatrix = ProjectionMatrix;
		ViewInitOptions.BackgroundColor = FLinearColor::Black;

		FSceneView* NewView = new FSceneView(ViewInitOptions);

		ViewFamily->Views.Add(NewView);

		NewView->StartFinalPostprocessSettings( ViewMatrix.GetOrigin() );
		NewView->EndFinalPostprocessSettings();

		FFinalPostProcessSettings::FCubemapEntry& CubemapEntry = *new(NewView->FinalPostProcessSettings.ContributingCubemaps) FFinalPostProcessSettings::FCubemapEntry;
		CubemapEntry.AmbientCubemap = GUnrealEd->GetThumbnailManager()->AmbientCubemap;
		const float AmbientCubemapIntensity = 1.69;
		CubemapEntry.AmbientCubemapTintMulScaleValue = FLinearColor::White * AmbientCubemapIntensity;

		// Tell the texture streaming system about this thumbnail view, so the textures will stream in as needed
		// NOTE: Sizes may not actually be in screen space depending on how the thumbnail ends up stretched by the UI.  Not a big deal though.
		// NOTE: Textures still take a little time to stream if the view has not been re-rendered recently, so they may briefly appear blurry while mips are prepared
		// NOTE: Content Browser only renders thumbnails for loaded assets, and only when the mouse is over the panel. They'll be frozen in their last state while the mouse cursor is not over the panel.  This is for performance reasons
		IStreamingManager::Get().AddViewInformation( Origin, SizeX, SizeX / FMath::Tan( FOVDegrees ) );
	}
}

float FThumbnailPreviewScene::GetBoundsZOffset(const FBoxSphereBounds& Bounds) const
{
	// Return half the height of the bounds plus one to avoid ZFighting with the floor plane
	return Bounds.BoxExtent.Z + 1;
}

/*
***************************************************************
  FParticleSystemThumbnailScene
***************************************************************
*/

FParticleSystemThumbnailScene::FParticleSystemThumbnailScene()
	: FThumbnailPreviewScene()
{
	bForceAllUsedMipsResident = false;
	PartComponent = NULL;

	ThumbnailFXSystem = FFXSystemInterface::Create(GRHIFeatureLevel);
	GetScene()->SetFXSystem( ThumbnailFXSystem );
}

FParticleSystemThumbnailScene::~FParticleSystemThumbnailScene()
{
	FFXSystemInterface::Destroy( ThumbnailFXSystem );
}

void FParticleSystemThumbnailScene::SetParticleSystem(UParticleSystem* ParticleSystem)
{
	bool bNewComponent = false;

	// If no preview component currently existing - create it now and warm it up.
	if ( ParticleSystem && !ParticleSystem->PreviewComponent)
	{
		ParticleSystem->PreviewComponent = ConstructObject<UParticleSystemComponent>(UParticleSystemComponent::StaticClass());
		ParticleSystem->PreviewComponent->Template = ParticleSystem;

		ParticleSystem->PreviewComponent->ComponentToWorld.SetIdentity();

		bNewComponent = true;
	}

	if ( ParticleSystem == NULL || PartComponent != ParticleSystem->PreviewComponent )
	{
		if ( PartComponent != NULL )
		{
			PartComponent->ResetParticles(/*bEmptyInstances=*/ true);
			FPreviewScene::RemoveComponent(PartComponent);
		}

		if ( ParticleSystem != NULL )
		{
			PartComponent = ParticleSystem->PreviewComponent;
			check(PartComponent);

			// Add Particle component to this scene.
			FPreviewScene::AddComponent(PartComponent,FTransform::Identity);

			PartComponent->InitializeSystem();
			PartComponent->ActivateSystem();

			// If its new - tick it so its at the warmup time.
			//		if (bNewComponent && (PartComponent->WarmupTime == 0.0f))
			if (PartComponent->WarmupTime == 0.0f)
			{
				ParticleSystem->PreviewComponent->ResetBurstLists();

				float WarmupElapsed = 0.f;
				float WarmupTimestep = 0.02f;
				while(WarmupElapsed < ParticleSystem->ThumbnailWarmup)
				{
					ParticleSystem->PreviewComponent->TickComponent(WarmupTimestep, LEVELTICK_All, NULL);
					WarmupElapsed += WarmupTimestep;
					ThumbnailFXSystem->Tick(WarmupTimestep);
				}
			}
		}
	}
}

void FParticleSystemThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	check(PartComponent);
	check(PartComponent->Template);

	UParticleSystem* ParticleSystem = PartComponent->Template;

	OutOrigin = FVector::ZeroVector;
	OutOrbitPitch = -11.25f;
	OutOrbitYaw = -157.5f;
	OutOrbitZoom = ParticleSystem->ThumbnailDistance;
}


/*
***************************************************************
  FMaterialThumbnailScene
***************************************************************
*/

FMaterialThumbnailScene::FMaterialThumbnailScene()
	: FThumbnailPreviewScene()
{
	bForceAllUsedMipsResident = false;

	// Create preview actor
	// checked
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.bNoCollisionFail = true;
	SpawnInfo.bNoFail = true;
	SpawnInfo.ObjectFlags = RF_Transient;
	PreviewActor = GetWorld()->SpawnActor<AStaticMeshActor>( SpawnInfo );

	PreviewActor->StaticMeshComponent->SetMobility(EComponentMobility::Movable);
	PreviewActor->SetActorEnableCollision(false);
}

void FMaterialThumbnailScene::SetMaterialInterface(UMaterialInterface* InMaterial)
{
	check(PreviewActor);
	check(PreviewActor->StaticMeshComponent);

	if ( InMaterial )
	{
		// Transform the preview mesh as necessary
		FTransform Transform = FTransform::Identity;

		const USceneThumbnailInfoWithPrimitive* ThumbnailInfo = Cast<USceneThumbnailInfoWithPrimitive>(InMaterial->ThumbnailInfo);
		if ( !ThumbnailInfo )
		{
			ThumbnailInfo = USceneThumbnailInfoWithPrimitive::StaticClass()->GetDefaultObject<USceneThumbnailInfoWithPrimitive>();
		}

		switch( ThumbnailInfo->PrimitiveType )
		{
		case TPT_None:
			{
				bool bFoundCustomMesh = false;
				if ( ThumbnailInfo->PreviewMesh.IsValid() )
				{
					UStaticMesh* MeshToUse = Cast<UStaticMesh>(ThumbnailInfo->PreviewMesh.ResolveObject());
					if ( MeshToUse )
					{
						PreviewActor->StaticMeshComponent->SetStaticMesh(MeshToUse);
						bFoundCustomMesh = true;
					}
				}
				
				if ( !bFoundCustomMesh )
				{
					// Just use a plane if the mesh was not found
					Transform.SetRotation(FQuat(FRotator(0, -90, 0)));
					PreviewActor->StaticMeshComponent->SetStaticMesh(GUnrealEd->GetThumbnailManager()->EditorPlane);
				}
			}
			break;

		case TPT_Cube:
			PreviewActor->StaticMeshComponent->SetStaticMesh(GUnrealEd->GetThumbnailManager()->EditorCube);
			break;

		case TPT_Sphere:
			// The sphere is a little big, scale it down to 256x256x256
			Transform.SetScale3D( FVector(0.8f) );
			PreviewActor->StaticMeshComponent->SetStaticMesh(GUnrealEd->GetThumbnailManager()->EditorSphere);
			break;

		case TPT_Cylinder:
			PreviewActor->StaticMeshComponent->SetStaticMesh(GUnrealEd->GetThumbnailManager()->EditorCylinder);
			break;

		case TPT_Plane:
			// The plane needs to be rotated 90 degrees to face the camera
			Transform.SetRotation(FQuat(FRotator(0, -90, 0)));
			PreviewActor->StaticMeshComponent->SetStaticMesh(GUnrealEd->GetThumbnailManager()->EditorPlane);
			break;

		default:
			check(0);
		}

		PreviewActor->StaticMeshComponent->SetRelativeTransform(Transform);
		PreviewActor->StaticMeshComponent->UpdateBounds();

		// Center the mesh at the world origin then offset to put it on top of the plane
		const float BoundsZOffset = GetBoundsZOffset(PreviewActor->StaticMeshComponent->Bounds);
		Transform.SetLocation(-PreviewActor->StaticMeshComponent->Bounds.Origin + FVector(0, 0, BoundsZOffset));

		PreviewActor->StaticMeshComponent->SetRelativeTransform(Transform);
	}

	PreviewActor->StaticMeshComponent->SetMaterial(0, InMaterial);
	PreviewActor->StaticMeshComponent->RecreateRenderState_Concurrent();
}

void FMaterialThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	check(PreviewActor);
	check(PreviewActor->StaticMeshComponent);
	check(PreviewActor->StaticMeshComponent->GetMaterial(0));

	// Fit the mesh in the view using the following formula
	// tan(HalfFOV) = Width/TargetCameraDistance
	const float HalfFOVRadians = FMath::DegreesToRadians<float>(InFOVDegrees) * 0.5f;
	// Add extra size to view slightly outside of the bounds to compensate for perspective
	const float BoundsMultiplier = 1.15f;
	const float HalfMeshSize = PreviewActor->StaticMeshComponent->Bounds.SphereRadius * BoundsMultiplier;
	const float BoundsZOffset = GetBoundsZOffset(PreviewActor->StaticMeshComponent->Bounds);
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);

	USceneThumbnailInfo* ThumbnailInfo = Cast<USceneThumbnailInfo>(PreviewActor->StaticMeshComponent->GetMaterial(0)->ThumbnailInfo);
	if ( ThumbnailInfo )
	{
		if ( TargetDistance + ThumbnailInfo->OrbitZoom < 0 )
		{
			ThumbnailInfo->OrbitZoom = -TargetDistance;
		}
	}
	else
	{
		ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();
	}

	OutOrigin = FVector(0, 0, -BoundsZOffset);
	OutOrbitPitch = ThumbnailInfo->OrbitPitch;
	OutOrbitYaw = ThumbnailInfo->OrbitYaw;
	OutOrbitZoom = TargetDistance + ThumbnailInfo->OrbitZoom;
}


/*
***************************************************************
  FSkeletalMeshThumbnailScene
***************************************************************
*/

FSkeletalMeshThumbnailScene::FSkeletalMeshThumbnailScene()
	: FThumbnailPreviewScene()
{
	bForceAllUsedMipsResident = false;
	// Create preview actor
	// checked
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.bNoCollisionFail = true;
	SpawnInfo.bNoFail = true;
	SpawnInfo.ObjectFlags = RF_Transient;
	PreviewActor = GetWorld()->SpawnActor<ASkeletalMeshActor>( SpawnInfo );

	PreviewActor->SetActorEnableCollision(false);
}

void FSkeletalMeshThumbnailScene::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	PreviewActor->SkeletalMeshComponent->Materials.Empty();
	PreviewActor->SkeletalMeshComponent->SetSkeletalMesh(InSkeletalMesh);

	if ( InSkeletalMesh )
	{
		FTransform MeshTransform = FTransform::Identity;

		PreviewActor->SetActorLocation(FVector(0,0,0), false);
		PreviewActor->SkeletalMeshComponent->UpdateBounds();

		// Center the mesh at the world origin then offset to put it on top of the plane
		const float BoundsZOffset = GetBoundsZOffset(PreviewActor->SkeletalMeshComponent->Bounds);
		PreviewActor->SetActorLocation( -PreviewActor->SkeletalMeshComponent->Bounds.Origin + FVector(0, 0, BoundsZOffset), false );
		PreviewActor->SkeletalMeshComponent->RecreateRenderState_Concurrent();
	}
}

void FSkeletalMeshThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	check(PreviewActor->SkeletalMeshComponent);
	check(PreviewActor->SkeletalMeshComponent->SkeletalMesh);

	const float HalfFOVRadians = FMath::DegreesToRadians<float>(InFOVDegrees) * 0.5f;
	// No need to add extra size to view slightly outside of the sphere to compensate for perspective since skeletal meshes already buffer bounds.
	const float HalfMeshSize = PreviewActor->SkeletalMeshComponent->Bounds.SphereRadius; 
	const float BoundsZOffset = GetBoundsZOffset(PreviewActor->SkeletalMeshComponent->Bounds);
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);

	USceneThumbnailInfo* ThumbnailInfo = Cast<USceneThumbnailInfo>(PreviewActor->SkeletalMeshComponent->SkeletalMesh->ThumbnailInfo);
	if ( ThumbnailInfo )
	{
		if ( TargetDistance + ThumbnailInfo->OrbitZoom < 0 )
		{
			ThumbnailInfo->OrbitZoom = -TargetDistance;
		}
	}
	else
	{
		ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();
	}

	OutOrigin = FVector(0, 0, -BoundsZOffset);
	OutOrbitPitch = ThumbnailInfo->OrbitPitch;
	OutOrbitYaw = ThumbnailInfo->OrbitYaw;
	OutOrbitZoom = TargetDistance + ThumbnailInfo->OrbitZoom;
}


/*
***************************************************************
  FStaticMeshThumbnailScene
***************************************************************
*/

FStaticMeshThumbnailScene::FStaticMeshThumbnailScene()
	: FThumbnailPreviewScene()
{
	bForceAllUsedMipsResident = false;

	// Create preview actor
	// checked
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.bNoCollisionFail = true;
	SpawnInfo.bNoFail = true;
	SpawnInfo.ObjectFlags = RF_Transient;
	PreviewActor = GetWorld()->SpawnActor<AStaticMeshActor>( SpawnInfo );

	PreviewActor->StaticMeshComponent->SetMobility(EComponentMobility::Movable);
	PreviewActor->SetActorEnableCollision(false);
}

void FStaticMeshThumbnailScene::SetStaticMesh(UStaticMesh* StaticMesh)
{
	PreviewActor->StaticMeshComponent->SetStaticMesh(StaticMesh);

	if ( StaticMesh )
	{
		FTransform MeshTransform = FTransform::Identity;

		PreviewActor->SetActorLocation(FVector(0,0,0), false);
		PreviewActor->StaticMeshComponent->UpdateBounds();

		// Center the mesh at the world origin then offset to put it on top of the plane
		const float BoundsZOffset = GetBoundsZOffset(PreviewActor->StaticMeshComponent->Bounds);
		PreviewActor->SetActorLocation( -PreviewActor->StaticMeshComponent->Bounds.Origin + FVector(0, 0, BoundsZOffset), false );
		PreviewActor->StaticMeshComponent->RecreateRenderState_Concurrent();
	}
}

void FStaticMeshThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	check(PreviewActor);
	check(PreviewActor->StaticMeshComponent);
	check(PreviewActor->StaticMeshComponent->StaticMesh);

	const float HalfFOVRadians = FMath::DegreesToRadians<float>(InFOVDegrees) * 0.5f;
	// Add extra size to view slightly outside of the sphere to compensate for perspective
	const float HalfMeshSize = PreviewActor->StaticMeshComponent->Bounds.SphereRadius * 1.15;
	const float BoundsZOffset = GetBoundsZOffset(PreviewActor->StaticMeshComponent->Bounds);
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);

	USceneThumbnailInfo* ThumbnailInfo = Cast<USceneThumbnailInfo>(PreviewActor->StaticMeshComponent->StaticMesh->ThumbnailInfo);
	if ( ThumbnailInfo )
	{
		if ( TargetDistance + ThumbnailInfo->OrbitZoom < 0 )
		{
			ThumbnailInfo->OrbitZoom = -TargetDistance;
		}
	}
	else
	{
		ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();
	}

	OutOrigin = FVector(0, 0, -BoundsZOffset);
	OutOrbitPitch = ThumbnailInfo->OrbitPitch;
	OutOrbitYaw = ThumbnailInfo->OrbitYaw;
	OutOrbitZoom = TargetDistance + ThumbnailInfo->OrbitZoom;
}

/*
***************************************************************
  FBlueprintThumbnailScene
***************************************************************
*/

FBlueprintThumbnailScene::FBlueprintThumbnailScene()
	: FThumbnailPreviewScene()
{
	CurrentBlueprint = NULL;

	FCoreDelegates::PreGarbageCollect.AddRaw(this, &FBlueprintThumbnailScene::OnPreGarbageCollect);
}

FBlueprintThumbnailScene::~FBlueprintThumbnailScene()
{
	FCoreDelegates::PreGarbageCollect.RemoveAll(this);
}

void FBlueprintThumbnailScene::SetBlueprint(UBlueprint* Blueprint)
{
	CurrentBlueprint = Blueprint;

	if ( Blueprint )
	{
		VisualizableBlueprintComponents = GetPooledVisualizableComponents(Blueprint);

		for ( auto CompIt = VisualizableBlueprintComponents.CreateConstIterator(); CompIt; ++CompIt )
		{
			UPrimitiveComponent* PrimComp = *CompIt;
			PrimComp->bVisible = true;
			PrimComp->MarkRenderStateDirty();
		}
	}
	else
	{
		for ( auto CompIt = VisualizableBlueprintComponents.CreateConstIterator(); CompIt; ++CompIt )
		{
			UPrimitiveComponent* PrimComp = *CompIt;
			PrimComp->bVisible = false;
			PrimComp->MarkRenderStateDirty();
		}

		VisualizableBlueprintComponents.Empty();
	}

	GetWorld()->SendAllEndOfFrameUpdates();
}

bool FBlueprintThumbnailScene::IsValidComponentForVisualization(UActorComponent* Component) const
{
	UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
	if ( PrimComp && PrimComp->IsVisible() && !PrimComp->bHiddenInGame )
	{
		UParticleSystemComponent* ParicleSystemComp = Cast<UParticleSystemComponent>(PrimComp);

		// @TODO Support particle systems in thumbnails
		return !ParicleSystemComp;
	}

	return false;
}

void FBlueprintThumbnailScene::BlueprintChanged(class UBlueprint* Blueprint)
{
	// We could clear only the components for the specified blueprint, but we clear all components anyway because it is quick to regenerate them.
	ClearComponentsPool();
}

void FBlueprintThumbnailScene::AddReferencedObjects( FReferenceCollector& Collector )
{
	// Clear all components so they are never considered "Referenced"
	ClearComponentsPool();

	FThumbnailPreviewScene::AddReferencedObjects(Collector);
}

void FBlueprintThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	check(CurrentBlueprint);

	const float HalfFOVRadians = FMath::DegreesToRadians<float>(InFOVDegrees) * 0.5f;
	// Add extra size to view slightly outside of the sphere to compensate for perspective
	FBoxSphereBounds Bounds(ForceInitToZero);
	for ( auto CompIt = VisualizableBlueprintComponents.CreateConstIterator(); CompIt; ++CompIt )
	{
		Bounds = Bounds + (*CompIt)->Bounds;
	}

	const float HalfMeshSize = Bounds.SphereRadius * 1.15;
	const float BoundsZOffset = GetBoundsZOffset(Bounds);
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);

	USceneThumbnailInfo* ThumbnailInfo = Cast<USceneThumbnailInfo>(CurrentBlueprint->ThumbnailInfo);
	if ( ThumbnailInfo )
	{
		if ( TargetDistance + ThumbnailInfo->OrbitZoom < 0 )
		{
			ThumbnailInfo->OrbitZoom = -TargetDistance;
		}
	}
	else
	{
		ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();
	}

	OutOrigin = FVector(0, 0, -BoundsZOffset);
	OutOrbitPitch = ThumbnailInfo->OrbitPitch;
	OutOrbitYaw = ThumbnailInfo->OrbitYaw;
	OutOrbitZoom = TargetDistance + ThumbnailInfo->OrbitZoom;
}

UActorComponent* FBlueprintThumbnailScene::CreateComponentInstanceFromTemplate(UActorComponent* ComponentTemplate) const
{
	check(ComponentTemplate);

	UActorComponent* NewComponent = NULL;
	EObjectFlags FlagMask = RF_AllFlags & ~RF_ArchetypeObject;
	if ( GetTransientPackage()->IsA(ComponentTemplate->GetClass()->ClassWithin) )
	{
		NewComponent = Cast<UActorComponent>( StaticDuplicateObject(ComponentTemplate, GetTransientPackage(), TEXT(""), FlagMask ) );

		USceneComponent* NewSceneComp = Cast<USceneComponent>(NewComponent);
		if ( NewSceneComp != NULL )
		{
			NewSceneComp->AttachParent = NULL;
		}
	}
	else
	{
		// We can not instance components that use the within keyword.
		// Make a placeholder scene component instead.
		USceneComponent* NewSceneComp = ConstructObject<USceneComponent>(USceneComponent::StaticClass(), GetTransientPackage());
		NewComponent = NewSceneComp;
		
		// Preserve relative location, rotation, scale, parent, and children if the template was a scene component.
		USceneComponent* SceneCompTemplate = Cast<USceneComponent>(ComponentTemplate);
		if ( SceneCompTemplate != NULL )
		{
			// Preserve relative location, rotation and scale
			// The parent and children are excluded as they will be references to the template components
			// and therefore may erroneously dirty the template components package.
			NewSceneComp->RelativeLocation = SceneCompTemplate->RelativeLocation;
			NewSceneComp->RelativeRotation = SceneCompTemplate->RelativeRotation;
			NewSceneComp->RelativeScale3D = SceneCompTemplate->RelativeScale3D;
			NewSceneComp->AttachChildren.Empty();
			NewSceneComp->AttachParent = NULL;
		}
	}

	return NewComponent;
}

void FBlueprintThumbnailScene::InstanceComponents(USCS_Node* CurrentNode, USceneComponent* ParentComponent, const TMap<UActorComponent*, UActorComponent*>& NativeInstanceMap, TArray<UActorComponent*>& OutComponents)
{
	check(CurrentNode != NULL);

	// Get the instanced actor component for this node. This is either an instance made from the native components, or a new instance we create using the current node's template.
	UActorComponent* CurrentActorComp = NULL;
	UActorComponent* ComponentTemplate = CurrentNode->ComponentTemplate;
	if ( ComponentTemplate != NULL )
	{
		// Try to find the template in the list of native components we processed. If we find it, use the corresponding instance instead of making a new one.
		UActorComponent*const* ExistingNativeComponent = NativeInstanceMap.Find(ComponentTemplate);
		if ( ExistingNativeComponent )
		{
			// This was an existing native component.
			CurrentActorComp = *ExistingNativeComponent;
		}
		else
		{
			// This was not a native component. Make an instance based on this node's template and attach it to the parent.
			CurrentActorComp = CreateComponentInstanceFromTemplate(ComponentTemplate);
			if ( CurrentActorComp )
			{
				OutComponents.Add(CurrentActorComp);
			}

			// Only attach to the parent if we were a scene component. Otherwise, we have no location.
			USceneComponent* NewSceneComp = Cast<USceneComponent>(CurrentActorComp);
			if (NewSceneComp != NULL)
			{
				if ( ParentComponent != NULL )
				{
					// Do the attachment
					NewSceneComp->AttachTo(ParentComponent, CurrentNode->AttachToName);
				}
				else
				{
					// If this is the root component, make sure the transform is Identity. Actors ignore the transform of the root component.
					NewSceneComp->SetRelativeTransform( FTransform::Identity );
				}
			}
		}
	}

	if ( CurrentActorComp != NULL )
	{
		USceneComponent* NewSceneComp = Cast<USceneComponent>(CurrentActorComp);

		// Determine the parent component for our children (it's still our parent if we're a non-scene component)
		USceneComponent* ParentSceneComponentOfChildren = (NewSceneComp != NULL) ? NewSceneComp : ParentComponent;

		// If we made a component, go ahead and process our children
		for (int32 NodeIdx = 0; NodeIdx < CurrentNode->ChildNodes.Num(); NodeIdx++)
		{
			USCS_Node* Node = CurrentNode->ChildNodes[NodeIdx];
			check(Node != NULL);
			InstanceComponents(Node, ParentSceneComponentOfChildren, NativeInstanceMap, OutComponents);
		}
	}
}

TArray<UPrimitiveComponent*> FBlueprintThumbnailScene::GetPooledVisualizableComponents(UBlueprint* Blueprint)
{
	check(Blueprint);

	TArray<UPrimitiveComponent*> VisualizableComponentsListForBlueprint;

	TArray< TWeakObjectPtr<UPrimitiveComponent> >* PooledComponentsPtr = VisualizableComponentsPool.Find(Blueprint);
	if ( PooledComponentsPtr )
	{
		TArray< TWeakObjectPtr<UPrimitiveComponent> >& PooledComponents = *PooledComponentsPtr;
		for ( auto CompIt = PooledComponents.CreateConstIterator(); CompIt; ++CompIt )
		{
			UPrimitiveComponent* Component = (*CompIt).GetEvenIfUnreachable();
			if ( Component )
			{
				VisualizableComponentsListForBlueprint.Add(Component);
			}
		}
	}
	else
	{
		// We need to find the RootComponent in order to property transform the components
		USceneComponent* RootComponent = NULL;
		TArray<UActorComponent*> AllCreatedActorComponents;
		TMap<UActorComponent*, UActorComponent*> NativeInstanceMap;

		if ( Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()) )
		{
			// Instance native components based on the CDO template
			AActor* CDO = Blueprint->GeneratedClass->GetDefaultObject<AActor>();

			TArray<UActorComponent*> Components;
			CDO->GetComponents(Components);

			for ( auto CompIt = Components.CreateConstIterator(); CompIt; ++CompIt )
			{
				UActorComponent* Comp = *CompIt;
				if ( Comp != NULL )
				{
					UActorComponent* InstancedComponent = CreateComponentInstanceFromTemplate(Comp);
					if ( InstancedComponent != NULL )
					{
						NativeInstanceMap.Add(Comp, InstancedComponent);
					}
				}
			}

			// Fix up parent and child attachments to point at the new instances
			for ( auto CompIt = NativeInstanceMap.CreateConstIterator(); CompIt; ++CompIt )
			{
				UActorComponent* ActorComp = CompIt.Value();
				if(ActorComp != NULL)
				{
					AllCreatedActorComponents.Add(ActorComp);

					USceneComponent* SceneComp = Cast<USceneComponent>(ActorComp);
					if(SceneComp != NULL)
					{
						if(SceneComp->AttachParent != NULL)
						{
							SceneComp->AttachParent = Cast<USceneComponent>(NativeInstanceMap.FindRef(SceneComp->AttachParent));
						}
						else if(CompIt.Key() == CDO->GetRootComponent())
						{
							RootComponent = SceneComp;

							// Make sure the root component transform is Identity. Actors ignore the transform of the root component.
							RootComponent->SetRelativeTransform( FTransform::Identity );
						}

						for(auto ChildCompIt = SceneComp->AttachChildren.CreateIterator(); ChildCompIt; ++ChildCompIt)
						{
							*ChildCompIt = Cast<USceneComponent>(NativeInstanceMap.FindRef(*ChildCompIt));
						}
					}
				}
			}
		}

		// Instance user-defined components based on the SCS, and attach to the native RootComponent (if it exists)
		// Do this for all parent blueprint generated classes as well
		{
			UBlueprint* BlueprintToHarvestComponents = Blueprint;
			TSet<UBlueprint*> AllVisitedBlueprints;
			while ( BlueprintToHarvestComponents )
			{
				AllVisitedBlueprints.Add(BlueprintToHarvestComponents);

				if ( BlueprintToHarvestComponents->SimpleConstructionScript )
				{
					const TArray<USCS_Node*>& RootNodes = BlueprintToHarvestComponents->SimpleConstructionScript->GetRootNodes();
					for(int32 NodeIndex = 0; NodeIndex < RootNodes.Num(); ++NodeIndex)
					{
						// For each root node in the SCS tree
						USCS_Node* RootNode = RootNodes[NodeIndex];
						if(RootNode)
						{
							// By default, parent it to the Actor's RootComponent
							USceneComponent* ParentComponent = RootComponent;

							// Check to see if the root node has set an explicit parent
							if(RootNode->ParentComponentOrVariableName != NAME_None)
							{
								USceneComponent* ParentComponentTemplate = RootNode->GetParentComponentTemplate(Blueprint);
								if(ParentComponentTemplate != NULL)
								{
									if(NativeInstanceMap.Contains(ParentComponentTemplate))
									{
										ParentComponent = Cast<USceneComponent>(NativeInstanceMap.FindRef(ParentComponentTemplate));
									}
								}
							}

							InstanceComponents(RootNode, ParentComponent, NativeInstanceMap, AllCreatedActorComponents);
						}
					}
				}

				UClass* ParentClass = BlueprintToHarvestComponents->ParentClass;
				BlueprintToHarvestComponents = NULL;

				// If the parent class was a blueprint generated class, add it's simple construction script components as well
				if ( ParentClass )
				{
					UBlueprint* ParentBlueprint = Cast<UBlueprint>(ParentClass->ClassGeneratedBy);

					// Also make sure we haven't visited the blueprint already. This would only happen if there was a loop of parent classes.
					if ( ParentBlueprint && !AllVisitedBlueprints.Contains(ParentBlueprint) )
					{
						BlueprintToHarvestComponents = ParentBlueprint;
					}
				}
			}
		}

		// Calculate the bounds. Include all visualizable components.
		// Update the transform for all components since they will be used to transform the visualizable ones too.
		FBoxSphereBounds Bounds(ForceInitToZero);
		for ( auto CompIt = AllCreatedActorComponents.CreateConstIterator(); CompIt; ++CompIt )
		{
			USceneComponent* SceneComp = Cast<USceneComponent>(*CompIt);
			if ( SceneComp )
			{
				SceneComp->UpdateComponentToWorld();

				if ( IsValidComponentForVisualization(*CompIt) )
				{
					UPrimitiveComponent* PrimComp = CastChecked<UPrimitiveComponent>(*CompIt);
					Bounds = Bounds + PrimComp->Bounds;
				}
			}
		}

		// Center the mesh at the world origin then offset to put it on top of the plane
		const float BoundsZOffset = GetBoundsZOffset(Bounds);
		FTransform CompTransform(-Bounds.Origin + FVector(0, 0, BoundsZOffset));

		// Add all instanced scene components to the scene.
		// Hide all non-visualizable ones.
		TArray< TWeakObjectPtr<UActorComponent> > WeakAllComponentsList;
		TArray< TWeakObjectPtr<UPrimitiveComponent> > WeakVisualizableComponentsList;
		for ( auto CompIt = AllCreatedActorComponents.CreateConstIterator(); CompIt; ++CompIt )
		{
			WeakAllComponentsList.Add(*CompIt);

			USceneComponent* SceneComp = Cast<USceneComponent>(*CompIt);
			if ( SceneComp != NULL )
			{
				if ( IsValidComponentForVisualization(SceneComp) )
				{
					UPrimitiveComponent* PrimComp = CastChecked<UPrimitiveComponent>(SceneComp);
					WeakVisualizableComponentsList.Add(PrimComp);
					VisualizableComponentsListForBlueprint.Add(PrimComp);
				}
				else
				{
					// If this was a non-visualizable scene component, mark it invisible.
					SceneComp->bVisible = false;
				}

				// Add the component to the scene.
				FPreviewScene::AddComponent(*CompIt, CompTransform);
			}
		}

		// Keep track of all components to reuse them in future render calls.
		// These lists are transient and are rebuilt after garbage collection
		AllComponentsPool.Add( Blueprint, WeakAllComponentsList );
		VisualizableComponentsPool.Add( Blueprint, WeakVisualizableComponentsList );
	}
	
	return VisualizableComponentsListForBlueprint;
}

void FBlueprintThumbnailScene::OnPreGarbageCollect()
{
	// This is a good time to clear the component pool to deal with invalid or stale entries. It will be re-populated as needed.
	ClearComponentsPool();
}

void FBlueprintThumbnailScene::ClearComponentsPool()
{
	for ( auto PoolIt = AllComponentsPool.CreateConstIterator(); PoolIt; ++PoolIt )
	{
		TArray< TWeakObjectPtr<UActorComponent> > WeakComponents = PoolIt.Value();
		for ( auto CompIt = WeakComponents.CreateConstIterator(); CompIt; ++CompIt )
		{
			UActorComponent* ActorComp = (*CompIt).GetEvenIfUnreachable();
			if ( ActorComp )
			{
				FPreviewScene::RemoveComponent(ActorComp);
			}
		}
	}

	AllComponentsPool.Empty();
	VisualizableComponentsPool.Empty();
}
