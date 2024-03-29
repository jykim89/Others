﻿// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class PhysX : ModuleRules
{
	enum PhysXLibraryMode
	{
		Debug,
		Profile,
		Checked,
		Shipping
	}

	static PhysXLibraryMode GetPhysXLibraryMode(UnrealTargetConfiguration Config)
	{
		switch (Config)
		{
			case UnrealTargetConfiguration.Debug:
				return PhysXLibraryMode.Debug;
			case UnrealTargetConfiguration.Shipping:
			case UnrealTargetConfiguration.Test:
				return PhysXLibraryMode.Shipping;
			case UnrealTargetConfiguration.Development:
			case UnrealTargetConfiguration.DebugGame:
			case UnrealTargetConfiguration.Unknown:
			default:
				return PhysXLibraryMode.Profile;
		}
	}

	static bool bShippingBuildsActuallyUseShippingPhysXLibraries = false;

	static string GetPhysXLibrarySuffix(PhysXLibraryMode Mode)
	{
		switch (Mode)
		{
			case PhysXLibraryMode.Debug:
				{ 
					if( BuildConfiguration.bDebugBuildsActuallyUseDebugCRT )
					{ 
						return "DEBUG";
					}
					else
					{
						return "PROFILE";
					}
				}
			case PhysXLibraryMode.Checked:
				return "CHECKED";
			case PhysXLibraryMode.Profile:
				return "PROFILE";
			default:
			case PhysXLibraryMode.Shipping:
				{
					if( bShippingBuildsActuallyUseShippingPhysXLibraries )
					{
						return "";	
					}
					else
					{
						return "PROFILE";
					}
				}
		}
	}

	public PhysX(TargetInfo Target)
	{
		Type = ModuleType.External;

		// Determine which kind of libraries to link against
		PhysXLibraryMode LibraryMode = GetPhysXLibraryMode(Target.Configuration);
		string LibrarySuffix = GetPhysXLibrarySuffix(LibraryMode);

		Definitions.Add("WITH_PHYSX=1");
		if (UEBuildConfiguration.bCompileAPEX == false)
		{
			// Since APEX is dependent on PhysX, if APEX is not being include, set the flag properly.
			// This will properly cover the case where PhysX is compiled but APEX is not.
			Definitions.Add("WITH_APEX=0");
		}

		string PhysXDir = UEBuildConfiguration.UEThirdPartyDirectory + "PhysX/PhysX-3.3/";

		string PhysXLibDir = PhysXDir + "lib/";

		PublicSystemIncludePaths.AddRange(
			new string[] {
				PhysXDir + "include",
				PhysXDir + "include/foundation",
				PhysXDir + "include/cooking",
				PhysXDir + "include/common",
				PhysXDir + "include/extensions",
				PhysXDir + "include/geometry",
				PhysXDir + "include/vehicle"
			}
			);

		// Libraries and DLLs for windows platform
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.Add(PhysXDir + "include/foundation/windows");

			PhysXLibDir += "Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(PhysXLibDir);

			string[] StaticLibrariesX64 = new string[] {
				"PhysX3{0}_x64.lib",
				"PhysX3Extensions{0}.lib",
				"PhysX3Cooking{0}_x64.lib",
				"PhysX3Common{0}_x64.lib",
				"PhysX3Vehicle{0}.lib",
				"PxTask{0}.lib",
				"PhysXVisualDebuggerSDK{0}.lib",
				"PhysXProfileSDK{0}.lib"
			};

			string[] DelayLoadDLLsX64 = new string[] {
				"PhysX3{0}_x64.dll", 
				"PhysX3Cooking{0}_x64.dll",
				"PhysX3Common{0}_x64.dll"
			};

			foreach (string Lib in StaticLibrariesX64)
			{
				PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
			}

			foreach (string DLL in DelayLoadDLLsX64)
			{
				PublicDelayLoadDLLs.Add(String.Format(DLL, LibrarySuffix));
			}
	
            PublicDelayLoadDLLs.Add("nvToolsExt64_1.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 || (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
		{
			PublicIncludePaths.Add(PhysXDir + "include/foundation/windows");

			PhysXLibDir += "Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(PhysXLibDir);

			string[] StaticLibrariesX86 = new string[] {
				"PhysX3{0}_x86.lib",
				"PhysX3Extensions{0}.lib",
				"PhysX3Cooking{0}_x86.lib",
				"PhysX3Common{0}_x86.lib",
				"PhysX3Vehicle{0}.lib",
				"PxTask{0}.lib",
				"PhysXVisualDebuggerSDK{0}.lib",
				"PhysXProfileSDK{0}.lib"
			};

			string[] DelayLoadDLLsX86 = new string[] {
				"PhysX3{0}_x86.dll", 
				"PhysX3Cooking{0}_x86.dll",
				"PhysX3Common{0}_x86.dll"
			};

			foreach (string Lib in StaticLibrariesX86)
			{
				PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
			}

			foreach (string DLL in DelayLoadDLLsX86)
			{
				PublicDelayLoadDLLs.Add(String.Format(DLL, LibrarySuffix));
			}

            PublicDelayLoadDLLs.Add("nvToolsExt32_1.dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicSystemIncludePaths.Add(PhysXDir + "include/foundation/unix");

			PhysXLibDir += "/osx64";
			PublicLibraryPaths.Add(PhysXLibDir);

			string[] StaticLibrariesMac = new string[] {
				PhysXLibDir + "/libLowLevel{0}.a",
				PhysXLibDir + "/libLowLevelCloth{0}.a",
				PhysXLibDir + "/libPhysX3{0}.a",
				PhysXLibDir + "/libPhysX3CharacterKinematic{0}.a",
				PhysXLibDir + "/libPhysX3Extensions{0}.a",
				PhysXLibDir + "/libPhysX3Cooking{0}.a",
				PhysXLibDir + "/libPhysX3Common{0}.a",
				PhysXLibDir + "/libPhysX3Vehicle{0}.a",
				PhysXLibDir + "/libPxTask{0}.a",
				PhysXLibDir + "/libPhysXVisualDebuggerSDK{0}.a",
				PhysXLibDir + "/libPhysXProfileSDK{0}.a",
				PhysXLibDir + "/libPvdRuntime{0}.a",
				PhysXLibDir + "/libSceneQuery{0}.a",
				PhysXLibDir + "/libSimulationController{0}.a"
			};

			foreach (string Lib in StaticLibrariesMac)
			{
				PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			if (Target.Architecture == "-armv7")
			{
				PhysXLibDir += "/Android/ARMv7";
			}
			else
			{
				PhysXLibDir += "/Android/x86";
			}

			PublicSystemIncludePaths.Add(PhysXDir + "include/foundation/unix");
			PublicLibraryPaths.Add(PhysXLibDir);

			string[] StaticLibrariesAndroid = new string[] {
				"LowLevel{0}",
				"LowLevelCloth{0}",
				"PhysX3{0}",
				"PhysX3CharacterKinematic{0}",
				"PhysX3Extensions{0}",
				"PhysX3Cooking{0}",
				"PhysX3Common{0}",
				"PhysX3Vehicle{0}",
				"PxTask{0}",
				"PhysXVisualDebuggerSDK{0}",
				"PhysXProfileSDK{0}",
				"PvdRuntime{0}",
				"SceneQuery{0}",
				"SimulationController{0}",
			};

			// the "shipping" don't need the nvTools library
			if (LibraryMode != PhysXLibraryMode.Shipping || !bShippingBuildsActuallyUseShippingPhysXLibraries)
			{
				PublicAdditionalLibraries.Add("nvToolsExt");
			}

			foreach (string Lib in StaticLibrariesAndroid)
			{
                if (!Lib.Contains("Cooking") || Target.IsCooked == false)
                {
                    PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
                }
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PhysXLibDir += "/Linux";

			PublicSystemIncludePaths.Add(PhysXDir + "include/foundation/unix");
			PublicLibraryPaths.Add(PhysXLibDir);

			string[] StaticLibrariesLinux = new string[] {
				"rt",
				"LowLevel{0}",
				"LowLevelCloth{0}",
				"PhysX3{0}",
				"PhysX3CharacterKinematic{0}",
				"PhysX3Extensions{0}",
				"PhysX3Cooking{0}",
				"PhysX3Common{0}",
				"PhysX3Vehicle{0}",
				"PxTask{0}",
				"PhysXVisualDebuggerSDK{0}",
				"PhysXProfileSDK{0}",
				"PvdRuntime{0}",
				"SceneQuery{0}",
				"SimulationController{0}",
			};

			if (LibraryMode == PhysXLibraryMode.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
				//@TODO: Needed?
				PublicAdditionalLibraries.Add("m");
			}

			foreach (string Lib in StaticLibrariesLinux)
			{
				PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicSystemIncludePaths.Add(PhysXDir + "include/foundation/unix");

			PhysXLibDir = Path.Combine(PhysXLibDir, "IOS/");
			PublicLibraryPaths.Add(PhysXLibDir);

			string[] PhysXLibs = new string[] 
				{
                    "LowLevel",
                    "LowLevelCloth",
					"PhysX3",
					"PhysX3CharacterKinematic",
					"PhysX3Common",
					"PhysX3Cooking",
					"PhysX3Extensions",
					"PhysX3Vehicle",
					"PxTask",
					"PhysXVisualDebuggerSDK",
					"PhysXProfileSDK",
					"PvdRuntime",
					"SceneQuery",
					"SimulationController",

				};

			foreach (string PhysXLib in PhysXLibs)
			{
                if (!PhysXLib.Contains("Cooking") || Target.IsCooked == false)
                {
                    PublicAdditionalLibraries.Add(PhysXLib + LibrarySuffix);
                    PublicAdditionalShadowFiles.Add(Path.Combine(PhysXLibDir, "lib" + PhysXLib + LibrarySuffix + ".a"));
                }
			}
		}
        else if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
            PublicSystemIncludePaths.Add(PhysXDir + "include/foundation/unix");

            PhysXLibDir = Path.Combine(PhysXLibDir, "HTML5/");

            string[] PhysXLibs = new string[] 
				{
                    "LowLevel",
                    "LowLevelCloth",
					"PhysX3",
					"PhysX3CharacterKinematic",
					"PhysX3Common",
					"PhysX3Cooking",
					"PhysX3Extensions",
					"PhysX3Vehicle",
					"PxTask",
					"PhysXVisualDebuggerSDK",
					"PhysXProfileSDK",
					"PvdRuntime",
					"SceneQuery",
					"SimulationController",
				}; 

            foreach (var lib in PhysXLibs)
            {
                if (!lib.Contains("Cooking") || Target.IsCooked == false)
                {
                    PublicAdditionalLibraries.Add(PhysXLibDir + lib + ".bc");
                }
            }
        }
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			PublicSystemIncludePaths.Add(PhysXDir + "include/foundation/PS4");
			PublicLibraryPaths.Add(PhysXDir + "lib/PS4");

			string[] StaticLibrariesPS4 = new string[] {
				"PhysX3{0}",
				"PhysX3Extensions{0}",
				"PhysX3Cooking{0}",
				"PhysX3Common{0}",
				"PhysX3Vehicle{0}",
				"PxTask{0}",
				"PhysXVisualDebuggerSDK{0}",
				"PhysXProfileSDK{0}"
			};

			foreach (string Lib in StaticLibrariesPS4)
			{
				PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			PublicSystemIncludePaths.Add("include/foundation/xboxone");
			PublicLibraryPaths.Add(PhysXDir + "Lib/XboxOne");

			string[] StaticLibrariesXB1 = new string[] {
				"PhysX3{0}.lib",
				"PhysX3Extensions{0}.lib",
				"PhysX3Cooking{0}.lib",
				"PhysX3Common{0}.lib",
				"PhysX3Vehicle{0}.lib",
				"PxTask{0}.lib",
				"PhysXVisualDebuggerSDK{0}.lib",
				"PhysXProfileSDK{0}.lib"
			};

			foreach (string Lib in StaticLibrariesXB1)
			{
				PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
			}
		}
    }
}
