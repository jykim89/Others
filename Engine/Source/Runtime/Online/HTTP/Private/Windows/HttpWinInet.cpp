// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "HttpPrivatePCH.h"
#include "HttpWinInet.h"

bool FWinInetConnection::bStaticConnectionInitialized = false;

/** 
 * Translates an error returned from GetLastError returned from a WinInet API call. 
 *
 * @param GetLastErrorResult - error code to translate
 * @return string for the error code
 */
FString InternetTranslateError(::DWORD GetLastErrorResult)
{
	FString ErrorStr = FString::Printf(TEXT("ErrorCode: %08X. "), (uint32)GetLastErrorResult);

	HANDLE ProcessHeap = GetProcessHeap();
	if (ProcessHeap == NULL)
	{
		ErrorStr += TEXT("Call to GetProcessHeap() failed, cannot translate error... "); 
		return ErrorStr;
	}

	TCHAR FormatBuffer[1024];

	uint32 BaseLength = FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE,
		GetModuleHandle(TEXT("wininet.dll")),
		GetLastErrorResult,
		0,
		FormatBuffer,
		ARRAYSIZE(FormatBuffer),
		NULL);

	if (!BaseLength)
	{
		ErrorStr += FString::Printf(TEXT("Call to FormatMessage() failed: %08X. "), 
			(uint32)GetLastError()); 
		return ErrorStr;
	}

	ErrorStr += FString::Printf(TEXT("Desc: %s. "), FormatBuffer);

	if (GetLastErrorResult == ERROR_INTERNET_EXTENDED_ERROR)
	{
		::DWORD InetError;
		::DWORD ExtLength = 0;

		InternetGetLastResponseInfo(&InetError, NULL, &ExtLength);
		ExtLength = ExtLength+1;
		TArray<TCHAR> ExtErrMsg;
		ExtErrMsg.AddUninitialized(ExtLength);
		if (!InternetGetLastResponseInfo(&InetError, ExtErrMsg.GetTypedData(), &ExtLength))
		{
			ErrorStr += FString::Printf(TEXT("Call to InternetGetLastResponseInfo() failed: %08X. "), 
				(uint32) GetLastError());
			return ErrorStr;
		}
	}
	return ErrorStr;
}

bool FWinInetConnection::InitConnection()
{
	// Make sure previous connection is closed
	ShutdownConnection();

	UE_LOG(LogHttp, Log, TEXT("Initializing WinInet connection"));

	// Check and log the connected state so we can report early errors.
	::DWORD ConnectedFlags;
	BOOL bConnected = InternetGetConnectedState(&ConnectedFlags, 0);
	FString ConnectionType;
	ConnectionType += (ConnectedFlags & INTERNET_CONNECTION_CONFIGURED) ? TEXT("Configured ") : TEXT("");
	ConnectionType += (ConnectedFlags & INTERNET_CONNECTION_LAN) ? TEXT("LAN ") : TEXT("");
	ConnectionType += (ConnectedFlags & INTERNET_CONNECTION_MODEM) ? TEXT("Modem ") : TEXT("");
	ConnectionType += (ConnectedFlags & INTERNET_CONNECTION_MODEM_BUSY) ? TEXT("Modem Busy ") : TEXT("");
	ConnectionType += (ConnectedFlags & INTERNET_CONNECTION_OFFLINE) ? TEXT("Offline ") : TEXT("");
	ConnectionType += (ConnectedFlags & INTERNET_CONNECTION_PROXY) ? TEXT("Proxy Server ") : TEXT("");
	ConnectionType += (ConnectedFlags & INTERNET_RAS_INSTALLED) ? TEXT("RAS Installed ") : TEXT("");
	UE_LOG(LogHttp, Log, TEXT("Connected State: %s. Flags: (%s)"), 
		bConnected ? TEXT("Good") : TEXT("Bad"), *ConnectionType);

	// max simultaneous connections allowed by wininet
	::DWORD MaxServerConnections = FHttpModule::Get().GetHttpMaxConnectionsPerServer();
	InternetSetOption(NULL, INTERNET_OPTION_MAX_CONNS_PER_SERVER, (LPVOID)&MaxServerConnections, sizeof(::DWORD));
	InternetSetOption(NULL, INTERNET_OPTION_MAX_CONNS_PER_1_0_SERVER, (LPVOID)&MaxServerConnections, sizeof(::DWORD));

	if (InternetAttemptConnect(0) != ERROR_SUCCESS)
	{
		UE_LOG(LogHttp, Warning, TEXT("InternetAttemptConnect failed: %s\n"), 
			*InternetTranslateError(GetLastError()));
		return false;
	}

	// setup net connection
	InternetHandle = InternetOpen(
		*FString::Printf(TEXT("game=%s, engine=UE4, version=%d"), FApp::GetGameName(), GEngineNetVersion), 
		INTERNET_OPEN_TYPE_PRECONFIG, 
		NULL, 
		NULL, 
		INTERNET_FLAG_ASYNC);

	if (InternetHandle == NULL)
	{
		UE_LOG(LogHttp, Warning, TEXT("Failed WinHttpOpen: %s"), 
			*InternetTranslateError(GetLastError()));
		return false;
	}
	
	{
		FScopeLock ScopeLock(&FHttpManager::RequestLock);
		bStaticConnectionInitialized = true;
	}

	// Register callback to update based on WinInet connection state
	InternetSetStatusCallback(InternetHandle, InternetStatusCallbackWinInet);

	return true;
}

bool FWinInetConnection::ShutdownConnection()
{
	UE_LOG(LogHttp, Log, TEXT("Closing internet connection"));

	{
		FScopeLock ScopeLock(&FHttpManager::RequestLock);
		bStaticConnectionInitialized = false;

		if (InternetHandle != NULL)
		{
			// Clear the callback if still set
			InternetSetStatusCallback(InternetHandle, NULL);
			// shut down WinINet
			if (!InternetCloseHandle(InternetHandle))
			{
				UE_LOG(LogHttp, Warning, TEXT("InternetCloseHandle failed on the FHttpRequestWinInet: %s"), 
					*InternetTranslateError(GetLastError()));
				return false;
			}
			InternetHandle = NULL;
		}
	}

	return true;
}

/**
 * Global callback for WinInet API. Will use the dwInternetStatus and dwContext
 * fields to route the results back to the appropriate class instance if necessary.
 *
 * @param	hInternet - See WinInet API
 * @param	dwContext - See WinInet API
 * @param	dwInternetStatus - See WinInet API
 * @param	lpvStatusInformation - See WinInet API
 * @param	dwStatusInformationLength - See WinInet API
 */
void CALLBACK InternetStatusCallbackWinInet(
	HINTERNET hInternet, DWORD_PTR dwContext,
	::DWORD dwInternetStatus, LPVOID lpvStatusInformation,
	::DWORD dwStatusInformationLength)
{
	FScopeLock ScopeLock(&FHttpManager::RequestLock);

	// Ignore callbacks on module shutdown
	if ( !FWinInetConnection::bStaticConnectionInitialized )
	{
		return;
	}

	check(FWinInetConnection::Get().GetHttpManager() != NULL);

	uint32 Error = GetLastError();
	if (Error != 0 && 
		Error != ERROR_HTTP_HEADER_NOT_FOUND &&
		Error != ERROR_IO_PENDING)
	{
		// Connection has been closed an any further processing is using an invalid handle
		if (Error == ERROR_INVALID_HANDLE)
		{
			return;
		}		
	}

    // Original request/response that kicked off the connection attempt
	FHttpResponseWinInet* Response = NULL;
	FHttpRequestWinInet* Request = (FHttpRequestWinInet*)dwContext;
	// Verify request is still valid
	if (Request != NULL)
	{
		if (!FWinInetConnection::Get().GetHttpManager()->IsValidRequest(Request))
		{
			// Invalid since not in the active request list
			Request = NULL;
			UE_LOG(LogHttp, Warning, TEXT("InternetStatusCallbackWinInet: on invalid request %p. "), dwContext);
		}
	}
	// Response is always valid while there is a request in flight
	if (Request != NULL)
	{
		Response = Request->Response.Get();
	}

	switch (dwInternetStatus)
	{
	case INTERNET_STATUS_PREFETCH:
		UE_LOG(LogHttp, VeryVerbose, TEXT("STATUS_PREFETCH: %p"), dwContext);
		break;
	case INTERNET_STATUS_USER_INPUT_REQUIRED:
		UE_LOG(LogHttp, VeryVerbose, TEXT("USER_INPUT_REQUIRED: %p"), dwContext);
		break;
	case INTERNET_STATUS_DETECTING_PROXY:
		UE_LOG(LogHttp, VeryVerbose, TEXT("DETECTING_PROXY: %p"), dwContext);
		break;
	case INTERNET_STATUS_CLOSING_CONNECTION:
		UE_LOG(LogHttp, VeryVerbose, TEXT("CLOSING_CONNECTION: %p"), dwContext);
		break;
	case INTERNET_STATUS_CONNECTED_TO_SERVER:
		UE_LOG(LogHttp, VeryVerbose, TEXT("CONNECTED_TO_SERVER: %p"), dwContext);
		break;
	case INTERNET_STATUS_CONNECTING_TO_SERVER:
		UE_LOG(LogHttp, VeryVerbose, TEXT("CONNECTING_TO_SERVER: %p"), dwContext);
		break;
	case INTERNET_STATUS_CONNECTION_CLOSED:
		UE_LOG(LogHttp, VeryVerbose, TEXT("CONNECTION_CLOSED: %p"), dwContext);
		break;
	case INTERNET_STATUS_HANDLE_CLOSING:
		UE_LOG(LogHttp, VeryVerbose, TEXT("HANDLE_CLOSING: %p"), dwContext);
		break;
	case INTERNET_STATUS_HANDLE_CREATED:
		UE_LOG(LogHttp, VeryVerbose, TEXT("HANDLE_CREATED: %p"), dwContext);
		break;
	case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
		UE_LOG(LogHttp, VeryVerbose, TEXT("INTERMEDIATE_RESPONSE: %p"), dwContext);
		break;
	case INTERNET_STATUS_NAME_RESOLVED:
		UE_LOG(LogHttp, VeryVerbose, TEXT("NAME_RESOLVED: %p"), dwContext);
		break;
	case INTERNET_STATUS_RECEIVING_RESPONSE:
		UE_LOG(LogHttp, VeryVerbose, TEXT("RECEIVING_RESPONSE: %p"), dwContext);
		break;
	case INTERNET_STATUS_RESPONSE_RECEIVED:
		UE_LOG(LogHttp, VeryVerbose, TEXT("RESPONSE_RECEIVED (%d bytes): %p"), *(uint32*)lpvStatusInformation, dwContext);		
		break;
	case INTERNET_STATUS_REDIRECT:
		UE_LOG(LogHttp, VeryVerbose, TEXT("STATUS_REDIRECT: %p"), dwContext);
		break;
	case INTERNET_STATUS_REQUEST_COMPLETE:
		UE_LOG(LogHttp, VeryVerbose, TEXT("REQUEST_COMPLETE: %p"), dwContext);

		if (lpvStatusInformation != NULL)
		{
			const INTERNET_ASYNC_RESULT* AsyncResult = (const INTERNET_ASYNC_RESULT*)lpvStatusInformation;
			// callback failed so can't process response
			if (AsyncResult->dwResult == 0)
			{
				UE_LOG(LogHttp, Log, TEXT("InternetStatusCallbackWinInet request=%p AsyncResult.dwError: %08X. "), 
					dwContext, AsyncResult->dwError, *InternetTranslateError(AsyncResult->dwError));

				if (Response != NULL)
				{
					// Done processing response due to error
					FPlatformAtomics::InterlockedExchange(&Response->bIsReady, 1);
				}
				Response = NULL;
			}
		}
		
		if (Request != NULL && Request->GetStatus() == EHttpRequestStatus::Processing &&
			Response != NULL && !Response->bIsReady)
		{
			Request->ResetRequestTimeout();
			Response->ProcessResponse();
		}

		break;
	case INTERNET_STATUS_REQUEST_SENT:
		UE_LOG(LogHttp, VeryVerbose, TEXT("REQUEST_SENT: %p"), dwContext);
		break;
	case INTERNET_STATUS_RESOLVING_NAME:
		UE_LOG(LogHttp, VeryVerbose, TEXT("RESOLVING_NAME: %p"), dwContext);
		break;
	case INTERNET_STATUS_SENDING_REQUEST:
		UE_LOG(LogHttp, VeryVerbose, TEXT("SENDING_REQUEST: %p"), dwContext);
		break;
	case INTERNET_STATUS_STATE_CHANGE:
		UE_LOG(LogHttp, VeryVerbose, TEXT("STATE_CHANGE: %p"), dwContext);
		break;
	case INTERNET_STATUS_COOKIE_SENT:
		UE_LOG(LogHttp, VeryVerbose, TEXT("COOKIE_SENT: %p"), dwContext);
		break;
	case INTERNET_STATUS_COOKIE_RECEIVED:
		UE_LOG(LogHttp, VeryVerbose, TEXT("COOKIE_RECEIVED: %p"), dwContext);
		break;
	case INTERNET_STATUS_PRIVACY_IMPACTED:
		UE_LOG(LogHttp, VeryVerbose, TEXT("PRIVACY_IMPACTED: %p"), dwContext);
		break;
	case INTERNET_STATUS_P3P_HEADER:
		UE_LOG(LogHttp, VeryVerbose, TEXT("P3P_HEADER: %p"), dwContext);
		break;
	case INTERNET_STATUS_P3P_POLICYREF:
		UE_LOG(LogHttp, VeryVerbose, TEXT("P3P_POLICYREF: %p"), dwContext);
		break;
	case INTERNET_STATUS_COOKIE_HISTORY:
		{
			const InternetCookieHistory* CookieHistory = (const InternetCookieHistory*)lpvStatusInformation;
			UE_LOG(LogHttp, VeryVerbose, TEXT("COOKIE_HISTORY: %p. Accepted: %u. Leashed: %u. Downgraded: %u. Rejected: %u.")
				, dwContext
				, CookieHistory->fAccepted
				, CookieHistory->fLeashed
				, CookieHistory->fDowngraded
				, CookieHistory->fRejected);
		}
		break;
	default:
		UE_LOG(LogHttp, VeryVerbose, TEXT("Unknown Status: %u. %p"), (uint32)dwInternetStatus, dwContext);
		break;
	}
}

// FHttpRequestWinInet

FHttpRequestWinInet::FHttpRequestWinInet()
:	CompletionStatus(EHttpRequestStatus::NotStarted)
,	ConnectionHandle(NULL)
,	RequestHandle(NULL)
,	ElapsedTimeSinceLastServerResponse(0)
,	ProgressBytesSent(0)
,	StartRequestTime(0)
{

}

FHttpRequestWinInet::~FHttpRequestWinInet()
{
	UE_LOG(LogHttp, Verbose, TEXT("Destroying FHttpRequestWinInet %p %p"), this, RequestHandle);

	CleanupRequest();
}

FString FHttpRequestWinInet::GetURL()
{
	return RequestURL.GetURL();
}

FString FHttpRequestWinInet::GetURLParameter(const FString& ParameterName)
{
	const FString* Result = RequestURL.GetParameter(ParameterName);
	return Result != NULL ? *Result : FString();
}

FString FHttpRequestWinInet::GetHeader(const FString& HeaderName)
{
	FString* Header = RequestHeaders.Find(HeaderName);
	return Header != NULL ? *Header : TEXT("");
}

TArray<FString> FHttpRequestWinInet::GetAllHeaders()
{
	TArray<FString> Result;
	for (TMap<FString, FString>::TConstIterator It(RequestHeaders); It; ++It)
	{
		Result.Add(It.Key() + TEXT(": ") + It.Value());
	}
	return Result;
}

FString FHttpRequestWinInet::GetContentType()
{
	return GetHeader(TEXT("Content-Type"));
}

int32 FHttpRequestWinInet::GetContentLength()
{
	return RequestPayload.Num();
}

const TArray<BYTE>& FHttpRequestWinInet::GetContent()
{
	return RequestPayload;
}

FString FHttpRequestWinInet::GetVerb()
{
	return RequestVerb;
}

void FHttpRequestWinInet::SetVerb(const FString& Verb)
{
	RequestVerb = Verb;
}

void FHttpRequestWinInet::SetURL(const FString& URL)
{
	RequestURL = FURLWinInet(URL);
}

void FHttpRequestWinInet::SetContent(const TArray<uint8>& ContentPayload)
{
	RequestPayload = ContentPayload;
}

void FHttpRequestWinInet::SetContentAsString(const FString& ContentString)
{
	FTCHARToUTF8 Converter(*ContentString);
	RequestPayload.SetNumUninitialized(ContentString.Len());
	FMemory::Memcpy(RequestPayload.GetTypedData(), (const uint8*)Converter.Get(), RequestPayload.Num());
}

void FHttpRequestWinInet::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	RequestHeaders.Add(HeaderName, HeaderValue);
}

bool FHttpRequestWinInet::ProcessRequest()
{
	bool bStarted = false;

	// Disabled http request processing
	if (!FHttpModule::Get().IsHttpEnabled())
	{
		UE_LOG(LogHttp, Verbose, TEXT("Http disabled. Skipping request. url=%s"), *GetURL());
	}
	// Prevent overlapped requests using the same instance
	else if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Still processing last request."));
	}
	// Make sure Internet connection has been setup 
	else if (!FWinInetConnection::Get().IsConnectionValid() &&
			 !FWinInetConnection::Get().InitConnection())
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Could not initialize Internet connection."));
	}
	// Nothing to do without a valid URL
	else if (RequestURL.GetURL().IsEmpty())
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. No URL was specified."));
	}
	// Make sure the URL is parsed correctly with a valid HTTP scheme
	else if (RequestURL.GetURLComponents().nScheme != INTERNET_SCHEME_HTTP &&
			 RequestURL.GetURLComponents().nScheme != INTERNET_SCHEME_HTTPS)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. URL '%s' is not a valid HTTP request. %p"), 
			*RequestURL.GetURL(), this);
	}
	else
	{
		// Mark as in-flight to prevent overlapped requests using the same object
		CompletionStatus = EHttpRequestStatus::Processing;
		// Response object to handle data that comes back after starting this request
		Response = MakeShareable(new FHttpResponseWinInet(*this));
		// Add to global list while being processed so that the ref counted request does not get deleted
		FHttpModule::Get().GetHttpManager().AddRequest(SharedThis(this));
		// keep track of time when request was started
		StartRequestTime = FPlatformTime::Seconds();
		// Try to start the connection and send the Http request
		bStarted = StartRequest();
	}
	
	if (!bStarted)
	{
		// No response since connection failed
		Response = NULL;
		// Cleanup and call delegate
		FinishedRequest();
	}

	// Successfully started the request
	return bStarted;
}

bool FHttpRequestWinInet::StartRequest()
{
	// Make sure old handles are not being reused
	CleanupRequest();

	UE_LOG(LogHttp, Log, TEXT("Start request. %p %s url=%s"), this, *GetVerb(), *GetURL());
	if (UE_LOG_ACTIVE(LogHttp, Verbose))
	{
		for (TMap<FString, FString>::TConstIterator It(RequestHeaders); It; ++It)
		{
			if (!It.Key().Contains(TEXT("Authorization")))
			{
				UE_LOG(LogHttp, Verbose, TEXT("%p Header %s : %s"), this, *It.Key(), *It.Value());
			}
		}
	}

	if (FWinInetConnection::Get().IsConnectionValid())
	{
		// Open an internet connection to the URL endpoint
		ConnectionHandle = InternetConnect(
			FWinInetConnection::Get().InternetHandle, 
			*RequestURL.GetHost(), 
			RequestURL.GetPort(), 
			NULL, 
			NULL, 
			INTERNET_SERVICE_HTTP, 
			0, 
			(DWORD_PTR)this);
	}	
	if (ConnectionHandle == NULL)
	{
		UE_LOG(LogHttp, Warning, TEXT("InternetConnect failed: %s"), *InternetTranslateError(GetLastError()));
		return false;
	}

	// Disable IE offline mode
	::BOOL bEnabled = true;
	InternetSetOption(ConnectionHandle, INTERNET_OPTION_IGNORE_OFFLINE, &bEnabled, sizeof(::BOOL));

	// Set connection timeout in ms
	if (FHttpModule::Get().GetHttpConnectionTimeout() >= 0)
	{
		uint32 HttpConnectionTimeout =  FHttpModule::Get().GetHttpConnectionTimeout() == 0 ? 0xFFFFFFFF : FHttpModule::Get().GetHttpConnectionTimeout() * 1000;
		InternetSetOption(ConnectionHandle, INTERNET_OPTION_CONNECT_TIMEOUT, (::LPVOID)&HttpConnectionTimeout, sizeof(::DWORD));
	}
	// Set receive timeout in ms
	if (FHttpModule::Get().GetHttpReceiveTimeout() >= 0)
	{
		uint32 HttpReceiveTimeout = FHttpModule::Get().GetHttpReceiveTimeout() * 1000;
		InternetSetOption(ConnectionHandle, INTERNET_OPTION_RECEIVE_TIMEOUT, (::LPVOID)&HttpReceiveTimeout, sizeof(::DWORD));
	}
	// Set send timeout in ms
	if (FHttpModule::Get().GetHttpSendTimeout() >= 0)
	{
		uint32 HttpSendTimeout = FHttpModule::Get().GetHttpSendTimeout() * 1000;
		InternetSetOption(ConnectionHandle, INTERNET_OPTION_SEND_TIMEOUT, (::LPVOID)&HttpSendTimeout, sizeof(::DWORD));
	}

	// Query these options to verify
	{
		::DWORD OptionSize = sizeof(::DWORD);
		::DWORD OptionData = 0;
		InternetQueryOption(ConnectionHandle, INTERNET_OPTION_CONNECT_TIMEOUT, (::LPVOID)&OptionData, (::LPDWORD)&OptionSize);
		UE_LOG(LogHttp, VeryVerbose, TEXT("INTERNET_OPTION_CONNECT_TIMEOUT: %d"), OptionData);
		InternetQueryOption(ConnectionHandle, INTERNET_OPTION_RECEIVE_TIMEOUT, (::LPVOID)&OptionData, (::LPDWORD)&OptionSize);
		UE_LOG(LogHttp, VeryVerbose, TEXT("INTERNET_OPTION_RECEIVE_TIMEOUT: %d"), OptionData);
		InternetQueryOption(ConnectionHandle, INTERNET_OPTION_SEND_TIMEOUT, (::LPVOID)&OptionData, (::LPDWORD)&OptionSize);
		UE_LOG(LogHttp, VeryVerbose, TEXT("INTERNET_OPTION_SEND_TIMEOUT: %d"), OptionData);
	}

	// Only custom request flag is for SSL/HTTPS requests
	uint32 RequestFlags = RequestURL.GetURLComponents().nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_FLAG_SECURE : 0;
	// Always download from server instead of cache
	// Forces the request to be resolved by the origin server, even if a cached copy exists on the proxy. 
	RequestFlags |= INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE;
	// Keep the connection open
	RequestFlags |= INTERNET_FLAG_KEEP_CONNECTION;
	// Build full path
	FString PathAndExtra(RequestURL.GetPath() + RequestURL.GetExtraInfo());
	// Create the request
	RequestHandle = HttpOpenRequest(
		ConnectionHandle, 
		RequestVerb.IsEmpty() ? NULL : *RequestVerb, 
		*PathAndExtra, 
		NULL, 
		NULL, 
		NULL, 
		RequestFlags, 
		(DWORD_PTR)this);

	if (RequestHandle == NULL)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpOpenRequest failed: %s"), *InternetTranslateError(GetLastError()));
		return false;
	}

	// Disable certificate checks
	::DWORD SecurityFlags = SECURITY_FLAG_IGNORE_REVOCATION;
	if (!InternetSetOption(RequestHandle, INTERNET_OPTION_SECURITY_FLAGS, (LPVOID)&SecurityFlags, sizeof(::DWORD)))
	{
		UE_LOG(LogHttp, Warning, TEXT("InternetSetOption failed: %s"), *InternetTranslateError(GetLastError()));
	}

	// Send the request with the payload if any
	FString Headers = GenerateHeaderBuffer(RequestPayload.Num());
	BOOL bSentRequest = HttpSendRequest(
		RequestHandle, 
		*Headers, 
		Headers.Len(), 
		RequestPayload.Num() > 0 ? RequestPayload.GetTypedData() : NULL,
		RequestPayload.Num());

	if (!bSentRequest &&
		GetLastError() != ERROR_IO_PENDING)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed: %s"), *InternetTranslateError(GetLastError()));
		return false;
	}

	// Successfully started the request
	return true;
}

void FHttpRequestWinInet::FinishedRequest()
{
	// Clean up session/request handles that may have been created
	CleanupRequest();
	TSharedRef<IHttpRequest> Request = SharedThis(this);
	// Remove from global list since processing is now complete
	FHttpModule::Get().GetHttpManager().RemoveRequest(Request);

	double ElapsedTime = FPlatformTime::Seconds() - StartRequestTime;
	if (Response.IsValid() &&
		Response->bResponseSucceeded)
	{
		UE_LOG(LogHttp, Log, TEXT("Finished request %p. response=%d %s url=%s elapsed=%.3f DownloadSize=%d"), 
			this, Response->GetResponseCode(), *GetVerb(), *GetURL(), ElapsedTime, Response->GetContentLength());

		// log info about cloud front to identify failed downloads
		if (UE_LOG_ACTIVE(LogHttp, Verbose))
		{
			TArray<FString> AllHeaders = Response->GetAllHeaders();
			for (TArray<FString>::TConstIterator It(AllHeaders); It; ++It)
			{
				const FString& HeaderStr = *It;
				if (!HeaderStr.Contains(TEXT("Authorization")))
				{
					UE_LOG(LogHttp, Verbose, TEXT("%p Response Header %s"), this, *HeaderStr);
				}
			}
		}

		// Mark last request attempt as completed successfully
		CompletionStatus = EHttpRequestStatus::Succeeded;
		// Call delegate with valid request/response objects
		OnProcessRequestComplete().ExecuteIfBound(Request,Response,true);
	}
	else
	{
		UE_LOG(LogHttp, Log, TEXT("Finished request %p. no response %s url=%s elapsed=%.3f"), 
			this, *GetVerb(), *GetURL(), ElapsedTime);

		// Mark last request attempt as completed but failed
		CompletionStatus = EHttpRequestStatus::Failed;
		// No response since connection failed
		Response = NULL;
		// Call delegate with failure
		OnProcessRequestComplete().ExecuteIfBound(Request,NULL,false);
	}
}

void FHttpRequestWinInet::CleanupRequest()
{
	if (RequestHandle != NULL)
	{
		InternetCloseHandle(RequestHandle);
		RequestHandle = NULL;
	}
	if (ConnectionHandle != NULL)
	{
		InternetCloseHandle(ConnectionHandle);
		ConnectionHandle = NULL;
	}
}

FString FHttpRequestWinInet::GenerateHeaderBuffer(uint32 ContentLength)
{
	FString Result;
	for (TMap<FString, FString>::TConstIterator It(RequestHeaders); It; ++It)
	{
		Result += It.Key() + TEXT(": ") + It.Value() + TEXT("\r\n");
	}
	if (ContentLength > 0)
	{
		Result += FString(TEXT("Content-Length: ")) + FString::FromInt(ContentLength) + TEXT("\r\n");
	}
	return Result;
}

void FHttpRequestWinInet::ResetRequestTimeout()
{
	FPlatformAtomics::InterlockedExchange(&ElapsedTimeSinceLastServerResponse, 0);
}

FHttpRequestCompleteDelegate& FHttpRequestWinInet::OnProcessRequestComplete()
{
	return RequestCompleteDelegate;
}

FHttpRequestProgressDelegate& FHttpRequestWinInet::OnRequestProgress()
{
	return RequestProgressDelegate;
}

void FHttpRequestWinInet::CancelRequest()
{
	UE_LOG(LogHttp, Log, TEXT("Canceling Http request. %p url=%s"),
			this, *GetURL());

	// force finish/cleanup of request
	// note: will still call completion delegates
	FinishedRequest();
}

EHttpRequestStatus::Type FHttpRequestWinInet::GetStatus()
{
	return CompletionStatus;
}

void FHttpRequestWinInet::Tick(float DeltaSeconds)
{
	// keep track of elapsed milliseconds
	const int32 ElapsedMillisecondsThisFrame = DeltaSeconds * 1000;
	FPlatformAtomics::InterlockedAdd(&ElapsedTimeSinceLastServerResponse, ElapsedMillisecondsThisFrame);

	// Update response progress
	if(Response.IsValid())
	{
		int32 ResponseBytes = Response->ProgressBytesRead.GetValue();
		if(ResponseBytes > ProgressBytesSent)
		{
			ProgressBytesSent = ResponseBytes;
			OnRequestProgress().ExecuteIfBound(SharedThis(this),ResponseBytes);
		}
	}

	// Convert to seconds for comparison to the timeout value
	const float TotalElapsedSeconds = ElapsedTimeSinceLastServerResponse / 1000.f;

	const float HttpTimeout = FHttpModule::Get().GetHttpTimeout();
	if (HttpTimeout > 0 && 
		TotalElapsedSeconds >= HttpTimeout)
	{
		UE_LOG(LogHttp, Warning, TEXT("Timeout processing Http request. %p url=%s"),
			this, *GetURL());

		// finish it off since it is timeout
		FinishedRequest();
	}
	// No longer waiting for a response and done processing it
	else if (CompletionStatus == EHttpRequestStatus::Processing &&
		Response.IsValid() &&
		Response->bIsReady)
	{
		FinishedRequest();
	}
}

// FHttpResponseWinInet

FHttpResponseWinInet::FHttpResponseWinInet(const FHttpRequestWinInet& InRequest)
:	Request(InRequest)
,	RequestURL(InRequest.RequestURL)
,	AsyncBytesRead(0)
,	TotalBytesRead(0)
,	ResponseCode(EHttpResponseCodes::Unknown)
,	ContentLength(0)
,	bIsReady(0)
,	bResponseSucceeded(0)
,	MaxReadBufferSize(FHttpModule::Get().GetMaxReadBufferSize())
{

}

FHttpResponseWinInet::~FHttpResponseWinInet()
{	

}

FString FHttpResponseWinInet::GetURL()
{
	return RequestURL.GetURL();
}

FString FHttpResponseWinInet::GetURLParameter(const FString& ParameterName)
{
	const FString* Result = RequestURL.GetParameter(ParameterName);
	return Result != NULL ? *Result : FString();
}

FString FHttpResponseWinInet::GetHeader(const FString& HeaderName)
{
	FString Result(TEXT(""));
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Can't get cached header [%s]. Response still processing. %p"),
			*HeaderName, &Request);
	}
	else
	{
		FString* Header = ResponseHeaders.Find(HeaderName);
		if (Header != NULL)
		{
			return *Header;
		}
	}
	return Result;
}

TArray<FString> FHttpResponseWinInet::GetAllHeaders()	
{
	TArray<FString> Result;
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Can't get cached headers. Response still processing. %p"),&Request);
	}
	else
	{
		for (TMap<FString, FString>::TConstIterator It(ResponseHeaders); It; ++It)
		{
			Result.Add(It.Key() + TEXT(": ") + It.Value());
		}
	}
	return Result;
}

FString FHttpResponseWinInet::GetContentType()
{
	return GetHeader(TEXT("Content-Type"));
}

int32 FHttpResponseWinInet::GetContentLength()
{
	return ContentLength;
}

const TArray<BYTE>& FHttpResponseWinInet::GetContent()
{
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Payload is incomplete. Response still processing. %p"),&Request);
	}
	return ResponsePayload;
}

int32 FHttpResponseWinInet::GetResponseCode()
{
	return ResponseCode;
}

FString FHttpResponseWinInet::GetContentAsString()
{
	TArray<uint8> ZeroTerminatedPayload(GetContent());
	ZeroTerminatedPayload.Add(0);
	return UTF8_TO_TCHAR(ZeroTerminatedPayload.GetData());
}

void FHttpResponseWinInet::ProcessResponse()
{
	// Keep track of total read from last async callback
	TotalBytesRead += AsyncBytesRead;
	// We might be calling back into this from another asynchronous read, so continue where we left off.
	// if there is no content length, we're probably receiving chunked data.
	ContentLength = QueryContentLength();
	// Set buffer size based on content length or hard code if chunked response
	int32 BufferSize = 0;
	// For non-chunked responses, allocate one extra uint8 to check if we are sent extra content
	if (ContentLength > 0)
	{
		if (TotalBytesRead == 0)
		{
			BufferSize = ContentLength + 1;
			// Size read buffer
			ResponsePayload.SetNum(BufferSize);
		}
	}
	else
	{
		// For chunked responses, add data using a fixed size buffer at a time.
		BufferSize = TotalBytesRead + MaxReadBufferSize;
		// Size read buffer
		ResponsePayload.SetNum(BufferSize);
	}

	bool bFailed = false;
	int32 LoopCount = 0;
	do
	{
		const int32 NumBytesToRead = FMath::Min<int32>(ResponsePayload.Num() - TotalBytesRead, MaxReadBufferSize);
		// Read directly into the response payload
		const BOOL bReadFile = InternetReadFile(
			Request.RequestHandle, 
			&ResponsePayload[TotalBytesRead], 
			NumBytesToRead, 
			(LPDWORD)&AsyncBytesRead);

		UE_LOG(LogHttp, VeryVerbose, TEXT("InternetReadFile result=%d (%u bytes read) (%u bytes total read). LoopCount=%d %p"), 
			bReadFile, AsyncBytesRead, TotalBytesRead, LoopCount, &Request);

		if (!bReadFile)
		{
			int32 ErrorCode = GetLastError();
			if (ErrorCode == ERROR_IO_PENDING)
			{
				// Chunked responses could cause InternetReadFile to return IO_PENDING, in which
				// case the largely undocumented behavior is to return and wait for the callback function
				// to be called again later when you receive another REQUEST_COMPLETE signal.
				// You are supposed to keep doing this until InternetReadFile returns 0 with a successful return value.

				UE_LOG(LogHttp, VeryVerbose, TEXT("InternetReadFile is completing asynchronously, so waiting for callback again. %p"), 
					&Request);
			}
			else if (ErrorCode != ERROR_SUCCESS)
			{
				UE_LOG(LogHttp, Log, TEXT("InternetReadFile failed (%u bytes read). Returning what we've read so far: %s. %p"), 
					AsyncBytesRead, *InternetTranslateError(ErrorCode), &Request);
			}
			// Allow processing to continue
			return;
		}
		else
		{
			// Keep track of total read so far
			TotalBytesRead += AsyncBytesRead;
			// resize the buffer if we don't know our content length, otherwise don't let the buffer grow larger than content length.
			if (TotalBytesRead >= ResponsePayload.Num())
			{
				if (ContentLength > 0)
				{
					UE_LOG(LogHttp, Log, TEXT("Response payload (%d bytes read so far) is larger than the content-length (%d). Resizing buffer to accommodate. %p"), 
						TotalBytesRead, ContentLength, &Request);
				}
				ResponsePayload.AddZeroed(MaxReadBufferSize);
			}
		}
		LoopCount++;

		// Update progress bytes
		ProgressBytesRead.Set( TotalBytesRead );

	} while (AsyncBytesRead > 0);

	if (ContentLength != 0 && 
		TotalBytesRead != ContentLength)
	{
		UE_LOG(LogHttp, Warning, TEXT("Response payload was %d bytes, content-length indicated (%d) bytes. %p"), 
			TotalBytesRead, ContentLength, &Request);
	}
	UE_LOG(LogHttp, Verbose, TEXT("TotalBytesRead = %d. %p"), TotalBytesRead, &Request);

	// Shrink array to only the valid data
	ResponsePayload.SetNum(TotalBytesRead);
	// Query for header data and cache it
	ProcessResponseHeaders();
	// Query for response code and cache it
	ProcessResponseCode();
	// Cache content length now that response is done
	ContentLength = QueryContentLength();
	// Mark as valid processed response
	FPlatformAtomics::InterlockedExchange(&bResponseSucceeded, 1);
	// Done processing
	FPlatformAtomics::InterlockedExchange(&bIsReady, 1);
	// Update progress bytes
	ProgressBytesRead.Set( TotalBytesRead );
}
void FHttpResponseWinInet::ProcessResponseHeaders()
{
	::DWORD HeaderSize = 0;
	TArray<FString> Result;
	if (!HttpQueryInfo(Request.RequestHandle, HTTP_QUERY_RAW_HEADERS_CRLF, NULL, &HeaderSize, NULL))
	{
		uint32 ErrorCode = GetLastError();
		if (ErrorCode != ERROR_INSUFFICIENT_BUFFER)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpQueryInfo to get header length for all headers failed: %s. %p"), 
				*InternetTranslateError(GetLastError()), &Request);
		}
		if (HeaderSize == 0)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpQueryInfo for all headers returned zero header size. %p"), this);
		}
		TArray<TCHAR> HeaderBuffer;
		HeaderBuffer.AddUninitialized(HeaderSize/sizeof(TCHAR));
		if (!HttpQueryInfo(Request.RequestHandle, HTTP_QUERY_RAW_HEADERS_CRLF, HeaderBuffer.GetTypedData(), &HeaderSize, NULL))
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpQueryInfo for all headers failed: %s. %p"), 
				*InternetTranslateError(GetLastError()), &Request);
		}
		// parse all the key/value pairs
		const TCHAR* HeaderPtr = HeaderBuffer.GetTypedData();
		// don't count the terminating NULL character as one to search.
		const TCHAR* EndPtr = HeaderPtr + HeaderBuffer.Num()-1;
		while (HeaderPtr < EndPtr)
		{
			const TCHAR* DelimiterPtr = FCString::Strstr(HeaderPtr, TEXT("\r\n"));
			if (DelimiterPtr == NULL)
			{
				DelimiterPtr = EndPtr;
			}
			FString HeaderLine(DelimiterPtr-HeaderPtr, HeaderPtr);
			FString HeaderKey,HeaderValue;
			if (HeaderLine.Split(TEXT(":"), &HeaderKey, &HeaderValue))
			{
				if (!HeaderKey.IsEmpty())
				{
					ResponseHeaders.Add(HeaderKey, HeaderValue.Trim());
				}
			}
			HeaderPtr = DelimiterPtr + 2;
		}
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpQueryInfo for all headers failed when trying to determine the size for the header buffer. %p"), 
			&Request);
	}
}

void FHttpResponseWinInet::ProcessResponseCode()
{
	// get the response code
	ResponseCode = EHttpResponseCodes::Unknown;
	::DWORD CodeSize = sizeof(ResponseCode);
	if (!HttpQueryInfo(Request.RequestHandle, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &ResponseCode, &CodeSize, NULL))
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpQueryInfo for response code failed: %s. %p"), 
			*InternetTranslateError(GetLastError()), &Request);
	}
}
FString FHttpResponseWinInet::QueryHeaderString(uint32 HttpQueryInfoLevel, const FString& HeaderName) const
{
	// try to use stack allocation where possible.
	::DWORD HeaderSize = 0;
	TCHAR HeaderValue[128];
	TArray<TCHAR> HeaderValueLong;
	TCHAR* HeaderValueReal = HeaderValue;
	if (!HttpQueryInfo(Request.RequestHandle, HttpQueryInfoLevel, const_cast<TCHAR*>(*HeaderName), &HeaderSize, NULL))
	{
		uint32 ErrorCode = GetLastError();
		if (ErrorCode == ERROR_HTTP_HEADER_NOT_FOUND)
		{
			return FString();
		}
		else if (ErrorCode == ERROR_INSUFFICIENT_BUFFER)
		{
			// make sure we have enough room to supply the HeaderName and Value. If not, dynamically allocate.
			uint32 HeaderSizeChars = HeaderSize / sizeof(TCHAR) + 1;
			uint32 HeaderNameChars = HeaderName.Len() == 0 ? 0 : HeaderName.Len() + 1;
			if (HeaderSizeChars > ARRAYSIZE(HeaderValue) || HeaderNameChars > ARRAYSIZE(HeaderValue))
			{
				// we have to create a dynamic allocation to hold the result.
				UE_LOG(LogHttp, Verbose, TEXT("Having to resize default buffer for retrieving header %s. Name length: %u. Value length: %u. %p"), 
					*HeaderName, HeaderNameChars, HeaderSizeChars, &Request);
				uint32 NewBufferSizeChars = FMath::Max(HeaderSizeChars, HeaderNameChars);
				// Have to copy the HeaderName into the buffer as well for the API to work.
				if (HeaderName.Len() > 0)
				{
					HeaderValueLong = HeaderName.GetCharArray();
				}
				// Set the size of the array to hold the entire value.
				HeaderValueLong.SetNum(NewBufferSizeChars);
				HeaderSize = NewBufferSizeChars * sizeof(TCHAR);
				HeaderValueReal = HeaderValueLong.GetData();
			}
			else
			{
				// Use the stack allocated space if we have the room.
				FMemory::Memcpy(HeaderValue, *HeaderName, HeaderName.Len()*sizeof(TCHAR));
				HeaderValue[HeaderName.Len()] = 0;
			}
			if (!HttpQueryInfo(Request.RequestHandle, HttpQueryInfoLevel, HeaderValueReal, &HeaderSize, NULL))
			{
				UE_LOG(LogHttp, Warning, TEXT("HttpQueryInfo failed trying to get Header Value for Name %s: %s. %p"), 
					*HeaderName, *InternetTranslateError(GetLastError()), &Request);
				return FString();
			}
		}
	}
	return FString(HeaderValueReal);
}

int32 FHttpResponseWinInet::QueryContentLength() const
{
	int32 Result = 0;

	FString ContentLengthStr(QueryHeaderString(HTTP_QUERY_CONTENT_LENGTH, FString()));
	if (ContentLengthStr.Len() > 0)
	{
		Result = FCString::Strtoi(*ContentLengthStr, NULL, 10);
	}
	return Result;
}

// FURLWinInet

FURLWinInet::FURLWinInet()
{
	ClearCachedData();
}

FURLWinInet::FURLWinInet( const FString& InURL ) :RequestURL(InURL)
{
	ClearCachedData();
}

FURLWinInet::FURLWinInet( const FURLWinInet& InURL ) :RequestURL(InURL.RequestURL)
{
	ClearCachedData();
}

FURLWinInet& FURLWinInet::operator=( const FURLWinInet& InURL )
{
	if (this != &InURL)
	{
		RequestURL = InURL.RequestURL;
		ClearCachedData();
	}
	return *this;
}

const FString& FURLWinInet::GetURL() const
{
	return RequestURL;
}

FString FURLWinInet::GetHost() const
{
	if (URLPtr == NULL) CrackUrlParameters();

	if (URLPtr != NULL)
	{
		return FString(URLParts.dwHostNameLength, URLParts.lpszHostName);
	}
	return FString();
}

WORD FURLWinInet::GetPort() const
{
	if (URLPtr == NULL) CrackUrlParameters();

	if (URLPtr != NULL)
	{
		return URLParts.nPort;
	}
	return 0;
}

FString FURLWinInet::GetPath() const
{
	if (URLPtr == NULL) CrackUrlParameters();

	if (URLPtr != NULL)
	{
		return FString(URLParts.dwUrlPathLength, URLParts.lpszUrlPath);
	}
	return FString();
}

FString FURLWinInet::GetExtraInfo() const
{
	if (URLPtr == NULL) CrackUrlParameters();

	if (URLPtr != NULL)
	{
		return FString(URLParts.dwExtraInfoLength, URLParts.lpszExtraInfo);
	}
	return FString();
}

const URL_COMPONENTS& FURLWinInet::GetURLComponents() const
{
	if (URLPtr == NULL) CrackUrlParameters();

	return URLParts;
}

const FString* FURLWinInet::GetParameter( const FString& ParameterName ) const
{
	if (URLPtr == NULL) CrackUrlParameters();

	return URLParameters.Find(ParameterName);
}

void FURLWinInet::ClearCachedData() const
{
	URLPtr = NULL;
	URLParameters.Reset();
	FMemory::MemZero(URLParts);
}

void FURLWinInet::CrackUrlParameters() const
{
	// used to make sure we can't early exit from this function without cleaning up.
	struct FClearCachedDataGuard
	{
		FClearCachedDataGuard(const FURLWinInet& InURL)
			:	CachedURL(InURL)
			,	bClearCachedData(true) 
		{}
		~FClearCachedDataGuard() 
		{ 
			if (bClearCachedData) 
			{
				CachedURL.ClearCachedData(); 
			}
		}
		bool bClearCachedData;
		const FURLWinInet& CachedURL;
	};
	FClearCachedDataGuard CachedDataGuard(*this);

	// don't crack anything if the request is empty
	if (RequestURL.IsEmpty()) 
	{
		return;
	}

	URLPtr = *RequestURL;
	// crack open the URL into its component parts
	URLParts.dwStructSize = sizeof(URLParts);
	URLParts.dwHostNameLength = 1;
	URLParts.dwUrlPathLength = 1;
	URLParts.dwExtraInfoLength = 1;

	if (!InternetCrackUrl(URLPtr, 0, 0, &URLParts))
	{
		UE_LOG(LogHttp, Warning, TEXT("Failed to crack URL parameters for URL:%s"), *RequestURL);
		return;
	}

	// make sure we didn't fail.
	FString Result;
	if (URLParts.dwExtraInfoLength > 1)
	{
		if (URLParts.lpszExtraInfo[0] == TEXT('?'))
		{
			const TCHAR* ParamPtr = URLParts.lpszExtraInfo+1;
			const TCHAR* ParamEnd = URLParts.lpszExtraInfo+URLParts.dwExtraInfoLength;

			while (ParamPtr < ParamEnd)
			{
				const TCHAR* DelimiterPtr = FCString::Strchr(ParamPtr, TEXT('&'));
				if (DelimiterPtr == NULL)
				{
					DelimiterPtr = ParamEnd;
				}
				const TCHAR* EqualPtr = FCString::Strchr(ParamPtr, TEXT('='));
				if (EqualPtr != NULL && EqualPtr < DelimiterPtr)
				{
					// This will handle the case of Key=&Key2=Value... by allocating a zero-length string with the math below
					URLParameters.Add(FString(EqualPtr-ParamPtr, ParamPtr), FString(DelimiterPtr-EqualPtr-1, EqualPtr+1));
				}
				else
				{
					URLParameters.Add(FString(DelimiterPtr-ParamPtr, ParamPtr), FString());
				}
				ParamPtr = DelimiterPtr + 1;
			}
		}
		else
		{
			UE_LOG(LogHttp, Warning, TEXT("URL '%s' extra info did not start with a '?', so can't parse headers."), *RequestURL);
		}
	}
	CachedDataGuard.bClearCachedData = false;
}
