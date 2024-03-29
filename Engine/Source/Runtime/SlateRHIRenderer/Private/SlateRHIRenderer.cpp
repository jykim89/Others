// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "SlateRHIRendererPrivatePCH.h"
#include "SlateRHIRenderer.h"
#include "SlateRHIFontTexture.h"
#include "SlateRHIResourceManager.h"
#include "SlateCore.h"
#include "SlateRHIRenderingPolicy.h"
#include "Runtime/Engine/Public/ScreenRendering.h"
#include "Runtime/Engine/Public/ShaderCompiler.h"
#include "SlateShaders.h"

DECLARE_CYCLE_STAT(TEXT("Map Staging Buffer"),STAT_MapStagingBuffer,STATGROUP_CrashTracker);
DECLARE_CYCLE_STAT(TEXT("Generate Capture Buffer"),STAT_GenerateCaptureBuffer,STATGROUP_CrashTracker);
DECLARE_CYCLE_STAT(TEXT("Unmap Staging Buffer"),STAT_UnmapStagingBuffer,STATGROUP_CrashTracker);


namespace CrashTrackerConstants
{
	static const float ScreenScaling = 0.5f;
}

// Defines the maximum size that a slate viewport will create
#define MAX_VIEWPORT_SIZE 16384

static FMatrix CreateProjectionMatrix( uint32 Width, uint32 Height )
{
	
	float PixelOffset = GPixelCenterOffset;

	// Create ortho projection matrix
	const float Left = PixelOffset;
	const float Right = Left+Width;
	const float Top = PixelOffset;
	const float Bottom = Top+Height;
	const float ZNear = -100.0f;
	const float ZFar = 100.0f;
	return AdjustProjectionMatrixForRHI(
		FMatrix(
			FPlane(2.0f/(Right-Left),			0,							0,					0 ),
			FPlane(0,							2.0f/(Top-Bottom),			0,					0 ),
			FPlane(0,							0,							1/(ZNear-ZFar),		0 ),
			FPlane((Left+Right)/(Left-Right),	(Top+Bottom)/(Bottom-Top),	ZNear/(ZNear-ZFar), 1 )
			)
		);
}


void FSlateCrashReportResource::InitDynamicRHI()
{
	CrashReportBuffer = RHICreateTexture2D(
		VirtualScreen.Width() * CrashTrackerConstants::ScreenScaling,
		VirtualScreen.Height() * CrashTrackerConstants::ScreenScaling,
		PF_R8G8B8A8,
		1,
		1,
		TexCreate_RenderTargetable,
		NULL
		);

	for (int32 i = 0; i < 2; ++i)
	{
		ReadbackBuffer[i] = RHICreateTexture2D(
			VirtualScreen.Width() * CrashTrackerConstants::ScreenScaling,
			VirtualScreen.Height() * CrashTrackerConstants::ScreenScaling,
			PF_R8G8B8A8,
			1,
			1,
			TexCreate_CPUReadback,
			NULL
			);
	}
	
	ReadbackBufferIndex = 0;
}

void FSlateCrashReportResource::ReleaseDynamicRHI()
{
	ReadbackBuffer[0].SafeRelease();
	ReadbackBuffer[1].SafeRelease();
	CrashReportBuffer.SafeRelease();
}

FSlateWindowElementList* FSlateCrashReportResource::GetNextElementList()
{
	ElementListIndex = (ElementListIndex + 1) % 2;
	return &ElementList[ElementListIndex];
}


void FSlateRHIRenderer::FViewportInfo::InitRHI()
{
	// Viewport RHI is created on the game thread
	// Create the depth-stencil surface if needed.
	RecreateDepthBuffer_RenderThread();
}

void FSlateRHIRenderer::FViewportInfo::ReleaseRHI()
{
	DepthStencil.SafeRelease();
	ViewportRHI.SafeRelease();
}

void FSlateRHIRenderer::FViewportInfo::ConditionallyUpdateDepthBuffer(bool bInRequiresStencilTest)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER( UpdateDepthBufferCommand, 
		FViewportInfo*, ViewportInfo, this,
		bool, bNewRequiresStencilTest, bInRequiresStencilTest,
	{
		// Allocate a stencil buffer if needed and not already allocated
		if (bNewRequiresStencilTest && !ViewportInfo->bRequiresStencilTest)
		{
			ViewportInfo->bRequiresStencilTest = bNewRequiresStencilTest;
			ViewportInfo->RecreateDepthBuffer_RenderThread();
		}
	});
}

void FSlateRHIRenderer::FViewportInfo::RecreateDepthBuffer_RenderThread()
{
	check(IsInRenderingThread());
	DepthStencil.SafeRelease();
	if (bRequiresStencilTest)
	{		
		FTexture2DRHIRef ShaderResourceUnused;
		RHICreateTargetableShaderResource2D( Width, Height, PF_DepthStencil, 1, TexCreate_None, TexCreate_DepthStencilTargetable, false, DepthStencil, ShaderResourceUnused );
		check( IsValidRef(DepthStencil) );
	}
}

FSlateRHIRenderer::FSlateRHIRenderer()
#if USE_MAX_DRAWBUFFERS
	: EnqueuedWindowDrawBuffer(NULL)
	, FreeBufferIndex(1)
#endif
{
	CrashTrackerResource = NULL;
	ViewMatrix = FMatrix(	FPlane(1,	0,	0,	0),
							FPlane(0,	1,	0,	0),
							FPlane(0,	0,	1,  0),
							FPlane(0,	0,	0,	1));

	bTakingAScreenShot = false;
	OutScreenshotData = NULL;
}

FSlateRHIRenderer::~FSlateRHIRenderer()
{
}

class FSlateRHIFontAtlasFactory : public ISlateFontAtlasFactory
{
public:
	FSlateRHIFontAtlasFactory()
	{
		/** Size of each font texture, width and height */
		AtlasSize = 1024;
		if( !GIsEditor && GConfig )
		{
			GConfig->GetInt( TEXT("SlateRenderer"), TEXT("FontAtlasSize"), AtlasSize, GEngineIni );
			AtlasSize = FMath::Clamp( AtlasSize, 0, 2048 );
		}
	}

	virtual ~FSlateRHIFontAtlasFactory()
	{
	}


	virtual TSharedRef<FSlateFontAtlas> CreateFontAtlas() const OVERRIDE
	{
		return MakeShareable( new FSlateFontAtlasRHI( AtlasSize, AtlasSize ) );
	}
private:
	int32 AtlasSize;
};

void FSlateRHIRenderer::Initialize()
{
	ResourceManager = MakeShareable( new FSlateRHIResourceManager );
	LoadUsedTextures();

	FontCache = MakeShareable( new FSlateFontCache( MakeShareable( new FSlateRHIFontAtlasFactory ) ) );
	FontMeasure = FSlateFontMeasure::Create( FontCache.ToSharedRef() );

	RenderingPolicy = MakeShareable( new FSlateRHIRenderingPolicy( FontCache, ResourceManager.ToSharedRef() ) ); 

	ElementBatcher = MakeShareable( new FSlateElementBatcher( RenderingPolicy.ToSharedRef() ) );
	
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if (GIsEditor)
	{
		FDisplayMetrics DisplayMetrics;
		FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);
		const FIntPoint VirtualScreenOrigin = FIntPoint(DisplayMetrics.VirtualDisplayRect.Left, DisplayMetrics.VirtualDisplayRect.Top);
		const FIntPoint VirtualScreenLowerRight = FIntPoint(DisplayMetrics.VirtualDisplayRect.Right, DisplayMetrics.VirtualDisplayRect.Bottom);
		const FIntRect VirtualScreen = FIntRect(VirtualScreenOrigin, VirtualScreenLowerRight);

		CrashTrackerResource = new FSlateCrashReportResource(VirtualScreen);
		BeginInitResource(CrashTrackerResource);
	}
#endif
}

void FSlateRHIRenderer::Destroy()
{
	RenderingPolicy->ReleaseResources();
	ResourceManager->ReleaseResources();
	FontCache->ReleaseResources();

	for( TMap< const SWindow*, FViewportInfo*>::TIterator It(WindowToViewportInfo); It; ++It )
	{
		BeginReleaseResource( It.Value() );
	}
	
#if PLATFORM_WINDOWS || PLATFORM_MAC
	if (GIsEditor)
	{
		BeginReleaseResource(CrashTrackerResource);
	}
#endif

	FlushRenderingCommands();
	
	check( ElementBatcher.IsUnique() );
	ElementBatcher.Reset();
	FontCache.Reset();
	RenderingPolicy.Reset();
	ResourceManager.Reset();

	for( TMap< const SWindow*, FViewportInfo*>::TIterator It(WindowToViewportInfo); It; ++It )
	{
		FViewportInfo* ViewportInfo = It.Value();
		delete ViewportInfo;
	}
	
	if (CrashTrackerResource)
	{
		delete CrashTrackerResource;
		CrashTrackerResource = NULL;
	}

	WindowToViewportInfo.Empty();
}

/** Returns a draw buffer that can be used by Slate windows to draw window elements */
FSlateDrawBuffer& FSlateRHIRenderer::GetDrawBuffer()
{
#if USE_MAX_DRAWBUFFERS
	FreeBufferIndex = (FreeBufferIndex + 1) % NumDrawBuffers;
	
	FSlateDrawBuffer* Buffer = &DrawBuffers[FreeBufferIndex];
	
	while( !Buffer->Lock() )
	{
		// If the buffer cannot be locked then the buffer is still in use.  If we are here all buffers are in use
		// so wait until one is free.
		if (IsInSlateThread())
		{
			// We can't flush commands on the slate thread, so simply spinlock until we're done
			// this happens if the render thread becomes completely blocked by expensive tasks when the Slate thread is running
			// in this case we cannot tick Slate.
			FPlatformProcess::Sleep(0.001f);
		}
		else
		{
			FlushCommands();
			UE_LOG(LogSlate, Log, TEXT("Slate: Had to block on waiting for a draw buffer"));
			FreeBufferIndex = (FreeBufferIndex + 1) % NumDrawBuffers;
		}
	

		Buffer = &DrawBuffers[FreeBufferIndex];
	}


	Buffer->ClearBuffer();
	return *Buffer;
#else
	// With this method buffers are created on this thread and deleted on the rendering thead
	return *(new FSlateDrawBuffer);
#endif
}

void FSlateRHIRenderer::CreateViewport( const TSharedRef<SWindow> Window )
{
	FlushRenderingCommands();

	if( !WindowToViewportInfo.Contains( &Window.Get() ) )
	{
		const FVector2D WindowSize = Window->GetSizeInScreen();
		// Clamp the window size to a reasonable default anything below 8 is a d3d warning and 8 is used anyway.
		// @todo Slate: This is a hack to work around menus being summoned with 0,0 for window size until they are ticked.
		const uint32 Width = FMath::Max(8,FMath::TruncToInt(WindowSize.X));
		const uint32 Height = FMath::Max(8,FMath::TruncToInt(WindowSize.Y));

		FViewportInfo* NewInfo = new FViewportInfo();
		// Create Viewport RHI if it doesn't exist (this must be done on the game thread)
		TSharedRef<FGenericWindow> NativeWindow = Window->GetNativeWindow().ToSharedRef();
		NewInfo->OSWindow = NativeWindow->GetOSWindowHandle();
		NewInfo->Width = Width;
		NewInfo->Height = Height;
		NewInfo->DesiredWidth = Width;
		NewInfo->DesiredHeight = Height;
		NewInfo->ProjectionMatrix = CreateProjectionMatrix( Width, Height );

		// Sanity check dimensions
		checkf(Width <= MAX_VIEWPORT_SIZE && Height <= MAX_VIEWPORT_SIZE, TEXT("Invalid window with Width=%u and Height=%u"), Width, Height);

		bool bFullscreen = IsViewportFullscreen( *Window );
		NewInfo->ViewportRHI = RHICreateViewport( NewInfo->OSWindow, Width, Height, bFullscreen );
		NewInfo->bFullscreen = bFullscreen;

		WindowToViewportInfo.Add( &Window.Get(), NewInfo );

		BeginInitResource( NewInfo );
	}
}

void FSlateRHIRenderer::ConditionalResizeViewport( FViewportInfo* ViewInfo, uint32 Width, uint32 Height, bool bFullscreen )
{
	check( IsThreadSafeForSlateRendering() );

	if( ViewInfo && ( ViewInfo->Height != Height || ViewInfo->Width != Width ||  ViewInfo->bFullscreen != bFullscreen || !IsValidRef(ViewInfo->ViewportRHI) ) )
	{
		// The viewport size we have doesn't match the requested size of the viewport.
		// Resize it now.

		// Suspend the rendering thread to avoid deadlocks with the gpu
		bool bRecreateThread = true;
		SCOPED_SUSPEND_RENDERING_THREAD( bRecreateThread );

		// Windows are allowed to be zero sized ( sometimes they are animating to/from zero for example)
		// but viewports cannot be zero sized.  Use 8x8 as a reasonably sized viewport in this case.
		uint32 NewWidth = FMath::Max<uint32>( 8, Width );
		uint32 NewHeight = FMath::Max<uint32>( 8, Height );

		// Sanity check dimensions
		if (NewWidth > MAX_VIEWPORT_SIZE)
		{
			UE_LOG(LogSlate, Warning, TEXT("Tried to set viewport width size to %d.  Clamping size to max allowed size of %d instead."), NewWidth, MAX_VIEWPORT_SIZE);
			NewWidth = MAX_VIEWPORT_SIZE;
		}

		if (NewHeight > MAX_VIEWPORT_SIZE)
		{
			UE_LOG(LogSlate, Warning, TEXT("Tried to set viewport height size to %d.  Clamping size to max allowed size of %d instead."), NewHeight, MAX_VIEWPORT_SIZE);
			NewHeight = MAX_VIEWPORT_SIZE;
		}

		ViewInfo->Width = NewWidth;
		ViewInfo->Height = NewHeight;
		ViewInfo->DesiredWidth = NewWidth;
		ViewInfo->DesiredHeight = NewHeight;
		ViewInfo->ProjectionMatrix = CreateProjectionMatrix( NewWidth, NewHeight );
		ViewInfo->bFullscreen = bFullscreen;

		if( IsValidRef( ViewInfo->ViewportRHI ) )
		{
			RHIResizeViewport( ViewInfo->ViewportRHI, NewWidth, NewHeight, bFullscreen );
		}
		else
		{
			ViewInfo->ViewportRHI = RHICreateViewport( ViewInfo->OSWindow, NewWidth, NewHeight, bFullscreen );
		}

		// Safe to call here as the rendering thread has been suspended: game thread == render thread!
		ViewInfo->RecreateDepthBuffer_RenderThread();
	}
}

void FSlateRHIRenderer::UpdateFullscreenState( const TSharedRef<SWindow> Window, uint32 OverrideResX, uint32 OverrideResY )
{
	FViewportInfo* ViewInfo = WindowToViewportInfo.FindRef( &Window.Get() );

	if( !ViewInfo )
	{
		CreateViewport( Window );
	}

	ViewInfo = WindowToViewportInfo.FindRef( &Window.Get() );

	if( ViewInfo )
	{
		const bool bFullscreen = IsViewportFullscreen( *Window );
		
		uint32 ResX = OverrideResX ? OverrideResX : GSystemResolution.ResX;
		uint32 ResY = OverrideResY ? OverrideResY : GSystemResolution.ResY;

		if(GIsEditor || Window->GetWindowMode() == EWindowMode::WindowedFullscreen)
		{
			ResX = ViewInfo->Width;
			ResY = ViewInfo->Height;
		}

		ConditionalResizeViewport( ViewInfo, ResX, ResY, bFullscreen );
	}
}

/** Called when a window is destroyed to give the renderer a chance to free resources */
void FSlateRHIRenderer::OnWindowDestroyed( const TSharedRef<SWindow>& InWindow )
{
	check(IsThreadSafeForSlateRendering());

	FViewportInfo** ViewportInfoPtr = WindowToViewportInfo.Find( &InWindow.Get() );
	if( ViewportInfoPtr )
	{
		BeginReleaseResource( *ViewportInfoPtr );

		// Need to flush rendering commands as the viewport may be in use by the render thread
		// and the rendering resources must be released on the render thread before the viewport can be deleted
		FlushRenderingCommands();
	
		delete *ViewportInfoPtr;
	}

	WindowToViewportInfo.Remove( &InWindow.Get() );
}

/** Draws windows from a FSlateDrawBuffer on the render thread */
void FSlateRHIRenderer::DrawWindow_RenderThread( const FViewportInfo& ViewportInfo, const FSlateWindowElementList& WindowElementList, bool bLockToVsync )
{
	SCOPED_DRAW_EVENT(SlateUI, DEC_SCENE_ITEMS);

	// Should only be called by the rendering thread
	check(IsInRenderingThread());
	
	{
		SCOPE_CYCLE_COUNTER( STAT_SlateRenderingRTTime );

		// Update the vertex and index buffer
		RenderingPolicy->UpdateBuffers( WindowElementList );
		// should have been created by the game thread
		check( IsValidRef(ViewportInfo.ViewportRHI) );

		RHIBeginDrawingViewport( ViewportInfo.ViewportRHI, FTextureRHIRef() );
		RHISetViewport( 0,0,0,ViewportInfo.Width, ViewportInfo.Height, 0.0f );

		FTexture2DRHIRef BackBuffer = RHIGetViewportBackBuffer( ViewportInfo.ViewportRHI );

		if( ViewportInfo.bRequiresStencilTest )
		{
			check(IsValidRef( ViewportInfo.DepthStencil ));

			// Reset the backbuffer as our color render target and also set a depth stencil buffer
			RHISetRenderTarget( BackBuffer, ViewportInfo.DepthStencil );
			// Clear the stencil buffer
			RHIClear( false, FLinearColor::White, false, 0.0f, true, 0x00, FIntRect());
		}

		if( WindowElementList.GetRenderBatches().Num() > 0 )
		{
			FSlateRenderTarget BackBufferTarget( BackBuffer, FIntPoint( ViewportInfo.Width, ViewportInfo.Height ) );

			RenderingPolicy->DrawElements
			(
				FIntPoint(ViewportInfo.Width, ViewportInfo.Height),
				BackBufferTarget,
				ViewMatrix*ViewportInfo.ProjectionMatrix,
				WindowElementList.GetRenderBatches()
			);
		}
	}


	// Calculate renderthread time (excluding idle time).	
	uint32 StartTime		= FPlatformTime::Cycles();
		
	// Note - We do not include present time in the slate render thread stat
	RHIEndDrawingViewport( ViewportInfo.ViewportRHI, true, bLockToVsync );

	uint32 EndTime		= FPlatformTime::Cycles();

	GSwapBufferTime		= EndTime - StartTime;
	SET_CYCLE_COUNTER(STAT_PresentTime, GSwapBufferTime);

	static uint32 LastTimestamp	= 0;
	uint32 ThreadTime	= EndTime - LastTimestamp;
	LastTimestamp		= EndTime;

	uint32 RenderThreadIdle = 0;	

	FThreadIdleStats& RenderThread = FThreadIdleStats::Get();
	GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForAllOtherSleep] = RenderThread.Waits;
	GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += GSwapBufferTime;
	GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;
	RenderThread.Waits = 0;

	SET_CYCLE_COUNTER(STAT_RenderingIdleTime_RenderThreadSleepTime, GRenderThreadIdle[0]);
	SET_CYCLE_COUNTER(STAT_RenderingIdleTime_WaitingForGPUQuery, GRenderThreadIdle[1]);
	SET_CYCLE_COUNTER(STAT_RenderingIdleTime_WaitingForGPUPresent, GRenderThreadIdle[2]);

	for (int32 Index = 0; Index < ERenderThreadIdleTypes::Num; Index++)
	{
		RenderThreadIdle += GRenderThreadIdle[Index];
		GRenderThreadIdle[Index] = 0;
		GRenderThreadNumIdle[Index] = 0;
	}

	SET_CYCLE_COUNTER(STAT_RenderingIdleTime, RenderThreadIdle);	
	GRenderThreadTime	= (ThreadTime > RenderThreadIdle) ? (ThreadTime - RenderThreadIdle) : ThreadTime;
}

void FSlateRHIRenderer::DrawWindows( FSlateDrawBuffer& WindowDrawBuffer )
{
	if (IsInSlateThread())
	{
		EnqueuedWindowDrawBuffer = &WindowDrawBuffer;
	}
	else
	{
		DrawWindows_Private(WindowDrawBuffer);
	}
}

void FSlateRHIRenderer::DrawWindows()
{
	if (EnqueuedWindowDrawBuffer)
	{
		DrawWindows_Private(*EnqueuedWindowDrawBuffer);
		EnqueuedWindowDrawBuffer = NULL;
	}
}

static void EndDrawingWindows( FSlateDrawBuffer* DrawBuffer, FSlateRHIRenderingPolicy& Policy )
{
#if USE_MAX_DRAWBUFFERS
	DrawBuffer->Unlock();
#else
	delete DrawBuffer;
#endif

	Policy.EndDrawingWindows();
}


void FSlateRHIRenderer::PrepareToTakeScreenshot(const FIntRect& Rect, TArray<FColor>* OutColorData)
{
	check(OutColorData);

	bTakingAScreenShot = true;
	ScreenshotRect = Rect;
	OutScreenshotData = OutColorData;
}

/** 
 * Creates necessary resources to render a window and sends draw commands to the rendering thread
 *
 * @param WindowDrawBuffer	The buffer containing elements to draw 
 */
void FSlateRHIRenderer::DrawWindows_Private( FSlateDrawBuffer& WindowDrawBuffer )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateRenderingGTTime );

	check( IsThreadSafeForSlateRendering() );

	// Enqueue a command to unlock the draw buffer after all windows have been drawn
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( SlateBeginDrawingWindowsCommand, 
		FSlateRHIRenderingPolicy&, Policy, *RenderingPolicy,
	{
		Policy.BeginDrawingWindows();
	});

	// Clear accessed UTexture objects from the previous frame
	ResourceManager->ClearAccessedUTextures();

	// Update texture atlases if needed
	ResourceManager->UpdateTextureAtlases();

	// Iterate through each element list and set up an RHI window for it if needed
	TArray<FSlateWindowElementList>& WindowElementLists = WindowDrawBuffer.GetWindowElementLists();
	for( int32 ListIndex = 0; ListIndex < WindowElementLists.Num(); ++ListIndex )
	{
		FSlateWindowElementList& ElementList = WindowElementLists[ListIndex];

		TSharedPtr<SWindow> Window = ElementList.GetWindow();

		if( Window.IsValid() )
		{
			const FVector2D WindowSize = Window->GetSizeInScreen();
			if ( WindowSize.X > 0 && WindowSize.Y > 0 )
			{
				// Add all elements for this window to the element batcher
				ElementBatcher->AddElements( ElementList.GetDrawElements() );

				// Update the font cache with new text after elements are batched
				FontCache->UpdateCache();

				bool bRequiresStencilTest = false;
				bool bLockToVsync = false;
				bool temp = false;
				// Populate the element list with batched vertices and indicies
				ElementBatcher->FillBatchBuffers( ElementList, temp );
				bLockToVsync = ElementBatcher->RequiresVsync();

				if( !GIsEditor )
				{
					static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
					bLockToVsync = bLockToVsync || (CVar->GetInt() != 0);
				}

				// All elements for this window have been batched and rendering data updated
				ElementBatcher->ResetBatches();

				// The viewport had better exist at this point  
				FViewportInfo* ViewInfo = WindowToViewportInfo.FindChecked( Window.Get() );

				// Resize the viewport if needed
				ConditionalResizeViewport( ViewInfo, ViewInfo->DesiredWidth, ViewInfo->DesiredHeight, IsViewportFullscreen( *Window )  );

				if( bRequiresStencilTest )
				{	
					ViewInfo->ConditionallyUpdateDepthBuffer(bRequiresStencilTest);
				}

				// Tell the rendering thread to draw the windows
				{
					struct FSlateDrawWindowCommandParams
					{
						FSlateRHIRenderer* Renderer;
						FSlateRHIRenderer::FViewportInfo* ViewportInfo;
						FSlateWindowElementList* WindowElementList;
						SWindow* SlateWindow;
						bool bLockToVsync;
						FSimpleDelegate MarkWindowAsDrawn;
					} Params;

					Params.Renderer = this;
					Params.ViewportInfo = ViewInfo;
					Params.WindowElementList = &ElementList;
					Params.bLockToVsync = bLockToVsync;

					if( !Window->HasEverBeenDrawn() )
					{
						Params.MarkWindowAsDrawn = Window->MakeMarkWindowAsDrawnDelegate();
					}

					// NOTE: We pass a raw pointer to the SWindow so that we don't have to use a thread-safe weak pointer in
					// the FSlateWindowElementList structure
					Params.SlateWindow = Window.Get();

					ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( SlateDrawWindowsCommand, 
						FSlateDrawWindowCommandParams, Params, Params,
					{
						Params.Renderer->DrawWindow_RenderThread( *Params.ViewportInfo, *Params.WindowElementList, Params.bLockToVsync );
						Params.MarkWindowAsDrawn.ExecuteIfBound();
					});

					if ( bTakingAScreenShot )
					{
						ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(SlateCaptureScreenshotCommand,
							FSlateDrawWindowCommandParams, Params, Params,
							FIntRect, ScreenshotRect, ScreenshotRect,
							TArray<FColor>*, OutScreenshotData, OutScreenshotData,
						{
							FTexture2DRHIRef BackBuffer = RHIGetViewportBackBuffer(Params.ViewportInfo->ViewportRHI);
							RHIReadSurfaceData(BackBuffer, ScreenshotRect, *OutScreenshotData, FReadSurfaceDataFlags());
						});

						FlushRenderingCommands();

						bTakingAScreenShot = false;
						OutScreenshotData = NULL;
					}
				}
			}
		}
		else
		{
			ensureMsgf( false, TEXT("Window isnt valid but being drawn!") );
		}
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER( SlateEndDrawingWindowsCommand, 
		FSlateDrawBuffer*, DrawBuffer, &WindowDrawBuffer,
		FSlateRHIRenderingPolicy&, Policy, *RenderingPolicy,
	{
		EndDrawingWindows( DrawBuffer, Policy );
	});

	// flush the cache if needed
	FontCache->ConditionalFlushCache();

	ElementBatcher->ResetStats();
}

void FSlateRHIRenderer::CopyWindowsToDrawBuffer(const TArray<FString>& KeypressBuffer)
{
#if !PLATFORM_WINDOWS && !PLATFORM_MAC
	ensureMsg(0, TEXT("This functionality is not valid for this platform"));
	return;
#endif

	SCOPE_CYCLE_COUNTER(STAT_GenerateCaptureBuffer);

	const FIntRect ScaledVirtualScreen = CrashTrackerResource->GetVirtualScreen().Scale(CrashTrackerConstants::ScreenScaling);
	const FIntPoint ScaledVirtualScreenPos = ScaledVirtualScreen.Min;
	const FIntPoint ScaledVirtualScreenSize = ScaledVirtualScreen.Size();
	
	// setup state
	struct FSetupWindowStateContext
	{
		FSlateCrashReportResource* CrashReportResource;
		FIntRect IntermediateBufferSize;
	};
	FSetupWindowStateContext SetupWindowStateContext =
	{
		CrashTrackerResource,
		ScaledVirtualScreen
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		SetupWindowState,
		FSetupWindowStateContext,Context,SetupWindowStateContext,
	{
		RHISetRenderTarget(Context.CrashReportResource->GetBuffer(), FTextureRHIRef());
		
		RHISetViewport(0, 0, 0.0f, Context.IntermediateBufferSize.Width(), Context.IntermediateBufferSize.Height(), 1.0f);
		RHISetBlendState(TStaticBlendState<>::GetRHI());
		RHISetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHISetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

		RHIClear(true, FLinearColor::Gray, false, 0.f, false, 0x00, FIntRect());
	});

	// draw windows to buffer
	TArray< TSharedRef<SWindow> > OutWindows;
	FSlateApplication::Get().GetAllVisibleWindowsOrdered(OutWindows);

	for (int32 i = 0; i < OutWindows.Num(); ++i)
	{
		TSharedPtr<SWindow> WindowPtr = OutWindows[i];
		SWindow* Window = WindowPtr.Get();
		FViewportInfo* ViewportInfo = WindowToViewportInfo.FindChecked(Window);

		const FSlateRect SlateWindowRect = Window->GetRectInScreen();
		const FVector2D WindowSize = SlateWindowRect.GetSize();
		if ( WindowSize.X > 0 && WindowSize.Y > 0 )
		{
			const FIntRect ScaledWindowRect = FIntRect(SlateWindowRect.Left, SlateWindowRect.Top, SlateWindowRect.Right, SlateWindowRect.Bottom).Scale(CrashTrackerConstants::ScreenScaling) -
				ScaledVirtualScreenPos;

			struct FDrawWindowToBufferContext
			{
				FViewportInfo* InViewportInfo;
				FIntRect WindowRect;
				FIntRect IntermediateBufferSize;
			};
			FDrawWindowToBufferContext DrawWindowToBufferContext =
			{
				ViewportInfo,
				ScaledWindowRect,
				ScaledVirtualScreen
			};

			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				DrawWindowToBuffer,
				FDrawWindowToBufferContext,Context,DrawWindowToBufferContext,
			{
				DrawNormalizedScreenQuad(
					Context.WindowRect.Min.X, Context.WindowRect.Min.Y,
					0, 0,
					Context.WindowRect.Width(), Context.WindowRect.Height(),
					1, 1,
					FIntPoint(Context.IntermediateBufferSize.Width(), Context.IntermediateBufferSize.Height()),
					RHIGetViewportBackBuffer(Context.InViewportInfo->ViewportRHI));
			});
		}
	}

	// draw mouse cursor and keypresses
	const FVector2D MouseCursorLocation = FSlateApplication::Get().GetCursorPos();
	const FIntPoint ScaledCursorLocation = FIntPoint(MouseCursorLocation.X * CrashTrackerConstants::ScreenScaling, MouseCursorLocation.Y * CrashTrackerConstants::ScreenScaling) -
		ScaledVirtualScreenPos;

	FSlateWindowElementList* WindowElementList = CrashTrackerResource->GetNextElementList();
	*WindowElementList = FSlateWindowElementList(TSharedPtr<SWindow>());

	FSlateDrawElement::MakeBox(
		*WindowElementList,
		0,
		FPaintGeometry(ScaledCursorLocation, FVector2D(32, 32), 1.f),
		FCoreStyle::Get().GetBrush("CrashTracker.Cursor"),
		FSlateRect(0, 0, ScaledVirtualScreenSize.X, ScaledVirtualScreenSize.Y));
	
	for (int32 i = 0; i < KeypressBuffer.Num(); ++i)
	{
		FSlateDrawElement::MakeText(
			*WindowElementList,
			0,
			FPaintGeometry(FVector2D(10, 10 + i * 30), FVector2D(300, 30), 1.f),
			KeypressBuffer[i],
			FCoreStyle::Get().GetFontStyle(TEXT("CrashTracker.Font")),
			FSlateRect(0, 0, ScaledVirtualScreenSize.X, ScaledVirtualScreenSize.Y));
	}
	
	ElementBatcher->AddElements(WindowElementList->GetDrawElements());
	bool bRequiresStencilTest = false;
	ElementBatcher->FillBatchBuffers(*WindowElementList, bRequiresStencilTest );
	check(!bRequiresStencilTest);
	ElementBatcher->ResetBatches();
	
	struct FWriteMouseCursorAndKeyPressesContext
	{
		FSlateCrashReportResource* CrashReportResource;
		FIntRect IntermediateBufferSize;
		FSlateRHIRenderingPolicy* RenderPolicy;
		FSlateWindowElementList* SlateElementList;
		FIntPoint ViewportSize;
	};
	FWriteMouseCursorAndKeyPressesContext WriteMouseCursorAndKeyPressesContext =
	{
		CrashTrackerResource,
		ScaledVirtualScreen,
		RenderingPolicy.Get(),
		WindowElementList,
		ScaledVirtualScreenSize
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		WriteMouseCursorAndKeyPresses,
		FWriteMouseCursorAndKeyPressesContext, Context, WriteMouseCursorAndKeyPressesContext,
	{
		RHISetBlendState(TStaticBlendState<CW_RGBA,BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_Zero,BF_One>::GetRHI());
		
		Context.RenderPolicy->UpdateBuffers(*Context.SlateElementList);
		if( Context.SlateElementList->GetRenderBatches().Num() > 0 )
		{
			FTexture2DRHIRef UnusedTargetTexture;
			FSlateRenderTarget UnusedTarget( UnusedTargetTexture, FIntPoint::ZeroValue );

			Context.RenderPolicy->DrawElements(Context.ViewportSize, UnusedTarget, CreateProjectionMatrix(Context.ViewportSize.X, Context.ViewportSize.Y), Context.SlateElementList->GetRenderBatches());
		}
	});

	// copy back to the cpu
	struct FReadbackFromIntermediateBufferContext
	{
		FSlateCrashReportResource* CrashReportResource;
		FIntRect IntermediateBufferSize;
	};
	FReadbackFromIntermediateBufferContext ReadbackFromIntermediateBufferContext =
	{
		CrashTrackerResource,
		ScaledVirtualScreen
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadbackFromIntermediateBuffer,
		FReadbackFromIntermediateBufferContext,Context,ReadbackFromIntermediateBufferContext,
	{
		RHICopyToResolveTarget(
			Context.CrashReportResource->GetBuffer(),
			Context.CrashReportResource->GetReadbackBuffer(),
			false,
			FResolveParams());
	});
}


void FSlateRHIRenderer::MapCrashTrackerBuffer(void** OutImageData, int32* OutWidth, int32* OutHeight)
{
	struct FReadbackFromStagingBufferContext
	{
		FSlateCrashReportResource* CrashReportResource;
		void** OutData;
		int32* OutputWidth;
		int32* OutputHeight;
	};
	FReadbackFromStagingBufferContext ReadbackFromStagingBufferContext =
	{
		CrashTrackerResource,
		OutImageData,
		OutWidth,
		OutHeight
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadbackFromStagingBuffer,
		FReadbackFromStagingBufferContext,Context,ReadbackFromStagingBufferContext,
	{
		SCOPE_CYCLE_COUNTER(STAT_MapStagingBuffer);
		RHIMapStagingSurface(Context.CrashReportResource->GetReadbackBuffer(), *Context.OutData, *Context.OutputWidth, *Context.OutputHeight);
		Context.CrashReportResource->SwapTargetReadbackBuffer();
	});
}

void FSlateRHIRenderer::UnmapCrashTrackerBuffer()
{
	struct FReadbackFromStagingBufferContext
	{
		FSlateCrashReportResource* CrashReportResource;
	};
	FReadbackFromStagingBufferContext ReadbackFromStagingBufferContext =
	{
		CrashTrackerResource
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadbackFromStagingBuffer,
		FReadbackFromStagingBufferContext,Context,ReadbackFromStagingBufferContext,
	{
		SCOPE_CYCLE_COUNTER(STAT_UnmapStagingBuffer);
		RHIUnmapStagingSurface(Context.CrashReportResource->GetReadbackBuffer());
	});
}

FIntPoint FSlateRHIRenderer::GenerateDynamicImageResource(const FName InTextureName)
{
	TSharedPtr<FSlateRHIResourceManager::FDynamicTextureResource> TextureResource = ResourceManager->MakeDynamicTextureResource(false, true, InTextureName.ToString(), InTextureName, NULL);
	return TextureResource.IsValid() ? TextureResource->Proxy->ActualSize : FIntPoint( 0, 0 );
}

bool FSlateRHIRenderer::GenerateDynamicImageResource( FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes )
{
	TSharedPtr<FSlateRHIResourceManager::FDynamicTextureResource> TextureResource = ResourceManager->MakeDynamicTextureResource( ResourceName, Width, Height, Bytes );
	return TextureResource.IsValid();
}

/**
 * Gives the renderer a chance to wait for any render commands to be completed before returning/
 */
void FSlateRHIRenderer::FlushCommands() const
{
	check(!IsInSlateThread());
	FlushRenderingCommands();
}

/**
 * Gives the renderer a chance to synchronize with another thread in the event that the renderer runs 
 * in a multi-threaded environment.  This function does not return until the sync is complete
 */
void FSlateRHIRenderer::Sync() const
{
	// Sync game and render thread. Either total sync or allowing one frame lag.
	static FFrameEndSync FrameEndSync;
	static auto CVarAllowOneFrameThreadLag = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OneFrameThreadLag"));
	FrameEndSync.Sync( CVarAllowOneFrameThreadLag->GetValueOnAnyThread() != 0 );
}

void FSlateRHIRenderer::ReloadTextureResources()
{
	ResourceManager->ReloadTextures();
}

void FSlateRHIRenderer::LoadUsedTextures()
{
	if ( ResourceManager.IsValid() )
	{
		ResourceManager->LoadUsedTextures();
	}
}

void FSlateRHIRenderer::LoadStyleResources( const ISlateStyle& Style )
{
	if ( ResourceManager.IsValid() )
	{
		ResourceManager->LoadStyleResources( Style );
	}
}

void FSlateRHIRenderer::DisplayTextureAtlases()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.SizingRule( ESizingRule::Autosized )
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.Title( FText::GetEmpty() )
		[
			SNew( SBorder )
			.BorderImage( FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder") )
			[
				ResourceManager->CreateTextureDisplayWidget()
			]
		];

	FSlateApplication::Get().AddWindow( Window );
}

void FSlateRHIRenderer::ReleaseDynamicResource( const FSlateBrush& InBrush )
{
	ResourceManager->ReleaseDynamicResource( InBrush );
}

void* FSlateRHIRenderer::GetViewportResource( const SWindow& Window )
{
	check(IsThreadSafeForSlateRendering());

	FViewportInfo** InfoPtr = WindowToViewportInfo.Find( &Window );

	if( InfoPtr )
	{
		FViewportInfo* ViewportInfo = *InfoPtr;

		// Create the viewport if it doesnt exist
		if( !IsValidRef(ViewportInfo->ViewportRHI) )
		{
			// Sanity check dimensions
			checkf(ViewportInfo->Width <= MAX_VIEWPORT_SIZE && ViewportInfo->Height <= MAX_VIEWPORT_SIZE, TEXT("Invalid window with Width=%u and Height=%u"), ViewportInfo->Width, ViewportInfo->Height);

			const bool bFullscreen = IsViewportFullscreen( Window );

			ViewportInfo->ViewportRHI = RHICreateViewport( ViewportInfo->OSWindow, ViewportInfo->Width, ViewportInfo->Height, bFullscreen );
		}

		return &ViewportInfo->ViewportRHI;
	}
	else
	{
		return NULL;
	}
}

void FSlateRHIRenderer::SetColorVisionDeficiencyType( uint32 Type )
{
	GSlateShaderColorVisionDeficiencyType = Type;
}

bool FSlateRHIRenderer::AreShadersInitialized() const
{
#if WITH_EDITORONLY_DATA
	return AreGlobalShadersComplete(TEXT("SlateElement"));
#else
	return true;
#endif
}

void FSlateRHIRenderer::InvalidateAllViewports()
{
	for( TMap< const SWindow*, FViewportInfo*>::TIterator It(WindowToViewportInfo); It; ++It )
	{
		It.Value()->ViewportRHI = NULL;
	}
}

void FSlateRHIRenderer::RequestResize( const TSharedPtr<SWindow>& Window, uint32 NewWidth, uint32 NewHeight )
{
	check( IsThreadSafeForSlateRendering() );

	FViewportInfo* ViewInfo = WindowToViewportInfo.FindRef( Window.Get() );

	if( ViewInfo )
	{
		ViewInfo->DesiredWidth = NewWidth;
		ViewInfo->DesiredHeight = NewHeight;
	}
}
