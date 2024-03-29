// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using AutomationTool;
using System.Runtime.Serialization;
using System.Net;
using System.Reflection;
using UnrealBuildTool;

namespace EpicGames.MCP.Automation
{
	using EpicGames.MCP.Config;

    /// <summary>
    /// Utility class to provide commit/rollback functionality via an RAII-like functionality.
    /// Usage is to provide a rollback action that will be called on Dispose if the Commit() method is not called.
    /// This is expected to be used from within a using() ... clause.
    /// </summary>
    public class CommitRollbackTransaction : IDisposable
    {
        /// <summary>
        /// Track whether the transaction will be committed.
        /// </summary>
        private bool IsCommitted = false;

        /// <summary>
        /// 
        /// </summary>
        private System.Action RollbackAction;

        /// <summary>
        /// Call when you want to commit your transaction. Ensures the Rollback action is not called on Dispose().
        /// </summary>
        public void Commit()
        {
            IsCommitted = true;
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="RollbackAction">Action to be executed to rollback the transaction.</param>
        public CommitRollbackTransaction(System.Action InRollbackAction)
        {
            RollbackAction = InRollbackAction;
        }

        /// <summary>
        /// Rollback the transaction if its not committed on Dispose.
        /// </summary>
        public void Dispose()
        {
            if (!IsCommitted)
            {
                RollbackAction();
            }
        }
    }

    /// <summary>
    /// Enum that defines the MCP backend-compatible platform
    /// </summary>
    public enum MCPPlatform
    {
        /// <summary>
        /// MCP doesn't care about Win32 vs. Win64
        /// </summary>
        Windows,

        /// <summary>
        /// Only other platform MCP understands is Mac.
        /// </summary>
        Mac,
    }

    /// <summary>
    /// Enum that defines CDN types
    /// </summary>
    public enum CDNType
    {
        /// <summary>
        /// Internal HTTP CDN server
        /// </summary>
        Internal,

        /// <summary>
        /// Production HTTP CDN server
        /// </summary>
        Production,
    }

    /// <summary>
    /// Class that holds common state used to control the BuildPatchTool build commands that chunk and create patching manifests and publish build info to the BuildInfoService.
    /// </summary>
    public class BuildPatchToolStagingInfo
    {
        /// <summary>
        /// The currently running command, used to get command line overrides
        /// </summary>
        public BuildCommand OwnerCommand;
        /// <summary>
        /// name of the app. Can't always use this to define the staging dir because some apps are not staged to a place that matches their AppName.
        /// </summary>
        public readonly string AppName;
        /// <summary>
        /// Usually the base name of the app. Used to get the MCP key from a branch dictionary. 
        /// </summary>
        public readonly string McpConfigKey;
        /// <summary>
        /// ID of the app (needed for the BuildPatchTool)
        /// </summary>
        public readonly int AppID;
        /// <summary>
        /// BuildVersion of the App we are staging.
        /// </summary>
        public readonly string BuildVersion;
        /// <summary>
        /// Directory where builds will be staged. Rooted at the BuildRootPath, using a subfolder passed in the ctor, 
        /// and using BuildVersion/PlatformName to give each builds their own home.
        /// </summary>
        public readonly string StagingDir;
        /// <summary>
        /// Path to the CloudDir where chunks will be written (relative to the BuildRootPath)
        /// This is used to copy to the web server, so it can use the same relative path to the root build directory.
        /// This allows file to be either copied from the local file system or the webserver using the same relative paths.
        /// </summary>
        public readonly string CloudDirRelativePath;
        /// <summary>
        /// full path to the CloudDir where chunks and manifests should be staged. Rooted at the BuildRootPath, using a subfolder pass in the ctor.
        /// </summary>
        public readonly string CloudDir;
        /// <summary>
        /// Platform we are staging for.
        /// </summary>
        public readonly MCPPlatform Platform;

        /// <summary>
        /// Gets the base filename of the manifest that would be created by invoking the BuildPatchTool with the given parameters.
        /// </summary>
        public string ManifestFilename
        {
            get
            {
                return AppName + BuildVersion + "-" + Platform.ToString() + ".manifest";
            }
        }

        /// <summary>
        /// Determine the platform name (Win32/64 becomes Windows, Mac is Mac, the rest we don't currently understand)
        /// </summary>
        static public MCPPlatform ToMCPPlatform(UnrealTargetPlatform TargetPlatform)
        {
            if (TargetPlatform != UnrealTargetPlatform.Win64 && TargetPlatform != UnrealTargetPlatform.Win32 && TargetPlatform != UnrealTargetPlatform.Mac)
            {
                throw new AutomationException("Platform {0} is not properly supported by the MCP backend yet", TargetPlatform);
            }
            return (TargetPlatform == UnrealTargetPlatform.Win64 || TargetPlatform == UnrealTargetPlatform.Win32) ? MCPPlatform.Windows : MCPPlatform.Mac;
        }
        /// <summary>
        /// Determine the platform name (Win32/64 becomes Windows, Mac is Mac, the rest we don't currently understand)
        /// </summary>
        static public UnrealTargetPlatform FromMCPPlatform(MCPPlatform TargetPlatform)
        {
            if (TargetPlatform != MCPPlatform.Windows && TargetPlatform != MCPPlatform.Mac)
            {
                throw new AutomationException("Platform {0} is not properly supported by the MCP backend yet", TargetPlatform);
            }
            return (TargetPlatform == MCPPlatform.Windows) ? UnrealTargetPlatform.Win64 : UnrealTargetPlatform.Mac;
        }
        /// <summary>
        /// Returns the build root path (P:\Builds on build machines usually)
        /// </summary>
        /// <returns></returns>
        static public string GetBuildRootPath()
        {
            return CommandUtils.IsBuildMachine
                ? (Utils.IsRunningOnMono ? "/Volumes/Builds" : @"P:\Builds")
                : CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "LocalBuilds");
        }

        /// <summary>
        /// Basic constructor. 
        /// </summary>
        /// <param name="InAppName"></param>
        /// <param name="InAppID"></param>
        /// <param name="InBuildVersion"></param>
        /// <param name="platform"></param>
        /// <param name="stagingDirRelativePath">Relative path from the BuildRootPath where files will be staged. Commonly matches the AppName.</param>
        public BuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string InAppName, string InMcpConfigKey, int InAppID, string InBuildVersion, UnrealTargetPlatform platform, string stagingDirRelativePath)
            : this(InOwnerCommand, InAppName, InMcpConfigKey, InAppID, InBuildVersion, ToMCPPlatform(platform), stagingDirRelativePath)
        {
        }

        /// <summary>
        /// Basic constructor. 
        /// </summary>
        /// <param name="InAppName"></param>
        /// <param name="InAppID"></param>
        /// <param name="InBuildVersion"></param>
        /// <param name="platform"></param>
        /// <param name="stagingDirRelativePath">Relative path from the BuildRootPath where files will be staged. Commonly matches the AppName.</param>
        public BuildPatchToolStagingInfo(BuildCommand InOwnerCommand, string InAppName, string InMcpConfigKey, int InAppID, string InBuildVersion, MCPPlatform platform, string stagingDirRelativePath)
        {
            OwnerCommand = InOwnerCommand;
            AppName = InAppName;
            McpConfigKey = InMcpConfigKey;
            AppID = InAppID;
            BuildVersion = InBuildVersion;
            Platform = platform;
            var BuildRootPath = GetBuildRootPath();
            StagingDir = CommandUtils.CombinePaths(BuildRootPath, stagingDirRelativePath, BuildVersion, Platform.ToString());
            CloudDirRelativePath = CommandUtils.CombinePaths(stagingDirRelativePath, "CloudDir");
            CloudDir = CommandUtils.CombinePaths(BuildRootPath, CloudDirRelativePath);
        }
    }


    /// <summary>
    /// Class that provides programmatic access to the BuildPatchTool
    /// </summary>
    public abstract class BuildPatchToolBase
    {
        /// <summary>
        /// Controls the chunking type used by the buildinfo server (nochunks parameter)
        /// </summary>
        public enum ChunkType
        {
            /// <summary>
            /// Chunk the files
            /// </summary>
            Chunk,
            /// <summary>
            /// Don't chunk the files, just build a file manifest.
            /// </summary>
            NoChunk,
        }


        public class BuildPatchToolOptions
        {
            /// <summary>
            /// Staging information
            /// </summary>
            public BuildPatchToolStagingInfo StagingInfo;
            /// <summary>
            /// Matches the corresponding BuildPatchTool command line argument.
            /// </summary>
            public string BuildRoot;
            /// <summary>
            /// Matches the corresponding BuildPatchTool command line argument.
            /// </summary>
            public string FileIgnoreList;
            /// <summary>
            /// Matches the corresponding BuildPatchTool command line argument.
            /// </summary>
            public string AppLaunchCmd;
            /// <summary>
            /// Matches the corresponding BuildPatchTool command line argument.
            /// </summary>
            public string AppLaunchCmdArgs;
            /// <summary>
            /// Corresponds to the -nochunks parameter
            /// </summary>
            public ChunkType AppChunkType;
            /// <summary>
            /// Matches the corresponding BuildPatchTool command line argument.
            /// </summary>
            public MCPPlatform Platform;
        }

        static BuildPatchToolBase Handler = null;

        public static BuildPatchToolBase Get()
        {
            if (Handler == null)
            {
                Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
                foreach (var Dll in LoadedAssemblies)
                {
                    Type[] AllTypes = Dll.GetTypes();
                    foreach (var PotentialConfigType in AllTypes)
                    {
                        if (PotentialConfigType != typeof(BuildPatchToolBase) && typeof(BuildPatchToolBase).IsAssignableFrom(PotentialConfigType))
                        {
                            Handler = Activator.CreateInstance(PotentialConfigType) as BuildPatchToolBase;
                            break;
                        }
                    }
                }
                if (Handler == null)
                {
                    throw new AutomationException("Attempt to use BuildPatchToolBase.Get() and it doesn't appear that there are any modules that implement this class.");
                }
            }
            return Handler;
        }

        /// <summary>
        /// Runs the patcher executable using the supplied parameters.
        /// </summary>
        /// <returns>BuildInfo descringing the output of the chunking process.</returns>
        public abstract void Execute(BuildPatchToolOptions Opts);

        public abstract HashSet<string> ExtractChunksFromManifestLegacy(string ManifestFilename, BuildPatchToolBase.ChunkType ChunkType);

        /// <summary>
        /// Extracts the chunk subdirectory given a chunk Guid in the legacy manifest (V1) format.
        /// </summary>
        /// <param name="chunkGuid">Guid of hte string (in UE4 string format)</param>
        /// <returns>2-digit subdirectory string where the chunk can be found.</returns>
        public abstract string GetChunkSubdirectoryV1(string chunkGuid);

        /// <summary>
        /// Extracts the chunk filenames from a manifest file.
        /// </summary>
        /// <remarks>
        /// This code has to undo a lot of UE4 serialization stuff. Bytes are converted to 3-digit decimal and strung together as a string, so we have to undo that.
        /// </remarks>
        /// <param name="ManifestFilename">Full path to the manifest file</param>
        /// <param name="bLookForV1Chunks">Whether to look for V1 chunks as well. If we know we didn't write any, this saves time.</param>
        /// <returns>Set of full paths to the chunks in the manifest.</returns>
        public abstract HashSet<string> ExtractChunksFromManifest(string manifestFilename, bool bLookForV1Chunks);

        /// <summary>
        /// Handles cleaning up unused chunks in the CloudDir for a specified app.
        /// </summary>
        /// <param name="stagingInfo">The staging info used to determine what cloud dir to compactify</param>
        public abstract void CompactifyCloudDir(BuildPatchToolStagingInfo stagingInfo);
    }


    /// <summary>
    /// Helper class
    /// </summary>
    public abstract class BuildInfoPublisherBase
    {
        static BuildInfoPublisherBase Handler = null;
 
        public static BuildInfoPublisherBase Get()
        {
            if (Handler == null)
            {
                Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
                foreach (var Dll in LoadedAssemblies)
                {
                    Type[] AllTypes = Dll.GetTypes();
                    foreach (var PotentialConfigType in AllTypes)
                    {
                        if (PotentialConfigType != typeof(BuildInfoPublisherBase) && typeof(BuildInfoPublisherBase).IsAssignableFrom(PotentialConfigType))
                        {
                            Handler = Activator.CreateInstance(PotentialConfigType) as BuildInfoPublisherBase;
                            break;
                        }
                    }
                }
                if (Handler == null)
                {
                    throw new AutomationException("Attempt to use BuildInfoPublisherBase.Get() and it doesn't appear that there are any modules that implement this class.");
                }
            }
            return Handler;
        }
        /// <summary>
        /// Given a MCPStagingInfo defining our build info, posts the build to the MCP BuildInfo Service.
        /// </summary>
        /// <param name="stagingInfo">Staging Info describing the BuildInfo to post.</param>
        abstract public void PostBuildInfo(BuildPatchToolStagingInfo stagingInfo);

		/// <summary>
		/// Given a MCPStagingInfo defining our build info and a MCP config name, posts the build to the requested MCP BuildInfo Service.
		/// </summary>
		/// <param name="StagingInfo">Staging Info describing the BuildInfo to post.</param>
		/// <param name="McpConfigName">Name of which MCP config to post to.</param>
		abstract public void PostBuildInfo(BuildPatchToolStagingInfo StagingInfo, string McpConfigName);

		/// <summary>
		/// Given a BuildVersion defining our a build, return the labels applied to that build
		/// </summary>
		/// <param name="BuildVersion">Build version to return labels for.</param>
		/// <param name="McpConfigName">Which BuildInfo backend to get labels from for this promotion attempt.</param>
		abstract public List<string> GetBuildLabels(BuildPatchToolStagingInfo StagingInfo, string McpConfigName);

		/// <summary>
		/// Get a label string for the specific Platform requested.
		/// </summary>
		/// <param name="DestinationLabel">Base of label</param>
		/// <param name="Platform">Platform to add to base label.</param>
		abstract public string GetLabelWithPlatform(string DestinationLabel, MCPPlatform Platform);

		/// <summary>
		/// Get a BuildVersion string with the Platform concatenated on.
		/// </summary>
		/// <param name="DestinationLabel">Base of label</param>
		/// <param name="Platform">Platform to add to base label.</param>
		abstract public string GetBuildVersionWithPlatform(BuildPatchToolStagingInfo StagingInfo);

		/// <summary>
		/// Apply the requested label to the requested build in the BuildInfo backend for the requested MCP environment
		/// </summary>
		/// <param name="StagingInfo">Staging info for the build to label.</param>
		/// <param name="DestinationLabelWithPlatform">Label, including platform, to apply</param>
		/// <param name="McpConfigName">Which BuildInfo backend to label the build in.</param>
		abstract public void LabelBuild(BuildPatchToolStagingInfo StagingInfo, string DestinationLabelWithPlatform, string McpConfigName);

        /// <summary>
        /// Informs Patcher Service of a new build availability after async labeling is complete
        /// (this usually means the build was copied to a public file server before the label could be applied).
        /// </summary>
        /// <param name="Command">Parent command</param>
        /// <param name="AppName">Application name that the patcher service will use.</param>
        /// <param name="BuildVersion">BuildVersion string that the patcher service will use.</param>
        /// <param name="ManifestRelativePath">Relative path to the Manifest file relative to the global build root (which is like P:\Builds) </param>
        /// <param name="LabelName">Name of the label that we will be setting.</param>
        abstract public void BuildPromotionCompleted(BuildPatchToolStagingInfo stagingInfo, string AppName, string BuildVersion, string ManifestRelativePath, string PlatformName, string LabelName);

        /// <summary>
        /// Mounts the production CDN share (allows overriding via -CDNDrive command line arg)
        /// </summary>
        /// <param name="command"></param>
        /// <returns>Path to the share (allows for override)</returns>
        abstract public string MountProductionCDNShare(BuildCommand command);

        /// <summary>
        /// Mounts the production CDN share (allows overriding via -CDNDrive command line arg)
        /// </summary>
        /// <param name="command"></param>
        /// <returns>Path to the share (allows for override)</returns>
        abstract public string MountInternalCDNShare(BuildCommand command);

        /// <summary>
        /// Mounts an NFS share (allows overriding via -CDNDrive command line arg)
        /// </summary>
        /// <param name="command"></param>
        /// <returns>Full path to the NFSShare where builds should be placed.</returns>
        [Help("CDNDrive", "Allows local testing of the CDN steps by rerouting the NFS mount to a local location (should already exist as it simulated mounting it!)")]
        abstract public string MountNFSShare(BuildCommand command, string nfsSharePath, string nfsMountDrive);

        /// <summary>
        /// Copies chunks from a staged location to the production CDN.
        /// </summary>
        /// <param name="command">Build command (used to allow the -CDNDrive cmdline override).</param>
        /// <param name="stagingInfo">Staging info used to determine where the chunks are to copy.</param>
        abstract public void CopyChunksToProductionCDN(BuildPatchToolStagingInfo stagingInfo);

        /// <summary>
        /// Copies chunks from a staged location to the production CDN.
        /// NOTE: This code assumes the location of the BuildRootPath at the time this build 
        /// by calling <see cref="BuildPatchToolStagingInfo.GetBuildRootPath"/> (usually P:\Builds).
        /// If this path changes then this code posting older builds will break because we won't know
        /// where the BuildRootPath for the older build was!
        /// </summary>
        /// <param name="command">Build command (used to allow the -CDNDrive cmdline override).</param>
        /// <param name="manifestUrlPath">relative path to the manifest file from the build info service</param>
        abstract public void CopyChunksToProductionCDN(BuildCommand command, string manifestUrlPath);

        /// <summary>
        /// Mirrors the CloudDir with the internal web server that can also serve chunks.
        /// </summary>
        /// <param name="command">Build command (used to allow the -CDNDrive cmdline override).</param>
        /// <param name="stagingInfo">Staging info used to determine where the chunks are to copy.</param>
        abstract public void MirrorCloudDirToInternalCDN(BuildPatchToolStagingInfo stagingInfo);
    }
    /// <summary>
    /// Helpers for using the MCP account service
    /// </summary>
    public abstract class McpAccountServiceBase
    {
        static McpAccountServiceBase Handler = null;

        public static McpAccountServiceBase Get()
        {
            if (Handler == null)
            {
                Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
                foreach (var Dll in LoadedAssemblies)
                {
                    Type[] AllTypes = Dll.GetTypes();
                    foreach (var PotentialConfigType in AllTypes)
                    {
                        if (PotentialConfigType != typeof(McpAccountServiceBase) && typeof(McpAccountServiceBase).IsAssignableFrom(PotentialConfigType))
                        {
                            Handler = Activator.CreateInstance(PotentialConfigType) as McpAccountServiceBase;
                            break;
                        }
                    }
                }
                if (Handler == null)
                {
                    throw new AutomationException("Attempt to use McpAccountServiceBase.Get() and it doesn't appear that there are any modules that implement this class.");
                }
            }
            return Handler;
        }
        public abstract string GetClientToken(BuildPatchToolStagingInfo StagingInfo);
		public abstract string GetClientToken(McpConfigData McpConfig);
        public abstract string SendWebRequest(WebRequest Upload, string Method, string ContentType, byte[] Data);
    }

}

namespace EpicGames.MCP.Config
{
    /// <summary>
    /// Class for retrieving MCP configuration data
    /// </summary>
    public class McpConfigHelper
    {
        // List of configs is cached off for fetching from multiple times
        private static Dictionary<string, McpConfigData> Configs;

        public static McpConfigData Find(string ConfigName)
        {
            if (Configs == null)
            {
                // Load all secret configs by trying to instantiate all classes derived from McpConfig from all loaded DLLs.
                // Note that we're using the default constructor on the secret config types.
                Configs = new Dictionary<string, McpConfigData>();
                Assembly[] LoadedAssemblies = AppDomain.CurrentDomain.GetAssemblies();
                foreach (var Dll in LoadedAssemblies)
                {
                    Type[] AllTypes = Dll.GetTypes();
                    foreach (var PotentialConfigType in AllTypes)
                    {
                        if (PotentialConfigType != typeof(McpConfigData) && typeof(McpConfigData).IsAssignableFrom(PotentialConfigType))
                        {
                            try
                            {
                                McpConfigData Config = Activator.CreateInstance(PotentialConfigType) as McpConfigData;
                                if (Config != null)
                                {
                                    Configs.Add(Config.Name, Config);
                                }
                            }
                            catch
                            {
                                BuildCommand.LogWarning("Unable to create McpConfig: {0}", PotentialConfigType.Name);
                            }
                        }
                    }
                }
            }
            McpConfigData LoadedConfig;
            Configs.TryGetValue(ConfigName, out LoadedConfig);
            if (LoadedConfig == null)
            {
                throw new AutomationException("Unable to find requested McpConfig: {0}", ConfigName);
            }
            return LoadedConfig;
        }
    }

    // Class for storing mcp configuration data
    public class McpConfigData
    {
        public McpConfigData(string InName, string InAccountBaseUrl, string InFortniteBaseUrl, string InBuildInfoBaseUrl, string InLauncherBaseUrl, string InClientId, string InClientSecret)
        {
            Name = InName;
            AccountBaseUrl = InAccountBaseUrl;
            FortniteBaseUrl = InFortniteBaseUrl;
            BuildInfoBaseUrl = InBuildInfoBaseUrl;
            LauncherBaseUrl = InLauncherBaseUrl;
            ClientId = InClientId;
            ClientSecret = InClientSecret;
        }

        public string Name;
        public string AccountBaseUrl;
        public string FortniteBaseUrl;
        public string BuildInfoBaseUrl;
        public string LauncherBaseUrl;
        public string ClientId;
        public string ClientSecret;

        public void SpewValues()
        {
            CommandUtils.Log("Name : {0}", Name);
            CommandUtils.Log("AccountBaseUrl : {0}", AccountBaseUrl);
            CommandUtils.Log("FortniteBaseUrl : {0}", FortniteBaseUrl);
            CommandUtils.Log("BuildInfoBaseUrl : {0}", BuildInfoBaseUrl);
            CommandUtils.Log("LauncherBaseUrl : {0}", LauncherBaseUrl);
            CommandUtils.Log("ClientId : {0}", ClientId);
            // we don't really want this in logs CommandUtils.Log("ClientSecret : {0}", ClientSecret);
        }
    }

    public class McpConfigMapper
    {
        static public McpConfigData FromMcpConfigKey(string McpConfigKey)
        {
            return McpConfigHelper.Find("MainGameDevNet");
        }

        static public McpConfigData FromStagingInfo(EpicGames.MCP.Automation.BuildPatchToolStagingInfo StagingInfo)
        {
            string McpConfigNameToLookup = null;
            if (StagingInfo.OwnerCommand != null)
            {
                McpConfigNameToLookup = StagingInfo.OwnerCommand.ParseParamValue("MCPConfig");
            }
            if (String.IsNullOrEmpty(McpConfigNameToLookup))
            {
                return FromMcpConfigKey(StagingInfo.McpConfigKey);
            }
            return McpConfigHelper.Find(McpConfigNameToLookup);
        }
    }

}
