// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneFilterRendering.cpp: Filter rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessWeightedSampleSum.h"

/**
* Static vertex and index buffer used for 2D screen rectangles.
*/
class FScreenRectangleVertexBuffer : public FVertexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	void InitRHI() OVERRIDE
	{
		TResourceArray<FFilterVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.Init(6);

		Vertices[0].Position = FVector4(1,  1,	0,	1);
		Vertices[0].UV = FVector2D(1,	1);

		Vertices[1].Position = FVector4(0,  1,	0,	1);
		Vertices[1].UV = FVector2D(0,	1);

		Vertices[2].Position = FVector4(1,	0,	0,	1);
		Vertices[2].UV = FVector2D(1,	0);

		Vertices[3].Position = FVector4(0,	0,	0,	1);
		Vertices[3].UV = FVector2D(0,	0);

		//The final two vertices are used for the triangle optimization (a single triangle spans the entire viewport )
		Vertices[4].Position = FVector4(-1,  1,	0,	1);
		Vertices[4].UV = FVector2D(-1,	1);

		Vertices[5].Position = FVector4(1,  -1,	0,	1);
		Vertices[5].UV = FVector2D(1, -1);

		// Create vertex buffer. Fill buffer with initial data upon creation
		VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), &Vertices, BUF_Static);
	}
};

class FScreenRectangleIndexBuffer : public FIndexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	void InitRHI() OVERRIDE
	{
		// Indices 0 - 5 are used for rendering a quad. Indices 6 - 8 are used for triangle optimization. 
		const uint16 Indices[] = { 0, 1, 2, 2, 1, 3, 0, 4, 5 };
		
		TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;
		uint32 NumIndices = ARRAY_COUNT(Indices);
		IndexBuffer.AddUninitialized(NumIndices);
		FMemory::Memcpy(IndexBuffer.GetData(), Indices, NumIndices * sizeof(uint16));

		// Create index buffer. Fill buffer with initial data upon creation
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), &IndexBuffer, BUF_Static);
	}
};

/** Global resource  */
static TGlobalResource<FScreenRectangleVertexBuffer> GScreenRectangleVertexBuffer;
static TGlobalResource<FScreenRectangleIndexBuffer> GScreenRectangleIndexBuffer;

/** Vertex declaration for the 2D screen rectangle. */
TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

/** Uniform buffer for computing the vertex positional and UV adjustments in the vertex shader. */
BEGIN_UNIFORM_BUFFER_STRUCT( FDrawRectangleParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, PosScaleBias )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, UVScaleBias )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, InvTargetSizeAndTextureSize )
END_UNIFORM_BUFFER_STRUCT( FDrawRectangleParameters )

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FDrawRectangleParameters,TEXT("DrawRectangleParameters"));

typedef TUniformBufferRef<FDrawRectangleParameters> FDrawRectangleBufferRef;

static void DoDrawRectangleFlagOverride(EDrawRectangleFlags& Flags)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Determine triangle draw mode
	static auto* TriangleModeCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DrawRectangleOptimization"));
	int Value = TriangleModeCVar->GetValueOnRenderThread();

	if(!Value)
	{
		// don't use triangle optimization
		Flags = EDRF_Default;
	}
#endif
}

void DrawRectangle(
	float X,
	float Y,
	float SizeX,
	float SizeY,
	float U,
	float V,
	float SizeU,
	float SizeV,
	FIntPoint TargetSize,
	FIntPoint TextureSize,
	FShader* VertexShader,
	EDrawRectangleFlags Flags
	)
{
	float ClipSpaceQuadZ = 0.0f;

	DoDrawRectangleFlagOverride(Flags);

	// triangle if extending to left and top of the given rectangle, if it's not left top of the viewport it can cause artifacts
	if(X > 0.0f || Y > 0.0f)
	{
		// don't use triangle optimization
		Flags = EDRF_Default;
	}

	// Set up vertex uniform parameters for scaling and biasing the rectangle.
	// Note: Use DrawRectangle in the vertex shader to calculate the correct vertex position and uv.

	FDrawRectangleParameters Parameters;
	Parameters.PosScaleBias = FVector4(SizeX, SizeY, X, Y);
	Parameters.UVScaleBias = FVector4(SizeU, SizeV, U, V);

	Parameters.InvTargetSizeAndTextureSize = FVector4( 
		1.0f / TargetSize.X, 1.0f / TargetSize.Y, 
		1.0f / TextureSize.X, 1.0f / TextureSize.Y);

	SetUniformBufferParameterImmediate(VertexShader->GetVertexShader(), VertexShader->GetUniformBufferParameter<FDrawRectangleParameters>(), Parameters);
	RHISetStreamSource(0, GScreenRectangleVertexBuffer.VertexBufferRHI, sizeof(FFilterVertex),0);


	if (Flags == EDRF_UseTriangleOptimization)
	{
		// A single triangle spans the entire viewport this results in a quad that fill the viewport. This can increase rasterization efficiency
		// as we do not have a diagonal edge (through the center) for the rasterizer/span-dispatch. Although the actual benefit of this technique is dependent upon hardware.

		// We offset into the index buffer when using the triangle optimization to access the correct vertices.
		RHIDrawIndexedPrimitive(
			GScreenRectangleIndexBuffer.IndexBufferRHI,
			PT_TriangleList,
			/*BaseVertexIndex=*/ 0,
			/*MinIndex=*/ 0,
			/*NumVertices=*/ 3,
			/*StartIndex=*/ 6,
			/*NumPrimitives=*/ 1,
			/*NumInstances=*/ 0
			);
	}
	else
	{
		RHIDrawIndexedPrimitive(
			GScreenRectangleIndexBuffer.IndexBufferRHI,
			PT_TriangleList,
			/*BaseVertexIndex=*/ 0,
			/*MinIndex=*/ 0,
			/*NumVertices=*/ 4,
			/*StartIndex=*/ 0,
			/*NumPrimitives=*/ 2,
			/*NumInstances=*/ 0
			);
	}
}

void DrawTransformedRectangle(
    float X,
    float Y,
    float SizeX,
    float SizeY,
    const FMatrix& PosTransform,
    float U,
    float V,
    float SizeU,
    float SizeV,
    const FMatrix& TexTransform,
    FIntPoint TargetSize,
    FIntPoint TextureSize
    )
{
	float ClipSpaceQuadZ = 0.0f;

	// we don't do the triangle optimization as this case is rare for the DrawTransformedRectangle case

	FFilterVertex Vertices[4];

	Vertices[0].Position = PosTransform.TransformFVector4(FVector4(X,			Y,			ClipSpaceQuadZ,	1));
	Vertices[1].Position = PosTransform.TransformFVector4(FVector4(X + SizeX,	Y,			ClipSpaceQuadZ,	1));
	Vertices[2].Position = PosTransform.TransformFVector4(FVector4(X,			Y + SizeY,	ClipSpaceQuadZ,	1));
	Vertices[3].Position = PosTransform.TransformFVector4(FVector4(X + SizeX,	Y + SizeY,	ClipSpaceQuadZ,	1));

	Vertices[0].UV = FVector2D(TexTransform.TransformFVector4(FVector(U,			V,         0)));
	Vertices[1].UV = FVector2D(TexTransform.TransformFVector4(FVector(U + SizeU,	V,         0)));
	Vertices[2].UV = FVector2D(TexTransform.TransformFVector4(FVector(U,			V + SizeV, 0)));
	Vertices[3].UV = FVector2D(TexTransform.TransformFVector4(FVector(U + SizeU,	V + SizeV, 0)));

	for (int32 VertexIndex = 0; VertexIndex < 4; VertexIndex++)
	{
		Vertices[VertexIndex].Position.X = -1.0f + 2.0f * (Vertices[VertexIndex].Position.X - GPixelCenterOffset) / (float)TargetSize.X;
		Vertices[VertexIndex].Position.Y = (+1.0f - 2.0f * (Vertices[VertexIndex].Position.Y - GPixelCenterOffset) / (float)TargetSize.Y) * GProjectionSignY;

		Vertices[VertexIndex].UV.X = Vertices[VertexIndex].UV.X / (float)TextureSize.X;
		Vertices[VertexIndex].UV.Y = Vertices[VertexIndex].UV.Y / (float)TextureSize.Y;
	}

	static const uint16 Indices[] =	{ 0, 1, 3, 0, 3, 2 };

	RHIDrawIndexedPrimitiveUP(PT_TriangleList, 0, 4, 2, Indices, sizeof(Indices[0]), Vertices, sizeof(Vertices[0]));
}
