// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "LevelEditorViewport.h"
#include "LevelViewportClickHandlers.h"
#include "AssetData.h"
#include "ScopedTransaction.h"
#include "ILevelEditor.h"
#include "SnappingUtils.h"
#include "Editor/GeometryMode/Public/GeometryEdMode.h"
#include "Editor/GeometryMode/Public/EditorGeometry.h"
#include "MessageLog.h"
#include "ActorEditorUtils.h"

#define LOCTEXT_NAMESPACE "ClickHandlers"

namespace ClickHandlers
{
	static void PrivateSummonContextMenu( FLevelEditorViewportClient* ViewportClient )
	{
		if( ViewportClient->ParentLevelEditor.IsValid() )
		{
			ViewportClient->ParentLevelEditor.Pin()->SummonLevelViewportContextMenu();
		}
	}	

	/**
 	 * Creates an actor of the specified type, trying first to find an actor factory,
	 * falling back to "ACTOR ADD" exec and SpawnActor if no factory is found.
	 * Does nothing if ActorClass is NULL.
	 */
	static AActor* PrivateAddActor(UClass* ActorClass, const FVector* ActorLocation=NULL, bool bUseSurfaceOrientation=false, bool bTransactional = true)
	{
		if ( ActorClass )
		{
			// use an actor factory if possible
			UActorFactory* ActorFactory = GEditor->FindActorFactoryForActorClass( ActorClass );
			if( ActorFactory )
			{
				return GEditor->UseActorFactory( ActorFactory, FAssetData(), ActorLocation, bUseSurfaceOrientation || ActorFactory->bUseSurfaceOrientation );
			}
			// otherwise use AddActor so that we can return the newly created actor
			else
			{
				// Get cursor origin and direction in world space.
				const FViewportCursorLocation& CursorLocation = GCurrentLevelEditingViewportClient->GetCursorWorldLocationFromMousePos();

				// Determine if the actor is being added onto the backdrop. If so, and it is being added from a perspective viewport,
				// it will be moved in front of the camera.
				FVector Location = FVector::ZeroVector;
				bool bOnBackdrop = false;
				if ( ActorLocation )
				{
					Location = *ActorLocation;
				}
				else
				{
					const FIntPoint& CursorPos = CursorLocation.GetCursorPos();
					bOnBackdrop = ( GCurrentLevelEditingViewportClient->Viewport->GetHitProxy( CursorPos.X, CursorPos.Y ) == NULL );

					AActor* Default = ActorClass->GetDefaultObject<AActor>();
					FVector Collision;
					UCapsuleComponent* CylComp = Cast<UCapsuleComponent>(Default->GetRootComponent());
					if (CylComp)
					{
						Collision = FVector(CylComp->GetScaledCapsuleRadius(), CylComp->GetScaledCapsuleRadius(), CylComp->GetScaledCapsuleHalfHeight());
					}
					else
					{
						float CollisionRadius, CollisionHeight;
						Default->GetComponentsBoundingCylinder(CollisionRadius, CollisionHeight);
						Collision = FVector(CollisionRadius, CollisionRadius, CollisionHeight);
					}

					Location = GEditor->ClickLocation + GEditor->ClickPlane * ( FVector::BoxPushOut( GEditor->ClickPlane, Collision ) + 0.1 );
				}					
				AActor* CreatedActor = GEditor->AddActor( GCurrentLevelEditingViewportClient->GetWorld()->GetCurrentLevel(), ActorClass, Location );

				// If the actor was added to the backdrop in a perspective viewport, move it in front of the camera 
				if ( CreatedActor && !ActorLocation && GCurrentLevelEditingViewportClient->IsPerspective() && bOnBackdrop )
				{
					GEditor->MoveActorInFrontOfCamera( *CreatedActor, CursorLocation.GetOrigin(), CursorLocation.GetDirection() );
				}
				return CreatedActor;
			}
		}
		return NULL;
	}

	/**
	 * This function picks a color from under the mouse in the viewport and adds a light with that color.
	 * This is to make it easy for LDs to add lights that fake radiosity.
	 * @param Viewport	Viewport to pick color from.
	 * @param Click		A class that has information about where and how the user clicked on the viewport.
	 */
	void PickColorAndAddLight(FViewport* Viewport, const FViewportClick &Click)
	{
		// Read pixels from viewport.
		TArray<FColor> OutputBuffer;

		// We need to redraw the viewport before reading pixels otherwise we may be reading back from an old buffer.
		Viewport->Draw();
		Viewport->ReadPixels(OutputBuffer);

		// Sample the color we want.
		const int32 ClickX = Click.GetClickPos().X;
		const int32 ClickY = Click.GetClickPos().Y;
		const int32 PixelIdx = ClickX + ClickY * (int32)Viewport->GetSizeXY().X;

		if(PixelIdx < OutputBuffer.Num())
		{
			const FColor PixelColor = OutputBuffer[PixelIdx];

			AActor* NewActor = PrivateAddActor( APointLight::StaticClass() );

			APointLight* Light = CastChecked<APointLight>(NewActor);
			Light->SetMobility(EComponentMobility::Stationary);
			UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>( Light->LightComponent );

			PointLightComponent->LightColor = PixelColor;
		}
	}

	bool ClickActor(FLevelEditorViewportClient* ViewportClient,AActor* Actor,const FViewportClick& Click,bool bAllowSelectionChange)
	{
		// Find the point on the actor component which was clicked on.
		// Do an accurate trace to avoid legacy pull back by an arbitrary amount.
		// TRACE_Accurate is needed for texel selection to work.
		FHitResult Hit(ForceInit);
		if ( GWorld->LineTraceSingle(Hit, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX, ECC_Pawn, FCollisionQueryParams(true)) )
		{	
			GEditor->ClickLocation = Hit.Location;
			GEditor->ClickPlane = FPlane(Hit.Location,Hit.Normal);
		}

		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );

			return true;
		}
		// Handle selection.
		else if( Click.GetKey() == EKeys::RightMouseButton && !Click.IsControlDown() && !ViewportClient->Viewport->KeyState(EKeys::LeftMouseButton) )
		{
			bool bNeedViewportRefresh = false;
			if( Actor )
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClickingOnActorsContextMenu", "Clicking on Actors (context menu)") );
				UE_LOG(LogEditorViewport, Log,  TEXT("Clicking on Actor (context menu): %s (%s)"), *Actor->GetClass()->GetName(), *Actor->GetActorLabel() );

				GEditor->GetSelectedActors()->Modify();

				if( bAllowSelectionChange )
				{
					// If the actor the user clicked on was already selected, then we won't bother clearing the selection
					if( !Actor->IsSelected() )
					{
						GEditor->SelectNone( false, true );
						bNeedViewportRefresh = true;
					}

					// Select the actor the user clicked on
					GEditor->SelectActor( Actor, true, true );
				}
			}

			if( bNeedViewportRefresh )
			{
				// Redraw the viewport so the user can see which object was right clicked on
				ViewportClient->Viewport->Draw();
				FlushRenderingCommands();
			}

			PrivateSummonContextMenu(ViewportClient);
			return true;
		}
		else if( Click.GetEvent() == IE_DoubleClick && Click.GetKey() == EKeys::LeftMouseButton && !Click.IsControlDown() && !Click.IsShiftDown() )
		{
			if( Actor )
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClickingOnActorsDouble-Click", "Clicking on Actors (double-click)") );
				UE_LOG(LogEditorViewport, Log,  TEXT("Clicking on Actor (double click): %s (%s)"), *Actor->GetClass()->GetName(), *Actor->GetActorLabel());

				GEditor->GetSelectedActors()->Modify();

				if( bAllowSelectionChange )
				{
					// Clear the selection
					GEditor->SelectNone( false, true );

					// Select the actor the user clicked on
					GEditor->SelectActor( Actor, true, true );
				}
			}

			return true;
		}
		else if( Click.GetKey() != EKeys::RightMouseButton )
		{
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
			if ( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::T) && Actor )
			{
				TArray<UActorComponent*> Components;
				Actor->GetComponents(Components);
				SetDebugLightmapSample(&Components, NULL, 0, GEditor->ClickLocation);
			}
			else 
#endif
			if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::L) )
			{
				// If shift is down, we pick a color from under the mouse in the viewport and create a light with that color.
				if(Click.IsControlDown())
				{
					PickColorAndAddLight(ViewportClient->Viewport, Click);
				}
				else
				{
					// Create a point light (they default to stationary)
					PrivateAddActor( APointLight::StaticClass() );
				}

				return true;
			}
			else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::S) )
			{
				// Create a static mesh.
				PrivateAddActor( AStaticMeshActor::StaticClass(), NULL, Click.IsAltDown() );

				return true;
			}
			else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::A) )
			{
				// Create an actor of the selected class.
				UClass* SelectedClass = GEditor->GetSelectedObjects()->GetTop<UClass>();
				if( SelectedClass )
				{
					PrivateAddActor( SelectedClass );
				}

				return true;
			}
			else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::Period) )
			{
				if(Click.IsControlDown())
				{
					// create a pylon
					UClass* PylonClass = GEditor->GetClassFromPairMap( FString(TEXT("Pylon")) );
					PrivateAddActor( PylonClass );
				}

				return true;
			}
			else if ( Actor )
			{
				if( bAllowSelectionChange )
				{
					const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClickingOnActors", "Clicking on Actors") );
					GEditor->GetSelectedActors()->Modify();

					if( Click.IsControlDown() )
					{
						const bool bSelect = !Actor->IsSelected();
						if( bSelect )
						{
							UE_LOG(LogEditorViewport, Log,  TEXT("Clicking on Actor (CTRL LMB): %s (%s)"), *Actor->GetClass()->GetName(), *Actor->GetActorLabel());
						}
						GEditor->SelectActor( Actor, bSelect, true, true );
					}
					else if( Click.IsShiftDown() )
					{
						if( !Actor->IsSelected() )
						{
							const bool bSelect = true;
							GEditor->SelectActor( Actor, bSelect, true, true );
						}
					}
					else
					{
						// check to see how many actors need deselecting first - and warn as appropriate
						int32 NumSelectedActors = GEditor->GetSelectedActors()->Num();
						if( NumSelectedActors >= EditorActorSelectionDefs::MaxActorsToSelectBeforeWarning )
						{
							const FText ConfirmText = FText::Format( NSLOCTEXT( "UnrealEd", "Warning_ManyActorsToSelectOne", "There are {0} selected actors. Selecting this actor will deselect them all. Are you sure?" ), FText::AsNumber(NumSelectedActors) );

							FSuppressableWarningDialog::FSetupInfo Info( ConfirmText, NSLOCTEXT( "UnrealEd", "Warning_ManyActors", "Warning: Many Actors" ), "Warning_ManyActors" );
							Info.ConfirmText = NSLOCTEXT("ModalDialogs", "ManyActorsToSelectOneConfirm", "Continue Selection");
							Info.CancelText = NSLOCTEXT("ModalDialogs", "ManyActorsToSelectOneCancel", "Keep Current Selection");

							FSuppressableWarningDialog ManyActorsWarning( Info );
							if( ManyActorsWarning.ShowModal() == FSuppressableWarningDialog::Cancel )
							{
								return false;
							}
						}

						GEditor->SelectNone( false, true, false );
						UE_LOG(LogEditorViewport, Log,  TEXT("Clicking on Actor (LMB): %s (%s)"), *Actor->GetClass()->GetName(), *Actor->GetActorLabel());
						GEditor->SelectActor( Actor, true, true, true );
					}
				}

				return false;
			}
		}

		return false;
	}

	void ClickBrushVertex(FLevelEditorViewportClient* ViewportClient,ABrush* InBrush,FVector* InVertex,const FViewportClick& Click)
	{
		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );
		}
		else if( Click.GetKey() == EKeys::RightMouseButton )
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClickingOnBrushVertex", "Clicking on Brush Vertex") );
			const FTransform ActorToWorld = InBrush->ActorToWorld();
			GEditor->SetPivot( ActorToWorld.TransformPosition(*InVertex), false, false );

			const FVector World = ActorToWorld.TransformPosition(*InVertex);
			FVector Snapped = World;
			FSnappingUtils::SnapPointToGrid( Snapped, FVector(GEditor->GetGridSize()) );
			const FVector Delta = Snapped - World;
			GEditor->SetPivot( Snapped, false, false );

			if( GEditorModeTools().IsModeActive( FBuiltinEditorModes::EM_Default ))
			{		
				// All selected actors need to move by the delta.
				for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
				{
					AActor* Actor = static_cast<AActor*>( *It );
					checkSlow( Actor->IsA(AActor::StaticClass()) );

					Actor->Modify();

					FVector ActorLocation = Actor->GetActorLocation() + Delta;
					Actor->SetActorLocation(ActorLocation, false);
				}
			}

			ViewportClient->Invalidate( true, true );

			// Update Bsp
			GEditor->RebuildAlteredBSP();
		}
	}

	void ClickStaticMeshVertex(FLevelEditorViewportClient* ViewportClient,AActor* InActor,FVector& InVertex,const FViewportClick& Click)
	{
		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );
		}
		else if( Click.GetKey() == EKeys::RightMouseButton )
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClickingOnStaticMeshVertex", "Clicking on Static Mesh Vertex") );

			FVector Snapped = InVertex;
			FSnappingUtils::SnapPointToGrid( Snapped, FVector(GEditor->GetGridSize()) );
			const FVector Delta = Snapped - InVertex;
			GEditor->SetPivot( Snapped, false, true );

			// All selected actors need to move by the delta.
			for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				Actor->Modify();

				FVector ActorLocation = Actor->GetActorLocation() + Delta;
				Actor->SetActorLocation(ActorLocation, false);
			}

			ViewportClient->Invalidate( true, true );
		}
	}

	bool ClickGeomPoly(FLevelEditorViewportClient* ViewportClient, HGeomPolyProxy* InHitProxy, const FViewportClick& Click)
	{
		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );

			return true;
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && Click.IsControlDown() && Click.IsShiftDown() && !Click.IsAltDown() )
		{
			GEditor->SelectActor( InHitProxy->GeomObject->GetActualBrush(), false, true );
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton )
		{
			// This should only happen in geometry mode
			FEdMode* Mode = GEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Geometry );
			if( Mode )
			{
				if( ( InHitProxy != NULL) && ( InHitProxy->GeomObject != NULL ) && (InHitProxy->GeomObject->PolyPool.IsValidIndex( InHitProxy->PolyIndex ) == true ) )					
				{
					Mode->GetCurrentTool()->StartTrans();

					if( !Click.IsControlDown() )
					{
						Mode->GetCurrentTool()->SelectNone();
					}

					FGeomPoly& gp = InHitProxy->GeomObject->PolyPool[ InHitProxy->PolyIndex ];
					gp.Select( Click.IsControlDown() ? !gp.IsSelected() : true );

					Mode->SelectionChanged();

					Mode->GetCurrentTool()->EndTrans();
					ViewportClient->Invalidate( true, false );
				}
				else
				{
					//something is wrong with the hitproxy relating to this click - create a debug log to help identify what
					if( InHitProxy == NULL) 
					{
						UE_LOG(LogEditorViewport, Warning, TEXT("Invalid hitproxy" ) );
					}
					else if ( InHitProxy->GeomObject == NULL ) 
					{
						UE_LOG(LogEditorViewport, Warning, TEXT("Invalid GeomObject" ) );		
					}
					else
					{
						//try to get the name of the object also
						FString name = TEXT("UNKNOWN");
						if( InHitProxy->GeomObject->GetActualBrush() != NULL )
						{
							name = InHitProxy->GeomObject->GetActualBrush()->GetName();
						}
						UE_LOG(LogEditorViewport, Warning, TEXT("Invalid PolyIndex %d on %s" ) ,InHitProxy->PolyIndex , *name );
					}

				}
			}
		}

		return false;
	}

	/**
	 * Utility method used by ClickGeomEdge and ClickGeomVertex.  Returns true if the projections of
	 * the vectors onto the specified viewport plane are equal within the given tolerance.
	 */
	bool OrthoEqual(ELevelViewportType ViewportType, const FVector& Vec0, const FVector& Vec1, float Tolerance=0.1f)
	{
		bool bResult = false;
		switch( ViewportType )
		{
		case LVT_OrthoXY:	bResult = FMath::Abs(Vec0.X - Vec1.X) < Tolerance && FMath::Abs(Vec0.Y - Vec1.Y) < Tolerance;	break;
		case LVT_OrthoXZ:	bResult = FMath::Abs(Vec0.X - Vec1.X) < Tolerance && FMath::Abs(Vec0.Z - Vec1.Z) < Tolerance;	break;
		case LVT_OrthoYZ:	bResult = FMath::Abs(Vec0.Y - Vec1.Y) < Tolerance && FMath::Abs(Vec0.Z - Vec1.Z) < Tolerance;	break;
		default:			check( 0 );		break;
		}
		return bResult;
	}

	bool ClickGeomEdge(FLevelEditorViewportClient* ViewportClient, HGeomEdgeProxy* InHitProxy, const FViewportClick& Click)
	{
		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );

			return true;
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && Click.IsControlDown() && Click.IsShiftDown() && !Click.IsAltDown() )
		{
			GEditor->SelectActor( InHitProxy->GeomObject->GetActualBrush(), false, true );

			return true;
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton )
		{
			FEdMode* Mode = GEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Geometry );

			if( Mode )
			{ 
				Mode->GetCurrentTool()->StartTrans();

				const bool bControlDown = Click.IsControlDown();
				if( !bControlDown )
				{
					Mode->GetCurrentTool()->SelectNone();
				}

				FGeomEdge& HitEdge = InHitProxy->GeomObject->EdgePool[ InHitProxy->EdgeIndex ];
				HitEdge.Select( bControlDown ? !HitEdge.IsSelected() : true );

				if( ViewportClient->IsOrtho() )
				{
					// Select all edges in the brush that match the projected mid point of the original edge.
					for( int32 EdgeIndex = 0 ; EdgeIndex < InHitProxy->GeomObject->EdgePool.Num() ; ++EdgeIndex )
					{
						if ( EdgeIndex != InHitProxy->EdgeIndex )
						{
							FGeomEdge& GeomEdge = InHitProxy->GeomObject->EdgePool[ EdgeIndex ];
							if ( OrthoEqual( ViewportClient->ViewportType, GeomEdge.GetMid(), HitEdge.GetMid() ) )
							{
								GeomEdge.Select( bControlDown ? !GeomEdge.IsSelected() : true );
							}
						}
					}
				}

				Mode->SelectionChanged();

				Mode->GetCurrentTool()->EndTrans();
				ViewportClient->Invalidate( true, true );
				return true;
			}

			return false;


		}

		return false;
	}

	bool ClickGeomVertex(FLevelEditorViewportClient* ViewportClient,HGeomVertexProxy* InHitProxy,const FViewportClick& Click)
	{
		if( !GEditorModeTools().IsModeActive( FBuiltinEditorModes::EM_Geometry ) )
		{
			return false;
		}

		FEdModeGeometry* Mode = static_cast<FEdModeGeometry*>( GEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Geometry) );

		// Note: The expected behavior is that right clicking on a vertex will snap the vertex that was
		// right-clicked on to the nearest grid point, then move all SELECTED verts by the appropriate
		// delta.  So, we need to handle the right mouse button click BEFORE we change the selection set below.

		if( Click.GetKey() == EKeys::RightMouseButton )
		{
			// sanity checks & defensive coding seem necessary based on Prio 1 TTP #318426
			ensure( NULL != InHitProxy->GeomObject );
			if ( InHitProxy->GeomObject == NULL ) 
			{
				UE_LOG(LogEditorViewport, Warning, TEXT("Invalid GeomObject" ) );	
				return false;
			}
			if (InHitProxy->VertexIndex < 0 || InHitProxy->VertexIndex >= InHitProxy->GeomObject->VertexPool.Num() )
			{
				UE_LOG(LogEditorViewport, Warning, TEXT("Invalid InHitProxy->VertexIndex" ) );		
				return false;
			}

			FModeTool_GeometryModify* Tool = static_cast<FModeTool_GeometryModify*>( Mode->GetCurrentTool() );
			Tool->StartTrans();

			// Compute out far to move to get back on the grid.
			const FVector WorldLoc = InHitProxy->GeomObject->GetActualBrush()->ActorToWorld().TransformPosition( InHitProxy->GeomObject->VertexPool[ InHitProxy->VertexIndex ] );

			FVector SnappedLoc = WorldLoc;
			FSnappingUtils::SnapPointToGrid( SnappedLoc, FVector(GEditor->GetGridSize()) );

			const FVector Delta = SnappedLoc - WorldLoc;
			GEditor->SetPivot( SnappedLoc, false, false );

			for( int32 VertexIndex = 0 ; VertexIndex < InHitProxy->GeomObject->VertexPool.Num() ; ++VertexIndex )
			{
				FGeomVertex& GeomVertex = InHitProxy->GeomObject->VertexPool[VertexIndex];
				if( GeomVertex.IsSelected() )
				{
					GeomVertex += Delta;
				}
			}

			Tool->EndTrans();
			InHitProxy->GeomObject->SendToSource();
			ViewportClient->Invalidate( true, true );

			// HACK: The Bsp update has to occur after SendToSource() updates the vert pool, putting it outside
			// of the mode tool's transaction, therefore, the Bsp update requires a transaction of its own
			{
				FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GeoModeVertexSnap", "Vertex Snap") );

				// Update Bsp
				GEditor->RebuildAlteredBSP();
			}
		}

		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			// Pivot snapping

			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );

			return true;
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && Click.IsControlDown() && Click.IsShiftDown() && !Click.IsAltDown() )
		{
			GEditor->SelectActor( InHitProxy->GeomObject->GetActualBrush(), false, true );
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton )
		{
			Mode->GetCurrentTool()->StartTrans();

			// Disable Ctrl+clicking for selection if selecting with RMB.
			const bool bControlDown = Click.IsControlDown();
			if( !bControlDown )
			{
				Mode->GetCurrentTool()->SelectNone();
			}

			FGeomVertex& HitVertex = InHitProxy->GeomObject->VertexPool[ InHitProxy->VertexIndex ];
			bool bSelect = bControlDown ? !HitVertex.IsSelected() : true;

			HitVertex.Select( bSelect );

			if( ViewportClient->IsOrtho() )
			{
				// Select all vertices that project to the same location.
				for( int32 VertexIndex = 0 ; VertexIndex < InHitProxy->GeomObject->VertexPool.Num() ; ++VertexIndex )
				{
					if ( VertexIndex != InHitProxy->VertexIndex )
					{
						FGeomVertex& GeomVertex = InHitProxy->GeomObject->VertexPool[VertexIndex];
						if ( OrthoEqual( ViewportClient->ViewportType, GeomVertex, HitVertex ) )
						{
							GeomVertex.Select( bSelect );
						}
					}
				}
			}

			Mode->SelectionChanged();

			Mode->GetCurrentTool()->EndTrans();

			ViewportClient->Invalidate( true, true );

			return true;
		}

		return false;
	}


	static FBspSurf GSaveSurf;

	void ClickSurface(FLevelEditorViewportClient* ViewportClient,UModel* Model,int32 iSurf,const FViewportClick& Click)
	{
		// Gizmos can cause BSP surfs to become selected without this check
		if(Click.GetKey() == EKeys::RightMouseButton && Click.IsControlDown())
		{
			return;
		}

		// Remember hit location for actor-adding.
		FBspSurf& Surf			= Model->Surfs[iSurf];
		const FPlane Plane		= Surf.Plane;
		GEditor->ClickLocation	= FMath::LinePlaneIntersection( Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection(), Plane );
		GEditor->ClickPlane		= Plane;

		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && Click.IsShiftDown() && Click.IsControlDown() )
		{

			if( !GetDefault<ULevelEditorViewportSettings>()->bClickBSPSelectsBrush )
			{
				// Add to the actor selection set the brush actor that belongs to this BSP surface.
				// Check Surf.Actor, as it can be NULL after deleting brushes and before rebuilding BSP.
				if( Surf.Actor )
				{
					const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectBrushFromSurface", "Select Brush from Surface") );

					// If the builder brush is selected, first deselect it.
					USelection* SelectedActors = GEditor->GetSelectedActors();
					for ( FSelectionIterator It( *SelectedActors ) ; It ; ++It )
					{
						ABrush* Brush = Cast<ABrush>( *It );
						if ( Brush && FActorEditorUtils::IsABuilderBrush( Brush ) )
						{
							GEditor->SelectActor( Brush, false, false );
							break;
						}
					}

					GEditor->SelectActor( Surf.Actor, true, true );
				}
			}
			else
			{
				// Select or deselect surfaces.
				{
					const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectSurfaces", "Select Surfaces") );
					Model->ModifySurf( iSurf, false );
					Surf.PolyFlags ^= PF_Selected;
				}
				GEditor->NoteSelectionChange();
			}
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && Click.IsShiftDown() )
		{
			FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

			// Apply texture to all selected.
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ApplyMaterialToSelectedSurfaces", "Apply Material to Selected Surfaces") );

			UMaterialInterface* SelectedMaterialInstance = GEditor->GetSelectedObjects()->GetTop<UMaterialInterface>();
			for( int32 i=0; i<Model->Surfs.Num(); i++ )
			{
				if( Model->Surfs[i].PolyFlags & PF_Selected )
				{
					Model->ModifySurf( i, 1 );
					Model->Surfs[i].Material = SelectedMaterialInstance;
					GEditor->polyUpdateMaster( Model, i, 0 );
				}
			}
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::A) )
		{
			// Create an actor of the selected class.
			UClass* SelectedClass = GEditor->GetSelectedObjects()->GetTop<UClass>();
			if( SelectedClass )
			{
				PrivateAddActor( SelectedClass );
			}
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::L) )
		{
			// If shift is down, we pick a color from under the mouse in the viewport and create a light with that color.
			if(Click.IsControlDown())
			{
				PickColorAndAddLight(ViewportClient->Viewport, Click);
			}
			else
			{
				// Create a point light (they default to stationary)
				PrivateAddActor( APointLight::StaticClass() );
			}
		}
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::T) )
		{
			SetDebugLightmapSample(NULL, Model, iSurf, GEditor->ClickLocation);
		}
#endif
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::S) )
		{
			// Create a static mesh.
			PrivateAddActor( AStaticMeshActor::StaticClass(), NULL, Click.IsAltDown() );
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::Period) )
		{
			if(Click.IsControlDown())
			{
				// create a pylon
				UClass* PylonClass = GEditor->GetClassFromPairMap( FString(TEXT("Pylon")) );
				PrivateAddActor( PylonClass );
			}
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::Semicolon) )
		{
			PrivateAddActor( ATargetPoint::StaticClass() );
		}
		else if( Click.IsAltDown() && Click.GetKey() == EKeys::RightMouseButton )
		{
			// Grab the texture.
			GEditor->GetSelectedObjects()->DeselectAll(UMaterialInterface::StaticClass());

			if(Surf.Material)
			{
				GEditor->GetSelectedObjects()->Select(Surf.Material);
			}
			GSaveSurf = Surf;
		}
		else if( Click.IsAltDown() && Click.GetKey() == EKeys::LeftMouseButton)
		{
			FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

			// Apply texture to the one polygon clicked on.
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ApplyMaterialToSurface", "Apply Material to Surface") );
			Model->ModifySurf( iSurf, true );
			Surf.Material = GEditor->GetSelectedObjects()->GetTop<UMaterialInterface>();
			if( Click.IsControlDown() )
			{
				Surf.vTextureU	= GSaveSurf.vTextureU;
				Surf.vTextureV	= GSaveSurf.vTextureV;
				if( Surf.vNormal == GSaveSurf.vNormal )
				{
					UE_LOG(LogEditorViewport, Log, TEXT("WARNING: the texture coordinates were not parallel to the surface.") );
				}
				Surf.PolyFlags	= GSaveSurf.PolyFlags;
				GEditor->polyUpdateMaster( Model, iSurf, 1 );
			}
			else
			{
				GEditor->polyUpdateMaster( Model, iSurf, 0 );
			}
		}
		else if( Click.GetKey() == EKeys::RightMouseButton && !Click.IsControlDown() )
		{
			// Select surface and display context menu
			check( Model );

			bool bNeedViewportRefresh = false;
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectSurfaces", "Select Surfaces") );

				// We only need to unselect surfaces if the surface the user clicked on was not already selected
				if( !( Surf.PolyFlags & PF_Selected ) )
				{
					GEditor->SelectNone( false, true );
					bNeedViewportRefresh = true;
				}

				// Select the surface the user clicked on
				Model->ModifySurf( iSurf, false );
				Surf.PolyFlags |= PF_Selected;

				GEditor->NoteSelectionChange();
			}

			if( bNeedViewportRefresh )
			{
				// Redraw the viewport so the user can see which object was right clicked on
				ViewportClient->Viewport->Draw();
				FlushRenderingCommands();
			}

			PrivateSummonContextMenu(ViewportClient);
		}
		else if( Click.GetEvent() == IE_DoubleClick && Click.GetKey() == EKeys::LeftMouseButton && !Click.IsControlDown() )
		{
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectSurface", "Select Surface") );

				// Clear the selection
				GEditor->SelectNone( false, true );

				// Select the surface
				const uint32 SelectMask = Surf.PolyFlags & PF_Selected;
				Model->ModifySurf( iSurf, false );
				Surf.PolyFlags = ( Surf.PolyFlags & ~PF_Selected ) | ( SelectMask ^ PF_Selected );				
			}
			GEditor->NoteSelectionChange();

			// Display the surface properties window
			GEditor->Exec( ViewportClient->GetWorld(), TEXT("EDCALLBACK SURFPROPS") );
		}
		else
		{	
			bool bDeselectAlreadyHandled = false;
			if( GetDefault<ULevelEditorViewportSettings>()->bClickBSPSelectsBrush )
			{
				// Add to the actor selection set the brush actor that belongs to this BSP surface.
				// Check Surf.Actor, as it can be NULL after deleting brushes and before rebuilding BSP.
				if( Surf.Actor )
				{
					const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectBrushFromSurface", "Select Brush from Surface") );
					if( !Click.IsControlDown() )
					{
						GEditor->SelectNone( false, true );
						bDeselectAlreadyHandled = true;
					}
					// If the builder brush is selected, first deselect it.
					USelection* SelectedActors = GEditor->GetSelectedActors();
					for ( FSelectionIterator It( *SelectedActors ) ; It ; ++It )
					{
						ABrush* Brush = Cast<ABrush>( *It );
						if ( Brush && FActorEditorUtils::IsABuilderBrush( Brush ) )
						{
							GEditor->SelectActor( Brush, false, false );
							break;
						}
					}

					GEditor->SelectActor( Surf.Actor, true, true );
				}
			}
			// Select or deselect surfaces.
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectSurfaces", "Select Surfaces") );

				if( !Click.IsControlDown() && !bDeselectAlreadyHandled)
				{
					GEditor->SelectNone( false, true );
				}
				Model->ModifySurf( iSurf, false );
				Surf.PolyFlags ^= PF_Selected;

				// If there are no surfaces selected now, deselect the actor
				if (!Model->HasSelectedSurfaces())
				{
					GEditor->SelectActor(Surf.Actor, false, true);
				}
			}
			GEditor->NoteSelectionChange();
		}
	}

	void ClickBackdrop(FLevelEditorViewportClient* ViewportClient,const FViewportClick& Click)
	{
		GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
		GEditor->ClickPlane    = FPlane(0,0,0,0);

		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->ClickLocation = Click.GetOrigin() + Click.GetDirection() * HALF_WORLD_MAX;
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::A) )
		{
			// Create an actor of the selected class.
			UClass* SelectedClass = GEditor->GetSelectedObjects()->GetTop<UClass>();
			if( SelectedClass )
			{
				PrivateAddActor( SelectedClass );
			}
		}
#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::T) )
		{
			SetDebugLightmapSample(NULL, NULL, 0, GEditor->ClickLocation);
		}
#endif
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::L) )
		{
			// If shift is down, we pick a color from under the mouse in the viewport and create a light with that color.
			if(Click.IsControlDown())
			{
				PickColorAndAddLight(ViewportClient->Viewport, Click);
			}
			else
			{
				// Create a point light (they default to stationary)
				PrivateAddActor( APointLight::StaticClass() );
			}
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::S) )
		{
			// Create a static mesh.
			PrivateAddActor( AStaticMeshActor::StaticClass() );
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::Period) )
		{
			// Create a pathnode.
			if(Click.IsControlDown())
			{
				// create a pylon
				UClass* PylonClass = GEditor->GetClassFromPairMap( FString(TEXT("Pylon")) );
				PrivateAddActor( PylonClass );
			}
		}
		else if( Click.GetKey() == EKeys::RightMouseButton && !Click.IsControlDown() && !ViewportClient->Viewport->KeyState(EKeys::LeftMouseButton) )
		{
			// NOTE: We intentionally do not deselect selected actors here even though the user right
			//		clicked on an empty background.  This is because LDs often use wireframe modes to
			//		interact with brushes and such, and it's easier to summon the context menu for
			//		these actors when right clicking *anywhere* will not deselect things.

			// Redraw the viewport so the user can see which object was right clicked on
			ViewportClient->Viewport->Draw();
			FlushRenderingCommands();

			PrivateSummonContextMenu(ViewportClient);
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton )
		{
			if( !Click.IsControlDown() )
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClickingBackground", "Clicking Background") );
				UE_LOG(LogEditorViewport, Log,  TEXT("Clicking Background") );
				GEditor->SelectNone( true, true );
			}
		}
	}

	void ClickLevelSocket(FLevelEditorViewportClient* ViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "LevelSocketClicked", "Level Socket Clicked") );

		FMessageLog EditorErrors("EditorErrors");
		EditorErrors.NewPage(LOCTEXT("SocketClickedNewPage", "Socket Clicked"));

		// Attach the selected actors to the socket that was clicked
		HLevelSocketProxy* SocketProxy = static_cast<HLevelSocketProxy*>( HitProxy );
		check( SocketProxy->SceneComponent );
		check( SocketProxy->Actor );

		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			if (AActor* Actor = Cast<AActor>( *It ))
			{
				// Parent actors and handle socket snapping.
				// Will cause editor to refresh viewport.
				FText ReasonText;
				if( GEditor->CanParentActors( SocketProxy->Actor, Actor, &ReasonText ) == false )
				{
					EditorErrors.Error(ReasonText);
				}
				else
				{
					GEditor->ParentActors(SocketProxy->Actor, Actor, SocketProxy->SocketName);
				}
			}
		}

		// Report errors
		EditorErrors.Notify(NSLOCTEXT("ActorAttachmentError", "AttachmentsFailed", "Attachments Failed!"));
	}
}

#undef LOCTEXT_NAMESPACE
