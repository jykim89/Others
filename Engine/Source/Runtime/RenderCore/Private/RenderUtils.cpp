// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "RenderCore.h"
#include "RenderUtils.h"
#include "RHI.h"
#include "RenderResource.h"

const uint16 GCubeIndices[12*3] =
{
	0, 2, 3,
	0, 3, 1,
	4, 5, 7,
	4, 7, 6,
	0, 1, 5,
	0, 5, 4,
	2, 6, 7,
	2, 7, 3,
	0, 4, 6,
	0, 6, 2,
	1, 3, 7,
	1, 7, 5,
};

/** X=127.5, Y=127.5, Z=1/127.5f, W=-1.0 */
const VectorRegister GVectorPackingConstants = MakeVectorRegister( 127.5f, 127.5f, 1.0f/127.5f, -1.0f );

/** Zero Normal **/
FPackedNormal FPackedNormal::ZeroNormal(127, 127, 127, 127);

//
// FPackedNormal serializer
//
FArchive& operator<<(FArchive& Ar,FPackedNormal& N)
{
	Ar << N.Vector.Packed;
	return Ar;
}

//
//	Pixel format information.
//

// NOTE: If you add a new basic texture format (ie a format that could be cooked - currently PF_A32B32G32R32F through
// PF_UYVY) you MUST also update XeTools.cpp and PS3Tools.cpp to match up!
FPixelFormatInfo	GPixelFormats[PF_MAX] =
{
	// Name						BlockSizeX	BlockSizeY	BlockSizeZ	BlockBytes	NumComponents	PlatformFormat	Supported		UnrealFormat

	{ TEXT("unknown"),			0,			0,			0,			0,			0,				0,				0,				PF_Unknown			},
	{ TEXT("A32B32G32R32F"),	1,			1,			1,			16,			4,				0,				1,				PF_A32B32G32R32F	},
	{ TEXT("B8G8R8A8"),			1,			1,			1,			4,			4,				0,				1,				PF_B8G8R8A8			},
	{ TEXT("G8"),				1,			1,			1,			1,			1,				0,				1,				PF_G8				},
	{ TEXT("G16"),				1,			1,			1,			2,			1,				0,				1,				PF_G16				},
	{ TEXT("DXT1"),				4,			4,			1,			8,			3,				0,				1,				PF_DXT1				},
	{ TEXT("DXT3"),				4,			4,			1,			16,			4,				0,				1,				PF_DXT3				},
	{ TEXT("DXT5"),				4,			4,			1,			16,			4,				0,				1,				PF_DXT5				},
	{ TEXT("UYVY"),				2,			1,			1,			4,			4,				0,				0,				PF_UYVY				},
	{ TEXT("FloatRGB"),			1,			1,			1,			0,			3,				0,				0,				PF_FloatRGB			},
	{ TEXT("FloatRGBA"),		1,			1,			1,			8,			4,				0,				1,				PF_FloatRGBA		},
	{ TEXT("DepthStencil"),		1,			1,			1,			0,			1,				0,				0,				PF_DepthStencil		},
	{ TEXT("ShadowDepth"),		1,			1,			1,			4,			1,				0,				0,				PF_ShadowDepth		},
	{ TEXT("R32_FLOAT"),		1,			1,			1,			4,			1,				0,				1,				PF_R32_FLOAT		},
	{ TEXT("G16R16"),			1,			1,			1,			4,			2,				0,				1,				PF_G16R16			},
	{ TEXT("G16R16F"),			1,			1,			1,			4,			2,				0,				1,				PF_G16R16F			},
	{ TEXT("G16R16F_FILTER"),	1,			1,			1,			4,			2,				0,				1,				PF_G16R16F_FILTER	},
	{ TEXT("G32R32F"),			1,			1,			1,			8,			2,				0,				1,				PF_G32R32F			},
	{ TEXT("A2B10G10R10"),      1,          1,          1,          4,          4,              0,              1,				PF_A2B10G10R10		},
	{ TEXT("A16B16G16R16"),		1,			1,			1,			8,			4,				0,				1,				PF_A16B16G16R16		},
	{ TEXT("D24"),				1,			1,			1,			4,			1,				0,				1,				PF_D24				},
	{ TEXT("PF_R16F"),			1,			1,			1,			2,			1,				0,				1,				PF_R16F				},
	{ TEXT("PF_R16F_FILTER"),	1,			1,			1,			2,			1,				0,				1,				PF_R16F_FILTER		},
	{ TEXT("BC5"),				4,			4,			1,			16,			2,				0,				1,				PF_BC5				},
	{ TEXT("V8U8"),				1,			1,			1,			2,			2,				0,				1,				PF_V8U8				},
	{ TEXT("A1"),				1,			1,			1,			1,			1,				0,				0,				PF_A1				},
	{ TEXT("FloatR11G11B10"),	1,			1,			1,			0,			3,				0,				0,				PF_FloatR11G11B10	},
	{ TEXT("A8"),				1,			1,			1,			1,			1,				0,				1,				PF_A8				},	
	{ TEXT("R32_UINT"),			1,			1,			1,			4,			1,				0,				1,				PF_R32_UINT			},
	{ TEXT("R32_SINT"),			1,			1,			1,			4,			1,				0,				1,				PF_R32_SINT			},

	// IOS Support
	{ TEXT("PVRTC2"),			8,			4,			1,			8,			4,				0,				0,				PF_PVRTC2			},
	{ TEXT("PVRTC4"),			4,			4,			1,			8,			4,				0,				0,				PF_PVRTC4			},

	{ TEXT("R16_UINT"),			1,			1,			1,			2,			1,				0,				1,				PF_R16_UINT			},
	{ TEXT("R16_SINT"),			1,			1,			1,			2,			1,				0,				1,				PF_R16_SINT			},
	{ TEXT("R16G16B16A16_UINT"),1,			1,			1,			8,			4,				0,				1,				PF_R16G16B16A16_UINT},
	{ TEXT("R16G16B16A16_SINT"),1,			1,			1,			8,			4,				0,				1,				PF_R16G16B16A16_SINT},
	{ TEXT("R5G6B5_UNORM"),     1,          1,          1,          2,          3,              0,              1,              PF_R5G6B5_UNORM		},
	{ TEXT("R8G8B8A8"),			1,			1,			1,			4,			4,				0,				1,				PF_R8G8B8A8			},
	{ TEXT("A8R8G8B8"),			1,			1,			1,			4,			4,				0,				1,				PF_A8R8G8B8			},
	{ TEXT("BC4"),				4,			4,			1,			8,			1,				0,				1,				PF_BC4				},
	{ TEXT("R8G8"),				1,			1,			1,			2,			2,				0,				1,				PF_R8G8				},

	{ TEXT("ATC_RGB"),			4,			4,			1,			8,			3,				0,				0,				PF_ATC_RGB			},
	{ TEXT("ATC_RGBA_E"),		4,			4,			1,			16,			4,				0,				0,				PF_ATC_RGBA_E		},
	{ TEXT("ATC_RGBA_I"),		4,			4,			1,			16,			4,				0,				0,				PF_ATC_RGBA_I		},
	{ TEXT("X24_G8"),			1,			1,			1,			1,			1,				0,				0,				PF_X24_G8			},
	{ TEXT("ETC1"),				4,			4,			1,			8,			3,				0,				0,				PF_ETC1				},
	{ TEXT("ETC2_RGB"),			4,			4,			1,			8,			3,				0,				0,				PF_ETC2_RGB			},
	{ TEXT("ETC2_RGBA"),		4,			4,			1,			16,			4,				0,				0,				PF_ETC2_RGBA		},
};

static struct FValidatePixelFormats
{
	FValidatePixelFormats()
	{
		for (int32 X = 0; X < ARRAY_COUNT(GPixelFormats); ++X)
		{
			// Make sure GPixelFormats has an entry for every unreal format
			check(X == GPixelFormats[X].UnrealFormat);
		}
	}
} ValidatePixelFormats;

//
//	CalculateImageBytes
//

SIZE_T CalculateImageBytes(uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 Format)
{
	if ( Format == PF_A1 )
	{
		// The number of bytes needed to store all 1 bit pixels in a line is the width of the image divided by the number of bits in a byte
		uint32 BytesPerLine = SizeX / 8;
		// The number of actual bytes in a 1 bit image is the bytes per line of pixels times the number of lines
		return sizeof(uint8) * BytesPerLine * SizeY;
	}
	else if( SizeZ > 0 )
	{
		return (SizeX / GPixelFormats[Format].BlockSizeX) * (SizeY / GPixelFormats[Format].BlockSizeY) * (SizeZ / GPixelFormats[Format].BlockSizeZ) * GPixelFormats[Format].BlockBytes;
	}
	else
	{	
		return (SizeX / GPixelFormats[Format].BlockSizeX) * (SizeY / GPixelFormats[Format].BlockSizeY) * GPixelFormats[Format].BlockBytes;
	}
}

//
// FWhiteTexture implementation
//

/**
 * A solid-colored 1x1 texture.
 */
template <int32 R, int32 G, int32 B, int32 A>
class FColoredTexture : public FTexture
{
public:
	// FResource interface.
	virtual void InitRHI()
	{
		// Create the texture RHI.  		
		FTexture2DRHIRef Texture2D = RHICreateTexture2D(1,1,PF_B8G8R8A8,1,1,TexCreate_ShaderResource,NULL);
		TextureRHI = Texture2D;

		// Write the contents of the texture.
		uint32 DestStride;
		FColor* DestBuffer = (FColor*)RHILockTexture2D(Texture2D,0,RLM_WriteOnly,DestStride,false);
		*DestBuffer = FColor(R, G, B, A);
		RHIUnlockTexture2D(Texture2D,0,false);

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point,AM_Wrap,AM_Wrap,AM_Wrap);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
	}

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const
	{
		return 1;
	}

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const
	{
		return 1;
	}
};

FTexture* GWhiteTexture = new TGlobalResource<FColoredTexture<255,255,255,255> >;
FTexture* GBlackTexture = new TGlobalResource<FColoredTexture<0,0,0,255> >;

/**
 * Bulk data interface for providing a single black color used to initialize a
 * volume texture.
 */
class FBlackVolumeTextureResourceBulkDataInterface : public FResourceBulkDataInterface
{
public:

	/** Default constructor. */
	FBlackVolumeTextureResourceBulkDataInterface()
		: Color(0)
	{
	}

	/**
	 * Returns a pointer to the bulk data.
	 */
	virtual const void* GetResourceBulkData() const OVERRIDE
	{
		return &Color;
	}

	/** 
	 * @return size of resource memory
	 */
	virtual uint32 GetResourceBulkDataSize() const OVERRIDE
	{
		return sizeof(Color);
	}

	/**
	 * Free memory after it has been used to initialize RHI resource 
	 */
	virtual void Discard() OVERRIDE
	{
	}

private:

	/** Storage for the color. */
	FColor Color;
};

/**
 * A class representing a 1x1x1 black volume texture.
 */
class FBlackVolumeTexture : public FTexture
{
public:
	
	/**
	 * Initialize RHI resources.
	 */
	virtual void InitRHI()
	{
		if (IsFeatureLevelSupported(GRHIShaderPlatform, ERHIFeatureLevel::SM4))
		{
			// Create the texture.
			FBlackVolumeTextureResourceBulkDataInterface BlackTextureBulkData;
			FTexture3DRHIRef Texture3D = RHICreateTexture3D(1,1,1,PF_B8G8R8A8,1,TexCreate_ShaderResource,&BlackTextureBulkData);
			TextureRHI = Texture3D;	

			// Create the sampler state.
			FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point,AM_Wrap,AM_Wrap,AM_Wrap);
			SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
		}
	}

	/**
	 * Return the size of the texture in the X dimension.
	 */
	virtual uint32 GetSizeX() const
	{
		return 1;
	}

	/**
	 * Return the size of the texture in the Y dimension.
	 */
	virtual uint32 GetSizeY() const
	{
		return 1;
	}
};

/** Global black volume texture resource. */
FTexture* GBlackVolumeTexture = new TGlobalResource<FBlackVolumeTexture>();

class FBlackArrayTexture : public FTexture
{
public:
	// FResource interface.
	virtual void InitRHI()
	{
		if (IsFeatureLevelSupported(GRHIShaderPlatform, ERHIFeatureLevel::SM4))
		{
			// Create the texture RHI.  		
			FTexture2DArrayRHIRef TextureArray = RHICreateTexture2DArray(1,1,1,PF_B8G8R8A8,1,TexCreate_ShaderResource,NULL);
			TextureRHI = TextureArray;

			uint32 DestStride;
			FColor* DestBuffer = (FColor*)RHILockTexture2DArray(TextureArray, 0, 0, RLM_WriteOnly, DestStride, false);
			*DestBuffer = FColor(0, 0, 0, 0);
			RHIUnlockTexture2DArray(TextureArray, 0, 0, false);

			// Create the sampler state RHI resource.
			FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point,AM_Wrap,AM_Wrap,AM_Wrap);
			SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
		}
	}

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const
	{
		return 1;
	}

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const
	{
		return 1;
	}
};

FTexture* GBlackArrayTexture = new TGlobalResource<FBlackArrayTexture>;

//
// FMipColorTexture implementation
//

/**
 * A texture that has a different solid color in each mip-level
 */
class FMipColorTexture : public FTexture
{
public:
	enum
	{
		NumMips = 12
	};
	static const FColor MipColors[NumMips];

	// FResource interface.
	virtual void InitRHI()
	{
		// Create the texture RHI.
		int32 TextureSize = 1 << (NumMips - 1);
		FTexture2DRHIRef Texture2D = RHICreateTexture2D(TextureSize,TextureSize,PF_B8G8R8A8,NumMips,1,TexCreate_ShaderResource,NULL);
		TextureRHI = Texture2D;

		// Write the contents of the texture.
		uint32 DestStride;
		int32 Size = TextureSize;
		for ( int32 MipIndex=0; MipIndex < NumMips; ++MipIndex )
		{
			FColor* DestBuffer = (FColor*) RHILockTexture2D(Texture2D,MipIndex,RLM_WriteOnly,DestStride,false);
			for ( int32 Y=0; Y < Size; ++Y )
			{
				for ( int32 X=0; X < Size; ++X )
				{
					DestBuffer[X] = MipColors[NumMips - 1 - MipIndex];
				}
				DestBuffer += DestStride / sizeof(FColor);
			}
			RHIUnlockTexture2D(Texture2D,MipIndex,false);
			Size >>= 1;
		}

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point,AM_Wrap,AM_Wrap,AM_Wrap);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
	}

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const
	{
		int32 TextureSize = 1 << (NumMips - 1);
		return TextureSize;
	}

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const
	{
		int32 TextureSize = 1 << (NumMips - 1);
		return TextureSize;
	}
};

const FColor FMipColorTexture::MipColors[NumMips] =
{
	FColor(  80,  80,  80, 0 ),		// Mip  0: 1x1			(dark grey)
	FColor( 200, 200, 200, 0 ),		// Mip  1: 2x2			(light grey)
	FColor( 200, 200,   0, 0 ),		// Mip  2: 4x4			(medium yellow)
	FColor( 255, 255,   0, 0 ),		// Mip  3: 8x8			(yellow)
	FColor( 160, 255,  40, 0 ),		// Mip  4: 16x16		(light green)
	FColor(   0, 255,   0, 0 ),		// Mip  5: 32x32		(green)
	FColor(   0, 255, 200, 0 ),		// Mip  6: 64x64		(cyan)
	FColor(   0, 170, 170, 0 ),		// Mip  7: 128x128		(light blue)
	FColor(  60,  60, 255, 0 ),		// Mip  8: 256x256		(dark blue)
	FColor( 255,   0, 255, 0 ),		// Mip  9: 512x512		(pink)
	FColor( 255,   0,   0, 0 ),		// Mip 10: 1024x1024	(red)
	FColor( 255, 130,   0, 0 ),		// Mip 11: 2048x2048	(orange)
};

RENDERCORE_API FTexture* GMipColorTexture = new FMipColorTexture;
RENDERCORE_API int32 GMipColorTextureMipLevels = FMipColorTexture::NumMips;

// 4: 8x8 cubemap resolution, shader needs to use the same value as preprocessing
RENDERCORE_API const uint32 GDiffuseConvolveMipLevel = 4;

//
// FWhiteTextureCube implementation
//

/** A solid color cube texture. */
class FSolidColorTextureCube : public FTexture
{
public:

	FSolidColorTextureCube(const FColor& InColor)
	:	Color(InColor)
	{}

	// FRenderResource interface.
	virtual void InitRHI()
	{
		// Create the texture RHI.
		FTextureCubeRHIRef TextureCube = RHICreateTextureCube(1,PF_B8G8R8A8,1,0,NULL);
		TextureRHI = TextureCube;

		// Write the contents of the texture.
		for(uint32 FaceIndex = 0;FaceIndex < 6;FaceIndex++)
		{
			uint32 DestStride;
			FColor* DestBuffer = (FColor*)RHILockTextureCubeFace(TextureCube,FaceIndex,0,0,RLM_WriteOnly,DestStride,false);
			*DestBuffer = Color;
			RHIUnlockTextureCubeFace(TextureCube,FaceIndex,0,0,false);
		}

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point,AM_Wrap,AM_Wrap,AM_Wrap);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
	}

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const
	{
		return 1;
	}

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const
	{
		return 1;
	}

private:

	FColor Color;
};

/** A white cube texture. */
class FWhiteTextureCube : public FSolidColorTextureCube
{
public:
	FWhiteTextureCube(): FSolidColorTextureCube(FColor(255,255,255)) {}
};
FTexture* GWhiteTextureCube = new TGlobalResource<FWhiteTextureCube>;

/** A black cube texture. */
class FBlackTextureCube : public FSolidColorTextureCube
{
public:
	FBlackTextureCube(): FSolidColorTextureCube(FColor(0,0,0)) {}
};
FTexture* GBlackTextureCube = new TGlobalResource<FBlackTextureCube>;

class FBlackCubeArrayTexture : public FTexture
{
public:
	// FResource interface.
	virtual void InitRHI()
	{
		if (IsFeatureLevelSupported(GRHIShaderPlatform, ERHIFeatureLevel::SM5))
		{
			// Create the texture RHI.
			FTextureCubeRHIRef TextureCubeArray = RHICreateTextureCubeArray(1,1,PF_B8G8R8A8,1,TexCreate_ShaderResource,NULL);
			TextureRHI = TextureCubeArray;

			for(uint32 FaceIndex = 0;FaceIndex < 6;FaceIndex++)
			{
				uint32 DestStride;
				FColor* DestBuffer = (FColor*)RHILockTextureCubeFace(TextureCubeArray,FaceIndex,0,0,RLM_WriteOnly,DestStride,false);
				// Note: alpha is used by reflection environment to say how much of the foreground texture is visible, so 0 says it is completely invisible
				*DestBuffer = FColor(0, 0, 0, 0);
				RHIUnlockTextureCubeFace(TextureCubeArray,FaceIndex,0,0,false);
			}

			// Create the sampler state RHI resource.
			FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point,AM_Wrap,AM_Wrap,AM_Wrap);
			SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
		}
	}

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const
	{
		return 1;
	}

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const
	{
		return 1;
	}
};
FTexture* GBlackCubeArrayTexture = new TGlobalResource<FBlackCubeArrayTexture>;

/*
 	3 XYZ packed in 4 bytes. (11:11:10 for X:Y:Z)
*/

/**
*	operator FVector - unpacked to -1 to 1
*/
FPackedPosition::operator FVector() const
{

	return FVector(Vector.X/1023.f, Vector.Y/1023.f, Vector.Z/511.f);
}

/**
* operator VectorRegister
*/
VectorRegister FPackedPosition::GetVectorRegister() const
{
	FVector UnpackedVect = *this;

	VectorRegister VectorToUnpack = VectorLoadFloat3_W0(&UnpackedVect);

	return VectorToUnpack;
}

/**
* Pack this vector(-1 to 1 for XYZ) to 4 bytes XYZ(11:11:10)
*/
void FPackedPosition::Set( const FVector& InVector )
{
	check (FMath::Abs<float>(InVector.X) <= 1.f && FMath::Abs<float>(InVector.Y) <= 1.f &&  FMath::Abs<float>(InVector.Z) <= 1.f);
	
#if !WITH_EDITORONLY_DATA
	// This should not happen in Console - this should happen during Cooking in PC
	check (false);
#else
	// Too confusing to use .5f - wanted to use the last bit!
	// Change to int for easier read
	Vector.X = FMath::Clamp<int32>(FMath::TruncToInt(InVector.X * 1023.0f),-1023,1023);
	Vector.Y = FMath::Clamp<int32>(FMath::TruncToInt(InVector.Y * 1023.0f),-1023,1023);
	Vector.Z = FMath::Clamp<int32>(FMath::TruncToInt(InVector.Z * 511.0f),-511,511);
#endif
}

/**
* operator << serialize
*/
FArchive& operator<<(FArchive& Ar,FPackedPosition& N)
{
	// Save N.Packed
	return Ar << N.Packed;
}

void CalcMipMapExtent3D( uint32 TextureSizeX, uint32 TextureSizeY, uint32 TextureSizeZ, EPixelFormat Format, uint32 MipIndex, uint32& OutXExtent, uint32& OutYExtent, uint32& OutZExtent )
{
	OutXExtent = FMath::Max<uint32>(TextureSizeX >> MipIndex, GPixelFormats[Format].BlockSizeX);
	OutYExtent = FMath::Max<uint32>(TextureSizeY >> MipIndex, GPixelFormats[Format].BlockSizeY);
	OutZExtent = FMath::Max<uint32>(TextureSizeZ >> MipIndex, GPixelFormats[Format].BlockSizeZ);
}

SIZE_T CalcTextureMipMapSize3D( uint32 TextureSizeX, uint32 TextureSizeY, uint32 TextureSizeZ, EPixelFormat Format, uint32 MipIndex )
{
	uint32 XExtent;
	uint32 YExtent;
	uint32 ZExtent;
	CalcMipMapExtent3D(TextureSizeX, TextureSizeY, TextureSizeZ, Format, MipIndex, XExtent, YExtent, ZExtent);

	const uint32 XPitch = (XExtent / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes;
	const uint32 NumRows = YExtent / GPixelFormats[Format].BlockSizeY;
	const uint32 NumLayers = ZExtent / GPixelFormats[Format].BlockSizeZ;

	return NumLayers * NumRows * XPitch;
}

SIZE_T CalcTextureSize3D( uint32 SizeX, uint32 SizeY, uint32 SizeZ, EPixelFormat Format, uint32 MipCount )
{
	SIZE_T Size = 0;
	for ( uint32 MipIndex=0; MipIndex < MipCount; ++MipIndex )
	{
		Size += CalcTextureMipMapSize3D(SizeX,SizeY,SizeZ,Format,MipIndex);
	}
	return Size;
}

FIntPoint CalcMipMapExtent( uint32 TextureSizeX, uint32 TextureSizeY, EPixelFormat Format, uint32 MipIndex )
{
	return FIntPoint(FMath::Max<uint32>(TextureSizeX >> MipIndex, GPixelFormats[Format].BlockSizeX), FMath::Max<uint32>(TextureSizeY >> MipIndex, GPixelFormats[Format].BlockSizeY));
}

SIZE_T CalcTextureMipMapSize( uint32 TextureSizeX, uint32 TextureSizeY, EPixelFormat Format, uint32 MipIndex )
{
	FIntPoint MipExtent = CalcMipMapExtent(TextureSizeX, TextureSizeY, Format, MipIndex);

	const uint32 Pitch = (MipExtent.X / GPixelFormats[Format].BlockSizeX) * GPixelFormats[Format].BlockBytes;
	const uint32 NumRows = MipExtent.Y / GPixelFormats[Format].BlockSizeY;

	return NumRows * Pitch;
}

SIZE_T CalcTextureSize( uint32 SizeX, uint32 SizeY, EPixelFormat Format, uint32 MipCount )
{
	SIZE_T Size = 0;
	for ( uint32 MipIndex=0; MipIndex < MipCount; ++MipIndex )
	{
		Size += CalcTextureMipMapSize(SizeX,SizeY,Format,MipIndex);
	}
	return Size;
}

void CopyTextureData2D(const void* Source,void* Dest,uint32 SizeY,EPixelFormat Format,uint32 SourceStride,uint32 DestStride)
{
	const uint32 BlockSizeY = GPixelFormats[Format].BlockSizeY;
	const uint32 NumBlocksY = (SizeY + BlockSizeY - 1) / BlockSizeY;

	// a DestStride of 0 means to use the SourceStride
	if(SourceStride == DestStride || DestStride == 0)
	{
		// If the source and destination have the same stride, copy the data in one block.
		FMemory::Memcpy(Dest,Source,NumBlocksY * SourceStride);
	}
	else
	{
		// If the source and destination have different strides, copy each row of blocks separately.
		const uint32 NumBytesPerRow = FMath::Min<uint32>(SourceStride, DestStride);
		for(uint32 BlockY = 0;BlockY < NumBlocksY;++BlockY)
		{
			FMemory::Memcpy(
				(uint8*)Dest   + DestStride   * BlockY,
				(uint8*)Source + SourceStride * BlockY,
				NumBytesPerRow
				);
		}
	}
}

/** Helper functions for text output of texture properties... */
#ifndef CASE_ENUM_TO_TEXT
#define CASE_ENUM_TO_TEXT(txt) case txt: return TEXT(#txt);
#endif

#ifndef TEXT_TO_ENUM
#define TEXT_TO_ENUM(eVal, txt)		if (FCString::Stricmp(TEXT(#eVal), txt) == 0)	return eVal;
#endif

const TCHAR* GetPixelFormatString(EPixelFormat InPixelFormat)
{
	switch (InPixelFormat)
	{
		FOREACH_ENUM_EPIXELFORMAT(CASE_ENUM_TO_TEXT)
	default:
		return TEXT("PF_Unknown");
	}	
}

EPixelFormat GetPixelFormatFromString(const TCHAR* InPixelFormatStr)
{
#define TEXT_TO_PIXELFORMAT(f) TEXT_TO_ENUM(f, InPixelFormatStr);
	FOREACH_ENUM_EPIXELFORMAT(TEXT_TO_PIXELFORMAT)
#undef TEXT_TO_PIXELFORMAT
	return PF_Unknown;
}


const TCHAR* GetCubeFaceName(ECubeFace Face)
{
	switch(Face)
	{
	case CubeFace_PosX:
		return TEXT("PosX");
	case CubeFace_NegX:
		return TEXT("NegX");
	case CubeFace_PosY:
		return TEXT("PosY");
	case CubeFace_NegY:
		return TEXT("NegY");
	case CubeFace_PosZ:
		return TEXT("PosZ");
	case CubeFace_NegZ:
		return TEXT("NegZ");
	default:
		return TEXT("");
	}
}

ECubeFace GetCubeFaceFromName(const FString& Name)
{
	// not fast but doesn't have to be
	if(Name.EndsWith(TEXT("PosX")))
	{
		return CubeFace_PosX;
	}
	else if(Name.EndsWith(TEXT("NegX")))
	{
		return CubeFace_NegX;
	}
	else if(Name.EndsWith(TEXT("PosY")))
	{
		return CubeFace_PosY;
	}
	else if(Name.EndsWith(TEXT("NegY")))
	{
		return CubeFace_NegY;
	}
	else if(Name.EndsWith(TEXT("PosZ")))
	{
		return CubeFace_PosZ;
	}
	else if(Name.EndsWith(TEXT("NegZ")))
	{
		return CubeFace_NegZ;
	}

	return CubeFace_MAX;
}

class FVector4VertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0,0,VET_Float4,0));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FVector4VertexDeclaration> GVector4VertexDeclaration;

RENDERCORE_API FVertexDeclarationRHIRef& GetVertexDeclarationFVector4()
{
	return GVector4VertexDeclaration.VertexDeclarationRHI;
}

class FVector3VertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0,0,VET_Float3,0));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FVector3VertexDeclaration> GVector3VertexDeclaration;

RENDERCORE_API FVertexDeclarationRHIRef& GetVertexDeclarationFVector3()
{
	return GVector3VertexDeclaration.VertexDeclarationRHI;
}

RENDERCORE_API bool IsSimpleDynamicLightingEnabled()
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SimpleDynamicLighting"));
	return (CVar->GetValueOnAnyThread() != 0);
}