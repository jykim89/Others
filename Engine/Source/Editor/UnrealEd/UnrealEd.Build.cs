// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealEd : ModuleRules
{
	public UnrealEd(TargetInfo Target)
	{
		SharedPCHHeaderFile = "Editor/UnrealEd/Public/UnrealEd.h";

		PrivateIncludePaths.AddRange(
			new string[] 
			{
				"Editor/UnrealEd/Private",
				"Editor/UnrealEd/Private/Settings",
				"Editor/PackagesDialog/Public",
				"Developer/DerivedDataCache/Public",
				"Developer/TargetPlatform/Public",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] 
			{
				"Analytics",
				"AssetRegistry",
				"AssetTools",
                "BehaviorTreeEditor",
				"ClassViewer",
				"ContentBrowser",
				"CrashTracker",
				"DerivedDataCache",
				"DesktopPlatform",
                "EnvironmentQueryEditor",
				"GameProjectGeneration",
				"ImageWrapper",
				"MainFrame",
				"MaterialEditor",
				"MeshUtilities",
				"Messaging",
				"NiagaraEditor",
				"PlacementMode",
				"Settings",
				"SoundClassEditor",
				"ViewportSnapping",
				"SourceCodeAccess",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] 
			{
				"BspMode",
				"Core",
				"CoreUObject",
				"Documentation",
				"Engine",
				"Projects",
				"SandboxFile",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"SourceControl",
				"UnrealEdMessages",
				"BlueprintGraph",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"AnimGraph",
				"BlueprintGraph",
				"OnlineSubsystem",
				"OnlineBlueprintSupport",
				"DirectoryWatcher",
				"EditorSettingsViewer",
				"EditorStyle",
				"EngineSettings",
				"InputCore",
				"InputBindingEditor",
				"LauncherAutomatedService",
				"LauncherServices",
				"MaterialEditor",
				"MessageLog",
				"NetworkFileSystem",
				"ProjectSettingsViewer",
				"PropertyEditor",
				"Projects",
				"RawMesh",
				"RenderCore", 
				"RHI", 
				"ShaderCore", 
				"Sockets",
				"SoundClassEditor",
				"SoundCueEditor",
				"SourceControlWindows", 
				"StatsViewer",
				"SwarmInterface",
				"TargetPlatform",
				"TargetDeviceServices",
				"VectorVM",
                "EditorWidgets",
				"GraphEditor",
				"Kismet",
                "InternationalizationSettings"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] 
			{
				"CrashTracker",
				"MaterialEditor",
				"FontEditor",
				"StaticMeshEditor",
				"TextureEditor",
				"Cascade",
                "UMGEditor",
				"Matinee",
				"AssetRegistry",
				"AssetTools",
				"ClassViewer",
				"CollectionManager",
				"ContentBrowser",
				"CurveTableEditor",
				"DataTableEditor",
				"DestructibleMeshEditor",
				"LandscapeEditor",
				"KismetCompiler",
				"DetailCustomizations",
				"ComponentVisualizers",
				"MainFrame",
				"LevelEditor",
				"InputBindingEditor",
				"PackagesDialog",
				"Persona",
				"PhAT",
				"DeviceManager",
				"SettingsEditor",
				"SessionFrontend",
				"SessionLauncher",
				"TaskBrowser",
				"Sequencer",
				"SoundClassEditor",
				"GeometryMode",
				"TextureAlignMode",
				"FoliageEdit",
				"PackageDependencyInfo",
				"ImageWrapper",
				"Blutility",
				"DesktopPlatform",
				"WorkspaceMenuStructure",
				"BspMode",
				"MeshPaint",
				"PlacementMode",
				"NiagaraEditor",
				"MeshUtilities",
				"GameProjectGeneration",
				"PListEditor",
                "Documentation",
                "BehaviorTreeEditor",
                "EnvironmentQueryEditor",
				"ViewportSnapping",
				"UserFeedback",
				"GameplayTagsEditor",
				"UndoHistory",
				"SourceCodeAccess"
			}
		);

		CircularlyReferencedDependentModules.AddRange(
			new string[] 
			{
                "GraphEditor",
				"Kismet",
            }
		); 


		// Add include directory for Lightmass
		PublicIncludePaths.Add("Programs/UnrealLightmass/Public");

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"UserFeedback",
             	"CollectionManager",
				"BlueprintGraph",
			}
			);

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			PublicDependencyModuleNames.Add("XAudio2");

			AddThirdPartyPrivateStaticDependencies(Target, 
				"UEOgg",
				"Vorbis",
				"VorbisFile",
				"DX11Audio"
				);

			
		}

        if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
            PublicDependencyModuleNames.Add("HTML5Audio");
        }

		AddThirdPartyPrivateStaticDependencies(Target, 
			"HACD",
			"FBX",
			"FreeType2"
		);

		SetupModulePhysXAPEXSupport(Target);

		if ((UEBuildConfiguration.bCompileSimplygon == true) &&
			(Directory.Exists(UEBuildConfiguration.UEThirdPartyDirectory + "NotForLicensees") == true) &&
			(Directory.Exists(UEBuildConfiguration.UEThirdPartyDirectory + "NotForLicensees/Simplygon") == true))
		{
			AddThirdPartyPrivateStaticDependencies(Target, "Simplygon");
		}
		else
		{
			Definitions.Add("WITH_SIMPLYGON=0");
		}

		if (UEBuildConfiguration.bCompileRecast)
		{
			AddThirdPartyPrivateStaticDependencies(Target, "Recast");
			Definitions.Add( "WITH_RECAST=1" );
		}
		else
		{
			Definitions.Add( "WITH_RECAST=0" );
		}

		if ((UEBuildConfiguration.bCompileSpeedTree == true) &&
			(Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32))
		{
			AddThirdPartyPrivateStaticDependencies(Target, "SpeedTree");

			// Because we test WITH_SPEEDTREE in public UnrealEd header files, we need to make sure that modules
			// that import us also have this definition set appropriately.  SpeedTree is a private dependency
			// module, so it's definitions won't propagate to modules that import UnrealEd.
			Definitions.Add("WITH_SPEEDTREE=1");
		}
		else
		{
			Definitions.Add("WITH_SPEEDTREE=0");
		}
	}
}
