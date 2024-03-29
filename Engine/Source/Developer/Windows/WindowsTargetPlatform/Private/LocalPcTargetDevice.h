// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LocalPcTargetDevice.h: Declares the TLocalPcTargetDevice class template.
=============================================================================*/

#pragma once


/**
 * Template for local PC target devices.
 *
 * @param WIN64 - Whether the target platform is 64-bit Windows.
 */
template<bool WIN64>
class TLocalPcTargetDevice
	: public ITargetDevice
{
public:

	/**
	 * Creates and initializes a new device for the specified target platform.
	 *
	 * @param InTargetPlatform - The target platform.
	 */
	TLocalPcTargetDevice( const ITargetPlatform& InTargetPlatform )
		: TargetPlatform(InTargetPlatform)
	{ }

public:

	virtual bool Connect( ) OVERRIDE
	{
		return true;
	}

	virtual bool Deploy( const FString& SourceFolder, FString& OutAppId ) OVERRIDE
	{
		OutAppId = TEXT("");

		FString PlatformName = WIN64 ? TEXT("Win64") : TEXT("Win32");
		FString DeploymentDir = FPaths::EngineIntermediateDir() / TEXT("Devices") / PlatformName;

		// delete previous build
		IFileManager::Get().DeleteDirectory(*DeploymentDir, false, true);

		// copy files into device directory
		TArray<FString> FileNames;

		IFileManager::Get().FindFilesRecursive(FileNames, *SourceFolder, TEXT("*.*"), true, false);

		for (int32 FileIndex = 0; FileIndex < FileNames.Num(); ++FileIndex)
		{
			const FString& SourceFilePath = FileNames[FileIndex];
			FString DestFilePath = DeploymentDir + SourceFilePath.RightChop(SourceFolder.Len());

			IFileManager::Get().Copy(*DestFilePath, *SourceFilePath);
		}

		return true;
	}

	virtual void Disconnect( )
	{ }

	virtual ETargetDeviceTypes::Type GetDeviceType( ) const OVERRIDE
	{
		if (::GetSystemMetrics(SM_TABLETPC) != 0)
		{
			return ETargetDeviceTypes::Tablet;
		}

		return ETargetDeviceTypes::Desktop;
	}

	virtual FTargetDeviceId GetId( ) const OVERRIDE
	{
		return FTargetDeviceId(TargetPlatform.PlatformName(), GetName());
	}

	virtual FString GetName( ) const OVERRIDE
	{
		return FPlatformProcess::ComputerName();
	}

	virtual FString GetOperatingSystemName( ) OVERRIDE
	{
		if (WIN64)
		{
			return TEXT("Windows (64-bit)");
		}

		return TEXT("Windows (32-bit)");
	}

	virtual int32 GetProcessSnapshot( TArray<FTargetDeviceProcessInfo>& OutProcessInfos ) OVERRIDE
	{
		// enumerate processes
		HANDLE ProcessSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		if (ProcessSnapshot != INVALID_HANDLE_VALUE)
		{
			PROCESSENTRY32 ProcessEntry;
			ProcessEntry.dwSize = sizeof(PROCESSENTRY32);

			if (::Process32First(ProcessSnapshot, &ProcessEntry))
			{
				do 
				{
					// only include processes that the user has permission to query
					HANDLE ProcessHandle = ::OpenProcess(PROCESS_QUERY_INFORMATION, false, ProcessEntry.th32ProcessID);

					if (ProcessHandle != NULL)
					{
						FTargetDeviceProcessInfo ProcessInfo;

						// get process details
						ProcessInfo.Id = ProcessEntry.th32ProcessID;
						ProcessInfo.Name = ProcessEntry.szExeFile;
						ProcessInfo.ParentId = ProcessEntry.th32ParentProcessID;
						ProcessInfo.UserName = TEXT("-");

						// get process user name
						HANDLE TokenHandle;

						if (::OpenProcessToken(ProcessHandle, TOKEN_QUERY, &TokenHandle))
						{
							::DWORD UserTokenSize;

							::GetTokenInformation(TokenHandle, TokenUser, NULL, 0, &UserTokenSize);

							if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER)
							{
								PTOKEN_USER UserToken = reinterpret_cast<PTOKEN_USER>(new BYTE[UserTokenSize]);

								if (UserToken != NULL)
								{
									if (::GetTokenInformation(TokenHandle, TokenUser, UserToken, UserTokenSize, &UserTokenSize))
									{
										WCHAR DomainName[256];
										::DWORD DomainNameLength;

										WCHAR UserName[256];
										::DWORD UserNameLength;

										SID_NAME_USE SidType;

										if (::LookupAccountSid(NULL, UserToken->User.Sid, UserName, &UserNameLength, DomainName, &DomainNameLength, &SidType))
										{
											ProcessInfo.UserName = UserName;
										}
										else
										{
											ProcessInfo.UserName = TEXT("SYSTEM");
										}
									}
								}
								else
								{
									::DWORD LastError = ::GetLastError();
								}
							}

							::CloseHandle(TokenHandle);
						}

						::CloseHandle(ProcessHandle);

						OutProcessInfos.Add(ProcessInfo);
					}
				}
				while (::Process32Next(ProcessSnapshot, &ProcessEntry));
			}

			::CloseHandle(ProcessSnapshot);
		}

		// get thread details
		HANDLE ThreadSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

		if (ThreadSnapshot != INVALID_HANDLE_VALUE)
		{
			THREADENTRY32 ThreadEntry;
			ThreadEntry.dwSize = sizeof(THREADENTRY32);

			if (::Thread32First(ThreadSnapshot, &ThreadEntry))
			{
				do 
				{
					for (int32 ProcessInfoIndex = 0; ProcessInfoIndex < OutProcessInfos.Num(); ++ProcessInfoIndex)
					{
						FTargetDeviceProcessInfo& ProcessInfo = OutProcessInfos[ProcessInfoIndex];

						if (ProcessInfo.Id == ThreadEntry.th32OwnerProcessID)
						{
							FTargetDeviceThreadInfo ThreadInfo;

							ThreadInfo.ExitCode = 0;
							ThreadInfo.Id = ThreadEntry.th32ThreadID;
							ThreadInfo.StackSize = 0;
							ThreadInfo.State = ETargetDeviceThreadStates::Unknown;
							ThreadInfo.WaitState = ETargetDeviceThreadWaitStates::Unknown;

							ProcessInfo.Threads.Add(ThreadInfo);

							break;
						}
					}
				}
				while (::Thread32Next(ThreadSnapshot, &ThreadEntry));

				::CloseHandle(ThreadSnapshot);
			}
		}

		return OutProcessInfos.Num();
	}

	virtual const class ITargetPlatform& GetTargetPlatform( ) const OVERRIDE
	{
		return TargetPlatform;
	}

	virtual bool IsConnected( )
	{
		return true;
	}

	virtual bool IsDefault( ) const OVERRIDE
	{
		return true;
	}

	virtual bool Launch( const FString& AppId, EBuildConfigurations::Type BuildConfiguration, EBuildTargets::Type BuildTarget, const FString& Params, uint32* OutProcessId ) OVERRIDE
	{
		// build executable path
		FString PlatformName = WIN64 ? TEXT("Win64") : TEXT("Win32");
		FString ExecutablePath = FPaths::EngineIntermediateDir() / TEXT("Devices") / PlatformName / TEXT("Engine") / TEXT("Binaries") / PlatformName;
		
		if (BuildTarget == EBuildTargets::Game)
		{
			ExecutablePath /= TEXT("UE4Game");
		}
		else if (BuildTarget == EBuildTargets::Server)
		{
			ExecutablePath /= TEXT("UE4Game");
		}
		else if (BuildTarget == EBuildTargets::Editor)
		{
			ExecutablePath /= TEXT("UE4Editor");
		}

		if (BuildConfiguration != EBuildConfigurations::Development)
		{
			ExecutablePath += FString::Printf(TEXT("-%s-%s"), *PlatformName, EBuildConfigurations::ToString(BuildConfiguration));
		}

		ExecutablePath += TEXT(".exe");

		// launch the game
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*ExecutablePath, *Params, true, false, false, OutProcessId, 0, NULL, NULL);
		return ProcessHandle.Close();
	}

	virtual bool PowerOff( bool Force ) OVERRIDE
	{
		if (!AdjustShutdownPrivileges())
		{
			return false;
		}

		return (::ExitWindowsEx(EWX_POWEROFF | EWX_FORCE, SHTDN_REASON_MINOR_MAINTENANCE | SHTDN_REASON_FLAG_PLANNED) != 0);
	}

	virtual bool PowerOn( ) OVERRIDE
	{
		return false;
	}

	virtual bool Reboot( bool bReconnect = false ) OVERRIDE
	{
		if (!AdjustShutdownPrivileges())
		{
			return false;
		}

		return (::ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MINOR_MAINTENANCE | SHTDN_REASON_FLAG_PLANNED) != 0);
	}

	virtual bool Run( const FString& ExecutablePath, const FString& Params, uint32* OutProcessId )
	{
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*ExecutablePath, *Params, true, false, false, OutProcessId, 0, NULL, NULL);
		return ProcessHandle.Close();
	}

	virtual bool SupportsFeature( ETargetDeviceFeatures::Type Feature ) const OVERRIDE
	{
		switch (Feature)
		{
		case ETargetDeviceFeatures::MultiLaunch:
			return true;

		case ETargetDeviceFeatures::PowerOff:
			return true;

		// @todo gmp: implement turning on remote PCs (wake on LAN)
		case ETargetDeviceFeatures::PowerOn:
			return false;

		case ETargetDeviceFeatures::ProcessSnapshot:
			return true;

		case ETargetDeviceFeatures::Reboot:
			return true;
		}

		return false;
	}

	virtual bool SupportsSdkVersion( const FString& VersionString ) const OVERRIDE
	{
		// @todo filter SDK versions
		return true;
	}

	virtual void SetUserCredentials( const FString & UserName, const FString & UserPassword ) OVERRIDE
	{
	}

	virtual bool GetUserCredentials( FString & OutUserName, FString & OutUserPassword ) OVERRIDE
	{
		return false;
	}

	virtual bool TerminateProcess( const int32 ProcessId ) OVERRIDE
	{
		HANDLE ProcessHandle = OpenProcess(PROCESS_TERMINATE, false, ProcessId);

		if (ProcessHandle != NULL)
		{
			bool Result = (::TerminateProcess(ProcessHandle, 0) != 0);

			CloseHandle(ProcessHandle);

			return Result;
		}

		return false;
	}

protected:

	/**
	 * Adjusts shutdown privileges for the local host PC.
	 *
	 * @return true on success, false otherwise.
	 */
	bool AdjustShutdownPrivileges( )
	{
		HANDLE TokenHandle;
		TOKEN_PRIVILEGES Token;

		if (!::OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &TokenHandle))
		{
			return false;
		}

		::LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &Token.Privileges[0].Luid);

		if (Token.Privileges[0].Attributes == SE_PRIVILEGE_ENABLED)
		{
			return true;
		}

		Token.PrivilegeCount = 1;
		Token.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		::AdjustTokenPrivileges(TokenHandle, 0, &Token, 0, (PTOKEN_PRIVILEGES)NULL, 0);
      
		return (GetLastError() == ERROR_SUCCESS);
	}

private:

	// Holds the collection of processes that were started using the Run() method.
	TMap<FGuid, void*> Processes;

	// Holds a reference to the device's target platform.
	const ITargetPlatform& TargetPlatform;
};
