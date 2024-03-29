// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;
using System.Diagnostics;
public class OpenAL : ModuleRules
{
	public OpenAL(TargetInfo Target)
	{
		Type = ModuleType.External;
		string version = "1.15.1";

		string OpenALPath = UEBuildConfiguration.UEThirdPartyDirectory + "OpenAL/" + version + "/";
		PublicIncludePaths.Add(OpenALPath + "include");
        
		if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			if (Target.Architecture == "-win32")
			{
				// add libs for OpenAL32 
				PublicAdditionalLibraries.Add(OpenALPath + "lib/Win32/libOpenAL32.dll.a");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicLibraryPaths.Add(OpenALPath + "lib/Linux/");
			PublicAdditionalLibraries.Add("openal");
		}
    }
}
