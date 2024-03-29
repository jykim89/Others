﻿// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UEOgg : ModuleRules
{
	public UEOgg(TargetInfo Target)
	{
		Type = ModuleType.External;

		string OggPath = UEBuildConfiguration.UEThirdPartyDirectory + "Ogg/libogg-1.2.2/";

		PublicSystemIncludePaths.Add(OggPath + "include");

		string OggLibPath = OggPath + "lib/";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			OggLibPath += "Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add( OggLibPath );

			PublicAdditionalLibraries.Add("libogg_64.lib");

			PublicDelayLoadDLLs.Add("libogg_64.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 )
		{
			OggLibPath += "Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add( OggLibPath );

			PublicAdditionalLibraries.Add("libogg.lib");

			PublicDelayLoadDLLs.Add("libogg.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")
        {
            OggLibPath += "HTML5Win32";
            PublicLibraryPaths.Add(OggLibPath);

            PublicAdditionalLibraries.Add("libogg.lib");
        }
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(OggPath + "macosx/libogg.dylib");
		}
        else if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
            PublicAdditionalLibraries.Add(OggLibPath + "HTML5/libogg.bc");
        }
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			switch (Target.Architecture)
			{
			case "-armv7":
				PublicLibraryPaths.Add(OggLibPath + "Android/ARMv7");
				break;
			case "-arm64":
				PublicLibraryPaths.Add(OggLibPath + "Android/ARM64");
				break;
			case "-x86":
				PublicLibraryPaths.Add(OggLibPath + "Android/x86");
				break;
			case "-x64":
				PublicLibraryPaths.Add(OggLibPath + "Android/x64");
				break;
			}

			PublicAdditionalLibraries.Add("ogg");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicLibraryPaths.Add(OggLibPath + "Linux");
			PublicAdditionalLibraries.Add("ogg");
		}
	}
}

