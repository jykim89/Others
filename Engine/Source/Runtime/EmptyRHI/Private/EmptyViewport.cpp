// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyViewport.cpp: Empty viewport RHI implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"


FEmptyViewport::FEmptyViewport(void* WindowHandle, uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen)
{

}

FEmptyViewport::~FEmptyViewport()
{
}


/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FEmptyDynamicRHI::RHICreateViewport(void* WindowHandle,uint32 SizeX,uint32 SizeY,bool bIsFullscreen)
{
	check( IsInGameThread() );
	return new FEmptyViewport(WindowHandle, SizeX, SizeY, bIsFullscreen);
}

void FEmptyDynamicRHI::RHIResizeViewport(FViewportRHIParamRef ViewportRHI,uint32 SizeX,uint32 SizeY,bool bIsFullscreen)
{
	check( IsInGameThread() );

	DYNAMIC_CAST_EMPTYRESOURCE(Viewport,Viewport);
}

void FEmptyDynamicRHI::RHITick( float DeltaTime )
{
	check( IsInGameThread() );

}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

void FEmptyDynamicRHI::RHIBeginDrawingViewport(FViewportRHIParamRef ViewportRHI, FTextureRHIParamRef RenderTargetRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(Viewport,Viewport);

	RHISetRenderTarget(RHIGetViewportBackBuffer(ViewportRHI), NULL);
}

void FEmptyDynamicRHI::RHIEndDrawingViewport(FViewportRHIParamRef ViewportRHI,bool bPresent,bool bLockToVsync)
{
	DYNAMIC_CAST_EMPTYRESOURCE(Viewport,Viewport);
}

bool FEmptyDynamicRHI::RHIIsDrawingViewport()
{
	return true;
}

FTexture2DRHIRef FEmptyDynamicRHI::RHIGetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(Viewport,Viewport);

	return FTexture2DRHIRef();
}
