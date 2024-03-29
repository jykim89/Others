// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlateCore : ModuleRules
{
	public SlateCore(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"InputCore",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/SlateCore/Private",
				"Runtime/SlateCore/Private/Animation",
				"Runtime/SlateCore/Private/Application",
				"Runtime/SlateCore/Private/Brushes",
				"Runtime/SlateCore/Private/Commands",
				"Runtime/SlateCore/Private/Fonts",
				"Runtime/SlateCore/Private/Input",
				"Runtime/SlateCore/Private/Layout",
				"Runtime/SlateCore/Private/Logging",
				"Runtime/SlateCore/Private/Rendering",
				"Runtime/SlateCore/Private/Sound",
				"Runtime/SlateCore/Private/Styling",
				"Runtime/SlateCore/Private/Textures",
				"Runtime/SlateCore/Private/Types",
				"Runtime/SlateCore/Private/Widgets",
			}
		);

        if (!UEBuildConfiguration.bBuildDedicatedServer)
        {
			AddThirdPartyPrivateStaticDependencies(Target, "FreeType2");
        }

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			AddThirdPartyPrivateStaticDependencies(Target, "XInput");
		}
	}
}
