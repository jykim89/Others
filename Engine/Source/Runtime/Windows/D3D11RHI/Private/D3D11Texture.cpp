// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11VertexBuffer.cpp: D3D texture RHI implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"

/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

// in bytes, never change after RHI, needed to scale game features
int64 GDedicatedVideoMemory = 0;
// in bytes, never change after RHI, needed to scale game features
int64 GDedicatedSystemMemory = 0;
// in bytes, never change after RHI, needed to scale game features
int64 GSharedSystemMemory = 0;
// In bytes. Never changed after RHI init. Our estimate of the amount of memory that we can use for graphics resources in total.
int64 GTotalGraphicsMemory = 0;

static bool ShouldCountAsTextureMemory(uint32 BindFlags)
{
	return (BindFlags & (D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS)) == 0;
}

// @param b3D true:3D, false:2D or cube map
static TStatId GetD3D11StatEnum(uint32 BindFlags, bool bCubeMap, bool b3D)
{
#if STATS
	if(ShouldCountAsTextureMemory(BindFlags))
	{
		// normal texture
		if(bCubeMap)
		{
			return GET_STATID(STAT_TextureMemoryCube);
		}
		else if(b3D)
		{
			return GET_STATID(STAT_TextureMemory3D);
		}
		else
		{
			return GET_STATID(STAT_TextureMemory2D);
		}
	}
	else
	{
		// render target
		if(bCubeMap)
		{
			return GET_STATID(STAT_RenderTargetMemoryCube);
		}
		else if(b3D)
		{
			return GET_STATID(STAT_RenderTargetMemory3D);
		}
		else
		{
			return GET_STATID(STAT_RenderTargetMemory2D);
		}
	}
#endif
	return TStatId();
}

// Note: This function can be called from many different threads
// @param TextureSize >0 to allocate, <0 to deallocate
// @param b3D true:3D, false:2D or cube map
void UpdateD3D11TextureStats(uint32 BindFlags, uint32 MiscFlags, int64 TextureSize, bool b3D)
{
	if(TextureSize == 0)
	{
		return;
	}
	
	if(ShouldCountAsTextureMemory(BindFlags))
	{
		FPlatformAtomics::InterlockedAdd(&GCurrentTextureMemorySize, Align(TextureSize, 1024) / 1024);
	}
	else
	{
		FPlatformAtomics::InterlockedAdd(&GCurrentRendertargetMemorySize, Align(TextureSize, 1024) / 1024);
	}

	bool bCubeMap = (MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0;

	INC_MEMORY_STAT_BY_FName(*GetD3D11StatEnum(BindFlags, bCubeMap, b3D), TextureSize);

	if(TextureSize > 0)
	{
		INC_DWORD_STAT(STAT_D3D11TexturesAllocated);
	}
	else
	{
		INC_DWORD_STAT(STAT_D3D11TexturesReleased);
	}
}

template<typename BaseResourceType>
void D3D11TextureAllocated( TD3D11Texture2D<BaseResourceType>& Texture )
{
	ID3D11Texture2D* D3D11Texture2D = Texture.GetResource();

	if(D3D11Texture2D)
	{
		if ( (Texture.Flags & TexCreate_Virtual) == TexCreate_Virtual )
		{
			Texture.SetMemorySize(0);
		}
		else
		{
			D3D11_TEXTURE2D_DESC Desc;

			D3D11Texture2D->GetDesc( &Desc );
			check(Texture.IsCubemap() == ((Desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0));

			int64 TextureSize = CalcTextureSize( Desc.Width, Desc.Height, Texture.GetFormat(), Desc.MipLevels ) * Desc.ArraySize;

			Texture.SetMemorySize( TextureSize );
			UpdateD3D11TextureStats(Desc.BindFlags, Desc.MiscFlags, TextureSize, false);
		}
	}
}

template<typename BaseResourceType>
void D3D11TextureDeleted( TD3D11Texture2D<BaseResourceType>& Texture )
{
	ID3D11Texture2D* D3D11Texture2D = Texture.GetResource();

	if(D3D11Texture2D)
	{
		D3D11_TEXTURE2D_DESC Desc;

		D3D11Texture2D->GetDesc( &Desc );
		check(Texture.IsCubemap() == ((Desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0));

		// When using virtual textures use the current memory size, which is the number of physical pages allocated, not virtual
		int64 TextureSize = 0;
		if ( (Texture.GetFlags() & TexCreate_Virtual) == TexCreate_Virtual )
		{
			TextureSize = Texture.GetMemorySize();
		}
		else
		{
			TextureSize = CalcTextureSize( Desc.Width, Desc.Height, Texture.GetFormat(), Desc.MipLevels ) * Desc.ArraySize;
		}

		UpdateD3D11TextureStats(Desc.BindFlags, Desc.MiscFlags, -TextureSize, false);
	}
}

void D3D11TextureAllocated2D( FD3D11Texture2D& Texture )
{
	D3D11TextureAllocated(Texture);
}

void D3D11TextureAllocated( FD3D11Texture3D& Texture )
{
	ID3D11Texture3D* D3D11Texture3D = Texture.GetResource();

	if(D3D11Texture3D)
	{
		D3D11_TEXTURE3D_DESC Desc;

		D3D11Texture3D->GetDesc( &Desc );

		int64 TextureSize = CalcTextureSize3D( Desc.Width, Desc.Height, Desc.Depth, Texture.GetFormat(), Desc.MipLevels );

		Texture.SetMemorySize( TextureSize );

		UpdateD3D11TextureStats(Desc.BindFlags, Desc.MiscFlags, TextureSize, true);
	}
}

void D3D11TextureDeleted( FD3D11Texture3D& Texture )
{
	ID3D11Texture3D* D3D11Texture3D = Texture.GetResource();

	if(D3D11Texture3D)
	{
		D3D11_TEXTURE3D_DESC Desc;

		D3D11Texture3D->GetDesc( &Desc );

		int64 TextureSize = CalcTextureSize3D( Desc.Width, Desc.Height, Desc.Depth, Texture.GetFormat(), Desc.MipLevels );

		UpdateD3D11TextureStats(Desc.BindFlags, Desc.MiscFlags, -TextureSize, true);
	}
}

template<typename BaseResourceType>
TD3D11Texture2D<BaseResourceType>::~TD3D11Texture2D()
{
	D3D11TextureDeleted(*this);
	if (bPooled)
	{
		ReturnPooledTexture2D(GetNumMips(), GetFormat(), GetResource());
	}

#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	D3DRHI->DestroyVirtualTexture(GetFlags(), GetRawTextureMemory());
#endif
}

FD3D11Texture3D::~FD3D11Texture3D()
{
	D3D11TextureDeleted( *this );
}

uint64 FD3D11DynamicRHI::RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
{
	OutAlign = 0;
	return CalcTextureSize(SizeX, SizeY, (EPixelFormat)Format, NumMips);
}

uint64 FD3D11DynamicRHI::RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	OutAlign = 0;
	return CalcTextureSize3D(SizeX, SizeY, SizeZ, (EPixelFormat)Format, NumMips);
}

uint64 FD3D11DynamicRHI::RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags,	uint32& OutAlign)
{
	OutAlign = 0;
	return CalcTextureSize(Size, Size, (EPixelFormat)Format, NumMips) * 6;
}

/**
 * Retrieves texture memory stats. 
 */
void FD3D11DynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	OutStats.DedicatedVideoMemory = GDedicatedVideoMemory;
    OutStats.DedicatedSystemMemory = GDedicatedSystemMemory;
    OutStats.SharedSystemMemory = GSharedSystemMemory;
	OutStats.TotalGraphicsMemory = GTotalGraphicsMemory ? GTotalGraphicsMemory : -1;

	OutStats.AllocatedMemorySize = int64(GCurrentTextureMemorySize) * 1024;
	OutStats.TexturePoolSize = GTexturePoolSize;
	OutStats.PendingMemoryAdjustment = 0;
}

/**
 * Fills a texture with to visualize the texture pool memory.
 *
 * @param	TextureData		Start address
 * @param	SizeX			Number of pixels along X
 * @param	SizeY			Number of pixels along Y
 * @param	Pitch			Number of bytes between each row
 * @param	PixelSize		Number of bytes each pixel represents
 *
 * @return true if successful, false otherwise
 */
bool FD3D11DynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	// currently only implemented for console (Note: Keep this function for further extension. Talk to NiklasS for more info.)
	return false;
}

/*------------------------------------------------------------------------------
	Texture pooling.
------------------------------------------------------------------------------*/

/** Define to 1 to enable the pooling of 2D texture resources. */
#define USE_TEXTURE_POOLING 0

/** A texture resource stored in the pool. */
struct FPooledTexture2D
{
	/** The texture resource. */
	TRefCountPtr<ID3D11Texture2D> Resource;
};

/** A pool of D3D texture resources. */
struct FTexturePool
{
	TArray<FPooledTexture2D> Textures;
};

/** The global texture pool. */
struct FGlobalTexturePool
{
	/** Formats stored in the pool. */
	enum EInternalFormat
	{
		IF_DXT1,
		IF_DXT5,
		IF_BC5,
		IF_Max
	};

	enum
	{
		/** Minimum mip count for which to pool textures. */
		MinMipCount = 7,
		/** Maximum mip count for which to pool textures. */
		MaxMipCount = 13,
		/** The number of pools based on mip levels. */
		MipPoolCount = MaxMipCount - MinMipCount,
	};

	/** The individual texture pools. */
	FTexturePool Pools[MipPoolCount][IF_Max];
};
FGlobalTexturePool GTexturePool;

/**
 * Releases all pooled textures.
 */
void ReleasePooledTextures()
{
	for (int32 MipPoolIndex = 0; MipPoolIndex < FGlobalTexturePool::MipPoolCount; ++MipPoolIndex)
	{
		for (int32 FormatPoolIndex = 0; FormatPoolIndex < FGlobalTexturePool::IF_Max; ++FormatPoolIndex)
		{
			GTexturePool.Pools[MipPoolIndex][FormatPoolIndex].Textures.Empty();
		}
	}
}

/**
 * Retrieves the texture pool for the specified mip count and format.
 */
FTexturePool* GetTexturePool(int32 MipCount, EPixelFormat PixelFormat)
{
	FTexturePool* Pool = NULL;
	int32 MipPool = MipCount - FGlobalTexturePool::MinMipCount;
	if (MipPool >= 0 && MipPool < FGlobalTexturePool::MipPoolCount)
	{
		int32 FormatPool = -1;
		switch (PixelFormat)
		{
			case PF_DXT1: FormatPool = FGlobalTexturePool::IF_DXT1; break;
			case PF_DXT5: FormatPool = FGlobalTexturePool::IF_DXT5; break;
			case PF_BC5: FormatPool = FGlobalTexturePool::IF_BC5; break;
		}
		if (FormatPool >= 0 && FormatPool < FGlobalTexturePool::IF_Max)
		{
			Pool = &GTexturePool.Pools[MipPool][FormatPool];
		}
	}
	return Pool;
}

/**
 * Retrieves a texture from the pool if one exists.
 */
bool GetPooledTexture2D(int32 MipCount, EPixelFormat PixelFormat, FPooledTexture2D* OutTexture)
{
#if USE_TEXTURE_POOLING
	FTexturePool* Pool = GetTexturePool(MipCount,PixelFormat);
	if (Pool && Pool->Textures.Num() > 0)
	{
		*OutTexture = Pool->Textures.Last();

		{
			D3D11_TEXTURE2D_DESC Desc;
			OutTexture->Resource->GetDesc(&Desc);
			check(Desc.Format == GPixelFormats[PixelFormat].PlatformFormat);
			check(MipCount == Desc.MipLevels);
			check(Desc.Width == Desc.Height);
			check(Desc.Width == (1 << (MipCount-1)));
			int32 TextureSize = CalcTextureSize(Desc.Width, Desc.Height, PixelFormat, Desc.MipLevels);
			DEC_MEMORY_STAT_BY(STAT_D3D11TexturePoolMemory,TextureSize);
		}

		Pool->Textures.RemoveAt(Pool->Textures.Num() - 1);
		return true;
	}
#endif // #if USE_TEXTURE_POOLING
	return false;
}

/**
 * Returns a texture to its pool.
 */
void ReturnPooledTexture2D(int32 MipCount, EPixelFormat PixelFormat, ID3D11Texture2D* InResource)
{
#if USE_TEXTURE_POOLING
	FTexturePool* Pool = GetTexturePool(MipCount,PixelFormat);
	if (Pool)
	{
		FPooledTexture2D* PooledTexture = new(Pool->Textures) FPooledTexture2D;
		PooledTexture->Resource = InResource;
		{
			D3D11_TEXTURE2D_DESC Desc;
			PooledTexture->Resource->GetDesc(&Desc);
			check(Desc.Format == GPixelFormats[PixelFormat].PlatformFormat);
			check(MipCount == Desc.MipLevels);
			check(Desc.Width == Desc.Height);
			check(Desc.Width == (1 << (MipCount-1)));
			int32 TextureSize = CalcTextureSize(Desc.Width, Desc.Height, PixelFormat, Desc.MipLevels);
			INC_MEMORY_STAT_BY(STAT_D3D11TexturePoolMemory,TextureSize);
		}
	}
#endif // #if USE_TEXTURE_POOLING
}

#if WITH_D3DX_LIBS
DXGI_FORMAT FD3D11DynamicRHI::GetPlatformTextureResourceFormat(DXGI_FORMAT InFormat, uint32 InFlags)
{
	// DX 11 Shared textures must be B8G8R8A8_UNORM
	if (InFlags & TexCreate_Shared)
	{
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	}
	return InFormat;
}
#endif	//WITH_D3DX_LIBS

/** If true, guard texture creates with SEH to log more information about a driver crash we are seeing during texture streaming. */
#define GUARDED_TEXTURE_CREATES (PLATFORM_WINDOWS && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))

/**
 * Creates a 2D texture optionally guarded by a structured exception handler.
 */
void SafeCreateTexture2D(ID3D11Device* Direct3DDevice, const D3D11_TEXTURE2D_DESC* TextureDesc, const D3D11_SUBRESOURCE_DATA* SubResourceData, ID3D11Texture2D** OutTexture2D)
{
#if GUARDED_TEXTURE_CREATES
	bool bDriverCrash = true;
	__try
	{
#endif // #if GUARDED_TEXTURE_CREATES
		VERIFYD3D11CREATETEXTURERESULT(
			Direct3DDevice->CreateTexture2D(TextureDesc,SubResourceData,OutTexture2D),
			TextureDesc->Width,
			TextureDesc->Height,
			TextureDesc->ArraySize,
			TextureDesc->Format,
			TextureDesc->MipLevels,
			TextureDesc->BindFlags
			);
#if GUARDED_TEXTURE_CREATES
		bDriverCrash = false;
	}
	__finally
	{
		if (bDriverCrash)
		{
			UE_LOG(LogD3D11RHI,Error,
				TEXT("Driver crashed while creating texture: %ux%ux%u %s(0x%08x) with %u mips"),
				TextureDesc->Width,
				TextureDesc->Height,
				TextureDesc->ArraySize,
				GetD3D11TextureFormatString(TextureDesc->Format),
				(uint32)TextureDesc->Format,
				TextureDesc->MipLevels
				);
		}
	}
#endif // #if GUARDED_TEXTURE_CREATES
}

template<typename BaseResourceType>
TD3D11Texture2D<BaseResourceType>* FD3D11DynamicRHI::CreateD3D11Texture2D(uint32 SizeX,uint32 SizeY,uint32 SizeZ,bool bTextureArray,bool bCubeTexture,uint8 Format,
	uint32 NumMips,uint32 NumSamples,uint32 Flags, FResourceBulkDataInterface* BulkData)
{
	check(SizeX > 0 && SizeY > 0 && NumMips > 0);

	if (bCubeTexture)
	{
		check(SizeX <= GetMaxCubeTextureDimension());
		check(SizeX == SizeY);
	}
	else
	{
		check(SizeX <= GetMax2DTextureDimension());
		check(SizeY <= GetMax2DTextureDimension());
	}

	if (bTextureArray)
	{
		check(SizeZ <= GetMaxTextureArrayLayers());
	}

	// Render target allocation with UAV flag will silently fail in feature level 10
	check(FeatureLevel >= D3D_FEATURE_LEVEL_11_0 || !(Flags & TexCreate_UAV));

	SCOPE_CYCLE_COUNTER(STAT_D3D11CreateTextureTime);

	bool bPooledTexture = true;

	if (GRHIFeatureLevel <= ERHIFeatureLevel::ES2)
	{
		// Remove sRGB read flag when not supported
		Flags &= ~TexCreate_SRGB;
	}

	const bool bSRGB = (Flags & TexCreate_SRGB) != 0;

	const DXGI_FORMAT PlatformResourceFormat = FD3D11DynamicRHI::GetPlatformTextureResourceFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat, Flags);
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	
	// Determine the MSAA settings to use for the texture.
	D3D11_DSV_DIMENSION DepthStencilViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	D3D11_RTV_DIMENSION RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	D3D11_SRV_DIMENSION ShaderResourceViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	uint32 CPUAccessFlags = 0;
	D3D11_USAGE TextureUsage = D3D11_USAGE_DEFAULT;
	uint32 BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bool bCreateShaderResource = true;

	uint32 ActualMSAACount = NumSamples;

	uint32 ActualMSAAQuality = GetMaxMSAAQuality(ActualMSAACount);

	// 0xffffffff means not supported
	if (ActualMSAAQuality == 0xffffffff || (Flags & TexCreate_Shared) != 0)
	{
		// no MSAA
		ActualMSAACount = 1;
		ActualMSAAQuality = 0;
	}

	if(ActualMSAACount > 1)
	{
		DepthStencilViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		RenderTargetViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
		ShaderResourceViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
		bPooledTexture = false;
	}

	if (NumMips < 1 || SizeX != SizeY || (1 << (NumMips - 1)) != SizeX || (Flags & TexCreate_Shared) != 0)
	{
		bPooledTexture = false;
	}

	if (Flags & TexCreate_CPUReadback)
	{
		check(!(Flags & TexCreate_RenderTargetable));
		check(!(Flags & TexCreate_DepthStencilTargetable));
		check(!(Flags & TexCreate_ShaderResource));

		CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		TextureUsage = D3D11_USAGE_STAGING;
		BindFlags = 0;
		bCreateShaderResource = false;
	}

	// Describe the texture.
	D3D11_TEXTURE2D_DESC TextureDesc;
	ZeroMemory( &TextureDesc, sizeof( D3D11_TEXTURE2D_DESC ) );
	TextureDesc.Width = SizeX;
	TextureDesc.Height = SizeY;
	TextureDesc.MipLevels = NumMips;
	TextureDesc.ArraySize = SizeZ;
	TextureDesc.Format = PlatformResourceFormat;
	TextureDesc.SampleDesc.Count = ActualMSAACount;
	TextureDesc.SampleDesc.Quality = ActualMSAAQuality;
	TextureDesc.Usage = TextureUsage;
	TextureDesc.BindFlags = BindFlags;
	TextureDesc.CPUAccessFlags = CPUAccessFlags;
	TextureDesc.MiscFlags = bCubeTexture ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

	if (Flags & TexCreate_Shared)
	{
		TextureDesc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
	}

	if (Flags & TexCreate_GenerateMipCapable)
	{
		// Set the flag that allows us to call GenerateMips on this texture later
		TextureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
		bPooledTexture = false;
	}

	// Set up the texture bind flags.
	bool bCreateRTV = false;
	bool bCreateDSV = false;
	bool bCreatedRTVPerSlice = false;

	if(Flags & TexCreate_RenderTargetable)
	{
		check(!(Flags & TexCreate_DepthStencilTargetable));
		check(!(Flags & TexCreate_ResolveTargetable));
		TextureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		bCreateRTV = true;
	}
	else if(Flags & TexCreate_DepthStencilTargetable)
	{
		check(!(Flags & TexCreate_RenderTargetable));
		check(!(Flags & TexCreate_ResolveTargetable));
		TextureDesc.BindFlags |= D3D11_BIND_DEPTH_STENCIL; 
		bCreateDSV = true;
	}
	else if(Flags & TexCreate_ResolveTargetable)
	{
		check(!(Flags & TexCreate_RenderTargetable));
		check(!(Flags & TexCreate_DepthStencilTargetable));
		if(Format == PF_DepthStencil || Format == PF_ShadowDepth || Format == PF_D24)
		{
			TextureDesc.BindFlags |= D3D11_BIND_DEPTH_STENCIL; 
			bCreateDSV = true;
		}
		else
		{
			TextureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
			bCreateRTV = true;
		}
	}

	if (Flags & TexCreate_UAV)
	{
		TextureDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		bPooledTexture = false;
	}

	if (bCreateDSV || bCreateRTV || bCubeTexture || bTextureArray)
	{
		bPooledTexture = false;
	}

	FVRamAllocation VRamAllocation;

	if (FPlatformProperties::SupportsFastVRAMMemory())
	{
		if (Flags & TexCreate_FastVRAM)
		{
			VRamAllocation = FFastVRAMAllocator::GetFastVRAMAllocator()->AllocTexture2D(TextureDesc);
		}
	}

	TRefCountPtr<ID3D11Texture2D> TextureResource;
	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;
	TArray<TRefCountPtr<ID3D11RenderTargetView> > RenderTargetViews;
	TRefCountPtr<ID3D11DepthStencilView> DepthStencilViews[DSAT_Count];
	
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	// Turn off pooling when we are using virtual textures or the texture is offline processed as we control when the memory is released
	if ( (Flags & (TexCreate_Virtual | TexCreate_OfflineProcessed)) != 0 )
	{
		bPooledTexture = false;
	}
	void* RawTextureMemory = nullptr;
#else
	Flags &= ~TexCreate_Virtual;
#endif

	if (bPooledTexture)
	{
		FPooledTexture2D PooledTexture;
		if (GetPooledTexture2D(NumMips, (EPixelFormat)Format, &PooledTexture))
		{
			TextureResource = PooledTexture.Resource;
		}
	}

	if (!IsValidRef(TextureResource))
	{
		TArray<D3D11_SUBRESOURCE_DATA> SubResourceData;

		if (BulkData)
		{
			uint8* Data = (uint8*)BulkData->GetResourceBulkData();

			// each mip of each array slice counts as a subresource
			SubResourceData.AddZeroed(NumMips * SizeZ);

			uint32 SliceOffset = 0;
			for (uint32 ArraySliceIndex = 0; ArraySliceIndex < SizeZ; ++ArraySliceIndex)
			{			
				uint32 MipOffset = 0;
				for(uint32 MipIndex = 0;MipIndex < NumMips;++MipIndex)
				{
					uint32 DataOffset = SliceOffset + MipOffset;
					uint32 SubResourceIndex = ArraySliceIndex * NumMips + MipIndex;

					uint32 NumBlocksX = FMath::Max<uint32>(1,(SizeX >> MipIndex) / GPixelFormats[Format].BlockSizeX);
					uint32 NumBlocksY = FMath::Max<uint32>(1,(SizeY >> MipIndex) / GPixelFormats[Format].BlockSizeY);

					SubResourceData[SubResourceIndex].pSysMem = &Data[DataOffset];
					SubResourceData[SubResourceIndex].SysMemPitch      =  NumBlocksX * GPixelFormats[Format].BlockBytes;
					SubResourceData[SubResourceIndex].SysMemSlicePitch =  NumBlocksX * NumBlocksY * SubResourceData[MipIndex].SysMemPitch;

					MipOffset                                  += NumBlocksY * SubResourceData[MipIndex].SysMemPitch;
				}
				SliceOffset += MipOffset;
			}
		}

#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
		if ( (Flags & (TexCreate_Virtual | TexCreate_OfflineProcessed)) != 0 )
		{
			RawTextureMemory = CreateVirtualTexture(SizeX, SizeY, SizeZ, NumMips, bCubeTexture, Flags, &TextureDesc, &TextureResource);
		}
		else
#endif
		{
			SafeCreateTexture2D(Direct3DDevice, &TextureDesc, BulkData != NULL ? (const D3D11_SUBRESOURCE_DATA*)SubResourceData.GetData() : NULL, TextureResource.GetInitReference());
		}

		if(bCreateRTV)
		{
			// Create a render target view for each mip
			for (uint32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
			{
				if ((Flags & TexCreate_TargetArraySlicesIndependently) && (bTextureArray || bCubeTexture))
				{
					bCreatedRTVPerSlice = true;

					for (uint32 SliceIndex = 0; SliceIndex < TextureDesc.ArraySize; SliceIndex++)
					{
						D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
						FMemory::Memzero(&RTVDesc,sizeof(RTVDesc));
						RTVDesc.Format = PlatformRenderTargetFormat;
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
						RTVDesc.Texture2DArray.FirstArraySlice = SliceIndex;
						RTVDesc.Texture2DArray.ArraySize = 1;
						RTVDesc.Texture2DArray.MipSlice = MipIndex;

						TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
						VERIFYD3D11RESULT(Direct3DDevice->CreateRenderTargetView(TextureResource,&RTVDesc,RenderTargetView.GetInitReference()));
						RenderTargetViews.Add(RenderTargetView);
					}
				}
				else
				{
					D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
					FMemory::Memzero(&RTVDesc,sizeof(RTVDesc));
					RTVDesc.Format = PlatformRenderTargetFormat;
					if (bTextureArray || bCubeTexture)
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
						RTVDesc.Texture2DArray.FirstArraySlice = 0;
						RTVDesc.Texture2DArray.ArraySize = TextureDesc.ArraySize;
						RTVDesc.Texture2DArray.MipSlice = MipIndex;
					}
					else
					{
						RTVDesc.ViewDimension = RenderTargetViewDimension;
						RTVDesc.Texture2D.MipSlice = MipIndex;
					}

					TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
					VERIFYD3D11RESULT(Direct3DDevice->CreateRenderTargetView(TextureResource,&RTVDesc,RenderTargetView.GetInitReference()));
					RenderTargetViews.Add(RenderTargetView);
				}
			}
		}
	
		if(bCreateDSV)
		{
			// Create a depth-stencil-view for the texture.
			D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc;
			FMemory::Memzero(&DSVDesc,sizeof(DSVDesc));
			DSVDesc.Format = FindDepthStencilDXGIFormat(PlatformResourceFormat);
			if(bTextureArray || bCubeTexture)
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
				DSVDesc.Texture2DArray.FirstArraySlice = 0;
				DSVDesc.Texture2DArray.ArraySize = TextureDesc.ArraySize;
				DSVDesc.Texture2DArray.MipSlice = 0;
			}
			else
			{
				DSVDesc.ViewDimension = DepthStencilViewDimension;
				DSVDesc.Texture2D.MipSlice = 0;
			}

			for(uint32 AccessType = 0; AccessType < DSAT_Count; ++AccessType)
			{
				// Create a read-only access views for the texture.
				// Read-only DSVs are not supported in Feature Level 10 so 
				// a dummy DSV is created in order reduce logic complexity at a higher-level.
				if(Direct3DDevice->GetFeatureLevel() == D3D_FEATURE_LEVEL_11_0)
				{
					DSVDesc.Flags  = (AccessType & DSAT_ReadOnlyDepth) ? D3D11_DSV_READ_ONLY_DEPTH : 0;
					if(HasStencilBits(DSVDesc.Format))
					{
						DSVDesc.Flags |= (AccessType & DSAT_ReadOnlyStencil) ? D3D11_DSV_READ_ONLY_STENCIL : 0;
					}
				}
				VERIFYD3D11RESULT(Direct3DDevice->CreateDepthStencilView(TextureResource,&DSVDesc,DepthStencilViews[AccessType].GetInitReference()));
			}
		}
	}
	check(IsValidRef(TextureResource));

	// Create a shader resource view for the texture.
	if (bCreateShaderResource)
	{
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			SRVDesc.Format = PlatformShaderResourceFormat;

			if (bCubeTexture && bTextureArray)
			{
				SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
				SRVDesc.TextureCubeArray.MostDetailedMip = 0;
				SRVDesc.TextureCubeArray.MipLevels = NumMips;
				SRVDesc.TextureCubeArray.First2DArrayFace = 0;
				SRVDesc.TextureCubeArray.NumCubes = SizeZ / 6;
			}
			else if(bCubeTexture)
			{
				SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
				SRVDesc.TextureCube.MostDetailedMip = 0;
				SRVDesc.TextureCube.MipLevels = NumMips;
			}
			else if(bTextureArray)
			{
				SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
				SRVDesc.Texture2DArray.MostDetailedMip = 0;
				SRVDesc.Texture2DArray.MipLevels = NumMips;
				SRVDesc.Texture2DArray.FirstArraySlice = 0;
				SRVDesc.Texture2DArray.ArraySize = TextureDesc.ArraySize;
			}
			else
			{
				SRVDesc.ViewDimension = ShaderResourceViewDimension;
				SRVDesc.Texture2D.MostDetailedMip = 0;
				SRVDesc.Texture2D.MipLevels = NumMips;
			}
			VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(TextureResource,&SRVDesc,ShaderResourceView.GetInitReference()));			
		}

		check(IsValidRef(ShaderResourceView));
	}

	TD3D11Texture2D<BaseResourceType>* Texture2D = new TD3D11Texture2D<BaseResourceType>(
		this,
		TextureResource,
		ShaderResourceView,
		bCreatedRTVPerSlice,
		TextureDesc.ArraySize,
		RenderTargetViews,
		DepthStencilViews,
		SizeX,
		SizeY,
		SizeZ,
		NumMips,
		ActualMSAACount,
		(EPixelFormat)Format,
		bCubeTexture,
		Flags,
		bPooledTexture
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
		, RawTextureMemory
#endif
		);

	Texture2D->ResourceInfo.VRamAllocation = VRamAllocation;

	D3D11TextureAllocated(*Texture2D);

	return Texture2D;
}

FD3D11Texture3D* FD3D11DynamicRHI::CreateD3D11Texture3D(uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 Format,uint32 NumMips,uint32 Flags,FResourceBulkDataInterface* BulkData)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11CreateTextureTime);
	
	const bool bSRGB = (Flags & TexCreate_SRGB) != 0;

	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);

	// Describe the texture.
	D3D11_TEXTURE3D_DESC TextureDesc;
	ZeroMemory( &TextureDesc, sizeof( D3D11_TEXTURE3D_DESC ) );
	TextureDesc.Width = SizeX;
	TextureDesc.Height = SizeY;
	TextureDesc.Depth = SizeZ;
	TextureDesc.MipLevels = NumMips;
	TextureDesc.Format = PlatformResourceFormat;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	if (Flags & TexCreate_GenerateMipCapable)
	{
		// Set the flag that allows us to call GenerateMips on this texture later
		TextureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
	}

	if (Flags & TexCreate_UAV)
	{
		TextureDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	bool bCreateRTV = false;

	if(Flags & TexCreate_RenderTargetable)
	{
		TextureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		bCreateRTV = true;
	}

	// Set up the texture bind flags.
	check(!(Flags & TexCreate_DepthStencilTargetable));
	check(!(Flags & TexCreate_ResolveTargetable));
	check(Flags & TexCreate_ShaderResource);


	TArray<D3D11_SUBRESOURCE_DATA> SubResourceData;

	if (BulkData)
	{
		uint8* Data = (uint8*)BulkData->GetResourceBulkData();
		SubResourceData.AddZeroed(NumMips);
		uint32 MipOffset = 0;
		for(uint32 MipIndex = 0;MipIndex < NumMips;++MipIndex)
		{
			SubResourceData[MipIndex].pSysMem = &Data[MipOffset];
			SubResourceData[MipIndex].SysMemPitch      =  FMath::Max<uint32>(1,SizeX >> MipIndex) * GPixelFormats[Format].BlockBytes;
			SubResourceData[MipIndex].SysMemSlicePitch =  FMath::Max<uint32>(1,SizeY >> MipIndex) * SubResourceData[MipIndex].SysMemPitch;
			MipOffset                                  += FMath::Max<uint32>(1,SizeZ >> MipIndex) * SubResourceData[MipIndex].SysMemSlicePitch;
		}
	}

	FVRamAllocation VRamAllocation;

	if (FPlatformProperties::SupportsFastVRAMMemory())
	{
		if (Flags & TexCreate_FastVRAM)
		{
			VRamAllocation = FFastVRAMAllocator::GetFastVRAMAllocator()->AllocTexture3D(TextureDesc);
		}
	}

	TRefCountPtr<ID3D11Texture3D> TextureResource;
	VERIFYD3D11CREATETEXTURERESULT(
		Direct3DDevice->CreateTexture3D(&TextureDesc,BulkData != NULL ? (const D3D11_SUBRESOURCE_DATA*)SubResourceData.GetData() : NULL,TextureResource.GetInitReference()),
		SizeX,
		SizeY,
		SizeZ,
		PlatformShaderResourceFormat,
		NumMips,
		TextureDesc.BindFlags
		);

	// Create a shader resource view for the texture.
	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		SRVDesc.Format = PlatformShaderResourceFormat;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		SRVDesc.Texture3D.MipLevels = NumMips;
		SRVDesc.Texture3D.MostDetailedMip = 0;
		VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(TextureResource,&SRVDesc,ShaderResourceView.GetInitReference()));
	}

	TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
	if(bCreateRTV)
	{
		// Create a render-target-view for the texture.
		D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
		FMemory::Memzero(&RTVDesc,sizeof(RTVDesc));
		RTVDesc.Format = PlatformRenderTargetFormat;
		RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
		RTVDesc.Texture3D.MipSlice = 0;
		RTVDesc.Texture3D.FirstWSlice = 0;
		RTVDesc.Texture3D.WSize = SizeZ;

		VERIFYD3D11RESULT(Direct3DDevice->CreateRenderTargetView(TextureResource,&RTVDesc,RenderTargetView.GetInitReference()));
	}

	TArray<TRefCountPtr<ID3D11RenderTargetView> > RenderTargetViews;
	RenderTargetViews.Add(RenderTargetView);
	FD3D11Texture3D* Texture3D = new FD3D11Texture3D(this,TextureResource,ShaderResourceView,RenderTargetViews,SizeX,SizeY,SizeZ,NumMips,(EPixelFormat)Format,Flags);

	Texture3D->ResourceInfo.VRamAllocation = VRamAllocation;

	D3D11TextureAllocated(*Texture3D);

	return Texture3D;
}

/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

FTexture2DRHIRef FD3D11DynamicRHI::RHICreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,uint32 NumSamples,uint32 Flags,FResourceBulkDataInterface* BulkData)
{
	return CreateD3D11Texture2D<FD3D11BaseTexture2D>(SizeX,SizeY,1,false,false,Format,NumMips,NumSamples,Flags,BulkData);
}

FTexture2DRHIRef FD3D11DynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,uint32 Flags,void** InitialMipData,uint32 NumInitialMips)
{
	FD3D11Texture2D* NewTexture = NULL;
	TRefCountPtr<ID3D11Texture2D> TextureResource;
	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;
	D3D11_TEXTURE2D_DESC TextureDesc = {0};
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	D3D11_SUBRESOURCE_DATA SubResourceData[MAX_TEXTURE_MIP_COUNT] = {0};
	uint32 InvalidFlags = TexCreate_RenderTargetable | TexCreate_ResolveTargetable | TexCreate_DepthStencilTargetable | TexCreate_GenerateMipCapable | TexCreate_UAV | TexCreate_Presentable | TexCreate_CPUReadback;
	TArray<TRefCountPtr<ID3D11RenderTargetView> > RenderTargetViews;
	
	check(GRHISupportsAsyncTextureCreation);
	check((Flags & InvalidFlags) == 0);

	if (GRHIFeatureLevel <= ERHIFeatureLevel::ES2)
	{
		// Remove sRGB read flag when not supported
		Flags &= ~TexCreate_SRGB;
	}

	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat,((Flags & TexCreate_SRGB) != 0));

	TextureDesc.Width = SizeX;
	TextureDesc.Height = SizeY;
	TextureDesc.MipLevels = NumMips;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = PlatformResourceFormat;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.SampleDesc.Quality = 0;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	for (uint32 MipIndex = 0; MipIndex < NumInitialMips; ++MipIndex)
	{
		uint32 NumBlocksX = FMath::Max<uint32>(1,(SizeX >> MipIndex) / GPixelFormats[Format].BlockSizeX);
		uint32 NumBlocksY = FMath::Max<uint32>(1,(SizeY >> MipIndex) / GPixelFormats[Format].BlockSizeY);

		SubResourceData[MipIndex].pSysMem = InitialMipData[MipIndex];
		SubResourceData[MipIndex].SysMemPitch = NumBlocksX * GPixelFormats[Format].BlockBytes;
		SubResourceData[MipIndex].SysMemSlicePitch = NumBlocksX * NumBlocksY * GPixelFormats[Format].BlockBytes;
	}

	void* TempBuffer = ZeroBuffer;
	uint32 TempBufferSize = ZeroBufferSize;
	for (uint32 MipIndex = NumInitialMips; MipIndex < NumMips; ++MipIndex)
	{
		uint32 NumBlocksX = FMath::Max<uint32>(1,(SizeX >> MipIndex) / GPixelFormats[Format].BlockSizeX);
		uint32 NumBlocksY = FMath::Max<uint32>(1,(SizeY >> MipIndex) / GPixelFormats[Format].BlockSizeY);
		uint32 MipSize = NumBlocksX * NumBlocksY * GPixelFormats[Format].BlockBytes;

		if (MipSize > TempBufferSize)
		{
			UE_LOG(LogD3D11RHI,Warning,TEXT("Temp texture streaming buffer not large enough, needed %d bytes"),MipSize);
			check(TempBufferSize == ZeroBufferSize);
			TempBufferSize = MipSize;
			TempBuffer = FMemory::Malloc(TempBufferSize);
			FMemory::Memzero(TempBuffer,TempBufferSize);
		}

		SubResourceData[MipIndex].pSysMem = TempBuffer;
		SubResourceData[MipIndex].SysMemPitch = NumBlocksX * GPixelFormats[Format].BlockBytes;
		SubResourceData[MipIndex].SysMemSlicePitch = MipSize;
	}

	SafeCreateTexture2D(Direct3DDevice,&TextureDesc,SubResourceData,TextureResource.GetInitReference());

	if (TempBufferSize != ZeroBufferSize)
	{
		FMemory::Free(TempBuffer);
	}

	SRVDesc.Format = PlatformShaderResourceFormat;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = NumMips;
	VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(TextureResource,&SRVDesc,ShaderResourceView.GetInitReference()));

	NewTexture = new FD3D11Texture2D(
		this,
		TextureResource,
		ShaderResourceView,
		false,
		1,
		RenderTargetViews,
		/*DepthStencilViews=*/ NULL,
		SizeX,
		SizeY,
		0,
		NumMips,
		/*ActualMSAACount=*/ 1,
		(EPixelFormat)Format,
		/*bInCubemap=*/ false,
		Flags,
		/*bPooledTexture=*/ false
		);

	D3D11TextureAllocated(*NewTexture);
	
	return NewTexture;
}

void FD3D11DynamicRHI::RHICopySharedMips(FTexture2DRHIParamRef DestTexture2DRHI,FTexture2DRHIParamRef SrcTexture2DRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,DestTexture2D);
	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,SrcTexture2D);

	// Use the GPU to asynchronously copy the old mip-maps into the new texture.
	const uint32 NumSharedMips = FMath::Min(DestTexture2D->GetNumMips(),SrcTexture2D->GetNumMips());
	const uint32 SourceMipOffset = SrcTexture2D->GetNumMips() - NumSharedMips;
	const uint32 DestMipOffset   = DestTexture2D->GetNumMips() - NumSharedMips;
	for(uint32 MipIndex = 0;MipIndex < NumSharedMips;++MipIndex)
	{
		// Use the GPU to copy between mip-maps.
		Direct3DDeviceIMContext->CopySubresourceRegion(
			DestTexture2D->GetResource(),
			D3D11CalcSubresource(MipIndex + DestMipOffset,0,DestTexture2D->GetNumMips()),
			0,
			0,
			0,
			SrcTexture2D->GetResource(),
			D3D11CalcSubresource(MipIndex + SourceMipOffset,0,SrcTexture2D->GetNumMips()),
			NULL
			);
	}
}

FTexture2DArrayRHIRef FD3D11DynamicRHI::RHICreateTexture2DArray(uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 Format,uint32 NumMips,uint32 Flags,FResourceBulkDataInterface* BulkData)
{
	check(SizeZ >= 1);
	return CreateD3D11Texture2D<FD3D11BaseTexture2DArray>(SizeX,SizeY,SizeZ,true,false,Format,NumMips,1,Flags,BulkData);
}

FTexture3DRHIRef FD3D11DynamicRHI::RHICreateTexture3D(uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 Format,uint32 NumMips,uint32 Flags,FResourceBulkDataInterface* BulkData)
{
	check(SizeZ >= 1);
	return CreateD3D11Texture3D(SizeX,SizeY,SizeZ,Format,NumMips,Flags,BulkData);
}

void FD3D11DynamicRHI::RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
{
	if(Ref)
	{
		OutInfo = Ref->ResourceInfo;
	}
}

FShaderResourceViewRHIRef FD3D11DynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,Texture2D);

	D3D11_TEXTURE2D_DESC TextureDesc;
	Texture2D->GetResource()->GetDesc(&TextureDesc);

	// Create a Shader Resource View
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = MipLevel;
	SRVDesc.Texture2D.MipLevels = 1;

	SRVDesc.Format = TextureDesc.Format;
	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;
	VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(Texture2D->GetResource(), &SRVDesc, (ID3D11ShaderResourceView**)ShaderResourceView.GetInitReference()));

	return new FD3D11ShaderResourceView(ShaderResourceView,Texture2D);
}

FShaderResourceViewRHIRef FD3D11DynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,Texture2D);

	D3D11_TEXTURE2D_DESC TextureDesc;
	Texture2D->GetResource()->GetDesc(&TextureDesc);
	
	const DXGI_FORMAT PlatformResourceFormat = FD3D11DynamicRHI::GetPlatformTextureResourceFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat, Texture2D->GetFlags());

	bool bSRGB = (Texture2D->GetFlags() & TexCreate_SRGB) != 0;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat,bSRGB);

	// Create a Shader Resource View
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;

	if(TextureDesc.SampleDesc.Count > 1)
	{
		///MS textures can't have mips apparently, so nothing else to set.
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
	}
	else
	{
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MostDetailedMip = MipLevel;
		SRVDesc.Texture2D.MipLevels = NumMipLevels;
	}
	

	SRVDesc.Format = PlatformShaderResourceFormat;
	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;
	VERIFYD3D11RESULT(Direct3DDevice->CreateShaderResourceView(Texture2D->GetResource(), &SRVDesc, (ID3D11ShaderResourceView**)ShaderResourceView.GetInitReference()));

	return new FD3D11ShaderResourceView(ShaderResourceView,Texture2D);
}

/** Generates mip maps for the surface. */
void FD3D11DynamicRHI::RHIGenerateMips(FTextureRHIParamRef TextureRHI)
{
	FD3D11TextureBase* Texture = GetD3D11TextureFromRHITexture(TextureRHI);
	// Surface must have been created with D3D11_BIND_RENDER_TARGET for GenerateMips to work
	check(Texture->GetShaderResourceView() && Texture->GetRenderTargetView(0, -1));
	Direct3DDeviceIMContext->GenerateMips(Texture->GetShaderResourceView());

	GPUProfilingData.RegisterGPUWork(0);
}

/**
 * Computes the size in memory required by a given texture.
 *
 * @param	TextureRHI		- Texture we want to know the size of
 * @return					- Size in Bytes
 */
uint32 FD3D11DynamicRHI::RHIComputeMemorySize(FTextureRHIParamRef TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}
	
	FD3D11TextureBase* Texture = GetD3D11TextureFromRHITexture(TextureRHI);
	return Texture->GetMemorySize();
}

/**
 * Starts an asynchronous texture reallocation. It may complete immediately if the reallocation
 * could be performed without any reshuffling of texture memory, or if there isn't enough memory.
 * The specified status counter will be decremented by 1 when the reallocation is complete (success or failure).
 *
 * Returns a new reference to the texture, which will represent the new mip count when the reallocation is complete.
 * RHIGetAsyncReallocateTexture2DStatus() can be used to check the status of an ongoing or completed reallocation.
 *
 * @param Texture2D		- Texture to reallocate
 * @param NewMipCount	- New number of mip-levels
 * @param NewSizeX		- New width, in pixels
 * @param NewSizeY		- New height, in pixels
 * @param RequestStatus	- Will be decremented by 1 when the reallocation is complete (success or failure).
 * @return				- New reference to the texture, or an invalid reference upon failure
 */
FTexture2DRHIRef FD3D11DynamicRHI::RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2DRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,Texture2D);

	// Allocate a new texture.
	FD3D11Texture2D* NewTexture2D = CreateD3D11Texture2D<FD3D11BaseTexture2D>(NewSizeX,NewSizeY,1,false,false,Texture2D->GetFormat(),NewMipCount,1,Texture2D->GetFlags());
	
	// Use the GPU to asynchronously copy the old mip-maps into the new texture.
	const uint32 NumSharedMips = FMath::Min(Texture2D->GetNumMips(),NewTexture2D->GetNumMips());
	const uint32 SourceMipOffset = Texture2D->GetNumMips()    - NumSharedMips;
	const uint32 DestMipOffset   = NewTexture2D->GetNumMips() - NumSharedMips;
	for(uint32 MipIndex = 0;MipIndex < NumSharedMips;++MipIndex)
	{
		// Use the GPU to copy between mip-maps.
		// This is serialized with other D3D commands, so it isn't necessary to increment Counter to signal a pending asynchronous copy.
		Direct3DDeviceIMContext->CopySubresourceRegion(
			NewTexture2D->GetResource(),
			D3D11CalcSubresource(MipIndex + DestMipOffset,0,NewTexture2D->GetNumMips()),
			0,
			0,
			0,
			Texture2D->GetResource(),
			D3D11CalcSubresource(MipIndex + SourceMipOffset,0,Texture2D->GetNumMips()),
			NULL
			);
	}

	// Decrement the thread-safe counter used to track the completion of the reallocation, since D3D handles sequencing the
	// async mip copies with other D3D calls.
	RequestStatus->Decrement();

	return NewTexture2D;
}

/**
 * Returns the status of an ongoing or completed texture reallocation:
 *	TexRealloc_Succeeded	- The texture is ok, reallocation is not in progress.
 *	TexRealloc_Failed		- The texture is bad, reallocation is not in progress.
 *	TexRealloc_InProgress	- The texture is currently being reallocated async.
 *
 * @param Texture2D		- Texture to check the reallocation status for
 * @return				- Current reallocation status
 */
ETextureReallocationStatus FD3D11DynamicRHI::RHIFinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Succeeded;
}

/**
 * Cancels an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to cancel
 * @param bBlockUntilCompleted	If true, blocks until the cancellation is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus FD3D11DynamicRHI::RHICancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Succeeded;
}

template<typename RHIResourceType>
void* TD3D11Texture2D<RHIResourceType>::Lock(uint32 MipIndex,uint32 ArrayIndex,EResourceLockMode LockMode,uint32& DestStride)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11LockTextureTime);

	// Calculate the subresource index corresponding to the specified mip-map.
	const uint32 Subresource = D3D11CalcSubresource(MipIndex,ArrayIndex,GetNumMips());

	// Calculate the dimensions of the mip-map.
	const uint32 BlockSizeX = GPixelFormats[GetFormat()].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[GetFormat()].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[GetFormat()].BlockBytes;
	const uint32 MipSizeX = FMath::Max(GetSizeX() >> MipIndex,BlockSizeX);
	const uint32 MipSizeY = FMath::Max(GetSizeY() >> MipIndex,BlockSizeY);
	const uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	const uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
	const uint32 MipBytes = NumBlocksX * NumBlocksY * BlockBytes;

	FD3D11LockedData LockedData;
#if	PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	if (D3DRHI->HandleSpecialLock(LockedData, MipIndex, ArrayIndex, GetFlags(), LockMode, GetResource(), RawTextureMemory, GetNumMips(), DestStride))
	{
		// nothing left to do...
	}
	else
#endif
	if( LockMode == RLM_WriteOnly )
	{
		// If we're writing to the texture, allocate a system memory buffer to receive the new contents.
		LockedData.AllocData(MipBytes);
		LockedData.Pitch = DestStride = NumBlocksX * BlockBytes;
	}
	else
	{
		// If we're reading from the texture, we create a staging resource, copy the texture contents to it, and map it.

		// Create the staging texture.
		D3D11_TEXTURE2D_DESC StagingTextureDesc;
		GetResource()->GetDesc(&StagingTextureDesc);
		StagingTextureDesc.Width = MipSizeX;
		StagingTextureDesc.Height = MipSizeY;
		StagingTextureDesc.MipLevels = 1;
		StagingTextureDesc.ArraySize = 1;
		StagingTextureDesc.Usage = D3D11_USAGE_STAGING;
		StagingTextureDesc.BindFlags = 0;
		StagingTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		StagingTextureDesc.MiscFlags = 0;
		TRefCountPtr<ID3D11Texture2D> StagingTexture;
		VERIFYD3D11CREATETEXTURERESULT(
			D3DRHI->GetDevice()->CreateTexture2D(&StagingTextureDesc,NULL,StagingTexture.GetInitReference()),
			GetSizeX(),
			GetSizeY(),
			GetSizeZ(),
			StagingTextureDesc.Format,
			1,
			0
			);
		LockedData.StagingResource = StagingTexture;

		// Copy the mip-map data from the real resource into the staging resource
		D3DRHI->GetDeviceContext()->CopySubresourceRegion(StagingTexture,0,0,0,0,GetResource(),Subresource,NULL);

		// Map the staging resource, and return the mapped address.
		D3D11_MAPPED_SUBRESOURCE MappedTexture;
		VERIFYD3D11RESULT(D3DRHI->GetDeviceContext()->Map(StagingTexture,0,D3D11_MAP_READ,0,&MappedTexture));
		LockedData.SetData(MappedTexture.pData);
		LockedData.Pitch = DestStride = MappedTexture.RowPitch;
	}

	// Add the lock to the outstanding lock list.
	D3DRHI->OutstandingLocks.Add(FD3D11LockedKey(GetResource(),Subresource),LockedData);

	return (void*)LockedData.GetData();
}

template<typename RHIResourceType>
void TD3D11Texture2D<RHIResourceType>::Unlock(uint32 MipIndex,uint32 ArrayIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11UnlockTextureTime);

	// Calculate the subresource index corresponding to the specified mip-map.
	const uint32 Subresource = D3D11CalcSubresource(MipIndex,ArrayIndex,GetNumMips());

	// Find the object that is tracking this lock
	const FD3D11LockedKey LockedKey(GetResource(),Subresource);
	FD3D11LockedData* LockedData = D3DRHI->OutstandingLocks.Find(LockedKey);
	check(LockedData);

#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	if (D3DRHI->HandleSpecialUnlock(MipIndex, GetFlags(), GetResource(), RawTextureMemory))
	{
		// nothing left to do...
	}
	else
#endif
	if(!LockedData->StagingResource)
	{
		// If we're writing, we need to update the subresource
		D3DRHI->GetDeviceContext()->UpdateSubresource(GetResource(),Subresource,NULL,LockedData->GetData(),LockedData->Pitch,0);
		LockedData->FreeData();
	}

	// Remove the lock from the outstanding lock list.
	D3DRHI->OutstandingLocks.Remove(LockedKey);
}

void* FD3D11DynamicRHI::RHILockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	check(TextureRHI);
	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,Texture);
	return Texture->Lock(MipIndex,0,LockMode,DestStride);
}

void FD3D11DynamicRHI::RHIUnlockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,bool bLockWithinMiptail)
{
	check(TextureRHI);
	DYNAMIC_CAST_D3D11RESOURCE(Texture2D,Texture);
	Texture->Unlock(MipIndex,0);
}

void* FD3D11DynamicRHI::RHILockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D11RESOURCE(Texture2DArray,Texture);
	return Texture->Lock(MipIndex,TextureIndex,LockMode,DestStride);
}

void FD3D11DynamicRHI::RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D11RESOURCE(Texture2DArray,Texture);
	Texture->Unlock(MipIndex,TextureIndex);
}

void FD3D11DynamicRHI::RHIUpdateTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,const FUpdateTextureRegion2D& UpdateRegion,uint32 SourcePitch,const uint8* SourceData)
{
    DYNAMIC_CAST_D3D11RESOURCE(Texture2D,Texture);

    D3D11_BOX DestBox =
	{
		UpdateRegion.DestX,                      UpdateRegion.DestY,                       0,
        UpdateRegion.DestX + UpdateRegion.Width, UpdateRegion.DestY + UpdateRegion.Height, 1
	};

	check(GPixelFormats[Texture->GetFormat()].BlockSizeX == 1);
	check(GPixelFormats[Texture->GetFormat()].BlockSizeY == 1);

	Direct3DDeviceIMContext->UpdateSubresource(Texture->GetResource(), MipIndex, &DestBox, SourceData, SourcePitch, 0);
}

void FD3D11DynamicRHI::RHIUpdateTexture3D(FTexture3DRHIParamRef TextureRHI,uint32 MipIndex,const FUpdateTextureRegion3D& UpdateRegion,uint32 SourceRowPitch,uint32 SourceDepthPitch,const uint8* SourceData)
{
	DYNAMIC_CAST_D3D11RESOURCE(Texture3D,Texture);

	D3D11_BOX DestBox =
	{
		UpdateRegion.DestX,                      UpdateRegion.DestY,                       UpdateRegion.DestZ,
		UpdateRegion.DestX + UpdateRegion.Width, UpdateRegion.DestY + UpdateRegion.Height, UpdateRegion.DestZ + UpdateRegion.Depth
	};

	check(GPixelFormats[Texture->GetFormat()].BlockSizeX == 1);
	check(GPixelFormats[Texture->GetFormat()].BlockSizeY == 1);

	Direct3DDeviceIMContext->UpdateSubresource(Texture->GetResource(), MipIndex, &DestBox, SourceData, SourceRowPitch, SourceDepthPitch);
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/
FTextureCubeRHIRef FD3D11DynamicRHI::RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData)
{
	return CreateD3D11Texture2D<FD3D11BaseTextureCube>(Size,Size,6,false,true,Format,NumMips,1,Flags,BulkData);
}

FTextureCubeRHIRef FD3D11DynamicRHI::RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData)
{
	return CreateD3D11Texture2D<FD3D11BaseTextureCube>(Size,Size,6 * ArraySize,true,true,Format,NumMips,1,Flags,BulkData);
}

void* FD3D11DynamicRHI::RHILockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D11RESOURCE(TextureCube,TextureCube);
	uint32 D3DFace = GetD3D11CubeFace((ECubeFace)FaceIndex);
	return TextureCube->Lock(MipIndex,D3DFace + ArrayIndex * 6,LockMode,DestStride);
}
void FD3D11DynamicRHI::RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	DYNAMIC_CAST_D3D11RESOURCE(TextureCube,TextureCube);
	uint32 D3DFace = GetD3D11CubeFace((ECubeFace)FaceIndex);
	TextureCube->Unlock(MipIndex,D3DFace + ArrayIndex * 6);
}

void FD3D11DynamicRHI::RHIBindDebugLabelName(FTextureRHIParamRef TextureRHI, const TCHAR* Name)
{
}

void FD3D11DynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
}

void FD3D11DynamicRHI::RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
}
