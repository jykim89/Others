// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Diagnostics;
using System.Security.AccessControl;
using System.Xml;
using System.Text;
using Ionic.Zip;
using Ionic.Zlib;

namespace UnrealBuildTool
{
	class IOSToolChain : RemoteToolChain
	{
		public override void RegisterToolChain()
		{
			RegisterRemoteToolChain(UnrealTargetPlatform.Mac, CPPTargetPlatform.IOS);
		}


		/***********************************************************************
		 * NOTE:
		 *  Do NOT change the defaults to set your values, instead you should set the environment variables
		 *  properly in your system, as other tools make use of them to work properly!
		 *  The defaults are there simply for examples so you know what to put in your env vars...
		 ***********************************************************************/

		// If you are looking for where to change the remote compile server name, look in RemoteToolChain.cs

		/** If this is set, then we don't do any post-compile steps except moving the executable into the proper spot on the Mac */
		public static bool bUseDangerouslyFastMode = false;

		/** Which version of the iOS SDK to target at build time */
		public static string IOSSDKVersion = "latest";

		/** The architecture(s) to compile */
		public static string NonShippingArchitectures = "armv7";
		public static string ShippingArchitectures = "armv7";

		// In case the SDK checking fails for some reason, use this version
		private static string BackupVersion = "6.0";

		/** Which version of the iOS to allow at run time */
		public static string IOSVersion = "6.0";

		/** Which developer directory to root from */
		public static string XcodeDeveloperDir = "/Applications/Xcode.app/Contents/Developer/";

		/** Location of the SDKs */
		private static string BaseSDKDir;
		private static string BaseSDKDirSim;

		/** Which compiler frontend to use */
		private static string IOSCompiler = "clang++";

		/** Which linker frontend to use */
		private static string IOSLinker = "clang++";

		/** Which library archiver to use */
		private static string IOSArchiver = "libtool";

		public List<string> BuiltBinaries = new List<string>();

		/** Additional frameworks stored locally so we have access without LinkEnvironment */
		public List<UEBuildFramework> RememberedAdditionalFrameworks = new List<UEBuildFramework>();

		/// <summary>
		/// Function to call to reset default data.
		/// </summary>
		public static void Reset()
		{
			XmlConfigLoader.Load(typeof(IOSToolChain));

			/** Location of the SDKs */
			BaseSDKDir = XcodeDeveloperDir + "Platforms/iPhoneOS.platform/Developer/SDKs";
			BaseSDKDirSim = XcodeDeveloperDir + "Platforms/iPhoneSimulator.platform/Developer/SDKs";
		}

		/** Hunt down the latest IOS sdk if desired */
		public override void SetUpGlobalEnvironment()
		{
			base.SetUpGlobalEnvironment();

			if (IOSSDKVersion == "latest")
			{
				try
				{
					string[] SubDirs = null;
					if (Utils.IsRunningOnMono)
					{
						// on the Mac, we can just get the directory name
						SubDirs = System.IO.Directory.GetDirectories(BaseSDKDir);
						Log.TraceInformation(String.Format("Directories : {0} {1}", SubDirs, SubDirs[0]));
					}
					else
					{
						Hashtable Results = RPCUtilHelper.Command("/", "ls", BaseSDKDir, null);
						if (Results != null)
						{
							string Result = (string)Results["CommandOutput"];
							SubDirs = Result.Split("\r\n".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);
						}
					}

					// loop over the subdirs and parse out the version
					float MaxSDKVersion = 0.0f;
					string MaxSDKVersionString = null;
					foreach (string SubDir in SubDirs)
					{
						string SubDirName = Path.GetFileNameWithoutExtension(SubDir);
						if (SubDirName.StartsWith("iPhoneOS"))
						{
							// get the SDK version from the directory name
							string SDKString = SubDirName.Replace("iPhoneOS", "");
							float SDKVersion = 0.0f;
							try
							{
								SDKVersion = float.Parse(SDKString, System.Globalization.CultureInfo.InvariantCulture);
							}
							catch (Exception)
							{
								// weirdly formatted SDKs
								continue;
							}

							// update largest SDK version number
							if (SDKVersion > MaxSDKVersion)
							{
								MaxSDKVersion = SDKVersion;
								MaxSDKVersionString = SDKString;
							}
						}
					}

					// convert back to a string with the exact format
					if (MaxSDKVersionString != null)
					{
						IOSSDKVersion = MaxSDKVersionString;
					}
				}
				catch (Exception)
				{
					// on any exception, just use the backup version
					IOSSDKVersion = BackupVersion;
				}

				if (ExternalExecution.GetRuntimePlatform() != UnrealTargetPlatform.Mac)
				{
					Log.TraceInformation("Compiling with IOS SDK {0} on Mac {1}", IOSSDKVersion, RemoteServerName);
				}
				else
				{
					Log.TraceInformation("Compiling with IOS SDK {0}", IOSSDKVersion);
				}
			}
		}

		static public void AddStubToManifest(ref FileManifest Manifest, UEBuildBinary Binary)
		{
			// Daniel DON'T INTEGRATE TO MAIN removed this because the stub file isn't needed and causes errors when we try and check for it's existance due to the manifest including it
			//  	Recommended by peter sauerbrie don't 
			/*if (BuildConfiguration.bCreateStubIPA)
			{
				string StubFile = Path.Combine (Path.GetDirectoryName (Binary.Config.OutputFilePath), Path.GetFileNameWithoutExtension (Binary.Config.OutputFilePath) + ".stub");
				Manifest.AddFileName (StubFile);
			}*/
		}

		static bool bHasPrinted = false;
		static string GetArchitectureArgument(CPPTargetConfiguration Configuration, string UBTArchitecture)
		{

			// get the list of architectures to compile
			string Archs =
				UBTArchitecture == "-simulator" ? "i386" :
				(Configuration == CPPTargetConfiguration.Shipping) ? ShippingArchitectures : NonShippingArchitectures;

			if (!bHasPrinted)
			{
				bHasPrinted = true;
				Console.WriteLine("Compiling with these architectures: " + Archs);
			}

			// parse the string
			string[] Tokens = Archs.Split(",".ToCharArray());

			string Result = "";
			foreach (string Token in Tokens)
			{
				Result += " -arch " + Token;
			}

			return Result;
		}
		
		static string GetCompileArguments_Global(CPPEnvironment CompileEnvironment)
		{
			string Result = "";

			Result += " -fmessage-length=0";
			Result += " -pipe";
			Result += " -fpascal-strings";

			Result += " -fno-exceptions";
			Result += " -fno-rtti";
			Result += " -fvisibility=hidden"; // hides the linker warnings with PhysX


			Result += " -Wall -Werror";

			Result += " -Wno-unused-variable";
			Result += " -Wno-unused-value";
			// this will hide the warnings about static functions in headers that aren't used in every single .cpp file
			Result += " -Wno-unused-function";
			// this hides the "enumeration value 'XXXXX' not handled in switch [-Wswitch]" warnings - we should maybe remove this at some point and add UE_LOG(, Fatal, ) to default cases
			Result += " -Wno-switch";
			// this hides the "warning : comparison of unsigned expression < 0 is always false" type warnings due to constant comparisons, which are possible with template arguments
			Result += " -Wno-tautological-compare";
			//This will prevent the issue of warnings for unused private variables.
			Result += " -Wno-unused-private-field";
			Result += " -Wno-invalid-offsetof"; // needed to suppress warnings about using offsetof on non-POD types.
	
			Result += " -c";

			// What architecture(s) to build for
			Result += GetArchitectureArgument(CompileEnvironment.Config.TargetConfiguration, CompileEnvironment.Config.TargetArchitecture);

			if (CompileEnvironment.Config.TargetArchitecture == "-simulator")
			{
				Result += " -isysroot " + BaseSDKDirSim + "/iPhoneSimulator" + IOSSDKVersion + ".sdk";
			}
			else
			{
				Result += " -isysroot " + BaseSDKDir + "/iPhoneOS" + IOSSDKVersion + ".sdk";
			}

			Result += " -miphoneos-version-min=" + IOSVersion;

			// Optimize non- debug builds.
			if (CompileEnvironment.Config.TargetConfiguration != CPPTargetConfiguration.Debug)
			{
				Result += " -O3";
			}
			else
			{
				Result += " -O0";
			}

			// Create DWARF format debug info if wanted,
			if (CompileEnvironment.Config.bCreateDebugInfo)
			{
				Result += " -gdwarf-2";
			}

			return Result;
		}

		static string GetCompileArguments_CPP()
		{
			string Result = "";
			Result += " -x objective-c++";
			Result += " -fno-rtti";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++0x";
			return Result;
		}

		static string GetCompileArguments_MM()
		{
			string Result = "";
			Result += " -x objective-c++";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -fno-rtti";
			Result += " -std=c++0x";
			return Result;
		}

		static string GetCompileArguments_M()
		{
			string Result = "";
			Result += " -x objective-c";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=c++0x";
			return Result;
		}

		static string GetCompileArguments_C()
		{
			string Result = "";
			Result += " -x c";
			return Result;
		}

		static string GetCompileArguments_PCH()
		{
			string Result = "";
			Result += " -x objective-c++-header";
			Result += " -fno-rtti";
			Result += " -std=c++0x";
			return Result;
		}

		string GetLocalFrameworkZipPath( UEBuildFramework Framework )
		{
			if ( Framework.OwningModule != null )
			{
				// If we have a source module, assume that the path name is relative to that
				return Path.GetFullPath( Framework.OwningModule.ModuleDirectory + "/" + Framework.FrameworkZipPath );
			}
			return Path.GetFullPath( Framework.FrameworkZipPath );
		}

		string GetRemoteFrameworkZipPath( UEBuildFramework Framework )
		{
			if ( ExternalExecution.GetRuntimePlatform() != UnrealTargetPlatform.Mac )
			{
				return ConvertPath( GetLocalFrameworkZipPath( Framework ) );
			}

			return GetLocalFrameworkZipPath( Framework );
		}

		string GetLinkArguments_Global( LinkEnvironment LinkEnvironment )
		{
			string Result = "";
			if (LinkEnvironment.Config.TargetArchitecture == "-simulator")
			{
				Result += " -arch i386";
				Result += " -isysroot " + XcodeDeveloperDir + "Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator" + IOSSDKVersion + ".sdk";
			}
			else
			{
				Result += Result += GetArchitectureArgument(LinkEnvironment.Config.TargetConfiguration, LinkEnvironment.Config.TargetArchitecture);
				Result += " -isysroot " + XcodeDeveloperDir + "Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS" + IOSSDKVersion + ".sdk";
			}
			Result += " -dead_strip";
			Result += " -miphoneos-version-min=" + IOSVersion;
			Result += " -Wl,-no_pie";
			//			Result += " -v";

			// link in the frameworks
			foreach (string Framework in LinkEnvironment.Config.Frameworks)
			{
				Result += " -framework " + Framework;
			}
			foreach (UEBuildFramework Framework in LinkEnvironment.Config.AdditionalFrameworks)
			{
				if ( Framework.FrameworkZipPath != null )
				{
					// If this framework has a zip specified, we'll need to setup the path as well
					string FrameworkZipPath = GetRemoteFrameworkZipPath( Framework );

					// Assume the path is the full name without the zip extension
					Result += " -F " + FrameworkZipPath.Replace( ".zip", "" ); ;
				}

				Result += " -framework " + Framework.FrameworkName;
			}
			foreach (string Framework in LinkEnvironment.Config.WeakFrameworks)
			{
				Result += " -weak_framework " + Framework;
			}

			return Result;
		}

		static string GetArchiveArguments_Global(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			Result += " -static";

			return Result;
		}
		
		public override CPPOutput CompileCPPFiles(CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName)
		{
			string Arguments = GetCompileArguments_Global(CompileEnvironment);
			string PCHArguments = "";

			if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				// Add the precompiled header file's path to the include path so GCC can find it.
				// This needs to be before the other include paths to ensure GCC uses it instead of the source header file.
				PCHArguments += string.Format(" -include \"{0}\"", ConvertPath(CompileEnvironment.PrecompiledHeaderFile.AbsolutePath.Replace(".gch", "")));
			}

			// Add include paths to the argument list.
			List<string> AllIncludes = CompileEnvironment.Config.IncludePaths;
			AllIncludes.AddRange(CompileEnvironment.Config.SystemIncludePaths);
			foreach (string IncludePath in AllIncludes)
			{
				Arguments += string.Format(" -I\"{0}\"", ConvertPath(Path.GetFullPath(IncludePath)));

				if (ExternalExecution.GetRuntimePlatform() != UnrealTargetPlatform.Mac)
				{
					// sync any third party headers we may need
					if (IncludePath.Contains("ThirdParty"))
					{
						string[] FileList = Directory.GetFiles(IncludePath, "*.h", SearchOption.AllDirectories);
						foreach (string File in FileList)
						{
							FileItem ExternalDependency = FileItem.GetItemByPath(File);
							LocalToRemoteFileItem(ExternalDependency, true);
						}

						FileList = Directory.GetFiles(IncludePath, "*.cpp", SearchOption.AllDirectories);
						foreach (string File in FileList)
						{
							FileItem ExternalDependency = FileItem.GetItemByPath(File);
							LocalToRemoteFileItem(ExternalDependency, true);
						}
					}
				}
			}

			foreach (string Definition in CompileEnvironment.Config.Definitions)
			{
				Arguments += string.Format(" -D\"{0}\"", Definition);
			}

			CPPOutput Result = new CPPOutput();
			// Create a compile action for each source file.
			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = new Action(ActionType.Compile);
				string FileArguments = "";
				string Extension = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant();

				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Compile the file as a C++ PCH.
					FileArguments += GetCompileArguments_PCH();
				}
				else if (Extension == ".C")
				{
					// Compile the file as C code.
					FileArguments += GetCompileArguments_C();
				}
				else if (Extension == ".CC")
				{
					// Compile the file as C++ code.
					FileArguments += GetCompileArguments_CPP();
				}
				else if (Extension == ".MM")
				{
					// Compile the file as Objective-C++ code.
					FileArguments += GetCompileArguments_MM();
				}
				else if (Extension == ".M")
				{
					// Compile the file as Objective-C++ code.
					FileArguments += GetCompileArguments_M();
				}
				else
				{
					// Compile the file as C++ code.
					FileArguments += GetCompileArguments_CPP();

					// only use PCH for .cpp files
					FileArguments += PCHArguments;
				}

				// Add the C++ source file and its included files to the prerequisite item list.
				CompileAction.PrerequisiteItems.Add(SourceFile);

				if (ExternalExecution.GetRuntimePlatform() != UnrealTargetPlatform.Mac)
				{
					QueueFileForBatchUpload(SourceFile);
				}

				foreach (FileItem IncludedFile in CompileEnvironment.GetIncludeDependencies(SourceFile))
				{
					if (ExternalExecution.GetRuntimePlatform() != UnrealTargetPlatform.Mac)
					{
						QueueFileForBatchUpload(IncludedFile);
					}

					CompileAction.PrerequisiteItems.Add(IncludedFile);
				}

				if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Add the precompiled header file to the produced item list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByPath(
						Path.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ".gch"
							)
						);

					FileItem RemotePrecompiledHeaderFile = LocalToRemoteFileItem(PrecompiledHeaderFile, false);
					CompileAction.ProducedItems.Add(RemotePrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = RemotePrecompiledHeaderFile;

					// Add the parameters needed to compile the precompiled header file to the command-line.
					FileArguments += string.Format(" -o \"{0}\"", RemotePrecompiledHeaderFile.AbsolutePath, false);
				}
				else
				{
					if (CompileEnvironment.Config.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);
					}

					// Add the object file to the produced item list.
					FileItem ObjectFile = FileItem.GetItemByPath(
						Path.Combine(
							CompileEnvironment.Config.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ".o"
							)
						);

					FileItem RemoteObjectFile = LocalToRemoteFileItem(ObjectFile, false);
					CompileAction.ProducedItems.Add(RemoteObjectFile);
					Result.ObjectFiles.Add(RemoteObjectFile);
					FileArguments += string.Format(" -o \"{0}\"", RemoteObjectFile.AbsolutePath, false);
				}

				// Add the source file path to the command-line.
				FileArguments += string.Format(" \"{0}\"", ConvertPath(SourceFile.AbsolutePath), false);

				string CompilerPath = XcodeDeveloperDir + "Toolchains/XcodeDefault.xctoolchain/usr/bin/" + IOSCompiler;
				if (!Utils.IsRunningOnMono && ExternalExecution.GetRuntimePlatform() != UnrealTargetPlatform.Mac)
				{
					CompileAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
				}

				// RPC utility parameters are in terms of the Mac side
				CompileAction.WorkingDirectory = GetMacDevSrcRoot();
				CompileAction.CommandPath = CompilerPath;
				CompileAction.CommandArguments = Arguments + FileArguments + CompileEnvironment.Config.AdditionalArguments;
				CompileAction.StatusDescription = string.Format("{0}", Path.GetFileName(SourceFile.AbsolutePath));
				CompileAction.StatusDetailedDescription = SourceFile.Description;
				CompileAction.bIsGCCCompiler = true;
				// We're already distributing the command by execution on Mac.
				CompileAction.bCanExecuteRemotely = false;
				CompileAction.OutputEventHandler = new DataReceivedEventHandler(RemoteOutputReceivedEventHandler);
			}
			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly)
		{
			string LinkerPath = XcodeDeveloperDir + "Toolchains/XcodeDefault.xctoolchain/usr/bin/" + 
				(LinkEnvironment.Config.bIsBuildingLibrary ? IOSArchiver : IOSLinker);

			// Create an action that invokes the linker.
			Action LinkAction = new Action(ActionType.Link);

			if (!Utils.IsRunningOnMono && ExternalExecution.GetRuntimePlatform() != UnrealTargetPlatform.Mac)
			{
				LinkAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}

			// RPC utility parameters are in terms of the Mac side
			LinkAction.WorkingDirectory = GetMacDevSrcRoot();
			LinkAction.CommandPath = LinkerPath;

			// build this up over the rest of the function
			LinkAction.CommandArguments = LinkEnvironment.Config.bIsBuildingLibrary ? GetArchiveArguments_Global(LinkEnvironment) : GetLinkArguments_Global(LinkEnvironment);

			if (!LinkEnvironment.Config.bIsBuildingLibrary)
			{
				// Add the library paths to the argument list.
				foreach (string LibraryPath in LinkEnvironment.Config.LibraryPaths)
				{
					LinkAction.CommandArguments += string.Format(" -L\"{0}\"", LibraryPath);
				}

				// Add the additional libraries to the argument list.
				foreach (string AdditionalLibrary in LinkEnvironment.Config.AdditionalLibraries)
				{
					// for absolute library paths, convert to remote filename
					if (!String.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)))
					{
						// add it to the prerequisites to make sure it's built first (this should be the case of non-system libraries)
						FileItem LibFile = FileItem.GetItemByPath(Path.GetFullPath(AdditionalLibrary));
						FileItem RemoteLibFile = LocalToRemoteFileItem(LibFile, false);
						LinkAction.PrerequisiteItems.Add(RemoteLibFile);

						// and add to the commandline
						LinkAction.CommandArguments += string.Format(" \"{0}\"", ConvertPath(Path.GetFullPath(AdditionalLibrary)));
					}
					else
					{
						LinkAction.CommandArguments += string.Format(" -l\"{0}\"", AdditionalLibrary);
					}
				}
			}

			if (ExternalExecution.GetRuntimePlatform() != UnrealTargetPlatform.Mac)
			{
				// Add any additional files that we'll need in order to link the app
				foreach (string AdditionalShadowFile in LinkEnvironment.Config.AdditionalShadowFiles)
				{
					FileItem ShadowFile = FileItem.GetExistingItemByPath(AdditionalShadowFile);
					if (ShadowFile != null)
					{
						QueueFileForBatchUpload(ShadowFile);
						LinkAction.PrerequisiteItems.Add(ShadowFile);
					}
					else
					{
						throw new BuildException("Couldn't find required additional file to shadow: {0}", AdditionalShadowFile);
					}
				}
			}

			// Handle additional framework assets that might need to be shadowed
			foreach ( UEBuildFramework Framework in LinkEnvironment.Config.AdditionalFrameworks )
			{
				if ( Framework.OwningModule == null || Framework.FrameworkZipPath == null || Framework.FrameworkZipPath  == "" )
				{
					continue;	// Only care about frameworks that have a zip specified
				}

				// If we've already remembered this framework, skip
				if ( RememberedAdditionalFrameworks.Contains( Framework ) )
				{
					continue;
				}

				// Remember any files we need to unzip
				RememberedAdditionalFrameworks.Add( Framework );

				// Copy them to remote mac if needed
				if ( ExternalExecution.GetRuntimePlatform() != UnrealTargetPlatform.Mac )
				{
					FileItem ShadowFile = FileItem.GetExistingItemByPath( GetLocalFrameworkZipPath( Framework ) );

					if ( ShadowFile != null )
					{
						QueueFileForBatchUpload( ShadowFile );
						LinkAction.PrerequisiteItems.Add( ShadowFile );
					}
					else
					{
						throw new BuildException( "Couldn't find required additional file to shadow: {0}", Framework.FrameworkZipPath );
					}
				}
			}

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByPath(Path.GetFullPath(LinkEnvironment.Config.OutputFilePath));
			FileItem RemoteOutputFile = LocalToRemoteFileItem(OutputFile, false);
			LinkAction.ProducedItems.Add(RemoteOutputFile);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			// Write the list of input files to a response file, with a tempfilename, on remote machine
			if (LinkEnvironment.Config.bIsBuildingLibrary)
			{
				foreach (string Filename in InputFileNames)
				{
					LinkAction.CommandArguments += " " + Filename;
				}
				// @todo rocket lib: the -filelist command should take a response file (see else condition), except that it just says it can't
				// find the file that's in there. Rocket.lib may overflow the commandline by putting all files on the commandline, so this 
				// may be needed:
				// LinkAction.CommandArguments += string.Format(" -filelist \"{0}\"", ConvertPath(ResponsePath));
			}
			else
			{
				string ResponsePath = Path.GetFullPath("..\\Intermediate\\IOS\\LinkFileList_" + Path.GetFileNameWithoutExtension(LinkEnvironment.Config.OutputFilePath) + ".tmp"); 
				if (!Utils.IsRunningOnMono && ExternalExecution.GetRuntimePlatform () != UnrealTargetPlatform.Mac)
				{
					ResponseFile.Create (ResponsePath, InputFileNames);
					RPCUtilHelper.CopyFile (ResponsePath, ConvertPath (ResponsePath), true);
				}
				else
				{
					ResponseFile.Create(ConvertPath(ResponsePath), InputFileNames);
				}
				LinkAction.CommandArguments += string.Format(" @\"{0}\"", ConvertPath(ResponsePath));
			}

			// Add the output file to the command-line.
			LinkAction.CommandArguments += string.Format(" -o \"{0}\"", RemoteOutputFile.AbsolutePath);

			// Add the additional arguments specified by the environment.
			LinkAction.CommandArguments += LinkEnvironment.Config.AdditionalArguments;

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			LinkAction.StatusDescription = string.Format("{0}", OutputFile.AbsolutePath);
			LinkAction.OutputEventHandler = new DataReceivedEventHandler(RemoteOutputReceivedEventHandler);
			// For iPhone, generate the dSYM file if the config file is set to do so
			if (BuildConfiguration.bGeneratedSYMFile == true)
			{
				Log.TraceInformation("Generating the dSYM file - this will add some time to your build...");
				RemoteOutputFile = GenerateDebugInfo(RemoteOutputFile);
			}

			return RemoteOutputFile;
		}

		/**
		 * Generates debug info for a given executable
		 * 
		 * @param Executable FileItem describing the executable to generate debug info for
		 */
		static public FileItem GenerateDebugInfo(FileItem Executable)
		{
			// Make a file item for the source and destination files
			string FullDestPathRoot = Executable.AbsolutePath + ".app.dSYM";
			string FullDestPath = FullDestPathRoot;
			FileItem DestFile = FileItem.GetRemoteItemByPath(FullDestPath, UnrealTargetPlatform.IOS);

			// Make the compile action
			Action GenDebugAction = new Action(ActionType.GenerateDebugInfo);
			if (!Utils.IsRunningOnMono)
			{
				GenDebugAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			}
			GenDebugAction.WorkingDirectory = GetMacDevSrcRoot();
			GenDebugAction.CommandPath = "sh";

			// note that the source and dest are switched from a copy command
			GenDebugAction.CommandArguments = string.Format("-c '{0}/usr/bin/dsymutil {1} -o {2}; cd {2}/..; zip -r -y -1 {3}.app.dSYM.zip {3}.app.dSYM'",
				XcodeDeveloperDir,
				Executable.AbsolutePath,
				FullDestPathRoot,
				Path.GetFileName(Executable.AbsolutePath));
			GenDebugAction.PrerequisiteItems.Add(Executable);
			GenDebugAction.ProducedItems.Add(DestFile);
			GenDebugAction.StatusDescription = GenDebugAction.CommandArguments;// string.Format("Generating debug info for {0}", Path.GetFileName(Executable.AbsolutePath));
			GenDebugAction.bCanExecuteRemotely = false;

			return DestFile;
		}

		private void PackageStub(string BinaryPath, string GameName, string ExeName)
		{
			// create the ipa
			string IPAName = BinaryPath + "/" + ExeName + ".stub";
			// delete the old one
			if (File.Exists(IPAName))
			{
				File.Delete(IPAName);
			}

			// make the subdirectory if needed
			string DestSubdir = Path.GetDirectoryName(IPAName);
			if (!Directory.Exists(DestSubdir))
			{
				Directory.CreateDirectory(DestSubdir);
			}

			// set up the directories
			string ZipWorkingDir = String.Format("Payload/{0}.app/", GameName);
			string ZipSourceDir = string.Format("{0}/Payload/{1}.app", BinaryPath, GameName);

			// create the file
			using (ZipFile Zip = new ZipFile())
			{
				// add the entire directory
				Zip.AddDirectory(ZipSourceDir, ZipWorkingDir);

				// Update permissions to be UNIX-style
				// Modify the file attributes of any added file to unix format
				foreach (ZipEntry E in Zip.Entries)
				{
					const byte FileAttributePlatform_NTFS = 0x0A;
					const byte FileAttributePlatform_UNIX = 0x03;
					const byte FileAttributePlatform_FAT = 0x00;

					const int UNIX_FILETYPE_NORMAL_FILE = 0x8000;
					//const int UNIX_FILETYPE_SOCKET = 0xC000;
					//const int UNIX_FILETYPE_SYMLINK = 0xA000;
					//const int UNIX_FILETYPE_BLOCKSPECIAL = 0x6000;
					const int UNIX_FILETYPE_DIRECTORY = 0x4000;
					//const int UNIX_FILETYPE_CHARSPECIAL = 0x2000;
					//const int UNIX_FILETYPE_FIFO = 0x1000;

					const int UNIX_EXEC = 1;
					const int UNIX_WRITE = 2;
					const int UNIX_READ = 4;


					int MyPermissions = UNIX_READ | UNIX_WRITE;
					int OtherPermissions = UNIX_READ;

					int PlatformEncodedBy = (E.VersionMadeBy >> 8) & 0xFF;
					int LowerBits = 0;

					// Try to preserve read-only if it was set
					bool bIsDirectory = E.IsDirectory;

					// Check to see if this 
					bool bIsExecutable = false;
					if (Path.GetFileNameWithoutExtension(E.FileName).Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
					{
						bIsExecutable = true;
					}

					if (bIsExecutable)
					{
						// The executable will be encrypted in the final distribution IPA and will compress very poorly, so keeping it
						// uncompressed gives a better indicator of IPA size for our distro builds
						E.CompressionLevel = CompressionLevel.None;
					}

					if ((PlatformEncodedBy == FileAttributePlatform_NTFS) || (PlatformEncodedBy == FileAttributePlatform_FAT))
					{
						FileAttributes OldAttributes = E.Attributes;
						//LowerBits = ((int)E.Attributes) & 0xFFFF;

						if ((OldAttributes & FileAttributes.Directory) != 0)
						{
							bIsDirectory = true;
						}

						// Permissions
						if ((OldAttributes & FileAttributes.ReadOnly) != 0)
						{
							MyPermissions &= ~UNIX_WRITE;
							OtherPermissions &= ~UNIX_WRITE;
						}
					}

					if (bIsDirectory || bIsExecutable)
					{
						MyPermissions |= UNIX_EXEC;
						OtherPermissions |= UNIX_EXEC;
					}

					// Re-jigger the external file attributes to UNIX style if they're not already that way
					if (PlatformEncodedBy != FileAttributePlatform_UNIX)
					{
						int NewAttributes = bIsDirectory ? UNIX_FILETYPE_DIRECTORY : UNIX_FILETYPE_NORMAL_FILE;

						NewAttributes |= (MyPermissions << 6);
						NewAttributes |= (OtherPermissions << 3);
						NewAttributes |= (OtherPermissions << 0);

						// Now modify the properties
						E.AdjustExternalFileAttributes(FileAttributePlatform_UNIX, (NewAttributes << 16) | LowerBits);
					}
				}

				// Save it out
				Zip.Save(IPAName);
			}
		}

		public override void PreBuildSync()
		{
			if (ExternalExecution.GetRuntimePlatform() != UnrealTargetPlatform.Mac)
			{
				BuiltBinaries = new List<string>();
			}

			base.PreBuildSync();

			// Unzip any third party frameworks that are stored as zips
			foreach ( UEBuildFramework Framework in RememberedAdditionalFrameworks )
			{
				string LocalZipPath = GetLocalFrameworkZipPath( Framework );

				FileItem FrameworkZipItem = FileItem.GetExistingItemByPath( LocalZipPath );

				if ( FrameworkZipItem == null )
				{
					Log.TraceInformation( "FrameworkZipItem not found for {0}", LocalZipPath );
					continue;
				}

				if ( ExternalExecution.GetRuntimePlatform() == UnrealTargetPlatform.Mac )
				{
					Log.TraceInformation( "Unzipping {0}", FrameworkZipItem.AbsolutePath );

					// If we're on the mac, just unzip using the shell
					string ResultsText;
					string LocalUnzipZipPath = FrameworkZipItem.AbsolutePath.Substring( 0, FrameworkZipItem.AbsolutePath.LastIndexOf( '/' ) );
					RunExecutableAndWait( "unzip", String.Format( "-o {0} -d {1}", FrameworkZipItem.AbsolutePath, LocalUnzipZipPath ), out ResultsText );
					continue;
				}

				// We copied using RPC utility, we need to unzip using RPC utility as well
				FileItem RemoteShadowFile = LocalToRemoteFileItem( FrameworkZipItem, false );

				string ZipPath = RemoteShadowFile.AbsolutePath.Substring( 0, RemoteShadowFile.AbsolutePath.LastIndexOf( '/' ) );

				Log.TraceInformation( "Unzipping: {0}", RemoteShadowFile.AbsolutePath );

				Hashtable Result = RPCUtilHelper.Command( "/", String.Format( "unzip -o {0} -d {1}", RemoteShadowFile.AbsolutePath, ZipPath ), "", null );

				foreach ( DictionaryEntry Entry in Result )
				{
					Log.TraceInformation( "{0}", Entry.Value );
				}
			}
		}

		public override void PostBuildSync(UEBuildTarget Target)
		{
			base.PostBuildSync(Target);

			string AppName = Target.Rules.Type == TargetRules.TargetType.Game ? Target.GameName : Target.AppName;

			if (ExternalExecution.GetRuntimePlatform() == UnrealTargetPlatform.Mac)
			{
				string RemoteShadowDirectoryMac = Path.GetDirectoryName(Target.OutputPath);
				string FinalRemoteExecutablePath = String.Format("{0}/Payload/{1}.app/{1}", RemoteShadowDirectoryMac, Target.GameName);

				// strip the debug info from the executable if needed
				if (BuildConfiguration.bStripSymbolsOnIOS || (Target.Configuration == UnrealTargetConfiguration.Shipping))
				{
					Process StripProcess = new Process();
					StripProcess.StartInfo.WorkingDirectory = RemoteShadowDirectoryMac;
					StripProcess.StartInfo.FileName = "/usr/bin/xcrun";
					StripProcess.StartInfo.Arguments = "strip \"" + Target.OutputPath + "\"";
					StripProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);
					StripProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);

					OutputReceivedDataEventHandlerEncounteredError = false;
					OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
					Utils.RunLocalProcess(StripProcess);
					if (OutputReceivedDataEventHandlerEncounteredError)
					{
						throw new Exception(OutputReceivedDataEventHandlerEncounteredErrorMessage);
					}
				}

				// copy the executable
				if (!File.Exists(FinalRemoteExecutablePath))
				{
					Directory.CreateDirectory(String.Format("{0}/Payload/{1}.app", RemoteShadowDirectoryMac, Target.GameName));
				}
				File.Copy(Target.OutputPath, FinalRemoteExecutablePath, true);

				if (BuildConfiguration.bCreateStubIPA)
				{
					string Project = Target.ProjectDirectory + "/" + Target.GameName + ".uproject";

					// generate the dummy project so signing works
					if (Target.GameName == "UE4Game" || Target.GameName == "UE4Client" || Utils.IsFileUnderDirectory(Target.ProjectDirectory + "/" + Target.GameName + ".uproject", Path.GetFullPath("../..")))
					{
						UnrealBuildTool.GenerateProjectFiles (new XcodeProjectFileGenerator (), new string[] {"-platforms=IOS", "-NoIntellIsense", "-iosdeployonly", "-ignorejunk"});
						Project = Path.GetFullPath("../..") + "/UE4_IOS.xcodeproj";
					}
					else
					{
						Project = Target.ProjectDirectory + "/" + Target.GameName + ".xcodeproj";
					}

					if (Directory.Exists (Project))
					{
						// ensure the plist, entitlements, and provision files are properly copied
						UEBuildDeploy DeployHandler = UEBuildDeploy.GetBuildDeploy(Target.Platform);
						if (DeployHandler != null)
						{
							DeployHandler.PrepTargetForDeployment(Target);
						}

						// code sign the project
						string CmdLine = XcodeDeveloperDir + "usr/bin/xcodebuild" +
						                " -project \"" + Project + "\"" +
						                " -configuration " + Target.Configuration +
						                " -scheme '" + Target.GameName + " - iOS'" +
						                " -sdk iphoneos" +
						                " CODE_SIGN_IDENTITY=\"iPhone Developer\"";

                        Console.WriteLine("Code signing with command line: " + CmdLine);

						Process SignProcess = new Process ();
						SignProcess.StartInfo.WorkingDirectory = RemoteShadowDirectoryMac;
						SignProcess.StartInfo.FileName = "/usr/bin/xcrun";
						SignProcess.StartInfo.Arguments = CmdLine;
						SignProcess.OutputDataReceived += new DataReceivedEventHandler (OutputReceivedDataEventHandler);
						SignProcess.ErrorDataReceived += new DataReceivedEventHandler (OutputReceivedDataEventHandler);

						OutputReceivedDataEventHandlerEncounteredError = false;
						OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
						Utils.RunLocalProcess (SignProcess);

						// delete the temp project
						if (Project.Contains ("_IOS.xcodeproj"))
						{
							Directory.Delete (Project, true);
						}

						if (OutputReceivedDataEventHandlerEncounteredError)
						{
							throw new Exception (OutputReceivedDataEventHandlerEncounteredErrorMessage);
						}

						// Package the stub
						PackageStub (RemoteShadowDirectoryMac, Target.GameName, Path.GetFileNameWithoutExtension (Target.OutputPath));
					}
				}

				{
					// Copy bundled assets from additional frameworks to the intermediate assets directory (so they can get picked up during staging)
					String LocalFrameworkAssets = Path.GetFullPath( Target.ProjectDirectory + "/Intermediate/IOS/FrameworkAssets" );

					// Delete the local dest directory if it exists
					if ( Directory.Exists( LocalFrameworkAssets ) )
					{
						Directory.Delete( LocalFrameworkAssets, true );
					}

					// Create the intermediate local directory
					string ResultsText;
					RunExecutableAndWait( "mkdir", String.Format( "-p {0}", LocalFrameworkAssets ), out ResultsText );

					foreach ( UEBuildFramework Framework in RememberedAdditionalFrameworks )
					{
						if ( Framework.OwningModule == null || Framework.CopyBundledAssets == null || Framework.CopyBundledAssets == "" )
						{
							continue;		// Only care if we need to copy bundle assets
						}

						string LocalZipPath = GetLocalFrameworkZipPath( Framework );

						LocalZipPath = LocalZipPath.Replace( ".zip", "" );

						// For now, this is hard coded, but we need to loop over all modules, and copy bundled assets that need it
						string LocalSource	= LocalZipPath + "/" + Framework.CopyBundledAssets;
						string BundleName	= Framework.CopyBundledAssets.Substring( Framework.CopyBundledAssets.LastIndexOf( '/' ) + 1 );
						string LocalDest	= LocalFrameworkAssets + "/" + BundleName;

						Log.TraceInformation( "Copying bundled asset... LocalSource: {0}, LocalDest: {1}", LocalSource, LocalDest );

						RunExecutableAndWait( "cp", String.Format( "-R -L {0} {1}", LocalSource, LocalDest ), out ResultsText );
					}
				}
			}
			else
			{
				// store off the binaries
				foreach (UEBuildBinary Binary in Target.AppBinaries)
				{
					BuiltBinaries.Add(Path.GetFullPath(Binary.ToString()));
				}

				// Generate static libraries for monolithic games in Rocket
				if ((UnrealBuildTool.BuildingRocket() || UnrealBuildTool.RunningRocket()) && TargetRules.IsAGame(Target.Rules.Type))
				{
					List<UEBuildModule> Modules = Target.AppBinaries[0].GetAllDependencyModules(true, false);
					foreach (UEBuildModuleCPP Module in Modules.OfType<UEBuildModuleCPP>())
					{
						if (Utils.IsFileUnderDirectory(Module.ModuleDirectory, BuildConfiguration.RelativeEnginePath) && Module.Binary == Target.AppBinaries[0])
						{
							if (Module.bBuildingRedistStaticLibrary)
							{
								BuiltBinaries.Add(Path.GetFullPath(Module.RedistStaticLibraryPath));
							}
						}
					}
				}

				// check to see if the DangerouslyFast mode is valid (in other words, a build has gone through since a Rebuild/Clean operation)
				string DangerouslyFastValidFile = Path.Combine(Target.GlobalLinkEnvironment.Config.IntermediateDirectory, "DangerouslyFastIsNotDangerous");
				bool bUseDangerouslyFastModeWasRequested = bUseDangerouslyFastMode;
				if (bUseDangerouslyFastMode)
				{
					if (!File.Exists(DangerouslyFastValidFile))
					{
						Log.TraceInformation("Dangeroulsy Fast mode was requested, but a slow mode hasn't completed. Performing slow now...");
						bUseDangerouslyFastMode = false;
					}
				}	
	
				string RemoteExecutablePath = ConvertPath(Target.OutputPath);
	
				// when going super fast, just copy the executable to the final resting spot
				if (bUseDangerouslyFastMode)
				{
					Log.TraceInformation("==============================================================================");
					Log.TraceInformation("USING DANGEROUSLY FAST MODE! IF YOU HAVE ANY PROBLEMS, TRY A REBUILD/CLEAN!!!!");
					Log.TraceInformation("==============================================================================");	

					// copy the executable
					string RemoteShadowDirectoryMac = ConvertPath(Path.GetDirectoryName(Target.OutputPath));
					string FinalRemoteExecutablePath = String.Format("{0}/Payload/{1}.app/{1}", RemoteShadowDirectoryMac, Target.GameName);
					RPCUtilHelper.Command("/", String.Format("cp -f {0} {1}", RemoteExecutablePath, FinalRemoteExecutablePath), "", null);
				}
				else
				{
					RPCUtilHelper.CopyFile(RemoteExecutablePath, Target.OutputPath, false);

					if (BuildConfiguration.bGeneratedSYMFile == true)
					{
						string DSYMExt = ".app.dSYM.zip";
						RPCUtilHelper.CopyFile(RemoteExecutablePath + DSYMExt, Target.OutputPath + DSYMExt, false);
					}
				}

				// Generate the stub
				if (BuildConfiguration.bCreateStubIPA || bUseDangerouslyFastMode)
				{
					if (!bUseDangerouslyFastMode)
					{
						// generate the dummy project so signing works
						UnrealBuildTool.GenerateProjectFiles(new XcodeProjectFileGenerator(), new string[] { "-NoIntellisense", "-iosdeployonly" });
					}

					Process StubGenerateProcess = new Process();
					StubGenerateProcess.StartInfo.WorkingDirectory = Path.GetFullPath("..\\Binaries\\DotNET\\IOS");
					StubGenerateProcess.StartInfo.FileName = Path.Combine(StubGenerateProcess.StartInfo.WorkingDirectory, "iPhonePackager.exe");
						
					string PathToApp = RulesCompiler.GetTargetFilename(AppName);

					// right now, no programs have a Source subdirectory, so assume the PathToApp is directly in the root
					if (Path.GetDirectoryName(PathToApp).Contains(@"\Engine\Source\Programs"))
					{
						PathToApp = Path.GetDirectoryName(PathToApp);
					}
					else
					{
						Int32 SourceIndex = PathToApp.LastIndexOf(@"\Source");
						if (SourceIndex != -1)
						{
							PathToApp = PathToApp.Substring(0, SourceIndex);
						}
						else
						{
							throw new BuildException("The target was not in a /Source subdirectory");
						}
					}

					if (bUseDangerouslyFastMode)
					{
						// the quickness!
						StubGenerateProcess.StartInfo.Arguments = "DangerouslyFast " + PathToApp;
					}
					else
					{
						StubGenerateProcess.StartInfo.Arguments = "PackageIPA " + PathToApp + " -createstub";
						// if we are making the dsym, then we can strip the debug info from the executable
						if (BuildConfiguration.bStripSymbolsOnIOS || (Target.Configuration == UnrealTargetConfiguration.Shipping))
						{
							StubGenerateProcess.StartInfo.Arguments += " -strip";
						}
					}
					StubGenerateProcess.StartInfo.Arguments += " -config " + Target.Configuration + " -mac " + RemoteServerName;

					UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Target.Platform);
					string Architecture = BuildPlatform.GetActiveArchitecture();
					if (Architecture != "")
					{
						// pass along the architecture if we need, skipping the initial -, so we have "-architecture simulator"
						StubGenerateProcess.StartInfo.Arguments += " -architecture " + Architecture.Substring(1);
					}

					// programmers that use Xcode packaging mode should use the following commandline instead, as it will package for Xcode on each compile
					//				StubGenerateProcess.StartInfo.Arguments = "PackageApp " + GameName + " " + Configuration;

					StubGenerateProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);
					StubGenerateProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);

					OutputReceivedDataEventHandlerEncounteredError = false;
					OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
					Utils.RunLocalProcess(StubGenerateProcess);
					if (OutputReceivedDataEventHandlerEncounteredError)
					{
						throw new Exception(OutputReceivedDataEventHandlerEncounteredErrorMessage);
					}
					
					// now that a slow mode sync has finished, we can now do DangerouslyFast mode again (if requested)
					if (bUseDangerouslyFastModeWasRequested)
					{
						File.Create(DangerouslyFastValidFile);
					}
					else
					{
						// if we didn't want dangerously fast, then delete the file so that setting/unsetting the flag will do the right thing without a Rebuild
						File.Delete(DangerouslyFastValidFile);
					}
				}

				{
					// Copy bundled assets from additional frameworks to the intermediate assets directory (so they can get picked up during staging)
					String LocalFrameworkAssets = Path.GetFullPath( Target.ProjectDirectory + "/Intermediate/IOS/FrameworkAssets" );
					String RemoteFrameworkAssets = ConvertPath( LocalFrameworkAssets );

					// Delete the intermediate directory on the mac
					RPCUtilHelper.Command( "/", String.Format( "rm -rf {0}", RemoteFrameworkAssets ), "", null );

					// Create a fresh intermediate after we delete it
					RPCUtilHelper.Command( "/", String.Format( "mkdir -p {0}", RemoteFrameworkAssets ), "", null );

					// Delete the local dest directory if it exists
					if ( Directory.Exists( LocalFrameworkAssets ) )
					{
						Directory.Delete( LocalFrameworkAssets, true );
					}

					foreach ( UEBuildFramework Framework in RememberedAdditionalFrameworks )
					{
						if ( Framework.OwningModule == null || Framework.CopyBundledAssets == null || Framework.CopyBundledAssets == "" )
						{
							continue;		// Only care if we need to copy bundle assets
						}

						string RemoteZipPath = GetRemoteFrameworkZipPath( Framework );

						RemoteZipPath = RemoteZipPath.Replace( ".zip", "" );

						// For now, this is hard coded, but we need to loop over all modules, and copy bundled assets that need it
						string RemoteSource = RemoteZipPath + "/" + Framework.CopyBundledAssets;
						string BundleName	= Framework.CopyBundledAssets.Substring( Framework.CopyBundledAssets.LastIndexOf( '/' ) + 1 );

						String RemoteDest	= RemoteFrameworkAssets + "/" + BundleName;
						String LocalDest	= LocalFrameworkAssets + "\\" + BundleName;

						Log.TraceInformation( "Copying bundled asset... RemoteSource: {0}, RemoteDest: {1}, LocalDest: {2}", RemoteSource, RemoteDest, LocalDest );

						Hashtable Results = RPCUtilHelper.Command( "/", String.Format( "cp -R -L {0} {1}", RemoteSource, RemoteDest ), "", null );

						foreach ( DictionaryEntry Entry in Results )
						{
							Log.TraceInformation( "{0}", Entry.Value );
						}

						// Copy the bundled resource from the remote mac to the local dest
						RPCUtilHelper.CopyDirectory( RemoteDest, LocalDest, RPCUtilHelper.ECopyOptions.None );
					}
				}
			}
		}

		public static int RunExecutableAndWait( string ExeName, string ArgumentList, out string StdOutResults )
		{
			// Create the process
			ProcessStartInfo PSI = new ProcessStartInfo( ExeName, ArgumentList );
			PSI.RedirectStandardOutput = true;
			PSI.UseShellExecute = false;
			PSI.CreateNoWindow = true;
			Process NewProcess = Process.Start( PSI );

			// Wait for the process to exit and grab it's output
			StdOutResults = NewProcess.StandardOutput.ReadToEnd();
			NewProcess.WaitForExit();
			return NewProcess.ExitCode;
		}
	};
}
