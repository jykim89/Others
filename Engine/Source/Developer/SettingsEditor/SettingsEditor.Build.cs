// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SettingsEditor : ModuleRules
{
	public SettingsEditor(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
                "EditorStyle",
				"Slate",
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);

		PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
                "CoreUObject",
                "InputCore",
				"DesktopPlatform",
				"PropertyEditor",
				"SlateCore",
				"SourceControl",
            }
        );

		PrivateIncludePaths.AddRange(
			new string[] {
                "Developer/SettingsEditor/Private",
				"Developer/SettingsEditor/Private/Models",
                "Developer/SettingsEditor/Private/Widgets",
            }
		);
	}
}
