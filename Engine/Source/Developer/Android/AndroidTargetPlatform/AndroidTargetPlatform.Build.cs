// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AndroidTargetPlatform : ModuleRules
{
	public AndroidTargetPlatform(TargetInfo Target)
	{
		BinariesSubFolder = "Android";

        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"TargetPlatform",
				"AndroidDeviceDetection",
			}
		);

		PublicIncludePaths.AddRange(
			new string[]
			{
				"Runtime/Core/Public/Android"
			}
		);

		if (UEBuildConfiguration.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");		//@todo android: AndroidTargetPlatform.Build
		}

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Developer/AndroidTargetPlatform/Classes"
			}
		);
	}
}