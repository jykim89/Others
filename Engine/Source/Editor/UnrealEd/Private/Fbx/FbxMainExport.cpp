// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*
* Copyright 2010 Autodesk, Inc.  All Rights Reserved.
*
* Permission to use, copy, modify, and distribute this software in object
* code form for any purpose and without fee is hereby granted, provided
* that the above copyright notice appears in all copies and that both
* that copyright notice and the limited warranty and restricted rights
* notice below appear in all supporting documentation.
*
* AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS.
* AUTODESK SPECIFICALLY DISCLAIMS ANY AND ALL WARRANTIES, WHETHER EXPRESS
* OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTY
* OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE OR NON-INFRINGEMENT
* OF THIRD PARTY RIGHTS.  AUTODESK DOES NOT WARRANT THAT THE OPERATION
* OF THE PROGRAM WILL BE UNINTERRUPTED OR ERROR FREE.
*
* In no event shall Autodesk, Inc. be liable for any direct, indirect,
* incidental, special, exemplary, or consequential damages (including,
* but not limited to, procurement of substitute goods or services;
* loss of use, data, or profits; or business interruption) however caused
* and on any theory of liability, whether in contract, strict liability,
* or tort (including negligence or otherwise) arising in any way out
* of such code.
*
* This software is provided to the U.S. Government with the same rights
* and restrictions as described herein.
*/

/*=============================================================================
	Main implementation of FFbxExporter : export FBX data from Unreal
=============================================================================*/

#include "UnrealEd.h"
#include "LandscapeDataAccess.h"

#include "FbxExporter.h"
#include "RawMesh.h"

namespace UnFbx
{

TSharedPtr<FFbxExporter> FFbxExporter::StaticInstance;

// By default we want to weld verts, but provide option to not weld
bool FFbxExporter::bStaticMeshExportUnWeldedVerts = false;

FFbxExporter::FFbxExporter()
{
	// Create the SdkManager
	SdkManager = FbxManager::Create();

	// create an IOSettings object
	FbxIOSettings * ios = FbxIOSettings::Create(SdkManager, IOSROOT );
	SdkManager->SetIOSettings(ios);

	DefaultCamera = NULL;

	if( GConfig )
	{
		GConfig->GetBool( TEXT("FBXMeshExport"), TEXT("StaticMeshExport_UnWeldedVerts"), bStaticMeshExportUnWeldedVerts, GEditorIni);
	}
}

FFbxExporter::~FFbxExporter()
{
	if (SdkManager)
	{
		SdkManager->Destroy();
		SdkManager = NULL;
	}
}

FFbxExporter* FFbxExporter::GetInstance()
{
	if (!StaticInstance.IsValid())
	{
		StaticInstance = MakeShareable( new FFbxExporter() );
	}
	return StaticInstance.Get();
}

void FFbxExporter::DeleteInstance()
{
	StaticInstance.Reset();
}

void FFbxExporter::CreateDocument()
{
	Scene = FbxScene::Create(SdkManager,"");
	
	// create scene info
	FbxDocumentInfo* SceneInfo = FbxDocumentInfo::Create(SdkManager,"SceneInfo");
	SceneInfo->mTitle = "Unreal FBX Exporter";
	SceneInfo->mSubject = "Export FBX meshes from Unreal";


	Scene->SetSceneInfo(SceneInfo);
	
	//FbxScene->GetGlobalSettings().SetOriginalUpAxis(KFbxAxisSystem::Max);
	FbxAxisSystem::EFrontVector FrontVector = (FbxAxisSystem::EFrontVector)-FbxAxisSystem::eParityOdd;
	const FbxAxisSystem UnrealZUp(FbxAxisSystem::eZAxis, FrontVector, FbxAxisSystem::eRightHanded);
	Scene->GetGlobalSettings().SetAxisSystem(UnrealZUp);
	Scene->GetGlobalSettings().SetOriginalUpAxis(UnrealZUp);
	// Maya use cm by default
	Scene->GetGlobalSettings().SetSystemUnit(FbxSystemUnit::cm);
	//FbxScene->GetGlobalSettings().SetOriginalSystemUnit( KFbxSystemUnit::m );
	
	// setup anim stack
	AnimStack = FbxAnimStack::Create(Scene, "Unreal Take");
	//KFbxSet<KTime>(AnimStack->LocalStart, KTIME_ONE_SECOND);
	AnimStack->Description.Set("Animation Take for Unreal.");

	// this take contains one base layer. In fact having at least one layer is mandatory.
	AnimLayer = FbxAnimLayer::Create(Scene, "Base Layer");
	AnimStack->AddMember(AnimLayer);
}

#ifdef IOS_REF
#undef  IOS_REF
#define IOS_REF (*(SdkManager->GetIOSettings()))
#endif

void FFbxExporter::WriteToFile(const TCHAR* Filename)
{
	int32 Major, Minor, Revision;
	bool Status = true;

	int32 FileFormat = -1;
	bool bEmbedMedia = false;

	// Create an exporter.
	FbxExporter* Exporter = FbxExporter::Create(SdkManager, "");

	// set file format
	if( FileFormat < 0 || FileFormat >= SdkManager->GetIOPluginRegistry()->GetWriterFormatCount() )
	{
		// Write in fall back format if pEmbedMedia is true
		FileFormat = SdkManager->GetIOPluginRegistry()->GetNativeWriterFormat();
	}

	// Set the export states. By default, the export states are always set to 
	// true except for the option eEXPORT_TEXTURE_AS_EMBEDDED. The code below 
	// shows how to change these states.

	IOS_REF.SetBoolProp(EXP_FBX_MATERIAL,        true);
	IOS_REF.SetBoolProp(EXP_FBX_TEXTURE,         true);
	IOS_REF.SetBoolProp(EXP_FBX_EMBEDDED,        bEmbedMedia);
	IOS_REF.SetBoolProp(EXP_FBX_SHAPE,           true);
	IOS_REF.SetBoolProp(EXP_FBX_GOBO,            true);
	IOS_REF.SetBoolProp(EXP_FBX_ANIMATION,       true);
	IOS_REF.SetBoolProp(EXP_FBX_GLOBAL_SETTINGS, true);

	// We export using FBX 2013 format because many users are still on that version and FBX 2014 files has compatibility issues with
	// normals when importing to an earlier version of the plugin
	Exporter->SetFileExportVersion(FBX_FILE_VERSION_7300, FbxSceneRenamer::eNone );

	// Initialize the exporter by providing a filename.
	if( !Exporter->Initialize(TCHAR_TO_UTF8(Filename), FileFormat, SdkManager->GetIOSettings()) )
	{
		UE_LOG(LogFbx, Warning, TEXT("Call to KFbxExporter::Initialize() failed.\n"));
		UE_LOG(LogFbx, Warning, TEXT("Error returned: %s\n\n"), Exporter->GetStatus().GetErrorString() );
		return;
	}

	FbxManager::GetFileFormatVersion(Major, Minor, Revision);
	UE_LOG(LogFbx, Warning, TEXT("FBX version number for this version of the FBX SDK is %d.%d.%d\n\n"), Major, Minor, Revision);

	// Export the scene.
	Status = Exporter->Export(Scene); 

	// Destroy the exporter.
	Exporter->Destroy();
	
	CloseDocument();
	
	return;
}

/**
 * Release the FBX scene, releasing its memory.
 */
void FFbxExporter::CloseDocument()
{
	FbxActors.Reset();
	FbxMaterials.Reset();
	FbxNodeNameToIndexMap.Reset();
	
	if (Scene)
	{
		Scene->Destroy();
		Scene = NULL;
	}
}

void FFbxExporter::CreateAnimatableUserProperty(FbxNode* Node, float Value, const char* Name, const char* Label)
{
	// Add one user property for recording the animation
	FbxProperty IntensityProp = FbxProperty::Create(Node, FbxFloatDT, Name, Label);
	IntensityProp.Set(Value);
	IntensityProp.ModifyFlag(FbxPropertyAttr::eUserDefined, true);
	IntensityProp.ModifyFlag(FbxPropertyAttr::eAnimatable, true);
}

/**
 * Exports the basic scene information to the FBX document.
 */
void FFbxExporter::ExportLevelMesh( ULevel* InLevel, AMatineeActor* InMatineeActor, bool bSelectedOnly )
{
	if (InLevel == NULL)
	{
		return;
	}

	if( !bSelectedOnly )
	{
		// Exports the level's scene geometry
		// the vertex number of Model must be more than 2 (at least a triangle panel)
		if (InLevel->Model != NULL && InLevel->Model->VertexBuffer.Vertices.Num() > 2 && InLevel->Model->MaterialIndexBuffers.Num() > 0)
		{
			// create a FbxNode
			FbxNode* Node = FbxNode::Create(Scene,"LevelMesh");

			// set the shading mode to view texture
			Node->SetShadingMode(FbxNode::eTextureShading);
			Node->LclScaling.Set(FbxVector4(1.0, 1.0, 1.0));

			Scene->GetRootNode()->AddChild(Node);

			// Export the mesh for the world
			ExportModel(InLevel->Model, Node, "Level Mesh");
		}
	}

	// Export all the recognized global actors.
	// Right now, this only includes lights.
	UWorld* World = NULL;
	if( InMatineeActor )
	{
		World = InMatineeActor->GetWorld();
	}
	else
	{
		World = CastChecked<UWorld>( InLevel->GetOuter() );
	}
	check(World);
	int32 ActorCount = World->GetCurrentLevel()->Actors.Num();
	for (int32 ActorIndex = 0; ActorIndex < ActorCount; ++ActorIndex)
	{
		AActor* Actor = World->GetCurrentLevel()->Actors[ActorIndex];
		if ( Actor != NULL && ( !bSelectedOnly || bSelectedOnly && Actor->IsSelected() ) )
		{
			if (Actor->IsA(ALight::StaticClass()))
			{
				ExportLight((ALight*) Actor, InMatineeActor );
			}
			else if (Actor->IsA(AStaticMeshActor::StaticClass()))
			{
				ExportStaticMesh( Actor, CastChecked<AStaticMeshActor>(Actor)->StaticMeshComponent, InMatineeActor );
			}
			else if (Actor->IsA(ALandscapeProxy::StaticClass()))
			{
				ExportLandscape(CastChecked<ALandscapeProxy>(Actor), false);
			}
			else if (Actor->IsA(ABrush::StaticClass()))
			{
				// All brushes should be included within the world geometry exported above.
				ExportBrush((ABrush*) Actor, NULL, 0 );
			}
			else if (Actor->IsA(AEmitter::StaticClass()))
			{
				ExportActor( Actor, InMatineeActor ); // Just export the placement of the particle emitter.
			}
			else if( Actor->GetClass()->ClassGeneratedBy != NULL )
			{
				// Export blueprint actors and all their components
				ExportActor( Actor, InMatineeActor, true );
			}
		}
	}
}

/**
 * Exports the light-specific information for a light actor.
 */
void FFbxExporter::ExportLight( ALight* Actor, AMatineeActor* InMatineeActor )
{
	if (Scene == NULL || Actor == NULL || !Actor->LightComponent.IsValid()) return;

	// Export the basic actor information.
	FbxNode* FbxActor = ExportActor( Actor, InMatineeActor ); // this is the pivot node
	// The real fbx light node
	FbxNode* FbxLightNode = FbxActor->GetParent();

	ULightComponent* BaseLight = Actor->LightComponent;

	FString FbxNodeName = GetActorNodeName(Actor, InMatineeActor);

	// Export the basic light information
	FbxLight* Light = FbxLight::Create(Scene, TCHAR_TO_ANSI(*FbxNodeName));
	Light->Intensity.Set(BaseLight->Intensity);
	Light->Color.Set(Converter.ConvertToFbxColor(BaseLight->LightColor));
	
	// Add one user property for recording the Brightness animation
	CreateAnimatableUserProperty(FbxLightNode, BaseLight->Intensity, "UE_Intensity", "UE_Matinee_Light_Intensity");
	
	// Look for the higher-level light types and determine the lighting method
	if (BaseLight->IsA(UPointLightComponent::StaticClass()))
	{
		UPointLightComponent* PointLight = (UPointLightComponent*) BaseLight;
		if (BaseLight->IsA(USpotLightComponent::StaticClass()))
		{
			USpotLightComponent* SpotLight = (USpotLightComponent*) BaseLight;
			Light->LightType.Set(FbxLight::eSpot);

			// Export the spot light parameters.
			if (!FMath::IsNearlyZero(SpotLight->InnerConeAngle))
			{
				Light->InnerAngle.Set(SpotLight->InnerConeAngle);
			}
			else // Maya requires a non-zero inner cone angle
			{
				Light->InnerAngle.Set(0.01f);
			}
			Light->OuterAngle.Set(SpotLight->OuterConeAngle);
		}
		else
		{
			Light->LightType.Set(FbxLight::ePoint);
		}
		
		// Export the point light parameters.
		Light->EnableFarAttenuation.Set(true);
		Light->FarAttenuationEnd.Set(PointLight->AttenuationRadius);
		// Add one user property for recording the FalloffExponent animation
		CreateAnimatableUserProperty(FbxLightNode, PointLight->AttenuationRadius, "UE_Radius", "UE_Matinee_Light_Radius");
		
		// Add one user property for recording the FalloffExponent animation
		CreateAnimatableUserProperty(FbxLightNode, PointLight->LightFalloffExponent, "UE_FalloffExponent", "UE_Matinee_Light_FalloffExponent");
	}
	else if (BaseLight->IsA(UDirectionalLightComponent::StaticClass()))
	{
		// The directional light has no interesting properties.
		Light->LightType.Set(FbxLight::eDirectional);
	}
	
	FbxActor->SetNodeAttribute(Light);
}

void FFbxExporter::ExportCamera( ACameraActor* Actor, AMatineeActor* InMatineeActor )
{
	if (Scene == NULL || Actor == NULL) return;

	// Export the basic actor information.
	FbxNode* FbxActor = ExportActor( Actor, InMatineeActor ); // this is the pivot node
	// The real fbx camera node
	FbxNode* FbxCameraNode = FbxActor->GetParent();

	FString FbxNodeName = GetActorNodeName(Actor, NULL);

	// Create a properly-named FBX camera structure and instantiate it in the FBX scene graph
	FbxCamera* Camera = FbxCamera::Create(Scene, TCHAR_TO_ANSI(*FbxNodeName));

	// Export the view area information
	Camera->ProjectionType.Set(FbxCamera::ePerspective);
	Camera->SetAspect(FbxCamera::eFixedRatio, Actor->CameraComponent->AspectRatio, 1.0f);
	Camera->FilmAspectRatio.Set(Actor->CameraComponent->AspectRatio);
	Camera->SetApertureWidth(Actor->CameraComponent->AspectRatio * 0.612f); // 0.612f is a magic number from Maya that represents the ApertureHeight
	Camera->SetApertureMode(FbxCamera::eFocalLength);
	Camera->FocalLength.Set(Camera->ComputeFocalLength(Actor->CameraComponent->FieldOfView));
	
	// Add one user property for recording the AspectRatio animation
	CreateAnimatableUserProperty(FbxCameraNode, Actor->CameraComponent->AspectRatio, "UE_AspectRatio", "UE_Matinee_Camera_AspectRatio");

	// Push the near/far clip planes away, as the engine uses larger values than the default.
	Camera->SetNearPlane(10.0f);
	Camera->SetFarPlane(100000.0f);

	FbxActor->SetNodeAttribute(Camera);

	DefaultCamera = Camera;
}

/**
 * Exports the mesh and the actor information for a brush actor.
 */
void FFbxExporter::ExportBrush(ABrush* Actor, UModel* InModel, bool bConvertToStaticMesh )
{
	if (Scene == NULL || Actor == NULL || !Actor->BrushComponent.IsValid()) return;

 	if (!bConvertToStaticMesh)
 	{
 		// Retrieve the information structures, verifying the integrity of the data.
 		UModel* Model = Actor->BrushComponent->Brush;

 		if (Model == NULL || Model->VertexBuffer.Vertices.Num() < 3 || Model->MaterialIndexBuffers.Num() == 0) return;
 
 		// Create the FBX actor, the FBX geometry and instantiate it.
 		FbxNode* FbxActor = ExportActor( Actor, NULL );
 		Scene->GetRootNode()->AddChild(FbxActor);
 
 		// Export the mesh information
 		ExportModel(Model, FbxActor, TCHAR_TO_ANSI(*Actor->GetName()));
 	}
 	else
 	{
		FRawMesh Mesh;
		TArray<UMaterialInterface*>	Materials;
		GetBrushMesh(Actor,Actor->Brush,Mesh,Materials);

 		if( Mesh.VertexPositions.Num() )
		{
			UStaticMesh* StaticMesh = CreateStaticMesh(Mesh,Materials,GetTransientPackage(),Actor->GetFName());
			ExportStaticMesh( StaticMesh, &Materials );
 		}
 	}
}

void FFbxExporter::ExportModel(UModel* Model, FbxNode* Node, const char* Name)
{
	//int32 VertexCount = Model->VertexBuffer.Vertices.Num();
	int32 MaterialCount = Model->MaterialIndexBuffers.Num();

	const float BiasedHalfWorldExtent = HALF_WORLD_MAX * 0.95f;

	// Create the mesh and three data sources for the vertex positions, normals and texture coordinates.
	FbxMesh* Mesh = FbxMesh::Create(Scene, Name);
	
	// Create control points.
	uint32 VertCount(Model->VertexBuffer.Vertices.Num());
	Mesh->InitControlPoints(VertCount);
	FbxVector4* ControlPoints = Mesh->GetControlPoints();
	
	// Set the normals on Layer 0.
	FbxLayer* Layer = Mesh->GetLayer(0);
	if (Layer == NULL)
	{
		Mesh->CreateLayer();
		Layer = Mesh->GetLayer(0);
	}
	
	// We want to have one normal for each vertex (or control point),
	// so we set the mapping mode to eBY_CONTROL_POINT.
	FbxLayerElementNormal* LayerElementNormal= FbxLayerElementNormal::Create(Mesh, "");

	LayerElementNormal->SetMappingMode(FbxLayerElement::eByControlPoint);

	// Set the normal values for every control point.
	LayerElementNormal->SetReferenceMode(FbxLayerElement::eDirect);
	
	// Create UV for Diffuse channel.
	FbxLayerElementUV* UVDiffuseLayer = FbxLayerElementUV::Create(Mesh, "DiffuseUV");
	UVDiffuseLayer->SetMappingMode(FbxLayerElement::eByControlPoint);
	UVDiffuseLayer->SetReferenceMode(FbxLayerElement::eDirect);
	Layer->SetUVs(UVDiffuseLayer, FbxLayerElement::eTextureDiffuse);
	
	for (uint32 VertexIdx = 0; VertexIdx < VertCount; ++VertexIdx)
	{
		FModelVertex& Vertex = Model->VertexBuffer.Vertices[VertexIdx];
		FVector Normal = (FVector) Vertex.TangentZ;

		// If the vertex is outside of the world extent, snap it to the origin.  The faces associated with
		// these vertices will be removed before exporting.  We leave the snapped vertex in the buffer so
		// we won't have to deal with reindexing everything.
		FVector FinalVertexPos = Vertex.Position;
		if( FMath::Abs( Vertex.Position.X ) > BiasedHalfWorldExtent ||
			FMath::Abs( Vertex.Position.Y ) > BiasedHalfWorldExtent ||
			FMath::Abs( Vertex.Position.Z ) > BiasedHalfWorldExtent )
		{
			FinalVertexPos = FVector::ZeroVector;
		}

		ControlPoints[VertexIdx] = FbxVector4(FinalVertexPos.X, -FinalVertexPos.Y, FinalVertexPos.Z);
		FbxVector4 FbxNormal = FbxVector4(Normal.X, -Normal.Y, Normal.Z);
		FbxAMatrix NodeMatrix;
		FbxVector4 Trans = Node->LclTranslation.Get();
		NodeMatrix.SetT(FbxVector4(Trans[0], Trans[1], Trans[2]));
		FbxVector4 Rot = Node->LclRotation.Get();
		NodeMatrix.SetR(FbxVector4(Rot[0], Rot[1], Rot[2]));
		NodeMatrix.SetS(Node->LclScaling.Get());
		FbxNormal = NodeMatrix.MultT(FbxNormal);
		FbxNormal.Normalize();
		LayerElementNormal->GetDirectArray().Add(FbxNormal);
		
		// update the index array of the UVs that map the texture to the face
		UVDiffuseLayer->GetDirectArray().Add(FbxVector2(Vertex.TexCoord.X, -Vertex.TexCoord.Y));
	}
	
	Layer->SetNormals(LayerElementNormal);
	Layer->SetUVs(UVDiffuseLayer);
	
	FbxLayerElementMaterial* MatLayer = FbxLayerElementMaterial::Create(Mesh, "");
	MatLayer->SetMappingMode(FbxLayerElement::eByPolygon);
	MatLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);
	Layer->SetMaterials(MatLayer);
	
	//Make sure the Index buffer is accessible.
	
	for (auto MaterialIterator = Model->MaterialIndexBuffers.CreateIterator(); MaterialIterator; ++MaterialIterator)
	{
		BeginReleaseResource(MaterialIterator.Value());
	}
	FlushRenderingCommands();

	// Create the materials and the per-material tesselation structures.
	for (auto MaterialIterator = Model->MaterialIndexBuffers.CreateIterator(); MaterialIterator; ++MaterialIterator)
	{
		UMaterialInterface* MaterialInterface = MaterialIterator.Key();
		FRawIndexBuffer16or32& IndexBuffer = *MaterialIterator.Value();
		int32 IndexCount = IndexBuffer.Indices.Num();
		if (IndexCount < 3) continue;
		
		// Are NULL materials okay?
		int32 MaterialIndex = -1;
		FbxSurfaceMaterial* FbxMaterial;
		if (MaterialInterface != NULL && MaterialInterface->GetMaterial() != NULL)
		{
			FbxMaterial = ExportMaterial(MaterialInterface->GetMaterial());
		}
		else
		{
			// Set default material
			FbxMaterial = CreateDefaultMaterial();
		}
		MaterialIndex = Node->AddMaterial(FbxMaterial);

		// Create the Fbx polygons set.

		// Retrieve and fill in the index buffer.
		const int32 TriangleCount = IndexCount / 3;
		for( int32 TriangleIdx = 0; TriangleIdx < TriangleCount; ++TriangleIdx )
		{
			bool bSkipTriangle = false;

			for( int32 IndexIdx = 0; IndexIdx < 3; ++IndexIdx )
			{
				// Skip triangles that belong to BSP geometry close to the world extent, since its probably
				// the automatically-added-brush for new levels.  The vertices will be left in the buffer (unreferenced)
				FVector VertexPos = Model->VertexBuffer.Vertices[ IndexBuffer.Indices[ TriangleIdx * 3 + IndexIdx ] ].Position;
				if( FMath::Abs( VertexPos.X ) > BiasedHalfWorldExtent ||
					FMath::Abs( VertexPos.Y ) > BiasedHalfWorldExtent ||
					FMath::Abs( VertexPos.Z ) > BiasedHalfWorldExtent )
				{
					bSkipTriangle = true;
					break;
				}
			}

			if( !bSkipTriangle )
			{
				// all faces of the cube have the same texture
				Mesh->BeginPolygon(MaterialIndex);
				for( int32 IndexIdx = 0; IndexIdx < 3; ++IndexIdx )
				{
					// Control point index
					Mesh->AddPolygon(IndexBuffer.Indices[ TriangleIdx * 3 + IndexIdx ]);

				}
				Mesh->EndPolygon ();
			}
		}

		BeginInitResource(&IndexBuffer);
	}
	
	FlushRenderingCommands();

	Node->SetNodeAttribute(Mesh);
}

void FFbxExporter::ExportStaticMesh( AActor* Actor, UStaticMeshComponent* StaticMeshComponent, AMatineeActor* InMatineeActor )
{
	if (Scene == NULL || Actor == NULL || StaticMeshComponent == NULL)
	{ 
		return;
	}

	// Retrieve the static mesh rendering information at the correct LOD level.
	UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh;
	if (StaticMesh == NULL || !StaticMesh->HasValidRenderData())
	{
		return;
	}
	int32 LODIndex = StaticMeshComponent->ForcedLodModel;
	FStaticMeshLODResources& RenderMesh = StaticMesh->GetLODForExport(LODIndex);

	FString FbxNodeName = GetActorNodeName(Actor, InMatineeActor);

	FColorVertexBuffer* ColorBuffer = NULL;
	
	if (LODIndex != INDEX_NONE && LODIndex < StaticMeshComponent->LODData.Num())
	{
		ColorBuffer = StaticMeshComponent->LODData[LODIndex].OverrideVertexColors;
	}

	FbxNode* FbxActor = ExportActor( Actor, InMatineeActor );
	ExportStaticMeshToFbx(StaticMesh, RenderMesh, *FbxNodeName, FbxActor);
}

struct FBSPExportData
{
	FRawMesh Mesh;
	TArray<UMaterialInterface*> Materials;
	uint32 NumVerts;
	uint32 NumFaces;
	uint32 CurrentVertAddIndex;
	uint32 CurrentFaceAddIndex;
	bool bInitialised;

	FBSPExportData()
		:NumVerts(0)
		,NumFaces(0)
		,CurrentVertAddIndex(0)
		,CurrentFaceAddIndex(0)
		,bInitialised(false)
	{

	}
};

void FFbxExporter::ExportBSP( UModel* Model, bool bSelectedOnly )
{
	TMap< ABrush*, FBSPExportData > BrushToMeshMap;
	TArray<UMaterialInterface*> AllMaterials;

	for(int32 NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
	{
		FBspNode& Node = Model->Nodes[NodeIndex];
		if( Node.NumVertices >= 3 )
		{
			FBspSurf& Surf = Model->Surfs[Node.iSurf];
		
			ABrush* BrushActor = Surf.Actor;

			if( (Surf.PolyFlags & PF_Selected) || !bSelectedOnly || (BrushActor && BrushActor->IsSelected() ) )
			{
				FBSPExportData& Data = BrushToMeshMap.FindOrAdd( BrushActor );

				Data.NumVerts += Node.NumVertices;
				Data.NumFaces += Node.NumVertices-2;
			}
		}
	}
	
	for(int32 NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
	{
		FBspNode& Node = Model->Nodes[NodeIndex];
		FBspSurf& Surf = Model->Surfs[Node.iSurf];

		ABrush* BrushActor = Surf.Actor;

		if( (Surf.PolyFlags & PF_Selected) || !bSelectedOnly || (BrushActor && BrushActor->IsSelected() ) )
		{
			FPoly Poly;
			GEditor->polyFindMaster( Model, Node.iSurf, Poly );

			FBSPExportData* ExportData = BrushToMeshMap.Find( BrushActor );
			if( NULL == ExportData )
			{
				UE_LOG(LogFbx, Fatal, TEXT("Error in FBX export of BSP."));
				return;
			}

			TArray<UMaterialInterface*>& Materials = ExportData->Materials;
			FRawMesh& Mesh = ExportData->Mesh;

			//Pre-allocate space for this mesh.
			if( !ExportData->bInitialised )
			{
				ExportData->bInitialised = true;
				Mesh.VertexPositions.Empty();
				Mesh.VertexPositions.AddUninitialized(ExportData->NumVerts);

				Mesh.FaceMaterialIndices.Empty();
				Mesh.FaceMaterialIndices.AddUninitialized(ExportData->NumFaces);
				Mesh.FaceSmoothingMasks.Empty();
				Mesh.FaceSmoothingMasks.AddUninitialized(ExportData->NumFaces);
				
				uint32 NumWedges = ExportData->NumFaces*3;
				Mesh.WedgeIndices.Empty();
				Mesh.WedgeIndices.AddUninitialized(NumWedges);
				Mesh.WedgeTexCoords[0].Empty();
				Mesh.WedgeTexCoords[0].AddUninitialized(NumWedges);
				Mesh.WedgeColors.Empty();
				Mesh.WedgeColors.AddUninitialized(NumWedges);
				Mesh.WedgeTangentZ.Empty();
				Mesh.WedgeTangentZ.AddUninitialized(NumWedges);
			}
			
			UMaterialInterface*	Material = Poly.Material;

			AllMaterials.AddUnique(Material);

			int32 MaterialIndex = ExportData->Materials.AddUnique(Material);

			const FVector& TextureBase = Model->Points[Surf.pBase];
			const FVector& TextureX = Model->Vectors[Surf.vTextureU];
			const FVector& TextureY = Model->Vectors[Surf.vTextureV];
			const FVector& Normal = Model->Vectors[Surf.vNormal];

			int32 StartIndex=ExportData->CurrentVertAddIndex;
	
			int32 VertexIndex = 0;
			for( ; VertexIndex < Node.NumVertices ; VertexIndex++ )
			{					
				const FVert& Vert = Model->Verts[Node.iVertPool + VertexIndex];
				const FVector& Vertex = Model->Points[Vert.pVertex];
				Mesh.VertexPositions[ExportData->CurrentVertAddIndex+VertexIndex] = Vertex;
			}
			ExportData->CurrentVertAddIndex += VertexIndex;

			for( int32 StartVertexIndex = 1 ; StartVertexIndex < Node.NumVertices - 1 ; StartVertexIndex++ )
			{
 				// These map the node's vertices to the 3 triangle indices to triangulate the convex polygon.
 				int32 TriVertIndices[3] = {	Node.iVertPool + StartVertexIndex + 1,
 					Node.iVertPool + StartVertexIndex,
 					Node.iVertPool };
				
				int32 WedgeIndices[3] = {	StartIndex + StartVertexIndex + 1 ,
					 						StartIndex + StartVertexIndex,
					 						StartIndex };

				Mesh.FaceMaterialIndices[ExportData->CurrentFaceAddIndex] = MaterialIndex;
				Mesh.FaceSmoothingMasks[ExportData->CurrentFaceAddIndex] =  ( 1 << ( Node.iSurf % 32 ) );

				for(uint32 WedgeIndex = 0; WedgeIndex < 3; WedgeIndex++)
				{
					const FVert& Vert = Model->Verts[TriVertIndices[WedgeIndex]];
					const FVector& Vertex = Model->Points[Vert.pVertex];

					float U = ((Vertex - TextureBase) | TextureX) / UModel::GetGlobalBSPTexelScale();
					float V = ((Vertex - TextureBase) | TextureY) / UModel::GetGlobalBSPTexelScale();

					uint32 RealWedgeIndex = ( ExportData->CurrentFaceAddIndex * 3 ) + WedgeIndex;

					Mesh.WedgeIndices[RealWedgeIndex] = WedgeIndices[WedgeIndex];
					Mesh.WedgeTexCoords[0][RealWedgeIndex] = FVector2D(U,V);
					//This is not exported when exporting the whole level via ExportModel so leaving out here for now. 
					//Mesh.WedgeTexCoords[1][RealWedgeIndex] = Vert.ShadowTexCoord;
					Mesh.WedgeColors[RealWedgeIndex] = FColor(255,255,255,255);
					Mesh.WedgeTangentZ[RealWedgeIndex] = Normal;
				}

				++ExportData->CurrentFaceAddIndex;
			}
		}
	}

	for( TMap< ABrush*, FBSPExportData >::TIterator It(BrushToMeshMap); It; ++It )
	{
		if( It.Value().Mesh.VertexPositions.Num() )
		{
			UStaticMesh* NewMesh = CreateStaticMesh( It.Value().Mesh, It.Value().Materials, GetTransientPackage(), It.Key()->GetFName() );

			ExportStaticMesh( NewMesh, &AllMaterials );
		}
	}
}

void FFbxExporter::ExportStaticMesh( UStaticMesh* StaticMesh, const TArray<UMaterialInterface*>* MaterialOrder )
{
	if (Scene == NULL || StaticMesh == NULL || !StaticMesh->HasValidRenderData()) return;
	FString MeshName;
	StaticMesh->GetName(MeshName);
	FStaticMeshLODResources& RenderMesh = StaticMesh->GetLODForExport(0);
	FbxNode* MeshNode = FbxNode::Create(Scene, TCHAR_TO_ANSI(*MeshName));
	Scene->GetRootNode()->AddChild(MeshNode);
	ExportStaticMeshToFbx(StaticMesh, RenderMesh, *MeshName, MeshNode, -1, NULL, MaterialOrder );
}

void FFbxExporter::ExportStaticMeshLightMap( UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannel )
{
	if (Scene == NULL || StaticMesh == NULL || !StaticMesh->HasValidRenderData()) return;

	FString MeshName;
	StaticMesh->GetName(MeshName);
	FStaticMeshLODResources& RenderMesh = StaticMesh->GetLODForExport(LODIndex);
	FbxNode* MeshNode = FbxNode::Create(Scene, TCHAR_TO_ANSI(*MeshName));
	Scene->GetRootNode()->AddChild(MeshNode);
	ExportStaticMeshToFbx(StaticMesh, RenderMesh, *MeshName, MeshNode, UVChannel);
}

void FFbxExporter::ExportSkeletalMesh( USkeletalMesh* SkeletalMesh )
{
	if (Scene == NULL || SkeletalMesh == NULL) return;

	FString MeshName;
	SkeletalMesh->GetName(MeshName);

	FbxNode* MeshNode = FbxNode::Create(Scene, TCHAR_TO_ANSI(*MeshName));
	Scene->GetRootNode()->AddChild(MeshNode);

	ExportSkeletalMeshToFbx(*SkeletalMesh, *MeshName, MeshNode);
}

void FFbxExporter::ExportSkeletalMesh( AActor* Actor, USkeletalMeshComponent* SkeletalMeshComponent )
{
	if (Scene == NULL || Actor == NULL || SkeletalMeshComponent == NULL) return;

	// Retrieve the skeletal mesh rendering information.
	USkeletalMesh* SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;

	FString FbxNodeName = GetActorNodeName(Actor, NULL);

	FbxNode* FbxActorNode = ExportActor( Actor, NULL );
	ExportSkeletalMeshToFbx(*SkeletalMesh, *FbxNodeName, FbxActorNode);
}

FbxSurfaceMaterial* FFbxExporter::CreateDefaultMaterial()
{
	FbxSurfaceMaterial* FbxMaterial = Scene->GetMaterial("Fbx Default Material");
	
	if (!FbxMaterial)
	{
		FbxMaterial = FbxSurfaceLambert::Create(Scene, "Fbx Default Material");
		((FbxSurfaceLambert*)FbxMaterial)->Diffuse.Set(FbxDouble3(0.72, 0.72, 0.72));
	}
	
	return FbxMaterial;
}

void FFbxExporter::ExportLandscape(ALandscapeProxy* Actor, bool bSelectedOnly)
{
	if (Scene == NULL || Actor == NULL)
	{ 
		return;
	}

	AMatineeActor* InMatineeActor = NULL;

	FString FbxNodeName = GetActorNodeName(Actor, InMatineeActor);

	FbxNode* FbxActor = ExportActor(Actor, InMatineeActor, true);
	ExportLandscapeToFbx(Actor, *FbxNodeName, FbxActor, bSelectedOnly);
}

FbxDouble3 SetMaterialComponent(FColorMaterialInput& MatInput)
{
	FColor FinalColor;
	
	if (MatInput.Expression)
	{
		if (Cast<UMaterialExpressionConstant>(MatInput.Expression))
		{
			UMaterialExpressionConstant* Expr = Cast<UMaterialExpressionConstant>(MatInput.Expression);
			FinalColor = FColor(Expr->R);
		}
		else if (Cast<UMaterialExpressionVectorParameter>(MatInput.Expression))
		{
			UMaterialExpressionVectorParameter* Expr = Cast<UMaterialExpressionVectorParameter>(MatInput.Expression);
			FinalColor = Expr->DefaultValue;
		}
		else if (Cast<UMaterialExpressionConstant3Vector>(MatInput.Expression))
		{
			UMaterialExpressionConstant3Vector* Expr = Cast<UMaterialExpressionConstant3Vector>(MatInput.Expression);
			FinalColor.R = Expr->Constant.R;
			FinalColor.G = Expr->Constant.G;
			FinalColor.B = Expr->Constant.B;
		}
		else if (Cast<UMaterialExpressionConstant4Vector>(MatInput.Expression))
		{
			UMaterialExpressionConstant4Vector* Expr = Cast<UMaterialExpressionConstant4Vector>(MatInput.Expression);
			FinalColor.R = Expr->Constant.R;
			FinalColor.G = Expr->Constant.G;
			FinalColor.B = Expr->Constant.B;
			//FinalColor.A = Expr->A;
		}
		else if (Cast<UMaterialExpressionConstant2Vector>(MatInput.Expression))
		{
			UMaterialExpressionConstant2Vector* Expr = Cast<UMaterialExpressionConstant2Vector>(MatInput.Expression);
			FinalColor.R = Expr->R;
			FinalColor.G = Expr->G;
			FinalColor.B = 0;
		}
		else
		{
			FinalColor.R = MatInput.Constant.R / 128.0;
			FinalColor.G = MatInput.Constant.G / 128.0;
			FinalColor.B = MatInput.Constant.B / 128.0;
		}
	}
	else
	{
		FinalColor.R = MatInput.Constant.R / 128.0;
		FinalColor.G = MatInput.Constant.G / 128.0;
		FinalColor.B = MatInput.Constant.B / 128.0;
	}
	
	return FbxDouble3(FinalColor.R, FinalColor.G, FinalColor.B);
}

/**
* Exports the profile_COMMON information for a material.
*/
FbxSurfaceMaterial* FFbxExporter::ExportMaterial(UMaterial* Material)
{
	if (Scene == NULL || Material == NULL) return NULL;
	
	// Verify that this material has not already been exported:
	if (FbxMaterials.Find(Material))
	{
		return *FbxMaterials.Find(Material);
	}

	// Create the Fbx material
	FbxSurfaceMaterial* FbxMaterial = NULL;
	
	// Set the lighting model
	if (Material->GetLightingModel() == MLM_DefaultLit)
	{
		FbxMaterial = FbxSurfacePhong::Create(Scene, TCHAR_TO_ANSI(*Material->GetName()));
		((FbxSurfacePhong*)FbxMaterial)->Specular.Set(SetMaterialComponent(Material->SpecularColor));
		//((FbxSurfacePhong*)FbxMaterial)->Shininess.Set(Material->SpecularPower.Constant);
	}
	else // if (Material->LightingModel == MLM_Unlit)
	{
		FbxMaterial = FbxSurfaceLambert::Create(Scene, TCHAR_TO_ANSI(*Material->GetName()));
	}
	
	((FbxSurfaceLambert*)FbxMaterial)->Emissive.Set(SetMaterialComponent(Material->EmissiveColor));
	((FbxSurfaceLambert*)FbxMaterial)->Diffuse.Set(SetMaterialComponent(Material->DiffuseColor));
	((FbxSurfaceLambert*)FbxMaterial)->TransparencyFactor.Set(Material->Opacity.Constant);

	// Fill in the profile_COMMON effect with the material information.
	// TODO: Look for textures/constants in the Material expressions...
	
	FbxMaterials.Add(Material, FbxMaterial);
	
	return FbxMaterial;
}


/**
 * Exports the given Matinee sequence information into a FBX document.
 */
void FFbxExporter::ExportMatinee(AMatineeActor* InMatineeActor)
{
	if (InMatineeActor == NULL || Scene == NULL) return;

	// If the Matinee editor is not open, we need to initialize the sequence.
	//bool InitializeMatinee = InMatineeActor->MatineeData == NULL;
	//if (InitializeMatinee)
	//{
	//	InMatineeActor->InitInterp();
	//}

	// Iterate over the Matinee data groups and export the known tracks
	int32 GroupCount = InMatineeActor->GroupInst.Num();
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		UInterpGroupInst* Group = InMatineeActor->GroupInst[GroupIndex];
		AActor* Actor = Group->GetGroupActor();
		if (Group->Group == NULL || Actor == NULL) continue;

		// Look for the class-type of the actor.
		if (Actor->IsA(ACameraActor::StaticClass()))
		{
			ExportCamera( (ACameraActor*) Actor, InMatineeActor );
		}

		FbxNode* FbxActor = ExportActor( Actor, InMatineeActor );

		// Look for the tracks that we currently support
		int32 TrackCount = FMath::Min(Group->TrackInst.Num(), Group->Group->InterpTracks.Num());
		for (int32 TrackIndex = 0; TrackIndex < TrackCount; ++TrackIndex)
		{
			UInterpTrackInst* TrackInst = Group->TrackInst[TrackIndex];
			UInterpTrack* Track = Group->Group->InterpTracks[TrackIndex];
			if (TrackInst->IsA(UInterpTrackInstMove::StaticClass()) && Track->IsA(UInterpTrackMove::StaticClass()))
			{
				UInterpTrackInstMove* MoveTrackInst = (UInterpTrackInstMove*) TrackInst;
				UInterpTrackMove* MoveTrack = (UInterpTrackMove*) Track;
				ExportMatineeTrackMove(FbxActor, MoveTrackInst, MoveTrack, InMatineeActor->MatineeData->InterpLength);
			}
			else if (TrackInst->IsA(UInterpTrackInstFloatProp::StaticClass()) && Track->IsA(UInterpTrackFloatProp::StaticClass()))
			{
				UInterpTrackInstFloatProp* PropertyTrackInst = (UInterpTrackInstFloatProp*) TrackInst;
				UInterpTrackFloatProp* PropertyTrack = (UInterpTrackFloatProp*) Track;
				ExportMatineeTrackFloatProp(FbxActor, PropertyTrack);
			}
		}
	}

	//if (InitializeMatinee)
	//{
	//	InMatineeActor->TermInterp();
	//}

	DefaultCamera = NULL;
}


/**
 * Exports a scene node with the placement indicated by a given actor.
 * This scene node will always have two transformations: one translation vector and one Euler rotation.
 */
FbxNode* FFbxExporter::ExportActor(AActor* Actor, AMatineeActor* InMatineeActor, bool bExportComponents )
{
	// Verify that this actor isn't already exported, create a structure for it
	// and buffer it.
	FbxNode* ActorNode = FindActor(Actor);
	if (ActorNode == NULL)
	{
		FString FbxNodeName = GetActorNodeName(Actor, InMatineeActor);

		// See if a node with this name was already found
		// if so add and increment the number on the end of it
		int32 *NodeIndex = FbxNodeNameToIndexMap.Find( FbxNodeName );
		if( NodeIndex )
		{
			FbxNodeName = FString::Printf( TEXT("%s%d"), *FbxNodeName, *NodeIndex );
			++(*NodeIndex);
		}
		else
		{
			FbxNodeNameToIndexMap.Add( FbxNodeName, 1 );	
		}

		ActorNode = FbxNode::Create(Scene, TCHAR_TO_ANSI(*FbxNodeName));
		Scene->GetRootNode()->AddChild(ActorNode);

		FbxActors.Add(Actor, ActorNode);

		// Set the default position of the actor on the transforms
		// The transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
		ActorNode->LclTranslation.Set(Converter.ConvertToFbxPos(Actor->GetActorLocation()));
		ActorNode->LclRotation.Set(Converter.ConvertToFbxRot(Actor->GetActorRotation().Euler()));
		const FVector DrawScale3D = Actor->GetRootComponent() ? Actor->GetRootComponent()->RelativeScale3D : FVector(1.f,1.f,1.f);
		ActorNode->LclScaling.Set(Converter.ConvertToFbxScale(DrawScale3D));
	
		// For cameras and lights: always add a Y-pivot rotation to get the correct coordinate system.
		if (Actor->IsA(ACameraActor::StaticClass()) || Actor->IsA(ALight::StaticClass()))
		{
			FString FbxPivotNodeName = GetActorNodeName(Actor, NULL);

			if (FbxPivotNodeName == FbxNodeName)
			{
				FbxPivotNodeName += ANSI_TO_TCHAR("_pivot");
			}

			FbxNode* PivotNode = FbxNode::Create(Scene, TCHAR_TO_ANSI(*FbxPivotNodeName));
			PivotNode->LclRotation.Set(FbxVector4(90, 0, -90));

			if (Actor->IsA(ACameraActor::StaticClass()))
			{
				PivotNode->SetPostRotation(FbxNode::eSourcePivot, FbxVector4(0, -90, 0));
			}
			else if (Actor->IsA(ALight::StaticClass()))
			{
				PivotNode->SetPostRotation(FbxNode::eSourcePivot, FbxVector4(-90, 0, 0));
			}
			ActorNode->AddChild(PivotNode);

			ActorNode = PivotNode;
		}

		if( bExportComponents )
		{
			TArray<UMeshComponent*> MeshComponents;
			Actor->GetComponents(MeshComponents);

			TArray<UActorComponent*> ComponentsToExport;
			for( int32 ComponentIndex = 0; ComponentIndex < MeshComponents.Num(); ++ComponentIndex )
			{
				UMeshComponent* Component = MeshComponents[ComponentIndex];

				UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>( Component );
				USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>( Component );

				if( StaticMeshComp && StaticMeshComp->StaticMesh )
				{
					ComponentsToExport.Add( StaticMeshComp );
				}
				else if( SkelMeshComp && SkelMeshComp->SkeletalMesh )
				{
					ComponentsToExport.Add( SkelMeshComp );
				}
			}


			for( int32 CompIndex = 0; CompIndex < ComponentsToExport.Num(); ++CompIndex )
			{
				UActorComponent* Component = ComponentsToExport[CompIndex];

				FbxNode* ExportNode = ActorNode;
				if( ComponentsToExport.Num() > 1 )
				{
					USceneComponent* SceneComp = CastChecked<USceneComponent>( Component );

					// This actor has multiple components
					// create a child node under the actor for each component
					FbxNode* CompNode = FbxNode::Create(Scene, TCHAR_TO_ANSI(*Component->GetName()));

					if( SceneComp != Actor->GetRootComponent() )
					{
						// Transform is relative to the root component
						const FTransform RelativeTransform = SceneComp->GetComponentToWorld().GetRelativeTransform(Actor->GetTransform());
						CompNode->LclTranslation.Set(Converter.ConvertToFbxPos(RelativeTransform.GetTranslation()));
						CompNode->LclRotation.Set(Converter.ConvertToFbxRot(RelativeTransform.GetRotation().Euler()));
						CompNode->LclScaling.Set(Converter.ConvertToFbxScale(RelativeTransform.GetScale3D()));
					}

					ActorNode->AddChild(CompNode);

					ExportNode = CompNode;
				}

				UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>( Component );
				USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>( Component );

				if (StaticMeshComp && StaticMeshComp->StaticMesh)
				{
					int32 LODIndex = StaticMeshComp->ForcedLodModel;
					FStaticMeshLODResources& RenderMesh = StaticMeshComp->StaticMesh->GetLODForExport(LODIndex);

					if (USplineMeshComponent* SplineMeshComp = Cast<USplineMeshComponent>(StaticMeshComp))
					{
						ExportSplineMeshToFbx(SplineMeshComp, RenderMesh, *SplineMeshComp->GetName(), ExportNode);
					}
					else
					{
						ExportStaticMeshToFbx(StaticMeshComp->StaticMesh, RenderMesh, *StaticMeshComp->GetName(), ExportNode);
					}
				}
				else if (SkelMeshComp && SkelMeshComp->SkeletalMesh)
				{
					ExportSkeletalMeshToFbx( *SkelMeshComp->SkeletalMesh, *SkelMeshComp->GetName(), ExportNode );
				}
			}
		}
		
	}

	return ActorNode;
}

/**
 * Exports the Matinee movement track into the FBX animation library.
 */
void FFbxExporter::ExportMatineeTrackMove(FbxNode* FbxActor, UInterpTrackInstMove* MoveTrackInst, UInterpTrackMove* MoveTrack, float InterpLength)
{
	if (FbxActor == NULL || MoveTrack == NULL) return;
	
	// For the Y and Z angular rotations, we need to invert the relative animation frames,
	// While keeping the standard angles constant.

	if (MoveTrack != NULL)
	{
		FbxAnimLayer* BaseLayer = AnimStack->GetMember<FbxAnimLayer>(0);
		FbxAnimCurve* Curve;

		bool bPosCurve = true;
		if( MoveTrack->SubTracks.Num() == 0 )
		{
			// Translation;
			FbxActor->LclTranslation.GetCurveNode(BaseLayer, true);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportAnimatedVector(Curve, "X", MoveTrack, MoveTrackInst, bPosCurve, 0, false, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportAnimatedVector(Curve, "Y", MoveTrack, MoveTrackInst, bPosCurve, 1, true, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportAnimatedVector(Curve, "Z", MoveTrack, MoveTrackInst, bPosCurve, 2, false, InterpLength);

			// Rotation
			FbxActor->LclRotation.GetCurveNode(BaseLayer, true);
			bPosCurve = false;

			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportAnimatedVector(Curve, "X", MoveTrack, MoveTrackInst, bPosCurve, 0, false, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportAnimatedVector(Curve, "Y", MoveTrack, MoveTrackInst, bPosCurve, 1, true, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportAnimatedVector(Curve, "Z", MoveTrack, MoveTrackInst, bPosCurve, 2, true, InterpLength);
		}
		else
		{
			// Translation;
			FbxActor->LclTranslation.GetCurveNode(BaseLayer, true);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportMoveSubTrack(Curve, "X", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[0]), MoveTrackInst, bPosCurve, 0, false, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportMoveSubTrack(Curve, "Y", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[1]), MoveTrackInst, bPosCurve, 1, true, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportMoveSubTrack(Curve, "Z", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[2]), MoveTrackInst, bPosCurve, 2, false, InterpLength);

			// Rotation
			FbxActor->LclRotation.GetCurveNode(BaseLayer, true);
			bPosCurve = false;

			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportMoveSubTrack(Curve, "X", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[3]), MoveTrackInst, bPosCurve, 0, false, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportMoveSubTrack(Curve, "Y", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[4]), MoveTrackInst, bPosCurve, 1, true, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportMoveSubTrack(Curve, "Z", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[5]), MoveTrackInst, bPosCurve, 2, true, InterpLength);
		}
	}
}

/**
 * Exports the Matinee float property track into the FBX animation library.
 */
void FFbxExporter::ExportMatineeTrackFloatProp(FbxNode* FbxActor, UInterpTrackFloatProp* PropTrack)
{
	if (FbxActor == NULL || PropTrack == NULL) return;
	
	FbxNodeAttribute* FbxNodeAttr = NULL;
	// camera and light is appended on the fbx pivot node
	if( FbxActor->GetChild(0) )
	{
		FbxNodeAttr = ((FbxNode*)FbxActor->GetChild(0))->GetNodeAttribute();

		if (FbxNodeAttr == NULL) return;
	}
	
	FbxProperty Property;
	FString PropertyName = PropTrack->PropertyName.ToString();
	bool IsFoV = false;
	// most properties are created as user property, only FOV of camera in FBX supports animation
	if (PropertyName == "Intensity")
	{
		Property = FbxActor->FindProperty("UE_Intensity", false);
	}
	else if (PropertyName == "FalloffExponent")
	{
		Property = FbxActor->FindProperty("UE_FalloffExponent", false);
	}
	else if (PropertyName == "AttenuationRadius")
	{
		Property = FbxActor->FindProperty("UE_Radius", false);
	}
	else if (PropertyName == "FOVAngle" && FbxNodeAttr )
	{
		Property = ((FbxCamera*)FbxNodeAttr)->FocalLength;
		IsFoV = true;
	}
	else if (PropertyName == "AspectRatio")
	{
		Property = FbxActor->FindProperty("UE_AspectRatio", false);
	}
	else if (PropertyName == "MotionBlur_Amount")
	{
		Property = FbxActor->FindProperty("UE_MotionBlur_Amount", false);
	}

	if (Property != 0)
	{
		ExportAnimatedFloat(&Property, &PropTrack->FloatTrack, IsFoV);
	}
}

void ConvertInterpToFBX(uint8 UnrealInterpMode, FbxAnimCurveDef::EInterpolationType& Interpolation, FbxAnimCurveDef::ETangentMode& Tangent)
{
	switch(UnrealInterpMode)
	{
	case CIM_Linear:
		Interpolation = FbxAnimCurveDef::eInterpolationLinear;
		Tangent = FbxAnimCurveDef::eTangentUser;
		break;
	case CIM_CurveAuto:
		Interpolation = FbxAnimCurveDef::eInterpolationCubic;
		Tangent = FbxAnimCurveDef::eTangentAuto;
		break;
	case CIM_Constant:
		Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		Tangent = (FbxAnimCurveDef::ETangentMode)FbxAnimCurveDef::eConstantStandard;
		break;
	case CIM_CurveUser:
		Interpolation = FbxAnimCurveDef::eInterpolationCubic;
		Tangent = FbxAnimCurveDef::eTangentUser;
		break;
	case CIM_CurveBreak:
		Interpolation = FbxAnimCurveDef::eInterpolationCubic;
		Tangent = (FbxAnimCurveDef::ETangentMode) FbxAnimCurveDef::eTangentBreak;
		break;
	case CIM_CurveAutoClamped:
		Interpolation = FbxAnimCurveDef::eInterpolationCubic;
		Tangent = (FbxAnimCurveDef::ETangentMode) (FbxAnimCurveDef::eTangentAuto | FbxAnimCurveDef::eTangentGenericClamp);
		break;
	case CIM_Unknown:  // ???
		FbxAnimCurveDef::EInterpolationType Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		FbxAnimCurveDef::ETangentMode Tangent = FbxAnimCurveDef::eTangentAuto;
		break;
	}
}


// float-float comparison that allows for a certain error in the floating point values
// due to floating-point operations never being exact.
static bool IsEquivalent(float a, float b, float Tolerance = KINDA_SMALL_NUMBER)
{
	return (a - b) > -Tolerance && (a - b) < Tolerance;
}

// Set the default FPS to 30 because the SetupMatinee MEL script sets up Maya this way.
const float FFbxExporter::BakeTransformsFPS = DEFAULT_SAMPLERATE;

/**
 * Exports a given interpolation curve into the FBX animation curve.
 */
void FFbxExporter::ExportAnimatedVector(FbxAnimCurve* FbxCurve, const char* ChannelName, UInterpTrackMove* MoveTrack, UInterpTrackInstMove* MoveTrackInst, bool bPosCurve, int32 CurveIndex, bool bNegative, float InterpLength)
{
	if (Scene == NULL) return;
	
	FInterpCurveVector* Curve = bPosCurve ? &MoveTrack->PosTrack : &MoveTrack->EulerTrack;

	if (Curve == NULL || CurveIndex >= 3) return;

#define FLT_TOLERANCE 0.000001

	// Determine how many key frames we are exporting. If the user wants to export a key every 
	// frame, calculate this number. Otherwise, use the number of keys the user created. 
	int32 KeyCount = bBakeKeys ? (InterpLength * BakeTransformsFPS) + Curve->Points.Num() : Curve->Points.Num();

	// Write out the key times from the curve to the FBX curve.
	TArray<float> KeyTimes;
	for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		// The Unreal engine allows you to place more than one key at one time value:
		// displace the extra keys. This assumes that Unreal's keys are always ordered.
		float KeyTime = bBakeKeys ? (KeyIndex * InterpLength) / KeyCount : Curve->Points[KeyIndex].InVal;
		if (KeyTimes.Num() && KeyTime < KeyTimes[KeyIndex-1] + FLT_TOLERANCE)
		{
			KeyTime = KeyTimes[KeyIndex-1] + 0.01f; // Add 1 millisecond to the timing of this key.
		}
		KeyTimes.Add(KeyTime);
	}

	// Write out the key values from the curve to the FBX curve.
	FbxCurve->KeyModifyBegin();
	for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		// First, convert the output value to the correct coordinate system, if we need that.  For movement
		// track keys that are in a local coordinate system (IMF_RelativeToInitial), we need to transform
		// the keys to world space first
		FVector FinalOutVec;
		{
			FVector KeyPosition;
			FRotator KeyRotation;

			// If we are baking trnasforms, ask the movement track what are transforms are at the given time.
			if( bBakeKeys )
			{
				MoveTrack->GetKeyTransformAtTime(MoveTrackInst, KeyTimes[KeyIndex], KeyPosition, KeyRotation);
			}
			// Else, this information is already present in the position and rotation tracks stored on the movement track.
			else
			{
				KeyPosition = MoveTrack->PosTrack.Points[KeyIndex].OutVal;
				KeyRotation = FRotator( FQuat::MakeFromEuler(MoveTrack->EulerTrack.Points[KeyIndex].OutVal) );
			}

			FVector WorldSpacePos;
			FRotator WorldSpaceRotator;
			MoveTrack->ComputeWorldSpaceKeyTransform(
				MoveTrackInst,
				KeyPosition,
				KeyRotation,
				WorldSpacePos,			// Out
				WorldSpaceRotator );	// Out

			if( bPosCurve )
			{
				FinalOutVec = WorldSpacePos;
			}
			else
			{
				FinalOutVec = WorldSpaceRotator.Euler();
			}
		}

		float KeyTime = KeyTimes[KeyIndex];
		float OutValue = (CurveIndex == 0) ? FinalOutVec.X : (CurveIndex == 1) ? FinalOutVec.Y : FinalOutVec.Z;
		float FbxKeyValue = bNegative ? -OutValue : OutValue;
		
		// Add a new key to the FBX curve
		FbxTime Time;
		FbxAnimCurveKey FbxKey;
		Time.SetSecondDouble((float)KeyTime);
		int FbxKeyIndex = FbxCurve->KeyAdd(Time);
		

		FbxAnimCurveDef::EInterpolationType Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		FbxAnimCurveDef::ETangentMode Tangent = FbxAnimCurveDef::eTangentAuto;
		
		if( !bBakeKeys )
		{
			ConvertInterpToFBX(Curve->Points[KeyIndex].InterpMode, Interpolation, Tangent);
		}

		if (bBakeKeys || Interpolation != FbxAnimCurveDef::eInterpolationCubic)
		{
			FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent);
		}
		else
		{
			FInterpCurvePoint<FVector>& Key = Curve->Points[KeyIndex];

			// Setup tangents for bezier curves. Avoid this for keys created from baking 
			// transforms since there is no tangent info created for these types of keys. 
			if( Interpolation == FbxAnimCurveDef::eInterpolationCubic )
			{
				float OutTangentValue = (CurveIndex == 0) ? Key.LeaveTangent.X : (CurveIndex == 1) ? Key.LeaveTangent.Y : Key.LeaveTangent.Z;
				float OutTangentX = (KeyIndex < KeyCount - 1) ? (KeyTimes[KeyIndex + 1] - KeyTime) / 3.0f : 0.333f;
				if (IsEquivalent(OutTangentX, KeyTime))
				{
					OutTangentX = 0.00333f; // 1/3rd of a millisecond.
				}
				float OutTangentY = OutTangentValue / 3.0f;
				float RightTangent =  OutTangentY / OutTangentX ;
				
				float NextLeftTangent = 0;
				
				if (KeyIndex < KeyCount - 1)
				{
					FInterpCurvePoint<FVector>& NextKey = Curve->Points[KeyIndex + 1];
					float NextInTangentValue = (CurveIndex == 0) ? NextKey.ArriveTangent.X : (CurveIndex == 1) ? NextKey.ArriveTangent.Y : NextKey.ArriveTangent.Z;
					float NextInTangentX;
					NextInTangentX = (KeyTimes[KeyIndex + 1] - KeyTimes[KeyIndex]) / 3.0f;
					float NextInTangentY = NextInTangentValue / 3.0f;
					NextLeftTangent =  NextInTangentY / NextInTangentX ;
				}

				FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent, RightTangent, NextLeftTangent );
			}
		}
	}
	FbxCurve->KeyModifyEnd();
}

void FFbxExporter::ExportMoveSubTrack(FbxAnimCurve* FbxCurve, const ANSICHAR* ChannelName, UInterpTrackMoveAxis* SubTrack, UInterpTrackInstMove* MoveTrackInst, bool bPosCurve, int32 CurveIndex, bool bNegative, float InterpLength)
{
	if (Scene == NULL || FbxCurve == NULL) return;

	FInterpCurveFloat* Curve = &SubTrack->FloatTrack;
	UInterpTrackMove* ParentTrack = CastChecked<UInterpTrackMove>( SubTrack->GetOuter() );

#define FLT_TOLERANCE 0.000001

	// Determine how many key frames we are exporting. If the user wants to export a key every 
	// frame, calculate this number. Otherwise, use the number of keys the user created. 
	int32 KeyCount = bBakeKeys ? (InterpLength * BakeTransformsFPS) + Curve->Points.Num() : Curve->Points.Num();

	// Write out the key times from the curve to the FBX curve.
	TArray<float> KeyTimes;
	for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		FInterpCurvePoint<float>& Key = Curve->Points[KeyIndex];

		// The Unreal engine allows you to place more than one key at one time value:
		// displace the extra keys. This assumes that Unreal's keys are always ordered.
		float KeyTime = bBakeKeys ? (KeyIndex * InterpLength) / KeyCount : Key.InVal;
		if (KeyTimes.Num() && KeyTime < KeyTimes[KeyIndex-1] + FLT_TOLERANCE)
		{
			KeyTime = KeyTimes[KeyIndex-1] + 0.01f; // Add 1 millisecond to the timing of this key.
		}
		KeyTimes.Add(KeyTime);
	}

	// Write out the key values from the curve to the FBX curve.
	FbxCurve->KeyModifyBegin();
	for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		// First, convert the output value to the correct coordinate system, if we need that.  For movement
		// track keys that are in a local coordinate system (IMF_RelativeToInitial), we need to transform
		// the keys to world space first
		FVector FinalOutVec;
		{
			FVector KeyPosition;
			FRotator KeyRotation;

			ParentTrack->GetKeyTransformAtTime(MoveTrackInst, KeyTimes[KeyIndex], KeyPosition, KeyRotation);
		
			FVector WorldSpacePos;
			FRotator WorldSpaceRotator;
			ParentTrack->ComputeWorldSpaceKeyTransform(
				MoveTrackInst,
				KeyPosition,
				KeyRotation,
				WorldSpacePos,			// Out
				WorldSpaceRotator );	// Out

			if( bPosCurve )
			{
				FinalOutVec = WorldSpacePos;
			}
			else
			{
				FinalOutVec = WorldSpaceRotator.Euler();
			}
		}

		float KeyTime = KeyTimes[KeyIndex];
		float OutValue = (CurveIndex == 0) ? FinalOutVec.X : (CurveIndex == 1) ? FinalOutVec.Y : FinalOutVec.Z;
		float FbxKeyValue = bNegative ? -OutValue : OutValue;

		FInterpCurvePoint<float>& Key = Curve->Points[KeyIndex];

		// Add a new key to the FBX curve
		FbxTime Time;
		FbxAnimCurveKey FbxKey;
		Time.SetSecondDouble((float)KeyTime);
		int FbxKeyIndex = FbxCurve->KeyAdd(Time);

		FbxAnimCurveDef::EInterpolationType Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		FbxAnimCurveDef::ETangentMode Tangent = FbxAnimCurveDef::eTangentAuto;
		ConvertInterpToFBX(Key.InterpMode, Interpolation, Tangent);

		if (bBakeKeys || Interpolation != FbxAnimCurveDef::eInterpolationCubic)
		{
			FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent);
		}
		else
		{
			// Setup tangents for bezier curves. Avoid this for keys created from baking 
			// transforms since there is no tangent info created for these types of keys. 
			if( Interpolation == FbxAnimCurveDef::eInterpolationCubic )
			{
				float OutTangentValue = Key.LeaveTangent;
				float OutTangentX = (KeyIndex < KeyCount - 1) ? (KeyTimes[KeyIndex + 1] - KeyTime) / 3.0f : 0.333f;
				if (IsEquivalent(OutTangentX, KeyTime))
				{
					OutTangentX = 0.00333f; // 1/3rd of a millisecond.
				}
				float OutTangentY = OutTangentValue / 3.0f;
				float RightTangent =  OutTangentY / OutTangentX ;

				float NextLeftTangent = 0;

				if (KeyIndex < KeyCount - 1)
				{
					FInterpCurvePoint<float>& NextKey = Curve->Points[KeyIndex + 1];
					float NextInTangentValue =  Key.LeaveTangent;
					float NextInTangentX;
					NextInTangentX = (KeyTimes[KeyIndex + 1] - KeyTimes[KeyIndex]) / 3.0f;
					float NextInTangentY = NextInTangentValue / 3.0f;
					NextLeftTangent =  NextInTangentY / NextInTangentX ;
				}

				FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent, RightTangent, NextLeftTangent );
			}
		}
	}
	FbxCurve->KeyModifyEnd();
}

void FFbxExporter::ExportAnimatedFloat(FbxProperty* FbxProperty, FInterpCurveFloat* Curve, bool IsCameraFoV)
{
	if (FbxProperty == NULL || Curve == NULL) return;

	// do not export an empty anim curve
	if (Curve->Points.Num() == 0) return;

	FbxAnimCurve* AnimCurve = FbxAnimCurve::Create(Scene, "");
	FbxAnimCurveNode* CurveNode = FbxProperty->GetCurveNode(true);
	if (!CurveNode)
	{
		return;
	}
	CurveNode->SetChannelValue<double>(0U, Curve->Points[0].OutVal);
	CurveNode->ConnectToChannel(AnimCurve, 0U);

	// Write out the key times from the curve to the FBX curve.
	int32 KeyCount = Curve->Points.Num();
	TArray<float> KeyTimes;
	for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		FInterpCurvePoint<float>& Key = Curve->Points[KeyIndex];

		// The Unreal engine allows you to place more than one key at one time value:
		// displace the extra keys. This assumes that Unreal's keys are always ordered.
		float KeyTime = Key.InVal;
		if (KeyTimes.Num() && KeyTime < KeyTimes[KeyIndex-1] + FLT_TOLERANCE)
		{
			KeyTime = KeyTimes[KeyIndex-1] + 0.01f; // Add 1 millisecond to the timing of this key.
		}
		KeyTimes.Add(KeyTime);
	}

	// Write out the key values from the curve to the FBX curve.
	AnimCurve->KeyModifyBegin();
	for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		FInterpCurvePoint<float>& Key = Curve->Points[KeyIndex];
		float KeyTime = KeyTimes[KeyIndex];
		
		// Add a new key to the FBX curve
		FbxTime Time;
		FbxAnimCurveKey FbxKey;
		Time.SetSecondDouble((float)KeyTime);
		int FbxKeyIndex = AnimCurve->KeyAdd(Time);
		float OutVal = (IsCameraFoV && DefaultCamera)? DefaultCamera->ComputeFocalLength(Key.OutVal): (float)Key.OutVal;

		FbxAnimCurveDef::EInterpolationType Interpolation = FbxAnimCurveDef::eInterpolationConstant;
		FbxAnimCurveDef::ETangentMode Tangent = FbxAnimCurveDef::eTangentAuto;
		ConvertInterpToFBX(Key.InterpMode, Interpolation, Tangent);
		
		if (Interpolation != FbxAnimCurveDef::eInterpolationCubic)
		{
			AnimCurve->KeySet(FbxKeyIndex, Time, OutVal, Interpolation, Tangent);
		}
		else
		{
			// Setup tangents for bezier curves.
			float OutTangentX = (KeyIndex < KeyCount - 1) ? (KeyTimes[KeyIndex + 1] - KeyTime) / 3.0f : 0.333f;
			float OutTangentY = Key.LeaveTangent / 3.0f;
			float RightTangent =  OutTangentY / OutTangentX ;

			float NextLeftTangent = 0;

			if (KeyIndex < KeyCount - 1)
			{
				FInterpCurvePoint<float>& NextKey = Curve->Points[KeyIndex + 1];
				float NextInTangentX;
				NextInTangentX = (KeyTimes[KeyIndex + 1] - KeyTimes[KeyIndex]) / 3.0f;
				float NextInTangentY = NextKey.ArriveTangent / 3.0f;
				NextLeftTangent =  NextInTangentY / NextInTangentX ;
			}

			AnimCurve->KeySet(FbxKeyIndex, Time, OutVal, Interpolation, Tangent, RightTangent, NextLeftTangent );

		}
	}
	AnimCurve->KeyModifyEnd();
}

/**
 * Finds the given actor in the already-exported list of structures
 */
FbxNode* FFbxExporter::FindActor(AActor* Actor)
{
	if (FbxActors.Find(Actor))
	{
		return *FbxActors.Find(Actor);
	}
	else
	{
		return NULL;
	}
}

/**
 * Determines the UVS to weld when exporting a Static Mesh
 * 
 * @param VertRemap		Index of each UV (out)
 * @param UniqueVerts	
 */
void DetermineUVsToWeld(TArray<int32>& VertRemap, TArray<int32>& UniqueVerts, FStaticMeshVertexBuffer& VertexBuffer, int32 TexCoordSourceIndex)
{
	const int32 VertexCount = VertexBuffer.GetNumVertices();

	// Maps unreal verts to reduced list of verts
	VertRemap.Empty(VertexCount);
	VertRemap.AddUninitialized(VertexCount);

	// List of Unreal Verts to keep
	UniqueVerts.Empty(VertexCount);

	// Combine matching verts using hashed search to maintain good performance
	TMap<FVector2D,int32> HashedVerts;
	for(int32 Vertex=0; Vertex < VertexCount; Vertex++)
	{
		const FVector2D& PositionA = VertexBuffer.GetVertexUV(Vertex,TexCoordSourceIndex);
		const int32* FoundIndex = HashedVerts.Find(PositionA);
		if ( !FoundIndex )
		{
			int32 NewIndex = UniqueVerts.Add(Vertex);
			VertRemap[Vertex] = NewIndex;
			HashedVerts.Add(PositionA, NewIndex);
		}
		else
		{
			VertRemap[Vertex] = *FoundIndex;
		}
	}
}

void DetermineVertsToWeld(TArray<int32>& VertRemap, TArray<int32>& UniqueVerts, FStaticMeshLODResources& RenderMesh)
{
	const int32 VertexCount = RenderMesh.VertexBuffer.GetNumVertices();

	// Maps unreal verts to reduced list of verts 
	VertRemap.Empty(VertexCount);
	VertRemap.AddUninitialized(VertexCount);

	// List of Unreal Verts to keep
	UniqueVerts.Empty(VertexCount);

	// Combine matching verts using hashed search to maintain good performance
	TMap<FVector,int32> HashedVerts;
	for(int32 a=0; a < VertexCount; a++)
	{
		const FVector& PositionA = RenderMesh.PositionVertexBuffer.VertexPosition(a);
		const int32* FoundIndex = HashedVerts.Find(PositionA);
		if ( !FoundIndex )
		{
			int32 NewIndex = UniqueVerts.Add(a);
			VertRemap[a] = NewIndex;
			HashedVerts.Add(PositionA, NewIndex);
		}
		else
		{
			VertRemap[a] = *FoundIndex;
		}
	}
}

/**
 * Exports a static mesh
 * @param RenderMesh	The static mesh render data to export
 * @param MeshName		The name of the mesh for the FBX file
 * @param FbxActor		The fbx node representing the mesh
 * @param LightmapUVChannel Optional UV channel to export
 * @param ColorBuffer	Vertex color overrides to export
 * @param MaterialOrderOverride	Optional ordering of materials to set up correct material ID's across multiple meshes being export such as BSP surfaces which share common materials. Should be used sparingly
 */
FbxNode* FFbxExporter::ExportStaticMeshToFbx(UStaticMesh* StaticMesh, FStaticMeshLODResources& RenderMesh, const TCHAR* MeshName, FbxNode* FbxActor, int32 LightmapUVChannel, FColorVertexBuffer* ColorBuffer, const TArray<UMaterialInterface*>* MaterialOrderOverride )
{
	// Verify the integrity of the static mesh.
	if (RenderMesh.VertexBuffer.GetNumVertices() == 0)
	{
		return NULL;
	}

	if (RenderMesh.Sections.Num() == 0)
	{
		return NULL;
	}

	// Remaps an Unreal vert to final reduced vertex list
	TArray<int32> VertRemap;
	TArray<int32> UniqueVerts;

	if ( bStaticMeshExportUnWeldedVerts == false )
	{
		// Weld verts
		DetermineVertsToWeld(VertRemap, UniqueVerts, RenderMesh);
	}
	else
	{
		// Do not weld verts
		VertRemap.Add(RenderMesh.VertexBuffer.GetNumVertices());
		for(int32 i=0; i < VertRemap.Num(); i++)
		{
			VertRemap[i] = i;
		}
		UniqueVerts = VertRemap;
	}

	FbxMesh* Mesh = FbxMesh::Create(Scene, TCHAR_TO_ANSI(MeshName));

	// Create and fill in the vertex position data source.
	// The position vertices are duplicated, for some reason, retrieve only the first half vertices.
	const int32 VertexCount = VertRemap.Num();
	const int32 PolygonsCount = RenderMesh.Sections.Num();
	
	Mesh->InitControlPoints(UniqueVerts.Num());

	FbxVector4* ControlPoints = Mesh->GetControlPoints();
	for (int32 PosIndex = 0; PosIndex < UniqueVerts.Num(); ++PosIndex)
	{
		int32 UnrealPosIndex = UniqueVerts[PosIndex];
		FVector Position = RenderMesh.PositionVertexBuffer.VertexPosition(UnrealPosIndex);
		ControlPoints[PosIndex] = FbxVector4(Position.X, -Position.Y, Position.Z);
	}
	
	// Set the normals on Layer 0.
	FbxLayer* Layer = Mesh->GetLayer(0);
	if (Layer == NULL)
	{
		Mesh->CreateLayer();
		Layer = Mesh->GetLayer(0);
	}

	// Build list of Indices re-used multiple times to lookup Normals, UVs, other per face vertex information
	TArray<uint32> Indices;
	for (int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex)
	{
		FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
		FStaticMeshSection& Polygons = RenderMesh.Sections[PolygonsIndex];
		const uint32 TriangleCount = Polygons.NumTriangles;
		for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			for (uint32 PointIndex = 0; PointIndex < 3; PointIndex++)
			{
				uint32 UnrealVertIndex = RawIndices[Polygons.FirstIndex + ((TriangleIndex * 3) + PointIndex)];
				Indices.Add(UnrealVertIndex);
			}
		}
	}

	// Create and fill in the per-face-vertex normal data source.
	// We extract the Z-tangent and drop the X/Y-tangents which are also stored in the render mesh.
	FbxLayerElementNormal* LayerElementNormal= FbxLayerElementNormal::Create(Mesh, "");

	// Set 3 normals per triangle instead of storing normals on positional control points
	LayerElementNormal->SetMappingMode(FbxLayerElement::eByPolygonVertex);

	// Set the normal values for every polygon vertex.
	LayerElementNormal->SetReferenceMode(FbxLayerElement::eDirect);

	TArray<FbxVector4> FbxNormals;
	FbxNormals.AddUninitialized(VertexCount);
	for (int32 NormalIndex = 0; NormalIndex < VertexCount; ++NormalIndex)
	{
		FVector Normal = (FVector) (RenderMesh.VertexBuffer.VertexTangentZ(NormalIndex));
		FbxVector4& FbxNormal = FbxNormals[NormalIndex];
		FbxNormal = FbxVector4(Normal.X, -Normal.Y, Normal.Z);
		FbxNormal.Normalize();
	}

	// Add one normal per each face index (3 per triangle)
	for (int32 i=0; i < Indices.Num(); i++)
	{
		uint32 UnrealVertIndex = Indices[i];
		LayerElementNormal->GetDirectArray().Add( FbxNormals[UnrealVertIndex] );
	}
	Layer->SetNormals(LayerElementNormal);
	FbxNormals.Empty();

	// Create and fill in the per-face-vertex texture coordinate data source(s).
	// Create UV for Diffuse channel.
	int32 TexCoordSourceCount = (LightmapUVChannel == -1)? RenderMesh.VertexBuffer.GetNumTexCoords(): LightmapUVChannel + 1;
	int32 TexCoordSourceIndex = (LightmapUVChannel == -1)? 0: LightmapUVChannel;
	TCHAR UVChannelName[32] = { 0 };
	for (; TexCoordSourceIndex < TexCoordSourceCount; ++TexCoordSourceIndex)
	{
		FbxLayer* UVsLayer = (LightmapUVChannel == -1)? Mesh->GetLayer(TexCoordSourceIndex): Mesh->GetLayer(0);
		if (UVsLayer == NULL)
		{
			Mesh->CreateLayer();
			UVsLayer = (LightmapUVChannel == -1)? Mesh->GetLayer(TexCoordSourceIndex): Mesh->GetLayer(0);
		}

		if ((LightmapUVChannel >= 0) || ((LightmapUVChannel == -1) && (TexCoordSourceIndex == 1)))
		{
			FCString::Sprintf(UVChannelName, TEXT("LightMapUV"));
		}
		else
		{
			FCString::Sprintf(UVChannelName, TEXT(""));
		}

		FbxLayerElementUV* UVDiffuseLayer = FbxLayerElementUV::Create(Mesh, TCHAR_TO_ANSI(UVChannelName));

		// Note: when eINDEX_TO_DIRECT is used, IndexArray must be 3xTriangle count, DirectArray can be smaller
		UVDiffuseLayer->SetMappingMode(FbxLayerElement::eByPolygonVertex);
		UVDiffuseLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);

		TArray<int32> UvsRemap;
		TArray<int32> UniqueUVs;
		if ( bStaticMeshExportUnWeldedVerts == false )
		{
			// Weld UVs
			DetermineUVsToWeld(UvsRemap, UniqueUVs, RenderMesh.VertexBuffer, TexCoordSourceIndex);
		}
		else
		{
			// Do not weld UVs
			UvsRemap = VertRemap;
			UniqueUVs = UvsRemap;
		}
		
		// Create the texture coordinate data source.
		for (int32 i=0; i < UniqueUVs.Num(); i++)
		{
			int32 UnrealVertIndex = UniqueUVs[i];
			const FVector2D& TexCoord = RenderMesh.VertexBuffer.GetVertexUV(UnrealVertIndex, TexCoordSourceIndex);
			UVDiffuseLayer->GetDirectArray().Add(FbxVector2(TexCoord.X, -TexCoord.Y + 1.0));
		}

		// For each face index, point to a texture uv
		UVDiffuseLayer->GetIndexArray().SetCount( Indices.Num() );
		for (int32 i=0; i < Indices.Num(); i++)
		{
			uint32 UnrealVertIndex = Indices[i];
			int32 NewVertIndex = UvsRemap[UnrealVertIndex];
			UVDiffuseLayer->GetIndexArray().SetAt(i,NewVertIndex);
		}

		UVsLayer->SetUVs(UVDiffuseLayer, FbxLayerElement::eTextureDiffuse);
	}
	
	FbxLayerElementMaterial* MatLayer = FbxLayerElementMaterial::Create(Mesh, "");
	MatLayer->SetMappingMode(FbxLayerElement::eByPolygon);
	MatLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);
	Layer->SetMaterials(MatLayer);
	
	// Keep track of the number of tri's we export
	uint32 AccountedTriangles = 0;
	for (int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex)
	{
		FStaticMeshSection& Polygons = RenderMesh.Sections[PolygonsIndex];
		FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
		UMaterialInterface* Material = StaticMesh->GetMaterial(Polygons.MaterialIndex);

		FbxSurfaceMaterial* FbxMaterial = Material ? ExportMaterial(Material->GetMaterial()) : NULL;
		if (!FbxMaterial)
		{
			FbxMaterial = CreateDefaultMaterial();
		}
		int32 MatIndex = FbxActor->AddMaterial(FbxMaterial);
		
		// Determine the actual material index
		int32 ActualIndex = MatIndex;

		if( MaterialOrderOverride )
		{
			ActualIndex = MaterialOrderOverride->Find( Material );
		}
		// Static meshes contain one triangle list per element.
		// [GLAFORTE] Could it occasionally contain triangle strips? How do I know?
		uint32 TriangleCount = Polygons.NumTriangles;
		
		// Copy over the index buffer into the FBX polygons set.
		for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			Mesh->BeginPolygon(ActualIndex);
			for (uint32 PointIndex = 0; PointIndex < 3; PointIndex++)
			{
				uint32 OriginalUnrealVertIndex = RawIndices[Polygons.FirstIndex + ((TriangleIndex * 3) + PointIndex)];
				int32 RemappedVertIndex = VertRemap[OriginalUnrealVertIndex];
				Mesh->AddPolygon( RemappedVertIndex );
			}
			Mesh->EndPolygon();
		}

		AccountedTriangles += TriangleCount;
	}

#if TODO_FBX
	// Throw a warning if this is a lightmap export and the exported poly count does not match the raw triangle data count
	if (LightmapUVChannel != -1 && AccountedTriangles != RenderMesh.RawTriangles.GetElementCount())
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "StaticMeshEditor_LightmapExportFewerTriangles", "Fewer polygons have been exported than the raw triangle count.  This Lightmapped UV mesh may contain fewer triangles than the destination mesh on import.") );
	}

	// Create and fill in the smoothing data source.
	FbxLayerElementSmoothing* SmoothingInfo = FbxLayerElementSmoothing::Create(Mesh, "");
	SmoothingInfo->SetMappingMode(FbxLayerElement::eByPolygon);
	SmoothingInfo->SetReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElementArrayTemplate<int>& SmoothingArray = SmoothingInfo->GetDirectArray();
	Layer->SetSmoothing(SmoothingInfo);

	// This is broken. We are exporting the render mesh but providing smoothing
	// information from the source mesh. The render triangles are not in the
	// same order. Therefore we should export the raw mesh or not export
	// smoothing group information!
	int32 TriangleCount = RenderMesh.RawTriangles.GetElementCount();
	FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*)RenderMesh.RawTriangles.Lock(LOCK_READ_ONLY);
	for( int32 TriangleIndex = 0 ; TriangleIndex < TriangleCount ; TriangleIndex++ )
	{
		FStaticMeshTriangle* Triangle = (RawTriangleData++);
		
		SmoothingArray.Add(Triangle->SmoothingMask);
	}
	RenderMesh.RawTriangles.Unlock();
#endif // #if TODO_FBX

	// Create and fill in the vertex color data source.
	FColorVertexBuffer* ColorBufferToUse = ColorBuffer? ColorBuffer: &RenderMesh.ColorVertexBuffer;
	uint32 ColorVertexCount = ColorBufferToUse->GetNumVertices();
	
	// Only export vertex colors if they exist
	if (ColorVertexCount > 0)
	{
		FbxLayerElementVertexColor* VertexColor = FbxLayerElementVertexColor::Create(Mesh, "");
		VertexColor->SetMappingMode(FbxLayerElement::eByPolygonVertex);
		VertexColor->SetReferenceMode(FbxLayerElement::eIndexToDirect);
		FbxLayerElementArrayTemplate<FbxColor>& VertexColorArray = VertexColor->GetDirectArray();
		Layer->SetVertexColors(VertexColor);

		for (int32 i=0; i < Indices.Num(); i++)
		{
			FLinearColor VertColor(1.0f, 1.0f, 1.0f);
			uint32 UnrealVertIndex = Indices[i];
			if (UnrealVertIndex < ColorVertexCount)
			{
				VertColor = ColorBufferToUse->VertexColor(UnrealVertIndex).ReinterpretAsLinear();
			}

			VertexColorArray.Add( FbxColor(VertColor.R, VertColor.G, VertColor.B, VertColor.A ));
		}

		VertexColor->GetIndexArray().SetCount( Indices.Num() );
		for (int32 i=0; i < Indices.Num(); i++)
		{
			VertexColor->GetIndexArray().SetAt(i,i);
		}
	}

	FbxActor->SetNodeAttribute(Mesh);

	return FbxActor;
}

static float& GetAxisValue(FVector& InVector, ESplineMeshAxis::Type InAxis)
{
	switch (InAxis)
	{
	case ESplineMeshAxis::X:
		return InVector.X;
	case ESplineMeshAxis::Y:
		return InVector.Y;
	case ESplineMeshAxis::Z:
		return InVector.Z;
	default:
		check(0);
		return InVector.Z;
	}
}

FbxNode* FFbxExporter::ExportSplineMeshToFbx(USplineMeshComponent* SplineMeshComp, FStaticMeshLODResources& RenderMesh, const TCHAR* MeshName, FbxNode* FbxActor)
{
	const UStaticMesh* StaticMesh = SplineMeshComp->StaticMesh;
	check(StaticMesh);

	// Verify the integrity of the static mesh.
	if (RenderMesh.VertexBuffer.GetNumVertices() == 0)
	{
		return NULL;
	}

	if (RenderMesh.Sections.Num() == 0)
	{
		return NULL;
	}

	// Remaps an Unreal vert to final reduced vertex list
	TArray<int32> VertRemap;
	TArray<int32> UniqueVerts;

	if (bStaticMeshExportUnWeldedVerts == false)
	{
		// Weld verts
		DetermineVertsToWeld(VertRemap, UniqueVerts, RenderMesh);
	}
	else
	{
		// Do not weld verts
		VertRemap.Add(RenderMesh.VertexBuffer.GetNumVertices());
		for (int32 i = 0; i < VertRemap.Num(); i++)
		{
			VertRemap[i] = i;
		}
		UniqueVerts = VertRemap;
	}

	FbxMesh* Mesh = FbxMesh::Create(Scene, TCHAR_TO_ANSI(MeshName));

	// Create and fill in the vertex position data source.
	// The position vertices are duplicated, for some reason, retrieve only the first half vertices.
	const int32 VertexCount = VertRemap.Num();
	const int32 PolygonsCount = RenderMesh.Sections.Num();

	Mesh->InitControlPoints(UniqueVerts.Num());

	FbxVector4* ControlPoints = Mesh->GetControlPoints();
	for (int32 PosIndex = 0; PosIndex < UniqueVerts.Num(); ++PosIndex)
	{
		int32 UnrealPosIndex = UniqueVerts[PosIndex];
		FVector Position = RenderMesh.PositionVertexBuffer.VertexPosition(UnrealPosIndex);

		const FTransform SliceTransform = SplineMeshComp->CalcSliceTransform(GetAxisValue(Position, SplineMeshComp->ForwardAxis));
		GetAxisValue(Position, SplineMeshComp->ForwardAxis) = 0;
		Position = SliceTransform.TransformPosition(Position);

		ControlPoints[PosIndex] = FbxVector4(Position.X, -Position.Y, Position.Z);
	}

	// Set the normals on Layer 0.
	FbxLayer* Layer = Mesh->GetLayer(0);
	if (Layer == NULL)
	{
		Mesh->CreateLayer();
		Layer = Mesh->GetLayer(0);
	}

	// Build list of Indices re-used multiple times to lookup Normals, UVs, other per face vertex information
	TArray<uint32> Indices;
	for (int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex)
	{
		FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
		FStaticMeshSection& Polygons = RenderMesh.Sections[PolygonsIndex];
		const uint32 TriangleCount = Polygons.NumTriangles;
		for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			for (uint32 PointIndex = 0; PointIndex < 3; PointIndex++)
			{
				uint32 UnrealVertIndex = RawIndices[Polygons.FirstIndex + ((TriangleIndex * 3) + PointIndex)];
				Indices.Add(UnrealVertIndex);
			}
		}
	}

	// Create and fill in the per-face-vertex normal data source.
	// We extract the Z-tangent and drop the X/Y-tangents which are also stored in the render mesh.
	FbxLayerElementNormal* LayerElementNormal = FbxLayerElementNormal::Create(Mesh, "");

	// Set 3 normals per triangle instead of storing normals on positional control points
	LayerElementNormal->SetMappingMode(FbxLayerElement::eByPolygonVertex);

	// Set the normal values for every polygon vertex.
	LayerElementNormal->SetReferenceMode(FbxLayerElement::eDirect);

	TArray<FbxVector4> FbxNormals;
	FbxNormals.AddUninitialized(VertexCount);
	for (int32 VertIndex = 0; VertIndex < VertexCount; ++VertIndex)
	{
		FVector Position = RenderMesh.PositionVertexBuffer.VertexPosition(VertIndex);
		const FTransform SliceTransform = SplineMeshComp->CalcSliceTransform(GetAxisValue(Position, SplineMeshComp->ForwardAxis));
		FVector Normal = FVector(RenderMesh.VertexBuffer.VertexTangentZ(VertIndex));
		Normal = SliceTransform.TransformVector(Normal);
		FbxVector4& FbxNormal = FbxNormals[VertIndex];
		FbxNormal = FbxVector4(Normal.X, -Normal.Y, Normal.Z);
		FbxNormal.Normalize();
	}

	// Add one normal per each face index (3 per triangle)
	for (uint32 UnrealVertIndex : Indices)
	{
		LayerElementNormal->GetDirectArray().Add(FbxNormals[UnrealVertIndex]);
	}
	Layer->SetNormals(LayerElementNormal);
	FbxNormals.Empty();

	// Create and fill in the per-face-vertex texture coordinate data source(s).
	// Create UV for Diffuse channel.
	int32 TexCoordSourceCount = RenderMesh.VertexBuffer.GetNumTexCoords();
	TCHAR UVChannelName[32] = { 0 };
	for (int32 TexCoordSourceIndex = 0; TexCoordSourceIndex < TexCoordSourceCount; ++TexCoordSourceIndex)
	{
		FbxLayer* UVsLayer = Mesh->GetLayer(TexCoordSourceIndex);
		if (UVsLayer == NULL)
		{
			Mesh->CreateLayer();
			UVsLayer =  Mesh->GetLayer(TexCoordSourceIndex);
		}

		if (TexCoordSourceIndex == 1)
		{
			FCString::Sprintf(UVChannelName, TEXT("LightMapUV"));
		}
		else
		{
			FCString::Sprintf(UVChannelName, TEXT(""));
		}

		FbxLayerElementUV* UVDiffuseLayer = FbxLayerElementUV::Create(Mesh, TCHAR_TO_ANSI(UVChannelName));

		// Note: when eINDEX_TO_DIRECT is used, IndexArray must be 3xTriangle count, DirectArray can be smaller
		UVDiffuseLayer->SetMappingMode(FbxLayerElement::eByPolygonVertex);
		UVDiffuseLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);

		TArray<int32> UvsRemap;
		TArray<int32> UniqueUVs;
		if (bStaticMeshExportUnWeldedVerts == false)
		{
			// Weld UVs
			DetermineUVsToWeld(UvsRemap, UniqueUVs, RenderMesh.VertexBuffer, TexCoordSourceIndex);
		}
		else
		{
			// Do not weld UVs
			UvsRemap = VertRemap;
			UniqueUVs = UvsRemap;
		}

		// Create the texture coordinate data source.
		for (int32 UnrealVertIndex : UniqueUVs)
		{
			const FVector2D& TexCoord = RenderMesh.VertexBuffer.GetVertexUV(UnrealVertIndex, TexCoordSourceIndex);
			UVDiffuseLayer->GetDirectArray().Add(FbxVector2(TexCoord.X, -TexCoord.Y + 1.0));
		}

		// For each face index, point to a texture uv
		UVDiffuseLayer->GetIndexArray().SetCount(Indices.Num());
		for (int32 i = 0; i < Indices.Num(); i++)
		{
			uint32 UnrealVertIndex = Indices[i];
			int32 NewVertIndex = UvsRemap[UnrealVertIndex];
			UVDiffuseLayer->GetIndexArray().SetAt(i, NewVertIndex);
		}

		UVsLayer->SetUVs(UVDiffuseLayer, FbxLayerElement::eTextureDiffuse);
	}

	FbxLayerElementMaterial* MatLayer = FbxLayerElementMaterial::Create(Mesh, "");
	MatLayer->SetMappingMode(FbxLayerElement::eByPolygon);
	MatLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);
	Layer->SetMaterials(MatLayer);

	for (int32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex)
	{
		FStaticMeshSection& Polygons = RenderMesh.Sections[PolygonsIndex];
		FIndexArrayView RawIndices = RenderMesh.IndexBuffer.GetArrayView();
		UMaterialInterface* Material = StaticMesh->GetMaterial(Polygons.MaterialIndex);

		FbxSurfaceMaterial* FbxMaterial = Material ? ExportMaterial(Material->GetMaterial()) : NULL;
		if (!FbxMaterial)
		{
			FbxMaterial = CreateDefaultMaterial();
		}
		int32 MatIndex = FbxActor->AddMaterial(FbxMaterial);

		// Static meshes contain one triangle list per element.
		uint32 TriangleCount = Polygons.NumTriangles;

		// Copy over the index buffer into the FBX polygons set.
		for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			Mesh->BeginPolygon(MatIndex);
			for (uint32 PointIndex = 0; PointIndex < 3; PointIndex++)
			{
				uint32 OriginalUnrealVertIndex = RawIndices[Polygons.FirstIndex + ((TriangleIndex * 3) + PointIndex)];
				int32 RemappedVertIndex = VertRemap[OriginalUnrealVertIndex];
				Mesh->AddPolygon(RemappedVertIndex);
			}
			Mesh->EndPolygon();
		}
	}

#if TODO_FBX
	// This is broken. We are exporting the render mesh but providing smoothing
	// information from the source mesh. The render triangles are not in the
	// same order. Therefore we should export the raw mesh or not export
	// smoothing group information!
	int32 TriangleCount = RenderMesh.RawTriangles.GetElementCount();
	FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*)RenderMesh.RawTriangles.Lock(LOCK_READ_ONLY);
	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; TriangleIndex++)
	{
		FStaticMeshTriangle* Triangle = (RawTriangleData++);

		SmoothingArray.Add(Triangle->SmoothingMask);
	}
	RenderMesh.RawTriangles.Unlock();
#endif // #if TODO_FBX

	// Create and fill in the vertex color data source.
	FColorVertexBuffer* ColorBufferToUse = &RenderMesh.ColorVertexBuffer;
	uint32 ColorVertexCount = ColorBufferToUse->GetNumVertices();

	// Only export vertex colors if they exist
	if (ColorVertexCount > 0)
	{
		FbxLayerElementVertexColor* VertexColor = FbxLayerElementVertexColor::Create(Mesh, "");
		VertexColor->SetMappingMode(FbxLayerElement::eByPolygonVertex);
		VertexColor->SetReferenceMode(FbxLayerElement::eIndexToDirect);
		FbxLayerElementArrayTemplate<FbxColor>& VertexColorArray = VertexColor->GetDirectArray();
		Layer->SetVertexColors(VertexColor);

		for (int32 i = 0; i < Indices.Num(); i++)
		{
			FLinearColor VertColor(1.0f, 1.0f, 1.0f);
			uint32 UnrealVertIndex = Indices[i];
			if (UnrealVertIndex < ColorVertexCount)
			{
				VertColor = ColorBufferToUse->VertexColor(UnrealVertIndex).ReinterpretAsLinear();
			}

			VertexColorArray.Add(FbxColor(VertColor.R, VertColor.G, VertColor.B, VertColor.A));
		}

		VertexColor->GetIndexArray().SetCount(Indices.Num());
		for (int32 i = 0; i < Indices.Num(); i++)
		{
			VertexColor->GetIndexArray().SetAt(i, i);
		}
	}

	FbxActor->SetNodeAttribute(Mesh);

	return FbxActor;
}

/**
 * Exports a Landscape
 */
FbxNode* FFbxExporter::ExportLandscapeToFbx(ALandscapeProxy* Landscape, const TCHAR* MeshName, FbxNode* FbxActor, bool bSelectedOnly)
{
	const ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo(false);
	
	TSet<ULandscapeComponent*> SelectedComponents;
	if (bSelectedOnly && LandscapeInfo)
	{
		SelectedComponents = LandscapeInfo->GetSelectedComponents();
	}

	bSelectedOnly = bSelectedOnly && SelectedComponents.Num() > 0;

	int32 MinX = MAX_int32, MinY = MAX_int32;
	int32 MaxX = MIN_int32, MaxY = MIN_int32;

	// Find range of entire landscape
	for (int32 ComponentIndex = 0; ComponentIndex < Landscape->LandscapeComponents.Num(); ComponentIndex++)
	{
		ULandscapeComponent* Component = Landscape->LandscapeComponents[ComponentIndex];

		if (bSelectedOnly && !SelectedComponents.Contains(Component))
		{
			continue;
		}

		Component->GetComponentExtent(MinX, MinY, MaxX, MaxY);
	}

	FbxMesh* Mesh = FbxMesh::Create(Scene, TCHAR_TO_ANSI(MeshName));

	// Create and fill in the vertex position data source.
	const int32 ComponentSizeQuads = ((Landscape->ComponentSizeQuads + 1) >> Landscape->ExportLOD) - 1;
	const float ScaleFactor = (float)Landscape->ComponentSizeQuads / (float)ComponentSizeQuads;
	const int32 NumComponents = bSelectedOnly ? SelectedComponents.Num() : Landscape->LandscapeComponents.Num();
	const int32 VertexCountPerComponent = FMath::Square(ComponentSizeQuads + 1);
	const int32 VertexCount = NumComponents * VertexCountPerComponent;
	const int32 TriangleCount = NumComponents * FMath::Square(ComponentSizeQuads) * 2;
	
	Mesh->InitControlPoints(VertexCount);

	// Normals and Tangents
	FbxLayerElementNormal* LayerElementNormals= FbxLayerElementNormal::Create(Mesh, "");
	LayerElementNormals->SetMappingMode(FbxLayerElement::eByControlPoint);
	LayerElementNormals->SetReferenceMode(FbxLayerElement::eDirect);

	FbxLayerElementTangent* LayerElementTangents= FbxLayerElementTangent::Create(Mesh, "");
	LayerElementTangents->SetMappingMode(FbxLayerElement::eByControlPoint);
	LayerElementTangents->SetReferenceMode(FbxLayerElement::eDirect);

	FbxLayerElementBinormal* LayerElementBinormals= FbxLayerElementBinormal::Create(Mesh, "");
	LayerElementBinormals->SetMappingMode(FbxLayerElement::eByControlPoint);
	LayerElementBinormals->SetReferenceMode(FbxLayerElement::eDirect);

	// Add Texture UVs (which are simply incremented 1.0 per vertex)
	FbxLayerElementUV* LayerElementTextureUVs = FbxLayerElementUV::Create(Mesh, "TextureUVs");
	LayerElementTextureUVs->SetMappingMode(FbxLayerElement::eByControlPoint);
	LayerElementTextureUVs->SetReferenceMode(FbxLayerElement::eDirect);

	// Add Weightmap UVs (to match up with an exported weightmap, not the original weightmap UVs, which are per-component)
	const FVector2D UVScale = FVector2D(1.0f, 1.0f) / FVector2D((MaxX - MinX) + 1, (MaxY - MinY) + 1);
	FbxLayerElementUV* LayerElementWeightmapUVs = FbxLayerElementUV::Create(Mesh, "WeightmapUVs");
	LayerElementWeightmapUVs->SetMappingMode(FbxLayerElement::eByControlPoint);
	LayerElementWeightmapUVs->SetReferenceMode(FbxLayerElement::eDirect);

	FbxVector4* ControlPoints = Mesh->GetControlPoints();
	FbxLayerElementArrayTemplate<FbxVector4>& Normals = LayerElementNormals->GetDirectArray();
	Normals.Resize(VertexCount);
	FbxLayerElementArrayTemplate<FbxVector4>& Tangents = LayerElementTangents->GetDirectArray();
	Tangents.Resize(VertexCount);
	FbxLayerElementArrayTemplate<FbxVector4>& Binormals = LayerElementBinormals->GetDirectArray();
	Binormals.Resize(VertexCount);
	FbxLayerElementArrayTemplate<FbxVector2>& TextureUVs = LayerElementTextureUVs->GetDirectArray();
	TextureUVs.Resize(VertexCount);
	FbxLayerElementArrayTemplate<FbxVector2>& WeightmapUVs = LayerElementWeightmapUVs->GetDirectArray();
	WeightmapUVs.Resize(VertexCount);

	for (int32 ComponentIndex = 0, SelectedComponentIndex = 0; ComponentIndex < Landscape->LandscapeComponents.Num(); ComponentIndex++)
	{
		ULandscapeComponent* Component = Landscape->LandscapeComponents[ComponentIndex];

		if (bSelectedOnly && !SelectedComponents.Contains(Component))
		{
			continue;
		}

		FLandscapeComponentDataInterface CDI(Component, Landscape->ExportLOD);
		const int32 BaseVertIndex = SelectedComponentIndex++ * VertexCountPerComponent;

		for (int32 VertIndex = 0; VertIndex < VertexCountPerComponent; VertIndex++)
		{
			int32 VertX, VertY;
			CDI.VertexIndexToXY(VertIndex, VertX, VertY);

			FVector Position = CDI.GetLocalVertex(VertX, VertY) + Component->RelativeLocation;
			FbxVector4 FbxPosition = FbxVector4(Position.X, -Position.Y, Position.Z);
			ControlPoints[BaseVertIndex + VertIndex] = FbxPosition;

			FVector Normal, TangentX, TangentY;
			CDI.GetLocalTangentVectors(VertX, VertY, TangentX, TangentY, Normal);
			Normal /= Component->ComponentToWorld.GetScale3D(); Normal.Normalize();
			TangentX /= Component->ComponentToWorld.GetScale3D(); TangentX.Normalize();
			TangentY /= Component->ComponentToWorld.GetScale3D(); TangentY.Normalize();
			FbxVector4 FbxNormal = FbxVector4(Normal.X, -Normal.Y, Normal.Z); FbxNormal.Normalize();
			Normals.SetAt(BaseVertIndex + VertIndex, FbxNormal);
			FbxVector4 FbxTangent = FbxVector4(TangentX.X, -TangentX.Y, TangentX.Z); FbxTangent.Normalize();
			Tangents.SetAt(BaseVertIndex + VertIndex, FbxTangent);
			FbxVector4 FbxBinormal = FbxVector4(TangentY.X, -TangentY.Y, TangentY.Z); FbxBinormal.Normalize();
			Binormals.SetAt(BaseVertIndex + VertIndex, FbxBinormal);

			FVector2D TextureUV = FVector2D(VertX * ScaleFactor + Component->GetSectionBase().X, VertY * ScaleFactor + Component->GetSectionBase().Y);
			FbxVector2 FbxTextureUV = FbxVector2(TextureUV.X, TextureUV.Y);
			TextureUVs.SetAt(BaseVertIndex + VertIndex, FbxTextureUV);

			FVector2D WeightmapUV = (TextureUV - FVector2D(MinX, MinY)) * UVScale;
			FbxVector2 FbxWeightmapUV = FbxVector2(WeightmapUV.X, WeightmapUV.Y);
			WeightmapUVs.SetAt(BaseVertIndex + VertIndex, FbxWeightmapUV);
		}
	}

	FbxLayer* Layer0 = Mesh->GetLayer(0);
	if (Layer0 == NULL)
	{
		Mesh->CreateLayer();
		Layer0 = Mesh->GetLayer(0);
	}

	Layer0->SetNormals(LayerElementNormals);
	Layer0->SetTangents(LayerElementTangents);
	Layer0->SetBinormals(LayerElementBinormals);
	Layer0->SetUVs(LayerElementTextureUVs);
	Layer0->SetUVs(LayerElementWeightmapUVs, FbxLayerElement::eTextureBump);

	// this doesn't seem to work, on import the mesh has no smoothing layer at all
	//FbxLayerElementSmoothing* SmoothingInfo = FbxLayerElementSmoothing::Create(Mesh, "");
	//SmoothingInfo->SetMappingMode(FbxLayerElement::eAllSame);
	//SmoothingInfo->SetReferenceMode(FbxLayerElement::eDirect);
	//FbxLayerElementArrayTemplate<int>& Smoothing = SmoothingInfo->GetDirectArray();
	//Smoothing.Add(0);
	//Layer0->SetSmoothing(SmoothingInfo);

	FbxLayerElementMaterial* LayerElementMaterials = FbxLayerElementMaterial::Create(Mesh, "");
	LayerElementMaterials->SetMappingMode(FbxLayerElement::eAllSame);
	LayerElementMaterials->SetReferenceMode(FbxLayerElement::eIndexToDirect);
	Layer0->SetMaterials(LayerElementMaterials);

	UMaterialInterface* Material = Landscape->GetLandscapeMaterial();
	FbxSurfaceMaterial* FbxMaterial = Material ? ExportMaterial(Material->GetMaterial()) : NULL;
	if (!FbxMaterial)
	{
		FbxMaterial = CreateDefaultMaterial();
	}
	const int32 MaterialIndex = FbxActor->AddMaterial(FbxMaterial);
	LayerElementMaterials->GetIndexArray().Add(MaterialIndex);

	// Copy over the index buffer into the FBX polygons set.
	for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ComponentIndex++)
	{
		int32 BaseVertIndex = ComponentIndex * VertexCountPerComponent;

		for (int32 Y = 0; Y < ComponentSizeQuads; Y++)
		{
			for (int32 X = 0; X < ComponentSizeQuads; X++)
			{
				Mesh->BeginPolygon();
				Mesh->AddPolygon(BaseVertIndex + (X+0) + (Y+0)*(ComponentSizeQuads+1));
				Mesh->AddPolygon(BaseVertIndex + (X+1) + (Y+1)*(ComponentSizeQuads+1));
				Mesh->AddPolygon(BaseVertIndex + (X+1) + (Y+0)*(ComponentSizeQuads+1));
				Mesh->EndPolygon();

				Mesh->BeginPolygon();
				Mesh->AddPolygon(BaseVertIndex + (X+0) + (Y+0)*(ComponentSizeQuads+1));
				Mesh->AddPolygon(BaseVertIndex + (X+0) + (Y+1)*(ComponentSizeQuads+1));
				Mesh->AddPolygon(BaseVertIndex + (X+1) + (Y+1)*(ComponentSizeQuads+1));
				Mesh->EndPolygon();
			}
		}
	}

	FbxActor->SetNodeAttribute(Mesh);

	return FbxActor;
}


} // namespace UnFbx