// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShadowSetup.cpp: Dynamic shadow setup implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "LightPropagationVolume.h"

static float GMinScreenRadiusForShadowCaster = 0.03f;
static TAutoConsoleVariable<float> CVarMinScreenRadiusForShadowCaster(
	TEXT("r.Shadow.RadiusThreshold"),
	0.03f,
	TEXT("Cull shadow casters if they are too small, values is the minimal screen space bounding sphere radius\n")
	TEXT("(default 0.03)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static float GMinScreenRadiusForShadowCasterRSM = 0.06f;
static FAutoConsoleVariableRef CVarMinScreenRadiusForShadowCasterRSM(
	TEXT("r.Shadow.RadiusThresholdRSM"),
	GMinScreenRadiusForShadowCasterRSM,
	TEXT("Cull shadow casters in the RSM if they are too small, values is the minimal screen space bounding sphere radius\n")
	TEXT("(default 0.06)")
	);

/** Can be used to visualize preshadow frustums when the shadowfrustums show flag is enabled. */
static TAutoConsoleVariable<int32> CVarDrawPreshadowFrustum(
	TEXT("r.Shadow.DrawPreshadowFrustums"),
	0,
	TEXT("visualize preshadow frustums when the shadowfrustums show flag is enabled"),
	ECVF_RenderThreadSafe
	);

/** Whether to allow preshadows (static world casting on character), can be disabled for debugging. */
static TAutoConsoleVariable<int32> CVarAllowPreshadows(
	TEXT("r.Shadow.Preshadows"),
	1,
	TEXT("Whether to allow preshadows (static world casting on character)"),
	ECVF_RenderThreadSafe
	);

/** Whether to allow per object shadows (character casting on world), can be disabled for debugging. */
static TAutoConsoleVariable<int32> CVarAllowPerObjectShadows(
	TEXT("r.Shadow.PerObject"),
	1,
	TEXT("Whether to render per object shadows (character casting on world)\n")
	TEXT("0: off\n")
	TEXT("1: on (default)"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarShadowFadeExponent(
	TEXT("r.Shadow.FadeExponent"),
	0.25f,
	TEXT("Controls the rate at which shadows are faded out"),
	ECVF_RenderThreadSafe);

/**
 * Whether preshadows can be cached as an optimization.  
 * Disabling the caching through this setting is useful when debugging.
 */
static TAutoConsoleVariable<int32> CVarCachePreshadows(
	TEXT("r.Shadow.CachePreshadow"),
	1,
	TEXT("Whether preshadows can be cached as an optimization"),
	ECVF_RenderThreadSafe
	);
bool ShouldUseCachePreshadows()
{
	return CVarCachePreshadows.GetValueOnRenderThread() != 0;
}

/**
 * This value specifies how much bounds will be expanded when rendering a cached preshadow (0.15 = 15% larger).
 * Larger values result in more cache hits, but lower resolution and pull more objects into the depth pass.
 */
static TAutoConsoleVariable<float> CVarPreshadowExpandFraction(
	TEXT("r.Shadow.PreshadowExpand"),
	0.15f,
	TEXT("How much bounds will be expanded when rendering a cached preshadow (0.15 = 15% larger)"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarPreShadowResolutionFactor(
	TEXT("r.Shadow.PreShadowResolutionFactor"),
	0.5f,
	TEXT("Mulitplier for preshadow resolution"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarShadowTexelsPerPixel(
	TEXT("r.Shadow.TexelsPerPixel"),
	1.27324f,
	TEXT("The ratio of subject pixels to shadow texels"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarPreShadowFadeResolution(
	TEXT("r.Shadow.PreShadowFadeResolution"),
	16,
	TEXT("Resolution in texels below which preshadows are faded out"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarShadowFadeResolution(
	TEXT("r.Shadow.FadeResolution"),
	64,
	TEXT("Resolution in texels below which shadows are faded out"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMinShadowResolution(
	TEXT("r.Shadow.MinResolution"),
	32,
	TEXT("Minimum dimensions (in texels) allowed for rendering shadow subject depths"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMinPreShadowResolution(
	TEXT("r.Shadow.MinPreShadowResolution"),
	8,
	TEXT("Minimum dimensions (in texels) allowed for rendering preshadow depths"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarUseConservativeShadowBounds(
	TEXT("r.Shadow.ConservativeBounds"),
	0,
	TEXT("Whether to use safe and conservative shadow frustum creation that wastes some shadowmap space"),
	ECVF_RenderThreadSafe);

/**
 * Helper function to determine fade alpha value for shadows based on resolution. In the below ASCII art (1) is
 * the MinShadowResolution and (2) is the ShadowFadeResolution. Alpha will be 0 below the min resolution and 1
 * above the fade resolution. In between it is going to be an exponential curve with the values between (1) and (2)
 * being normalized in the 0..1 range.
 *
 *  
 *  |    /-------
 *  |  /
 *  |/
 *  1-----2-------
 *
 * @param	MaxUnclampedResolution		Requested resolution, unclamped so it can be below min
 * @param	ShadowFadeResolution		Resolution at which fade begins
 * @param	MinShadowResolution			Minimum resolution of shadow
 *
 * @return	fade value between 0 and 1
 */
float CalculateShadowFadeAlpha(int32 MaxUnclampedResolution, int32 ShadowFadeResolution, int32 MinShadowResolution)
{
	float FadeAlpha = 0.0f;
	// Shadow size is above fading resolution.
	if (MaxUnclampedResolution > ShadowFadeResolution)
	{
		FadeAlpha = 1.0f;
	}
	// Shadow size is below fading resolution but above min resolution.
	else if (MaxUnclampedResolution > MinShadowResolution)
	{
		const float InverseRange = 1.0f / (ShadowFadeResolution - MinShadowResolution);
		const float FirstFadeValue = FMath::Pow(InverseRange, CVarShadowFadeExponent.GetValueOnRenderThread());
		const float SizeRatio = (float)(MaxUnclampedResolution - MinShadowResolution) * InverseRange;
		// Rescale the fade alpha to reduce the change between no fading and the first value, which reduces popping with small ShadowFadeExponent's
		FadeAlpha = (FMath::Pow(SizeRatio, CVarShadowFadeExponent.GetValueOnRenderThread()) - FirstFadeValue) / (1.0f - FirstFadeValue);
	}
	return FadeAlpha;
}

typedef TArray<FVector,TInlineAllocator<8> > FBoundingBoxVertexArray;

/** Stores the indices for an edge of a bounding volume. */
struct FBoxEdge
{
	uint16 FirstEdgeIndex;
	uint16 SecondEdgeIndex;
	FBoxEdge(uint16 InFirst, uint16 InSecond) :
		FirstEdgeIndex(InFirst),
		SecondEdgeIndex(InSecond)
	{}
};

typedef TArray<FBoxEdge,TInlineAllocator<12> > FBoundingBoxEdgeArray;

/**
 * Creates an array of vertices and edges for a bounding box.
 * @param Box - The bounding box
 * @param OutVertices - Upon return, the array will contain the vertices of the bounding box.
 * @param OutEdges - Upon return, will contain indices of the edges of the bounding box.
 */
static void GetBoundingBoxVertices(const FBox& Box,FBoundingBoxVertexArray& OutVertices, FBoundingBoxEdgeArray& OutEdges)
{
	OutVertices.Empty(8);
	OutVertices.AddUninitialized(8);
	for(int32 X = 0;X < 2;X++)
	{
		for(int32 Y = 0;Y < 2;Y++)
		{
			for(int32 Z = 0;Z < 2;Z++)
			{
				OutVertices[X * 4 + Y * 2 + Z] = FVector(
					X ? Box.Min.X : Box.Max.X,
					Y ? Box.Min.Y : Box.Max.Y,
					Z ? Box.Min.Z : Box.Max.Z
					);
			}
		}
	}

	OutEdges.Empty(12);
	OutEdges.AddUninitialized(12);
	for(uint16 X = 0;X < 2;X++)
	{
		uint16 BaseIndex = X * 4;
		OutEdges[X * 4 + 0] = FBoxEdge(BaseIndex, BaseIndex + 1);
		OutEdges[X * 4 + 1] = FBoxEdge(BaseIndex + 1, BaseIndex + 3);
		OutEdges[X * 4 + 2] = FBoxEdge(BaseIndex + 3, BaseIndex + 2);
		OutEdges[X * 4 + 3] = FBoxEdge(BaseIndex + 2, BaseIndex);
	}
	for(uint16 XEdge = 0;XEdge < 4;XEdge++)
	{
		OutEdges[8 + XEdge] = FBoxEdge(XEdge, XEdge + 4);
	}
}

/**
 * Computes the transform contains a set of bounding box vertices and minimizes the pre-transform volume inside the post-transform clip space.
 * @param ZAxis - The Z axis of the transform.
 * @param Points - The points that represent the bounding volume.
 * @param Edges - The edges of the bounding volume.
 * @param OutAspectRatio - Upon successful return, contains the aspect ratio of the AABB; the ratio of width:height.
 * @param OutTransform - Upon successful return, contains the transform.
 * @return true if it successfully found a non-zero area projection of the bounding points.
 */
static bool GetBestShadowTransform(const FVector& ZAxis,const FBoundingBoxVertexArray& Points, const FBoundingBoxEdgeArray& Edges, float& OutAspectRatio, FMatrix& OutTransform)
{
	// Find the axis parallel to the edge between any two boundary points with the smallest projection of the bounds onto the axis.
	FVector XAxis(0,0,0);
	FVector YAxis(0,0,0);
	FVector Translation(0,0,0);
	float BestProjectedExtent = FLT_MAX;
	bool bValidProjection = false;

	// Cache unaliased pointers to point and edge data
	const FVector* RESTRICT PointsPtr = Points.GetData();
	const FBoxEdge* RESTRICT EdgesPtr = Edges.GetData();

	const int32 NumPoints = Points.Num();
	const int32 NumEdges = Edges.Num();

	// We're always dealing with box geometry here, so we can hint the compiler
	ASSUME( NumPoints == 8 );
	ASSUME( NumEdges == 12 );

	for(int32 EdgeIndex = 0;EdgeIndex < NumEdges; ++EdgeIndex)
	{
		const FVector Point = PointsPtr[EdgesPtr[EdgeIndex].FirstEdgeIndex];
		const FVector OtherPoint = PointsPtr[EdgesPtr[EdgeIndex].SecondEdgeIndex];
		const FVector PointDelta = OtherPoint - Point;
		const FVector TrialXAxis = (PointDelta - ZAxis * (PointDelta | ZAxis)).SafeNormal();
		const FVector TrialYAxis = (ZAxis ^ TrialXAxis).SafeNormal();

		// Calculate the size of the projection of the bounds onto this axis and an axis orthogonal to it and the Z axis.
		float MinProjectedX = FLT_MAX;
		float MaxProjectedX = -FLT_MAX;
		float MinProjectedY = FLT_MAX;
		float MaxProjectedY = -FLT_MAX;
		for(int32 ProjectedPointIndex = 0;ProjectedPointIndex < NumPoints; ++ProjectedPointIndex)
		{
			const float ProjectedX = PointsPtr[ProjectedPointIndex] | TrialXAxis;
			MinProjectedX = FMath::Min(MinProjectedX,ProjectedX);
			MaxProjectedX = FMath::Max(MaxProjectedX,ProjectedX);
			const float ProjectedY = PointsPtr[ProjectedPointIndex] | TrialYAxis;
			MinProjectedY = FMath::Min(MinProjectedY,ProjectedY);
			MaxProjectedY = FMath::Max(MaxProjectedY,ProjectedY);
		}

		float ProjectedExtentX;
		float ProjectedExtentY;
		if (CVarUseConservativeShadowBounds.GetValueOnRenderThread() != 0)
		{
			ProjectedExtentX = 2 * FMath::Max(FMath::Abs(MaxProjectedX), FMath::Abs(MinProjectedX));
			ProjectedExtentY = 2 * FMath::Max(FMath::Abs(MaxProjectedY), FMath::Abs(MinProjectedY));
		}
		else
		{
			ProjectedExtentX = MaxProjectedX - MinProjectedX;
			ProjectedExtentY = MaxProjectedY - MinProjectedY;
		}

		const float ProjectedExtent = ProjectedExtentX * ProjectedExtentY;
		if(ProjectedExtent < BestProjectedExtent - .05f 
			// Only allow projections with non-zero area
			&& ProjectedExtent > DELTA)
		{
			bValidProjection = true;
			BestProjectedExtent = ProjectedExtent;
			XAxis = TrialXAxis * 2.0f / ProjectedExtentX;
			YAxis = TrialYAxis * 2.0f / ProjectedExtentY;

			// Translating in post-transform clip space can cause the corners of the world space bounds to be outside of the transform generated by this function
			// This usually manifests in cinematics where the character's head is near the top of the bounds
			if (CVarUseConservativeShadowBounds.GetValueOnRenderThread() == 0)
			{
				Translation.X = (MinProjectedX + MaxProjectedX) * 0.5f;
				Translation.Y = (MinProjectedY + MaxProjectedY) * 0.5f;
			}

			if(ProjectedExtentY > ProjectedExtentX)
			{
				// Always make the X axis the largest one.
				Exchange(XAxis,YAxis);
				Exchange(Translation.X,Translation.Y);
				XAxis *= -1.0f;
				Translation.X *= -1.0f;
				OutAspectRatio = ProjectedExtentY / ProjectedExtentX;
			}
			else
			{
				OutAspectRatio = ProjectedExtentX / ProjectedExtentY;
			}
		}
	}

	// Only create the shadow if the projected extent of the given points has a non-zero area.
	if(bValidProjection && BestProjectedExtent > DELTA)
	{
		OutTransform = FBasisVectorMatrix(XAxis,YAxis,ZAxis,FVector(0,0,0)) * FTranslationMatrix(Translation);
		return true;
	}
	else
	{
		return false;
	}
}

/** A transform the remaps depth and potentially projects onto some plane. */
struct FShadowProjectionMatrix: FMatrix
{
	FShadowProjectionMatrix(float MinZ,float MaxZ,const FVector4& WAxis):
		FMatrix(
			FPlane(1,	0,	0,													WAxis.X),
			FPlane(0,	1,	0,													WAxis.Y),
			FPlane(0,	0,	(WAxis.Z * MaxZ + WAxis.W) / (MaxZ - MinZ),			WAxis.Z),
			FPlane(0,	0,	-MinZ * (WAxis.Z * MaxZ + WAxis.W) / (MaxZ - MinZ),	WAxis.W)
			)
	{}
};

/** Initialization constructor for a per-object shadow. e.g. translucent particle system */
FProjectedShadowInfo::FProjectedShadowInfo(
	FLightSceneInfo* InLightSceneInfo,
	const FPrimitiveSceneInfo* InParentSceneInfo,
	const FPerObjectProjectedShadowInitializer& Initializer,
	bool bInPreShadow,
	uint32 InResolutionX,
	uint32 MaxShadowResolutionY,
	float InMaxScreenPercent,
	const TArray<float, TInlineAllocator<2> >& InFadeAlphas,
	bool bInTranslucentShadow
	):
	LightSceneInfo(InLightSceneInfo),
	LightSceneInfoCompact(InLightSceneInfo),
	ParentSceneInfo(InParentSceneInfo),
	DependentView(NULL),
	ShadowId(INDEX_NONE),
	PreShadowTranslation(Initializer.PreShadowTranslation),
	ShadowBounds(Initializer.SubjectBounds.Origin - Initializer.PreShadowTranslation, Initializer.SubjectBounds.SphereRadius),
	X(0),
	Y(0),
	ResolutionX(InResolutionX),
	ResolutionY(0),
	MaxScreenPercent(InMaxScreenPercent),
	FadeAlphas(InFadeAlphas),
	SplitIndex(INDEX_NONE),
	bAllocated(false),
	bAllocatedInTranslucentLayout(false),
	bRendered(false),
	bAllocatedInPreshadowCache(false),
	bDepthsCached(false),
	bDirectionalLight(Initializer.bDirectionalLight),
	bWholeSceneShadow(false),
	bOnePassPointLightShadow(false),
	bReflectiveShadowmap(false),
	bTranslucentShadow(bInTranslucentShadow),
	bPreShadow(bInPreShadow)
{
	const FMatrix WorldToLightScaled = Initializer.WorldToLight * FScaleMatrix(Initializer.Scales);

	// Create an array of the extreme vertices of the subject's bounds.
	FBoundingBoxVertexArray BoundsPoints;
	FBoundingBoxEdgeArray BoundsEdges;
	GetBoundingBoxVertices(Initializer.SubjectBounds.GetBox(),BoundsPoints,BoundsEdges);

	// Project the bounding box vertices.
	FBoundingBoxVertexArray ProjectedBoundsPoints;
	for (int32 PointIndex = 0; PointIndex < BoundsPoints.Num(); PointIndex++)
	{
		const FVector TransformedBoundsPoint = WorldToLightScaled.TransformPosition(BoundsPoints[PointIndex]);
		const float TransformedBoundsPointW = Dot4(FVector4(0, 0, TransformedBoundsPoint | Initializer.FaceDirection,1), Initializer.WAxis);
		if (TransformedBoundsPointW >= DELTA)
		{
			ProjectedBoundsPoints.Add(TransformedBoundsPoint / TransformedBoundsPointW);
		}
		else
		{
			ProjectedBoundsPoints.Add(FVector(FLT_MAX, FLT_MAX, FLT_MAX));
		}
	}

	// Compute the transform from light-space to shadow-space.
	FMatrix LightToShadow;
	float AspectRatio;

	if (GetBestShadowTransform(Initializer.FaceDirection.SafeNormal(),ProjectedBoundsPoints,BoundsEdges,AspectRatio,LightToShadow))
	{
		bValidTransform = true;
		const FMatrix WorldToShadow = WorldToLightScaled * LightToShadow;

		const FBox ShadowSubjectBounds = Initializer.SubjectBounds.GetBox().TransformBy(WorldToShadow);

		MinSubjectZ = FMath::Max(Initializer.MinLightW, ShadowSubjectBounds.Min.Z);
		float MaxReceiverZ = FMath::Min(MinSubjectZ + Initializer.MaxDistanceToCastInLightW, (float)HALF_WORLD_MAX);
		// Max can end up smaller than min due to the clamp to HALF_WORLD_MAX above
		MaxReceiverZ = FMath::Max(MaxReceiverZ, MinSubjectZ + 1);
		MaxSubjectZ = FMath::Max(ShadowSubjectBounds.Max.Z, MinSubjectZ + 1);

		const FMatrix SubjectMatrix = WorldToShadow * FShadowProjectionMatrix(MinSubjectZ, MaxSubjectZ, Initializer.WAxis);
		const float MaxSubjectAndReceiverDepth = Initializer.SubjectBounds.GetBox().TransformBy(SubjectMatrix).Max.Z;

		float MaxSubjectDepth;

		if (bPreShadow)
		{
			const FMatrix PreSubjectMatrix = WorldToShadow * FShadowProjectionMatrix(Initializer.MinLightW, MaxSubjectZ, Initializer.WAxis);
			// Preshadow frustum bounds go from the light to the furthest extent of the object in light space
			SubjectAndReceiverMatrix = PreSubjectMatrix;
			ReceiverMatrix = SubjectMatrix;
			MaxSubjectDepth = bDirectionalLight ? MaxSubjectAndReceiverDepth : Initializer.SubjectBounds.GetBox().TransformBy(PreSubjectMatrix).Max.Z;
		}
		else
		{
			const FMatrix PostSubjectMatrix = WorldToShadow * FShadowProjectionMatrix(MinSubjectZ, MaxReceiverZ, Initializer.WAxis);
			SubjectAndReceiverMatrix = SubjectMatrix;
			ReceiverMatrix = PostSubjectMatrix;
			MaxSubjectDepth = MaxSubjectAndReceiverDepth;
		}

		InvMaxSubjectDepth = 1.0f / MaxSubjectDepth;

		MinPreSubjectZ = Initializer.MinLightW;

		ResolutionY = FMath::Min<uint32>(FMath::TruncToInt(InResolutionX / AspectRatio), MaxShadowResolutionY);

		// Store the view matrix
		// Reorder the vectors to match the main view, since ShadowViewMatrix will be used to override the main view's view matrix during shadow depth rendering
		ShadowViewMatrix = Initializer.WorldToLight * 
			FMatrix(
				FPlane(0,	0,	1,	0),
				FPlane(1,	0,	0,	0),
				FPlane(0,	1,	0,	0),
				FPlane(0,	0,	0,	1));

		GetViewFrustumBounds(CasterFrustum,SubjectAndReceiverMatrix,true);

		InvReceiverMatrix = ReceiverMatrix.Inverse();
		GetViewFrustumBounds(ReceiverFrustum,ReceiverMatrix,true);
	}
	else
	{
		bValidTransform = false;
	}

	UpdateShaderDepthBias();
}

/** Initialization constructor for a whole-scene shadow. e.g. directional light cascade or point light */
FProjectedShadowInfo::FProjectedShadowInfo(
	FLightSceneInfo* InLightSceneInfo,
	FViewInfo* InDependentView,
	const FWholeSceneProjectedShadowInitializer& Initializer,
	uint32 InResolutionX,
	uint32 InResolutionY,
	const TArray<float, TInlineAllocator<2> >& InFadeAlphas
	)
:	LightSceneInfo(InLightSceneInfo)
,	LightSceneInfoCompact(InLightSceneInfo)
,	ParentSceneInfo(NULL)
,	DependentView(InDependentView)
,	ShadowId(INDEX_NONE)
,	PreShadowTranslation(Initializer.PreShadowTranslation)
,	FrustumCullPlanes(Initializer.FrustumCullPlanes)
,	CascadeSettings(Initializer.CascadeSettings)
,	X(0)
,	Y(0)
,	ResolutionX(InResolutionX)
,	ResolutionY(InResolutionY)
,	MaxScreenPercent(1.0f)
,	FadeAlphas(InFadeAlphas)
,	SplitIndex(Initializer.SplitIndex)
,	bAllocated(false)
,	bAllocatedInTranslucentLayout(false)
,	bRendered(false)
,	bAllocatedInPreshadowCache(false)
,	bDepthsCached(false)
,	bDirectionalLight(Initializer.bDirectionalLight)
,	bWholeSceneShadow(true)
,	bOnePassPointLightShadow(Initializer.bOnePassPointLightShadow)
,	bReflectiveShadowmap(false) 
,	bTranslucentShadow(false)
,	bPreShadow(false)
,	bValidTransform(true)
{
	FVector	XAxis, YAxis;
	Initializer.FaceDirection.FindBestAxisVectors(XAxis,YAxis);
	const FMatrix WorldToLightScaled = Initializer.WorldToLight * FScaleMatrix(Initializer.Scales);
	const FMatrix WorldToFace = WorldToLightScaled * FBasisVectorMatrix(-XAxis,YAxis,Initializer.FaceDirection.SafeNormal(),FVector::ZeroVector);

	MaxSubjectZ = WorldToFace.TransformPosition(Initializer.SubjectBounds.Origin).Z + Initializer.SubjectBounds.SphereRadius;
	MinSubjectZ = FMath::Max(MaxSubjectZ - Initializer.SubjectBounds.SphereRadius * 2,Initializer.MinLightW);
	
	if(Initializer.bDirectionalLight)
	{
		// Limit how small the depth range can be for smaller cascades
		// This is needed for shadow modes like subsurface shadows which need depth information outside of the smaller cascade depth range
		//@todo - expose this value to the ini
		const float DepthRangeClamp = 5000;
		MaxSubjectZ = FMath::Max(MaxSubjectZ, DepthRangeClamp);
		MinSubjectZ = FMath::Min(MinSubjectZ, -DepthRangeClamp);

		const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetShadowDepthTextureResolution();
		const uint32 ShadowDepthBufferSizeX = ShadowBufferResolution.X - SHADOW_BORDER * 2;
		const uint32 ShadowDepthBufferSizeY = ShadowBufferResolution.Y - SHADOW_BORDER * 2;
		// Transform the shadow's position into shadowmap space
		const FVector TransformedPosition = WorldToFace.TransformPosition(-PreShadowTranslation);

		// Largest amount that the shadowmap will be downsampled to during sampling
		// We need to take this into account when snapping to get a stable result
		// This corresponds to the maximum kernel filter size used by subsurface shadows in ShadowProjectionPixelShader.usf
		const int32 MaxDownsampleFactor = 4;
		// Determine the distance necessary to snap the shadow's position to the nearest texel
		const float SnapX = FMath::Fmod(TransformedPosition.X, 2.0f * MaxDownsampleFactor / ShadowDepthBufferSizeX);
		const float SnapY = FMath::Fmod(TransformedPosition.Y, 2.0f * MaxDownsampleFactor / ShadowDepthBufferSizeY);
		// Snap the shadow's position and transform it back into world space
		// This snapping prevents sub-texel camera movements which removes view dependent aliasing from the final shadow result
		// This only maintains stable shadows under camera translation and rotation
		const FVector SnappedWorldPosition = WorldToFace.Inverse().TransformPosition(TransformedPosition - FVector(SnapX, SnapY, 0.0f));
		PreShadowTranslation = -SnappedWorldPosition;
	}

	check(MaxSubjectZ > MinSubjectZ);

	const float ClampedMaxLightW = FMath::Min(MinSubjectZ + Initializer.MaxDistanceToCastInLightW, (float)HALF_WORLD_MAX);
	MinPreSubjectZ = Initializer.MinLightW;

	const FMatrix SubjectMatrix = WorldToFace * FShadowProjectionMatrix(MinSubjectZ,MaxSubjectZ,Initializer.WAxis);
	const FMatrix PostSubjectMatrix = WorldToFace * FShadowProjectionMatrix(MinSubjectZ, ClampedMaxLightW, Initializer.WAxis);

	float MaxSubjectDepth = SubjectMatrix.TransformPosition(
		Initializer.SubjectBounds.Origin
		+ WorldToLightScaled.Inverse().TransformVector(Initializer.FaceDirection) * Initializer.SubjectBounds.SphereRadius
		).Z;

	if (Initializer.bOnePassPointLightShadow)
	{
		MaxSubjectDepth = Initializer.SubjectBounds.SphereRadius;
	}

	InvMaxSubjectDepth = 1.0f / MaxSubjectDepth;

	// Store the view matrix
	// Reorder the vectors to match the main view, since ShadowViewMatrix will be used to override the main view's view matrix during shadow depth rendering
	ShadowViewMatrix = Initializer.WorldToLight * 
		FMatrix(
			FPlane(0,	0,	1,	0),
			FPlane(1,	0,	0,	0),
			FPlane(0,	1,	0,	0),
			FPlane(0,	0,	0,	1));

	SubjectAndReceiverMatrix = SubjectMatrix,
	ReceiverMatrix = PostSubjectMatrix;
	InvReceiverMatrix = ReceiverMatrix.Inverse();

	if (Initializer.SplitIndex >= 0 && Initializer.bDirectionalLight)
	{
		checkSlow(InDependentView);
		ShadowBounds = InLightSceneInfo->Proxy->GetShadowSplitBounds(*InDependentView, SplitIndex, 0);
	}
	else
	{
		ShadowBounds = FSphere(-Initializer.PreShadowTranslation, Initializer.SubjectBounds.SphereRadius);
	}

	// Any meshes between the light and the subject can cast shadows, also any meshes inside the subject region
	const FMatrix CasterMatrix = WorldToFace * FShadowProjectionMatrix(Initializer.MinLightW,MaxSubjectZ,Initializer.WAxis);
	GetViewFrustumBounds(CasterFrustum,CasterMatrix,true);
	GetViewFrustumBounds(ReceiverFrustum,ReceiverMatrix,true);

	UpdateShaderDepthBias();
}

FProjectedShadowInfo::FProjectedShadowInfo(
	FLightSceneInfo* InLightSceneInfo,
	FViewInfo* InDependentView,
	const FRsmWholeSceneProjectedShadowInitializer& Initializer,
	uint32 InResolutionX,
	uint32 InResolutionY
	)
	:	LightSceneInfo(InLightSceneInfo)
	,	LightSceneInfoCompact(InLightSceneInfo)
	,	ParentSceneInfo(NULL)
	,	DependentView(InDependentView)
	,	ShadowId(INDEX_NONE)
	,	PreShadowTranslation(Initializer.PreShadowTranslation)
	,	CascadeSettings(Initializer.CascadeSettings)
	,	X(0)
	,	Y(0)
	,	ResolutionX(InResolutionX)
	,	ResolutionY(InResolutionY)
	,	MaxScreenPercent(1.0f)
	,   SplitIndex(0)
	,	bAllocated(false)
	,	bAllocatedInTranslucentLayout(false)
	,	bRendered(false)
	,	bAllocatedInPreshadowCache(false)
	,	bDepthsCached(false)
	,	bDirectionalLight(Initializer.bDirectionalLight)
	,	bWholeSceneShadow(true)
	,	bOnePassPointLightShadow(false)
	,	bReflectiveShadowmap(true) 
	,	bTranslucentShadow(false)
	,	bPreShadow(false)
	,	bValidTransform(true)
{
	FVector	XAxis, YAxis;
	Initializer.FaceDirection.FindBestAxisVectors(XAxis,YAxis);
	const FMatrix WorldToLightScaled = Initializer.WorldToLight * FScaleMatrix(Initializer.Scales);
	const FMatrix WorldToFace = WorldToLightScaled * FBasisVectorMatrix(-XAxis,YAxis,Initializer.FaceDirection.SafeNormal(),FVector::ZeroVector);

	MaxSubjectZ = WorldToFace.TransformPosition(Initializer.SubjectBounds.Origin).Z + Initializer.SubjectBounds.SphereRadius;

	MinSubjectZ = FMath::Max(MaxSubjectZ - Initializer.SubjectBounds.SphereRadius * 2,Initializer.MinLightW);

	static float Maxz = 0.0f;
	static float Minz = 0.0f;
	if ( Minz != 0.0f ) MinSubjectZ = Minz;
	if ( Maxz != 0.0f ) MaxSubjectZ = Maxz;


	const float ClampedMaxLightW = FMath::Min(MinSubjectZ + Initializer.MaxDistanceToCastInLightW, (float)HALF_WORLD_MAX);
	MinPreSubjectZ = Initializer.MinLightW;

	const FMatrix SubjectMatrix = WorldToFace * FShadowProjectionMatrix(MinSubjectZ,MaxSubjectZ,Initializer.WAxis);
	const FMatrix PostSubjectMatrix = WorldToFace * FShadowProjectionMatrix(MinSubjectZ, ClampedMaxLightW, Initializer.WAxis);

	// Quantise the RSM in shadow texel space
	static bool quantize = true;
	if ( quantize )
	{
		const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetReflectiveShadowMapTextureResolution();
		uint32 ShadowDepthBufferSizeX = ShadowBufferResolution.X ;
		uint32 ShadowDepthBufferSizeY = ShadowBufferResolution.Y ;
		// Transform the shadow's position into shadowmap space
		const FVector TransformedPosition = WorldToFace.TransformPosition(-PreShadowTranslation);

		// Largest amount that the shadowmap will be downsampled to during sampling
		// We need to take this into account when snapping to get a stable result
		// This corresponds to the maximum kernel filter size used by subsurface shadows in ShadowProjectionPixelShader.usf
		static int32 MaxDownsampleFactor = 4;
		// Determine the distance necessary to snap the shadow's position to the nearest texel
		const float SnapX = FMath::Fmod(TransformedPosition.X, 2.0f * MaxDownsampleFactor / ShadowDepthBufferSizeX);
		const float SnapY = FMath::Fmod(TransformedPosition.Y, 2.0f * MaxDownsampleFactor / ShadowDepthBufferSizeY);
		// Snap the shadow's position and transform it back into world space
		// This snapping prevents sub-texel camera movements which removes view dependent aliasing from the final shadow result
		// This only maintains stable shadows under camera translation and rotation
		const FVector SnappedWorldPosition = WorldToFace.Inverse().TransformPosition(TransformedPosition - FVector(SnapX, SnapY, 0.0f));
		PreShadowTranslation = -SnappedWorldPosition;
	}

	float MaxSubjectDepth = SubjectMatrix.TransformPosition(
		Initializer.SubjectBounds.Origin
		+ WorldToLightScaled.Inverse().TransformVector(Initializer.FaceDirection) * Initializer.SubjectBounds.SphereRadius
		).Z;

	InvMaxSubjectDepth = 1.0f / MaxSubjectDepth;

	// Store the view matrix
	// Reorder the vectors to match the main view, since ShadowViewMatrix will be used to override the main view's view matrix during shadow depth rendering
	ShadowViewMatrix = Initializer.WorldToLight * 
		FMatrix(
		FPlane(0,	0,	1,	0),
		FPlane(1,	0,	0,	0),
		FPlane(0,	1,	0,	0),
		FPlane(0,	0,	0,	1));

	SubjectAndReceiverMatrix = SubjectMatrix,
	ReceiverMatrix = PostSubjectMatrix;
	InvReceiverMatrix = ReceiverMatrix.Inverse();

	ShadowBounds = FSphere(-PreShadowTranslation, Initializer.SubjectBounds.SphereRadius);
	
	GetViewFrustumBounds(CasterFrustum, SubjectAndReceiverMatrix, true);
	GetViewFrustumBounds(ReceiverFrustum,ReceiverMatrix,true);

	UpdateShaderDepthBias();
}

void FProjectedShadowInfo::AddSubjectPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo, TArray<FViewInfo>* ViewArray)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AddSubjectPrimitive);

	if (!ReceiverPrimitives.Contains(PrimitiveSceneInfo))
	{
		TArray<FViewInfo*, TInlineAllocator<1> > Views;
		const bool bWholeSceneDirectionalShadow = IsWholeSceneDirectionalShadow();

		if (bWholeSceneDirectionalShadow)
		{
			Views.Add(DependentView);
		}
		else
		{
			check(ViewArray);

			for (int32 ViewIndex = 0; ViewIndex < ViewArray->Num(); ViewIndex++)
			{
				Views.Add(&(*ViewArray)[ViewIndex]);
			}
		}

		bool bOpaqueRelevance = false;
		bool bTranslucentRelevance = false;
		bool bShadowRelevance = false;
		bool bNeedsPreRenderView = false;
		uint32 ViewMask = 0;
		int32 PrimitiveId = PrimitiveSceneInfo->GetIndex();

		for (int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ViewIndex++)
		{
			FViewInfo& CurrentView = *Views[ViewIndex];
			FPrimitiveViewRelevance& ViewRelevance = CurrentView.PrimitiveViewRelevanceMap[PrimitiveId];

			if (!ViewRelevance.bInitializedThisFrame)
			{
				if( CurrentView.IsPerspectiveProjection() )
				{
					const FPrimitiveSceneProxy* Proxy = PrimitiveSceneInfo->Proxy;

					// Compute the distance between the view and the primitive.
					float DistanceSquared = (Proxy->GetBounds().Origin - CurrentView.ShadowViewMatrices.ViewOrigin).SizeSquared();

					bool bIsDistanceCulled = CurrentView.IsDistanceCulled(
						DistanceSquared,
						Proxy->GetMinDrawDistance(),
						Proxy->GetMaxDrawDistance(),
						PrimitiveSceneInfo
						);
					if( bIsDistanceCulled )
					{
						continue;
					}
				}

				// Compute the subject primitive's view relevance since it wasn't cached
				// Update the main view's PrimitiveViewRelevanceMap
				ViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(&CurrentView);

				bNeedsPreRenderView |= ViewRelevance.bNeedsPreRenderView;
				ViewMask |= (1 << ViewIndex);
			}

			bOpaqueRelevance |= ViewRelevance.bOpaqueRelevance;
			bTranslucentRelevance |= ViewRelevance.HasTranslucency();
			bShadowRelevance |= ViewRelevance.bShadowRelevance;
		}

		if (bNeedsPreRenderView)
		{
			// Call PreRenderView on primitives that weren't visible in any of the main views, but need to be rendered in this shadow's depth pass
			PrimitiveSceneInfo->Proxy->PreRenderView(Views[0]->Family, ViewMask, Views[0]->FrameNumber);
		}

		if (bOpaqueRelevance && bShadowRelevance)
		{
			const FPrimitiveSceneProxy* Proxy = PrimitiveSceneInfo->Proxy;
			const FBoxSphereBounds& Bounds = Proxy->GetBounds();
			bool bDrawingStaticMeshes = false;

			if (PrimitiveSceneInfo->StaticMeshes.Num() > 0)
			{
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					FViewInfo& CurrentView = *Views[ViewIndex];

					const float DistanceSquared = ( Bounds.Origin - CurrentView.ShadowViewMatrices.ViewOrigin ).SizeSquared();
					const bool bDrawShadowDepth = FMath::Square( Bounds.SphereRadius ) > FMath::Square( CVarMinScreenRadiusForShadowCaster.GetValueOnRenderThread() ) * DistanceSquared;
					if( !bDrawShadowDepth )
					{
						// cull object if it's too small to be considered as shadow caster
						continue;
					}

					// Update visibility for meshes which weren't visible in the main views or were visible with static relevance
					if (!CurrentView.PrimitiveVisibilityMap[PrimitiveId] || CurrentView.PrimitiveViewRelevanceMap[PrimitiveId].bStaticRelevance)
					{
						bool bUseExistingVisibility = false;

						if(!bReflectiveShadowmap) // Don't use existing visibility for RSMs 
						{
							for (int32 MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshes.Num(); MeshIndex++)
							{
								const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[MeshIndex];
								bool bMeshIsVisible = CurrentView.StaticMeshShadowDepthMap[StaticMesh.Id] && StaticMesh.CastShadow;
								bUseExistingVisibility = bUseExistingVisibility || bMeshIsVisible;

								if (bMeshIsVisible && bWholeSceneDirectionalShadow)
								{
									StaticMeshWholeSceneShadowDepthMap[StaticMesh.Id] = true;
									StaticMeshWholeSceneShadowBatchVisibility[StaticMesh.Id] = CurrentView.StaticMeshBatchVisibility[StaticMesh.Id];
								}
							}
						}

						if (bUseExistingVisibility)
						{
							bDrawingStaticMeshes = true;
						}
						// Don't overwrite visibility set by the main views
						// This is necessary to avoid popping when transitioning between LODs, because on the frame of the transition, 
						// The old LOD will continue to be drawn even though a different LOD would be chosen by distance.
						else
						{
							int8 LODToRender = 0;
							int32 ForcedLODLevel = (CurrentView.Family->EngineShowFlags.LOD) ? GetCVarForceLOD() : 0;

							// Add the primitive's static mesh elements to the draw lists.
							if ( bReflectiveShadowmap) 
							{
								LODToRender = -CHAR_MAX;
								// Force the lowest detail LOD Level in reflective shadow maps.
								for (int32 Index = 0; Index < PrimitiveSceneInfo->StaticMeshes.Num(); Index++)
								{
									LODToRender = FMath::Max<int8>(PrimitiveSceneInfo->StaticMeshes[Index].LODIndex, LODToRender);
								}
							}
							else
							{
								FPrimitiveBounds PrimitiveBounds;
								PrimitiveBounds.Origin = Bounds.Origin;
								PrimitiveBounds.SphereRadius = Bounds.SphereRadius;
								LODToRender = ComputeLODForMeshes(PrimitiveSceneInfo->StaticMeshes, CurrentView, PrimitiveBounds.Origin, PrimitiveBounds.SphereRadius, ForcedLODLevel);
							}

							for (int32 MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshes.Num(); MeshIndex++)
							{
								const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[MeshIndex];
								if (StaticMesh.CastShadow && StaticMesh.LODIndex == LODToRender)
								{
									if (bWholeSceneDirectionalShadow)
									{
										StaticMeshWholeSceneShadowDepthMap[StaticMesh.Id] = true;
										StaticMeshWholeSceneShadowBatchVisibility[StaticMesh.Id] = StaticMesh.Elements.Num() == 1 ? 1 : StaticMesh.VertexFactory->GetStaticBatchElementVisibility(*DependentView, &StaticMesh);
									}
									else
									{
										CurrentView.StaticMeshShadowDepthMap[StaticMesh.Id] = true;
										CurrentView.StaticMeshBatchVisibility[StaticMesh.Id] = StaticMesh.Elements.Num() == 1 ? 1 : StaticMesh.VertexFactory->GetStaticBatchElementVisibility(CurrentView, &StaticMesh);
									}

									bDrawingStaticMeshes = true;
								}
							}
						}
					}
				}
			}

			if (bDrawingStaticMeshes)
			{
				if (!bWholeSceneDirectionalShadow)
				{
					// Add the primitive's static mesh elements to the draw lists.
					for (int32 MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshes.Num(); MeshIndex++)
					{
						FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[MeshIndex];
						if (StaticMesh.CastShadow)
						{
							const FMaterialRenderProxy* MaterialRenderProxy = StaticMesh.MaterialRenderProxy;
							const FMaterial* Material = MaterialRenderProxy->GetMaterial(GRHIFeatureLevel);
							const EBlendMode BlendMode = Material->GetBlendMode();
							const EMaterialLightingModel LightingModel = Material->GetLightingModel();

							if(((!IsTranslucentBlendMode(BlendMode)) && LightingModel != MLM_Unlit) || (bReflectiveShadowmap && Material->ShouldInjectEmissiveIntoLPV())) 
							{
								const bool bTwoSided = Material->IsTwoSided() || PrimitiveSceneInfo->Proxy->CastsShadowAsTwoSided();
								OverrideWithDefaultMaterialForShadowDepth(MaterialRenderProxy, Material, bReflectiveShadowmap, GRHIFeatureLevel);
								SubjectMeshElements.Add(FShadowStaticMeshElement(MaterialRenderProxy, Material, &StaticMesh,bTwoSided));
							}
						}
					}
				}
			}
			else
			{
				// Add the primitive to the subject primitive list.
				SubjectPrimitives.Add(PrimitiveSceneInfo);
			}
		}

		// Add translucent shadow casting primitives to SubjectTranslucentPrimitives
		if (bTranslucentRelevance && bShadowRelevance && bTranslucentShadow)
		{
			SubjectTranslucentPrimitives.Add(PrimitiveSceneInfo);
		}
	}
}

bool FProjectedShadowInfo::HasSubjectPrims() const
{
	return SubjectPrimitives.Num() > 0 || SubjectMeshElements.Num() > 0;
}

void FProjectedShadowInfo::AddReceiverPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	// Add the primitive to the receiver primitive list.
	ReceiverPrimitives.Add(PrimitiveSceneInfo);
}

/** 
 * @param View view to check visibility in
 * @return true if this shadow info has any subject prims visible in the view
 */
bool FProjectedShadowInfo::SubjectsVisible(const FViewInfo& View) const
{
	checkSlow(!IsWholeSceneDirectionalShadow());
	for(int32 PrimitiveIndex = 0;PrimitiveIndex < SubjectPrimitives.Num();PrimitiveIndex++)
	{
		const FPrimitiveSceneInfo* SubjectPrimitiveSceneInfo = SubjectPrimitives[PrimitiveIndex];
		if(View.PrimitiveVisibilityMap[SubjectPrimitiveSceneInfo->GetIndex()])
		{
			return true;
		}
	}
	return false;
}

/** Clears arrays allocated with the scene rendering allocator. */
void FProjectedShadowInfo::ClearTransientArrays()
{
	SubjectTranslucentPrimitives.Empty();
	SubjectPrimitives.Empty();
	ReceiverPrimitives.Empty();
	SubjectMeshElements.Empty();
}

/** Returns a cached preshadow matching the input criteria if one exists. */
TRefCountPtr<FProjectedShadowInfo> FDeferredShadingSceneRenderer::GetCachedPreshadow(
	const FLightPrimitiveInteraction* InParentInteraction, 
	const FProjectedShadowInitializer& Initializer,
	const FBoxSphereBounds& Bounds,
	uint32 InResolutionX)
{
	if (ShouldUseCachePreshadows() && !Views[0].bIsSceneCapture)
	{
		const FPrimitiveSceneInfo* PrimitiveInfo = InParentInteraction->GetPrimitiveSceneInfo();
		const FLightSceneInfo* LightInfo = InParentInteraction->GetLight();
		const FSphere QueryBounds(Bounds.Origin, Bounds.SphereRadius);

		for (int32 ShadowIndex = 0; ShadowIndex < Scene->CachedPreshadows.Num(); ShadowIndex++)
		{
			TRefCountPtr<FProjectedShadowInfo> CachedShadow = Scene->CachedPreshadows[ShadowIndex];
			// Only reuse a cached preshadow if it was created for the same primitive and light
			if (CachedShadow->ParentSceneInfo == PrimitiveInfo
				&& CachedShadow->LightSceneInfo == LightInfo
				// Only reuse if it contains the bounds being queried, with some tolerance
				&& QueryBounds.IsInside(CachedShadow->ShadowBounds, CachedShadow->ShadowBounds.W * .04f)
				// Only reuse if the resolution matches
				&& CachedShadow->ResolutionX == InResolutionX
				&& CachedShadow->bAllocated)
			{
				// Reset any allocations using the scene rendering allocator, 
				// Since those will point to freed memory now that we are using the shadow on a different frame than it was created on.
				CachedShadow->ClearTransientArrays();
				return CachedShadow;
			}
		}
	}
	// No matching cached preshadow was found
	return NULL;
}

struct FComparePreshadows
{
	FORCEINLINE bool operator()(const TRefCountPtr<FProjectedShadowInfo>& A, const TRefCountPtr<FProjectedShadowInfo>& B) const
	{
		if (B->ResolutionX * B->ResolutionY < A->ResolutionX * A->ResolutionY)
		{
			return true;
		}

		return false;
	}
};

/** Removes stale shadows and attempts to add new preshadows to the cache. */
void FDeferredShadingSceneRenderer::UpdatePreshadowCache()
{
	if (ShouldUseCachePreshadows() && !Views[0].bIsSceneCapture)
	{
		if (Scene->PreshadowCacheLayout.GetSizeX() == 0)
		{
			// Initialize the texture layout if necessary
			const FIntPoint PreshadowCacheBufferSize = GSceneRenderTargets.GetPreShadowCacheTextureResolution();
			Scene->PreshadowCacheLayout = FTextureLayout(1, 1, PreshadowCacheBufferSize.X, PreshadowCacheBufferSize.Y, false, false);
		}

		// Iterate through the cached preshadows, removing those that are not going to be rendered this frame
		for (int32 CachedShadowIndex = Scene->CachedPreshadows.Num() - 1; CachedShadowIndex >= 0; CachedShadowIndex--)
		{
			TRefCountPtr<FProjectedShadowInfo> CachedShadow = Scene->CachedPreshadows[CachedShadowIndex];
			bool bShadowBeingRenderedThisFrame = false;

			for (int32 LightIndex = 0; LightIndex < VisibleLightInfos.Num() && !bShadowBeingRenderedThisFrame; LightIndex++)
			{
				bShadowBeingRenderedThisFrame = VisibleLightInfos[LightIndex].ProjectedPreShadows.Find(CachedShadow) != INDEX_NONE;
			}

			if (!bShadowBeingRenderedThisFrame)
			{
				// Must succeed, since we added it to the layout earlier
				verify(Scene->PreshadowCacheLayout.RemoveElement(
					CachedShadow->X,
					CachedShadow->Y,
					CachedShadow->ResolutionX + SHADOW_BORDER * 2,
					CachedShadow->ResolutionY + SHADOW_BORDER * 2));
				Scene->CachedPreshadows.RemoveAt(CachedShadowIndex);
			}
			else if (GSceneRenderTargets.bPreshadowCacheNewlyAllocated)
			{
				CachedShadow->bDepthsCached = false;
			}
		}

		GSceneRenderTargets.bPreshadowCacheNewlyAllocated = false;

		TArray<TRefCountPtr<FProjectedShadowInfo>, SceneRenderingAllocator> UncachedPreShadows;

		// Gather a list of preshadows that can be cached
		for (int32 LightIndex = 0; LightIndex < VisibleLightInfos.Num(); LightIndex++)
		{
			for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfos[LightIndex].ProjectedPreShadows.Num(); ShadowIndex++)
			{
				TRefCountPtr<FProjectedShadowInfo> CurrentShadow = VisibleLightInfos[LightIndex].ProjectedPreShadows[ShadowIndex];
				checkSlow(CurrentShadow->bPreShadow);

				if (!CurrentShadow->bAllocatedInPreshadowCache)
				{
					UncachedPreShadows.Add(CurrentShadow);
				}
			}
		}

		// Sort them from largest to smallest, based on the assumption that larger preshadows will have more objects in their depth only pass
		UncachedPreShadows.Sort(FComparePreshadows());

		for (int32 ShadowIndex = 0; ShadowIndex < UncachedPreShadows.Num(); ShadowIndex++)
		{
			TRefCountPtr<FProjectedShadowInfo> CurrentShadow = UncachedPreShadows[ShadowIndex];

			// Try to find space for the preshadow in the texture layout
			if (Scene->PreshadowCacheLayout.AddElement(
				CurrentShadow->X,
				CurrentShadow->Y,
				CurrentShadow->ResolutionX + SHADOW_BORDER * 2,
				CurrentShadow->ResolutionY + SHADOW_BORDER * 2))
			{
				// Mark the preshadow as existing in the cache
				// It must now use the preshadow cache render target to render and read its depths instead of the usual shadow depth buffers
				CurrentShadow->bAllocatedInPreshadowCache = true;
				// Indicate that the shadow's X and Y have been initialized
				CurrentShadow->bAllocated = true;
				Scene->CachedPreshadows.Add(CurrentShadow);
			}
		}
	}
}

bool FDeferredShadingSceneRenderer::ShouldCreateObjectShadowForStationaryLight(const FLightSceneInfo* LightSceneInfo, const FPrimitiveSceneProxy* PrimitiveSceneProxy, bool bInteractionShadowMapped) const
{
	const bool bCreateObjectShadowForStationaryLight = 
		LightSceneInfo->bCreatePerObjectShadowsForDynamicObjects 
		&& LightSceneInfo->bPrecomputedLightingIsValid
		&& LightSceneInfo->Proxy->GetShadowMapChannel() != INDEX_NONE
		// Create a per-object shadow if the object does not want static lighting and needs to integrate with the static shadowing of a stationary light
		// Or if the object wants static lighting but does not have a built shadowmap (Eg has been moved in the editor)
		&& (!PrimitiveSceneProxy->HasStaticLighting() || !bInteractionShadowMapped);

	return bCreateObjectShadowForStationaryLight;
}

void FDeferredShadingSceneRenderer::SetupInteractionShadows(
	FLightPrimitiveInteraction* Interaction, 
	FVisibleLightInfo& VisibleLightInfo, 
	bool bReflectionCaptureScene,
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows,
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& PreShadows)
{
	FPrimitiveSceneInfo* PrimitiveSceneInfo = Interaction->GetPrimitiveSceneInfo();
	FLightSceneProxy* LightProxy = Interaction->GetLight()->Proxy;
	extern bool GUseTranslucencyShadowDepths;

	bool bShadowHandledByParent = false;

	if (PrimitiveSceneInfo->LightingAttachmentRoot.IsValid())
	{
		FAttachmentGroupSceneInfo& AttachmentGroup = Scene->AttachmentGroups.FindChecked(PrimitiveSceneInfo->LightingAttachmentRoot);
		bShadowHandledByParent = AttachmentGroup.ParentSceneInfo && AttachmentGroup.ParentSceneInfo->Proxy->LightAttachmentsAsGroup();
	}

	// Shadowing for primitives with a shadow parent will be handled by that shadow parent
	if (!bShadowHandledByParent)
	{
		const bool bCreateTranslucentObjectShadow = GUseTranslucencyShadowDepths && Interaction->HasTranslucentObjectShadow();
		const bool bCreateInsetObjectShadow = Interaction->HasInsetObjectShadow();
		const bool bCreateObjectShadowForStationaryLight = ShouldCreateObjectShadowForStationaryLight(Interaction->GetLight(), PrimitiveSceneInfo->Proxy, Interaction->IsShadowMapped());

		if (Interaction->HasShadow() 
			// Only render shadows from objects that use static lighting during a reflection capture, since the reflection capture doesn't update at runtime
			&& (!bReflectionCaptureScene || PrimitiveSceneInfo->Proxy->HasStaticLighting())
			&& (bCreateTranslucentObjectShadow || bCreateInsetObjectShadow || bCreateObjectShadowForStationaryLight))
		{
			// Create projected shadow infos
			CreatePerObjectProjectedShadow(Interaction, bCreateTranslucentObjectShadow, bCreateInsetObjectShadow || bCreateObjectShadowForStationaryLight, ViewDependentWholeSceneShadows, PreShadows);
		}
	}
}

void FDeferredShadingSceneRenderer::CreatePerObjectProjectedShadow(
	FLightPrimitiveInteraction* Interaction, 
	bool bCreateTranslucentObjectShadow, 
	bool bCreateOpaqueObjectShadow,
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows,
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& OutPreShadows)
{
	check(bCreateOpaqueObjectShadow || bCreateTranslucentObjectShadow);
	FPrimitiveSceneInfo* PrimitiveSceneInfo = Interaction->GetPrimitiveSceneInfo();
	const int32 PrimitiveId = PrimitiveSceneInfo->GetIndex();

	FLightSceneInfo* LightSceneInfo = Interaction->GetLight();
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

	// Check if the shadow is visible in any of the views.
	bool bShadowIsPotentiallyVisibleNextFrame = false;
	bool bOpaqueShadowIsVisibleThisFrame = false;
	bool bSubjectIsVisible = false;
	bool bOpaqueRelevance = false;
	bool bTranslucentRelevance = false;
	bool bTranslucentShadowIsVisibleThisFrame = false;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		// Lookup the primitive's cached view relevance
		FPrimitiveViewRelevance ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveId];

		if (!ViewRelevance.bInitializedThisFrame)
		{
			// Compute the subject primitive's view relevance since it wasn't cached
			ViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(&View);
		}

		// Check if the subject primitive is shadow relevant.
		const bool bPrimitiveIsShadowRelevant = ViewRelevance.bShadowRelevance;

		// Check if the shadow and preshadow are occluded.
		const bool bOpaqueShadowIsOccluded = 
			!bCreateOpaqueObjectShadow ||
			(
				!View.bIgnoreExistingQueries &&	View.State &&
				((FSceneViewState*)View.State)->IsShadowOccluded(PrimitiveSceneInfo->PrimitiveComponentId, LightSceneInfo->Proxy->GetLightComponent(), INDEX_NONE, false)
			);

		const bool bTranslucentShadowIsOccluded = 
			!bCreateTranslucentObjectShadow ||
			(
				!View.bIgnoreExistingQueries && View.State &&
				((FSceneViewState*)View.State)->IsShadowOccluded(PrimitiveSceneInfo->PrimitiveComponentId, LightSceneInfo->Proxy->GetLightComponent(), INDEX_NONE, true)
			);

		const bool bSubjectIsVisibleInThisView = View.PrimitiveVisibilityMap[PrimitiveSceneInfo->GetIndex()];
		bSubjectIsVisible |= bSubjectIsVisibleInThisView;

		// The shadow is visible if it is view relevant and unoccluded.
		bOpaqueShadowIsVisibleThisFrame |= (bPrimitiveIsShadowRelevant && !bOpaqueShadowIsOccluded);
		bTranslucentShadowIsVisibleThisFrame |= (bPrimitiveIsShadowRelevant && !bTranslucentShadowIsOccluded);
		bShadowIsPotentiallyVisibleNextFrame |= bPrimitiveIsShadowRelevant;
		bOpaqueRelevance |= ViewRelevance.bOpaqueRelevance;
		bTranslucentRelevance |= ViewRelevance.HasTranslucency();
	}

	if (!bOpaqueShadowIsVisibleThisFrame && !bTranslucentShadowIsVisibleThisFrame && !bShadowIsPotentiallyVisibleNextFrame)
	{
		// Don't setup the shadow info for shadows which don't need to be rendered or occlusion tested.
		return;
	}

	TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator> ShadowGroupPrimitives;
	PrimitiveSceneInfo->GatherLightingAttachmentGroupPrimitives(ShadowGroupPrimitives);

	// Compute the composite bounds of this group of shadow primitives.
	FBoxSphereBounds OriginalBounds = ShadowGroupPrimitives[0]->Proxy->GetBounds();

	for (int32 ChildIndex = 1; ChildIndex < ShadowGroupPrimitives.Num(); ChildIndex++)
	{
		const FPrimitiveSceneInfo* ShadowChild = ShadowGroupPrimitives[ChildIndex];
		OriginalBounds = OriginalBounds + ShadowChild->Proxy->GetBounds();
	}
	
	// Shadowing constants.
	const uint32 MinShadowResolution = CVarMinShadowResolution.GetValueOnRenderThread();
	const uint32 MaxShadowResolutionSetting = GetCachedScalabilityCVars().MaxShadowResolution;
	const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetShadowDepthTextureResolution();
	const uint32 MaxShadowResolution = FMath::Min<int32>(MaxShadowResolutionSetting, ShadowBufferResolution.X) - SHADOW_BORDER * 2;
	const uint32 MaxShadowResolutionY = FMath::Min<int32>(MaxShadowResolutionSetting, ShadowBufferResolution.Y) - SHADOW_BORDER * 2;
	const int32 ShadowFadeResolution = CVarShadowFadeResolution.GetValueOnRenderThread();

	// Compute the maximum resolution required for the shadow by any view. Also keep track of the unclamped resolution for fading.
	uint32 MaxDesiredResolution = 0;
	uint32 MaxUnclampedResolution	= 0;
	float MaxScreenPercent = 0;
	TArray<float, TInlineAllocator<2> > ResolutionFadeAlphas;
	TArray<float, TInlineAllocator<2> > ResolutionPreShadowFadeAlphas;
	float MaxResolutionFadeAlpha = 0;
	float MaxResolutionPreShadowFadeAlpha = 0;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		// Stereo renders at half horizontal resolution, but compute shadow resolution based on full resolution.
		const bool bStereo = View.StereoPass != eSSP_FULL;
		const float ScreenXScale = bStereo ? 2.0f : 1.0f;

		// Determine the size of the subject's bounding sphere in this view.
		const FVector4 ScreenPosition = View.WorldToScreen(OriginalBounds.Origin);
		const float ScreenRadius = FMath::Max(
			ScreenXScale * View.ViewRect.Size().X / 2.0f * View.ShadowViewMatrices.ProjMatrix.M[0][0],
			View.ViewRect.Size().Y / 2.0f * View.ShadowViewMatrices.ProjMatrix.M[1][1]
			) *
			OriginalBounds.SphereRadius /
			FMath::Max(ScreenPosition.W,1.0f);

		const float ScreenPercent = FMath::Max(
			1.0f / 2.0f * View.ShadowViewMatrices.ProjMatrix.M[0][0],
			1.0f / 2.0f * View.ShadowViewMatrices.ProjMatrix.M[1][1]
			) *
			OriginalBounds.SphereRadius /
			FMath::Max(ScreenPosition.W,1.0f);

		MaxScreenPercent = FMath::Max(MaxScreenPercent, ScreenPercent);

		// Determine the amount of shadow buffer resolution needed for this view.
		const uint32 UnclampedResolution = FMath::TruncToInt(ScreenRadius * CVarShadowTexelsPerPixel.GetValueOnRenderThread());
		MaxUnclampedResolution = FMath::Max( MaxUnclampedResolution, UnclampedResolution );
		MaxDesiredResolution = FMath::Max(
			MaxDesiredResolution,
			FMath::Clamp<uint32>(
				UnclampedResolution,
				FMath::Min<int32>(MinShadowResolution,ShadowBufferResolution.X - SHADOW_BORDER * 2),
				MaxShadowResolution
				)
			);

		// Calculate fading based on resolution
		const float ViewSpecificAlpha = CalculateShadowFadeAlpha( UnclampedResolution, ShadowFadeResolution, MinShadowResolution );
		MaxResolutionFadeAlpha = FMath::Max(MaxResolutionFadeAlpha, ViewSpecificAlpha);
		ResolutionFadeAlphas.Add(ViewSpecificAlpha);

		const float ViewSpecificPreShadowAlpha = CalculateShadowFadeAlpha( FMath::TruncToInt(UnclampedResolution * CVarPreShadowResolutionFactor.GetValueOnRenderThread()), CVarPreShadowFadeResolution.GetValueOnRenderThread(), CVarMinPreShadowResolution.GetValueOnRenderThread() );
		MaxResolutionPreShadowFadeAlpha = FMath::Max(MaxResolutionPreShadowFadeAlpha, ViewSpecificPreShadowAlpha);
		ResolutionPreShadowFadeAlphas.Add(ViewSpecificPreShadowAlpha);
	}

	FBoxSphereBounds Bounds = OriginalBounds;

	const bool bRenderPreShadow = 
		CVarAllowPreshadows.GetValueOnRenderThread() 
		// Preshadow only affects the subject's pixels
		&& bSubjectIsVisible 
		// Only objects with dynamic lighting should create a preshadow
		// Unless we're in the editor and need to preview an object without built lighting
		&& (!PrimitiveSceneInfo->Proxy->HasStaticLighting() || !Interaction->IsShadowMapped());

	if (bRenderPreShadow && ShouldUseCachePreshadows())
	{
		float PreshadowExpandFraction = FMath::Max(CVarPreshadowExpandFraction.GetValueOnRenderThread(), 0.0f);

		// If we're creating a preshadow, expand the bounds somewhat so that the preshadow will be cached more often as the shadow caster moves around.
		//@todo - only expand the preshadow bounds for this, not the per object shadow.
		Bounds.SphereRadius += (Bounds.BoxExtent * PreshadowExpandFraction).Size();
		Bounds.BoxExtent *= PreshadowExpandFraction + 1.0f;
	}

	// Compute the projected shadow initializer for this primitive-light pair.
	FPerObjectProjectedShadowInitializer ShadowInitializer;

	if ((MaxResolutionFadeAlpha > 1.0f / 256.0f || (bRenderPreShadow && MaxResolutionPreShadowFadeAlpha > 1.0f / 256.0f))
		&& LightSceneInfo->Proxy->GetPerObjectProjectedShadowInitializer(Bounds, ShadowInitializer))
	{
		const float MaxFadeAlpha = MaxResolutionFadeAlpha;

		// Only create a shadow from this object if it hasn't completely faded away
		if (CVarAllowPerObjectShadows.GetValueOnRenderThread() && MaxFadeAlpha > 1.0f / 256.0f)
		{
			// Round down to the nearest power of two so that resolution changes are always doubling or halving the resolution, which increases filtering stability
			// Use the max resolution if the desired resolution is larger than that
			const int32 SizeX = MaxDesiredResolution >= MaxShadowResolution ? MaxShadowResolution : (1 << (FMath::CeilLogTwo(MaxDesiredResolution) - 1));

			if (bOpaqueRelevance && bCreateOpaqueObjectShadow && (bOpaqueShadowIsVisibleThisFrame || bShadowIsPotentiallyVisibleNextFrame))
			{
				// Create a projected shadow for this interaction's shadow.
				FProjectedShadowInfo* ProjectedShadowInfo = new(FMemStack::Get(),1,16) FProjectedShadowInfo(
					LightSceneInfo,
					PrimitiveSceneInfo,
					ShadowInitializer,
					false,
					SizeX,
					MaxShadowResolutionY,
					MaxScreenPercent,
					ResolutionFadeAlphas,
					false
					);
				VisibleLightInfo.MemStackProjectedShadows.Add(ProjectedShadowInfo);

				if (ProjectedShadowInfo->bValidTransform)
				{
					if (bOpaqueShadowIsVisibleThisFrame)
					{
						VisibleLightInfo.AllProjectedShadows.Add(ProjectedShadowInfo);

						for (int32 ChildIndex = 0; ChildIndex < ShadowGroupPrimitives.Num(); ChildIndex++)
						{
							FPrimitiveSceneInfo* ShadowChild = ShadowGroupPrimitives[ChildIndex];
							ProjectedShadowInfo->AddSubjectPrimitive(ShadowChild, &Views);
						}
					}
					else if (bShadowIsPotentiallyVisibleNextFrame)
					{
						VisibleLightInfo.OccludedPerObjectShadows.Add(ProjectedShadowInfo);
					}
				}
			}

			if (bTranslucentRelevance
				&& Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM4
				&& bCreateTranslucentObjectShadow 
				&& (bTranslucentShadowIsVisibleThisFrame || bShadowIsPotentiallyVisibleNextFrame))
			{
				// Create a projected shadow for this interaction's shadow.
				FProjectedShadowInfo* ProjectedShadowInfo = new(FMemStack::Get(),1,16) FProjectedShadowInfo(
					LightSceneInfo,
					PrimitiveSceneInfo,
					ShadowInitializer,
					false,
					SizeX,
					MaxShadowResolutionY,
					MaxScreenPercent,
					ResolutionFadeAlphas,
					true
					);
				VisibleLightInfo.MemStackProjectedShadows.Add(ProjectedShadowInfo);

				if (ProjectedShadowInfo->bValidTransform)
				{
					if (bTranslucentShadowIsVisibleThisFrame)
					{
						VisibleLightInfo.AllProjectedShadows.Add(ProjectedShadowInfo);

						for (int32 ChildIndex = 0; ChildIndex < ShadowGroupPrimitives.Num(); ChildIndex++)
						{
							FPrimitiveSceneInfo* ShadowChild = ShadowGroupPrimitives[ChildIndex];
							ProjectedShadowInfo->AddSubjectPrimitive(ShadowChild, &Views);
						}
					}
					else if (bShadowIsPotentiallyVisibleNextFrame)
					{
						VisibleLightInfo.OccludedPerObjectShadows.Add(ProjectedShadowInfo);
					}
				}
			}
		}

		const float MaxPreFadeAlpha = MaxResolutionPreShadowFadeAlpha;

		// If the subject is visible in at least one view, create a preshadow for static primitives shadowing the subject.
		if (MaxPreFadeAlpha > 1.0f / 256.0f 
			&& bRenderPreShadow
			&& bOpaqueRelevance)
		{
			// Round down to the nearest power of two so that resolution changes are always doubling or halving the resolution, which increases filtering stability.
			int32 PreshadowSizeX = 1 << (FMath::CeilLogTwo(FMath::TruncToInt(MaxDesiredResolution * CVarPreShadowResolutionFactor.GetValueOnRenderThread())) - 1);

			const FIntPoint PreshadowCacheResolution = GSceneRenderTargets.GetPreShadowCacheTextureResolution();
			checkSlow(PreshadowSizeX <= PreshadowCacheResolution.X);
			bool bIsOutsideWholeSceneShadow = true;

			for (int32 i = 0; i < ViewDependentWholeSceneShadows.Num(); i++)
			{
				const FProjectedShadowInfo* WholeSceneShadow = ViewDependentWholeSceneShadows[i];
				const FVector2D DistanceFadeValues = WholeSceneShadow->LightSceneInfo->Proxy->GetDirectionalLightDistanceFadeParameters();
				const float DistanceFromShadowCenterSquared = (WholeSceneShadow->ShadowBounds.Center - Bounds.Origin).SizeSquared();
				//@todo - if view dependent whole scene shadows are ever supported in splitscreen, 
				// We can only disable the preshadow at this point if it is inside a whole scene shadow for all views
				const float DistanceFromViewSquared = ((FVector)WholeSceneShadow->DependentView->ShadowViewMatrices.ViewOrigin - Bounds.Origin).SizeSquared();
				// Mark the preshadow as inside the whole scene shadow if its bounding sphere is inside the near fade distance
				if (DistanceFromShadowCenterSquared < FMath::Square(FMath::Max(WholeSceneShadow->ShadowBounds.W - Bounds.SphereRadius, 0.0f))
					//@todo - why is this extra threshold required?
					&& DistanceFromViewSquared < FMath::Square(FMath::Max(DistanceFadeValues.X - 200.0f - Bounds.SphereRadius, 0.0f)))
				{
					bIsOutsideWholeSceneShadow = false;
					break;
				}
			}

			// Only create opaque preshadows when part of the caster is outside the whole scene shadow.
			if (bIsOutsideWholeSceneShadow)
			{
				// Try to reuse a preshadow from the cache
				TRefCountPtr<FProjectedShadowInfo> ProjectedPreShadowInfo = GetCachedPreshadow(Interaction, ShadowInitializer, OriginalBounds, PreshadowSizeX);

				if (ProjectedPreShadowInfo)
				{
					// Update fade alpha on the cached preshadow
					ProjectedPreShadowInfo->FadeAlphas = ResolutionPreShadowFadeAlphas;
				}
				else
				{
					// Create a new projected shadow for this interaction's preshadow
					// Not using the scene rendering mem stack because this shadow info may need to persist for multiple frames if it gets cached
					ProjectedPreShadowInfo = new FProjectedShadowInfo(
						LightSceneInfo,
						PrimitiveSceneInfo,
						ShadowInitializer,
						true,
						PreshadowSizeX,
						FMath::TruncToInt(MaxShadowResolutionY * CVarPreShadowResolutionFactor.GetValueOnRenderThread()),
						MaxScreenPercent,
						ResolutionPreShadowFadeAlphas,
						false
						);
				}

				VisibleLightInfo.AllProjectedShadows.Add(ProjectedPreShadowInfo);
				VisibleLightInfo.ProjectedPreShadows.Add(ProjectedPreShadowInfo);

				// Only add to OutPreShadows if the preshadow doesn't already have depths cached, 
				// Since OutPreShadows is used to generate information only used when rendering the shadow depths.
				if (!ProjectedPreShadowInfo->bDepthsCached)
				{
					OutPreShadows.Add(ProjectedPreShadowInfo);
				}

				for (int32 ChildIndex = 0; ChildIndex < ShadowGroupPrimitives.Num(); ChildIndex++)
				{
					FPrimitiveSceneInfo* ShadowChild = ShadowGroupPrimitives[ChildIndex];
					ProjectedPreShadowInfo->AddReceiverPrimitive(ShadowChild);
				}
			}
		}
	}
}

/**  Creates a projected shadow for all primitives affected by a light.  If the light doesn't support whole-scene shadows, it returns false.
 * @param LightSceneInfo - The light to create a shadow for.
 * @return true if a whole scene shadow was created
 */
void FDeferredShadingSceneRenderer::CreateWholeSceneProjectedShadow(FLightSceneInfo* LightSceneInfo)
{
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

	// Try to create a whole-scene projected shadow initializer for the light.
	TArray<FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> > ProjectedShadowInitializers;
	if (LightSceneInfo->Proxy->GetWholeSceneProjectedShadowInitializer(ViewFamily, ProjectedShadowInitializers))
	{
		checkSlow(ProjectedShadowInitializers.Num() > 0);

		// Shadow resolution constants.
		const uint32 EffectiveDoubleShadowBorder = ProjectedShadowInitializers[0].bOnePassPointLightShadow ? 0 : SHADOW_BORDER * 2;
		const int32 MinShadowResolution = CVarMinShadowResolution.GetValueOnRenderThread();
		const int32 MaxShadowResolutionSetting = GetCachedScalabilityCVars().MaxShadowResolution;
		const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetShadowDepthTextureResolution();
		const uint32 MaxShadowResolution = FMath::Min(MaxShadowResolutionSetting, ShadowBufferResolution.X) - EffectiveDoubleShadowBorder;
		const uint32 MaxShadowResolutionY = FMath::Min(MaxShadowResolutionSetting, ShadowBufferResolution.Y) - EffectiveDoubleShadowBorder;
		const int32 ShadowFadeResolution = CVarShadowFadeResolution.GetValueOnRenderThread();

		// Compute the maximum resolution required for the shadow by any view. Also keep track of the unclamped resolution for fading.
		uint32 MaxDesiredResolution = 0;
		uint32 MaxUnclampedResolution	= 0;
		TArray<float, TInlineAllocator<2> > FadeAlphas;
		float MaxFadeAlpha = 0;
		bool bReflectionCaptureScene = false;

		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			// Stereo renders at half horizontal resolution, but compute shadow resolution based on full resolution.
			const bool bStereo = View.StereoPass != eSSP_FULL;
			const float ScreenXScale = bStereo ? 2.0f : 1.0f;

			// Determine the size of the light's bounding sphere in this view.
			const FVector4 ScreenPosition = View.WorldToScreen(LightSceneInfo->Proxy->GetOrigin());
			const float ScreenRadius = FMath::Max(
				ScreenXScale * View.ViewRect.Width() / 2.0f * View.ShadowViewMatrices.ProjMatrix.M[0][0],
				View.ViewRect.Height() / 2.0f * View.ShadowViewMatrices.ProjMatrix.M[1][1]
				) *
				LightSceneInfo->Proxy->GetRadius() /
				FMath::Max(ScreenPosition.W,1.0f);

			// Determine the amount of shadow buffer resolution needed for this view.
			const uint32 UnclampedResolution = FMath::TruncToInt(ScreenRadius * CVarShadowTexelsPerPixel.GetValueOnRenderThread());
			MaxUnclampedResolution = FMath::Max( MaxUnclampedResolution, UnclampedResolution );
			MaxDesiredResolution = FMath::Max(
				MaxDesiredResolution,
				FMath::Clamp<uint32>(
					UnclampedResolution,
					FMath::Min<int32>(MinShadowResolution,ShadowBufferResolution.X - EffectiveDoubleShadowBorder),
					MaxShadowResolution
					)
				);

			bReflectionCaptureScene = bReflectionCaptureScene || View.bIsReflectionCapture;

			const float FadeAlpha = CalculateShadowFadeAlpha( MaxUnclampedResolution, ShadowFadeResolution, MinShadowResolution );
			MaxFadeAlpha = FMath::Max(MaxFadeAlpha, FadeAlpha);
			FadeAlphas.Add(FadeAlpha);
		}

		if (MaxFadeAlpha > 1.0f / 256.0f)
		{
			for (int32 ShadowIndex = 0; ShadowIndex < ProjectedShadowInitializers.Num(); ShadowIndex++)
			{
				const FWholeSceneProjectedShadowInitializer& ProjectedShadowInitializer = ProjectedShadowInitializers[ShadowIndex];

				// Round down to the nearest power of two so that resolution changes are always doubling or halving the resolution, which increases filtering stability
				// Use the max resolution if the desired resolution is larger than that
				int32 SizeX = MaxDesiredResolution >= MaxShadowResolution ? MaxShadowResolution : (1 << (FMath::CeilLogTwo(MaxDesiredResolution) - 1));
				const uint32 DesiredSizeY = FMath::TruncToInt(MaxDesiredResolution);
				int32 SizeY = DesiredSizeY >= MaxShadowResolutionY ? MaxShadowResolutionY : (1 << (FMath::CeilLogTwo(DesiredSizeY) - 1));

				if (ProjectedShadowInitializer.bOnePassPointLightShadow)
				{
					// Round to a resolution that is supported for one pass point light shadows
					SizeX = SizeY = GSceneRenderTargets.GetCubeShadowDepthZResolution(GSceneRenderTargets.GetCubeShadowDepthZIndex(MaxDesiredResolution));
				}

				// Create the projected shadow info.
				FProjectedShadowInfo* ProjectedShadowInfo = new(FMemStack::Get(),1,16) FProjectedShadowInfo(
					LightSceneInfo,
					NULL,
					ProjectedShadowInitializer,
					SizeX,
					SizeY,
					FadeAlphas
					);
				VisibleLightInfo.MemStackProjectedShadows.Add(ProjectedShadowInfo);
				VisibleLightInfo.AllProjectedShadows.Add(ProjectedShadowInfo);

				if (ProjectedShadowInitializer.bOnePassPointLightShadow)
				{
					const static FVector CubeDirections[6] = 
					{
						FVector(-1, 0, 0),
						FVector(1, 0, 0),
						FVector(0, -1, 0),
						FVector(0, 1, 0),
						FVector(0, 0, -1),
						FVector(0, 0, 1)
					};

					const static FVector UpVectors[6] = 
					{
						FVector(0, 1, 0),
						FVector(0, 1, 0),
						FVector(0, 0, -1),
						FVector(0, 0, 1),
						FVector(0, 1, 0),
						FVector(0, 1, 0)
					};

					const FMatrix FaceProjection = FPerspectiveMatrix(PI / 4.0f, 1, 1, 1, ProjectedShadowInfo->LightSceneInfo->Proxy->GetRadius());
					const FVector LightPosition = ProjectedShadowInfo->LightSceneInfo->Proxy->GetPosition();
					
					ProjectedShadowInfo->OnePassShadowViewProjectionMatrices.Empty(6);
					ProjectedShadowInfo->OnePassShadowFrustums.Empty(6);
					ProjectedShadowInfo->OnePassShadowFrustums.AddZeroed(6);
					const FMatrix ScaleMatrix = FScaleMatrix(FVector(1, -1, 1));
					for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
					{
						// Create a view projection matrix for each cube face
						const FMatrix ShadowViewProjectionMatrix = FLookAtMatrix(LightPosition, LightPosition + CubeDirections[FaceIndex], UpVectors[FaceIndex]) * ScaleMatrix * FaceProjection;
						ProjectedShadowInfo->OnePassShadowViewProjectionMatrices.Add(ShadowViewProjectionMatrix);
						// Create a convex volume out of the frustum so it can be used for object culling
						GetViewFrustumBounds(ProjectedShadowInfo->OnePassShadowFrustums[FaceIndex], ShadowViewProjectionMatrix, false);
					}
				}

				// Add all the shadow casting primitives affected by the light to the shadow's subject primitive list.
				for(FLightPrimitiveInteraction* Interaction = LightSceneInfo->DynamicPrimitiveList;
					Interaction;
					Interaction = Interaction->GetNextPrimitive())
				{
					if (Interaction->HasShadow() 
						&& (!bReflectionCaptureScene || Interaction->GetPrimitiveSceneInfo()->Proxy->HasStaticLighting()))
					{
						ProjectedShadowInfo->AddSubjectPrimitive(Interaction->GetPrimitiveSceneInfo(), &Views);
					}
				}
			}
		}
	}
}

void FDeferredShadingSceneRenderer::InitProjectedShadowVisibility()
{
	// Initialize the views' ProjectedShadowVisibilityMaps and remove shadows without subjects.
	for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights);LightIt;++LightIt)
	{
		FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightIt.GetIndex()];

		// Allocate the light's projected shadow visibility and view relevance maps for this view.
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];
			FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightIt.GetIndex()];
			VisibleLightViewInfo.ProjectedShadowVisibilityMap.Init(false,VisibleLightInfo.AllProjectedShadows.Num());
			VisibleLightViewInfo.ProjectedShadowViewRelevanceMap.Empty(VisibleLightInfo.AllProjectedShadows.Num());
			VisibleLightViewInfo.ProjectedShadowViewRelevanceMap.AddZeroed(VisibleLightInfo.AllProjectedShadows.Num());
		}

		for( int32 ShadowIndex=0; ShadowIndex<VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++ )
		{
			FProjectedShadowInfo& ProjectedShadowInfo = *VisibleLightInfo.AllProjectedShadows[ShadowIndex];

			// Assign the shadow its id.
			ProjectedShadowInfo.ShadowId = ShadowIndex;

			for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				FViewInfo& View = Views[ViewIndex];
				if (ProjectedShadowInfo.DependentView && ProjectedShadowInfo.DependentView != &View)
				{
					// The view dependent projected shadow is valid for this view if it's the
					// right eye and the projected shadow is being rendered for the left eye.
					bool bIsValidForView = View.StereoPass == eSSP_RIGHT_EYE
						&& Views.IsValidIndex(ViewIndex - 1)
						&& Views[ViewIndex - 1].StereoPass == eSSP_LEFT_EYE
						&& ProjectedShadowInfo.FadeAlphas[ViewIndex] == 1.0f;
					if (!bIsValidForView)
					{
						continue;
					}
				}
				FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightIt.GetIndex()];

				if(VisibleLightViewInfo.bInViewFrustum)
				{
					// Compute the subject primitive's view relevance.  Note that the view won't necessarily have it cached,
					// since the primitive might not be visible.
					FPrimitiveViewRelevance ViewRelevance;
					if(ProjectedShadowInfo.ParentSceneInfo)
					{
						ViewRelevance = ProjectedShadowInfo.ParentSceneInfo->Proxy->GetViewRelevance(&View);
					}
					else
					{
						ViewRelevance.bDrawRelevance = ViewRelevance.bStaticRelevance = ViewRelevance.bDynamicRelevance = ViewRelevance.bShadowRelevance = true;
					}							
					VisibleLightViewInfo.ProjectedShadowViewRelevanceMap[ShadowIndex] = ViewRelevance;

					// Check if the subject primitive's shadow is view relevant.
					const bool bPrimitiveIsShadowRelevant = ViewRelevance.bShadowRelevance;

					// Check if the shadow and preshadow are occluded.
					const bool bShadowIsOccluded =
						!View.bIgnoreExistingQueries &&
						View.State &&
						((FSceneViewState*)View.State)->IsShadowOccluded(
							ProjectedShadowInfo.ParentSceneInfo ? 
								ProjectedShadowInfo.ParentSceneInfo->PrimitiveComponentId :
								FPrimitiveComponentId(),
							ProjectedShadowInfo.LightSceneInfo->Proxy->GetLightComponent(),
							ProjectedShadowInfo.SplitIndex,
							ProjectedShadowInfo.bTranslucentShadow
							);

					// The shadow is visible if it is view relevant and unoccluded.
					if(bPrimitiveIsShadowRelevant && !bShadowIsOccluded)
					{
						VisibleLightViewInfo.ProjectedShadowVisibilityMap[ShadowIndex] = true;
					}

					// Draw the shadow frustum.
					if(bPrimitiveIsShadowRelevant && !bShadowIsOccluded && !ProjectedShadowInfo.bReflectiveShadowmap)  
					{
						bool bDrawPreshadowFrustum = CVarDrawPreshadowFrustum.GetValueOnRenderThread() != 0;

						if ((ViewFamily.EngineShowFlags.ShadowFrustums)
							&& ((bDrawPreshadowFrustum && ProjectedShadowInfo.bPreShadow) || (!bDrawPreshadowFrustum && !ProjectedShadowInfo.bPreShadow)))
						{
							FViewElementPDI ShadowFrustumPDI(&Views[ViewIndex],NULL);
							
							if(ProjectedShadowInfo.IsWholeSceneDirectionalShadow())
							{
								// Get split color
								FColor Color = FColor::White;
								switch(ProjectedShadowInfo.SplitIndex)
								{
									case 0: Color = FColor::Red; break;
									case 1: Color = FColor::Yellow; break;
									case 2: Color = FColor::Green; break;
									case 3: Color = FColor::Blue; break;
								}

								const FMatrix ViewMatrix = View.ViewMatrices.ViewMatrix;
								const FMatrix ProjectionMatrix = View.ViewMatrices.ProjMatrix;
								const FVector4 ViewOrigin = View.ViewMatrices.ViewOrigin;

								float AspectRatio = ProjectionMatrix.M[1][1] / ProjectionMatrix.M[0][0];
								float ActualFOV = (ViewOrigin.W > 0.0f) ? FMath::Atan(1.0f / ProjectionMatrix.M[0][0]) : PI/4.0f;

								float Near = ProjectedShadowInfo.CascadeSettings.SplitNear;
								float Mid = ProjectedShadowInfo.CascadeSettings.FadePlaneOffset;
								float Far = ProjectedShadowInfo.CascadeSettings.SplitFar;

								// Camera Subfrustum
								DrawFrustumWireframe(&ShadowFrustumPDI, (ViewMatrix * FPerspectiveMatrix(ActualFOV, AspectRatio, 1.0f, Near, Mid)).Inverse(), Color, 0);
								DrawFrustumWireframe(&ShadowFrustumPDI, (ViewMatrix * FPerspectiveMatrix(ActualFOV, AspectRatio, 1.0f, Mid, Far)).Inverse(), FColor::White, 0);

								// Subfrustum Sphere Bounds
								DrawWireSphere(&ShadowFrustumPDI, FTransform(ProjectedShadowInfo.ShadowBounds.Center), Color, ProjectedShadowInfo.ShadowBounds.W, 40, 0);

								// Shadow Map Projection Bounds
								DrawFrustumWireframe(&ShadowFrustumPDI, ProjectedShadowInfo.SubjectAndReceiverMatrix.Inverse() * FTranslationMatrix(-ProjectedShadowInfo.PreShadowTranslation), Color, 0);
							}
							else
							{
								ProjectedShadowInfo.RenderFrustumWireframe(&ShadowFrustumPDI);
							}
						}
					}
				}
			}
		}
	}
}

inline void FDeferredShadingSceneRenderer::GatherShadowsForPrimitiveInner(
	const FPrimitiveSceneInfoCompact& PrimitiveSceneInfoCompact, 
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& PreShadows,
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows,
	bool bReflectionCaptureScene)
{
	if(PrimitiveSceneInfoCompact.bCastDynamicShadow)
	{
		FPrimitiveSceneInfo* RESTRICT	PrimitiveSceneInfo	= PrimitiveSceneInfoCompact.PrimitiveSceneInfo;
		FPrimitiveSceneProxy* RESTRICT	PrimitiveProxy		= PrimitiveSceneInfoCompact.Proxy;
		const FBoxSphereBounds&			PrimitiveBounds		= PrimitiveSceneInfoCompact.Bounds;

		// Check if the primitive is a subject for any of the preshadows.
		// Only allow preshadows from lightmapped primitives that cast both dynamic and static shadows.
		if (PreShadows.Num() && PrimitiveProxy->CastsStaticShadow() && PrimitiveProxy->HasStaticLighting())
		{
			for( int32 ShadowIndex = 0, Num = PreShadows.Num(); ShadowIndex < PreShadows.Num(); ShadowIndex++ )
			{
				FProjectedShadowInfo* RESTRICT ProjectedShadowInfo = PreShadows[ShadowIndex];

				// Check if this primitive is in the shadow's frustum.
				bool bInFrustum = ProjectedShadowInfo->CasterFrustum.IntersectBox( PrimitiveBounds.Origin, ProjectedShadowInfo->PreShadowTranslation, PrimitiveBounds.BoxExtent );

				if( bInFrustum && ProjectedShadowInfo->LightSceneInfoCompact.AffectsPrimitive(PrimitiveSceneInfoCompact) )
				{
					// Add this primitive to the shadow.
					ProjectedShadowInfo->AddSubjectPrimitive(PrimitiveSceneInfo, &Views);
				}
			}
		}

		if(PrimitiveSceneInfoCompact.bCastDynamicShadow || PrimitiveSceneInfoCompact.bAffectDynamicIndirectLighting )
		{
			for(int32 ShadowIndex = 0, Num = ViewDependentWholeSceneShadows.Num();ShadowIndex < Num;ShadowIndex++)
			{
				FProjectedShadowInfo* RESTRICT ProjectedShadowInfo = ViewDependentWholeSceneShadows[ShadowIndex];

				if ( ProjectedShadowInfo->bReflectiveShadowmap && !PrimitiveSceneInfoCompact.bAffectDynamicIndirectLighting )
				{
					continue;
				}
				if ( !ProjectedShadowInfo->bReflectiveShadowmap && !PrimitiveSceneInfoCompact.bCastDynamicShadow )
				{
					continue;
				}

				FPrimitiveSceneProxy* PrimitiveProxy = PrimitiveSceneInfoCompact.Proxy;
				FLightSceneProxy* RESTRICT LightProxy = ProjectedShadowInfo->LightSceneInfo->Proxy;

				const FVector LightDirection = LightProxy->GetDirection();
				const FVector PrimitiveToShadowCenter = ProjectedShadowInfo->ShadowBounds.Center - PrimitiveBounds.Origin;
				// Project the primitive's bounds origin onto the light vector
				const float ProjectedDistanceFromShadowOriginAlongLightDir = PrimitiveToShadowCenter | LightDirection;
				// Calculate the primitive's squared distance to the cylinder's axis
				const float PrimitiveDistanceFromCylinderAxisSq = (-LightDirection * ProjectedDistanceFromShadowOriginAlongLightDir + PrimitiveToShadowCenter).SizeSquared();

				// Include all primitives for movable lights, but only statically shadowed primitives from a light with static shadowing,
				// Since lights with static shadowing still create per-object shadows for primitives without static shadowing.
				if( (!LightProxy->HasStaticLighting() || !ProjectedShadowInfo->LightSceneInfo->bPrecomputedLightingIsValid)
					// Check if this primitive is in the shadow's cylinder
					&& PrimitiveDistanceFromCylinderAxisSq < FMath::Square(ProjectedShadowInfo->ShadowBounds.W + PrimitiveBounds.SphereRadius)
					// Check if the primitive is closer than the cylinder cap toward the light
					&& ProjectedDistanceFromShadowOriginAlongLightDir - PrimitiveBounds.SphereRadius < -ProjectedShadowInfo->MinPreSubjectZ
					// If the primitive is further along the cone axis than the shadow bounds origin, 
					// Check if the primitive is inside the spherical cap of the cascade's bounds
					&& !(ProjectedDistanceFromShadowOriginAlongLightDir < 0 
						&& PrimitiveToShadowCenter.SizeSquared() > FMath::Square(ProjectedShadowInfo->ShadowBounds.W + PrimitiveBounds.SphereRadius)))
				{
					const bool bInFrustum = ProjectedShadowInfo->CascadeSettings.ShadowBoundsAccurate.IntersectBox( PrimitiveBounds.Origin, PrimitiveBounds.BoxExtent );

					if( bInFrustum )
					{
						// Distance culling for RSMs
						float MinScreenRadiusForShadowCaster = GMinScreenRadiusForShadowCaster;
						if ( ProjectedShadowInfo->bReflectiveShadowmap )
						{
							MinScreenRadiusForShadowCaster = GMinScreenRadiusForShadowCasterRSM;
						}

						bool bScreenSpaceSizeCulled = false;
						check( ProjectedShadowInfo->DependentView );
						if ( ProjectedShadowInfo->DependentView ) 
						{
							const float DistanceSquared = ( PrimitiveBounds.Origin - ProjectedShadowInfo->DependentView->ShadowViewMatrices.ViewOrigin ).SizeSquared();
							bScreenSpaceSizeCulled = FMath::Square( PrimitiveBounds.SphereRadius ) < FMath::Square( MinScreenRadiusForShadowCaster ) * DistanceSquared;
						}

						if (ProjectedShadowInfo->LightSceneInfoCompact.AffectsPrimitive(PrimitiveSceneInfoCompact)
							// Exclude primitives that will create their own per-object shadow, except when rendering RSMs
							&& ( !PrimitiveProxy->CastsInsetShadow() || ProjectedShadowInfo->bReflectiveShadowmap )
							// Exclude primitives that will create a per-object shadow from a stationary light
							&& !ShouldCreateObjectShadowForStationaryLight(ProjectedShadowInfo->LightSceneInfo, PrimitiveSceneInfo->Proxy, true)
							// Only render shadows from objects that use static lighting during a reflection capture, since the reflection capture doesn't update at runtime
							&& (!bReflectionCaptureScene || PrimitiveProxy->HasStaticLighting()) 
							&& !bScreenSpaceSizeCulled )
						{
							// Add this primitive to the shadow.
							ProjectedShadowInfo->AddSubjectPrimitive(PrimitiveSceneInfo, NULL);
						}
					}
				}
			}
		}
	}
}

void FDeferredShadingSceneRenderer::GatherShadowPrimitives(
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& PreShadows,
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& ViewDependentWholeSceneShadows,
	bool bReflectionCaptureScene
	)
{
	SCOPE_CYCLE_COUNTER(STAT_GatherShadowPrimitivesTime);

	if(PreShadows.Num() || ViewDependentWholeSceneShadows.Num())
	{
		for(int32 ShadowIndex = 0, Num = ViewDependentWholeSceneShadows.Num(); ShadowIndex < Num;ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = ViewDependentWholeSceneShadows[ShadowIndex];
			checkSlow(ProjectedShadowInfo->DependentView);
			// Initialize the whole scene shadow's depth map with the shadow independent depth map from the view
			ProjectedShadowInfo->StaticMeshWholeSceneShadowDepthMap.Init(false,Scene->StaticMeshes.GetMaxIndex());
			ProjectedShadowInfo->StaticMeshWholeSceneShadowBatchVisibility.AddZeroed(Scene->StaticMeshes.GetMaxIndex());
		}

		// Find primitives that are in a shadow frustum in the octree.
		for(FScenePrimitiveOctree::TConstIterator<SceneRenderingAllocator> PrimitiveOctreeIt(Scene->PrimitiveOctree);
			PrimitiveOctreeIt.HasPendingNodes();
			PrimitiveOctreeIt.Advance())
		{
			const FScenePrimitiveOctree::FNode& PrimitiveOctreeNode = PrimitiveOctreeIt.GetCurrentNode();
			const FOctreeNodeContext& PrimitiveOctreeNodeContext = PrimitiveOctreeIt.GetCurrentContext();

			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_ShadowOctreeTraversal);
				// Find children of this octree node that may contain relevant primitives.
				FOREACH_OCTREE_CHILD_NODE(ChildRef)
				{
					if(PrimitiveOctreeNode.HasChild(ChildRef))
					{
						// Check that the child node is in the frustum for at least one shadow.
						const FOctreeNodeContext ChildContext = PrimitiveOctreeNodeContext.GetChildContext(ChildRef);
						bool bIsInFrustum = false;

						// Check for subjects of preshadows.
						if(!bIsInFrustum)
						{
							for(int32 ShadowIndex = 0, Num = PreShadows.Num(); ShadowIndex < Num; ShadowIndex++)
							{
								FProjectedShadowInfo* ProjectedShadowInfo = PreShadows[ShadowIndex];

								// Check if this primitive is in the shadow's frustum.
								if(ProjectedShadowInfo->CasterFrustum.IntersectBox(
									ChildContext.Bounds.Center + ProjectedShadowInfo->PreShadowTranslation,
									ChildContext.Bounds.Extent
									))
								{
									bIsInFrustum = true;
									break;
								}
							}
						}

						if (!bIsInFrustum)
						{
							for(int32 ShadowIndex = 0, Num = ViewDependentWholeSceneShadows.Num(); ShadowIndex < Num; ShadowIndex++)
							{
								FProjectedShadowInfo* ProjectedShadowInfo = ViewDependentWholeSceneShadows[ShadowIndex];

								// Check if this primitive is in the shadow's frustum.
								if(ProjectedShadowInfo->CasterFrustum.IntersectBox(
									ChildContext.Bounds.Center + ProjectedShadowInfo->PreShadowTranslation,
									ChildContext.Bounds.Extent
									))
								{
									bIsInFrustum = true;
									break;
								}
							}
						}

						if(bIsInFrustum)
						{
							// If the child node was in the frustum of at least one preshadow, push it on
							// the iterator's pending node stack.
							PrimitiveOctreeIt.PushChild(ChildRef);
						}
					}
				}
			}

			// Check all the primitives in this octree node.
			for(FScenePrimitiveOctree::ElementConstIt NodePrimitiveIt(PrimitiveOctreeNode.GetElementIt());NodePrimitiveIt;++NodePrimitiveIt)
			{
				// gather the shadows for this one primitive
				GatherShadowsForPrimitiveInner(*NodePrimitiveIt, PreShadows, ViewDependentWholeSceneShadows, bReflectionCaptureScene);
			}
		}

		for(int32 ShadowIndex = 0, Num = PreShadows.Num(); ShadowIndex < Num; ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = PreShadows[ShadowIndex];
			//@todo - sort other shadow types' subject mesh elements?
			// Probably needed for good performance with non-dominant whole scene shadows (spotlightmovable)
			ProjectedShadowInfo->SortSubjectMeshElements();
		}
	}
}

void FDeferredShadingSceneRenderer::InitDynamicShadows()
{
	SCOPE_CYCLE_COUNTER(STAT_DynamicShadowSetupTime);

	bool bReflectionCaptureScene = false;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		bReflectionCaptureScene = bReflectionCaptureScene || View.bIsReflectionCapture;
	}

	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> PreShadows;
	TArray<FProjectedShadowInfo*,SceneRenderingAllocator> ViewDependentWholeSceneShadows;
	{
		SCOPE_CYCLE_COUNTER(STAT_InitDynamicShadowsTime);

		for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
		{
			const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
			FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;
			FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

			// Only consider lights that may have shadows.
			if (LightSceneInfoCompact.bCastStaticShadow || LightSceneInfoCompact.bCastDynamicShadow)
			{
				// see if the light is visible in any view
				bool bIsVisibleInAnyView = false;

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					// View frustums are only checked when lights have visible primitives or have modulated shadows,
					// so we don't need to check for that again here
					bIsVisibleInAnyView = LightSceneInfo->ShouldRenderLight(Views[ViewIndex]);

					if (bIsVisibleInAnyView) 
					{
						break;
					}
				}

				if (bIsVisibleInAnyView)
				{
					static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
					const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnRenderThread() != 0);

					// Only create whole scene shadows for lights that don't precompute shadowing (movable lights)
					const bool bCreateShadowForMovableLight = 
						LightSceneInfoCompact.bCastDynamicShadow
						&& (!LightSceneInfo->Proxy->HasStaticShadowing() || !bAllowStaticLighting);

					// Also create a whole scene shadow for lights with precomputed shadows that are unbuilt
					const bool bCreateShadowToPreviewStaticLight = 
						LightSceneInfo->Proxy->HasStaticShadowing()
						&& LightSceneInfoCompact.bCastStaticShadow 
						&& !LightSceneInfo->bPrecomputedLightingIsValid;

					// Create a whole scene shadow for lights that want static shadowing but didn't get assigned to a valid shadowmap channel due to overlap
					const bool bCreateShadowForOverflowStaticShadowing =
						LightSceneInfo->Proxy->HasStaticShadowing()
						&& !LightSceneInfo->Proxy->HasStaticLighting()
						&& LightSceneInfoCompact.bCastStaticShadow 
						&& LightSceneInfo->bPrecomputedLightingIsValid
						&& LightSceneInfo->Proxy->GetShadowMapChannel() == INDEX_NONE;

					if (bCreateShadowForMovableLight || bCreateShadowToPreviewStaticLight || bCreateShadowForOverflowStaticShadowing)
					{
						// Try to create a whole scene projected shadow.
						CreateWholeSceneProjectedShadow(LightSceneInfo);
					}

					// Allow movable and stationary lights to create CSM, or static lights that are unbuilt
					if (!LightSceneInfo->Proxy->HasStaticLighting() || bCreateShadowToPreviewStaticLight)
					{
						TArray<float, TInlineAllocator<2> > FadeAlphas;
						// Allow each view to create a whole scene view dependent shadow
						for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
						{
							FViewInfo& View = Views[ViewIndex];

							// If rendering in stereo mode we render shadow depths only for the left eye, but project for both eyes!
							if (View.StereoPass != eSSP_RIGHT_EYE)
							{
								FadeAlphas.Init(0.0f,Views.Num());
								FadeAlphas[ViewIndex] = 1.0f;

								if (View.StereoPass == eSSP_LEFT_EYE
									&& Views.IsValidIndex(ViewIndex + 1)
									&& Views[ViewIndex + 1].StereoPass == eSSP_RIGHT_EYE)
								{
									FadeAlphas[ViewIndex + 1] = 1.0f;
								}

								const int32 NumSplits = LightSceneInfo->Proxy->GetNumViewDependentWholeSceneShadows(View);
								for (int32 SplitIndex = 0; SplitIndex < NumSplits; SplitIndex++)
								{
									FWholeSceneProjectedShadowInitializer ProjectedShadowInitializer;

									if (LightSceneInfo->Proxy->GetViewDependentWholeSceneProjectedShadowInitializer(View, SplitIndex, ProjectedShadowInitializer))
									{
										const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetShadowDepthTextureResolution();
										// Create the projected shadow info.
										FProjectedShadowInfo* ProjectedShadowInfo = new(FMemStack::Get(),1,16) FProjectedShadowInfo(
											LightSceneInfo,
											&View,
											ProjectedShadowInitializer,
											//@todo - remove the shadow border for whole scene shadows
											ShadowBufferResolution.X - SHADOW_BORDER * 2,
											ShadowBufferResolution.Y - SHADOW_BORDER * 2,
											FadeAlphas
											);

										FVisibleLightInfo& LightViewInfo = VisibleLightInfos[LightSceneInfo->Id];
										VisibleLightInfo.MemStackProjectedShadows.Add(ProjectedShadowInfo);
										VisibleLightInfo.AllProjectedShadows.Add(ProjectedShadowInfo);
										ViewDependentWholeSceneShadows.Add(ProjectedShadowInfo);
									}
								}
								FSceneViewState* ViewState = (FSceneViewState*)View.State;
								if(ViewState)
								{
									FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume();

									if (LightPropagationVolume && View.FinalPostProcessSettings.LPVIntensity > 0 )
									{
										// Generate the RSM shadow info
										FRsmWholeSceneProjectedShadowInitializer ProjectedShadowInitializer;
										FLightPropagationVolume& Lpv = *LightPropagationVolume;

										if (LightSceneInfo->Proxy->GetViewDependentRsmWholeSceneProjectedShadowInitializer( View, Lpv.GetBoundingBox(), ProjectedShadowInitializer ))
										{
											const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetReflectiveShadowMapTextureResolution();

											// Create the projected shadow info.
											FProjectedShadowInfo* ProjectedShadowInfo = new(FMemStack::Get(),1,16) FProjectedShadowInfo(
												LightSceneInfo,
												&View,
												ProjectedShadowInitializer,
												ShadowBufferResolution.X,
												ShadowBufferResolution.Y );

											FVisibleLightInfo& LightViewInfo = VisibleLightInfos[LightSceneInfo->Id];
											VisibleLightInfo.MemStackProjectedShadows.Add(ProjectedShadowInfo);
											VisibleLightInfo.AllProjectedShadows.Add(ProjectedShadowInfo);
											VisibleLightInfo.ReflectiveShadowMaps.Add(ProjectedShadowInfo); 
											ViewDependentWholeSceneShadows.Add(ProjectedShadowInfo); // or separate list?
										}
									}
								}
							}
						}

						// Look for individual primitives with a dynamic shadow.
						for (FLightPrimitiveInteraction* Interaction = LightSceneInfo->DynamicPrimitiveList;
							Interaction;
							Interaction = Interaction->GetNextPrimitive()
							)
						{
							SetupInteractionShadows(Interaction, VisibleLightInfo, bReflectionCaptureScene, ViewDependentWholeSceneShadows, PreShadows);
						}
					}
				}
			}
		}

		// Calculate visibility of the projected shadows.
		InitProjectedShadowVisibility();
	}

	// Clear old preshadows and attempt to add new ones to the cache
	UpdatePreshadowCache();

	// Gathers the list of primitives used to draw various shadow types
	GatherShadowPrimitives(PreShadows, ViewDependentWholeSceneShadows, bReflectionCaptureScene);
}
