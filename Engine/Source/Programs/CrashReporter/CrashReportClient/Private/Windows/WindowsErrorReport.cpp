// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"

#include "WindowsErrorReport.h"
#include "XmlFile.h"
#include "CrashDebugHelperModule.h"
#include "../CrashReportUtil.h"

#include "AllowWindowsPlatformTypes.h"
#include <ShlObj.h>
#include "HideWindowsPlatformTypes.h"

#define LOCTEXT_NAMESPACE "CrashReportClient"

namespace
{
	/** Pointer to dynamically loaded crash diagnosis module */
	FCrashDebugHelperModule* CrashHelperModule;
}

/** Helper class used to parse specified string value based on the marker. */
struct FWindowsReportParser
{
	static FString Find( const FString& ReportDirectory, const TCHAR* Marker )
	{
		FString Result;

		TArray<uint8> FileData;
		FFileHelper::LoadFileToArray( FileData, *(ReportDirectory / TEXT( "Report.wer" )) );
		FileData.Add( 0 );
		FileData.Add( 0 );

		const FString FileAsString = reinterpret_cast<TCHAR*>(FileData.GetData());

		TArray<FString> String;
		FileAsString.ParseIntoArray( &String, TEXT( "\r\n" ), true );

		for( const auto& StringLine : String )
		{
			if( StringLine.Contains( Marker ) )
			{
				TArray<FString> SeparatedParameters;
				StringLine.ParseIntoArray( &SeparatedParameters, Marker, true );

				FString MatchedValue;
				const bool bFound = FParse::Value( *StringLine, Marker, MatchedValue );

				if( bFound )
				{
					Result = MatchedValue;
					break;
				}
			}
		}

		return Result;
	}
};

FWindowsErrorReport::FWindowsErrorReport(const FString& Directory)
	: FGenericErrorReport(Directory)
{
	CrashHelperModule = &FModuleManager::LoadModuleChecked<FCrashDebugHelperModule>(FName("CrashDebugHelper"));
}

FWindowsErrorReport::~FWindowsErrorReport()
{
	CrashHelperModule->ShutdownModule();
}

FText FWindowsErrorReport::DiagnoseReport() const
{
	// Should check if there are local PDBs before doing anything
	auto CrashDebugHelper = CrashHelperModule->Get();
	if (!CrashDebugHelper)
	{
		// Not localized: should never be seen
		return FText::FromString(TEXT("Failed to load CrashDebugHelper."));
	}

	FString DumpFilename;
	if (!FindFirstReportFileWithExtension(DumpFilename, TEXT(".dmp")))
	{
		if (!FindFirstReportFileWithExtension(DumpFilename, TEXT(".mdmp")))
		{
			return LOCTEXT("MinidumpNotFound", "No minidump found for this crash.");
		}
	}

	if (!CrashDebugHelper->CreateMinidumpDiagnosticReport(ReportDirectory / DumpFilename))
	{
		if ( FRocketSupport::IsRocket() )
		{
			return LOCTEXT("NoDebuggingSymbolsRocket", "We apologize for the inconvenience.\nPlease send this crash report to help improve our software.");
		}
		else
		{
			return LOCTEXT("NoDebuggingSymbols", "You do not have any debugging symbols required to display the callstack for this crash.");
		}
	}

	// Don't write a Diagnostics.txt to disk in rocket. It will be displayed in the UI but not sent to the server.
	if ( !FRocketSupport::IsRocket() )
	{
		// There's a callstack, so write it out to save the server trying to do it
		CrashDebugHelper->CrashInfo.GenerateReport(ReportDirectory / GDiagnosticsFilename);
	}

	const auto& Exception = CrashDebugHelper->CrashInfo.Exception;
	const FString Assertion = FWindowsReportParser::Find( ReportDirectory, TEXT( "AssertLog=" ) );

	return FormatReportDescription( Exception.ExceptionString, Assertion, Exception.CallStackString );
}

FString FWindowsErrorReport::FindCrashedAppName() const
{
	const FString CrashedAppName = FWindowsReportParser::Find( ReportDirectory, TEXT( "AppPath=" ) );
	return CrashedAppName;
}

FString FWindowsErrorReport::FindMostRecentErrorReport()
{
	auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	auto DirectoryModifiedTime = FDateTime::MinValue();
	FString ReportDirectory;
	auto ReportFinder = MakeDirectoryVisitor([&](const TCHAR* FilenameOrDirectory, bool bIsDirectory) {
		if (bIsDirectory)
		{
			auto TimeStamp = PlatformFile.GetTimeStamp(FilenameOrDirectory);
			if (TimeStamp > DirectoryModifiedTime)
			{
				ReportDirectory = FilenameOrDirectory;
				DirectoryModifiedTime = TimeStamp;
			}
		}
		return true;
	});

	TCHAR LocalAppDataPath[MAX_PATH];
	SHGetFolderPath(0, CSIDL_LOCAL_APPDATA, NULL, 0, LocalAppDataPath);

	PlatformFile.IterateDirectory(
		*(FString(LocalAppDataPath) / TEXT("Microsoft/Windows/WER/ReportQueue")),
		ReportFinder);

	return ReportDirectory;
}

#undef LOCTEXT_NAMESPACE
