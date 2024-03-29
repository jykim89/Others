// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11Device.cpp: D3D device RHI implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"
#include "AllowWindowsPlatformTypes.h"
	#include <delayimp.h>
#include "HideWindowsPlatformTypes.h"


bool D3D11RHI_ShouldCreateWithD3DDebug()
{
	// Use a debug device if specified on the command line.
	return
		FParse::Param(FCommandLine::Get(),TEXT("d3ddebug")) ||
		FParse::Param(FCommandLine::Get(),TEXT("d3debug")) ||
		FParse::Param(FCommandLine::Get(),TEXT("dxdebug"));
}

bool D3D11RHI_ShouldAllowAsyncResourceCreation()
{
	static bool bAllowAsyncResourceCreation = !FParse::Param(FCommandLine::Get(),TEXT("nod3dasync"));
	return bAllowAsyncResourceCreation;
}

IMPLEMENT_MODULE(FD3D11DynamicRHIModule, D3D11RHI);

FD3D11DynamicRHI::FD3D11DynamicRHI(IDXGIFactory* InDXGIFactory,D3D_FEATURE_LEVEL InFeatureLevel, int32 InChosenAdapter) :
	DXGIFactory(InDXGIFactory),
	bDeviceRemoved(false),
	FeatureLevel(InFeatureLevel),
	bUsingTessellation(false),
	PendingNumVertices(0),
	PendingVertexDataStride(0),
	PendingPrimitiveType(0),
	PendingNumPrimitives(0),
	PendingMinVertexIndex(0),
	PendingNumIndices(0),
	PendingIndexDataStride(0),
	CurrentDepthTexture(NULL),
	NumSimultaneousRenderTargets(0),
	NumUAVs(0),
	CurrentDSVAccessType(DSAT_Writable),
	bDiscardSharedConstants(false),
	GPUProfilingData(this),
	ChosenAdapter(InChosenAdapter)
{
	// This should be called once at the start 
	check(ChosenAdapter >= 0);
	check( IsInGameThread() );
	check( !GIsThreadedRendering );

	// Allocate a buffer of zeroes. This is used when we need to pass D3D memory
	// that we don't care about and will overwrite with valid data in the future.
	ZeroBufferSize = 4 * (1 << 20); // 4MB in the zero buffer.
	ZeroBuffer = FMemory::Malloc(ZeroBufferSize);
	FMemory::Memzero(ZeroBuffer,ZeroBufferSize);

	GPoolSizeVRAMPercentage = 0;
	GTexturePoolSize = 0;
	if ( GReadTexturePoolSizeFromIni )
	{
		int32 PoolSize;
		GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSize"), PoolSize, GEngineIni);

		GTexturePoolSize = int64(PoolSize) * 1024 * 1024;
	}
	else
	{
		GConfig->GetInt( TEXT( "TextureStreaming" ), TEXT( "PoolSizeVRAMPercentage" ), GPoolSizeVRAMPercentage, GEngineIni );
	}

	// Initialize the RHI capabilities.
	check(FeatureLevel == D3D_FEATURE_LEVEL_11_0 || FeatureLevel == D3D_FEATURE_LEVEL_10_0 );

	if(FeatureLevel == D3D_FEATURE_LEVEL_10_0)
	{
		GSupportsDepthFetchDuringDepthTest = false;
	}

	// ES2 feature level emulation in D3D11
	if (FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES2")))
	{
		SetMaxRHIFeatureLevel(ERHIFeatureLevel::ES2);
		GRHIShaderPlatform = SP_PCD3D_ES2;
	}
	else if(FeatureLevel == D3D_FEATURE_LEVEL_11_0)
	{
		SetMaxRHIFeatureLevel(ERHIFeatureLevel::SM5);
		GRHIShaderPlatform = SP_PCD3D_SM5;
	}
	else if(FeatureLevel == D3D_FEATURE_LEVEL_10_0)
	{
		SetMaxRHIFeatureLevel(ERHIFeatureLevel::SM4);
		GRHIShaderPlatform = SP_PCD3D_SM4;
	}

	GPixelCenterOffset = 0.0f;	// Note that in D3D11, there is no half-texel offset (ala DX9)	
	GSupportsVertexInstancing = true;
	GSupportsVertexTextureFetch = true;

	// Initialize the platform pixel format map.
	GPixelFormats[ PF_Unknown		].PlatformFormat	= DXGI_FORMAT_UNKNOWN;
	GPixelFormats[ PF_A32B32G32R32F	].PlatformFormat	= DXGI_FORMAT_R32G32B32A32_FLOAT;
	GPixelFormats[ PF_B8G8R8A8		].PlatformFormat	= DXGI_FORMAT_B8G8R8A8_TYPELESS;
	GPixelFormats[ PF_G8			].PlatformFormat	= DXGI_FORMAT_R8_UNORM;
	GPixelFormats[ PF_G16			].PlatformFormat	= DXGI_FORMAT_R16_UNORM;
	GPixelFormats[ PF_DXT1			].PlatformFormat	= DXGI_FORMAT_BC1_TYPELESS;
	GPixelFormats[ PF_DXT3			].PlatformFormat	= DXGI_FORMAT_BC2_TYPELESS;
	GPixelFormats[ PF_DXT5			].PlatformFormat	= DXGI_FORMAT_BC3_TYPELESS;
	GPixelFormats[ PF_BC4			].PlatformFormat	= DXGI_FORMAT_BC4_UNORM;
	GPixelFormats[ PF_UYVY			].PlatformFormat	= DXGI_FORMAT_UNKNOWN;		// TODO: Not supported in D3D11
#if DEPTH_32_BIT_CONVERSION
	GPixelFormats[ PF_DepthStencil	].PlatformFormat	= DXGI_FORMAT_R32G8X24_TYPELESS; 
	GPixelFormats[ PF_DepthStencil	].BlockBytes		= 5;
	GPixelFormats[ PF_X24_G8 ].PlatformFormat			= DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
	GPixelFormats[ PF_X24_G8].BlockBytes				= 5;
#else
	GPixelFormats[ PF_DepthStencil	].PlatformFormat	= DXGI_FORMAT_R24G8_TYPELESS;
	GPixelFormats[ PF_DepthStencil	].BlockBytes		= 4;
	GPixelFormats[ PF_X24_G8 ].PlatformFormat			= DXGI_FORMAT_X24_TYPELESS_G8_UINT;
	GPixelFormats[ PF_X24_G8].BlockBytes				= 4;
#endif
	GPixelFormats[ PF_ShadowDepth	].PlatformFormat	= DXGI_FORMAT_R16_TYPELESS;
	GPixelFormats[ PF_ShadowDepth	].BlockBytes		= 2;
	GPixelFormats[ PF_R32_FLOAT		].PlatformFormat	= DXGI_FORMAT_R32_FLOAT;
	GPixelFormats[ PF_G16R16		].PlatformFormat	= DXGI_FORMAT_R16G16_UNORM;
	GPixelFormats[ PF_G16R16F		].PlatformFormat	= DXGI_FORMAT_R16G16_FLOAT;
	GPixelFormats[ PF_G16R16F_FILTER].PlatformFormat	= DXGI_FORMAT_R16G16_FLOAT;
	GPixelFormats[ PF_G32R32F		].PlatformFormat	= DXGI_FORMAT_R32G32_FLOAT;
	GPixelFormats[ PF_A2B10G10R10   ].PlatformFormat    = DXGI_FORMAT_R10G10B10A2_UNORM;
	GPixelFormats[ PF_A16B16G16R16  ].PlatformFormat    = DXGI_FORMAT_R16G16B16A16_UNORM;
	GPixelFormats[ PF_D24 ].PlatformFormat				= DXGI_FORMAT_R24G8_TYPELESS;
	GPixelFormats[ PF_R16F			].PlatformFormat	= DXGI_FORMAT_R16_FLOAT;
	GPixelFormats[ PF_R16F_FILTER	].PlatformFormat	= DXGI_FORMAT_R16_FLOAT;

	GPixelFormats[ PF_FloatRGB	].PlatformFormat		= DXGI_FORMAT_R11G11B10_FLOAT;
	GPixelFormats[ PF_FloatRGB	].BlockBytes			= 4;
	GPixelFormats[ PF_FloatRGBA	].PlatformFormat		= DXGI_FORMAT_R16G16B16A16_FLOAT;
	GPixelFormats[ PF_FloatRGBA	].BlockBytes			= 8;

	GPixelFormats[ PF_FloatR11G11B10].PlatformFormat	= DXGI_FORMAT_R11G11B10_FLOAT;
	GPixelFormats[ PF_FloatR11G11B10].BlockBytes		= 4;

	GPixelFormats[ PF_V8U8			].PlatformFormat	= DXGI_FORMAT_R8G8_SNORM;
	GPixelFormats[ PF_BC5			].PlatformFormat	= DXGI_FORMAT_BC5_UNORM;
	GPixelFormats[ PF_A1			].PlatformFormat	= DXGI_FORMAT_R1_UNORM; // Not supported for rendering.
	GPixelFormats[ PF_A8			].PlatformFormat	= DXGI_FORMAT_A8_UNORM;
	GPixelFormats[ PF_R32_UINT		].PlatformFormat	= DXGI_FORMAT_R32_UINT;
	GPixelFormats[ PF_R32_SINT		].PlatformFormat	= DXGI_FORMAT_R32_SINT;

	GPixelFormats[ PF_R16_UINT         ].PlatformFormat = DXGI_FORMAT_R16_UINT;
	GPixelFormats[ PF_R16_SINT         ].PlatformFormat = DXGI_FORMAT_R16_SINT;
	GPixelFormats[ PF_R16G16B16A16_UINT].PlatformFormat = DXGI_FORMAT_R16G16B16A16_UINT;
	GPixelFormats[ PF_R16G16B16A16_SINT].PlatformFormat = DXGI_FORMAT_R16G16B16A16_SINT;

	GPixelFormats[ PF_R5G6B5_UNORM	].PlatformFormat	= DXGI_FORMAT_B5G6R5_UNORM;
	GPixelFormats[ PF_R8G8B8A8		].PlatformFormat	= DXGI_FORMAT_R8G8B8A8_TYPELESS;
	GPixelFormats[ PF_R8G8			].PlatformFormat	= DXGI_FORMAT_R8G8_UNORM;

	if (FeatureLevel >= D3D_FEATURE_LEVEL_11_0)
	{
		GSupportsSeparateRenderTargetBlendState = true;
		GMaxTextureDimensions = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
		GMaxCubeTextureDimensions = D3D11_REQ_TEXTURECUBE_DIMENSION;
		GMaxTextureArrayLayers = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
	}
	else if (FeatureLevel >= D3D_FEATURE_LEVEL_10_0)
	{
		GMaxTextureDimensions = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
		GMaxCubeTextureDimensions = D3D10_REQ_TEXTURECUBE_DIMENSION;
		GMaxTextureArrayLayers = D3D10_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
	}

	GMaxTextureMipCount = FMath::CeilLogTwo( GMaxTextureDimensions ) + 1;
	GMaxTextureMipCount = FMath::Min<int32>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );
	GMaxShadowDepthBufferSizeX = 4096;
	GMaxShadowDepthBufferSizeY = 4096;

	// Initialize the constant buffers.
	InitConstantBuffers();

	// Create the dynamic vertex and index buffers used for Draw[Indexed]PrimitiveUP.
	uint32 DynamicVBSizes[] = {128,1024,64*1024,1024*1024,0};
	DynamicVB = new FD3D11DynamicBuffer(this,D3D11_BIND_VERTEX_BUFFER,DynamicVBSizes);
	uint32 DynamicIBSizes[] = {128,1024,64*1024,1024*1024,0};
	DynamicIB = new FD3D11DynamicBuffer(this,D3D11_BIND_INDEX_BUFFER,DynamicIBSizes);
}

void FD3D11DynamicRHI::Shutdown()
{
	check(IsInGameThread() && IsInRenderingThread());  // require that the render thread has been shut down

	// Cleanup the D3D device.
	CleanupD3DDevice();

	// Release buffered timestamp queries
	GPUProfilingData.FrameTiming.ReleaseResource();

	// Release the buffer of zeroes.
	FMemory::Free(ZeroBuffer);
	ZeroBuffer = NULL;
	ZeroBufferSize = 0;
}

/**
 * Returns a supported screen resolution that most closely matches the input.
 * @param Width - Input: Desired resolution width in pixels. Output: A width that the platform supports.
 * @param Height - Input: Desired resolution height in pixels. Output: A height that the platform supports.
 */
void FD3D11DynamicRHI::RHIGetSupportedResolution( uint32 &Width, uint32 &Height )
{
	uint32 InitializedMode = false;
	DXGI_MODE_DESC BestMode;
	BestMode.Width = 0;
	BestMode.Height = 0;

	{
		HRESULT hr = S_OK;
		TRefCountPtr<IDXGIAdapter> Adapter;
		hr = DXGIFactory->EnumAdapters(ChosenAdapter,Adapter.GetInitReference());
		if( DXGI_ERROR_NOT_FOUND == hr )
		{
			return;
		}
		if( FAILED(hr) )
		{
			return;
		}

		// get the description of the adapter
		DXGI_ADAPTER_DESC AdapterDesc;
		VERIFYD3D11RESULT(Adapter->GetDesc(&AdapterDesc));
	  
#ifndef USE_MONOLITHIC_GRAPHICS_DRIVERS
		// Enumerate outputs for this adapter
		// TODO: Cap at 1 for default output
		for(uint32 o = 0;o < 1; o++)
		{
			TRefCountPtr<IDXGIOutput> Output;
			hr = Adapter->EnumOutputs(o,Output.GetInitReference());
			if(DXGI_ERROR_NOT_FOUND == hr)
				break;
			if(FAILED(hr))
				return;

			// TODO: GetDisplayModeList is a terribly SLOW call.  It can take up to a second per invocation.
			//  We might want to work around some DXGI badness here.
			DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			uint32 NumModes = 0;
			hr = Output->GetDisplayModeList(Format,0,&NumModes,NULL);
			if(hr == DXGI_ERROR_NOT_FOUND)
			{
				return;
			}
			else if(hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
			{
				UE_LOG(LogD3D11RHI, Fatal,
					TEXT("This application cannot be run over a remote desktop configuration")
					);
				return;
			}
			DXGI_MODE_DESC* ModeList = new DXGI_MODE_DESC[ NumModes ];
			VERIFYD3D11RESULT(Output->GetDisplayModeList(Format,0,&NumModes,ModeList));

			for(uint32 m = 0;m < NumModes;m++)
			{
				// Search for the best mode
				
				// Suppress static analysis warnings about a potentially out-of-bounds read access to ModeList. This is a false positive - Index is always within range.
				CA_SUPPRESS( 6385 );
				bool IsEqualOrBetterWidth = FMath::Abs((int32)ModeList[m].Width - (int32)Width) <= FMath::Abs((int32)BestMode.Width - (int32)Width);
				bool IsEqualOrBetterHeight = FMath::Abs((int32)ModeList[m].Height - (int32)Height) <= FMath::Abs((int32)BestMode.Height - (int32)Height);
				if(!InitializedMode || (IsEqualOrBetterWidth && IsEqualOrBetterHeight))
				{
					BestMode = ModeList[m];
					InitializedMode = true;
				}
			}

			delete[] ModeList;
		}
#endif // USE_MONOLITHIC_GRAPHICS_DRIVERS
	}

	check(InitializedMode);
	Width = BestMode.Width;
	Height = BestMode.Height;
}

// Suppress static analysis warnings in FD3D11DynamicRHI::RHIGetAvailableResolutions() about a potentially out-of-bounds read access to ModeList. This is a false positive - Index is always within range.
#if USING_CODE_ANALYSIS
	MSVC_PRAGMA(warning(push))
	MSVC_PRAGMA(warning(disable:6385))
#endif	// USING_CODE_ANALYSIS

// Re-enable static code analysis warning C6385.
#if USING_CODE_ANALYSIS
	MSVC_PRAGMA(warning(pop))
#endif	// USING_CODE_ANALYSIS

void FD3D11DynamicRHI::GetBestSupportedMSAASetting( DXGI_FORMAT PlatformFormat, uint32 MSAACount, uint32& OutBestMSAACount, uint32& OutMSAAQualityLevels )
{
	//  We disable MSAA for Feature level 10
	if (GRHIFeatureLevel == ERHIFeatureLevel::SM4)
	{
		OutBestMSAACount = 1;
		OutMSAAQualityLevels = 0;
		return;
	}

	// start counting down from current setting (indicated the current "best" count) and move down looking for support
	for(uint32 IndexCount = MSAACount;IndexCount > 0;IndexCount--)
	{
		uint32 NumMultiSampleQualities = 0;
		if(	SUCCEEDED(Direct3DDevice->CheckMultisampleQualityLevels(PlatformFormat,IndexCount,&NumMultiSampleQualities)) && NumMultiSampleQualities > 0 )
		{
			OutBestMSAACount = IndexCount;
			OutMSAAQualityLevels = NumMultiSampleQualities;
			break;
		}
	}
}

uint32 FD3D11DynamicRHI::GetMaxMSAAQuality(uint32 SampleCount)
{
	if(SampleCount <= DX_MAX_MSAA_COUNT)
	{
		// 0 has better quality (a more even distribution)
		// higher quality levels might be useful for non box filtered AA or when using weighted samples 
		return 0;
//		return AvailableMSAAQualities[SampleCount];
	}
	// not supported
	return 0xffffffff;
}

void FD3D11DynamicRHI::SetupAfterDeviceCreation()
{
	// without that the first RHIClear would get a scissor rect of (0,0)-(0,0) which means we get a draw call clear 
	RHISetScissorRect(false, 0, 0, 0, 0);

	UpdateMSAASettings();

	if (GRHISupportsAsyncTextureCreation)
	{
		UE_LOG(LogD3D11RHI,Log,TEXT("Async texture creation enabled"));
	}
	else
	{
		UE_LOG(LogD3D11RHI,Log,TEXT("Async texture creation disabled: %s"),
			D3D11RHI_ShouldAllowAsyncResourceCreation() ? TEXT("no driver support") : TEXT("disabled by user"));
	}
}

void FD3D11DynamicRHI::UpdateMSAASettings()
{	
	check(DX_MAX_MSAA_COUNT == 8);

	// quality levels are only needed for CSAA which we cannot use with custom resolves

	// 0xffffffff means not available
	AvailableMSAAQualities[0] = 0xffffffff;
	AvailableMSAAQualities[1] = 0xffffffff;
	AvailableMSAAQualities[2] = 0;
	AvailableMSAAQualities[3] = 0xffffffff;
	AvailableMSAAQualities[4] = 0;
	AvailableMSAAQualities[5] = 0xffffffff;
	AvailableMSAAQualities[6] = 0xffffffff;
	AvailableMSAAQualities[7] = 0xffffffff;
	AvailableMSAAQualities[8] = 0;
}

void FD3D11DynamicRHI::CleanupD3DDevice()
{
	if(GIsRHIInitialized)
	{
		check(Direct3DDevice);
		check(Direct3DDeviceIMContext);

		// Reset the RHI initialized flag.
		GIsRHIInitialized = false;

		check(!GIsCriticalError);

		// Ask all initialized FRenderResources to release their RHI resources.
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			FRenderResource* Resource = *ResourceIt;
			check(Resource->IsInitialized());
			Resource->ReleaseRHI();
		}

		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->ReleaseDynamicRHI();
		}

		extern void EmptyD3DSamplerStateCache();
		EmptyD3DSamplerStateCache();

		// release our dynamic VB and IB buffers
		DynamicVB = NULL;
		DynamicIB = NULL;

		ReleasePooledUniformBuffers();
		ReleasePooledTextures();

		// Release the device and its IC
		StateCache.SetContext(nullptr);
		Direct3DDeviceIMContext = NULL;

		Direct3DDevice = NULL;
	}
}

void FD3D11DynamicRHI::RHIFlushResources()
{
	// Nothing to do (yet!)
}

void FD3D11DynamicRHI::RHIAcquireThreadOwnership()
{
	// Nothing to do
}
void FD3D11DynamicRHI::RHIReleaseThreadOwnership()
{
	// Nothing to do
}

void FD3D11DynamicRHI::RHIAutomaticCacheFlushAfterComputeShader(bool bEnable) 
{
	// Nothing to do
}

void FD3D11DynamicRHI::RHIFlushComputeShaderCache()
{
	// Nothing to do
}
