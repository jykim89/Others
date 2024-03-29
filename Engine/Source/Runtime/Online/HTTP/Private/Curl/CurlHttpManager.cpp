// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "HttpPrivatePCH.h"
#include "CurlHttpManager.h"
#include "CurlHttp.h"

#if WITH_LIBCURL

CURLM * FCurlHttpManager::GMultiHandle = NULL;

void FCurlHttpManager::InitCurl()
{
	if (GMultiHandle != NULL)
	{
		UE_LOG(LogInit, Warning, TEXT("Already initialized multi handle"));
		return;
	}

	CURLcode InitResult = curl_global_init_mem(CURL_GLOBAL_ALL, CurlMalloc, CurlFree, CurlRealloc, CurlStrdup, CurlCalloc);
	if (InitResult == 0)
	{
		curl_version_info_data * VersionInfo = curl_version_info(CURLVERSION_NOW);
		if (VersionInfo)
		{
			UE_LOG(LogInit, Log, TEXT("Using libcurl %s"), ANSI_TO_TCHAR(VersionInfo->version));
			UE_LOG(LogInit, Log, TEXT(" - built for %s"), ANSI_TO_TCHAR(VersionInfo->host));

			if (VersionInfo->features & CURL_VERSION_SSL)
			{
				UE_LOG(LogInit, Log, TEXT(" - supports SSL with %s"), ANSI_TO_TCHAR(VersionInfo->ssl_version));
			}
			else
			{
				// No SSL
				UE_LOG(LogInit, Log, TEXT(" - NO SSL SUPPORT!"));
			}

			if (VersionInfo->features & CURL_VERSION_LIBZ)
			{
				UE_LOG(LogInit, Log, TEXT(" - supports HTTP deflate (compression) using libz %s"), ANSI_TO_TCHAR(VersionInfo->libz_version));
			}

			UE_LOG(LogInit, Log, TEXT(" - other features:"));

#define PrintCurlFeature(Feature)	\
			if (VersionInfo->features & Feature) \
			{ \
			UE_LOG(LogInit, Log, TEXT("     %s"), TEXT(#Feature));	\
			}

			PrintCurlFeature(CURL_VERSION_SSL);
			PrintCurlFeature(CURL_VERSION_LIBZ);

			PrintCurlFeature(CURL_VERSION_DEBUG);
			PrintCurlFeature(CURL_VERSION_IPV6);
			PrintCurlFeature(CURL_VERSION_ASYNCHDNS);
			PrintCurlFeature(CURL_VERSION_LARGEFILE);
			PrintCurlFeature(CURL_VERSION_IDN);
			PrintCurlFeature(CURL_VERSION_CONV);
			PrintCurlFeature(CURL_VERSION_TLSAUTH_SRP);
#undef PrintCurlFeature
		}

		GMultiHandle = curl_multi_init();
		if (NULL == GMultiHandle)
		{
			UE_LOG(LogInit, Fatal, TEXT("Could not initialize create libcurl multi handle! HTTP transfers will not function properly."));
		}

		UE_LOG(LogInit, Log, TEXT("Libcurl will %s"), FParse::Param(FCommandLine::Get(), TEXT("reuseconn")) ? TEXT("reuse connections") : TEXT("NOT reuse connections"));
	}
	else
	{
		UE_LOG(LogInit, Fatal, TEXT("Could not initialize libcurl (result=%d), HTTP transfers will not function properly."), (int32)InitResult);
	}
}

void FCurlHttpManager::ShutdownCurl()
{
	if (NULL != GMultiHandle)
	{
		curl_multi_cleanup(GMultiHandle);
		GMultiHandle = NULL;
	}

	curl_global_cleanup();
}

FCurlHttpManager::FCurlHttpManager()
	:	FHttpManager()
	,	MultiHandle(GMultiHandle)
	,	LastRunningRequests(0)
{
	check(MultiHandle);
}

// note that we cannot call parent implementation because lock might be possible non-multiple
void FCurlHttpManager::AddRequest(TSharedRef<class IHttpRequest> Request)
{
	FScopeLock ScopeLock(&RequestLock);

	Requests.AddUnique(Request);

	FCurlHttpRequest* CurlRequest = static_cast< FCurlHttpRequest* >( &Request.Get() );
	HandlesToRequests.Add(CurlRequest->GetEasyHandle(), Request);
}

// note that we cannot call parent implementation because lock might be possible non-multiple
void FCurlHttpManager::RemoveRequest(TSharedRef<class IHttpRequest> Request)
{
	FScopeLock ScopeLock(&RequestLock);

	// Keep track of requests that have been removed to be destroyed later
	PendingDestroyRequests.AddUnique(FRequestPendingDestroy(DeferredDestroyDelay,Request));

	FCurlHttpRequest* CurlRequest = static_cast< FCurlHttpRequest* >( &Request.Get() );
	HandlesToRequests.Remove(CurlRequest->GetEasyHandle());

	Requests.RemoveSingle(Request);
}

bool FCurlHttpManager::Tick(float DeltaSeconds)
{
	check(MultiHandle);
	if (Requests.Num() > 0)
	{
		FScopeLock ScopeLock(&RequestLock);

		int RunningRequests = -1;
		curl_multi_perform(MultiHandle, &RunningRequests);

		// read more info if number of requests changed or if there's zero running
		// (note that some requests might have never be "running" from libcurl's point of view)
		if (RunningRequests == 0 || RunningRequests != LastRunningRequests)
		{
			for(;;)
			{
				int MsgsStillInQueue = 0;	// may use that to impose some upper limit we may spend in that loop
				CURLMsg * Message = curl_multi_info_read(MultiHandle, &MsgsStillInQueue);

				if (Message == NULL)
				{
					break;
				}

				// find out which requests have completed
				if (Message->msg == CURLMSG_DONE)
				{
					CURL* CompletedHandle = Message->easy_handle;
					TSharedRef<IHttpRequest> * RequestRefPtr = HandlesToRequests.Find(CompletedHandle);
					if (RequestRefPtr)
					{
						FCurlHttpRequest* CurlRequest = static_cast< FCurlHttpRequest* >( &RequestRefPtr->Get() );
						CurlRequest->MarkAsCompleted(Message->data.result);

						UE_LOG(LogHttp, Verbose, TEXT("Request %p (easy handle:%p) has completed (code:%d) and has been marked as such"), CurlRequest, CompletedHandle, (int32)Message->data.result);
					}
					else
					{
						UE_LOG(LogHttp, Warning, TEXT("Could not find mapping for completed request (easy handle: %p)"), CompletedHandle);
					}
				}
			}
		}

		LastRunningRequests = RunningRequests;
	}

	// we should be outside scope lock here to be able to call parent!
	return FHttpManager::Tick(DeltaSeconds);
}

#endif //WITH_LIBCURL
