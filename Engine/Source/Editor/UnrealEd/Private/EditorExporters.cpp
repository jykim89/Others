// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorExporters.cpp: Editor exporters.
=============================================================================*/

#include "UnrealEd.h"
#include "SoundDefinitions.h"
#include "Landscape/LandscapeDataAccess.h"
#include "Kismet2/DebuggerCommands.h"
#include "MaterialCompiler.h"
#include "SurfaceIterators.h"
#include "FbxExporter.h"
#include "AudioDerivedData.h"
#include "RawMesh.h"
#include "MaterialExportUtils.h"
#include "ImageUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogEditorExporters, Log, All);

/*------------------------------------------------------------------------------
	UTextBufferExporterTXT implementation.
------------------------------------------------------------------------------*/
UTextBufferExporterTXT::UTextBufferExporterTXT(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UTextBuffer::StaticClass();
	FormatExtension.Add(TEXT("TXT"));
	PreferredFormatIndex = 0;
	FormatDescription.Add(TEXT("Text file"));
	bText = true;
}

bool UTextBufferExporterTXT::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags)
{
	UTextBuffer* TextBuffer = CastChecked<UTextBuffer>( Object );
	FString Str( TextBuffer->GetText() );

	TCHAR* Start = const_cast<TCHAR*>(*Str);
	TCHAR* End   = Start + Str.Len();
	while( Start<End && (Start[0]=='\r' || Start[0]=='\n' || Start[0]==' ') )
		Start++;
	while( End>Start && (End [-1]=='\r' || End [-1]=='\n' || End [-1]==' ') )
		End--;
	*End = 0;

	Ar.Log( Start );

	return 1;
}

/*------------------------------------------------------------------------------
	USoundExporterWAV implementation.
------------------------------------------------------------------------------*/
USoundExporterWAV::USoundExporterWAV(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = USoundWave::StaticClass();
	bText = false;
	FormatDescription.Add(TEXT("Sound"));
	FormatExtension.Add(TEXT("WAV"));

}

bool USoundExporterWAV::SupportsObject(UObject* Object) const
{
	bool bSupportsObject = false;
	if (Super::SupportsObject(Object))
	{
		USoundWave* SoundWave = CastChecked<USoundWave>(Object);
		bSupportsObject = (SoundWave->NumChannels <= 2);
	}
	return bSupportsObject;
}

bool USoundExporterWAV::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
	USoundWave* Sound = CastChecked<USoundWave>( Object );
	void* RawWaveData = Sound->RawData.Lock( LOCK_READ_ONLY );
	Ar.Serialize( RawWaveData, Sound->RawData.GetBulkDataSize() );
	Sound->RawData.Unlock();
	return true;
}

/*------------------------------------------------------------------------------
	USoundExporterOGG implementation.
------------------------------------------------------------------------------*/
USoundExporterOGG::USoundExporterOGG(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = USoundWave::StaticClass();
	bText = false;
	FormatDescription.Add(TEXT("Sound"));

	FormatExtension.Add(TEXT("OGG"));

}

bool USoundExporterOGG::SupportsObject(UObject* Object) const
{
	bool bSupportsObject = false;
	if (Super::SupportsObject(Object))
	{
		USoundWave* SoundWave = CastChecked<USoundWave>(Object);
		bSupportsObject = (SoundWave->GetCompressedData("OGG") != NULL);
	}
	return bSupportsObject;
}


bool USoundExporterOGG::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
	USoundWave* Sound = CastChecked<USoundWave>( Object );

	FByteBulkData* Bulk = Sound->GetCompressedData("OGG");
	if (Bulk)
	{
		Ar.Serialize(Bulk->Lock(LOCK_READ_ONLY), Bulk->GetBulkDataSize());
		Bulk->Unlock();
		return true;
	}

	return false;
}

/*------------------------------------------------------------------------------
	USoundSurroundExporterWAV implementation.
------------------------------------------------------------------------------*/
USoundSurroundExporterWAV::USoundSurroundExporterWAV(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	
	SupportedClass = USoundWave::StaticClass();
	bText = false;
	FormatExtension.Add(TEXT("WAV"));
	FormatDescription.Add(TEXT("Multichannel Sound"));

}

bool USoundSurroundExporterWAV::SupportsObject(UObject* Object) const
{
	bool bSupportsObject = false;
	if (Super::SupportsObject(Object))
	{
		USoundWave* SoundWave = CastChecked<USoundWave>(Object);
		bSupportsObject = (SoundWave->NumChannels > 2);
	}
	return bSupportsObject;
}

int32 USoundSurroundExporterWAV::GetFileCount( void ) const
{
	return( SPEAKER_Count );
}

FString USoundSurroundExporterWAV::GetUniqueFilename( const TCHAR* Filename, int32 FileIndex )
{
	static FString SpeakerLocations[SPEAKER_Count] =
	{
		TEXT( "_fl" ),			// SPEAKER_FrontLeft
		TEXT( "_fr" ),			// SPEAKER_FrontRight
		TEXT( "_fc" ),			// SPEAKER_FrontCenter
		TEXT( "_lf" ),			// SPEAKER_LowFrequency
		TEXT( "_sl" ),			// SPEAKER_SideLeft
		TEXT( "_sr" ),			// SPEAKER_SideRight
		TEXT( "_bl" ),			// SPEAKER_BackLeft
		TEXT( "_br" )			// SPEAKER_BackRight
	};

	FString ReturnName = FPaths::GetBaseFilename( Filename, false ) + SpeakerLocations[FileIndex] + FString( ".WAV" );

	return( ReturnName );
}

bool USoundSurroundExporterWAV::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
	bool bResult = false;

	USoundWave* Sound = CastChecked<USoundWave>( Object );
	if ( Sound->ChannelSizes.Num() > 0 )
	{
		uint8* RawWaveData = ( uint8 * )Sound->RawData.Lock( LOCK_READ_ONLY );

		if( Sound->ChannelSizes[ FileIndex ] )
		{
			Ar.Serialize( RawWaveData + Sound->ChannelOffsets[ FileIndex ], Sound->ChannelSizes[ FileIndex ] );
		}

		Sound->RawData.Unlock();

		bResult = Sound->ChannelSizes[ FileIndex ] != 0;
	}

	return bResult;
}

/*------------------------------------------------------------------------------
	UObjectExporterT3D implementation.
------------------------------------------------------------------------------*/
UObjectExporterT3D::UObjectExporterT3D(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UObject::StaticClass();
	bText = true;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("T3D"));
	FormatExtension.Add(TEXT("COPY"));
	FormatDescription.Add(TEXT("Unreal object text"));
	FormatDescription.Add(TEXT("Unreal object text"));
}

bool UObjectExporterT3D::ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags )
{
	EmitBeginObject(Ar, Object, PortFlags);
		ExportObjectInner( Context, Object, Ar, PortFlags);
	EmitEndObject(Ar);

	return true;
}

/*------------------------------------------------------------------------------
	UPolysExporterT3D implementation.
------------------------------------------------------------------------------*/
UPolysExporterT3D::UPolysExporterT3D(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UPolys::StaticClass();
	bText = true;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("T3D"));
	FormatDescription.Add(TEXT("Unreal poly text"));

}

bool UPolysExporterT3D::ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags )
{
	UPolys* Polys = CastChecked<UPolys>( Object );

	Ar.Logf( TEXT("%sBegin PolyList\r\n"), FCString::Spc(TextIndent) );
	for( int32 i=0; i<Polys->Element.Num(); i++ )
	{
		FPoly* Poly = &Polys->Element[i];
		TCHAR TempStr[MAX_SPRINTF]=TEXT("");

		// Start of polygon plus group/item name if applicable.
		// The default values need to jive FPoly::Init().
		Ar.Logf( TEXT("%s   Begin Polygon"), FCString::Spc(TextIndent) );
		if( Poly->ItemName != NAME_None )
		{
			Ar.Logf( TEXT(" Item=%s"), *Poly->ItemName.ToString() );
		}
		if( Poly->Material )
		{
			Ar.Logf( TEXT(" Texture=%s"), *Poly->Material->GetPathName() );
		}
		if( Poly->PolyFlags != 0 )
		{
			Ar.Logf( TEXT(" Flags=%i"), Poly->PolyFlags );
		}
		if( Poly->iLink != INDEX_NONE )
		{
			Ar.Logf( TEXT(" Link=%i"), Poly->iLink );
		}
		if ( Poly->LightMapScale != 32.0f )
		{
			Ar.Logf( TEXT(" LightMapScale=%f"), Poly->LightMapScale );
		}
		Ar.Logf( TEXT("\r\n") );

		// All coordinates.
		Ar.Logf( TEXT("%s      Origin   %s\r\n"), FCString::Spc(TextIndent), SetFVECTOR(TempStr,&Poly->Base) );
		Ar.Logf( TEXT("%s      Normal   %s\r\n"), FCString::Spc(TextIndent), SetFVECTOR(TempStr,&Poly->Normal) );
		Ar.Logf( TEXT("%s      TextureU %s\r\n"), FCString::Spc(TextIndent), SetFVECTOR(TempStr,&Poly->TextureU) );
		Ar.Logf( TEXT("%s      TextureV %s\r\n"), FCString::Spc(TextIndent), SetFVECTOR(TempStr,&Poly->TextureV) );
		for( int32 j=0; j<Poly->Vertices.Num(); j++ )
		{
			Ar.Logf( TEXT("%s      Vertex   %s\r\n"), FCString::Spc(TextIndent), SetFVECTOR(TempStr,&Poly->Vertices[j]) );
		}
		Ar.Logf( TEXT("%s   End Polygon\r\n"), FCString::Spc(TextIndent) );
	}
	Ar.Logf( TEXT("%sEnd PolyList\r\n"), FCString::Spc(TextIndent) );

	return 1;
}

/*------------------------------------------------------------------------------
	UModelExporterT3D implementation.
------------------------------------------------------------------------------*/
UModelExporterT3D::UModelExporterT3D(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UModel::StaticClass();
	bText = true;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("T3D"));
	FormatDescription.Add(TEXT("Unreal model text"));

}

bool UModelExporterT3D::ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags )
{
	UModel* Model = CastChecked<UModel>( Object );

	Ar.Logf( TEXT("%sBegin Brush Name=%s\r\n"), FCString::Spc(TextIndent), *Model->GetName() );
		UExporter::ExportToOutputDevice( Context, Model->Polys, NULL, Ar, Type, TextIndent+3, PortFlags );
//		ExportObjectInner( Context, Model, Ar, PortFlags | PPF_ExportsNotFullyQualified );
	Ar.Logf( TEXT("%sEnd Brush\r\n"), FCString::Spc(TextIndent) );

	return 1;
}

/*------------------------------------------------------------------------------
	ULevelExporterT3D implementation.
------------------------------------------------------------------------------*/

void ExporterHelper_DumpPackageInners(const FExportObjectInnerContext* Context, UPackage* InPackage, int32 TabCount)
{
	const TArray<UObject*>* Inners = Context->GetObjectInners(InPackage);
	if (Inners)
	{
		for (int32 InnerIndex = 0; InnerIndex < Inners->Num(); InnerIndex++)
		{
			UObject* InnerObj = (*Inners)[InnerIndex];

			FString TabString;
			for (int32 TabOutIndex = 0; TabOutIndex < TabCount; TabOutIndex++)
			{
				TabString += TEXT("\t");
			}

			UE_LOG(LogEditorExporters, Log, TEXT("%s%s : %s (%s)"), *TabString,
				InnerObj ? *InnerObj->GetClass()->GetName()	: TEXT("*NULL*"),
				InnerObj ? *InnerObj->GetName()				: TEXT("*NULL*"),
				InnerObj ? *InnerObj->GetPathName()			: TEXT("*NULL*"));

			UPackage* InnerPackage = Cast<UPackage>(InnerObj);
			if (InnerPackage)
			{
				TabCount++;
				ExporterHelper_DumpPackageInners(Context, InnerPackage, TabCount);
				TabCount--;
			}
		}
	}
}

ULevelExporterT3D::ULevelExporterT3D(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UWorld::StaticClass();
	bText = true;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("T3D"));
	FormatExtension.Add(TEXT("COPY"));
	FormatDescription.Add(TEXT("Unreal world text"));
	FormatDescription.Add(TEXT("Unreal world text"));
}

bool ULevelExporterT3D::ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags )
{
	UWorld* World = CastChecked<UWorld>( Object );
	APhysicsVolume* DefaultPhysicsVolume = World->GetDefaultPhysicsVolume();

	UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

	UPackage* MapPackage = NULL;
	if ((PortFlags & PPF_Copy) == 0)
	{
		// If we are not copying to clipboard, then export objects contained in the map package itself...
		MapPackage = Cast<UPackage>(Object->GetOutermost());
	}

	// this is the top level in the .t3d file
	if (MapPackage)
	{
		Ar.Logf(TEXT("%sBegin Map Name=%s\r\n"), FCString::Spc(TextIndent),  *(MapPackage->GetName()));
	}
	else
	{
		Ar.Logf(TEXT("%sBegin Map\r\n"), FCString::Spc(TextIndent));
	}

	// are we exporting all actors or just selected actors?
	bool bAllActors = FCString::Stricmp(Type,TEXT("COPY"))!=0 && !bSelectedOnly;

	TextIndent += 3;

	ULevel* Level;

	// start a new level section
	if (FCString::Stricmp(Type, TEXT("COPY")) == 0)
	{
		// for copy and paste, we want to select actors in the current level
		Level = World->GetCurrentLevel();

		// if we are copy/pasting, then we don't name the level - we paste into the current level
		Ar.Logf(TEXT("%sBegin Level\r\n"), FCString::Spc(TextIndent));

		// mark that we are doing a clipboard copy
		PortFlags |= PPF_Copy;
	}
	else
	{
		// for export, we only want the persistent level
		Level = World->PersistentLevel;

		//@todo seamless if we are exporting only selected, should we export from all levels? or maybe from the current level?

		// if we aren't copy/pasting, then we name the level so that when we import, we get the same level structure
		Ar.Logf(TEXT("%sBegin Level NAME=%s\r\n"), FCString::Spc(TextIndent), *Level->GetName());
	}

	TextIndent += 3;

	// loop through all of the actors just in this level
	for (AActor* Actor : Level->Actors)
	{
		// Don't export the default physics volume, as it doesn't have a UModel associated with it
		// and thus will not import properly.
		if ( Actor == DefaultPhysicsVolume )
		{
			continue;
		}
		// Ensure actor is not a group if grouping is disabled and that the actor is currently selected
		if( Actor && !Actor->IsA(AGroupActor::StaticClass()) &&
			( bAllActors || Actor->IsSelected() ) )
		{
			if (Actor->ShouldExport())
			{
				AActor* ParentActor = Actor->GetAttachParentActor();
				FName SocketName = Actor->GetAttachParentSocketName();
				Actor->DetachRootComponentFromParent(true);

				FString ParentActorString = ( ParentActor ? FString::Printf(TEXT(" ParentActor=%s"), *ParentActor->GetName() ) : TEXT(""));
				FString SocketNameString = ( (ParentActor && SocketName != NAME_None) ? FString::Printf(TEXT(" SocketName=%s"), *SocketName.ToString() ) : TEXT(""));
				FString GroupActor = (Actor->GroupActor? FString::Printf(TEXT(" GroupActor=%s"), *Actor->GroupActor->GetName() ) : TEXT(""));
				Ar.Logf( TEXT("%sBegin Actor Class=%s Name=%s Archetype=%s'%s'%s%s%s") LINE_TERMINATOR, 
					FCString::Spc(TextIndent), *Actor->GetClass()->GetName(), *Actor->GetName(),
					*Actor->GetArchetype()->GetClass()->GetName(), *Actor->GetArchetype()->GetPathName(), *ParentActorString, *SocketNameString, *GroupActor );

				ExportObjectInner( Context, Actor, Ar, PortFlags | PPF_ExportsNotFullyQualified );

				Ar.Logf( TEXT("%sEnd Actor\r\n"), FCString::Spc(TextIndent) );
				Actor->AttachRootComponentToActor(ParentActor, SocketName, EAttachLocation::KeepWorldPosition);
			}
			else
			{
				GEditor->GetSelectedActors()->Deselect( Actor );
			}
		}
	}

	TextIndent -= 3;

	Ar.Logf(TEXT("%sEnd Level\r\n"), FCString::Spc(TextIndent));

	TextIndent -= 3;

	// Export information about the first selected surface in the map.  Used for copying/pasting
	// information from poly to poly.
	Ar.Logf( TEXT("%sBegin Surface\r\n"), FCString::Spc(TextIndent) );
	TCHAR TempStr[256];
	const UModel* Model = World->GetModel();
	for (const FBspSurf& Poly : Model->Surfs)
	{
		if (Poly.PolyFlags & PF_Selected )
		{
			Ar.Logf(TEXT("%sTEXTURE=%s\r\n"),   FCString::Spc(TextIndent + 3), *Poly.Material->GetPathName());
			Ar.Logf(TEXT("%sBASE      %s\r\n"), FCString::Spc(TextIndent + 3), SetFVECTOR(TempStr, &(Model->Points[Poly.pBase])));
			Ar.Logf(TEXT("%sTEXTUREU  %s\r\n"), FCString::Spc(TextIndent + 3), SetFVECTOR(TempStr, &(Model->Vectors[Poly.vTextureU])));
			Ar.Logf(TEXT("%sTEXTUREV  %s\r\n"), FCString::Spc(TextIndent + 3), SetFVECTOR(TempStr, &(Model->Vectors[Poly.vTextureV])));
			Ar.Logf(TEXT("%sNORMAL    %s\r\n"), FCString::Spc(TextIndent + 3), SetFVECTOR(TempStr, &(Model->Vectors[Poly.vNormal])));
			Ar.Logf(TEXT("%sPOLYFLAGS=%d\r\n"), FCString::Spc(TextIndent + 3), Poly.PolyFlags);
			break;
		}
	}
	Ar.Logf( TEXT("%sEnd Surface\r\n"), FCString::Spc(TextIndent) );

	Ar.Logf( TEXT("%sEnd Map\r\n"), FCString::Spc(TextIndent) );


	return 1;
}

void ULevelExporterT3D::ExportComponentExtra(const FExportObjectInnerContext* Context, const TArray<UActorComponent*>& Components, FOutputDevice& Ar, uint32 PortFlags)
{
	for (UActorComponent* ActorComponent : Components)
	{
		if (ActorComponent != NULL)
		{
			AActor* ActorOwner = ActorComponent->GetOwner();
			if (ActorOwner != NULL)
			{
				ULevel* ComponentLevel = Cast<ULevel>(ActorOwner->GetOuter());
				AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(ComponentLevel);
				if (IFA)
				{
					TMap<class UStaticMesh*, TArray<const FFoliageInstancePlacementInfo*>> FoliageInstanceMap = IFA->GetInstancesForComponent(ActorComponent);
					for (const TPair<class UStaticMesh*, TArray<const FFoliageInstancePlacementInfo*>>& MapEntry : FoliageInstanceMap)
					{
						Ar.Logf(TEXT("%sBegin Foliage StaticMesh=%s Component=%s%s"), FCString::Spc(TextIndent), *MapEntry.Key->GetPathName(), *ActorComponent->GetName(), LINE_TERMINATOR);
						for (const FFoliageInstancePlacementInfo* Inst : MapEntry.Value)
						{
							Ar.Logf(TEXT("%sLocation=%f,%f,%f Rotation=%f,%f,%f PreAlignRotation=%f,%f,%f DrawScale3D=%f,%f,%f Flags=%u%s"), FCString::Spc(TextIndent+3),
								Inst->Location.X, Inst->Location.Y, Inst->Location.Z,
								Inst->Rotation.Pitch, Inst->Rotation.Yaw, Inst->Rotation.Roll,
								Inst->PreAlignRotation.Pitch, Inst->PreAlignRotation.Yaw, Inst->PreAlignRotation.Roll,
								Inst->DrawScale3D.X, Inst->DrawScale3D.Y, Inst->DrawScale3D.Z,
								Inst->Flags,
								LINE_TERMINATOR);
						}

						Ar.Logf(TEXT("%sEnd Foliage%s"), FCString::Spc(TextIndent), LINE_TERMINATOR);
					}
				}
			}
		}
	}
}

void ULevelExporterT3D::ExportPackageObject(FExportPackageParams& ExpPackageParams){}
void ULevelExporterT3D::ExportPackageInners(FExportPackageParams& ExpPackageParams){}

/*------------------------------------------------------------------------------
	ULevelExporterSTL implementation.
------------------------------------------------------------------------------*/
ULevelExporterSTL::ULevelExporterSTL(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UWorld::StaticClass();
	bText = true;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("STL"));
	FormatDescription.Add(TEXT("Stereolithography"));
}

bool ULevelExporterSTL::ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags )
{
	//@todo seamless - this needs to be world, like the t3d version above
	UWorld* World = CastChecked<UWorld>(Object);
	ULevel* Level = World->PersistentLevel;

	for( FObjectIterator It; It; ++It )
		It->UnMark(EObjectMark(OBJECTMARK_TagImp | OBJECTMARK_TagExp));

	//
	// GATHER TRIANGLES
	//

	TArray<FVector> Triangles;

	for( int32 iActor=0; iActor<Level->Actors.Num(); iActor++ )
	{
		// Landscape
		ALandscape* Landscape = Cast<ALandscape>(Level->Actors[iActor]);
		if( Landscape && ( !bSelectedOnly || Landscape->IsSelected() ) )
		{
			ULandscapeInfo* LandscapeInfo = Landscape ? Landscape->GetLandscapeInfo() : NULL;
			if( Landscape && LandscapeInfo )
			{
				auto SelectedComponents = LandscapeInfo->GetSelectedComponents();
				
				// Export data for each component
				for( auto It = LandscapeInfo->XYtoComponentMap.CreateIterator(); It; ++It )
				{
					if (bSelectedOnly && SelectedComponents.Num() && !SelectedComponents.Contains(It.Value()))
					{
						continue;
					}
					ULandscapeComponent* Component = It.Value();
					FLandscapeComponentDataInterface CDI(Component);

					for( int32 y=0;y<Component->ComponentSizeQuads;y++ )
					{
						for( int32 x=0;x<Component->ComponentSizeQuads;x++ )
						{
							FVector P00	= CDI.GetWorldVertex(x,y);
							FVector P01	= CDI.GetWorldVertex(x,y+1);
							FVector P11	= CDI.GetWorldVertex(x+1,y+1);
							FVector P10	= CDI.GetWorldVertex(x+1,y);

							// triangulation matches FLandscapeIndexBuffer constructor
							Triangles.Add(P00);
							Triangles.Add(P11);
							Triangles.Add(P10);

							Triangles.Add(P00);
							Triangles.Add(P01);
							Triangles.Add(P11);
						}	
					}
				}		
			}
		}

		// Static meshes

		AStaticMeshActor* Actor = Cast<AStaticMeshActor>(Level->Actors[iActor]);
		if( Actor && ( !bSelectedOnly || Actor->IsSelected() ) && Actor->StaticMeshComponent->StaticMesh && Actor->StaticMeshComponent->StaticMesh->HasValidRenderData() )
		{
			FStaticMeshLODResources& LODModel = Actor->StaticMeshComponent->StaticMesh->RenderData->LODResources[0];
			FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
			int32 NumSections = LODModel.Sections.Num();
			for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
			{
				const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
				for (int32 TriIndex = 0; TriIndex < (int32)Section.NumTriangles; ++TriIndex)
				{
					int32 BaseIndex = Section.FirstIndex + TriIndex * 3;
					for( int32 v = 2 ; v > -1 ; v-- )
					{
						int32 i = Indices[BaseIndex + v];
						FVector vtx = Actor->ActorToWorld().TransformPosition(LODModel.PositionVertexBuffer.VertexPosition(i));
						Triangles.Add(vtx);
					}
				}
			}
		}
	}

	// BSP Surfaces
	for( int32 i=0;i<World->GetModel()->Nodes.Num();i++ )
	{
		FBspNode* Node = &World->GetModel()->Nodes[i];
		if( !bSelectedOnly || World->GetModel()->Surfs[Node->iSurf].PolyFlags&PF_Selected )
		{
			if( Node->NumVertices > 2 )
			{
				FVector vtx1(World->GetModel()->Points[World->GetModel()->Verts[Node->iVertPool+0].pVertex]),
					vtx2(World->GetModel()->Points[World->GetModel()->Verts[Node->iVertPool+1].pVertex]),
					vtx3;

				for( int32 v = 2 ; v < Node->NumVertices ; v++ )
				{
					vtx3 = World->GetModel()->Points[World->GetModel()->Verts[Node->iVertPool+v].pVertex];

					Triangles.Add( vtx1 );
					Triangles.Add( vtx2 );
					Triangles.Add( vtx3 );

					vtx2 = vtx3;
				}
			}
		}
	}

	//
	// WRITE THE FILE
	//

	Ar.Logf( TEXT("%ssolid LevelBSP\r\n"), FCString::Spc(TextIndent) );

	for( int32 tri = 0 ; tri < Triangles.Num() ; tri += 3 )
	{
		FVector vtx[3];
		vtx[0] = Triangles[tri] * FVector(1,-1,1);
		vtx[1] = Triangles[tri+1] * FVector(1,-1,1);
		vtx[2] = Triangles[tri+2] * FVector(1,-1,1);

		FPlane Normal( vtx[0], vtx[1], vtx[2] );

		Ar.Logf( TEXT("%sfacet normal %1.6f %1.6f %1.6f\r\n"), FCString::Spc(TextIndent+2), Normal.X, Normal.Y, Normal.Z );
		Ar.Logf( TEXT("%souter loop\r\n"), FCString::Spc(TextIndent+4) );

		Ar.Logf( TEXT("%svertex %1.6f %1.6f %1.6f\r\n"), FCString::Spc(TextIndent+6), vtx[0].X, vtx[0].Y, vtx[0].Z );
		Ar.Logf( TEXT("%svertex %1.6f %1.6f %1.6f\r\n"), FCString::Spc(TextIndent+6), vtx[1].X, vtx[1].Y, vtx[1].Z );
		Ar.Logf( TEXT("%svertex %1.6f %1.6f %1.6f\r\n"), FCString::Spc(TextIndent+6), vtx[2].X, vtx[2].Y, vtx[2].Z );

		Ar.Logf( TEXT("%sendloop\r\n"), FCString::Spc(TextIndent+4) );
		Ar.Logf( TEXT("%sendfacet\r\n"), FCString::Spc(TextIndent+2) );
	}

	Ar.Logf( TEXT("%sendsolid LevelBSP\r\n"), FCString::Spc(TextIndent) );

	Triangles.Empty();

	return 1;
}


/*------------------------------------------------------------------------------
	Helper classes for render material to texture
	@todo UE4: Replace this with shared Lightmass version when we merge
------------------------------------------------------------------------------*/


/**
* Class for rendering previews of material expressions in the material editor's linked object viewport.
*/
class FExportMaterialProxy : public FMaterial, public FMaterialRenderProxy
{
public:
	FExportMaterialProxy()
	: FMaterial()
	{
		SetQualityLevelProperties(EMaterialQualityLevel::High, false, GRHIFeatureLevel);
	}

	FExportMaterialProxy(UMaterialInterface* InMaterialInterface, EMaterialProperty InPropertyToCompile)
	: FMaterial()
	, MaterialInterface(InMaterialInterface)
	, PropertyToCompile(InPropertyToCompile)
	{
		SetQualityLevelProperties(EMaterialQualityLevel::High, false, GRHIFeatureLevel);
		Material = InMaterialInterface->GetMaterial();
		Material->AppendReferencedTextures(ReferencedTextures);
		FPlatformMisc::CreateGuid(Id);
		CacheShaders(GRHIShaderPlatform, true);
	}

	/** This override is required otherwise the shaders aren't ready for use when the surface is rendered resulting in a blank image */
	virtual bool RequiresSynchronousCompilation() const OVERRIDE { return true; };

	/**
	* Should the shader for this material with the given platform, shader type and vertex 
	* factory type combination be compiled
	*
	* @param Platform		The platform currently being compiled for
	* @param ShaderType	Which shader is being compiled
	* @param VertexFactory	Which vertex factory is being compiled (can be NULL)
	*
	* @return true if the shader should be compiled
	*/
	virtual bool ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
	{
		// Always cache - decreases performance but avoids missing shaders during exports.
		return true;
	}

	virtual const TArray<UTexture*>& GetReferencedTextures() const OVERRIDE
	{
		return ReferencedTextures;
	}

	////////////////
	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterial(ERHIFeatureLevel::Type FeatureLevel) const
	{
		if(GetRenderingThreadShaderMap())
		{
			return this;
		}
		else
		{
			return UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false)->GetMaterial(FeatureLevel);
		}
	}

	virtual bool GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		return MaterialInterface->GetRenderProxy(0)->GetVectorValue(ParameterName, OutValue, Context);
	}

	virtual bool GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const
	{
		return MaterialInterface->GetRenderProxy(0)->GetScalarValue(ParameterName, OutValue, Context);
	}

	virtual bool GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		return MaterialInterface->GetRenderProxy(0)->GetTextureValue(ParameterName,OutValue,Context);
	}

	// Material properties.
	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	virtual int32 CompileProperty(EMaterialProperty Property,EShaderFrequency InShaderFrequency,FMaterialCompiler* Compiler) const
	{
		static const auto UseDiffuseSpecularMaterialInputs = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.UseDiffuseSpecularMaterialInputs"));
				
		Compiler->SetMaterialProperty(Property, InShaderFrequency);
		// MAKE SURE THIS MATCHES THE CHART IN WillFillData
		// 						  RETURNED VALUES (F16 'textures')
		// 	BLEND MODE  | DIFFUSE     | SPECULAR     | EMISSIVE    | NORMAL    | TRANSMISSIVE              |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|
		// 	Opaque      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | 0 (EMPTY)                 |
		// 	Masked      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | Opacity Mask              |
		// 	Translucent | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Additive    | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Modulative  | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | Emsv | Diffuse            |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|
		if( Property == MP_EmissiveColor )
		{
			UMaterial* ProxyMaterial = MaterialInterface->GetMaterial();
			EBlendMode BlendMode = MaterialInterface->GetBlendMode();
			EMaterialLightingModel LightingModel = MaterialInterface->GetLightingModel();
			check(ProxyMaterial);
			switch (PropertyToCompile)
			{
			case MP_EmissiveColor:
				// Emissive is ALWAYS returned...
				return Compiler->ForceCast(MaterialInterface->CompileProperty(Compiler,MP_EmissiveColor),MCT_Float3,true,true);
			case MP_DiffuseColor:
				// Only return for Opaque and Masked...
				if (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
				{
					EMaterialProperty DiffuseInput = UseDiffuseSpecularMaterialInputs->GetValueOnGameThread() == 1 ? MP_DiffuseColor : MP_BaseColor;
					return Compiler->ForceCast(MaterialInterface->CompileProperty(Compiler, DiffuseInput),MCT_Float3,true,true);
				}
				break;
			case MP_SpecularColor: 
				// Only return for Opaque and Masked...
				if (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
				{
					return Compiler->AppendVector(
						Compiler->ForceCast(MaterialInterface->CompileProperty(Compiler, MP_SpecularColor),MCT_Float3,true,true), 
						Compiler->ForceCast(MaterialInterface->CompileProperty(Compiler,MP_Roughness),MCT_Float1));
				}
				break;
			case MP_Normal:
				// Only return for Opaque and Masked...
				if (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
				{
					return Compiler->ForceCast( 
						Compiler->Add( 
							Compiler->Mul(MaterialInterface->CompileProperty(Compiler, MP_Normal), Compiler->Constant(0.5f)), // [-1,1] * 0.5
							Compiler->Constant(0.5f)), // [-0.5,0.5] + 0.5
						MCT_Float3, true, true );
				}
				break;
			
			case MP_Opacity:
				if (BlendMode == BLEND_Masked)
				{
					return MaterialInterface->CompileProperty(Compiler, MP_OpacityMask);
				}
				else if (IsTranslucentBlendMode((EBlendMode)BlendMode) && ProxyMaterial->GetCastShadowAsMasked())
				{
					return MaterialInterface->CompileProperty(Compiler, MP_Opacity);
				}
				else if (BlendMode == BLEND_Modulate)
				{
					if (LightingModel == MLM_Unlit)
					{
						return Compiler->ForceCast(MaterialInterface->CompileProperty(Compiler, MP_EmissiveColor),MCT_Float3,true,true);
					}
					else
					{
						return Compiler->ForceCast(MaterialInterface->CompileProperty(Compiler, MP_DiffuseColor),MCT_Float3,true,true);
					}
				}
				else if ((BlendMode == BLEND_Translucent) || (BlendMode == BLEND_Additive))
				{
					int32 ColoredOpacity = -1;
					if (LightingModel == MLM_Unlit)
					{
						ColoredOpacity = Compiler->ForceCast(MaterialInterface->CompileProperty(Compiler, MP_EmissiveColor),MCT_Float3,true,true);
					}
					else
					{
						ColoredOpacity = Compiler->ForceCast(MaterialInterface->CompileProperty(Compiler, MP_DiffuseColor),MCT_Float3,true,true);
					}
					return Compiler->Lerp(Compiler->Constant3(1.0f, 1.0f, 1.0f), ColoredOpacity, Compiler->ForceCast( MaterialInterface->CompileProperty(Compiler,MP_Opacity), MCT_Float1));
				}
				break;
			default:
				return Compiler->Constant(1.0f);
			}
	
			return Compiler->Constant(0.0f);
		}
		else if( Property == MP_WorldPositionOffset)
		{
			//This property MUST return 0 as a default or during the process of rendering textures out for lightmass to use, pixels will be off by 1.
			return Compiler->Constant(0.0f);
		}
		else if (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7)
		{
			// Pass through customized UVs
			return MaterialInterface->CompileProperty(Compiler, Property);
		}
		else
		{
			return Compiler->Constant(1.0f);
		}
	}

	virtual FString GetMaterialUsageDescription() const { return FString::Printf(TEXT("FExportMaterialRenderer %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL")); }
	virtual int32 GetMaterialDomain() const OVERRIDE
	{
		if (Material)
		{
			return Material->MaterialDomain;
		}
		return MD_Surface;
	}
	virtual bool IsTwoSided() const 
	{ 
		if (MaterialInterface)
		{
			return MaterialInterface->IsTwoSided();
		}
		return false;
	}
	virtual bool IsLightFunction() const
	{
		if (Material)
		{
			return (Material->MaterialDomain == MD_LightFunction);
		}
		return false;
	}
	virtual bool IsUsedWithDeferredDecal() const
	{
		return	Material &&
				Material->MaterialDomain == MD_DeferredDecal;
	}
	virtual bool IsSpecialEngineMaterial() const
	{
		if (Material)
		{
			return (Material->bUsedAsSpecialEngineMaterial == 1);
		}
		return false;
	}
	virtual bool IsWireframe() const
	{
		if (Material)
		{
			return (Material->Wireframe == 1);
		}
		return false;
	}
	virtual bool IsMasked() const									{ return false; }
	virtual enum EBlendMode GetBlendMode() const					{ return BLEND_Opaque; }
	virtual enum EMaterialLightingModel GetLightingModel() const	{ return MLM_Unlit; }
	virtual float GetOpacityMaskClipValue() const					{ return 0.5f; }
	virtual FString GetFriendlyName() const { return FString::Printf(TEXT("FExportMaterialRenderer %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL")); }
	/**
	* Should shaders compiled for this material be saved to disk?
	*/
	virtual bool IsPersistent() const { return false; }
	virtual FGuid GetMaterialId() const OVERRIDE { return Id; }

	const UMaterialInterface* GetMaterialInterface() const
	{
		return MaterialInterface;
	}

	friend FArchive& operator<< ( FArchive& Ar, FExportMaterialProxy& V )
	{
		return Ar << V.MaterialInterface;
	}

	bool IsMaterialInputConnected(UMaterial* InMaterial, EMaterialProperty MaterialInput)
	{
		bool bConnected = false;
		static const auto UseDiffuseSpecularMaterialInputs = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.UseDiffuseSpecularMaterialInputs"));

		switch (MaterialInput)
		{
		case MP_EmissiveColor:
			bConnected = InMaterial->EmissiveColor.Expression != NULL;
			break;
		case MP_DiffuseColor:
			bConnected = (UseDiffuseSpecularMaterialInputs->GetValueOnGameThread() == 1 ? InMaterial->DiffuseColor.Expression : InMaterial->BaseColor.Expression) != NULL;
			break;
		case MP_SpecularColor:
			bConnected = InMaterial->SpecularColor.Expression != NULL;
			break;
		case MP_Normal:
			bConnected = InMaterial->Normal.Expression != NULL;
			break;
		case MP_Opacity:
			bConnected = InMaterial->Opacity.Expression != NULL;
			break;
		case MP_OpacityMask:
			bConnected = InMaterial->OpacityMask.Expression != NULL;
			break;
		default:
			break;
		}

		// Note: only checking to see whether the entire material attributes connection exists.  
		// This means materials using the material attributes input will export more attributes than is necessary.
		bConnected = InMaterial->bUseMaterialAttributes ? InMaterial->MaterialAttributes.Expression != NULL : bConnected;
		return bConnected;
	}

	/**
	 *	Checks if the configuration of the material proxy will generate a uniform
	 *	value across the sampling... (Ie, nothing is hooked to the property)
	 *
	 *	@param	OutUniformValue		The value that will be returned.
	 *
	 *	@return	bool				true if a single value would be generated.
	 *								false if not.
	 */
	bool WillGenerateUniformData(FColor& OutUniformValue)
	{
		// Pre-fill the value...
		OutUniformValue.R = 0;
		OutUniformValue.G = 0;
		OutUniformValue.B = 0;
		OutUniformValue.A = 0;

		EBlendMode BlendMode = MaterialInterface->GetBlendMode();
		EMaterialLightingModel LightingModel = MaterialInterface->GetLightingModel();
		
		check(Material);
		bool bExpressionIsNULL = false;
		switch (PropertyToCompile)
		{
		case MP_EmissiveColor:
			// Emissive is ALWAYS returned...
			bExpressionIsNULL = !IsMaterialInputConnected(Material, PropertyToCompile);
			break;
		case MP_DiffuseColor:
			// Only return for Opaque and Masked...
			if (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
			{
				bExpressionIsNULL = !IsMaterialInputConnected(Material, PropertyToCompile);
			}
			break;
		case MP_SpecularColor: 
			// Only return for Opaque and Masked...
			if (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
			{
				bExpressionIsNULL = !IsMaterialInputConnected(Material, PropertyToCompile);
				OutUniformValue.A = 255;
			}
			break;
		case MP_Normal:
			// Only return for Opaque and Masked...
			if (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
			{
				bExpressionIsNULL = !IsMaterialInputConnected(Material, PropertyToCompile);
				OutUniformValue.B = 255;	// Default normal is (0,0,1)
			}
			break;
		case MP_Opacity:
			if (BlendMode == BLEND_Masked)
			{
				bExpressionIsNULL = !IsMaterialInputConnected(Material, MP_OpacityMask);
				OutUniformValue.R = 255;
				OutUniformValue.G = 255;
				OutUniformValue.B = 255;
				OutUniformValue.A = 255;
			}
			else
			if ((BlendMode == BLEND_Modulate) ||
				(BlendMode == BLEND_Translucent) || 
				(BlendMode == BLEND_Additive))
			{
				bool bColorInputIsNULL = false;
				if (LightingModel == MLM_Unlit)
				{
					bColorInputIsNULL = !IsMaterialInputConnected(Material, MP_EmissiveColor);
				}
				else
				{
					bColorInputIsNULL = !IsMaterialInputConnected(Material, MP_DiffuseColor);
				}
				if (BlendMode == BLEND_Translucent
					|| BlendMode == BLEND_Additive)
				{
					bExpressionIsNULL = bColorInputIsNULL && !IsMaterialInputConnected(Material, PropertyToCompile);
				}
				else
				{
					bExpressionIsNULL = bColorInputIsNULL;
				}
			}
			break;
		}

		return bExpressionIsNULL;
	}

	/**
	* Iterate through all textures used by the material and return the maximum texture resolution used
	* (ideally this could be made dependent of the material property)
	*
	* @param MaterialInterface The material to scan for texture size
	*
	* @return Size (width and height)
	*/
	FIntPoint FindMaxTextureSize(UMaterialInterface* InMaterialInterface, FIntPoint MinimumSize = FIntPoint(1, 1)) const
	{
		// static lod settings so that we only initialize them once
		static FTextureLODSettings GameTextureLODSettings;
		static bool bAreLODSettingsInitialized = false;
		if (!bAreLODSettingsInitialized)
		{
			// initialize LOD settings with game texture resolutions, since we don't want to use 
			// potentially bloated editor settings
			GameTextureLODSettings.Initialize(GEngineIni, TEXT("SystemSettings"));
		}

		TArray<UTexture*> MaterialTextures;

		InMaterialInterface->GetUsedTextures(MaterialTextures, EMaterialQualityLevel::Num, false);

		// find the largest texture in the list (applying it's LOD bias)
		FIntPoint MaxSize = MinimumSize;
		for (int32 TexIndex = 0; TexIndex < MaterialTextures.Num(); TexIndex++)
		{
			UTexture* Texture = MaterialTextures[TexIndex];

			if (Texture == NULL)
			{
				continue;
			}

			// get the max size of the texture
			FIntPoint LocalSize(0, 0);
			if (Texture->IsA(UTexture2D::StaticClass()))
			{
				UTexture2D* Tex2D = (UTexture2D*)Texture;
				LocalSize = FIntPoint(Tex2D->GetSizeX(), Tex2D->GetSizeY());
			}
			else if (Texture->IsA(UTextureCube::StaticClass()))
			{
				UTextureCube* TexCube = (UTextureCube*)Texture;
				LocalSize = FIntPoint(TexCube->GetSizeX(), TexCube->GetSizeY());
			}

			int32 LocalBias = GameTextureLODSettings.CalculateLODBias(Texture);

			// bias the texture size based on LOD group
			FIntPoint BiasedLocalSize(LocalSize.X >> LocalBias, LocalSize.Y >> LocalBias);

			MaxSize.X = FMath::Max(BiasedLocalSize.X, MaxSize.X);
			MaxSize.Y = FMath::Max(BiasedLocalSize.Y, MaxSize.Y);
		}

		return MaxSize;
	}

	static bool WillFillData(EBlendMode InBlendMode, EMaterialProperty InMaterialProperty)
	{
		// MAKE SURE THIS MATCHES THE CHART IN CompileProperty
		// 						  RETURNED VALUES (F16 'textures')
		// 	BLEND MODE  | DIFFUSE     | SPECULAR     | EMISSIVE    | NORMAL    | TRANSMISSIVE              |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|
		// 	Opaque      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | 0 (EMPTY)                 |
		// 	Masked      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | Opacity Mask              |
		// 	Translucent | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Additive    | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Modulative  | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | Emsv | Diffuse            |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|

		if (InMaterialProperty == MP_EmissiveColor)
		{
			return true;
		}

		switch (InBlendMode)
		{
		case BLEND_Opaque:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return true;
				case MP_SpecularColor:	return true;
				case MP_Normal:			return true;
				case MP_Opacity:		return false;
				}
			}
			break;
		case BLEND_Masked:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return true;
				case MP_SpecularColor:	return true;
				case MP_Normal:			return true;
				case MP_Opacity:		return true;
				}
			}
			break;
		case BLEND_Translucent:
		case BLEND_Additive:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return false;
				case MP_SpecularColor:	return false;
				case MP_Normal:			return false;
				case MP_Opacity:		return true;
				}
			}
			break;
		case BLEND_Modulate:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return false;
				case MP_SpecularColor:	return false;
				case MP_Normal:			return false;
				case MP_Opacity:		return true;
				}
			}
			break;
		}
		return false;
	}

private:
	/** The material interface for this proxy */
	UMaterialInterface* MaterialInterface;
	UMaterial* Material;	
	TArray<UTexture*> ReferencedTextures;
	/** The property to compile for rendering the sample */
	EMaterialProperty PropertyToCompile;
	FGuid Id;
};

/**
 * Render a material to a render target
 *
 */
bool GenerateExportMaterialPropertyData(
		UMaterialInterface* InMaterial, EMaterialProperty InMaterialProperty, 
		int32& InOutSizeX, int32& InOutSizeY, 
		TArray<FColor>& OutBMP,
		UTextureRenderTarget2D*& RenderTarget,
		FCanvas*& Canvas)
{
	FExportMaterialProxy* MaterialProxy = new FExportMaterialProxy(InMaterial, InMaterialProperty);
	if (MaterialProxy == NULL)
	{
		UE_LOG(LogEditorExporters, Warning, TEXT("Failed to create FExportMaterialProxy!"));
		return false;
	}

	bool bNormalmap = (InMaterialProperty == MP_Normal);

	FReadSurfaceDataFlags ReadPixelFlags(bNormalmap ? RCM_SNorm : RCM_UNorm);

	bool bResult = true;

	FColor UniformValue;
	if (MaterialProxy->WillGenerateUniformData(UniformValue) == false)
	{
		//@todo UE4. The format may be determined by the material property...
		// For example, if Diffuse doesn't need to be F16 it can create a standard RGBA8 target.
		EPixelFormat Format = PF_FloatRGB;

		FIntPoint MaxTextureSize = MaterialProxy->FindMaxTextureSize(InMaterial);
		InOutSizeX = MaxTextureSize.X;
		InOutSizeY = MaxTextureSize.Y;

		bool bForceLinear = bNormalmap;

		if(RenderTarget && 
			((RenderTarget->bForceLinearGamma != bForceLinear) || (RenderTarget->OverrideFormat != Format) || (RenderTarget->SizeX != InOutSizeX) || (RenderTarget->SizeY != InOutSizeY))
			)
		{
			RenderTarget->RemoveFromRoot();
			RenderTarget = NULL;
			delete Canvas;
			Canvas = NULL;
		}

		if(!RenderTarget)
		{
			RenderTarget = new UTextureRenderTarget2D(FPostConstructInitializeProperties());
			check(RenderTarget);
			RenderTarget->AddToRoot();
			RenderTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
			RenderTarget->InitCustomFormat(InOutSizeX, InOutSizeY, Format, bForceLinear);

			Canvas = new FCanvas(RenderTarget->GetRenderTargetResource(), NULL, 0, 0, 0);
			check(Canvas);
		}

		// start drawing to the render target
		Canvas->SetRenderTarget(RenderTarget->GetRenderTargetResource());
		// Freeze time while capturing the material's inputs
		Canvas->Clear(FLinearColor(0,0,0,0));
		FCanvasTileItem TileItem( FVector2D( 0.0f, 0.0f ), MaterialProxy, FVector2D( InOutSizeX, InOutSizeY ) );
		TileItem.bFreezeTime = true;
		Canvas->DrawItem( TileItem );
		Canvas->Flush();
		FlushRenderingCommands();
		Canvas->SetRenderTarget(NULL);
		FlushRenderingCommands();

		// if PF_FloatRGB was used as render target format, the gamma conversion during rendering is deactivated
		// if we want it not in linear space we need to convert
		ReadPixelFlags.SetLinearToGamma(Format == PF_FloatRGB && !bForceLinear);

		if(!RenderTarget->GetRenderTargetResource()->ReadPixels(OutBMP, ReadPixelFlags))
		{
			bResult = false;
		}
	}
	else
	{
		// Single value... fill it in.
		InOutSizeX = 1;
		InOutSizeY = 1; 
		OutBMP.Empty(1);
		OutBMP.AddZeroed(1);
		OutBMP[0] = UniformValue;
	}

	return bResult;
}




/*------------------------------------------------------------------------------
	Helper classes for the OBJ exporters.
------------------------------------------------------------------------------*/

// An individual face.

class FOBJFace
{
public:
	// index into FOBJGeom::VertexData (local within FOBJGeom)
	uint32 VertexIndex[3];
	/** List of vertices that make up this face. */

	/** The material that was applied to this face. */
	UMaterialInterface* Material;
};

class FOBJVertex
{
public:
	// position
	FVector Vert;
	// texture coordiante
	FVector2D UV;
	// normal
	FVector Normal;
	//	FLinearColor Colors[3];
};

// A geometric object.  This will show up as a separate object when imported into a modeling program.
class FOBJGeom
{
public:
	/** List of faces that make up this object. */
	TArray<FOBJFace> Faces;

	/** Vertex positions that make up this object. */
	TArray<FOBJVertex> VertexData;

	/** Name used when writing this object to the OBJ file. */
	FString Name;

	// Constructors.
	FORCEINLINE FOBJGeom( const FString& InName )
		:	Name( InName )
	{}
};

inline FString FixupMaterialName(UMaterialInterface* Material)
{
	return Material->GetPathName().Replace(TEXT("."), TEXT("_")).Replace(TEXT(":"), TEXT("_"));
}


/**
 * Adds the given actor's mesh to the GOBJObjects array if possible
 * 
 * @param Actor The actor to export
 * @param Objects The array that contains cached exportable object data
 * @param Materials Optional set of materials to gather all used materials by the objects (currently only StaticMesh materials are supported)
 */
static void AddActorToOBJs(AActor* Actor, TArray<FOBJGeom*>& Objects, TSet<UMaterialInterface*>* Materials, bool bSelectedOnly)
{
	FMatrix LocalToWorld = Actor->ActorToWorld().ToMatrixWithScale();

	// Landscape
	ALandscape* Landscape = Cast<ALandscape>( Actor );
	ULandscapeInfo* LandscapeInfo = Landscape ? Landscape->GetLandscapeInfo() : NULL;
	if( Landscape && LandscapeInfo )
	{
		auto SelectedComponents =  LandscapeInfo->GetSelectedComponents();

		// Export data for each component
		for( auto It = Landscape->GetLandscapeInfo()->XYtoComponentMap.CreateIterator(); It; ++It )
		{
			if (bSelectedOnly && SelectedComponents.Num() && !SelectedComponents.Contains(It.Value()))
			{
				continue;
			}
			ULandscapeComponent* Component = It.Value();
			FLandscapeComponentDataInterface CDI(Component, Landscape->ExportLOD);
			const int32 ComponentSizeQuads = ((Component->ComponentSizeQuads+1)>>Landscape->ExportLOD)-1;
			const int32 SubsectionSizeQuads = ((Component->SubsectionSizeQuads+1)>>Landscape->ExportLOD)-1;
			const float ScaleFactor = (float)Component->ComponentSizeQuads / (float)ComponentSizeQuads;

			FOBJGeom* OBJGeom = new FOBJGeom( Component->GetName() );
			OBJGeom->VertexData.AddZeroed( FMath::Square(ComponentSizeQuads + 1) );
			OBJGeom->Faces.AddZeroed( FMath::Square(ComponentSizeQuads) * 2 );

			// Check if there are any holes
			TArray<uint8> RawVisData;
			uint8* VisDataMap = NULL;
			int32 TexIndex = INDEX_NONE;
			int32 WeightMapSize = (SubsectionSizeQuads + 1) * Component->NumSubsections;
			int32 ChannelOffsets[4] = {(int32)STRUCT_OFFSET(FColor,R),(int32)STRUCT_OFFSET(FColor,G),(int32)STRUCT_OFFSET(FColor,B),(int32)STRUCT_OFFSET(FColor,A)};

			for( int32 AllocIdx=0;AllocIdx < Component->WeightmapLayerAllocations.Num(); AllocIdx++ )
			{
				FWeightmapLayerAllocationInfo& AllocInfo = Component->WeightmapLayerAllocations[AllocIdx];
				if( AllocInfo.LayerInfo == ALandscapeProxy::DataLayer )
				{
					TexIndex = AllocInfo.WeightmapTextureIndex;
					Component->WeightmapTextures[TexIndex]->Source.GetMipData(RawVisData, 0);
					VisDataMap = RawVisData.GetTypedData() + ChannelOffsets[AllocInfo.WeightmapTextureChannel];
				}
			}

			// Export verts
			FOBJVertex* Vert = OBJGeom->VertexData.GetTypedData();
			for( int32 y=0;y<ComponentSizeQuads+1;y++ )
			{
				for( int32 x=0;x<ComponentSizeQuads+1;x++ )
				{
					FVector WorldPos, WorldTangentX, WorldTangentY, WorldTangentZ;

					CDI.GetWorldPositionTangents( x, y, WorldPos, WorldTangentX, WorldTangentY, WorldTangentZ );

					Vert->Vert = WorldPos;
					Vert->UV = FVector2D(Component->GetSectionBase().X + x * ScaleFactor, Component->GetSectionBase().Y + y * ScaleFactor);
					Vert->Normal = WorldTangentZ;
					Vert++;
				}
			}

			int32 VisThreshold = 170;
			int32 SubNumX, SubNumY, SubX, SubY;

			FOBJFace* Face = OBJGeom->Faces.GetTypedData();
			for( int32 y=0;y<ComponentSizeQuads;y++ )
			{
				for( int32 x=0;x<ComponentSizeQuads;x++ )
				{
					CDI.ComponentXYToSubsectionXY(x, y, SubNumX, SubNumY, SubX, SubY );
					int32 WeightIndex = SubX + SubNumX*(SubsectionSizeQuads + 1) + (SubY+SubNumY*(SubsectionSizeQuads + 1))*WeightMapSize;

					bool bInvisible = VisDataMap && VisDataMap[WeightIndex * sizeof(FColor)] >= VisThreshold;
					// triangulation matches FLandscapeIndexBuffer constructor
					Face->VertexIndex[0] = (x+0) + (y+0) * (ComponentSizeQuads+1);
					Face->VertexIndex[1] = bInvisible ? Face->VertexIndex[0] : (x+1) + (y+1) * (ComponentSizeQuads+1);
					Face->VertexIndex[2] = bInvisible ? Face->VertexIndex[0] : (x+1) + (y+0) * (ComponentSizeQuads+1);
					Face++;

					Face->VertexIndex[0] = (x+0) + (y+0) * (ComponentSizeQuads+1);
					Face->VertexIndex[1] = bInvisible ? Face->VertexIndex[0] : (x+0) + (y+1) * (ComponentSizeQuads+1);
					Face->VertexIndex[2] = bInvisible ? Face->VertexIndex[0] : (x+1) + (y+1) * (ComponentSizeQuads+1);
					Face++;
				}	
			}

			Objects.Add( OBJGeom );
		}
	}


	// Static mesh components

	UStaticMeshComponent* StaticMeshComponent = NULL;
	UStaticMesh* StaticMesh = NULL;

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	Actor->GetComponents(StaticMeshComponents);

	for( int32 j=0; j<StaticMeshComponents.Num(); j++ )
	{
		// If its a static mesh component, with a static mesh

		StaticMeshComponent = StaticMeshComponents[j];
		if( StaticMeshComponent->IsRegistered() && StaticMeshComponent->StaticMesh
			&& StaticMeshComponent->StaticMesh->HasValidRenderData() )
		{
			LocalToWorld = StaticMeshComponent->ComponentToWorld.ToMatrixWithScale();
			StaticMesh = StaticMeshComponent->StaticMesh;
			break;
		}
	}

	if( StaticMeshComponent && StaticMesh )
	{
		// make room for the faces
		FOBJGeom* OBJGeom = new FOBJGeom( Actor->GetName() );
		
		FStaticMeshLODResources* RenderData = &StaticMesh->RenderData->LODResources[0];
		FIndexArrayView Indices = RenderData->IndexBuffer.GetArrayView();
		uint32 NumIndices = Indices.Num();

		// 3 indices for each triangle
		check(NumIndices % 3 == 0);
		uint32 TriangleCount = NumIndices / 3;
		OBJGeom->Faces.AddUninitialized(TriangleCount);
		
		uint32 VertexCount = RenderData->PositionVertexBuffer.GetNumVertices();
		OBJGeom->VertexData.AddUninitialized(VertexCount);
		FOBJVertex* VerticesOut = OBJGeom->VertexData.GetTypedData();

		check(VertexCount == RenderData->VertexBuffer.GetNumVertices());

		FMatrix LocalToWorldInverseTranspose = LocalToWorld.Inverse().GetTransposed();
		for(uint32 i = 0; i < VertexCount; i++)
		{
			// Vertices
			VerticesOut[i].Vert = LocalToWorld.TransformPosition( RenderData->PositionVertexBuffer.VertexPosition(i) );
			// UVs from channel 0
			VerticesOut[i].UV = RenderData->VertexBuffer.GetVertexUV(i, 0);
			// Normal
			VerticesOut[i].Normal = LocalToWorldInverseTranspose.TransformVector(RenderData->VertexBuffer.VertexTangentZ(i));
		}

		bool bFlipCullMode = LocalToWorld.RotDeterminant() < 0.0f;

		uint32 CurrentTriangleId = 0;
		for(int32 SectionIndex = 0;SectionIndex < RenderData->Sections.Num(); ++SectionIndex)
		{
			FStaticMeshSection& Section = RenderData->Sections[SectionIndex];
			UMaterialInterface* Material = 0;

			// Get the material for this triangle by first looking at the material overrides array and if that is NULL by looking at the material array in the original static mesh
			if(StaticMeshComponent)
			{
				Material = StaticMeshComponent->GetMaterial(Section.MaterialIndex);
			}

			// cache the set of needed materials if desired
			if(Materials && Material)
			{
				Materials->Add(Material);
			}

			for(uint32 i = 0; i < Section.NumTriangles; i++)
			{
				FOBJFace& OBJFace = OBJGeom->Faces[CurrentTriangleId++];

				uint32 a = Indices[Section.FirstIndex + i * 3 + 0];
				uint32 b = Indices[Section.FirstIndex + i * 3 + 1];
				uint32 c = Indices[Section.FirstIndex + i * 3 + 2];

				if(bFlipCullMode)
				{
					Swap(a, c);
				}

				OBJFace.VertexIndex[0] = a;
				OBJFace.VertexIndex[1] = b; 
				OBJFace.VertexIndex[2] = c;

				// Material
				OBJFace.Material = Material;
			}
		}

		Objects.Add( OBJGeom );	
	}
}


// @param Material must not be 0
// @param MatProp e.g. MP_DiffuseColor
static void ExportMaterialPropertyTexture(
	const FString &BMPFilename, 
	UMaterialInterface* Material, 
	const EMaterialProperty MatProp,
	UTextureRenderTarget2D*& RenderTarget,
	FCanvas*& Canvas
	)
{
	check(Material);

	// make the BMP for the diffuse channel
	TArray<FColor> OutputBMP;
	int32 SizeX = 1024;
	int32 SizeY = 1024;

	TEnumAsByte<EBlendMode> BlendMode = Material->GetBlendMode();
	bool bIsValidMaterial = FExportMaterialProxy::WillFillData((EBlendMode)(BlendMode), MatProp);

	if (bIsValidMaterial)
	{
		// make space for the bmp
		OutputBMP.AddUninitialized(SizeX * SizeY);

		// render the material to a texture to export as a bmp
		if (!GenerateExportMaterialPropertyData(Material, MatProp, SizeX, SizeY, OutputBMP, RenderTarget, Canvas))
		{
			bIsValidMaterial = false;
		}
	}

	// make invalid textures a solid red
	if (!bIsValidMaterial)
	{
		SizeX = SizeY = 1;
		OutputBMP.Empty();
		OutputBMP.Add(FColor(255, 0, 0, 255));
	}

	// export the diffuse channel bmp
	FFileHelper::CreateBitmap(*BMPFilename, SizeX, SizeY, OutputBMP.GetTypedData());
}

/**
 * Exports the GOBJObjects array to the given Archive
 *
 * @param FileAr The main archive to output device. However, if MemAr exists, it will write to that until and flush it out for each object
 * @param MemAr Optional string output device for caching writes to
 * @param Warn Feedback context for updating status
 * @param OBJFilename Name of the main OBJ file to export to, used for tagalong files (.mtl, etc)
 * @param Objects The list of objects to export
 * @param Materials Optional list of materials to export
 */
void ExportOBJs(FOutputDevice& FileAr, FStringOutputDevice* MemAr, FFeedbackContext* Warn, const FString& OBJFilename, TArray<FOBJGeom*>& Objects, const TSet<UMaterialInterface*>* Materials, uint32 &IndexOffset)
{
	// write to the memory archive if it exists, otherwise use the FileAr
	FOutputDevice& Ar = MemAr ? *MemAr : FileAr;

	// export extra material info if we added any
	if (Materials)
	{
		// stop the rendering thread so we can easily render to texture
		SCOPED_SUSPEND_RENDERING_THREAD(true);

		// make a .MTL file next to the .obj file that contains the materials
		FString MaterialLibFilename = FPaths::GetBaseFilename(OBJFilename, false) + TEXT(".mtl");

		// use the output device file, just like the Exporter makes for the .obj, no backup
		FOutputDeviceFile* MaterialLib = new FOutputDeviceFile(*MaterialLibFilename, true);
		MaterialLib->SetSuppressEventTag(true);
		MaterialLib->SetAutoEmitLineTerminator(false);

		UTextureRenderTarget2D* RenderTarget = NULL;
		FCanvas* Canvas = NULL;

		// export the material set to a mtllib
		int32 MaterialIndex = 0;
		for (TSet<UMaterialInterface*>::TConstIterator It(*Materials); It; ++It, ++MaterialIndex)
		{
			UMaterialInterface* Material = *It;
			FString MaterialName = FixupMaterialName(Material);

			// export the material info
			MaterialLib->Logf(TEXT("newmtl %s\r\n"), *MaterialName);

			{
				FString BMPFilename = FPaths::GetPath(MaterialLibFilename) / MaterialName + TEXT("_D.bmp");
				ExportMaterialPropertyTexture(BMPFilename, Material, MP_DiffuseColor, RenderTarget, Canvas);
				MaterialLib->Logf(TEXT("\tmap_Kd %s\r\n"), *FPaths::GetCleanFilename(BMPFilename));
			}

			{
				FString BMPFilename = FPaths::GetPath(MaterialLibFilename) / MaterialName + TEXT("_S.bmp");
				ExportMaterialPropertyTexture(BMPFilename, Material, MP_SpecularColor, RenderTarget, Canvas);
				MaterialLib->Logf(TEXT("\tmap_Ks %s\r\n"), *FPaths::GetCleanFilename(BMPFilename));
			}

			{
				FString BMPFilename = FPaths::GetPath(MaterialLibFilename) / MaterialName + TEXT("_N.bmp");
				ExportMaterialPropertyTexture(BMPFilename, Material, MP_Normal, RenderTarget, Canvas);
				MaterialLib->Logf(TEXT("\tbump %s\r\n"), *FPaths::GetCleanFilename(BMPFilename));
			}

			MaterialLib->Logf(TEXT("\r\n"));
		}
		
		if(RenderTarget)
		{
			RenderTarget->RemoveFromRoot();
			RenderTarget = NULL;
			delete Canvas;
			Canvas = NULL;
		}

		MaterialLib->TearDown();
		delete MaterialLib;

		Ar.Logf(TEXT("mtllib %s\n"), *FPaths::GetCleanFilename(MaterialLibFilename));
	}

	for( int32 o = 0 ; o < Objects.Num() ; ++o )
	{
		FOBJGeom* object = Objects[o];
		UMaterialInterface* CurrentMaterial = NULL;

		// Object header

		Ar.Logf( TEXT("g %s\n"), *object->Name );
		Ar.Logf( TEXT("\n") );

		// Verts

		for( int32 f = 0 ; f < object->VertexData.Num() ; ++f )
		{
			const FOBJVertex& vertex = object->VertexData[f];
			const FVector& vtx = vertex.Vert;

			Ar.Logf( TEXT("v %.4f %.4f %.4f\n"), vtx.X, vtx.Z, vtx.Y );
		}

		Ar.Logf( TEXT("\n") );

		// Texture coordinates

		for( int32 f = 0 ; f < object->VertexData.Num() ; ++f )
		{
			const FOBJVertex& face = object->VertexData[f];
			const FVector2D& uv = face.UV;

			Ar.Logf( TEXT("vt %.4f %.4f\n"), uv.X, 1.0f - uv.Y );
		}

		Ar.Logf( TEXT("\n") );

		// Normals

		for( int32 f = 0 ; f < object->VertexData.Num() ; ++f )
		{
			const FOBJVertex& face = object->VertexData[f];
			const FVector& Normal = face.Normal;

			Ar.Logf( TEXT("vn %.3f %.3f %.3f\n"), Normal.X, Normal.Z, Normal.Y );
		}

		Ar.Logf( TEXT("\n") );
		
		// Faces

		for( int32 f = 0 ; f < object->Faces.Num() ; ++f )
		{
			const FOBJFace& face = object->Faces[f];

			if( face.Material != CurrentMaterial )
			{
				CurrentMaterial = face.Material;
				Ar.Logf( TEXT("usemtl %s\n"), *FixupMaterialName(face.Material));
			}

			Ar.Logf( TEXT("f ") );

			for( int32 v = 0 ; v < 3 ; ++v )
			{
				// +1 as Wavefront files are 1 index based
				uint32 VertexIndex = IndexOffset + face.VertexIndex[v] + 1; 
				Ar.Logf( TEXT("%d/%d/%d "), VertexIndex, VertexIndex, VertexIndex);
			}

			Ar.Logf( TEXT("\n") );
		}

		IndexOffset += object->VertexData.Num();

		Ar.Logf( TEXT("\n") );

		// dump to disk so we don't run out of memory ganging up all objects
		if (MemAr)
		{
			FileAr.Log(*MemAr);
			FileAr.Flush();
			MemAr->Empty();
		}

		// we are now done with the object, free it
		delete object;
		Objects[o] = NULL;
	}
}

/**
 * Compiles the selection order array by putting every geometry object
 * with a valid selection index into the array, and then sorting it.
 */
struct FCompareMaterial
{
	FORCEINLINE bool operator()( const FOBJFace& InA, const FOBJFace& InB ) const
	{
		PTRINT A = (PTRINT)(InA.Material);
		PTRINT B = (PTRINT)(InB.Material);

		return (A < B) ? true : false;
	}
};

/*------------------------------------------------------------------------------
	ULevelExporterLOD implementation.
------------------------------------------------------------------------------*/
ULevelExporterLOD::ULevelExporterLOD(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UWorld::StaticClass();
	bText = true;
	bForceFileOperations = true;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("LOD.OBJ"));
	FormatDescription.Add(TEXT("Object File for LOD"));
}

bool ULevelExporterLOD::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& FileAr, FFeedbackContext* Warn, uint32 PortFlags)
{
	GWarn->BeginSlowTask( NSLOCTEXT("UnrealEd", "ExportingLevelToLOD OBJ", "Exporting Level To LOD OBJ"), true );

	// containers to hold exportable objects and their materials
	TArray<FOBJGeom*> Objects;
	TSet<UMaterialInterface*> Materials;

	UWorld* World = CastChecked<UWorld>(Object);

	// write to memory to buffer file writes
	FStringOutputDevice Ar;

	// OBJ file header
	Ar.Logf (TEXT("# LOD OBJ File Generated by UnrealEd\n"));
	Ar.Logf( TEXT("\n") );

	TArray<AActor*> ActorsToExport;
	for( FActorIterator It(World); It; ++It )
	{
		AActor* Actor = *It;
		// only export selected actors if the flag is set
		if( !Actor || (bSelectedOnly && !Actor->IsSelected()))
		{
			continue;
		}

		ActorsToExport.Add(Actor);
	}

	// Export actors
	uint32 IndexOffset = 0;
	for (int32 Index = 0; Index < ActorsToExport.Num(); ++Index)
	{
		AActor* Actor = ActorsToExport[Index];
		Warn->StatusUpdate( Index, ActorsToExport.Num(), NSLOCTEXT("UnrealEd", "ExportingLevelToOBJ", "Exporting Level To OBJ") );

		// for now, only export static mesh actors
		if (Cast<AStaticMeshActor>(Actor) == NULL)
		{
			continue;
		}

		// export any actor that passes the tests
		AddActorToOBJs(Actor, Objects, &Materials, bSelectedOnly);

		for( int32 o = 0 ; o < Objects.Num() ; ++o )
		{
			FOBJGeom* object = Objects[o];
			object->Faces.Sort(FCompareMaterial());
		}

		// Export to the OBJ file
		ExportOBJs(FileAr, &Ar, Warn, CurrentFilename, Objects, &Materials, IndexOffset);
		Objects.Reset();
	}

	// OBJ file footer
	Ar.Logf (TEXT("# dElaernU yb detareneG eliF JBO DOL\n"));

	GWarn->EndSlowTask();

	// dump the rest to the file
	FileAr.Log(*Ar);

	return true;
}


/*------------------------------------------------------------------------------
	ULevelExporterOBJ implementation.
------------------------------------------------------------------------------*/

static void ExportPolys( UPolys* Polys, int32 &PolyNum, int32 TotalPolys, FFeedbackContext* Warn, bool bSelectedOnly, UModel* Model, TArray<FOBJGeom*>& Objects )
{
	FOBJGeom* OBJGeom = new FOBJGeom( TEXT("BSP") );

	for( int32 i=0; i<Model->Nodes.Num(); i++ )
	{
		FBspNode* Node = &Model->Nodes[i];
		FBspSurf& Surf = Model->Surfs[Node->iSurf];

		if( (Surf.PolyFlags & PF_Selected) || !bSelectedOnly )
		{
			const FVector& TextureBase = Model->Points[Surf.pBase];
			const FVector& TextureX = Model->Vectors[Surf.vTextureU];
			const FVector& TextureY = Model->Vectors[Surf.vTextureV];
			const FVector& Normal = Model->Vectors[Surf.vNormal];

			FPoly Poly;
			GEditor->polyFindMaster( Model, Node->iSurf, Poly );

			// Triangulate this node and generate an OBJ face from the vertices.
			for(int32 StartVertexIndex = 1;StartVertexIndex < Node->NumVertices-1;StartVertexIndex++)
			{
				int32 TriangleIndex = OBJGeom->Faces.AddZeroed();
				FOBJFace& OBJFace = OBJGeom->Faces[TriangleIndex];
				int32 VertexIndex = OBJGeom->VertexData.AddZeroed(3);
				FOBJVertex* Vertices = &OBJGeom->VertexData[VertexIndex];

				OBJFace.VertexIndex[0] = VertexIndex;
				OBJFace.VertexIndex[1] = VertexIndex + 1;
				OBJFace.VertexIndex[2] = VertexIndex + 2;

				// These map the node's vertices to the 3 triangle indices to triangulate the convex polygon.
				int32 TriVertIndices[3] = { Node->iVertPool,
										  Node->iVertPool + StartVertexIndex,
										  Node->iVertPool + StartVertexIndex + 1 };

				for(uint32 TriVertexIndex = 0; TriVertexIndex < 3; TriVertexIndex++)
				{
					const FVert& Vert = Model->Verts[TriVertIndices[TriVertexIndex]];
					const FVector& Vertex = Model->Points[Vert.pVertex];

					float U = ((Vertex - TextureBase) | TextureX) / UModel::GetGlobalBSPTexelScale();
					float V = ((Vertex - TextureBase) | TextureY) / UModel::GetGlobalBSPTexelScale();

					Vertices[TriVertexIndex].Vert = Vertex;
					Vertices[TriVertexIndex].UV = FVector2D( U, V );
					Vertices[TriVertexIndex].Normal = Normal;
				}
			}
		}
	}

	// Save the object representing the BSP into the OBJ pool
	if( OBJGeom->Faces.Num() > 0 )
	{
		Objects.Add( OBJGeom );
	}
	else
	{
		delete OBJGeom;
	}
}

ULevelExporterOBJ::ULevelExporterOBJ(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UWorld::StaticClass();
	bText = true;
	bForceFileOperations = true;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("OBJ"));
	FormatDescription.Add(TEXT("Object File"));
}

bool ULevelExporterOBJ::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& FileAr, FFeedbackContext* Warn, uint32 PortFlags)
{
	TSet<UMaterialInterface*> GlobalMaterials;
	TSet<UMaterialInterface*> *Materials = 0;

	int32 YesNoCancelReply = FMessageDialog::Open( EAppMsgType::YesNoCancel, NSLOCTEXT("UnrealEd", "Prompt_OBJExportWithBMP", "Would you like to export the materials as images (slower)?"));

	switch (YesNoCancelReply)
	{
	case EAppReturnType::Yes:
			Materials = &GlobalMaterials;
			break;

	case EAppReturnType::No:
			break;

	case EAppReturnType::Cancel:
			return 1;
	}


	GWarn->BeginSlowTask( NSLOCTEXT("UnrealEd", "ExportingLevelToOBJ", "Exporting Level To OBJ"), true );

	// container to hold all exportable objects
	TArray<FOBJGeom*> Objects;

	UWorld* World = CastChecked<UWorld>(Object);

	GEditor->bspBuildFPolys(World->GetModel(), 0, 0 );
	UPolys* Polys = World->GetModel()->Polys;

	// write to memory to buffer file writes
	FStringOutputDevice Ar;

	// OBJ file header

	Ar.Logf (TEXT("# OBJ File Generated by UnrealEd\n"));
	Ar.Logf( TEXT("\n") );

	uint32 IndexOffset = 0;
	// Export the BSP

	int32 Dummy;
	ExportPolys( Polys, Dummy, 0, Warn, bSelectedOnly, World->GetModel(), Objects );
	// Export polys to the OBJ file
	ExportOBJs(FileAr, &Ar, Warn, CurrentFilename, Objects, NULL, IndexOffset);
	Objects.Reset();
	// Export actors
	
	TArray<AActor*> ActorsToExport;
	for( FActorIterator It(World); It; ++It )
	{
		AActor* Actor = *It;
		// only export selected actors if the flag is set
		if( !Actor || (bSelectedOnly && !Actor->IsSelected()))
		{
			continue;
		}

		ActorsToExport.Add(Actor);
	}

	for( int32 Index = 0; Index < ActorsToExport.Num(); ++Index )
	{
		AActor* Actor = ActorsToExport[Index];
		Warn->StatusUpdate( Index, ActorsToExport.Num(), NSLOCTEXT("UnrealEd", "ExportingLevelToOBJ", "Exporting Level To OBJ") );

		// try to export every object
		AddActorToOBJs(Actor, Objects, Materials, bSelectedOnly);

		for( int32 o = 0 ; o < Objects.Num() ; ++o )
		{
			FOBJGeom* object = Objects[o];
			object->Faces.Sort(FCompareMaterial());
		}
	}

	// Export to the OBJ file
	ExportOBJs(FileAr, &Ar, Warn, CurrentFilename, Objects, Materials, IndexOffset);
	Objects.Reset();

	// OBJ file footer
	Ar.Logf (TEXT("# dElaernU yb detareneG eliF JBO\n"));

	GWarn->EndSlowTask();

	// write anything left in the memory Ar to disk
	FileAr.Log(*Ar);

	return 1;
}

/*------------------------------------------------------------------------------
	ULevelExporterFBX implementation.
------------------------------------------------------------------------------*/
ULevelExporterFBX::ULevelExporterFBX(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UWorld::StaticClass();
	bText = false;
	bForceFileOperations = false;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("FBX"));
	FormatDescription.Add(TEXT("FBX File"));
}

bool ULevelExporterFBX::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
	GWarn->BeginSlowTask( NSLOCTEXT("UnrealEd", "ExportingLevelToFBX", "Exporting Level To FBX"), true );

	UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
	Exporter->CreateDocument();

	GWarn->StatusUpdate( 0, 1, NSLOCTEXT("UnrealEd", "ExportingLevelToFBX", "Exporting Level To FBX") );

	{
		UWorld* World = CastChecked<UWorld>(Object);
		ULevel* Level = World->PersistentLevel;

		if( bSelectedOnly )
		{
			Exporter->ExportBSP( World->GetModel(), true );
		}

		Exporter->ExportLevelMesh( Level, NULL, bSelectedOnly );

		// Export streaming levels and actors
		for( int32 CurLevelIndex = 0; CurLevelIndex < World->GetNumLevels(); ++CurLevelIndex )
		{
			ULevel* CurLevel = World->GetLevel( CurLevelIndex );
			if( CurLevel != NULL && CurLevel != Level )
			{
				Exporter->ExportLevelMesh( CurLevel, NULL, bSelectedOnly );
			}
		}
	}
	Exporter->WriteToFile(*UExporter::CurrentFilename);

	GWarn->EndSlowTask();

	return true;
}

/*------------------------------------------------------------------------------
	UPolysExporterOBJ implementation.
------------------------------------------------------------------------------*/
UPolysExporterOBJ::UPolysExporterOBJ(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UPolys::StaticClass();
	bText = true;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("OBJ"));
	FormatDescription.Add(TEXT("Object File"));

}

bool UPolysExporterOBJ::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags)
{
	TArray<FOBJGeom*> Objects;

	UPolys* Polys = CastChecked<UPolys> (Object);

	int32 PolyNum = 0;
	int32 TotalPolys = Polys->Element.Num();

	Ar.Logf (TEXT("# OBJ File Generated by UnrealEd\n"));

	ExportPolys( Polys, PolyNum, TotalPolys, Warn, false, NULL, Objects );

	for( int32 o = 0 ; o < Objects.Num() ; ++o )
	{
		FOBJGeom* object = Objects[o];
		object->Faces.Sort(FCompareMaterial());
	}

	uint32 IndexOffset = 0;
	// Export to the OBJ file
	ExportOBJs(Ar, NULL, Warn, CurrentFilename, Objects, NULL, IndexOffset);

	Ar.Logf (TEXT("# dElaernU yb detareneG eliF JBO\n"));

	return 1;
}


/*------------------------------------------------------------------------------
	USequenceExporterT3D implementation.
------------------------------------------------------------------------------*/
USequenceExporterT3D::USequenceExporterT3D(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

bool USequenceExporterT3D::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags)
{
	return true;
}

/*------------------------------------------------------------------------------
UStaticMeshExporterOBJ implementation.
------------------------------------------------------------------------------*/
UStaticMeshExporterOBJ::UStaticMeshExporterOBJ(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = UStaticMesh::StaticClass();
	bText = true;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("OBJ"));
	FormatDescription.Add(TEXT("Object File"));

}

bool UStaticMeshExporterOBJ::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags)
{
	UStaticMesh* StaticMesh = CastChecked<UStaticMesh>( Object );

	{
		// Create a new filename for the lightmap coordinate OBJ file (by adding "_UV1" to the end of the filename)
		FString Filename = UExporter::CurrentFilename.Left( UExporter::CurrentFilename.Len() - 4 ) + "_UV1." + UExporter::CurrentFilename.Right( 3 );

		// Open a second archive here so we can export lightmap coordinates at the same time we export the regular mesh
		FArchive* UV1File = IFileManager::Get().CreateFileWriter( *Filename );

		TArray<FVector> Verts;				// The verts in the mesh
		TArray<FVector2D> UVs;				// UV coords from channel 0
		TArray<FVector2D> UVLMs;			// Lightmap UVs from channel 1
		TArray<FVector> Normals;			// Normals
		TArray<uint32> SmoothingMasks;		// Complete collection of the smoothing groups from all triangles
		TArray<uint32> UniqueSmoothingMasks;	// Collection of the unique smoothing groups (used when writing out the face info into the OBJ file so we can group by smoothing group)

		UV1File->Logf( TEXT("# UnrealEd OBJ exporter\r\n") );
		Ar.Log( TEXT("# UnrealEd OBJ exporter\r\n") );

		// Currently, we only export LOD 0 of the static mesh. In the future, we could potentially export all available LODs
		FStaticMeshLODResources& RenderData = StaticMesh->GetLODForExport(0);
		FRawMesh RawMesh;
		StaticMesh->SourceModels[0].RawMeshBulkData->LoadRawMesh(RawMesh);

		uint32 Count = RenderData.GetNumTriangles();

		// Collect all the data about the mesh
		Verts.Reserve(3 * Count);
		UVs.Reserve(3 * Count);
		UVLMs.Reserve(3 * Count);
		Normals.Reserve(3 * Count);
		SmoothingMasks.Reserve(Count);
		UniqueSmoothingMasks.Reserve(Count);

		FIndexArrayView Indices = RenderData.IndexBuffer.GetArrayView();

		for( uint32 tri = 0 ; tri < Count ; tri++ )
		{
			uint32 Index1 = Indices[(tri * 3) + 0];
			uint32 Index2 = Indices[(tri * 3) + 1];
			uint32 Index3 = Indices[(tri * 3) + 2];

			FVector Vertex1 = RenderData.PositionVertexBuffer.VertexPosition(Index1);		//(FStaticMeshFullVertex*)(RawVertexData + Index1 * VertexStride);
			FVector Vertex2 = RenderData.PositionVertexBuffer.VertexPosition(Index2);
			FVector Vertex3 = RenderData.PositionVertexBuffer.VertexPosition(Index3);
			
			// Vertices
			Verts.Add( Vertex1 );
			Verts.Add( Vertex2 );
			Verts.Add( Vertex3 );

			// UVs from channel 0
			UVs.Add( RenderData.VertexBuffer.GetVertexUV(Index1, 0) );
			UVs.Add( RenderData.VertexBuffer.GetVertexUV(Index2, 0) );
			UVs.Add( RenderData.VertexBuffer.GetVertexUV(Index3, 0) );

			// UVs from channel 1 (lightmap coords)
			UVLMs.Add( RenderData.VertexBuffer.GetVertexUV(Index1, 1) );
			UVLMs.Add( RenderData.VertexBuffer.GetVertexUV(Index2, 1) );
			UVLMs.Add( RenderData.VertexBuffer.GetVertexUV(Index3, 1) );

			// Normals
			Normals.Add( RenderData.VertexBuffer.VertexTangentZ(Index1) );
			Normals.Add( RenderData.VertexBuffer.VertexTangentZ(Index2) );
			Normals.Add( RenderData.VertexBuffer.VertexTangentZ(Index3) );

			// Smoothing groups
			SmoothingMasks.Add( RawMesh.FaceSmoothingMasks[tri] );

			// Unique smoothing groups
			UniqueSmoothingMasks.AddUnique( RawMesh.FaceSmoothingMasks[tri] );
		}

		// Write out the vertex data

		UV1File->Logf( TEXT("\r\n") );
		Ar.Log( TEXT("\r\n") );
		for( int32 v = 0 ; v < Verts.Num() ; ++v )
		{
			// Transform to Lightwave's coordinate system
			UV1File->Logf( TEXT("v %f %f %f\r\n"), Verts[v].X, Verts[v].Z, Verts[v].Y );
			Ar.Logf( TEXT("v %f %f %f\r\n"), Verts[v].X, Verts[v].Z, Verts[v].Y );
		}

		// Write out the UV data (the lightmap file differs here in that it writes from the UVLMs array instead of UVs)

		UV1File->Logf( TEXT("\r\n") );
		Ar.Log( TEXT("\r\n") );
		for( int32 uv = 0 ; uv < UVs.Num() ; ++uv )
		{
			// Invert the y-coordinate (Lightwave has their bitmaps upside-down from us).
			UV1File->Logf( TEXT("vt %f %f\r\n"), UVLMs[uv].X, 1.0f - UVLMs[uv].Y );
			Ar.Logf( TEXT("vt %f %f\r\n"), UVs[uv].X, 1.0f - UVs[uv].Y );
		}

		// Write object header

		UV1File->Logf( TEXT("\r\n") );
		Ar.Log( TEXT("\r\n") );
		UV1File->Logf( TEXT("g UnrealEdObject\r\n") );
		Ar.Log( TEXT("g UnrealEdObject\r\n") );
		UV1File->Logf( TEXT("\r\n") );
		Ar.Log( TEXT("\r\n") );

		// Write out the face windings, sectioned by unique smoothing groups

		int32 SmoothingGroup = 0;

		for( int32 sm = 0 ; sm < UniqueSmoothingMasks.Num() ; ++sm )
		{
			UV1File->Logf( TEXT("s %i\r\n"), SmoothingGroup );
			Ar.Logf( TEXT("s %i\r\n"), SmoothingGroup );
			SmoothingGroup++;

			for( int32 tri = 0 ; tri < RenderData.GetNumTriangles() ; tri++ )
			{
				if( SmoothingMasks[tri] == UniqueSmoothingMasks[sm]  )
				{
					int idx = 1 + (tri * 3);

					UV1File->Logf( TEXT("f %d/%d %d/%d %d/%d\r\n"), idx, idx, idx+1, idx+1, idx+2, idx+2 );
					Ar.Logf( TEXT("f %d/%d %d/%d %d/%d\r\n"), idx, idx, idx+1, idx+1, idx+2, idx+2 );
				}
			}
		}

		// Write out footer

		UV1File->Logf( TEXT("\r\n") );
		Ar.Log( TEXT("\r\n") );
		UV1File->Logf( TEXT("g\r\n") );
		Ar.Log( TEXT("g\r\n") );

		// Clean up and finish
		delete UV1File;
	}

	// ------------------------------------------------------

	// 
	{
		// Create a new filename for the internal OBJ file (by adding "_Internal" to the end of the filename)
		FString Filename = UExporter::CurrentFilename.Left( UExporter::CurrentFilename.Len() - 4 ) + "_Internal." + UExporter::CurrentFilename.Right( 3 );

		// Open another archive
		FArchive* File = IFileManager::Get().CreateFileWriter( *Filename );

		File->Logf( TEXT("# UnrealEd OBJ exporter (_Internal)\r\n") );

		// Currently, we only export LOD 0 of the static mesh. In the future, we could potentially export all available LODs
		FStaticMeshLODResources& RenderData = StaticMesh->GetLODForExport(0);
		uint32 VertexCount = RenderData.GetNumVertices();

		check(VertexCount == RenderData.VertexBuffer.GetNumVertices());

		File->Logf( TEXT("\r\n") );
		for(uint32 i = 0; i < VertexCount; i++)
		{
			const FVector& OSPos = RenderData.PositionVertexBuffer.VertexPosition( i );
//			const FVector WSPos = StaticMeshComponent->LocalToWorld.TransformPosition( OSPos );
			const FVector WSPos = OSPos;

			// Transform to Lightwave's coordinate system
			File->Logf( TEXT("v %f %f %f\r\n"), WSPos.X, WSPos.Z, WSPos.Y );
		}

		File->Logf( TEXT("\r\n") );
		for(uint32 i = 0 ; i < VertexCount; ++i)
		{
			// takes the first UV
			const FVector2D UV = RenderData.VertexBuffer.GetVertexUV(i, 0);

			// Invert the y-coordinate (Lightwave has their bitmaps upside-down from us).
			File->Logf( TEXT("vt %f %f\r\n"), UV.X, 1.0f - UV.Y );
		}

		File->Logf( TEXT("\r\n") );

		for(uint32 i = 0 ; i < VertexCount; ++i)
		{
			const FVector& OSNormal = RenderData.VertexBuffer.VertexTangentZ( i ); 
			const FVector WSNormal = OSNormal; 

			// Transform to Lightwave's coordinate system
			File->Logf( TEXT("vn %f %f %f\r\n"), WSNormal.X, WSNormal.Z, WSNormal.Y );
		}

		{
			FIndexArrayView Indices = RenderData.IndexBuffer.GetArrayView();
			uint32 NumIndices = Indices.Num();

			check(NumIndices % 3 == 0);
			for(uint32 i = 0; i < NumIndices / 3; i++)
			{
				// Wavefront indices are 1 based
				uint32 a = Indices[3 * i + 0] + 1;
				uint32 b = Indices[3 * i + 1] + 1;
				uint32 c = Indices[3 * i + 2] + 1;

				File->Logf( TEXT("f %d/%d/%d %d/%d/%d %d/%d/%d\r\n"), 
					a,a,a,
					b,b,b,
					c,c,c);
			}
		}

		delete File;
	}

	return true;
}

/*------------------------------------------------------------------------------
UStaticMeshExporterFBX implementation.
------------------------------------------------------------------------------*/
UStaticMeshExporterFBX::UStaticMeshExporterFBX(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{


	SupportedClass = UStaticMesh::StaticClass();
	bText = false;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("FBX"));
	FormatDescription.Add(TEXT("FBX File"));

}

bool UStaticMeshExporterFBX::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
	UStaticMesh* StaticMesh = CastChecked<UStaticMesh>( Object );
	UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
	Exporter->CreateDocument();
	Exporter->ExportStaticMesh(StaticMesh);
	Exporter->WriteToFile(*UExporter::CurrentFilename);

	return true;
}


/*------------------------------------------------------------------------------
USkeletalMeshExporterFBX implementation.
------------------------------------------------------------------------------*/
USkeletalMeshExporterFBX::USkeletalMeshExporterFBX(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

	SupportedClass = USkeletalMesh::StaticClass();
	bText = false;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("FBX"));
	FormatDescription.Add(TEXT("FBX File"));
}

bool USkeletalMeshExporterFBX::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
	USkeletalMesh* SkeletalMesh = CastChecked<USkeletalMesh>( Object );
	UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
	Exporter->CreateDocument();
	Exporter->ExportSkeletalMesh(SkeletalMesh);
	Exporter->WriteToFile(*UExporter::CurrentFilename);

	return true;
}

/*------------------------------------------------------------------------------
UAnimSequenceExporterFBX implementation.
------------------------------------------------------------------------------*/
UAnimSequenceExporterFBX::UAnimSequenceExporterFBX(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SupportedClass = UAnimSequence::StaticClass();
	bText = false;
	PreferredFormatIndex = 0;
	FormatExtension.Add(TEXT("FBX"));
	FormatDescription.Add(TEXT("FBX File"));

}

bool UAnimSequenceExporterFBX::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
	UAnimSequence* AnimSequence = CastChecked<UAnimSequence>( Object );
	USkeleton * AnimSkeleton = AnimSequence->GetSkeleton();
	USkeletalMesh* PreviewMesh = AnimSkeleton->GetPreviewMesh(true);

	if (AnimSkeleton && PreviewMesh)
	{
		UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
		Exporter->CreateDocument();
		Exporter->ExportAnimSequence(AnimSequence, PreviewMesh, false);
		Exporter->WriteToFile( *UExporter::CurrentFilename );

		return true;
	}

	return false;
}

void UEditorEngine::RebuildStaticNavigableGeometry(ULevel* Level)
{
	// iterate through all BSPs and gather it's geometry, without any filtering - filtering will be done while building
	// NOTE: any other game-time static geometry can (and should) be added here
	if (Level != NULL)
	{
		Level->StaticNavigableGeometry.Reset();

		if (Level->Model != NULL)
		{
			UModel* Model =  Level->Model;
			int32 TotalPolys = 0;

			TArray<FPoly> TempPolys;

			bspBuildFPolys(Model, 0, 0, &TempPolys);
			UPolys* Polys = Model->Polys;
			int32 PolyNum = TempPolys.Num();

			TotalPolys = TotalPolys + PolyNum;

			for( int32 i = 0; i < Model->Nodes.Num(); ++i )
			{
				FBspNode* Node = &Model->Nodes[i];
				FBspSurf& Surf = Model->Surfs[Node->iSurf];

				const FVector& TextureBase = Model->Points[Surf.pBase];
				const FVector& TextureX = Model->Vectors[Surf.vTextureU];
				const FVector& TextureY = Model->Vectors[Surf.vTextureV];
				const FVector& Normal = Model->Vectors[Surf.vNormal];

				FPoly Poly;
				polyFindMaster( Model, Node->iSurf, Poly );

				// Triangulate this node and generate an OBJ face from the vertices.
				for(int32 StartVertexIndex = 1;StartVertexIndex < Node->NumVertices-1;StartVertexIndex++)
				{
					// These map the node's vertices to the 3 triangle indices to triangulate the convex polygon.
					int32 TriVertIndices[3] = { Node->iVertPool,
						Node->iVertPool + StartVertexIndex,
						Node->iVertPool + StartVertexIndex + 1 };

					for(uint32 TriVertexIndex = 0; TriVertexIndex < 3; TriVertexIndex++)
					{
						const FVert& Vert = Model->Verts[TriVertIndices[TriVertexIndex]];
						Level->StaticNavigableGeometry.Add( Model->Points[Vert.pVertex] );
					}
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	UExportTextContainer
-----------------------------------------------------------------------------*/
UExportTextContainer::UExportTextContainer(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}




/*-----------------------------------------------------------------------------
	Material export utilities
-----------------------------------------------------------------------------*/
namespace MaterialExportUtils
{
	
	bool ExportMaterialProperty(UMaterialInterface* InMaterial, 
								EMaterialProperty InMaterialProperty, 
								UTextureRenderTarget2D* InRenderTarget, 
								TArray<FColor>& OutBMP)
	{
		TScopedPointer<FExportMaterialProxy> MaterialProxy(new FExportMaterialProxy(InMaterial, InMaterialProperty));
		if (MaterialProxy == NULL)
		{
			return false;
		}

		FColor UniformValue;
		if (MaterialProxy->WillGenerateUniformData(UniformValue))
		{
			// Single value... fill it in.
			OutBMP.Empty();
			OutBMP.Add(UniformValue);
			return true;
		}
	
		check(InRenderTarget);
		FTextureRenderTargetResource* RTResource = InRenderTarget->GameThread_GetRenderTargetResource();

		RHIBeginScene();
		{
			// Create a canvas for the render target and clear it to black
			FCanvas Canvas(RTResource, NULL, FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime);
			Canvas.Clear(FLinearColor::Black);
			FCanvasTileItem TileItem(FVector2D(0.0f, 0.0f), MaterialProxy, FVector2D(InRenderTarget->SizeX, InRenderTarget->SizeY));
			TileItem.bFreezeTime = true;
			Canvas.DrawItem( TileItem );
			Canvas.Flush();
			FlushRenderingCommands();
			Canvas.SetRenderTarget(NULL);
			FlushRenderingCommands();
		}
		RHIEndScene();
				
		bool bNormalmap = (InMaterialProperty == MP_Normal);
		FReadSurfaceDataFlags ReadPixelFlags(bNormalmap ? RCM_SNorm : RCM_UNorm);
		ReadPixelFlags.SetLinearToGamma(false);

		return RTResource->ReadPixels(OutBMP, ReadPixelFlags);
	}
	
	bool ExportMaterial(UMaterialInterface* InMaterial, FFlattenMaterial& OutFlattenMaterial)
	{
		// Render diffuse property
		{
			// Reset to default in case user specified invalid diffuse texture size
			if (OutFlattenMaterial.DiffuseSize.X <= 0 || 
				OutFlattenMaterial.DiffuseSize.Y <= 0)
			{
				OutFlattenMaterial.DiffuseSize = FFlattenMaterial().DiffuseSize;
			}
			
			// Create temporary render target
			UTextureRenderTarget2D* RenderTargetDiffuse = new UTextureRenderTarget2D(FPostConstructInitializeProperties());
			check(RenderTargetDiffuse);
			RenderTargetDiffuse->AddToRoot();
			RenderTargetDiffuse->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
			RenderTargetDiffuse->InitCustomFormat(
				OutFlattenMaterial.DiffuseSize.X, 
				OutFlattenMaterial.DiffuseSize.Y, PF_B8G8R8A8, true);
			
			//
			OutFlattenMaterial.DiffuseSamples.Empty(OutFlattenMaterial.DiffuseSize.X * OutFlattenMaterial.DiffuseSize.Y);
			bool bResult = ExportMaterialProperty(InMaterial, MP_DiffuseColor, RenderTargetDiffuse, OutFlattenMaterial.DiffuseSamples);

			// Uniform value
			if (OutFlattenMaterial.DiffuseSamples.Num() == 1)
			{
				OutFlattenMaterial.DiffuseSize = FIntPoint(1,1);
			}
			
			RenderTargetDiffuse->RemoveFromRoot();
			RenderTargetDiffuse = nullptr;

			if (!bResult)
			{
				return false;
			}
		}

		// Render normal property
		if (InMaterial->GetMaterial()->HasNormalConnected())
		{
			// Reset to default in case user specified invalid normal texture size
			if (OutFlattenMaterial.NormalSize.X <= 0 || 
				OutFlattenMaterial.NormalSize.Y <= 0)
			{
				OutFlattenMaterial.NormalSize = FFlattenMaterial().NormalSize;
			}
			
			// Create temporary render target
			UTextureRenderTarget2D* RenderTargetNormal = new UTextureRenderTarget2D(FPostConstructInitializeProperties());
			check(RenderTargetNormal);
			RenderTargetNormal->AddToRoot();
			RenderTargetNormal->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
			RenderTargetNormal->InitCustomFormat(
				OutFlattenMaterial.NormalSize.X,
				OutFlattenMaterial.NormalSize.Y, PF_FloatRGB, true);

			//
			OutFlattenMaterial.NormalSamples.Empty(OutFlattenMaterial.NormalSize.X * OutFlattenMaterial.NormalSize.Y);
			bool bResult = ExportMaterialProperty(InMaterial, MP_Normal, RenderTargetNormal, OutFlattenMaterial.NormalSamples);

			// Uniform value
			if (OutFlattenMaterial.NormalSamples.Num() == 1)
			{
				OutFlattenMaterial.NormalSize = FIntPoint(1,1);
			}
		
			RenderTargetNormal->RemoveFromRoot();
			RenderTargetNormal = nullptr;

			if (!bResult)
			{
				return false;
			}
		}

		OutFlattenMaterial.MaterialId = InMaterial->GetLightingGuid();
		return true;
	}
	
	bool ExportMaterial(ALandscapeProxy* InLandscape, FFlattenMaterial& OutFlattenMaterial)
	{
		check(InLandscape);
		TArray<ULandscapeComponent*> ComponentsToRender;
		InLandscape->GetComponents<ULandscapeComponent>(ComponentsToRender);
		if (ComponentsToRender.Num() == 0)
		{
			return false;
		}

		// Reset to default in case user specified invalid diffuse texture size
		if (OutFlattenMaterial.DiffuseSize.X <= 0 || 
			OutFlattenMaterial.DiffuseSize.Y <= 0)
		{
			OutFlattenMaterial.DiffuseSize = FFlattenMaterial().DiffuseSize;
		}

		// Normal map will not be used 
		OutFlattenMaterial.NormalSamples.Empty();
		OutFlattenMaterial.NormalSize = FIntPoint::ZeroValue;

		FIntRect LandscapeRect = InLandscape->GetBoundingRect();
		FVector MidPoint = FVector(LandscapeRect.Min, 0.f) + FVector(LandscapeRect.Size(), 0.f)*0.5f;
		
		FVector LandscapeCenter = InLandscape->GetTransform().TransformPosition(MidPoint);
		FVector LandscapeExtent = FVector(LandscapeRect.Size(), 0.f)*InLandscape->GetActorScale()*0.5f; 
		{
			UTextureRenderTarget2D* RenderTargetTexture = new UTextureRenderTarget2D(FPostConstructInitializeProperties());
			check(RenderTargetTexture);
			RenderTargetTexture->AddToRoot();
			RenderTargetTexture->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
			RenderTargetTexture->InitCustomFormat(
				OutFlattenMaterial.DiffuseSize.X, 
				OutFlattenMaterial.DiffuseSize.Y, PF_B8G8R8A8, true);
			FTextureRenderTargetResource* RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource();
			FSceneInterface* Scene = InLandscape->GetWorld()->Scene;
			{
				// Manually call RHIBeginScene since we are issuing draw calls outside of the main rendering function
				ENQUEUE_UNIQUE_RENDER_COMMAND(
					BeginCommand,
				{
					RHIBeginScene();
				});

				FSceneViewFamilyContext ViewFamily(
					FSceneViewFamily::ConstructionValues(RenderTargetResource, Scene, FEngineShowFlags(ESFIM_Game))
						.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime)
					);

				ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
				ViewFamily.EngineShowFlags.MotionBlur = 0;
				ViewFamily.EngineShowFlags.Lighting = 0;
				ViewFamily.EngineShowFlags.PostProcessing = 0;
				ViewFamily.EngineShowFlags.LightFunctions = 0;
				ViewFamily.EngineShowFlags.DynamicShadows = 0;
				ViewFamily.EngineShowFlags.Atmosphere = 0;

				FSceneViewInitOptions ViewInitOptions;
				ViewInitOptions.SetViewRectangle(FIntRect(0, 0, OutFlattenMaterial.DiffuseSize.X, OutFlattenMaterial.DiffuseSize.Y));
				ViewInitOptions.ViewFamily = &ViewFamily;

				ViewInitOptions.ViewMatrix = FTranslationMatrix(-LandscapeCenter);
				ViewInitOptions.ViewMatrix*= FInverseRotationMatrix(InLandscape->GetActorRotation());
				ViewInitOptions.ViewMatrix*= FMatrix(	FPlane(1,	0,	0,	0),
														FPlane(0,	-1,	0,	0),
														FPlane(0,	0,	-1,	0),
														FPlane(0,	0,	0,	1));
				
				const float ZOffset = WORLD_MAX;
				ViewInitOptions.ProjectionMatrix =  FReversedZOrthoMatrix(
					LandscapeExtent.X,
					LandscapeExtent.Y,
					0.5f / ZOffset,
					ZOffset);

				FSceneView* NewView = new FSceneView(ViewInitOptions);
				ViewFamily.Views.Add(NewView);

				// We need to hide all primitives except target landscape
				for (TActorIterator<AActor> It(InLandscape->GetWorld()); It; ++It)
				{
					AActor* Actor = *It;
					if (Actor && Actor != InLandscape)
					{
						TArray<UPrimitiveComponent*> PrimitiveComponents;
						Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
						for (UPrimitiveComponent* Component : PrimitiveComponents)
						{
							if (Component->IsRegistered() && Component->SceneProxy)
							{
								NewView->HiddenPrimitives.Add(Component->SceneProxy->GetPrimitiveComponentId());
							}
						}
					}
				}
			
				FCanvas Canvas(RenderTargetResource, NULL, FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime);
				Canvas.Clear(FLinearColor::Black);
				GetRendererModule().BeginRenderingViewFamily(&Canvas, &ViewFamily);

				ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
					UpdateThumbnailRTCommand,
					FTextureRenderTargetResource*, RenderTargetResource, RenderTargetResource,
				{
					// Copy (resolve) the rendered thumbnail from the render target to its texture
					RHICopyToResolveTarget(
						RenderTargetResource->GetRenderTargetTexture(),		// Source texture
						RenderTargetResource->TextureRHI,					// Dest texture
						false,												// Do we need the source image content again?
						FResolveParams() );									// Resolve parameters
				});

				OutFlattenMaterial.DiffuseSamples.Empty();
				OutFlattenMaterial.DiffuseSamples.AddUninitialized(OutFlattenMaterial.DiffuseSize.X*OutFlattenMaterial.DiffuseSize.Y);

				// Copy the contents of the remote texture to system memory
				// NOTE: OutRawImageData must be a preallocated buffer!
				RenderTargetResource->ReadPixelsPtr(
					(FColor*)OutFlattenMaterial.DiffuseSamples.GetTypedData(), 
					FReadSurfaceDataFlags(), 
					FIntRect(0, 0, OutFlattenMaterial.DiffuseSize.X, OutFlattenMaterial.DiffuseSize.Y)
					);
			
				ENQUEUE_UNIQUE_RENDER_COMMAND(
					EndCommand,
				{
					RHIEndScene();
				});

				FlushRenderingCommands();
			}

			RenderTargetTexture->RemoveFromRoot();
			RenderTargetTexture = nullptr;
		}

		OutFlattenMaterial.MaterialId = InLandscape->GetLandscapeGuid();
		return true;
	}

	UMaterial* CreateMaterial(const FFlattenMaterial& InFlattenMaterial, UObject* Outer, const FString& BaseName, EObjectFlags Flags)
	{
		FName MaterialName = MakeUniqueObjectName(Outer, UMaterial::StaticClass(), *(BaseName + TEXT("_Material")));
		
		UMaterial* Material = ConstructObject<UMaterial>(UMaterial::StaticClass(), Outer, MaterialName, Flags);
		Material->TwoSided = false;
		Material->SetLightingModel(MLM_DefaultLit);

		//Set Metallic as a default constant
		{
			UMaterialExpressionConstant* MetallicExpression = ConstructObject<UMaterialExpressionConstant>(UMaterialExpressionConstant::StaticClass(), Material);
			MetallicExpression->R = 0.0f;
			Material->Expressions.Add(MetallicExpression);
			Material->Metallic.Expression = MetallicExpression;

			MetallicExpression->MaterialExpressionEditorX = 250;
			MetallicExpression->MaterialExpressionEditorY = 0;
		}
		
		//Set Roughness as default constant
		{
			UMaterialExpressionConstant* RoughnessExpression = ConstructObject<UMaterialExpressionConstant>(UMaterialExpressionConstant::StaticClass(), Material);
			RoughnessExpression->R = 0.8f;
			Material->Expressions.Add(RoughnessExpression);
			Material->Roughness.Expression = RoughnessExpression;

			RoughnessExpression->MaterialExpressionEditorX = 250;
			RoughnessExpression->MaterialExpressionEditorY = 150;
		}

		//Build the Diffuse UTexture
		if (InFlattenMaterial.DiffuseSamples.Num() > 1)
		{
			FString DiffuseTextureName = MakeUniqueObjectName(Outer, UTexture2D::StaticClass(), *(BaseName + TEXT("_Diffuse"))).ToString();
			FCreateTexture2DParameters TexParams;
			TexParams.bUseAlpha = false;
			TexParams.CompressionSettings = TC_Default;
			TexParams.bDeferCompression = true;
			TexParams.bSRGB = false;

			UTexture2D* DiffuseTexture = FImageUtils::CreateTexture2D(
				InFlattenMaterial.DiffuseSize.X, 
				InFlattenMaterial.DiffuseSize.Y,
				InFlattenMaterial.DiffuseSamples,
				Outer,
				DiffuseTextureName,
				Flags, 
				TexParams);

			//Create BaseColor Expression of the diffuse texture
			UMaterialExpressionTextureSample* BasecolorExpression = ConstructObject<UMaterialExpressionTextureSample>(UMaterialExpressionTextureSample::StaticClass(), Material);
			BasecolorExpression->Texture = DiffuseTexture;
			BasecolorExpression->SamplerType = EMaterialSamplerType::SAMPLERTYPE_Color;
			BasecolorExpression->MaterialExpressionEditorX = 400;
			BasecolorExpression->MaterialExpressionEditorY = -150;
			Material->Expressions.Add(BasecolorExpression);
			Material->BaseColor.Expression = BasecolorExpression;
		}

		//Build the Normal UTexture
		if (InFlattenMaterial.NormalSamples.Num() > 1)
		{
			FString NormalTextureName = MakeUniqueObjectName(Outer, UTexture2D::StaticClass(), *(BaseName + TEXT("_Normal"))).ToString();
			FCreateTexture2DParameters TexParams;
			TexParams.bUseAlpha = false;
			TexParams.CompressionSettings = TC_Normalmap;
			TexParams.bDeferCompression = true;
			TexParams.bSRGB = false;

			UTexture2D* NormalTexture = FImageUtils::CreateTexture2D(
				InFlattenMaterial.NormalSize.X, 
				InFlattenMaterial.NormalSize.Y,
				InFlattenMaterial.NormalSamples,
				Outer,
				NormalTextureName,
				Flags, 
				TexParams);

			NormalTexture->LODGroup = TEXTUREGROUP_WorldNormalMap;
			NormalTexture->PostEditChange();
						
			//Assign the normalmap to the material
			UMaterialExpressionTextureSample* NormalExpression = ConstructObject<UMaterialExpressionTextureSample>(UMaterialExpressionTextureSample::StaticClass(), Material);
			NormalExpression->Texture = NormalTexture;
			NormalExpression->SamplerType = EMaterialSamplerType::SAMPLERTYPE_Normal;
			NormalExpression->MaterialExpressionEditorX = 400;
			NormalExpression->MaterialExpressionEditorY = 300;
			Material->Expressions.Add(NormalExpression);
			Material->Normal.Expression = NormalExpression;
		}
			
		Material->PostEditChange();
		return Material;
	}
}