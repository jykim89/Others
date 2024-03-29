// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnMaterial.cpp: Shader implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "MaterialShader.h"
#include "MaterialInstance.h"
#include "UObjectAnnotation.h"
#include "MaterialCompiler.h"
#include "TargetPlatform.h"

#if WITH_EDITOR
#include "UnrealEd.h"
#endif

#if WITH_EDITOR
const FMaterialsWithDirtyUsageFlags FMaterialsWithDirtyUsageFlags::DefaultAnnotation;

void FMaterialsWithDirtyUsageFlags::MarkUsageFlagDirty(EMaterialUsage UsageFlag)
{
	MaterialFlagsThatHaveChanged |= (1 << UsageFlag);
}

bool FMaterialsWithDirtyUsageFlags::IsUsageFlagDirty(EMaterialUsage UsageFlag)
{
	return (MaterialFlagsThatHaveChanged & (1 << UsageFlag)) != 0;
}

FUObjectAnnotationSparseBool GMaterialsThatNeedSamplerFixup;
FUObjectAnnotationSparseBool GMaterialsThatNeedPhysicalConversion;
FUObjectAnnotationSparse<FMaterialsWithDirtyUsageFlags,true> GMaterialsWithDirtyUsageFlags;
FUObjectAnnotationSparseBool GMaterialsThatNeedExpressionsFlipped;
FUObjectAnnotationSparseBool GMaterialsThatNeedCoordinateCheck;

#endif // #if WITH_EDITOR

FMaterialResource::FMaterialResource()
	: FMaterial()
	, Material(NULL)
	, MaterialInstance(NULL)
{
}

int32 FMaterialResource::CompileProperty(EMaterialProperty Property,EShaderFrequency InShaderFrequency,FMaterialCompiler* Compiler) const
{
	Compiler->SetMaterialProperty(Property, InShaderFrequency);
	int32 SelectionColorIndex = INDEX_NONE;
	if (InShaderFrequency == SF_Pixel)
	{
		SelectionColorIndex = Compiler->ComponentMask(Compiler->VectorParameter(NAME_SelectionColor,FLinearColor::Black),1,1,1,0);
	}
	
	//Compile the material instance if we have one.
	UMaterialInterface* MaterialInterface = MaterialInstance ? Cast<UMaterialInterface>(MaterialInstance) : Cast<UMaterialInterface>(Material);
	switch(Property)
	{
	case MP_EmissiveColor:
		if (SelectionColorIndex != INDEX_NONE)
		{
			return Compiler->Add(Compiler->ForceCast(MaterialInterface->CompileProperty(Compiler, MP_EmissiveColor),MCT_Float3),SelectionColorIndex);
		}
		else
		{
			return Compiler->ForceCast(MaterialInterface->CompileProperty(Compiler, MP_EmissiveColor),MCT_Float3);
		}
	case MP_Opacity: return MaterialInterface->CompileProperty(Compiler, MP_Opacity);
	case MP_OpacityMask: return MaterialInterface->CompileProperty(Compiler, MP_OpacityMask);
	case MP_DiffuseColor: 
		return Compiler->Mul(Compiler->ForceCast(MaterialInterface->CompileProperty(Compiler, MP_DiffuseColor),MCT_Float3),Compiler->Sub(Compiler->Constant(1.0f),SelectionColorIndex));
	case MP_SpecularColor: return MaterialInterface->CompileProperty(Compiler, MP_SpecularColor);
	case MP_BaseColor: 
		return Compiler->Mul(Compiler->ForceCast(MaterialInterface->CompileProperty(Compiler, MP_BaseColor),MCT_Float3),Compiler->Sub(Compiler->Constant(1.0f),SelectionColorIndex));
	case MP_Metallic: return MaterialInterface->CompileProperty(Compiler, MP_Metallic);
	case MP_Specular: return MaterialInterface->CompileProperty(Compiler, MP_Specular);
	case MP_Roughness: return MaterialInterface->CompileProperty(Compiler, MP_Roughness);
	case MP_Normal: return MaterialInterface->CompileProperty(Compiler, MP_Normal);
	case MP_WorldPositionOffset: return MaterialInterface->CompileProperty(Compiler, MP_WorldPositionOffset);
	case MP_WorldDisplacement: return MaterialInterface->CompileProperty(Compiler, MP_WorldDisplacement);
	case MP_TessellationMultiplier: return MaterialInterface->CompileProperty(Compiler, MP_TessellationMultiplier);
	case MP_SubsurfaceColor: return MaterialInterface->CompileProperty(Compiler, MP_SubsurfaceColor);
	case MP_AmbientOcclusion: return MaterialInterface->CompileProperty(Compiler, MP_AmbientOcclusion);
	case MP_Refraction: return MaterialInterface->CompileProperty(Compiler, MP_Refraction);
	default:

		if (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7)
		{
			return MaterialInterface->CompileProperty(Compiler, (EMaterialProperty)Property);
		}

		return INDEX_NONE;
	};
}

void FMaterialResource::GetShaderMapId(EShaderPlatform Platform, FMaterialShaderMapId& OutId) const
{
	FMaterial::GetShaderMapId(Platform, OutId);
	Material->GetReferencedFunctionIds(OutId.ReferencedFunctions);
	Material->GetReferencedParameterCollectionIds(OutId.ReferencedParameterCollections);
	if(MaterialInstance)
	{
		MaterialInstance->GetBasePropertyOverridesHash(OutId.BasePropertyOverridesHash);
	}
}

/**
 * A resource which represents the default instance of a UMaterial to the renderer.
 * Note that default parameter values are stored in the FMaterialUniformExpressionXxxParameter objects now.
 * This resource is only responsible for the selection color.
 */
class FDefaultMaterialInstance : public FMaterialRenderProxy
{
public:

	/**
	 * Called from the game thread to destroy the material instance on the rendering thread.
	 */
	void GameThread_Destroy()
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FDestroyDefaultMaterialInstanceCommand,
			FDefaultMaterialInstance*,Resource,this,
		{
			delete Resource;
		});
	}

	// FMaterialRenderProxy interface.
	virtual const class FMaterial* GetMaterial(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		const FMaterialResource* MaterialResource = Material->GetMaterialResource(InFeatureLevel);
		if (MaterialResource && MaterialResource->GetRenderingThreadShaderMap())
		{
			// Verify that compilation has been finalized, the rendering thread shouldn't be touching it otherwise
			checkSlow(MaterialResource->GetRenderingThreadShaderMap()->IsCompilationFinalized());
			// The shader map reference should have been NULL'ed if it did not compile successfully
			checkSlow(MaterialResource->GetRenderingThreadShaderMap()->CompiledSuccessfully());
			return MaterialResource;
		}

		// If we are the default material, must not try to fall back to the default material in an error state as that will be infinite recursion
		check(!Material->IsDefaultMaterial());

		return GetFallbackRenderProxy().GetMaterial(InFeatureLevel);
	}

	virtual FMaterial* GetMaterialNoFallback(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		checkSlow(IsInRenderingThread());
		return Material->GetMaterialResource(InFeatureLevel);
	}

	virtual bool GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		const FMaterialResource* MaterialResource = Material->GetMaterialResource(Context.Material.GetFeatureLevel());
		if(MaterialResource && MaterialResource->GetRenderingThreadShaderMap())
		{
			if(ParameterName == NAME_SelectionColor)
			{
				*OutValue = FLinearColor::Black;
				if( GIsEditor && Context.bShowSelection )
				{
					if( IsSelected() )
					{
						*OutValue = GEngine->GetSelectedMaterialColor() * GEngine->SelectionHighlightIntensity;
					}
					else if( IsHovered() )
					{
						*OutValue = GEngine->GetHoveredMaterialColor() * GEngine->HoverHighlightIntensity;
					}
				}
				return true;
			}
			return false;
		}
		else
		{
			return GetFallbackRenderProxy().GetVectorValue(ParameterName, OutValue, Context);
		}
	}
	virtual bool GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const
	{
		const FMaterialResource* MaterialResource = Material->GetMaterialResource(Context.Material.GetFeatureLevel());
		if(MaterialResource && MaterialResource->GetRenderingThreadShaderMap())
		{
			return false;
		}
		else
		{
			return GetFallbackRenderProxy().GetScalarValue(ParameterName, OutValue, Context);
		}
	}
	virtual bool GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		const FMaterialResource* MaterialResource = Material->GetMaterialResource(Context.Material.GetFeatureLevel());
		if(MaterialResource && MaterialResource->GetRenderingThreadShaderMap())
		{
			return false;
		}
		else
		{
			return GetFallbackRenderProxy().GetTextureValue(ParameterName,OutValue,Context);
		}
	}

	virtual float GetDistanceFieldPenumbraScale() const { return DistanceFieldPenumbraScale; }

	// FRenderResource interface.
	virtual FString GetFriendlyName() const { return Material->GetName(); }

	// Constructor.
	FDefaultMaterialInstance(UMaterial* InMaterial,bool bInSelected,bool bInHovered):
		FMaterialRenderProxy(bInSelected, bInHovered),
		Material(InMaterial),
		DistanceFieldPenumbraScale(1.0f)
	{}

	/** Called from the game thread to update DistanceFieldPenumbraScale. */
	void GameThread_UpdateDistanceFieldPenumbraScale(float NewDistanceFieldPenumbraScale)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateDistanceFieldPenumbraScaleCommand,
			float*,DistanceFieldPenumbraScale,&DistanceFieldPenumbraScale,
			float,NewDistanceFieldPenumbraScale,NewDistanceFieldPenumbraScale,
		{
			*DistanceFieldPenumbraScale = NewDistanceFieldPenumbraScale;
		});
	}

private:

	/** Get the fallback material. */
	FMaterialRenderProxy& GetFallbackRenderProxy() const
	{
		return *(UMaterial::GetDefaultMaterial(Material->MaterialDomain)->GetRenderProxy(IsSelected(),IsHovered()));
	}

	UMaterial* Material;
	float DistanceFieldPenumbraScale;
};

#if WITH_EDITOR
static bool GAllowCompilationInPostLoad=true;
#else
#define GAllowCompilationInPostLoad true
#endif

void UMaterial::ForceNoCompilationInPostLoad(bool bForceNoCompilation)
{
#if WITH_EDITOR
	GAllowCompilationInPostLoad = !bForceNoCompilation;
#endif
}

static UMaterialFunction* GPowerToRoughnessMaterialFunction = NULL;
static UMaterialFunction* GConvertFromDiffSpecMaterialFunction = NULL;

static UMaterial* GDefaultMaterials[MD_MAX] = {0};

static const TCHAR* GDefaultMaterialNames[MD_MAX] =
{
	TEXT("engine-ini:/Script/Engine.Engine.DefaultMaterialName"),
	TEXT("engine-ini:/Script/Engine.Engine.DefaultDeferredDecalMaterialName"),
	TEXT("engine-ini:/Script/Engine.Engine.DefaultLightFunctionMaterialName"),
	TEXT("engine-ini:/Script/Engine.Engine.DefaultPostProcessMaterialName")
};

void UMaterialInterface::InitDefaultMaterials()
{
	// Note that this function will (in fact must!) be called recursively. This
	// guarantees that the default materials will have been loaded and pointers
	// set before any other material interface has been instantiated -- even
	// one of the default materials! It is actually possible to assert that
	// these materials exist in the UMaterial or UMaterialInstance constructor.
	// 
	// The check for initialization is purely an optimization as initializing
	// the default materials is only done very early in the boot process.
	static bool bInitialized = false;
	if (!bInitialized)
	{		
		check(IsInGameThread());
		
#if WITH_EDITOR
		GPowerToRoughnessMaterialFunction = LoadObject< UMaterialFunction >( NULL, TEXT("/Engine/Functions/Engine_MaterialFunctions01/Shading/PowerToRoughness.PowerToRoughness"), NULL, LOAD_None, NULL );
		checkf( GPowerToRoughnessMaterialFunction, TEXT("Cannot load PowerToRoughness") );

		GConvertFromDiffSpecMaterialFunction = LoadObject< UMaterialFunction >( NULL, TEXT("/Engine/Functions/Engine_MaterialFunctions01/Shading/ConvertFromDiffSpec.ConvertFromDiffSpec"), NULL, LOAD_None, NULL );
		checkf( GConvertFromDiffSpecMaterialFunction, TEXT("Cannot load ConvertFromDiffSpec") );
#endif

		for (int32 Domain = 0; Domain < MD_MAX; ++Domain)
		{
			if (GDefaultMaterials[Domain] == NULL)
			{
				GDefaultMaterials[Domain] = FindObject<UMaterial>(NULL,GDefaultMaterialNames[Domain]);
				if (GDefaultMaterials[Domain] == NULL)
				{
					GDefaultMaterials[Domain] = LoadObject<UMaterial>(NULL,GDefaultMaterialNames[Domain],NULL,LOAD_None,NULL);
					checkf(GDefaultMaterials[Domain] != NULL, TEXT("Cannot load default material '%s'"), GDefaultMaterialNames[Domain]);
				}
			}
		}
		bInitialized = true;
	}
}

void UMaterialInterface::PostLoadDefaultMaterials()
{
	// Here we prevent this function from being called recursively. Mostly this
	// is an optimization and guarantees that default materials are post loaded
	// in the order material domains are defined. Surface -> deferred decal -> etc.
	static bool bPostLoaded = false;
	if (!bPostLoaded)
	{
		check(IsInGameThread());
		bPostLoaded = true;

#if WITH_EDITOR
		GPowerToRoughnessMaterialFunction->ConditionalPostLoad();
		GConvertFromDiffSpecMaterialFunction->ConditionalPostLoad();
#endif

		for (int32 Domain = 0; Domain < MD_MAX; ++Domain)
		{
			UMaterial* Material = GDefaultMaterials[Domain];
			check(Material);
			Material->ConditionalPostLoad();
		}
	}
}

void UMaterialInterface::AssertDefaultMaterialsExist()
{
	for (int32 Domain = 0; Domain < MD_MAX; ++Domain)
	{
		check(GDefaultMaterials[Domain] != NULL);
	}
}

void UMaterialInterface::AssertDefaultMaterialsPostLoaded()
{
	for (int32 Domain = 0; Domain < MD_MAX; ++Domain)
	{
		check(GDefaultMaterials[Domain] != NULL);
		check(!GDefaultMaterials[Domain]->HasAnyFlags(RF_NeedPostLoad));
	}
}

void SerializeInlineShaderMaps(const TMap<const ITargetPlatform*,TArray<FMaterialResource*>>& PlatformMaterialResourcesToSave, FArchive& Ar, FMaterialResource* OutMaterialResourcesLoaded[][ERHIFeatureLevel::Num])
{
	if (Ar.IsSaving())
	{
		int32 NumResourcesToSave = 0;
		const TArray<FMaterialResource*> *MaterialResourcesToSavePtr = NULL;
		if (Ar.IsCooking())
		{
			MaterialResourcesToSavePtr = PlatformMaterialResourcesToSave.Find( Ar.CookingTarget() );
			check( MaterialResourcesToSavePtr != NULL || (Ar.GetLinker()==NULL) );
			if (MaterialResourcesToSavePtr!= NULL )
			{
				NumResourcesToSave = MaterialResourcesToSavePtr->Num();
			}
		}

		Ar << NumResourcesToSave;

		if ( MaterialResourcesToSavePtr )
		{
			const TArray<FMaterialResource*> &MaterialResourcesToSave = *MaterialResourcesToSavePtr;
			for (int32 ResourceIndex = 0; ResourceIndex < NumResourcesToSave; ResourceIndex++)
			{
				MaterialResourcesToSave[ResourceIndex]->SerializeInlineShaderMap(Ar);
			}
		}
		
	}
	else if (Ar.IsLoading())
	{
		int32 NumLoadedResources = 0;
		Ar << NumLoadedResources;

		TArray<FMaterialResource> LoadedResources;
		LoadedResources.Empty(NumLoadedResources);

		for (int32 ResourceIndex = 0; ResourceIndex < NumLoadedResources; ResourceIndex++)
		{
			FMaterialResource LoadedResource;
			LoadedResource.SerializeInlineShaderMap(Ar);
			LoadedResources.Add(LoadedResource);
		}

		// Apply in 2 passes - first pass is for shader maps without a specified quality level
		// Second pass is where shader maps with a specified quality level override
		for (int32 PassIndex = 0; PassIndex < 2; PassIndex++)
		{
			for (int32 ResourceIndex = 0; ResourceIndex < LoadedResources.Num(); ResourceIndex++)
			{
				FMaterialResource& LoadedResource = LoadedResources[ResourceIndex];
				FMaterialShaderMap* LoadedShaderMap = LoadedResource.GetGameThreadShaderMap();

				if (LoadedShaderMap && LoadedShaderMap->GetShaderPlatform() == GRHIShaderPlatform)
				{
					EMaterialQualityLevel::Type LoadedQualityLevel = LoadedShaderMap->GetShaderMapId().QualityLevel;
					ERHIFeatureLevel::Type LoadedFeatureLevel = LoadedShaderMap->GetShaderMapId().FeatureLevel;

					for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
					{
						// Apply to all resources in the first pass if the shader map does not have a quality level specified
						if ((PassIndex == 0 && LoadedQualityLevel == EMaterialQualityLevel::Num)
							// Apply to just the corresponding resource in the second pass if the shader map has a quality level specified
							|| (PassIndex == 1 && QualityLevelIndex == LoadedQualityLevel))
						{
							if (!OutMaterialResourcesLoaded[QualityLevelIndex][LoadedFeatureLevel])
							{
								OutMaterialResourcesLoaded[QualityLevelIndex][LoadedFeatureLevel] = new FMaterialResource();
							}

							OutMaterialResourcesLoaded[QualityLevelIndex][LoadedFeatureLevel]->SetInlineShaderMap(LoadedShaderMap);
						}
					}
				}
			}
		}
	}
}

UMaterial* UMaterial::GetDefaultMaterial(EMaterialDomain Domain)
{
	InitDefaultMaterials();
	check(Domain >= MD_Surface && Domain < MD_MAX);
	check(GDefaultMaterials[Domain] != NULL);
	return GDefaultMaterials[Domain];
}

bool UMaterial::IsDefaultMaterial() const
{
	bool bDefault = false;
	for (int32 Domain = MD_Surface; !bDefault && Domain < MD_MAX; ++Domain)
	{
		bDefault = (this == GDefaultMaterials[Domain]);
	}
	return bDefault;
}

UMaterial::UMaterial(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	BlendMode = BLEND_Opaque;
	TranslucencyLightingMode = TLM_VolumetricNonDirectional;
	TranslucencyDirectionalLightingIntensity = 1.0f;
	TranslucentShadowDensityScale = 0.5f;
	TranslucentSelfShadowDensityScale = 2.0f;
	TranslucentSelfShadowSecondDensityScale = 10.0f;
	TranslucentSelfShadowSecondOpacity = 0.0f;
	TranslucentBackscatteringExponent = 30.0f;
	TranslucentMultipleScatteringExtinction = FLinearColor(1.0f, 0.833f, 0.588f, 1.0f);
	TranslucentShadowStartOffset = 100.0f;

	DiffuseColor.Constant = FColor(128,128,128);
	SpecularColor.Constant = FColor(128,128,128);
	BaseColor.Constant = FColor(128,128,128);	
	Metallic.Constant = 0.0f;
	Specular.Constant = 0.5f;
	Roughness.Constant = 0.5f;
	
	Opacity.Constant = 1.0f;
	OpacityMask.Constant = 1.0f;
	OpacityMaskClipValue = 0.3333f;
	FresnelBaseReflectFraction_DEPRECATED = 0.04f;
	bPhysicallyBasedInputs_DEPRECATED = true;
	bUsedWithStaticLighting = false;
	D3D11TessellationMode = MTM_NoTessellation;
	bEnableCrackFreeDisplacement = false;
	bEnableAdaptiveTessellation = true;
	bEnableSeparateTranslucency = true;
	bEnableResponsiveAA = false;
	bTangentSpaceNormal = true;
	bUseLightmapDirectionality = true;

	bUseMaterialAttributes = false;
	bUseTranslucencyVertexFog = true;
	BlendableLocation = BL_AfterTonemapping;
	BlendablePriority = 0;

	bUseEmissiveForDynamicAreaLighting = false;
	RefractionDepthBias = 0.0f;
	MaterialDecalResponse = MDR_ColorNormalRoughness;

	bAllowDevelopmentShaderCompile = true;
	bIsMaterialEditorStatsMaterial = false;

#if WITH_EDITORONLY_DATA
	MaterialGraph = NULL;
#endif //WITH_EDITORONLY_DATA
}

void UMaterial::PreSave()
{
	Super::PreSave();
#if WITH_EDITOR
	GMaterialsWithDirtyUsageFlags.RemoveAnnotation(this);
#endif
}

void UMaterial::PostInitProperties()
{
	Super::PostInitProperties();
	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		DefaultMaterialInstances[0] = new FDefaultMaterialInstance(this,false,false);
		if(GIsEditor)
		{
			DefaultMaterialInstances[1] = new FDefaultMaterialInstance(this,true,false);
			DefaultMaterialInstances[2] = new FDefaultMaterialInstance(this,false,true);
		}
	}

	// Initialize StateId to something unique, in case this is a new material
	FPlatformMisc::CreateGuid(StateId);

	UpdateResourceAllocations();
}

FMaterialResource* UMaterial::AllocateResource()
{
	return new FMaterialResource();
}

void UMaterial::GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel, bool bAllQualityLevels) const
{
	OutTextures.Empty();

	if (QualityLevel == EMaterialQualityLevel::Num)
	{
		QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	}

	if (!FPlatformProperties::IsServerOnly())
	{
		for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
		{
			const FMaterialResource* CurrentResource = MaterialResources[QualityLevelIndex][GRHIFeatureLevel];

			if (QualityLevelIndex == QualityLevel || bAllQualityLevels)
			{
				const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2] =
				{
					&CurrentResource->GetUniform2DTextureExpressions(),
					&CurrentResource->GetUniformCubeTextureExpressions()
				};
				for(int32 TypeIndex = 0;TypeIndex < ARRAY_COUNT(ExpressionsByType);TypeIndex++)
				{
					// Iterate over each of the material's texture expressions.
					for (FMaterialUniformExpressionTexture* Expression : *ExpressionsByType[TypeIndex])
					{
						const bool bAllowOverride = false;
						UTexture* Texture = NULL;
						Expression->GetGameThreadTextureValue(this,*CurrentResource,Texture,bAllowOverride);

						if (Texture)
						{
							OutTextures.Add(Texture);
						}
					}
				}
			}
		}
	}
}

void UMaterial::OverrideTexture( const UTexture* InTextureToOverride, UTexture* OverrideTexture )
{
#if WITH_EDITOR
	bool bShouldRecacheMaterialExpressions = false;
	const bool bES2Preview = false;
	ERHIFeatureLevel::Type FeatureLevelsToUpdate[2] = { GRHIFeatureLevel, ERHIFeatureLevel::ES2 };
	int32 NumFeatureLevelsToUpdate = bES2Preview ? 2 : 1;
	
	for (int32 i = 0; i < NumFeatureLevelsToUpdate; ++i)
	{
		FMaterialResource* Resource = GetMaterialResource(FeatureLevelsToUpdate[i]);
		// Iterate over both the 2D textures and cube texture expressions.
		const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2] =
		{
			&Resource->GetUniform2DTextureExpressions(),
			&Resource->GetUniformCubeTextureExpressions()
		};
		for(int32 TypeIndex = 0;TypeIndex < ARRAY_COUNT(ExpressionsByType);TypeIndex++)
		{
			// Iterate over each of the material's texture expressions.
			for (FMaterialUniformExpressionTexture* Expression : *ExpressionsByType[TypeIndex])
			{
				// Evaluate the expression in terms of this material instance.
				const bool bAllowOverride = false;
				UTexture* Texture = NULL;
				Expression->GetGameThreadTextureValue(this,*Resource,Texture,bAllowOverride);

				if( Texture != NULL && Texture == InTextureToOverride )
				{
					// Override this texture!
					Expression->SetTransientOverrideTextureValue( OverrideTexture );
					bShouldRecacheMaterialExpressions = true;
				}
			}
		}
	}

	if (bShouldRecacheMaterialExpressions)
	{
		RecacheUniformExpressions();
		RecacheMaterialInstanceUniformExpressions(this);
	}
#endif // #if WITH_EDITOR
}


void UMaterial::RecacheUniformExpressions() const
{
	// Ensure that default material is available before caching expressions.
	UMaterial::GetDefaultMaterial(MD_Surface);

	// Only cache the unselected + unhovered material instance. Selection color
	// can change at runtime and would invalidate the parameter cache.
	if (DefaultMaterialInstances[0])
	{
		DefaultMaterialInstances[0]->CacheUniformExpressions_GameThread();
	}
}

bool UMaterial::GetUsageByFlag(EMaterialUsage Usage) const
{
	bool UsageValue = false;
	switch(Usage)
	{
		case MATUSAGE_SkeletalMesh: UsageValue = bUsedWithSkeletalMesh; break;
		case MATUSAGE_Landscape: UsageValue = bUsedWithLandscape; break;
		case MATUSAGE_ParticleSprites: UsageValue = bUsedWithParticleSprites; break;
		case MATUSAGE_BeamTrails: UsageValue = bUsedWithBeamTrails; break;
		case MATUSAGE_MeshParticles: UsageValue = bUsedWithMeshParticles; break;
		case MATUSAGE_StaticLighting: UsageValue = bUsedWithStaticLighting; break;
		case MATUSAGE_MorphTargets: UsageValue = bUsedWithMorphTargets; break;
		case MATUSAGE_SplineMesh: UsageValue = bUsedWithSplineMeshes; break;
		case MATUSAGE_InstancedStaticMeshes: UsageValue = bUsedWithInstancedStaticMeshes; break;
		case MATUSAGE_Clothing: UsageValue = bUsedWithClothing; break;
		default: UE_LOG(LogMaterial, Fatal,TEXT("Unknown material usage: %u"), (int32)Usage);
	};
	return UsageValue;
}

bool UMaterial::IsUsageFlagDirty(EMaterialUsage Usage)
{
#if WITH_EDITOR
	return GMaterialsWithDirtyUsageFlags.GetAnnotation(this).IsUsageFlagDirty(Usage);
#endif
	return false;
}

bool UMaterial::IsCompilingOrHadCompileError()
{
	FMaterialResource* Res = GetMaterialResource(GRHIFeatureLevel);

	// should never be the case
	check(Res);

	return Res->GetGameThreadShaderMap() == NULL;
}

void UMaterial::MarkUsageFlagDirty(EMaterialUsage Usage, bool CurrentValue, bool NewValue)
{
#if WITH_EDITOR
	if(CurrentValue != NewValue)
	{
		FMaterialsWithDirtyUsageFlags Annotation = GMaterialsWithDirtyUsageFlags.GetAnnotation(this);
		Annotation.MarkUsageFlagDirty(Usage);
		GMaterialsWithDirtyUsageFlags.AddAnnotation(this,Annotation);
	}
#endif
}

void UMaterial::SetUsageByFlag(EMaterialUsage Usage, bool NewValue)
{
	bool bOldValue = GetUsageByFlag(Usage);
	MarkUsageFlagDirty(Usage, bOldValue, NewValue);

	switch(Usage)
	{
		case MATUSAGE_SkeletalMesh:
		{
			bUsedWithSkeletalMesh = NewValue; break;
		}
		case MATUSAGE_Landscape: 
		{
			bUsedWithLandscape = NewValue; break;
		}
		case MATUSAGE_ParticleSprites:
		{
			bUsedWithParticleSprites = NewValue; break;
		}
		case MATUSAGE_BeamTrails:
		{
			bUsedWithBeamTrails = NewValue; break;
		}
		case MATUSAGE_MeshParticles:
		{	
			bUsedWithMeshParticles = NewValue; break;
		}
		case MATUSAGE_StaticLighting:
		{
			bUsedWithStaticLighting = NewValue; break;
		}
		case MATUSAGE_MorphTargets:
		{
			bUsedWithMorphTargets = NewValue; break;
		}
		case MATUSAGE_SplineMesh:
		{
			bUsedWithSplineMeshes = NewValue; break;
		}
		case MATUSAGE_InstancedStaticMeshes:
		{
			bUsedWithInstancedStaticMeshes = NewValue; break;
		}
		case MATUSAGE_Clothing:
		{
			bUsedWithClothing = NewValue; break;
		}
		default: UE_LOG(LogMaterial, Fatal,TEXT("Unknown material usage: %u"), (int32)Usage);
	};
#if WITH_EDITOR
	FEditorSupportDelegates::MaterialUsageFlagsChanged.Broadcast(this, Usage);
#endif
}


FString UMaterial::GetUsageName(EMaterialUsage Usage) const
{
	FString UsageName = TEXT("");
	switch(Usage)
	{
		case MATUSAGE_SkeletalMesh: UsageName = TEXT("bUsedWithSkeletalMesh"); break;
		case MATUSAGE_Landscape: UsageName = TEXT("bUsedWithLandscape"); break;
		case MATUSAGE_ParticleSprites: UsageName = TEXT("bUsedWithParticleSprites"); break;
		case MATUSAGE_BeamTrails: UsageName = TEXT("bUsedWithBeamTrails"); break;
		case MATUSAGE_MeshParticles: UsageName = TEXT("bUsedWithMeshParticles"); break;
		case MATUSAGE_StaticLighting: UsageName = TEXT("bUsedWithStaticLighting"); break;
		case MATUSAGE_MorphTargets: UsageName = TEXT("bUsedWithMorphTargets"); break;
		case MATUSAGE_SplineMesh: UsageName = TEXT("bUsedWithSplineMeshes"); break;
		case MATUSAGE_InstancedStaticMeshes: UsageName = TEXT("bUsedWithInstancedStaticMeshes"); break;
		case MATUSAGE_Clothing: UsageName = TEXT("bUsedWithClothing"); break;
		default: UE_LOG(LogMaterial, Fatal,TEXT("Unknown material usage: %u"), (int32)Usage);
	};
	return UsageName;
}


bool UMaterial::CheckMaterialUsage(EMaterialUsage Usage, const bool bSkipPrim)
{
	check(IsInGameThread());
	bool bNeedsRecompile = false;
	return SetMaterialUsage(bNeedsRecompile, Usage, bSkipPrim);
}

bool UMaterial::CheckMaterialUsage_Concurrent(EMaterialUsage Usage, const bool bSkipPrim) const 
{
	bool bUsageSetSuccessfully = false;
	if (NeedsSetMaterialUsage_Concurrent(bUsageSetSuccessfully, Usage))
	{
		if (IsInGameThread())
		{
			bUsageSetSuccessfully = const_cast<UMaterial*>(this)->CheckMaterialUsage(Usage, bSkipPrim);
		}	
		else
		{
			struct FCallSMU
			{
				UMaterial* Material;
				EMaterialUsage Usage;
				bool bSkipPrim;
				bool& bUsageSetSuccessfully;
				FScopedEvent& Event;

				FCallSMU(UMaterial* InMaterial, EMaterialUsage InUsage, bool bInSkipPrim, bool& bInUsageSetSuccessfully, FScopedEvent& InEvent)
					: Material(InMaterial)
					, Usage(InUsage)
					, bSkipPrim(bInSkipPrim)
					, bUsageSetSuccessfully(bInUsageSetSuccessfully)
					, Event(InEvent)
				{
				}

				void Task()
				{
					bUsageSetSuccessfully = Material->CheckMaterialUsage(Usage, bSkipPrim);
					Event.Trigger();
				}
			};
			UE_LOG(LogMaterial, Warning, TEXT("Has to pass SMU back to game thread. This stalls the tasks graph, but since it is editor only, is not such a big deal."));

			FScopedEvent Event;
			FCallSMU CallSMU(const_cast<UMaterial*>(this), Usage, bSkipPrim, bUsageSetSuccessfully, Event);
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateRaw(&CallSMU, &FCallSMU::Task)
				, TEXT("CheckMaterialUsage")
				, NULL
				, ENamedThreads::GameThread_Local
				);
		}
	}
	return bUsageSetSuccessfully;
}

/** Returns true if the given usage flag controls support for a primitive type. */
static bool IsPrimitiveTypeUsageFlag(EMaterialUsage Usage)
{
	return Usage == MATUSAGE_SkeletalMesh
		|| Usage == MATUSAGE_ParticleSprites
		|| Usage == MATUSAGE_BeamTrails
		|| Usage == MATUSAGE_MeshParticles
		|| Usage == MATUSAGE_MorphTargets
		|| Usage == MATUSAGE_SplineMesh
		|| Usage == MATUSAGE_InstancedStaticMeshes
		|| Usage == MATUSAGE_Clothing;
}

bool UMaterial::NeedsSetMaterialUsage_Concurrent(bool &bOutHasUsage, EMaterialUsage Usage) const
{
	bOutHasUsage = true;
	// Material usage is only relevant for surface materials.
	if (MaterialDomain != MD_Surface)
	{
		bOutHasUsage = false;
		return false;
	}
	// Check that the material has been flagged for use with the given usage flag.
	if(!GetUsageByFlag(Usage) && !bUsedAsSpecialEngineMaterial)
	{
		// This will be overwritten later by SetMaterialUsage, since we are saying that it needs to be called with the return value
		bOutHasUsage = false;
		return true;
	}
	return false;
}


bool UMaterial::SetMaterialUsage(bool &bNeedsRecompile, EMaterialUsage Usage, const bool bSkipPrim)
{
	bNeedsRecompile = false;

	// Material usage is only relevant for surface materials.
	if (MaterialDomain != MD_Surface)
	{
		return false;
	}

	// Check that the material has been flagged for use with the given usage flag.
	if(!GetUsageByFlag(Usage) && !bUsedAsSpecialEngineMaterial)
	{
		// For materials which do not have their bUsedWith____ correctly set the DefaultMaterial<type> should be used in game
		// Leaving this GIsEditor ensures that in game on PC will not look different than on the Consoles as we will not be compiling shaders on the fly
		if( GIsEditor && !FApp::IsGame() )
		{
			check(IsInGameThread());
			UE_LOG(LogMaterial, Warning, TEXT("Material %s needed to have new flag set %s !"), *GetPathName(), *GetUsageName(Usage));

			// Open a material update context so this material can be modified safely.
			FMaterialUpdateContext UpdateContext(
				// We need to sync with the rendering thread but don't reregister components
				// because SetMaterialUsage may be called during registration!
				FMaterialUpdateContext::EOptions::SyncWithRenderingThread
				);
			UpdateContext.AddMaterial(this);

			// If the flag is missing in the editor, set it, and recompile shaders.
			SetUsageByFlag(Usage, true);
			bNeedsRecompile = true;

			// Compile and force the Id to be regenerated, since we changed the material in a way that changes compilation
			CacheResourceShadersForRendering(true);

			// Mark the package dirty so that hopefully it will be saved with the new usage flag.
			MarkPackageDirty();
		}
		else
		{
			uint32 UsageFlagBit = (1 << (uint32)Usage);
			if ((UsageFlagWarnings  & UsageFlagBit) == 0)
			{
				UE_LOG(LogMaterial, Warning, TEXT("Material %s missing %s=True! Default Material will be used in game."), *GetPathName(), *GetUsageName(Usage));
				UsageFlagWarnings |= UsageFlagBit;
			}

			// Return failure if the flag is missing in game, since compiling shaders in game is not supported on some platforms.
			return false;
		}
	}
	return true;
}

/**
* @param	OutParameterNames		Storage array for the parameter names we are returning.
* @param	OutParameterIds			Storage array for the parameter id's we are returning.
*
* @return	Returns a array of vector parameter names used in this material.
*/
template<typename ExpressionType>
void UMaterial::GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds)
{
	for(int32 ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
	{
		ExpressionType* ParameterExpression =
			Cast<ExpressionType>(Expressions[ExpressionIndex]);

		if(ParameterExpression)
		{
			ParameterExpression->GetAllParameterNames(OutParameterNames, OutParameterIds);
		}
	}

	check(OutParameterNames.Num() == OutParameterIds.Num());
}

void UMaterial::GetAllVectorParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds)
{
	OutParameterNames.Empty();
	OutParameterIds.Empty();
	GetAllParameterNames<UMaterialExpressionVectorParameter>(OutParameterNames, OutParameterIds);
}
void UMaterial::GetAllScalarParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds)
{
	OutParameterNames.Empty();
	OutParameterIds.Empty();
	GetAllParameterNames<UMaterialExpressionScalarParameter>(OutParameterNames, OutParameterIds);
}
void UMaterial::GetAllTextureParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds)
{
	OutParameterNames.Empty();
	OutParameterIds.Empty();
	GetAllParameterNames<UMaterialExpressionTextureSampleParameter>(OutParameterNames, OutParameterIds);
}

void UMaterial::GetAllFontParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds)
{
	OutParameterNames.Empty();
	OutParameterIds.Empty();
	GetAllParameterNames<UMaterialExpressionFontSampleParameter>(OutParameterNames, OutParameterIds);
}

void UMaterial::GetAllStaticSwitchParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds)
{
	OutParameterNames.Empty();
	OutParameterIds.Empty();
	GetAllParameterNames<UMaterialExpressionStaticBoolParameter>(OutParameterNames, OutParameterIds);
}

void UMaterial::GetAllStaticComponentMaskParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds)
{
	OutParameterNames.Empty();
	OutParameterIds.Empty();
	GetAllParameterNames<UMaterialExpressionStaticComponentMaskParameter>(OutParameterNames, OutParameterIds);
}

void UMaterial::GetAllTerrainLayerWeightParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds)
{
	OutParameterNames.Empty();
	OutParameterIds.Empty();
	GetAllParameterNames<UMaterialExpressionLandscapeLayerWeight>(OutParameterNames, OutParameterIds);
	GetAllParameterNames<UMaterialExpressionLandscapeLayerSwitch>(OutParameterNames, OutParameterIds);
	GetAllParameterNames<UMaterialExpressionLandscapeLayerBlend>(OutParameterNames, OutParameterIds);
	GetAllParameterNames<UMaterialExpressionLandscapeVisibilityMask>(OutParameterNames, OutParameterIds);
}

extern FPostProcessMaterialNode* IteratePostProcessMaterialNodes(const FFinalPostProcessSettings& Dest, const UMaterial* Material, FBlendableEntry*& Iterator);

void UMaterialInterface::OverrideBlendableSettings(class FSceneView& View, float Weight) const
{
	check(Weight >= 0.0f && Weight <= 1.0f);

	FFinalPostProcessSettings& Dest = View.FinalPostProcessSettings;

	const UMaterial* Material = GetMaterial();

	//	should we use UMaterial::GetDefaultMaterial(Domain) instead of skipping the material

	if(!Material || Material->MaterialDomain != MD_PostProcess || !View.State)
	{
		return;
	}

	FBlendableEntry* Iterator = 0;

	FPostProcessMaterialNode* PostProcessMaterialNode = IteratePostProcessMaterialNodes(Dest, Material, Iterator);

	if(PostProcessMaterialNode)
	{
		// no blend needed
		return;
	}
	else
	{
		UMaterialInstanceDynamic* MID = View.State->GetReusableMID((UMaterialInterface*)Material);//, (UMaterialInterface*)this);

		if(MID)
		{
			MID->K2_CopyMaterialInstanceParameters((UMaterialInterface*)this);

			FPostProcessMaterialNode NewNode(MID, Material->BlendableLocation, Material->BlendablePriority);

			// a material already exists, blend with existing ones
			Dest.BlendableManager.PushBlendableData(Weight, NewNode);
		}
	}
}

UMaterial* UMaterial::GetMaterial()
{
	return this;
}

const UMaterial* UMaterial::GetMaterial() const
{
	return this;
}

const UMaterial* UMaterial::GetMaterial_Concurrent(TMicRecursionGuard&) const
{
	return this;
}

bool UMaterial::GetGroupName(FName ParameterName, FName& OutDesc) const
{
	bool bSuccess = false;
	for(int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* Expression = Expressions[ExpressionIndex];
		// Parameter is a basic Expression Parameter
		if(Expression->IsA(UMaterialExpressionParameter::StaticClass()))
		{
			UMaterialExpressionParameter* Parameter = CastChecked<UMaterialExpressionParameter>(Expression);
			if(Parameter && Parameter->ParameterName == ParameterName)
			{
				OutDesc = Parameter->Group;
				bSuccess = true;
				break;
			}
		}
		// Parameter is a Texture Sample Parameter
		else if(Expression->IsA(UMaterialExpressionTextureSampleParameter::StaticClass()))
		{
			UMaterialExpressionTextureSampleParameter* Parameter = CastChecked<UMaterialExpressionTextureSampleParameter>(Expression);
			if(Parameter && Parameter->ParameterName == ParameterName)
			{
				OutDesc = Parameter->Group;
				bSuccess = true;
				break;
			}
		}
		// Parameter is a Font Sample Parameter
		else if(Expression->IsA(UMaterialExpressionFontSampleParameter::StaticClass()))
		{
			UMaterialExpressionFontSampleParameter* Parameter = CastChecked<UMaterialExpressionFontSampleParameter>(Expression);
			if(Parameter && Parameter->ParameterName == ParameterName)
			{
				OutDesc = Parameter->Group;
				bSuccess = true;
				break;
			}
		}
	}
	return bSuccess;
}

bool UMaterial::GetParameterDesc(FName ParameterName, FString& OutDesc) const
{
	bool bSuccess = false;
	for(int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* Expression = Expressions[ExpressionIndex];
		if ( Expression )
		{
			// Parameter is a basic Expression Parameter
			if(Expression->IsA(UMaterialExpressionParameter::StaticClass()))
			{
				UMaterialExpressionParameter* Parameter = CastChecked<UMaterialExpressionParameter>(Expression);
				if(Parameter && Parameter->ParameterName == ParameterName)
				{
					OutDesc = Parameter->Desc;
					bSuccess = true;
					break;
				}
			}
			// Parameter is a Texture Sample Parameter
			else if(Expression->IsA(UMaterialExpressionTextureSampleParameter::StaticClass()))
			{
				UMaterialExpressionTextureSampleParameter* Parameter = CastChecked<UMaterialExpressionTextureSampleParameter>(Expression);
				if(Parameter && Parameter->ParameterName == ParameterName)
				{
					OutDesc = Parameter->Desc;
					bSuccess = true;
					break;
				}
			}
			// Parameter is a Font Sample Parameter
			else if(Expression->IsA(UMaterialExpressionFontSampleParameter::StaticClass()))
			{
				UMaterialExpressionFontSampleParameter* Parameter = CastChecked<UMaterialExpressionFontSampleParameter>(Expression);
				if(Parameter && Parameter->ParameterName == ParameterName)
				{
					OutDesc = Parameter->Desc;
					bSuccess = true;
					break;
				}
			}
		}
	}
	return bSuccess;
}

bool UMaterial::GetVectorParameterValue(FName ParameterName, FLinearColor& OutValue) const
{
	bool bSuccess = false;
	for(int32 ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
	{
		UMaterialExpressionVectorParameter* VectorParameter =
			Cast<UMaterialExpressionVectorParameter>(Expressions[ExpressionIndex]);

		if(VectorParameter && VectorParameter->ParameterName == ParameterName)
		{
			OutValue = VectorParameter->DefaultValue;
			bSuccess = true;
			break;
		}
	}
	return bSuccess;
}

bool UMaterial::GetScalarParameterValue(FName ParameterName, float& OutValue) const
{
	bool bSuccess = false;
	for(int32 ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
	{
		UMaterialExpressionScalarParameter* ScalarParameter =
			Cast<UMaterialExpressionScalarParameter>(Expressions[ExpressionIndex]);

		if(ScalarParameter && ScalarParameter->ParameterName == ParameterName)
		{
			OutValue = ScalarParameter->DefaultValue;
			bSuccess = true;
			break;
		}
	}
	return bSuccess;
}

bool UMaterial::GetTextureParameterValue(FName ParameterName, UTexture*& OutValue) const
{
	bool bSuccess = false;
	for(int32 ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
	{
		UMaterialExpressionTextureSampleParameter* TextureSampleParameter =
			Cast<UMaterialExpressionTextureSampleParameter>(Expressions[ExpressionIndex]);

		if(TextureSampleParameter && TextureSampleParameter->ParameterName == ParameterName)
		{
			OutValue = TextureSampleParameter->Texture;
			bSuccess = true;
			break;
		}
	}
	return bSuccess;
}

bool UMaterial::GetFontParameterValue(FName ParameterName,class UFont*& OutFontValue,int32& OutFontPage) const
{
	bool bSuccess = false;
	for(int32 ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
	{
		UMaterialExpressionFontSampleParameter* FontSampleParameter =
			Cast<UMaterialExpressionFontSampleParameter>(Expressions[ExpressionIndex]);

		if(FontSampleParameter && FontSampleParameter->ParameterName == ParameterName)
		{
			OutFontValue = FontSampleParameter->Font;
			OutFontPage = FontSampleParameter->FontTexturePage;
			bSuccess = true;
			break;
		}
	}
	return bSuccess;
}


bool UMaterial::GetStaticSwitchParameterValue(FName ParameterName,bool &OutValue,FGuid &OutExpressionGuid)
{
	bool bSuccess = false;
	for(int32 ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
	{
		UMaterialExpressionStaticBoolParameter* StaticSwitchParameter =
			Cast<UMaterialExpressionStaticBoolParameter>(Expressions[ExpressionIndex]);

		if(StaticSwitchParameter && StaticSwitchParameter->ParameterName == ParameterName)
		{
			OutValue = StaticSwitchParameter->DefaultValue;
			OutExpressionGuid = StaticSwitchParameter->ExpressionGUID;
			bSuccess = true;
			break;
		}
	}
	return bSuccess;
}


bool UMaterial::GetStaticComponentMaskParameterValue(FName ParameterName, bool &OutR, bool &OutG, bool &OutB, bool &OutA, FGuid &OutExpressionGuid)
{
	bool bSuccess = false;
	for(int32 ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
	{
		UMaterialExpressionStaticComponentMaskParameter* StaticComponentMaskParameter =
			Cast<UMaterialExpressionStaticComponentMaskParameter>(Expressions[ExpressionIndex]);

		if(StaticComponentMaskParameter && StaticComponentMaskParameter->ParameterName == ParameterName)
		{
			OutR = StaticComponentMaskParameter->DefaultR;
			OutG = StaticComponentMaskParameter->DefaultG;
			OutB = StaticComponentMaskParameter->DefaultB;
			OutA = StaticComponentMaskParameter->DefaultA;
			OutExpressionGuid = StaticComponentMaskParameter->ExpressionGUID;
			bSuccess = true;
			break;
		}
	}
	return bSuccess;
}


bool UMaterial::GetTerrainLayerWeightParameterValue(FName ParameterName, int32& OutWeightmapIndex, FGuid &OutExpressionGuid)
{
	bool bSuccess = false;
	OutWeightmapIndex = INDEX_NONE;
	bSuccess = true;
	return bSuccess;
}



bool UMaterial::GetRefractionSettings(float& OutBiasValue) const
{
	OutBiasValue = RefractionDepthBias;
	return true;
}

FMaterialRenderProxy* UMaterial::GetRenderProxy(bool Selected,bool Hovered) const
{
	check(!( Selected || Hovered ) || GIsEditor);
	return DefaultMaterialInstances[Selected ? 1 : ( Hovered ? 2 : 0 )];
}

UPhysicalMaterial* UMaterial::GetPhysicalMaterial() const
{
	return (PhysMaterial != NULL) ? PhysMaterial : GEngine->DefaultPhysMaterial;
}

/** Helper functions for text output of properties... */
#ifndef CASE_ENUM_TO_TEXT
#define CASE_ENUM_TO_TEXT(txt) case txt: return TEXT(#txt);
#endif

#ifndef TEXT_TO_ENUM
#define TEXT_TO_ENUM(eVal, txt)		if (FCString::Stricmp(TEXT(#eVal), txt) == 0)	return eVal;
#endif

const TCHAR* UMaterial::GetMaterialLightingModelString(EMaterialLightingModel InMaterialLightingModel)
{
	switch (InMaterialLightingModel)
	{
		FOREACH_ENUM_EMATERIALLIGHTINGMODEL(CASE_ENUM_TO_TEXT)
	}
	return TEXT("MLM_DefaultLit");
}

EMaterialLightingModel UMaterial::GetMaterialLightingModelFromString(const TCHAR* InMaterialLightingModelStr)
{
	#define TEXT_TO_LIGHTINGMODEL(m) TEXT_TO_ENUM(m, InMaterialLightingModelStr);
	FOREACH_ENUM_EMATERIALLIGHTINGMODEL(TEXT_TO_LIGHTINGMODEL)
	#undef TEXT_TO_LIGHTINGMODEL
	return MLM_DefaultLit;
}

const TCHAR* UMaterial::GetBlendModeString(EBlendMode InBlendMode)
{
	switch (InBlendMode)
	{
		FOREACH_ENUM_EBLENDMODE(CASE_ENUM_TO_TEXT)
	}
	return TEXT("BLEND_Opaque");
}

EBlendMode UMaterial::GetBlendModeFromString(const TCHAR* InBlendModeStr)
{
	#define TEXT_TO_BLENDMODE(b) TEXT_TO_ENUM(b, InBlendModeStr);
	FOREACH_ENUM_EBLENDMODE(TEXT_TO_BLENDMODE)
	#undef TEXT_TO_BLENDMODE
	return BLEND_Opaque;
}

static FAutoConsoleVariable GCompileMaterialsForShaderFormatCVar(
	TEXT("r.CompileMaterialsForShaderFormat"),
	TEXT(""),
	TEXT("When enabled, compile materials for this shader format in addition to those for the running platform.\n")
	TEXT("Note that these shaders are compiled and immediately tossed. This is only useful when directly inspecting output via r.DebugDumpShaderInfo.")
	);

void UMaterial::CacheResourceShadersForRendering(bool bRegenerateId)
{
	if (bRegenerateId)
	{
		// Regenerate this material's Id if requested
		FlushResourceShaderMaps();
	}

	UpdateResourceAllocations();

	if (FApp::CanEverRender())
	{
		uint32 FeatureLevelsToCompile = GetFeatureLevelsToCompileForRendering();
		EMaterialQualityLevel::Type ActiveQualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
		TArray<FMaterialResource*> ResourcesToCache;

		while (FeatureLevelsToCompile != 0)
		{
			ERHIFeatureLevel::Type FeatureLevel = (ERHIFeatureLevel::Type)FBitSet::GetAndClearNextBit(FeatureLevelsToCompile); 
			EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];

			// Only cache shaders for the quality level that will actually be used to render
			ResourcesToCache.Reset();
			ResourcesToCache.Add(MaterialResources[ActiveQualityLevel][FeatureLevel]);
			CacheShadersForResources(ShaderPlatform, ResourcesToCache, true);
		}

		FString AdditionalFormatToCache = GCompileMaterialsForShaderFormatCVar->GetString();
		if (!AdditionalFormatToCache.IsEmpty())
		{
			EShaderPlatform AdditionalPlatform = ShaderFormatToLegacyShaderPlatform(FName(*AdditionalFormatToCache));
			if (AdditionalPlatform != SP_NumPlatforms)
			{
				ResourcesToCache.Reset();
				CacheResourceShadersForCooking(AdditionalPlatform,ResourcesToCache);
				for (int32 i = 0; i < ResourcesToCache.Num(); ++i)
				{
					FMaterialResource* Resource = ResourcesToCache[i];
					delete Resource;
				}
				ResourcesToCache.Reset();
			}
		}

		RecacheUniformExpressions();
	}
}

void UMaterial::CacheResourceShadersForCooking(EShaderPlatform ShaderPlatform, TArray<FMaterialResource*>& OutCachedMaterialResources)
{
	TArray<FMaterialResource*> ResourcesToCache;
	ERHIFeatureLevel::Type TargetFeatureLevel = GetMaxSupportedFeatureLevel(ShaderPlatform);

	TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> > QualityLevelsUsed;
	GetQualityLevelNodeUsage(QualityLevelsUsed);

	bool bAnyQualityLevelUsed = false;

	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		bAnyQualityLevelUsed = bAnyQualityLevelUsed || QualityLevelsUsed[QualityLevelIndex];
	}

	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		// Add all quality levels if multiple are needed (due to different node graphs), otherwise just add the high quality entry
		if (bAnyQualityLevelUsed || QualityLevelIndex == EMaterialQualityLevel::High)
		{
			FMaterialResource* NewResource = AllocateResource();
			NewResource->SetMaterial(this, (EMaterialQualityLevel::Type)QualityLevelIndex, QualityLevelsUsed[QualityLevelIndex], (ERHIFeatureLevel::Type)TargetFeatureLevel);
			ResourcesToCache.Add(NewResource);
		}
	}

	check(ResourcesToCache.Num() > 0);

	CacheShadersForResources(ShaderPlatform, ResourcesToCache, false);

	for (int32 ResourceIndex = 0; ResourceIndex < ResourcesToCache.Num(); ResourceIndex++)
	{
		OutCachedMaterialResources.Add(ResourcesToCache[ResourceIndex]);
	}
}

void UMaterial::CacheShadersForResources(EShaderPlatform ShaderPlatform, const TArray<FMaterialResource*>& ResourcesToCache, bool bApplyCompletedShaderMapForRendering)
{
	RebuildExpressionTextureReferences();

	for (int32 ResourceIndex = 0; ResourceIndex < ResourcesToCache.Num(); ResourceIndex++)
	{
		FMaterialResource* CurrentResource = ResourcesToCache[ResourceIndex];
		const bool bSuccess = CurrentResource->CacheShaders(ShaderPlatform, bApplyCompletedShaderMapForRendering);

		if (!bSuccess)
		{
			if (IsDefaultMaterial())
			{
				UE_LOG(LogMaterial, Fatal, TEXT("Failed to compile Default Material %s for platform %s!"), 
					*GetPathName(), 
					*LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString());
			}

			UE_LOG(LogMaterial, Warning, TEXT("Failed to compile Material %s for platform %s, Default Material will be used in game."), 
				*GetPathName(), 
				*LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString());

			const TArray<FString>& CompileErrors = CurrentResource->GetCompileErrors();
			for (int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
			{
				// Always log material errors in an unsuppressed category
				UE_LOG(LogMaterial, Warning, TEXT("	%s"), *CompileErrors[ErrorIndex]);
			}
		}
	}
}

void UMaterial::FlushResourceShaderMaps()
{
	FPlatformMisc::CreateGuid(StateId);

	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		FMaterialResource* CurrentResource = MaterialResources[QualityLevelIndex][GRHIFeatureLevel];
		CurrentResource->ReleaseShaderMap();
	}
}

void UMaterial::RebuildMaterialFunctionInfo()
{	
	MaterialFunctionInfos.Empty();

	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* Expression = Expressions[ExpressionIndex];
		UMaterialExpressionMaterialFunctionCall* MaterialFunctionNode = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);

		if (MaterialFunctionNode)
		{
			if (MaterialFunctionNode->MaterialFunction)
			{
				{
					FMaterialFunctionInfo NewFunctionInfo;
					NewFunctionInfo.Function = MaterialFunctionNode->MaterialFunction;
					// Store the Id separate from the function, so we can detect changes to the function
					NewFunctionInfo.StateId = MaterialFunctionNode->MaterialFunction->StateId;
					MaterialFunctionInfos.Add(NewFunctionInfo);
				}

				TArray<UMaterialFunction*> DependentFunctions;
				MaterialFunctionNode->MaterialFunction->GetDependentFunctions(DependentFunctions);

				// Handle nested functions
				for (int32 FunctionIndex = 0; FunctionIndex < DependentFunctions.Num(); FunctionIndex++)
				{
					FMaterialFunctionInfo NewFunctionInfo;
					NewFunctionInfo.Function = DependentFunctions[FunctionIndex];
					NewFunctionInfo.StateId = DependentFunctions[FunctionIndex]->StateId;
					MaterialFunctionInfos.Add(NewFunctionInfo);
				}
			}

			// Update the function call node, so it can relink inputs and outputs as needed
			// Update even if MaterialFunctionNode->MaterialFunction is NULL, because we need to remove the invalid inputs in that case
			MaterialFunctionNode->UpdateFromFunctionResource();
		}
	}
}

void UMaterial::RebuildMaterialParameterCollectionInfo()
{
	MaterialParameterCollectionInfos.Empty();

	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* Expression = Expressions[ExpressionIndex];
		UMaterialExpressionCollectionParameter* CollectionParameter = Cast<UMaterialExpressionCollectionParameter>(Expression);
		UMaterialExpressionMaterialFunctionCall* MaterialFunctionNode = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);

		if (CollectionParameter && CollectionParameter->Collection)
		{
			FMaterialParameterCollectionInfo NewInfo;
			NewInfo.ParameterCollection = CollectionParameter->Collection;
			NewInfo.StateId = CollectionParameter->Collection->StateId;
			MaterialParameterCollectionInfos.AddUnique(NewInfo);
		}
		else if (MaterialFunctionNode && MaterialFunctionNode->MaterialFunction)
		{
			TArray<UMaterialFunction*> Functions;
			Functions.Add(MaterialFunctionNode->MaterialFunction);

			MaterialFunctionNode->MaterialFunction->GetDependentFunctions(Functions);

			// Handle nested functions
			for (int32 FunctionIndex = 0; FunctionIndex < Functions.Num(); FunctionIndex++)
			{
				UMaterialFunction* CurrentFunction = Functions[FunctionIndex];

				for (int32 FunctionExpressionIndex = 0; FunctionExpressionIndex < CurrentFunction->FunctionExpressions.Num(); FunctionExpressionIndex++)
				{
					UMaterialExpressionCollectionParameter* FunctionCollectionParameter = Cast<UMaterialExpressionCollectionParameter>(CurrentFunction->FunctionExpressions[FunctionExpressionIndex]);

					if (FunctionCollectionParameter && FunctionCollectionParameter->Collection)
					{
						FMaterialParameterCollectionInfo NewInfo;
						NewInfo.ParameterCollection = FunctionCollectionParameter->Collection;
						NewInfo.StateId = FunctionCollectionParameter->Collection->StateId;
						MaterialParameterCollectionInfos.AddUnique(NewInfo);
					}
				}
			}
		}
	}
}

void UMaterial::CacheExpressionTextureReferences()
{
	if ( ExpressionTextureReferences.Num() <= 0 )
	{
		RebuildExpressionTextureReferences();
	}
}

void UMaterial::RebuildExpressionTextureReferences()
{
	// Note: builds without editor only data will have an incorrect shader map id due to skipping this
	// That's ok, FMaterial::CacheShaders handles this 
	if (FPlatformProperties::HasEditorOnlyData())
	{
		// Rebuild all transient cached material properties which are based off of the editor-only data (expressions) and need to be up to date for compiling
		// Update the cached material function information, which will store off information about the functions this material uses
		RebuildMaterialFunctionInfo();
		RebuildMaterialParameterCollectionInfo();
	}

	ExpressionTextureReferences.Empty();
	AppendReferencedTextures(ExpressionTextureReferences);
}

FMaterialResource* UMaterial::GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel)
{
	if (QualityLevel == EMaterialQualityLevel::Num)
	{
		QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	}

	return MaterialResources[QualityLevel][InFeatureLevel];
}

const FMaterialResource* UMaterial::GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel) const
{
	if (QualityLevel == EMaterialQualityLevel::Num)
	{
		QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	}

	return MaterialResources[QualityLevel][InFeatureLevel];
}

void UMaterial::FixupTerrainLayerWeightNodes()
{
	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpressionLandscapeLayerWeight* LayerWeight = Cast<UMaterialExpressionLandscapeLayerWeight>(Expressions[ExpressionIndex]);
		UMaterialExpressionLandscapeLayerSwitch* LayerSwitch = Cast<UMaterialExpressionLandscapeLayerSwitch>(Expressions[ExpressionIndex]);

		// Regenerate parameter guids since the old ones were not generated consistently
		if (LayerWeight)
		{
			LayerWeight->UpdateParameterGuid(true, true);
		}
		else if (LayerSwitch)
		{
			LayerSwitch->UpdateParameterGuid(true, true);
		}
	}
}

void UMaterial::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() >= VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS)
	{
		SerializeInlineShaderMaps( CachedMaterialResourcesForCooking, Ar, MaterialResources );
	}
	else
	{
		FMaterialResource* LegacyResource = AllocateResource();
		LegacyResource->LegacySerialize(Ar);
		StateId = LegacyResource->GetLegacyId();
		delete LegacyResource;
	}

#if WITH_EDITOR
	if ( Ar.UE4Ver() < VER_UE4_PHYSICAL_MATERIAL_MODEL )
	{
		GMaterialsThatNeedPhysicalConversion.Set( this );
	}

	if (Ar.UE4Ver() < VER_UE4_FLIP_MATERIAL_COORDS)
	{
		GMaterialsThatNeedExpressionsFlipped.Set(this);
	}
	else if (Ar.UE4Ver() < VER_UE4_FIX_MATERIAL_COORDS)
	{
		GMaterialsThatNeedCoordinateCheck.Set(this);
	}
#endif // #if WITH_EDITOR

	if( Ar.UE4Ver() < VER_UE4_MATERIAL_ATTRIBUTES_REORDERING )
	{
		DoMaterialAttributeReorder(&DiffuseColor);
		DoMaterialAttributeReorder(&SpecularColor);
		DoMaterialAttributeReorder(&BaseColor);
		DoMaterialAttributeReorder(&Metallic);
		DoMaterialAttributeReorder(&Specular);
		DoMaterialAttributeReorder(&Roughness);
		DoMaterialAttributeReorder(&Normal);
		DoMaterialAttributeReorder(&EmissiveColor);
		DoMaterialAttributeReorder(&Opacity);
		DoMaterialAttributeReorder(&OpacityMask);
		DoMaterialAttributeReorder(&WorldPositionOffset);
		DoMaterialAttributeReorder(&WorldDisplacement);
		DoMaterialAttributeReorder(&TessellationMultiplier);
		DoMaterialAttributeReorder(&SubsurfaceColor);
		DoMaterialAttributeReorder(&AmbientOcclusion);
		DoMaterialAttributeReorder(&Refraction);
	}
}

void UMaterial::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// Reset the StateId on duplication since it needs to be unique for each material.
	FPlatformMisc::CreateGuid(StateId);
}

void UMaterial::BackwardsCompatibilityInputConversion()
{
#if WITH_EDITOR
	static const auto UseDiffuseSpecularMaterialInputs = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.UseDiffuseSpecularMaterialInputs"));

	if ( GMaterialsThatNeedPhysicalConversion.Get( this ) )
	{
		GMaterialsThatNeedPhysicalConversion.Clear( this );

		Roughness.Constant = 0.4238f;

		if( LightingModel != MLM_Unlit )
		{
			// Multiply SpecularColor by FresnelBaseReflectFraction
			if( SpecularColor.IsConnected() && FresnelBaseReflectFraction_DEPRECATED != 1.0f )
			{
				UMaterialExpressionMultiply* MulExpression = ConstructObject< UMaterialExpressionMultiply >( UMaterialExpressionMultiply::StaticClass(), this );
				Expressions.Add( MulExpression );

				if( UseDiffuseSpecularMaterialInputs->GetValueOnGameThread() )
				{
					MulExpression->MaterialExpressionEditorX += 200;
					MulExpression->MaterialExpressionEditorY += 20;
				}
				else
				{
					MulExpression->MaterialExpressionEditorX += 450;
					MulExpression->MaterialExpressionEditorY += 20;
				}

				MulExpression->Desc = TEXT("FresnelBaseReflectFraction");
				MulExpression->ConstA = 1.0f;
				MulExpression->ConstB = FresnelBaseReflectFraction_DEPRECATED;

				MulExpression->A.Connect( SpecularColor.OutputIndex, SpecularColor.Expression );
				SpecularColor.Connect( 0, MulExpression );
			}

			// Convert from SpecularPower to Roughness
			if( SpecularPower_DEPRECATED.IsConnected() )
			{
				check( GPowerToRoughnessMaterialFunction );

				UMaterialExpressionMaterialFunctionCall* FunctionExpression = ConstructObject< UMaterialExpressionMaterialFunctionCall >( UMaterialExpressionMaterialFunctionCall::StaticClass(), this );
				Expressions.Add( FunctionExpression );

				FunctionExpression->MaterialExpressionEditorX += 200;
				FunctionExpression->MaterialExpressionEditorY += 100;

				FunctionExpression->MaterialFunction = GPowerToRoughnessMaterialFunction;
				FunctionExpression->UpdateFromFunctionResource();

				FunctionExpression->GetInput(0)->Connect( SpecularPower_DEPRECATED.OutputIndex, SpecularPower_DEPRECATED.Expression );
				Roughness.Connect( 0, FunctionExpression );
			}
		}
	}

	if( LightingModel != MLM_Unlit && UseDiffuseSpecularMaterialInputs->GetValueOnGameThread() == 0 )
	{
		bool bIsDS = DiffuseColor.IsConnected() || SpecularColor.IsConnected();
		bool bIsBMS = BaseColor.IsConnected() || Metallic.IsConnected() || Specular.IsConnected();

		if( bIsDS && !bIsBMS )
		{
			// ConvertFromDiffSpec

			check( GConvertFromDiffSpecMaterialFunction );

			UMaterialExpressionMaterialFunctionCall* FunctionExpression = ConstructObject< UMaterialExpressionMaterialFunctionCall >( UMaterialExpressionMaterialFunctionCall::StaticClass(), this );
			Expressions.Add( FunctionExpression );

			FunctionExpression->MaterialExpressionEditorX += 200;

			FunctionExpression->MaterialFunction = GConvertFromDiffSpecMaterialFunction;
			FunctionExpression->UpdateFromFunctionResource();

			if( DiffuseColor.IsConnected() )
			{
				FunctionExpression->GetInput(0)->Connect( DiffuseColor.OutputIndex, DiffuseColor.Expression );
			}

			if( SpecularColor.IsConnected() )
			{
				FunctionExpression->GetInput(1)->Connect( SpecularColor.OutputIndex, SpecularColor.Expression );
			}

			BaseColor.Connect( 0, FunctionExpression );
			Metallic.Connect( 1, FunctionExpression );
			Specular.Connect( 2, FunctionExpression );
		}
	}
#endif // WITH_EDITOR
}

void UMaterial::GetQualityLevelNodeUsage(TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> >& OutQualityLevelsUsed)
{
	OutQualityLevelsUsed.AddZeroed(EMaterialQualityLevel::Num);

	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* Expression = Expressions[ExpressionIndex];
		UMaterialExpressionQualitySwitch* QualitySwitchNode = Cast<UMaterialExpressionQualitySwitch>(Expression);
		UMaterialExpressionMaterialFunctionCall* MaterialFunctionNode = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);

		if (QualitySwitchNode)
		{
			for (int32 InputIndex = 0; InputIndex < EMaterialQualityLevel::Num; InputIndex++)
			{
				if (QualitySwitchNode->Inputs[InputIndex].Expression)
				{
					OutQualityLevelsUsed[InputIndex] = true;
				}
			}
		}
		else if (MaterialFunctionNode && MaterialFunctionNode->MaterialFunction)
		{
			TArray<UMaterialFunction*> Functions;
			Functions.Add(MaterialFunctionNode->MaterialFunction);

			MaterialFunctionNode->MaterialFunction->GetDependentFunctions(Functions);

			// Handle nested functions
			for (int32 FunctionIndex = 0; FunctionIndex < Functions.Num(); FunctionIndex++)
			{
				UMaterialFunction* CurrentFunction = Functions[FunctionIndex];

				for (int32 FunctionExpressionIndex = 0; FunctionExpressionIndex < CurrentFunction->FunctionExpressions.Num(); FunctionExpressionIndex++)
				{
					UMaterialExpressionQualitySwitch* SwitchNode = Cast<UMaterialExpressionQualitySwitch>(CurrentFunction->FunctionExpressions[FunctionExpressionIndex]);

					if (SwitchNode)
					{
						for (int32 InputIndex = 0; InputIndex < EMaterialQualityLevel::Num; InputIndex++)
						{
							if (SwitchNode->Inputs[InputIndex].Expression)
							{
								OutQualityLevelsUsed[InputIndex] = true;
							}
						}
					}
				}
			}
		}
	}
}

void UMaterial::UpdateResourceAllocations()
{
	TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> > QualityLevelsUsed;
	GetQualityLevelNodeUsage(QualityLevelsUsed);

	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
		{
			FMaterialResource*& CurrentResource = MaterialResources[QualityLevelIndex][FeatureLevelIndex];

			if (!CurrentResource)
			{
				CurrentResource = AllocateResource();
			}

			const bool bQualityLevelHasDifferentNodes = QualityLevelsUsed[QualityLevelIndex];
			// Setup transient FMaterialResource properties that are needed to use this resource for rendering or compilation
			CurrentResource->SetMaterial(this, (EMaterialQualityLevel::Type)QualityLevelIndex, bQualityLevelHasDifferentNodes, (ERHIFeatureLevel::Type)FeatureLevelIndex);
		}
	}
}

TMap<FGuid, UMaterialInterface*> LightingGuidFixupMap;

void UMaterial::PostLoad()
{
	Super::PostLoad();

	if (!IsDefaultMaterial())
	{
		AssertDefaultMaterialsPostLoaded();
	}	

	if ( GIsEditor && GetOuter() == GetTransientPackage() && FCString::Strstr(*GetName(), TEXT("MEStatsMaterial_")))
	{
		bIsMaterialEditorStatsMaterial = true;
	}

	// Ensure expressions have been postloaded before we use them for compiling
	// Any UObjects used by material compilation must be postloaded here
	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		if (Expressions[ExpressionIndex])
		{
			Expressions[ExpressionIndex]->ConditionalPostLoad();
		}
	}

	for (int32 CollectionIndex = 0; CollectionIndex < MaterialParameterCollectionInfos.Num(); CollectionIndex++)
	{
		if (MaterialParameterCollectionInfos[CollectionIndex].ParameterCollection)
		{
			MaterialParameterCollectionInfos[CollectionIndex].ParameterCollection->ConditionalPostLoad();
		}
	}

	if (GetLinkerUE4Version() < VER_UE4_FIXUP_TERRAIN_LAYER_NODES)
	{
		FixupTerrainLayerWeightNodes();
	}

	// Fixup for legacy materials which didn't recreate the lighting guid properly on duplication
	if (GetLinker() && GetLinker()->UE4Ver() < VER_UE4_BUMPED_MATERIAL_EXPORT_GUIDS)
	{
		UMaterialInterface** ExistingMaterial = LightingGuidFixupMap.Find(GetLightingGuid());

		if (ExistingMaterial)
		{
			SetLightingGuid();
		}

		LightingGuidFixupMap.Add(GetLightingGuid(), this);
	}

	// Fix exclusive material usage flags moved to an enum.
	if (bUsedAsLightFunction_DEPRECATED)
	{
		MaterialDomain = MD_LightFunction;
	}
	else if (bUsedWithDeferredDecal_DEPRECATED)
	{
		MaterialDomain = MD_DeferredDecal;
	}
	bUsedAsLightFunction_DEPRECATED = false;
	bUsedWithDeferredDecal_DEPRECATED = false;

	// Fix the lighting model to be valid.  Loading a material saved with a lighting model that has been removed will yield a MLM_MAX.
	if(LightingModel == MLM_MAX)
	{
		LightingModel = MLM_DefaultLit;
	}

	if(DecalBlendMode == DBM_MAX)
	{
		DecalBlendMode = DBM_Translucent;
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Clean up any removed material expression classes	
		if (Expressions.Remove(NULL) != 0)
		{
			// Force this material to recompile because its expressions have changed
			// Warning: any content taking this path will recompile every load until saved!
			// Which means removing an expression class will cause the need for a resave of all materials
			FlushResourceShaderMaps();
		}
	}
#endif

	if (!StateId.IsValid())
	{
		// Fixup for some legacy content
		// This path means recompiling every time the material is loaded until it is saved
		FPlatformMisc::CreateGuid(StateId);
	}

	BackwardsCompatibilityInputConversion();

#if WITH_EDITOR
	if ( GMaterialsThatNeedSamplerFixup.Get( this ) )
	{
		GMaterialsThatNeedSamplerFixup.Clear( this );
		const int32 ExpressionCount = Expressions.Num();
		for ( int32 ExpressionIndex = 0; ExpressionIndex < ExpressionCount; ++ExpressionIndex )
		{
			UMaterialExpressionTextureSample* TextureExpression = Cast<UMaterialExpressionTextureSample>( Expressions[ ExpressionIndex ] );
			if ( TextureExpression && TextureExpression->Texture )
			{
				switch( TextureExpression->Texture->CompressionSettings )
				{
				case TC_Normalmap:
					TextureExpression->SamplerType = SAMPLERTYPE_Normal;
					break;
					
				case TC_Grayscale:
					TextureExpression->SamplerType = SAMPLERTYPE_Grayscale;
					break;

				case TC_Masks:
					TextureExpression->SamplerType = SAMPLERTYPE_Masks;
					break;

				case TC_Alpha:
					TextureExpression->SamplerType = SAMPLERTYPE_Alpha;
					break;
				default:
					TextureExpression->SamplerType = SAMPLERTYPE_Color;
					break;
				}
			}
		}
	}
#endif // #if WITH_EDITOR

	STAT(double MaterialLoadTime = 0);
	{
		SCOPE_SECONDS_COUNTER(MaterialLoadTime);

		// enable caching in postload for derived data cache commandlet and cook by the book
		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		if (TPM && (TPM->RestrictFormatsToRuntimeOnly() == false))
		{
			ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
			TArray<ITargetPlatform*> Platforms = TPM->GetActiveTargetPlatforms();
			// Cache for all the shader formats that the cooking target requires
			for (int32 FormatIndex = 0; FormatIndex < Platforms.Num(); FormatIndex++)
			{
				BeginCacheForCookedPlatformData(Platforms[FormatIndex]);
			}
		}

		//Don't compile shaders in post load for dev overhead materials.
		if (FApp::CanEverRender() && !bIsMaterialEditorStatsMaterial)
		{
			CacheResourceShadersForRendering(false);
		}
	}
	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_MaterialLoading,(float)MaterialLoadTime);

	if( GIsEditor && !IsTemplate() )
	{
		// Ensure that the ReferencedTextureGuids array is up to date.
		UpdateLightmassTextureTracking();
	}

	for (int32 i = 0; i < ARRAY_COUNT(DefaultMaterialInstances); i++)
	{
		if (DefaultMaterialInstances[i])
		{
			DefaultMaterialInstances[i]->GameThread_UpdateDistanceFieldPenumbraScale(GetDistanceFieldPenumbraScale());
		}
	}

#if WITH_EDITOR
	if (GMaterialsThatNeedExpressionsFlipped.Get(this))
	{
		GMaterialsThatNeedExpressionsFlipped.Clear(this);
		FlipExpressionPositions(Expressions, EditorComments, true, this);
	}
	else if (GMaterialsThatNeedCoordinateCheck.Get(this))
	{
		GMaterialsThatNeedCoordinateCheck.Clear(this);
		if (HasFlippedCoordinates())
		{
			FlipExpressionPositions(Expressions, EditorComments, false, this);
		}
	}
#endif // #if WITH_EDITOR
}

void UMaterial::BeginCacheForCookedPlatformData( const ITargetPlatform *TargetPlatform )
{
	TArray<FName> DesiredShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

	TArray<FMaterialResource*> *CachedMaterialResourcesForPlatform = CachedMaterialResourcesForCooking.Find( TargetPlatform );

	if ( CachedMaterialResourcesForPlatform == NULL )
	{
		CachedMaterialResourcesForCooking.Add( TargetPlatform );
		CachedMaterialResourcesForPlatform = CachedMaterialResourcesForCooking.Find( TargetPlatform );

		check( CachedMaterialResourcesForPlatform != NULL );

		if (DesiredShaderFormats.Num())
		{
			// Cache for all the shader formats that the cooking target requires
			for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
			{
				const EShaderPlatform TargetPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);

				// Begin caching shaders for the target platform and store the material resource being compiled into CachedMaterialResourcesForCooking
				CacheResourceShadersForCooking(TargetPlatform, *CachedMaterialResourcesForPlatform);
			}
		}
	}
}




void UMaterial::ClearCachedCookedPlatformData( const ITargetPlatform *TargetPlatform )
{
	TArray<FMaterialResource*> *CachedMaterialResourcesForPlatform = CachedMaterialResourcesForCooking.Find( TargetPlatform );
	if ( CachedMaterialResourcesForPlatform != NULL )
	{
		for (int32 CachedResourceIndex = 0; CachedResourceIndex < CachedMaterialResourcesForPlatform->Num(); CachedResourceIndex++)
		{
			delete (*CachedMaterialResourcesForPlatform)[CachedResourceIndex];
		}
	}
	CachedMaterialResourcesForCooking.Remove( TargetPlatform );
}

void UMaterial::ClearAllCachedCookedPlatformData()
{
	for ( auto It : CachedMaterialResourcesForCooking )
	{
		TArray<FMaterialResource*> &CachedMaterialResourcesForPlatform = It.Value;
		for (int32 CachedResourceIndex = 0; CachedResourceIndex < CachedMaterialResourcesForPlatform.Num(); CachedResourceIndex++)
		{
			delete (CachedMaterialResourcesForPlatform)[CachedResourceIndex];
		}
	}
	CachedMaterialResourcesForCooking.Empty();
}

#if WITH_EDITOR
bool UMaterial::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, OpacityMaskClipValue))
		{
			return BlendMode == BLEND_Masked;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, MaterialDecalResponse))
		{
			static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DBuffer"));

			return CVar->GetValueOnGameThread() > 0;
		}		

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, BlendableLocation) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, BlendablePriority)
			)
		{
			return MaterialDomain == MD_PostProcess;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, BlendMode))
		{
			return MaterialDomain == MD_Surface;
		}
	
		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, LightingModel))
		{
			return MaterialDomain == MD_Surface;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, DecalBlendMode))
		{
			return MaterialDomain == MD_DeferredDecal;
		}
		else if (
			FCString::Strncmp(*PropertyName, TEXT("bUsedWith"), 9) == 0 ||
			FCString::Strcmp(*PropertyName, TEXT("bUsesDistortion")) == 0
			)
		{
			return MaterialDomain == MD_Surface;
		}
		else if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, RefractionDepthBias))
		{
			return Refraction.IsConnected();
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bEnableSeparateTranslucency)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bEnableResponsiveAA)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bDisableDepthTest)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bUseTranslucencyVertexFog))
		{
			return IsTranslucentBlendMode(BlendMode);
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucencyLightingMode)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucencyDirectionalLightingIntensity)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentShadowDensityScale)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentSelfShadowDensityScale)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentSelfShadowSecondDensityScale)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentSelfShadowSecondOpacity)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentBackscatteringExponent)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentMultipleScatteringExtinction)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentShadowStartOffset))
		{
			return IsTranslucentBlendMode(BlendMode) && LightingModel != MLM_Unlit;
		}
	}

	return true;
}

void UMaterial::PreEditChange(UProperty* PropertyThatChanged)
{
	Super::PreEditChange(PropertyThatChanged);

	// Flush all pending rendering commands.
	FlushRenderingCommands();
}

void UMaterial::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	// check for distortion in material 
	{
		bUsesDistortion = false;
		// can only have distortion with translucent blend modes
		if(IsTranslucentBlendMode((EBlendMode)BlendMode))
		{
			// check for a distortion value
			if(Refraction.Expression
			|| (Refraction.UseConstant && FMath::Abs(Refraction.Constant) >= KINDA_SMALL_NUMBER))
			{
				bUsesDistortion = true;
			}
		}
	}

	// Check if the material is masked and uses a custom opacity (that's not 1.0f).
	bIsMasked = ((EBlendMode(BlendMode) == BLEND_Masked) && (OpacityMask.Expression || (OpacityMask.UseConstant && OpacityMask.Constant < 0.999f)));

	bool bRequiresCompilation = true;
	if( PropertyThatChanged ) 
	{
		// Don't recompile the material if we only changed the PhysMaterial property.
		if (PropertyThatChanged->GetName() == TEXT("PhysMaterial"))
		{
			bRequiresCompilation = false;
		}
	}

	TranslucencyDirectionalLightingIntensity = FMath::Clamp(TranslucencyDirectionalLightingIntensity, .1f, 10.0f);

	// Don't want to recompile after a duplicate because it's just been done by PostLoad
	if( PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate )
	{
		bRequiresCompilation = false;
	}

	// Prevent constant recompliation while spinning properties
	if (bRequiresCompilation && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive )
	{
		CacheResourceShadersForRendering(true);
		RecacheMaterialInstanceUniformExpressions(this);

		// Ensure that the ReferencedTextureGuids array is up to date.
		if (GIsEditor)
		{
			UpdateLightmassTextureTracking();
		}

		// Ensure that any components with static elements using this material are reregistered so changes
		// are propagated to them. The preview material is only applied to the preview mesh component,
		// and that reregister is handled by the material editor.
		if (!bIsPreviewMaterial && !bIsMaterialEditorStatsMaterial)
		{
			FGlobalComponentReregisterContext RecreateComponents;
		}
	}
	
	for (int32 i = 0; i < ARRAY_COUNT(DefaultMaterialInstances); i++)
	{
		if (DefaultMaterialInstances[i])
		{
			DefaultMaterialInstances[i]->GameThread_UpdateDistanceFieldPenumbraScale(GetDistanceFieldPenumbraScale());
		}
	}

	// many property changes can require rebuild of graph so always mark as changed
	// not interested in PostEditChange calls though as the graph may have instigated it
	if (PropertyThatChanged && MaterialGraph)
	{
		MaterialGraph->NotifyGraphChanged();
	}
} 
#endif // WITH_EDITOR

bool UMaterial::AddExpressionParameter(UMaterialExpression* Expression)
{
	if(!Expression)
	{
		return false;
	}

	bool bRet = false;

	if(Expression->IsA(UMaterialExpressionParameter::StaticClass()))
	{
		UMaterialExpressionParameter *Param = (UMaterialExpressionParameter*)Expression;

		TArray<UMaterialExpression*> *ExpressionList = EditorParameters.Find(Param->ParameterName);

		if(!ExpressionList)
		{
			ExpressionList = &EditorParameters.Add(Param->ParameterName, TArray<UMaterialExpression*>());
		}

		ExpressionList->Add(Param);
		bRet = true;
	}
	else if(Expression->IsA(UMaterialExpressionTextureSampleParameter::StaticClass()))
	{
		UMaterialExpressionTextureSampleParameter *Param = (UMaterialExpressionTextureSampleParameter*)Expression;

		TArray<UMaterialExpression*> *ExpressionList = EditorParameters.Find(Param->ParameterName);

		if(!ExpressionList)
		{
			ExpressionList = &EditorParameters.Add(Param->ParameterName, TArray<UMaterialExpression*>());
		}

		ExpressionList->Add(Param);
		bRet = true;
	}
	else if(Expression->IsA(UMaterialExpressionFontSampleParameter::StaticClass()))
	{
		UMaterialExpressionFontSampleParameter *Param = (UMaterialExpressionFontSampleParameter*)Expression;

		TArray<UMaterialExpression*> *ExpressionList = EditorParameters.Find(Param->ParameterName);

		if(!ExpressionList)
		{
			ExpressionList = &EditorParameters.Add(Param->ParameterName, TArray<UMaterialExpression*>());
		}

		ExpressionList->Add(Param);
		bRet = true;
	}

	return bRet;
}


bool UMaterial::RemoveExpressionParameter(UMaterialExpression* Expression)
{
	FName ParmName;

	if(GetExpressionParameterName(Expression, ParmName))
	{
		TArray<UMaterialExpression*> *ExpressionList = EditorParameters.Find(ParmName);

		if(ExpressionList)
		{
			return ExpressionList->Remove(Expression) > 0;
		}
	}

	return false;
}


bool UMaterial::IsParameter(UMaterialExpression* Expression)
{
	bool bRet = false;

	if(Expression->IsA(UMaterialExpressionParameter::StaticClass()))
	{
		bRet = true;
	}
	else if(Expression->IsA(UMaterialExpressionTextureSampleParameter::StaticClass()))
	{
		bRet = true;
	}
	else if(Expression->IsA(UMaterialExpressionFontSampleParameter::StaticClass()))
	{
		bRet = true;
	}

	return bRet;
}


bool UMaterial::IsDynamicParameter(UMaterialExpression* Expression)
{
	if (Expression->IsA(UMaterialExpressionDynamicParameter::StaticClass()))
	{
		return true;
	}

	return false;
}



void UMaterial::BuildEditorParameterList()
{
	EmptyEditorParameters();

	for(int32 MaterialExpressionIndex = 0 ; MaterialExpressionIndex < Expressions.Num() ; ++MaterialExpressionIndex)
	{
		AddExpressionParameter(Expressions[ MaterialExpressionIndex ]);
	}
}


bool UMaterial::HasDuplicateParameters(UMaterialExpression* Expression)
{
	FName ExpressionName;

	if(GetExpressionParameterName(Expression, ExpressionName))
	{
		TArray<UMaterialExpression*> *ExpressionList = EditorParameters.Find(ExpressionName);

		if(ExpressionList)
		{
			for(int32 ParmIndex = 0; ParmIndex < ExpressionList->Num(); ++ParmIndex)
			{
				UMaterialExpression *CurNode = (*ExpressionList)[ParmIndex];
				if(CurNode != Expression && CurNode->GetClass() == Expression->GetClass())
				{
					return true;
				}
			}
		}
	}

	return false;
}


bool UMaterial::HasDuplicateDynamicParameters(UMaterialExpression* Expression)
{
	UMaterialExpressionDynamicParameter* DynParam = Cast<UMaterialExpressionDynamicParameter>(Expression);
	if (DynParam)
	{
		for (int32 ExpIndex = 0; ExpIndex < Expressions.Num(); ExpIndex++)
		{
			UMaterialExpressionDynamicParameter* CheckDynParam = Cast<UMaterialExpressionDynamicParameter>(Expressions[ExpIndex]);
			if (CheckDynParam != Expression)
			{
				return true;
			}
		}
	}
	return false;
}


void UMaterial::UpdateExpressionDynamicParameterNames(UMaterialExpression* Expression)
{
	UMaterialExpressionDynamicParameter* DynParam = Cast<UMaterialExpressionDynamicParameter>(Expression);
	if (DynParam)
	{
		for (int32 ExpIndex = 0; ExpIndex < Expressions.Num(); ExpIndex++)
		{
			UMaterialExpressionDynamicParameter* CheckParam = Cast<UMaterialExpressionDynamicParameter>(Expressions[ExpIndex]);
			if (CheckParam && (CheckParam != DynParam))
			{
				for (int32 NameIndex = 0; NameIndex < 4; NameIndex++)
				{
					CheckParam->ParamNames[NameIndex] = DynParam->ParamNames[NameIndex];
				}
#if WITH_EDITORONLY_DATA
				CheckParam->GraphNode->ReconstructNode();
#endif // WITH_EDITORONLY_DATA
			}
		}
	}
}


void UMaterial::PropagateExpressionParameterChanges(UMaterialExpression* Parameter)
{
	FName ParmName;
	bool bRet = GetExpressionParameterName(Parameter, ParmName);

	if(bRet)
	{
		TArray<UMaterialExpression*> *ExpressionList = EditorParameters.Find(ParmName);

		if(ExpressionList && ExpressionList->Num() > 1)
		{
			for(int32 Index = 0; Index < ExpressionList->Num(); ++Index)
			{
				CopyExpressionParameters(Parameter, (*ExpressionList)[Index]);
			}
		}
		else if(!ExpressionList)
		{
			bRet = false;
		}
	}
}


void UMaterial::UpdateExpressionParameterName(UMaterialExpression* Expression)
{
	FName ExpressionName;

	for(TMap<FName, TArray<UMaterialExpression*> >::TIterator Iter(EditorParameters); Iter; ++Iter)
	{
		if(Iter.Value().Remove(Expression) > 0)
		{
			if(Iter.Value().Num() == 0)
			{
				EditorParameters.Remove(Iter.Key());
			}

			AddExpressionParameter(Expression);
			break;
		}
	}
}


bool UMaterial::GetExpressionParameterName(UMaterialExpression* Expression, FName& OutName)
{
	bool bRet = false;

	if(Expression->IsA(UMaterialExpressionParameter::StaticClass()))
	{
		OutName = ((UMaterialExpressionParameter*)Expression)->ParameterName;
		bRet = true;
	}
	else if(Expression->IsA(UMaterialExpressionTextureSampleParameter::StaticClass()))
	{
		OutName = ((UMaterialExpressionTextureSampleParameter*)Expression)->ParameterName;
		bRet = true;
	}
	else if(Expression->IsA(UMaterialExpressionFontSampleParameter::StaticClass()))
	{
		OutName = ((UMaterialExpressionFontSampleParameter*)Expression)->ParameterName;
		bRet = true;
	}

	return bRet;
}


bool UMaterial::CopyExpressionParameters(UMaterialExpression* Source, UMaterialExpression* Destination)
{
	if(!Source || !Destination || Source == Destination || Source->GetClass() != Destination->GetClass())
	{
		return false;
	}

	bool bRet = true;

	if(Source->IsA(UMaterialExpressionTextureSampleParameter::StaticClass()))
	{
		UMaterialExpressionTextureSampleParameter *SourceTex = (UMaterialExpressionTextureSampleParameter*)Source;
		UMaterialExpressionTextureSampleParameter *DestTex = (UMaterialExpressionTextureSampleParameter*)Destination;

		DestTex->Modify();
		DestTex->Texture = SourceTex->Texture;
	}
	else if(Source->IsA(UMaterialExpressionVectorParameter::StaticClass()))
	{
		UMaterialExpressionVectorParameter *SourceVec = (UMaterialExpressionVectorParameter*)Source;
		UMaterialExpressionVectorParameter *DestVec = (UMaterialExpressionVectorParameter*)Destination;

		DestVec->Modify();
		DestVec->DefaultValue = SourceVec->DefaultValue;
	}
	else if(Source->IsA(UMaterialExpressionStaticBoolParameter::StaticClass()))
	{
		UMaterialExpressionStaticBoolParameter *SourceVec = (UMaterialExpressionStaticBoolParameter*)Source;
		UMaterialExpressionStaticBoolParameter *DestVec = (UMaterialExpressionStaticBoolParameter*)Destination;

		DestVec->Modify();
		DestVec->DefaultValue = SourceVec->DefaultValue;
	}
	else if(Source->IsA(UMaterialExpressionStaticComponentMaskParameter::StaticClass()))
	{
		UMaterialExpressionStaticComponentMaskParameter *SourceVec = (UMaterialExpressionStaticComponentMaskParameter*)Source;
		UMaterialExpressionStaticComponentMaskParameter *DestVec = (UMaterialExpressionStaticComponentMaskParameter*)Destination;

		DestVec->Modify();
		DestVec->DefaultR = SourceVec->DefaultR;
		DestVec->DefaultG = SourceVec->DefaultG;
		DestVec->DefaultB = SourceVec->DefaultB;
		DestVec->DefaultA = SourceVec->DefaultA;
	}
	else if(Source->IsA(UMaterialExpressionScalarParameter::StaticClass()))
	{
		UMaterialExpressionScalarParameter *SourceVec = (UMaterialExpressionScalarParameter*)Source;
		UMaterialExpressionScalarParameter *DestVec = (UMaterialExpressionScalarParameter*)Destination;

		DestVec->Modify();
		DestVec->DefaultValue = SourceVec->DefaultValue;
	}
	else if(Source->IsA(UMaterialExpressionFontSampleParameter::StaticClass()))
	{
		UMaterialExpressionFontSampleParameter *SourceFont = (UMaterialExpressionFontSampleParameter*)Source;
		UMaterialExpressionFontSampleParameter *DestFont = (UMaterialExpressionFontSampleParameter*)Destination;

		DestFont->Modify();
		DestFont->Font = SourceFont->Font;
		DestFont->FontTexturePage = SourceFont->FontTexturePage;
	}
	else
	{
		bRet = false;
	}

	return bRet;
}

void UMaterial::BeginDestroy()
{
	Super::BeginDestroy();

	ReleaseFence.BeginFence();
}

bool UMaterial::IsReadyForFinishDestroy()
{
	bool bReady = Super::IsReadyForFinishDestroy();

	return bReady && ReleaseFence.IsFenceComplete();
}

void UMaterial::ReleaseResources()
{
	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
		{
			FMaterialResource*& CurrentResource = MaterialResources[QualityLevelIndex][FeatureLevelIndex];
			delete CurrentResource;
			CurrentResource = NULL;
		}
	}

	ClearAllCachedCookedPlatformData();

	for (int32 InstanceIndex = 0; InstanceIndex < 3; ++InstanceIndex)
	{
		if (DefaultMaterialInstances[InstanceIndex])
		{
			DefaultMaterialInstances[InstanceIndex]->GameThread_Destroy();
			DefaultMaterialInstances[InstanceIndex] = NULL;
		}
	}
}

void UMaterial::FinishDestroy()
{
	ReleaseResources();

	Super::FinishDestroy();
}


SIZE_T UMaterial::GetResourceSize(EResourceSizeMode::Type Mode)
{
	int32 ResourceSize = 0;

	for (int32 InstanceIndex = 0; InstanceIndex < 3; ++InstanceIndex)
	{
		if (DefaultMaterialInstances[InstanceIndex])
		{
			ResourceSize += sizeof(FDefaultMaterialInstance);
		}
	}

	if (Mode == EResourceSizeMode::Inclusive)
	{
		for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
		{
			for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
			{
				FMaterialResource* CurrentResource = MaterialResources[QualityLevelIndex][FeatureLevelIndex];
				ResourceSize += CurrentResource->GetResourceSizeInclusive();
			}
		}

		TArray<UTexture*> TheReferencedTextures;
		for ( int32 ExpressionIndex= 0 ; ExpressionIndex < Expressions.Num() ; ++ExpressionIndex )
		{
			UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>( Expressions[ExpressionIndex] );
			if ( TextureSample && TextureSample->Texture )
			{
				UTexture* Texture						= TextureSample->Texture;
				const bool bTextureAlreadyConsidered	= TheReferencedTextures.Contains( Texture );
				if ( !bTextureAlreadyConsidered )
				{
					TheReferencedTextures.Add( Texture );
					ResourceSize += Texture->GetResourceSize(Mode);
				}
			}
		}
	}

	return ResourceSize;
}

void UMaterial::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UMaterial* This = CastChecked<UMaterial>(InThis);

	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
		{
			FMaterialResource* CurrentResource = This->MaterialResources[QualityLevelIndex][FeatureLevelIndex];
			CurrentResource->AddReferencedObjects(Collector);
		}
	}
#if WITH_EDITORONLY_DATA
	Collector.AddReferencedObject(This->MaterialGraph, This);
#endif

	Super::AddReferencedObjects(This, Collector);
}

void UMaterial::UpdateMaterialShaders(TArray<FShaderType*>& ShaderTypesToFlush, TArray<const FVertexFactoryType*>& VFTypesToFlush, EShaderPlatform ShaderPlatform)
{
	// Create a material update context so we can safely update materials.
	{
		FMaterialUpdateContext UpdateContext(FMaterialUpdateContext::EOptions::Default, ShaderPlatform);

		// Go through all material shader maps and flush the appropriate shaders
		FMaterialShaderMap::FlushShaderTypes(ShaderTypesToFlush, VFTypesToFlush);

		// There should be no references to the given material shader types at this point
		// If there still are shaders of the given types, they may be reused when we call CacheResourceShaders instead of compiling new shaders
		for (int32 ShaderTypeIndex = 0; ShaderTypeIndex < ShaderTypesToFlush.Num(); ShaderTypeIndex++)
		{
			FShaderType* CurrentType = ShaderTypesToFlush[ShaderTypeIndex];
			if (CurrentType->GetMaterialShaderType() || CurrentType->GetMeshMaterialShaderType())
			{
				checkf(CurrentType->GetNumShaders() == 0, TEXT("Type %s, Shaders %u"), CurrentType->GetName(), CurrentType->GetNumShaders());
			}
		}

		int32 NumMaterials = 0;

		for( TObjectIterator<UMaterial> It; It; ++It )
		{
			NumMaterials++;
		}

		GWarn->StatusUpdate(0, NumMaterials, NSLOCTEXT("Material", "BeginAsyncMaterialShaderCompilesTask", "Kicking off async material shader compiles..."));

		int32 UpdateStatusDivisor = FMath::Max<int32>(NumMaterials / 20, 1);
		int32 MaterialIndex = 0;

		// Reinitialize the material shader maps
		for( TObjectIterator<UMaterial> It; It; ++It )
		{
			UMaterial* BaseMaterial = *It;
			UpdateContext.AddMaterial(BaseMaterial);
			BaseMaterial->CacheResourceShadersForRendering(false);

			// Limit the frequency of progress updates
			if (MaterialIndex % UpdateStatusDivisor == 0)
			{
				GWarn->UpdateProgress(MaterialIndex, NumMaterials);
			}
			MaterialIndex++;
		}

		// The material update context will safely update all dependent material instances when
		// it leaves scope.
	}

	// Update any FMaterials not belonging to a UMaterialInterface, for example FExpressionPreviews
	// If we did not do this, the editor would crash the next time it tried to render one of those previews
	// And didn't find a shader that had been flushed for the preview's shader map.
	FMaterial::UpdateEditorLoadedMaterialResources();
}

void UMaterial::BackupMaterialShadersToMemory(EShaderPlatform ShaderPlatform, TMap<FMaterialShaderMap*, TScopedPointer<TArray<uint8> > >& ShaderMapToSerializedShaderData)
{
	// Process FMaterialShaderMap's referenced by UObjects (UMaterial, UMaterialInstance)
	for (TObjectIterator<UMaterialInterface> It; It; ++It)
	{
		UMaterialInterface* Material = *It;
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
		UMaterial* BaseMaterial = Cast<UMaterial>(Material);

		if (MaterialInstance)
		{
			if (MaterialInstance->bHasStaticPermutationResource)
			{
				TArray<FMaterialShaderMap*> MIShaderMaps;
				MaterialInstance->GetAllShaderMaps(MIShaderMaps);

				for (int32 ShaderMapIndex = 0; ShaderMapIndex < MIShaderMaps.Num(); ShaderMapIndex++)
				{
					FMaterialShaderMap* ShaderMap = MIShaderMaps[ShaderMapIndex];

					if (ShaderMap && !ShaderMapToSerializedShaderData.Contains(ShaderMap))
					{
						TArray<uint8>* ShaderData = ShaderMap->BackupShadersToMemory();
						ShaderMapToSerializedShaderData.Emplace(ShaderMap, ShaderData);
					}
				}
			}
		}
		else if (BaseMaterial)
		{
			for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
			{
				for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
				{
					FMaterialResource* CurrentResource = BaseMaterial->MaterialResources[QualityLevelIndex][FeatureLevelIndex];
					FMaterialShaderMap* ShaderMap = CurrentResource->GetGameThreadShaderMap();

					if (ShaderMap && !ShaderMapToSerializedShaderData.Contains(ShaderMap))
					{
						TArray<uint8>* ShaderData = ShaderMap->BackupShadersToMemory();
						ShaderMapToSerializedShaderData.Emplace(ShaderMap, ShaderData);
					}
				}
			}
		}
	}

	// Process FMaterialShaderMap's referenced by the editor
	FMaterial::BackupEditorLoadedMaterialShadersToMemory(ShaderMapToSerializedShaderData);
}

void UMaterial::RestoreMaterialShadersFromMemory(EShaderPlatform ShaderPlatform, const TMap<FMaterialShaderMap*, TScopedPointer<TArray<uint8> > >& ShaderMapToSerializedShaderData)
{
	// Process FMaterialShaderMap's referenced by UObjects (UMaterial, UMaterialInstance)
	for (TObjectIterator<UMaterialInterface> It; It; ++It)
	{
		UMaterialInterface* Material = *It;
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
		UMaterial* BaseMaterial = Cast<UMaterial>(Material);

		if (MaterialInstance)
		{
			if (MaterialInstance->bHasStaticPermutationResource)
			{
				TArray<FMaterialShaderMap*> MIShaderMaps;
				MaterialInstance->GetAllShaderMaps(MIShaderMaps);

				for (int32 ShaderMapIndex = 0; ShaderMapIndex < MIShaderMaps.Num(); ShaderMapIndex++)
				{
					FMaterialShaderMap* ShaderMap = MIShaderMaps[ShaderMapIndex];

					if (ShaderMap)
					{
						const TScopedPointer<TArray<uint8> >* ShaderData = ShaderMapToSerializedShaderData.Find(ShaderMap);

						if (ShaderData)
						{
							ShaderMap->RestoreShadersFromMemory(**ShaderData);
						}
					}
				}
			}
		}
		else if (BaseMaterial)
		{
			for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
			{
				for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
				{
					FMaterialResource* CurrentResource = BaseMaterial->MaterialResources[QualityLevelIndex][FeatureLevelIndex];
					FMaterialShaderMap* ShaderMap = CurrentResource->GetGameThreadShaderMap();

					if (ShaderMap)
					{
						const TScopedPointer<TArray<uint8> >* ShaderData = ShaderMapToSerializedShaderData.Find(ShaderMap);

						if (ShaderData)
						{
							ShaderMap->RestoreShadersFromMemory(**ShaderData);
						}
					}
				}
			}
		}
	}

	// Process FMaterialShaderMap's referenced by the editor
	FMaterial::RestoreEditorLoadedMaterialShadersFromMemory(ShaderMapToSerializedShaderData);
}

void UMaterial::CompileMaterialsForRemoteRecompile(
	const TArray<UMaterialInterface*>& MaterialsToCompile,
	EShaderPlatform ShaderPlatform, 
	TMap<FString, TArray<TRefCountPtr<FMaterialShaderMap> > >& OutShaderMaps)
{
	// Build a map from UMaterial / UMaterialInstance to the resources which are being compiled
	TMap<FString, TArray<FMaterialResource*> > CompilingResources;

	// compile the requested materials
	for (int32 Index = 0; Index < MaterialsToCompile.Num(); Index++)
	{
		// get the material resource from the UMaterialInterface
		UMaterialInterface* Material = MaterialsToCompile[Index];
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
		UMaterial* BaseMaterial = Cast<UMaterial>(Material);

		if (MaterialInstance && MaterialInstance->bHasStaticPermutationResource)
		{
			TArray<FMaterialResource*>& ResourceArray = CompilingResources.Add(Material->GetPathName(), TArray<FMaterialResource*>());
			MaterialInstance->CacheResourceShadersForCooking(ShaderPlatform, ResourceArray);
		}
		else if (BaseMaterial)
		{
			TArray<FMaterialResource*>& ResourceArray = CompilingResources.Add(Material->GetPathName(), TArray<FMaterialResource*>());
			BaseMaterial->CacheResourceShadersForCooking(ShaderPlatform, ResourceArray);
		}
	}

	// Wait until all compilation is finished and all of the gathered FMaterialResources have their GameThreadShaderMap up to date
	GShaderCompilingManager->FinishAllCompilation();

	for(TMap<FString, TArray<FMaterialResource*> >::TIterator It(CompilingResources); It; ++It)
	{
		TArray<FMaterialResource*>& ResourceArray = It.Value();
		TArray<TRefCountPtr<FMaterialShaderMap> >& OutShaderMapArray = OutShaderMaps.Add(It.Key(), TArray<TRefCountPtr<FMaterialShaderMap> >());

		for (int32 Index = 0; Index < ResourceArray.Num(); Index++)
		{
			FMaterialResource* CurrentResource = ResourceArray[Index];
			OutShaderMapArray.Add(CurrentResource->GetGameThreadShaderMap());
			delete CurrentResource;
		}
	}
}

bool UMaterial::UpdateLightmassTextureTracking()
{
	bool bTexturesHaveChanged = false;
#if WITH_EDITORONLY_DATA
	TArray<UTexture*> UsedTextures;
	
	GetUsedTextures(UsedTextures, EMaterialQualityLevel::Num, true);
	if (UsedTextures.Num() != ReferencedTextureGuids.Num())
	{
		bTexturesHaveChanged = true;
		// Just clear out all the guids and the code below will
		// fill them back in...
		ReferencedTextureGuids.Empty(UsedTextures.Num());
		ReferencedTextureGuids.AddZeroed(UsedTextures.Num());
	}
	
	for (int32 CheckIdx = 0; CheckIdx < UsedTextures.Num(); CheckIdx++)
	{
		UTexture* Texture = UsedTextures[CheckIdx];
		if (Texture)
		{
			if (ReferencedTextureGuids[CheckIdx] != Texture->GetLightingGuid())
			{
				ReferencedTextureGuids[CheckIdx] = Texture->GetLightingGuid();
				bTexturesHaveChanged = true;
			}
		}
		else
		{
			if (ReferencedTextureGuids[CheckIdx] != FGuid(0,0,0,0))
			{
				ReferencedTextureGuids[CheckIdx] = FGuid(0,0,0,0);
				bTexturesHaveChanged = true;
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	if ( bTexturesHaveChanged )
	{
		// This will invalidate any cached Lightmass material exports
		SetLightingGuid();
	}

	return bTexturesHaveChanged;
}


FExpressionInput* UMaterial::GetExpressionInputForProperty(EMaterialProperty InProperty)
{
	switch (InProperty)
	{
	case MP_EmissiveColor:
		return &EmissiveColor;
	case MP_Opacity:
		return &Opacity;
	case MP_OpacityMask:
		return &OpacityMask;
	case MP_DiffuseColor:
		return &DiffuseColor;
	case MP_SpecularColor:
		return &SpecularColor;
	case MP_BaseColor:
		return &BaseColor;
	case MP_Metallic:
		return &Metallic;
	case MP_Specular:
		return &Specular;
	case MP_Roughness:
		return &Roughness;
	case MP_Normal:
		return &Normal;
	case MP_WorldPositionOffset:
		return &WorldPositionOffset;
	case MP_WorldDisplacement:
		return &WorldDisplacement;
	case MP_TessellationMultiplier:
		return &TessellationMultiplier;
	case MP_SubsurfaceColor:
		return &SubsurfaceColor;
	case MP_AmbientOcclusion:
		return &AmbientOcclusion;
	case MP_Refraction:
		return &Refraction;
	case MP_MaterialAttributes: 
		return &MaterialAttributes;
	}

	if (InProperty >= MP_CustomizedUVs0 && InProperty <= MP_CustomizedUVs7)
	{
		return &CustomizedUVs[InProperty - MP_CustomizedUVs0];
	}

	return NULL;
}


bool UMaterial::GetAllReferencedExpressions(TArray<UMaterialExpression*>& OutExpressions, class FStaticParameterSet* InStaticParameterSet)
{
	OutExpressions.Empty();

	for (int32 MPIdx = 0; MPIdx < MP_MAX; MPIdx++)
	{
		EMaterialProperty MaterialProp = EMaterialProperty(MPIdx);
		TArray<UMaterialExpression*> MPRefdExpressions;
		if (GetExpressionsInPropertyChain(MaterialProp, MPRefdExpressions, InStaticParameterSet) == true)
		{
			for (int32 AddIdx = 0; AddIdx < MPRefdExpressions.Num(); AddIdx++)
			{
				OutExpressions.AddUnique(MPRefdExpressions[AddIdx]);
			}
		}
	}

	return true;
}


bool UMaterial::GetExpressionsInPropertyChain(EMaterialProperty InProperty, 
	TArray<UMaterialExpression*>& OutExpressions, class FStaticParameterSet* InStaticParameterSet)
{
	OutExpressions.Empty();
	FExpressionInput* StartingExpression = GetExpressionInputForProperty(InProperty);

	if (StartingExpression == NULL)
	{
		// Failed to find the starting expression
		return false;
	}

	TArray<FExpressionInput*> ProcessedInputs;
	if (StartingExpression->Expression)
	{
		ProcessedInputs.AddUnique(StartingExpression);
		RecursiveGetExpressionChain(StartingExpression->Expression, ProcessedInputs, OutExpressions, InStaticParameterSet);
	}
	return true;
}


bool UMaterial::GetTexturesInPropertyChain(EMaterialProperty InProperty, TArray<UTexture*>& OutTextures,  
	TArray<FName>* OutTextureParamNames, class FStaticParameterSet* InStaticParameterSet)
{
	TArray<UMaterialExpression*> ChainExpressions;
	if (GetExpressionsInPropertyChain(InProperty, ChainExpressions, InStaticParameterSet) == true)
	{
		// Extract the texture and texture parameter expressions...
		for (int32 ExpressionIdx = 0; ExpressionIdx < ChainExpressions.Num(); ExpressionIdx++)
		{
			UMaterialExpression* MatExp = ChainExpressions[ExpressionIdx];
			if (MatExp != NULL)
			{
				// Is it a texture sample or texture parameter sample?
				UMaterialExpressionTextureSample* TextureSampleExp = Cast<UMaterialExpressionTextureSample>(MatExp);
				if (TextureSampleExp != NULL)
				{
					// Check the default texture...
					if (TextureSampleExp->Texture != NULL)
					{
						OutTextures.Add(TextureSampleExp->Texture);
					}

					if (OutTextureParamNames != NULL)
					{
						// If the expression is a parameter, add it's name to the texture names array
						UMaterialExpressionTextureSampleParameter* TextureSampleParamExp = Cast<UMaterialExpressionTextureSampleParameter>(MatExp);
						if (TextureSampleParamExp != NULL)
						{
							OutTextureParamNames->AddUnique(TextureSampleParamExp->ParameterName);
						}
					}
				}
			}
		}
	
		return true;
	}

	return false;
}


bool UMaterial::RecursiveGetExpressionChain(UMaterialExpression* InExpression, TArray<FExpressionInput*>& InOutProcessedInputs, 
	TArray<UMaterialExpression*>& OutExpressions, class FStaticParameterSet* InStaticParameterSet)
{
	OutExpressions.AddUnique(InExpression);
	TArray<FExpressionInput*> Inputs = InExpression->GetInputs();
	for (int32 InputIdx = 0; InputIdx < Inputs.Num(); InputIdx++)
	{
		FExpressionInput* InnerInput = Inputs[InputIdx];
		if (InnerInput != NULL)
		{
			int32 DummyIdx;
			if (InOutProcessedInputs.Find(InnerInput,DummyIdx) == false)
			{
				if (InnerInput->Expression)
				{
					bool bProcessInput = true;
					if (InStaticParameterSet != NULL)
					{
						// By default, static switches use B...
						// Is this a static switch parameter?
						//@todo. Handle Terrain weight map layer expression here as well!
						UMaterialExpressionStaticSwitchParameter* StaticSwitchExp = Cast<UMaterialExpressionStaticSwitchParameter>(InExpression);
						if (StaticSwitchExp != NULL)
						{
							bool bUseInputA = StaticSwitchExp->DefaultValue;
							FName StaticSwitchExpName = StaticSwitchExp->ParameterName;
							for (int32 CheckIdx = 0; CheckIdx < InStaticParameterSet->StaticSwitchParameters.Num(); CheckIdx++)
							{
								FStaticSwitchParameter& SwitchParam = InStaticParameterSet->StaticSwitchParameters[CheckIdx];
								if (SwitchParam.ParameterName == StaticSwitchExpName)
								{
									// Found it...
									if (SwitchParam.bOverride == true)
									{
										bUseInputA = SwitchParam.Value;
										break;
									}
								}
							}

							if (bUseInputA == true)
							{
								if (InnerInput->Expression != StaticSwitchExp->A.Expression)
								{
									bProcessInput = false;
								}
							}
							else
							{
								if (InnerInput->Expression != StaticSwitchExp->B.Expression)
								{
									bProcessInput = false;
								}
							}
						}
					}

					if (bProcessInput == true)
					{
						InOutProcessedInputs.Add(InnerInput);
						RecursiveGetExpressionChain(InnerInput->Expression, InOutProcessedInputs, OutExpressions, InStaticParameterSet);
					}
				}
			}
		}
	}

	return true;
}

void UMaterial::AppendReferencedTextures(TArray<UTexture*>& InOutTextures) const
{
	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* Expression = Expressions[ExpressionIndex];
		UMaterialExpressionMaterialFunctionCall* MaterialFunctionNode = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);

		if (MaterialFunctionNode && MaterialFunctionNode->MaterialFunction)
		{
			TArray<UMaterialFunction*> Functions;
			Functions.Add(MaterialFunctionNode->MaterialFunction);

			MaterialFunctionNode->MaterialFunction->GetDependentFunctions(Functions);

			// Handle nested functions
			for (int32 FunctionIndex = 0; FunctionIndex < Functions.Num(); FunctionIndex++)
			{
				UMaterialFunction* CurrentFunction = Functions[FunctionIndex];
				CurrentFunction->AppendReferencedTextures(InOutTextures);
			}
		}
		else
		{
			UTexture* ReferencedTexture = Expression->GetReferencedTexture();

			if (ReferencedTexture)
			{
				InOutTextures.AddUnique(ReferencedTexture);
			}
		}
	}
}

void UMaterial::RecursiveUpdateRealtimePreview( UMaterialExpression* InExpression, TArray<UMaterialExpression*>& InOutExpressionsToProcess )
{
	// remove ourselves from the list to process
	InOutExpressionsToProcess.Remove(InExpression);

	bool bOldRealtimePreview = InExpression->bRealtimePreview;

	// See if we know ourselves if we need realtime preview or not.
	InExpression->bRealtimePreview = InExpression->NeedsRealtimePreview();

	if( InExpression->bRealtimePreview )
	{
		if( InExpression->bRealtimePreview != bOldRealtimePreview )
		{
			InExpression->bNeedToUpdatePreview = true;
		}

		return;		
	}

	// We need to examine our inputs. If any of them need realtime preview, so do we.
	TArray<FExpressionInput*> Inputs = InExpression->GetInputs();
	for (int32 InputIdx = 0; InputIdx < Inputs.Num(); InputIdx++)
	{
		FExpressionInput* InnerInput = Inputs[InputIdx];
		if (InnerInput != NULL && InnerInput->Expression != NULL)
		{
			// See if we still need to process this expression, and if so do that first.
			if (InOutExpressionsToProcess.Find(InnerInput->Expression) != INDEX_NONE)
			{
				RecursiveUpdateRealtimePreview(InnerInput->Expression, InOutExpressionsToProcess);
			}

			// If our input expression needed realtime preview, we do too.
			if( InnerInput->Expression->bRealtimePreview )
			{

				InExpression->bRealtimePreview = true;
				if( InExpression->bRealtimePreview != bOldRealtimePreview )
				{
					InExpression->bNeedToUpdatePreview = true;
				}
				return;		
			}
		}
	}

	if( InExpression->bRealtimePreview != bOldRealtimePreview )
	{
		InExpression->bNeedToUpdatePreview = true;
	}
}

void UMaterial::GetReferencedFunctionIds(TArray<FGuid>& Ids) const
{
	Ids.Reset();

	for (int32 FunctionIndex = 0; FunctionIndex < MaterialFunctionInfos.Num(); FunctionIndex++)
	{
		Ids.AddUnique(MaterialFunctionInfos[FunctionIndex].StateId);
	}
}

void UMaterial::GetReferencedParameterCollectionIds(TArray<FGuid>& Ids) const
{
	Ids.Reset();

	for (int32 CollectionIndex = 0; CollectionIndex < MaterialParameterCollectionInfos.Num(); CollectionIndex++)
	{
		Ids.AddUnique(MaterialParameterCollectionInfos[CollectionIndex].StateId);
	}
}

int32 UMaterial::CompileProperty( FMaterialCompiler* Compiler, EMaterialProperty Property, float DefaultFloat, FLinearColor DefaultColor, const FVector4& DefaultVector )
{
	int32 Ret = INDEX_NONE;

	if( bUseMaterialAttributes && MP_DiffuseColor != Property && MP_SpecularColor != Property )
	{
		Ret = MaterialAttributes.Compile(Compiler,Property, DefaultFloat, DefaultColor, DefaultVector);
	}
	else
	{
		switch(Property)
		{
		case MP_Opacity: Ret = Opacity.Compile(Compiler,DefaultFloat); break;
		case MP_OpacityMask: Ret = OpacityMask.Compile(Compiler,DefaultFloat); break;
		case MP_Metallic: Ret = Metallic.Compile(Compiler,DefaultFloat); break;
		case MP_Specular: Ret = Specular.Compile(Compiler,DefaultFloat); break;
		case MP_Roughness: Ret = Roughness.Compile(Compiler,DefaultFloat); break;
		case MP_TessellationMultiplier: Ret = TessellationMultiplier.Compile(Compiler,DefaultFloat); break;
		case MP_AmbientOcclusion: Ret = AmbientOcclusion.Compile(Compiler,DefaultFloat); break;
		case MP_Refraction: 
			Ret = Compiler->AppendVector( 
					Compiler->ForceCast(Refraction.Compile(Compiler,DefaultFloat), MCT_Float1), 
					Compiler->ForceCast(Compiler->ScalarParameter(FName(TEXT("RefractionDepthBias")), Compiler->GetRefractionDepthBiasValue()), MCT_Float1) 
					);
			break;
		case MP_EmissiveColor: Ret = EmissiveColor.Compile(Compiler,DefaultColor); break;
		case MP_DiffuseColor: Ret = DiffuseColor.Compile(Compiler,DefaultColor); break;
		case MP_SpecularColor: Ret = SpecularColor.Compile(Compiler,DefaultColor); break;
		case MP_BaseColor: Ret = BaseColor.Compile(Compiler,DefaultColor); break;
		case MP_SubsurfaceColor: Ret = SubsurfaceColor.Compile(Compiler,DefaultColor); break;
		case MP_Normal: Ret = Normal.Compile(Compiler,DefaultVector); break;
		case MP_WorldPositionOffset: Ret = WorldPositionOffset.Compile(Compiler,DefaultVector); break;
		case MP_WorldDisplacement: Ret = WorldDisplacement.Compile(Compiler,DefaultVector); break;
		};

			if (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7)
			{
				const int32 TextureCoordinateIndex = Property - MP_CustomizedUVs0;

				if (CustomizedUVs[TextureCoordinateIndex].Expression && TextureCoordinateIndex < NumCustomizedUVs)
				{
					Ret = CustomizedUVs[TextureCoordinateIndex].Compile(Compiler,FVector2D(DefaultVector.X, DefaultVector.Y));
				}
				else
				{
					// The user did not customize this UV, pass through the vertex texture coordinates
					Ret = Compiler->TextureCoordinate(TextureCoordinateIndex, false, false);
				}
			}
	}
	return Ret;
}

void UMaterial::NotifyCompilationFinished(FMaterialResource* CompiledResource)
{
	// we don't know if it was actually us or one of our MaterialInstances (with StaticPermutationResources)...
	UMaterial::OnMaterialCompilationFinished().Broadcast(this);
}

void UMaterial::ForceRecompileForRendering()
{
	CacheResourceShadersForRendering( false );
}

UMaterial::FMaterialCompilationFinished UMaterial::MaterialCompilationFinishedEvent;
UMaterial::FMaterialCompilationFinished& UMaterial::OnMaterialCompilationFinished()
{
	return MaterialCompilationFinishedEvent;
}

void UMaterial::AllMaterialsCacheResourceShadersForRendering()
{
	for (TObjectIterator<UMaterial> It; It; ++It)
	{
		UMaterial* Material = *It;

		Material->CacheResourceShadersForRendering(false);
	}
}

/**
 * Lists all materials that read from scene color.
 */
static void ListSceneColorMaterials()
{
	int32 NumSceneColorMaterials = 0;
	for (TObjectIterator<UMaterialInterface> It; It; ++It)
	{
		UMaterialInterface* Mat = *It;
		const FMaterial* MatRes = Mat->GetRenderProxy(false)->GetMaterial(GRHIFeatureLevel);
		if (MatRes && MatRes->UsesSceneColor())
		{
			UMaterial* BaseMat = Mat->GetMaterial();
			UE_LOG(LogConsoleResponse,Display,TEXT("[SepTrans=%d] %s"),
				BaseMat ? BaseMat->bEnableSeparateTranslucency : 3,
				*Mat->GetPathName()
				);
			NumSceneColorMaterials++;
		}
	}
	UE_LOG(LogConsoleResponse,Display,TEXT("%d loaded materials read from scene color."),NumSceneColorMaterials);
}

static FAutoConsoleCommand CmdListSceneColorMaterials(
	TEXT("r.ListSceneColorMaterials"),
	TEXT("Lists all materials that read from scene color."),
	FConsoleCommandDelegate::CreateStatic(ListSceneColorMaterials)
	);

float UMaterial::GetOpacityMaskClipValue_Internal() const
{
	return OpacityMaskClipValue;
}

EBlendMode UMaterial::GetBlendMode_Internal() const
{
	return BlendMode;
}

EMaterialLightingModel UMaterial::GetLightingModel_Internal() const
{
	switch (MaterialDomain)
	{
		case MD_Surface:
		case MD_DeferredDecal:
			return LightingModel;

		// Post process and light function materials must be rendered with the unlit model.
		case MD_PostProcess:
		case MD_LightFunction:
			return MLM_Unlit;

		default:
			checkNoEntry();
			return MLM_Unlit;
	}
}

bool UMaterial::IsTwoSided_Internal() const
{
	return TwoSided != 0;
}

bool UMaterial::IsPropertyActive(EMaterialProperty InProperty)const 
{
	if(MaterialDomain == MD_PostProcess)
	{
		return InProperty == MP_EmissiveColor;
	}
	else if(MaterialDomain == MD_LightFunction)
	{
		// light functions should already use MLM_Unlit but we also we don't want WorldPosOffset
		return InProperty == MP_EmissiveColor;
	}
	else if(MaterialDomain == MD_DeferredDecal)
	{
		if(InProperty >= MP_CustomizedUVs0 )
		{
			return true;
		}

		switch(DecalBlendMode)
		{
			case DBM_Translucent:
				return InProperty == MP_EmissiveColor
					|| InProperty == MP_Normal
					|| InProperty == MP_Metallic
					|| InProperty == MP_Specular
					|| InProperty == MP_BaseColor
					|| InProperty == MP_Roughness
					|| InProperty == MP_Opacity;

			case DBM_Stain:
				return InProperty == MP_EmissiveColor
					|| InProperty == MP_Normal
					|| InProperty == MP_Metallic
					|| InProperty == MP_Specular
					|| InProperty == MP_BaseColor
					|| InProperty == MP_Roughness
					|| InProperty == MP_Opacity;

			case DBM_Normal:
				return InProperty == MP_Normal
					|| InProperty == MP_Opacity;

			case DBM_Emissive:
				// even emissive supports opacity
				return InProperty == MP_EmissiveColor
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_ColorNormalRoughness:
				return InProperty == MP_Normal
					|| InProperty == MP_DiffuseColor
					|| InProperty == MP_BaseColor
					|| InProperty == MP_Roughness
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_Color:
				return InProperty == MP_DiffuseColor
					|| InProperty == MP_BaseColor
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_ColorNormal:
				return InProperty == MP_DiffuseColor
					|| InProperty == MP_BaseColor
					|| InProperty == MP_Normal
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_ColorRoughness:
				return InProperty == MP_DiffuseColor
					|| InProperty == MP_BaseColor
					|| InProperty == MP_Roughness
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_NormalRoughness:
				return InProperty == MP_Normal
					|| InProperty == MP_Roughness
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_Normal:
				return InProperty == MP_Normal
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_Roughness:
				return InProperty == MP_Roughness
					|| InProperty == MP_Opacity;

			default:
				// if you create a new mode it needs to expose the right pins
				return false;
		}
	}

	bool Active = true;

	switch (InProperty)
	{
	case MP_Refraction:
		Active = IsTranslucentBlendMode((EBlendMode)BlendMode) && BlendMode != BLEND_Modulate;
		break;
	case MP_Opacity:
		Active = IsTranslucentBlendMode((EBlendMode)BlendMode) && BlendMode != BLEND_Modulate;
		if(LightingModel == MLM_Subsurface || LightingModel == MLM_PreintegratedSkin)
		{
			Active = true;
		}
		break;
	case MP_OpacityMask:
		Active = BlendMode == BLEND_Masked;
		break;
	case MP_DiffuseColor:
	case MP_SpecularColor:
	case MP_BaseColor:
	case MP_Metallic:
	case MP_Specular:
	case MP_Roughness:
	case MP_AmbientOcclusion:
		Active = LightingModel != MLM_Unlit;
		break;
	case MP_Normal:
		Active = LightingModel != MLM_Unlit || Refraction.IsConnected();
		break;
	case MP_SubsurfaceColor:
		Active = LightingModel == MLM_Subsurface || LightingModel == MLM_PreintegratedSkin;
		break;
	case MP_TessellationMultiplier:
	case MP_WorldDisplacement:
		Active = D3D11TessellationMode != MTM_NoTessellation;
		break;
	case MP_EmissiveColor:
		// Emissive is always active, even for light functions and post process materials
		Active = true;
		break;
	case MP_WorldPositionOffset:
	case MP_MaterialAttributes:
	default:
		Active = true;
		break;
	}
	return Active;
}

#if WITH_EDITORONLY_DATA
void UMaterial::FlipExpressionPositions(const TArray<UMaterialExpression*>& Expressions, const TArray<UMaterialExpressionComment*>& Comments, bool bScaleCoords, UMaterial* InMaterial)
{
	// Rough estimate of average increase in node size for the new editor
	const float PosScaling = bScaleCoords ? 1.25f : 1.0f;

	if (InMaterial)
	{
		InMaterial->EditorX = -InMaterial->EditorX;
	}
	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* Expression = Expressions[ExpressionIndex];
		Expression->MaterialExpressionEditorX = -Expression->MaterialExpressionEditorX * PosScaling;
		Expression->MaterialExpressionEditorY *= PosScaling;
	}
	for (int32 ExpressionIndex = 0; ExpressionIndex < Comments.Num(); ExpressionIndex++)
	{
		UMaterialExpressionComment* Comment = Comments[ExpressionIndex];
		Comment->MaterialExpressionEditorX = -Comment->MaterialExpressionEditorX * PosScaling - Comment->SizeX;
		Comment->MaterialExpressionEditorY *= PosScaling;
		Comment->SizeX *= PosScaling;
		Comment->SizeY *= PosScaling;
	}
}

bool UMaterial::HasFlippedCoordinates()
{
	uint32 ReversedInputCount = 0;
	uint32 StandardInputCount = 0;

	// Check inputs to see if they are right of the root node
	for (int32 InputIndex = 0; InputIndex < MP_MAX; InputIndex++)
	{
		FExpressionInput* Input = GetExpressionInputForProperty((EMaterialProperty)InputIndex);
		if (Input->Expression)
		{
			if (Input->Expression->MaterialExpressionEditorX > EditorX)
			{
				++ReversedInputCount;
			}
			else
			{
				++StandardInputCount;
			}
		}
	}

	// Can't be sure coords are flipped if most are set out correctly
	return ReversedInputCount > StandardInputCount;
}
#endif //WITH_EDITORONLY_DATA
