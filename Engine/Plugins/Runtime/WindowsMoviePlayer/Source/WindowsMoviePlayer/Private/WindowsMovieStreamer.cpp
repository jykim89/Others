// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "WindowsMoviePlayerPrivatePCH.h"

#include "Slate.h"
#include "RenderingCommon.h"
#include "Slate/SlateTextures.h"
#include "MoviePlayer.h"

#pragma comment(lib, "shlwapi")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfplay")
#pragma comment(lib, "mfuuid")

#include "AllowWindowsPlatformTypes.h"

#include <windows.h>
#include <shlwapi.h>
#include <mfapi.h>
#include <mfidl.h>



DEFINE_LOG_CATEGORY(LogWindowsMoviePlayer);

FMediaFoundationMovieStreamer::FMediaFoundationMovieStreamer()
	: VideoPlayer(NULL)
	, SampleGrabberCallback(NULL)
	, TextureData()
{
	MovieViewport = MakeShareable(new FMovieViewport());
}

FMediaFoundationMovieStreamer::~FMediaFoundationMovieStreamer()
{
	CloseMovie();
	CleanupRenderingResources();

	FlushRenderingCommands();
	TextureFreeList.Empty();
}

void FMediaFoundationMovieStreamer::Init(const TArray<FString>& MoviePaths)
{
	if (MoviePaths.Num() == 0)
	{
		return;
	}

	StoredMoviePaths = MoviePaths;

	OpenNextMovie();
}

void FMediaFoundationMovieStreamer::ForceCompletion()
{
	CloseMovie();
}

bool FMediaFoundationMovieStreamer::Tick(float DeltaTime)
{
	FSlateTexture2DRHIRef* CurrentTexture = Texture.Get();

	if ((CurrentTexture != nullptr) && SampleGrabberCallback->GetIsSampleReadyToUpdate())
	{
		check( IsInRenderingThread() );

		if( !CurrentTexture->IsInitialized() )
		{
			CurrentTexture->InitResource();
		}

		uint32 Stride;
		uint8* DestTextureData = (uint8*)RHILockTexture2D( CurrentTexture->GetTypedResource(), 0, RLM_WriteOnly, Stride, false );
		FMemory::Memcpy( DestTextureData, TextureData.GetData(), TextureData.Num() );
		RHIUnlockTexture2D( CurrentTexture->GetTypedResource(), 0, false );

		SampleGrabberCallback->SetNeedNewSample();
	}

	if (!VideoPlayer->MovieIsRunning())
	{
		CloseMovie();
		if (StoredMoviePaths.Num() > 0)
		{
			OpenNextMovie();
		}
		else
		{
			return true;
		}
	}

	return false;
}

void FMediaFoundationMovieStreamer::Cleanup()
{
	CleanupRenderingResources();
}

void FMediaFoundationMovieStreamer::OpenNextMovie()
{
	check(StoredMoviePaths.Num() > 0);
	FString MoviePath = FPaths::GameContentDir() + TEXT("Movies/") + StoredMoviePaths[0];
	StoredMoviePaths.RemoveAt(0);

    SampleGrabberCallback = new FSampleGrabberCallback(TextureData);
	
	VideoPlayer = new FVideoPlayer();
	FIntPoint VideoDimensions = VideoPlayer->OpenFile(MoviePath, SampleGrabberCallback);

	if( VideoDimensions != FIntPoint::ZeroValue )
	{
		TextureData.Empty();
		TextureData.AddZeroed(VideoDimensions.X*VideoDimensions.Y*GPixelFormats[PF_B8G8R8A8].BlockBytes);

		if( TextureFreeList.Num() > 0 )
		{
			Texture = TextureFreeList.Pop();

			if( Texture->GetWidth() != VideoDimensions.X || Texture->GetHeight() != VideoDimensions.Y )
			{
				ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(UpdateMovieTexture,
					FSlateTexture2DRHIRef*, TextureRHIRef, Texture.Get(),
					FIntPoint, InVideoDimensions, VideoDimensions,
				{
					TextureRHIRef->Resize( InVideoDimensions.X, InVideoDimensions.Y );
				});
			}
		}
		else
		{
			const bool bCreateEmptyTexture = true;
			Texture = MakeShareable(new FSlateTexture2DRHIRef(VideoDimensions.X, VideoDimensions.Y, PF_B8G8R8A8, NULL, TexCreate_Dynamic, bCreateEmptyTexture));

			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(InitMovieTexture,
				FSlateTexture2DRHIRef*, TextureRHIRef, Texture.Get(),
			{
				TextureRHIRef->InitResource();
			});
		}

		MovieViewport->SetTexture(Texture);
		VideoPlayer->StartPlayback();
	}
}

void FMediaFoundationMovieStreamer::CloseMovie()
{
	if (Texture.IsValid())
	{
		TextureFreeList.Add(Texture);

		MovieViewport->SetTexture(NULL);
		Texture.Reset();
	}

	if (VideoPlayer)
	{
		VideoPlayer->Shutdown();
		VideoPlayer->Release();
		VideoPlayer = NULL;
	}
	if (SampleGrabberCallback)
	{
		SampleGrabberCallback->Release();
		SampleGrabberCallback = NULL;
	}
}

void FMediaFoundationMovieStreamer::CleanupRenderingResources()
{
	for( int32 TextureIndex = 0; TextureIndex < TextureFreeList.Num(); ++TextureIndex )
	{
		BeginReleaseResource( TextureFreeList[TextureIndex].Get() );
	}
}

STDMETHODIMP FVideoPlayer::QueryInterface(REFIID RefID, void** Object)
{
	static const QITAB QITab[] =
	{
		QITABENT(FVideoPlayer, IMFAsyncCallback),
		{ 0 }
	};
	return QISearch(this, QITab, RefID, Object);
}

STDMETHODIMP_(ULONG) FVideoPlayer::AddRef()
{
	return FPlatformAtomics::InterlockedIncrement(&RefCount);
}

STDMETHODIMP_(ULONG) FVideoPlayer::Release()
{
    int32 Ref = FPlatformAtomics::InterlockedDecrement(&RefCount);
	if (Ref == 0) {delete this;}
	return Ref;
}

HRESULT FVideoPlayer::Invoke(IMFAsyncResult* AsyncResult)
{
	IMFMediaEvent* Event = NULL;
	HRESULT HResult = MediaSession->EndGetEvent(AsyncResult, &Event);
	if (FAILED(HResult))
	{
		Event->Release();
		return S_OK;
	}

	MediaEventType EventType = MEUnknown;
	HResult = Event->GetType(&EventType);
	Event->Release();
	if (FAILED(HResult))
	{
		return S_OK;
	}

	if (EventType == MESessionClosed)
	{
		MovieIsFinished.Set(1);
		CloseIsPosted.Set(1);
	}
	else
	{
		HResult = MediaSession->BeginGetEvent(this, NULL);
		if (FAILED(HResult))
		{
			return S_OK;
		}
		
		if (MovieIsRunning())
		{
			if (EventType == MEEndOfPresentation)
			{
				MovieIsFinished.Set(1);
			}
			else if( EventType == MEError )
			{
				// Unknown fatal error
				MovieIsFinished.Set(1);
				CloseIsPosted.Set(1);
			}
		}
	}
	
	return S_OK;
}

FIntPoint FVideoPlayer::OpenFile(const FString& FilePath, class FSampleGrabberCallback* SampleGrabberCallback)
{
	FIntPoint OutDimensions = FIntPoint::ZeroValue;

	HRESULT HResult = S_OK;

	{
		HResult = MFCreateMediaSession(NULL, &MediaSession);
		check(SUCCEEDED(HResult));

		HResult = MediaSession->BeginGetEvent(this, NULL);
		check(SUCCEEDED(HResult));
	}

	{
		IMFSourceResolver* SourceResolver = NULL;
		IUnknown* Source = NULL;
		HResult = MFCreateSourceResolver(&SourceResolver);
		check(SUCCEEDED(HResult));

		// Assume MP4 for now.
		FString PathPlusExt = FString::Printf( TEXT("%s.%s"), *FilePath, TEXT("mp4") );

		MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;
		HResult = SourceResolver->CreateObjectFromURL(*PathPlusExt, MF_RESOLUTION_MEDIASOURCE, NULL, &ObjectType, &Source);
		SourceResolver->Release();
		if( SUCCEEDED(HResult ) )
		{
			HResult = Source->QueryInterface(IID_PPV_ARGS(&MediaSource));
			Source->Release();
			
			OutDimensions = SetPlaybackTopology(SampleGrabberCallback);
		}
		else
		{
			UE_LOG(LogWindowsMoviePlayer, Log, TEXT("Unable to load movie: %s"), *PathPlusExt );
			MovieIsFinished.Set(1);
		}
		
	}

	return OutDimensions;
}

void FVideoPlayer::StartPlayback()
{
	check(MediaSession != NULL);

	PROPVARIANT VariantStart;
	PropVariantInit(&VariantStart);

	HRESULT HResult = MediaSession->Start(&GUID_NULL, &VariantStart);
	check(SUCCEEDED(HResult));
	PropVariantClear(&VariantStart);
}

void FVideoPlayer::Shutdown()
{
	if (MediaSession)
	{
		HRESULT HResult = MediaSession->Close();
		if (SUCCEEDED(HResult))
		{
			while (CloseIsPosted.GetValue() == 0)
			{
				FPlatformProcess::Sleep(0.010f);
			}
		}
	}

	if (MediaSource)
	{
		MediaSource->Shutdown();
		MediaSource->Release();
		MediaSource = NULL;
	}
	if (MediaSession)
	{
		MediaSession->Shutdown();
		MediaSession->Release();
		MediaSession = NULL;
	}
}

FIntPoint FVideoPlayer::SetPlaybackTopology(FSampleGrabberCallback* SampleGrabberCallback)
{
	FIntPoint OutDimensions = FIntPoint(ForceInit);

	HRESULT HResult = S_OK;
	
	IMFPresentationDescriptor* PresentationDesc = NULL;
	HResult = MediaSource->CreatePresentationDescriptor(&PresentationDesc);
	check(SUCCEEDED(HResult));
	
	IMFTopology* Topology = NULL;
	HResult = MFCreateTopology(&Topology);
	check(SUCCEEDED(HResult));

	DWORD StreamCount = 0;
	HResult = PresentationDesc->GetStreamDescriptorCount(&StreamCount);
	check(SUCCEEDED(HResult));

	for (uint32 i = 0; i < StreamCount; ++i)
	{
		BOOL bSelected = 0;
		IMFStreamDescriptor* StreamDesc = NULL;
		HResult = PresentationDesc->GetStreamDescriptorByIndex(i, &bSelected, &StreamDesc);
		check(SUCCEEDED(HResult));

		if (bSelected)
		{
			FIntPoint VideoDimensions = AddStreamToTopology(Topology, PresentationDesc, StreamDesc, SampleGrabberCallback);
			if (VideoDimensions != FIntPoint(ForceInit))
			{
				OutDimensions = VideoDimensions;
			}
		}

		StreamDesc->Release();
	}
	
	HResult = MediaSession->SetTopology(0, Topology);
	check(SUCCEEDED(HResult));

	Topology->Release();
	PresentationDesc->Release();

	return OutDimensions;
}

FIntPoint FVideoPlayer::AddStreamToTopology(IMFTopology* Topology, IMFPresentationDescriptor* PresentationDesc, IMFStreamDescriptor* StreamDesc, FSampleGrabberCallback* SampleGrabberCallback)
{
	FIntPoint OutDimensions = FIntPoint(ForceInit);

	HRESULT HResult = S_OK;
	
	IMFActivate* SinkActivate = NULL;
	{
		IMFMediaTypeHandler* Handler = NULL;
		HResult = StreamDesc->GetMediaTypeHandler(&Handler);
		check(SUCCEEDED(HResult));

		GUID MajorType;
		HResult = Handler->GetMajorType(&MajorType);
		check(SUCCEEDED(HResult));

		if (MajorType == MFMediaType_Audio)
		{
			HResult = MFCreateAudioRendererActivate(&SinkActivate);
			check(SUCCEEDED(HResult));
		}
		else if (MajorType == MFMediaType_Video)
		{
			
			IMFMediaType* OutputType = NULL;
			HResult = Handler->GetCurrentMediaType(&OutputType);
			check(SUCCEEDED(HResult));

			IMFMediaType* InputType = NULL;
			HResult = MFCreateMediaType(&InputType);

			UINT32 Width = 0, Height = 0;
			HResult = MFGetAttributeSize(OutputType, MF_MT_FRAME_SIZE, &Width, &Height);
			check(SUCCEEDED(HResult));

			HResult = InputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
			check(SUCCEEDED(HResult));
			HResult = InputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
			check(SUCCEEDED(HResult));
			HResult = InputType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
			check(SUCCEEDED(HResult));
			HResult = MFCreateSampleGrabberSinkActivate(InputType, SampleGrabberCallback, &SinkActivate);
		
			check(SUCCEEDED(HResult));
			InputType->Release();

			OutputType->Release();

			OutDimensions = FIntPoint(Width, Height);
		}

		Handler->Release();
	}
	
	IMFTopologyNode* SourceNode = NULL;
	{
		HResult = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &SourceNode);
		check(SUCCEEDED(HResult));
		HResult = SourceNode->SetUnknown(MF_TOPONODE_SOURCE, MediaSource);
		check(SUCCEEDED(HResult));
		HResult = SourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, PresentationDesc);
		check(SUCCEEDED(HResult));
		HResult = SourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, StreamDesc);
		check(SUCCEEDED(HResult));
		HResult = Topology->AddNode(SourceNode);
		check(SUCCEEDED(HResult));
	}
	
	IMFTopologyNode* OutputNode = NULL;
	{
		HResult = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &OutputNode);
		check(SUCCEEDED(HResult));
		HResult = OutputNode->SetObject(SinkActivate);
		check(SUCCEEDED(HResult));
		HResult = OutputNode->SetUINT32(MF_TOPONODE_STREAMID, 0);
		check(SUCCEEDED(HResult));
		HResult = OutputNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, 0);
		check(SUCCEEDED(HResult));
		HResult = Topology->AddNode(OutputNode);
		check(SUCCEEDED(HResult));
	}

	HResult = SourceNode->ConnectOutput(0, OutputNode, 0);
	check(SUCCEEDED(HResult));

	SourceNode->Release();
	OutputNode->Release();
	SinkActivate->Release();

	return OutDimensions;
}



STDMETHODIMP FSampleGrabberCallback::QueryInterface(REFIID RefID, void** Object)
{
	static const QITAB QITab[] =
	{
		QITABENT(FSampleGrabberCallback, IMFSampleGrabberSinkCallback),
		QITABENT(FSampleGrabberCallback, IMFClockStateSink),
		{ 0 }
	};
	return QISearch(this, QITab, RefID, Object);
}

STDMETHODIMP_(ULONG) FSampleGrabberCallback::AddRef()
{
	return FPlatformAtomics::InterlockedIncrement(&RefCount);
}

STDMETHODIMP_(ULONG) FSampleGrabberCallback::Release()
{
    int32 Ref = FPlatformAtomics::InterlockedDecrement(&RefCount);
	if (Ref == 0)
	{
		delete this;
	}

	return Ref;
}

STDMETHODIMP FSampleGrabberCallback::OnProcessSample(REFGUID MajorMediaType, DWORD SampleFlags, LONGLONG SampleTime, LONGLONG SampleDuration, const BYTE* SampleBuffer, DWORD SampleSize)
{
	if (VideoSampleReady.GetValue() == 0)
	{
		check(TextureData.Num() == SampleSize);
		FMemory::Memcpy(&TextureData[0], SampleBuffer, SampleSize);

		VideoSampleReady.Set(1);
	}

	return S_OK;
}

bool FSampleGrabberCallback::GetIsSampleReadyToUpdate() const
{
	return VideoSampleReady.GetValue() != 0;
}

void FSampleGrabberCallback::SetNeedNewSample()
{
	VideoSampleReady.Set(0);
}

#include "HideWindowsPlatformTypes.h"

