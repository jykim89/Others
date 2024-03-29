// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Texture.h"
#include "Texture2D.generated.h"

UCLASS(hidecategories=Object, MinimalAPI, BlueprintType)
class UTexture2D : public UTexture
{
	GENERATED_UCLASS_BODY()

public:
	/** Number of miplevels the texture should have resident.					*/
	UPROPERTY(transient, NonTransactional)
	int32 RequestedMips;

	/** Number of miplevels currently resident.									*/
	UPROPERTY(transient, NonTransactional)
	int32 ResidentMips;

private:
	/** FStreamingTexture index used by the texture streaming system. */
	UPROPERTY(transient, duplicatetransient, NonTransactional)
	int32 StreamingIndex;

public:
	/** keep track of first mip level used for ResourceMem creation */
	UPROPERTY()
	int32 FirstResourceMemMip;

	/** Used for various timing measurements, e.g. streaming latency. */
	float Timer;

	/** The width of the texture. */
	UPROPERTY()
	int32 SizeX_DEPRECATED;

	/** The height of the texture. */
	UPROPERTY()
	int32 SizeY_DEPRECATED;

	/** The original width of the texture source art we imported from.			*/
	UPROPERTY()
	int32 OriginalSizeX_DEPRECATED;

	/** The original height of the texture source art we imported from.			*/
	UPROPERTY()
	int32 OriginalSizeY_DEPRECATED;

private:
	/**
	 * The imported size of the texture. Only valid on cooked builds when texture source is not
	 * available. Access ONLY via the GetImportedSize() accessor!
	 */
	UPROPERTY()
	FIntPoint ImportedSize;

public:
	/**
	 * Retrieves the size of the source image from which the texture was created.
	 */
	FORCEINLINE FIntPoint GetImportedSize() const
	{
#if WITH_EDITORONLY_DATA
		return FIntPoint(Source.GetSizeX(),Source.GetSizeY());
#else // #if WITH_EDITORONLY_DATA
		return ImportedSize;
#endif // #if WITH_EDITORONLY_DATA
	}

private:
	/** WorldSettings timestamp that tells the streamer to force all miplevels to be resident up until that time. */
	UPROPERTY(transient)
	float ForceMipLevelsToBeResidentTimestamp;

	/** True if streaming is temporarily disabled so we can update subregions of this texture's resource 
	without streaming clobbering it. Automatically cleared before saving. */
	UPROPERTY(transient)
	uint32 bTemporarilyDisableStreaming:1;

public:
	/** true if the texture's mips should be stored directly and not use the derived data cache. Used by procedurally generated textures. */
	UPROPERTY()
	uint32 bDisableDerivedDataCache_DEPRECATED:1;

	/** Whether the texture is currently streamable or not.						*/
	UPROPERTY(transient, NonTransactional)
	uint32 bIsStreamable:1;

	/** Whether the current texture mip change request is pending cancellation.	*/
	UPROPERTY(transient, NonTransactional)
	uint32 bHasCancelationPending:1;

	/** Override whether to fully stream even if texture hasn't been rendered.	*/
	UPROPERTY(transient)
	uint32 bForceMiplevelsToBeResident:1;

	/** Global and serialized version of ForceMiplevelsToBeResident.				*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LevelOfDetail, meta=(DisplayName="Global Force Resident Mip Levels"), AdvancedDisplay)
	uint32 bGlobalForceMipLevelsToBeResident:1;

#if WITH_EDITORONLY_DATA
	/** Whether the texture has been painted in the editor.						*/
	UPROPERTY()
	uint32 bHasBeenPaintedInEditor:1;
#endif // WITH_EDITORONLY_DATA

	/** The format of the texture data. */
	UPROPERTY()
	TEnumAsByte<enum EPixelFormat> Format_DEPRECATED;

	/** The addressing mode to use for the X axis.								*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, meta=(DisplayName="X-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
	TEnumAsByte<enum TextureAddress> AddressX;

	/** The addressing mode to use for the Y axis.								*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Texture, meta=(DisplayName="Y-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
	TEnumAsByte<enum TextureAddress> AddressY;

	/** ID generated whenever the texture is changed so that its bulk data can be updated in the TextureFileCache during cook */
	FGuid TextureFileCacheGuid_DEPRECATED;

public:
	/** The derived data for this texture on this platform. */
	FTexturePlatformData *PlatformData;
	/* cooked platform data for this texture */
	TMap<FString, FTexturePlatformData*> CookedPlatformData;
	/**
	 * Thread-safe counter indicating the texture streaming state. The definitions below are mirrored in Texture.h.
	 *
		 enum ETextureStreamingState
		 {
			// The renderer hasn't created the resource yet.
			TexState_InProgress_Initialization	= -1,
			// There are no pending requests/ all requests have been fulfilled.
			TexState_ReadyFor_Requests			= 0,
			// Finalization has been kicked off and is in progress.
			TexState_InProgress_Finalization	= 1,
			// Initial request has completed and finalization needs to be kicked off.
			TexState_ReadyFor_Finalization		= 2,
			// We're currently loading in mip data.
			TexState_InProgress_Loading			= 3,
			// ...
			// States 3+N means we're currently loading in N mips
			// ...
			// Memory has been allocated and we're ready to start loading in mips.
			TexState_ReadyFor_Loading			= 100,
			// We're currently allocating/preparing memory for the new mip count.
			TexState_InProgress_Allocating		= 101,
		};
	*/
	mutable FThreadSafeCounter	PendingMipChangeRequestStatus;

	/** memory used for directly loading bulk mip data */
	FTexture2DResourceMem*		ResourceMem;

public:

	// Begin UObject interface.
	virtual void Serialize(FArchive& Ar) OVERRIDE;	
#if WITH_EDITOR
	virtual void CookerWillNeverCookAgain() OVERRIDE;
	virtual void PostLinkerChange() OVERRIDE;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
	virtual void BeginDestroy() OVERRIDE;
	virtual void PostLoad() OVERRIDE;
	virtual void PreSave() OVERRIDE;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const OVERRIDE;
	virtual FString GetDesc() OVERRIDE;
	// End UObject interface.

	// Begin UTexture interface.
	virtual float GetSurfaceWidth() const OVERRIDE { return GetSizeX(); }
	virtual float GetSurfaceHeight() const OVERRIDE { return GetSizeY(); }
	virtual FTextureResource* CreateResource() OVERRIDE;
	virtual EMaterialValueType GetMaterialType() OVERRIDE { return MCT_Texture2D; }
	virtual void UpdateResource() OVERRIDE;
	virtual float GetAverageBrightness(bool bIgnoreTrueBlack, bool bUseGrayscale) OVERRIDE;
	virtual FTexturePlatformData** GetRunningPlatformData() OVERRIDE { return &PlatformData; }
	virtual TMap<FString,FTexturePlatformData*>* GetCookedPlatformData() OVERRIDE { return &CookedPlatformData; }
	// End UTexture interface.

	/** Trivial accessors. */
	FORCEINLINE int32 GetSizeX() const
	{
		if (PlatformData)
		{
			return PlatformData->SizeX;
		}
		return 0;
	}
	FORCEINLINE int32 GetSizeY() const
	{
		if (PlatformData)
		{
			return PlatformData->SizeY;
		}
		return 0;
	}
	FORCEINLINE int32 GetNumMips() const
	{
		if (PlatformData)
		{
			return PlatformData->Mips.Num();
		}
		return 0;
	}
	FORCEINLINE EPixelFormat GetPixelFormat() const
	{
		if (PlatformData)
		{
			return PlatformData->PixelFormat;
		}
		return PF_Unknown;
	}
	FORCEINLINE int32 GetMipTailBaseIndex() const
	{
		if (PlatformData)
		{
			return FMath::Max(0, PlatformData->Mips.Num() - 1);
		}
		return 0;
	}
	FORCEINLINE const TIndirectArray<FTexture2DMipMap>& GetPlatformMips() const
	{
		check(PlatformData);
		return PlatformData->Mips;
	}

private:
	/** The minimum number of mips that must be resident in memory (cannot be streamed). */
	static int32 GMinTextureResidentMipCount;

public:
	/** Returns the minimum number of mips that must be resident in memory (cannot be streamed). */
	static FORCEINLINE int32 GetMinTextureResidentMipCount()
	{
		return GMinTextureResidentMipCount;
	}

	/** Sets the minimum number of mips that must be resident in memory (cannot be streamed). */
	static void SetMinTextureResidentMipCount(int32 InMinTextureResidentMipCount);

	/**
	 * Get mip data starting with the specified mip index.
	 * @param FirstMipToLoad - The first mip index to cache.
	 * @param OutMipData -	Must point to an array of pointers with at least
	 *						Mips.Num() - FirstMipToLoad + 1 entries. Upon
	 *						return those pointers will contain mip data.
	 */
	void GetMipData(int32 FirstMipToLoad, void** OutMipData);

	/**
	 * Returns the number of mips in this texture that are not able to be streamed.
	 */
	int32 GetNumNonStreamingMips() const;

	/**
	 * Computes the minimum and maximum allowed mips for a texture.
	 * @param MipCount - The number of mip levels in the texture.
	 * @param NumNonStreamingMips - The number of mip levels that are not allowed to stream.
	 * @param LODBias - Bias applied to the number of mip levels.
	 * @param OutMinAllowedMips - Returns the minimum number of mip levels that must be loaded.
	 * @param OutMaxAllowedMips - Returns the maximum number of mip levels that must be loaded.
	 */
	static void CalcAllowedMips( int32 MipCount, int32 NumNonStreamingMips, int32 LODBias, int32& OuMinAllowedMips, int32& OutMaxAllowedMips );

	/**
	 * Calculates the size of this texture in bytes if it had MipCount miplevels streamed in.
	 *
	 * @param	MipCount	Number of mips to calculate size for, counting from the smallest 1x1 mip-level and up.
	 * @return	Size of MipCount mips in bytes
	 */
	int32 CalcTextureMemorySize( int32 MipCount ) const;

	/**
	 * Calculates the size of this texture if it had MipCount miplevels streamed in.
	 *
	 * @param	Enum	Which mips to calculate size for.
	 * @return	Total size of all specified mips, in bytes
	 */
	virtual uint32 CalcTextureMemorySizeEnum( ETextureMipCount Enum ) const OVERRIDE;

	/**
	 *	Get the CRC of the source art pixels.
	 *
	 *	@param	[out]	OutSourceCRC		The CRC value of the source art pixels.
	 *
	 *	@return			bool				true if successful, false if failed (or no source art)
	 */
	ENGINE_API bool GetSourceArtCRC(uint32& OutSourceCRC);

	/**
	 *	See if the source art of the two textures matches...
	 *
	 *	@param		InTexture		The texture to compare it to
	 *
	 *	@return		bool			true if they matche, false if not
	 */
	ENGINE_API bool HasSameSourceArt(UTexture2D* InTexture);
	
	/**
	 * Returns true if the runtime texture has an alpha channel that is not completely white.
	 */
	ENGINE_API bool HasAlphaChannel() const;

	/**
	 * Returns whether the texture is ready for streaming aka whether it has had InitRHI called on it.
	 *
	 * @return true if initialized and ready for streaming, false otherwise
	 */
	bool IsReadyForStreaming();

	/**
	 * Waits until all streaming requests for this texture has been fully processed.
	 */
	virtual void WaitForStreaming();
	
	/**
	 * Updates the streaming status of the texture and performs finalization when appropriate. The function returns
	 * true while there are pending requests in flight and updating needs to continue.
	 *
	 * @param bWaitForMipFading	Whether to wait for Mip Fading to complete before finalizing.
	 * @return					true if there are requests in flight, false otherwise
	 */
	virtual bool UpdateStreamingStatus( bool bWaitForMipFading = false );

	/**
	 * Tries to cancel a pending mip change request. Requests cannot be canceled if they are in the
	 * finalization phase.
	 *
	 * @param	true if cancelation was successful, false otherwise
	 */
	bool CancelPendingMipChangeRequest();

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) OVERRIDE;

	/**
	 * Returns whether miplevels should be forced resident.
	 *
	 * @return true if either transient or serialized override requests miplevels to be resident, false otherwise
	 */
	bool ShouldMipLevelsBeForcedResident() const;

	/**
	 * Whether all miplevels of this texture have been fully streamed in, LOD settings permitting.
	 */
	ENGINE_API bool IsFullyStreamedIn();

	/**
	 * Links texture to the texture streaming manager.
	 */
	void LinkStreaming();

	/**
	 * Unlinks texture from the texture streaming manager.
	 */
	void UnlinkStreaming();
	
	/**
	 * Cancels any pending texture streaming actions if possible.
	 * Returns when no more async loading requests are in flight.
	 */
	ENGINE_API static void CancelPendingTextureStreaming();


	/**
	 * Returns the global mip map bias applied as an offset for 2d textures.
	 */
	ENGINE_API static float GetGlobalMipMapLODBias();

	/**
	 * Calculates and returns the corresponding ResourceMem parameters for this texture.
	 *
	 * @param FirstMipIdx		Index of the largest mip-level stored within a seekfree (level) package
	 * @param OutSizeX			[out] Width of the stored largest mip-level
	 * @param OutSizeY			[out] Height of the stored largest mip-level
	 * @param OutNumMips		[out] Number of stored mips
	 * @param OutTexCreateFlags	[out] ETextureCreateFlags bit flags
	 * @return					true if the texture should use a ResourceMem. If false, none of the out parameters will be filled in.
	 */
	bool GetResourceMemSettings(int32 FirstMipIdx, int32& OutSizeX, int32& OutSizeY, int32& OutNumMips, uint32& OutTexCreateFlags);

#if WITH_EDITOR
	/**
	 *	Asynchronously update a set of regions of a texture with new data.
	 *	@param MipIndex - the mip number to update
	 *	@param NumRegions - number of regions to update
	 *	@param Regions - regions to update
	 *	@param SrcPitch - the pitch of the source data in bytes
	 *	@param SrcBpp - the size one pixel data in bytes
	 *	@param SrcData - the source data
	 *  @param bFreeData - if true, the SrcData and Regions pointers will be freed after the update.
	 */
	void UpdateTextureRegions( int32 MipIndex, uint32 NumRegions, FUpdateTextureRegion2D* Regions, uint32 SrcPitch, uint32 SrcBpp, uint8* SrcData, bool bFreeData );

	/**
	 * Temporarily disable streaming so we update subregions of this texture without streaming clobbering it. 
	 */
	void TemporarilyDisableStreaming();

	/** Called after an editor or undo operation is formed on texture
	*/
	virtual void PostEditUndo();

#endif // WITH_EDITOR

	friend struct FStreamingManagerTexture;
	friend struct FStreamingTexture;
	
	/**
	 * Tells the streaming system that it should force all mip-levels to be resident for a number of seconds.
	 * @param Seconds					Duration in seconds
	 * @param CinematicTextureGroups	Bitfield indicating which texture groups that use extra high-resolution mips
	 */
	virtual void SetForceMipLevelsToBeResident( float Seconds, int32 CinematicTextureGroups = 0 );
	
	/** creates and initializes a new Texture2D with the requested settings */
	ENGINE_API static class UTexture2D* CreateTransient(int32 InSizeX, int32 InSizeY, EPixelFormat InFormat = PF_B8G8R8A8);

#if WITH_EDITORONLY_DATA
	/**
	 * Legacy serialization.
	 */
	void LegacySerialize(class FArchive& Ar, class FStripDataFlags& StripDataFlags);
#endif

	/**
	 * Gets the X size of the texture, in pixels
	 */
	UFUNCTION(BlueprintCallable, meta=(FriendlyName = "GetSizeX"), Category="Rendering|Texture")
	int32 Blueprint_GetSizeX() const;

	/**
	 * Gets the Y size of the texture, in pixels
	 */
	UFUNCTION(BlueprintCallable, meta=(FriendlyName = "GetSizeY"), Category="Rendering|Texture")
	int32 Blueprint_GetSizeY() const;

	/**
	 * Update the offset for mip map lod bias.
	 * This is added to any existing mip bias values.
	 */
	virtual void RefreshSamplerStates();
};
