// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Model.cpp: Unreal model functions
=============================================================================*/

#include "EnginePrivate.h"
#include "MeshBuild.h"

float UModel::BSPTexelScale = 100.0f;

DEFINE_LOG_CATEGORY_STATIC(LogModel, Log, All);

/*-----------------------------------------------------------------------------
	FBspSurf object implementation.
-----------------------------------------------------------------------------*/
#if WITH_EDITOR
/**
 * Returns true if this surface is currently hidden in the editor
 *
 * @return true if this surface is hidden in the editor; false otherwise
 */
bool FBspSurf::IsHiddenEd() const
{
	return bHiddenEdTemporary || bHiddenEdLevel;
}

/**
 * Returns true if this surface is hidden at editor startup
 *
 * @return true if this surface is hidden at editor startup; false otherwise
 */
bool FBspSurf::IsHiddenEdAtStartup() const
{
	return ( ( PolyFlags & PF_HiddenEd ) != 0 ); 
}
#endif

/*-----------------------------------------------------------------------------
	Struct serializers.
-----------------------------------------------------------------------------*/

FArchive& operator<<( FArchive& Ar, FBspSurf& Surf )
{
	Ar << Surf.Material;
	Ar << Surf.PolyFlags;	
	Ar << Surf.pBase << Surf.vNormal;
	Ar << Surf.vTextureU << Surf.vTextureV;
	Ar << Surf.iBrushPoly;
	Ar << Surf.Actor;
	Ar << Surf.Plane;
	Ar << Surf.LightMapScale;
	Ar << Surf.iLightmassIndex;

	// If transacting, we do want to serialize the temporary visibility
	// flags; but not in any other situation
	if ( Ar.IsTransacting() )
	{
		Ar << Surf.bHiddenEdTemporary;
		Ar << Surf.bHiddenEdLevel;
	}

	return Ar;
}

void FBspSurf::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( Material );
	Collector.AddReferencedObject( Actor );
}

FArchive& operator<<( FArchive& Ar, FPoly& Poly )
{
	int32 LegacyNumVertices = Poly.Vertices.Num();
	Ar << Poly.Base << Poly.Normal << Poly.TextureU << Poly.TextureV;
	Ar << Poly.Vertices;
	Ar << Poly.PolyFlags;
	Ar << Poly.Actor << Poly.ItemName;
	Ar << Poly.Material;
	Ar << Poly.iLink << Poly.iBrushPoly;
	Ar << Poly.LightMapScale;
	Ar << Poly.LightmassSettings;
	Ar << Poly.RulesetVariation;
	
	return Ar;
}

FArchive& operator<<( FArchive& Ar, FBspNode& N )
{
	// @warning BulkSerialize: FBSPNode is serialized as memory dump
	// See TArray::BulkSerialize for detailed description of implied limitations.

	// Serialize in the order of variable declaration so the data is compatible with BulkSerialize
	Ar	<< N.Plane;
	Ar	<< N.iVertPool
		<< N.iSurf
		<< N.iVertexIndex
		<< N.ComponentIndex 
		<< N.ComponentNodeIndex
		<< N.ComponentElementIndex;
	
	Ar	<< N.iChild[0]
		<< N.iChild[1]
		<< N.iChild[2]
		<< N.iCollisionBound
		<< N.iZone[0]
		<< N.iZone[1]
		<< N.NumVertices
		<< N.NodeFlags
		<< N.iLeaf[0]
		<< N.iLeaf[1];

	if( Ar.IsLoading() )
	{
		//@warning: this code needs to be in sync with UModel::Serialize as we use bulk serialization.
		N.NodeFlags &= ~(NF_IsNew|NF_IsFront|NF_IsBack);
	}

	return Ar;
}

FArchive& operator<<( FArchive& Ar, FZoneProperties& P )
{
	Ar	<< P.ZoneActor
		<< P.Connectivity
		<< P.Visibility
		<< P.LastRenderTime;
	return Ar;
}

/**
* Serializer
*
* @param Ar - archive to serialize with
* @param V - vertex to serialize
* @return archive that was used
*/
FArchive& operator<<(FArchive& Ar,FModelVertex& V)
{
	Ar << V.Position;
	Ar << V.TangentX;
	Ar << V.TangentZ;	
	Ar << V.TexCoord;
	Ar << V.ShadowTexCoord;	

	return Ar;
}

/*---------------------------------------------------------------------------------------
	UModel object implementation.
---------------------------------------------------------------------------------------*/

void UModel::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	const int32 StripVertexBufferFlag = 1;
	FStripDataFlags StripFlags( Ar, GetOuter() && GetOuter()->IsA(ABrush::StaticClass()) ? StripVertexBufferFlag : FStripDataFlags::None );

	Ar << Bounds;

	Vectors.BulkSerialize( Ar );
	Points.BulkSerialize( Ar );
	Nodes.BulkSerialize( Ar );
	if( Ar.IsLoading() )
	{
		for( int32 NodeIndex=0; NodeIndex<Nodes.Num(); NodeIndex++ )
		{
			Nodes[NodeIndex].NodeFlags &= ~(NF_IsNew|NF_IsFront|NF_IsBack);
		}
	}
	Ar << Surfs;
	Verts.BulkSerialize( Ar );

	if( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_REMOVE_ZONES_FROM_MODEL)
	{
		int32 NumZones;
		Ar << NumSharedSides << NumZones;

		FZoneProperties DummyZones[FBspNode::MAX_ZONES];
		for( int32 i=0; i<NumZones; i++ )
		{
			Ar << DummyZones[i];
		}
	}
	else
	{
		Ar << NumSharedSides;
	}

#if WITH_EDITOR
	Ar << Polys;
	LeafHulls.BulkSerialize( Ar );
	Leaves.BulkSerialize( Ar );
#else
	if(Ar.IsLoading())
	{
		UPolys* DummyPolys;
		Ar << DummyPolys;

		TArray<int32> DummyLeafHulls;
		DummyLeafHulls.BulkSerialize( Ar );

		TArray<FLeaf> DummyLeaves;
		DummyLeaves.BulkSerialize( Ar );
	}
#endif

	Ar << RootOutside << Linked;

	if(Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_REMOVE_ZONES_FROM_MODEL)
	{
		TArray<int32> DummyPortalNodes;
		DummyPortalNodes.BulkSerialize( Ar );
	}

	Ar << NumUniqueVertices; 
	// load/save vertex buffer
	if( StripFlags.IsEditorDataStripped() == false || StripFlags.IsClassDataStripped( StripVertexBufferFlag ) == false )
	{
		Ar << VertexBuffer;
	}

#if WITH_EDITOR
	if(GIsEditor)
	{
		CalculateUniqueVertCount();
	}
#endif // WITH_EDITOR

	// serialize the lighting guid if it's there
	Ar << LightingGuid;

	Ar << LightmassSettings;
}

void UModel::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UModel* This = CastChecked<UModel>(InThis);
#if WITH_EDITOR
	Collector.AddReferencedObject( This->Polys, This );
#endif // WITH_EDITOR
	for( int32 Index = 0; Index < This->Surfs.Num(); Index++ )
	{
		This->Surfs[ Index ].AddReferencedObjects( Collector );
	}
	UObject* NodesOwner = This->Nodes.GetOwner();
	Collector.AddReferencedObject( NodesOwner, This );
	UObject* VertsOwner = This->Verts.GetOwner();
	Collector.AddReferencedObject( VertsOwner, This );
	UObject* VectorsOwner = This->Vectors.GetOwner();
	Collector.AddReferencedObject( VectorsOwner, This );
	UObject* PointsOwner = This->Points.GetOwner();
	Collector.AddReferencedObject( PointsOwner, This );
	UObject* SurfsOwner = This->Surfs.GetOwner();
	Collector.AddReferencedObject( SurfsOwner, This );

	Super::AddReferencedObjects( This, Collector );
}

#if WITH_EDITOR
void UModel::CalculateUniqueVertCount()
{
	NumUniqueVertices = Points.Num();

	if(NumUniqueVertices == 0 && Polys != NULL)
	{
		TArray<FVector> UniquePoints;

		for(int32 PolyIndex(0); PolyIndex < Polys->Element.Num(); ++PolyIndex)
		{
			for(int32 VertIndex(0); VertIndex < Polys->Element[PolyIndex].Vertices.Num(); ++VertIndex)
			{
				bool bAlreadyAdded(false);
				for(int32 UniqueIndex(0); UniqueIndex < UniquePoints.Num(); ++UniqueIndex)
				{
					if(Polys->Element[PolyIndex].Vertices[VertIndex] == UniquePoints[UniqueIndex])
					{
						bAlreadyAdded = true;
						break;
					}
				}

				if(!bAlreadyAdded)
				{
					UniquePoints.Push(Polys->Element[PolyIndex].Vertices[VertIndex]);
				}
			}
		}

		NumUniqueVertices = UniquePoints.Num();
	}
}
#endif // WITH_EDITOR

void UModel::PostLoad()
{
	Super::PostLoad();
	
	if( FApp::CanEverRender() && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		UpdateVertices();
	}

	// If in the editor, initialize each surface to hidden or not depending upon
	// whether the poly flag dictates being hidden at editor startup or not
	if ( GIsEditor )
	{
		for ( TArray<FBspSurf>::TIterator SurfIter( Surfs ); SurfIter; ++SurfIter )
		{
			FBspSurf& CurSurf = *SurfIter;
			CurSurf.bHiddenEdTemporary = ( ( CurSurf.PolyFlags & PF_HiddenEd ) != 0 );
			CurSurf.bHiddenEdLevel = 0;
		}
	}
}

#if WITH_EDITOR
void UModel::PostEditUndo()
{
	InvalidSurfaces = true;

	Super::PostEditUndo();
}

void UModel::ModifySurf( int32 InIndex, bool UpdateMaster )
{
	Surfs.ModifyItem( InIndex );
	FBspSurf& Surf = Surfs[InIndex];
	if( UpdateMaster && Surf.Actor )
	{
		Surf.Actor->Brush->Polys->Element.ModifyItem( Surf.iBrushPoly );
	}
}
void UModel::ModifyAllSurfs( bool UpdateMaster )
{
	for( int32 i=0; i<Surfs.Num(); i++ )
		ModifySurf( i, UpdateMaster );

}
void UModel::ModifySelectedSurfs( bool UpdateMaster )
{
	for( int32 i=0; i<Surfs.Num(); i++ )
		if( Surfs[i].PolyFlags & PF_Selected )
			ModifySurf( i, UpdateMaster );

}

bool UModel::HasSelectedSurfaces() const
{
	for (const auto& Surface : Surfs)
	{
		if (Surface.PolyFlags & PF_Selected)
		{
			return true;
		}
	}

	return false;
}
#endif // WITH_EDITOR

bool UModel::Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
{
#if WITH_EDITOR
	// Also rename the UPolys.
	if (NewOuter && Polys && Polys->GetOuter() == GetOuter())
	{
		if (Polys->Rename(*MakeUniqueObjectName(NewOuter, Polys->GetClass()).ToString(), NewOuter, Flags) == false)
		{
			return false;
		}
	}
#endif // WITH_EDITOR

	return Super::Rename( InName, NewOuter, Flags );
}

/**
 * Called after duplication & serialization and before PostLoad. Used to make sure UModel's FPolys
 * get duplicated as well.
 */
void UModel::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
#if WITH_EDITOR
	if( Polys )
	{
		Polys = CastChecked<UPolys>(StaticDuplicateObject( Polys, this, NULL ));
	}
#endif // WITH_EDITOR
}

void UModel::BeginDestroy()
{
	Super::BeginDestroy();
	BeginReleaseResources();
}

bool UModel::IsReadyForFinishDestroy()
{
	return ReleaseResourcesFence.IsFenceComplete() && Super::IsReadyForFinishDestroy();
}

SIZE_T UModel::GetResourceSize(EResourceSizeMode::Type Mode)
{
	int32 ResourceSize = 0;
	
	// I'm adding extra stuff that haven't been covered by Serialize 
	// I don't have to include VertexFactories (based on Sam Z)
	for(TMap<UMaterialInterface*,TScopedPointer<FRawIndexBuffer16or32> >::TConstIterator IndexBufferIt(MaterialIndexBuffers);IndexBufferIt;++IndexBufferIt)
	{
		const TScopedPointer<FRawIndexBuffer16or32> &IndexBuffer = IndexBufferIt.Value();
		ResourceSize += IndexBuffer->Indices.Num() * sizeof(uint32);
	}
	
	return ResourceSize;
}

#if WITH_EDITOR

IMPLEMENT_INTRINSIC_CLASS(UModel, ENGINE_API, UObject, CORE_API, 
	{
		Class->ClassAddReferencedObjects = &UModel::AddReferencedObjects;
		Class->EmitObjectReference( STRUCT_OFFSET( UModel, Polys ) );
		const uint32 SkipIndexIndex = Class->EmitStructArrayBegin( STRUCT_OFFSET( UModel, Surfs ), sizeof(FBspSurf) );
		Class->EmitObjectReference( STRUCT_OFFSET( FBspSurf, Material ) );
		Class->EmitObjectReference( STRUCT_OFFSET( FBspSurf, Actor ) );
		Class->EmitStructArrayEnd( SkipIndexIndex );
	}
);

#else

IMPLEMENT_INTRINSIC_CLASS(UModel, ENGINE_API, UObject, CORE_API, 
	{
		Class->ClassAddReferencedObjects = &UModel::AddReferencedObjects;
		const uint32 SkipIndexIndex = Class->EmitStructArrayBegin( STRUCT_OFFSET( UModel, Surfs ), sizeof(FBspSurf) );
		Class->EmitObjectReference( STRUCT_OFFSET( FBspSurf, Material ) );
		Class->EmitObjectReference( STRUCT_OFFSET( FBspSurf, Actor ) );
		Class->EmitStructArrayEnd( SkipIndexIndex );
	}
);

#endif // WITH_EDITOR

/*---------------------------------------------------------------------------------------
	UModel implementation.
---------------------------------------------------------------------------------------*/

bool UModel::Modify( bool bAlwaysMarkDirty/*=false*/ )
{
	bool bSavedToTransactionBuffer = Super::Modify(bAlwaysMarkDirty);

	// make a new guid whenever this model changes
	LightingGuid = FGuid::NewGuid();

#if WITH_EDITOR
	// Modify all child objects.
	if( Polys )
	{
		bSavedToTransactionBuffer = Polys->Modify(bAlwaysMarkDirty) || bSavedToTransactionBuffer;
	}
#endif

	return bSavedToTransactionBuffer;
}

//
// Empty the contents of a model.
//
void UModel::EmptyModel( int32 EmptySurfInfo, int32 EmptyPolys )
{
	Nodes			.Empty();
	Verts			.Empty();

#if WITH_EDITOR
	Leaves			.Empty();
	LeafHulls		.Empty();
#endif // WITH_EDITOR

	if( EmptySurfInfo )
	{
		Vectors.Empty();
		Points.Empty();
		Surfs.Empty();
	}

#if WITH_EDITOR
	if( EmptyPolys )
	{
		Polys = new( GetOuter(), NAME_None, RF_Transactional )UPolys(FPostConstructInitializeProperties());
	}
#endif // WITH_EDITOR

	// Init variables.
	NumSharedSides	= 4;
}

//
// Create a new model and allocate all objects needed for it.
//
UModel::UModel( const class FPostConstructInitializeProperties& PCIP,ABrush* Owner, bool InRootOutside )
:	UObject(PCIP)
,	Nodes		( this )
,	Verts		( this )
,	Vectors		( this )
,	Points		( this )
,	Surfs		( this )
,	VertexBuffer( this )
,	LightingGuid( FGuid::NewGuid() )
,	RootOutside	( InRootOutside )
{
	SetFlags( RF_Transactional );
	EmptyModel( 1, 1 );
	if( Owner )
	{
		check(Owner->BrushComponent);
		Owner->Brush = this;
#if WITH_EDITOR
		Owner->InitPosRotScale();
#endif
	}
	if( GIsEditor && !FApp::IsGame() )
	{
		UpdateVertices();
	}
}

#if WITH_EDITOR
void UModel::BuildBound()
{
	if( Polys && Polys->Element.Num() )
	{
		TArray<FVector> NewPoints;
		for( int32 i=0; i<Polys->Element.Num(); i++ )
			for( int32 j=0; j<Polys->Element[i].Vertices.Num(); j++ )
				NewPoints.Add(Polys->Element[i].Vertices[j]);
		Bounds = FBoxSphereBounds( NewPoints.GetTypedData(), NewPoints.Num() );
	}
}

void UModel::Transform( ABrush* Owner )
{
	check(Owner);

	Polys->Element.ModifyAllItems();

	for( int32 i=0; i<Polys->Element.Num(); i++ )
		Polys->Element[ i ].Transform( Owner->GetPrePivot(), Owner->GetActorLocation());

}

void UModel::ShrinkModel()
{
	Vectors		.Shrink();
	Points		.Shrink();
	Verts		.Shrink();
	Nodes		.Shrink();
	Surfs		.Shrink();
	if( Polys     ) Polys    ->Element.Shrink();
	LeafHulls	.Shrink();
}

#endif // WITH_EDITOR

void UModel::BeginReleaseResources()
{
	// Release the index buffers.
	for(TMap<UMaterialInterface*,TScopedPointer<FRawIndexBuffer16or32> >::TIterator IndexBufferIt(MaterialIndexBuffers);IndexBufferIt;++IndexBufferIt)
	{
		BeginReleaseResource(IndexBufferIt.Value());
	}

	// Release the vertex buffer and factory.
	BeginReleaseResource(&VertexBuffer);
	BeginReleaseResource(&VertexFactory);

	// Use a fence to keep track of the release progress.
	ReleaseResourcesFence.BeginFence();
}

void UModel::UpdateVertices()
{
	// Wait for pending resource release commands to execute.
	ReleaseResourcesFence.Wait();

	// Don't initialize brush rendering resources on consoles
	if (!GetOuter() || !GetOuter()->IsA(ABrush::StaticClass()) || !FPlatformProperties::RequiresCookedData())
	{
#if WITH_EDITOR
		// rebuild vertex buffer if the resource array is not static 
		if( GIsEditor && !FApp::IsGame() && !VertexBuffer.Vertices.IsStatic() )
		{	
			int32 NumVertices = 0;

			NumVertices = BuildVertexBuffers();

			// We want to check whenever we build the vertex buffer that we have the
			// appropriate number of verts, but since we no longer serialize the total
			// non-unique vertcount we only do this check when building the buffer.
			check(NumVertices == VertexBuffer.Vertices.Num());	
		}
#endif
		BeginInitResource(&VertexBuffer);
		if( GIsEditor && !FApp::IsGame() )
		{
			// needed since we may call UpdateVertices twice and the first time
			// NumVertices might be 0. 
			BeginUpdateResourceRHI(&VertexBuffer);
		}

		// Set up the vertex factory.
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitModelVertexFactory,
			FLocalVertexFactory*,VertexFactory,&VertexFactory,
			FVertexBuffer*,VertexBuffer,&VertexBuffer,
			{
				FLocalVertexFactory::DataType Data;
				Data.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,Position,VET_Float3);
				Data.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,TangentX,VET_PackedNormal);
				Data.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,TangentZ,VET_PackedNormal);
				Data.TextureCoordinates.Empty();
				Data.TextureCoordinates.Add(STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,TexCoord,VET_Float2));
				Data.LightMapCoordinateComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FModelVertex,ShadowTexCoord,VET_Float2);
				VertexFactory->SetData(Data);
			});
		BeginInitResource(&VertexFactory);
	}
}

/** 
 *	Compute the "center" location of all the verts 
 */
FVector UModel::GetCenter()
{
	FVector Center(0.f);
	uint32 Cnt = 0;
	for(int32 NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
	{
		FBspNode& Node = Nodes[NodeIndex];
		uint32 NumVerts = (Node.NodeFlags & PF_TwoSided) ? Node.NumVertices / 2 : Node.NumVertices;
		for(uint32 VertexIndex = 0;VertexIndex < NumVerts;VertexIndex++)
		{
			const FVert& Vert = Verts[Node.iVertPool + VertexIndex];
			const FVector& Position = Points[Vert.pVertex];
			Center += Position;
			Cnt++;
		}
	}

	if( Cnt > 0 )
	{
		Center /= Cnt;
	}
	
	return Center;
}

#if WITH_EDITOR
/**
* Initialize vertex buffer data from UModel data
* Returns the number of vertices in the vertex buffer.
*/
int32 UModel::BuildVertexBuffers()
{
	// Calculate the size of the vertex buffer and the base vertex index of each node.
	int32 NumVertices = 0;
	for(int32 NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
	{
		FBspNode& Node = Nodes[NodeIndex];
		FBspSurf& Surf = Surfs[Node.iSurf];
		Node.iVertexIndex = NumVertices;
		NumVertices += (Surf.PolyFlags & PF_TwoSided) ? (Node.NumVertices * 2) : Node.NumVertices;
	}

	// size vertex buffer data
	VertexBuffer.Vertices.Empty(NumVertices);
	VertexBuffer.Vertices.AddUninitialized(NumVertices);

	if(NumVertices > 0)
	{
		// Initialize the vertex data
		FModelVertex* DestVertex = (FModelVertex*)VertexBuffer.Vertices.GetData();
		for(int32 NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
		{
			FBspNode& Node = Nodes[NodeIndex];
			FBspSurf& Surf = Surfs[Node.iSurf];
			const FVector& TextureBase = Points[Surf.pBase];
			const FVector& TextureX = Vectors[Surf.vTextureU];
			const FVector& TextureY = Vectors[Surf.vTextureV];

			// Use the texture coordinates and normal to create an orthonormal tangent basis.
			FVector TangentX = TextureX;
			FVector TangentY = TextureY;
			FVector TangentZ = Vectors[Surf.vNormal];
			FVector::CreateOrthonormalBasis(TangentX,TangentY,TangentZ);

			for(uint32 VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
			{
				const FVert& Vert = Verts[Node.iVertPool + VertexIndex];
				const FVector& Position = Points[Vert.pVertex];
				DestVertex->Position = Position;
				DestVertex->TexCoord.X = ((Position - TextureBase) | TextureX) / UModel::GetGlobalBSPTexelScale();
				DestVertex->TexCoord.Y = ((Position - TextureBase) | TextureY) / UModel::GetGlobalBSPTexelScale();
				DestVertex->ShadowTexCoord = Vert.ShadowTexCoord;
				DestVertex->TangentX = TangentX;
				DestVertex->TangentZ = TangentZ;

				// store the sign of the determinant in TangentZ.W
				DestVertex->TangentZ.Vector.W = GetBasisDeterminantSign( TangentX, TangentY, TangentZ ) < 0 ? 0 : 255;

				DestVertex++;
			}

			if(Surf.PolyFlags & PF_TwoSided)
			{
				for(int32 VertexIndex = Node.NumVertices - 1;VertexIndex >= 0;VertexIndex--)
				{
					const FVert& Vert = Verts[Node.iVertPool + VertexIndex];
					const FVector& Position = Points[Vert.pVertex];
					DestVertex->Position = Position;
					DestVertex->TexCoord.X = ((Position - TextureBase) | TextureX) / UModel::GetGlobalBSPTexelScale();
					DestVertex->TexCoord.Y = ((Position - TextureBase) | TextureY) / UModel::GetGlobalBSPTexelScale();
					DestVertex->ShadowTexCoord = Vert.BackfaceShadowTexCoord;
					DestVertex->TangentX = TangentX;
					DestVertex->TangentZ = -TangentZ;

					// store the sign of the determinant in TangentZ.W
					DestVertex->TangentZ.Vector.W = GetBasisDeterminantSign( TangentX, TangentY, -TangentZ ) < 0 ? 0 : 255;

					DestVertex++;
				}
			}
		}
	}

	return NumVertices;
}

#endif

#if WITH_EDITOR

/**
* Clears local (non RHI) data associated with MaterialIndexBuffers
*/
void UModel::ClearLocalMaterialIndexBuffersData()
{
	TMap<UMaterialInterface*,TScopedPointer<FRawIndexBuffer16or32> >::TIterator MaterialIterator(MaterialIndexBuffers);
	for(; MaterialIterator; ++MaterialIterator)
	{
		FRawIndexBuffer16or32& IndexBuffer = *MaterialIterator.Value();
		IndexBuffer.Indices.Empty();
	}
}

#endif // WITH_EDITOR

void UModel::ReleaseVertices()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReleaseModelVertices,
		FModelVertexBuffer*,VertexBuffer,&VertexBuffer,
	{
		VertexBuffer->Vertices.SetAllowCPUAccess(false);
		VertexBuffer->Vertices.Discard();
	});
}