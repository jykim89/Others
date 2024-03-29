// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LevelEditor : ModuleRules
{
	public LevelEditor(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"Kismet",
				"MainFrame",
                "PlacementMode"
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
				"UserFeedback",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Analytics",
				"Core",
				"CoreUObject",
				"DesktopPlatform",
                "InputCore",
				"Slate",
				"SlateCore",
				"SlateReflector",
                "EditorStyle",
				"Engine",
				"MessageLog",
				"NewsFeed",
                "SourceControl",
                "StatsViewer",
				"UnrealEd", 
				"RenderCore",
				"DeviceProfileServices",
				"ContentBrowser",
                "SceneOutliner",
                "ActorPickerMode",
                "RHI"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"PropertyEditor",
				"SceneOutliner",
				"ClassViewer",
				"DeviceManager",
				"SettingsEditor",
				"SessionFrontend",
				"AutomationWindow",
				"Layers",
				"Levels",
                "WorldBrowser",
				"TaskBrowser",
				"EditorWidgets",
				"AssetTools",
				"WorkspaceMenuStructure",
				"NewLevelDialog",
				"DeviceProfileEditor",
				"DeviceProfileServices",
                "PlacementMode",
				"UserFeedback"
			}
		);
	}
}
