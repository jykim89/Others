/*
 * Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Ionic.Zlib;

namespace iPhonePackager
{
	public class Settings
	{
	}

	/**
	 * Operations done at compile time - these may involve the Mac
	 */
	public class CompileTime
	{
		/**
		 * Location of the Xcode installation on the Mac.  For example: 
		 * "/Applications/Xcode.app/Contents/Developer"
		 */
		private static string XcodeDeveloperDir = "";

		private static string iPhone_SigningDevRootMac = "";
		private static string iPhone_SigningDevRootWin = "";

		/// <summary>
		/// The file name (no path) of the temporary mobile provision that will be placed on the remote mac for use in makeapp
		/// </summary>
		private static string MacMobileProvisionFilename;

		private static string MacName = "";
		private static string MacStagingRootDir = "";
		private static string MacBinariesDir = "";
		private static string MacXcodeStagingDir = "";

		/** /MacStagingRootDir/Payload */
		public static string RemoteAppPayloadDirectory
		{
			get { return MacStagingRootDir + "/Payload"; }
		}
		
		/** /MacStagingRootDir/Payload/GameName.app */
		protected static string RemoteAppDirectory
		{
			get { return RemoteAppPayloadDirectory + "/" + Program.GameName + Program.Architecture + ".app"; }
		}


		/** /MacStagingRootDir/Payload/GameName.app/GameName */
		protected static string RemoteExecutablePath
		{
			get { return RemoteAppDirectory + "/" + Program.GameName + Program.Architecture; }
		}

		private static string CurrentBaseXCodeCommandLine;

		/**
		 * @return the commandline used to run Xcode (can add commands like "clean" as needed to the result)
		 */
		static private string GetBaseXcodeCommandline()
		{
			string CmdLine = XcodeDeveloperDir + "usr/bin/xcodebuild" +
					" -project UE4_FromPC.xcodeproj" + 
					" -configuration " + Program.GameConfiguration +
					" -target '" + Program.GameName + " - iOS'" +
					" -sdk " + ((Program.Architecture == "-simulator") ? "iphonesimulator" : "iphoneos");

			// sign with the Distribution identity when packaging for distribution
			if (!Config.bForDistribution)
			{
				CmdLine += String.Format(" CODE_SIGN_RESOURCE_RULES_PATH={0}/CustomResourceRules.plist", MacStagingRootDir);
			}
			CmdLine += String.Format(" CODE_SIGN_IDENTITY=\\\"{0}\\\"", Config.CodeSigningIdentity);

			CmdLine += String.Format(" IPHONEOS_DEPLOYMENT_TARGET=\\\"{0}\\\"", Config.MinOSVersion);

			return CmdLine;
		}

		/** 
		 * Handle the plethora of environment variables required to remote to the Mac
		 */
		static public void ConfigurePaths()
		{
			string MachineName = Environment.MachineName;

			XcodeDeveloperDir = Utilities.GetEnvironmentVariable("ue.XcodeDeveloperDir", "/Applications/Xcode.app/Contents/Developer/");

			// MacName=%ue4.iPhone_SigningServerName%
			MacName = Config.OverrideMacName != null ? Config.OverrideMacName : Utilities.GetEnvironmentVariable( "ue.IOSSigningServer", "a1487" );
			iPhone_SigningDevRootWin = "\\\\" + MacName + "\\UE4\\Builds";
			iPhone_SigningDevRootMac = "/UE4/Builds";

			// get the path to mirror into on the Mac
			string BinariesDir = Path.GetFullPath(Path.Combine(Environment.CurrentDirectory, @"..\.."));
			string Root = Path.GetPathRoot(BinariesDir);
			string BranchPath = MachineName + "/" + BinariesDir.Substring( Root.Length );
			BranchPath = BranchPath.Replace('\\', '/');

			// similar for the game path (strip off the D:\ tpe root)
			BinariesDir = Path.GetFullPath(Path.Combine(Config.BinariesDirectory, ".."));
			Root = Path.GetPathRoot(BinariesDir);

			string GameBranchPath;
			if (Program.GameName == "UE4Game")
			{
				GameBranchPath = BranchPath;
			}
			else
			{
				GameBranchPath = MachineName + "/" + BinariesDir.Substring(Root.Length);
				GameBranchPath = GameBranchPath.Replace('\\', '/');
			}

			Console.WriteLine("BranchPath = {0} --- GameBranchPath = {1}", BranchPath, GameBranchPath);

			// generate the directories to recursively copy into later on
			MacStagingRootDir = string.Format("{0}/{1}/IOS", iPhone_SigningDevRootMac, GameBranchPath);
			MacBinariesDir = string.Format("{0}/{1}/IOS", iPhone_SigningDevRootMac, BranchPath);
			MacXcodeStagingDir = string.Format("{0}/{1}/IOS/XcodeSupportFiles", iPhone_SigningDevRootMac, BranchPath);

			MacMobileProvisionFilename = MachineName + "_UE4Temp.mobileprovision";

			CurrentBaseXCodeCommandLine = GetBaseXcodeCommandline();
		}

		/// <summary>
		/// Logs out a line of stdout or stderr from RPCUtility.exe to the program log
		/// </summary>
		/// <param name="Sender"></param>
		/// <param name="Line"></param>
		static public void OutputReceivedRemoteProcessCall(Object Sender, DataReceivedEventArgs Line)
		{
			if ((Line != null) && (Line.Data != null) && (Line.Data != ""))
			{
				Program.Log("[RPC] " + Line.Data);

				if (Line.Data.Contains("** BUILD FAILED **"))
				{
					Program.Error("Xcode build failed!");
				}
			}
		}

		/// <summary>
		/// Copy the files always needed (even in a stub IPA)
		/// </summary>
		static public void CopyFilesNeededForMakeApp()
		{
			// Copy Info.plist over (modifiying it as needed)
			string SourcePListFilename = Utilities.GetPrecompileSourcePListFilename();
			Utilities.PListHelper Info = Utilities.PListHelper.CreateFromFile(SourcePListFilename);
			
			// Edit the plist
			CookTime.UpdateVersion(Info);

			// Write out the <GameName>-Info.plist file to the xcode staging directory
			string TargetPListFilename = Path.Combine(Config.PCXcodeStagingDir, Program.GameName + "-Info.plist");
			Directory.CreateDirectory(Path.GetDirectoryName(TargetPListFilename));
			string OutString = Info.SaveToString();
			OutString = OutString.Replace("${EXECUTABLE_NAME}", Program.GameName);
			OutString = OutString.Replace("${BUNDLE_IDENTIFIER}", Program.GameName.Replace("_", ""));
			byte[] RawInfoPList = Encoding.UTF8.GetBytes(OutString);
			File.WriteAllBytes(TargetPListFilename, RawInfoPList);

			Program.Log("Updating .plist: {0} --> {1}", SourcePListFilename, TargetPListFilename);

			// look for an entitlements file (optional)
			string SourceEntitlements = FileOperations.FindPrefixedFile(Config.BuildDirectory, Program.GameName + ".entitlements");
			
			// set where to make the entitlements file (
			string TargetEntitlements = Path.Combine(Config.PCXcodeStagingDir, Program.GameName + ".entitlements");
			if (File.Exists(SourceEntitlements))
			{
				FileOperations.CopyRequiredFile(SourceEntitlements, TargetEntitlements);
			}
			else
			{
				// we need to have something so Xcode will compile, so we just set the get-task-allow, since we know the value, 
				// which is based on distribution or not (true means debuggable)
				File.WriteAllText(TargetEntitlements, string.Format("<plist><dict><key>get-task-allow</key><{0}/></dict></plist>",
					Config.bForDistribution ? "false" : "true"));
			}
			
			
			// Copy the no sign resource rules file over
			if (!Config.bForDistribution)
			{
				FileOperations.CopyRequiredFile(@"..\..\..\Build\IOS\XcodeSupportFiles\CustomResourceRules.plist", Path.Combine(Config.PCStagingRootDir, "CustomResourceRules.plist"));
			}

			// Copy the mobile provision file over
			string ProvisionWithPrefix = FileOperations.FindPrefixedFile(Config.BuildDirectory, Program.GameName + ".mobileprovision");
			if (!File.Exists(ProvisionWithPrefix))
			{
				ProvisionWithPrefix = FileOperations.FindPrefixedFile(Config.BuildDirectory + "/NotForLicensees/", Program.GameName + ".mobileprovision");
				if (!File.Exists(ProvisionWithPrefix))
				{
					ProvisionWithPrefix = FileOperations.FindPrefixedFile(Config.EngineBuildDirectory, "UE4Game.mobileprovision");
					if (!File.Exists(ProvisionWithPrefix))
					{
						ProvisionWithPrefix = FileOperations.FindPrefixedFile(Config.EngineBuildDirectory + "/NotForLicensees/", "UE4Game.mobileprovision");
					}
				}
			}
			string FinalMobileProvisionFilename = Path.Combine(Config.PCXcodeStagingDir, MacMobileProvisionFilename);
			FileOperations.CopyRequiredFile(ProvisionWithPrefix, FinalMobileProvisionFilename);
			// make sure this .mobileprovision file is newer than any other .mobileprovision file on the Mac (this file gets multiple games named the same file, 
			// so the time stamp checking can fail when moving between games, a la the buildmachines!)
			File.SetLastWriteTime(FinalMobileProvisionFilename, DateTime.UtcNow);

			FileOperations.CopyRequiredFile(Config.RootRelativePath + @"Engine\Intermediate\IOS\UE4.xcodeproj\project.pbxproj", Path.Combine(Config.PCXcodeStagingDir, @"project.pbxproj.datecheck"));
			
			// needs Mac line endings so it can be executed
			string SrcPath = @"..\..\..\Build\IOS\XcodeSupportFiles\prepackage.sh";
			string DestPath = Path.Combine(Config.PCXcodeStagingDir, @"prepackage.sh");
			Program.Log(" ... '" + SrcPath + "' -> '" + DestPath + "'");
			string SHContents = File.ReadAllText(SrcPath);
			SHContents = SHContents.Replace("\r\n", "\n");
			File.WriteAllText(DestPath, SHContents);

			CookTime.CopySignedFiles();
		}
		
		/**
		 * Handle spawning of the RPCUtility with parameters
		 */
		public static bool RunRPCUtilty( string RPCCommand )
		{
			string CommandLine = "";
			string WorkingFolder = "";
			string DisplayCommandLine = "";

			Program.Log( "Running RPC on " + MacName + " ... " );

			switch (RPCCommand.ToLowerInvariant())
			{
			case "deletemacstagingfiles":
				Program.Log( " ... deleting staging files on the Mac" );
				DisplayCommandLine = "rm -rf Payload";
				CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
				WorkingFolder = MacStagingRootDir;
				break;

			case "ensureprovisiondirexists":
				Program.Log(" ... creating provisioning profiles directory");

				DisplayCommandLine = String.Format("mkdir -p ~/Library/MobileDevice/Provisioning\\ Profiles");

				CommandLine = MacXcodeStagingDir + " " + DisplayCommandLine;
				WorkingFolder = MacXcodeStagingDir;
				break;

			case "installprovision":
				// Note: The provision must have already been copied over to the Mac
				Program.Log(" ... installing .mobileprovision");

				DisplayCommandLine = String.Format("cp -f {0} ~/Library/MobileDevice/Provisioning\\ Profiles", MacMobileProvisionFilename);

				CommandLine = MacXcodeStagingDir + " " + DisplayCommandLine;
				WorkingFolder = MacXcodeStagingDir;
				break;

			case "removeprovision":
				Program.Log(" ... removing .mobileprovision");
				DisplayCommandLine = String.Format("rm -f ~/Library/MobileDevice/Provisioning\\ Profiles/{0}", MacMobileProvisionFilename);
				CommandLine = MacXcodeStagingDir + " " + DisplayCommandLine;
				WorkingFolder = MacXcodeStagingDir;
				break;

			case "setexec": 
				// Note: The executable must have already been copied over
				Program.Log(" ... setting executable bit");
				DisplayCommandLine = "chmod a+x " + RemoteExecutablePath;
				CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
				WorkingFolder = MacStagingRootDir;
				break;

			case "prepackage":
				Program.Log(" ... running prepackage script remotely ");
				DisplayCommandLine = String.Format("sh prepackage.sh {0} IOS {1} {2}", Program.GameName, Program.GameConfiguration, Program.Architecture);
				CommandLine = MacXcodeStagingDir + " " + DisplayCommandLine;
				WorkingFolder = MacXcodeStagingDir;
				break;

			case "makeapp":
				Program.Log(" ... making application (codesign, etc...)");
				Program.Log("  Using signing identity '{0}'", Config.CodeSigningIdentity);
				DisplayCommandLine = CurrentBaseXCodeCommandLine;
				CommandLine = MacXcodeStagingDir + "/.. " + DisplayCommandLine;
				WorkingFolder = MacXcodeStagingDir + "/..";
				break;

			case "validation":
				Program.Log( " ... validating distribution package" );
				DisplayCommandLine = XcodeDeveloperDir + "Platforms/iPhoneOS.platform/Developer/usr/bin/Validation " + RemoteAppDirectory;
				CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
				WorkingFolder = MacStagingRootDir;
				break;

			case "deleteipa":
				Program.Log(" ... deleting IPA on Mac");
				DisplayCommandLine = "rm -f " + Config.IPAFilenameOnMac;
				CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
				WorkingFolder = MacStagingRootDir;
				break;

			case "kill":
				Program.Log( " ... killing" );
				DisplayCommandLine = "killall " + Program.GameName;
				CommandLine = ". " + DisplayCommandLine;
				WorkingFolder = ".";
				break;

			case "strip":
				Program.Log( " ... stripping" );
				DisplayCommandLine = XcodeDeveloperDir + "Platforms/iPhoneOS.platform/Developer/usr/bin/strip " + RemoteExecutablePath;
				CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
				WorkingFolder = MacStagingRootDir;
				break;

			case "resign":
				Program.Log("... resigning");
				DisplayCommandLine = "bash -c '" + "chmod a+x ResignScript" + ";" + "./ResignScript" + "'";
				CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
				WorkingFolder = MacStagingRootDir;
				break;

			case "zip":
				Program.Log( " ... zipping" );

				// NOTE: -y preserves symbolic links which is needed for iOS distro builds
				// -x excludes a file (excluding the dSYM keeps sizes smaller, and it shouldn't be in the IPA anyways)
				string dSYMName = "Payload/" + Program.GameName + Program.Architecture + ".app.dSYM";
				DisplayCommandLine = String.Format("zip -q -r -y -{0} -T {1} Payload iTunesArtwork -x {2}/ -x {2}/* " +
					"-x {2}/Contents/ -x {2}/Contents/* -x {2}/Contents/Resources/ -x {2}/Contents/Resources/* " +
					" -x {2}/Contents/Resources/DWARF/ -x {2}/Contents/Resources/DWARF/*",
					(int)Config.RecompressionSetting,
					Config.IPAFilenameOnMac,
					dSYMName);

				CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
				WorkingFolder = MacStagingRootDir;
				break;

			case "gendsym":
				Program.Log( " ... generating DSYM" );

				string ExePath  = "Payload/" + Program.GameName + ".app/" + Program.GameName;
				string dSYMPath = Program.GameName + ".app.dSYM";
				DisplayCommandLine = String.Format("dsymutil -o {0} {1}", dSYMPath, ExePath);

				CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
				WorkingFolder = MacStagingRootDir;
				break;

			default:
				Program.Error( "Unrecognized RPC command" );
				return ( false );
			}

			Program.Log( " ... working folder: " + WorkingFolder );
			Program.Log( " ... " + DisplayCommandLine );

			Process RPCUtil = new Process();
			RPCUtil.StartInfo.FileName = @"..\RPCUtility.exe";
			RPCUtil.StartInfo.UseShellExecute = false;
			RPCUtil.StartInfo.Arguments = MacName + " " + CommandLine;
			RPCUtil.StartInfo.RedirectStandardOutput = true;
			RPCUtil.StartInfo.RedirectStandardError = true;
			RPCUtil.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedRemoteProcessCall);
			RPCUtil.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedRemoteProcessCall);

			RPCUtil.Start();
			
			RPCUtil.BeginOutputReadLine();
			RPCUtil.BeginErrorReadLine();

			RPCUtil.WaitForExit();

			if (RPCUtil.ExitCode != 0)
			{
				Program.Error("RPCCommand {0} failed with return code {1}", RPCCommand, RPCUtil.ExitCode);
				switch (RPCCommand.ToLowerInvariant())
				{
					case "installprovision":
						Program.Error("Ensure your access permissions for '~/Library/MobileDevice/Provisioning Profiles' are set correctly.");
						break;
					default:
						break;
				}
			}

			return (RPCUtil.ExitCode == 0);
		}


		/** 
		 * Creates the application directory on the Mac
		 */
		static public void CreateApplicationDirOnMac()
		{
			DateTime StartTime = DateTime.Now;

			// Cleans out the intermediate folders on both ends
			CompileTime.ExecuteRemoteCommand("DeleteIPA");
			CompileTime.ExecuteRemoteCommand("DeleteMacStagingFiles");
			Program.ExecuteCommand("Clean", null);
			//@TODO: mnoland 10/5/2010
			// Need to come up with a way to prevent this from causing an error on the remote machine
			// CompileTime.ExecuteRemoteCommand("Clean");

			// Stage files
			Program.Log("Staging files before copying to Mac ...");
			CopyFilesNeededForMakeApp();

			// Copy staged files from PC to Mac
			Program.ExecuteCommand("StageMacFiles", null);

			// Set the executable bit on the EXE
			CompileTime.ExecuteRemoteCommand("SetExec");

			// Install the provision (necessary for MakeApp to succeed)
			CompileTime.ExecuteRemoteCommand("EnsureProvisionDirExists");
			CompileTime.ExecuteRemoteCommand("InstallProvision");

			// strip the symbols if desired or required
			if (Config.bForceStripSymbols || Config.bForDistribution)
			{
				CompileTime.ExecuteRemoteCommand("Strip");
			}

			// sign the exe, etc...
			CompileTime.ExecuteRemoteCommand("PrePackage");
			CompileTime.ExecuteRemoteCommand("MakeApp");

			Program.Log(String.Format("Finished creating .app directory on Mac (took {0:0.00} s)",
				(DateTime.Now - StartTime).TotalSeconds));
		}
		
		/** 
		 * Packages an IPA on the Mac
		 */
		static public void PackageIPAOnMac()
		{
			// Create the .app structure on the Mac (and codesign, etc...)
			CreateApplicationDirOnMac();

			DateTime StartTime = DateTime.Now;

			// zip up
			CompileTime.ExecuteRemoteCommand("Zip");

			// fetch the IPA
			if (Config.bCreateStubSet)
			{
				Program.ExecuteCommand("GetStubIPA", null);
			}
			else
			{
				Program.ExecuteCommand("GetIPA", null);
			}

			Program.Log(String.Format("Finished packaging into IPA (took {0:0.00} s)",
				(DateTime.Now - StartTime).TotalSeconds));
		}

		static public void DangerouslyFastMode()
		{
			CompileTime.ExecuteRemoteCommand("MakeApp");
		}

		static public void ExecuteRemoteCommand(string RemoteCommand)
		{
			Program.Log("Running RPC on " + MacName + " ... ");
			RunRPCUtilty(RemoteCommand);
		}

		static public bool ExecuteCompileCommand(string Command, string RPCCommand)
		{
			switch (Command.ToLowerInvariant())
			{
				case "clean":
					Program.Log("Cleaning temporary files from PC ... ");
					Program.Log(" ... cleaning: " + Config.PCStagingRootDir);
					FileOperations.DeleteDirectory(new DirectoryInfo(Config.PCStagingRootDir));
					break;
 
				case "rpc":
					ExecuteRemoteCommand(RPCCommand);
					break;

				case "getipa":
					{
						Program.Log("Fetching IPA from Mac...");

						string IpaDestFilename = Config.GetIPAPath(".ipa");
						FileOperations.DownloadFile(MacName, MacStagingRootDir + "/" + Config.IPAFilenameOnMac, IpaDestFilename);

						Program.Log("... Saved IPA to '{0}'", Path.GetFullPath(IpaDestFilename));
					}
					break;

				case "getstubipa":
					{
						Program.Log("Fetching stub IPA from Mac...");

						string IpaDestFilename = Config.GetIPAPath(".stub");
						FileOperations.DownloadFile(MacName, MacStagingRootDir + "/" + Config.IPAFilenameOnMac, IpaDestFilename);

						Program.Log("... Saved stub IPA to '{0}'", Path.GetFullPath(IpaDestFilename));
					}
					break;

				case "stagemacfiles":
					Program.Log("Copying all staged files to Mac " + MacName + " ...");
					FileOperations.BatchUploadFolder(MacName, Config.PCStagingRootDir, MacStagingRootDir, false);
					FileOperations.BatchUploadFolder(MacName, Config.PCXcodeStagingDir, MacXcodeStagingDir, false);
					break;

				default:
					return false;
			}

			return true;
		}
	}
}
