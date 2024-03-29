// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;

namespace UnrealBuildTool
{
	/// <summary>
	/// List of all files included in a file and helper class for handling circular dependencies.
	/// </summary>
	public class IncludedFilesSet : HashSet<FileItem>
	{
		/** Whether this file list has been fully initialized or not. */
		public bool bIsInitialized;

		/** List of files which include this file in one of its includes. */
		public List<FileItem> CircularDependencies = new List<FileItem>();
	}

	public partial class CPPEnvironment
	{
		/** Contains a cache of include dependencies (direct and indirect). */
		public static DependencyCache IncludeDependencyCache = null;

		/** Contains a mapping from filename to the full path of the header in this environment. */
		public Dictionary<string, FileItem> IncludeFileSearchDictionary = new Dictionary<string, FileItem>();

		/** Absolute path to external folder, upper invariant. */
		private static string AbsoluteExternalPathUpperInvariant = Path.GetFullPath( "." + Path.DirectorySeparatorChar + "ThirdParty" + Path.DirectorySeparatorChar ).ToUpperInvariant();

		public static int TotalFindIncludedFileCalls = 0;
		public static int IncludePathSearchAttempts = 0;


		/** 
		 * Finds the header file that is referred to by a partial include filename. 
		 * @param RelativeIncludePath path relative to the project
		 * @param bSkipExternalHeader true to skip processing of headers in external path
		 * @param SourceFilesDirectory - The folder containing the source files we're generating a PCH for
		 */
		public FileItem FindIncludedFile( string RelativeIncludePath, bool bSkipExternalHeader, String SourceFilesDirectory )
		{
			FileItem Result = null;

			++TotalFindIncludedFileCalls;

			// Only search for the include file if the result hasn't been cached.
			string InvariantPath = RelativeIncludePath.ToLowerInvariant();
			if( !IncludeFileSearchDictionary.TryGetValue( InvariantPath, out Result ) )
			{
				int SearchAttempts = 0;
				if( Path.IsPathRooted( RelativeIncludePath ) )
				{
					if( DirectoryLookupCache.FileExists( RelativeIncludePath ) )
					{
						Result = FileItem.GetItemByFullPath( RelativeIncludePath );
					}
					++SearchAttempts;
				}
				else
				{
					// Build a single list of include paths to search.
					var IncludePathsToSearch = new List<string>();
					if( !String.IsNullOrEmpty( SourceFilesDirectory ) )
					{
						IncludePathsToSearch.Add( SourceFilesDirectory );
					}
					IncludePathsToSearch.AddRange( Config.IncludePaths );
					if( BuildConfiguration.bCheckSystemHeadersForModification )
					{
						IncludePathsToSearch.AddRange( Config.SystemIncludePaths );
					}

					// Find the first include path that the included file exists in.
					foreach( string IncludePath in IncludePathsToSearch )
					{
						++SearchAttempts;
						string RelativeFilePath = "";
						try
						{
							RelativeFilePath = Path.Combine(IncludePath, RelativeIncludePath);
						}
						catch (ArgumentException Exception)
						{
							throw new BuildException(Exception, "Failed to combine null or invalid include paths.");
						}
						string FullFilePath = Path.GetFullPath( RelativeFilePath );
						if( DirectoryLookupCache.FileExists( FullFilePath ) )
						{
							Result = FileItem.GetItemByFullPath( FullFilePath );
							break;
						}
					}
				}

				IncludePathSearchAttempts += SearchAttempts;

				if( BuildConfiguration.bPrintPerformanceInfo )
				{
					// More than two search attempts indicates:
					//		- Include path was not relative to the directory that the including file was in
					//		- Include path was not relative to the project's base
					if( SearchAttempts > 2 )
					{
						Trace.TraceInformation( "   Cache miss: " + RelativeIncludePath + " found after " + SearchAttempts.ToString() + " attempts: " + ( Result != null ? Result.AbsolutePath : "NOT FOUND!" ) );
					}
				}

				// Cache the result of the include path search.
				IncludeFileSearchDictionary.Add( InvariantPath, Result );
			}

			// Check whether the header should be skipped. We need to do this after resolving as we need 
			// the absolute path to compare against the External folder.
			bool bWasHeaderSkipped = false;
			if( Result != null && bSkipExternalHeader )
			{
				// Check whether header path is under External root.
				if( Result.AbsolutePath.StartsWith( AbsoluteExternalPathUpperInvariant, StringComparison.InvariantCultureIgnoreCase ) )
				{
					// It is, skip and reset result.
					Result = null;
					bWasHeaderSkipped = true;
				}
			}

			// Log status for header.
			if( Result != null )
			{
				Log.TraceVerbose("Resolved included file \"{0}\" to: {1}", RelativeIncludePath, Result.AbsolutePath);
			}
			else if( bWasHeaderSkipped )
			{
				Log.TraceVerbose("Skipped included file \"{0}\"", RelativeIncludePath);
			}
			else
			{
				Log.TraceVerbose("Couldn't resolve included file \"{0}\"", RelativeIncludePath);
			}

			return Result;
		}

		/** A cache of the list of other files that are directly or indirectly included by a C++ file. */
		static Dictionary<FileItem, IncludedFilesSet> IncludedFilesMap = new Dictionary<FileItem, IncludedFilesSet>();

		public static void Reset()
		{
			IncludedFilesMap = new Dictionary<FileItem, IncludedFilesSet>();
		}

		/// <summary>
		/// Finds the files directly or indirectly included by the given C++ file.
		/// </summary>
		/// <param name="CPPFile">C++ file to get the dependencies for.</param>
		/// <param name="Result">List of CPPFile dependencies.</param>
		/// <returns>false if CPPFile is still being processed further down the callstack, true otherwise.</returns>
		private bool GetIncludeDependencies(FileItem CPPFile, ref IncludedFilesSet Result)
		{
			IncludedFilesSet IncludedFileList;
			if( !IncludedFilesMap.TryGetValue( CPPFile, out IncludedFileList ) )
			{
				IncludedFileList = new IncludedFilesSet();

				// Add an uninitialized entry for the include file to avoid infinitely recursing on include file loops.
				IncludedFilesMap.Add(CPPFile, IncludedFileList);

				// Gather a list of names of files directly included by this C++ file.
				List<DependencyInclude> DirectIncludes = GetDirectIncludeDependencies(CPPFile, Config.TargetPlatform);

				// Build a list of the unique set of files that are included by this file.
				string SourceFilesDirectory = Path.GetDirectoryName( CPPFile.AbsolutePath );
				var DirectlyIncludedFiles = new HashSet<FileItem>();
				// require a for loop here because we need to keep track of the index in the list.
				for (int DirectlyIncludedFileNameIndex = 0; DirectlyIncludedFileNameIndex < DirectIncludes.Count; ++DirectlyIncludedFileNameIndex)
				{
					// Resolve the included file name to an actual file.
					DependencyInclude DirectInclude = DirectIncludes[DirectlyIncludedFileNameIndex];
					if (DirectInclude.IncludeResolvedName == null || 
						// ignore any preexisting resolve cache if we are not configured to use it.
						!BuildConfiguration.bUseIncludeDependencyResolveCache ||
						// if we are testing the resolve cache, we force UBT to resolve every time to look for conflicts
						BuildConfiguration.bTestIncludeDependencyResolveCache
						)
					{
						++TotalDirectIncludeResolveCacheMisses;
						try
						{
							// search the include paths to resolve the file
							FileItem DirectIncludeResolvedFile = FindIncludedFile(DirectInclude.IncludeName, !BuildConfiguration.bCheckExternalHeadersForModification, SourceFilesDirectory);
							if (DirectIncludeResolvedFile != null)
							{
								DirectlyIncludedFiles.Add(DirectIncludeResolvedFile);
							}
							IncludeDependencyCache.CacheResolvedIncludeFullPath(CPPFile, DirectlyIncludedFileNameIndex, DirectIncludeResolvedFile != null ? DirectIncludeResolvedFile.AbsolutePath : "");
						}
						catch (System.Exception ex)
						{
							string BuildExceptionMessage = "GetIncludeDependencies: Failed to FindIncludedFile \n\t\"";
							BuildExceptionMessage += DirectInclude.IncludeName;
							BuildExceptionMessage += "\"\n while processing \n\t\"";
							BuildExceptionMessage += CPPFile.AbsolutePath;
							BuildExceptionMessage += "\"\n";
							BuildExceptionMessage += "Exception:\n";
							BuildExceptionMessage += ex.ToString();
							BuildExceptionMessage += "\n";
							throw new BuildException(ex, BuildExceptionMessage);
						}
					}
					else
					{
						// we might have cached an attempt to resolve the file, but couldn't actually find the file (system headers, etc).
						if (DirectInclude.IncludeResolvedName != string.Empty)
						{
							DirectlyIncludedFiles.Add(FileItem.GetItemByFullPath(DirectInclude.IncludeResolvedName));
						}
					}
				}
				TotalDirectIncludeResolves += DirectIncludes.Count;

				// Convert the dictionary of files included by this file into a list.
				foreach( var DirectlyIncludedFile in DirectlyIncludedFiles )
				{
					// Add the file we're directly including
					IncludedFileList.Add( DirectlyIncludedFile );

					// Also add all of the indirectly included files!
					if (GetIncludeDependencies(DirectlyIncludedFile, ref IncludedFileList) == false)
					{
						// DirectlyIncludedFile is a circular dependency which is still being processed
						// further down the callstack. Add this file to its circular dependencies list 
						// so that it can update its dependencies later.
						IncludedFilesSet DirectlyIncludedFileIncludedFileList;
						if (IncludedFilesMap.TryGetValue(DirectlyIncludedFile, out DirectlyIncludedFileIncludedFileList))
						{
							DirectlyIncludedFileIncludedFileList.CircularDependencies.Add(CPPFile);
						}
					}
				}

				// All dependencies have been processed by now so update all circular dependencies
				// with the full list.
				foreach (var CircularDependency in IncludedFileList.CircularDependencies)
				{
					IncludedFilesSet CircularDependencyIncludedFiles = IncludedFilesMap[CircularDependency];
					foreach (FileItem IncludedFile in IncludedFileList)
					{
						CircularDependencyIncludedFiles.Add(IncludedFile);
					}
				}
				// No need to keep this around anymore.
				IncludedFileList.CircularDependencies.Clear();

				// Done collecting files.
				IncludedFileList.bIsInitialized = true;
			}

			if (IncludedFileList.bIsInitialized)
			{
				// Copy the list of files included by this file into the result list.
				foreach( FileItem IncludedFile in IncludedFileList )
				{
					// If the result list doesn't contain this file yet, add the file and the files it includes.
					// NOTE: For some reason in .NET 4, Add() is over twice as fast as calling UnionWith() on the set
					Result.Add( IncludedFile );
				}
				return true;
			}
			else
			{
				// The IncludedFileList.bIsInitialized was false because we added a dummy entry further down the call stack.  We're already processing
				// the include list for this header elsewhere in the stack frame, so we don't need to add anything here.
				return false;
			}
		}

		public static double TotalTimeSpentGettingIncludes = 0.0;
		public static int TotalIncludesRequested = 0;
		public static double DirectIncludeCacheMissesTotalTime = 0.0;
		public static int TotalDirectIncludeCacheMisses = 0;
		public static int TotalDirectIncludeResolveCacheMisses = 0;
		public static int TotalDirectIncludeResolves = 0;

		/** @return The list of files which are directly or indirectly included by a C++ file. */
		public List<FileItem> GetIncludeDependencies( FileItem CPPFile )
		{
			++TotalIncludesRequested;

			List<FileItem> Result = new List<FileItem>();

			// Don't bother gathering includes when only generating IntelliSense data.  We don't need to actually
			// know about source file dependencies to create the data we need.
			if( !ProjectFileGenerator.bGenerateProjectFiles )
			{
				var TimerStartTime = DateTime.UtcNow;

				// Find the dependencies of the file.
				var IncludedFileDictionary = new IncludedFilesSet();
				GetIncludeDependencies( CPPFile, ref IncludedFileDictionary );

				// Convert the dependency dictionary into a list.
				foreach( var IncludedFile in IncludedFileDictionary )
				{
					Result.Add( IncludedFile );
				}

				var TimerDuration = DateTime.UtcNow - TimerStartTime;
				TotalTimeSpentGettingIncludes += TimerDuration.TotalSeconds;
			}

			return Result;
		}

		/** Regex that matches #include statements. */
		static Regex CPPHeaderRegex = new Regex("(([ \t]*#[ \t]*include[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">][^\n]*\n*)|([^\n]*\n*))*",
													RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture );

		static Regex MMHeaderRegex = new Regex("(([ \t]*#[ \t]*import[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">][^\n]*\n*)|([^\n]*\n*))*",
													RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture );

		/** Finds the names of files directly included by the given C++ file. */
		public static List<DependencyInclude> GetDirectIncludeDependencies( FileItem CPPFile, CPPTargetPlatform TargetPlatform )
		{
			// Try to fulfill request from cache first.
			List<DependencyInclude> Result = null;
			bool HasUObjects;
			if( IncludeDependencyCache.GetDirectDependencies( CPPFile, out Result, out HasUObjects ) )
			{
				CPPFile.HasUObjects = HasUObjects;
				return Result;
			}

			HasUObjects = false;

			var TimerStartTime = DateTime.UtcNow;
			++CPPEnvironment.TotalDirectIncludeCacheMisses;

			// Get the adjusted filename
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform( TargetPlatform );
			string FileToRead = CPPFile.AbsolutePath;

			if (BuildPlatform.RequiresExtraUnityCPPWriter() == true &&
				Path.GetFileName(FileToRead).StartsWith("Module."))
			{
				FileToRead += ".ex";
			}

			// Read lines from the C++ file.
			string FileContents = Utils.ReadAllText( FileToRead );
            if (string.IsNullOrEmpty(FileContents))
            {
                return new List<DependencyInclude>();
            }

			// Note: This depends on UBT executing w/ a working directory of the Engine/Source folder!
			string EngineSourceFolder = Directory.GetCurrentDirectory();
			string InstalledFolder = EngineSourceFolder;
			Int32 EngineSourceIdx = EngineSourceFolder.IndexOf("\\Engine\\Source");
			if (EngineSourceIdx != -1)
			{
				InstalledFolder = EngineSourceFolder.Substring(0, EngineSourceIdx);
			}

			Result = new List<DependencyInclude>();
			if (Utils.IsRunningOnMono)
			{
				// Mono crashes when running a regex on a string longer than about 5000 characters, so we parse the file in chunks
				int StartIndex = 0;
				const int SafeTextLength = 4000;
				while( StartIndex < FileContents.Length )
				{
					int EndIndex = StartIndex + SafeTextLength < FileContents.Length ? FileContents.IndexOf( "\n", StartIndex + SafeTextLength ) : FileContents.Length;
					if( EndIndex == -1 )
					{
						EndIndex = FileContents.Length;
					}

					CollectHeaders(CPPFile, Result, ref HasUObjects, FileToRead, FileContents, InstalledFolder, StartIndex, EndIndex);

					StartIndex = EndIndex + 1;
				}
			}
			else
			{
				CollectHeaders(CPPFile, Result, ref HasUObjects, FileToRead, FileContents, InstalledFolder, 0, FileContents.Length);
			}

			// Store whether this C++ file has UObjects right inside the FileItem for this file
			CPPFile.HasUObjects = HasUObjects;

			// Populate cache with results.
			IncludeDependencyCache.SetDependencyInfo( CPPFile, Result, CPPFile.HasUObjects );

			CPPEnvironment.DirectIncludeCacheMissesTotalTime += ( DateTime.UtcNow - TimerStartTime ).TotalSeconds;

			return Result;
		}

		/// <summary>
		/// Collects all header files included in a CPPFile
		/// </summary>
		/// <param name="CPPFile"></param>
		/// <param name="Result"></param>
		/// <param name="HasUObjects"></param>
		/// <param name="FileToRead"></param>
		/// <param name="FileContents"></param>
		/// <param name="InstalledFolder"></param>
		/// <param name="StartIndex"></param>
		/// <param name="EndIndex"></param>
		private static void CollectHeaders(FileItem CPPFile, List<DependencyInclude> Result, ref bool HasUObjects, string FileToRead, string FileContents, string InstalledFolder, int StartIndex, int EndIndex)
		{
			Match M = CPPHeaderRegex.Match(FileContents, StartIndex, EndIndex - StartIndex);
			CaptureCollection CC = M.Groups["HeaderFile"].Captures;
			Result.Capacity = Math.Max(Result.Count + CC.Count, Result.Capacity);
			foreach (Capture C in CC)
			{
				string HeaderValue = C.Value;

				// Is this include statement including a generated UObject header?  If so, then we know we'll need to pass this file to UnrealHeaderTool for code generation
				if (!HasUObjects && HeaderValue.EndsWith(".generated.h", StringComparison.InvariantCultureIgnoreCase))
				{
					// @todo uht: Ideally we would also check for other clues, like USTRUCT or UCLASS.
					// @todo uht: We need to be careful to skip comments or string text that may have these clues
					HasUObjects = true;
				}

				//@TODO: The intermediate exclusion is to work around autogenerated absolute paths in Module.SomeGame.cpp style files
				bool bIsIntermediateOrThirdParty = FileToRead.Contains("Intermediate") || FileToRead.Contains("ThirdParty");
				bool bCheckForBackwardSlashes = FileToRead.StartsWith(InstalledFolder);
				if (UnrealBuildTool.HasUProjectFile())
				{
					bCheckForBackwardSlashes |= Utils.IsFileUnderDirectory(FileToRead, UnrealBuildTool.GetUProjectPath());
				}
				if (bCheckForBackwardSlashes && !bIsIntermediateOrThirdParty)
				{
					if (HeaderValue.IndexOf('\\', 0) >= 0)
					{
						throw new BuildException("In {0}: #include \"{1}\" contains backslashes ('\\'), please use forward slashes ('/') instead.", FileToRead, C.Value);
					}
				}
				HeaderValue = Utils.CleanDirectorySeparators(HeaderValue);
				Result.Add(new DependencyInclude(HeaderValue));
			}

			// also look for #import in objective C files
			string Ext = Path.GetExtension(CPPFile.AbsolutePath).ToUpperInvariant();
			if (Ext == ".MM" || Ext == ".M")
			{
				M = MMHeaderRegex.Match(FileContents, StartIndex, EndIndex - StartIndex);
				CC = M.Groups["HeaderFile"].Captures;
				Result.Capacity += CC.Count;
				foreach (Capture C in CC)
				{
					Result.Add(new DependencyInclude(C.Value));
				}
			}
		}
	}
}
