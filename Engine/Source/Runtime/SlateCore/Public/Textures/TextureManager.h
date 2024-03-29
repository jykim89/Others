// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Represents a new texture that has been loaded but no resource created for it
 */
struct FNewTextureInfo
{
	/** Raw data */
	FSlateTextureDataPtr TextureData;
	/** Whether or not the texture should be atlased */
	bool bShouldAtlas;
	/** Whether or not the texture is in srgb space */
	bool bSrgb;
	FNewTextureInfo()
		: bShouldAtlas(true)
		, bSrgb(true)
	{

	}
};

struct FCompareFNewTextureInfoByTextureSize
{
	FORCEINLINE bool operator()( const FNewTextureInfo& A, const FNewTextureInfo& B ) const
	{
		return (B.TextureData->GetWidth()+B.TextureData->GetHeight()) < (A.TextureData->GetWidth()+A.TextureData->GetHeight());
	}
};


/** 
 * Base texture manager class used by a Slate renderer to manage texture resources
 */
class FSlateShaderResourceManager
{
public:
	FSlateShaderResourceManager() {};
	virtual ~FSlateShaderResourceManager() 
	{
		ClearTextureMap();
	}


	/** 
	 * Returns a texture associated with the passed in name.  Should return nullptr if not found 
	 */
	virtual FSlateShaderResourceProxy* GetTexture( const FSlateBrush& InBrush ) = 0;

protected:

	void ClearTextureMap()
	{
		// delete all allocated textures
		for( TMap<FName,FSlateShaderResourceProxy*>::TIterator It(ResourceMap); It; ++It )
		{
			delete It.Value();
		}
		ResourceMap.Empty();
	}

	FString GetResourcePath( const FSlateBrush& InBrush )
	{
		// assume the brush name contains the whole path
		return InBrush.GetResourceName().ToString();
	}

	/** Mapping of names to texture pointers */
	TMap<FName,FSlateShaderResourceProxy*> ResourceMap;
};
