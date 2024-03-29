// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessMobile.cpp: Uber post for mobile implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessMobile.h"
#include "PostProcessing.h"
#include "PostProcessHistogram.h"
#include "PostProcessEyeAdaptation.h"

//
// BLOOM SETUP
//

class FPostProcessBloomSetupVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomSetupVS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	/** Default constructor. */
	FPostProcessBloomSetupVS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FPostProcessBloomSetupVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters(ShaderRHI, Context.View);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

template <uint32 UseSunDof> // 0=none, 1=dof, 2=sun, 3=sun&dof
class FPostProcessBloomSetupPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomSetupPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		//Need to hack in exposure scale for < SM5
		OutEnvironment.SetDefine(TEXT("NO_EYEADAPTATION_EXPOSURE_FIX"), 1);

		OutEnvironment.SetDefine(TEXT("ES2_USE_SUN"), (UseSunDof & 2) ? (uint32)1 : (uint32)0);
		OutEnvironment.SetDefine(TEXT("ES2_USE_DOF"), (UseSunDof & 1) ? (uint32)1 : (uint32)0);
	}

	FPostProcessBloomSetupPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter BloomThreshold;

	/** Initialization constructor. */
	FPostProcessBloomSetupPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		BloomThreshold.Bind(Initializer.ParameterMap, TEXT("BloomThreshold"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;

		FGlobalShader::SetParameters(ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		float ExposureScale = FRCPassPostProcessEyeAdaptation::ComputeExposureScaleValue(Context.View);

		FVector4 BloomThresholdValue(Settings.BloomThreshold, 0, 0, ExposureScale);
		SetShaderValue(ShaderRHI, BloomThreshold, BloomThresholdValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << BloomThreshold;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomSetupVS_ES2,TEXT("PostProcessMobile"),TEXT("BloomVS_ES2"),SF_Vertex);

typedef FPostProcessBloomSetupPS_ES2<0> FPostProcessBloomSetupPS_ES2_0;
typedef FPostProcessBloomSetupPS_ES2<1> FPostProcessBloomSetupPS_ES2_1;
typedef FPostProcessBloomSetupPS_ES2<2> FPostProcessBloomSetupPS_ES2_2;
typedef FPostProcessBloomSetupPS_ES2<3> FPostProcessBloomSetupPS_ES2_3;
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessBloomSetupPS_ES2_0,TEXT("PostProcessMobile"),TEXT("BloomPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessBloomSetupPS_ES2_1,TEXT("PostProcessMobile"),TEXT("BloomPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessBloomSetupPS_ES2_2,TEXT("PostProcessMobile"),TEXT("BloomPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessBloomSetupPS_ES2_3,TEXT("PostProcessMobile"),TEXT("BloomPS_ES2"),SF_Pixel);

template <uint32 UseSunDof>
static void BloomSetup_SetShader(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessBloomSetupVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessBloomSetupPS_ES2<UseSunDof> > PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessBloomSetupES2::SetShader(const FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;
	uint32 UseSun = Context.View.bLightShaftUse ? 1 : 0;
	uint32 UseDof =  (Context.View.FinalPostProcessSettings.DepthOfFieldScale > 0.0f) ? 1 : 0;
	uint32 UseSunDof = (UseSun << 1) + UseDof;

	switch(UseSunDof)
	{
		case 0: BloomSetup_SetShader<0>(Context); break;
		case 1: BloomSetup_SetShader<1>(Context); break;
		case 2: BloomSetup_SetShader<2>(Context); break;
		case 3: BloomSetup_SetShader<3>(Context); break;
	}
}

void FRCPassPostProcessBloomSetupES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessBloomSetup, DEC_SCENE_ITEMS);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	FIntPoint DstSize = PrePostSourceViewportSize / 4;

	FIntPoint SrcSize;
	FIntRect SrcRect;
	if(bUsedFramebufferFetch)
	{
		// Mobile with framebuffer fetch uses view rect as source.
		const FSceneView& View = Context.View;
		SrcSize = InputDesc->Extent;
		//	uint32 ScaleFactor = View.ViewRect.Width() / SrcSize.X;
		//	SrcRect = View.ViewRect / ScaleFactor;
		// TODO: This won't work with scaled views.
		SrcRect = View.ViewRect;
	}
	else
	{
		// Otherwise using exact size texture.
		SrcSize = DstSize;
		SrcRect = DstRect;
	}

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	SetShader(Context);

	TShaderMapRef<FPostProcessBloomSetupVS_ES2> VertexShader(GetGlobalShaderMap());

	DrawRectangle(
		0, 0,
		DstX, DstY,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DstSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessBloomSetupES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_FloatRGBA;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("BloomSetup");
	return Ret;
}




//
// BLOOM SETUP SMALL (BLOOM)
//

class FPostProcessBloomSetupSmallVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomSetupSmallVS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	/** Default constructor. */
	FPostProcessBloomSetupSmallVS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FPostProcessBloomSetupSmallVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters(ShaderRHI, Context.View);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

class FPostProcessBloomSetupSmallPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomSetupSmallPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessBloomSetupSmallPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter BloomThreshold;

	/** Initialization constructor. */
	FPostProcessBloomSetupSmallPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		BloomThreshold.Bind(Initializer.ParameterMap, TEXT("BloomThreshold"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;

		FGlobalShader::SetParameters(ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		float ExposureScale = FRCPassPostProcessEyeAdaptation::ComputeExposureScaleValue(Context.View);

		FVector4 BloomThresholdValue(Settings.BloomThreshold, 0, 0, ExposureScale);
		SetShaderValue(ShaderRHI, BloomThreshold, BloomThresholdValue);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << BloomThreshold;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomSetupSmallVS_ES2,TEXT("PostProcessMobile"),TEXT("BloomSmallVS_ES2"),SF_Vertex);

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomSetupSmallPS_ES2,TEXT("PostProcessMobile"),TEXT("BloomSmallPS_ES2"),SF_Pixel);


void FRCPassPostProcessBloomSetupSmallES2::SetShader(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessBloomSetupSmallVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessBloomSetupSmallPS_ES2> PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessBloomSetupSmallES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessBloomSetupSmall, DEC_SCENE_ITEMS);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	FIntPoint DstSize = PrePostSourceViewportSize / 4;

	FIntPoint SrcSize;
	FIntRect SrcRect;
	if(bUsedFramebufferFetch)
	{
		// Mobile with framebuffer fetch uses view rect as source.
		const FSceneView& View = Context.View;
		SrcSize = InputDesc->Extent;
		//	uint32 ScaleFactor = View.ViewRect.Width() / SrcSize.X;
		//	SrcRect = View.ViewRect / ScaleFactor;
		// TODO: This won't work with scaled views.
		SrcRect = View.ViewRect;
	}
	else
	{
		// Otherwise using exact size texture.
		SrcSize = DstSize;
		SrcRect = DstRect;
	}

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	SetShader(Context);

	TShaderMapRef<FPostProcessBloomSetupSmallVS_ES2> VertexShader(GetGlobalShaderMap());
	DrawRectangle(
		0, 0,
		DstX, DstY,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DstSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessBloomSetupSmallES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_FloatRGBA;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("BloomSetupSmall");
	return Ret;
}






//
// BLOOM DOWNSAMPLE
//

class FPostProcessBloomDownPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomDownPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessBloomDownPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessBloomDownPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomDownPS_ES2,TEXT("PostProcessMobile"),TEXT("BloomDownPS_ES2"),SF_Pixel);


class FPostProcessBloomDownVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomDownVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessBloomDownVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter BloomDownScale;

	FPostProcessBloomDownVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		BloomDownScale.Bind(Initializer.ParameterMap, TEXT("BloomDownScale"));
	}

	void SetVS(const FRenderingCompositePassContext& Context, float InScale)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		SetShaderValue(ShaderRHI, BloomDownScale, InScale);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << BloomDownScale;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomDownVS_ES2,TEXT("PostProcessMobile"),TEXT("BloomDownVS_ES2"),SF_Vertex);


void FRCPassPostProcessBloomDownES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessBloomDown, DEC_SCENE_ITEMS);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/2);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/2);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessBloomDownVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessBloomDownPS_ES2> PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context, Scale);
	PixelShader->SetPS(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize/2;

	DrawRectangle(
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessBloomDownES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_FloatRGBA;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/2);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/2);
	Ret.DebugName = TEXT("BloomDown");
	return Ret;
}





//
// BLOOM UPSAMPLE
//

class FPostProcessBloomUpPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomUpPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessBloomUpPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter TintA;
	FShaderParameter TintB;

	FPostProcessBloomUpPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		TintA.Bind(Initializer.ParameterMap, TEXT("BloomTintA"));
		TintB.Bind(Initializer.ParameterMap, TEXT("BloomTintB"));
	}

	void SetPS(const FRenderingCompositePassContext& Context, FVector4& InTintA, FVector4& InTintB)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		SetShaderValue(ShaderRHI, TintA, InTintA);
		SetShaderValue(ShaderRHI, TintB, InTintB);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << TintA << TintB;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomUpPS_ES2,TEXT("PostProcessMobile"),TEXT("BloomUpPS_ES2"),SF_Pixel);


class FPostProcessBloomUpVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessBloomUpVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessBloomUpVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter BloomUpScales;

	FPostProcessBloomUpVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		BloomUpScales.Bind(Initializer.ParameterMap, TEXT("BloomUpScales"));
	}

	void SetVS(const FRenderingCompositePassContext& Context, FVector2D InScale)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		SetShaderValue(ShaderRHI, BloomUpScales, InScale);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << BloomUpScales;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessBloomUpVS_ES2,TEXT("PostProcessMobile"),TEXT("BloomUpVS_ES2"),SF_Vertex);


void FRCPassPostProcessBloomUpES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessBloomUp, DEC_SCENE_ITEMS);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessBloomUpVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessBloomUpPS_ES2> PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	// The 1/8 factor is because bloom is using 8 taps in the filter.
	VertexShader->SetVS(Context, FVector2D(ScaleAB.X, ScaleAB.Y));
	FVector4 TintAScaled = TintA * (1.0f/8.0f);
	FVector4 TintBScaled = TintB * (1.0f/8.0f);
	PixelShader->SetPS(Context, TintAScaled, TintBScaled);

	FIntPoint SrcDstSize = PrePostSourceViewportSize;

	DrawRectangle(
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessBloomUpES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_FloatRGBA;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y);
	Ret.DebugName = TEXT("BloomUp");
	return Ret;
}





//
// SUN MASK
//

template <uint32 UseFetchSunDof> // 0=none, 1=dof, 2=sun, 3=sun&dof, {4,5,6,7}=ES2_USE_FETCH
class FPostProcessSunMaskPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunMaskPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ES2_USE_FETCH"), (UseFetchSunDof & 4) ? (uint32)1 : (uint32)0);
		OutEnvironment.SetDefine(TEXT("ES2_USE_SUN"), (UseFetchSunDof & 2) ? (uint32)1 : (uint32)0);
		OutEnvironment.SetDefine(TEXT("ES2_USE_DOF"), (UseFetchSunDof & 1) ? (uint32)1 : (uint32)0);
	}

	FPostProcessSunMaskPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter SunColorApertureDiv2Parameter;

	FPostProcessSunMaskPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		SunColorApertureDiv2Parameter.Bind(Initializer.ParameterMap, TEXT("SunColorApertureDiv2"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		FVector4 SunColorApertureDiv2;
		SunColorApertureDiv2.X = Context.View.LightShaftColorMask.R;
		SunColorApertureDiv2.Y = Context.View.LightShaftColorMask.G;
		SunColorApertureDiv2.Z = Context.View.LightShaftColorMask.B;
		SunColorApertureDiv2.W = Context.View.FinalPostProcessSettings.DepthOfFieldScale * 0.5f;
		SetShaderValue(ShaderRHI, SunColorApertureDiv2Parameter, SunColorApertureDiv2);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << SunColorApertureDiv2Parameter;
		return bShaderHasOutdatedParameters;
	}
};

typedef FPostProcessSunMaskPS_ES2<0> FPostProcessSunMaskPS_ES2_0;
typedef FPostProcessSunMaskPS_ES2<1> FPostProcessSunMaskPS_ES2_1;
typedef FPostProcessSunMaskPS_ES2<2> FPostProcessSunMaskPS_ES2_2;
typedef FPostProcessSunMaskPS_ES2<3> FPostProcessSunMaskPS_ES2_3;
typedef FPostProcessSunMaskPS_ES2<4> FPostProcessSunMaskPS_ES2_4;
typedef FPostProcessSunMaskPS_ES2<5> FPostProcessSunMaskPS_ES2_5;
typedef FPostProcessSunMaskPS_ES2<6> FPostProcessSunMaskPS_ES2_6;
typedef FPostProcessSunMaskPS_ES2<7> FPostProcessSunMaskPS_ES2_7;
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMaskPS_ES2_0,TEXT("PostProcessMobile"),TEXT("SunMaskPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMaskPS_ES2_1,TEXT("PostProcessMobile"),TEXT("SunMaskPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMaskPS_ES2_2,TEXT("PostProcessMobile"),TEXT("SunMaskPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMaskPS_ES2_3,TEXT("PostProcessMobile"),TEXT("SunMaskPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMaskPS_ES2_4,TEXT("PostProcessMobile"),TEXT("SunMaskPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMaskPS_ES2_5,TEXT("PostProcessMobile"),TEXT("SunMaskPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMaskPS_ES2_6,TEXT("PostProcessMobile"),TEXT("SunMaskPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMaskPS_ES2_7,TEXT("PostProcessMobile"),TEXT("SunMaskPS_ES2"),SF_Pixel);


class FPostProcessSunMaskVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunMaskVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessSunMaskVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessSunMaskVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunMaskVS_ES2,TEXT("PostProcessMobile"),TEXT("SunMaskVS_ES2"),SF_Vertex);

template <uint32 UseFetchSunDof>
static void SunMask_SetShader(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessSunMaskVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessSunMaskPS_ES2<UseFetchSunDof> > PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessSunMaskES2::SetShader(const FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;
	uint32 UseSun = Context.View.bLightShaftUse ? 1 : 0;
	uint32 UseDof = (Context.View.FinalPostProcessSettings.DepthOfFieldScale > 0.0f) ? 1 : 0;
	uint32 UseFetch = GSupportsShaderFramebufferFetch ? 1 : 0;
	uint32 UseFetchSunDof = (UseFetch << 2) + (UseSun << 1) + UseDof;

	switch(UseFetchSunDof)
	{
		case 0: SunMask_SetShader<0>(Context); break;
		case 1: SunMask_SetShader<1>(Context); break;
		case 2: SunMask_SetShader<2>(Context); break;
		case 3: SunMask_SetShader<3>(Context); break;
		case 4: SunMask_SetShader<4>(Context); break;
		case 5: SunMask_SetShader<5>(Context); break;
		case 6: SunMask_SetShader<6>(Context); break;
		case 7: SunMask_SetShader<7>(Context); break;
	}
}

void FRCPassPostProcessSunMaskES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessSunMask, DEC_SCENE_ITEMS);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	FIntPoint DstSize = PrePostSourceViewportSize;

	FIntPoint SrcSize;
	FIntRect SrcRect;
	const FSceneView& View = Context.View;

	TShaderMapRef<FPostProcessSunMaskVS_ES2> VertexShader(GetGlobalShaderMap());

	if(bOnChip)
	{
		SrcSize = DstSize;
		SrcRect = DstRect;

		Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

		RHISetBlendState(TStaticBlendState<>::GetRHI());
		RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

		SetShader(Context);

		DrawRectangle(
			0, 0,
			DstX, DstY,
			SrcRect.Min.X, SrcRect.Min.Y,
			SrcRect.Width(), SrcRect.Height(),
			DstSize,
			SrcSize,
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}
	else
	{
		SrcSize = InputDesc->Extent;
		//	uint32 ScaleFactor = View.ViewRect.Width() / SrcSize.X;
		//	SrcRect = View.ViewRect / ScaleFactor;
		// TODO: This won't work with scaled views.
		SrcRect = View.ViewRect;

		const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
		RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

		// is optimized away if possible (RT size=view size, )
		RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

		Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

		RHISetBlendState(TStaticBlendState<>::GetRHI());
		RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

		SetShader(Context);

		DrawRectangle(
			0, 0,
			DstX, DstY,
			SrcRect.Min.X, SrcRect.Min.Y,
			SrcRect.Width(), SrcRect.Height(),
			DstSize,
			SrcSize,
			*VertexShader,
			EDRF_UseTriangleOptimization);

		RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
	}
}

FPooledRenderTargetDesc FRCPassPostProcessSunMaskES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_FloatRGBA;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y);
	Ret.DebugName = TEXT("SunMask");
	return Ret;
}




//
// SUN ALPHA
//

template<uint32 UseDof>
class FPostProcessSunAlphaPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunAlphaPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ES2_USE_DOF"), UseDof ? (uint32)1 : (uint32)0);
	}

	FPostProcessSunAlphaPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessSunAlphaPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

typedef FPostProcessSunAlphaPS_ES2<0> FPostProcessSunAlphaPS_ES2_0;
typedef FPostProcessSunAlphaPS_ES2<1> FPostProcessSunAlphaPS_ES2_1;
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunAlphaPS_ES2_0,TEXT("PostProcessMobile"),TEXT("SunAlphaPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunAlphaPS_ES2_1,TEXT("PostProcessMobile"),TEXT("SunAlphaPS_ES2"),SF_Pixel);

class FPostProcessSunAlphaVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunAlphaVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessSunAlphaVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter LightShaftCenter;

	FPostProcessSunAlphaVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		LightShaftCenter.Bind(Initializer.ParameterMap, TEXT("LightShaftCenter"));
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		SetShaderValue(ShaderRHI, LightShaftCenter, Context.View.LightShaftCenter);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << LightShaftCenter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunAlphaVS_ES2,TEXT("PostProcessMobile"),TEXT("SunAlphaVS_ES2"),SF_Vertex);

template <uint32 UseDof>
static void SunAlpha_SetShader(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessSunAlphaVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessSunAlphaPS_ES2<UseDof> > PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessSunAlphaES2::SetShader(const FRenderingCompositePassContext& Context)
{
	if(Context.View.FinalPostProcessSettings.DepthOfFieldScale > 0.0f)
	{
		SunAlpha_SetShader<1>(Context);
	}
	else
	{
		SunAlpha_SetShader<0>(Context);
	}
}

void FRCPassPostProcessSunAlphaES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessSunAlpha, DEC_SCENE_ITEMS);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	SetShader(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 4;
	TShaderMapRef<FPostProcessSunAlphaVS_ES2> VertexShader(GetGlobalShaderMap());

	DrawRectangle(
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessSunAlphaES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	// Only need one 8-bit channel as output (but mobile hardware often doesn't support that as a render target format).
	// Highlight compression (tonemapping) was used to keep this in 8-bit.
	Ret.Format = PF_B8G8R8A8;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("SunAlpha");
	return Ret;
}





//
// SUN BLUR
//

class FPostProcessSunBlurPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunBlurPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessSunBlurPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessSunBlurPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunBlurPS_ES2,TEXT("PostProcessMobile"),TEXT("SunBlurPS_ES2"),SF_Pixel);


class FPostProcessSunBlurVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunBlurVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessSunBlurVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter LightShaftCenter;

	FPostProcessSunBlurVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		LightShaftCenter.Bind(Initializer.ParameterMap, TEXT("LightShaftCenter"));
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		SetShaderValue(ShaderRHI, LightShaftCenter, Context.View.LightShaftCenter);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << LightShaftCenter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunBlurVS_ES2,TEXT("PostProcessMobile"),TEXT("SunBlurVS_ES2"),SF_Vertex);


void FRCPassPostProcessSunBlurES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessSunBlur, DEC_SCENE_ITEMS);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessSunBlurVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessSunBlurPS_ES2> PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 4;

	DrawRectangle(
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessSunBlurES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	// Only need one 8-bit channel as output (but mobile hardware often doesn't support that as a render target format).
	// Highlight compression (tonemapping) was used to keep this in 8-bit.
	Ret.Format = PF_B8G8R8A8;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("SunBlur");
	return Ret;
}




//
// SUN MERGE
//

template <uint32 UseSunBloom>
class FPostProcessSunMergePS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunMergePS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ES2_USE_BLOOM"), (UseSunBloom & 1) ? (uint32)1 : (uint32)0);
		OutEnvironment.SetDefine(TEXT("ES2_USE_SUN"), (UseSunBloom >> 1) ? (uint32)1 : (uint32)0);
	}

	FPostProcessSunMergePS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter SunColorVignetteIntensity;
	FShaderParameter VignetteColor;
	FShaderParameter BloomColor;

	FPostProcessSunMergePS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		SunColorVignetteIntensity.Bind(Initializer.ParameterMap, TEXT("SunColorVignetteIntensity"));
		VignetteColor.Bind(Initializer.ParameterMap, TEXT("VignetteColor"));
		BloomColor.Bind(Initializer.ParameterMap, TEXT("BloomColor"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		FVector4 SunColorVignetteIntensityParam;
		SunColorVignetteIntensityParam.X = Context.View.LightShaftColorApply.R;
		SunColorVignetteIntensityParam.Y = Context.View.LightShaftColorApply.G;
		SunColorVignetteIntensityParam.Z = Context.View.LightShaftColorApply.B;
		SunColorVignetteIntensityParam.W = Settings.VignetteIntensity;
		SetShaderValue(ShaderRHI, SunColorVignetteIntensity, SunColorVignetteIntensityParam);

		SetShaderValue(ShaderRHI, VignetteColor, Context.View.FinalPostProcessSettings.VignetteColor);

		// Scaling Bloom1 by extra factor to match filter area difference between PC default and mobile.
		SetShaderValue(ShaderRHI, BloomColor, Context.View.FinalPostProcessSettings.Bloom1Tint * Context.View.FinalPostProcessSettings.BloomIntensity * 0.5);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << SunColorVignetteIntensity << VignetteColor << BloomColor;
		return bShaderHasOutdatedParameters;
	}
};

typedef FPostProcessSunMergePS_ES2<0> FPostProcessSunMergePS_ES2_0;
typedef FPostProcessSunMergePS_ES2<1> FPostProcessSunMergePS_ES2_1;
typedef FPostProcessSunMergePS_ES2<2> FPostProcessSunMergePS_ES2_2;
typedef FPostProcessSunMergePS_ES2<3> FPostProcessSunMergePS_ES2_3;
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMergePS_ES2_0,TEXT("PostProcessMobile"),TEXT("SunMergePS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMergePS_ES2_1,TEXT("PostProcessMobile"),TEXT("SunMergePS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMergePS_ES2_2,TEXT("PostProcessMobile"),TEXT("SunMergePS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessSunMergePS_ES2_3,TEXT("PostProcessMobile"),TEXT("SunMergePS_ES2"),SF_Pixel);


class FPostProcessSunMergeVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunMergeVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessSunMergeVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter LightShaftCenter;

	FPostProcessSunMergeVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		LightShaftCenter.Bind(Initializer.ParameterMap, TEXT("LightShaftCenter"));
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		SetShaderValue(ShaderRHI, LightShaftCenter, Context.View.LightShaftCenter);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << LightShaftCenter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunMergeVS_ES2,TEXT("PostProcessMobile"),TEXT("SunMergeVS_ES2"),SF_Vertex);



template <uint32 UseSunBloom>
static void SunMerge_SetShader(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessSunMergeVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessSunMergePS_ES2<UseSunBloom> > PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessSunMergeES2::SetShader(const FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;
	uint32 UseBloom = (View.FinalPostProcessSettings.BloomIntensity > 0.0f) ? 1 : 0;
	uint32 UseSun = Context.View.bLightShaftUse ? 1 : 0;
	uint32 UseSunBloom = UseBloom + (UseSun<<1);

	switch(UseSunBloom)
	{
	case 0: SunMerge_SetShader<0>(Context); break;
	case 1: SunMerge_SetShader<1>(Context); break;
	case 2: SunMerge_SetShader<2>(Context); break;
	case 3: SunMerge_SetShader<3>(Context); break;
	}
}

void FRCPassPostProcessSunMergeES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessSunMerge, DEC_SCENE_ITEMS);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	SetShader(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 4;
	TShaderMapRef<FPostProcessSunMergeVS_ES2> VertexShader(GetGlobalShaderMap());

	DrawRectangle(
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	// Double buffer sun+bloom+vignette composite.
	if(Context.View.FinalPostProcessSettings.AntiAliasingMethod == AAM_TemporalAA)
	{
		FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;
		if(ViewState) 
		{
			ViewState->MobileAaBloomSunVignette0 = PassOutputs[0].PooledRenderTarget;
		}
	}
}

FPooledRenderTargetDesc FRCPassPostProcessSunMergeES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// This might not have a valid input texture.
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_FloatRGBA;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("SunMerge");
	return Ret;
}





//
// SUN MERGE SMALL (BLOOM)
//

class FPostProcessSunMergeSmallPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunMergeSmallPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessSunMergeSmallPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter SunColorVignetteIntensity;
	FShaderParameter VignetteColor;
	FShaderParameter BloomColor;
	FShaderParameter BloomColor2;

	FPostProcessSunMergeSmallPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		SunColorVignetteIntensity.Bind(Initializer.ParameterMap, TEXT("SunColorVignetteIntensity"));
		VignetteColor.Bind(Initializer.ParameterMap, TEXT("VignetteColor"));
		BloomColor.Bind(Initializer.ParameterMap, TEXT("BloomColor"));
		BloomColor2.Bind(Initializer.ParameterMap, TEXT("BloomColor2"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		FVector4 SunColorVignetteIntensityParam;
		SunColorVignetteIntensityParam.X = Context.View.LightShaftColorApply.R;
		SunColorVignetteIntensityParam.Y = Context.View.LightShaftColorApply.G;
		SunColorVignetteIntensityParam.Z = Context.View.LightShaftColorApply.B;
		SunColorVignetteIntensityParam.W = Settings.VignetteIntensity;
		SetShaderValue(ShaderRHI, SunColorVignetteIntensity, SunColorVignetteIntensityParam);

		SetShaderValue(ShaderRHI, VignetteColor, Context.View.FinalPostProcessSettings.VignetteColor);

		// Scaling Bloom1 by extra factor to match filter area difference between PC default and mobile.
		SetShaderValue(ShaderRHI, BloomColor, Context.View.FinalPostProcessSettings.Bloom1Tint * Context.View.FinalPostProcessSettings.BloomIntensity * 0.5);
		SetShaderValue(ShaderRHI, BloomColor2, Context.View.FinalPostProcessSettings.Bloom2Tint * Context.View.FinalPostProcessSettings.BloomIntensity);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << SunColorVignetteIntensity << VignetteColor << BloomColor << BloomColor2;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunMergeSmallPS_ES2,TEXT("PostProcessMobile"),TEXT("SunMergeSmallPS_ES2"),SF_Pixel);

class FPostProcessSunMergeSmallVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunMergeSmallVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessSunMergeSmallVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessSunMergeSmallVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunMergeSmallVS_ES2,TEXT("PostProcessMobile"),TEXT("SunMergeSmallVS_ES2"),SF_Vertex);

void FRCPassPostProcessSunMergeSmallES2::SetShader(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessSunMergeSmallVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessSunMergeSmallPS_ES2> PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessSunMergeSmallES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessSunMergeSmall, DEC_SCENE_ITEMS);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	SetShader(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 4;
	TShaderMapRef<FPostProcessSunMergeSmallVS_ES2> VertexShader(GetGlobalShaderMap());

	DrawRectangle(
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	// Double buffer sun+bloom+vignette composite.
	if(Context.View.FinalPostProcessSettings.AntiAliasingMethod == AAM_TemporalAA)
	{
		FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;
		if(ViewState) 
		{
			ViewState->MobileAaBloomSunVignette0 = PassOutputs[0].PooledRenderTarget;
		}
	}
}

FPooledRenderTargetDesc FRCPassPostProcessSunMergeSmallES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// This might not have a valid input texture.
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_FloatRGBA;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("SunMergeSmall");
	return Ret;
}









//
// DOF DOWNSAMPLE
//

class FPostProcessDofDownVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDofDownVS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	/** Default constructor. */
	FPostProcessDofDownVS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FPostProcessDofDownVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters(ShaderRHI, Context.View);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

template<uint32 UseSun>
class FPostProcessDofDownPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDofDownPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ES2_USE_SUN"), UseSun ? (uint32)1 : (uint32)0);
	}

	FPostProcessDofDownPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessDofDownPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessDofDownVS_ES2,TEXT("PostProcessMobile"),TEXT("DofDownVS_ES2"),SF_Vertex);

typedef FPostProcessDofDownPS_ES2<0> FPostProcessDofDownPS_ES2_0;
typedef FPostProcessDofDownPS_ES2<1> FPostProcessDofDownPS_ES2_1;
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessDofDownPS_ES2_0,TEXT("PostProcessMobile"),TEXT("DofDownPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessDofDownPS_ES2_1,TEXT("PostProcessMobile"),TEXT("DofDownPS_ES2"),SF_Pixel);

template <uint32 UseSun>
static void DofDown_SetShader(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessDofDownVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessDofDownPS_ES2<UseSun> > PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessDofDownES2::SetShader(const FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;
	if(Context.View.bLightShaftUse)
	{
		DofDown_SetShader<1>(Context);
	}
	else
	{
		DofDown_SetShader<0>(Context);
	}
}

void FRCPassPostProcessDofDownES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessDofDown, DEC_SCENE_ITEMS);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/2);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/2);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	FIntPoint DstSize = PrePostSourceViewportSize / 2;

	FIntPoint SrcSize;
	FIntRect SrcRect;
	if(bUsedFramebufferFetch)
	{
		// Mobile with framebuffer fetch uses view rect as source.
		const FSceneView& View = Context.View;
		SrcSize = InputDesc->Extent;
		//	uint32 ScaleFactor = View.ViewRect.Width() / SrcSize.X;
		//	SrcRect = View.ViewRect / ScaleFactor;
		// TODO: This won't work with scaled views.
		SrcRect = View.ViewRect;
	}
	else
	{
		// Otherwise using exact size texture.
		SrcSize = DstSize;
		SrcRect = DstRect;
	}

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	SetShader(Context);

	TShaderMapRef<FPostProcessDofDownVS_ES2> VertexShader(GetGlobalShaderMap());

	DrawRectangle(
		0, 0,
		DstX, DstY,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DstSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessDofDownES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_FloatRGBA;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/2);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/2);
	Ret.DebugName = TEXT("DofDown");
	return Ret;
}




//
// DOF NEAR
//

class FPostProcessDofNearVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDofNearVS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	/** Default constructor. */
	FPostProcessDofNearVS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FPostProcessDofNearVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters(ShaderRHI, Context.View);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

template<uint32 UseSun>
class FPostProcessDofNearPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDofNearPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ES2_USE_SUN"), UseSun ? (uint32)1 : (uint32)0);
	}

	FPostProcessDofNearPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessDofNearPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessDofNearVS_ES2,TEXT("PostProcessMobile"),TEXT("DofNearVS_ES2"),SF_Vertex);

typedef FPostProcessDofNearPS_ES2<0> FPostProcessDofNearPS_ES2_0;
typedef FPostProcessDofNearPS_ES2<1> FPostProcessDofNearPS_ES2_1;
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessDofNearPS_ES2_0,TEXT("PostProcessMobile"),TEXT("DofNearPS_ES2"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FPostProcessDofNearPS_ES2_1,TEXT("PostProcessMobile"),TEXT("DofNearPS_ES2"),SF_Pixel);

template <uint32 UseSun>
static void DofNear_SetShader(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessDofNearVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessDofNearPS_ES2<UseSun> > PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessDofNearES2::SetShader(const FRenderingCompositePassContext& Context)
{
	const FSceneView& View = Context.View;
	if(Context.View.bLightShaftUse)
	{
		DofNear_SetShader<1>(Context);
	}
	else
	{
		DofNear_SetShader<0>(Context);
	}
}

void FRCPassPostProcessDofNearES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessDofNear, DEC_SCENE_ITEMS);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	SetShader(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 4;
	TShaderMapRef<FPostProcessDofNearVS_ES2> VertexShader(GetGlobalShaderMap());

	DrawRectangle(
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessDofNearES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	// Only need one 8-bit channel as output (but mobile hardware often doesn't support that as a render target format).
	Ret.Format = PF_B8G8R8A8;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("DofNear");
	return Ret;
}



//
// DOF BLUR
//

class FPostProcessDofBlurPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDofBlurPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessDofBlurPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessDofBlurPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessDofBlurPS_ES2,TEXT("PostProcessMobile"),TEXT("DofBlurPS_ES2"),SF_Pixel);


class FPostProcessDofBlurVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessDofBlurVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessDofBlurVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessDofBlurVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessDofBlurVS_ES2,TEXT("PostProcessMobile"),TEXT("DofBlurVS_ES2"),SF_Vertex);


void FRCPassPostProcessDofBlurES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessDofBlur, DEC_SCENE_ITEMS);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/2);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/2);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessDofBlurVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessDofBlurPS_ES2> PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 2;

	DrawRectangle(
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessDofBlurES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_FloatRGBA;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/2);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/2);
	Ret.DebugName = TEXT("DofBlur");
	return Ret;
}






//
// SUN AVG
//

class FPostProcessSunAvgPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunAvgPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessSunAvgPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessSunAvgPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunAvgPS_ES2,TEXT("PostProcessMobile"),TEXT("SunAvgPS_ES2"),SF_Pixel);



class FPostProcessSunAvgVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessSunAvgVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessSunAvgVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessSunAvgVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessSunAvgVS_ES2,TEXT("PostProcessMobile"),TEXT("SunAvgVS_ES2"),SF_Vertex);



static void SunAvg_SetShader(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessSunAvgVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessSunAvgPS_ES2> PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessSunAvgES2::SetShader(const FRenderingCompositePassContext& Context)
{
	SunAvg_SetShader(Context);
}

void FRCPassPostProcessSunAvgES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessSunAvg, DEC_SCENE_ITEMS);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X/4);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y/4);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	SetShader(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize / 4;
	TShaderMapRef<FPostProcessSunAvgVS_ES2> VertexShader(GetGlobalShaderMap());

	DrawRectangle(
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessSunAvgES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_FloatRGBA;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X/4);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y/4);
	Ret.DebugName = TEXT("SunAvg");
	return Ret;
}





//
// MOBILE AA
//

class FPostProcessAaPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessAaPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}	

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FPostProcessAaPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter AaBlendAmount;

	FPostProcessAaPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		AaBlendAmount.Bind(Initializer.ParameterMap, TEXT("AaBlendAmount"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		// Compute the blend factor which decides the trade off between ghosting in motion and flicker when not moving.
		// This works by computing the screen space motion vector of distant point at the center of the screen.
		// This factor will effectively provide an idea of the amount of camera rotation.
		// Higher camera rotation = less blend factor (0.0).
		// Lower or no camera rotation = high blend factor (0.25).
		FSceneViewState* ViewState = (FSceneViewState*)Context.View.State;
		if(ViewState)
		{
			FMatrix Proj = Context.View.ViewMatrices.ProjMatrix;
			FMatrix PrevProj = ViewState->PrevViewMatrices.ProjMatrix;

			// Remove jitter
			Proj.M[2][0] = 0.0f;
			Proj.M[2][1] = 0.0f;
			PrevProj.M[2][0] = 0.0f;
			PrevProj.M[2][1] = 0.0f;

			FMatrix ViewProj = ( Context.View.ViewMatrices.ViewMatrix * Proj ).GetTransposed();
			FMatrix PrevViewProj = ( ViewState->PrevViewMatrices.ViewMatrix * PrevProj ).GetTransposed();

			double InvViewProj[16];
			Inverse4x4( InvViewProj, (float*)ViewProj.M );

			const float* p = (float*)PrevViewProj.M;

			const double cxx = InvViewProj[ 0]; const double cxy = InvViewProj[ 1]; const double cxz = InvViewProj[ 2]; const double cxw = InvViewProj[ 3];
			const double cyx = InvViewProj[ 4]; const double cyy = InvViewProj[ 5]; const double cyz = InvViewProj[ 6]; const double cyw = InvViewProj[ 7];
			const double czx = InvViewProj[ 8]; const double czy = InvViewProj[ 9]; const double czz = InvViewProj[10]; const double czw = InvViewProj[11];
			const double cwx = InvViewProj[12]; const double cwy = InvViewProj[13]; const double cwz = InvViewProj[14]; const double cww = InvViewProj[15];

			const double pxx = (double)(p[ 0]); const double pxy = (double)(p[ 1]); const double pxz = (double)(p[ 2]); const double pxw = (double)(p[ 3]);
			const double pyx = (double)(p[ 4]); const double pyy = (double)(p[ 5]); const double pyz = (double)(p[ 6]); const double pyw = (double)(p[ 7]);
			const double pwx = (double)(p[12]); const double pwy = (double)(p[13]); const double pwz = (double)(p[14]); const double pww = (double)(p[15]);

			float CameraMotion0W = (float)(2.0*(cww*pww - cwx*pww + cwy*pww + (cxw - cxx + cxy)*pwx + (cyw - cyx + cyy)*pwy + (czw - czx + czy)*pwz));
			float CameraMotion2Z = (float)(cwy*pww + cwy*pxw + cww*(pww + pxw) - cwx*(pww + pxw) + (cxw - cxx + cxy)*(pwx + pxx) + (cyw - cyx + cyy)*(pwy + pxy) + (czw - czx + czy)*(pwz + pxz));
			float CameraMotion4Z = (float)(cwy*pww + cww*(pww - pyw) - cwy*pyw + cwx*((-pww) + pyw) + (cxw - cxx + cxy)*(pwx - pyx) + (cyw - cyx + cyy)*(pwy - pyy) + (czw - czx + czy)*(pwz - pyz));

			// Depth surface 0=far, 1=near.
			// This is simplified to compute camera motion with depth = 0.0 (infinitely far away).
			// Camera motion for pixel (in ScreenPos space).
			float ScaleM = 1.0f / CameraMotion0W;
			// Back projection value (projected screen space).
			float BackX = CameraMotion2Z * ScaleM;
			float BackY = CameraMotion4Z * ScaleM;

			// Start with the distance in screen space.
			float BlendAmount = BackX * BackX + BackY * BackY;
			if(BlendAmount > 0.0f)
			{
				BlendAmount = sqrt(BlendAmount);
			}
			
			// Higher numbers truncate anti-aliasing and ghosting faster.
			float BlendEffect = 8.0f;
			BlendAmount = 0.25f - BlendAmount * BlendEffect;
			if(BlendAmount < 0.0f)
			{
				BlendAmount = 0.0f;
			}

			SetShaderValue(ShaderRHI, AaBlendAmount, BlendAmount);
		}
		else
		{
			float BlendAmount = 0.0;
			SetShaderValue(ShaderRHI, AaBlendAmount, BlendAmount);
		}
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << AaBlendAmount;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessAaPS_ES2,TEXT("PostProcessMobile"),TEXT("AaPS_ES2"),SF_Pixel);



class FPostProcessAaVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessAaVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessAaVS_ES2(){}

public:
	FPostProcessPassParameters PostprocessParameter;

	FPostProcessAaVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessAaVS_ES2,TEXT("PostProcessMobile"),TEXT("AaVS_ES2"),SF_Vertex);



static void Aa_SetShader(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessAaVS_ES2> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessAaPS_ES2> PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetVS(Context);
	PixelShader->SetPS(Context);
}

void FRCPassPostProcessAaES2::SetShader(const FRenderingCompositePassContext& Context)
{
	Aa_SetShader(Context);
}

void FRCPassPostProcessAaES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessAa, DEC_SCENE_ITEMS);

	uint32 DstX = FMath::Max(1, PrePostSourceViewportSize.X);
	uint32 DstY = FMath::Max(1, PrePostSourceViewportSize.Y);

	FIntRect DstRect;
	DstRect.Min.X = 0;
	DstRect.Min.Y = 0;
	DstRect.Max.X = DstX;
	DstRect.Max.Y = DstY;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	

	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());

	Context.SetViewportAndCallRHI(0, 0, 0.0f, DstX, DstY, 1.0f );

	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	SetShader(Context);

	FIntPoint SrcDstSize = PrePostSourceViewportSize;
	TShaderMapRef<FPostProcessAaVS_ES2> VertexShader(GetGlobalShaderMap());

	DrawRectangle(
		0, 0,
		DstX, DstY,
		0, 0,
		DstX, DstY,
		SrcDstSize,
		SrcDstSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessAaES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret;
	Ret.Depth = 0;
	Ret.ArraySize = 1;
	Ret.bIsArray = false;
	Ret.NumMips = 1;
	Ret.TargetableFlags = TexCreate_RenderTargetable;
	Ret.bForceSeparateTargetAndShaderResource = false;
	Ret.Format = PF_B8G8R8A8;
	Ret.NumSamples = 1;
	Ret.Extent.X = FMath::Max(1, PrePostSourceViewportSize.X);
	Ret.Extent.Y = FMath::Max(1, PrePostSourceViewportSize.Y);
	Ret.DebugName = TEXT("Aa");
	return Ret;
}

