// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoviePlayer.h"

#include "AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>

DECLARE_LOG_CATEGORY_EXTERN(LogWindowsMoviePlayer, Log, All);


/** The Movie Streamer is what is registered to the global movie player for Windows */
class FMediaFoundationMovieStreamer : public IMovieStreamer
{
public:
	FMediaFoundationMovieStreamer();
	~FMediaFoundationMovieStreamer();

	/** IMovieStreamer interface */
	virtual void Init(const TArray<FString>& MoviePaths) OVERRIDE;
	virtual void ForceCompletion() OVERRIDE;
	virtual bool Tick(float DeltaTime) OVERRIDE;
	virtual TSharedPtr<class ISlateViewport> GetViewportInterface() OVERRIDE
	{
		return MovieViewport;
	}
	virtual float GetAspectRatio() const OVERRIDE
	{
		return (float)MovieViewport->GetSize().X / (float)MovieViewport->GetSize().Y;
	}
	virtual void Cleanup() OVERRIDE;

private:
	/** Opens up the next movie in the movie path queue */
	void OpenNextMovie();
	/** Closes the currently running video */
	void CloseMovie();
	/** Cleans up rendering resources once movies are done playing */
	void CleanupRenderingResources();

private:
	/** A list of all the stored movie paths we have enqueued for playing */
	TArray<FString> StoredMoviePaths;

	/** Texture and viewport data for displaying to Slate */
	TArray<uint8> TextureData;
	TSharedPtr<FMovieViewport> MovieViewport;
	TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe> Texture;

	/**
	 * List of textures pending deletion, need to keep this list because we can't immediately
	 * destroy them since they could be getting used on the main thread
	 */
	TArray<TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe>> TextureFreeList;

	/** The video player and sample grabber for use of Media Foundation */
	class FVideoPlayer* VideoPlayer;
    class FSampleGrabberCallback* SampleGrabberCallback;
};



/** The video player is the class which handles all the loading and playing of videos */
class FVideoPlayer : public IMFAsyncCallback
{
public:
	FVideoPlayer()
		: RefCount(1)
		, MediaSession(NULL)
		, MediaSource(NULL)
		, MovieIsFinished(0)
		, CloseIsPosted(0) {}
	virtual ~FVideoPlayer()
	{
		check(MediaSession == NULL);
		Shutdown();
	}
	
	// IUnknown interface
	STDMETHODIMP QueryInterface(REFIID RefID, void** Object);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// IMFAsyncCallback interface
	STDMETHODIMP GetParameters(unsigned long*, unsigned long*) {return E_NOTIMPL;}
	STDMETHODIMP Invoke(IMFAsyncResult* AsyncResult);

	/** Opens the specified file, and returns the video dimensions */
	FIntPoint OpenFile(const FString& FilePath, class FSampleGrabberCallback* SampleGrabberCallback);
	/** Starts the video player playback */
	void StartPlayback();
	/** Shuts the video player down, destroying all threads with it */
	void Shutdown();

	/** True if the movie is still playing and rendering frames */
	bool MovieIsRunning() const {return MovieIsFinished.GetValue() == 0;}
	
private:
	/** Sets up the topology of all the nodes in the media session, returning the video dimensions */
	FIntPoint SetPlaybackTopology(class FSampleGrabberCallback* SampleGrabberCallback);
	/** Adds a single audio or video stream to the passed in topology, returning video dimensions if applicable */
	FIntPoint AddStreamToTopology(IMFTopology* Topology, IMFPresentationDescriptor* PresentationDesc, IMFStreamDescriptor* StreamDesc, class FSampleGrabberCallback* SampleGrabberCallback);

private:
	/** Media Foundation boilerplate */
	int32 RefCount;

	/** The media session which handles all playback */
	IMFMediaSession* MediaSession;
	/** The source, which reads in the data from the file */
	IMFMediaSource* MediaSource;

	/** This counter lets the ticking thread know that the current movie finished */
	FThreadSafeCounter MovieIsFinished;
	/** This counter locks the ticking thread while all Media Foundation threads shutdown */
	FThreadSafeCounter CloseIsPosted;
};


/** The sample grabber callback is for pulling frames off the video stream to render to texture */
class FSampleGrabberCallback : public IMFSampleGrabberSinkCallback
{
public:
	FSampleGrabberCallback(TArray<uint8>& InTextureData)
		: RefCount(1)
		, TextureData(InTextureData) {}

	// IUnknown interface
	STDMETHODIMP QueryInterface(REFIID RefID, void** Object);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// IMFClockStateSink interface
	STDMETHODIMP OnClockStart(MFTIME SystemTime, LONGLONG llClockStartOffset) {return S_OK;}
	STDMETHODIMP OnClockStop(MFTIME SystemTime) {return S_OK;}
	STDMETHODIMP OnClockPause(MFTIME SystemTime) {return S_OK;}
	STDMETHODIMP OnClockRestart(MFTIME SystemTime) {return S_OK;}
	STDMETHODIMP OnClockSetRate(MFTIME SystemTime, float flRate) {return S_OK;}
	
	// IMFSampleGrabberSinkCallback interface
	STDMETHODIMP OnSetPresentationClock(IMFPresentationClock* Clock) {return S_OK;}
	STDMETHODIMP OnProcessSample(REFGUID MajorMediaType, DWORD SampleFlags,
        LONGLONG SampleTime, LONGLONG SampleDuration, const BYTE* SampleBuffer,
        DWORD SampleSize);
	STDMETHODIMP OnShutdown() {return S_OK;}

	/** True if we have a new sample for readback */
	bool GetIsSampleReadyToUpdate() const;

	/** Tells this callback that we need a new sample to read back */
	void SetNeedNewSample();

private:
	/** Media Foundation boilerplate */
	int32 RefCount;

	/** Counter which determines when a sample can be safely read back */
	FThreadSafeCounter VideoSampleReady;
	/** The texture data sample we read back to */
	TArray<uint8>& TextureData;
};

#include "HideWindowsPlatformTypes.h"
