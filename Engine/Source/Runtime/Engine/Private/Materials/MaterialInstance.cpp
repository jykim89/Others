// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "MaterialInstance.h"
#include "MaterialShader.h"
#include "TargetPlatform.h"

/**
 * Cache uniform expressions for the given material.
 * @param MaterialInstance - The material instance for which to cache uniform expressions.
 */
void CacheMaterialInstanceUniformExpressions(const UMaterialInstance* MaterialInstance)
{
	// Only cache the unselected + unhovered material instance. Selection color
	// can change at runtime and would invalidate the parameter cache.
	if (MaterialInstance->Resources[0])
	{
		MaterialInstance->Resources[0]->CacheUniformExpressions_GameThread();
	}
}

/**
 * Recaches uniform expressions for all material instances with a given parent.
 * WARNING: This function is a noop outside of the Editor!
 * @param ParentMaterial - The parent material to look for.
 */
void RecacheMaterialInstanceUniformExpressions(const UMaterialInterface* ParentMaterial)
{
	if (GIsEditor)
	{
		UE_LOG(LogMaterial,Verbose,TEXT("Recaching MI Uniform Expressions for parent %s"), *ParentMaterial->GetFullName());
		TArray<FMICReentranceGuard> ReentranceGuards;
		for (TObjectIterator<UMaterialInstance> It; It; ++It)
		{
			UMaterialInstance* MaterialInstance = *It;
			do 
			{
				if (MaterialInstance->Parent == ParentMaterial)
				{
					UE_LOG(LogMaterial,Verbose,TEXT("--> %s"), *MaterialInstance->GetFullName());
					CacheMaterialInstanceUniformExpressions(*It);
					break;
				}
				new (ReentranceGuards) FMICReentranceGuard(MaterialInstance);
				MaterialInstance = Cast<UMaterialInstance>(MaterialInstance->Parent);
			} while (MaterialInstance && !MaterialInstance->ReentrantFlag);
			ReentranceGuards.Reset();
		}
	}
}

/**
 * This function takes a array of parameter structs and attempts to establish a reference to the expression object each parameter represents.
 * If a reference exists, the function checks to see if the parameter has been renamed.
 *
 * @param Parameters		Array of parameters to operate on.
 * @param ParentMaterial	Parent material to search in for expressions.
 *
 * @return Returns whether or not any of the parameters was changed.
 */
template <typename ParameterType, typename ExpressionType>
bool UpdateParameterSet(TArray<ParameterType> &Parameters, UMaterial* ParentMaterial)
{
	bool bChanged = false;

	// Loop through all of the parameters and try to either establish a reference to the 
	// expression the parameter represents, or check to see if the parameter's name has changed.
	for(int32 ParameterIdx=0; ParameterIdx<Parameters.Num(); ParameterIdx++)
	{
		bool bTryToFindByName = true;

		ParameterType &Parameter = Parameters[ParameterIdx];

		if(Parameter.ExpressionGUID.IsValid())
		{
			ExpressionType* Expression = ParentMaterial->FindExpressionByGUID<ExpressionType>(Parameter.ExpressionGUID);

			// Check to see if the parameter name was changed.
			if(Expression)
			{
				bTryToFindByName = false;

				if(Parameter.ParameterName != Expression->ParameterName)
				{
					Parameter.ParameterName = Expression->ParameterName;
					bChanged = true;
				}
			}
		}

		// No reference to the material expression exists, so try to find one in the material expression's array if we are in the editor.
		if(bTryToFindByName && GIsEditor && !FApp::IsGame())
		{
			for(int32 ExpressionIndex = 0;ExpressionIndex < ParentMaterial->Expressions.Num();ExpressionIndex++)
			{
				ExpressionType* ParameterExpression = Cast<ExpressionType>(ParentMaterial->Expressions[ExpressionIndex]);

				if(ParameterExpression && ParameterExpression->ParameterName == Parameter.ParameterName)
				{
					Parameter.ExpressionGUID = ParameterExpression->ExpressionGUID;
					bChanged = true;
					break;
				}
			}
		}
	}

	return bChanged;
}

FMaterialInstanceResource::FMaterialInstanceResource(UMaterialInstance* InOwner,bool bInSelected,bool bInHovered)
	: FMaterialRenderProxy(bInSelected, bInHovered)
	, Parent(NULL)
	, Owner(InOwner)
	, DistanceFieldPenumbraScale(1.0f)
	, GameThreadParent(NULL)
{
}

const FMaterial* FMaterialInstanceResource::GetMaterial(ERHIFeatureLevel::Type FeatureLevel) const
{
	checkSlow(IsInRenderingThread());

	if (Owner->bHasStaticPermutationResource)
	{
		EMaterialQualityLevel::Type ActiveQualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
		FMaterialResource* StaticPermutationResource = Owner->StaticPermutationMaterialResources[ActiveQualityLevel][FeatureLevel];

		if (StaticPermutationResource->GetRenderingThreadShaderMap())
		{
			// Verify that compilation has been finalized, the rendering thread shouldn't be touching it otherwise
			checkSlow(StaticPermutationResource->GetRenderingThreadShaderMap()->IsCompilationFinalized());
			// The shader map reference should have been NULL'ed if it did not compile successfully
			checkSlow(StaticPermutationResource->GetRenderingThreadShaderMap()->CompiledSuccessfully());
			return StaticPermutationResource;
		}
		else
		{
			EMaterialDomain Domain = (EMaterialDomain)StaticPermutationResource->GetMaterialDomain();
			UMaterial* FallbackMaterial = UMaterial::GetDefaultMaterial(Domain);
			//there was an error, use the default material's resource
			return FallbackMaterial->GetRenderProxy(IsSelected(), IsHovered())->GetMaterial(FeatureLevel);
		}
	}
	else if (Parent)
	{
		//use the parent's material resource
		return Parent->GetRenderProxy(IsSelected(), IsHovered())->GetMaterial(FeatureLevel);
	}
	else
	{
		UMaterial* FallbackMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		return FallbackMaterial->GetRenderProxy(IsSelected(), IsHovered())->GetMaterial(FeatureLevel);
	}
}

FMaterial* FMaterialInstanceResource::GetMaterialNoFallback(ERHIFeatureLevel::Type FeatureLevel) const
{
	checkSlow(IsInRenderingThread());

	if (Owner->bHasStaticPermutationResource)
	{
		EMaterialQualityLevel::Type ActiveQualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
		FMaterialResource* StaticPermutationResource = Owner->StaticPermutationMaterialResources[ActiveQualityLevel][FeatureLevel];
		return StaticPermutationResource;
	}
	else
	{
		if (Parent)
		{
			FMaterialRenderProxy* ParentProxy = Parent->GetRenderProxy(IsSelected(), IsHovered());

			if (ParentProxy)
			{
				return ParentProxy->GetMaterialNoFallback(FeatureLevel);
			}
		}
		return NULL;
	}
}

bool FMaterialInstanceResource::GetScalarValue(
	const FName ParameterName, 
	float* OutValue,
	const FMaterialRenderContext& Context
	) const
{
	checkSlow(IsInRenderingThread());
	const float* Value = RenderThread_FindParameterByName<float>(ParameterName);
	if(Value)
	{
		*OutValue = *Value;
		return true;
	}
	else if(Parent)
	{
		return Parent->GetRenderProxy(IsSelected(),IsHovered())->GetScalarValue(ParameterName, OutValue, Context);
	}
	else
	{
		return false;
	}
}

bool FMaterialInstanceResource::GetVectorValue(
	const FName ParameterName, 
	FLinearColor* OutValue,
	const FMaterialRenderContext& Context
	) const
{
	checkSlow(IsInRenderingThread());
	const FLinearColor* Value = RenderThread_FindParameterByName<FLinearColor>(ParameterName);
	if(Value)
	{
		*OutValue = *Value;
		return true;
	}
	else if(Parent)
	{
		return Parent->GetRenderProxy(IsSelected(),IsHovered())->GetVectorValue(ParameterName, OutValue, Context);
	}
	else
	{
		return false;
	}
}

bool FMaterialInstanceResource::GetTextureValue(
	const FName ParameterName,
	const UTexture** OutValue,
	const FMaterialRenderContext& Context
	) const
{
	checkSlow(IsInRenderingThread());
	const UTexture* const * Value = RenderThread_FindParameterByName<const UTexture*>(ParameterName);
	if(Value && *Value)
	{
		*OutValue = *Value;
		return true;
	}
	else if(Parent)
	{
		return Parent->GetRenderProxy(IsSelected(),IsHovered())->GetTextureValue(ParameterName,OutValue,Context);
	}
	else
	{
		return false;
	}
}

/** Called from the game thread to update DistanceFieldPenumbraScale. */
void FMaterialInstanceResource::GameThread_UpdateDistanceFieldPenumbraScale(float NewDistanceFieldPenumbraScale)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		UpdateDistanceFieldPenumbraScaleCommand,
		float*,DistanceFieldPenumbraScale,&DistanceFieldPenumbraScale,
		float,NewDistanceFieldPenumbraScale,NewDistanceFieldPenumbraScale,
	{
		*DistanceFieldPenumbraScale = NewDistanceFieldPenumbraScale;
	});
}

void FMaterialInstanceResource::GameThread_SetParent(UMaterialInterface* InParent)
{
	check(IsInGameThread());

	if( GameThreadParent != InParent )
	{
		// Set the game thread accessible parent.
		UMaterialInterface* OldParent = GameThreadParent;
		GameThreadParent = InParent;

		// Set the rendering thread's parent and instance pointers.
		check(InParent != NULL);
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitMaterialInstanceResource,
			FMaterialInstanceResource*,Resource,this,
			UMaterialInterface*,Parent,InParent,
		{
			Resource->Parent = Parent;
			Resource->InvalidateUniformExpressionCache();
		});

		if( OldParent )
		{
			// make sure that the old parent sticks around until we've set the new parent on FMaterialInstanceResource
			OldParent->ParentRefFence.BeginFence();
		}
	}
}

ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER_DECLARE_TEMPLATE(
	SetMIParameterValue,ParameterType,
	const UMaterialInstance*,Instance,Instance,
	FName,ParameterName,Parameter.ParameterName,
	typename ParameterType::ValueType,Value,ParameterType::GetValue(Parameter),
{
	Instance->Resources[0]->RenderThread_UpdateParameter(ParameterName, Value);
	if (Instance->Resources[1])
	{
		Instance->Resources[1]->RenderThread_UpdateParameter(ParameterName, Value);
	}
	if (Instance->Resources[2])
	{
		Instance->Resources[2]->RenderThread_UpdateParameter(ParameterName, Value);
	}
});

/**
 * Updates a parameter on the material instance from the game thread.
 */
template <typename ParameterType>
void GameThread_UpdateMIParameter(const UMaterialInstance* Instance, const ParameterType& Parameter)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER_CREATE_TEMPLATE(
		SetMIParameterValue,ParameterType,
		const UMaterialInstance*,Instance,
		FName,Parameter.ParameterName,
		typename ParameterType::ValueType,ParameterType::GetValue(Parameter)
		);
}


bool UMaterialInstance::UpdateParameters()
{
	bool bDirty = false;
	if(IsTemplate(RF_ClassDefaultObject)==false)
	{
		// Get a pointer to the parent material.
		UMaterial* ParentMaterial = NULL;
		UMaterialInstance* ParentInst = this;
		while(ParentInst && ParentInst->Parent)
		{
			if(ParentInst->Parent->IsA(UMaterial::StaticClass()))
			{
				ParentMaterial = Cast<UMaterial>(ParentInst->Parent);
				break;
			}
			else
			{
				ParentInst = Cast<UMaterialInstance>(ParentInst->Parent);
			}
		}

		if(ParentMaterial)
		{
			// Scalar parameters
			bDirty = UpdateParameterSet<FScalarParameterValue, UMaterialExpressionScalarParameter>(ScalarParameterValues, ParentMaterial) || bDirty;

			// Vector parameters	
			bDirty = UpdateParameterSet<FVectorParameterValue, UMaterialExpressionVectorParameter>(VectorParameterValues, ParentMaterial) || bDirty;

			// Texture parameters
			bDirty = UpdateParameterSet<FTextureParameterValue, UMaterialExpressionTextureSampleParameter>(TextureParameterValues, ParentMaterial) || bDirty;

			// Font parameters
			bDirty = UpdateParameterSet<FFontParameterValue, UMaterialExpressionFontSampleParameter>(FontParameterValues, ParentMaterial) || bDirty;

			// Static switch parameters
			bDirty = UpdateParameterSet<FStaticSwitchParameter, UMaterialExpressionStaticBoolParameter>(StaticParameters.StaticSwitchParameters, ParentMaterial) || bDirty;

			// Static component mask parameters
			bDirty = UpdateParameterSet<FStaticComponentMaskParameter, UMaterialExpressionStaticComponentMaskParameter>(StaticParameters.StaticComponentMaskParameters, ParentMaterial) || bDirty;

			bDirty = UpdateParameterSet<FStaticTerrainLayerWeightParameter, UMaterialExpressionLandscapeLayerWeight>(StaticParameters.TerrainLayerWeightParameters, ParentMaterial) || bDirty;
		}
	}
	return bDirty;
}

UMaterialInstance::UMaterialInstance(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bHasStaticPermutationResource = false;
	bOverrideBaseProperties = false;
}

void UMaterialInstance::PostInitProperties()	
{
	Super::PostInitProperties();

	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		Resources[0] = new FMaterialInstanceResource(this,false,false);
		if(GIsEditor)
		{
			Resources[1] = new FMaterialInstanceResource(this,true,false);
			Resources[2] = new FMaterialInstanceResource(this,false,true);
		}
	}
}

/**
 * Initializes MI parameters from the game thread.
 */
template <typename ParameterType>
void GameThread_InitMIParameters(const UMaterialInstance* Instance, const TArray<ParameterType>& Parameters)
{
	if (!Instance->HasAnyFlags(RF_ClassDefaultObject))
	{
		for (int32 ParameterIndex = 0; ParameterIndex < Parameters.Num(); ParameterIndex++)
		{
			GameThread_UpdateMIParameter(Instance, Parameters[ParameterIndex]);
		}
	}
}

void UMaterialInstance::InitResources()
{
	// Find the instance's parent.
	UMaterialInterface* SafeParent = NULL;
	if(Parent)
	{
		SafeParent = Parent;
	}

	// Don't use the instance's parent if it has a circular dependency on the instance.
	if(SafeParent && SafeParent->IsDependent(this))
	{
		SafeParent = NULL;
	}

	// Don't allow MIDs as parents for material instances.
	if (SafeParent && SafeParent->IsA(UMaterialInstanceDynamic::StaticClass()))
	{
		SafeParent = NULL;
	}

	// If the instance doesn't have a valid parent, use the default material as the parent.
	if(!SafeParent)
	{
		SafeParent = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	checkf(SafeParent, TEXT("Invalid parent on %s"), *GetFullName());

	// Set the material instance's parent on its resources.
	for (int32 CurResourceIndex = 0; CurResourceIndex < ARRAY_COUNT( Resources ); ++CurResourceIndex)
	{	
		if (Resources[CurResourceIndex] != NULL)
		{
			Resources[CurResourceIndex]->GameThread_SetParent(SafeParent);
		}
	}

	GameThread_InitMIParameters(this, ScalarParameterValues);
	GameThread_InitMIParameters(this, VectorParameterValues);
	GameThread_InitMIParameters(this, TextureParameterValues);
	GameThread_InitMIParameters(this, FontParameterValues);
	CacheMaterialInstanceUniformExpressions(this);
}

const UMaterial* UMaterialInstance::GetMaterial() const
{
	check(IsInGameThread());
	if(ReentrantFlag)
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}

	FMICReentranceGuard	Guard(this);
	if(Parent)
	{
		return Parent->GetMaterial();
	}
	else
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}
}

const UMaterial* UMaterialInstance::GetMaterial_Concurrent(TMicRecursionGuard& RecursionGuard) const
{
	if(!Parent || RecursionGuard.Contains(this))
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}

	RecursionGuard.Add(this);
	return Parent->GetMaterial_Concurrent(RecursionGuard);
}

UMaterial* UMaterialInstance::GetMaterial()
{
	if(ReentrantFlag)
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}

	FMICReentranceGuard	Guard(this);
	if(Parent)
	{
		return Parent->GetMaterial();
	}
	else
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}
}

bool UMaterialInstance::GetVectorParameterValue(FName ParameterName, FLinearColor& OutValue) const
{
	bool bFoundAValue = false;

	if(ReentrantFlag)
	{
		return false;
	}

	const FVectorParameterValue* ParameterValue = GameThread_FindParameterByName(VectorParameterValues, ParameterName);
	if(ParameterValue)
	{
		OutValue = ParameterValue->ParameterValue;
		return true;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetVectorParameterValue(ParameterName,OutValue);
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::GetScalarParameterValue(FName ParameterName, float& OutValue) const
{
	bool bFoundAValue = false;

	if(ReentrantFlag)
	{
		return false;
	}

	const FScalarParameterValue* ParameterValue = GameThread_FindParameterByName(ScalarParameterValues,ParameterName);
	if(ParameterValue)
	{
		OutValue = ParameterValue->ParameterValue;
		return true;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetScalarParameterValue(ParameterName,OutValue);
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::GetTextureParameterValue(FName ParameterName, UTexture*& OutValue) const
{
	if(ReentrantFlag)
	{
		return false;
	}

	const FTextureParameterValue* ParameterValue = GameThread_FindParameterByName(TextureParameterValues,ParameterName);
	if(ParameterValue && ParameterValue->ParameterValue)
	{
		OutValue = ParameterValue->ParameterValue;
		return true;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetTextureParameterValue(ParameterName,OutValue);
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::GetFontParameterValue(FName ParameterName,class UFont*& OutFontValue, int32& OutFontPage) const
{
	if( ReentrantFlag )
	{
		return false;
	}

	const FFontParameterValue* ParameterValue = GameThread_FindParameterByName(FontParameterValues,ParameterName);
	if(ParameterValue && ParameterValue->FontValue)
	{
		OutFontValue = ParameterValue->FontValue;
		OutFontPage = ParameterValue->FontPage;
		return true;
	}
	else if( Parent )
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetFontParameterValue(ParameterName,OutFontValue,OutFontPage);
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::GetRefractionSettings(float& OutBiasValue) const
{
	bool bFoundAValue = false;

	FName ParamName;
	if( GetLinkerUE4Version() >= VER_UE4_REFRACTION_BIAS_TO_REFRACTION_DEPTH_BIAS )
	{
		ParamName = FName(TEXT("RefractionDepthBias"));
	}
	else
	{
		ParamName = FName(TEXT("RefractionBias"));
	}

	const FScalarParameterValue* BiasParameterValue = GameThread_FindParameterByName(ScalarParameterValues, ParamName);
	if(BiasParameterValue )
	{
		OutBiasValue = BiasParameterValue->ParameterValue;
		return true;
	}
	else if(Parent)
	{
		return Parent->GetRefractionSettings(OutBiasValue);
	}
	else
	{
		return false;
	}
}

void UMaterialInstance::GetTextureExpressionValues(const FMaterialResource* MaterialResource, TArray<UTexture*>& OutTextures) const
{
	const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2];

	ExpressionsByType[0] = &MaterialResource->GetUniform2DTextureExpressions();
	ExpressionsByType[1] = &MaterialResource->GetUniformCubeTextureExpressions();

	for(int32 TypeIndex = 0;TypeIndex < ARRAY_COUNT(ExpressionsByType);TypeIndex++)
	{
		const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& Expressions = *ExpressionsByType[TypeIndex];

		// Iterate over each of the material's texture expressions.
		for(int32 ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
		{
			FMaterialUniformExpressionTexture* Expression = Expressions[ExpressionIndex];

			// Evaluate the expression in terms of this material instance.
			UTexture* Texture = NULL;
			Expression->GetGameThreadTextureValue(this,*MaterialResource,Texture, true);
			OutTextures.AddUnique(Texture);
		}
	}
}

void UMaterialInstance::GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel, bool bAllQualityLevels) const
{
	OutTextures.Empty();

	// Do not care if we're running dedicated server
	if (!FPlatformProperties::IsServerOnly())
	{
		if (QualityLevel == EMaterialQualityLevel::Num)
		{
			QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
		}

		const UMaterialInstance* MaterialInstanceToUse = this;
		// Walk up the material instance chain to the first parent that has static parameters
		while (MaterialInstanceToUse && !MaterialInstanceToUse->bHasStaticPermutationResource)
		{
			MaterialInstanceToUse = Cast<const UMaterialInstance>(MaterialInstanceToUse->Parent);
		}

		// Use the uniform expressions from the lowest material instance with static parameters in the chain, if one exists
		if (MaterialInstanceToUse
			&& MaterialInstanceToUse->bHasStaticPermutationResource)
		{
			for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
			{
				const FMaterialResource* CurrentResource = MaterialInstanceToUse->StaticPermutationMaterialResources[QualityLevelIndex][GRHIFeatureLevel];

				//@todo - GetUsedTextures is incorrect during cooking since we don't cache shaders for the current platform during cooking
				if (QualityLevelIndex == QualityLevel || bAllQualityLevels)
				{
					GetTextureExpressionValues(CurrentResource, OutTextures);
				}
			}
		}
		else
		{
			// Use the uniform expressions from the base material
			const UMaterial* Material = GetMaterial();

			if (Material)
			{
				const FMaterialResource* MaterialResource = Material->GetMaterialResource(GRHIFeatureLevel, QualityLevel);
				GetTextureExpressionValues(MaterialResource, OutTextures);
			}
			else
			{
				// If the material instance has no material, use the default material.
				UMaterial::GetDefaultMaterial(MD_Surface)->GetUsedTextures(OutTextures, QualityLevel, bAllQualityLevels);
			}
		}
	}
}



void UMaterialInstance::OverrideTexture( const UTexture* InTextureToOverride, UTexture* OverrideTexture )
{
#if WITH_EDITOR
	bool bShouldRecacheMaterialExpressions = false;
	const bool bES2Preview = false;
	ERHIFeatureLevel::Type FeatureLevelsToUpdate[2] = { GRHIFeatureLevel, ERHIFeatureLevel::ES2 };
	int32 NumFeatureLevelsToUpdate = bES2Preview ? 2 : 1;
	
	for (int32 i = 0; i < NumFeatureLevelsToUpdate; ++i)
	{
		const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2];
		ERHIFeatureLevel::Type FeatureLevel = FeatureLevelsToUpdate[i];

		const FMaterialResource* SourceMaterialResource = NULL;
		if (bHasStaticPermutationResource)
		{
			SourceMaterialResource = GetMaterialResource(FeatureLevel);
			// Iterate over both the 2D textures and cube texture expressions.
			ExpressionsByType[0] = &SourceMaterialResource->GetUniform2DTextureExpressions();
			ExpressionsByType[1] = &SourceMaterialResource->GetUniformCubeTextureExpressions();
			SourceMaterialResource = SourceMaterialResource;
		}
		else
		{
			//@todo - this isn't handling chained MIC's correctly, where a parent in the chain has static parameters
			UMaterial* Material = GetMaterial();
			SourceMaterialResource = Material->GetMaterialResource(FeatureLevel);
			
			// Iterate over both the 2D textures and cube texture expressions.
			ExpressionsByType[0] = &SourceMaterialResource->GetUniform2DTextureExpressions();
			ExpressionsByType[1] = &SourceMaterialResource->GetUniformCubeTextureExpressions();
			SourceMaterialResource = SourceMaterialResource;
		}
		
		for(int32 TypeIndex = 0;TypeIndex < ARRAY_COUNT(ExpressionsByType);TypeIndex++)
		{
			const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& Expressions = *ExpressionsByType[TypeIndex];

			// Iterate over each of the material's texture expressions.
			for(int32 ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
			{
				FMaterialUniformExpressionTexture* Expression = Expressions[ExpressionIndex];

				// Evaluate the expression in terms of this material instance.
				const bool bAllowOverride = false;
				UTexture* Texture = NULL;
				Expression->GetGameThreadTextureValue(this,*SourceMaterialResource,Texture,bAllowOverride);

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

bool UMaterialInstance::CheckMaterialUsage(const EMaterialUsage Usage, const bool bSkipPrim)
{
	check(IsInGameThread());
	UMaterial* Material = GetMaterial();
	if(Material)
	{
		bool bNeedsRecompile = false;
		bool bUsageSetSuccessfully = Material->SetMaterialUsage(bNeedsRecompile, Usage, bSkipPrim);
		if (bNeedsRecompile)
		{
			CacheResourceShadersForRendering();
			MarkPackageDirty();
		}
		return bUsageSetSuccessfully;
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::CheckMaterialUsage_Concurrent(const EMaterialUsage Usage, const bool bSkipPrim) const
{
	TMicRecursionGuard RecursionGuard;
	UMaterial const* Material = GetMaterial_Concurrent(RecursionGuard);
	if(Material)
	{
		bool bUsageSetSuccessfully = false;
		if (Material->NeedsSetMaterialUsage_Concurrent(bUsageSetSuccessfully, Usage))
		{
			if (IsInGameThread())
			{
				bUsageSetSuccessfully = const_cast<UMaterialInstance*>(this)->CheckMaterialUsage(Usage, bSkipPrim);
			}
			else
			{
				struct FCallSMU
				{
					UMaterialInstance* Material;
					EMaterialUsage Usage;
					bool bSkipPrim;
					bool& bUsageSetSuccessfully;
					FScopedEvent& Event;

					FCallSMU(UMaterialInstance* InMaterial, EMaterialUsage InUsage, bool bInSkipPrim, bool& bInUsageSetSuccessfully, FScopedEvent& InEvent)
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
				FCallSMU CallSMU(const_cast<UMaterialInstance*>(this), Usage, bSkipPrim, bUsageSetSuccessfully, Event);
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
	else
	{
		return false;
	}
}

bool UMaterialInstance::IsDependent(UMaterialInterface* TestDependency)
{
	if(TestDependency == this)
	{
		return true;
	}
	else if(Parent)
	{
		if(ReentrantFlag)
		{
			return true;
		}

		FMICReentranceGuard	Guard(this);
		return Parent->IsDependent(TestDependency);
	}
	else
	{
		return false;
	}
}

void UMaterialInstance::CopyMaterialInstanceParameters(UMaterialInterface* MaterialInterface)
{
	if(MaterialInterface)
	{
		//first, clear out all the parameter values
		ClearParameterValuesInternal();

		//setup some arrays to use
		TArray<FName> Names;
		TArray<FGuid> Guids;

		//handle all the fonts
		GetMaterial()->GetAllFontParameterNames(Names, Guids);
		for(int32 i = 0; i < Names.Num(); ++i)
		{
			FName ParameterName = Names[i];
			UFont* FontValue = NULL;
			int32 FontPage;
			if(MaterialInterface->GetFontParameterValue(ParameterName, FontValue, FontPage))
			{
				FFontParameterValue* ParameterValue = new(FontParameterValues) FFontParameterValue;
				ParameterValue->ParameterName = ParameterName;
				ParameterValue->ExpressionGUID.Invalidate();
				ParameterValue->FontValue = FontValue;
				ParameterValue->FontPage = FontPage;
			}
		}


		//now do the scalar params
		Names.Reset();
		Guids.Reset();
		GetMaterial()->GetAllScalarParameterNames(Names, Guids);
		for(int32 i = 0; i < Names.Num(); ++i)
		{
			FName ParameterName = Names[i];
			float ScalarValue = 1.0f;
			if(MaterialInterface->GetScalarParameterValue(ParameterName, ScalarValue))
			{
				FScalarParameterValue* ParameterValue = new(ScalarParameterValues) FScalarParameterValue;
				ParameterValue->ParameterName = ParameterName;
				ParameterValue->ExpressionGUID.Invalidate();
				ParameterValue->ParameterValue = ScalarValue;
			}
		}

		//now do the vector params
		Names.Reset();
		Guids.Reset();
		GetMaterial()->GetAllVectorParameterNames(Names, Guids);
		for(int32 i = 0; i < Names.Num(); ++i)
		{
			FName ParameterName = Names[i];
			FLinearColor VectorValue;
			if(MaterialInterface->GetVectorParameterValue(ParameterName, VectorValue))
			{
				FVectorParameterValue* ParameterValue = new(VectorParameterValues) FVectorParameterValue;
				ParameterValue->ParameterName = ParameterName;
				ParameterValue->ExpressionGUID.Invalidate();
				ParameterValue->ParameterValue = VectorValue;
			}
		}

		//now do the texture params
		Names.Reset();
		Guids.Reset();
		GetMaterial()->GetAllTextureParameterNames(Names, Guids);
		for(int32 i = 0; i < Names.Num(); ++i)
		{
			FName ParameterName = Names[i];
			UTexture* TextureValue = NULL;
			if(MaterialInterface->GetTextureParameterValue(ParameterName, TextureValue))
			{
				FTextureParameterValue* ParameterValue = new(TextureParameterValues) FTextureParameterValue;
				ParameterValue->ParameterName = ParameterName;
				ParameterValue->ExpressionGUID.Invalidate();
				ParameterValue->ParameterValue = TextureValue;
			}
		}

		//now, init the resources
		InitResources();
	}
}

FMaterialResource* UMaterialInstance::GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel)
{
	check(IsInGameThread());

	if (QualityLevel == EMaterialQualityLevel::Num)
	{
		QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	}

	if (bHasStaticPermutationResource)
	{
		//if there is a static permutation resource, use that
		return StaticPermutationMaterialResources[QualityLevel][InFeatureLevel];
	}

	//there was no static permutation resource
	return Parent ? Parent->GetMaterialResource(InFeatureLevel,QualityLevel) : NULL;
}

const FMaterialResource* UMaterialInstance::GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel) const
{
	check(IsInGameThread());

	if (QualityLevel == EMaterialQualityLevel::Num)
	{
		QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	}

	if (bHasStaticPermutationResource)
	{
		//if there is a static permutation resource, use that
		return StaticPermutationMaterialResources[QualityLevel][InFeatureLevel];
	}

	//there was no static permutation resource
	return Parent ? Parent->GetMaterialResource(InFeatureLevel,QualityLevel) : NULL;
}

FMaterialRenderProxy* UMaterialInstance::GetRenderProxy(bool Selected, bool bHovered) const
{
	check(!( Selected || bHovered ) || GIsEditor);
	return Resources[Selected ? 1 : ( bHovered ? 2 : 0 )];
}

UPhysicalMaterial* UMaterialInstance::GetPhysicalMaterial() const
{
	if(ReentrantFlag)
	{
		return UMaterial::GetDefaultMaterial(MD_Surface)->GetPhysicalMaterial();
	}

	FMICReentranceGuard	Guard(const_cast<UMaterialInstance*>(this));  // should not need this to determine loop
	if(PhysMaterial)
	{
		return PhysMaterial;
	}
	else if(Parent)
	{
		// If no physical material has been associated with this instance, simply use the parent's physical material.
		return Parent->GetPhysicalMaterial();
	}
	else
	{
		// no material specified and no parent, fall back to default physical material
		check( GEngine->DefaultPhysMaterial != NULL );
		return GEngine->DefaultPhysMaterial;
	}
}

void UMaterialInstance::GetStaticParameterValues(FStaticParameterSet& OutStaticParameters)
{
	check(IsInGameThread());

	if (Parent)
	{
		UMaterial* ParentMaterial = Parent->GetMaterial();
		TArray<FName> ParameterNames;
		TArray<FGuid> Guids;

		// Static Switch Parameters
		ParentMaterial->GetAllStaticSwitchParameterNames(ParameterNames, Guids);
		OutStaticParameters.StaticSwitchParameters.AddZeroed(ParameterNames.Num());

		for(int32 ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FStaticSwitchParameter& ParentParameter = OutStaticParameters.StaticSwitchParameters[ParameterIdx];
			FName ParameterName = ParameterNames[ParameterIdx];
			bool Value = false;
			FGuid ExpressionId = Guids[ParameterIdx];

			ParentParameter.bOverride = false;
			ParentParameter.ParameterName = ParameterName;

			//get the settings from the parent in the MIC chain
			if(Parent->GetStaticSwitchParameterValue(ParameterName, Value, ExpressionId))
			{
				ParentParameter.Value = Value;
			}
			ParentParameter.ExpressionGUID = ExpressionId;

			//if the SourceInstance is overriding this parameter, use its settings
			for(int32 SwitchParamIdx = 0; SwitchParamIdx < StaticParameters.StaticSwitchParameters.Num(); SwitchParamIdx++)
			{
				const FStaticSwitchParameter& StaticSwitchParam = StaticParameters.StaticSwitchParameters[SwitchParamIdx];

				if(ParameterName == StaticSwitchParam.ParameterName)
				{
					ParentParameter.bOverride = StaticSwitchParam.bOverride;
					if (StaticSwitchParam.bOverride)
					{
						ParentParameter.Value = StaticSwitchParam.Value;
					}
				}
			}
		}

		// Static Component Mask Parameters
		ParentMaterial->GetAllStaticComponentMaskParameterNames(ParameterNames, Guids);
		OutStaticParameters.StaticComponentMaskParameters.AddZeroed(ParameterNames.Num());
		for(int32 ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FStaticComponentMaskParameter& ParentParameter = OutStaticParameters.StaticComponentMaskParameters[ParameterIdx];
			FName ParameterName = ParameterNames[ParameterIdx];
			bool R = false;
			bool G = false;
			bool B = false;
			bool A = false;
			FGuid ExpressionId = Guids[ParameterIdx];

			ParentParameter.bOverride = false;
			ParentParameter.ParameterName = ParameterName;

			//get the settings from the parent in the MIC chain
			if(Parent->GetStaticComponentMaskParameterValue(ParameterName, R, G, B, A, ExpressionId))
			{
				ParentParameter.R = R;
				ParentParameter.G = G;
				ParentParameter.B = B;
				ParentParameter.A = A;
			}
			ParentParameter.ExpressionGUID = ExpressionId;

			//if the SourceInstance is overriding this parameter, use its settings
			for(int32 MaskParamIdx = 0; MaskParamIdx < StaticParameters.StaticComponentMaskParameters.Num(); MaskParamIdx++)
			{
				const FStaticComponentMaskParameter &StaticComponentMaskParam = StaticParameters.StaticComponentMaskParameters[MaskParamIdx];

				if(ParameterName == StaticComponentMaskParam.ParameterName)
				{
					ParentParameter.bOverride = StaticComponentMaskParam.bOverride;
					if (StaticComponentMaskParam.bOverride)
					{
						ParentParameter.R = StaticComponentMaskParam.R;
						ParentParameter.G = StaticComponentMaskParam.G;
						ParentParameter.B = StaticComponentMaskParam.B;
						ParentParameter.A = StaticComponentMaskParam.A;
					}
				}
			}
		}

		// TerrainLayerWeight Parameters
		ParentMaterial->GetAllTerrainLayerWeightParameterNames(ParameterNames, Guids);
		OutStaticParameters.TerrainLayerWeightParameters.AddZeroed(ParameterNames.Num());
		for(int32 ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FStaticTerrainLayerWeightParameter& ParentParameter = OutStaticParameters.TerrainLayerWeightParameters[ParameterIdx];
			FName ParameterName = ParameterNames[ParameterIdx];
			FGuid ExpressionId = Guids[ParameterIdx];
			int32 WeightmapIndex = INDEX_NONE;

			ParentParameter.bOverride = false;
			ParentParameter.ParameterName = ParameterName;
			//get the settings from the parent in the MIC chain
			if(Parent->GetTerrainLayerWeightParameterValue(ParameterName, WeightmapIndex, ExpressionId))
			{
				ParentParameter.WeightmapIndex = WeightmapIndex;
			}
			ParentParameter.ExpressionGUID = ExpressionId;

			// if the SourceInstance is overriding this parameter, use its settings
			for(int32 WeightParamIdx = 0; WeightParamIdx < StaticParameters.TerrainLayerWeightParameters.Num(); WeightParamIdx++)
			{
				const FStaticTerrainLayerWeightParameter &TerrainLayerWeightParam = StaticParameters.TerrainLayerWeightParameters[WeightParamIdx];

				if(ParameterName == TerrainLayerWeightParam.ParameterName)
				{
					ParentParameter.bOverride = TerrainLayerWeightParam.bOverride;
					if (TerrainLayerWeightParam.bOverride)
					{
						ParentParameter.WeightmapIndex = TerrainLayerWeightParam.WeightmapIndex;
					}
				}
			}
		}
	}
}

void UMaterialInstance::ForceRecompileForRendering()
{
	CacheResourceShadersForRendering();
}

void UMaterialInstance::InitStaticPermutation()
{
	// Allocate material resources if needed even if we are cooking, so that StaticPermutationMaterialResources will always be valid
	UpdatePermutationAllocations();

	if ( FApp::CanEverRender() ) 
	{
		// Cache shaders for the current platform to be used for rendering
		CacheResourceShadersForRendering();
	}
}

void UMaterialInstance::GetAllShaderMaps(TArray<FMaterialShaderMap*>& OutShaderMaps)
{
	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
		{
			FMaterialResource* CurrentResource = StaticPermutationMaterialResources[QualityLevelIndex][FeatureLevelIndex];
			FMaterialShaderMap* ShaderMap = CurrentResource->GetGameThreadShaderMap();
			OutShaderMaps.Add(ShaderMap);
		}
	}
}

void UMaterialInstance::GetMaterialResourceId(EShaderPlatform ShaderPlatform, EMaterialQualityLevel::Type QualityLevel, FMaterialShaderMapId& OutId)
{
	UMaterial* BaseMaterial = GetMaterial();

	FStaticParameterSet CompositedStaticParameters;
	GetStaticParameterValues(CompositedStaticParameters);

	// TODO: Is this right?
	// ERHIFeatureLevel::Type FeatureLevel = GetMaxSupportedFeatureLevel(ShaderPlatform);
	const FMaterialResource* BaseResource = BaseMaterial->GetMaterialResource(GRHIFeatureLevel, QualityLevel);

	GetMaterialResourceId(BaseResource, ShaderPlatform, CompositedStaticParameters, OutId);
}

void UMaterialInstance::GetMaterialResourceId(const FMaterialResource* Resource, EShaderPlatform ShaderPlatform, const FStaticParameterSet& CompositedStaticParameters, FMaterialShaderMapId& OutId)
{
	Resource->GetShaderMapId(ShaderPlatform, OutId);

	//@todo - should the resource be able to generate its own shadermap id?
	OutId.ParameterSet = CompositedStaticParameters;
}

void UMaterialInstance::UpdatePermutationAllocations()
{
	if (bHasStaticPermutationResource)
	{
		UMaterial* BaseMaterial = GetMaterial();

		TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> > QualityLevelsUsed;
		BaseMaterial->GetQualityLevelNodeUsage(QualityLevelsUsed);

		for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
		{
			for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
			{
				FMaterialResource*& CurrentResource = StaticPermutationMaterialResources[QualityLevelIndex][FeatureLevelIndex];

				if (!CurrentResource)
				{
					CurrentResource = new FMaterialResource();
				}

				const bool bQualityLevelHasDifferentNodes = QualityLevelsUsed[QualityLevelIndex];
				CurrentResource->SetMaterial(BaseMaterial, (EMaterialQualityLevel::Type)QualityLevelIndex, bQualityLevelHasDifferentNodes, (ERHIFeatureLevel::Type)FeatureLevelIndex, this);
			}
		}
	}
}

void UMaterialInstance::CacheResourceShadersForRendering()
{
	check(IsInGameThread());

	// Fix-up the parent lighting guid if it has changed...
	if (Parent && (Parent->GetLightingGuid() != ParentLightingGuid))
	{
		SetLightingGuid();
		ParentLightingGuid = Parent ? Parent->GetLightingGuid() : FGuid(0,0,0,0);
	}

	UpdatePermutationAllocations();

	if (bHasStaticPermutationResource && FApp::CanEverRender())
	{
		check(IsA(UMaterialInstanceConstant::StaticClass()));

		uint32 FeatureLevelsToCompile = GetFeatureLevelsToCompileForRendering();
		EMaterialQualityLevel::Type ActiveQualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
		TArray<FMaterialResource*> ResourcesToCache;

		while (FeatureLevelsToCompile != 0)
		{
			ERHIFeatureLevel::Type FeatureLevel = (ERHIFeatureLevel::Type)FBitSet::GetAndClearNextBit(FeatureLevelsToCompile); 
			EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];

			// Only cache shaders for the quality level that will actually be used to render
			ResourcesToCache.Reset();
			ResourcesToCache.Add(StaticPermutationMaterialResources[ActiveQualityLevel][FeatureLevel]);
			CacheShadersForResources(ShaderPlatform, ResourcesToCache, true);
		}
	}

	InitResources();
}

void UMaterialInstance::CacheResourceShadersForCooking(EShaderPlatform ShaderPlatform, TArray<FMaterialResource*>& OutCachedMaterialResources)
{
	if (bHasStaticPermutationResource)
	{
		UMaterial* BaseMaterial = GetMaterial();

		TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> > QualityLevelsUsed;
		BaseMaterial->GetQualityLevelNodeUsage(QualityLevelsUsed);

		TArray<FMaterialResource*> ResourcesToCache;
		ERHIFeatureLevel::Type TargetFeatureLevel = GetMaxSupportedFeatureLevel(ShaderPlatform);

		bool bAnyQualityLevelUsed = false;

		for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
		{
			bAnyQualityLevelUsed = bAnyQualityLevelUsed || QualityLevelsUsed[QualityLevelIndex];
		}

		for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
		{
			// Cache all quality levels, unless they are all the same (due to using the same nodes), then just cache the high quality
			if (bAnyQualityLevelUsed || QualityLevelIndex == EMaterialQualityLevel::High)
			{
				FMaterialResource* NewResource = new FMaterialResource();
				NewResource->SetMaterial(BaseMaterial, (EMaterialQualityLevel::Type)QualityLevelIndex, QualityLevelsUsed[QualityLevelIndex], (ERHIFeatureLevel::Type)TargetFeatureLevel, this);
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
}

void UMaterialInstance::CacheShadersForResources(EShaderPlatform ShaderPlatform, const TArray<FMaterialResource*>& ResourcesToCache, bool bApplyCompletedShaderMapForRendering)
{
	FStaticParameterSet CompositedStaticParameters;
	GetStaticParameterValues(CompositedStaticParameters);

	UMaterial* BaseMaterial = GetMaterial();

	BaseMaterial->CacheExpressionTextureReferences();

	for (int32 ResourceIndex = 0; ResourceIndex < ResourcesToCache.Num(); ResourceIndex++)
	{
		FMaterialResource* CurrentResource = ResourcesToCache[ResourceIndex];

		FMaterialShaderMapId ShaderMapId;
		GetMaterialResourceId(CurrentResource, ShaderPlatform, CompositedStaticParameters, ShaderMapId);

		const bool bSuccess = CurrentResource->CacheShaders(ShaderMapId, ShaderPlatform, bApplyCompletedShaderMapForRendering);

		if (!bSuccess)
		{
			UE_LOG(LogMaterial, Warning,
				TEXT("Failed to compile Material Instance %s with Base %s for platform %s, Default Material will be used in game."), 
				*GetPathName(), 
				BaseMaterial ? *BaseMaterial->GetName() : TEXT("Null"), 
				*LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString()
				);

			const TArray<FString>& CompileErrors = CurrentResource->GetCompileErrors();
			for (int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
			{
				UE_LOG(LogMaterial, Warning, TEXT("	%s"), *CompileErrors[ErrorIndex]);
			}
		}
	}
}

bool UMaterialInstance::GetStaticSwitchParameterValue(FName ParameterName, bool &OutValue,FGuid &OutExpressionGuid)
{
	if(ReentrantFlag)
	{
		return false;
	}

	bool* Value = NULL;
	FGuid* Guid = NULL;
	for (int32 ValueIndex = 0;ValueIndex < StaticParameters.StaticSwitchParameters.Num();ValueIndex++)
	{
		if (StaticParameters.StaticSwitchParameters[ValueIndex].ParameterName == ParameterName)
		{
			Value = &StaticParameters.StaticSwitchParameters[ValueIndex].Value;
			Guid = &StaticParameters.StaticSwitchParameters[ValueIndex].ExpressionGUID;
			break;
		}
	}
	if(Value)
	{
		OutValue = *Value;
		OutExpressionGuid = *Guid;
		return true;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetStaticSwitchParameterValue(ParameterName,OutValue,OutExpressionGuid);
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::GetStaticComponentMaskParameterValue(FName ParameterName, bool &OutR, bool &OutG, bool &OutB, bool &OutA, FGuid &OutExpressionGuid)
{
	if(ReentrantFlag)
	{
		return false;
	}

	bool* R = NULL;
	bool* G = NULL;
	bool* B = NULL;
	bool* A = NULL;
	FGuid* ExpressionId = NULL;
	for (int32 ValueIndex = 0;ValueIndex < StaticParameters.StaticComponentMaskParameters.Num();ValueIndex++)
	{
		if (StaticParameters.StaticComponentMaskParameters[ValueIndex].ParameterName == ParameterName)
		{
			R = &StaticParameters.StaticComponentMaskParameters[ValueIndex].R;
			G = &StaticParameters.StaticComponentMaskParameters[ValueIndex].G;
			B = &StaticParameters.StaticComponentMaskParameters[ValueIndex].B;
			A = &StaticParameters.StaticComponentMaskParameters[ValueIndex].A;
			ExpressionId = &StaticParameters.StaticComponentMaskParameters[ValueIndex].ExpressionGUID;
			break;
		}
	}
	if(R && G && B && A)
	{
		OutR = *R;
		OutG = *G;
		OutB = *B;
		OutA = *A;
		OutExpressionGuid = *ExpressionId;
		return true;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetStaticComponentMaskParameterValue(ParameterName, OutR, OutG, OutB, OutA, OutExpressionGuid);
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::GetTerrainLayerWeightParameterValue(FName ParameterName, int32& OutWeightmapIndex, FGuid &OutExpressionGuid)
{
	if(ReentrantFlag)
	{
		return false;
	}

	int32 WeightmapIndex = INDEX_NONE;

	FGuid* ExpressionId = NULL;
	for (int32 ValueIndex = 0;ValueIndex < StaticParameters.TerrainLayerWeightParameters.Num();ValueIndex++)
	{
		if (StaticParameters.TerrainLayerWeightParameters[ValueIndex].ParameterName == ParameterName)
		{
			WeightmapIndex = StaticParameters.TerrainLayerWeightParameters[ValueIndex].WeightmapIndex;
			ExpressionId = &StaticParameters.TerrainLayerWeightParameters[ValueIndex].ExpressionGUID;
			break;
		}
	}
	if (WeightmapIndex >= 0)
	{
		OutWeightmapIndex = WeightmapIndex;
		OutExpressionGuid = *ExpressionId;
		return true;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetTerrainLayerWeightParameterValue(ParameterName, OutWeightmapIndex, OutExpressionGuid);
	}
	else
	{
		return false;
	}
}

template <typename ParameterType>
void TrimToOverriddenOnly(TArray<ParameterType>& Parameters)
{
	for (int32 ParameterIndex = Parameters.Num() - 1; ParameterIndex >= 0; ParameterIndex--)
	{
		if (!Parameters[ParameterIndex].bOverride)
		{
			Parameters.RemoveAt(ParameterIndex);
		}
	}
}

void UMaterialInstance::BeginCacheForCookedPlatformData( const ITargetPlatform *TargetPlatform )
{
	TArray<FName> DesiredShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

	TArray<FMaterialResource*> *CachedMaterialResourcesForPlatform = CachedMaterialResourcesForCooking.Find( TargetPlatform );

	if ( CachedMaterialResourcesForPlatform == NULL )
	{
		check( CachedMaterialResourcesForPlatform == NULL );

		CachedMaterialResourcesForCooking.Add( TargetPlatform );
		CachedMaterialResourcesForPlatform = CachedMaterialResourcesForCooking.Find( TargetPlatform );

		check( CachedMaterialResourcesForPlatform != NULL );

		// Cache shaders for each shader format, storing the results in CachedMaterialResourcesForCooking so they will be available during saving
		for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
		{
			const EShaderPlatform TargetPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);

			CacheResourceShadersForCooking(TargetPlatform, *CachedMaterialResourcesForPlatform );
		}
	}
}

void UMaterialInstance::ClearCachedCookedPlatformData( const ITargetPlatform *TargetPlatform )
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


void UMaterialInstance::ClearAllCachedCookedPlatformData()
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

void UMaterialInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Only serialize the static permutation resource if one exists
	if (bHasStaticPermutationResource)
	{
		if (Ar.UE4Ver() >= VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS)
		{
			StaticParameters.Serialize(Ar);

			SerializeInlineShaderMaps( CachedMaterialResourcesForCooking, Ar, StaticPermutationMaterialResources );
		}
		else
		{
			FMaterialResource LegacyResource;
			LegacyResource.LegacySerialize(Ar);

			FMaterialShaderMapId LegacyId;
			LegacyId.Serialize(Ar);

			StaticParameters.StaticSwitchParameters = LegacyId.ParameterSet.StaticSwitchParameters;
			StaticParameters.StaticComponentMaskParameters = LegacyId.ParameterSet.StaticComponentMaskParameters;
			StaticParameters.TerrainLayerWeightParameters = LegacyId.ParameterSet.TerrainLayerWeightParameters;

			TrimToOverriddenOnly(StaticParameters.StaticSwitchParameters);
			TrimToOverriddenOnly(StaticParameters.StaticComponentMaskParameters);
			TrimToOverriddenOnly(StaticParameters.TerrainLayerWeightParameters);
		}
	}

	if (Ar.UE4Ver() >= VER_UE4_MATERIAL_INSTANCE_BASE_PROPERTY_OVERRIDES )
	{
		Ar << bOverrideBaseProperties;
		bool bHasPropertyOverrides = NULL != BasePropertyOverrides;
		Ar << bHasPropertyOverrides;
		if( bHasPropertyOverrides )
		{
			if( !BasePropertyOverrides )
			{
				BasePropertyOverrides = new FMaterialInstanceBasePropertyOverrides();
				BasePropertyOverrides->Init(*this);
			}
			Ar << *BasePropertyOverrides;
		}
	}
	else
	{
		bOverrideBaseProperties = false;
		BasePropertyOverrides = NULL;
	}
}

void UMaterialInstance::PostLoad()
{
	Super::PostLoad();

	AssertDefaultMaterialsPostLoaded();

	// Ensure that the instance's parent is PostLoaded before the instance.
	if(Parent)
	{
		Parent->ConditionalPostLoad();
	}

	// Add references to the expression object if we do not have one already, and fix up any names that were changed.
	UpdateParameters();

	// We have to make sure the resources are created for all used textures.
	for( int32 ValueIndex=0; ValueIndex<TextureParameterValues.Num(); ValueIndex++ )
	{
		// Make sure the texture is postloaded so the resource isn't null.
		UTexture* Texture = TextureParameterValues[ValueIndex].ParameterValue;
		if( Texture )
		{
			Texture->ConditionalPostLoad();
		}
	}
	// do the same for font textures
	for( int32 ValueIndex=0; ValueIndex < FontParameterValues.Num(); ValueIndex++ )
	{
		// Make sure the font is postloaded so the resource isn't null.
		UFont* Font = FontParameterValues[ValueIndex].FontValue;
		if( Font )
		{
			Font->ConditionalPostLoad();
		}
	}

	// Update bHasStaticPermutationResource in case the parent was not found
	bHasStaticPermutationResource = (!StaticParameters.IsEmpty() || (bOverrideBaseProperties && BasePropertyOverrides)) && Parent;

	STAT(double MaterialLoadTime = 0);
	{
		SCOPE_SECONDS_COUNTER(MaterialLoadTime);

		// Make sure static parameters are up to date and shaders are cached for the current platform
		InitStaticPermutation();

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
	}
	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_MaterialLoading,(float)MaterialLoadTime);

	if (GIsEditor && GEngine != NULL && !IsTemplate() && Parent)
	{
		// Ensure that the ReferencedTextureGuids array is up to date.
		UpdateLightmassTextureTracking();
	}

	for (int32 i = 0; i < ARRAY_COUNT(Resources); i++)
	{
		if (Resources[i])
		{
			Resources[i]->GameThread_UpdateDistanceFieldPenumbraScale(GetDistanceFieldPenumbraScale());
		}
	}

	// Fixup for legacy instances which didn't recreate the lighting guid properly on duplication
	if (GetLinker() && GetLinker()->UE4Ver() < VER_UE4_BUMPED_MATERIAL_EXPORT_GUIDS)
	{
		extern TMap<FGuid, UMaterialInterface*> LightingGuidFixupMap;
		UMaterialInterface** ExistingMaterial = LightingGuidFixupMap.Find(GetLightingGuid());

		if (ExistingMaterial)
		{
			SetLightingGuid();
		}

		LightingGuidFixupMap.Add(GetLightingGuid(), this);
	}
}

void UMaterialInstance::BeginDestroy()
{
	Super::BeginDestroy();

	ReleaseFence.BeginFence();
}

bool UMaterialInstance::IsReadyForFinishDestroy()
{
	bool bIsReady = Super::IsReadyForFinishDestroy();

	return bIsReady && ReleaseFence.IsFenceComplete();;
}

void UMaterialInstance::FinishDestroy()
{
	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		Resources[0]->GameThread_Destroy();
		Resources[0] = NULL;

		if(GIsEditor)
		{
			Resources[1]->GameThread_Destroy();
			Resources[1] = NULL;
			Resources[2]->GameThread_Destroy();
			Resources[2] = NULL;
		}
	}

	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
		{
			FMaterialResource*& CurrentResource = StaticPermutationMaterialResources[QualityLevelIndex][FeatureLevelIndex];
			delete CurrentResource;
			CurrentResource = NULL;
		}
	}

	ClearAllCachedCookedPlatformData();

	if( BasePropertyOverrides )
	{
		delete BasePropertyOverrides;
		BasePropertyOverrides = NULL;
	}

	Super::FinishDestroy();
}

void UMaterialInstance::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UMaterialInstance* This = CastChecked<UMaterialInstance>(InThis);

	if (This->bHasStaticPermutationResource)
	{
		for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
		{
			for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
			{
				FMaterialResource* CurrentResource = This->StaticPermutationMaterialResources[QualityLevelIndex][FeatureLevelIndex];
				CurrentResource->AddReferencedObjects(Collector);
			}
		}
	}

	Super::AddReferencedObjects(This, Collector);
}

void UMaterialInstance::SetParentInternal(UMaterialInterface* NewParent)
{
	if (Parent == NULL || Parent != NewParent)
	{
		if (NewParent &&
			!NewParent->IsA(UMaterial::StaticClass()) &&
			!NewParent->IsA(UMaterialInstanceConstant::StaticClass()))
		{
			UE_LOG(LogMaterial, Warning, TEXT("%s is not a valid parent for %s. Only Materials and MaterialInstanceConstants are valid parents for a material instance."),
				*NewParent->GetFullName(),
				*GetFullName());
		}
		else
		{
			Parent = NewParent;

			if( Parent )
			{
				// It is possible to set a material's parent while post-loading. In
				// such a case it is also possible that the parent has not been
				// post-loaded, so call ConditionalPostLoad() just in case.
				Parent->ConditionalPostLoad();
			}
		}
		InitResources();
	}
}

void UMaterialInstance::SetVectorParameterValueInternal(FName ParameterName, FLinearColor Value)
{
	FVectorParameterValue* ParameterValue = GameThread_FindParameterByName(
		VectorParameterValues,
		ParameterName
		);

	if(!ParameterValue)
	{
		// If there's no element for the named parameter in array yet, add one.
		ParameterValue = new(VectorParameterValues) FVectorParameterValue;
		ParameterValue->ParameterName = ParameterName;
		ParameterValue->ExpressionGUID.Invalidate();
		// Force an update on first use
		ParameterValue->ParameterValue.B = Value.B - 1.f;
	}

	// Don't enqueue an update if it isn't needed
	if (ParameterValue->ParameterValue != Value)
	{
		ParameterValue->ParameterValue = Value;
		// Update the material instance data in the rendering thread.
		GameThread_UpdateMIParameter(this, *ParameterValue);
		CacheMaterialInstanceUniformExpressions(this);
	}
}

void UMaterialInstance::SetScalarParameterValueInternal(FName ParameterName, float Value)
{
	FScalarParameterValue* ParameterValue = GameThread_FindParameterByName(
		ScalarParameterValues,
		ParameterName
		);

	if(!ParameterValue)
	{
		// If there's no element for the named parameter in array yet, add one.
		ParameterValue = new(ScalarParameterValues) FScalarParameterValue;
		ParameterValue->ParameterName = ParameterName;
		ParameterValue->ExpressionGUID.Invalidate();
		// Force an update on first use
		ParameterValue->ParameterValue = Value - 1.f;
	}

	// Don't enqueue an update if it isn't needed
	if (ParameterValue->ParameterValue != Value)
	{
		ParameterValue->ParameterValue = Value;
		// Update the material instance data in the rendering thread.
		GameThread_UpdateMIParameter(this, *ParameterValue);
		CacheMaterialInstanceUniformExpressions(this);
	}
}

void UMaterialInstance::SetTextureParameterValueInternal(FName ParameterName, UTexture* Value)
{
	FTextureParameterValue* ParameterValue = GameThread_FindParameterByName(
		TextureParameterValues,
		ParameterName
		);

	if(!ParameterValue)
	{
		// If there's no element for the named parameter in array yet, add one.
		ParameterValue = new(TextureParameterValues) FTextureParameterValue;
		ParameterValue->ParameterName = ParameterName;
		ParameterValue->ExpressionGUID.Invalidate();
		// Force an update on first use
		ParameterValue->ParameterValue = Value == GEngine->DefaultDiffuseTexture ? NULL : GEngine->DefaultDiffuseTexture;
	}

	// Don't enqueue an update if it isn't needed
	if (ParameterValue->ParameterValue != Value)
	{
		ParameterValue->ParameterValue = Value;
		// Update the material instance data in the rendering thread.
		GameThread_UpdateMIParameter(this, *ParameterValue);
		CacheMaterialInstanceUniformExpressions(this);
	}
}

void UMaterialInstance::SetFontParameterValueInternal(FName ParameterName,class UFont* FontValue,int32 FontPage)
{
	FFontParameterValue* ParameterValue = GameThread_FindParameterByName(
		FontParameterValues,
		ParameterName
		);

	if(!ParameterValue)
	{
			// If there's no element for the named parameter in array yet, add one.
			ParameterValue = new(FontParameterValues) FFontParameterValue;
			ParameterValue->ParameterName = ParameterName;
			ParameterValue->ExpressionGUID.Invalidate();
			// Force an update on first use
			ParameterValue->FontValue == GEngine->GetTinyFont() ? NULL : GEngine->GetTinyFont();
			ParameterValue->FontPage = FontPage - 1;
	}

	// Don't enqueue an update if it isn't needed
	if (ParameterValue->FontValue != FontValue ||
		ParameterValue->FontPage != FontPage)
	{
		ParameterValue->FontValue = FontValue;
		ParameterValue->FontPage = FontPage;
		// Update the material instance data in the rendering thread.
		GameThread_UpdateMIParameter(this, *ParameterValue);
		CacheMaterialInstanceUniformExpressions(this);
	}
}

void UMaterialInstance::ClearParameterValuesInternal()
{
	VectorParameterValues.Empty();
	ScalarParameterValues.Empty();
	TextureParameterValues.Empty();
	FontParameterValues.Empty();

	for (int32 ResourceIndex = 0; ResourceIndex < ARRAY_COUNT(Resources); ++ResourceIndex)
	{
		if (Resources[ResourceIndex])
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				FClearMIParametersCommand,
				FMaterialInstanceResource*,Resource,Resources[ResourceIndex],
			{
				Resource->RenderThread_ClearParameters();
			});
		}
	}

	InitResources();
}

#if WITH_EDITOR
void UMaterialInstance::UpdateStaticPermutation(const FStaticParameterSet& NewParameters, bool bForceRecompile)
{
	check(GIsEditor);

	FStaticParameterSet CompareParameters = NewParameters;

	TrimToOverriddenOnly(CompareParameters.StaticSwitchParameters);
	TrimToOverriddenOnly(CompareParameters.StaticComponentMaskParameters);
	TrimToOverriddenOnly(CompareParameters.TerrainLayerWeightParameters);

	const bool bWantsStaticPermutationResource = (!CompareParameters.IsEmpty() || (BasePropertyOverrides && bOverrideBaseProperties) || bForceRecompile) && Parent;

	if (bForceRecompile || bHasStaticPermutationResource != bWantsStaticPermutationResource || StaticParameters != CompareParameters)
	{
		// This will flush the rendering thread which is necessary before changing bHasStaticPermutationResource, since the RT is reading from that directly
		// The update context will also make sure any dependent MI's with static parameters get recompiled
		FMaterialUpdateContext MaterialUpdateContext;
		MaterialUpdateContext.AddMaterialInstance(this);
		bHasStaticPermutationResource = bWantsStaticPermutationResource;
		StaticParameters = CompareParameters;

		CacheResourceShadersForRendering();
	}
}


void UMaterialInstance::UpdateParameterNames()
{
	bool bDirty = UpdateParameters();

	// Atleast 1 parameter changed, initialize parameters
	if (bDirty)
	{
		InitResources();
	}
}
#endif

void UMaterialInstance::RecacheUniformExpressions() const
{	
	CacheMaterialInstanceUniformExpressions(this);
}

#if WITH_EDITOR
void UMaterialInstance::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged )
	{
		if(PropertyThatChanged->GetName()==TEXT("Parent"))
		{
			ParentLightingGuid = Parent ? Parent->GetLightingGuid() : FGuid(0,0,0,0);
		}
	}

	// Ensure that the ReferencedTextureGuids array is up to date.
	if (GIsEditor)
	{
		UpdateLightmassTextureTracking();
	}

	for (int32 i = 0; i < ARRAY_COUNT(Resources); i++)
	{
		if (Resources[i])
		{
			Resources[i]->GameThread_UpdateDistanceFieldPenumbraScale(GetDistanceFieldPenumbraScale());
		}
	}

	InitResources();

	UpdateStaticPermutation(StaticParameters);

	if(PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
	{
		RecacheMaterialInstanceUniformExpressions(this);
	}
}

#endif // WITH_EDITOR


bool UMaterialInstance::UpdateLightmassTextureTracking()
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


bool UMaterialInstance::GetCastShadowAsMasked() const
{
	if (LightmassSettings.bOverrideCastShadowAsMasked)
	{
		return LightmassSettings.bCastShadowAsMasked;
	}

	if (Parent)
	{
		return Parent->GetCastShadowAsMasked();
	}

	return false;
}

float UMaterialInstance::GetEmissiveBoost() const
{
	if (LightmassSettings.bOverrideEmissiveBoost)
	{
		return LightmassSettings.EmissiveBoost;
	}

	if (Parent)
	{
		return Parent->GetEmissiveBoost();
	}

	return 1.0f;
}

float UMaterialInstance::GetDiffuseBoost() const
{
	if (LightmassSettings.bOverrideDiffuseBoost)
	{
		return LightmassSettings.DiffuseBoost;
	}

	if (Parent)
	{
		return Parent->GetDiffuseBoost();
	}

	return 1.0f;
}

float UMaterialInstance::GetExportResolutionScale() const
{
	if (LightmassSettings.bOverrideExportResolutionScale)
	{
		return FMath::Clamp(LightmassSettings.ExportResolutionScale, .1f, 10.0f);
	}

	if (Parent)
	{
		return FMath::Clamp(Parent->GetExportResolutionScale(), .1f, 10.0f);
	}

	return 1.0f;
}

float UMaterialInstance::GetDistanceFieldPenumbraScale() const
{
	if (LightmassSettings.bOverrideDistanceFieldPenumbraScale)
	{
		return LightmassSettings.DistanceFieldPenumbraScale;
	}

	if (Parent)
	{
		return Parent->GetDistanceFieldPenumbraScale();
	}

	return 1.0f;
}


bool UMaterialInstance::GetTexturesInPropertyChain(EMaterialProperty InProperty, TArray<UTexture*>& OutTextures,  
	TArray<FName>* OutTextureParamNames, class FStaticParameterSet* InStaticParameterSet)
{
	if (Parent != NULL)
	{
		TArray<FName> LocalTextureParamNames;
		bool bResult = Parent->GetTexturesInPropertyChain(InProperty, OutTextures, &LocalTextureParamNames, InStaticParameterSet);
		if (LocalTextureParamNames.Num() > 0)
		{
			// Check textures set in parameters as well...
			for (int32 TPIdx = 0; TPIdx < LocalTextureParamNames.Num(); TPIdx++)
			{
				UTexture* ParamTexture = NULL;
				if (GetTextureParameterValue(LocalTextureParamNames[TPIdx], ParamTexture) == true)
				{
					if (ParamTexture != NULL)
					{
						OutTextures.AddUnique(ParamTexture);
					}
				}

				if (OutTextureParamNames != NULL)
				{
					OutTextureParamNames->AddUnique(LocalTextureParamNames[TPIdx]);
				}
			}
		}
		return bResult;
	}
	return false;
}

SIZE_T UMaterialInstance::GetResourceSize(EResourceSizeMode::Type Mode)
{
	SIZE_T ResourceSize = 0;

	if (bHasStaticPermutationResource)
	{
		if (Mode == EResourceSizeMode::Inclusive)
		{
			for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
			{
				for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
				{
					FMaterialResource* CurrentResource = StaticPermutationMaterialResources[QualityLevelIndex][FeatureLevelIndex];
					ResourceSize += CurrentResource->GetResourceSizeInclusive();
				}
			}
		}
	}

	for (int32 ResourceIndex = 0; ResourceIndex < 3; ++ResourceIndex)
	{
		if (Resources[ResourceIndex])
		{
			ResourceSize += sizeof(FMaterialInstanceResource);
			ResourceSize += ScalarParameterValues.Num() * sizeof(FMaterialInstanceResource::TNamedParameter<float>);
			ResourceSize += VectorParameterValues.Num() * sizeof(FMaterialInstanceResource::TNamedParameter<FLinearColor>);
			ResourceSize += TextureParameterValues.Num() * sizeof(FMaterialInstanceResource::TNamedParameter<const UTexture*>);
			ResourceSize += FontParameterValues.Num() * sizeof(FMaterialInstanceResource::TNamedParameter<const UTexture*>);
		}
	}

	return ResourceSize;
}

FPostProcessMaterialNode* IteratePostProcessMaterialNodes(const FFinalPostProcessSettings& Dest, const UMaterial* Material, FBlendableEntry*& Iterator)
{
	EBlendableLocation Location = Material->BlendableLocation;
	int32 Priority = Material->BlendablePriority;

	for(;;)
	{
		FPostProcessMaterialNode* DataPtr = Dest.BlendableManager.IterateBlendables<FPostProcessMaterialNode>(Iterator);

		if(!DataPtr)
		{
			// end reached
			return 0;
		}

		if(DataPtr->MID->GetMaterial() == Material && DataPtr->Location == Location && DataPtr->Priority == Priority)
		{
			return DataPtr;
		}
	}
}

void UMaterialInstance::OverrideBlendableSettings(class FSceneView& View, float Weight) const
{
	check(Weight >= 0.0f && Weight <= 1.0f);

	FFinalPostProcessSettings& Dest = View.FinalPostProcessSettings;

	if(!Parent)
	{
		return;
	}

	UMaterial* Material = Parent->GetMaterial();

	//	should we use UMaterial::GetDefaultMaterial(Domain) instead of skipping the material

	if(!Material || Material->MaterialDomain != MD_PostProcess || !View.State)
	{
		return;
	}

	FBlendableEntry* Iterator = 0;

	FPostProcessMaterialNode* PostProcessMaterialNode = IteratePostProcessMaterialNodes(Dest, Material, Iterator);

	// is this the first one of this material?
	if(!PostProcessMaterialNode)
	{
		// do we partly want to fade this one in.
		if(Weight < 1.0f)
		{
			UMaterial* Base = Material->GetBaseMaterial();

			UMaterialInstanceDynamic* MID = View.State->GetReusableMID((UMaterialInterface*)Base);//, (UMaterialInterface*)this);

			if(MID)
			{
				MID->K2_CopyMaterialInstanceParameters((UMaterialInterface*)Base);

				FPostProcessMaterialNode NewNode(MID, Base->BlendableLocation, Base->BlendablePriority);

				// it's the first material, no blending needed
				Dest.BlendableManager.PushBlendableData(1.0f, NewNode);

				// can be optimized
				PostProcessMaterialNode = IteratePostProcessMaterialNodes(Dest, Base, Iterator);
			}
		}
	}

	if(PostProcessMaterialNode)
	{
		UMaterialInstanceDynamic* DestMID = PostProcessMaterialNode->MID;
		UMaterialInstance* SrcMID = (UMaterialInstance*)this;

		check(DestMID);
		check(SrcMID);

		// a material already exists, blend with existing ones
		DestMID->K2_InterpolateMaterialInstanceParams(DestMID, SrcMID, Weight);
	}
	else
	{
		UMaterialInstanceDynamic* MID = View.State->GetReusableMID((UMaterialInterface*)Material);//, (UMaterialInterface*)this);

		if(MID)
		{
			MID->K2_CopyMaterialInstanceParameters((UMaterialInterface*)this);

			FPostProcessMaterialNode NewNode(MID, Material->BlendableLocation, Material->BlendablePriority);

			// it's the first material, no blending needed
			Dest.BlendableManager.PushBlendableData(Weight, NewNode);
		}
	}
}

void UMaterialInstance::AllMaterialsCacheResourceShadersForRendering()
{
	for (TObjectIterator<UMaterialInstance> It; It; ++It)
	{
		UMaterialInstance* MaterialInstance = *It;

		MaterialInstance->CacheResourceShadersForRendering();
	}
}

/**
	Properties of the base material. Can now be overridden by instances.
*/

void UMaterialInstance::GetBasePropertyOverridesHash(FSHAHash& OutHash)const
{
	FSHA1 HashState;

	if( bOverrideBaseProperties && BasePropertyOverrides )
	{
		BasePropertyOverrides->UpdateHash(HashState);
	}
	
	HashState.Final();
	HashState.GetHash(&OutHash.Hash[0]);
}

float UMaterialInstance::GetOpacityMaskClipValue_Internal() const
{
	checkSlow(IsInGameThread());
	if( bOverrideBaseProperties && BasePropertyOverrides && BasePropertyOverrides->bOverride_OpacityMaskClipValue )
	{
		return BasePropertyOverrides->OpacityMaskClipValue;
	}
	return GetMaterial()->GetOpacityMaskClipValue();
}
EBlendMode UMaterialInstance::GetBlendMode_Internal() const
{
	checkSlow(IsInGameThread());
	if( bOverrideBaseProperties && BasePropertyOverrides && BasePropertyOverrides->bOverride_BlendMode )
	{
		return BasePropertyOverrides->BlendMode;
	}
	return GetMaterial()->GetBlendMode();
}

EMaterialLightingModel UMaterialInstance::GetLightingModel_Internal() const
{
	checkSlow(IsInGameThread());
	if( bOverrideBaseProperties && BasePropertyOverrides && BasePropertyOverrides->bOverride_LightingModel )
	{
		return BasePropertyOverrides->LightingModel;
	}
	return GetMaterial()->GetLightingModel();
}

bool UMaterialInstance::IsTwoSided_Internal() const
{
	checkSlow(IsInGameThread());
	if( bOverrideBaseProperties && BasePropertyOverrides && BasePropertyOverrides->bOverride_TwoSided )
	{
		return BasePropertyOverrides->TwoSided != 0;
	}
	return GetMaterial()->IsTwoSided();
}

bool UMaterialInstance::GetOpacityMaskClipValueOverride(float& OutResult) const
{
	if( bOverrideBaseProperties && BasePropertyOverrides && BasePropertyOverrides->bOverride_OpacityMaskClipValue )
	{
		OutResult = BasePropertyOverrides->OpacityMaskClipValue;
		return true;
	}
	return false;
}

bool UMaterialInstance::GetBlendModeOverride(EBlendMode& OutResult) const
{
	if( bOverrideBaseProperties && BasePropertyOverrides && BasePropertyOverrides->bOverride_BlendMode )
	{
		OutResult = BasePropertyOverrides->BlendMode;
		return true;
	}
	return false;
}

bool UMaterialInstance::GetLightingModelOverride(EMaterialLightingModel& OutResult) const
{
	if( bOverrideBaseProperties && BasePropertyOverrides && BasePropertyOverrides->bOverride_LightingModel )
	{
		OutResult = BasePropertyOverrides->LightingModel;
		return true;
	}
	return false;
}

bool UMaterialInstance::IsTwoSidedOverride(bool& OutResult) const
{
	if( bOverrideBaseProperties && BasePropertyOverrides && BasePropertyOverrides->bOverride_TwoSided )
	{
		OutResult = BasePropertyOverrides->TwoSided != 0;
		return true;
	}
	return false;
}

/** Checks to see if an input property should be active, based on the state of the material */
bool UMaterialInstance::IsPropertyActive(EMaterialProperty InProperty) const
{
	return true;
}

int32 UMaterialInstance::CompileProperty( class FMaterialCompiler* Compiler, EMaterialProperty Property, float DefaultFloat, FLinearColor DefaultColor, const FVector4& DefaultVector )
{
	return Parent ? Parent->CompileProperty(Compiler,Property,DefaultFloat,DefaultColor,DefaultVector) : INDEX_NONE;
}