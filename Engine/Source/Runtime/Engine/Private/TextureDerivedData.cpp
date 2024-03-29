// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureDerivedData.cpp: Derived data management for textures.
=============================================================================*/

#include "EnginePrivate.h"

enum
{
	/** The number of mips to store inline. */
	NUM_INLINE_DERIVED_MIPS = 7,
};

#if WITH_EDITORONLY_DATA

#include "UObjectAnnotation.h"
#include "DerivedDataCacheInterface.h"
#include "DerivedDataPluginInterface.h"
#include "TextureCompressorModule.h"
#include "DDSLoader.h"
#include "RenderUtils.h"
#include "TargetPlatform.h"

#include "ImageCore.h"

/*------------------------------------------------------------------------------
	Versioning for texture derived data.
------------------------------------------------------------------------------*/

// The current version string is set up to mimic the old versioning scheme and to make
// sure the DDC does not get invalidated right now. If you need to bump the version, replace it
// with a guid ( ex.: TEXT("855EE5B3574C43ABACC6700C4ADC62E6") )
// In case of merge conflicts with DDC versions, you MUST generate a new GUID and set this new
// guid as version

#define TEXTURE_DERIVEDDATA_VER		TEXT("4F83E7F4EC4E4AB788364F736C9E4311")

/*------------------------------------------------------------------------------
	Timing of derived data operations.
------------------------------------------------------------------------------*/

namespace TextureDerivedDataTimings
{
	enum ETimingId
	{
		GetMipDataTime,
		AsyncBlockTime,
		SyncBlockTime,
		BuildTextureTime,
		SerializeCookedTime,
		NumTimings
	};

	static FThreadSafeCounter Timings[NumTimings] = {0};

	static const TCHAR* TimingStrings[NumTimings] =
	{
		TEXT("Get Mip Data"),
		TEXT("Asynchronous Block"),
		TEXT("Synchronous Loads"),
		TEXT("Build Textures"),
		TEXT("Serialize Cooked")
	};

	void PrintTimings()
	{
		UE_LOG(LogTexture,Log,TEXT("--- Texture Derived Data Timings ---"));
		for (int32 TimingIndex = 0; TimingIndex < NumTimings; ++TimingIndex)
		{
			UE_LOG(LogTexture,Display,TEXT("%s: %fs"), TimingStrings[TimingIndex], FPlatformTime::ToSeconds(Timings[TimingIndex].GetValue()) );
		}
	}

	FAutoConsoleCommand DumpTimingsCommand(
		TEXT("Tex.DerivedDataTimings"),
		TEXT("Print timings related to texture derived data."),
		FConsoleCommandDelegate::CreateStatic(PrintTimings)
		);

	struct FScopedMeasurement
	{
		ETimingId TimingId;
		uint32 StartCycles;

		explicit FScopedMeasurement(ETimingId InTimingId)
			: TimingId(InTimingId)
		{
			StartCycles = FPlatformTime::Cycles();
		}

		~FScopedMeasurement()
		{
			uint32 TimeInCycles = FPlatformTime::Cycles() - StartCycles;
			Timings[TimingId].Add(TimeInCycles);
		}
	};
}

#endif // #if WITH_EDITORONLY_DATA

/*------------------------------------------------------------------------------
	Derived data key generation.
------------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA

/**
 * Serialize build settings for use when generating the derived data key.
 */
static void SerializeForKey(FArchive& Ar, const FTextureBuildSettings& Settings)
{
	uint32 TempUint32;
	float TempFloat;
	uint8 TempByte;

	TempFloat = Settings.ColorAdjustment.AdjustBrightness; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustBrightnessCurve; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustSaturation; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustVibrance; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustRGBCurve; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustHue; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustMinAlpha; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustMaxAlpha; Ar << TempFloat;
	TempFloat = Settings.MipSharpening; Ar << TempFloat;
	TempUint32 = Settings.DiffuseConvolveMipLevel; Ar << TempUint32;
	TempUint32 = Settings.SharpenMipKernelSize; Ar << TempUint32;
	// NOTE: TextureFormatName is not stored in the key here.
	TempByte = Settings.MipGenSettings; Ar << TempByte;
	TempByte = Settings.bCubemap; Ar << TempByte;
	TempByte = Settings.bSRGB; Ar << TempByte;
	TempByte = Settings.bPreserveBorder; Ar << TempByte;
	TempByte = Settings.bDitherMipMapAlpha; Ar << TempByte;
	TempByte = Settings.bComputeBokehAlpha; Ar << TempByte;
	TempByte = Settings.bReplicateRed; Ar << TempByte;
	TempByte = Settings.bReplicateAlpha; Ar << TempByte;
	TempByte = Settings.bDownsampleWithAverage; Ar << TempByte;
	TempByte = Settings.bSharpenWithoutColorShift; Ar << TempByte;
	TempByte = Settings.bBorderColorBlack; Ar << TempByte;
	TempByte = Settings.bFlipGreenChannel; Ar << TempByte;
	TempByte = Settings.bApplyKernelToTopMip; Ar << TempByte;
	TempByte = Settings.CompositeTextureMode; Ar << TempByte;
	TempFloat = Settings.CompositePower; Ar << TempFloat;
}

/**
 * Computes the derived data key suffix for a texture with the specified compression settings.
 * @param Texture - The texture for which to compute the derived data key.
 * @param CompressionSettings - Compression settings for which to compute the derived data key.
 * @param OutKeySuffix - The derived data key suffix.
 */
static void GetTextureDerivedDataKeySuffix(
	const UTexture& Texture,
	const FTextureBuildSettings& BuildSettings,
	FString& OutKeySuffix
	)
{
	uint16 Version = 0;

	// get the version for this texture's platform format
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	const ITextureFormat* TextureFormat = NULL;
	if (TPM)
	{
		TextureFormat = TPM->FindTextureFormat(BuildSettings.TextureFormatName);
		if (TextureFormat)
		{
			Version = TextureFormat->GetVersion(BuildSettings.TextureFormatName);
		}
	}
	
	FString CompositeTextureStr;

	if(IsValid(Texture.CompositeTexture) && Texture.CompositeTextureMode != CTM_Disabled)
	{
		CompositeTextureStr += TEXT("_");
		CompositeTextureStr += Texture.CompositeTexture->Source.GetIdString();
	}

	// build the key, but don't use include the version if it's 0 to be backwards compatible
	OutKeySuffix = FString::Printf(TEXT("%s_%s%s%s_%02u_%s"),
		*BuildSettings.TextureFormatName.GetPlainNameString(),
		Version == 0 ? TEXT("") : *FString::Printf(TEXT("%d_"), Version),
		*Texture.Source.GetIdString(),
		*CompositeTextureStr,
		(uint32)NUM_INLINE_DERIVED_MIPS,
		(TextureFormat == NULL) ? TEXT("") : *TextureFormat->GetDerivedDataKeyString(Texture)
		);

	// Serialize the compressor settings into a temporary array. The archive
	// is flagged as persistent so that machines of different endianness produce
	// identical binary results.
	TArray<uint8> TempBytes;
	TempBytes.Reserve(64);
	FMemoryWriter Ar(TempBytes, /*bIsPersistent=*/ true);
	SerializeForKey(Ar, BuildSettings);

	// Now convert the raw bytes to a string.
	const uint8* SettingsAsBytes = TempBytes.GetTypedData();
	OutKeySuffix.Reserve(OutKeySuffix.Len() + TempBytes.Num());
	for (int32 ByteIndex = 0; ByteIndex < TempBytes.Num(); ++ByteIndex)
	{
		ByteToHex(SettingsAsBytes[ByteIndex], OutKeySuffix);
	}
}

/**
 * Constructs a derived data key from the key suffix.
 * @param KeySuffix - The key suffix.
 * @param OutKey - The full derived data key.
 */
static void GetTextureDerivedDataKeyFromSuffix(const FString& KeySuffix, FString& OutKey)
{
	OutKey = FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("TEXTURE"),
		TEXTURE_DERIVEDDATA_VER,
		*KeySuffix
		);
}

/**
 * Constructs the derived data key for an individual mip.
 * @param KeySuffix - The key suffix.
 * @param MipIndex - The mip index.
 * @param OutKey - The full derived data key for the mip.
 */
static void GetTextureDerivedMipKey(
	int32 MipIndex,
	const FTexture2DMipMap& Mip,
	const FString& KeySuffix,
	FString& OutKey
	)
{
	OutKey = FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("TEXTURE"),
		TEXTURE_DERIVEDDATA_VER,
		*FString::Printf(TEXT("%s_MIP%u_%dx%d"), *KeySuffix, MipIndex, Mip.SizeX, Mip.SizeY)
		);
}

/**
 * Computes the derived data key for a texture with the specified compression settings.
 * @param Texture - The texture for which to compute the derived data key.
 * @param CompressionSettings - Compression settings for which to compute the derived data key.
 * @param OutKey - The derived data key.
 */
static void GetTextureDerivedDataKey(
	const UTexture& Texture,
	const FTextureBuildSettings& BuildSettings,
	FString& OutKey
	)
{
	FString KeySuffix;
	GetTextureDerivedDataKeySuffix(Texture, BuildSettings, KeySuffix);
	GetTextureDerivedDataKeyFromSuffix(KeySuffix, OutKey);
}

#endif // #if WITH_EDITORONLY_DATA

/*------------------------------------------------------------------------------
	Texture compression.
------------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA

/**
 * Sets texture build settings.
 * @param Texture - The texture for which to build compressor settings.
 * @param OutBuildSettings - Build settings.
 */
static void GetTextureBuildSettings(
	const UTexture& Texture,
	const FTextureLODSettings& TextureLODSettings,
	FTextureBuildSettings& OutBuildSettings
	)
{
	OutBuildSettings.ColorAdjustment.AdjustBrightness = Texture.AdjustBrightness;
	OutBuildSettings.ColorAdjustment.AdjustBrightnessCurve = Texture.AdjustBrightnessCurve;
	OutBuildSettings.ColorAdjustment.AdjustVibrance = Texture.AdjustVibrance;
	OutBuildSettings.ColorAdjustment.AdjustSaturation = Texture.AdjustSaturation;
	OutBuildSettings.ColorAdjustment.AdjustRGBCurve = Texture.AdjustRGBCurve;
	OutBuildSettings.ColorAdjustment.AdjustHue = Texture.AdjustHue;
	OutBuildSettings.ColorAdjustment.AdjustMinAlpha = Texture.AdjustMinAlpha;
	OutBuildSettings.ColorAdjustment.AdjustMaxAlpha = Texture.AdjustMaxAlpha;
	OutBuildSettings.bSRGB = Texture.SRGB;
	OutBuildSettings.bPreserveBorder = Texture.bPreserveBorder;
	OutBuildSettings.bDitherMipMapAlpha = Texture.bDitherMipMapAlpha;
	OutBuildSettings.bComputeBokehAlpha = (Texture.LODGroup == TEXTUREGROUP_Bokeh);
	OutBuildSettings.bReplicateAlpha = false;
	OutBuildSettings.bReplicateRed = false;

	if (Texture.IsA(UTextureCube::StaticClass()))
	{
		OutBuildSettings.bCubemap = true;
		OutBuildSettings.DiffuseConvolveMipLevel = GDiffuseConvolveMipLevel;
	}
	else
	{
		OutBuildSettings.bCubemap = false;
		OutBuildSettings.DiffuseConvolveMipLevel = 0;
	}

	if (Texture.CompressionSettings == TC_Displacementmap)
	{
		OutBuildSettings.bReplicateAlpha = true;
	}
	else if (Texture.CompressionSettings == TC_Grayscale || Texture.CompressionSettings == TC_Alpha)
	{
		OutBuildSettings.bReplicateRed = true;
	}

	bool bDownsampleWithAverage;
	bool bSharpenWithoutColorShift;
	bool bBorderColorBlack;
	TextureMipGenSettings MipGenSettings;
	TextureLODSettings.GetMipGenSettings( 
		Texture,
		MipGenSettings,
		OutBuildSettings.MipSharpening,
		OutBuildSettings.SharpenMipKernelSize,
		bDownsampleWithAverage,
		bSharpenWithoutColorShift,
		bBorderColorBlack
		);
	OutBuildSettings.MipGenSettings = MipGenSettings;
	OutBuildSettings.bDownsampleWithAverage = bDownsampleWithAverage;
	OutBuildSettings.bSharpenWithoutColorShift = bSharpenWithoutColorShift;
	OutBuildSettings.bBorderColorBlack = bBorderColorBlack;
	OutBuildSettings.bFlipGreenChannel = Texture.bFlipGreenChannel;
	OutBuildSettings.CompositeTextureMode = Texture.CompositeTextureMode;
	OutBuildSettings.CompositePower = Texture.CompositePower;
	OutBuildSettings.LODBias = GSystemSettings.TextureLODSettings.CalculateLODBias(Texture.Source.GetSizeX(), Texture.Source.GetSizeY(), Texture.LODGroup, Texture.LODBias, Texture.NumCinematicMipLevels, Texture.MipGenSettings);
	OutBuildSettings.bStreamable = !Texture.NeverStream && (Texture.LODGroup != TEXTUREGROUP_UI) && (Cast<const UTexture2D>(&Texture) != NULL);
}

/**
 * Sets build settings for a texture on the current running platform
 * @param Texture - The texture for which to build compressor settings.
 * @param OutBuildSettings - Array of desired texture settings
 */
static void GetBuildSettingsForRunningPlatform(
	const UTexture& Texture,
	FTextureBuildSettings& OutBuildSettings
	)
{
	// Compress to whatever formats the active target platforms want
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM)
	{
		ITargetPlatform* CurrentPlatform = NULL;
		const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();

		check(Platforms.Num());

		CurrentPlatform = Platforms[0];

		for (int32 Index = 1; Index < Platforms.Num(); Index++)
		{
			if (Platforms[Index]->IsRunningPlatform())
			{
				CurrentPlatform = Platforms[Index];
				break;
			}
		}

		check(CurrentPlatform != NULL);

		TArray<FName> PlatformFormats;
		CurrentPlatform->GetTextureFormats(&Texture, PlatformFormats);

		// Assume there is at least one format and the first one is what we want at runtime.
		check(PlatformFormats.Num());
		GetTextureBuildSettings(Texture, GSystemSettings.TextureLODSettings, OutBuildSettings);
		OutBuildSettings.TextureFormatName = PlatformFormats[0];
	}
}

/**
 * Stores derived data in the DDC.
 * @param DerivedData - The data to store in the DDC.
 * @param DerivedDataKeySuffix - The key suffix at which to store derived data.
 */
static void PutDerivedDataInCache(
	FTexturePlatformData* DerivedData,
	const FString& DerivedDataKeySuffix
	)
{
	TArray<uint8> RawDerivedData;
	FString DerivedDataKey;

	// Build the key with which to cache derived data.
	GetTextureDerivedDataKeyFromSuffix(DerivedDataKeySuffix, DerivedDataKey);

	FString LogString;
	if (UE_LOG_ACTIVE(LogTexture,Verbose))
	{
		LogString = FString::Printf(
			TEXT("Storing texture in DDC:\n  Key: %s\n  Format: %s\n"),
			*DerivedDataKey,
			GPixelFormats[DerivedData->PixelFormat].Name
			);
	}

	// Write out individual mips to the derived data cache.
	const int32 MipCount = DerivedData->Mips.Num();
	const bool bCubemap = (DerivedData->NumSlices == 6);
	const int32 FirstInlineMip = bCubemap ? 0 : FMath::Max(0, MipCount - NUM_INLINE_DERIVED_MIPS);
	for (int32 MipIndex = 0; MipIndex < MipCount; ++MipIndex)
	{
		FString MipDerivedDataKey;
		FTexture2DMipMap& Mip = DerivedData->Mips[MipIndex];
		const bool bInline = (MipIndex >= FirstInlineMip);
		GetTextureDerivedMipKey(MipIndex, Mip, DerivedDataKeySuffix, MipDerivedDataKey);

		if (UE_LOG_ACTIVE(LogTexture,Verbose))
		{
			LogString += FString::Printf(TEXT("  Mip%d %dx%d %d bytes%s %s\n"),
				MipIndex,
				Mip.SizeX,
				Mip.SizeY,
				Mip.BulkData.GetBulkDataSize(),
				bInline ? TEXT(" [inline]") : TEXT(""),
				*MipDerivedDataKey
				);
		}

		if (!bInline)
		{
			Mip.StoreInDerivedDataCache(MipDerivedDataKey);
		}
	}

	// Store derived data.
	FMemoryWriter Ar(RawDerivedData, /*bIsPersistent=*/ true);
	DerivedData->Serialize(Ar, NULL);
	GetDerivedDataCacheRef().Put(*DerivedDataKey, RawDerivedData);
	UE_LOG(LogTexture,Verbose,TEXT("%s  Derived Data: %d bytes"),*LogString,RawDerivedData.Num());
}

#endif // #if WITH_EDITORONLY_DATA

/*------------------------------------------------------------------------------
	Derived data.
------------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA

class FTextureStatusMessageContext : public FStatusMessageContext
{
public:
	explicit FTextureStatusMessageContext(const FText& InMessage)
		: FStatusMessageContext(InMessage)
	{
		UE_LOG(LogTexture,Display,TEXT("%s"),*InMessage.ToString());
	}
};

namespace ETextureCacheFlags
{
	enum Type
	{
		None			= 0x00,
		Async			= 0x01,
		ForceRebuild	= 0x02,
		InlineMips		= 0x08,
		AllowAsyncBuild	= 0x10,
		ForDDCBuild		= 0x20,
	};
};

/**
 * Unpack a DXT 565 color to RGB32.
 */
static uint16 UnpackDXTColor(int32* OutColors, const uint8* Block)
{
	uint16 PackedColor = (Block[1] << 8) | Block[0];
	uint8 Red = (PackedColor >> 11) & 0x1f;
	OutColors[0] = (Red << 3) | ( Red >> 2);
	uint8 Green = (PackedColor >> 5) & 0x3f;
	OutColors[1] = (Green << 2) | ( Green >> 4);
	uint8 Blue = PackedColor & 0x1f;
	OutColors[2] = (Blue << 3) | ( Blue >> 2);
	return PackedColor;
}

/**
 * Computes the squared error between a DXT compression block and the source colors.
 */
static double ComputeDXTColorBlockSquaredError(const uint8* Block, const FColor* Colors, int32 ColorPitch)
{
	int32 ColorTable[4][3];

	uint16 c0 = UnpackDXTColor(ColorTable[0], Block);
	uint16 c1 = UnpackDXTColor(ColorTable[1], Block + 2);
	if (c0 > c1)
	{
		for (int32 ColorIndex = 0; ColorIndex < 3; ++ColorIndex)
		{
			ColorTable[2][ColorIndex] = (2 * ColorTable[0][ColorIndex]) / 3 + (1 * ColorTable[1][ColorIndex]) / 3;
			ColorTable[3][ColorIndex] = (1 * ColorTable[0][ColorIndex]) / 3 + (2 * ColorTable[1][ColorIndex]) / 3;
		}
	}
	else
	{
		for (int32 ColorIndex = 0; ColorIndex < 3; ++ColorIndex)
		{
			ColorTable[2][ColorIndex] = (1 * ColorTable[0][ColorIndex]) / 2 + (1 * ColorTable[1][ColorIndex]) / 2;
			ColorTable[3][ColorIndex] = 0;
		}
	}

	double SquaredError = 0.0;
	for (int32 Y = 0; Y < 4; ++Y)
	{
		uint8 IndexTable[4];
		uint8 RowIndices = Block[4+Y];
		IndexTable[0] = RowIndices & 0x3;
		IndexTable[1] = (RowIndices >> 2) & 0x3;
		IndexTable[2] = (RowIndices >> 4) & 0x3;
		IndexTable[3] = (RowIndices >> 6) & 0x3;

		for (int32 X = 0; X < 4; ++X)
		{
			FColor Color = Colors[Y * ColorPitch + X];
			int32* DXTColor = ColorTable[IndexTable[X]];
			SquaredError += ((int32)Color.R - DXTColor[0]) * ((int32)Color.R - DXTColor[0]);
			SquaredError += ((int32)Color.G - DXTColor[1]) * ((int32)Color.G - DXTColor[1]);
			SquaredError += ((int32)Color.B - DXTColor[2]) * ((int32)Color.B - DXTColor[2]);
		}
	}
	return SquaredError;
}

/**
 * Computes the squared error between the alpha values in the block and the source colors.
 */
static double ComputeDXTAlphaBlockSquaredError(const uint8* Block, const FColor* Colors, int32 ColorPitch)
{
	int32 AlphaTable[8];

	int32 a0 = Block[0];
	int32 a1 = Block[1];

	AlphaTable[0] = a0;
	AlphaTable[1] = a1;
	if (AlphaTable[0] > AlphaTable[1])
	{
		for (int32 AlphaIndex = 0; AlphaIndex < 6; ++AlphaIndex)
		{
			AlphaTable[AlphaIndex+2] = ((6 - AlphaIndex) * a0 + (1 + AlphaIndex) * a1) / 7;
		}
	}
	else
	{
		for (int32 AlphaIndex = 0; AlphaIndex < 4; ++AlphaIndex)
		{
			AlphaTable[AlphaIndex+2] = ((4 - AlphaIndex) * a0 + (1 + AlphaIndex) * a1) / 5;
		}
		AlphaTable[6] = 0;
		AlphaTable[7] = 255;
	}

	uint64 IndexBits = (uint64)Block[7];
	IndexBits = (IndexBits << 8) | (uint64)Block[6];
	IndexBits = (IndexBits << 8) | (uint64)Block[5];
	IndexBits = (IndexBits << 8) | (uint64)Block[4];
	IndexBits = (IndexBits << 8) | (uint64)Block[3];
	IndexBits = (IndexBits << 8) | (uint64)Block[2];

	double SquaredError = 0.0;
	for (int32 Y = 0; Y < 4; ++Y)
	{
		for (int32 X = 0; X < 4; ++X)
		{
			const FColor Color = Colors[Y * ColorPitch + X];
			uint8 Index = IndexBits & 0x7;
			SquaredError += ((int32)Color.A - AlphaTable[Index]) * ((int32)Color.A - AlphaTable[Index]);
			IndexBits = IndexBits >> 3;
		}
	}
	return SquaredError;
}

/**
 * Computes the PSNR value for the compressed image.
 */
static float ComputePSNR(const FImage& SrcImage, const FCompressedImage2D& CompressedImage)
{
	double SquaredError = 0.0;
	int32 NumErrors = 0;
	const uint8* CompressedData = CompressedImage.RawData.GetTypedData();

	if (SrcImage.Format == ERawImageFormat::BGRA8 && (CompressedImage.PixelFormat == PF_DXT1 || CompressedImage.PixelFormat == PF_DXT5))
	{
		int32 NumBlocksX = CompressedImage.SizeX / 4;
		int32 NumBlocksY = CompressedImage.SizeY / 4;
		for (int32 BlockY = 0; BlockY < NumBlocksY; ++BlockY)
		{
			for (int32 BlockX = 0; BlockX < NumBlocksX; ++BlockX)
			{
				if (CompressedImage.PixelFormat == PF_DXT1)
				{
					SquaredError += ComputeDXTColorBlockSquaredError(
						CompressedData + (BlockY * NumBlocksX + BlockX) * 8,
						SrcImage.AsBGRA8() + (BlockY * NumBlocksX * 16 + BlockX * 4),
						SrcImage.SizeX
						);
					NumErrors += 16 * 3;
				}
				else if (CompressedImage.PixelFormat == PF_DXT5)
				{
					SquaredError += ComputeDXTAlphaBlockSquaredError(
						CompressedData + (BlockY * NumBlocksX + BlockX) * 16,
						SrcImage.AsBGRA8() + (BlockY * NumBlocksX * 16 + BlockX * 4),
						SrcImage.SizeX
						);
					SquaredError += ComputeDXTColorBlockSquaredError(
						CompressedData + (BlockY * NumBlocksX + BlockX) * 16 + 8,
						SrcImage.AsBGRA8() + (BlockY * NumBlocksX * 16 + BlockX * 4),
						SrcImage.SizeX
						);
					NumErrors += 16 * 4;
				}
			}
		}
	}

	double MeanSquaredError = NumErrors > 0 ? SquaredError / (double)NumErrors : 0.0;
	double RMSE = FMath::Sqrt(MeanSquaredError);
	return RMSE > 0.0 ? 20.0f * (float)log10(255.0 / RMSE) : 500.0f;
}

/**
 * Worker used to cache texture derived data.
 */
class FTextureCacheDerivedDataWorker : public FNonAbandonableTask
{
	/** Texture compressor module, loaded in the game thread. */
	ITextureCompressorModule* Compressor;
	/** Where to store derived data. */
	FTexturePlatformData* DerivedData;
	/** The texture for which derived data is being cached. */
	UTexture& Texture;
	/** Compression settings. */
	FTextureBuildSettings BuildSettings;
	/** Derived data key suffix. */
	FString KeySuffix;
	/** Source mip images. */
	TArray<FImage> SourceMips;
	/** Source mip images of the composite texture (e.g. normal map for compute roughness). Not necessarily in RGBA32F, usually only top mip as other mips need to be generated first */
	TArray<FImage> CompositeSourceMips;
	/** Texture cache flags. */
	uint32 CacheFlags;
	/** true if caching has succeeded. */
	bool bSucceeded;

	/** Gathers information needed to build a texture. */
	void GetBuildInfo()
	{
		// Dump any existing mips.
		DerivedData->Mips.Empty();
		
		// At this point, the texture *MUST* have a valid GUID.
		if (!Texture.Source.GetId().IsValid())
		{
			UE_LOG(LogTexture,Warning,
				TEXT("Building texture with an invalid GUID: %s"),
				*Texture.GetPathName()
				);
			Texture.Source.ForceGenerateGuid();
		}
		check(Texture.Source.GetId().IsValid());

		// Get the source mips. There must be at least one.
		int32 NumSourceMips = Texture.Source.GetNumMips();
		int32 NumSourceSlices = Texture.Source.GetNumSlices();
		if (NumSourceMips < 1 || NumSourceSlices < 1)
		{
			UE_LOG(LogTexture,Warning,
				TEXT("Texture has no source mips: %s"),
				*Texture.GetPathName()
				);
			return;
		}

		if (BuildSettings.MipGenSettings != TMGS_LeaveExistingMips)
		{
			NumSourceMips = 1;
		}

		if (!BuildSettings.bCubemap)
		{
			NumSourceSlices = 1;
		}

		GetSourceMips(Texture, SourceMips, NumSourceMips, NumSourceSlices);

		if(Texture.CompositeTexture && Texture.CompositeTextureMode != CTM_Disabled)
		{
			const int32 SizeX = Texture.CompositeTexture->Source.GetSizeX();
			const int32 SizeY = Texture.CompositeTexture->Source.GetSizeY();
			const bool bUseCompositeTexture = (FMath::IsPowerOfTwo(SizeX) && FMath::IsPowerOfTwo(SizeY)) ? true : false;

			if(bUseCompositeTexture)
			{
				GetSourceMips(*Texture.CompositeTexture, CompositeSourceMips, Texture.CompositeTexture->Source.GetNumMips(), NumSourceSlices);
			}
			else
			{
				UE_LOG(LogTexture, Warning, 
					TEXT("Composite texture with non-power of two dimensions cannot be used: %s (Assigned on texture: %s)"), 
					*Texture.CompositeTexture->GetPathName(), *Texture.GetPathName());
			}
		}

		GetTextureDerivedDataKeySuffix(Texture, BuildSettings, KeySuffix);
	}

	/** Build the texture. This function is safe to call from any thread. */
	void BuildTexture()
	{
		if (SourceMips.Num())
		{
			TextureDerivedDataTimings::FScopedMeasurement Timer(TextureDerivedDataTimings::BuildTextureTime);

			FFormatNamedArguments Args;
			Args.Add( TEXT("TextureName"), FText::FromString( Texture.GetName() ) );
			Args.Add( TEXT("TextureFormatName"), FText::FromString( BuildSettings.TextureFormatName.GetPlainNameString() ) );
			FTextureStatusMessageContext StatusMessage( FText::Format( NSLOCTEXT("Engine", "BuildTextureStatus", "Building textures: {TextureName} ({TextureFormatName})"), Args ) );

			check(DerivedData->Mips.Num() == 0);
			DerivedData->SizeX = 0;
			DerivedData->SizeY = 0;
			DerivedData->PixelFormat = PF_Unknown;
	
			// Compress the texture.
			TArray<FCompressedImage2D> CompressedMips;
			if (Compressor->BuildTexture(SourceMips, CompositeSourceMips, BuildSettings, CompressedMips))
			{
				check(CompressedMips.Num());

				// Build the derived data.
				const int32 MipCount = CompressedMips.Num();
				for (int32 MipIndex = 0; MipIndex < MipCount; ++MipIndex)
				{
					const FCompressedImage2D& CompressedImage = CompressedMips[MipIndex];
					FTexture2DMipMap* NewMip = new(DerivedData->Mips) FTexture2DMipMap();
					NewMip->SizeX = CompressedImage.SizeX;
					NewMip->SizeY = CompressedImage.SizeY;
					NewMip->BulkData.Lock(LOCK_READ_WRITE);
					check(CompressedImage.RawData.GetTypeSize() == 1);
					void* NewMipData = NewMip->BulkData.Realloc(CompressedImage.RawData.Num());
					FMemory::Memcpy(NewMipData, CompressedImage.RawData.GetTypedData(), CompressedImage.RawData.Num());
					NewMip->BulkData.Unlock();

					if (MipIndex == 0)
					{
						DerivedData->SizeX = CompressedImage.SizeX;
						DerivedData->SizeY = CompressedImage.SizeY;
						DerivedData->PixelFormat = (EPixelFormat)CompressedImage.PixelFormat;
					}
					else
					{
						check(CompressedImage.PixelFormat == DerivedData->PixelFormat);
					}
				}
				DerivedData->NumSlices = BuildSettings.bCubemap ? 6 : 1;

				// Store it in the cache.
				PutDerivedDataInCache(DerivedData, KeySuffix);
			}

			if (DerivedData->Mips.Num())
			{
				bool bInlineMips = (CacheFlags & ETextureCacheFlags::InlineMips) != 0;
				bSucceeded = !bInlineMips || DerivedData->TryInlineMipData();
			}
			else
			{
				UE_LOG(LogTexture, Warning, TEXT("Failed to build %s derived data for %s"),
					*BuildSettings.TextureFormatName.GetPlainNameString(),
					*Texture.GetPathName()
					);
			}
		}
	}

	static void GetSourceMips(UTexture& Texture, TArray<FImage>& SourceMips, uint32 NumSourceMips, uint32 NumSourceSlices)
	{
		ERawImageFormat::Type ImageFormat = ERawImageFormat::BGRA8;

		switch (Texture.Source.GetFormat())
		{
			case TSF_G8:		ImageFormat = ERawImageFormat::G8;		break;
			case TSF_BGRA8:		ImageFormat = ERawImageFormat::BGRA8;	break;
			case TSF_BGRE8:		ImageFormat = ERawImageFormat::BGRE8;	break;
			case TSF_RGBA16:	ImageFormat = ERawImageFormat::RGBA16;	break;
			case TSF_RGBA16F:	ImageFormat = ERawImageFormat::RGBA16F; break;
			default: UE_LOG(LogTexture,Fatal,TEXT("Texture %s has source art in an invalid format."), *Texture.GetName());
		}

		SourceMips.Empty(NumSourceMips);
		for (uint32 MipIndex = 0; MipIndex < NumSourceMips; ++MipIndex)
		{
			FImage* SourceMip = new(SourceMips) FImage(
				(MipIndex == 0) ? Texture.Source.GetSizeX() : FMath::Max(1, SourceMips[MipIndex - 1].SizeX >> 1),
				(MipIndex == 0) ? Texture.Source.GetSizeY() : FMath::Max(1, SourceMips[MipIndex - 1].SizeY >> 1),
				NumSourceSlices,
				ImageFormat,
				Texture.SRGB
				);
			if (Texture.Source.GetMipData(SourceMip->RawData, MipIndex) == false)
			{
				UE_LOG(LogTexture,Warning,
					TEXT("Cannot retrieve source data for mip %d of texture %s"),
					MipIndex,
					*Texture.GetName()
					);
				SourceMips.Empty();
				break;
			}
		}
	}

public:
	/** Initialization constructor. */
	FTextureCacheDerivedDataWorker(
		ITextureCompressorModule* InCompressor,
		FTexturePlatformData* InDerivedData,
		UTexture* InTexture,
		const FTextureBuildSettings& InSettings,
		uint32 InCacheFlags
		)
		: Compressor(InCompressor)
		, DerivedData(InDerivedData)
		, Texture(*InTexture)
		, BuildSettings(InSettings)
		, CacheFlags(InCacheFlags)
		, bSucceeded(false)
	{
		const bool bAllowAsyncBuild = (CacheFlags & ETextureCacheFlags::AllowAsyncBuild) != 0;
		if (bAllowAsyncBuild)
		{
			GetBuildInfo();
		}
	}

	/** Does the work to cache derived data. Safe to call from any thread. */
	void DoWork()
	{
		TArray<uint8> RawDerivedData;
		bool bForceRebuild = (CacheFlags & ETextureCacheFlags::ForceRebuild) != 0;
		bool bInlineMips = (CacheFlags & ETextureCacheFlags::InlineMips) != 0;
		bool bForDDC = (CacheFlags & ETextureCacheFlags::ForDDCBuild) != 0;

		if (!bForceRebuild && GetDerivedDataCacheRef().GetSynchronous(*DerivedData->DerivedDataKey, RawDerivedData))
		{
			FMemoryReader Ar(RawDerivedData, /*bIsPersistent=*/ true);
			DerivedData->Serialize(Ar, NULL);
			bSucceeded = true;
			if (bForDDC)
			{
				bSucceeded = DerivedData->TryLoadMips(0,NULL);
			}
			else if (bInlineMips)
			{
				bSucceeded = DerivedData->TryInlineMipData();
			}
			else
			{
				bSucceeded = DerivedData->AreDerivedMipsAvailable();
			}
		}
		else if (SourceMips.Num())
		{
			BuildTexture();
		}
	}

	/** Finalize work. Must be called ONLY by the game thread! */
	void Finalize()
	{
		check(IsInGameThread());
		if (!bSucceeded && SourceMips.Num() == 0)
		{
			GetBuildInfo();
			BuildTexture();
		}
	}

	/** Interface for FAsyncTask. */
	static const TCHAR* Name()
	{
		return TEXT("FTextureAsyncCacheDerivedDataTask");
	}
};

struct FTextureAsyncCacheDerivedDataTask : public FAsyncTask<FTextureCacheDerivedDataWorker>
{
	FTextureAsyncCacheDerivedDataTask(
		ITextureCompressorModule* InCompressor,
		FTexturePlatformData* InDerivedData,
		UTexture* InTexture,
		const FTextureBuildSettings& InSettings,
		uint32 InCacheFlags
		)
		: FAsyncTask<FTextureCacheDerivedDataWorker>(
			InCompressor,
			InDerivedData,
			InTexture,
			InSettings,
			InCacheFlags
			)
	{
	}
};

void FTexturePlatformData::Cache(
	UTexture& InTexture,
	const FTextureBuildSettings& InSettings,
	uint32 InFlags
	)
{
	// Flush any existing async task and ignore results.
	FinishCache();

	uint32 Flags = InFlags;

	static bool bForDDC = FString(FCommandLine::Get()).Contains(TEXT("DerivedDataCache"));
	if (bForDDC)
	{
		Flags |= ETextureCacheFlags::ForDDCBuild;
	}

	bool bForceRebuild = (Flags & ETextureCacheFlags::ForceRebuild) != 0;
	bool bAsync = !bForDDC && (Flags & ETextureCacheFlags::Async) != 0;
	GetTextureDerivedDataKey(InTexture, InSettings, DerivedDataKey);
	
	ITextureCompressorModule* Compressor = &FModuleManager::LoadModuleChecked<ITextureCompressorModule>(TEXTURE_COMPRESSOR_MODULENAME);

	if (bAsync && !bForceRebuild)
	{
		AsyncTask = new FTextureAsyncCacheDerivedDataTask(Compressor, this, &InTexture, InSettings, Flags);
		AsyncTask->StartBackgroundTask();
	}
	else
	{
		TextureDerivedDataTimings::FScopedMeasurement Timer(TextureDerivedDataTimings::SyncBlockTime);
		FTextureCacheDerivedDataWorker Worker(Compressor, this, &InTexture, InSettings, Flags);
		Worker.DoWork();
		Worker.Finalize();
	}
}

void FTexturePlatformData::FinishCache()
{
	if (AsyncTask)
	{
		{
			TextureDerivedDataTimings::FScopedMeasurement Timer(TextureDerivedDataTimings::AsyncBlockTime);
			AsyncTask->EnsureCompletion();
		}
		FTextureCacheDerivedDataWorker& Worker = AsyncTask->GetTask();
		Worker.Finalize();
		delete AsyncTask;
		AsyncTask = NULL;
	}
}

typedef TArray<uint32, TInlineAllocator<MAX_TEXTURE_MIP_COUNT> > FAsyncMipHandles;

/**
 * Executes async DDC gets for mips stored in the derived data cache.
 * @param Mip - Mips to retrieve.
 * @param FirstMipToLoad - Index of the first mip to retrieve.
 * @param OutHandles - Handles to the asynchronous DDC gets.
 */
static void BeginLoadDerivedMips(TIndirectArray<FTexture2DMipMap>& Mips, int32 FirstMipToLoad, FAsyncMipHandles& OutHandles)
{
	FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();
	OutHandles.AddZeroed(Mips.Num());
	for (int32 MipIndex = FirstMipToLoad; MipIndex < Mips.Num(); ++MipIndex)
	{
		const FTexture2DMipMap& Mip = Mips[MipIndex];
		if (Mip.DerivedDataKey.IsEmpty() == false)
		{
			OutHandles[MipIndex] = DDC.GetAsynchronous(*Mip.DerivedDataKey);
		}
	}
}

/** Asserts that MipSize is correct for the mipmap. */
static void CheckMipSize(FTexture2DMipMap& Mip, EPixelFormat PixelFormat, int32 MipSize)
{
	if (MipSize != CalcTextureMipMapSize(Mip.SizeX, Mip.SizeY, PixelFormat, 0))
	{
		UE_LOG(LogTexture, Warning,
			TEXT("%dx%d mip of %s texture has invalid data in the DDC. Got %d bytes, expected %d. Key=%s"),
			Mip.SizeX,
			Mip.SizeY,
			GPixelFormats[PixelFormat].Name,
			MipSize,
			CalcTextureMipMapSize(Mip.SizeX, Mip.SizeY, PixelFormat, 0),
			*Mip.DerivedDataKey
			);
	}
}

bool FTexturePlatformData::TryInlineMipData()
{
	FAsyncMipHandles AsyncHandles;
	TArray<uint8> TempData;
	FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();

	BeginLoadDerivedMips(Mips, 0, AsyncHandles);
	for (int32 MipIndex = 0; MipIndex < Mips.Num(); ++MipIndex)
	{
		FTexture2DMipMap& Mip = Mips[MipIndex];
		if (Mip.DerivedDataKey.IsEmpty() == false)
		{
			uint32 AsyncHandle = AsyncHandles[MipIndex];
			DDC.WaitAsynchronousCompletion(AsyncHandle);
			if (DDC.GetAsynchronousResults(AsyncHandle, TempData))
			{
				int32 MipSize = 0;
				FMemoryReader Ar(TempData, /*bIsPersistent=*/ true);
				Ar << MipSize;

				Mip.BulkData.Lock(LOCK_READ_WRITE);
				void* MipData = Mip.BulkData.Realloc(MipSize);
				Ar.Serialize(MipData, MipSize);
				Mip.BulkData.Unlock();
				Mip.DerivedDataKey.Empty();
			}
			else
			{
				return false;
			}
			TempData.Reset();
		}
	}
	return true;
}

#endif // #if WITH_EDITORONLY_DATA

FTexturePlatformData::FTexturePlatformData()
	: SizeX(0)
	, SizeY(0)
	, NumSlices(0)
	, PixelFormat(PF_Unknown)
#if WITH_EDITORONLY_DATA
	, AsyncTask(NULL)
#endif // #if WITH_EDITORONLY_DATA
{
}

FTexturePlatformData::~FTexturePlatformData()
{
#if WITH_EDITORONLY_DATA
	if (AsyncTask)
	{
		AsyncTask->EnsureCompletion();
		delete AsyncTask;
		AsyncTask = NULL;
	}
#endif
}

bool FTexturePlatformData::TryLoadMips(int32 FirstMipToLoad, void** OutMipData)
{
	int32 NumMipsCached = 0;

#if WITH_EDITORONLY_DATA
	TArray<uint8> TempData;
	FAsyncMipHandles AsyncHandles;
	FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();
	BeginLoadDerivedMips(Mips, FirstMipToLoad, AsyncHandles);
#endif // #if WITH_EDITORONLY_DATA

	// Load remaining mips (if any) from bulk data.
	for (int32 MipIndex = FirstMipToLoad; MipIndex < Mips.Num(); ++MipIndex)
	{
		FTexture2DMipMap& Mip = Mips[MipIndex];
		if (Mip.BulkData.GetBulkDataSize() > 0)
		{
			if (OutMipData)
			{
				OutMipData[MipIndex - FirstMipToLoad] = FMemory::Malloc(Mip.BulkData.GetBulkDataSize());
				Mip.BulkData.GetCopy(&OutMipData[MipIndex - FirstMipToLoad]);
			}
			NumMipsCached++;
		}
	}

#if WITH_EDITORONLY_DATA
	// Wait for async DDC gets.
	for (int32 MipIndex = FirstMipToLoad; MipIndex < Mips.Num(); ++MipIndex)
	{
		FTexture2DMipMap& Mip = Mips[MipIndex];
		if (Mip.DerivedDataKey.IsEmpty() == false)
		{
			uint32 AsyncHandle = AsyncHandles[MipIndex];
			DDC.WaitAsynchronousCompletion(AsyncHandle);
			if (DDC.GetAsynchronousResults(AsyncHandle, TempData))
			{
				int32 MipSize = 0;
				FMemoryReader Ar(TempData, /*bIsPersistent=*/ true);
				Ar << MipSize;
				CheckMipSize(Mip, PixelFormat, MipSize);
				NumMipsCached++;

				if (OutMipData)
				{
					OutMipData[MipIndex - FirstMipToLoad] = FMemory::Malloc(MipSize);
					Ar.Serialize(OutMipData[MipIndex - FirstMipToLoad], MipSize);
				}
			}
			TempData.Reset();
		}
	}
#endif // #if WITH_EDITORONLY_DATA

	if (NumMipsCached != (Mips.Num() - FirstMipToLoad))
	{
		// Unable to cache all mips. Release memory for those that were cached.
		for (int32 MipIndex = FirstMipToLoad; MipIndex < Mips.Num(); ++MipIndex)
		{
			if (OutMipData && OutMipData[MipIndex - FirstMipToLoad])
			{
				FMemory::Free(OutMipData[MipIndex - FirstMipToLoad]);
				OutMipData[MipIndex - FirstMipToLoad] = NULL;
			}
		}
		return false;
	}

	return true;
}

#if WITH_EDITORONLY_DATA
bool FTexturePlatformData::AreDerivedMipsAvailable() const
{
	bool bMipsAvailable = true;
	FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();
	for (int32 MipIndex = 0; bMipsAvailable && MipIndex < Mips.Num(); ++MipIndex)
	{
		const FTexture2DMipMap& Mip = Mips[MipIndex];
		if (Mip.DerivedDataKey.IsEmpty() == false)
		{
			bMipsAvailable = DDC.CachedDataProbablyExists(*Mip.DerivedDataKey);
		}
	}
	return bMipsAvailable;
}
#endif // #if WITH_EDITORONLY_DATA

static void SerializePlatformData(
	FArchive& Ar,
	FTexturePlatformData* PlatformData, 
	UTexture* Texture,
	bool bCooked
	)
{
	UEnum* PixelFormatEnum = UTexture::GetPixelFormatEnum();

	Ar << PlatformData->SizeX;
	Ar << PlatformData->SizeY;
	Ar << PlatformData->NumSlices;
	if (Ar.IsLoading())
	{
		FString PixelFormatString;
		Ar << PixelFormatString;
		PlatformData->PixelFormat = (EPixelFormat)PixelFormatEnum->FindEnumIndex(*PixelFormatString);
	}
	else if (Ar.IsSaving())
	{
		FString PixelFormatString = PixelFormatEnum->GetEnum(PlatformData->PixelFormat).GetPlainNameString();
		Ar << PixelFormatString;
	}
	
	int32 NumMips = PlatformData->Mips.Num();
	int32 FirstMipToSerialize = 0;

	if (bCooked)
	{
#if WITH_EDITORONLY_DATA
		if (Ar.IsSaving())
		{
			check(Ar.CookingTarget());
			check(Texture);

			const int32 Width = PlatformData->SizeX;
			const int32 Height = PlatformData->SizeY;
			const int32 LODGroup = Texture->LODGroup;
			const int32 LODBias = Texture->LODBias;
			const int32 NumCinematicMipLevels = Texture->NumCinematicMipLevels;
			const TextureMipGenSettings MipGenSetting = Texture->MipGenSettings;

			FirstMipToSerialize = Ar.CookingTarget()->GetTextureLODSettings().CalculateLODBias(Width, Height, LODGroup, LODBias, NumCinematicMipLevels, MipGenSetting);
			FirstMipToSerialize = FMath::Clamp(FirstMipToSerialize, 0, FMath::Max(NumMips-1,0));
			NumMips -= FirstMipToSerialize;
		}
#endif // #if WITH_EDITORONLY_DATA
		Ar << FirstMipToSerialize;
		if (Ar.IsLoading())
		{
			check(Texture);
			Texture->LODBias -= FirstMipToSerialize;
			FirstMipToSerialize = 0;
		}
	}

	Ar << NumMips;
	if (Ar.IsLoading())
	{
		check(FirstMipToSerialize == 0);
		PlatformData->Mips.Empty(NumMips);
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			new(PlatformData->Mips) FTexture2DMipMap();
		}
	}
	for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
	{
		PlatformData->Mips[FirstMipToSerialize + MipIndex].Serialize(Ar, Texture, MipIndex);
	}
}

void FTexturePlatformData::Serialize(FArchive& Ar, UTexture* Owner)
{
	SerializePlatformData(Ar, this, Owner, false);
}

void FTexturePlatformData::SerializeCooked(FArchive& Ar, UTexture* Owner)
{
	SerializePlatformData(Ar, this, Owner, true);
	if (Ar.IsLoading() && Mips.Num() > 0)
	{
		SizeX = Mips[0].SizeX;
		SizeY = Mips[0].SizeY;
	}
}

/*------------------------------------------------------------------------------
	Streaming mips from the derived data cache.
------------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA

/** Initialization constructor. */
FAsyncStreamDerivedMipWorker::FAsyncStreamDerivedMipWorker(
	const FString& InDerivedDataKey,
	void* InDestMipData,
	int32 InMipSize,
	FThreadSafeCounter* InThreadSafeCounter
	)
	: DerivedDataKey(InDerivedDataKey)
	, DestMipData(InDestMipData)
	, ExpectedMipSize(InMipSize)
	, bRequestFailed(false)
	, ThreadSafeCounter(InThreadSafeCounter)
{
}

/** Retrieves the derived mip from the derived data cache. */
void FAsyncStreamDerivedMipWorker::DoWork()
{
	TArray<uint8> DerivedMipData;

	if (GetDerivedDataCacheRef().GetSynchronous(*DerivedDataKey, DerivedMipData))
	{
		FMemoryReader Ar(DerivedMipData, true);
		int32 MipSize = 0;
		Ar << MipSize;
		checkf(ExpectedMipSize == ExpectedMipSize, TEXT("MipSize(%d) == ExpectedSize(%d)"), MipSize, ExpectedMipSize);
		Ar.Serialize(DestMipData, MipSize);
	}
	else
	{
		bRequestFailed = true;
	}
	FPlatformMisc::MemoryBarrier();
	ThreadSafeCounter->Decrement();
}

#endif // #if WITH_EDITORONLY_DATA

/*------------------------------------------------------------------------------
	Texture derived data interface.
------------------------------------------------------------------------------*/

void UTexture2D::GetMipData(int32 FirstMipToLoad, void** OutMipData)
{
#if WITH_EDITORONLY_DATA
	TextureDerivedDataTimings::FScopedMeasurement Timer(TextureDerivedDataTimings::GetMipDataTime);
#endif // #if WITH_EDITORONLY_DATA
	if (PlatformData->TryLoadMips(FirstMipToLoad, OutMipData) == false)
	{
		// Unable to load mips from the cache. Rebuild the texture and try again.
		UE_LOG(LogTexture,Warning,TEXT("GetMipData failed for %s (%s)"),
			*GetPathName(), GPixelFormats[GetPixelFormat()].Name);
#if WITH_EDITORONLY_DATA
		ForceRebuildPlatformData();
		if (PlatformData->TryLoadMips(FirstMipToLoad, OutMipData) == false)
		{
			UE_LOG(LogTexture,Error,TEXT("Failed to build texture %s."), *GetPathName());
		}
#endif // #if WITH_EDITORONLY_DATA
	}
}



void UTexture::CleanupCachedCookedPlatformData()
{
	TMap<FString, FTexturePlatformData*> *CookedPlatformDataPtr = GetCookedPlatformData();

	if ( CookedPlatformDataPtr )
	{
		TMap<FString, FTexturePlatformData*> &CookedPlatformData = *CookedPlatformDataPtr;

		for ( auto It : CookedPlatformData )
		{
			delete It.Value;
		}

		CookedPlatformData.Empty();
	}
}

#if WITH_EDITORONLY_DATA
void UTexture::CachePlatformData(bool bAsyncCache)
{
	FTexturePlatformData** PlatformDataLinkPtr = GetRunningPlatformData();
	if (PlatformDataLinkPtr)
	{
		FTexturePlatformData*& PlatformDataLink = *PlatformDataLinkPtr;
		if (Source.IsValid() && FApp::CanEverRender())
		{
			FString DerivedDataKey;
			FTextureBuildSettings BuildSettings;
			GetBuildSettingsForRunningPlatform(*this, BuildSettings);
			GetTextureDerivedDataKey(*this, BuildSettings, DerivedDataKey);

			if (PlatformDataLink == NULL || PlatformDataLink->DerivedDataKey != DerivedDataKey)
			{
				// Release our resource if there is existing derived data.
				if (PlatformDataLink)
				{
					ReleaseResource();
				}
				else
				{
					PlatformDataLink = new FTexturePlatformData();
				}
				PlatformDataLink->Cache(*this, BuildSettings, bAsyncCache ? ETextureCacheFlags::Async : ETextureCacheFlags::None);
			}
		}
		else if (PlatformDataLink == NULL)
		{
			// If there is no source art available, create an empty platform data container.
			PlatformDataLink = new FTexturePlatformData();
		}

		
		UpdateCachedLODBias();
	}
}

void UTexture::UpdateCachedLODBias( bool bIncTextureMips )
{
	CachedCombinedLODBias = GSystemSettings.TextureLODSettings.CalculateLODBias(this, bIncTextureMips);
}

void UTexture::BeginCachePlatformData()
{
	CachePlatformData(true);

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

void UTexture::BeginCacheForCookedPlatformData( const ITargetPlatform *TargetPlatform )
{
	TMap<FString, FTexturePlatformData*>* CookedPlatformDataPtr = GetCookedPlatformData();
	if (CookedPlatformDataPtr)
	{
		TMap<FString,FTexturePlatformData*>& CookedPlatformData = *CookedPlatformDataPtr;
		
		// Make sure the pixel format enum has been cached.
		UTexture::GetPixelFormatEnum();

		// Retrieve formats to cache for targetplatform.
		
		TArray<FName> PlatformFormats;
		

		TArray<FTextureBuildSettings> BuildSettingsToCache;

		FTextureBuildSettings BuildSettings;
		GetTextureBuildSettings(*this, TargetPlatform->GetTextureLODSettings(), BuildSettings);
		TargetPlatform->GetTextureFormats(this, PlatformFormats);
		for (int32 FormatIndex = 0; FormatIndex < PlatformFormats.Num(); ++FormatIndex)
		{
			BuildSettings.TextureFormatName = PlatformFormats[FormatIndex];
			if (BuildSettings.TextureFormatName != NAME_None)
			{
				BuildSettingsToCache.Add(BuildSettings);
			}
		}



		uint32 CacheFlags = ETextureCacheFlags::Async | ETextureCacheFlags::InlineMips;

		// If source data is resident in memory then allow the texture to be built
		// in a background thread.
		if (Source.BulkData.IsBulkDataLoaded())
		{
			CacheFlags |= ETextureCacheFlags::AllowAsyncBuild;
		}


		// Cull redundant settings by comparing derived data keys.
		for (int32 SettingsIndex = 0; SettingsIndex < BuildSettingsToCache.Num(); ++SettingsIndex)
		{
			FString DerivedDataKey;
			GetTextureDerivedDataKeySuffix(*this, BuildSettingsToCache[SettingsIndex], DerivedDataKey);

			FTexturePlatformData *PlatformData = CookedPlatformData.FindRef( DerivedDataKey );

			if ( PlatformData == NULL )
			{
				FTexturePlatformData* PlatformDataToCache;
				PlatformDataToCache = new FTexturePlatformData();
				PlatformDataToCache->Cache(
					*this,
					BuildSettingsToCache[SettingsIndex],
					CacheFlags
					);
				CookedPlatformData.Add( DerivedDataKey, PlatformDataToCache );
			}
		}
	}
}

bool UTexture::IsAsyncCacheComplete()
{
	bool bComplete = true;
	FTexturePlatformData** RunningPlatformDataPtr = GetRunningPlatformData();
	if (RunningPlatformDataPtr)
	{
		FTexturePlatformData* RunningPlatformData = *RunningPlatformDataPtr;
		if (RunningPlatformData )
		{
			bComplete &= (RunningPlatformData->AsyncTask == NULL) || RunningPlatformData->AsyncTask->IsWorkDone();
		}
	}

	

	TMap<FString,FTexturePlatformData*>* CookedPlatformDataPtr = GetCookedPlatformData();
	if (CookedPlatformDataPtr)
	{
		for ( auto It : *CookedPlatformDataPtr )
		{
			FTexturePlatformData* PlatformData = It.Value;
			if (PlatformData)
			{
				bComplete &= (PlatformData->AsyncTask == NULL) || PlatformData->AsyncTask->IsWorkDone();
			}
		}
	}

	return bComplete;
}
	
void UTexture::FinishCachePlatformData()
{
	FTexturePlatformData** RunningPlatformDataPtr = GetRunningPlatformData();
	if (RunningPlatformDataPtr)
	{
		FTexturePlatformData*& RunningPlatformData = *RunningPlatformDataPtr;
		
		if ( FApp::CanEverRender() )
		{
			if ( RunningPlatformData == NULL )
			{
				// begin cache never called
				CachePlatformData();
			}
			else
			{
				// make sure async requests are finished
				RunningPlatformData->FinishCache();
			}

#if DO_CHECK
			FString DerivedDataKey;
			FTextureBuildSettings BuildSettings;
			GetBuildSettingsForRunningPlatform(*this, BuildSettings);
			GetTextureDerivedDataKey(*this, BuildSettings, DerivedDataKey);

			check( RunningPlatformData->DerivedDataKey == DerivedDataKey );
#endif
		}
	}

	UpdateCachedLODBias();
}

void UTexture::ForceRebuildPlatformData()
{
	FTexturePlatformData** PlatformDataLinkPtr = GetRunningPlatformData();
	if (PlatformDataLinkPtr && *PlatformDataLinkPtr && FApp::CanEverRender())
	{
		FTexturePlatformData *&PlatformDataLink = *PlatformDataLinkPtr;
		FlushRenderingCommands();
		FTextureBuildSettings BuildSettings;
		GetBuildSettingsForRunningPlatform(*this, BuildSettings);
		PlatformDataLink->Cache(
			*this,
			BuildSettings,
			ETextureCacheFlags::ForceRebuild
			);
	}
}



void UTexture::MarkPlatformDataTransient()
{
	FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();

	FTexturePlatformData** RunningPlatformData = GetRunningPlatformData();
	if (RunningPlatformData)
	{
		FTexturePlatformData* PlatformData = *RunningPlatformData;
		if (PlatformData)
		{
			for (int32 MipIndex = 0; MipIndex < PlatformData->Mips.Num(); ++MipIndex)
			{
				FTexture2DMipMap& Mip = PlatformData->Mips[MipIndex];
				if (Mip.DerivedDataKey.Len() > 0)
				{
					DDC.MarkTransient(*Mip.DerivedDataKey);
				}
			}
			DDC.MarkTransient(*PlatformData->DerivedDataKey);
		}
	}

	TMap<FString, FTexturePlatformData*> *CookedPlatformData = GetCookedPlatformData();
	if ( CookedPlatformData )
	{
		FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();
		for ( auto It : *CookedPlatformData )
		{
			FTexturePlatformData* PlatformData = It.Value;
			for (int32 MipIndex = 0; MipIndex < PlatformData->Mips.Num(); ++MipIndex)
			{
				FTexture2DMipMap& Mip = PlatformData->Mips[MipIndex];
				if (Mip.DerivedDataKey.Len() > 0)
				{
					DDC.MarkTransient(*Mip.DerivedDataKey);
				}
			}
			DDC.MarkTransient(*PlatformData->DerivedDataKey);
		}
	}
}
#endif // #if WITH_EDITORONLY_DATA

void UTexture::CleanupCachedRunningPlatformData()
{
	FTexturePlatformData **RunningPlatformDataPtr = GetRunningPlatformData();

	if ( RunningPlatformDataPtr )
	{
		FTexturePlatformData *&RunningPlatformData = *RunningPlatformDataPtr;

		if ( RunningPlatformData != NULL )
		{
			delete RunningPlatformData;
			RunningPlatformData = NULL;
		}
	}
}


void UTexture::SerializeCookedPlatformData(FArchive& Ar)
{
	if (IsTemplate() )
	{
		return;
	}

	UEnum* PixelFormatEnum = UTexture::GetPixelFormatEnum();


#if WITH_EDITORONLY_DATA
	TextureDerivedDataTimings::FScopedMeasurement Timer(TextureDerivedDataTimings::SerializeCookedTime);
	if (Ar.IsCooking() && Ar.IsPersistent())
	{
		if (!Ar.CookingTarget()->IsServerOnly())
		{
			TMap<FString, FTexturePlatformData*> *CookedPlatformDataPtr = GetCookedPlatformData();
			if ( CookedPlatformDataPtr == NULL )
				return;



			FTextureBuildSettings BuildSettings;
			TArray<FName> PlatformFormats;
			TArray<FTexturePlatformData*> PlatformDataToSerialize;

			GetTextureBuildSettings(*this, Ar.CookingTarget()->GetTextureLODSettings(), BuildSettings);
			Ar.CookingTarget()->GetTextureFormats(this, PlatformFormats);
			for (int32 FormatIndex = 0; FormatIndex < PlatformFormats.Num(); FormatIndex++)
			{
				FString DerivedDataKey;
				BuildSettings.TextureFormatName = PlatformFormats[FormatIndex];
				GetTextureDerivedDataKey(*this, BuildSettings, DerivedDataKey);

				FTexturePlatformData *PlatformDataPtr = (*CookedPlatformDataPtr).FindRef(DerivedDataKey);
				
				if (PlatformDataPtr == NULL)
				{
					PlatformDataPtr = new FTexturePlatformData();
					PlatformDataPtr->Cache(*this, BuildSettings, ETextureCacheFlags::InlineMips | ETextureCacheFlags::Async);
					
					CookedPlatformDataPtr->Add( DerivedDataKey, PlatformDataPtr );
					
				}
				PlatformDataToSerialize.Add(PlatformDataPtr);
			}

			for (int32 i = 0; i < PlatformDataToSerialize.Num(); ++i)
			{
				

				FTexturePlatformData* PlatformDataToSave = PlatformDataToSerialize[i];
				PlatformDataToSave->FinishCache();
				FName PixelFormatName = PixelFormatEnum->GetEnum(PlatformDataToSave->PixelFormat);
				Ar << PixelFormatName;
				int32 SkipOffsetLoc = Ar.Tell();
				int32 SkipOffset = 0;
				Ar << SkipOffset;
				PlatformDataToSave->SerializeCooked(Ar, this);
				SkipOffset = Ar.Tell();
				Ar.Seek(SkipOffsetLoc);
				Ar << SkipOffset;
				Ar.Seek(SkipOffset);
			}
		}
		FName PixelFormatName = NAME_None;
		Ar << PixelFormatName;
	}
	else
#endif // #if WITH_EDITORONLY_DATA
	{

		FTexturePlatformData** RunningPlatformDataPtr = GetRunningPlatformData();
		if ( RunningPlatformDataPtr == NULL )
			return;

		FTexturePlatformData*& RunningPlatformData = *RunningPlatformDataPtr;

		FName PixelFormatName = NAME_None;

		CleanupCachedRunningPlatformData();
		check( RunningPlatformData == NULL );

		RunningPlatformData = new FTexturePlatformData();
		Ar << PixelFormatName;
		while (PixelFormatName != NAME_None)
		{
			EPixelFormat PixelFormat = (EPixelFormat)PixelFormatEnum->FindEnumIndex(PixelFormatName);
			int32 SkipOffset = 0;
			Ar << SkipOffset;
			bool bFormatSupported = GPixelFormats[PixelFormat].Supported;
			if (RunningPlatformData->PixelFormat == PF_Unknown && bFormatSupported)
			{
				RunningPlatformData->SerializeCooked(Ar, this);
			}
			else
			{
				Ar.Seek(SkipOffset);
			}
			Ar << PixelFormatName;
		}
	}
}

int32 UTexture2D::GMinTextureResidentMipCount = NUM_INLINE_DERIVED_MIPS;

void UTexture2D::SetMinTextureResidentMipCount(int32 InMinTextureResidentMipCount)
{
	int32 MinAllowedMipCount = FPlatformProperties::RequiresCookedData() ? 1 : NUM_INLINE_DERIVED_MIPS;
	GMinTextureResidentMipCount = FMath::Max(InMinTextureResidentMipCount, MinAllowedMipCount);
}
