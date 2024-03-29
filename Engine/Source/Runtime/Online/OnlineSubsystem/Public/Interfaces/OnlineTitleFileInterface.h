// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
 #include "OnlineDelegateMacros.h"

/**
 * Delegate fired when the list of files has been returned from the network store
 *
 * @param bWasSuccessful whether the file list was successful or not
 *
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEnumerateFilesComplete, bool);
typedef FOnEnumerateFilesComplete::FDelegate FOnEnumerateFilesCompleteDelegate;

/**
 * Delegate fired when a file read from the network platform's storage is complete
 *
 * @param bWasSuccessful whether the file read was successful or not
 * @param FileName the name of the file this was for
 *
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnReadFileComplete, bool, const FString&);
typedef FOnReadFileComplete::FDelegate FOnReadFileCompleteDelegate;

class IOnlineTitleFile
{
protected:
	IOnlineTitleFile() {};

public:
	virtual ~IOnlineTitleFile() {};

	/**
	 * Copies the file data into the specified buffer for the specified file
	 *
	 * @param FileName the name of the file to read
	 * @param FileContents the out buffer to copy the data into
	 *
 	 * @return true if the data was copied, false otherwise
	 */
	virtual bool GetFileContents(const FString& FileName, TArray<uint8>& FileContents) = 0;

	/**
	 * Empties the set of downloaded files if possible (no async tasks outstanding)
	 *
	 * @return true if they could be deleted, false if they could not
	 */
	virtual bool ClearFiles() = 0;

	/**
	 * Empties the cached data for this file if it is not being downloaded currently
	 *
	 * @param FileName the name of the file to remove from the cache
	 *
	 * @return true if it could be deleted, false if it could not
	 */
	virtual bool ClearFile(const FString& FileName) = 0;

	/**
	* Requests a list of available files from the network store
	*
	* @param Start first file entry to enumerate (default 0)
	* @param Count number of files to enumerate (default -1 for all)
	*
	* @return true if the request has started, false if not
	*/
	virtual bool EnumerateFiles() = 0;

	/**
	 * Requests a list of available files from the network store
	 *
	 * @param Start first file entry to enumerate (default 0)
	 * @param Count number of files to enumerate (default -1 for all)
	 *
	 * @return true if the request has started, false if not
	 */
	virtual bool EnumerateFiles(int32 Start, int32 Count) = 0;

	/**
	 * Delegate fired when the list of files has been returned from the network store
	 *
	 * @param bWasSuccessful whether the file list was successful or not
	 *
	 */
	DEFINE_ONLINE_DELEGATE_ONE_PARAM(OnEnumerateFilesComplete, bool);

	/**
	 * Returns the list of files that was returned by the network store
	 * 
	 * @param Files out array of file metadata
	 *
	 */
	virtual void GetFileList(TArray<FCloudFileHeader>& Files) = 0;

	/**
	 * Starts an asynchronous read of the specified file from the network platform's file store
	 *
	 * @param FileToRead the name of the file to read
	 *
	 * @return true if the calls starts successfully, false otherwise
	 */
	virtual bool ReadFile(const FString& FileName) = 0;

	/**
	 * Delegate fired when a file read from the network platform's storage is complete
	 *
	 * @param bWasSuccessful whether the file read was successful or not
	 * @param FileName the name of the file this was for
	 *
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnReadFileComplete, bool, const FString&);



};

typedef TSharedPtr<IOnlineTitleFile, ESPMode::ThreadSafe> IOnlineTitleFilePtr;
typedef TSharedRef<IOnlineTitleFile, ESPMode::ThreadSafe> IOnlineTitleFileRef;