// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyRHI.cpp: Empty device RHI implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"

DEFINE_LOG_CATEGORY(LogEmpty)

bool FEmptyDynamicRHIModule::IsSupported()
{
	return true;
}

FDynamicRHI* FEmptyDynamicRHIModule::CreateRHI()
{
	return new FEmptyDynamicRHI();
}

IMPLEMENT_MODULE(FEmptyDynamicRHIModule, EmptyRHI);

FEmptyDynamicRHI::FEmptyDynamicRHI()
{
	// This should be called once at the start 
	check( IsInGameThread() );
	check( !GIsThreadedRendering );

	// Initialize the RHI capabilities.

//	GRHIShaderPlatform = 

// 	GPixelCenterOffset = 0.0f;
// 	GSupportsVertexInstancing = false;
// 	GSupportsEmulatedVertexInstancing = false;
// 	GSupportsVertexTextureFetch = false;
//	GRHIAdapterName = 
//	GRHIVendorId = 
// 	GSupportsRenderTargetFormat_PF_G8 = false;
// 	GSupportsQuads = true;
// 	GRHISupportsTextureStreaming = true;
// 	GMaxShadowDepthBufferSizeX = 4096;
// 	GMaxShadowDepthBufferSizeY = 4096;
// 	GReadTexturePoolSizeFromIni = true;
// 
// 	GMaxTextureDimensions = 16384;
// 	GMaxTextureMipCount = FPlatformMath::CeilLogTwo( GMaxTextureDimensions ) + 1;
// 	GMaxTextureMipCount = FPlatformMath::Min<int32>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );
// 	GMaxCubeTextureDimensions = 16384;
// 	GMaxTextureArrayLayers = 8192;

//	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_NumPlatforms;
//	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM3] = SP_NumPlatforms;
//	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = SP_NumPlatforms;
//	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = ;

// 	GDrawUPVertexCheckCount = MAX_uint16;

	// Initialize the platform pixel format map.
// 	GPixelFormats[PF_Unknown			].PlatformFormat	= 
// 	GPixelFormats[PF_A32B32G32R32F		].PlatformFormat	= 
// 	GPixelFormats[PF_B8G8R8A8			].PlatformFormat	= 
// 	GPixelFormats[PF_G8					].PlatformFormat	= 
// 	GPixelFormats[PF_G16				].PlatformFormat	= 
// 	GPixelFormats[PF_DXT1				].PlatformFormat	= 
// 	GPixelFormats[PF_DXT3				].PlatformFormat	= 
// 	GPixelFormats[PF_DXT5				].PlatformFormat	= 
// 	GPixelFormats[PF_UYVY				].PlatformFormat	= 
// 	GPixelFormats[PF_FloatRGB			].PlatformFormat	= 
// 	GPixelFormats[PF_FloatRGB			].BlockBytes		= 4;
// 	GPixelFormats[PF_FloatRGBA			].PlatformFormat	= 
// 	GPixelFormats[PF_FloatRGBA			].BlockBytes		= 8;
// 	GPixelFormats[PF_DepthStencil		].PlatformFormat	= 
// 	GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;
// 	GPixelFormats[PF_X24_G8				].PlatformFormat	= 
// 	GPixelFormats[PF_X24_G8				].BlockBytes		= 1;
// 	GPixelFormats[PF_ShadowDepth		].PlatformFormat	= 
// 	GPixelFormats[PF_R32_FLOAT			].PlatformFormat	= 
// 	GPixelFormats[PF_G16R16				].PlatformFormat	= 
// 	GPixelFormats[PF_G16R16F			].PlatformFormat	= 
// 	GPixelFormats[PF_G16R16F_FILTER		].PlatformFormat	= 
// 	GPixelFormats[PF_G32R32F			].PlatformFormat	= 
// 	GPixelFormats[PF_A2B10G10R10		].PlatformFormat    = 
// 	GPixelFormats[PF_A16B16G16R16		].PlatformFormat    = 
// 	GPixelFormats[PF_D24				].PlatformFormat	= 
// 	GPixelFormats[PF_R16F				].PlatformFormat	= 
// 	GPixelFormats[PF_R16F_FILTER		].PlatformFormat	= 
// 	GPixelFormats[PF_BC5				].PlatformFormat	= 
// 	GPixelFormats[PF_V8U8				].PlatformFormat	= 
// 	GPixelFormats[PF_A1					].PlatformFormat	= 
// 	GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= 
// 	GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 4;
// 	GPixelFormats[PF_A8					].PlatformFormat	= 
// 	GPixelFormats[PF_R32_UINT			].PlatformFormat	= 
// 	GPixelFormats[PF_R32_SINT			].PlatformFormat	= 
// 	GPixelFormats[PF_R16G16B16A16_UINT	].PlatformFormat	= 
// 	GPixelFormats[PF_R16G16B16A16_SINT	].PlatformFormat	= 
// 	GPixelFormats[PF_R5G6B5_UNORM		].PlatformFormat	= 
// 	GPixelFormats[PF_R8G8B8A8			].PlatformFormat	= 
// 	GPixelFormats[PF_R8G8				].PlatformFormat	= 

	GDynamicRHI = this;

	// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitDynamicRHI();
	}
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitRHI();
	}

	GIsRHIInitialized = true;
}

void FEmptyDynamicRHI::Init()
{

}

void FEmptyDynamicRHI::Shutdown()
{
	check(IsInGameThread() && IsInRenderingThread());
}

void FEmptyDynamicRHI::RHIBeginFrame()
{

}

void FEmptyDynamicRHI::RHIEndFrame()
{

}

void FEmptyDynamicRHI::RHIBeginScene()
{

}

void FEmptyDynamicRHI::RHIEndScene()
{

}

void FEmptyDynamicRHI::PushEvent(const TCHAR* Name)
{

}

void FEmptyDynamicRHI::PopEvent()
{

}

void FEmptyDynamicRHI::RHIGetSupportedResolution( uint32 &Width, uint32 &Height )
{

}

bool FEmptyDynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{

	return false;
}

void FEmptyDynamicRHI::RHIFlushResources()
{

}

void FEmptyDynamicRHI::RHIAcquireThreadOwnership()
{

}
void FEmptyDynamicRHI::RHIReleaseThreadOwnership()
{

}
