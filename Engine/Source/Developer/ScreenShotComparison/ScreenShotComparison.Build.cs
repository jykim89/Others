// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ScreenShotComparison : ModuleRules
{
	public ScreenShotComparison(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "EditorStyle",
                "InputCore",
				"ScreenShotComparisonTools",
				"Slate",
				"SlateCore",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"SessionServices",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/ScreenShotComparison/Private",
				"Developer/ScreenShotComparison/Private/Widgets",
				"Developer/ScreenShotComparison/Private/Models",
			}
		);
	}
}
