﻿// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenSSL : ModuleRules
{
	public OpenSSL(TargetInfo Target)
	{
		Type = ModuleType.External;

		string LibFolder = "lib/";
		string LibPrefix = "";
		string LibPostfixAndExt = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d." : ".";
		string OpenSSLPath = UEBuildConfiguration.UEThirdPartyDirectory + "OpenSSL/1.0.1g/";
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicIncludePaths.Add(OpenSSLPath + "include");

			LibFolder += "Win64/";
			
			if(WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2013)
			{
				LibFolder += "VS2013/";
			}
			else
			{
				LibFolder += "VS2012/";
			}
			
			LibPostfixAndExt += "lib";
			PublicLibraryPaths.Add(OpenSSLPath + LibFolder);
		}

		PublicAdditionalLibraries.Add(LibPrefix + "libeay32" + LibPostfixAndExt);
		PublicAdditionalLibraries.Add(LibPrefix + "ssleay32" + LibPostfixAndExt);

		PublicDelayLoadDLLs.AddRange(
					   new string[] {
						"libeay32.dll", 
						"ssleay32.dll" 
				   }
				   );
	}
}