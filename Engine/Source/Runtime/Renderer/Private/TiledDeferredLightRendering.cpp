// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TiledDeferredLightRendering.cpp: Implementation of tiled deferred shading
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "LightRendering.h"
#include "ScreenRendering.h"

/** 
 * Maximum number of lights that can be handled by tiled deferred in a single compute shader pass.
 * If the scene has more visible lights than this, multiple tiled deferred passes will be needed which incurs the tile setup multiple times.
 * This is currently limited by the size of the light constant buffers. 
 */
static const int32 GMaxNumTiledDeferredLights = 1024;

/** 
 * Tile size for the deferred light compute shader.  Larger tiles have more threads in flight, but less accurate culling.
 * Tweaked for ~200 onscreen lights on a 7970.
 * Changing this requires touching the shader to cause a recompile.
 */
const int32 GDeferredLightTileSizeX = 16;
const int32 GDeferredLightTileSizeY = 16;

int32 GUseTiledDeferredShading = 1;
static FAutoConsoleVariableRef CVarUseTiledDeferredShading(
	TEXT("r.TiledDeferredShading"),
	GUseTiledDeferredShading,
	TEXT("Whether to use tiled deferred shading.  0 is off, 1 is on (default)"),
	ECVF_RenderThreadSafe
	);

// Tiled deferred has fixed overhead due to tile setup, but scales better than standard deferred
int32 GNumLightsBeforeUsingTiledDeferred = 80;
static FAutoConsoleVariableRef CVarNumLightsBeforeUsingTiledDeferred(
	TEXT("r.TiledDeferredShading.MinimumCount"),
	GNumLightsBeforeUsingTiledDeferred,
	TEXT("Number of applicable lights that must be on screen before switching to tiled deferred.\n")
	TEXT("0 means all lights that qualify (e.g. no shadows, ...) are rendered tiled deferred. Default: 80"),
	ECVF_RenderThreadSafe
	);

/** 
 * First constant buffer of light data for tiled deferred. 
 * Light data is split into two constant buffers to allow more lights per pass before hitting the d3d11 max constant buffer size of 4096 float4's
 */
BEGIN_UNIFORM_BUFFER_STRUCT(FTiledDeferredLightData,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,LightPositionAndInvRadius,[GMaxNumTiledDeferredLights])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,LightColorAndFalloffExponent,[GMaxNumTiledDeferredLights])
END_UNIFORM_BUFFER_STRUCT(FTiledDeferredLightData)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FTiledDeferredLightData,TEXT("TiledDeferred"));

/** Second constant buffer of light data for tiled deferred. */
BEGIN_UNIFORM_BUFFER_STRUCT(FTiledDeferredLightData2,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,LightDirectionAndSpotlightMaskAndMinRoughness,[GMaxNumTiledDeferredLights])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,SpotAnglesAndSourceRadiusAndSimpleLighting,[GMaxNumTiledDeferredLights])
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4,ShadowMapChannelMask,[GMaxNumTiledDeferredLights])
END_UNIFORM_BUFFER_STRUCT(FTiledDeferredLightData2)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FTiledDeferredLightData2,TEXT("TiledDeferred2"));

/** Compute shader used to implement tiled deferred lighting. */
template <bool bVisualizeLightCulling>
class FTiledDeferredLightingCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTiledDeferredLightingCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDeferredLightTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDeferredLightTileSizeY);
		OutEnvironment.SetDefine(TEXT("MAX_LIGHTS"), GMaxNumTiledDeferredLights);
		OutEnvironment.SetDefine(TEXT("VISUALIZE_LIGHT_CULLING"), (uint32)bVisualizeLightCulling);
		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FTiledDeferredLightingCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		InTexture.Bind(Initializer.ParameterMap, TEXT("InTexture"));
		OutTexture.Bind(Initializer.ParameterMap, TEXT("OutTexture"));
		NumLights.Bind(Initializer.ParameterMap, TEXT("NumLights"));
		ViewDimensions.Bind(Initializer.ParameterMap, TEXT("ViewDimensions"));
		PreIntegratedBRDF.Bind(Initializer.ParameterMap, TEXT("PreIntegratedBRDF"));
		PreIntegratedBRDFSampler.Bind(Initializer.ParameterMap, TEXT("PreIntegratedBRDFSampler"));
	}

	FTiledDeferredLightingCS()
	{
	}

	void SetParameters(
		const FSceneView& View, 
		int32 ViewIndex,
		int32 NumViews,
		const TArray<FSortedLightSceneInfo, SceneRenderingAllocator>& SortedLights, 
		int32 NumLightsToRenderInSortedLights, 
		const FSimpleLightArray& SimpleLights, 
		int32 StartIndex, 
		int32 NumThisPass,
		IPooledRenderTarget& InTextureValue,
		IPooledRenderTarget& OutTextureValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(ShaderRHI, View);
		DeferredParameters.Set(ShaderRHI, View);
		SetTextureParameter(ShaderRHI, InTexture, InTextureValue.GetRenderTargetItem().ShaderResourceTexture);
		OutTexture.SetTexture(ShaderRHI, 0, OutTextureValue.GetRenderTargetItem().UAV);
	
		SetShaderValue(ShaderRHI, ViewDimensions, View.ViewRect);

		SetTextureParameter(
			ShaderRHI,
			PreIntegratedBRDF,
			PreIntegratedBRDFSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GEngine->PreIntegratedSkinBRDFTexture->Resource->TextureRHI
			);

		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
		const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnRenderThread() != 0);

		FTiledDeferredLightData LightData;
		FTiledDeferredLightData2 LightData2;

		for (int32 LightIndex = 0; LightIndex < NumThisPass; LightIndex++)
		{
			if (StartIndex + LightIndex < NumLightsToRenderInSortedLights)
			{
				const FSortedLightSceneInfo& SortedLightInfo = SortedLights[StartIndex + LightIndex];
				const FLightSceneInfoCompact& LightSceneInfoCompact = SortedLightInfo.SceneInfo;
				const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

				FVector NormalizedLightDirection;
				FVector2D SpotAngles;
				float SourceRadius;
				float SourceLength;
				float MinRoughness;

				// Get the light parameters
				LightSceneInfo->Proxy->GetParameters(
					LightData.LightPositionAndInvRadius[LightIndex],
					LightData.LightColorAndFalloffExponent[LightIndex],
					NormalizedLightDirection,
					SpotAngles,
					SourceRadius,
					SourceLength,
					MinRoughness);

				if (LightSceneInfo->Proxy->IsInverseSquared())
				{
					LightData.LightColorAndFalloffExponent[LightIndex].W = 0;
				}

				{
					// SpotlightMaskAndMinRoughness, >0:Spotlight, MinRoughness = abs();
					float W = FMath::Max(0.0001f, MinRoughness) * ((LightSceneInfo->Proxy->GetLightType() == LightType_Spot) ? 1 : -1);

					LightData2.LightDirectionAndSpotlightMaskAndMinRoughness[LightIndex] = FVector4(NormalizedLightDirection, W);
				}

				LightData2.SpotAnglesAndSourceRadiusAndSimpleLighting[LightIndex] = FVector4(SpotAngles.X, SpotAngles.Y, SourceRadius, 0);

				int32 ShadowMapChannel = LightSceneInfo->Proxy->GetShadowMapChannel();

				if (!bAllowStaticLighting)
				{
					ShadowMapChannel = INDEX_NONE;
				}

				LightData2.ShadowMapChannelMask[LightIndex] = FVector4(
					ShadowMapChannel == 0 ? 1 : 0,
					ShadowMapChannel == 1 ? 1 : 0,
					ShadowMapChannel == 2 ? 1 : 0,
					ShadowMapChannel == 3 ? 1 : 0);
			}
			else
			{
				int32 SimpleLightIndex = StartIndex + LightIndex - NumLightsToRenderInSortedLights;
				const FSimpleLightEntry& SimpleLight = SimpleLights.InstanceData[SimpleLightIndex];
				const FSimpleLightPerViewEntry& SimpleLightPerViewData = SimpleLights.GetViewDependentData(SimpleLightIndex, ViewIndex, NumViews);
				LightData.LightPositionAndInvRadius[LightIndex] = FVector4(SimpleLightPerViewData.Position, 1.0f / SimpleLight.Radius);
				LightData.LightColorAndFalloffExponent[LightIndex] = FVector4(SimpleLight.Color, SimpleLight.Exponent);
				LightData2.LightDirectionAndSpotlightMaskAndMinRoughness[LightIndex] = FVector4(FVector(1, 0, 0), 0);
				LightData2.SpotAnglesAndSourceRadiusAndSimpleLighting[LightIndex] = FVector4(-2, 1, 0, 1);
				LightData2.ShadowMapChannelMask[LightIndex] = FVector4(0, 0, 0, 0);
			}
		}

		SetUniformBufferParameterImmediate(ShaderRHI, GetUniformBufferParameter<FTiledDeferredLightData>(), LightData);
		SetUniformBufferParameterImmediate(ShaderRHI, GetUniformBufferParameter<FTiledDeferredLightData2>(), LightData2);
		SetShaderValue(ShaderRHI, NumLights, NumThisPass);
	}

	void UnsetParameters()
	{
		OutTexture.UnsetUAV(GetComputeShader());
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << OutTexture;
		Ar << InTexture;
		Ar << NumLights;
		Ar << ViewDimensions;
		Ar << PreIntegratedBRDF;
		Ar << PreIntegratedBRDFSampler;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("TiledDeferredLightShaders");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("TiledDeferredLightingMain");
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter InTexture;
	FRWShaderParameter OutTexture;
	FShaderParameter NumLights;
	FShaderParameter ViewDimensions;
	FShaderResourceParameter PreIntegratedBRDF;
	FShaderResourceParameter PreIntegratedBRDFSampler;
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FTiledDeferredLightingCS<A> FTiledDeferredLightingCS##A; \
	IMPLEMENT_SHADER_TYPE2(FTiledDeferredLightingCS##A, SF_Compute);

VARIATION1(0)			VARIATION1(1)
#undef VARIATION1


bool FDeferredShadingSceneRenderer::CanUseTiledDeferred() const
{
	return GUseTiledDeferredShading != 0 && Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM5;
}

bool FDeferredShadingSceneRenderer::ShouldUseTiledDeferred(int32 NumUnshadowedLights, int32 NumSimpleLights) const
{
	// Only use tiled deferred if there are enough unshadowed lights to justify the fixed cost, 
	// Or if there are any simple lights, because those can only be rendered through tiled deferred
	return (NumUnshadowedLights >= GNumLightsBeforeUsingTiledDeferred || NumSimpleLights > 0);
}

template <bool bVisualizeLightCulling>
static void SetShaderTemplTiledLighting(
	const FSceneView& View,
	int32 ViewIndex,
	int32 NumViews,
	const TArray<FSortedLightSceneInfo, SceneRenderingAllocator>& SortedLights,
	int32 NumLightsToRenderInSortedLights,
	const FSimpleLightArray& SimpleLights,
	int32 StartIndex, 
	int32 NumThisPass,
	IPooledRenderTarget& InTexture,
	IPooledRenderTarget& OutTexture)
{
	TShaderMapRef<FTiledDeferredLightingCS<bVisualizeLightCulling> > ComputeShader(GetGlobalShaderMap());
	RHISetComputeShader(ComputeShader->GetComputeShader());

	ComputeShader->SetParameters(View, ViewIndex, NumViews, SortedLights, NumLightsToRenderInSortedLights, SimpleLights, StartIndex, NumThisPass, InTexture, OutTexture);

	uint32 GroupSizeX = (View.ViewRect.Size().X + GDeferredLightTileSizeX - 1) / GDeferredLightTileSizeX;
	uint32 GroupSizeY = (View.ViewRect.Size().Y + GDeferredLightTileSizeY - 1) / GDeferredLightTileSizeY;
	DispatchComputeShader(*ComputeShader, GroupSizeX, GroupSizeY, 1);

	ComputeShader->UnsetParameters();
}


void FDeferredShadingSceneRenderer::RenderTiledDeferredLighting(const TArray<FSortedLightSceneInfo, SceneRenderingAllocator>& SortedLights, int32 NumUnshadowedLights, const FSimpleLightArray& SimpleLights)
{
	check(GUseTiledDeferredShading);
	check(SortedLights.Num() >= NumUnshadowedLights);

	const int32 NumLightsToRender = NumUnshadowedLights + SimpleLights.InstanceData.Num();
	const int32 NumLightsToRenderInSortedLights = NumUnshadowedLights;

	if (NumLightsToRender > 0)
	{
		INC_DWORD_STAT_BY(STAT_NumLightsUsingTiledDeferred, NumLightsToRender);
		INC_DWORD_STAT_BY(STAT_NumLightsUsingSimpleTiledDeferred, SimpleLights.InstanceData.Num());
		SCOPE_CYCLE_COUNTER(STAT_DirectLightRenderingTime);

		RHISetRenderTarget(NULL, NULL);

		// Determine how many compute shader passes will be needed to process all the lights
		const int32 NumPassesNeeded = FMath::DivideAndRoundUp(NumLightsToRender, GMaxNumTiledDeferredLights);
		for (int32 PassIndex = 0; PassIndex < NumPassesNeeded; PassIndex++)
		{
			const int32 StartIndex = PassIndex * GMaxNumTiledDeferredLights;
			const int32 NumThisPass = (PassIndex == NumPassesNeeded - 1) ? NumLightsToRender - StartIndex : GMaxNumTiledDeferredLights;
			check(NumThisPass > 0);

			// One some hardware we can read and write from the same UAV with a 32 bit format. We don't do that yet.
			TRefCountPtr<IPooledRenderTarget> OutTexture;
			{
				GSceneRenderTargets.ResolveSceneColor(FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));

				FPooledRenderTargetDesc Desc = GSceneRenderTargets.GetSceneColor()->GetDesc();
				Desc.TargetableFlags |= TexCreate_UAV;

				GRenderTargetPool.FindFreeElement( Desc, OutTexture, TEXT("SceneColorTiled") );
			}

			{
				SCOPED_DRAW_EVENT(TiledDeferredLighting, DEC_SCENE_ITEMS);

				IPooledRenderTarget& InTexture = *GSceneRenderTargets.GetSceneColor();

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					const FViewInfo& View = Views[ViewIndex];

					if(View.Family->EngineShowFlags.VisualizeLightCulling)
					{
						SetShaderTemplTiledLighting<1>(View, ViewIndex, Views.Num(), SortedLights, NumLightsToRenderInSortedLights, SimpleLights, StartIndex, NumThisPass, InTexture, *OutTexture);
					}
					else
					{
						SetShaderTemplTiledLighting<0>(View, ViewIndex, Views.Num(), SortedLights, NumLightsToRenderInSortedLights, SimpleLights, StartIndex, NumThisPass, InTexture, *OutTexture);
					}
				}
			}

			// swap with the former SceneColor
			GSceneRenderTargets.SetSceneColor(OutTexture);
		}
	}
}
