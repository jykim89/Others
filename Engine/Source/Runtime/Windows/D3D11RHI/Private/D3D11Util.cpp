// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11Util.h: D3D RHI utility implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"

#define D3DERR(x) case x: ErrorCodeText = TEXT(#x); break;
#define LOCTEXT_NAMESPACE "Developer.MessageLog"

#ifndef _FACD3D 
	#define _FACD3D  0x876
#endif	//_FACD3D 
#ifndef MAKE_D3DHRESULT
	#define _FACD3D  0x876
	#define MAKE_D3DHRESULT( code )  MAKE_HRESULT( 1, _FACD3D, code )
#endif	//MAKE_D3DHRESULT

#if WITH_D3DX_LIBS
	#ifndef D3DERR_INVALIDCALL
		#define D3DERR_INVALIDCALL MAKE_D3DHRESULT(2156)
	#endif//D3DERR_INVALIDCALL
	#ifndef D3DERR_WASSTILLDRAWING
		#define D3DERR_WASSTILLDRAWING MAKE_D3DHRESULT(540)
	#endif//D3DERR_WASSTILLDRAWING
#endif

static FString GetD3D11DeviceHungErrorString(HRESULT ErrorCode)
{
	FString ErrorCodeText;

	switch(ErrorCode)
	{
		D3DERR(DXGI_ERROR_DEVICE_HUNG)
		D3DERR(DXGI_ERROR_DEVICE_REMOVED)
		D3DERR(DXGI_ERROR_DEVICE_RESET)
		D3DERR(DXGI_ERROR_DRIVER_INTERNAL_ERROR)
		D3DERR(DXGI_ERROR_INVALID_CALL)
		default: ErrorCodeText = FString::Printf(TEXT("%08X"),(int32)ErrorCode);
	}

	return ErrorCodeText;
}

static FString GetD3D11ErrorString(HRESULT ErrorCode, ID3D11Device* Device)
{
	FString ErrorCodeText;

	switch(ErrorCode)
	{
		D3DERR(S_OK);
		D3DERR(D3D11_ERROR_FILE_NOT_FOUND)
		D3DERR(D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS)
#if WITH_D3DX_LIBS
		D3DERR(D3DERR_INVALIDCALL)
		D3DERR(D3DERR_WASSTILLDRAWING)
#endif	//WITH_D3DX_LIBS
		D3DERR(E_FAIL)
		D3DERR(E_INVALIDARG)
		D3DERR(E_OUTOFMEMORY)
		D3DERR(DXGI_ERROR_INVALID_CALL)
		D3DERR(E_NOINTERFACE)
		D3DERR(DXGI_ERROR_DEVICE_REMOVED)
		default: ErrorCodeText = FString::Printf(TEXT("%08X"),(int32)ErrorCode);
	}

	if(ErrorCode == DXGI_ERROR_DEVICE_REMOVED && Device)
	{
		HRESULT hResDeviceRemoved = Device->GetDeviceRemovedReason();
		ErrorCodeText += FString(TEXT(" ")) + GetD3D11DeviceHungErrorString(hResDeviceRemoved);
	}

	return ErrorCodeText;
}

#undef D3DERR

const TCHAR* GetD3D11TextureFormatString(DXGI_FORMAT TextureFormat)
{
	static const TCHAR* EmptyString = TEXT("");
	const TCHAR* TextureFormatText = EmptyString;
#define D3DFORMATCASE(x) case x: TextureFormatText = TEXT(#x); break;
	switch(TextureFormat)
	{
		D3DFORMATCASE(DXGI_FORMAT_R8G8B8A8_UNORM)
		D3DFORMATCASE(DXGI_FORMAT_B8G8R8A8_UNORM)
		D3DFORMATCASE(DXGI_FORMAT_B8G8R8X8_UNORM)
		D3DFORMATCASE(DXGI_FORMAT_BC1_UNORM)
		D3DFORMATCASE(DXGI_FORMAT_BC2_UNORM)
		D3DFORMATCASE(DXGI_FORMAT_BC3_UNORM)
		D3DFORMATCASE(DXGI_FORMAT_BC4_UNORM)
		D3DFORMATCASE(DXGI_FORMAT_R16G16B16A16_FLOAT)
		D3DFORMATCASE(DXGI_FORMAT_R32G32B32A32_FLOAT)
		D3DFORMATCASE(DXGI_FORMAT_UNKNOWN)
		D3DFORMATCASE(DXGI_FORMAT_R8_UNORM)
#if DEPTH_32_BIT_CONVERSION
		D3DFORMATCASE(DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
		D3DFORMATCASE(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS)
#endif
		D3DFORMATCASE(DXGI_FORMAT_D24_UNORM_S8_UINT)
		D3DFORMATCASE(DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
		D3DFORMATCASE(DXGI_FORMAT_R32_FLOAT)
		D3DFORMATCASE(DXGI_FORMAT_R16G16_UINT)
		D3DFORMATCASE(DXGI_FORMAT_R16G16_FLOAT)
		D3DFORMATCASE(DXGI_FORMAT_R32G32_FLOAT)
		D3DFORMATCASE(DXGI_FORMAT_R10G10B10A2_UNORM)
		D3DFORMATCASE(DXGI_FORMAT_R16G16B16A16_UINT)
		D3DFORMATCASE(DXGI_FORMAT_R8G8_SNORM)
		D3DFORMATCASE(DXGI_FORMAT_BC5_UNORM)
		D3DFORMATCASE(DXGI_FORMAT_R1_UNORM)
		D3DFORMATCASE(DXGI_FORMAT_R8G8B8A8_TYPELESS)
		D3DFORMATCASE(DXGI_FORMAT_B8G8R8A8_TYPELESS)
		default: TextureFormatText = EmptyString;
	}
#undef D3DFORMATCASE
	return TextureFormatText;
}

static FString GetD3D11TextureFlagString(uint32 TextureFlags)
{
	FString TextureFormatText = TEXT("");

	if (TextureFlags & D3D11_BIND_RENDER_TARGET)
	{
		TextureFormatText += TEXT("D3D11_BIND_RENDER_TARGET ");
	}

	if (TextureFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		TextureFormatText += TEXT("D3D11_BIND_DEPTH_STENCIL ");
	}

	if (TextureFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		TextureFormatText += TEXT("D3D11_BIND_SHADER_RESOURCE ");
	}

	if (TextureFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		TextureFormatText += TEXT("D3D11_BIND_UNORDERED_ACCESS ");
	}

	return TextureFormatText;
}


static void TerminateOnDeviceRemoved(HRESULT D3DResult)
{
	if (D3DResult == DXGI_ERROR_DEVICE_REMOVED)
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *LOCTEXT("DeviceRemoved", "Video driver crashed and was reset!  Make sure your video drivers are up to date.  Exiting...").ToString(), TEXT("Error"));
		FPlatformMisc::RequestExit(true);
	}
}

#ifndef MAKE_D3DHRESULT
	#define _FACD3D						0x876
	#define MAKE_D3DHRESULT( code)		MAKE_HRESULT( 1, _FACD3D, code )
#endif	//MAKE_D3DHRESULT

void VerifyD3D11Result(HRESULT D3DResult,const ANSICHAR* Code,const ANSICHAR* Filename,uint32 Line, ID3D11Device* Device)
{
	check(FAILED(D3DResult));

	const FString& ErrorString = GetD3D11ErrorString(D3DResult, Device);

	UE_LOG(LogD3D11RHI, Error,TEXT("%s failed \n at %s:%u \n with error %s"),ANSI_TO_TCHAR(Code),ANSI_TO_TCHAR(Filename),Line,*ErrorString);

	TerminateOnDeviceRemoved(D3DResult);

	// this is to track down a rarely happening crash
	if(D3DResult == E_OUTOFMEMORY)
	{
		if (IsInGameThread())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("D3D11RHI", "OutOfMemory", "Out of video memory trying to allocate a rendering resource."));
		}
#if STATS
		GetRendererModule().DebugLogOnCrash();
#endif
		FPlatformMisc::RequestExit(/*bForce=*/ true);
	}

	UE_LOG(LogD3D11RHI, Fatal,TEXT("%s failed \n at %s:%u \n with error %s"),ANSI_TO_TCHAR(Code),ANSI_TO_TCHAR(Filename),Line,*ErrorString);
}

void VerifyD3D11CreateTextureResult(HRESULT D3DResult,const ANSICHAR* Code,const ANSICHAR* Filename,uint32 Line,uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 Format,uint32 NumMips,uint32 Flags)
{
	check(FAILED(D3DResult));

	const FString ErrorString = GetD3D11ErrorString(D3DResult, 0);
	const TCHAR* D3DFormatString = GetD3D11TextureFormatString((DXGI_FORMAT)Format);

	UE_LOG(LogD3D11RHI, Error,TEXT("%s failed \n at %s:%u \n with error %s"),ANSI_TO_TCHAR(Code),ANSI_TO_TCHAR(Filename),Line,*ErrorString);

	TerminateOnDeviceRemoved(D3DResult);

	// this is to track down a rarely happening crash
	if (D3DResult == E_OUTOFMEMORY)
	{
#if STATS
		GetRendererModule().DebugLogOnCrash();
#endif // STATS
	}

	UE_LOG(LogD3D11RHI, Fatal,
		TEXT("%s failed \n at %s:%u \n with error %s, \n Size=%ix%ix%i Format=%s(0x%08X), NumMips=%i, Flags=%s"),
		ANSI_TO_TCHAR(Code),
		ANSI_TO_TCHAR(Filename),
		Line,
		*ErrorString, 
		SizeX, 
		SizeY, 
		SizeZ, 
		D3DFormatString,
		Format,
		NumMips, 
		*GetD3D11TextureFlagString(Flags));
}

void VerifyComRefCount(IUnknown* Object,int32 ExpectedRefs,const TCHAR* Code,const TCHAR* Filename,int32 Line)
{
	int32 NumRefs;

	if (Object)
	{
		NumRefs = Object->AddRef();
		NumRefs = Object->Release();
		if (NumRefs != ExpectedRefs)
		{
			UE_LOG(LogD3D11RHI,Fatal,
				TEXT("%s:(%d): %s has %d refs, expected %d"),
				Filename,
				Line,
				Code,
				NumRefs,
				ExpectedRefs
				);
		}
	}
}

FD3D11BoundRenderTargets::FD3D11BoundRenderTargets(ID3D11DeviceContext* InDeviceContext)
{
	FMemory::Memzero(RenderTargetViews,sizeof(RenderTargetViews));
	DepthStencilView = NULL;
	InDeviceContext->OMGetRenderTargets(
		MaxSimultaneousRenderTargets,
		&RenderTargetViews[0],
		&DepthStencilView
		);
	for (NumActiveTargets = 0; NumActiveTargets < MaxSimultaneousRenderTargets; ++NumActiveTargets)
	{
		if (RenderTargetViews[NumActiveTargets] == NULL)
		{
			break;
		}
	}
}

FD3D11BoundRenderTargets::~FD3D11BoundRenderTargets()
{
	// OMGetRenderTargets calls AddRef on each RTV/DSV it returns. We need
	// to make a corresponding call to Release.
	for (int32 TargetIndex = 0; TargetIndex < NumActiveTargets; ++TargetIndex)
	{
		RenderTargetViews[TargetIndex]->Release();
	}
	if (DepthStencilView)
	{
		DepthStencilView->Release();
	}
}

FD3D11DynamicBuffer::FD3D11DynamicBuffer(FD3D11DynamicRHI* InD3DRHI, D3D11_BIND_FLAG InBindFlags, uint32* InBufferSizes)
	: D3DRHI(InD3DRHI)
	, BindFlags(InBindFlags)
	, LockedBufferIndex(-1)
{
	while (BufferSizes.Num() < MAX_BUFFERS && *InBufferSizes > 0)
	{
		uint32 Size = *InBufferSizes++;
		BufferSizes.Add(Size);
	}
	check(*InBufferSizes == 0);
	InitResource();
}

FD3D11DynamicBuffer::~FD3D11DynamicBuffer()
{
	ReleaseResource();
}

void FD3D11DynamicBuffer::InitRHI()
{
	D3D11_BUFFER_DESC Desc;
	ZeroMemory( &Desc, sizeof( D3D11_BUFFER_DESC ) );
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = BindFlags;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = 0;

	while (Buffers.Num() < BufferSizes.Num())
	{
		TRefCountPtr<ID3D11Buffer> Buffer;
		Desc.ByteWidth = BufferSizes[Buffers.Num()];
		VERIFYD3D11RESULT(D3DRHI->GetDevice()->CreateBuffer(&Desc,NULL,Buffer.GetInitReference()));
		UpdateBufferStats(Buffer,true);
		Buffers.Add(Buffer);
	}
}

void FD3D11DynamicBuffer::ReleaseRHI()
{
	for (int32 i = 0; i < Buffers.Num(); ++i)
	{
		UpdateBufferStats(Buffers[i],false);
	}
	Buffers.Empty();
}

void* FD3D11DynamicBuffer::Lock(uint32 Size)
{
	check(LockedBufferIndex == -1 && Buffers.Num() > 0);
	
	int32 BufferIndex = 0;
	int32 NumBuffers = Buffers.Num();
	while (BufferIndex < NumBuffers && BufferSizes[BufferIndex] < Size)
	{
		BufferIndex++;
	}
	if (BufferIndex == NumBuffers)
	{
		BufferIndex--;

		TRefCountPtr<ID3D11Buffer> Buffer;
		D3D11_BUFFER_DESC Desc;
		ZeroMemory( &Desc, sizeof( D3D11_BUFFER_DESC ) );
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = BindFlags;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Desc.MiscFlags = 0;
		Desc.ByteWidth = Size;
		VERIFYD3D11RESULT(D3DRHI->GetDevice()->CreateBuffer(&Desc,NULL,Buffer.GetInitReference()));
		UpdateBufferStats(Buffers[BufferIndex],false);
		UpdateBufferStats(Buffer,true);
		Buffers[BufferIndex] = Buffer;
		BufferSizes[BufferIndex] = Size;
	}

	LockedBufferIndex = BufferIndex;
	D3D11_MAPPED_SUBRESOURCE MappedSubresource;
	VERIFYD3D11RESULT( D3DRHI->GetDeviceContext()->Map( Buffers[BufferIndex],0,D3D11_MAP_WRITE_DISCARD,0,&MappedSubresource ) );
	return MappedSubresource.pData;
}

ID3D11Buffer* FD3D11DynamicBuffer::Unlock()
{
	check(LockedBufferIndex != -1);
	ID3D11Buffer* LockedBuffer = Buffers[LockedBufferIndex];
	D3DRHI->GetDeviceContext()->Unmap(LockedBuffer,0);
	LockedBufferIndex = -1;
	return LockedBuffer;
}

//
// Stat declarations.
//


DEFINE_STAT(STAT_D3D11PresentTime);
DEFINE_STAT(STAT_D3D11TexturesAllocated);
DEFINE_STAT(STAT_D3D11TexturesReleased);
DEFINE_STAT(STAT_D3D11ClearShaderResourceTime);
DEFINE_STAT(STAT_D3D11CreateTextureTime);
DEFINE_STAT(STAT_D3D11LockTextureTime);
DEFINE_STAT(STAT_D3D11UnlockTextureTime);
DEFINE_STAT(STAT_D3D11CopyTextureTime);
DEFINE_STAT(STAT_D3D11NewBoundShaderStateTime);
DEFINE_STAT(STAT_D3D11CreateBoundShaderStateTime);
DEFINE_STAT(STAT_D3D11CleanUniformBufferTime);
DEFINE_STAT(STAT_D3D11UpdateUniformBufferTime);
DEFINE_STAT(STAT_D3D11TexturePoolMemory);
DEFINE_STAT(STAT_D3D11FreeUniformBufferMemory);
DEFINE_STAT(STAT_D3D11NumFreeUniformBuffers);

#undef LOCTEXT_NAMESPACE
