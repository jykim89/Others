// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml;
using System.Xml.Serialization;
using System.Runtime.CompilerServices;
using System.Linq;

namespace UnrealBuildTool
{
	public class PrecompileHeaderEnvironment
	{
		/** The name of the module this PCH header is a member of */
		public readonly string ModuleName;

		/** PCH header file name as it appears in an #include statement in source code (might include partial, or no relative path.)
			This is needed by some compilers to use PCH features. */
		public string PCHHeaderNameInCode;

		/** The source header file that this precompiled header will be generated for */
		public readonly FileItem PrecompiledHeaderIncludeFilename;

		/** Whether this precompiled header will be built with CLR features enabled.  We won't mix and match CLR PCHs with non-CLR PCHs */
		public readonly CPPCLRMode CLRMode;

		/** Whether this precompiled header will be built with code optimization enabled. */
		public readonly ModuleRules.CodeOptimization OptimizeCode;

		/** The PCH file we're generating */
		public FileItem PrecompiledHeaderFile = null;

		/** Object files emitted from the compiler when generating this precompiled header.  These will be linked into modules that
			include this PCH */
		public readonly List<FileItem> OutputObjectFiles = new List<FileItem>();

		public PrecompileHeaderEnvironment( string InitModuleName, string InitPCHHeaderNameInCode, FileItem InitPrecompiledHeaderIncludeFilename, CPPCLRMode InitCLRMode, ModuleRules.CodeOptimization InitOptimizeCode )
		{
			ModuleName = InitModuleName;
			PCHHeaderNameInCode = InitPCHHeaderNameInCode;
			PrecompiledHeaderIncludeFilename = InitPrecompiledHeaderIncludeFilename;
			CLRMode = InitCLRMode;
			OptimizeCode = InitOptimizeCode;
		}

		/// <summary>
		/// Creates a precompiled header action to generate a new pch file 
		/// </summary>
		/// <param name="PCHHeaderNameInCode">The precompiled header name as it appeared in an #include statement</param>
		/// <param name="PrecompiledHeaderIncludeFilename">Name of the header used for pch.</param>
		/// <param name="ProjectCPPEnvironment">The environment the C/C++ files in the project are compiled with.</param>
		/// <param name="OutputDirectory">The folder to save the generated PCH file to</param>
		/// <param name="ModuleName">Name of the module this PCH is being generated for</param>
		/// <param name="bAllowDLLExports">True if we should allow DLLEXPORT definitions for this PCH</param>
		/// <returns>the compilation output result of the created pch.</returns>
		public static CPPOutput GeneratePCHCreationAction(string PCHHeaderNameInCode, FileItem PrecompiledHeaderIncludeFilename, CPPEnvironment ProjectCPPEnvironment, string OutputDirectory, string ModuleName, bool bAllowDLLExports )
		{
			// Find the header file to be precompiled. Don't skip external headers
			if (PrecompiledHeaderIncludeFilename.bExists)
			{
				// Create a Dummy wrapper around the PCH to avoid problems with #pragma once on clang
				var ToolChain = UEToolChain.GetPlatformToolChain(ProjectCPPEnvironment.Config.TargetPlatform);
				string PCHGuardDefine = Path.GetFileNameWithoutExtension(PrecompiledHeaderIncludeFilename.AbsolutePath).ToUpper();
				string LocalPCHHeaderNameInCode = ToolChain.ConvertPath(PrecompiledHeaderIncludeFilename.AbsolutePath);
				string TmpPCHHeaderContents = String.Format("#ifndef __AUTO_{0}_H__\n#define __AUTO_{0}_H__\n//Last Write: {2}\n#include \"{1}\"\n#endif//__AUTO_{0}_H__", PCHGuardDefine, LocalPCHHeaderNameInCode, PrecompiledHeaderIncludeFilename.LastWriteTime);
				string DummyPath = Path.Combine(
					ProjectCPPEnvironment.Config.OutputDirectory,
					Path.GetFileName(PrecompiledHeaderIncludeFilename.AbsolutePath));
				FileItem DummyPCH = FileItem.CreateIntermediateTextFile(DummyPath, TmpPCHHeaderContents);

				// Create a new C++ environment that is used to create the PCH.
				CPPEnvironment ProjectPCHEnvironment = new CPPEnvironment(ProjectCPPEnvironment);
				ProjectPCHEnvironment.Config.PrecompiledHeaderAction = PrecompiledHeaderAction.Create;
				ProjectPCHEnvironment.Config.PrecompiledHeaderIncludeFilename = PrecompiledHeaderIncludeFilename.AbsolutePath;
				ProjectPCHEnvironment.Config.PCHHeaderNameInCode = PCHHeaderNameInCode;
				ProjectPCHEnvironment.Config.OutputDirectory = OutputDirectory;

				if( !bAllowDLLExports )
				{
					for( var CurDefinitionIndex = 0; CurDefinitionIndex < ProjectPCHEnvironment.Config.Definitions.Count; ++CurDefinitionIndex )
					{
						// We change DLLEXPORT to DLLIMPORT for "shared" PCH headers
						var OldDefinition = ProjectPCHEnvironment.Config.Definitions[ CurDefinitionIndex ];
						if( OldDefinition.EndsWith( "=DLLEXPORT" ) )
						{
							ProjectPCHEnvironment.Config.Definitions[ CurDefinitionIndex ] = OldDefinition.Replace( "DLLEXPORT", "DLLIMPORT" );
						}
					}
				}

				Log.TraceVerbose( "Found PCH file \"{0}\".", PrecompiledHeaderIncludeFilename );

				// Create the action to compile the PCH file.
				return ProjectPCHEnvironment.CompileFiles(new List<FileItem>() { DummyPCH }, ModuleName);
			}
			throw new BuildException( "Couldn't find PCH file \"{0}\".", PrecompiledHeaderIncludeFilename );
		}
	}

	public abstract class Utils
	{
		/// <summary>
		/// Whether we are currently running on Mono platform.  We cache this statically because it is a bit slow to check.
		/// </summary>
		public static readonly bool IsRunningOnMono = Type.GetType( "Mono.Runtime" ) != null;

		/** Searches for a flag in a set of command-line arguments. */
		public static bool ParseCommandLineFlag(string[] Arguments, string FlagName, out int ArgumentIndex )
		{
			// Find an argument with the given name.
			for (ArgumentIndex = 0; ArgumentIndex < Arguments.Length; ArgumentIndex++)
			{
				string Argument = Arguments[ArgumentIndex].ToUpperInvariant();
				if (Argument == FlagName.ToUpperInvariant())
				{
					return true;
				}
			}
			return false;
		}

		/** Regular expression to match $(ENV) and/ or %ENV% environment variables. */
		static Regex EnvironmentVariableRegex = new Regex( @"\$\((.*?)\)|\%(.*?)\%", RegexOptions.None );

		/**
		 * Resolves $(ENV) and/ or %ENV% to the value of the environment variable in the passed in string.
		 * 
		 * @param	InString	String to resolve environment variable in.
		 * @return	String with environment variable expanded/ resolved.
		 */
		public static string ResolveEnvironmentVariable( string InString )
		{
			string Result = InString;
		
			// Try to find $(ENV) substring.
			Match M = EnvironmentVariableRegex.Match( InString );

			// Iterate over all matches, resolving the match to an environment variable.
			while( M.Success )
			{
				// Convoluted way of stripping first and last character and '(' in the case of $(ENV) to get to ENV
				string EnvironmentVariable = M.ToString();
				if ( EnvironmentVariable.StartsWith("$") && EnvironmentVariable.EndsWith(")") )
				{
					EnvironmentVariable = EnvironmentVariable.Substring(1, EnvironmentVariable.Length - 2).Replace("(", "");
				}

				if ( EnvironmentVariable.StartsWith("%") && EnvironmentVariable.EndsWith("%") )
				{
					EnvironmentVariable = EnvironmentVariable.Substring(1, EnvironmentVariable.Length - 2);
				}
			
				// Resolve environment variable.				
				Result = Result.Replace( M.ToString(), Environment.GetEnvironmentVariable( EnvironmentVariable ) );

				// Move on to next match. Multiple environment variables are handled correctly by regexp.
				M = M.NextMatch();
			}
				
			return Result;
		}
				
		/**
		 * This is a faster replacement of File.ReadAllText. Code snippet based on code 
		 * and analysis by Sam Allen
		 * 
		 * http://dotnetperls.com/Content/File-Handling.aspx
		 * 
		 * @param	SourceFile		Source file to fully read and convert to string
		 * @return	Textual representation of file.
		 */
		public static string ReadAllText( string SourceFile )
		{
			using( StreamReader Reader = new StreamReader( SourceFile, System.Text.Encoding.UTF8 ) )
			{
				return Reader.ReadToEnd();
			}
		}

		/**
		 * Reads the specified environment variable
		 *
		 * @param	VarName		the environment variable to read
		 * @param	bDefault	the default value to use if missing
		 * @return	the value of the environment variable if found and the default value if missing
		 */
		public static bool GetEnvironmentVariable(string VarName, bool bDefault)
		{
			string Value = Environment.GetEnvironmentVariable(VarName);
			if (Value != null)
			{
				// Convert the string to its boolean value
				return Convert.ToBoolean(Value);
			}
			return bDefault;
		}

		/**
		 * Reads the specified environment variable
		 *
		 * @param	VarName		the environment variable to read
		 * @param	Default	the default value to use if missing
		 * @return	the value of the environment variable if found and the default value if missing
		 */
		public static string GetStringEnvironmentVariable(string VarName, string Default)
		{
			string Value = Environment.GetEnvironmentVariable(VarName);
			if (Value != null)
			{
				return Value;
			}
			return Default;
		}

		/**
		 * Reads the specified environment variable
		 *
		 * @param	VarName		the environment variable to read
		 * @param	Default	the default value to use if missing
		 * @return	the value of the environment variable if found and the default value if missing
		 */
		public static double GetEnvironmentVariable(string VarName, double Default)
		{
			string Value = Environment.GetEnvironmentVariable(VarName);
			if (Value != null)
			{
				return Convert.ToDouble(Value);
			}
			return Default;
		}

		/**
		 * Reads the specified environment variable
		 *
		 * @param	VarName		the environment variable to read
		 * @param	Default	the default value to use if missing
		 * @return	the value of the environment variable if found and the default value if missing
		 */
		public static string GetEnvironmentVariable(string VarName, string Default)
		{
			string Value = Environment.GetEnvironmentVariable(VarName);
			if (Value != null)
			{
				return Value;
			}
			return Default;
		}

		/**
		 * Sets the environment variables from the passed in batch file
		 * 
		 * @param	BatchFileName	Name of the batch file to parse
		 */
		public static void SetEnvironmentVariablesFromBatchFile(string BatchFileName)
		{
			if( File.Exists( BatchFileName ) )
			{
				// Create a wrapper batch file that echoes environment variables to a text file
				var EnvOutputFileName = Path.GetTempFileName();
				var EnvReaderBatchFileName = EnvOutputFileName + ".bat";
				Log.TraceVerbose( "Creating .bat file {0} for harvesting environment variables.", EnvReaderBatchFileName );

				{
					var EnvReaderBatchFileContent = new List<string>();

					// Run 'vcvars32.bat' (or similar x64 version) to set environment variables
					EnvReaderBatchFileContent.Add( String.Format( "call \"{0}\"", BatchFileName ) );

					// Pipe all environment variables to a file where we can read them in
					EnvReaderBatchFileContent.Add( String.Format( "set >\"{0}\"", EnvOutputFileName ) );

					ResponseFile.Create( EnvReaderBatchFileName, EnvReaderBatchFileContent );
				}

				Log.TraceVerbose( "Finished creating .bat file.  Environment variables will be written to {0}.", EnvOutputFileName );

				// process needs to be disposed when done
				using(var BatchFileProcess = new Process())
				{
					var StartInfo = BatchFileProcess.StartInfo;
					StartInfo.FileName = EnvReaderBatchFileName;
					StartInfo.CreateNoWindow = true;
					StartInfo.UseShellExecute = false;
					StartInfo.RedirectStandardOutput = true;
					StartInfo.RedirectStandardError = true;
					StartInfo.RedirectStandardInput = true;

					Log.TraceVerbose( "Launching {0} to harvest Visual Studio environment settings...", StartInfo.FileName );

					// Try to launch the process, and produce a friendly error message if it fails.
					try
					{
						// Start the process up and then wait for it to finish
						BatchFileProcess.Start();
						BatchFileProcess.WaitForExit();
					}
					catch(Exception ex)
					{
						throw new BuildException( ex, "Failed to start local process for action (\"{0}\"): {1} {2}", ex.Message, StartInfo.FileName, StartInfo.Arguments );
					}

					Log.TraceVerbose( "Finished launching {0}.", StartInfo.FileName );
				}

				// Load environment variables
				var EnvStringsFromFile = File.ReadAllLines( EnvOutputFileName );
				foreach( var EnvString in EnvStringsFromFile )
				{
					// Parse the environment variable name and value from the string ("name=value")
					int EqualSignPos = EnvString.IndexOf( '=' );
					var EnvironmentVariableName = EnvString.Substring( 0, EqualSignPos );
					var EnvironmentVariableValue = EnvString.Substring( EqualSignPos + 1 );

					Log.TraceVerbose("Setting environment variable: {0}={1}", EnvironmentVariableName, EnvironmentVariableValue);

					// Set the environment variable
					Environment.SetEnvironmentVariable( EnvironmentVariableName, EnvironmentVariableValue );
				}

				// Clean up the temporary files we created earlier on, so the temp directory doesn't fill up
				// with these guys over time
				try
				{
					File.Delete( EnvOutputFileName );
				}
				catch( Exception )
				{
					// Unable to delete the temporary file.  Not a big deal.
					Log.TraceInformation( "Warning: Was not able to delete temporary file created by Unreal Build Tool: " + EnvOutputFileName );
				}
				try
				{
					File.Delete( EnvReaderBatchFileName );
				}
				catch( Exception )
				{
					// Unable to delete the temporary file.  Not a big deal.
					Log.TraceInformation( "Warning: Was not able to delete temporary file created by Unreal Build Tool: " + EnvReaderBatchFileName );
				}
			}
			else
			{
				throw new BuildException("SetEnvironmentVariablesFromBatchFile: BatchFile {0} does not exist!", BatchFileName);
			}
		}


		/**
		/* Try to launch a local process, and produce a friendly error message if it fails.
		/*/
		public static void RunLocalProcess(Process LocalProcess)
		{
			// release all process resources
			using(LocalProcess)
			{
				LocalProcess.StartInfo.UseShellExecute = false;
				LocalProcess.StartInfo.RedirectStandardOutput = true;
				LocalProcess.StartInfo.RedirectStandardError = true;

				try
				{
					// Start the process up and then wait for it to finish
					LocalProcess.Start();
					LocalProcess.BeginOutputReadLine();
					LocalProcess.BeginErrorReadLine();
					LocalProcess.WaitForExit();
				}
				catch(Exception ex)
				{
					throw new BuildException(ex, "Failed to start local process for action (\"{0}\"): {1} {2}", ex.Message, LocalProcess.StartInfo.FileName, LocalProcess.StartInfo.Arguments);
				}
			}
		}


		/// <summary>
		/// Given a list of supported platforms, returns a list of names of platforms that should not be supported
		/// </summary>
		/// <param name="SupportedPlatforms">List of supported platforms</param>
		/// <returns>List of unsupported platforms in string format</returns>
		public static List<string> MakeListOfUnsupportedPlatforms( List<UnrealTargetPlatform> SupportedPlatforms )
		{
			// Make a list of all platform name strings that we're *not* currently compiling, to speed
			// up file path comparisons later on
			var OtherPlatformNameStrings = new List<string>();
			{
				// look at each group to see if any supported platforms are in it
				List<UnrealPlatformGroup> SupportedGroups = new List<UnrealPlatformGroup>();
				foreach (UnrealPlatformGroup Group in Enum.GetValues(typeof(UnrealPlatformGroup)))
				{
					// get the list of platforms registered to this group, if any
					List<UnrealTargetPlatform> Platforms = UEBuildPlatform.GetPlatformsInGroup(Group);
					if (Platforms != null)
					{
						// loop over each one
						foreach (UnrealTargetPlatform Platform in Platforms)
						{
							// if it's a compiled platform, then add this group to be supported
							if (SupportedPlatforms.Contains(Platform))
							{
								SupportedGroups.Add(Group);
							}
						}
					}
				}

				// loop over groups one more time, anything NOT in SupportedGroups is now unsuppored, and should be added to the output list
				foreach (UnrealPlatformGroup Group in Enum.GetValues(typeof(UnrealPlatformGroup)))
				{
					if (SupportedGroups.Contains(Group) == false)
					{
						OtherPlatformNameStrings.Add(Group.ToString());
					}
				}

				foreach( UnrealTargetPlatform CurPlatform in Enum.GetValues( typeof( UnrealTargetPlatform ) ) )
				{
					if (CurPlatform != UnrealTargetPlatform.Unknown)
					{
                        bool ShouldConsider = true;

                        // If we have a platform and a group with the same name, don't add the platform
                        // to the other list if the same-named group is supported.  This is a lot of
                        // lines because we need to do the comparisons as strings.
                        string CurPlatformString = CurPlatform.ToString();
                        foreach (UnrealPlatformGroup Group in Enum.GetValues(typeof(UnrealPlatformGroup)))
                        {
                            if (Group.ToString().Equals(CurPlatformString))
                            {
                                ShouldConsider = false;
                                break;
                            }
                        }

						// Don't add our current platform to the list of platform sub-directory names that
						// we'll skip source files for
						if ( ShouldConsider && !SupportedPlatforms.Contains( CurPlatform ) )
						{
							OtherPlatformNameStrings.Add(CurPlatform.ToString());
						}
					}
				}

				return OtherPlatformNameStrings;
			}
		}


		/// <summary>
		/// Takes a path string and makes all of the path separator characters consistent. Also removes unnecessary multiple separators.
		/// </summary>
		/// <param name="FilePath">File path with potentially inconsistent slashes</param>
		/// <returns>File path with consistent separators</returns>
		public static string CleanDirectorySeparators( string FilePath, char UseDirectorySeparatorChar = '\0' )
		{
			StringBuilder CleanPath = new StringBuilder(FilePath.Length);
			if (UseDirectorySeparatorChar == '\0')
			{
				UseDirectorySeparatorChar = Environment.OSVersion.Platform == PlatformID.Unix ? '/' : '\\';
			}
			char PrevC = '\0';
			// Don't check for double separators until we run across a valid dir name. Paths that start with '//' or '\\' can still be valid.			
			bool bCanCheckDoubleSeparators = false;
			for (int Index = 0; Index < FilePath.Length; ++Index)
			{
				char C = FilePath[Index];				
				if (C == '/' || C == '\\')
				{
					C = UseDirectorySeparatorChar;
					if (bCanCheckDoubleSeparators && C == PrevC)
					{
						continue;
					}
				}
				else
				{
					// First non-separator character, safe to check double separators
					bCanCheckDoubleSeparators = true;
				}
				CleanPath.Append(C);
				PrevC = C;
			}
			return CleanPath.ToString();
		}

	
		/// <summary>
		/// Given a file path and a directory, returns a file path that is relative to the specified directory
		/// </summary>
		/// <param name="SourcePath">File path to convert</param>
		/// <param name="RelativeToDirectory">The directory that the source file path should be converted to be relative to.  If this path is not rooted, it will be assumed to be relative to the current working directory.</param>
		/// <param name="AlwaysTreatSourceAsDirectory">True if we should treat the source path like a directory even if it doesn't end with a path separator</param>
		/// <returns>Converted relative path</returns>
		public static string MakePathRelativeTo( string SourcePath, string RelativeToDirectory, bool AlwaysTreatSourceAsDirectory = false )
		{
			if( String.IsNullOrEmpty( RelativeToDirectory ) )
			{
				// Assume CWD
				RelativeToDirectory = ".";
			}

			var AbsolutePath = SourcePath;
			if( !Path.IsPathRooted( AbsolutePath ) )
			{
				AbsolutePath = Path.GetFullPath( SourcePath );
			}
			var SourcePathEndsWithDirectorySeparator = AbsolutePath.EndsWith( Path.DirectorySeparatorChar.ToString() ) || AbsolutePath.EndsWith( Path.AltDirectorySeparatorChar.ToString() );
			if( AlwaysTreatSourceAsDirectory && !SourcePathEndsWithDirectorySeparator )
			{
				AbsolutePath += Path.DirectorySeparatorChar;
			}

			var AbsolutePathUri = new Uri( AbsolutePath );

			var AbsoluteRelativeDirectory = RelativeToDirectory;
			if( !Path.IsPathRooted( AbsoluteRelativeDirectory ) )
			{
				AbsoluteRelativeDirectory = Path.GetFullPath( AbsoluteRelativeDirectory );
			}

			// Make sure the directory has a trailing directory separator so that the relative directory that
			// MakeRelativeUri creates doesn't include our directory -- only the directories beneath it!
			if( !AbsoluteRelativeDirectory.EndsWith( Path.DirectorySeparatorChar.ToString() ) && !AbsoluteRelativeDirectory.EndsWith( Path.AltDirectorySeparatorChar.ToString() ) )
			{
				AbsoluteRelativeDirectory += Path.DirectorySeparatorChar;
			}

			// Convert to URI form which is where we can make the relative conversion happen
			var AbsoluteRelativeDirectoryUri = new Uri( AbsoluteRelativeDirectory );

			// Ask the URI system to convert to a nicely formed relative path, then convert it back to a regular path string
			var UriRelativePath = AbsoluteRelativeDirectoryUri.MakeRelativeUri( AbsolutePathUri );
			var RelativePath = Uri.UnescapeDataString( UriRelativePath.ToString() ).Replace( '/', Path.DirectorySeparatorChar );

			// If we added a directory separator character earlier on, remove it now
			if( !SourcePathEndsWithDirectorySeparator && AlwaysTreatSourceAsDirectory && RelativePath.EndsWith( Path.DirectorySeparatorChar.ToString() ) )
			{
				RelativePath = RelativePath.Substring( 0, RelativePath.Length - 1 );
			}

			// Uri.MakeRelativeUri is broken in Mono 2.x and sometimes returns broken path
			if (IsRunningOnMono)
			{
				// Check if result is correct
				string TestPath = Path.GetFullPath(Path.Combine(AbsoluteRelativeDirectory, RelativePath));
				if (TestPath != AbsolutePath)
				{
					TestPath += "/";
					if (TestPath != AbsolutePath)
					{
						// Fix the path. @todo Mac: replace this hack with something better
						RelativePath = "../" + RelativePath;
					}
				}
			}

			return RelativePath;
		}


		/// <summary>
		/// Backspaces the specified number of characters, then displays a progress percentage value to the console
		/// </summary>
		/// <param name="Numerator">Progress numerator</param>
		/// <param name="Denominator">Progress denominator</param>
		/// <param name="NumCharsToBackspaceOver">Number of characters to backspace before writing the text.  This value will be updated with the length of the new progress string.  The first time progress is displayed, you should pass 0 for this value.</param>
		public static void DisplayProgress( int Numerator, int Denominator, ref int NumCharsToBackspaceOver )
		{
			// Backspace over previous progress value
			while( NumCharsToBackspaceOver-- > 0 )
			{
				Console.Write( "\b" );
			}

			// Display updated progress string and keep track of how long it was
			float ProgressValue = Denominator > 0 ? ( (float)Numerator / (float)Denominator ) : 1.0f;
			var ProgressString = String.Format( "{0}%", Math.Round( ProgressValue * 100.0f ) );
			NumCharsToBackspaceOver = ProgressString.Length;
			Console.Write( ProgressString );
		}


		/*
		 * Read and write classes with xml specifiers
		 */
		static private void UnknownAttributeDelegate( object sender, XmlAttributeEventArgs e )
		{
		}

		static private void UnknownNodeDelegate( object sender, XmlNodeEventArgs e )
		{
		}

		static public T ReadClass<T>( string FileName ) where T : new()
		{
			T Instance = new T();
			StreamReader XmlStream = null;
			try
			{
				// Get the XML data stream to read from
				XmlStream = new StreamReader( FileName );

				// Creates an instance of the XmlSerializer class so we can read the settings object
				XmlSerializer Serialiser = new XmlSerializer( typeof( T ) );
				// Add our callbacks for unknown nodes and attributes
				Serialiser.UnknownNode += new XmlNodeEventHandler( UnknownNodeDelegate );
				Serialiser.UnknownAttribute += new XmlAttributeEventHandler( UnknownAttributeDelegate );

				// Create an object graph from the XML data
				Instance = ( T )Serialiser.Deserialize( XmlStream );
			}
			catch( Exception E )
			{
				Log.TraceInformation( E.Message );
			}
			finally
			{
				if( XmlStream != null )
				{
					// Done with the file so close it
					XmlStream.Close();
				}
			}

			return Instance;
		}

		static public bool WriteClass<T>( T Data, string FileName, string DefaultNameSpace )
		{
			bool bSuccess = true;
			StreamWriter XmlStream = null;
			try
			{
				FileInfo Info = new FileInfo( FileName );
				if( Info.Exists )
				{
					Info.IsReadOnly = false;
				}

				// Make sure the output directory exists
				Directory.CreateDirectory(Path.GetDirectoryName(FileName));

				XmlSerializerNamespaces EmptyNameSpace = new XmlSerializerNamespaces();
				EmptyNameSpace.Add( "", DefaultNameSpace );

				XmlStream = new StreamWriter( FileName, false, Encoding.Unicode );
				XmlSerializer Serialiser = new XmlSerializer( typeof( T ) );

				// Add our callbacks for unknown nodes and attributes
				Serialiser.UnknownNode += new XmlNodeEventHandler( UnknownNodeDelegate );
				Serialiser.UnknownAttribute += new XmlAttributeEventHandler( UnknownAttributeDelegate );

				Serialiser.Serialize( XmlStream, Data, EmptyNameSpace );
			}
			catch( Exception E )
			{
				Log.TraceInformation( E.Message );
				bSuccess = false;
			}
			finally
			{
				if( XmlStream != null )
				{
					// Done with the file so close it
					XmlStream.Close();
				}
			}

			return ( bSuccess );
		}

		/// <summary>
		/// Returns true if the specified Process has been created, started and remains valid (i.e. running).
		/// </summary>
		/// <param name="p">Process object to test</param>
		/// <returns>True if valid, false otherwise.</returns>
		public static bool IsValidProcess(Process p)
		{
			// null objects are always invalid
			if(p == null)
				return false;
			// due to multithreading on Windows, lock the object
			lock(p)
			{
				// Mono has a specific requirement if testing for an alive process
				if(IsRunningOnMono)
					return p.Handle != IntPtr.Zero; // native handle to the process
				// on Windows, simply test the process ID to be non-zero. 
				// note that this can fail and have a race condition in threads, but the framework throws an exception when this occurs.
				try
				{
					return p.Id != 0;
				}
				catch { } // all exceptions can be safely caught and ignored, meaning the process is not started or has stopped.
			}
			return false;
		}

		/// <summary>
		/// Removes multi-dot extensions from a filename (i.e. *.automation.csproj)
		/// </summary>
		/// <param name="Filename">Filename to remove the extensions from</param>
		/// <returns>Clean filename.</returns>
		public static string GetFilenameWithoutAnyExtensions(string Filename)
		{
			Filename = Path.GetFileName(Filename);

			var DotIndex = Filename.IndexOf('.');
			if (DotIndex == -1)
			{
				return Filename; // No need to copy string
			}
			else
			{
				return Filename.Substring(0, DotIndex);
			}
		}

		/// <summary>
		/// Returns Filename with path but without extension.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <returns>Path to the file with its extension removed.</returns>
		public static string GetPathWithoutExtension(string Filename)
		{
			if (!String.IsNullOrEmpty(Path.GetExtension(Filename)))
			{
				return Path.Combine(Path.GetDirectoryName(Filename), Path.GetFileNameWithoutExtension(Filename));
			}
			else
			{
				return Filename;
			}
		}


		/// <summary>
		/// Returns true if the specified file's path is located under the specified directory, or any of that directory's sub-folders.  Does not care whether the file or directory exist or not.  This is a simple string-based check.
		/// </summary>
		/// <param name="FilePath">The path to the file</param>
		/// <param name="Directory">The directory to check to see if the file is located under (or any of this directory's subfolders)</param>
		/// <returns></returns>
		public static bool IsFileUnderDirectory( string FilePath, string Directory )
		{
			var DirectoryPathPlusSeparator = Path.GetFullPath( Directory );
			if( !DirectoryPathPlusSeparator.EndsWith( Path.DirectorySeparatorChar.ToString() ) )
			{
				DirectoryPathPlusSeparator += Path.DirectorySeparatorChar;
			}
			return Path.GetFullPath( FilePath ).StartsWith( DirectoryPathPlusSeparator, StringComparison.InvariantCultureIgnoreCase );
		}


		/// <summary>
		/// Given a path to a file, strips off the base directory part of the path
		/// </summary>
		/// <param name="FilePath">The full path</param>
		/// <param name="BaseDirectory">The base directory, which must be the first part of the path</param>
		/// <returns>The part of the path after the base directory</returns>
		public static string StripBaseDirectory( string FilePath, string BaseDirectory )
		{
			return CleanDirectorySeparators( FilePath ).Substring( CleanDirectorySeparators( BaseDirectory ).Length + 1 );
		}


		/// <summary>
		/// Given a path to a "source" file, re-roots the file path to be located under the "destination" folder.  The part of the source file's path after the root folder is unchanged.
		/// </summary>
		/// <param name="FilePath"></param>
		/// <param name="BaseDirectory"></param>
		/// <param name="NewBaseDirectory"></param>
		/// <returns></returns>
		public static string MakeRerootedFilePath( string FilePath, string BaseDirectory, string NewBaseDirectory )
		{
			var RelativeFile = StripBaseDirectory( FilePath, BaseDirectory );
			var DestFile = Path.Combine( NewBaseDirectory, RelativeFile );
			return DestFile;
		}


		/// <summary>
		/// Correctly collapses any ../ or ./ entries in a path.
		/// </summary>
		/// <param name="InPath">The path to be collapsed</param>
		/// <returns>true if the path could be collapsed, false otherwise.</returns>
		public static bool CollapseRelativeDirectories(ref string InPath)
		{
			string ParentDir       = "/..";
			int    ParentDirLength = ParentDir.Length;

			for (;;)
			{
				// An empty path is finished
				if (string.IsNullOrEmpty(InPath))
					break;

				// Consider empty paths or paths which start with .. or /.. as invalid
				if (InPath.StartsWith("..") || InPath.StartsWith(ParentDir))
					return false;

				// If there are no "/.."s left then we're done
				int Index = InPath.IndexOf(ParentDir);
				if (Index == -1)
					break;

				int PreviousSeparatorIndex = Index;
				for (;;)
				{
					// Find the previous slash
					PreviousSeparatorIndex = Math.Max(0, InPath.LastIndexOf("/", PreviousSeparatorIndex - 1));

					// Stop if we've hit the start of the string
					if (PreviousSeparatorIndex == 0)
						break;

					// Stop if we've found a directory that isn't "/./"
					if ((Index - PreviousSeparatorIndex) > 1 && (InPath[PreviousSeparatorIndex + 1] != '.' || InPath[PreviousSeparatorIndex + 2] != '/'))
						break;
				}

				// If we're attempting to remove the drive letter, that's illegal
				int Colon = InPath.IndexOf(":", PreviousSeparatorIndex);
				if (Colon >= 0 && Colon < Index)
					return false;

				InPath = InPath.Substring(0, PreviousSeparatorIndex) + InPath.Substring(Index + ParentDirLength);
			}

			InPath = InPath.Replace("./", "");

			return true;
		}


		/// <summary>
		/// Finds the Engine Version from ObjVersion.cpp.
		/// </summary>
		/// <remarks>
		/// If UBT eventually gets the engine version passed from the build environment this code should be scrapped for that instead.
		/// </remarks>
		/// <returns></returns>
		public static int GetEngineVersionFromObjVersionCPP()
		{
			if(UnrealBuildTool.RunningRocket() == false)
			{
				try
				{
					return
						(from line in File.ReadLines("Runtime/Core/Private/UObject/ObjectVersion.cpp", Encoding.ASCII)
						 where line.StartsWith("#define	ENGINE_VERSION")
						 select int.Parse(line.Split()[2])).Single();
				}
				catch (Exception ex)
				{
					// Don't do a stack trace so we don't pollute the logs with spurious exception data, as we don't crash on this case.
					Log.TraceWarning("Could not parse Engine Version from ObjectVersion.cpp: {0}", ex.Message);
				}
			}
			return 0;
		}

		/// <summary>
		/// Gets the executing assembly path (including filename).
		/// This method is using Assembly.CodeBase property to properly resolve original
		/// assembly path in case shadow copying is enabled.
		/// </summary>
		/// <returns>Absolute path to the executing assembly including the assembly filename.</returns>
		internal static string GetExecutingAssemblyLocation()
		{
			return new Uri(System.Reflection.Assembly.GetExecutingAssembly().CodeBase).LocalPath;
		}

		/// <summary>
		/// Gets the executing assembly directory.
		/// This method is using Assembly.CodeBase property to properly resolve original
		/// assembly directory in case shadow copying is enabled.
		/// </summary>
		/// <returns>Absolute path to the directory containing the executing assembly.</returns>
		internal static string GetExecutingAssemblyDirectory()
		{
			return Path.GetDirectoryName(GetExecutingAssemblyLocation());
		}
	}

	/// <summary>
	/// Class to display an incrementing progress percentage. Handles progress markup and direct console output.
	/// </summary>
	public class ProgressWriter : IDisposable
	{
		public static bool bWriteMarkup = false;

		bool bWriteToConsole;
		string Message;
		int NumCharsToBackspaceOver;

		public ProgressWriter(string InMessage, bool bInWriteToConsole)
		{
			Message = InMessage;
			bWriteToConsole = bInWriteToConsole;
			if (!bWriteMarkup && bWriteToConsole)
			{
				Console.Write(Message + " ");
			}
			Write(0, 100);
		}

		public void Dispose()
		{
			if (!bWriteMarkup && bWriteToConsole)
			{
				Console.WriteLine();
			}
		}

		public void Write(int Numerator, int Denominator)
		{
			float ProgressValue = Denominator > 0 ? ((float)Numerator / (float)Denominator) : 1.0f;
			string ProgressString = String.Format("{0}%", Math.Round(ProgressValue * 100.0f));

			if (bWriteMarkup)
			{
				Log.WriteLine(TraceEventType.Information, "@progress '{0}' {1}", Message, ProgressString);
			}
			else if (bWriteToConsole)
			{
				// Backspace over previous progress value
				while (NumCharsToBackspaceOver-- > 0)
				{
					Console.Write("\b");
				}

				// Display updated progress string and keep track of how long it was
				NumCharsToBackspaceOver = ProgressString.Length;
				Console.Write(ProgressString);
			}
		}
	}

	/// <summary>
	/// Verbosity filter
	/// </summary>
	class VerbosityFilter : TraceFilter
	{
		public override bool ShouldTrace(TraceEventCache Cache, string Source, TraceEventType EventType, int Id, string FormatOrMessage, object[] Args, object Data1, object[] Data)
		{
			return EventType < TraceEventType.Verbose || BuildConfiguration.bPrintDebugInfo;
		}
	}

	/// <summary>
	/// Console Trace listener for UBT
	/// </summary>
	class ConsoleListener : TextWriterTraceListener
	{
		public override void Write(string Message)
		{
			Console.Write(Message);
		}

		public override void WriteLine(string Message)
		{
			Console.WriteLine(Message);
		}

		public override void TraceEvent(TraceEventCache EventCache, string Source, TraceEventType EventType, int Id, string Message)
		{
			if (Filter == null || Filter.ShouldTrace(EventCache, Source, EventType, Id, Message, null, null, null))
			{
				WriteLine(Message);
			}
		}

		public override void TraceEvent(TraceEventCache EventCache, string Source, TraceEventType EventType, int Id, string Format, params object[] Args)
		{
			if (Filter == null || Filter.ShouldTrace(EventCache, Source, EventType, Id, Format, Args, null, null))
			{
				WriteLine(String.Format(Format, Args));
			}
		}
	}

	/// <summary>
	/// UnrealBuiltTool console logging system.
	/// </summary>
	public sealed class Log
	{
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		private static string GetSource(int StackFramesToSkip)
		{
			StackFrame Frame = new StackFrame(2 + StackFramesToSkip);
			System.Reflection.MethodBase Method = Frame.GetMethod();
			return String.Format("{0}.{1}", Method.DeclaringType.Name, Method.Name);
		}

		/// <summary>
		/// Writes a formatted message to the console.
		/// </summary>
		/// <param name="Verbosity">Message verbosity level.</param>
		/// <param name="Format">Message format string.</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(int StackFramesToSkip, TraceEventType Verbosity, string Format, params object[] Args)
		{
			var Source = GetSource(StackFramesToSkip);
			var EventCache = new TraceEventCache();
			foreach (TraceListener Listener in Trace.Listeners)
			{
				Listener.TraceEvent(EventCache, Source, Verbosity, (int)Verbosity, Format, Args);
			}
		}

		/// <summary>
		/// Writes a message to the console.
		/// </summary>
		/// <param name="Verbosity">Message verbosity level.</param>
		/// <param name="Message">Message text.</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(int StackFramesToSkip, TraceEventType Verbosity, string Message)
		{
			var Source = GetSource(StackFramesToSkip);
			var EventCache = new TraceEventCache();
			foreach (TraceListener Listener in Trace.Listeners)
			{
				Listener.TraceEvent(EventCache, Source, Verbosity, (int)Verbosity, Message);
			}
		}

		/// <summary>
		/// Writes a formatted message to the console.
		/// </summary>
		/// <param name="Verbosity">Message verbosity level.</param>
		/// <param name="Format">Message format string.</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(TraceEventType Verbosity, string Format, params object[] Args)
		{
			var Source = GetSource(0);
			var EventCache = new TraceEventCache();
			foreach (TraceListener Listener in Trace.Listeners)
			{
				Listener.TraceEvent(EventCache, Source, Verbosity, (int)Verbosity, Format, Args);
			}
		}

		/// <summary>
		/// Writes a message to the console.
		/// </summary>
		/// <param name="Verbosity">Message verbosity level.</param>
		/// <param name="Message">Message text.</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(TraceEventType Verbosity, string Message)
		{
			var Source = GetSource(0);
			var EventCache = new TraceEventCache();
			foreach (TraceListener Listener in Trace.Listeners)
			{
				Listener.TraceEvent(EventCache, Source, Verbosity, (int)Verbosity, Message);
			}
		}

		/// <summary>
		/// Writes a formatted message to the console if the condition is met.
		/// </summary>
		/// <param name="Condition">Condition</param>
		/// <param name="Verbosity">Message verbosity level</param>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLineIf(bool Condition, TraceEventType Verbosity, string Format, params object[] Args)
		{
			if (Condition)
			{
				WriteLine(1, Verbosity, Format, Args);
			}
		}

		/// <summary>
		/// Writes a message to the console if the condition is met.
		/// </summary>
		/// <param name="Condition">Condition.</param>
		/// <param name="Verbosity">Message verbosity level</param>
		/// <param name="Message">Message text</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLineIf(bool Condition, TraceEventType Verbosity, string Message)
		{
			if (Condition)
			{
				WriteLine(1, Verbosity, Message);
			}
		}

		/// <summary>
		/// Writes a verbose message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVerbose(string Format, params object[] Args)
		{
			WriteLine(1, TraceEventType.Verbose, Format, Args);
		}

		/// <summary>
		/// Writes a verbose message to the console.
		/// </summary>
		/// <param name="Message">Message text</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVerbose(string Message)
		{
			WriteLine(1, TraceEventType.Verbose, Message);
		}

		/// <summary>
		/// Writes a message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceInformation(string Format, params object[] Args)
		{
			WriteLine(1, TraceEventType.Information, Format, Args);
		}

		/// <summary>
		/// Writes a message to the console.
		/// </summary>
		/// <param name="Message">Message text</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceInformation(string Message)
		{
			WriteLine(1, TraceEventType.Information, Message);
		}

		/// <summary>
		/// Writes a warning message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceWarning(string Format, params object[] Args)
		{
			WriteLine(1, TraceEventType.Warning, Format, Args);
		}

		/// <summary>
		/// Writes a warning message to the console.
		/// </summary>
		/// <param name="Message">Message text</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceWarning(string Message)
		{
			WriteLine(1, TraceEventType.Warning, Message);
		}

		/// <summary>
		/// Writes an error message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceError(string Format, params object[] Args)
		{
			WriteLine(1, TraceEventType.Error, Format, Args);
		}

		/// <summary>
		/// Writes an error message to the console.
		/// </summary>
		/// <param name="Message">Message text</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceError(string Message)
		{
			WriteLine(1, TraceEventType.Error, Message);
		}
	}

    #region StreamUtils
    public static class StreamUtils
    {
        /// <summary>
        /// Read a stream into another, buffering in 4K chunks.
        /// </summary>
        /// <param name="output">this</param>
        /// <param name="input">the Stream to read from</param>
        /// <returns>same stream for expression chaining.</returns>
        public static Stream ReadFrom(this Stream output, Stream input)
        {
            long bytesRead;
            return output.ReadFrom(input, out bytesRead);
        }

        /// <summary>
        /// Read a stream into another, buffering in 4K chunks.
        /// </summary>
        /// <param name="output">this</param>
        /// <param name="input">the Stream to read from</param>
        /// <param name="totalBytesRead">returns bytes read</param>
        /// <returns>same stream for expression chaining.</returns>
        public static Stream ReadFrom(this Stream output, Stream input, out long totalBytesRead)
        {
            totalBytesRead = 0;
            const int BytesToRead = 4096;
            var buf = new byte[BytesToRead];
            var bytesReadThisTime = 0;
            do
            {
                bytesReadThisTime = input.Read(buf, 0, BytesToRead);
                totalBytesRead += bytesReadThisTime;
                output.Write(buf, 0, bytesReadThisTime);
            } while (bytesReadThisTime != 0);
            return output;
        }

        /// <summary>
        /// Read stream into a new MemoryStream. Useful for chaining together expressions.
        /// </summary>
        /// <param name="input">Stream to read from.</param>
        /// <returns>memory stream that contains the stream contents.</returns>
        public static MemoryStream ReadIntoMemoryStream(this Stream input)
        {
            var data = new MemoryStream(4096);
            data.ReadFrom(input);
            return data;
        }

        /// <summary>
        /// Writes the entire contents of a byte array to the stream.
        /// </summary>
        /// <param name="stream"></param>
        /// <param name="arr"></param>
        /// <returns></returns>
        public static Stream Write(this Stream stream, byte[] arr)
        {
            stream.Write(arr, 0, arr.Length);
            return stream;
        }
    }
    #endregion
}
