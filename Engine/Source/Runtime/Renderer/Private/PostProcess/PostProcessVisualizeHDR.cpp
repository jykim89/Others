// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessVisualizeHDR.cpp: Post processing VisualizeHDR implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessVisualizeHDR.h"
#include "PostProcessing.h"
#include "PostProcessHistogram.h"
#include "PostProcessEyeAdaptation.h"
#include "PostProcessTonemap.h"

/** Encapsulates the post processing eye adaptation pixel shader. */
class FPostProcessVisualizeHDRPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVisualizeHDRPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_COLOR_MATRIX"), 1);
		OutEnvironment.SetDefine(TEXT("USE_SHADOW_TINT"), 1);
		OutEnvironment.SetDefine(TEXT("USE_CONTRAST"), 1);
		OutEnvironment.SetDefine(TEXT("USE_APPROXIMATE_SRGB"), (uint32)0);
	}

	/** Default constructor. */
	FPostProcessVisualizeHDRPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter EyeAdaptationParams;
	FShaderResourceParameter MiniFontTexture;
	FShaderParameter InverseGamma;

	FShaderParameter ColorMatrixR_ColorCurveCd1;
	FShaderParameter ColorMatrixG_ColorCurveCd3Cm3;
	FShaderParameter ColorMatrixB_ColorCurveCm2;
	FShaderParameter ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3;
	FShaderParameter ColorCurve_Ch1_Ch2;
	FShaderParameter ColorShadow_Luma;
	FShaderParameter ColorShadow_Tint1;
	FShaderParameter ColorShadow_Tint2;

	/** Initialization constructor. */
	FPostProcessVisualizeHDRPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		EyeAdaptationParams.Bind(Initializer.ParameterMap, TEXT("EyeAdaptationParams"));
		MiniFontTexture.Bind(Initializer.ParameterMap, TEXT("MiniFontTexture"));
		InverseGamma.Bind(Initializer.ParameterMap,TEXT("InverseGamma"));

		ColorMatrixR_ColorCurveCd1.Bind(Initializer.ParameterMap, TEXT("ColorMatrixR_ColorCurveCd1"));
		ColorMatrixG_ColorCurveCd3Cm3.Bind(Initializer.ParameterMap, TEXT("ColorMatrixG_ColorCurveCd3Cm3"));
		ColorMatrixB_ColorCurveCm2.Bind(Initializer.ParameterMap, TEXT("ColorMatrixB_ColorCurveCm2"));
		ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3"));
		ColorCurve_Ch1_Ch2.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Ch1_Ch2"));
		ColorShadow_Luma.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Luma"));
		ColorShadow_Tint1.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint1"));
		ColorShadow_Tint2.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint2"));
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters(ShaderRHI, Context.View);

		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FSceneViewFamily& ViewFamily = *(Context.View.Family);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		{
			FVector4 Temp[3];

			FRCPassPostProcessEyeAdaptation::ComputeEyeAdaptationParamsValue(Context.View, Temp);
			SetShaderValueArray(ShaderRHI, EyeAdaptationParams, Temp, 3);
		}

		SetTextureParameter(ShaderRHI, MiniFontTexture, GEngine->MiniFontTexture ? GEngine->MiniFontTexture->Resource->TextureRHI : GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture);

		{
			float InvDisplayGammaValue = 1.0f / ViewFamily.RenderTarget->GetDisplayGamma();

			SetShaderValue(ShaderRHI, InverseGamma, InvDisplayGammaValue);
		}

		{
			FVector4 Constants[8];
			FilmPostSetConstants(Constants, ~0, &Context.View.FinalPostProcessSettings, false);
			SetShaderValue(ShaderRHI, ColorMatrixR_ColorCurveCd1, Constants[0]);
			SetShaderValue(ShaderRHI, ColorMatrixG_ColorCurveCd3Cm3, Constants[1]);
			SetShaderValue(ShaderRHI, ColorMatrixB_ColorCurveCm2, Constants[2]); 
			SetShaderValue(ShaderRHI, ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3, Constants[3]); 
			SetShaderValue(ShaderRHI, ColorCurve_Ch1_Ch2, Constants[4]);
			SetShaderValue(ShaderRHI, ColorShadow_Luma, Constants[5]);
			SetShaderValue(ShaderRHI, ColorShadow_Tint1, Constants[6]);
			SetShaderValue(ShaderRHI, ColorShadow_Tint2, Constants[7]);
		}
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << EyeAdaptationParams << MiniFontTexture << InverseGamma 
			<< ColorMatrixR_ColorCurveCd1 << ColorMatrixG_ColorCurveCd3Cm3 << ColorMatrixB_ColorCurveCm2 << ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3 << ColorCurve_Ch1_Ch2 << ColorShadow_Luma << ColorShadow_Tint1 << ColorShadow_Tint2;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessVisualizeHDRPS,TEXT("PostProcessVisualizeHDR"),TEXT("MainPS"),SF_Pixel);

FString LogToString(float LogValue)
{
	if(LogValue > 0)
	{
		return FString::Printf(TEXT("%.0f"), FMath::Pow(2.0f, LogValue));
	}
	else
	{
		return FString::Printf(TEXT("1/%.0f"), FMath::Pow(2.0f, -LogValue));
	}
}

void FRCPassPostProcessVisualizeHDR::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(PostProcessVisualizeHDR, DEC_SCENE_ITEMS);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = View.ViewRect;
	FIntPoint SrcSize = InputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	RHISetRenderTarget(DestRenderTarget.TargetableTexture, FTextureRHIRef());	
	Context.SetViewportAndCallRHI(DestRect);

	// set the state
	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FPostProcessVisualizeHDRPS> PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState BoundShaderState;

	SetGlobalBoundShaderState(BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetPS(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);


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

	Line = FString::Printf(TEXT("HDR Histogram (Logarithmic, max of RGB)"));
	Canvas.DrawShadowedString( X, Y += YStep, *Line, GetStatsFont(), FLinearColor(1, 1, 1));
	
	Y += 160;

	float MinX = 64 + 10;
	float MaxY = View.ViewRect.Max.Y - 64;
	float SizeX = View.ViewRect.Size().X - 64 * 2 - 20;

	for(uint32 i = 0; i <= 4; ++i)
	{
		int XAdd = (int)(i * SizeX / 4);
		float HistogramPosition =  i / 4.0f;
		float LogValue = FMath::Lerp(View.FinalPostProcessSettings.HistogramLogMin, View.FinalPostProcessSettings.HistogramLogMax, HistogramPosition);

		Line = FString::Printf(TEXT("%.2g"), LogValue);
		Canvas.DrawShadowedString( MinX + XAdd - 5, MaxY, *Line, GetStatsFont(), FLinearColor(1, 0.3f, 0.3f));
		Line = LogToString(LogValue);
		Canvas.DrawShadowedString( MinX + XAdd - 5, MaxY + YStep, *Line, GetStatsFont(), FLinearColor(0.3f, 0.3f, 1));
	}
	Y += 3 * YStep;
	
	Line = FString::Printf(TEXT("%g%% .. %g%%"), View.FinalPostProcessSettings.AutoExposureLowPercent, View.FinalPostProcessSettings.AutoExposureHighPercent);
	Canvas.DrawShadowedString( X, Y += YStep, TEXT("EyeAdaptationPercent Low/High:"), GetStatsFont(), FLinearColor(1, 1, 1));
	Canvas.DrawShadowedString( X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(1, 1, 1));

	Line = FString::Printf(TEXT("%g .. %g"), View.FinalPostProcessSettings.AutoExposureMinBrightness, View.FinalPostProcessSettings.AutoExposureMaxBrightness);
	Canvas.DrawShadowedString( X, Y += YStep, TEXT("EyeAdaptationBrightness Min/Max:"), GetStatsFont(), FLinearColor(1, 1, 1));
	Canvas.DrawShadowedString( X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(0.3f, 0.3f, 1));

	Line = FString::Printf(TEXT("%g / %g"), View.FinalPostProcessSettings.AutoExposureSpeedUp, View.FinalPostProcessSettings.AutoExposureSpeedDown);
	Canvas.DrawShadowedString( X, Y += YStep, TEXT("EyeAdaptionSpeed Up/Down:"), GetStatsFont(), FLinearColor(1, 1, 1));
	Canvas.DrawShadowedString( X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(1, 1, 1));

	Line = FString::Printf(TEXT("%g"), View.FinalPostProcessSettings.AutoExposureBias);
	Canvas.DrawShadowedString( X, Y += YStep, TEXT("Exposure Offset: "), GetStatsFont(), FLinearColor(1, 1, 1));
	Canvas.DrawShadowedString( X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(1, 0.3f, 0.3f));

	Line = FString::Printf(TEXT("%g .. %g (log2)"),
		View.FinalPostProcessSettings.HistogramLogMin, View.FinalPostProcessSettings.HistogramLogMax);
	Canvas.DrawShadowedString( X, Y += YStep, TEXT("HistogramLog Min/Max:"), GetStatsFont(), FLinearColor(1, 1, 1));
	Canvas.DrawShadowedString( X + ColumnWidth, Y, *Line, GetStatsFont(), FLinearColor(1, 0.3f, 0.3f));
	Line = FString::Printf(TEXT("%s .. %s (Value)"),
		*LogToString(View.FinalPostProcessSettings.HistogramLogMin), *LogToString(View.FinalPostProcessSettings.HistogramLogMax));
	Canvas.DrawShadowedString( X + ColumnWidth, Y+= YStep, *Line, GetStatsFont(), FLinearColor(0.3f, 0.3f, 1));

	Canvas.Flush();

	RHICopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessVisualizeHDR::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("VisualizeHDR");

	return Ret;
}