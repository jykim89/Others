// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessLensFlares.cpp: Post processing lens fares implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessLensFlares.h"
#include "PostProcessing.h"

/** Encapsulates a simple copy pixel shader. */
class FPostProcessLensFlareBasePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessLensFlareBasePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM3);
	}

	/** Default constructor. */
	FPostProcessLensFlareBasePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;

	/** Initialization constructor. */
	FPostProcessLensFlareBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessLensFlareBasePS,TEXT("PostProcessLensFlares"),TEXT("CopyPS"),SF_Pixel);


/** Encapsulates the post processing lens flare pixel shader. */
class FPostProcessLensFlaresPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessLensFlaresPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM3);
	}

	/** Default constructor. */
	FPostProcessLensFlaresPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter FlareColor;
	FShaderParameter TexScale;

	/** Initialization constructor. */
	FPostProcessLensFlaresPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		FlareColor.Bind(Initializer.ParameterMap, TEXT("FlareColor"));
		TexScale.Bind(Initializer.ParameterMap, TEXT("TexScale"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << FlareColor << TexScale;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context, FVector2D TexScaleValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		SetShaderValue(ShaderRHI, TexScale, TexScaleValue);
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessLensFlaresPS,TEXT("PostProcessLensFlares"),TEXT("MainPS"),SF_Pixel);



FRCPassPostProcessLensFlares::FRCPassPostProcessLensFlares(float InSizeScale)
	: SizeScale(InSizeScale)
{
}

void FRCPassPostProcessLensFlares::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(LensFlares, DEC_SCENE_ITEMS);

	const FPooledRenderTargetDesc* InputDesc1 = GetInputDesc(ePId_Input0);
	const FPooledRenderTargetDesc* InputDesc2 = GetInputDesc(ePId_Input1);
	
	if(!InputDesc1 || !InputDesc2)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntPoint TexSize1 = InputDesc1->Extent;
	FIntPoint TexSize2 = InputDesc2->Extent;

	uint32 ScaleToFullRes1 = GSceneRenderTargets.GetBufferSizeXY().X / TexSize1.X;
	uint32 ScaleToFullRes2 = GSceneRenderTargets.GetBufferSizeXY().X / TexSize2.X;

	FIntRect ViewRect1 = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleToFullRes1);
	FIntRect ViewRect2 = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleToFullRes2);

	FIntPoint ViewSize1 = ViewRect1.Size();
	FIntPoint ViewSize2 = ViewRect2.Size();

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	
		
	// is optimized away if possible (RT size=view size, )
	RHIClear(true, FLinearColor::Black, false, 1.0f, false, 0, ViewRect1);

	Context.SetViewportAndCallRHI(ViewRect1);

	// set the state
	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(GetGlobalShaderMap());
	
	// setup background (bloom), can be implemented to use additive blending to avoid the read here
	{
		TShaderMapRef<FPostProcessLensFlareBasePS> PixelShader(GetGlobalShaderMap());

		static FGlobalBoundShaderState BoundShaderState;

		SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		VertexShader->SetParameters(Context);
		PixelShader->SetParameters(Context);

		// Draw a quad mapping scene color to the view's render target
		DrawRectangle(
			0, 0,
			ViewSize1.X, ViewSize1.Y,
			ViewRect1.Min.X, ViewRect1.Min.Y,
			ViewSize1.X, ViewSize1.Y,
			ViewSize1,
			TexSize1,
			*VertexShader,
			EDRF_UseTriangleOptimization);
	}

	// additive blend
	RHISetBlendState(TStaticBlendState<CW_RGB,BO_Add,BF_One,BF_One>::GetRHI());

	// add lens flares on top of that
	{
		TShaderMapRef<FPostProcessLensFlaresPS> PixelShader(GetGlobalShaderMap());

		static FGlobalBoundShaderState BoundShaderState;

		SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

		FVector2D TexScaleValue = FVector2D(TexSize2) / ViewSize2;

		VertexShader->SetParameters(Context);
		PixelShader->SetParameters(Context, TexScaleValue);

		// todo: expose
		const uint32 Count = 8;

		// we assume the center of the view is the center of the lens (would not be correct for tiled rendering)
		FVector2D Center = FVector2D(ViewSize1) * 0.5f;

		FLinearColor LensFlareHDRColor = Context.View.FinalPostProcessSettings.LensFlareTint * Context.View.FinalPostProcessSettings.LensFlareIntensity;
	
		// to get the same brightness with 4x more quads (TileSize=1 in LensBlur)
		LensFlareHDRColor.R *= 0.25f;
		LensFlareHDRColor.G *= 0.25f;
		LensFlareHDRColor.B *= 0.25f;

		for(uint32 i = 0; i < Count; ++i)
		{
			FLinearColor FlareColor = Context.View.FinalPostProcessSettings.LensFlareTints[i % 8];
			float NormalizedAlpha = FlareColor.A;
			float Alpha = NormalizedAlpha * 7.0f - 3.5f; 

			// scale to blur outside of the view (only if we use LensBlur)
			Alpha *= SizeScale;
			
			// set the individual flare color
			SetShaderValue(PixelShader->GetPixelShader(), PixelShader->FlareColor, FlareColor * LensFlareHDRColor);

			// Draw a quad mapping scene color to the view's render target
			DrawRectangle(
				Center.X - 0.5f * ViewSize1.X * Alpha, Center.Y - 0.5f * ViewSize1.Y * Alpha,
				ViewSize1.X * Alpha, ViewSize1.Y * Alpha,
				ViewRect2.Min.X, ViewRect2.Min.Y,
				ViewSize2.X, ViewSize2.Y,
				ViewSize1,
				TexSize2,
				*VertexShader,
				EDRF_Default);
		}
	}

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessLensFlares::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("LensFlares");

	return Ret;
}
