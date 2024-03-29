// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLState.h: OpenGL state definitions.
=============================================================================*/

#pragma once

#include "StaticArray.h"

#include "OpenGLResources.h"	// for OGL_COMBINED_TEXTURE_UNITS and OGL_NUM_PACKED_UNIFORM_ARRAYS definitions

#define ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE 65536

struct FOpenGLSamplerStateData
{
	// These enum is just used to count the number of members in this struct
	enum EGLSamplerData
	{
		EGLSamplerData_WrapS,
		EGLSamplerData_WrapT,
		EGLSamplerData_WrapR,
		EGLSamplerData_LODBias,
		EGLSamplerData_MagFilter,
		EGLSamplerData_MinFilter,
		EGLSamplerData_MaxAniso,
		EGLSamplerData_CompareMode,
		EGLSamplerData_CompareFunc,
		EGLSamplerData_Num,
	};

	GLint WrapS;
	GLint WrapT;
	GLint WrapR;
	GLint LODBias;
	GLint MagFilter;
	GLint MinFilter;
	GLint MaxAnisotropy;
	GLint CompareMode;
	GLint CompareFunc;

	FOpenGLSamplerStateData()
		: WrapS(GL_REPEAT)
		, WrapT(GL_REPEAT)
		, WrapR(GL_REPEAT)
		, LODBias(0)
		, MagFilter(GL_NEAREST)
		, MinFilter(GL_NEAREST)
		, MaxAnisotropy(1)
		, CompareMode(GL_NONE)
		, CompareFunc(GL_ALWAYS)
	{
	}
};

class FOpenGLSamplerState : public FRHISamplerState
{
public:
	GLuint Resource;
	FOpenGLSamplerStateData Data;

	~FOpenGLSamplerState();
};

struct FOpenGLRasterizerStateData
{
	GLenum FillMode;
	GLenum CullMode;
	float DepthBias;
	float SlopeScaleDepthBias;

	FOpenGLRasterizerStateData()
		: FillMode(GL_FILL)
		, CullMode(GL_NONE)
		, DepthBias(0.0f)
		, SlopeScaleDepthBias(0.0f)
	{
	}
};

class FOpenGLRasterizerState : public FRHIRasterizerState
{
public:
	FOpenGLRasterizerStateData Data;
};

struct FOpenGLDepthStencilStateData
{
	bool bZEnable;
	bool bZWriteEnable;
	GLenum ZFunc;
	

	bool bStencilEnable;
	bool bTwoSidedStencilMode;
	GLenum StencilFunc;
	GLenum StencilFail;
	GLenum StencilZFail;
	GLenum StencilPass;
	GLenum CCWStencilFunc;
	GLenum CCWStencilFail;
	GLenum CCWStencilZFail;
	GLenum CCWStencilPass;
	uint32 StencilReadMask;
	uint32 StencilWriteMask;

	FOpenGLDepthStencilStateData()
		: bZEnable(false)
		, bZWriteEnable(true)
		, ZFunc(GL_LESS)
		, bStencilEnable(false)
		, bTwoSidedStencilMode(false)
		, StencilFunc(GL_ALWAYS)
		, StencilFail(GL_KEEP)
		, StencilZFail(GL_KEEP)
		, StencilPass(GL_KEEP)
		, CCWStencilFunc(GL_ALWAYS)
		, CCWStencilFail(GL_KEEP)
		, CCWStencilZFail(GL_KEEP)
		, CCWStencilPass(GL_KEEP)
		, StencilReadMask(0xFFFFFFFF)
		, StencilWriteMask(0xFFFFFFFF)
	{
	}
};

class FOpenGLDepthStencilState : public FRHIDepthStencilState
{
public:
	FOpenGLDepthStencilStateData Data;
};

struct FOpenGLBlendStateData
{
	struct FRenderTarget
	{
		bool bAlphaBlendEnable;
		GLenum ColorBlendOperation;
		GLenum ColorSourceBlendFactor;
		GLenum ColorDestBlendFactor;
		bool bSeparateAlphaBlendEnable;
		GLenum AlphaBlendOperation;
		GLenum AlphaSourceBlendFactor;
		GLenum AlphaDestBlendFactor;
		uint32 ColorWriteMaskR : 1;
		uint32 ColorWriteMaskG : 1;
		uint32 ColorWriteMaskB : 1;
		uint32 ColorWriteMaskA : 1;
	};
	
	TStaticArray<FRenderTarget,MaxSimultaneousRenderTargets> RenderTargets;

	FOpenGLBlendStateData()
	{
		for (int32 i = 0; i < MaxSimultaneousRenderTargets; ++i)
		{
			FRenderTarget& Target = RenderTargets[i];
			Target.bAlphaBlendEnable = false;
			Target.ColorBlendOperation = GL_NONE;
			Target.ColorSourceBlendFactor = GL_NONE;
			Target.ColorDestBlendFactor = GL_NONE;
			Target.bSeparateAlphaBlendEnable = false;
			Target.AlphaBlendOperation = GL_NONE;
			Target.AlphaSourceBlendFactor = GL_NONE;
			Target.AlphaDestBlendFactor = GL_NONE;
			Target.ColorWriteMaskR = false;
			Target.ColorWriteMaskG = false;
			Target.ColorWriteMaskB = false;
			Target.ColorWriteMaskA = false;
		}
	}
};

class FOpenGLBlendState : public FRHIBlendState
{
public:
	FOpenGLBlendStateData Data;
};

struct FTextureStage
{
	class FOpenGLTextureBase* Texture;
	GLenum Target;
	GLuint Resource;
	int32 LimitMip;
	bool bHasMips;
	int32 NumMips;

	FTextureStage()
	:	Texture(NULL)
	,	Target(GL_NONE)
	,	Resource(0)
	,	LimitMip(-1)
	,	bHasMips(false)
	,	NumMips(0)
	{
	}
};

struct FUAVStage
{
	GLenum Format;
	GLuint Resource;
	
	FUAVStage()
	:	Format(GL_NONE)
	,	Resource(0)
	{
	}
};
#define FOpenGLCachedAttr_Invalid (void*)0xFFFFFFFF
#define FOpenGLCachedAttr_SingleVertex (void*)0xFFFFFFFE

struct FOpenGLCachedAttr
{
	void* Pointer;
	GLsizei Stride;
	GLuint Buffer;
	GLuint Size;
	GLuint Divisor;
	GLenum Type;
	GLuint StreamOffset;
	GLuint StreamIndex;
	GLboolean bNormalized;

	bool bEnabled;

	FOpenGLCachedAttr() : Pointer(FOpenGLCachedAttr_Invalid), bEnabled(false) {}
};

struct FOpenGLStream
{
	FOpenGLStream()
		: VertexBuffer(0)
		, Stride(0)
		, Offset(0)
		, Divisor(0)
	{}
	FOpenGLVertexBuffer *VertexBuffer;
	uint32 Stride;
	uint32 Offset;
	uint32 Divisor;
};

#define NUM_OPENGL_VERTEX_STREAMS 16

struct FOpenGLCommonState
{
	FTextureStage*			Textures;
	FOpenGLSamplerState**	SamplerStates;
	FUAVStage*				UAVs;

	FOpenGLCommonState()
	: Textures(NULL)
	, SamplerStates(NULL)
	, UAVs(NULL)
	{}

	~FOpenGLCommonState()
	{
		CleanupResources();
	}

	virtual void InitializeResources(int32 NumCombinedTextures, int32 NumComputeUAVUnits)
	{
		check(!Textures && !SamplerStates && !UAVs);
		Textures = new FTextureStage[NumCombinedTextures];
		SamplerStates = new FOpenGLSamplerState*[NumCombinedTextures];
		FMemory::Memset( SamplerStates, 0, NumCombinedTextures * sizeof(*SamplerStates) );
		UAVs = new FUAVStage[NumComputeUAVUnits];
	}

	virtual void CleanupResources()
	{
		delete [] UAVs;
		delete [] SamplerStates;
		delete [] Textures;

		UAVs = NULL;
		SamplerStates = NULL;
		Textures = NULL;
	}
};

struct FOpenGLContextState : public FOpenGLCommonState
{
	FOpenGLRasterizerStateData		RasterizerState;
	FOpenGLDepthStencilStateData	DepthStencilState;
	uint32							StencilRef;
	FOpenGLBlendStateData			BlendState;
	GLuint							Framebuffer;
	uint32							RenderTargetWidth;
	uint32							RenderTargetHeight;
	GLuint							OcclusionQuery;
	GLuint							Program;
	bool							bUsingTessellation;
	GLuint 							UniformBuffers[OGL_NUM_SHADER_STAGES*OGL_MAX_UNIFORM_BUFFER_BINDINGS];
	GLuint 							UniformBufferOffsets[OGL_NUM_SHADER_STAGES*OGL_MAX_UNIFORM_BUFFER_BINDINGS];
	TArray<FOpenGLSamplerState*>	CachedSamplerStates;
	GLenum							ActiveTexture;
	bool							bScissorEnabled;
	FIntRect						Scissor;
	FIntRect						Viewport;
	float							DepthMinZ;
	float							DepthMaxZ;
	GLuint							ArrayBufferBound;
	GLuint							ElementArrayBufferBound;
	GLuint							PixelUnpackBufferBound;
	GLuint							UniformBufferBound;
	FLinearColor					ClearColor;
	uint16							ClearStencil;
	float							ClearDepth;
	bool							bSeamlessCubemapEnabled;

	// @todo-mobile: Used to cache the last color attachment to optimize logical buffer loads
	GLuint							LastES2ColorRT;
	GLuint							LastES2DepthRT;

	FOpenGLCachedAttr				VertexAttrs[NUM_OPENGL_VERTEX_STREAMS];
	FOpenGLStream					VertexStreams[NUM_OPENGL_VERTEX_STREAMS];

	FOpenGLVertexDeclaration* VertexDecl;
	uint32 ActiveAttribMask;
	uint32 MaxActiveStream;
	uint32 MaxActiveAttrib;

	FOpenGLContextState()
	:	StencilRef(0)
	,	Framebuffer(0)
	,	Program(0)
	,	bUsingTessellation(false)
	,	ActiveTexture(GL_TEXTURE0)
	,	bScissorEnabled(false)
	,	DepthMinZ(0.0f)
	,	DepthMaxZ(1.0f)
	,	ArrayBufferBound(0)
	,	ElementArrayBufferBound(0)
	,	PixelUnpackBufferBound(0)
	,	UniformBufferBound(0)
	,	ClearColor(-1, -1, -1, -1)
	,	ClearStencil(0xFFFF)
	,	ClearDepth(-1.0f)
	,	bSeamlessCubemapEnabled(false)
#if PLATFORM_ANDROID
	,	LastES2ColorRT(0xFFFFFFFF)
	,	LastES2DepthRT(0xFFFFFFFF)
#else
	,	LastES2ColorRT(0)
	,	LastES2DepthRT(0)
#endif
	, VertexDecl(0)
	, ActiveAttribMask(0)
	, MaxActiveStream(0)
	, MaxActiveAttrib(0)
	{
		Scissor.Min.X = Scissor.Min.Y = Scissor.Max.X = Scissor.Max.Y = 0;
		Viewport.Min.X = Viewport.Min.Y = Viewport.Max.X = Viewport.Max.Y = 0;
		FMemory::Memzero(UniformBuffers, sizeof(UniformBuffers));
		FMemory::Memzero(UniformBufferOffsets, sizeof(UniformBufferOffsets));
	}

	virtual void InitializeResources(int32 NumCombinedTextures, int32 NumComputeUAVUnits) OVERRIDE
	{
		FOpenGLCommonState::InitializeResources(NumCombinedTextures, NumComputeUAVUnits);
		CachedSamplerStates.Empty(NumCombinedTextures);
		CachedSamplerStates.AddZeroed(NumCombinedTextures);
	}

	virtual void CleanupResources() OVERRIDE
	{
		CachedSamplerStates.Empty();
		FOpenGLCommonState::CleanupResources();
	}
};

struct FOpenGLRHIState : public FOpenGLCommonState
{
	FOpenGLRasterizerStateData		RasterizerState;
	FOpenGLDepthStencilStateData	DepthStencilState;
	uint32							StencilRef;
	FOpenGLBlendStateData			BlendState;
	GLuint							Framebuffer;
	bool							bScissorEnabled;
	FIntRect						Scissor;
	FIntRect						Viewport;
	float							DepthMinZ;
	float							DepthMaxZ;
	GLuint							ZeroFilledDummyUniformBuffer;
	uint32							RenderTargetWidth;
	uint32							RenderTargetHeight;
	GLuint							RunningOcclusionQuery;
	bool							bSeamlessCubemapEnabled;

	// Pending framebuffer setup
	int32							FirstNonzeroRenderTarget;
	FOpenGLTextureBase*				RenderTargets[MaxSimultaneousRenderTargets];
	uint32							RenderTargetMipmapLevels[MaxSimultaneousRenderTargets];
	uint32							RenderTargetArrayIndex[MaxSimultaneousRenderTargets];
	FOpenGLTextureBase*				DepthStencil;
	bool							bFramebufferSetupInvalid;

	// Information about pending BeginDraw[Indexed]PrimitiveUP calls.
	FOpenGLStream					DynamicVertexStream;
	uint32							NumVertices;
	uint32							PrimitiveType;
	uint32							NumPrimitives;
	uint32							MinVertexIndex;
	uint32							IndexDataStride;

	FOpenGLStream					Streams[NUM_OPENGL_VERTEX_STREAMS];
	FOpenGLShaderParameterCache*	ShaderParameters;

	TRefCountPtr<FOpenGLBoundShaderState>	BoundShaderState;
	FComputeShaderRHIRef					CurrentComputeShader;	

	// Used for if(!FOpenGL::SupportsFastBufferData())
	uint32 UpVertexBufferBytes;
	uint32 UpIndexBufferBytes;
	uint32 UpStride;
	void* UpVertexBuffer;
	void* UpIndexBuffer;

	FOpenGLRHIState()
	:	StencilRef(0)
	,	Framebuffer(0)
	,	bScissorEnabled(false)
	,	DepthMinZ(0.0f)
	,	DepthMaxZ(1.0f)
	,	ZeroFilledDummyUniformBuffer(0)
	,	RenderTargetWidth(0)
	,	RenderTargetHeight(0)
	,	RunningOcclusionQuery(0)
	,	bSeamlessCubemapEnabled(false)
	,	FirstNonzeroRenderTarget(-1)
	,	DepthStencil(0)
	,	bFramebufferSetupInvalid(true)
	,	NumVertices(0)
	,	PrimitiveType(0)
	,	NumPrimitives(0)
	,	MinVertexIndex(0)
	,	IndexDataStride(0)
	,	ShaderParameters(NULL)
	,	BoundShaderState(NULL)
	,	CurrentComputeShader(NULL)
	,	UpVertexBufferBytes(0)
	,   UpIndexBufferBytes(0)
	,	UpVertexBuffer(0)
	,	UpIndexBuffer(0)
	{
		Scissor.Min.X = Scissor.Min.Y = Scissor.Max.X = Scissor.Max.Y = 0;
		Viewport.Min.X = Viewport.Min.Y = Viewport.Max.X = Viewport.Max.Y = 0;
		FMemory::Memset( RenderTargets, 0, sizeof(RenderTargets) );	// setting all to 0 at start
		FMemory::Memset( RenderTargetMipmapLevels, 0, sizeof(RenderTargetMipmapLevels) );	// setting all to 0 at start
		FMemory::Memset( RenderTargetArrayIndex, 0, sizeof(RenderTargetArrayIndex) );	// setting all to 0 at start
	}

	~FOpenGLRHIState()
	{
		CleanupResources();
	}

	virtual void InitializeResources(int32 NumCombinedTextures, int32 NumComputeUAVUnits) OVERRIDE
	{
		check(!ShaderParameters);
		FOpenGLCommonState::InitializeResources(NumCombinedTextures, NumComputeUAVUnits);
		ShaderParameters = new FOpenGLShaderParameterCache[OGL_NUM_SHADER_STAGES];
		ShaderParameters[OGL_SHADER_STAGE_VERTEX].InitializeResources(FOpenGL::GetMaxVertexUniformComponents() * sizeof(float));
		ShaderParameters[OGL_SHADER_STAGE_PIXEL].InitializeResources(FOpenGL::GetMaxPixelUniformComponents() * sizeof(float));
		ShaderParameters[OGL_SHADER_STAGE_GEOMETRY].InitializeResources(FOpenGL::GetMaxGeometryUniformComponents() * sizeof(float));
		
		if ( FOpenGL::SupportsTessellation() )
		{
			ShaderParameters[OGL_SHADER_STAGE_HULL].InitializeResources(FOpenGL::GetMaxHullUniformComponents() * sizeof(float));
			ShaderParameters[OGL_SHADER_STAGE_DOMAIN].InitializeResources(FOpenGL::GetMaxDomainUniformComponents() * sizeof(float));
		}

		if ( FOpenGL::SupportsComputeShaders() )
		{
			ShaderParameters[OGL_SHADER_STAGE_COMPUTE].InitializeResources(FOpenGL::GetMaxComputeUniformComponents() * sizeof(float));
		}

	}

	virtual void CleanupResources() OVERRIDE
	{
		delete [] ShaderParameters;
		ShaderParameters = NULL;

		FOpenGLCommonState::CleanupResources();
	}
};
