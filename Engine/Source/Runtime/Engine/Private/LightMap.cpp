// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightMap.cpp: Light-map implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "TargetPlatform.h"

DEFINE_LOG_CATEGORY_STATIC(LogLightMap, Log, All);

FLightmassDebugOptions GLightmassDebugOptions;

/** Whether to use bilinear filtering on lightmaps */
bool GUseBilinearLightmaps = true;

/** Whether to allow padding around mappings. */
bool GAllowLightmapPadding = true;

/** Counts the number of lightmap textures generated each lighting build. */
ENGINE_API int32 GLightmapCounter = 0;
/** Whether to compress lightmaps. Reloaded from ini each lighting build. */
ENGINE_API bool GCompressLightmaps = true;

/** Whether to allow lighting builds to generate streaming lightmaps. */
ENGINE_API bool GAllowStreamingLightmaps = false;

/** Largest boundingsphere radius to use when packing lightmaps into a texture atlas. */
ENGINE_API float GMaxLightmapRadius = 5000.0f;	//10000.0;	//2000.0f;

/** The quality level of DXT encoding for lightmaps (values come from nvtt::Quality enum) */
int32 GLightmapEncodeQualityLevel = 2; // nvtt::Quality_Production

/** The quality level of the current lighting build */
ELightingBuildQuality GLightingBuildQuality = Quality_Preview;

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING && WITH_EDITOR
	/** Information about the lightmap sample that is selected */
	UNREALED_API extern FSelectedLightmapSample GCurrentSelectedLightmapSample;
#endif

/** The color to set selected texels to */
ENGINE_API FColor GTexelSelectionColor(255, 50, 0);

#if WITH_EDITOR
	// NOTE: We're only counting the top-level mip-map for the following variables.
	/** Total number of texels allocated for all lightmap textures. */
	ENGINE_API uint64 GNumLightmapTotalTexels = 0;
	/** Total number of texels used if the texture was non-power-of-two. */
	ENGINE_API uint64 GNumLightmapTotalTexelsNonPow2 = 0;
	/** Number of lightmap textures generated. */
	ENGINE_API int32 GNumLightmapTextures = 0;
	/** Total number of mapped texels. */
	ENGINE_API uint64 GNumLightmapMappedTexels = 0;
	/** Total number of unmapped texels. */
	ENGINE_API uint64 GNumLightmapUnmappedTexels = 0;
	/** Whether to allow cropping of unmapped borders in lightmaps and shadowmaps. Controlled by BaseEngine.ini setting. */
	ENGINE_API bool GAllowLightmapCropping = false;
	/** Total lightmap texture memory size (in bytes), including GLightmapTotalStreamingSize. */
	ENGINE_API uint64 GLightmapTotalSize = 0;
	/** Total memory size for streaming lightmaps (in bytes). */
	ENGINE_API uint64 GLightmapTotalStreamingSize = 0;
#endif

#include "TextureLayout.h"

FLightMap::FLightMap()
	: bAllowHighQualityLightMaps(true)
	, NumRefs(0) 
{
	bAllowHighQualityLightMaps = !IsES2Platform(GRHIShaderPlatform) && AllowHighQualityLightmaps();
#if !PLATFORM_DESKTOP 
	checkf(bAllowHighQualityLightMaps || IsES2Platform(GRHIShaderPlatform), TEXT("Low quality lightmaps are not currently supported on consoles. Make sure console variable r.HighQualityLightMaps is true for this platform"));
#endif
}

void FLightMap::Serialize(FArchive& Ar)
{
	Ar << LightGuids;
}

void FLightMap::Cleanup()
{
	BeginCleanup(this);
}

void FLightMap::FinishCleanup()
{
	delete this;
}

ULightMapTexture2D::ULightMapTexture2D(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void ULightMapTexture2D::Serialize(FArchive& Ar)
{
	LODGroup = TEXTUREGROUP_Lightmap;

	Super::Serialize(Ar);

	uint32 Flags = LightmapFlags;
	Ar << Flags;
	LightmapFlags = ELightMapFlags( Flags );
}

/** 
 * Returns a one line description of an object for viewing in the generic browser
 */
FString ULightMapTexture2D::GetDesc()
{
	return FString::Printf( TEXT("Lightmap: %dx%d [%s]"), GetSizeX(), GetSizeY(), GPixelFormats[GetPixelFormat()].Name );
}

#if WITH_EDITORONLY_DATA
static void DumpLightmapSizeOnDisk()
{
	UE_LOG(LogLightMap,Log,TEXT("Lightmap size on disk"));
	UE_LOG(LogLightMap,Log,TEXT("Source (KB),Source is PNG,Platform Data (KB),Lightmap"));
	for (TObjectIterator<ULightMapTexture2D> It; It; ++It)
	{
		ULightMapTexture2D* Lightmap = *It;
		UE_LOG(LogLightMap,Log,TEXT("%f,%d,%d,%f,%s"),
			Lightmap->Source.GetSizeOnDisk() / 1024.0f,
			Lightmap->Source.IsPNGCompressed(),
			Lightmap->CalcTextureMemorySizeEnum(TMC_AllMips) / 1024.0f,
			*Lightmap->GetPathName()
			);
	}
}

static FAutoConsoleCommand CmdDumpLightmapSizeOnDisk(
	TEXT("DumpLightmapSizeOnDisk"),
	TEXT("Dumps the size of all loaded lightmaps on disk (source and platform data)"),
	FConsoleCommandDelegate::CreateStatic(DumpLightmapSizeOnDisk)
	);
#endif // #if WITH_EDITORONLY_DATA

/** Lightmap resolution scaling factors for debugging.  The defaults are to use the original resolution unchanged. */
float TextureMappingDownsampleFactor0 = 1.0f;
int32 TextureMappingMinDownsampleSize0 = 16;
float TextureMappingDownsampleFactor1 = 1.0f;
int32 TextureMappingMinDownsampleSize1 = 128;
float TextureMappingDownsampleFactor2 = 1.0f;
int32 TextureMappingMinDownsampleSize2 = 256;

static int32 AdjustTextureMappingSize(int32 InSize)
{
	int32 NewSize = InSize;
	if (InSize > TextureMappingMinDownsampleSize0 && InSize <= TextureMappingMinDownsampleSize1)
	{
		NewSize = FMath::TruncToInt(InSize * TextureMappingDownsampleFactor0);
	}
	else if (InSize > TextureMappingMinDownsampleSize1 && InSize <= TextureMappingMinDownsampleSize2)
	{
		NewSize = FMath::TruncToInt(InSize * TextureMappingDownsampleFactor1);
	}
	else if (InSize > TextureMappingMinDownsampleSize2)
	{
		NewSize = FMath::TruncToInt(InSize * TextureMappingDownsampleFactor2);
	}
	return NewSize;
}

FStaticLightingMesh::FStaticLightingMesh(
	int32 InNumTriangles,
	int32 InNumShadingTriangles,
	int32 InNumVertices,
	int32 InNumShadingVertices,
	int32 InTextureCoordinateIndex,
	bool bInCastShadow,
	bool bInTwoSidedMaterial,
	const TArray<ULightComponent*>& InRelevantLights,
	const UPrimitiveComponent* const InComponent,
	const FBox& InBoundingBox,
	const FGuid& InGuid
	):
	NumTriangles(InNumTriangles),
	NumShadingTriangles(InNumShadingTriangles),
	NumVertices(InNumVertices),
	NumShadingVertices(InNumShadingVertices),
	TextureCoordinateIndex(InTextureCoordinateIndex),
	bCastShadow(bInCastShadow && InComponent->bCastStaticShadow),
	bTwoSidedMaterial(bInTwoSidedMaterial),
	RelevantLights(InRelevantLights),
	Component(InComponent),
	BoundingBox(InBoundingBox),
	Guid(FGuid::NewGuid()),
	SourceMeshGuid(InGuid)
{}

FStaticLightingTextureMapping::FStaticLightingTextureMapping(FStaticLightingMesh* InMesh,UObject* InOwner,int32 InSizeX,int32 InSizeY,int32 InLightmapTextureCoordinateIndex,bool bInBilinearFilter):
	FStaticLightingMapping(InMesh,InOwner),
	SizeX(AdjustTextureMappingSize(InSizeX)),
	SizeY(AdjustTextureMappingSize(InSizeY)),
	LightmapTextureCoordinateIndex(InLightmapTextureCoordinateIndex),
	bBilinearFilter(bInBilinearFilter)
{}

#if WITH_EDITOR

/**
 * An allocation of a region of light-map texture to a specific light-map.
 */
struct FLightMapAllocation
{
	/** 
	 * Basic constructor
	 */
	FLightMapAllocation()
	{
		MappedRect.Min.X = 0;
		MappedRect.Min.Y = 0;
		MappedRect.Max.X = 0;
		MappedRect.Max.Y = 0;
		Primitive = NULL;
		LightmapFlags = LMF_None;
		bSkipEncoding = false;
	}

	/**
	 * Copy construct from FQuantizedLightmapData
	 */
	FLightMapAllocation(const FQuantizedLightmapData* QuantizedData)
		: TotalSizeX(QuantizedData->SizeX)
		, TotalSizeY(QuantizedData->SizeY)
		, bHasSkyShadowing(QuantizedData->bHasSkyShadowing)
		, RawData(QuantizedData->Data)
	{
		FMemory::Memcpy(Scale, QuantizedData->Scale, sizeof(Scale));
		FMemory::Memcpy(Add, QuantizedData->Add, sizeof(Add));
		PaddingType = GAllowLightmapPadding ? LMPT_NormalPadding : LMPT_NoPadding;
		MappedRect.Min.X = 0;
		MappedRect.Min.Y = 0;
		MappedRect.Max.X = TotalSizeX;
		MappedRect.Max.Y = TotalSizeY;
		Primitive = NULL;
		LightmapFlags = LMF_None;
		bSkipEncoding = false;
	}

	FLightMap2D*	LightMap;
	UObject*		Outer;

	UObject*		Primitive;

	/** Upper-left X-coordinate in the texture atlas. */
	int32				OffsetX;
	/** Upper-left Y-coordinate in the texture atlas. */
	int32				OffsetY;
	/** Total number of texels along the X-axis. */
	int32				TotalSizeX;
	/** Total number of texels along the Y-axis. */
	int32				TotalSizeY;
	/** The rectangle of mapped texels within this mapping that is placed in the texture atlas. */
	FIntRect		MappedRect;
	bool			bDebug;
	bool			bHasSkyShadowing;
	ELightMapPaddingType			PaddingType;
	ELightMapFlags					LightmapFlags;
	TArray<FLightMapCoefficients>	RawData;
	float							Scale[NUM_STORED_LIGHTMAP_COEF][4];
	float							Add[NUM_STORED_LIGHTMAP_COEF][4];
	/** Bounds of the primitive that the mapping is applied to. */
	FBoxSphereBounds				Bounds;
	/** True if we can skip encoding this allocation because it's similar enough to an existing
	    allocation at the same offset */
	bool bSkipEncoding;
};

enum ELightmapTextureType
{
	LTT_Coefficients = NUM_STORED_LIGHTMAP_COEF,
	LTT_SkyOcclusion,
	LTT_Num
};

/**
 * A light-map texture which has been partially allocated, but not yet encoded.
 */
struct FLightMapPendingTexture : public FTextureLayout
{
	/**
	 * Information on light map textures being cached asynchronously.
	 */
	struct FAsyncLightMapCacheTask
	{
		/** The lightmap texture. */
		FLightMapPendingTexture* Texture;
		/** The coefficient index. */
		ELightmapTextureType TextureType;

		/** Initialization constructor. */
		FAsyncLightMapCacheTask(FLightMapPendingTexture* InTexture, ELightmapTextureType InTextureType)
			: Texture(InTexture)
			, TextureType(InTextureType)
		{
		}
	};

	/** List of async light map cache tasks. */
	static TArray<FAsyncLightMapCacheTask> TotalAsyncTasks;

	/**
	 * Checks for any completed asynchronous DXT compression tasks and finishes the texture creation.
	 * It will block until there are no more than 'NumUnfinishedTasksAllowed' tasks left unfinished.
	 *
	 * @param NumUnfinishedTasksAllowed		Maximum number of unfinished tasks to allow, before returning
	 */
	static void						FinishCompletedTasks( int32 NumUnfinishedTasksAllowed );

	/** Helper data to keep track of the asynchronous tasks for the 4 lightmap textures. */
	ULightMapTexture2D*				Textures[NUM_STORED_LIGHTMAP_COEF];
	ULightMapTexture2D*				SkyOcclusionTexture;

	TArray<FLightMapAllocation*>	Allocations;
	UObject*						Outer;
	TWeakObjectPtr<UWorld>			OwningWorld;
	/** Bounding volume for all mappings within this texture.							*/
	FBoxSphereBounds				Bounds;

	/** Lightmap streaming flags that must match in order to be stored in this texture.	*/
	ELightMapFlags					LightmapFlags;

	int32							NumOutstandingAsyncTasks;

	FLightMapPendingTexture(UWorld* InWorld, uint32 InSizeX,uint32 InSizeY)
		:	FTextureLayout(4,4,InSizeX,InSizeY,true) // Min size is 4x4 in case of block compression.
		,	SkyOcclusionTexture(NULL)
		,	OwningWorld(InWorld)
		,	Bounds(FBox(0))
		,	LightmapFlags( LMF_None )
		,	NumOutstandingAsyncTasks(0)
	{}

	~FLightMapPendingTexture()
	{
	}

	/**
	 * Processes the textures and starts asynchronous compression tasks for all mip-levels.
	 */
	void StartEncoding();

	/**
	 * Called once the compression tasks for all mip-levels of a texture has finished.
	 * Copies the compressed data into each of the mip-levels of the texture and deletes the tasks.
	 *
	 * @param CoefficientIndex	Texture coefficient index, identifying the specific texture with this FLightMapPendingTexture.
	 */
	void FinishEncoding( ELightmapTextureType TextureType );

	/**
	 * Finds a free area in the texture large enough to contain a surface with the given size.
	 * If a large enough area is found, it is marked as in use, the output parameters OutBaseX and OutBaseY are
	 * set to the coordinates of the upper left corner of the free area and the function return true.
	 * Otherwise, the function returns false and OutBaseX and OutBaseY remain uninitialized.
	 *
	 * If the allocation succeeded, Allocation.OffsetX and Allocation.OffsetY will be set to the upper-left corner
	 * of the area allocated.
	 *
	 * @param Allocation	Lightmap allocation to try to fit
	 * @param bForceIntoThisTexture	True if we should ignore distance and other factors when considering whether the mapping should be packed into this texture
	 *
	 * @return	True if succeeded, false otherwise.
	 */
	bool AddElement(FLightMapAllocation& Allocation, const bool bForceIntoThisTexture = false );

private:
	FName GetLightmapName(int32 TextureIndex, int32 CoefficientIndex);
	FName GetSkyOcclusionTextureName(int32 TextureIndex);
};

TArray<FLightMapPendingTexture::FAsyncLightMapCacheTask> FLightMapPendingTexture::TotalAsyncTasks;

/**
 * Checks for any completed asynchronous DXT compression tasks and finishes the texture creation.
 * It will block until there are no more than 'NumUnfinishedTasksAllowed' tasks left unfinished.
 *
 * @param NumUnfinishedTasksAllowed		Maximum number of unfinished tasks to allow, before returning
 */
void FLightMapPendingTexture::FinishCompletedTasks( int32 NumUnfinishedTasksAllowed )
{
	do
	{
		// Check for completed async compression tasks.
		for ( int32 TaskIndex=0; TaskIndex < TotalAsyncTasks.Num(); )
		{
			FAsyncLightMapCacheTask Task = TotalAsyncTasks[TaskIndex];
			FLightMapPendingTexture* PendingTexture = Task.Texture;
			ULightMapTexture2D* LightMapTexture = Task.TextureType == LTT_SkyOcclusion ? PendingTexture->SkyOcclusionTexture : PendingTexture->Textures[(int32)Task.TextureType];

			if (LightMapTexture->IsAsyncCacheComplete())
			{
				TotalAsyncTasks.RemoveAtSwap(TaskIndex);
				PendingTexture->FinishEncoding(Task.TextureType);
			}
			else
			{
				++TaskIndex;
			}
		}

		// If we still have too many unfinished tasks, wait for someone to finish.
		if ( TotalAsyncTasks.Num() > NumUnfinishedTasksAllowed )
		{
			FPlatformProcess::Sleep(0.1f);
		}

	} while ( TotalAsyncTasks.Num() > NumUnfinishedTasksAllowed );
}

/**
 * Called once the compression tasks for all mip-levels of a texture has finished.
 * Copies the compressed data into each of the mip-levels of the texture and deletes the tasks.
 *
 * @param CoefficientIndex	Texture coefficient index, identifying the specific texture with this FLightMapPendingTexture.
 */
void FLightMapPendingTexture::FinishEncoding( ELightmapTextureType TextureType )
{
	UTexture2D* Texture2D = TextureType == LTT_SkyOcclusion ? SkyOcclusionTexture : Textures[(int32)TextureType];

	Texture2D->FinishCachePlatformData();
	Texture2D->UpdateResource();

	if ( (int32)TextureType < NUM_HQ_LIGHTMAP_COEF )
	{
		int32 TextureSize = Texture2D->CalcTextureMemorySizeEnum( TMC_AllMips );
		GLightmapTotalSize += TextureSize;
		GLightmapTotalStreamingSize += (LightmapFlags & LMF_Streamed) ? TextureSize : 0;

		UPackage* TexturePackage = Texture2D->GetOutermost();
		if( OwningWorld.IsValid() )
		{
			for ( int32 LevelIndex=0; TexturePackage && LevelIndex < OwningWorld->GetNumLevels(); LevelIndex++ )
			{
				ULevel* Level = OwningWorld->GetLevel(LevelIndex);
				UPackage* LevelPackage = Level->GetOutermost();
				if ( TexturePackage == LevelPackage )
				{
					Level->LightmapTotalSize += float(Texture2D->CalcTextureMemorySizeEnum( TMC_AllMips )) / 1024.0f;
					break;
				}
			}
		}
	}

	// Delete the pending texture when all async tasks have completed.
	if (--NumOutstandingAsyncTasks == 0)
	{
		delete this;
	}
}

/** Whether to try to pack lightmaps/shadowmaps into the same texture. */
bool GGroupComponentLightmaps = true;

/**
 * Finds a free area in the texture large enough to contain a surface with the given size.
 * If a large enough area is found, it is marked as in use, the output parameters OutBaseX and OutBaseY are
 * set to the coordinates of the upper left corner of the free area and the function return true.
 * Otherwise, the function returns false and OutBaseX and OutBaseY remain uninitialized.
 *
 * If the allocation succeeded, Allocation.OffsetX and Allocation.OffsetY will be set to the upper-left corner
 * of the allocated area.
 *
 * @param Allocation	Lightmap allocation to try to fit
 * @param bForceIntoThisTexture	True if we should ignore distance and other factors when considering whether the mapping should be packed into this texture
 *
 * @return	True if succeeded, false otherwise.
 */
bool FLightMapPendingTexture::AddElement(FLightMapAllocation& Allocation, bool bForceIntoThisTexture )
{
	if( !bForceIntoThisTexture )
	{
		// Don't pack lightmaps from different packages into the same texture.
		if ( Outer != Allocation.Outer )
		{
			return false;
		}
	}

	const FBoxSphereBounds NewBounds = Bounds + Allocation.Bounds;
	const bool bEmptyTexture = Allocations.Num() == 0;

	if ( !bEmptyTexture && !bForceIntoThisTexture )
	{
		// Don't mix streaming lightmaps with non-streaming lightmaps.
		if ( (LightmapFlags & LMF_Streamed) != (Allocation.LightmapFlags & LMF_Streamed) )
		{
			return false;
		}

		// Is this a streaming lightmap?
		if ( LightmapFlags & LMF_Streamed )
		{
			bool bPerformDistanceCheck = true;

			// Don't pack together lightmaps that are too far apart
			if ( bPerformDistanceCheck && NewBounds.SphereRadius > GMaxLightmapRadius && NewBounds.SphereRadius > (Bounds.SphereRadius + SMALL_NUMBER) )
			{
				return false;
			}
		}
	}

	uint32 BaseX = 0;
	uint32 BaseY = 0;
	if( !FTextureLayout::AddElement( BaseX, BaseY, Allocation.MappedRect.Width(), Allocation.MappedRect.Height() ) )
	{
		return false;
	}

	// UE_LOG(LogLightMap, Warning, TEXT( "LightMapPacking: New Allocation (%i texels)" ), Allocation.MappedRect.Area() );

	// Save the position the light-maps (the Allocation.MappedRect portion) in the texture atlas.
	Allocation.OffsetX = BaseX;
	Allocation.OffsetY = BaseY;
	Bounds = bEmptyTexture ? Allocation.Bounds : NewBounds;

	return true;
}

/** Whether to color each lightmap texture with a different (random) color. */
bool GVisualizeLightmapTextures = false;

static void GenerateLightmapMipsAndDilate(int32 NumMips, int32 TextureSizeX, int32 TextureSizeY, FColor TextureColor, FColor** MipData, int8** MipCoverageData)
{
	for(int32 MipIndex = 1; MipIndex < NumMips; MipIndex++)
	{
		const int32 SourceMipSizeX = FMath::Max(1, TextureSizeX >> (MipIndex - 1));
		const int32 SourceMipSizeY = FMath::Max(1, TextureSizeY >> (MipIndex - 1));
		const int32 DestMipSizeX = FMath::Max(1, TextureSizeX >> MipIndex);
		const int32 DestMipSizeY = FMath::Max(1, TextureSizeY >> MipIndex);

		// Downsample the previous mip-level, taking into account which texels are mapped.
		FColor* NextMipData = MipData[MipIndex];
		FColor* LastMipData = MipData[MipIndex - 1];

		int8* NextMipCoverageData = MipCoverageData[ MipIndex ];
		int8* LastMipCoverageData = MipCoverageData[ MipIndex - 1 ];

		const int32 MipFactorX = SourceMipSizeX / DestMipSizeX;
		const int32 MipFactorY = SourceMipSizeY / DestMipSizeY;

		//@todo - generate mips before encoding lightmaps!  
		// Currently we are filtering in the encoded space, similar to generating mips of sRGB textures in sRGB space
		for(int32 Y = 0; Y < DestMipSizeY; Y++)
		{
			for(int32 X = 0; X < DestMipSizeX; X++)
			{
				FLinearColor AccumulatedColor = FLinearColor::Black;
				uint32 Coverage = 0;

				const uint32 MinSourceY = (Y + 0) * MipFactorY;
				const uint32 MaxSourceY = (Y + 1) * MipFactorY;
				for(uint32 SourceY = MinSourceY; SourceY < MaxSourceY; SourceY++)
				{
					const uint32 MinSourceX = (X + 0) * MipFactorX;
					const uint32 MaxSourceX = (X + 1) * MipFactorX;
					for(uint32 SourceX = MinSourceX; SourceX < MaxSourceX; SourceX++)
					{
						const FColor& SourceColor = LastMipData[SourceY * SourceMipSizeX + SourceX];
						int8 SourceCoverage = LastMipCoverageData[ SourceY * SourceMipSizeX + SourceX ];
						if( SourceCoverage )
						{
							AccumulatedColor += SourceColor.ReinterpretAsLinear() * SourceCoverage;
							Coverage += SourceCoverage;
						}
					}
				}
				FColor& DestColor = NextMipData[Y * DestMipSizeX + X];
				int8& DestCoverage = NextMipCoverageData[Y * DestMipSizeX + X];
				if ( GVisualizeLightmapTextures )
				{
					DestColor = TextureColor;
					DestCoverage = 127;
				}
				else if(Coverage)
				{
					DestColor = ( AccumulatedColor / Coverage ).Quantize();
					DestCoverage = Coverage / (MipFactorX * MipFactorY);
				}
				else
				{
					DestColor = FColor(0,0,0);
					DestCoverage = 0;
				}
			}
		}
	}

	// Expand texels which are mapped into adjacent texels which are not mapped to avoid artifacts when using texture filtering.
	for(int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		FColor* MipLevelData = MipData[MipIndex];
		int8* MipLevelCoverageData = MipCoverageData[MipIndex];

		uint32 MipSizeX = FMath::Max(1,TextureSizeX >> MipIndex);
		uint32 MipSizeY = FMath::Max(1,TextureSizeY >> MipIndex);
		for(uint32 DestY = 0;DestY < MipSizeY;DestY++)
		{
			for(uint32 DestX = 0; DestX < MipSizeX; DestX++)
			{
				FColor& DestColor = MipLevelData[DestY * MipSizeX + DestX];
				int8& DestCoverage = MipLevelCoverageData[DestY * MipSizeX + DestX];
				if(DestCoverage == 0)
				{
					FLinearColor AccumulatedColor = FLinearColor::Black;
					uint32 Coverage = 0;

					const int32 MinSourceY = FMath::Max((int32)DestY - 1, (int32)0);
					const int32 MaxSourceY = FMath::Min((int32)DestY + 1, (int32)MipSizeY - 1);
					for(int32 SourceY = MinSourceY; SourceY <= MaxSourceY; SourceY++)
					{
						const int32 MinSourceX = FMath::Max((int32)DestX - 1, (int32)0);
						const int32 MaxSourceX = FMath::Min((int32)DestX + 1, (int32)MipSizeX - 1);
						for(int32 SourceX = MinSourceX; SourceX <= MaxSourceX; SourceX++)
						{
							FColor& SourceColor = MipLevelData[SourceY * MipSizeX + SourceX];
							int8 SourceCoverage = MipLevelCoverageData[SourceY * MipSizeX + SourceX];
							if( SourceCoverage > 0 )
							{
								static const uint32 Weights[3][3] =
								{
									{ 1, 255, 1 },
									{ 255, 0, 255 },
									{ 1, 255, 1 },
								};
								AccumulatedColor += SourceColor.ReinterpretAsLinear() * SourceCoverage * Weights[SourceX - DestX + 1][SourceY - DestY + 1];
								Coverage += SourceCoverage * Weights[SourceX - DestX + 1][SourceY - DestY + 1];
							}
						}
					}

					if(Coverage)
					{
						DestColor = (AccumulatedColor / Coverage).Quantize();
						DestCoverage = -1;
					}
				}
			}
		}
	}

	// Fill zero coverage texels with closest colors using mips
	for(int32 MipIndex = NumMips - 2; MipIndex >= 0; MipIndex--)
	{
		const int32 DstMipSizeX = FMath::Max(1, TextureSizeX >> MipIndex);
		const int32 DstMipSizeY = FMath::Max(1, TextureSizeY >> MipIndex);
		const int32 SrcMipSizeX = FMath::Max(1, TextureSizeX >> (MipIndex + 1));
		const int32 SrcMipSizeY = FMath::Max(1, TextureSizeY >> (MipIndex + 1));

		// Source from higher mip, taking into account which texels are mapped.
		FColor* DstMipData = MipData[MipIndex];
		FColor* SrcMipData = MipData[MipIndex + 1];

		int8* DstMipCoverageData = MipCoverageData[ MipIndex ];
		int8* SrcMipCoverageData = MipCoverageData[ MipIndex + 1 ];

		for(int32 DstY = 0; DstY < DstMipSizeY; DstY++)
		{
			for(int32 DstX = 0; DstX < DstMipSizeX; DstX++)
			{
				const uint32 SrcX = DstX / 2;
				const uint32 SrcY = DstY / 2;

				const FColor& SrcColor = SrcMipData[ SrcY * SrcMipSizeX + SrcX ];
				int8 SrcCoverage = SrcMipCoverageData[ SrcY * SrcMipSizeX + SrcX ];

				FColor& DstColor = DstMipData[ DstY * DstMipSizeX + DstX ];
				int8& DstCoverage = DstMipCoverageData[ DstY * DstMipSizeX + DstX ];

				// Point upsample mip data for zero coverage texels
				// TODO bilinear upsample
				if( SrcCoverage != 0 && DstCoverage == 0 )
				{
					DstColor = SrcColor;
					DstCoverage = SrcCoverage;
				}
			}
		}
	}
}

void FLightMapPendingTexture::StartEncoding()
{
	GLightmapCounter++;

	FColor TextureColor;
	if ( GVisualizeLightmapTextures )
	{
		TextureColor = FColor::MakeRandomColor();
	}

	bool bNeedsSkyOcclusionTexture = false;

	for(int32 AllocationIndex = 0;AllocationIndex < Allocations.Num();AllocationIndex++)
	{
		FLightMapAllocation* Allocation = Allocations[AllocationIndex];

		if (Allocation->bHasSkyShadowing)
		{
			bNeedsSkyOcclusionTexture = true;
			break;
		}
	}

	if (bNeedsSkyOcclusionTexture)
	{
		ULightMapTexture2D* Texture = new(Outer, GetSkyOcclusionTextureName(GLightmapCounter)) ULightMapTexture2D(FPostConstructInitializeProperties());
		SkyOcclusionTexture = Texture;

		Texture->Source.Init2DWithMipChain(GetSizeX(), GetSizeY(), TSF_BGRA8);
		Texture->MipGenSettings = TMGS_LeaveExistingMips;
		int32 NumMips = Texture->Source.GetNumMips();
		Texture->SRGB = false;
		Texture->Filter	= GUseBilinearLightmaps ? TF_Default : TF_Nearest;
		Texture->LODGroup = TEXTUREGROUP_Lightmap;
		Texture->LightmapFlags = ELightMapFlags( LightmapFlags );
		Texture->CompressionNoAlpha = false;
		Texture->CompressionNone = !GCompressLightmaps;

		int32 TextureSizeX = Texture->Source.GetSizeX();
		int32 TextureSizeY = Texture->Source.GetSizeY();

		int32 StartBottom = GetSizeX() * GetSizeY();

		// Lock all mip levels.
		FColor* MipData[MAX_TEXTURE_MIP_COUNT] = {0};
		int8* MipCoverageData[MAX_TEXTURE_MIP_COUNT] = {0};
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			MipData[MipIndex] = (FColor*)Texture->Source.LockMip(MipIndex);

			const int32 MipSizeX = FMath::Max( 1, TextureSizeX >> MipIndex );
			const int32 MipSizeY = FMath::Max( 1, TextureSizeY >> MipIndex );
			MipCoverageData[ MipIndex ] = (int8*)FMemory::Malloc( MipSizeX * MipSizeY );
		}

		// Create the uncompressed top mip-level.
		FColor* TopMipData = MipData[0];
		FMemory::Memzero( TopMipData, TextureSizeX * TextureSizeY * sizeof(FColor) );
		FMemory::Memzero( MipCoverageData[0], TextureSizeX * TextureSizeY );

		FIntRect TextureRect( MAX_int32, MAX_int32, MIN_int32, MIN_int32 );
		for(int32 AllocationIndex = 0;AllocationIndex < Allocations.Num();AllocationIndex++)
		{
			FLightMapAllocation* Allocation = Allocations[AllocationIndex];
			// Link the light-map to the texture.
			Allocation->LightMap->SkyOcclusionTexture = Texture;
			
			// Skip encoding of this texture if we were asked not to bother
			if( !Allocation->bSkipEncoding )
			{
				TextureRect.Min.X = FMath::Min<int32>( TextureRect.Min.X, Allocation->OffsetX );
				TextureRect.Min.Y = FMath::Min<int32>( TextureRect.Min.Y, Allocation->OffsetY );
				TextureRect.Max.X = FMath::Max<int32>( TextureRect.Max.X, Allocation->OffsetX + Allocation->MappedRect.Width() );
				TextureRect.Max.Y = FMath::Max<int32>( TextureRect.Max.Y, Allocation->OffsetY + Allocation->MappedRect.Height() );

				// Copy the raw data for this light-map into the raw texture data array.
				for(int32 Y = Allocation->MappedRect.Min.Y; Y < Allocation->MappedRect.Max.Y; ++Y)
				{
					for(int32 X = Allocation->MappedRect.Min.X; X < Allocation->MappedRect.Max.X; ++X)
					{
						const FLightMapCoefficients& SourceCoefficients = Allocation->RawData[Y * Allocation->TotalSizeX + X];

						int32 DestY = Y - Allocation->MappedRect.Min.Y + Allocation->OffsetY;
						int32 DestX = X - Allocation->MappedRect.Min.X + Allocation->OffsetX;

						FColor&	DestColor = TopMipData[DestY * TextureSizeX + DestX];

						DestColor.R = SourceCoefficients.SkyOcclusion[0];
						DestColor.G = SourceCoefficients.SkyOcclusion[1];
						DestColor.B = SourceCoefficients.SkyOcclusion[2];
						DestColor.A = SourceCoefficients.SkyOcclusion[3];

						int8& DestCoverage = MipCoverageData[0][ DestY * TextureSizeX + DestX ];
						DestCoverage = SourceCoefficients.Coverage / 2;
					}
				}
			}
		}

		GenerateLightmapMipsAndDilate(NumMips, TextureSizeX, TextureSizeY, TextureColor, MipData, MipCoverageData);

		// Unlock all mip levels.
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			Texture->Source.UnlockMip(MipIndex);
			FMemory::Free( MipCoverageData[ MipIndex ] );
		}

		Texture->BeginCachePlatformData();
		new(TotalAsyncTasks) FAsyncLightMapCacheTask(this, LTT_SkyOcclusion);
		NumOutstandingAsyncTasks++;
	}
	

	// Encode and compress the coefficient textures.
	for(uint32 CoefficientIndex = 0; CoefficientIndex < NUM_STORED_LIGHTMAP_COEF; CoefficientIndex += 2)
	{
		// Skip generating simple lightmaps if wanted.
		if( !GEngine->bShouldGenerateLowQualityLightmaps && CoefficientIndex >= LQ_LIGHTMAP_COEF_INDEX )
		{
			continue;
		}

		// Create the light-map texture for this coefficient.
		ULightMapTexture2D* Texture = new(Outer, GetLightmapName(GLightmapCounter, CoefficientIndex)) ULightMapTexture2D(FPostConstructInitializeProperties());
		Textures[CoefficientIndex] = Texture;
		Texture->Source.Init2DWithMipChain(GetSizeX(), GetSizeY() * 2, TSF_BGRA8);	// Top/bottom atlased
		Texture->MipGenSettings = TMGS_LeaveExistingMips;
		int32 NumMips = Texture->Source.GetNumMips();
		Texture->SRGB = 0;
		Texture->Filter	= GUseBilinearLightmaps ? TF_Default : TF_Nearest;
		Texture->LODGroup = TEXTUREGROUP_Lightmap;
		Texture->LightmapFlags = ELightMapFlags( LightmapFlags );
		Texture->CompressionNoAlpha = CoefficientIndex >= LQ_LIGHTMAP_COEF_INDEX;
		Texture->CompressionNone = !GCompressLightmaps;
		Texture->bForcePVRTC4 = true;

		int32 TextureSizeX = Texture->Source.GetSizeX();
		int32 TextureSizeY = Texture->Source.GetSizeY();
		
		int32 StartBottom = GetSizeX() * GetSizeY();
		
		// Lock all mip levels.
		FColor* MipData[MAX_TEXTURE_MIP_COUNT] = {0};
		int8* MipCoverageData[MAX_TEXTURE_MIP_COUNT] = {0};
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			MipData[MipIndex] = (FColor*)Texture->Source.LockMip(MipIndex);

			const int32 MipSizeX = FMath::Max( 1, TextureSizeX >> MipIndex );
			const int32 MipSizeY = FMath::Max( 1, TextureSizeY >> MipIndex );
			MipCoverageData[ MipIndex ] = (int8*)FMemory::Malloc( MipSizeX * MipSizeY );
		}

		// Create the uncompressed top mip-level.
		FColor* TopMipData = MipData[0];
		FMemory::Memzero( TopMipData, TextureSizeX * TextureSizeY * sizeof(FColor) );
		FMemory::Memzero( MipCoverageData[0], TextureSizeX * TextureSizeY );

		FIntRect TextureRect( MAX_int32, MAX_int32, MIN_int32, MIN_int32 );
		for(int32 AllocationIndex = 0;AllocationIndex < Allocations.Num();AllocationIndex++)
		{
			FLightMapAllocation* Allocation = Allocations[AllocationIndex];
			// Link the light-map to the texture.
			Allocation->LightMap->Textures[ CoefficientIndex / 2 ] = Texture;
			for( int k = 0; k < 2; k++ )
			{
				Allocation->LightMap->ScaleVectors[ CoefficientIndex + k ] = FVector4(
					Allocation->Scale[ CoefficientIndex + k ][0],
					Allocation->Scale[ CoefficientIndex + k ][1],
					Allocation->Scale[ CoefficientIndex + k ][2],
					Allocation->Scale[ CoefficientIndex + k ][3]
					);

				Allocation->LightMap->AddVectors[ CoefficientIndex + k ] = FVector4(
					Allocation->Add[ CoefficientIndex + k ][0],
					Allocation->Add[ CoefficientIndex + k ][1],
					Allocation->Add[ CoefficientIndex + k ][2],
					Allocation->Add[ CoefficientIndex + k ][3]
					);
			}

			// Skip encoding of this texture if we were asked not to bother
			if( !Allocation->bSkipEncoding )
			{
				TextureRect.Min.X = FMath::Min<int32>( TextureRect.Min.X, Allocation->OffsetX );
				TextureRect.Min.Y = FMath::Min<int32>( TextureRect.Min.Y, Allocation->OffsetY );
				TextureRect.Max.X = FMath::Max<int32>( TextureRect.Max.X, Allocation->OffsetX + Allocation->MappedRect.Width() );
				TextureRect.Max.Y = FMath::Max<int32>( TextureRect.Max.Y, Allocation->OffsetY + Allocation->MappedRect.Height() );
	
				// Copy the raw data for this light-map into the raw texture data array.
				for(int32 Y = Allocation->MappedRect.Min.Y; Y < Allocation->MappedRect.Max.Y; ++Y)
				{
					for(int32 X = Allocation->MappedRect.Min.X; X < Allocation->MappedRect.Max.X; ++X)
					{
						const FLightMapCoefficients& SourceCoefficients = Allocation->RawData[Y * Allocation->TotalSizeX + X];
						
						int32 DestY = Y - Allocation->MappedRect.Min.Y + Allocation->OffsetY;
						int32 DestX = X - Allocation->MappedRect.Min.X + Allocation->OffsetX;

						FColor&	DestColor = TopMipData[DestY * TextureSizeX + DestX];
						int8&	DestCoverage = MipCoverageData[0][ DestY * TextureSizeX + DestX ];

						FColor&	DestBottomColor = TopMipData[ StartBottom + DestX + DestY * TextureSizeX ];
						int8&	DestBottomCoverage = MipCoverageData[0][ StartBottom + DestX + DestY * TextureSizeX ];
						
#if VISUALIZE_PACKING
						if( X == Allocation->MappedRect.Min.X || Y == Allocation->MappedRect.Min.Y ||
							X == Allocation->MappedRect.Max.X-1 || Y == Allocation->MappedRect.Max.Y-1 ||
							X == Allocation->MappedRect.Min.X+1 || Y == Allocation->MappedRect.Min.Y+1 ||
							X == Allocation->MappedRect.Max.X-2 || Y == Allocation->MappedRect.Max.Y-2 )
						{
							DestColor = FColor(255,0,0);
						}
						else
						{
							DestColor = FColor(0,255,0);
						}
#else
						DestColor.R = SourceCoefficients.Coefficients[CoefficientIndex][0];
						DestColor.G = SourceCoefficients.Coefficients[CoefficientIndex][1];
						DestColor.B = SourceCoefficients.Coefficients[CoefficientIndex][2];
						DestColor.A = SourceCoefficients.Coefficients[CoefficientIndex][3];

						DestBottomColor.R = SourceCoefficients.Coefficients[ CoefficientIndex + 1 ][0];
						DestBottomColor.G = SourceCoefficients.Coefficients[ CoefficientIndex + 1 ][1];
						DestBottomColor.B = SourceCoefficients.Coefficients[ CoefficientIndex + 1 ][2];
						DestBottomColor.A = SourceCoefficients.Coefficients[ CoefficientIndex + 1 ][3];

						if ( GVisualizeLightmapTextures )
						{
							DestColor = TextureColor;
						}

						// uint8 -> int8
						DestCoverage = DestBottomCoverage = SourceCoefficients.Coverage / 2;
						if ( SourceCoefficients.Coverage > 0 )
						{
							GNumLightmapMappedTexels++;
						}
						else
						{
							GNumLightmapUnmappedTexels++;
						}

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING && WITH_EDITOR
						int32 PaddedX = X;
						int32 PaddedY = Y;
						if (GLightmassDebugOptions.bPadMappings && (Allocation->PaddingType == LMPT_NormalPadding))
						{
							if (Allocation->TotalSizeX - 2 > 0 && Allocation->TotalSizeY - 2 > 0)
							{
								PaddedX -= 1;
								PaddedY -= 1;
							}
						}

						if (Allocation->bDebug
							&& PaddedX == GCurrentSelectedLightmapSample.LocalX
							&& PaddedY == GCurrentSelectedLightmapSample.LocalY)
						{
							GCurrentSelectedLightmapSample.OriginalColor = DestColor;
							extern FColor GTexelSelectionColor;
							DestColor = GTexelSelectionColor;
						}
#endif
#endif
					}
				}
			}
		}

		GNumLightmapTotalTexels += Texture->Source.GetSizeX() * Texture->Source.GetSizeY();
		GNumLightmapTotalTexelsNonPow2 += TextureRect.Width() * TextureRect.Height();
		GNumLightmapTextures++;

		GenerateLightmapMipsAndDilate(NumMips, TextureSizeX, TextureSizeY, TextureColor, MipData, MipCoverageData);

		// Unlock all mip levels.
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			Texture->Source.UnlockMip(MipIndex);
			FMemory::Free( MipCoverageData[ MipIndex ] );
		}

		Texture->BeginCachePlatformData();
		new(TotalAsyncTasks) FAsyncLightMapCacheTask(this, (ELightmapTextureType)CoefficientIndex);
		NumOutstandingAsyncTasks++;
	}

	for(int32 AllocationIndex = 0; AllocationIndex < Allocations.Num(); AllocationIndex++)
	{
		FLightMapAllocation* Allocation = Allocations[AllocationIndex];

		int32 PaddedSizeX = Allocation->TotalSizeX;
		int32 PaddedSizeY = Allocation->TotalSizeY;
		int32 BaseX = Allocation->OffsetX - Allocation->MappedRect.Min.X;
		int32 BaseY = Allocation->OffsetY - Allocation->MappedRect.Min.Y;
		if (FPlatformProperties::HasEditorOnlyData() && GLightmassDebugOptions.bPadMappings && (Allocation->PaddingType == LMPT_NormalPadding))
		{
			if ((PaddedSizeX - 2 > 0) && ((PaddedSizeY - 2) > 0))
			{
				PaddedSizeX -= 2;
				PaddedSizeY -= 2;
				BaseX += 1;
				BaseY += 1;
			}
		}
		
		// Calculate the coordinate scale/biases this light-map.
		FVector2D Scale((float)PaddedSizeX / (float)GetSizeX(), (float)PaddedSizeY / (float)GetSizeY());
		FVector2D Bias((float)BaseX / (float)GetSizeX(), (float)BaseY / (float)GetSizeY());

		// let the lightmap finish up after being encoded, setting the scale/bias, and returning if it can be deleted
		check( Allocation->LightMap );
		Allocation->LightMap->FinalizeEncoding(Scale, Bias, Textures[0]);

		// Free the light-map's raw data.
		Allocation->RawData.Empty();
	}
}

FName FLightMapPendingTexture::GetLightmapName(int32 TextureIndex, int32 CoefficientIndex)
{
	check(CoefficientIndex >= 0 && CoefficientIndex < NUM_STORED_LIGHTMAP_COEF);
	FString PotentialName = TEXT("");
	UObject* ExistingObject = NULL;
	int32 LightmapIndex = 0;
	// Search for an unused name
	do
	{
		if (CoefficientIndex < NUM_HQ_LIGHTMAP_COEF)
		{
			PotentialName = FString(TEXT("HQ_Lightmap")) + FString::FromInt(LightmapIndex) + TEXT("_") + FString::FromInt(TextureIndex);
		}
		else
		{
			PotentialName = FString(TEXT("LQ_Lightmap")) + TEXT("_") + FString::FromInt(LightmapIndex) + TEXT("_") + FString::FromInt(TextureIndex);
		}
		ExistingObject = FindObject<UObject>(Outer, *PotentialName);
		LightmapIndex++;
	}
	while (ExistingObject != NULL);
	return FName(*PotentialName);
}

FName FLightMapPendingTexture::GetSkyOcclusionTextureName(int32 TextureIndex)
{
	FString PotentialName = TEXT("");
	UObject* ExistingObject = NULL;
	int32 LightmapIndex = 0;
	// Search for an unused name
	do
	{
		PotentialName = FString(TEXT("SkyOcclusion")) + FString::FromInt(LightmapIndex) + TEXT("_") + FString::FromInt(TextureIndex);

		ExistingObject = FindObject<UObject>(Outer, *PotentialName);
		LightmapIndex++;
	}
	while (ExistingObject != NULL);
	return FName(*PotentialName);
}

/** The light-maps which have not yet been encoded into textures. */
static TIndirectArray<FLightMapAllocation> PendingLightMaps;
static uint32 PendingLightMapSize = 0;

#endif // WITH_EDITOR

/** If true, update the status when encoding light maps */
bool FLightMap2D::bUpdateStatus = true;

FLightMap2D* FLightMap2D::AllocateLightMap(UObject* LightMapOuter, FQuantizedLightmapData*& SourceQuantizedData, const FBoxSphereBounds& Bounds, ELightMapPaddingType InPaddingType, ELightMapFlags InLightmapFlags )
{
	// If the light-map has no lights in it, return NULL.
	if(!SourceQuantizedData)
	{
		return NULL;
	}

#if WITH_EDITOR

	FLightMapAllocation* Allocation = new(PendingLightMaps) FLightMapAllocation(SourceQuantizedData);

	Allocation->Outer		= LightMapOuter->GetOutermost();
	Allocation->PaddingType	= InPaddingType;
	Allocation->LightmapFlags = InLightmapFlags;
	Allocation->Bounds		= Bounds;
	Allocation->Primitive	= LightMapOuter;
	if ( !GAllowStreamingLightmaps )
	{
		Allocation->LightmapFlags = ELightMapFlags( Allocation->LightmapFlags & ~LMF_Streamed );
	}

	// Create a new light-map.
	FLightMap2D* LightMap = new FLightMap2D(SourceQuantizedData->LightGuids);
	Allocation->LightMap = LightMap;

#if ALLOW_LIGHTMAP_SAMPLE_DEBUGGING && WITH_EDITOR
	// Detect if this allocation belongs to the texture mapping that was being debugged
	//@todo - this only works for mappings that can be uniquely identified by a single component, BSP for example does not work.
	if (GCurrentSelectedLightmapSample.Component && GCurrentSelectedLightmapSample.Component == LightMapOuter)
	{
		GCurrentSelectedLightmapSample.Lightmap = LightMap;
		Allocation->bDebug = true;
	}
	else
	{
		Allocation->bDebug = false;
	}
#endif

	// Quantized data is no longer needed now that FLightMapAllocation has made a copy of it
	if (SourceQuantizedData != NULL)
	{
		delete SourceQuantizedData;
		SourceQuantizedData = NULL;
	}

	// Track the size of pending light-maps.
	PendingLightMapSize += ((Allocation->TotalSizeX + 3) & ~3) * ((Allocation->TotalSizeY + 3) & ~3);

	return LightMap;
#else
	return NULL;
#endif // WITH_EDITOR
}

#if WITH_EDITOR

struct FCompareLightmaps
{
	FORCEINLINE bool operator()( const FLightMapAllocation& A, const FLightMapAllocation& B ) const
	{
		return FMath::Max(B.TotalSizeX,B.TotalSizeY) < FMath::Max(A.TotalSizeX,A.TotalSizeY);
	}
};

#endif //WITH_EDITOR
/**
 * Executes all pending light-map encoding requests.
 * @param	bLightingSuccessful	Whether the lighting build was successful or not.
 * @param	bForceCompletion	Force all encoding to be fully completed (they may be asynchronous).
 */
void FLightMap2D::EncodeTextures( UWorld* InWorld, bool bLightingSuccessful, bool bForceCompletion )
{
#if WITH_EDITOR
	if ( bLightingSuccessful )
	{
		GWarn->BeginSlowTask( NSLOCTEXT("LightMap2D", "BeginEncodingLightMapsTask", "Encoding light-maps"), false );
		int32 PackedLightAndShadowMapTextureSize = InWorld->GetWorldSettings()->PackedLightAndShadowMapTextureSize;

		// Reset the pending light-map size.
		PendingLightMapSize = 0;

		// Sort the light-maps in descending order by size.
		Sort( PendingLightMaps.GetTypedData(), PendingLightMaps.Num(), FCompareLightmaps() );

		// Allocate texture space for each light-map.
		TArray<FLightMapPendingTexture*> PendingTextures;

		for(int32 LightMapIndex = 0;LightMapIndex < PendingLightMaps.Num();LightMapIndex++)
		{
			FLightMapAllocation& Allocation = PendingLightMaps[LightMapIndex];
			FLightMapPendingTexture* Texture = NULL;

			if ( GAllowLightmapCropping )
			{
				CropUnmappedTexels( Allocation.RawData, Allocation.TotalSizeX, Allocation.TotalSizeY, Allocation.MappedRect );
			}

			// Find an existing texture which the light-map can be stored in.
			int32 FoundIndex = -1;
			for(int32 TextureIndex = 0;TextureIndex < PendingTextures.Num();TextureIndex++)
			{
				FLightMapPendingTexture* ExistingTexture = PendingTextures[TextureIndex];

				// Lightmaps will always be 4-pixel aligned...
				if ( ExistingTexture->AddElement( Allocation ) )
				{
					Texture = ExistingTexture;
					FoundIndex = TextureIndex;
					break;
				}
			}

			if(!Texture)
			{
				int32 NewTextureSizeX = PackedLightAndShadowMapTextureSize;
				int32 NewTextureSizeY = PackedLightAndShadowMapTextureSize / 2;
				if(Allocation.MappedRect.Width() > NewTextureSizeX || Allocation.MappedRect.Height() > NewTextureSizeY)
				{
					NewTextureSizeX = FMath::RoundUpToPowerOfTwo(Allocation.MappedRect.Width());
					NewTextureSizeY = FMath::RoundUpToPowerOfTwo(Allocation.MappedRect.Height());

					// Force 2:1 aspect
					NewTextureSizeX = FMath::Max( NewTextureSizeX, NewTextureSizeY * 2 );
					NewTextureSizeY = FMath::Max( NewTextureSizeY, NewTextureSizeX / 2 );
				}

				// If there is no existing appropriate texture, create a new one.
				Texture = new FLightMapPendingTexture( InWorld, NewTextureSizeX, NewTextureSizeY );
				PendingTextures.Add( Texture );
				Texture->Outer = Allocation.Outer;
				Texture->Bounds = Allocation.Bounds;
				Texture->LightmapFlags = Allocation.LightmapFlags;
				verify( Texture->AddElement( Allocation ) );
				FoundIndex = PendingTextures.Num() - 1;
			}

			Texture->Allocations.Add(&Allocation);
		}

		// Encode all the pending textures.
		for(int32 TextureIndex = 0; TextureIndex < PendingTextures.Num(); TextureIndex++)
		{
			if (bUpdateStatus && (TextureIndex % 20) == 0)
			{
				GWarn->UpdateProgress(TextureIndex, PendingTextures.Num());
			}
			FLightMapPendingTexture* PendingTexture = PendingTextures[TextureIndex];
			PendingTexture->StartEncoding();
		}

		PendingTextures.Empty();
		PendingLightMaps.Empty();

		if ( bForceCompletion )
		{
			// Block until there are 0 unfinished tasks, making sure all compression has completed.
			FLightMapPendingTexture::FinishCompletedTasks( 0 );
		}
	
		
		// End the encoding lighmaps slow task
		GWarn->EndSlowTask();
		
	}
	else
	{
		PendingLightMaps.Empty();
	}
#endif //WITH_EDITOR
}

FLightMap2D::FLightMap2D()
{
	Textures[0] = NULL;
	Textures[1] = NULL;
	SkyOcclusionTexture = NULL;
}

FLightMap2D::FLightMap2D(const TArray<FGuid>& InLightGuids)
{
	LightGuids = InLightGuids;
	Textures[0] = NULL;
	Textures[1] = NULL;
	SkyOcclusionTexture = NULL;
}

const UTexture2D* FLightMap2D::GetTexture(uint32 BasisIndex) const
{
	check(IsValid(BasisIndex));
	return Textures[BasisIndex];
}

UTexture2D* FLightMap2D::GetTexture(uint32 BasisIndex)
{
	check(IsValid(BasisIndex));
	return Textures[BasisIndex];
}

/**
 * Returns whether the specified basis has a valid lightmap texture or not.
 * @param	BasisIndex - The basis index.
 * @return	true if the specified basis has a valid lightmap texture, otherwise false
 */
bool FLightMap2D::IsValid(uint32 BasisIndex) const
{
	return bAllowHighQualityLightMaps ? BasisIndex == 0 : BasisIndex == 1;
}

struct FLegacyLightMapTextureInfo
{
	ULightMapTexture2D* Texture;
	FLinearColor Scale;
	FLinearColor Bias;

	friend FArchive& operator<<(FArchive& Ar,FLegacyLightMapTextureInfo& I)
	{
		return Ar << I.Texture << I.Scale << I.Bias;
	}
};

/**
 * Finalizes the lightmap after encodeing, including setting the UV scale/bias for this lightmap 
 * inside the larger UTexture2D that this lightmap is in
 * 
 * @param Scale UV scale (size)
 * @param Bias UV Bias (offset)
 * @param ALightmapTexture One of the lightmap textures that this lightmap was put into
 *
 * @return true if the lightmap should be deleted after this call
 */
void FLightMap2D::FinalizeEncoding(const FVector2D& Scale, const FVector2D& Bias, UTexture2D* ALightmapTexture)
{
	CoordinateScale = Scale;
	CoordinateBias = Bias;
}

void FLightMap2D::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(Textures[0]);
	Collector.AddReferencedObject(Textures[1]);
	Collector.AddReferencedObject(SkyOcclusionTexture);
}

void FLightMap2D::Serialize(FArchive& Ar)
{
	FLightMap::Serialize(Ar);

	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_SH_LIGHTMAPS)
	{
		for(uint32 CoefficientIndex = 0;CoefficientIndex < 3;CoefficientIndex++)
		{
			ULightMapTexture2D* Dummy = NULL;
			Ar << Dummy;
			FVector Dummy2;
			Ar << Dummy2;
		}
	}
	else if( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_LIGHTMAP_COMPRESSION )
	{
		for( uint32 CoefficientIndex = 0; CoefficientIndex < 5; CoefficientIndex++ )
		{
			ULightMapTexture2D* Dummy = NULL;
			Ar << Dummy;
			FVector Dummy2;
			Ar << Dummy2;
			Ar << Dummy2;
		}
	}
	else if( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_LOW_QUALITY_DIRECTIONAL_LIGHTMAPS )
	{
		for(uint32 CoefficientIndex = 0;CoefficientIndex < 3;CoefficientIndex++)
		{
			Ar << Textures[CoefficientIndex];
			Ar << ScaleVectors[CoefficientIndex];
			Ar << AddVectors[CoefficientIndex];
		}

		ScaleVectors[0].W *= 11.5f;
		AddVectors[0].W = ( AddVectors[0].W - 0.5f ) * 11.5f;

		ScaleVectors[1] *= FVector4( -0.325735f, 0.325735f, -0.325735f, 0.0f );
		AddVectors[1] *= FVector4( -0.325735f, 0.325735f, -0.325735f, 0.0f );
		AddVectors[1].W = 0.282095f;
	}
	else if( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_COMBINED_LIGHTMAP_TEXTURES )
	{
		for( uint32 CoefficientIndex = 0; CoefficientIndex < 4; CoefficientIndex++ )
		{
			ULightMapTexture2D* Dummy = NULL;
			Ar << Dummy;
			FVector4 Dummy2;
			Ar << Dummy2;
			Ar << Dummy2;
		}
	}
	else
	{
		if (Ar.IsCooking())
		{
			bool bStripLQLightmaps = !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::LowQualityLightmaps);
			bool bStripHQLightmaps = !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::HighQualityLightmaps);

			ULightMapTexture2D* Dummy = NULL;
			Ar << ( bStripHQLightmaps ? Dummy : Textures[0] );
			Ar << ( bStripLQLightmaps ? Dummy : Textures[1] );
		}
		else
		{
			Ar << Textures[0];
			Ar << Textures[1];
		}

		if (Ar.UE4Ver() >= VER_UE4_SKY_LIGHT_COMPONENT)
		{
			if (Ar.IsCooking())
			{
				bool bStripHQLightmaps = !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::HighQualityLightmaps);

				ULightMapTexture2D* Dummy = NULL;
				Ar << (bStripHQLightmaps ? Dummy : SkyOcclusionTexture);
			}
			else
			{
				Ar << SkyOcclusionTexture;
			}
		}
		
		for(uint32 CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
		{
			Ar << ScaleVectors[CoefficientIndex];
			Ar << AddVectors[CoefficientIndex];
		}
	}

	Ar << CoordinateScale << CoordinateBias;
	
	// Force no divide by zeros even with low precision. This should be fixed during build but for some reason isn't.
	if( Ar.IsLoading() )
	{
		for( int k = 0; k < 3; k++ )
		{
			ScaleVectors[2][k] = FMath::Max( 0.0f, ScaleVectors[2][k] );
			AddVectors[2][k] = FMath::Max( 0.01f, AddVectors[2][k] );
		}
	}

	//Release unneeded texture references on load so they will be garbage collected.
	//In the editor we need to keep these references since they will need to be saved.
	if (Ar.IsLoading() && !GIsEditor)
	{
		Textures[ bAllowHighQualityLightMaps ? 1 : 0 ] = NULL;

		if (!bAllowHighQualityLightMaps)
		{
			SkyOcclusionTexture = NULL;
		}
	}
}

FLightMapInteraction FLightMap2D::GetInteraction() const
{
	int32 LightmapIndex = bAllowHighQualityLightMaps ? 0 : 1;

	bool bValidTextures = Textures[ LightmapIndex ] && Textures[ LightmapIndex ]->Resource;

	// When the FLightMap2D is first created, the textures aren't set, so that case needs to be handled.
	if(bValidTextures)
	{
		return FLightMapInteraction::Texture(Textures, SkyOcclusionTexture, ScaleVectors, AddVectors, CoordinateScale, CoordinateBias, true);
	}

	return FLightMapInteraction::None();
}

void FLegacyLightMap1D::Serialize(FArchive& Ar)
{
	FLightMap::Serialize(Ar);

	check(Ar.IsLoading());

	UObject* Owner;
	TQuantizedLightSampleBulkData<FQuantizedDirectionalLightSample> DirectionalSamples;
	TQuantizedLightSampleBulkData<FQuantizedSimpleLightSample> SimpleSamples;

	Ar << Owner;

	DirectionalSamples.Serialize( Ar, Owner );

	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_SH_LIGHTMAPS)
	{
		for (int32 ElementIndex = 0; ElementIndex < 3; ElementIndex++)
		{
			FVector Dummy;
			Ar << Dummy;
		}
	}
	else
	{
		for (int32 ElementIndex = 0; ElementIndex < 5; ElementIndex++)
		{
			FVector Dummy;
			Ar << Dummy;
		}
	}

	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_SH_LIGHTMAPS)
	{
		TQuantizedLightSampleBulkData<FLegacyQuantizedSimpleLightSample> Dummy;
		Dummy.Serialize(Ar, Owner);
	}
	else
	{
		SimpleSamples.Serialize( Ar, Owner );
	}
}

/*-----------------------------------------------------------------------------
	FQuantizedLightSample version of bulk data.
-----------------------------------------------------------------------------*/

/**
 * Returns whether single element serialization is required given an archive. This e.g.
 * can be the case if the serialization for an element changes and the single element
 * serialization code handles backward compatibility.
 */
template<class QuantizedLightSampleType>
bool TQuantizedLightSampleBulkData<QuantizedLightSampleType>::RequiresSingleElementSerialization( FArchive& Ar )
{
	return false;
}

/**
 * Returns size in bytes of single element.
 *
 * @return Size in bytes of single element
 */
template<class QuantizedLightSampleType>
int32 TQuantizedLightSampleBulkData<QuantizedLightSampleType>::GetElementSize() const
{
	return sizeof(QuantizedLightSampleType);
}

/**
 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
 * 
 * @param Ar			Archive to serialize with
 * @param Data			Base pointer to data
 * @param ElementIndex	Element index to serialize
 */
template<class QuantizedLightSampleType>
void TQuantizedLightSampleBulkData<QuantizedLightSampleType>::SerializeElement( FArchive& Ar, void* Data, int32 ElementIndex )
{
	QuantizedLightSampleType* QuantizedLightSample = (QuantizedLightSampleType*)Data + ElementIndex;
	// serialize as colors
	const uint32 NumCoefficients = sizeof(QuantizedLightSampleType) / sizeof(FColor);
	for(int32 CoefficientIndex = 0; CoefficientIndex < NumCoefficients; CoefficientIndex++)
	{
		uint32 ColorDWORD = QuantizedLightSample->Coefficients[CoefficientIndex].DWColor();
		Ar << ColorDWORD;
		QuantizedLightSample->Coefficients[CoefficientIndex] = FColor(ColorDWORD);
	} 
};

FArchive& operator<<(FArchive& Ar,FLightMap*& R)
{
	uint32 LightMapType = FLightMap::LMT_None;
	if(Ar.IsSaving())
	{
		if(R != NULL)
		{
			if(R->GetLightMap2D())
			{
				LightMapType = FLightMap::LMT_2D;
			}
		}
	}
	Ar << LightMapType;

	if(Ar.IsLoading())
	{
		if(LightMapType == FLightMap::LMT_1D)
		{
			R = new FLegacyLightMap1D();
		}
		else if(LightMapType == FLightMap::LMT_2D)
		{
			R = new FLightMap2D();
		}
	}

	if(R != NULL)
	{
		R->Serialize(Ar);

		// Toss legacy vertex lightmaps
		if (LightMapType == FLightMap::LMT_1D)
		{
			delete R;
			R = NULL;
		}

		// Dump old lightmaps
		if( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_COMBINED_LIGHTMAP_TEXTURES )
		{
			delete R;
			R = NULL;
		}
	}

	return Ar;
}

bool FQuantizedLightmapData::HasNonZeroData() const
{
	// 1D lightmaps don't have a valid coverage amount, so they shouldn't be discarded if the coverage is 0
	uint8 MinCoverageThreshold = (SizeY == 1) ? 0 : 1;

	// Check all of the samples for a non-zero coverage (if valid) and at least one non-zero coefficient
	for (int32 SampleIndex = 0; SampleIndex < Data.Num(); SampleIndex++)
	{
		const FLightMapCoefficients& LightmapSample = Data[SampleIndex];

		if (LightmapSample.Coverage >= MinCoverageThreshold)
		{
			for (int32 CoefficentIndex = 0; CoefficentIndex < NUM_STORED_LIGHTMAP_COEF; CoefficentIndex++)
			{
				if ((LightmapSample.Coefficients[CoefficentIndex][0] != 0) || (LightmapSample.Coefficients[CoefficentIndex][1] != 0) || (LightmapSample.Coefficients[CoefficentIndex][2] != 0))
				{
					return true;
				}
			}

			for (int32 Index = 0; Index < ARRAY_COUNT(LightmapSample.SkyOcclusion); Index++)
			{
				if (LightmapSample.SkyOcclusion[Index] != 0)
				{
					return true;
				}
			}
		}
	}

	return false;
}
