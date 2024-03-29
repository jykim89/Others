// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTestImage.cpp: Post processing TestImage implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessTestImage.h"
#include "PostProcessing.h"
#include "PostProcessHistogram.h"
#include "PostProcessEyeAdaptation.h"
#include "PostProcessCombineLUTs.h"

/** Encapsulates the post processing eye adaptation pixel shader. */
class FPostProcessTestImagePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessTestImagePS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessTestImagePS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter FrameNumber;
	FShaderParameter FrameTime;
	FColorRemapShaderParameters ColorRemapShaderParameters;

	/** Initialization constructor. */
	FPostProcessTestImagePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
		, ColorRemapShaderParameters(Initializer.ParameterMap)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		FrameNumber.Bind(Initializer.ParameterMap,TEXT("FrameNumber"));
		FrameTime.Bind(Initializer.ParameterMap,TEXT("FrameTime"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters(ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		DeferredParameters.Set(ShaderRHI, Context.View);

		{
			uint32 FrameNumberValue = Context.View.FrameNumber;
			SetShaderValue(ShaderRHI, FrameNumber, FrameNumberValue);
		}

		{
			float FrameTimeValue = Context.View.Family->CurrentRealTime;
			SetShaderValue(ShaderRHI, FrameTime, FrameTimeValue);
		}

		ColorRemapShaderParameters.Set(ShaderRHI);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << FrameNumber << FrameTime << ColorRemapShaderParameters;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessTestImagePS,TEXT("PostProcessTestImage"),TEXT("MainPS"),SF_Pixel);

FRCPassPostProcessTestImage::FRCPassPostProcessTestImage()
{
}

void FRCPassPostProcessTestImage::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(TestImage, DEC_SCENE_ITEMS);

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = View.ViewRect;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	
	Context.SetViewportAndCallRHI(DestRect);

	// set the state
	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessTestImagePS> PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetPS(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		0, 0,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		GSceneRenderTargets.GetBufferSizeXY(),
		*VertexShader,
		EDRF_UseTriangleOptimization);

	{
		// this is a helper class for FCanvas to be able to get screen size
		class FRenderTargetTemp : public FRenderTarget
		{
		public:
			const FSceneView& View;
			const FTexture2DRHIRef Texture;

			FRenderTargetTemp(const FSceneView& InView, const FTexture2DRHIRef InTexture)
				: View(InView), Texture(InTexture)
			{
			}
			virtual FIntPoint GetSizeXY() const
			{
				return View.ViewRect.Size();
			};
			virtual const FTexture2DRHIRef& GetRenderTargetTexture() const
			{
				return Texture;
			}
		} TempRenderTarget(View, (const FTexture2DRHIRef&)DestRenderTarget.TargetableTexture);

		FCanvas Canvas(&TempRenderTarget, NULL, ViewFamily.CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime);

		float X = 30;
		float Y = 8;
		const float YStep = 14;
		const float ColumnWidth = 250;

		FString Line;

		Line = FString::Printf(TEXT("Top bars:"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Moving bars using FrameTime"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Black and white raster, Pixel sized, Watch for Moire pattern"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Black and white raster, 2x2 block sized"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("Bottom bars:"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   8 bars near white, 4 right bars should appear as one (HDTV)"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   8 bars near black, 4 left bars should appear as one (HDTV)"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Linear Greyscale in sRGB from 0 to 255"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("Color bars:"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Red, Green, Blue"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("Outside:"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Moving bars using FrameNumber, Tearing without VSync"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("Circles:"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   Should be round and centered"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("Border:"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
		Line = FString::Printf(TEXT("   4 white pixel sized lines (only visible without overscan)"));
		Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));

		Canvas.Flush();
	}


	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessTestImage::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret(FPooledRenderTargetDesc::Create2DDesc(GSceneRenderTargets.GetBufferSizeXY(), PF_B8G8R8A8, TexCreate_None, TexCreate_RenderTargetable, false));

	Ret.DebugName = TEXT("TestImage");

	return Ret;
}