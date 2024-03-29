// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=================================================================================
	WindowsPlatformSurvey.cpp: Microsoft Windows platform hardware-survey classes
=================================================================================*/

#include "CorePrivate.h"

#define USING_WINSAT_API	1
#define USING_POWRPROF		1

#include "AllowWindowsPlatformTypes.h"
	#include <shlobj.h>					// Shell32.lib currently a global linked lib
#if USING_WINSAT_API
	#include <winsatcominterfacei.h>	// Uses Winsatapi.dll on Windows Vista and later versions only
#endif
#if USING_POWRPROF
	#include <PowrProf.h>				// Uses PowrProf.lib on Windows XP and later versions only
	#pragma comment( lib, "PowrProf.lib" )
#endif
	#include <subauth.h>

#include "UnrealString.h"
#include "SynthBenchmark.h"

#ifndef PROCESSOR_POWER_INFORMATION
typedef struct _PROCESSOR_POWER_INFORMATION {  
	ULONG Number;
	ULONG MaxMhz;
	ULONG CurrentMhz;
	ULONG MhzLimit;
	ULONG MaxIdleState;
	ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;
#endif

namespace WindowsPlatformSurveDefs
{
	static const double SurveyTimeoutSeconds = 300.0;
	static const float WaitSleepSeconds = 2.0;
}

bool FWindowsPlatformSurvey::bSurveyPending = false;
bool FWindowsPlatformSurvey::bSurveyComplete = false;
bool FWindowsPlatformSurvey::bSurveyFailed = false;
double FWindowsPlatformSurvey::SurveyStartTimeSeconds = 0.0;
FHardwareSurveyResults FWindowsPlatformSurvey::Results;

bool FWindowsPlatformSurvey::GetSurveyResults( FHardwareSurveyResults& OutResults, bool bWait )
{
	// Early out failed state
	if (bSurveyFailed)
	{
		return false;
	}

	if (!bSurveyComplete)
	{
		// Tick survey process
		double StartWaitTime = FPlatformTime::Seconds();
		do 
		{
			if (!bSurveyPending)
			{
				// start survey
				BeginSurveyHardware();
			}
			else
			{
				// tick pending survey
				TickSurveyHardware(Results);
			}

			if (bWait && bSurveyPending)
			{
				FPlatformProcess::Sleep(WindowsPlatformSurveDefs::WaitSleepSeconds);
			}

		} while (bWait && bSurveyPending);
	}

	if (bSurveyComplete)
	{
		OutResults = Results;
	}
	return bSurveyComplete;
}

void FWindowsPlatformSurvey::BeginSurveyHardware()
{
	if (bSurveyPending)
	{
		UE_LOG(LogWindows, Error, TEXT("FWindowsPlatformSurvey::BeginSurveyHardware() survey already in-progress") );
		bSurveyFailed = true;
		return;
	}

	// Get dxdiag filepath

	TCHAR System32Path[MAX_PATH];
	// get the System32 directory
	if (S_OK != SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_CURRENT, System32Path))
	{
		UE_LOG(LogWindows, Error, TEXT("FWindowsPlatformSurvey::BeginSurveyHardware() failed to get system folder CSIDL_SYSTEM from SHGetFolderPath") );
		bSurveyFailed = true;
		return;
	}

	FString DxDiagFilepath = FString(System32Path) + TEXT("/dxdiag.exe");
	if (0 >= IFileManager::Get().FileSize(*DxDiagFilepath))
	{
		UE_LOG(LogWindows, Error, TEXT("FWindowsPlatformSurvey::BeginSurveyHardware() file not found %s"), *DxDiagFilepath );
		bSurveyFailed = true;
		return;
	}

	// Generate a temp output filepath
	FString OutputFilepath = GetDxDiagOutputFilepath();

	// Make sure the directory exists before we run dxdiag.  It won't create a directory for us (it will instead just silently do nothing.)
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputFilepath), true);

	// Delete existing output file
	IFileManager::Get().Delete(*OutputFilepath);

	// Convert paths passed to CreateProc() to Windows format
	FPaths::MakePlatformFilename(DxDiagFilepath);
	FPaths::MakePlatformFilename(OutputFilepath);

	// Run dxdiag as a external process, outputting to a text file
	FString ProcessArgs = FString::Printf(TEXT("/t %s"), *OutputFilepath);
	if (!FPlatformProcess::CreateProc(*DxDiagFilepath, *ProcessArgs, true, false, false, NULL, 0, NULL, NULL ).IsValid())
	{
		UE_LOG(LogWindows, Error, TEXT("FWindowsPlatformSurvey::BeginSurveyHardware() couldn't start up the dxdiag process"));
		bSurveyFailed = true;
		return;
	}

	SurveyStartTimeSeconds = FPlatformTime::Seconds();
	bSurveyPending = true;
}

void FWindowsPlatformSurvey::TickSurveyHardware( FHardwareSurveyResults& OutResults )
{
	if (!bSurveyPending)
	{
		bSurveyFailed = true;
		return;
	}

	if (WindowsPlatformSurveDefs::SurveyTimeoutSeconds < FPlatformTime::Seconds() - SurveyStartTimeSeconds)
	{
		UE_LOG(LogWindows, Error, TEXT("FWindowsPlatformSurvey::EndSurveyHardware() survey timed out") );
		bSurveyPending = false;
		bSurveyFailed = true;
		return;
	}

	FString OutputFilepath = GetDxDiagOutputFilepath();

	// First attempt to open the text file then if it's there read the contents into a buffer
	// Wait for the file to appear from the process started in BeginSurveyHardware()
	if (0 >= IFileManager::Get().FileSize(*OutputFilepath))
	{
		// output file not yet created or missing
		return;
	}

	// Failure to read the file when present could be because it is still being written by dxdiag
	TArray<FString> DxdiagLines;
	if( !FFileHelper::LoadANSITextFileToStrings( *OutputFilepath, &IFileManager::Get(), DxdiagLines ) )
	{
		// output file not yet complete/unlocked
		return;
	}

	// Check that we're running on Vista or newer (version 6.0+).
	bool bIsVistaOrNewer = FWindowsPlatformMisc::VerifyWindowsMajorVersion(6);

	FMemory::Memset(&OutResults, 0, sizeof(FHardwareSurveyResults));

	bSurveyPending = false;

	WriteFStringToResults(OutResults.Platform, TEXT("Windows"));

	// Parse the dxdiag output and fill the results structure

	// Get memory
	OutResults.MemoryMB = -1;
	FString MemoryString;
	if (GetLineFollowing(TEXT("Available OS Memory: "), DxdiagLines, MemoryString))
	{
		int32 MBIdx = MemoryString.Find(TEXT("MB RAM"));
		if (1 <= MBIdx)
		{
			MemoryString = MemoryString.Left(MBIdx);
			if (MemoryString.IsNumeric())
			{
				TTypeFromString<uint32>::FromString(OutResults.MemoryMB, *MemoryString);
			}
		}
		else
		{
			OutResults.ErrorCount++;
			WriteFStringToResults(OutResults.LastSurveyError, TEXT("Dxdiag: can't find \"MB RAM\" in line beginning \"Available OS Memory:\""));
			WriteFStringToResults(OutResults.LastSurveyErrorDetail, MemoryString);
		}
	}
	else
	{
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("Dxdiag: can't find line beginning \"Available OS Memory:\""));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}

	if (-1 == OutResults.MemoryMB)
	{
		UE_LOG(LogWindows, Warning, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to parse available memory from dxdiag report") );
	}

	// Get DX version
	FString DirectXVerString;
	if (GetLineFollowing(TEXT("DirectX Version: "), DxdiagLines, DirectXVerString))
	{
		WriteFStringToResults(OutResults.MultimediaAPI, DirectXVerString);
	}
	else
	{
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("Dxdiag: can't find line beginning \"DirectX Version:\""));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}	

	// Get processor string
	FString ProcessorString;
	if (GetLineFollowing(TEXT("Processor: "), DxdiagLines, ProcessorString))
	{
		WriteFStringToResults(OutResults.CPUNameString, ProcessorString);
	}
	else
	{
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("Dxdiag: can't find line beginning \"Processor:\""));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}	

	// Identify "Display Devices" section
	OutResults.DisplayCount = 0;
	TArray<FString> DisplaySectionLines;
	if (GetNamedSection(TEXT("Display Devices"), DxdiagLines, DisplaySectionLines))
	{
		for (; OutResults.DisplayCount < FHardwareSurveyResults::MaxDisplayCount; OutResults.DisplayCount++)
		{
			FHardwareDisplay& Display = OutResults.Displays[OutResults.DisplayCount];

			// Get the card name
			WriteFStringToResults(Display.GPUCardName, TEXT(""));
			FString GPUCardString;
			if (GetLineFollowing(TEXT("Card name: "), DisplaySectionLines, GPUCardString, OutResults.DisplayCount))
			{
				WriteFStringToResults(Display.GPUCardName, GPUCardString);
			}
			else
			{
				// no more displays
				break;
			}

			// Get the display mode
			Display.CurrentModeWidth = -1;
			Display.CurrentModeHeight = -1;
			FString DispMode;
			if (GetLineFollowing(TEXT("Current Mode: "), DisplaySectionLines, DispMode, OutResults.DisplayCount))
			{
				// split DispMode which should be formatted thus "<width> x <height> (details)"
				FString WidthString;
				FString HeightString;
				if (DispMode.Split(TEXT(" x "), &WidthString, &HeightString))
				{
					int32 EndIdx;
					if (HeightString.FindChar(TCHAR(' '), EndIdx))
					{
						TTypeFromString<uint32>::FromString(Display.CurrentModeWidth, *WidthString);
						TTypeFromString<uint32>::FromString(Display.CurrentModeHeight, *HeightString.Left(EndIdx));
					}
					else
					{
						OutResults.ErrorCount++;
						WriteFStringToResults(OutResults.LastSurveyError, FString::Printf(TEXT("Dxdiag: can't find trailing space char in line beginning \"Current Mode:\" for display %d"), OutResults.DisplayCount));
						WriteFStringToResults(OutResults.LastSurveyErrorDetail, DispMode);
					}
				}
				else
				{
					OutResults.ErrorCount++;
					WriteFStringToResults(OutResults.LastSurveyError, FString::Printf(TEXT("Dxdiag: can't find \" x \" in line beginning \"Current Mode:\" for display %d"), OutResults.DisplayCount));
					WriteFStringToResults(OutResults.LastSurveyErrorDetail, DispMode);
				}
			}
			else
			{
				OutResults.ErrorCount++;
				WriteFStringToResults(OutResults.LastSurveyError, FString::Printf(TEXT("Dxdiag: can't find line beginning \"Current Mode:\" for display %d"), OutResults.DisplayCount));
				WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
			}

			// Get GPU memory
			Display.GPUDedicatedMemoryMB = -1;
			FString GPUMemoryString;
			if (GetLineFollowing(TEXT("Dedicated Memory: "), DisplaySectionLines, GPUMemoryString, OutResults.DisplayCount))
			{
				int32 MBIdx = GPUMemoryString.Find(TEXT(" MB"));
				if (1 <= MBIdx)
				{
					GPUMemoryString = GPUMemoryString.Left(MBIdx);
					if (GPUMemoryString.IsNumeric())
					{
						TTypeFromString<uint32>::FromString(Display.GPUDedicatedMemoryMB, *GPUMemoryString);
					}
					else
					{
						OutResults.ErrorCount++;
						WriteFStringToResults(OutResults.LastSurveyError, FString::Printf(TEXT("Dxdiag: can't parse integer in line beginning \"Dedicated Memory:\" for display %d"), OutResults.DisplayCount));
						WriteFStringToResults(OutResults.LastSurveyErrorDetail, GPUMemoryString);
					}
				}
				else
				{
					OutResults.ErrorCount++;
					WriteFStringToResults(OutResults.LastSurveyError, FString::Printf(TEXT("Dxdiag: can't find \" MB\" in line beginning \"Dedicated Memory:\" for display %d"), OutResults.DisplayCount));
					WriteFStringToResults(OutResults.LastSurveyErrorDetail, GPUMemoryString);
				}
			}
			else
			{
				OutResults.ErrorCount++;
				WriteFStringToResults(OutResults.LastSurveyError, FString::Printf(TEXT("Dxdiag: can't find line beginning \"Dedicated Memory:\" for display %d"), OutResults.DisplayCount));
				WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
			}

			// Get the card driver version
			WriteFStringToResults(Display.GPUDriverVersion, TEXT(""));
			FString GPUDriverString;
			if (GetLineFollowing(TEXT("Driver Version: "), DisplaySectionLines, GPUDriverString, OutResults.DisplayCount))
			{
				WriteFStringToResults(Display.GPUDriverVersion, GPUDriverString);
			}
			else
			{
				OutResults.ErrorCount++;
				WriteFStringToResults(OutResults.LastSurveyError, FString::Printf(TEXT("Dxdiag: can't find line beginning \"Driver Version:\" for display %d"), OutResults.DisplayCount));
				WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
			}
		}
	}
	else
	{
		UE_LOG(LogWindows, Warning, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get UE4 root-folder drive size from Win32") );
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("Dxdiag: can't find section beginning \"Display Devices\""));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}

	if (OutResults.DisplayCount == 0)
	{
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("Dxdiag: display count zero"));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}
	else if (OutResults.DisplayCount > 3)
	{
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, FString::Printf(TEXT("Dxdiag: display count %d"), OutResults.DisplayCount));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}

	// Get system info
	SYSTEM_INFO SystemInfo;
	if (FPlatformMisc::Is64bitOperatingSystem())
	{
		GetNativeSystemInfo(&SystemInfo);
	}
	else
	{
		GetSystemInfo(&SystemInfo);
	}

	// Get CPU count from SystemInfo
	OutResults.CPUCount = SystemInfo.dwNumberOfProcessors;

	ISynthBenchmark::Get().Run(OutResults.SynthBenchmark);

	// Get CPU speed
	if (OutResults.CPUCount > 0)
	{
#if USING_POWRPROF
		PROCESSOR_POWER_INFORMATION* PowerInfo = new PROCESSOR_POWER_INFORMATION[OutResults.CPUCount];
		if(PowerInfo != NULL)
		{
			uint32 PowerInfoSize = sizeof(PROCESSOR_POWER_INFORMATION) * OutResults.CPUCount;
			FMemory::Memset(PowerInfo, 0, PowerInfoSize);

			NTSTATUS NTStatus = CallNtPowerInformation(ProcessorInformation, NULL, 0L, (PVOID)PowerInfo, PowerInfoSize);
			if (NT_SUCCESS(NTStatus))
			{
				OutResults.CPUClockGHz = 0.001f * PowerInfo[0].MaxMhz;
			}
			else
			{
				OutResults.ErrorCount++;
				WriteFStringToResults(OutResults.LastSurveyError, TEXT("CallNtPowerInformation() failed to get processor power info"));
				WriteFStringToResults(OutResults.LastSurveyErrorDetail, FString::Printf(TEXT("NTSTATUS: 0x%0x"), NTStatus));
			}
			delete [] PowerInfo;
		}
		else
		{
			OutResults.ErrorCount++;
			WriteFStringToResults(OutResults.LastSurveyError, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get processor count"));
			WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
		}
#endif
	}
	else
	{
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get processor count from GetSystemInfo()"));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}

	// Get CPU brand
	FString CPUBrand = FWindowsPlatformMisc::GetCPUVendor();
	WriteFStringToResults(OutResults.CPUBrand, CPUBrand);
	if (CPUBrand.Len() == 0)
	{
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get processor brand from FWindowsPlatformMisc::GetCPUVendor()"));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}

	// Get CPU info
	OutResults.CPUInfo = FWindowsPlatformMisc::GetCPUInfo();

	// get HDD details
	OutResults.HardDriveGB = -1;
	ULARGE_INTEGER TotalBytes;
	if (GetDiskFreeSpaceEx(FPlatformProcess::BaseDir(), NULL, &TotalBytes, NULL))
	{
		OutResults.HardDriveGB = (TotalBytes.HighPart << 2) | (TotalBytes.LowPart >> 30);
	}
	else
	{
		uint32 ErrorCode = FPlatformMisc::GetLastError();
		UE_LOG(LogWindows, Warning, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get UE4 root-folder drive size from Win32") );
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("GetDiskFreeSpaceEx() failed"));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, FString::Printf(TEXT("ErrorCode: 0x%0x"), ErrorCode));
	}

	// OS info
	GetOSVersionLabels(SystemInfo, OutResults);
	OutResults.OSBits = FPlatformMisc::Is64bitOperatingSystem() ? 64 : 32;

	// OS language
	LCID DefaultLocale = GetSystemDefaultLCID();
	const int32 MaxLocaleStringLength = 9;
	TCHAR LangBuffer[MaxLocaleStringLength];
	int LangReturn = GetLocaleInfo(DefaultLocale, LOCALE_SISO639LANGNAME, LangBuffer, MaxLocaleStringLength * sizeof(TCHAR));
	TCHAR CountryBuffer[MaxLocaleStringLength];
	int CountryReturn = GetLocaleInfo(DefaultLocale, LOCALE_SISO3166CTRYNAME, CountryBuffer, MaxLocaleStringLength * sizeof(TCHAR));

	if (LangReturn == 0 || CountryReturn == 0)
	{
		uint32 ErrorCode = FPlatformMisc::GetLastError();
		UE_LOG(LogWindows, Warning, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get locale info from Win32") );
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("GetLocaleInfo() failed"));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, FString::Printf(TEXT("ErrorCode: 0x%0x"), ErrorCode));
	}
	else
	{
		WriteFStringToResults(OutResults.OSLanguage, FString::Printf(TEXT("%s-%s"), LangBuffer, CountryBuffer));
	}

#if USING_WINSAT_API
	// Use Windows System Assessment Tool?
	if (bIsVistaOrNewer)
	{
		// Get an instance to the most recent formal WinSAT assessmenet.
		IQueryRecentWinSATAssessment* Assessment;
		HRESULT COMResult = CoCreateInstance(
			__uuidof(CQueryWinSAT),
			NULL,
			CLSCTX_INPROC_SERVER,
			__uuidof(IQueryRecentWinSATAssessment),
			(void**)&Assessment);

		if (FAILED(COMResult))
		{
			UE_LOG(LogWindows, Warning, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get query interface from WinSAT API") );
			OutResults.ErrorCount++;
			WriteFStringToResults(OutResults.LastSurveyError, TEXT("CoCreateInstance() failed to get WinSAT"));
			WriteFStringToResults(OutResults.LastSurveyErrorDetail, FString::Printf(TEXT("HRESULT: 0x%0x"), COMResult));

		}
		else
		{
			// Get the summary information for the WinSAT assessment.
			IProvideWinSATResultsInfo* WinSATResults = NULL;
			COMResult = Assessment->get_Info(&WinSATResults);
			if (FAILED(COMResult))
			{
				UE_LOG(LogWindows, Error, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get assessment results from WinSAT API") );
				OutResults.ErrorCount++;
				WriteFStringToResults(OutResults.LastSurveyError, TEXT("get_Info() failed to get WinSAT assessment results"));
				WriteFStringToResults(OutResults.LastSurveyErrorDetail, FString::Printf(TEXT("HRESULT: 0x%0x"), COMResult));

			}
			else
			{
				// Get the state of the assessment.
				WINSAT_ASSESSMENT_STATE WinSATState;
				COMResult = WinSATResults->get_AssessmentState(&WinSATState);
				if (FAILED(COMResult))
				{
					UE_LOG(LogWindows, Error, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get assessment state from WinSAT API") );
					OutResults.ErrorCount++;
					WriteFStringToResults(OutResults.LastSurveyError, TEXT("get_AssessmentState() failed to get WinSAT assessment state"));
					WriteFStringToResults(OutResults.LastSurveyErrorDetail, FString::Printf(TEXT("HRESULT: 0x%0x"), COMResult));
				}
				else
				{
					// Examine the assessment state
					bool bAssessmentAvailable = false;
					switch(WinSATState)
					{
					case WINSAT_ASSESSMENT_STATE_VALID:
						bAssessmentAvailable = true;
						break;

					case WINSAT_ASSESSMENT_STATE_INCOHERENT_WITH_HARDWARE:
						UE_LOG(LogWindows, Warning, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() WinSAT assessment state is out-of-date. Unable to examine some hardware metrics. Run the Windows Experience Index Assessment.") );
						OutResults.ErrorCount++;
						WriteFStringToResults(OutResults.LastSurveyError, TEXT("WinSAT assessment out-of-date. Using old results."));
						WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
						bAssessmentAvailable = true;
						break;

					case WINSAT_ASSESSMENT_STATE_NOT_AVAILABLE:
						UE_LOG(LogWindows, Warning, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() WinSAT assessment unavailable. Unable to examine some hardware metrics. Run the Windows Experience Index Assessment.") );
						OutResults.ErrorCount++;
						WriteFStringToResults(OutResults.LastSurveyError, TEXT("WinSAT assessment unavailable. User hasn't run Windows Experience Index Assessment."));
						WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
						break;

					default:
						UE_LOG(LogWindows, Warning, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() WinSAT assessment data was invalid.") );
						OutResults.ErrorCount++;
						WriteFStringToResults(OutResults.LastSurveyError, TEXT("WinSAT assessment state unknown"));
						WriteFStringToResults(OutResults.LastSurveyErrorDetail, FString::Printf(TEXT("WinSATState: %d"), (int32)WinSATState));
					}

					// Get the index scores from the results
					if (bAssessmentAvailable)
					{
						if (!GetSubComponentIndex(WinSATResults, OutResults, WINSAT_ASSESSMENT_MEMORY, OutResults.RAMPerformanceIndex))
						{
							UE_LOG(LogWindows, Error, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get MEMORY score from WinSAT API.") );
						}

						if (!GetSubComponentIndex(WinSATResults, OutResults, WINSAT_ASSESSMENT_CPU, OutResults.CPUPerformanceIndex))
						{
							UE_LOG(LogWindows, Error, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get CPU score from WinSAT API.") );
						}

						float GPU3DScoreIndex = 0.0f;
						if (!GetSubComponentIndex(WinSATResults, OutResults, WINSAT_ASSESSMENT_D3D, GPU3DScoreIndex))
						{
							UE_LOG(LogWindows, Error, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get D3D score from WinSAT API.") );
						}

						float GPUDesktopScoreIndex = 0.0f;
						if (!GetSubComponentIndex(WinSATResults, OutResults, WINSAT_ASSESSMENT_D3D, GPUDesktopScoreIndex))
						{
							UE_LOG(LogWindows, Error, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get GRAPHICS score from WinSAT API.") );
						}

						OutResults.GPUPerformanceIndex = 0.5f * (GPU3DScoreIndex + GPUDesktopScoreIndex);
					}
				}
			}

			if (WinSATResults)
			{
				WinSATResults->Release();
			}
		}

		if (Assessment)
		{
			Assessment->Release();
		}
	}
	else
	{
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("WIE failed. Not supported on this version of Windows."));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, TEXT(""));
	}
#endif	// #if USING_WINSAT_API

	// Get system power info to determine whether we're running on a laptop or desktop computer
	OutResults.bIsLaptopComputer = false;
#if USING_POWRPROF
	SYSTEM_POWER_CAPABILITIES PowerCaps;
	NTSTATUS NTStatus = CallNtPowerInformation(SystemPowerCapabilities, NULL, 0L, (PVOID)&PowerCaps, sizeof(SYSTEM_POWER_CAPABILITIES));
	if (NT_SUCCESS(NTStatus))
	{
		OutResults.bIsLaptopComputer = PowerCaps.SystemBatteriesPresent && !PowerCaps.BatteriesAreShortTerm;
	}
	else
	{
		UE_LOG(LogWindows, Warning, TEXT("FWindowsPlatformSurvey::TickSurveyHardware() failed to get system power capabilities. Assuming desktop PC.") );
		OutResults.ErrorCount++;
		WriteFStringToResults(OutResults.LastSurveyError, TEXT("CallNtPowerInformation() failed to get system power capabilities"));
		WriteFStringToResults(OutResults.LastSurveyErrorDetail, FString::Printf(TEXT("NTSTATUS: 0x%0x"), NTStatus));
	}
#endif	// #if USING_POWRPROF

	// Get remote desktop session status
	OutResults.bIsRemoteSession = GetSystemMetrics(SM_REMOTESESSION) != 0;

	bSurveyComplete = true;
}

FString FWindowsPlatformSurvey::GetDxDiagOutputFilepath()
{
	FString RelativePath = FPaths::Combine(*FPaths::GameSavedDir(), TEXT( "HardwareSurvey" ), TEXT("dxdiag.txt"));
	return FPaths::ConvertRelativePathToFull(RelativePath);
}

bool FWindowsPlatformSurvey::GetSubComponentIndex( IProvideWinSATResultsInfo* WinSATResults, FHardwareSurveyResults& OutSurveyResults, int32 SubComponent, float& OutSubComponentIndex ) 
{
	bool bSuccess = false;

#if USING_WINSAT_API
	IProvideWinSATAssessmentInfo* AssessmentInfo = NULL;
	HRESULT COMResult = WinSATResults->GetAssessmentInfo((WINSAT_ASSESSMENT_TYPE)SubComponent, &AssessmentInfo);
	if (FAILED(COMResult))
	{
		UE_LOG(LogWindows, Log, TEXT("FWindowsPlatformSurvey::GetSubComponentIndex() failed to get assessment info for a sub-component from WinSAT API.") );
		OutSurveyResults.ErrorCount++;
		WriteFStringToResults(OutSurveyResults.LastPerformanceIndexError, FString::Printf(TEXT("GetAssessmentInfo() failed to get WinSAT assessment for sub-component %d"), SubComponent));
		WriteFStringToResults(OutSurveyResults.LastPerformanceIndexErrorDetail, FString::Printf(TEXT("HRESULT: 0x%0x"), COMResult));
	}
	else
	{
		OutSubComponentIndex = 0.0f;
		COMResult = AssessmentInfo->get_Score(&OutSubComponentIndex);
		if (FAILED(COMResult))
		{
			UE_LOG(LogWindows, Log, TEXT("FWindowsPlatformSurvey::GetSubComponentIndex() failed to get sub-component score from WinSAT API.") );
			OutSurveyResults.ErrorCount++;
			WriteFStringToResults(OutSurveyResults.LastPerformanceIndexError, FString::Printf(TEXT("get_Score() failed to get WinSAT WIE score for sub-component %d"), SubComponent));
			WriteFStringToResults(OutSurveyResults.LastPerformanceIndexErrorDetail, FString::Printf(TEXT("HRESULT: 0x%0x"), COMResult));
		}
		else
		{
			bSuccess = true;
		}
	}

	if (AssessmentInfo)
	{
		AssessmentInfo->Release();
	}
#endif

	return bSuccess;
}

void FWindowsPlatformSurvey::GetOSVersionLabels(const SYSTEM_INFO& SystemInfo, FHardwareSurveyResults& Results)
{
	OSVERSIONINFOEX OsVersionInfo = { 0 };
	OsVersionInfo.dwOSVersionInfoSize = sizeof( OSVERSIONINFOEX );
	FString OSVersionLabel = TEXT("Windows (unknown version)");
	FString OSSubVersionLabel;
#pragma warning(disable : 4996) // 'function' was declared deprecated
	if (GetVersionEx( ( LPOSVERSIONINFO )&OsVersionInfo ))
#pragma warning(default : 4996)

	{
		bool bIsInvalidVersion = false;

		switch (OsVersionInfo.dwMajorVersion)
		{
		case 5:
			switch (OsVersionInfo.dwMinorVersion)
			{
			case 0:
				OSVersionLabel = TEXT("Windows 2000");
				if (OsVersionInfo.wProductType == VER_NT_WORKSTATION)
				{
					OSSubVersionLabel = TEXT("Professional");
				}
				else
				{
					if ( OsVersionInfo.wSuiteMask & VER_SUITE_DATACENTER )
					{
						OSSubVersionLabel = TEXT("Datacenter Server");
					}
					else if ( OsVersionInfo.wSuiteMask & VER_SUITE_ENTERPRISE )
					{
						OSSubVersionLabel = TEXT("Advanced Server");
					}
					else
					{
						OSSubVersionLabel = TEXT("Server");
					}
				}
				break;
			case 1:
				OSVersionLabel = TEXT("Windows XP");
				if (OsVersionInfo.wSuiteMask & VER_SUITE_PERSONAL)
				{
					OSSubVersionLabel = TEXT("Home Edition");
				}
				else
				{
					OSSubVersionLabel = TEXT("Professional");
				}
				break;
			case 2:
				if (GetSystemMetrics(SM_SERVERR2))
				{
					OSVersionLabel = TEXT("Windows Server 2003 R2");
				}
				else if (OsVersionInfo.wSuiteMask & VER_SUITE_STORAGE_SERVER)
				{
					OSVersionLabel = TEXT("Windows Storage Server 2003");
				}
				else if (OsVersionInfo.wSuiteMask & VER_SUITE_WH_SERVER)
				{
					OSVersionLabel = TEXT("Windows Home Server");
				}
				else if (OsVersionInfo.wProductType == VER_NT_WORKSTATION && SystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
				{
					OSVersionLabel = TEXT("Windows XP");
					OSSubVersionLabel = TEXT("Professional x64 Edition");
				}
				else
				{
					OSVersionLabel = TEXT("Windows Server 2003");
				}
				break;
			default:
				bIsInvalidVersion = true;
			}
			break;
		case 6:
			switch (OsVersionInfo.dwMinorVersion)
			{
			case 0:
				if (OsVersionInfo.wProductType == VER_NT_WORKSTATION)
				{
					OSVersionLabel = TEXT("Windows Vista");
				}
				else
				{
					OSVersionLabel = TEXT("Windows Server 2008");
				}
				break;
			case 1:
				if (OsVersionInfo.wProductType == VER_NT_WORKSTATION)
				{
					OSVersionLabel = TEXT("Windows 7");
				}
				else
				{
					OSVersionLabel = TEXT("Windows Server 2008 R2");
				}
				break;
			case 2:
				if (OsVersionInfo.wProductType == VER_NT_WORKSTATION)
				{
					OSVersionLabel = TEXT("Windows 8");
				}
				else
				{
					OSVersionLabel = TEXT("Windows Server 2012");
				}
				break;
			default:
				bIsInvalidVersion = true;
			}
			{
				#pragma warning( push )
				#pragma warning( disable: 4191 )	// unsafe conversion from 'type of expression' to 'type required'
				typedef BOOL (WINAPI *LPFN_GETPRODUCTINFO)(DWORD, DWORD, DWORD, DWORD, PDWORD);
				LPFN_GETPRODUCTINFO fnGetProductInfo = (LPFN_GETPRODUCTINFO) GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetProductInfo");
				#pragma warning( pop )
				if (fnGetProductInfo != NULL)
				{
					DWORD Type;
					fnGetProductInfo(OsVersionInfo.dwMajorVersion, OsVersionInfo.dwMinorVersion, 0, 0, &Type);

					switch( Type )
					{
					case PRODUCT_ULTIMATE:
						OSSubVersionLabel = TEXT("Ultimate Edition" );
						break;
					case PRODUCT_PROFESSIONAL:
						OSSubVersionLabel = TEXT("Professional" );
						break;
					case PRODUCT_HOME_PREMIUM:
						OSSubVersionLabel = TEXT("Home Premium Edition" );
						break;
					case PRODUCT_HOME_BASIC:
						OSSubVersionLabel = TEXT("Home Basic Edition" );
						break;
					case PRODUCT_ENTERPRISE:
						OSSubVersionLabel = TEXT("Enterprise Edition" );
						break;
					case PRODUCT_BUSINESS:
						OSSubVersionLabel = TEXT("Business Edition" );
						break;
					case PRODUCT_STARTER:
						OSSubVersionLabel = TEXT("Starter Edition" );
						break;
					case PRODUCT_CLUSTER_SERVER:
						OSSubVersionLabel = TEXT("Cluster Server Edition" );
						break;
					case PRODUCT_DATACENTER_SERVER:
						OSSubVersionLabel = TEXT("Datacenter Edition" );
						break;
					case PRODUCT_DATACENTER_SERVER_CORE:
						OSSubVersionLabel = TEXT("Datacenter Edition (core installation)" );
						break;
					case PRODUCT_ENTERPRISE_SERVER:
						OSSubVersionLabel = TEXT("Enterprise Edition" );
						break;
					case PRODUCT_ENTERPRISE_SERVER_CORE:
						OSSubVersionLabel = TEXT("Enterprise Edition (core installation)" );
						break;
					case PRODUCT_ENTERPRISE_SERVER_IA64:
						OSSubVersionLabel = TEXT("Enterprise Edition for Itanium-based Systems" );
						break;
					case PRODUCT_SMALLBUSINESS_SERVER:
						OSSubVersionLabel = TEXT("Small Business Server" );
						break;
					case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
						OSSubVersionLabel = TEXT("Small Business Server Premium Edition" );
						break;
					case PRODUCT_STANDARD_SERVER:
						OSSubVersionLabel = TEXT("Standard Edition" );
						break;
					case PRODUCT_STANDARD_SERVER_CORE:
						OSSubVersionLabel = TEXT("Standard Edition (core installation)" );
						break;
					case PRODUCT_WEB_SERVER:
						OSSubVersionLabel = TEXT("Web Server Edition" );
						break;
					}
				}
				else
				{
					OSSubVersionLabel = TEXT("(type unknown)");
					Results.ErrorCount++;
					WriteFStringToResults(Results.LastSurveyError, TEXT("Failed to get GetProductInfo() function from GetProcAddress()."));
					WriteFStringToResults(Results.LastSurveyErrorDetail, TEXT(""));
				}
			}
			break;
		default:
			bIsInvalidVersion = true;
		}

#if 0
		// THIS BIT ADDS THE SERVICE PACK INFO TO THE EDITION STRING
		// Append service pack info
		if (OsVersionInfo.szCSDVersion[0] != 0)
		{
			OSSubVersionLabel += FString::Printf(TEXT(" (%s)"), OsVersionInfo.szCSDVersion);
		}
#else
		// THIS BIT USES SERVICE PACK INFO ONLY
		OSSubVersionLabel = OsVersionInfo.szCSDVersion;
#endif

		if (bIsInvalidVersion)
		{
			UE_LOG(LogWindows, Warning, TEXT("FWindowsPlatformSurvey::GetOSVersionLabel() unknown Windows version info from GetVersionEx()") );
			Results.ErrorCount++;
			WriteFStringToResults(Results.LastSurveyError, TEXT("GetVersionEx() returned unknown version"));
			WriteFStringToResults(Results.LastSurveyErrorDetail, FString::Printf(TEXT("dwMajorVersion: %d  dwMinorVersion: %d"), OsVersionInfo.dwMajorVersion, OsVersionInfo.dwMinorVersion));
		}
	}
	else
	{
		uint32 ErrorCode = FPlatformMisc::GetLastError();
		UE_LOG(LogWindows, Warning, TEXT("FWindowsPlatformSurvey::GetOSVersionLabel() failed to get Windows version info from GetVersionEx()") );
		Results.ErrorCount++;
		WriteFStringToResults(Results.LastSurveyError, TEXT("GetVersionEx() failed"));
		WriteFStringToResults(Results.LastSurveyErrorDetail, FString::Printf(TEXT("ErrorCode: 0x%0x"), ErrorCode));
	}

	WriteFStringToResults(Results.OSVersion, OSVersionLabel);
	WriteFStringToResults(Results.OSSubVersion, OSSubVersionLabel);
}

bool FWindowsPlatformSurvey::GetLineFollowing(const FString& Token, const TArray<FString>& InLines, FString& OutString, int32 NthHit)
{
	int32 HitIdx = 0;
	for (int32 LineIdx = 0; LineIdx < InLines.Num(); LineIdx++)
	{
		const FString& Line = InLines[LineIdx];

		int32 SubStrIdx =  Line.Find(Token);
		if (0 <= SubStrIdx && NthHit == HitIdx++)
		{
			OutString = Line.RightChop(SubStrIdx + Token.Len());
			return 0 < OutString.Len();
		}
	}
	return false;
}

void FWindowsPlatformSurvey::WriteFStringToResults(TCHAR* OutBuffer, const FString& InString)
{
	FMemory::Memset( OutBuffer, 0, sizeof(TCHAR) * FHardwareSurveyResults::MaxStringLength );
	TCHAR* Cursor = OutBuffer;
	for (int32 i = 0; i < FMath::Min(InString.Len(), FHardwareSurveyResults::MaxStringLength - 1); i++)
	{
		*Cursor++ = InString[i];
	}
}

bool FWindowsPlatformSurvey::GetNamedSection(FString SectionName, const TArray<FString>& InLines, TArray<FString>& OutSectionLines)
{
	OutSectionLines.Empty();
	int32 SectionStartLine = -1;
	int32 LineIdx = 0;
	for (; LineIdx < InLines.Num(); LineIdx++)
	{
		if (LineIdx < InLines.Num() - 2)
		{
			const FString& StartLine = InLines[LineIdx];

			if (StartLine.StartsWith(TEXT("---")))
			{
				const FString& EndLine = InLines[LineIdx+2];

				if (EndLine.StartsWith(TEXT("---")))
				{
					if (0 <= SectionStartLine)
					{
						break;
					}
					else
					{
						const FString& SectionLine = InLines[LineIdx+1];

						if (SectionLine.StartsWith(SectionName))
						{
							SectionStartLine = LineIdx + 3;
						}
					}
				}
			}
		}
	}

	if (0 <= SectionStartLine)
	{
		for (int32 CopyLineIdx = SectionStartLine; CopyLineIdx < LineIdx; CopyLineIdx++)
		{
			OutSectionLines.Add(InLines[CopyLineIdx]);
		}
	}

	return 0 < OutSectionLines.Num();
}

#include "HideWindowsPlatformTypes.h"
