// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "Landscape/LandscapeDataAccess.h"
#include "Landscape/LandscapeRender.h"

namespace
{
	enum PreviewType
	{
		Invalid = -1,
		Both = 0,
		Add = 1,
		Sub = 2,
	};
}


class FLandscapeGizmoMeshRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const float TopHeight;
	const float BottomHeight;
	const UTexture2D* AlphaTexture;
	const FLinearColor ScaleBias;
	const FMatrix WorldToLandscapeMatrix;

	/** Initialization constructor. */
	FLandscapeGizmoMeshRenderProxy(const FMaterialRenderProxy* InParent, const float InTop, const float InBottom, const UTexture2D* InAlphaTexture, const FLinearColor& InScaleBias, const FMatrix& InWorldToLandscapeMatrix)
	:	Parent(InParent)
	,	TopHeight(InTop)
	,	BottomHeight(InBottom)
	,	AlphaTexture(InAlphaTexture)
	,	ScaleBias(InScaleBias)
	,	WorldToLandscapeMatrix(InWorldToLandscapeMatrix)
	{}

	// FMaterialRenderProxy interface.
	virtual const class FMaterial* GetMaterial(ERHIFeatureLevel::Type FeatureLevel) const
	{
		return Parent->GetMaterial(FeatureLevel);
	}
	virtual bool GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("AlphaScaleBias")))
		{
			*OutValue = ScaleBias;
			return true;
		}
		else
		if (ParameterName == FName(TEXT("MatrixRow1")))
		{
			*OutValue = FLinearColor(WorldToLandscapeMatrix.M[0][0], WorldToLandscapeMatrix.M[0][1], WorldToLandscapeMatrix.M[0][2],WorldToLandscapeMatrix.M[0][3]);
			return true;
		}
		else
		if (ParameterName == FName(TEXT("MatrixRow2")))
		{
			*OutValue = FLinearColor(WorldToLandscapeMatrix.M[1][0], WorldToLandscapeMatrix.M[1][1], WorldToLandscapeMatrix.M[1][2],WorldToLandscapeMatrix.M[1][3]);
			return true;
		}
		else
		if (ParameterName == FName(TEXT("MatrixRow3")))
		{
			*OutValue = FLinearColor(WorldToLandscapeMatrix.M[2][0], WorldToLandscapeMatrix.M[2][1], WorldToLandscapeMatrix.M[2][2],WorldToLandscapeMatrix.M[2][3]);
			return true;
		}
		else
		if (ParameterName == FName(TEXT("MatrixRow4")))
		{
			*OutValue = FLinearColor(WorldToLandscapeMatrix.M[3][0], WorldToLandscapeMatrix.M[3][1], WorldToLandscapeMatrix.M[3][2],WorldToLandscapeMatrix.M[3][3]);
			return true;
		}

		return Parent->GetVectorValue(ParameterName, OutValue, Context);
	}
	virtual bool GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("Top")))
		{
			*OutValue = TopHeight;
			return true;
		}
		else if (ParameterName == FName(TEXT("Bottom")))
		{
			*OutValue = BottomHeight;
			return true;
		}
		return Parent->GetScalarValue(ParameterName, OutValue, Context);
	}
	virtual bool GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("AlphaTexture")))
		{
			// FIXME: This needs to return a black texture if AlphaTexture is NULL.
			// Returning NULL will cause the material to use GWhiteTexture.
			*OutValue = AlphaTexture;
			return true;
		}
		return Parent->GetTextureValue(ParameterName, OutValue, Context);
	}
};

/** Represents a LandscapeGizmoRenderingComponent to the scene manager. */
class FLandscapeGizmoRenderSceneProxy : public FPrimitiveSceneProxy
{
public:
	FMatrix MeshRT;
	FVector XAxis, YAxis, Origin;
	FVector FrustumVerts[8];
	float SampleSizeX, SampleSizeY;
	TArray<FVector> SampledPositions;
	TArray<FVector> SampledNormals;
	bool bHeightmapRendering;
	FLandscapeGizmoMeshRenderProxy* HeightmapRenderProxy;
	FMaterialRenderProxy* GizmoRenderProxy;

	FLandscapeGizmoRenderSceneProxy(const ULandscapeGizmoRenderComponent* InComponent):
		FPrimitiveSceneProxy(InComponent),
		bHeightmapRendering(false),
		HeightmapRenderProxy(NULL),
		GizmoRenderProxy(NULL)
	{
#if WITH_EDITORONLY_DATA	
		ALandscapeGizmoActiveActor* Gizmo = Cast<ALandscapeGizmoActiveActor>(InComponent->GetOwner());
		if (Gizmo && Gizmo->GizmoMeshMaterial && Gizmo->GizmoDataMaterial && Gizmo->GetRootComponent())
		{
			ULandscapeInfo* LandscapeInfo = Gizmo->TargetLandscapeInfo;
			if (LandscapeInfo && LandscapeInfo->GetLandscapeProxy())
			{
				SampleSizeX = Gizmo->SampleSizeX;
				SampleSizeY = Gizmo->SampleSizeY;
				bHeightmapRendering = (Gizmo->DataType & LGT_Height);
				FMatrix LToW = LandscapeInfo->GetLandscapeProxy()->LandscapeActorToWorld().ToMatrixWithScale();
				FMatrix WToL = LToW.Inverse();
				FVector BaseLocation = WToL.TransformPosition(Gizmo->GetActorLocation());
				float ScaleXY = LandscapeInfo->DrawScale.X;
				float ScaleZ = LandscapeInfo->DrawScale.Z;
				const float W = Gizmo->GetWidth() / (2 * ScaleXY);
				const float H = Gizmo->GetHeight() / (2 * ScaleXY);
				const float L = Gizmo->GetLength() / ScaleZ;
				FMatrix GizmoRT = FRotationTranslationMatrix(FRotator(0, Gizmo->GetActorRotation().Yaw, 0), FVector(BaseLocation.X, BaseLocation.Y, 0)) * LToW;

				FrustumVerts[0] = Gizmo->FrustumVerts[0] = GizmoRT.TransformPosition(FVector( - W, - H, BaseLocation.Z + L ));
				FrustumVerts[1] = Gizmo->FrustumVerts[1] = GizmoRT.TransformPosition(FVector( + W, - H, BaseLocation.Z + L ));
				FrustumVerts[2] = Gizmo->FrustumVerts[2] = GizmoRT.TransformPosition(FVector( + W, + H, BaseLocation.Z + L ));
				FrustumVerts[3] = Gizmo->FrustumVerts[3] = GizmoRT.TransformPosition(FVector( - W, + H, BaseLocation.Z + L ));

				FrustumVerts[4] = Gizmo->FrustumVerts[4] = GizmoRT.TransformPosition(FVector( - W, - H, BaseLocation.Z ));
				FrustumVerts[5] = Gizmo->FrustumVerts[5] = GizmoRT.TransformPosition(FVector( + W, - H, BaseLocation.Z ));
				FrustumVerts[6] = Gizmo->FrustumVerts[6] = GizmoRT.TransformPosition(FVector( + W, + H, BaseLocation.Z ));
				FrustumVerts[7] = Gizmo->FrustumVerts[7] = GizmoRT.TransformPosition(FVector( - W, + H, BaseLocation.Z ));

				XAxis = GizmoRT.TransformPosition(FVector( + W,	0,		BaseLocation.Z + L ));
				YAxis = GizmoRT.TransformPosition(FVector( 0,	+ H,	BaseLocation.Z + L ));
				Origin = GizmoRT.TransformPosition(FVector( 0,	0,		BaseLocation.Z + L ));

				MeshRT = FTranslationMatrix(FVector(- W + 0.5, - H + 0.5, 0)) * FRotationTranslationMatrix(FRotator(0, Gizmo->GetActorRotation().Yaw, 0), FVector(BaseLocation.X, BaseLocation.Y, 0)) * LToW;
				HeightmapRenderProxy = new FLandscapeGizmoMeshRenderProxy( Gizmo->GizmoMeshMaterial->GetRenderProxy(false), BaseLocation.Z + L, BaseLocation.Z, Gizmo->GizmoTexture, FLinearColor(Gizmo->TextureScale.X, Gizmo->TextureScale.Y, 0, 0), WToL );

				GizmoRenderProxy = (Gizmo->DataType != LGT_None) ? Gizmo->GizmoDataMaterial->GetRenderProxy(false) : Gizmo->GizmoMaterial->GetRenderProxy(false);

				// Cache sampled height
				float ScaleX = Gizmo->GetWidth() / Gizmo->CachedWidth / ScaleXY * Gizmo->CachedScaleXY;
				float ScaleY = Gizmo->GetHeight() / Gizmo->CachedHeight / ScaleXY * Gizmo->CachedScaleXY;
				FScaleMatrix Mat(FVector(ScaleX, ScaleY, L));
				FMatrix NormalM = Mat.Inverse().GetTransposed();

				int32 SamplingSize = Gizmo->SampleSizeX * Gizmo->SampleSizeY;
				SampledPositions.Empty(SamplingSize);
				SampledNormals.Empty(SamplingSize);

				for (int32 Y = 0; Y < Gizmo->SampleSizeY; ++Y)
				{
					for (int32 X = 0; X < Gizmo->SampleSizeX; ++X)
					{
						FVector SampledPos = Gizmo->SampledHeight[X + Y * ALandscapeGizmoActiveActor::DataTexSize];
						SampledPos.X *= ScaleX;
						SampledPos.Y *= ScaleY;
						SampledPos.Z = Gizmo->GetLandscapeHeight(SampledPos.Z);

						FVector SampledNormal = NormalM.TransformVector(Gizmo->SampledNormal[X + Y * ALandscapeGizmoActiveActor::DataTexSize]);
						SampledNormal = SampledNormal.SafeNormal();

						SampledPositions.Add(SampledPos);
						SampledNormals.Add(SampledNormal);
						//MeshBuilder.AddVertex(SampledPos, FVector2D((float)X / (Gizmo->SampleSizeX), (float)Y / (Gizmo->SampleSizeY)), TangentX, SampledNormal^TangentX, SampledNormal, FColor(255, 255, 255) );
					}
				}
			}
		}
#endif
	}

	~FLandscapeGizmoRenderSceneProxy()
	{
		delete HeightmapRenderProxy;
		HeightmapRenderProxy = NULL;
	}

	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View)
	{
		//FMemMark Mark(FMemStack::Get());
#if WITH_EDITOR
		if( GizmoRenderProxy &&  HeightmapRenderProxy )
		{
			// Axis
			PDI->DrawLine( Origin, XAxis, FLinearColor(1, 0, 0), SDPG_World );
			PDI->DrawLine( Origin, YAxis, FLinearColor(0, 1, 0), SDPG_World );

			{
				FDynamicMeshBuilder MeshBuilder;

				MeshBuilder.AddVertex(FrustumVerts[0], FVector2D(0, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[1], FVector2D(1, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[2], FVector2D(1, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[3], FVector2D(0, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

				MeshBuilder.AddVertex(FrustumVerts[4], FVector2D(0, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[5], FVector2D(1, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[6], FVector2D(1, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[7], FVector2D(0, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

				MeshBuilder.AddVertex(FrustumVerts[1], FVector2D(0, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[0], FVector2D(1, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[4], FVector2D(1, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[5], FVector2D(0, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

				MeshBuilder.AddVertex(FrustumVerts[3], FVector2D(0, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[2], FVector2D(1, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[6], FVector2D(1, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[7], FVector2D(0, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

				MeshBuilder.AddVertex(FrustumVerts[2], FVector2D(0, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[1], FVector2D(1, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[5], FVector2D(1, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[6], FVector2D(0, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

				MeshBuilder.AddVertex(FrustumVerts[0], FVector2D(0, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[3], FVector2D(1, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[7], FVector2D(1, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				MeshBuilder.AddVertex(FrustumVerts[4], FVector2D(0, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

				for (int32 i = 0; i < 6; ++i)
				{
					int32 Idx = i*4;
					MeshBuilder.AddTriangle( Idx, Idx+2, Idx+1 );
					MeshBuilder.AddTriangle( Idx, Idx+3, Idx+2 );
				}

				MeshBuilder.Draw(PDI, FMatrix::Identity, GizmoRenderProxy , SDPG_World, true);
			}

			if (bHeightmapRendering)
			{		  		
				FDynamicMeshBuilder MeshBuilder;

				for (int32 Y = 0; Y < SampleSizeY; ++Y)
				{
					for (int32 X = 0; X < SampleSizeX; ++X)
					{
						FVector SampledNormal = SampledNormals[X + Y * SampleSizeX];
						FVector TangentX(SampledNormal.Z, 0, -SampledNormal.X);
						TangentX = TangentX.SafeNormal();

						MeshBuilder.AddVertex(SampledPositions[X + Y * SampleSizeX], FVector2D((float)X / (SampleSizeX), (float)Y / (SampleSizeY)), TangentX, SampledNormal^TangentX, SampledNormal, FColor(255, 255, 255) );
					}
				}

				for (int32 Y = 0; Y < SampleSizeY; ++Y)
				{
					for (int32 X = 0; X < SampleSizeX; ++X)
					{
						if (X < SampleSizeX - 1 && Y < SampleSizeY - 1)
						{
							MeshBuilder.AddTriangle( (X+0) + (Y+0) * SampleSizeX, (X+1) + (Y+1) * SampleSizeX, (X+1) + (Y+0) * SampleSizeX );
							MeshBuilder.AddTriangle( (X+0) + (Y+0) * SampleSizeX, (X+0) + (Y+1) * SampleSizeX, (X+1) + (Y+1) * SampleSizeX );
						}
					}
				}

				MeshBuilder.Draw(PDI, MeshRT, HeightmapRenderProxy, SDPG_World, false);
			}
		}
#endif
	};

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Result;
#if WITH_EDITOR
		const bool bVisible = View->Family->EngineShowFlags.Landscape;
		Result.bDrawRelevance = IsShown(View) && bVisible && !View->bIsGameView && GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo;
		Result.bDynamicRelevance = true;
		Result.bNormalTranslucencyRelevance = true;
#endif
		return Result;
	}

	virtual uint32 GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }
};

ULandscapeGizmoRenderComponent::ULandscapeGizmoRenderComponent(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bHiddenInGame = true;
	AlwaysLoadOnClient = false;
	AlwaysLoadOnServer = false;
	bSelectable = false;
	BodyInstance.bEnableCollision_DEPRECATED = true;
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
}

FPrimitiveSceneProxy* ULandscapeGizmoRenderComponent::CreateSceneProxy()
{
	return new FLandscapeGizmoRenderSceneProxy(this);
}

FBoxSphereBounds ULandscapeGizmoRenderComponent::CalcBounds(const FTransform & LocalToWorld) const
{
#if WITH_EDITOR
	ALandscapeGizmoActiveActor* Gizmo = Cast<ALandscapeGizmoActiveActor>(GetOwner());
	if( Gizmo )
	{
		return FBoxSphereBounds(Gizmo->FrustumVerts, 8);
	}
#endif
	{
		return Super::CalcBounds(LocalToWorld);
	}
}

ALandscapeGizmoActor::ALandscapeGizmoActor(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
#if WITH_EDITORONLY_DATA
	SpriteComponent = PCIP.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("Sprite"));
	if (!IsRunningCommandlet() && (SpriteComponent != nullptr))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> DecalActorIconTexture;
			FName ID_Misc;
			FText NAME_Misc;
			FConstructorStatics()
				: DecalActorIconTexture(TEXT("Texture2D'/Engine/EditorResources/S_DecalActorIcon.S_DecalActorIcon'"))
				, ID_Misc(TEXT("Misc"))
				, NAME_Misc(NSLOCTEXT("SpriteCategory", "Misc", "Misc"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		SpriteComponent->Sprite = ConstructorStatics.DecalActorIconTexture.Get();
		SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		SpriteComponent->bHiddenInGame = true;
		SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Misc;
		SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Misc;
		SpriteComponent->bIsScreenSizeScaled = true;
	}
#endif

	TSubobjectPtr<USceneComponent> SceneComponent = PCIP.CreateDefaultSubobject<USceneComponent>(this, TEXT("RootComponent0"));
	RootComponent = SceneComponent;
	RootComponent->Mobility = EComponentMobility::Static;

#if WITH_EDITORONLY_DATA
	bEditable = false;
	Width = 1280.0f;
	Height = 1280.0f;
	LengthZ = 1280.0f;
	MarginZ = 512.0f;
	MinRelativeZ = 0.0f;
	RelativeScaleZ = 1.0f;
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR

void ALandscapeGizmoActor::Duplicate(ALandscapeGizmoActor* Gizmo)
{
	Gizmo->Width = Width;
	Gizmo->Height = Height;
	Gizmo->LengthZ = LengthZ;
	Gizmo->MarginZ = MarginZ;
	//Gizmo->TargetLandscapeInfo = TargetLandscapeInfo;

	Gizmo->SetActorLocation( GetActorLocation(), false );
	Gizmo->SetActorRotation( GetActorRotation() );

	if( Gizmo->GetRootComponent() != NULL && GetRootComponent() != NULL )
	{
		Gizmo->GetRootComponent()->SetRelativeScale3D( GetRootComponent()->RelativeScale3D );
	}

	Gizmo->MinRelativeZ = MinRelativeZ;
	Gizmo->RelativeScaleZ = RelativeScaleZ;

	Gizmo->ReregisterAllComponents();
}
#endif	//WITH_EDITOR

ALandscapeGizmoActiveActor::ALandscapeGizmoActiveActor(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP
		.DoNotCreateDefaultSubobject(TEXT("Sprite"))
	)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinder<UMaterial> LandscapeGizmo_Mat;
			ConstructorHelpers::FObjectFinder<UMaterialInstanceConstant> LandscapeGizmo_Mat_Copied;
			ConstructorHelpers::FObjectFinder<UMaterial> LandscapeGizmoHeight_Mat;
			FConstructorStatics()
				: LandscapeGizmo_Mat(TEXT("/Engine/EditorLandscapeResources/LandscapeGizmo_Mat"))
				, LandscapeGizmo_Mat_Copied(TEXT("/Engine/EditorLandscapeResources/LandscapeGizmo_Mat_Copied"))
				, LandscapeGizmoHeight_Mat(TEXT("/Engine/EditorLandscapeResources/LandscapeGizmoHeight_Mat"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		GizmoMaterial = ConstructorStatics.LandscapeGizmo_Mat.Object;
		GizmoDataMaterial = ConstructorStatics.LandscapeGizmo_Mat_Copied.Object;
		GizmoMeshMaterial = ConstructorStatics.LandscapeGizmoHeight_Mat.Object;
	}
#endif // WITH_EDITORONLY_DATA

	TSubobjectPtr<ULandscapeGizmoRenderComponent> LandscapeGizmoRenderComponent = PCIP.CreateDefaultSubobject<ULandscapeGizmoRenderComponent>(this, TEXT("GizmoRendererComponent0"));
	LandscapeGizmoRenderComponent->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

	RootComponent = LandscapeGizmoRenderComponent;
#if WITH_EDITORONLY_DATA
	bEditable = true;
	Width = 1280.0f;
	Height = 1280.0f;
	LengthZ = 1280.0f;
	MarginZ = 512.0f;
	DataType = LGT_None;
	SampleSizeX = 0;
	SampleSizeY = 0;
	CachedWidth = 0.0f;
	CachedHeight = 0.0f;
	CachedScaleXY = 1.0f;
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void ALandscapeGizmoActiveActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if( PropertyName == FName(TEXT("LengthZ")) )
	{
		if (LengthZ < 0)
		{
			LengthZ = MarginZ;
		}
	}
	else if ( PropertyName == FName(TEXT("TargetLandscapeInfo")) )
	{
		SetTargetLandscape(TargetLandscapeInfo);
	}
	// AActor::PostEditChange will ForceUpdateComponents()
	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void ALandscapeGizmoActiveActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove( bFinished );

	if (bFinished)
	{
		UnsnappedRotation = FRotator::ZeroRotator;
	}
}

FVector ALandscapeGizmoActiveActor::SnapToLandscapeGrid(const FVector& GizmoLocation) const
{
	check(TargetLandscapeInfo);
	const FTransform LToW = TargetLandscapeInfo->GetLandscapeProxy()->LandscapeActorToWorld();
	const FVector LandscapeSpaceLocation = LToW.InverseTransformPosition(GizmoLocation);
	const FVector SnappedLandscapeSpaceLocation = LandscapeSpaceLocation.GridSnap(1);
	const FVector ResultLocation = LToW.TransformPosition(SnappedLandscapeSpaceLocation);
	return ResultLocation;
}

void ALandscapeGizmoActiveActor::EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	if (bSnapToLandscapeGrid)
	{
		const FVector GizmoLocation = GetActorLocation() + DeltaTranslation;
		const FVector ResultLocation = SnapToLandscapeGrid(GizmoLocation);

		SetActorLocation(ResultLocation, false);
	}
	else
	{
		Super::EditorApplyTranslation(DeltaTranslation, bAltDown, bShiftDown, bCtrlDown);
	}

	ReregisterAllComponents();
}

FRotator ALandscapeGizmoActiveActor::SnapToLandscapeGrid(const FRotator& GizmoRotation) const
{
	// Snap to multiples of 90 Yaw in landscape coordinate system
	//check(TargetLandscapeInfo && TargetLandscapeInfo->LandscapeProxy);
	//const FTransform LToW = TargetLandscapeInfo->LandscapeProxy->ActorToWorld();
	//const FRotator LandscapeSpaceRotation = (LToW.GetRotation().Inverse() * GizmoRotation.Quaternion()).Rotator().GetNormalized();
	//const FRotator SnappedLandscapeSpaceRotation = FRotator(0, FMath::GridSnap(LandscapeSpaceRotation.Yaw, 90), 0);
	//const FRotator ResultRotation = (SnappedLandscapeSpaceRotation.Quaternion() * LToW.GetRotation()).Rotator().GetNormalized();

	// Gizmo rotation is used as if it was relative to the landscape even though it isn't, so snap in world space
	const FRotator ResultRotation = FRotator(0, FMath::GridSnap(GizmoRotation.Yaw, 90), 0);
	return ResultRotation;
}

void ALandscapeGizmoActiveActor::EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	if (bSnapToLandscapeGrid)
	{
		// Based on AActor::EditorApplyRotation
		FRotator GizmoRotation = GetActorRotation() + UnsnappedRotation;
		FRotator Winding, Remainder;
		GizmoRotation.GetWindingAndRemainder(Winding, Remainder);
		const FQuat ActorQ = Remainder.Quaternion();
		const FQuat DeltaQ = DeltaRotation.Quaternion();
		const FQuat ResultQ = DeltaQ * ActorQ;
		const FRotator NewActorRotRem = FRotator( ResultQ );
		FRotator DeltaRot = NewActorRotRem - Remainder;
		DeltaRot.Normalize();

		GizmoRotation += DeltaRot;

		const FRotator ResultRotation = SnapToLandscapeGrid(GizmoRotation);

		UnsnappedRotation = GizmoRotation - ResultRotation;
		UnsnappedRotation.Pitch = 0;
		UnsnappedRotation.Roll = 0;
		UnsnappedRotation.Normalize();

		SetActorRotation(ResultRotation);
	}
	else
	{
		Super::EditorApplyRotation(DeltaRotation, bAltDown, bShiftDown, bCtrlDown);
	}

	ReregisterAllComponents();
}

ALandscapeGizmoActor* ALandscapeGizmoActiveActor::SpawnGizmoActor()
{
	// ALandscapeGizmoActor is history for ALandscapeGizmoActiveActor
	ALandscapeGizmoActor* NewActor = GetWorld()->SpawnActor<ALandscapeGizmoActor>();
	Duplicate(NewActor);
	return NewActor;
}

void ALandscapeGizmoActiveActor::SetTargetLandscape(ULandscapeInfo* LandscapeInfo)
{
	ULandscapeInfo* PrevInfo = TargetLandscapeInfo;
	if (!LandscapeInfo || LandscapeInfo->HasAnyFlags(RF_BeginDestroyed))
	{
		TargetLandscapeInfo = NULL;
		if (GetWorld())
		{
			for (auto It = GetWorld()->LandscapeInfoMap.CreateIterator(); It; ++It)
			{
				LandscapeInfo = It.Value();
				if (LandscapeInfo && !LandscapeInfo->HasAnyFlags(RF_BeginDestroyed))
				{
					//if (LandscapeInfo->LandscapeProxy && !LandscapeInfo->LandscapeProxy->HasAnyFlags(RF_BeginDestroyed) )
					{
						TargetLandscapeInfo = LandscapeInfo;
						break;
					}
				}
			}
		}
	}
	else
	{
		TargetLandscapeInfo = LandscapeInfo;
	}

	if (TargetLandscapeInfo && TargetLandscapeInfo != PrevInfo)
	{
		MarginZ = TargetLandscapeInfo->DrawScale.Z * 3;
		Width = Height = TargetLandscapeInfo->DrawScale.X * (TargetLandscapeInfo->ComponentSizeQuads+1);

		float LengthZ;
		FVector NewLocation = TargetLandscapeInfo->GetLandscapeCenterPos(LengthZ);
		SetLength(LengthZ);
		SetActorLocation( NewLocation, false );
		SetActorRotation(FRotator::ZeroRotator);
		ReregisterAllComponents();
	}
}

void ALandscapeGizmoActiveActor::ClearGizmoData()
{
	DataType = LGT_None;
	SelectedData.Empty();
	LayerInfos.Empty();
	ReregisterAllComponents();
}

void ALandscapeGizmoActiveActor::FitToSelection()
{
	if (TargetLandscapeInfo)
	{
		// Find fit size
		int32 MinX = MAX_int32, MinY = MAX_int32, MaxX = MIN_int32, MaxY = MIN_int32;
		TargetLandscapeInfo->GetSelectedExtent(MinX, MinY, MaxX, MaxY);
		if (MinX != MAX_int32)
		{
			float ScaleXY = TargetLandscapeInfo->DrawScale.X;
			Width = ScaleXY * (MaxX - MinX + 1) / (GetRootComponent()->RelativeScale3D.X);
			Height = ScaleXY * (MaxY - MinY + 1) / (GetRootComponent()->RelativeScale3D.Y);
			float LengthZ;
			FVector NewLocation = TargetLandscapeInfo->GetLandscapeCenterPos(LengthZ, MinX, MinY, MaxX, MaxY);
			SetLength(LengthZ);
			SetActorLocation(NewLocation, false);
			SetActorRotation(FRotator::ZeroRotator);
			// Reset Z render scale values...
			MinRelativeZ = 0.f;
			RelativeScaleZ = 1.f;
			ReregisterAllComponents();
		}
	}
}

void ALandscapeGizmoActiveActor::FitMinMaxHeight()
{
	if (TargetLandscapeInfo)
	{
		float MinZ = HALF_WORLD_MAX, MaxZ = -HALF_WORLD_MAX;
		// Change MinRelativeZ and RelativeZScale to fit Gizmo Box
		for (auto It = SelectedData.CreateConstIterator(); It; ++It )
		{
			const FGizmoSelectData& Data = It.Value();
			MinZ = FMath::Min(MinZ, Data.HeightData);
			MaxZ = FMath::Max(MaxZ, Data.HeightData);
		}

		if (MinZ != HALF_WORLD_MAX && MaxZ > MinZ + KINDA_SMALL_NUMBER)
		{
			MinRelativeZ = MinZ;
			RelativeScaleZ = 1.f / (MaxZ - MinZ);
			ReregisterAllComponents();
		}
	}
}

float ALandscapeGizmoActiveActor::GetNormalizedHeight(uint16 LandscapeHeight) const
{
	if (TargetLandscapeInfo)
	{
		ALandscapeProxy* Proxy = TargetLandscapeInfo->GetCurrentLevelLandscapeProxy(true);
		if (Proxy)
		{
			// Need to make it scale...?
			float ZScale = GetLength();
			if (ZScale > KINDA_SMALL_NUMBER)
			{
				FVector LocalGizmoPos = Proxy->LandscapeActorToWorld().InverseTransformPosition(GetActorLocation());
				return FMath::Clamp<float>( (( LandscapeDataAccess::GetLocalHeight(LandscapeHeight) - LocalGizmoPos.Z) * TargetLandscapeInfo->DrawScale.Z) / ZScale, 0.f, 1.f );
			}
		}
	}
	return 0.f;
}

float ALandscapeGizmoActiveActor::GetWorldHeight(float NormalizedHeight) const
{
	if (TargetLandscapeInfo)
	{
		ALandscapeProxy* Proxy = TargetLandscapeInfo->GetCurrentLevelLandscapeProxy(true);
		if (Proxy)
		{
			float ZScale = GetLength();
			if (ZScale > KINDA_SMALL_NUMBER)
			{
				FVector LocalGizmoPos = Proxy->LandscapeActorToWorld().InverseTransformPosition(GetActorLocation());
				return NormalizedHeight * ZScale + LocalGizmoPos.Z * TargetLandscapeInfo->DrawScale.Z;
			}
		}
	}
	return 0.f;
}

float ALandscapeGizmoActiveActor::GetLandscapeHeight(float NormalizedHeight) const
{
	if (TargetLandscapeInfo)
	{
		NormalizedHeight = (NormalizedHeight - MinRelativeZ) * RelativeScaleZ;
		float ScaleZ = TargetLandscapeInfo->DrawScale.Z;
		return (GetWorldHeight(NormalizedHeight) / ScaleZ);
	}
	return 0.f;
}

void ALandscapeGizmoActiveActor::CalcNormal()
{
	int32 SquaredDataTex = DataTexSize * DataTexSize;
	if (SampledHeight.Num() == SquaredDataTex && SampleSizeX > 0 && SampleSizeY > 0 )
	{
		if (SampledNormal.Num() != SquaredDataTex)
		{
			SampledNormal.Empty(SquaredDataTex);
			SampledNormal.AddZeroed(SquaredDataTex);
		}
		for (int32 Y = 0; Y < SampleSizeY-1; ++Y)
		{
			for (int32 X = 0; X < SampleSizeX-1; ++X)
			{
				FVector Vert00 = SampledHeight[X + Y*DataTexSize];
				FVector Vert01 = SampledHeight[X + (Y+1)*DataTexSize];
				FVector Vert10 = SampledHeight[X+1 + Y*DataTexSize];
				FVector Vert11 = SampledHeight[X+1 + (Y+1)*DataTexSize];

				FVector FaceNormal1 = ((Vert00-Vert10) ^ (Vert10-Vert11)).SafeNormal();
				FVector FaceNormal2 = ((Vert11-Vert01) ^ (Vert01-Vert00)).SafeNormal(); 

				// contribute to the vertex normals.
				SampledNormal[X + Y*DataTexSize] += FaceNormal1;
				SampledNormal[X + (Y+1)*DataTexSize] += FaceNormal2;
				SampledNormal[X+1 + Y*DataTexSize] += FaceNormal1 + FaceNormal2;
				SampledNormal[X+1 + (Y+1)*DataTexSize] += FaceNormal1 + FaceNormal2;
			}
		}
		for (int32 Y = 0; Y < SampleSizeY; ++Y)
		{
			for (int32 X = 0; X < SampleSizeX; ++X)
			{
				SampledNormal[X + Y*DataTexSize] = SampledNormal[X + Y*DataTexSize].SafeNormal();
			}
		}
	}
}

void ALandscapeGizmoActiveActor::SampleData(int32 SizeX, int32 SizeY)
{
	if (TargetLandscapeInfo && GizmoTexture)
	{
		// Rasterize rendering Texture...
		int32 TexSizeX = FMath::Min(ALandscapeGizmoActiveActor::DataTexSize, SizeX);
		int32 TexSizeY = FMath::Min(ALandscapeGizmoActiveActor::DataTexSize, SizeY);
		SampleSizeX = TexSizeX;
		SampleSizeY = TexSizeY;

		// Update Data Texture...
		//DataTexture->SetFlags(RF_Transactional);
		//DataTexture->Modify();

		TextureScale = FVector2D( (float)SizeX / FMath::Max(ALandscapeGizmoActiveActor::DataTexSize, SizeX), (float)SizeY / FMath::Max(ALandscapeGizmoActiveActor::DataTexSize, SizeY));
		uint8* TexData = GizmoTexture->Source.LockMip(0);
		int32 GizmoTexSizeX = GizmoTexture->Source.GetSizeX();
		for (int32 Y = 0; Y < TexSizeY; ++Y)
		{
			for (int32 X = 0; X < TexSizeX; ++X)
			{
				float TexX = X * SizeX / TexSizeX;
				float TexY = Y * SizeY / TexSizeY;
				int32 LX = FMath::FloorToInt(TexX);
				int32 LY = FMath::FloorToInt(TexY);

				float FracX = TexX - LX;
				float FracY = TexY - LY;

				FGizmoSelectData* Data00 = SelectedData.Find(ALandscape::MakeKey(LX, LY));
				FGizmoSelectData* Data10 = SelectedData.Find(ALandscape::MakeKey(LX+1, LY));
				FGizmoSelectData* Data01 = SelectedData.Find(ALandscape::MakeKey(LX, LY+1));
				FGizmoSelectData* Data11 = SelectedData.Find(ALandscape::MakeKey(LX+1, LY+1));

				// Invert Tex Data to show selected region more visible
				TexData[X + Y*GizmoTexSizeX] = 255 - FMath::Lerp(
					FMath::Lerp(Data00 ? Data00->Ratio : 0, Data10 ? Data10->Ratio : 0, FracX),
					FMath::Lerp(Data01 ? Data01->Ratio : 0, Data11 ? Data11->Ratio : 0, FracX),
					FracY
					) * 255;

				if (DataType & LGT_Height)
				{
					float NormalizedHeight = FMath::Lerp(
						FMath::Lerp(Data00 ? Data00->HeightData : 0, Data10 ? Data10->HeightData : 0, FracX),
						FMath::Lerp(Data01 ? Data01->HeightData : 0, Data11 ? Data11->HeightData : 0, FracX),
						FracY
						);

					SampledHeight[X + Y*GizmoTexSizeX] = FVector(LX, LY, NormalizedHeight);
				}
			}
		}

		if (DataType & LGT_Height)
		{
			CalcNormal();
		}

		GizmoTexture->TemporarilyDisableStreaming();
		FUpdateTextureRegion2D Region(0, 0, 0, 0, TexSizeX, TexSizeY);
		GizmoTexture->UpdateTextureRegions(0, 1, &Region, GizmoTexSizeX, sizeof(uint8), TexData, false);
		FlushRenderingCommands();
		GizmoTexture->Source.UnlockMip(0);

		ReregisterAllComponents();
	}
}

ENGINE_API void ALandscapeGizmoActiveActor::Import( int32 VertsX, int32 VertsY, uint16* HeightData, TArray<ULandscapeLayerInfoObject*> ImportLayerInfos, uint8* LayerDataPointers[] )
{
	if (VertsX <= 0 || VertsY <= 0 || HeightData == NULL || TargetLandscapeInfo == NULL || GizmoTexture == NULL || (ImportLayerInfos.Num() && !LayerDataPointers) )
	{
		return;
	}

	GWarn->BeginSlowTask( NSLOCTEXT("Landscape", "BeginImportingGizmoDataTask", "Importing Gizmo Data"), true);

	ClearGizmoData();

	CachedScaleXY = TargetLandscapeInfo->DrawScale.X;
	CachedWidth = CachedScaleXY * VertsX; // (DrawScale * DrawScale3D.X);
	CachedHeight = CachedScaleXY * VertsY; // (DrawScale * DrawScale3D.Y);
	
	float CurrentWidth = GetWidth();
	float CurrentHeight = GetHeight();
	LengthZ = GetLength();

	FVector Scale3D = FVector(CurrentWidth / CachedWidth, CurrentHeight / CachedHeight, 1.f);
	GetRootComponent()->SetRelativeScale3D(Scale3D);

	Width = CachedWidth;
	Height = CachedHeight;

	DataType = ELandscapeGizmoType(DataType | LGT_Height);
	if (ImportLayerInfos.Num())
	{
		DataType = ELandscapeGizmoType(DataType | LGT_Weight);
	}

	for (int32 Y = 0; Y < VertsY; ++Y)
	{
		for (int32 X = 0; X < VertsX; ++X)
		{
			FGizmoSelectData Data;
			Data.Ratio = 1.f;
			Data.HeightData = (float)HeightData[X + Y*VertsX] / 65535.f; //GetNormalizedHeight(HeightData[X + Y*VertsX]);
			for (int32 i = 0; i < ImportLayerInfos.Num(); ++i)
			{
				Data.WeightDataMap.Add( ImportLayerInfos[i], LayerDataPointers[i][X + Y*VertsX] );
			}
			SelectedData.Add(ALandscape::MakeKey(X, Y), Data);
		}
	}

	SampleData(VertsX, VertsY);

	for (auto It = ImportLayerInfos.CreateConstIterator(); It; ++It)
	{
		LayerInfos.Add(*It);
	}

	GWarn->EndSlowTask();

	ReregisterAllComponents();
}

void ALandscapeGizmoActiveActor::Export(int32 Index, TArray<FString>& Filenames)
{
	//guard around case where landscape has no layer structs
	if (Filenames.Num() == 0)
	{
		return;
	}

	bool bExportOneTarget = (Filenames.Num() == 1);

	if (TargetLandscapeInfo)
	{
		int32 MinX = MAX_int32, MinY = MAX_int32, MaxX = MIN_int32, MaxY = MIN_int32;
		for (auto It = SelectedData.CreateConstIterator(); It; ++It )
		{
			int32 X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);
			if (MinX > X) MinX = X;
			if (MaxX < X) MaxX = X;
			if (MinY > Y) MinY = Y;
			if (MaxY < Y) MaxY = Y;
		}

		if (MinX != MAX_int32)
		{
			GWarn->BeginSlowTask( NSLOCTEXT("Landscape", "BeginExportingGizmoDataTask", "Exporting Gizmo Data"), true);

			TArray<uint8> HeightData;
			if (!bExportOneTarget || Index == -1)
			{
				HeightData.AddZeroed((1+MaxX-MinX)*(1+MaxY-MinY)*sizeof(uint16));
			}
			uint16* pHeightData = (uint16*)HeightData.GetTypedData();

			TArray<TArray<uint8> > WeightDatas;
			for( int32 i=1;i<Filenames.Num();i++ )
			{
				TArray<uint8> WeightData;
				if (!bExportOneTarget || Index == i-1)
				{
					WeightData.AddZeroed((1+MaxX-MinX)*(1+MaxY-MinY));
				}
				WeightDatas.Add(WeightData);
			}

			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 X = MinX; X <= MaxX; ++X)
				{
					const FGizmoSelectData* Data = SelectedData.Find(ALandscape::MakeKey(X, Y));
					if (Data)
					{
						int32 Idx = (X-MinX) + Y *(1+MaxX-MinX);
						if (!bExportOneTarget || Index == -1)
						{
							pHeightData[Idx] = FMath::Clamp<uint16>(Data->HeightData * 65535.f, 0, 65535);
						}

						for( int32 i=1;i<Filenames.Num();i++ )
						{
							if (!bExportOneTarget || Index == i-1)
							{
								TArray<uint8>& WeightData = WeightDatas[i-1];
								WeightData[Idx] = FMath::Clamp<uint8>(Data->WeightDataMap.FindRef(LayerInfos[i-1]), 0, 255);
							}
						}
					}
				}
			}

			if (!bExportOneTarget || Index == -1)
			{
				FFileHelper::SaveArrayToFile(HeightData,*Filenames[0]);
			}

			for( int32 i=1;i<Filenames.Num();i++ )
			{
				if (!bExportOneTarget || Index == i-1)
				{
					FFileHelper::SaveArrayToFile(WeightDatas[i-1],*Filenames[bExportOneTarget ? 0 : i]);
				}
			}

			GWarn->EndSlowTask();
		}
		else
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "LandscapeGizmoExport_Warning", "Landscape Gizmo has no copyed data. You need to choose proper targets and copy it to Gizmo."));
		}
	}
}

void ALandscapeGizmoActiveActor::ExportToClipboard()
{
	if (TargetLandscapeInfo && DataType != LGT_None)
	{
		//GWarn->BeginSlowTask( TEXT("Exporting Gizmo Data From Clipboard"), true);

		FString ClipboardString(TEXT("GizmoData="));

		ClipboardString += FString::Printf(TEXT(" Type=%d,TextureScaleX=%g,TextureScaleY=%g,SampleSizeX=%d,SampleSizeY=%d,CachedWidth=%g,CachedHeight=%g,CachedScaleXY=%g "), 
			(int32)DataType, TextureScale.X, TextureScale.Y, SampleSizeX, SampleSizeY, CachedWidth, CachedHeight, CachedScaleXY);

		for (int32 Y = 0; Y < SampleSizeY; ++Y )
		{
			for (int32 X = 0; X < SampleSizeX; ++X)
			{
				FVector& V = SampledHeight[X + Y * DataTexSize];
				ClipboardString += FString::Printf(TEXT("%d %d %d "), (int32)V.X, (int32)V.Y, *(int32*)(&V.Z) );
			}
		}

		ClipboardString += FString::Printf(TEXT("LayerInfos= "));

		for (auto It = LayerInfos.CreateConstIterator(); It; ++It)
		{
			ClipboardString += FString::Printf(TEXT("%s "), *(*It)->GetPathName() );
		}

		ClipboardString += FString::Printf(TEXT("Region= "));

		for (auto It = SelectedData.CreateConstIterator(); It; ++It )
		{
			int32 X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);
			const FGizmoSelectData& Data = It.Value();
			ClipboardString += FString::Printf(TEXT("%d %d %d %d %d "), X, Y, *(int32*)(&Data.Ratio), *(int32*)(&Data.HeightData), Data.WeightDataMap.Num());

			for (auto It2 = Data.WeightDataMap.CreateConstIterator(); It2; ++It2)
			{
				ClipboardString += FString::Printf(TEXT("%d %d "), LayerInfos.Find(It2.Key()), *(int32*)(&It2.Value()));
			}
		}

		FPlatformMisc::ClipboardCopy(*ClipboardString);

		//GWarn->EndSlowTask();
	}
}

#define MAX_GIZMO_PROP_TEXT_LENGTH			1024*1024*8

void ALandscapeGizmoActiveActor::ImportFromClipboard()
{
	FString ClipboardString;
	FPlatformMisc::ClipboardPaste(ClipboardString);
	const TCHAR* Str = *ClipboardString;
	
	if(FParse::Command(&Str,TEXT("GizmoData=")))
	{
		int32 ClipBoardSize = ClipboardString.Len();
		if (ClipBoardSize > MAX_GIZMO_PROP_TEXT_LENGTH)
		{
			if( EAppReturnType::Yes != FMessageDialog::Open( EAppMsgType::YesNo,
				FText::Format(NSLOCTEXT("UnrealEd", "LandscapeGizmoImport_Warning", "Landscape Gizmo is about to import large amount data ({0}MB) from the clipboard, which will take some time. Do you want to proceed?"),
				FText::AsNumber(ClipBoardSize >> 20) ) ) )
			{
				return;
			}
		}

		GWarn->BeginSlowTask( NSLOCTEXT("Landscape", "BeginImportingGizmoDataFromClipboardTask", "Importing Gizmo Data From Clipboard"), true);

		FParse::Next(&Str);


		int32 ReadNum = 0;

		uint8 Type = 0;
		ReadNum += FParse::Value(Str, TEXT("Type="), Type) ? 1 : 0;
		DataType = (ELandscapeGizmoType)Type;

		ReadNum += FParse::Value(Str, TEXT("TextureScaleX="), TextureScale.X) ? 1 : 0;
		ReadNum += FParse::Value(Str, TEXT("TextureScaleY="), TextureScale.Y) ? 1 : 0;
		ReadNum += FParse::Value(Str, TEXT("SampleSizeX="), SampleSizeX) ? 1 : 0;
		ReadNum += FParse::Value(Str, TEXT("SampleSizeY="), SampleSizeY) ? 1 : 0;
		ReadNum += FParse::Value(Str, TEXT("CachedWidth="), CachedWidth) ? 1 : 0;
		ReadNum += FParse::Value(Str, TEXT("CachedHeight="), CachedHeight) ? 1 : 0;
		ReadNum += FParse::Value(Str, TEXT("CachedScaleXY="), CachedScaleXY) ? 1 : 0;

		if (ReadNum > 0)
		{
			while (!FChar::IsWhitespace(*Str))
			{
				Str++;
			}
			FParse::Next(&Str);

			int32 SquaredDataTex = DataTexSize * DataTexSize;
			if (SampledHeight.Num() != SquaredDataTex)
			{
				SampledHeight.Empty(SquaredDataTex);
				SampledHeight.AddZeroed(SquaredDataTex);
			}

			// For Sample Height...
			TCHAR* StopStr;
			for (int32 Y = 0; Y < SampleSizeY; ++Y )
			{
				for (int32 X = 0; X < SampleSizeX; ++X)
				{
					FVector& V = SampledHeight[X + Y * DataTexSize];
					V.X = FCString::Strtoi(Str, &StopStr, 10);
					while (!FChar::IsWhitespace(*Str))
					{
						Str++;
					}
					FParse::Next(&Str);
					V.Y = FCString::Strtoi(Str, &StopStr, 10);
					while (!FChar::IsWhitespace(*Str))
					{
						Str++;
					}
					FParse::Next(&Str);
					//V.Z = FCString::Atof(Str);
					*((int32*)(&V.Z)) = FCString::Strtoi(Str, &StopStr, 10);
					while (!FChar::IsWhitespace(*Str))
					{
						Str++;
					}
					FParse::Next(&Str);
				}
			}

			CalcNormal();

			TCHAR StrBuf[1024];
			if(FParse::Command(&Str,TEXT("LayerInfos=")))
			{
				while( !FParse::Command(&Str,TEXT("Region=")) )
				{
					FParse::Next(&Str);
					int i = 0;
					while (!FChar::IsWhitespace(*Str))
					{
						StrBuf[i++] = *Str;
						Str++;
					}
					StrBuf[i] = 0;
					LayerInfos.Add( LoadObject<ULandscapeLayerInfoObject>(NULL, StrBuf) );
				}
			}

			//if(FParse::Command(&Str,TEXT("Region=")))
			{
				while (*Str)
				{
					FParse::Next(&Str);
					int32 X, Y, LayerNum;
					FGizmoSelectData Data;
					X = FCString::Strtoi(Str, &StopStr, 10);
					while (!FChar::IsWhitespace(*Str))
					{
						Str++;
					}
					FParse::Next(&Str);
					Y = FCString::Strtoi(Str, &StopStr, 10);
					while (!FChar::IsWhitespace(*Str))
					{
						Str++;
					}
					FParse::Next(&Str);
					*((int32*)(&Data.Ratio)) = FCString::Strtoi(Str, &StopStr, 10);
					while (!FChar::IsWhitespace(*Str))
					{
						Str++;
					}
					FParse::Next(&Str);
					*((int32*)(&Data.HeightData)) = FCString::Strtoi(Str, &StopStr, 10);
					while (!FChar::IsWhitespace(*Str))
					{
						Str++;
					}
					FParse::Next(&Str);
					LayerNum = FCString::Strtoi(Str, &StopStr, 10);
					while (!FChar::IsWhitespace(*Str))
					{
						Str++;
					}
					FParse::Next(&Str);
					for (int32 i = 0; i < LayerNum; ++i)
					{
						int32 LayerIndex = FCString::Strtoi(Str, &StopStr, 10);
						while (!FChar::IsWhitespace(*Str))
						{
							Str++;
						}
						FParse::Next(&Str);
						float Weight;
						*((int32*)(&Weight)) = FCString::Strtoi(Str, &StopStr, 10);
						while (!FChar::IsWhitespace(*Str))
						{
							Str++;
						}
						FParse::Next(&Str);
						Data.WeightDataMap.Add(LayerInfos[LayerIndex], Weight);
					}
					SelectedData.Add(ALandscape::MakeKey(X, Y), Data);
				}
			}
		}

		GWarn->EndSlowTask();

		ReregisterAllComponents();
	}
}
#endif	//WITH_EDITOR
