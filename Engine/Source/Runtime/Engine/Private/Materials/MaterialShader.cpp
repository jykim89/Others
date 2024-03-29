// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialShader.h: Material shader definitions.
=============================================================================*/

#include "EnginePrivate.h"
#include "DiagnosticTable.h"
#include "MaterialShader.h"
#include "ShaderCompiler.h"
#include "DerivedDataCacheInterface.h"
#include "MeshMaterialShader.h"
#include "TargetPlatform.h"
#include "../ShaderDerivedDataVersion.h"

int32 GCreateShadersOnLoad = 0;
static FAutoConsoleVariableRef CVarCreateShadersOnLoad(
	TEXT("r.CreateShadersOnLoad"),
	GCreateShadersOnLoad,
	TEXT("Whether to create shaders on load, which can reduce hitching, but use more memory.  Otherwise they will be created as needed.")
	);

//
// Globals
//
TMap<FMaterialShaderMapId,FMaterialShaderMap*> FMaterialShaderMap::GIdToMaterialShaderMap[SP_NumPlatforms];
TArray<FMaterialShaderMap*> FMaterialShaderMap::AllMaterialShaderMaps;
// The Id of 0 is reserved for global shaders
uint32 FMaterialShaderMap::NextCompilingId = 1;
/** 
 * Tracks material resources and their shader maps that are being compiled.
 * Uses a TRefCountPtr as this will be the only reference to a shader map while it is being compiled.
 */
TMap<TRefCountPtr<FMaterialShaderMap>, TArray<FMaterial*> > FMaterialShaderMap::ShaderMapsBeingCompiled;

/** Converts an EMaterialLightingModel to a string description. */
FString GetLightingModelString(EMaterialLightingModel LightingModel)
{
	FString LightingModelName;
	switch(LightingModel)
	{
		case MLM_DefaultLit: LightingModelName = TEXT("MLM_DefaultLit"); break;
		case MLM_Unlit: LightingModelName = TEXT("MLM_Unlit"); break;
		case MLM_Subsurface: LightingModelName = TEXT("MLM_Subsurface"); break;
		case MLM_PreintegratedSkin: LightingModelName = TEXT("MLM_PreintegratedSkin"); break;
		default: LightingModelName = TEXT("Unknown"); break;
	}
	return LightingModelName;
}

/** Converts an EBlendMode to a string description. */
FString GetBlendModeString(EBlendMode BlendMode)
{
	FString BlendModeName;
	switch(BlendMode)
	{
		case BLEND_Opaque: BlendModeName = TEXT("BLEND_Opaque"); break;
		case BLEND_Masked: BlendModeName = TEXT("BLEND_Masked"); break;
		case BLEND_Translucent: BlendModeName = TEXT("BLEND_Translucent"); break;
		case BLEND_Additive: BlendModeName = TEXT("BLEND_Additive"); break;
		case BLEND_Modulate: BlendModeName = TEXT("BLEND_Modulate"); break;
		default: BlendModeName = TEXT("Unknown"); break;
	}
	return BlendModeName;
}

/** Called for every material shader to update the appropriate stats. */
void UpdateMaterialShaderCompilingStats(const FMaterial* Material)
{
	INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumTotalMaterialShaders,1);

	switch(Material->GetBlendMode())
	{
		case BLEND_Opaque: INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumOpaqueMaterialShaders,1); break;
		case BLEND_Masked: INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumMaskedMaterialShaders,1); break;
		default: INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumTransparentMaterialShaders,1); break;
	}

	switch(Material->GetLightingModel())
	{
		case MLM_Subsurface: 
		case MLM_PreintegratedSkin: 
		case MLM_DefaultLit: INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumLitMaterialShaders,1); break;
		case MLM_Unlit: INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumUnlitMaterialShaders,1); break;
		default: break;
	};

	if (Material->IsSpecialEngineMaterial())
	{
		INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumSpecialMaterialShaders,1);
	}
	if (Material->IsUsedWithParticleSystem())
	{
		INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumParticleMaterialShaders,1);
	}
	if (Material->IsUsedWithSkeletalMesh())
	{
		INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumSkinnedMaterialShaders,1);
	}
}

void FStaticParameterSet::UpdateHash(FSHA1& HashState) const
{
	for (int32 ParamIndex = 0; ParamIndex < StaticSwitchParameters.Num(); ParamIndex++)
	{
		const FStaticSwitchParameter& SwitchParameter = StaticSwitchParameters[ParamIndex];
		const FString ParameterName = SwitchParameter.ParameterName.ToString();

		HashState.Update((const uint8*)*ParameterName, ParameterName.Len() * sizeof(TCHAR));
		HashState.Update((const uint8*)&SwitchParameter.ExpressionGUID, sizeof(SwitchParameter.ExpressionGUID));
		HashState.Update((const uint8*)&SwitchParameter.Value, sizeof(SwitchParameter.Value));
	}

	for (int32 ParamIndex = 0; ParamIndex < StaticComponentMaskParameters.Num(); ParamIndex++)
	{
		const FStaticComponentMaskParameter& ComponentMaskParameter = StaticComponentMaskParameters[ParamIndex];
		const FString ParameterName = ComponentMaskParameter.ParameterName.ToString();

		HashState.Update((const uint8*)*ParameterName, ParameterName.Len() * sizeof(TCHAR));
		HashState.Update((const uint8*)&ComponentMaskParameter.ExpressionGUID, sizeof(ComponentMaskParameter.ExpressionGUID));
		HashState.Update((const uint8*)&ComponentMaskParameter.R, sizeof(ComponentMaskParameter.R));
		HashState.Update((const uint8*)&ComponentMaskParameter.G, sizeof(ComponentMaskParameter.G));
		HashState.Update((const uint8*)&ComponentMaskParameter.B, sizeof(ComponentMaskParameter.B));
		HashState.Update((const uint8*)&ComponentMaskParameter.A, sizeof(ComponentMaskParameter.A));
	}

	for (int32 ParamIndex = 0; ParamIndex < TerrainLayerWeightParameters.Num(); ParamIndex++)
	{
		const FStaticTerrainLayerWeightParameter& TerrainLayerWeightParameter = TerrainLayerWeightParameters[ParamIndex];
		const FString ParameterName = TerrainLayerWeightParameter.ParameterName.ToString();

		HashState.Update((const uint8*)*ParameterName, ParameterName.Len() * sizeof(TCHAR));
		HashState.Update((const uint8*)&TerrainLayerWeightParameter.ExpressionGUID, sizeof(TerrainLayerWeightParameter.ExpressionGUID));
		HashState.Update((const uint8*)&TerrainLayerWeightParameter.WeightmapIndex, sizeof(TerrainLayerWeightParameter.WeightmapIndex));
	}
}

/** 
* Indicates whether two static parameter sets are unequal.  This takes into account parameter override settings.
* 
* @param ReferenceSet	The set to compare against
* @return				true if the sets are not equal
*/
bool FStaticParameterSet::ShouldMarkDirty(const FStaticParameterSet* ReferenceSet)
{
	if (ReferenceSet->StaticSwitchParameters.Num() != StaticSwitchParameters.Num()
		|| ReferenceSet->StaticComponentMaskParameters.Num() != StaticComponentMaskParameters.Num()
		|| ReferenceSet->TerrainLayerWeightParameters.Num() !=TerrainLayerWeightParameters.Num())
	{
		return true;
	}

	//switch parameters
	for (int32 RefParamIndex = 0;RefParamIndex < ReferenceSet->StaticSwitchParameters.Num();RefParamIndex++)
	{
		const FStaticSwitchParameter * ReferenceSwitchParameter = &ReferenceSet->StaticSwitchParameters[RefParamIndex];
		for (int32 ParamIndex = 0;ParamIndex < StaticSwitchParameters.Num();ParamIndex++)
		{
			FStaticSwitchParameter * SwitchParameter = &StaticSwitchParameters[ParamIndex];
			if (SwitchParameter->ParameterName == ReferenceSwitchParameter->ParameterName
				&& SwitchParameter->ExpressionGUID == ReferenceSwitchParameter->ExpressionGUID)
			{
				SwitchParameter->bOverride = ReferenceSwitchParameter->bOverride;
				if (SwitchParameter->Value != ReferenceSwitchParameter->Value)
				{
					return true;
				}
			}
		}
	}

	//component mask parameters
	for (int32 RefParamIndex = 0;RefParamIndex < ReferenceSet->StaticComponentMaskParameters.Num();RefParamIndex++)
	{
		const FStaticComponentMaskParameter * ReferenceComponentMaskParameter = &ReferenceSet->StaticComponentMaskParameters[RefParamIndex];
		for (int32 ParamIndex = 0;ParamIndex < StaticComponentMaskParameters.Num();ParamIndex++)
		{
			FStaticComponentMaskParameter * ComponentMaskParameter = &StaticComponentMaskParameters[ParamIndex];
			if (ComponentMaskParameter->ParameterName == ReferenceComponentMaskParameter->ParameterName
				&& ComponentMaskParameter->ExpressionGUID == ReferenceComponentMaskParameter->ExpressionGUID)
			{
				ComponentMaskParameter->bOverride = ReferenceComponentMaskParameter->bOverride;
				if (ComponentMaskParameter->R != ReferenceComponentMaskParameter->R
					|| ComponentMaskParameter->G != ReferenceComponentMaskParameter->G
					|| ComponentMaskParameter->B != ReferenceComponentMaskParameter->B
					|| ComponentMaskParameter->A != ReferenceComponentMaskParameter->A)
				{
					return true;
				}
			}
		}
	}

	// Terrain layer weight parameters
	for (int32 RefParamIndex = 0;RefParamIndex < ReferenceSet->TerrainLayerWeightParameters.Num();RefParamIndex++)
	{
		const FStaticTerrainLayerWeightParameter * ReferenceTerrainLayerWeightParameter  = &ReferenceSet->TerrainLayerWeightParameters[RefParamIndex];
		for (int32 ParamIndex = 0;ParamIndex < TerrainLayerWeightParameters.Num();ParamIndex++)
		{
			FStaticTerrainLayerWeightParameter * TerrainLayerWeightParameter = &TerrainLayerWeightParameters[ParamIndex];
			if (TerrainLayerWeightParameter->ParameterName == ReferenceTerrainLayerWeightParameter ->ParameterName
				&& TerrainLayerWeightParameter->ExpressionGUID == ReferenceTerrainLayerWeightParameter ->ExpressionGUID)
			{
				TerrainLayerWeightParameter->bOverride = ReferenceTerrainLayerWeightParameter ->bOverride;
				if (TerrainLayerWeightParameter->WeightmapIndex != ReferenceTerrainLayerWeightParameter->WeightmapIndex)
				{
					return true;
				}
			}
		}
	}

	return false;
}

FString FStaticParameterSet::GetSummaryString() const
{
	return FString::Printf(TEXT("(%u switches, %u masks, %u terrain layer weight params)"),
		StaticSwitchParameters.Num(),
		StaticComponentMaskParameters.Num(),
		TerrainLayerWeightParameters.Num()
		);
}

void FStaticParameterSet::AppendKeyString(FString& KeyString) const
{
	for (int32 ParamIndex = 0;ParamIndex < StaticSwitchParameters.Num();ParamIndex++)
	{
		const FStaticSwitchParameter& SwitchParameter = StaticSwitchParameters[ParamIndex];
		KeyString += SwitchParameter.ParameterName.ToString() + SwitchParameter.ExpressionGUID.ToString() + FString::FromInt(SwitchParameter.Value);
	}

	for (int32 ParamIndex = 0;ParamIndex < StaticComponentMaskParameters.Num();ParamIndex++)
	{
		const FStaticComponentMaskParameter& ComponentMaskParameter = StaticComponentMaskParameters[ParamIndex];
		KeyString += ComponentMaskParameter.ParameterName.ToString() 
			+ ComponentMaskParameter.ExpressionGUID.ToString() 
			+ FString::FromInt(ComponentMaskParameter.R)
			+ FString::FromInt(ComponentMaskParameter.G)
			+ FString::FromInt(ComponentMaskParameter.B)
			+ FString::FromInt(ComponentMaskParameter.A);
	}

	for (int32 ParamIndex = 0;ParamIndex < TerrainLayerWeightParameters.Num();ParamIndex++)
	{
		const FStaticTerrainLayerWeightParameter& TerrainLayerWeightParameter = TerrainLayerWeightParameters[ParamIndex];
		KeyString += TerrainLayerWeightParameter.ParameterName.ToString() 
			+ TerrainLayerWeightParameter.ExpressionGUID.ToString() 
			+ FString::FromInt(TerrainLayerWeightParameter.WeightmapIndex);
	}
}

/** 
* Tests this set against another for equality, disregarding override settings.
* 
* @param ReferenceSet	The set to compare against
* @return				true if the sets are equal
*/
bool FStaticParameterSet::operator==(const FStaticParameterSet& ReferenceSet) const
{
	if (StaticSwitchParameters.Num() == ReferenceSet.StaticSwitchParameters.Num()
		&& StaticComponentMaskParameters.Num() == ReferenceSet.StaticComponentMaskParameters.Num()
		&& TerrainLayerWeightParameters.Num() == ReferenceSet.TerrainLayerWeightParameters.Num() )
	{
		for (int32 SwitchIndex = 0; SwitchIndex < StaticSwitchParameters.Num(); SwitchIndex++)
		{
			if (StaticSwitchParameters[SwitchIndex].ParameterName != ReferenceSet.StaticSwitchParameters[SwitchIndex].ParameterName
				|| StaticSwitchParameters[SwitchIndex].ExpressionGUID != ReferenceSet.StaticSwitchParameters[SwitchIndex].ExpressionGUID
				|| StaticSwitchParameters[SwitchIndex].Value != ReferenceSet.StaticSwitchParameters[SwitchIndex].Value)
			{
				return false;
			}
		}

		for (int32 ComponentMaskIndex = 0; ComponentMaskIndex < StaticComponentMaskParameters.Num(); ComponentMaskIndex++)
		{
			if (StaticComponentMaskParameters[ComponentMaskIndex].ParameterName != ReferenceSet.StaticComponentMaskParameters[ComponentMaskIndex].ParameterName
				|| StaticComponentMaskParameters[ComponentMaskIndex].ExpressionGUID != ReferenceSet.StaticComponentMaskParameters[ComponentMaskIndex].ExpressionGUID
				|| StaticComponentMaskParameters[ComponentMaskIndex].R != ReferenceSet.StaticComponentMaskParameters[ComponentMaskIndex].R
				|| StaticComponentMaskParameters[ComponentMaskIndex].G != ReferenceSet.StaticComponentMaskParameters[ComponentMaskIndex].G
				|| StaticComponentMaskParameters[ComponentMaskIndex].B != ReferenceSet.StaticComponentMaskParameters[ComponentMaskIndex].B
				|| StaticComponentMaskParameters[ComponentMaskIndex].A != ReferenceSet.StaticComponentMaskParameters[ComponentMaskIndex].A)
			{
				return false;
			}
		}

		for (int32 TerrainLayerWeightIndex = 0; TerrainLayerWeightIndex < TerrainLayerWeightParameters.Num(); TerrainLayerWeightIndex++)
		{
			if (TerrainLayerWeightParameters[TerrainLayerWeightIndex].ParameterName != ReferenceSet.TerrainLayerWeightParameters[TerrainLayerWeightIndex].ParameterName
				|| TerrainLayerWeightParameters[TerrainLayerWeightIndex].ExpressionGUID != ReferenceSet.TerrainLayerWeightParameters[TerrainLayerWeightIndex].ExpressionGUID
				|| TerrainLayerWeightParameters[TerrainLayerWeightIndex].WeightmapIndex != ReferenceSet.TerrainLayerWeightParameters[TerrainLayerWeightIndex].WeightmapIndex)
			{
				return false;
			}
		}

		return true;
	}
	return false;
}

void FMaterialShaderMapId::Serialize(FArchive& Ar)
{
	// Note: FMaterialShaderMapId is saved both in packages (legacy UMaterialInstance) and the DDC (FMaterialShaderMap)
	// Backwards compatibility only works with FMaterialShaderMapId's stored in packages.  
	// You must bump MATERIALSHADERMAP_DERIVEDDATA_VER as well if changing the serialization of FMaterialShaderMapId.

	if (Ar.UE4Ver() >= VER_UE4_ADDED_MATERIALSHADERMAP_USAGE)
	{
		uint32 UsageInt = Usage;
		Ar << UsageInt;
		Usage = (EMaterialShaderMapUsage::Type)UsageInt;
	}

	Ar << BaseMaterialId;

	if (Ar.UE4Ver() >= VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS)
	{
		Ar << (int32&)QualityLevel;
		Ar << (int32&)FeatureLevel;
	}
	else if (Ar.UE4Ver() >= VER_UE4_MATERIAL_QUALITY_LEVEL_SWITCH)
	{
		uint8 LegacyQualityLevel;
		Ar << LegacyQualityLevel;
	}

	ParameterSet.Serialize(Ar);

	if (Ar.UE4Ver() >= VER_UE4_FUNCTIONS_IN_SHADERMAPID)
	{
		Ar << ReferencedFunctions;
	}

	if (Ar.UE4Ver() >= VER_UE4_COLLECTIONS_IN_SHADERMAPID)
	{
		Ar << ReferencedParameterCollections;
	}

	if (Ar.UE4Ver() >= VER_UE4_REMOVED_PERSHADER_DDC)
	{
		Ar << ShaderTypeDependencies;
		Ar << VertexFactoryTypeDependencies;
	}

	if (Ar.UE4Ver() >= VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS)
	{
		Ar << TextureReferencesHash;
	}
	else if (Ar.UE4Ver() >= VER_UE4_HASHED_MATERIAL_OUTPUT)
	{
		FSHAHash LegacyHash;
		Ar << LegacyHash;
	}

	if( Ar.UE4Ver() >= VER_UE4_MATERIAL_INSTANCE_BASE_PROPERTY_OVERRIDES)
	{
		Ar << BasePropertyOverridesHash;
	}
}

/** Hashes the material-specific part of this shader map Id. */
void FMaterialShaderMapId::GetMaterialHash(FSHAHash& OutHash) const
{
	FSHA1 HashState;

	HashState.Update((const uint8*)&Usage, sizeof(Usage));

	HashState.Update((const uint8*)&BaseMaterialId, sizeof(BaseMaterialId));

	FString QualityLevelString;
	GetMaterialQualityLevelName(QualityLevel, QualityLevelString);
	HashState.UpdateWithString(*QualityLevelString, QualityLevelString.Len());

	HashState.Update((const uint8*)&FeatureLevel, sizeof(FeatureLevel));

	ParameterSet.UpdateHash(HashState);

	for (int32 FunctionIndex = 0; FunctionIndex < ReferencedFunctions.Num(); FunctionIndex++)
	{
		HashState.Update((const uint8*)&ReferencedFunctions[FunctionIndex], sizeof(ReferencedFunctions[FunctionIndex]));
	}

	for (int32 CollectionIndex = 0; CollectionIndex < ReferencedParameterCollections.Num(); CollectionIndex++)
	{
		HashState.Update((const uint8*)&ReferencedParameterCollections[CollectionIndex], sizeof(ReferencedParameterCollections[CollectionIndex]));
	}

	HashState.Update((const uint8*)&TextureReferencesHash, sizeof(TextureReferencesHash));

	HashState.Update((const uint8*)&BasePropertyOverridesHash, sizeof(BasePropertyOverridesHash));

	HashState.Final();
	HashState.GetHash(&OutHash.Hash[0]);
}

/** 
* Tests this set against another for equality, disregarding override settings.
* 
* @param ReferenceSet	The set to compare against
* @return				true if the sets are equal
*/
bool FMaterialShaderMapId::operator==(const FMaterialShaderMapId& ReferenceSet) const
{
	if (Usage != ReferenceSet.Usage)
	{
		return false;
	}

	if (BaseMaterialId != ReferenceSet.BaseMaterialId 
		|| QualityLevel != ReferenceSet.QualityLevel
		|| FeatureLevel != ReferenceSet.FeatureLevel)
	{
		return false;
	}

	if (ParameterSet != ReferenceSet.ParameterSet
		|| ReferencedFunctions.Num() != ReferenceSet.ReferencedFunctions.Num()
		||  ReferencedParameterCollections.Num() != ReferenceSet.ReferencedParameterCollections.Num()
		|| ShaderTypeDependencies.Num() != ReferenceSet.ShaderTypeDependencies.Num()
		|| VertexFactoryTypeDependencies.Num() != ReferenceSet.VertexFactoryTypeDependencies.Num())
	{
		return false;
	}

	for (int32 RefFunctionIndex = 0; RefFunctionIndex < ReferenceSet.ReferencedFunctions.Num(); RefFunctionIndex++)
	{
		const FGuid& ReferenceGuid = ReferenceSet.ReferencedFunctions[RefFunctionIndex];

		if (ReferencedFunctions[RefFunctionIndex] != ReferenceGuid)
		{
			return false;
		}
	}

	for (int32 RefCollectionIndex = 0; RefCollectionIndex < ReferenceSet.ReferencedParameterCollections.Num(); RefCollectionIndex++)
	{
		const FGuid& ReferenceGuid = ReferenceSet.ReferencedParameterCollections[RefCollectionIndex];

		if (ReferencedParameterCollections[RefCollectionIndex] != ReferenceGuid)
		{
			return false;
		}
	}

	for (int32 ShaderIndex = 0; ShaderIndex < ShaderTypeDependencies.Num(); ShaderIndex++)
	{
		const FShaderTypeDependency& ShaderTypeDependency = ShaderTypeDependencies[ShaderIndex];
		
		if (ShaderTypeDependency.ShaderType != ReferenceSet.ShaderTypeDependencies[ShaderIndex].ShaderType
			|| ShaderTypeDependency.SourceHash != ReferenceSet.ShaderTypeDependencies[ShaderIndex].SourceHash)
		{
			return false;
		}
	}

	for (int32 VFIndex = 0; VFIndex < VertexFactoryTypeDependencies.Num(); VFIndex++)
	{
		const FVertexFactoryTypeDependency& VFDependency = VertexFactoryTypeDependencies[VFIndex];
		
		if (VFDependency.VertexFactoryType != ReferenceSet.VertexFactoryTypeDependencies[VFIndex].VertexFactoryType
			|| VFDependency.VFSourceHash != ReferenceSet.VertexFactoryTypeDependencies[VFIndex].VFSourceHash)
		{
			return false;
		}
	}

	if (TextureReferencesHash != ReferenceSet.TextureReferencesHash)
	{
		return false;
	}

	if( BasePropertyOverridesHash != ReferenceSet.BasePropertyOverridesHash )
	{
		return false;
	}

	return true;
}

void FMaterialShaderMapId::AppendKeyString(FString& KeyString) const
{
	KeyString += BaseMaterialId.ToString();
	KeyString += TEXT("_");

	FString QualityLevelName;
	GetMaterialQualityLevelName(QualityLevel, QualityLevelName);
	KeyString += QualityLevelName + TEXT("_");

	FString FeatureLevelString;
	GetFeatureLevelName(FeatureLevel, FeatureLevelString);
	KeyString += FeatureLevelString + TEXT("_");

	ParameterSet.AppendKeyString(KeyString);

	KeyString += TEXT("_");
	KeyString += FString::FromInt(Usage);
	KeyString += TEXT("_");

	// Add any referenced functions to the key so that we will recompile when they are changed
	for (int32 FunctionIndex = 0; FunctionIndex < ReferencedFunctions.Num(); FunctionIndex++)
	{
		KeyString += ReferencedFunctions[FunctionIndex].ToString();
	}

	KeyString += TEXT("_");

	for (int32 CollectionIndex = 0; CollectionIndex < ReferencedParameterCollections.Num(); CollectionIndex++)
	{
		KeyString += ReferencedParameterCollections[CollectionIndex].ToString();
	}

	TMap<const TCHAR*,FCachedUniformBufferDeclaration> ReferencedUniformBuffers;

	// Add the inputs for any shaders that are stored inline in the shader map
	for (int32 ShaderIndex = 0; ShaderIndex < ShaderTypeDependencies.Num(); ShaderIndex++)
	{
		KeyString += TEXT("_");

		const FShaderTypeDependency& ShaderTypeDependency = ShaderTypeDependencies[ShaderIndex];
		KeyString += ShaderTypeDependency.ShaderType->GetName();
		KeyString += ShaderTypeDependency.SourceHash.ToString();
		ShaderTypeDependency.ShaderType->GetSerializationHistory().AppendKeyString(KeyString);

		const TMap<const TCHAR*,FCachedUniformBufferDeclaration>& ReferencedUniformBufferStructsCache = ShaderTypeDependency.ShaderType->GetReferencedUniformBufferStructsCache();

		for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TConstIterator It(ReferencedUniformBufferStructsCache); It; ++It)
		{
			ReferencedUniformBuffers.Add(It.Key(), It.Value());
		}
	}

	// Add the inputs for any shaders that are stored inline in the shader map
	for (int32 VFIndex = 0; VFIndex < VertexFactoryTypeDependencies.Num(); VFIndex++)
	{
		KeyString += TEXT("_");

		const FVertexFactoryTypeDependency& VFDependency = VertexFactoryTypeDependencies[VFIndex];
		KeyString += VFDependency.VertexFactoryType->GetName();
		KeyString += VFDependency.VFSourceHash.ToString();

		for (int32 Frequency = 0; Frequency < SF_NumFrequencies; Frequency++)
		{
			VFDependency.VertexFactoryType->GetSerializationHistory((EShaderFrequency)Frequency)->AppendKeyString(KeyString);
		}

		const TMap<const TCHAR*,FCachedUniformBufferDeclaration>& ReferencedUniformBufferStructsCache = VFDependency.VertexFactoryType->GetReferencedUniformBufferStructsCache();

		for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TConstIterator It(ReferencedUniformBufferStructsCache); It; ++It)
		{
			ReferencedUniformBuffers.Add(It.Key(), It.Value());
		}
	}

	{
		TArray<uint8> TempData;
		FSerializationHistory SerializationHistory;
		FMemoryWriter Ar(TempData, true);
		FShaderSaveArchive SaveArchive(Ar, SerializationHistory);

		// Save uniform buffer member info so we can detect when layout has changed
		SerializeUniformBufferInfo(SaveArchive, ReferencedUniformBuffers);

		SerializationHistory.AppendKeyString(KeyString);
	}

	KeyString += BytesToHex(&TextureReferencesHash.Hash[0], sizeof(TextureReferencesHash.Hash));

	KeyString += BytesToHex(&BasePropertyOverridesHash.Hash[0], sizeof(BasePropertyOverridesHash.Hash));
}

void FMaterialShaderMapId::SetShaderDependencies(const TArray<FShaderType*>& ShaderTypes, const TArray<FVertexFactoryType*>& VFTypes)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		for (int32 ShaderTypeIndex = 0; ShaderTypeIndex < ShaderTypes.Num(); ShaderTypeIndex++)
		{
			FShaderTypeDependency Dependency;
			Dependency.ShaderType = ShaderTypes[ShaderTypeIndex];
			Dependency.SourceHash = ShaderTypes[ShaderTypeIndex]->GetSourceHash();
			ShaderTypeDependencies.Add(Dependency);
		}

		for (int32 VFTypeIndex = 0; VFTypeIndex < VFTypes.Num(); VFTypeIndex++)
		{
			FVertexFactoryTypeDependency Dependency;
			Dependency.VertexFactoryType = VFTypes[VFTypeIndex];
			Dependency.VFSourceHash = VFTypes[VFTypeIndex]->GetSourceHash();
			VertexFactoryTypeDependencies.Add(Dependency);
		}
	}
}

/**
* Finds a FMaterialShaderType by name.
*/
FMaterialShaderType* FMaterialShaderType::GetTypeByName(const FString& TypeName)
{
	for(TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next())
	{
		FString CurrentTypeName = FString(It->GetName());
		FMaterialShaderType* CurrentType = It->GetMaterialShaderType();
		if (CurrentType && CurrentTypeName == TypeName)
		{
			return CurrentType;
		}
	}
	return NULL;
}

/**
 * Enqueues a compilation for a new shader of this type.
 * @param Material - The material to link the shader with.
 */
void FMaterialShaderType::BeginCompileShader(
	uint32 ShaderMapId,
	const FMaterial* Material,
	FShaderCompilerEnvironment* MaterialEnvironment,
	EShaderPlatform Platform,
	TArray<FShaderCompileJob*>& NewJobs
	)
{
	FShaderCompileJob* NewJob = new FShaderCompileJob(ShaderMapId, NULL, this);

	NewJob->Input.SharedEnvironment = MaterialEnvironment;
	FShaderCompilerEnvironment& ShaderEnvironment = NewJob->Input.Environment;

	UE_LOG(LogShaders, Verbose, TEXT("			%s"), GetName());

	//update material shader stats
	UpdateMaterialShaderCompilingStats(Material);

	// Allow the shader type to modify the compile environment.
	SetupCompileEnvironment(Platform, Material, ShaderEnvironment);

	// Compile the shader environment passed in with the shader type's source code.
	::GlobalBeginCompileShader(
		Material->GetFriendlyName(),
		NULL,
		this,
		GetShaderFilename(),
		GetFunctionName(),
		FShaderTarget(GetFrequency(),Platform),
		NewJob,
		NewJobs
		);
}

/**
 * Either creates a new instance of this type or returns an equivalent existing shader.
 * @param Material - The material to link the shader with.
 * @param CurrentJob - Compile job that was enqueued by BeginCompileShader.
 */
FShader* FMaterialShaderType::FinishCompileShader(
	const FUniformExpressionSet& UniformExpressionSet,
	const FSHAHash& MaterialShaderMapHash,
	const FShaderCompileJob& CurrentJob,
	const FString& InDebugDescription
	)
{
	check(CurrentJob.bSucceeded);

	// Reuse an existing resource with the same key or create a new one based on the compile output
	// This allows FShaders to share compiled bytecode and RHI shader references
	FShaderResource* Resource = FShaderResource::FindOrCreateShaderResource(CurrentJob.Output);

	// Find a shader with the same key in memory
	FShader* Shader = CurrentJob.ShaderType->FindShaderById(FShaderId(MaterialShaderMapHash, CurrentJob.VFType, CurrentJob.ShaderType, CurrentJob.Input.Target));

	// There was no shader with the same key so create a new one with the compile output, which will bind shader parameters
	if (!Shader)
	{
		Shader = (*ConstructCompiledRef)(CompiledShaderInitializerType(this, CurrentJob.Output, Resource, UniformExpressionSet, MaterialShaderMapHash, NULL, InDebugDescription));
		CurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(GetName(), (EShaderFrequency)CurrentJob.Output.Target.Frequency, CurrentJob.VFType);
	}

	return Shader;
}

/**
* Finds the shader map for a material.
* @param StaticParameterSet - The static parameter set identifying the shader map
* @param Platform - The platform to lookup for
* @return NULL if no cached shader map was found.
*/
FMaterialShaderMap* FMaterialShaderMap::FindId(const FMaterialShaderMapId& ShaderMapId, EShaderPlatform InPlatform)
{
	check(ShaderMapId.BaseMaterialId != FGuid());
	return GIdToMaterialShaderMap[InPlatform].FindRef(ShaderMapId);
}

/** Flushes the given shader types from any loaded FMaterialShaderMap's. */
void FMaterialShaderMap::FlushShaderTypes(TArray<FShaderType*>& ShaderTypesToFlush, TArray<const FVertexFactoryType*>& VFTypesToFlush)
{
	for (int32 ShaderMapIndex = 0; ShaderMapIndex < AllMaterialShaderMaps.Num(); ShaderMapIndex++)
	{
		FMaterialShaderMap* CurrentShaderMap = AllMaterialShaderMaps[ShaderMapIndex];

		for (int32 ShaderTypeIndex = 0; ShaderTypeIndex < ShaderTypesToFlush.Num(); ShaderTypeIndex++)
		{
			CurrentShaderMap->FlushShadersByShaderType(ShaderTypesToFlush[ShaderTypeIndex]);
		}
		for (int32 VFTypeIndex = 0; VFTypeIndex < VFTypesToFlush.Num(); VFTypeIndex++)
		{
			CurrentShaderMap->FlushShadersByVertexFactoryType(VFTypesToFlush[VFTypeIndex]);
		}
	}
}

void FMaterialShaderMap::FixupShaderTypes(EShaderPlatform Platform, const TMap<FShaderType*, FString>& ShaderTypeNames, const TMap<FVertexFactoryType*, FString>& VertexFactoryTypeNames)
{
	TArray<FMaterialShaderMapId> Keys;
	FMaterialShaderMap::GIdToMaterialShaderMap[Platform].GenerateKeyArray(Keys);

	TArray<FMaterialShaderMap*> Values;
	FMaterialShaderMap::GIdToMaterialShaderMap[Platform].GenerateValueArray(Values);

	//@todo - what about the shader maps in AllMaterialShaderMaps that are not in GIdToMaterialShaderMap?
	FMaterialShaderMap::GIdToMaterialShaderMap[Platform].Empty();

	for (int32 PairIndex = 0; PairIndex < Keys.Num(); PairIndex++)
	{
		FMaterialShaderMapId& Key = Keys[PairIndex];

		for (int32 ShaderTypeIndex = 0; ShaderTypeIndex < Key.ShaderTypeDependencies.Num(); ShaderTypeIndex++)
		{
			const FString& ShaderTypeName = ShaderTypeNames.FindChecked(Key.ShaderTypeDependencies[ShaderTypeIndex].ShaderType);
			FShaderType* FoundShaderType = FShaderType::GetShaderTypeByName(*ShaderTypeName);
			Key.ShaderTypeDependencies[ShaderTypeIndex].ShaderType = FoundShaderType;
		}

		for (int32 VFTypeIndex = 0; VFTypeIndex < Key.VertexFactoryTypeDependencies.Num(); VFTypeIndex++)
		{
			const FString& VFTypeName = VertexFactoryTypeNames.FindChecked(Key.VertexFactoryTypeDependencies[VFTypeIndex].VertexFactoryType);
			FVertexFactoryType* FoundVFType = FVertexFactoryType::GetVFByName(VFTypeName);
			Key.VertexFactoryTypeDependencies[VFTypeIndex].VertexFactoryType = FoundVFType;
		}

		FMaterialShaderMap::GIdToMaterialShaderMap[Platform].Add(Key, Values[PairIndex]);
	}
}

/** Creates a string key for the derived data cache given a shader map id. */
FString GetMaterialShaderMapKeyString(const FMaterialShaderMapId& ShaderMapId, EShaderPlatform Platform)
{
	FName Format = LegacyShaderPlatformToShaderFormat(Platform);
	FString ShaderMapKeyString = Format.ToString() + TEXT("_") + FString(FString::FromInt(GetTargetPlatformManagerRef().ShaderFormatVersion(Format))) + TEXT("_");
	ShaderMapAppendKeyString(ShaderMapKeyString);
	ShaderMapId.AppendKeyString(ShaderMapKeyString);
	return FDerivedDataCacheInterface::BuildCacheKey(TEXT("MATSM"), MATERIALSHADERMAP_DERIVEDDATA_VER, *ShaderMapKeyString);
}

void FMaterialShaderMap::LoadFromDerivedDataCache(const FMaterial* Material, const FMaterialShaderMapId& ShaderMapId, EShaderPlatform Platform, TRefCountPtr<FMaterialShaderMap>& InOutShaderMap)
{
	if (InOutShaderMap != NULL)
	{
		check(InOutShaderMap->Platform == Platform);
		// If the shader map was non-NULL then it was found in memory but is incomplete, attempt to load the missing entries from memory
		InOutShaderMap->LoadMissingShadersFromMemory(Material);
	}
	else
	{
		// Shader map was not found in memory, try to load it from the DDC
		STAT(double MaterialDDCTime = 0);
		{
			SCOPE_SECONDS_COUNTER(MaterialDDCTime);

			TArray<uint8> CachedData;
			const FString DataKey = GetMaterialShaderMapKeyString(ShaderMapId, Platform);

			// Find the shader map in the derived data cache
			if (GetDerivedDataCacheRef().GetSynchronous(*DataKey, CachedData))
			{
				InOutShaderMap = new FMaterialShaderMap();
				FMemoryReader Ar(CachedData, true);

				// Deserialize from the cached data
				InOutShaderMap->Serialize(Ar);
				checkSlow(InOutShaderMap->GetShaderMapId() == ShaderMapId);

				// Register in the global map
				InOutShaderMap->Register();
			}
			else
			{
				InOutShaderMap = NULL;
			}
		}
		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_DDCLoading,(float)MaterialDDCTime);
	}
}

void FMaterialShaderMap::SaveToDerivedDataCache()
{
	TArray<uint8> SaveData;
	FMemoryWriter Ar(SaveData, true);
	Serialize(Ar);

	GetDerivedDataCacheRef().Put(*GetMaterialShaderMapKeyString(ShaderMapId, Platform), SaveData);
}

TArray<uint8>* FMaterialShaderMap::BackupShadersToMemory()
{
	TArray<uint8>* SavedShaderData = new TArray<uint8>();
	FMemoryWriter Ar(*SavedShaderData);
	
	for (int32 Index = 0; Index < MeshShaderMaps.Num(); Index++)
	{
		// Serialize data needed to handle shader key changes in between the save and the load of the FShaders
		const bool bHandleShaderKeyChanges = true;
		MeshShaderMaps[Index].SerializeInline(Ar, true, bHandleShaderKeyChanges);
		MeshShaderMaps[Index].Empty();
	}

	SerializeInline(Ar, true, true);
	Empty();

	return SavedShaderData;
}

void FMaterialShaderMap::RestoreShadersFromMemory(const TArray<uint8>& ShaderData)
{
	FMemoryReader Ar(ShaderData);

	for (int32 Index = 0; Index < MeshShaderMaps.Num(); Index++)
	{
		// Use the serialized shader key data to detect when the saved shader is no longer valid and skip it
		const bool bHandleShaderKeyChanges = true;
		MeshShaderMaps[Index].SerializeInline(Ar, true, bHandleShaderKeyChanges);
	}

	SerializeInline(Ar, true, true);
}

void FMaterialShaderMap::SaveForRemoteRecompile(FArchive& Ar, const TMap<FString, TArray<TRefCountPtr<FMaterialShaderMap> > >& CompiledShaderMaps, const TArray<FShaderResourceId>& ClientResourceIds)
{
	UE_LOG(LogMaterial, Display, TEXT("Looking for unique resources, %d were on client"), ClientResourceIds.Num());	

	// first, we look for the unique shader resources
	TArray<FShaderResource*> UniqueResources;
	int32 NumSkippedResources = 0;

	for (TMap<FString, TArray<TRefCountPtr<FMaterialShaderMap> > >::TConstIterator It(CompiledShaderMaps); It; ++It)
	{
		const TArray<TRefCountPtr<FMaterialShaderMap> >& ShaderMapArray = It.Value();

		for (int32 Index = 0; Index < ShaderMapArray.Num(); Index++)
		{
			FMaterialShaderMap* ShaderMap = ShaderMapArray[Index];

			if (ShaderMap)
			{
				// get all shaders in the shader map
				TMap<FShaderId,FShader*> Shaders;
				ShaderMap->GetShaderList(Shaders);

				// get the resources from the shaders
				for (TMap<FShaderId, FShader*>::TIterator ShaderIt(Shaders); ShaderIt; ++ShaderIt)
				{
					FShader *Shader = ShaderIt.Value();
					FShaderResourceId ShaderId = Shader->GetResourceId();

					// skip this shader if the Id was already on the client (ie, it didn't change)
					if (ClientResourceIds.Contains(ShaderId) == false )
					{
						// lookup the resource by ID
						FShaderResource* Resource = FShaderResource::FindShaderResourceById(ShaderId);
						// add it if it's unique
						UniqueResources.AddUnique(Resource);
					}
					else
					{
						NumSkippedResources++;
					}
				}
			}
		}
	}

	UE_LOG(LogMaterial, Display, TEXT("Sending %d new shader resources, skipped %d existing"), UniqueResources.Num(), NumSkippedResources);

	// now serialize them
	int32 NumUniqueResources = UniqueResources.Num();
	Ar << NumUniqueResources;

	for (int32 Index = 0; Index < NumUniqueResources; Index++)
	{
		UniqueResources[Index]->Serialize(Ar);
	}

	// now we serialize a map (for each material), but without inline the resources, since they are above
	int32 MapSize = CompiledShaderMaps.Num();
	Ar << MapSize;

	for (TMap<FString, TArray<TRefCountPtr<FMaterialShaderMap> > >::TConstIterator It(CompiledShaderMaps); It; ++It)
	{
		const TArray<TRefCountPtr<FMaterialShaderMap> >& ShaderMapArray = It.Value();

		FString MaterialName = It.Key();
		Ar << MaterialName;

		int32 NumShaderMaps = ShaderMapArray.Num();
		Ar << NumShaderMaps;

		for (int32 Index = 0; Index < ShaderMapArray.Num(); Index++)
		{
			FMaterialShaderMap* ShaderMap = ShaderMapArray[Index];

			if (ShaderMap && NumUniqueResources > 0)
			{
				uint8 bIsValid = 1;
				Ar << bIsValid;
				ShaderMap->Serialize(Ar, false);
			}
			else
			{
				uint8 bIsValid = 0;
				Ar << bIsValid;
			}
		}
	}
}

void FMaterialShaderMap::LoadForRemoteRecompile(FArchive& Ar, EShaderPlatform ShaderPlatform, const TArray<FString>& MaterialsForShaderMaps)
{
	int32 NumResources;
	Ar << NumResources;

	// KeepAliveReferences keeps resources alive until we are finished serializing in this function
	TArray<TRefCountPtr<FShaderResource> > KeepAliveReferences;

	// load and register the resources
	for (int32 Index = 0; Index < NumResources; Index++)
	{
		// Load the inlined shader resource
		FShaderResource* Resource = new FShaderResource();
		Resource->Serialize(Ar);

		// if this Id is already in memory, that means that this is a repeated resource and so we skip it
		if (FShaderResource::FindShaderResourceById(Resource->GetId()) != NULL)
		{
			delete Resource;
		}
		// otherwise, it's a new resource, so we register it for the maps to find below
		else
		{
			Resource->Register();

			// Keep this guy alive until we finish serializing all the FShaders in
			// The FShaders which are discarded may cause these resources to be discarded 
			KeepAliveReferences.Add( Resource );
		}
	}

	int32 MapSize;
	Ar << MapSize;

	for (int32 MaterialIndex = 0; MaterialIndex < MapSize; MaterialIndex++)
	{
		FString MaterialName;
		Ar << MaterialName;

		UMaterialInterface* MatchingMaterial = FindObjectChecked<UMaterialInterface>(NULL, *MaterialName);

		int32 NumShaderMaps = 0;
		Ar << NumShaderMaps;

		TArray<TRefCountPtr<FMaterialShaderMap> > LoadedShaderMaps;

		for (int32 ShaderMapIndex = 0; ShaderMapIndex < NumShaderMaps; ShaderMapIndex++)
		{
			uint8 bIsValid = 0;
			Ar << bIsValid;

			if (bIsValid)
			{
				FMaterialShaderMap* ShaderMap = new FMaterialShaderMap;

				// serialize the id and the material shader map
				ShaderMap->Serialize(Ar, false);

				// Register in the global map
				ShaderMap->Register();

				LoadedShaderMaps.Add(ShaderMap);
			}
		}

		// Assign in two passes: first pass for shader maps with unspecified quality levels,
		// Second pass for shader maps with a specific quality level
		for (int32 PassIndex = 0; PassIndex < 2; PassIndex++)
		{
			for (int32 ShaderMapIndex = 0; ShaderMapIndex < LoadedShaderMaps.Num(); ShaderMapIndex++)
			{
				FMaterialShaderMap* LoadedShaderMap = LoadedShaderMaps[ShaderMapIndex];

				if (LoadedShaderMap->GetShaderPlatform() == ShaderPlatform 
					&& LoadedShaderMap->GetShaderMapId().FeatureLevel == GRHIFeatureLevel)
				{
					EMaterialQualityLevel::Type LoadedQualityLevel = LoadedShaderMap->GetShaderMapId().QualityLevel;

					for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
					{
						// First pass: assign shader maps with unspecified quality levels to all material resources
						if ((PassIndex == 0 && LoadedQualityLevel == EMaterialQualityLevel::Num)
							// Second pass: assign shader maps with a specified quality level to only the appropriate material resource
							|| (PassIndex == 1 && QualityLevelIndex == LoadedQualityLevel))
						{
							FMaterialResource* MaterialResource = MatchingMaterial->GetMaterialResource(GRHIFeatureLevel, (EMaterialQualityLevel::Type)QualityLevelIndex);

							MaterialResource->SetGameThreadShaderMap(LoadedShaderMap);

							ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
								FSetShaderMapOnMaterialResources,
								FMaterial*,MaterialResource,MaterialResource,
								FMaterialShaderMap*,LoadedShaderMap,LoadedShaderMap,
							{
								MaterialResource->SetRenderingThreadShaderMap(LoadedShaderMap);
							});
						}
					}
				}
			}
		}
	}
}


/**
* Compiles the shaders for a material and caches them in this shader map.
* @param Material - The material to compile shaders for.
* @param InShaderMapId - the set of static parameters to compile for
* @param Platform - The platform to compile to
*/
void FMaterialShaderMap::Compile(
	FMaterial* Material,
	const FMaterialShaderMapId& InShaderMapId, 
	TRefCountPtr<FShaderCompilerEnvironment> MaterialEnvironment,
	const FMaterialCompilationOutput& InMaterialCompilationOutput,
	EShaderPlatform InPlatform,
	bool bSynchronousCompile,
	bool bApplyCompletedShaderMapForRendering)
{
	if (FPlatformProperties::RequiresCookedData())
	{
		UE_LOG(LogMaterial, Fatal, TEXT("Trying to compile %s at run-time, which is not supported on consoles!"), *Material->GetFriendlyName() );
	}
	else
	{
		check(!Material->bContainsInlineShaders);
  
		// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
		// Since it creates a temporary ref counted pointer.
		check(NumRefs > 0);
  
		// Add this shader map and material resource to ShaderMapsBeingCompiled
		TArray<FMaterial*>* CorrespondingMaterials = ShaderMapsBeingCompiled.Find(this);
  
		if (CorrespondingMaterials)
		{
			check(!bSynchronousCompile);
			CorrespondingMaterials->AddUnique(Material);
		}
		else
		{
			// Assign a unique identifier so that shaders from this shader map can be associated with it after a deferred compile
			CompilingId = NextCompilingId;
			check(NextCompilingId < UINT_MAX);
			NextCompilingId++;
  
			TArray<FMaterial*> NewCorrespondingMaterials;
			NewCorrespondingMaterials.Add(Material);
			ShaderMapsBeingCompiled.Add(this, NewCorrespondingMaterials);
  
			// Setup the material compilation environment.
			Material->SetupMaterialEnvironment(InPlatform, InMaterialCompilationOutput.UniformExpressionSet, *MaterialEnvironment);
  
			// Store the material name for debugging purposes.
			// Note: Material instances with static parameters will have the same FriendlyName for their shader maps!
			FriendlyName = Material->GetFriendlyName();
			MaterialCompilationOutput = InMaterialCompilationOutput;
			ShaderMapId = InShaderMapId;
			Platform = InPlatform;
			bIsPersistent = Material->IsPersistent();
		  
			// Log debug information about the material being compiled.
			const FString MaterialUsage = Material->GetMaterialUsageDescription();
			DebugDescription = FString::Printf(
				TEXT("Compiling %s: Platform=%s, Usage=%s"),
				*FriendlyName,
				*LegacyShaderPlatformToShaderFormat(InPlatform).ToString(),
				*MaterialUsage
				);
			for(int32 StaticSwitchIndex = 0;StaticSwitchIndex < InShaderMapId.ParameterSet.StaticSwitchParameters.Num();++StaticSwitchIndex)
			{
				const FStaticSwitchParameter& StaticSwitchParameter = InShaderMapId.ParameterSet.StaticSwitchParameters[StaticSwitchIndex];
				DebugDescription += FString::Printf(
					TEXT(", StaticSwitch'%s'=%s"),
					*StaticSwitchParameter.ParameterName.ToString(),
					StaticSwitchParameter.Value ? TEXT("True") : TEXT("False")
					);
			}
			for(int32 StaticMaskIndex = 0;StaticMaskIndex < InShaderMapId.ParameterSet.StaticComponentMaskParameters.Num();++StaticMaskIndex)
			{
				const FStaticComponentMaskParameter& StaticComponentMaskParameter = InShaderMapId.ParameterSet.StaticComponentMaskParameters[StaticMaskIndex];
				DebugDescription += FString::Printf(
					TEXT(", StaticMask'%s'=%s%s%s%s"),
					*StaticComponentMaskParameter.ParameterName.ToString(),
					StaticComponentMaskParameter.R ? TEXT("R") : TEXT(""),
					StaticComponentMaskParameter.G ? TEXT("G") : TEXT(""),
					StaticComponentMaskParameter.B ? TEXT("B") : TEXT(""),
					StaticComponentMaskParameter.A ? TEXT("A") : TEXT("")
					);
			}
			for(int32 StaticLayerIndex = 0;StaticLayerIndex < InShaderMapId.ParameterSet.TerrainLayerWeightParameters.Num();++StaticLayerIndex)
			{
				const FStaticTerrainLayerWeightParameter& StaticTerrainLayerWeightParameter = InShaderMapId.ParameterSet.TerrainLayerWeightParameters[StaticLayerIndex];
				DebugDescription += FString::Printf(
					TEXT(", StaticTerrainLayer'%s'=%s"),
					*StaticTerrainLayerWeightParameter.ParameterName.ToString(),
					*FString::Printf(TEXT("Weightmap%u"),StaticTerrainLayerWeightParameter.WeightmapIndex)
					);
			}
  
			UE_LOG(LogShaders, Warning, TEXT("	%s"), *DebugDescription);
  
			uint32 NumShaders = 0;
			uint32 NumVertexFactories = 0;
			TArray<FShaderCompileJob*> NewJobs;
  
			// Iterate over all vertex factory types.
			for(TLinkedList<FVertexFactoryType*>::TIterator VertexFactoryTypeIt(FVertexFactoryType::GetTypeList());VertexFactoryTypeIt;VertexFactoryTypeIt.Next())
			{
				FVertexFactoryType* VertexFactoryType = *VertexFactoryTypeIt;
				check(VertexFactoryType);
  
				if(VertexFactoryType->IsUsedWithMaterials())
				{
					FMeshMaterialShaderMap* MeshShaderMap = NULL;
  
					// look for existing map for this vertex factory type
					int32 MeshShaderMapIndex = INDEX_NONE;
					for (int32 ShaderMapIndex = 0; ShaderMapIndex < MeshShaderMaps.Num(); ShaderMapIndex++)
					{
						if (MeshShaderMaps[ShaderMapIndex].GetVertexFactoryType() == VertexFactoryType)
						{
							MeshShaderMap = &MeshShaderMaps[ShaderMapIndex];
							MeshShaderMapIndex = ShaderMapIndex;
							break;
						}
					}
  
					if (MeshShaderMap == NULL)
					{
						// Create a new mesh material shader map.
						MeshShaderMapIndex = MeshShaderMaps.Num();
						MeshShaderMap = new(MeshShaderMaps) FMeshMaterialShaderMap(VertexFactoryType);
					}
  
					// Enqueue compilation all mesh material shaders for this material and vertex factory type combo.
					const uint32 MeshShaders = MeshShaderMap->BeginCompile(
						CompilingId,
						InShaderMapId,
						Material,
						MaterialEnvironment,
						InPlatform,
						NewJobs
						);
					NumShaders += MeshShaders;
					if (MeshShaders > 0)
					{
						NumVertexFactories++;
					}
				}
			}
  
			// Iterate over all material shader types.
			for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
			{
				FMaterialShaderType* ShaderType = ShaderTypeIt->GetMaterialShaderType();
				if (ShaderType && 
					ShaderType->ShouldCache(InPlatform,Material) && 
					Material->ShouldCache(InPlatform, ShaderType, NULL)
					)
				{
					// Verify that the shader map Id contains inputs for any shaders that will be put into this shader map
					check(InShaderMapId.ContainsShaderType(ShaderType));
  
					// Compile this material shader for this material.
					TArray<FString> ShaderErrors;
  
					// Only compile the shader if we don't already have it
					if (!HasShader(ShaderType))
					{
						ShaderType->BeginCompileShader(
							CompilingId,
							Material,
							MaterialEnvironment,
							InPlatform,
							NewJobs
							);
					}
					NumShaders++;
				}
			}
  
			if (!CorrespondingMaterials)
			{
				UE_LOG(LogShaders, Warning, TEXT("		%u Shaders among %u VertexFactories"), NumShaders, NumVertexFactories);
			}

			// Register this shader map in the global map with the material's ID.
			Register();
  
			// Mark the shader map as not having been finalized with ProcessCompilationResults
			bCompilationFinalized = false;
  
			// Mark as not having been compiled
			bCompiledSuccessfully = false;
  
			// Note: using Material->IsPersistent() to detect whether this is a preview material which should have higher priority over background compiling
			GShaderCompilingManager->AddJobs(NewJobs, bApplyCompletedShaderMapForRendering && !bSynchronousCompile, bSynchronousCompile || !Material->IsPersistent());
  
			// Compile the shaders for this shader map now if the material is not deferring and deferred compiles are not enabled globally
			if (bSynchronousCompile)
			{
				TArray<int32> CurrentShaderMapId;
				CurrentShaderMapId.Add(CompilingId);
				GShaderCompilingManager->FinishCompilation(*FriendlyName, CurrentShaderMapId);
			}
		}
	}
}

bool FMaterialShaderMap::ProcessCompilationResults(const TArray<FShaderCompileJob*>& InCompilationResults, int32& InOutJobIndex, float& TimeBudget)
{
	check(InOutJobIndex < InCompilationResults.Num());

	const double StartTime = FPlatformTime::Seconds();

	FSHAHash MaterialShaderMapHash;
	ShaderMapId.GetMaterialHash(MaterialShaderMapHash);

	do 
	{
		const FShaderCompileJob& CurrentJob = *InCompilationResults[InOutJobIndex];
		check(CurrentJob.Id == CompilingId);

		if (CurrentJob.VFType)
		{
			FVertexFactoryType* VertexFactoryType = CurrentJob.VFType;
			check(VertexFactoryType->IsUsedWithMaterials());
			FMeshMaterialShaderMap* MeshShaderMap = NULL;
			int32 MeshShaderMapIndex = INDEX_NONE;

			// look for existing map for this vertex factory type
			for (int32 ShaderMapIndex = 0; ShaderMapIndex < MeshShaderMaps.Num(); ShaderMapIndex++)
			{
				if (MeshShaderMaps[ShaderMapIndex].GetVertexFactoryType() == VertexFactoryType)
				{
					MeshShaderMap = &MeshShaderMaps[ShaderMapIndex];
					MeshShaderMapIndex = ShaderMapIndex;
					break;
				}
			}

			check(MeshShaderMap);
			FMeshMaterialShaderType* MeshMaterialShaderType = CurrentJob.ShaderType->GetMeshMaterialShaderType();
			check(MeshMaterialShaderType);
			FShader* Shader = MeshMaterialShaderType->FinishCompileShader(MaterialCompilationOutput.UniformExpressionSet, MaterialShaderMapHash, CurrentJob, FriendlyName);
			check(Shader && !MeshShaderMap->HasShader(MeshMaterialShaderType));
			MeshShaderMap->AddShader(MeshMaterialShaderType, Shader);
		}
		else
		{
			FMaterialShaderType* MaterialShaderType = CurrentJob.ShaderType->GetMaterialShaderType();
			check(MaterialShaderType);
			FShader* Shader = MaterialShaderType->FinishCompileShader(MaterialCompilationOutput.UniformExpressionSet, MaterialShaderMapHash, CurrentJob, FriendlyName);
			check(Shader && !HasShader(MaterialShaderType));
			AddShader(MaterialShaderType, Shader);
		}

		InOutJobIndex++;

		TimeBudget -= FPlatformTime::Seconds() - StartTime;
	}
	while (TimeBudget > 0 && InOutJobIndex < InCompilationResults.Num());

	if (InOutJobIndex == InCompilationResults.Num())
	{
		for (int32 ShaderMapIndex = MeshShaderMaps.Num() - 1; ShaderMapIndex >= 0; ShaderMapIndex--)
		{
			if (MeshShaderMaps[ShaderMapIndex].GetNumShaders() == 0)
			{
				// If the mesh material shader map is complete and empty, discard it.
				MeshShaderMaps.RemoveAt(ShaderMapIndex);
			}
		}

		// Reinitialize the ordered mesh shader maps
		InitOrderedMeshShaderMaps();

		// Add the persistent shaders to the local shader cache.
		if (bIsPersistent)
		{
			SaveToDerivedDataCache();
		}

		// The shader map can now be used on the rendering thread
		bCompilationFinalized = true;

		return true;
	}

	return false;
}

bool FMaterialShaderMap::IsComplete(const FMaterial* Material, bool bSilent)
{
	bool bIsComplete = true;

	// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
	// Since it creates a temporary ref counted pointer.
	check(NumRefs > 0);
	const TArray<FMaterial*>* CorrespondingMaterials = FMaterialShaderMap::ShaderMapsBeingCompiled.Find(this);

	if (CorrespondingMaterials)
	{
		check(!bCompilationFinalized);
		return false;
	}

	// Iterate over all vertex factory types.
	for(TLinkedList<FVertexFactoryType*>::TIterator VertexFactoryTypeIt(FVertexFactoryType::GetTypeList());VertexFactoryTypeIt;VertexFactoryTypeIt.Next())
	{
		FVertexFactoryType* VertexFactoryType = *VertexFactoryTypeIt;

		if(VertexFactoryType->IsUsedWithMaterials())
		{
			// Find the shaders for this vertex factory type.
			const FMeshMaterialShaderMap* MeshShaderMap = GetMeshShaderMap(VertexFactoryType);
			if(!FMeshMaterialShaderMap::IsComplete(MeshShaderMap,Platform,Material,VertexFactoryType,bSilent))
			{
				if (!MeshShaderMap && !bSilent)
				{
					UE_LOG(LogShaders, Warning, TEXT("Incomplete material %s, missing Vertex Factory %s."), *Material->GetFriendlyName(), VertexFactoryType->GetName());
				}
				bIsComplete = false;
				break;
			}
		}
	}

	// Iterate over all material shader types.
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		// Find this shader type in the material's shader map.
		FMaterialShaderType* ShaderType = ShaderTypeIt->GetMaterialShaderType();
		if (ShaderType && 
			!HasShader(ShaderType) &&
			ShaderType->ShouldCache(Platform,Material) && 
			Material->ShouldCache(Platform, ShaderType, NULL)
			)
		{
			if (!bSilent)
			{
				UE_LOG(LogShaders, Warning, TEXT("Incomplete material %s, missing FMaterialShader %s."), *Material->GetFriendlyName(), ShaderType->GetName());
			}
			bIsComplete = false;
			break;
		}
	}

	return bIsComplete;
}

void FMaterialShaderMap::LoadMissingShadersFromMemory(const FMaterial* Material)
{
	// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
	// Since it creates a temporary ref counted pointer.
	check(NumRefs > 0);

	const TArray<FMaterial*>* CorrespondingMaterials = FMaterialShaderMap::ShaderMapsBeingCompiled.Find(this);

	if (CorrespondingMaterials)
	{
		check(!bCompilationFinalized);
		return;
	}

	FSHAHash MaterialShaderMapHash;
	ShaderMapId.GetMaterialHash(MaterialShaderMapHash);

	// Try to find necessary FMaterialShaderType's in memory
	for(TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FMaterialShaderType* ShaderType = ShaderTypeIt->GetMaterialShaderType();

		const bool bShaderAlreadyExists = HasShader(ShaderType);

		if (ShaderType && 
			ShaderType->ShouldCache(Platform, Material) && 
			Material->ShouldCache(Platform, ShaderType, NULL) &&
			!bShaderAlreadyExists)
		{
			FShaderId ShaderId(MaterialShaderMapHash, NULL, ShaderType, FShaderTarget(ShaderType->GetFrequency(), Platform));
			FShader* FoundShader = ShaderType->FindShaderById(ShaderId);	

			if (FoundShader)
			{
				AddShader(ShaderType, FoundShader);
			}
		}
	}

	// Try to find necessary FMeshMaterialShaderMap's in memory
	for(TLinkedList<FVertexFactoryType*>::TIterator VertexFactoryTypeIt(FVertexFactoryType::GetTypeList());VertexFactoryTypeIt;VertexFactoryTypeIt.Next())
	{
		FVertexFactoryType* VertexFactoryType = *VertexFactoryTypeIt;
		check(VertexFactoryType);

		if (VertexFactoryType->IsUsedWithMaterials())
		{
			FMeshMaterialShaderMap* MeshShaderMap = OrderedMeshShaderMaps[VertexFactoryType->GetId()];

			if (MeshShaderMap)
			{
				MeshShaderMap->LoadMissingShadersFromMemory(MaterialShaderMapHash, Material, Platform);
			}
		}
	}
}

void FMaterialShaderMap::GetShaderList(TMap<FShaderId,FShader*>& OutShaders) const
{
	TShaderMap<FMaterialShaderType>::GetShaderList(OutShaders);
	for(int32 Index = 0;Index < MeshShaderMaps.Num();Index++)
	{
		MeshShaderMaps[Index].GetShaderList(OutShaders);
	}
}

/**
 * Registers a material shader map in the global map so it can be used by materials.
 */
void FMaterialShaderMap::Register() 
{
	if (GCreateShadersOnLoad && Platform == GRHIShaderPlatform)
	{
		for (TMap<FShaderType*, TRefCountPtr<FShader> >::TConstIterator ShaderIt(GetShaders()); ShaderIt; ++ShaderIt)
		{
			FShader* Shader = ShaderIt.Value();

			if (Shader)
			{
				Shader->InitializeResource();
			}
		}

		for (int32 Index = 0; Index < MeshShaderMaps.Num(); Index++)
		{
			for (TMap<FShaderType*, TRefCountPtr<FShader> >::TConstIterator ShaderIt(MeshShaderMaps[Index].GetShaders()); ShaderIt; ++ShaderIt)
			{
				FShader* Shader = ShaderIt.Value();

				if (Shader)
				{
					Shader->InitializeResource();
				}
			}
		}
	}

	if (!bRegistered)
	{
		INC_DWORD_STAT(STAT_Shaders_NumShaderMaps);
		INC_DWORD_STAT_BY(STAT_Shaders_ShaderMapMemory, GetSizeBytes());
	}

	GIdToMaterialShaderMap[Platform].Add(ShaderMapId,this); 
	bRegistered = true;
}

void FMaterialShaderMap::AddRef()
{
	check(!bDeletedThroughDeferredCleanup);
	++NumRefs;
}

void FMaterialShaderMap::Release()
{
	check(NumRefs != 0);
	if(--NumRefs == 0)
	{
		if (bRegistered)
		{
			DEC_DWORD_STAT(STAT_Shaders_NumShaderMaps);
			DEC_DWORD_STAT_BY(STAT_Shaders_ShaderMapMemory, GetSizeBytes());

			GIdToMaterialShaderMap[Platform].Remove(ShaderMapId);
			bRegistered = false;
		}

		BeginCleanup(this);
	}
}

FMaterialShaderMap::FMaterialShaderMap() :
	Platform(SP_NumPlatforms),
	CompilingId(1),
	NumRefs(0),
	bDeletedThroughDeferredCleanup(false),
	bRegistered(false),
	bCompilationFinalized(true),
	bCompiledSuccessfully(true),
	bIsPersistent(true)
{
	checkSlow(IsInGameThread());
	AllMaterialShaderMaps.Add(this);
}

FMaterialShaderMap::~FMaterialShaderMap()
{ 
	checkSlow(IsInGameThread());
	check(bDeletedThroughDeferredCleanup);
	check(!bRegistered);
	AllMaterialShaderMaps.RemoveSwap(this);
}

/**
 * Removes all entries in the cache with exceptions based on a shader type
 * @param ShaderType - The shader type to flush
 */
void FMaterialShaderMap::FlushShadersByShaderType(FShaderType* ShaderType)
{
	// flush from all the vertex factory shader maps
	for(int32 Index = 0;Index < MeshShaderMaps.Num();Index++)
	{
		MeshShaderMaps[Index].FlushShadersByShaderType(ShaderType);
	}

	if (ShaderType->GetMaterialShaderType())
	{
		RemoveShaderType(ShaderType->GetMaterialShaderType());	
	}
}

/**
 * Removes all entries in the cache with exceptions based on a vertex factory type
 * @param ShaderType - The shader type to flush
 */
void FMaterialShaderMap::FlushShadersByVertexFactoryType(const FVertexFactoryType* VertexFactoryType)
{
	for (int32 Index = 0; Index < MeshShaderMaps.Num(); Index++)
	{
		FVertexFactoryType* VFType = MeshShaderMaps[Index].GetVertexFactoryType();
		// determine if this shaders vertex factory type should be flushed
		if (VFType == VertexFactoryType)
		{
			// remove the shader map
			MeshShaderMaps.RemoveAt(Index);
			// fix up the counter
			Index--;
		}
	}

	// reset the OrderedMeshShaderMap to remove references to the removed maps
	InitOrderedMeshShaderMaps();
}

struct FCompareMeshShaderMaps
{
	FORCEINLINE bool operator()(const FMeshMaterialShaderMap& A, const FMeshMaterialShaderMap& B ) const
	{
		return FCString::Strncmp(
			A.GetVertexFactoryType()->GetName(), 
			B.GetVertexFactoryType()->GetName(), 
			FMath::Min(FCString::Strlen(A.GetVertexFactoryType()->GetName()), FCString::Strlen(B.GetVertexFactoryType()->GetName()))) > 0;
	}
};

void FMaterialShaderMap::Serialize(FArchive& Ar, bool bInlineShaderResources)
{
	// Note: This is saved to the DDC, not into packages (except when cooked)
	// Backwards compatibility therefore will not work based on the version of Ar
	// Instead, just bump MATERIALSHADERMAP_DERIVEDDATA_VER

	ShaderMapId.Serialize(Ar);

	// serialize the platform enum as a uint8
	int32 TempPlatform = (int32)Platform;
	Ar << TempPlatform;
	Platform = (EShaderPlatform)TempPlatform;

	Ar << FriendlyName;

	MaterialCompilationOutput.Serialize(Ar);

	Ar << DebugDescription;

	if (Ar.IsSaving())
	{
		// Material shaders
		TShaderMap<FMaterialShaderType>::SerializeInline(Ar, bInlineShaderResources, false);

		// Mesh material shaders
		int32 NumMeshShaderMaps = 0;

		for (int32 VFIndex = 0; VFIndex < OrderedMeshShaderMaps.Num(); VFIndex++)
		{
			FMeshMaterialShaderMap* MeshShaderMap = OrderedMeshShaderMaps[VFIndex];

			if (MeshShaderMap)
			{
				// Count the number of non-empty mesh shader maps
				NumMeshShaderMaps++;
			}
		}

		Ar << NumMeshShaderMaps;

		TArray<FMeshMaterialShaderMap*> SortedMeshShaderMaps;
		SortedMeshShaderMaps.Empty(MeshShaderMaps.Num());

		for (int32 MapIndex = 0; MapIndex < MeshShaderMaps.Num(); MapIndex++)
		{
			SortedMeshShaderMaps.Add(&MeshShaderMaps[MapIndex]);
		}

		// Sort mesh shader maps by VF name so that the DDC entry always has the same binary result for a given key
		SortedMeshShaderMaps.Sort(FCompareMeshShaderMaps());

		for (int32 MapIndex = 0; MapIndex < SortedMeshShaderMaps.Num(); MapIndex++)
		{
			FMeshMaterialShaderMap* MeshShaderMap = SortedMeshShaderMaps[MapIndex];

			if (MeshShaderMap)
			{
				FVertexFactoryType* VFType = MeshShaderMap->GetVertexFactoryType();
				check(VFType);

				Ar << VFType;

				MeshShaderMap->SerializeInline(Ar, bInlineShaderResources, false);
			}
		}
	}

	if (Ar.IsLoading())
	{
		MeshShaderMaps.Empty();

		for(TLinkedList<FVertexFactoryType*>::TIterator VertexFactoryTypeIt(FVertexFactoryType::GetTypeList());VertexFactoryTypeIt;VertexFactoryTypeIt.Next())
		{
			FVertexFactoryType* VertexFactoryType = *VertexFactoryTypeIt;
			check(VertexFactoryType);

			if (VertexFactoryType->IsUsedWithMaterials())
			{
				new(MeshShaderMaps) FMeshMaterialShaderMap(VertexFactoryType);
			}
		}

		// Initialize OrderedMeshShaderMaps from the new contents of MeshShaderMaps.
		InitOrderedMeshShaderMaps();

		// Material shaders
		TShaderMap<FMaterialShaderType>::SerializeInline(Ar, bInlineShaderResources, false);

		// Mesh material shaders
		int32 NumMeshShaderMaps = 0;
		Ar << NumMeshShaderMaps;

		for (int32 VFIndex = 0; VFIndex < NumMeshShaderMaps; VFIndex++)
		{
			FVertexFactoryType* VFType = NULL;

			Ar << VFType;

			// Not handling missing vertex factory types on cooked data
			// The cooker binary and running binary are assumed to be on the same code version
			check(VFType);
			FMeshMaterialShaderMap* MeshShaderMap = OrderedMeshShaderMaps[VFType->GetId()];
			check(MeshShaderMap);
			MeshShaderMap->SerializeInline(Ar, bInlineShaderResources, false);
		}

		// Trim the mesh shader maps by removing empty entries
		for (int32 VFIndex = 0; VFIndex < OrderedMeshShaderMaps.Num(); VFIndex++)
		{
			if (OrderedMeshShaderMaps[VFIndex]->IsEmpty())
			{
				OrderedMeshShaderMaps[VFIndex] = NULL;
			}
		}

		for (int32 Index = MeshShaderMaps.Num() - 1; Index >= 0; Index--)
		{
			if (MeshShaderMaps[Index].IsEmpty())
			{
				MeshShaderMaps.RemoveAt(Index);
			}
		}
	}
}

void FMaterialShaderMap::RemovePendingMaterial(FMaterial* Material)
{
	for (TMap<TRefCountPtr<FMaterialShaderMap>, TArray<FMaterial*> >::TIterator It(ShaderMapsBeingCompiled); It; ++It)
	{
		TArray<FMaterial*>& Materials = It.Value();
		Materials.Remove(Material);
	}
}

const FMaterialShaderMap* FMaterialShaderMap::GetShaderMapBeingCompiled(const FMaterial* Material)
{
	// Inefficient search, but only when compiling a lot of shaders
	for (TMap<TRefCountPtr<FMaterialShaderMap>, TArray<FMaterial*> >::TIterator It(ShaderMapsBeingCompiled); It; ++It)
	{
		TArray<FMaterial*>& Materials = It.Value();
		
		for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); MaterialIndex++)
		{
			if (Materials[MaterialIndex] == Material)
			{
				return It.Key();
			}
		}
	}

	return NULL;
}

uint32 FMaterialShaderMap::GetMaxTextureSamplers() const
{
	uint32 MaxTextureSamplers = GetMaxTextureSamplersShaderMap();

	for (int32 Index = 0;Index < MeshShaderMaps.Num();Index++)
	{
		MaxTextureSamplers = FMath::Max(MaxTextureSamplers, MeshShaderMaps[Index].GetMaxTextureSamplersShaderMap());
	}

	return MaxTextureSamplers;
}

const FMeshMaterialShaderMap* FMaterialShaderMap::GetMeshShaderMap(FVertexFactoryType* VertexFactoryType) const
{
	checkSlow(bCompilationFinalized);
	const FMeshMaterialShaderMap* MeshShaderMap = OrderedMeshShaderMaps[VertexFactoryType->GetId()];
	checkSlow(!MeshShaderMap || MeshShaderMap->GetVertexFactoryType() == VertexFactoryType);
	return MeshShaderMap;
}

void FMaterialShaderMap::InitOrderedMeshShaderMaps()
{
	OrderedMeshShaderMaps.Empty(FVertexFactoryType::GetNumVertexFactoryTypes());
	OrderedMeshShaderMaps.AddZeroed(FVertexFactoryType::GetNumVertexFactoryTypes());

	for (int32 Index = 0;Index < MeshShaderMaps.Num();Index++)
	{
		check(MeshShaderMaps[Index].GetVertexFactoryType());
		const int32 VFIndex = MeshShaderMaps[Index].GetVertexFactoryType()->GetId();
		OrderedMeshShaderMaps[VFIndex] = &MeshShaderMaps[Index];
	}
}

/**
 * Dump material stats for a given platform.
 * 
 * @param	Platform	Platform to dump stats for.
 */
void DumpMaterialStats( EShaderPlatform Platform )
{
#if ALLOW_DEBUG_FILES
	FDiagnosticTableViewer MaterialViewer(*FDiagnosticTableViewer::GetUniqueTemporaryFilePath(TEXT("MaterialStats")));

	// Mapping from friendly material name to shaders associated with it.
	TMultiMap<FString,FShader*> MaterialToShaderMap;
	// Set of material names.
	TSet<FString> MaterialNames;

	// Look at in-memory shader use.
	for (int32 ShaderMapIndex = 0; ShaderMapIndex < FMaterialShaderMap::AllMaterialShaderMaps.Num(); ShaderMapIndex++)
	{
		FMaterialShaderMap* MaterialShaderMap = FMaterialShaderMap::AllMaterialShaderMaps[ShaderMapIndex];
		TMap<FShaderId,FShader*> Shaders;
		MaterialShaderMap->GetShaderList( Shaders );

		// Add friendly name to list of materials.
		FString FriendlyName = MaterialShaderMap->GetFriendlyName();
		MaterialNames.Add( FriendlyName );

		// Add shaders to mapping per friendly name as there might be multiple
		for( TMap<FShaderId,FShader*>::TConstIterator ShaderMapIt(Shaders); ShaderMapIt; ++ShaderMapIt )
		{
			FShader* Shader = ShaderMapIt.Value();
			MaterialToShaderMap.AddUnique(FriendlyName,Shader);
		}
	}

	// Write a row of headings for the table's columns.
	MaterialViewer.AddColumn(TEXT("Name"));
	MaterialViewer.AddColumn(TEXT("Shaders"));
	MaterialViewer.AddColumn(TEXT("Code Size"));
	MaterialViewer.CycleRow();

	// Iterate over all materials, gathering shader stats.
	int32 TotalCodeSize		= 0;
	int32 TotalShaderCount	= 0;
	for( TSet<FString>::TConstIterator It(MaterialNames); It; ++It )
	{
		// Retrieve list of shaders in map.
		TArray<FShader*> Shaders;
		MaterialToShaderMap.MultiFind( *It, Shaders );
		
		// Iterate over shaders and gather stats.
		int32 CodeSize = 0;
		for( int32 ShaderIndex=0; ShaderIndex<Shaders.Num(); ShaderIndex++ )
		{
			FShader* Shader = Shaders[ShaderIndex];
			CodeSize += Shader->GetCode().Num();
		}

		TotalCodeSize += CodeSize;
		TotalShaderCount += Shaders.Num();

		// Dump stats
		MaterialViewer.AddColumn(**It);
		MaterialViewer.AddColumn(TEXT("%u"),Shaders.Num());
		MaterialViewer.AddColumn(TEXT("%u"),CodeSize);
		MaterialViewer.CycleRow();
	}

	// Add a total row.
	MaterialViewer.AddColumn(TEXT("Total"));
	MaterialViewer.AddColumn(TEXT("%u"),TotalShaderCount);
	MaterialViewer.AddColumn(TEXT("%u"),TotalCodeSize);
	MaterialViewer.CycleRow();
#endif
}

