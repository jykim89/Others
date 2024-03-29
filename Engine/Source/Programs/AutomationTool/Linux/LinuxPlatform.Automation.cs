﻿// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;
using System.Diagnostics;
using AutomationTool;
using UnrealBuildTool;


public abstract class BaseLinuxPlatform : Platform
{
    static Regex deviceRegex = new Regex(@"linux@([A-Za-z0-9\.\-]+)[\+]?");
    static Regex usernameRegex = new Regex(@"\\([a-z_][a-z0-9_]{0,30})@");
    static String keyPath = CombinePaths(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "/UE4SSHKeys");
    static String pscpPath = CombinePaths(Environment.GetEnvironmentVariable("LINUX_ROOT"), "bin/PSCP.exe");
    static String plinkPath = CombinePaths(Environment.GetEnvironmentVariable("LINUX_ROOT"), "bin/PLINK.exe");

	public BaseLinuxPlatform(UnrealTargetPlatform P)
		: base(P)
	{ 
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
        if (SC.bStageCrashReporter)
        {
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), "CrashReportClient", false);
        }

        // assume that we always have to deploy Steam (FIXME: should be automatic)
		{
			string SteamVersion = "Steamv129";

			// Check if the Steam directory exists. We need it for Steam controller support, so we include it whenever we can.
			if (Directory.Exists(CommandUtils.CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Steamworks/" + SteamVersion)))
			{
				SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/Steamworks/" + SteamVersion, SC.PlatformDir), "libsteam_api.so", false, null, CombinePaths("Engine/Binaries", SC.PlatformDir));
			}

			SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Config"), "controller.vdf", false, null, CommandUtils.CombinePaths(SC.RelativeProjectRootForStage, "Saved/Config"));
		}

        // assume that we always have to deploy OpenAL (FIXME: should be automatic)
        {
            SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries/ThirdParty/OpenAL/", SC.PlatformDir), "libopenal.so.1", false, null, CombinePaths("Engine/Binaries", SC.PlatformDir));
        }

        SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Content/Splash"), "Splash.bmp", false, null, null, true);
        SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Content/Localization"), "*.dat", true, null, null, false, !Params.Pak);

        List<string> Exes = GetExecutableNames(SC);

        foreach (var Exe in Exes)
        {

            if (Exe.StartsWith(CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir)))
            {
                // remap the project root. For Rocket executables, rename the executable to the game name.
                if (Params.Rocket && Exe == Exes[0])
                {
                    SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), Path.GetFileNameWithoutExtension(Exe), true, null, CommandUtils.CombinePaths(SC.RelativeProjectRootForStage, "Binaries", SC.PlatformDir), false, true, SC.ShortProjectName);
                }
                else
                {
                    SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir), Path.GetFileNameWithoutExtension(Exe), true, null, CommandUtils.CombinePaths(SC.RelativeProjectRootForStage, "Binaries", SC.PlatformDir), false);
                }
            }
            else if (Exe.StartsWith(CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir)))
            {
                // Move the executable for non-code rocket projects into the game directory, using the game name, so it can figure out the UProject to look for and is consitent with code projects.
                if (Params.Rocket && Exe == Exes[0])
                {
                    SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), Path.GetFileNameWithoutExtension(Exe), true, null, CommandUtils.CombinePaths(SC.RelativeProjectRootForStage, "Binaries", SC.PlatformDir), false, true, SC.ShortProjectName);
                }
                else
                {
                    SC.StageFiles(StagedFileType.NonUFS, CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir), Path.GetFileNameWithoutExtension(Exe), true, null, null, false);
                }
            }
            else
            {
                throw new AutomationException("Can't stage the exe {0} because it doesn't start with {1} or {2}", Exe, CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir), CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir));
            }
        }
    }

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly, string CookFlavor)
	{
        const string NoEditorCookPlatform = "LinuxNoEditor";
        const string ServerCookPlatform = "LinuxServer";
        const string ClientCookPlatform = "LinuxClient";

        if (bDedicatedServer)
        {
            return ServerCookPlatform;
        }
        else if (bIsClientOnly)
        {
            return ClientCookPlatform;
        }
        else
        {
            return NoEditorCookPlatform;
        }
    }

    /// <summary>
    /// return true if we need to change the case of filenames outside of pak files
    /// </summary>
    /// <returns></returns>
    public override bool DeployLowerCaseFilenames(bool bUFSFile)
    {
        return false;
    }

    /// <summary>
    /// Deploy the application on the current platform
    /// </summary>
    /// <param name="Params"></param>
    /// <param name="SC"></param>
    public override void Deploy(ProjectParams Params, DeploymentContext SC)
    {
        if (!String.IsNullOrEmpty(Params.ServerDeviceAddress))
        {
            string sourcePath = CombinePaths(Params.BaseStageDirectory, GetCookPlatform(Params.DedicatedServer,false,""));
            string destPath = Params.DeviceUsername + "@" + Params.ServerDeviceAddress + ":.";
            RunAndLog(CmdEnv, pscpPath, String.Format("-batch -i {0} -r {1} {2}", Params.DevicePassword, sourcePath, destPath));

            List<string> Exes = GetExecutableNames(SC);

            string binPath = CombinePaths(GetCookPlatform(Params.DedicatedServer, false, ""), SC.RelativeProjectRootForStage, "Binaries", SC.PlatformDir, Path.GetFileName(Exes[0])).Replace("\\", "/");
            string iconPath = CombinePaths(GetCookPlatform(Params.DedicatedServer, false, ""), SC.RelativeProjectRootForStage, SC.ShortProjectName + ".png").Replace("\\", "/");

            string DesiredGLVersion = "4.3";

            // Begin Bash Shell Script
            string script = String.Format(@"#!/bin/bash
# Check for OpenGL4 support
glarg=''
if command -v glxinfo >/dev/null 2>&1 ; then
    export DISPLAY="":0""
    glversion=$(glxinfo | grep ""OpenGL version string:"" | sed 's/[^0-9.]*\([0-9.]*\).*/\1/')
    glmatch=$(echo -e ""$glversion\n{0}"" | sort -Vr | head -1)
    [[ ""$glmatch"" = ""$glversion"" ]] && glarg=' -opengl4'
fi

# Create .desktop file
cat > $HOME/Desktop/{1}.desktop << EOF
[Desktop Entry]
Type=Application
Name={2}
Comment=UE4 Game
Exec=$HOME/{3}{4}$glarg
Icon=$HOME/{5}
Terminal=false
Categories=Game;
EOF

# Set permissions
chmod 755 $HOME/{3}
chmod 700 $HOME/Desktop/{1}.desktop", DesiredGLVersion, SC.ShortProjectName, SC.ShortProjectName, binPath, (Params.Pak ? " -pak" : ""), iconPath);
            // End Bash Shell Script

            string scriptFile = Path.GetTempFileName();
            File.WriteAllText(scriptFile, script);
            RunAndLog(CmdEnv, plinkPath, String.Format("-ssh -t -batch -l {0} -i {1} {2} -m {3}", Params.DeviceUsername, Params.DevicePassword, Params.ServerDeviceAddress, scriptFile));
            File.Delete(scriptFile);
        }
    }

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
	}

    /// <summary>
    /// Allow the platform to alter the ProjectParams
    /// </summary>
    /// <param name="ProjParams"></param>
    public override void PlatformSetupParams(ref ProjectParams ProjParams)
    {
        Match match = deviceRegex.Match(ProjParams.ServerDevice);
        if (match.Success)
        {
            ProjParams.ServerDeviceAddress = match.Groups[1].Value;

            string linuxKey = null;
            string keyID = ProjParams.ServerDeviceAddress;

            // If username specified use key for that user
            if (!String.IsNullOrEmpty(ProjParams.DeviceUsername))
            {
                keyID = ProjParams.DeviceUsername + "@" + keyID;
            }

            if (!DirectoryExists(keyPath))
            {
                Directory.CreateDirectory(keyPath);
            }

            linuxKey = Directory.EnumerateFiles(keyPath).Where(f => f.Contains(keyID)).FirstOrDefault();

            // Generate key if it doesn't exist
            if (String.IsNullOrEmpty(linuxKey)
                && ((!String.IsNullOrEmpty(ProjParams.DeviceUsername) && !String.IsNullOrEmpty(ProjParams.DevicePassword))
                    || !ProjParams.Unattended)) // Skip key generation in unattended mode if information is missing
            {
                Log("Configuring linux host");

                // Prompt for username if not already set
                while (String.IsNullOrEmpty(ProjParams.DeviceUsername))
                {
                    Console.Write("Username: ");
                    ProjParams.DeviceUsername = Console.ReadLine();
                }

                // Prompty for password if not already set
                while (String.IsNullOrEmpty(ProjParams.DevicePassword))
                {
                    ProjParams.DevicePassword = String.Empty;
                    Console.Write("Password: ");
                    ConsoleKeyInfo key;
                    do
                    {
                        key = Console.ReadKey(true);
                        if (key.Key != ConsoleKey.Backspace && key.Key != ConsoleKey.Enter)
                        {
                            ProjParams.DevicePassword += key.KeyChar;
                            Console.Write("*");
                        }
                        else
                        {
                            if (key.Key == ConsoleKey.Backspace && ProjParams.DevicePassword.Length > 0)
                            {
                                ProjParams.DevicePassword = ProjParams.DevicePassword.Substring(0, (ProjParams.DevicePassword.Length - 1));
                                Console.Write("\b \b");
                            }
                        }

                    } while (key.Key != ConsoleKey.Enter);
                    Console.WriteLine();
                }
                String Command = @"C:\Windows\system32\cmd.exe";

                // Convienence names
                String keyName = String.Format("UE4:{0}@{1}", Environment.UserName, Environment.MachineName);
                String idRSA = String.Format(".ssh/id_rsa/{0}", keyName);
                String ppk = String.Format("{0}@{1}.ppk", ProjParams.DeviceUsername, ProjParams.ServerDeviceAddress);

                // Generate keys including the .ppk on Linux device
                String Script = String.Format("mkdir -p .ssh/id_rsa && if [ ! -f {0} ] ; then ssh-keygen -t rsa -C {1} -N '' -f {0} && cat {0}.pub >> .ssh/authorized_keys ; fi && chmod -R go-w ~/.ssh && puttygen {0} -o {2}", idRSA, keyName, ppk);
                String Args = String.Format("/c ECHO y | {0} -ssh -t -pw {1} {2}@{3} \"{4}\"", plinkPath, ProjParams.DevicePassword, ProjParams.DeviceUsername, ProjParams.ServerDeviceAddress, Script);
                RunAndLog(CmdEnv, Command, Args);

                // Retrive the .ppk
                Command = pscpPath;
                Args = String.Format("-batch -pw {0} -l {1} {2}:{3} {4}", ProjParams.DevicePassword, ProjParams.DeviceUsername, ProjParams.ServerDeviceAddress, ppk, keyPath);
                RunAndLog(CmdEnv, Command, Args);

                linuxKey = CombinePaths(keyPath, ppk);

                // Remove the .ppk from the Linux device
                Command = plinkPath;
                Script = String.Format("rm {0}", ppk);
                Args = String.Format("-batch -i {0} {1}@{2} \"{3}\"", linuxKey, ProjParams.DeviceUsername, ProjParams.ServerDeviceAddress, Script);
                RunAndLog(CmdEnv, Command, Args);
            }

            // Fail if a key couldn't be found or generated
            if (String.IsNullOrEmpty(linuxKey))
            {
                throw new AutomationException("can't deploy/run using a linux device without a valid SSH key");
            }

            // Set username from key
            if (String.IsNullOrEmpty(ProjParams.DeviceUsername))
            {
                match = usernameRegex.Match(linuxKey);
                if (!match.Success)
                {
                    throw new AutomationException("can't determine username from SSH key");
                }
                ProjParams.DeviceUsername = match.Groups[1].Value;
            }

            // Use key as password
            ProjParams.DevicePassword = linuxKey;
        }
        else if (ProjParams.Deploy || ProjParams.Run)
        {
            throw new AutomationException("must specify device for Linux target (-serverdevice=<ip>)");
        }
    }
    public override List<string> GetDebugFileExtentions()
    {
        return new List<string> {  };
    }

}

public class CentOSx64Platform : BaseLinuxPlatform
{
	public CentOSx64Platform()
		: base(UnrealTargetPlatform.Linux)
	{
	}

	public override bool IsSupported { get { return true; } }
}

