// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SlateShaderResource.h: Declares various shader resources types.
=============================================================================*/

#pragma once


namespace ESlateShaderResource
{
	/**
	 * Enumerates Slate render resource types.
	 */
	enum Type
	{
		/** Texture resource. */
		Texture,

		/** Material resource. */
		Material
	};
}


/** 
 * Base class for all platform independent texture types
 */
class SLATECORE_API FSlateShaderResource
{
public:

	/**
	 * Gets the width of the resource.
	 *
	 * @return Resource width (in pixels).
	 */
	virtual uint32 GetWidth() const = 0;

	/**
	 * Gets the height of the resource.
	 *
	 * @return Resource height(in pixels).
	 */
	virtual uint32 GetHeight() const = 0;

	/**
	 * Gets the type of the resource.
	 *
	 * @return Resource type.
	 */
	virtual ESlateShaderResource::Type GetType() const = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~FSlateShaderResource() { }
};


/** 
 * A proxy resource.  
 *
 * May point to a full resource or point or to a texture resource in an atlas
 * Note: This class does not free any resources.  Resources should be owned and freed elsewhere
 */
class SLATECORE_API FSlateShaderResourceProxy
{
public:

	/** The start uv of the texture.  If atlased this is some subUV of the atlas, 0,0 otherwise */
	FVector2D StartUV;

	/** The size of the texture in UV space.  If atlas this some sub uv of the atlas.  1,1 otherwise */
	FVector2D SizeUV;

	/** The resource to be used for rendering */
	FSlateShaderResource* Resource;

	/** The size of the texture.  Regardless of atlasing this is the size of the actual texture */
	FIntPoint ActualSize;

	/**
	 * Default constructor.
	 */
	FSlateShaderResourceProxy( )
		: StartUV(0.0f, 0.0f)
		, SizeUV(1.0f, 1.0f)
		, Resource(nullptr)
		, ActualSize(0.0f, 0.0f)
	{ }
};


/** 
 * Abstract base class for platform independent texture resource accessible by the shader.
 */
template <typename ResourceType>
class TSlateTexture
	: public FSlateShaderResource
{
public:

	/**
	 * Default constructor.
	 */
	TSlateTexture( ) { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InShaderResource The resource to use.
	 */
	TSlateTexture( ResourceType& InShaderResource )
		: ShaderResource( InShaderResource )
	{ }

	virtual ~TSlateTexture() { }

public:

	/**
	 * Gets the resource used by the shader.
	 *
	 * @return The resource.
	 */
	ResourceType& GetTypedResource()
	{
		return ShaderResource;
	}

public:

	// Begin FSlateShaderResource interface

	virtual ESlateShaderResource::Type GetType() const OVERRIDE
	{
		return ESlateShaderResource::Texture;
	}

	// End FSlateShaderResource interface

protected:

	// Holds the resource.
	ResourceType ShaderResource;
};
