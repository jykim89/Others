// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLVertexBuffer.cpp: OpenGL texture RHI implementation.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

/** Caching it here, to avoid getting it every time we create a texture. 0 is no multisampling. */
GLint GMaxOpenGLColorSamples = 0;
GLint GMaxOpenGLDepthSamples = 0;
GLint GMaxOpenGLIntegerSamples = 0;

// in bytes, never change after RHI, needed to scale game features
int64 GOpenGLDedicatedVideoMemory = 0;
// In bytes. Never changed after RHI init. Our estimate of the amount of memory that we can use for graphics resources in total.
int64 GOpenGLTotalGraphicsMemory = 0;

static bool ShouldCountAsTextureMemory(uint32 Flags)
{
	return (Flags & (TexCreate_RenderTargetable | TexCreate_ResolveTargetable | TexCreate_DepthStencilTargetable)) == 0;
}

void OpenGLTextureAllocated(FRHITexture* Texture, uint32 Flags)
{
	int32 TextureSize = 0;
	FOpenGLTextureCube* TextureCube = 0;
	FOpenGLTexture2D* Texture2D = 0;
	FOpenGLTexture2DArray* Texture2DArray = 0;
	FOpenGLTexture3D* Texture3D = 0;
	bool bRenderTarget = !ShouldCountAsTextureMemory(Flags);

	if (( TextureCube = (FOpenGLTextureCube*)Texture->GetTextureCube()) != NULL)
	{
		TextureSize = CalcTextureSize( TextureCube->GetSize(), TextureCube->GetSize(), TextureCube->GetFormat(), TextureCube->GetNumMips() );
		TextureSize *= TextureCube->GetArraySize() * (TextureCube->GetArraySize() == 1 ? 6 : 1);
		TextureCube->SetMemorySize( TextureSize );
		TextureCube->SetIsPowerOfTwo(FMath::IsPowerOfTwo(TextureCube->GetSizeX()) && FMath::IsPowerOfTwo(TextureCube->GetSizeY()));
		if (bRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemoryCube,TextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemoryCube,TextureSize);
		}
	}
	else if ((Texture2D = (FOpenGLTexture2D*)Texture->GetTexture2D()) != NULL)
	{
		TextureSize = CalcTextureSize( Texture2D->GetSizeX(), Texture2D->GetSizeY(), Texture2D->GetFormat(), Texture2D->GetNumMips() )*Texture2D->GetNumSamples();
		Texture2D->SetMemorySize( TextureSize );
		Texture2D->SetIsPowerOfTwo(FMath::IsPowerOfTwo(Texture2D->GetSizeX()) && FMath::IsPowerOfTwo(Texture2D->GetSizeY()));
		if (bRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D,TextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemory2D,TextureSize);
		}
	}
	else if ((Texture3D = (FOpenGLTexture3D*)Texture->GetTexture3D()) != NULL)
	{
		TextureSize = CalcTextureSize3D( Texture3D->GetSizeX(), Texture3D->GetSizeY(), Texture3D->GetSizeZ(), Texture3D->GetFormat(), Texture3D->GetNumMips() );
		Texture3D->SetMemorySize( TextureSize );
		Texture3D->SetIsPowerOfTwo(FMath::IsPowerOfTwo(Texture3D->GetSizeX()) && FMath::IsPowerOfTwo(Texture3D->GetSizeY()) && FMath::IsPowerOfTwo(Texture3D->GetSizeZ()));
		if (bRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemory3D,TextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemory3D,TextureSize);
		}
	}
	else if ((Texture2DArray = (FOpenGLTexture2DArray*)Texture->GetTexture2DArray()) != NULL)
	{
		TextureSize = Texture2DArray->GetSizeZ() * CalcTextureSize( Texture2DArray->GetSizeX(), Texture2DArray->GetSizeY(), Texture2DArray->GetFormat(), Texture2DArray->GetNumMips() );
		Texture2DArray->SetMemorySize( TextureSize );
		Texture2DArray->SetIsPowerOfTwo(FMath::IsPowerOfTwo(Texture2DArray->GetSizeX()) && FMath::IsPowerOfTwo(Texture2DArray->GetSizeY()));
		if (bRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D,TextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemory2D,TextureSize);
		}
	}
	else
	{
		check(0);	// Add handling of other texture types
	}

	if( bRenderTarget )
	{
		GCurrentRendertargetMemorySize += Align(TextureSize, 1024) / 1024;
	}
	else
	{
		GCurrentTextureMemorySize += Align(TextureSize, 1024) / 1024;
	}
}

void OpenGLTextureDeleted( FRHITexture* Texture )
{
	bool bRenderTarget = !ShouldCountAsTextureMemory(Texture->GetFlags());
	int32 TextureSize = 0;
	if (Texture->GetTextureCube())
	{
		TextureSize = ((FOpenGLTextureCube*)Texture->GetTextureCube())->GetMemorySize();
		if (bRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemory3D,TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemory3D,TextureSize);
		}
	}
	else if (Texture->GetTexture2D())
	{
		TextureSize = ((FOpenGLTexture2D*)Texture->GetTexture2D())->GetMemorySize();
		if (bRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D,TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemory2D,TextureSize);
		}
	}
	else if (Texture->GetTexture3D())
	{
		TextureSize = ((FOpenGLTexture3D*)Texture->GetTexture3D())->GetMemorySize();
		if (bRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemory3D,TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemory3D,TextureSize);
		}
	}
	else if (Texture->GetTexture2DArray())
	{
		TextureSize = ((FOpenGLTexture2DArray*)Texture->GetTexture2DArray())->GetMemorySize();
		if (bRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D,TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemory2D,TextureSize);
		}
	}
	else
	{
		check(0);	// Add handling of other texture types
	}

	if( bRenderTarget )
	{
		GCurrentRendertargetMemorySize -= Align(TextureSize, 1024) / 1024;
	}
	else
	{
		GCurrentTextureMemorySize -= Align(TextureSize, 1024) / 1024;
	}
}

uint64 FOpenGLDynamicRHI::RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
{
	OutAlign = 0;
	return CalcTextureSize(SizeX, SizeY, (EPixelFormat)Format, NumMips);
}

uint64 FOpenGLDynamicRHI::RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	OutAlign = 0;
	return CalcTextureSize3D(SizeX, SizeY, SizeZ, (EPixelFormat)Format, NumMips);
}

uint64 FOpenGLDynamicRHI::RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags,	uint32& OutAlign)
{
	OutAlign = 0;
	return CalcTextureSize(Size, Size, (EPixelFormat)Format, NumMips) * 6;
}

/**
 * Retrieves texture memory stats. Unsupported with this allocator.
 *
 * @return false, indicating that out variables were left unchanged.
 */
void FOpenGLDynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	OutStats.DedicatedVideoMemory = GOpenGLDedicatedVideoMemory;
    OutStats.DedicatedSystemMemory = 0;
    OutStats.SharedSystemMemory = 0;
	OutStats.TotalGraphicsMemory = GOpenGLTotalGraphicsMemory ? GOpenGLTotalGraphicsMemory : -1;

	OutStats.AllocatedMemorySize = int64(GCurrentTextureMemorySize) * 1024;
	OutStats.TexturePoolSize = GTexturePoolSize;
	OutStats.PendingMemoryAdjustment = 0;
}

/**
 * Fills a texture with to visualize the texture pool memory.
 *
 * @param	TextureData		Start address
 * @param	SizeX			Number of pixels along X
 * @param	SizeY			Number of pixels along Y
 * @param	Pitch			Number of bytes between each row
 * @param	PixelSize		Number of bytes each pixel represents
 *
 * @return true if successful, false otherwise
 */
bool FOpenGLDynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	return false;
}

FRHITexture* FOpenGLDynamicRHI::CreateOpenGLTexture(uint32 SizeX,uint32 SizeY,bool bCubeTexture, bool bArrayTexture, uint8 Format,uint32 NumMips,uint32 NumSamples, uint32 ArraySize, uint32 Flags, FResourceBulkDataInterface* BulkData)
{
	VERIFY_GL_SCOPE();

	SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateTextureTime);

	bool bAllocatedStorage = false;

	if(NumMips == 0)
	{
		if(NumSamples <= 1)
		{
			NumMips = FindMaxMipmapLevel(SizeX, SizeY);
		}
		else
		{
			NumMips = 1;
		}
	}

#if UE_BUILD_DEBUG
	check( !( NumSamples > 1 && bCubeTexture) );
	check( bArrayTexture != (ArraySize == 1));
#endif

	if (GRHIFeatureLevel <= ERHIFeatureLevel::ES2)
	{
		// Remove sRGB read flag when not supported
		Flags &= ~TexCreate_SRGB;
	}

	GLuint TextureID = 0;
	FOpenGL::GenTextures(1, &TextureID);

	GLenum Target = GL_NONE;
	if(bCubeTexture)
	{
		if ( FOpenGL::SupportsTexture3D() )
		{
			Target = bArrayTexture ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_CUBE_MAP;
		}
		else
		{
			check(!bArrayTexture);
			Target = GL_TEXTURE_CUBE_MAP;
		}
		check(SizeX == SizeY);
	}
	else
	{
		Target =  (NumSamples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

		// @todo: refactor 2d texture array support here?
		check(!bArrayTexture);
	}

	check(Target != GL_NONE);

	const bool bSRGB = (Flags&TexCreate_SRGB) != 0;
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Format];
	if (GLFormat.InternalFormat[bSRGB] == GL_NONE)
	{
		UE_LOG(LogRHI, Fatal,TEXT("Texture format '%s' not supported (sRGB=%d)."), GPixelFormats[Format].Name, bSRGB);
	}

	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();

	// Make sure PBO is disabled
	CachedBindPixelUnpackBuffer(ContextState,0);

	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, TextureID, 0, NumMips);

	if (NumSamples == 1)
	{
		if (!FMath::IsPowerOfTwo(SizeX) || !FMath::IsPowerOfTwo(SizeY))
		{
			glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			if ( FOpenGL::SupportsTexture3D() )
			{
				glTexParameteri(Target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			}
		}
		else
		{
			glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_REPEAT);
			if ( FOpenGL::SupportsTexture3D() )
			{
				glTexParameteri(Target, GL_TEXTURE_WRAP_R, GL_REPEAT);
			}
		}
		glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		if( FOpenGL::SupportsTextureFilterAnisotropic() )
		{
			glTexParameteri(Target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
		}
		if ( FOpenGL::SupportsTextureBaseLevel() )
		{
			glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, 0);
		}
		if ( FOpenGL::SupportsTextureMaxLevel() )
		{
			glTexParameteri(Target, GL_TEXTURE_MAX_LEVEL, NumMips - 1);
		}

		if (bArrayTexture)
		{
			FOpenGL::TexStorage3D( Target, NumMips, GLFormat.InternalFormat[bSRGB], SizeX, SizeY, ArraySize, GLFormat.Format, GLFormat.Type);
		}
		else
		{
			// Try to allocate using TexStorage2D
			if( FOpenGL::TexStorage2D( Target, NumMips, GLFormat.SizedInternalFormat[bSRGB], SizeX, SizeY, GLFormat.Format, GLFormat.Type, Flags) )
			{
				bAllocatedStorage = true;
			}
			else if (!GLFormat.bCompressed)
			{
				// Otherwise, allocate storage for each mip using TexImage2D
				// We can't do so for compressed textures because we can't pass NULL in to CompressedTexImage2D!
				bAllocatedStorage = true;

				const bool bIsCubeTexture = Target == GL_TEXTURE_CUBE_MAP;
				const GLenum FirstTarget = bIsCubeTexture ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : Target;
				const uint32 NumTargets = bIsCubeTexture ? 6 : 1;

				for(uint32 MipIndex = 0; MipIndex < uint32(NumMips); MipIndex++)
				{
					for(uint32 TargetIndex = 0; TargetIndex < NumTargets; TargetIndex++)
					{
						glTexImage2D(
							FirstTarget + TargetIndex,
							MipIndex,
							GLFormat.InternalFormat[bSRGB],
							FMath::Max<uint32>(1,(SizeX >> MipIndex)),
							FMath::Max<uint32>(1,(SizeY >> MipIndex)),
							0,
							GLFormat.Format,
							GLFormat.Type,
							NULL
							);
					}
				}
			}
		}

		if (BulkData != NULL)
		{
			uint8* Data = (uint8*)BulkData->GetResourceBulkData();
			uint32 MipOffset = 0;

			for(uint32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
			{
				if(bArrayTexture )
				{
					if(bCubeTexture)
					{
						check(FOpenGL::SupportsTexture3D());
						FOpenGL::TexSubImage3D(
							/*Target=*/ Target,
							/*Level=*/ MipIndex,
							/* XOffset */ 0,
							/* YOffset */ 0,
							/* ZOffset */ 0,
							/*SizeX=*/ FMath::Max<uint32>(1,(SizeX >> MipIndex)),
							/*SizeY=*/ FMath::Max<uint32>(1,(SizeY >> MipIndex)),
							/*SizeZ=*/ ArraySize,
							/*Format=*/ GLFormat.Format,
							/*Type=*/ GLFormat.Type,
							/*Data=*/ &Data[MipOffset]
							);
					}
					else
					{
						// @todo: refactor 2d texture arrays here?
						check(!bCubeTexture);
					}
				}
				else
				{
					GLenum FirstTarget = bCubeTexture ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : Target;
					uint32 NumTargets = bCubeTexture ? 6 : 1;

					for(uint32 TargetIndex = 0; TargetIndex < NumTargets; TargetIndex++)
					{
						glTexSubImage2D(
							/*Target=*/ FirstTarget + TargetIndex,
							/*Level=*/ MipIndex,
							/*XOffset*/ 0,
							/*YOffset*/ 0,
							/*SizeX=*/ FMath::Max<uint32>(1,(SizeX >> MipIndex)),
							/*SizeY=*/ FMath::Max<uint32>(1,(SizeY >> MipIndex)),
							/*Format=*/ GLFormat.Format,
							/*Type=*/ GLFormat.Type,
							/*Data=*/ &Data[MipOffset]
							);
					}
				}
				uint32 NumBlocksX = FMath::Max<uint32>(1,(SizeX >> MipIndex) / GPixelFormats[Format].BlockSizeX);
				uint32 NumBlocksY = FMath::Max<uint32>(1,(SizeY >> MipIndex) / GPixelFormats[Format].BlockSizeY);
				uint32 NumLayers = FMath::Max<uint32>(1,ArraySize);

				MipOffset               += NumBlocksX * NumBlocksY * NumLayers * GPixelFormats[Format].BlockBytes;

			}
		}
	}
	else
	{
		check( FOpenGL::SupportsMultisampledTextures() );
		check( BulkData == NULL);

		// Try to create an immutable texture and fallback if it fails
		if (!FOpenGL::TexStorage2DMultisample( Target, NumSamples, GLFormat.InternalFormat[bSRGB], SizeX, SizeY, true))
		{
			FOpenGL::TexImage2DMultisample(
				Target,
				NumSamples,
				GLFormat.InternalFormat[bSRGB],
				SizeX,
				SizeY,
				true
				);
		}
	}
	
	// Determine the attachment point for the texture.	
	GLenum Attachment = GL_NONE;
	if((Flags & TexCreate_RenderTargetable) || (Flags & TexCreate_CPUReadback))
	{
		Attachment = GL_COLOR_ATTACHMENT0;
	}
	else if(Flags & TexCreate_DepthStencilTargetable)
	{
		Attachment = (Format == PF_DepthStencil && FOpenGL::SupportsCombinedDepthStencilAttachment()) ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
	}
	else if(Flags & TexCreate_ResolveTargetable)
	{
		Attachment = (Format == PF_DepthStencil && FOpenGL::SupportsCombinedDepthStencilAttachment())
						? GL_DEPTH_STENCIL_ATTACHMENT
						: ((Format == PF_ShadowDepth || Format == PF_D24)
							? GL_DEPTH_ATTACHMENT
							: GL_COLOR_ATTACHMENT0);
	}

	switch(Attachment)
	{
		case GL_COLOR_ATTACHMENT0:
			check(GMaxOpenGLColorSamples>=(GLint)NumSamples);
			break;
		case GL_DEPTH_ATTACHMENT:
		case GL_DEPTH_STENCIL_ATTACHMENT:
			check(GMaxOpenGLDepthSamples>=(GLint)NumSamples);
			break;
		default:
			break;
	}
	// @todo: If integer pixel format
	//check(GMaxOpenGLIntegerSamples>=NumSamples);

	FRHITexture* Texture;
	if (bCubeTexture)
	{
		FOpenGLTextureCube* TextureCube = new FOpenGLTextureCube(this,TextureID,Target,Attachment,SizeX,SizeY,0,NumMips,1, ArraySize, (EPixelFormat)Format,true,bAllocatedStorage,Flags);
		Texture = TextureCube;
	}
	else
	{
		FOpenGLTexture2D* Texture2D = new FOpenGLTexture2D(this,TextureID,Target,Attachment,SizeX,SizeY,0,NumMips,NumSamples, 1, (EPixelFormat)Format,false,bAllocatedStorage,Flags);
		Texture = Texture2D;
	}
	OpenGLTextureAllocated( Texture, Flags );

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.

	return Texture;
}

#if PLATFORM_MAC // Flithy hack to workaround radr://16011763
GLuint FOpenGLTextureBase::GetOpenGLFramebuffer(uint32 ArrayIndices, uint32 MipmapLevels)
{
	GLuint FBO = 0;
	switch(Attachment)
	{
		case GL_COLOR_ATTACHMENT0:
		{
			FOpenGLTextureBase* RenderTarget[] = {this};
			FBO = OpenGLRHI->GetOpenGLFramebuffer(1, RenderTarget, &ArrayIndices, &MipmapLevels, NULL);
			break;
		}
		case GL_DEPTH_ATTACHMENT:
		case GL_DEPTH_STENCIL_ATTACHMENT:
		{
			FBO = OpenGLRHI->GetOpenGLFramebuffer(1, NULL, &ArrayIndices, &MipmapLevels, this);
			break;
		}
		default:
			break;
	}
	return FBO;
}
#endif

template<typename RHIResourceType>
void TOpenGLTexture<RHIResourceType>::Resolve(uint32 MipIndex,uint32 ArrayIndex)
{
	VERIFY_GL_SCOPE();
	
#if UE_BUILD_DEBUG
	if((FOpenGLTexture2D*)this->GetTexture2D())
	{
		check( ((FOpenGLTexture2D*)this->GetTexture2D())->GetNumSamples() == 1 );
	}
#endif
	
	// Calculate the dimensions of the mip-map.
	EPixelFormat PixelFormat = this->GetFormat();
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> MipIndex,BlockSizeX);
	const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> MipIndex,BlockSizeY);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
	if ( PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4 )
	{
		// PVRTC has minimum 2 blocks width and height
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
		NumBlocksY = FMath::Max<uint32>(NumBlocksY, 2);
	}
	const uint32 MipBytes = NumBlocksX * NumBlocksY * BlockBytes;
	
	const int32 BufferIndex = MipIndex * (bCubemap ? 6 : 1) * this->GetEffectiveSizeZ() + ArrayIndex;
	
	// Standard path with a PBO mirroring ever slice of a texture to allow multiple simulataneous maps
	if (!IsValidRef(PixelBuffers[BufferIndex]))
	{
		PixelBuffers[BufferIndex] = new FOpenGLPixelBuffer(0, MipBytes, BUF_Dynamic);
	}
	
	TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = PixelBuffers[BufferIndex];
	check(PixelBuffer->GetSize() == MipBytes);
	check(!PixelBuffer->IsLocked());
	
	check( FOpenGL::SupportsPixelBuffers() );
	
	// Transfer data from texture to pixel buffer.
	// This may be further optimized by caching information if surface content was changed since last lock.
	
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
	const bool bSRGB = (this->GetFlags() & TexCreate_SRGB) != 0;
	
	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	FOpenGLContextState& ContextState = OpenGLRHI->GetContextStateForCurrentContext();
	OpenGLRHI->CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, Resource, -1, this->GetNumMips());
	
	glBindBuffer( GL_PIXEL_PACK_BUFFER, PixelBuffer->Resource );
	
#if PLATFORM_MAC // glReadPixels is async with PBOs - glGetTexImage is not: radr://16011763
	if(Attachment == GL_COLOR_ATTACHMENT0 && !GLFormat.bCompressed)
	{
		GLuint SourceFBO = GetOpenGLFramebuffer(ArrayIndex, MipIndex);
		check(SourceFBO > 0);
		glBindFramebuffer(UGL_READ_FRAMEBUFFER, SourceFBO);
		FOpenGL::ReadBuffer(Attachment);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, MipSizeX, MipSizeY, GLFormat.Format, GLFormat.Type, 0 );
		glPixelStorei(GL_PACK_ALIGNMENT, 4);
		ContextState.Framebuffer = (GLuint)-1;
	}
	else
#endif
		if( this->GetSizeZ() )
		{
			// apparently it's not possible to retrieve compressed image from GL_TEXTURE_2D_ARRAY in OpenGL for compressed images
			// and for uncompressed ones it's not possible to specify the image index
			check(0);
		}
		else
		{
			if (GLFormat.bCompressed)
			{
				FOpenGL::GetCompressedTexImage(
											   bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
											   MipIndex,
											   0);	// offset into PBO
			}
			else
			{
				glPixelStorei(GL_PACK_ALIGNMENT, 1);
				FOpenGL::GetTexImage(
									 bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
									 MipIndex,
									 GLFormat.Format,
									 GLFormat.Type,
									 0);	// offset into PBO
				glPixelStorei(GL_PACK_ALIGNMENT, 4);
			}
		}
	
	glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
	
	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.
}

template<typename RHIResourceType>
void* TOpenGLTexture<RHIResourceType>::Lock(uint32 MipIndex,uint32 ArrayIndex,EResourceLockMode LockMode,uint32& DestStride)
{
	VERIFY_GL_SCOPE();

#if UE_BUILD_DEBUG
	if((FOpenGLTexture2D*)this->GetTexture2D())
	{
		check( ((FOpenGLTexture2D*)this->GetTexture2D())->GetNumSamples() == 1 );
	}
#endif

	SCOPE_CYCLE_COUNTER(STAT_OpenGLLockTextureTime);

	void* result = NULL;

	// Calculate the dimensions of the mip-map.
	EPixelFormat PixelFormat = this->GetFormat();
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> MipIndex,BlockSizeX);
	const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> MipIndex,BlockSizeY);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
	if ( PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4 )
	{
		// PVRTC has minimum 2 blocks width and height
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
		NumBlocksY = FMath::Max<uint32>(NumBlocksY, 2);
	}
	const uint32 MipBytes = NumBlocksX * NumBlocksY * BlockBytes;

	DestStride = NumBlocksX * BlockBytes;

	const int32 BufferIndex = MipIndex * (bCubemap ? 6 : 1) * this->GetEffectiveSizeZ() + ArrayIndex;

	// Standard path with a PBO mirroring ever slice of a texture to allow multiple simulataneous maps
	bool bBufferExists = true;
	if (!IsValidRef(PixelBuffers[BufferIndex]))
	{
		bBufferExists = false;
		PixelBuffers[BufferIndex] = new FOpenGLPixelBuffer(0, MipBytes, BUF_Dynamic);
	}

	TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = PixelBuffers[BufferIndex];
	check(PixelBuffer->GetSize() == MipBytes);
	check(!PixelBuffer->IsLocked());
	
	// If the buffer already exists & the flags are such that the texture cannot be rendered to & is CPU accessible then we can skip the internal resolve for read locks. This makes HZB occlusion faster.
	const bool bCPUTexResolved = bBufferExists && (this->GetFlags() & TexCreate_CPUReadback) && !(this->GetFlags() & (TexCreate_RenderTargetable|TexCreate_DepthStencilTargetable));

	if( LockMode != RLM_WriteOnly && !bCPUTexResolved && FOpenGL::SupportsPixelBuffers() )
	{
		Resolve(MipIndex, ArrayIndex);
	}

	result = PixelBuffer->Lock(0, PixelBuffer->GetSize(), LockMode == RLM_ReadOnly, LockMode != RLM_ReadOnly);
	
	return result;
}

template<typename RHIResourceType>
void TOpenGLTexture<RHIResourceType>::Unlock(uint32 MipIndex,uint32 ArrayIndex)
{
	VERIFY_GL_SCOPE();

	SCOPE_CYCLE_COUNTER(STAT_OpenGLUnlockTextureTime);

	const int32 BufferIndex = MipIndex * (bCubemap ? 6 : 1) * this->GetEffectiveSizeZ() + ArrayIndex;
	check(IsValidRef(PixelBuffers[BufferIndex]));

	TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = PixelBuffers[BufferIndex];
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[this->GetFormat()];
	const bool bSRGB = (this->GetFlags() & TexCreate_SRGB) != 0;

	if ( FOpenGL::SupportsPixelBuffers() )
	{
		// Code path for PBO per slice
		check(IsValidRef(PixelBuffers[BufferIndex]));
			
		PixelBuffer->Unlock();

		// Modify permission?
		if (!PixelBuffer->IsLockReadOnly())
		{
			// Use a texture stage that's not likely to be used for draws, to avoid waiting
			FOpenGLContextState& ContextState = OpenGLRHI->GetContextStateForCurrentContext();
			OpenGLRHI->CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, Resource, -1, this->GetNumMips());

			if( this->GetSizeZ() )
			{
				// texture 2D array
				if (GLFormat.bCompressed)
				{
					FOpenGL::CompressedTexSubImage3D(
						Target,
						MipIndex,
						0,
						0,
						ArrayIndex,
						FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
						FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
						1,
						GLFormat.InternalFormat[bSRGB],
						PixelBuffer->GetSize(),
						0);
				}
				else
				{
					glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
					check( FOpenGL::SupportsTexture3D() );
					FOpenGL::TexSubImage3D(
						Target,
						MipIndex,
						0,
						0,
						ArrayIndex,
						FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
						FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
						1,
						GLFormat.Format,
						GLFormat.Type,
						0);	// offset into PBO
					glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				}
			}
			else
			{
				if (GLFormat.bCompressed)
				{
					if (GetAllocatedStorageForMip(MipIndex,ArrayIndex))
					{
						glCompressedTexSubImage2D(
							bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
							MipIndex,
							0,
							0,
							FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
							FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
							GLFormat.InternalFormat[bSRGB],
							PixelBuffer->GetSize(),
							0);	// offset into PBO
					}
					else
					{
						glCompressedTexImage2D(
							bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
							MipIndex,
							GLFormat.InternalFormat[bSRGB],
							FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
							FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
							0,
							PixelBuffer->GetSize(),
							0);	// offset into PBO
						SetAllocatedStorageForMip(MipIndex,ArrayIndex);
					}
				}
				else
				{
					// All construction paths should have called TexStorage2D or TexImage2D. So we will
					// always call TexSubImage2D.
					check(GetAllocatedStorageForMip(MipIndex,ArrayIndex) == true);
					glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
					glTexSubImage2D(
						bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
						MipIndex,
						0,
						0,
						FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
						FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
						GLFormat.Format,
						GLFormat.Type,
						0);	// offset into PBO
					glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				}
			}
		}

		//need to free PBO if we aren't keeping shadow copies
		PixelBuffers[BufferIndex] = NULL;
	}
	else
	{
		// Code path for non-PBO:
		// Volume/array textures are currently only supported if PixelBufferObjects are also supported.
		check(this->GetSizeZ() == 0);

		// Use a texture stage that's not likely to be used for draws, to avoid waiting
		FOpenGLContextState& ContextState = OpenGLRHI->GetContextStateForCurrentContext();
		OpenGLRHI->CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, Resource, -1, this->GetNumMips());

		CachedBindPixelUnpackBuffer( 0 );
		
		if (GLFormat.bCompressed)
		{
			if (GetAllocatedStorageForMip(MipIndex,ArrayIndex))
			{
				glCompressedTexSubImage2D(
					bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
					MipIndex,
					0,
					0,
					FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
					FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
					GLFormat.InternalFormat[bSRGB],
					PixelBuffer->GetSize(),
					PixelBuffer->GetLockedBuffer());
			}
			else
			{
				glCompressedTexImage2D(
					bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
					MipIndex,
					GLFormat.InternalFormat[bSRGB],
					FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
					FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
					0,
					PixelBuffer->GetSize(),
					PixelBuffer->GetLockedBuffer());
					SetAllocatedStorageForMip(MipIndex,ArrayIndex);
			}
		}
		else
		{
			// All construction paths should have called TexStorage2D or TexImage2D. So we will
			// always call TexSubImage2D.
			check(GetAllocatedStorageForMip(MipIndex,ArrayIndex) == true);
			glTexSubImage2D(
				bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
				MipIndex,
				0,
				0,
				FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
				FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
				GLFormat.Format,
				GLFormat.Type,
				PixelBuffer->GetLockedBuffer());
		}

		// Unlock "PixelBuffer" and free the temp memory after the texture upload.
		PixelBuffer->Unlock();
	}

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.

	CachedBindPixelUnpackBuffer(0);
}

template<typename RHIResourceType>
void TOpenGLTexture<RHIResourceType>::CloneViaCopyImage( TOpenGLTexture<RHIResourceType>* Src, uint32 NumMips, int32 SrcOffset, int32 DstOffset)
{
	VERIFY_GL_SCOPE();
	
	check(FOpenGL::SupportsCopyImage());
	
	for (uint32 ArrayIndex = 0; ArrayIndex < this->GetEffectiveSizeZ(); ArrayIndex++)
	{
		// use the Copy Image functionality to copy mip level by mip level
		for(uint32 MipIndex = 0;MipIndex < NumMips;++MipIndex)
		{
			// Calculate the dimensions of the mip-map.
			const uint32 DstMipIndex = MipIndex + DstOffset;
			const uint32 SrcMipIndex = MipIndex + SrcOffset;
			const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> DstMipIndex,uint32(1));
			const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> DstMipIndex,uint32(1));
			
			// copy the texture data
			FOpenGL::CopyImageSubData( Src->Resource, Src->Target, SrcMipIndex, 0, 0, ArrayIndex,
									  Resource, Target, DstMipIndex, 0, 0, ArrayIndex, MipSizeX, MipSizeY, 1);
		}
	}
	
}

template<typename RHIResourceType>
void TOpenGLTexture<RHIResourceType>::CloneViaPBO( TOpenGLTexture<RHIResourceType>* Src, uint32 NumMips, int32 SrcOffset, int32 DstOffset)
{
	VERIFY_GL_SCOPE();
	
	// apparently it's not possible to retrieve compressed image from GL_TEXTURE_2D_ARRAY in OpenGL for compressed images
	// and for uncompressed ones it's not possible to specify the image index
	check(this->GetSizeZ() == 0);
	
	// only PBO path is supported here
	check( FOpenGL::SupportsPixelBuffers() );
	
	EPixelFormat PixelFormat = this->GetFormat();
	check(PixelFormat == Src->GetFormat());

	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
	const bool bSRGB = (this->GetFlags() & TexCreate_SRGB) != 0;
	check(bSRGB == ((Src->GetFlags() & TexCreate_SRGB) != 0));
	
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	
	FOpenGLContextState& ContextState = OpenGLRHI->GetContextStateForCurrentContext();
	
	for (uint32 ArrayIndex = 0; ArrayIndex < this->GetEffectiveSizeZ(); ArrayIndex++)
	{
		// use PBO functionality to copy mip level by mip level
		for(uint32 MipIndex = 0;MipIndex < NumMips;++MipIndex)
		{
			// Actual mip levels
			const uint32 DstMipIndex = MipIndex + DstOffset;
			const uint32 SrcMipIndex = MipIndex + SrcOffset;
			
			// Calculate the dimensions of the mip-map.
			const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> DstMipIndex,1u);
			const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> DstMipIndex,1u);
			
			// Then the rounded PBO size required to capture this mip
			const uint32 DataSizeX = FMath::Max(MipSizeX,BlockSizeX);
			const uint32 DataSizeY = FMath::Max(MipSizeY,BlockSizeY);
			uint32 NumBlocksX = (DataSizeX + BlockSizeX - 1) / BlockSizeX;
			uint32 NumBlocksY = (DataSizeY + BlockSizeY - 1) / BlockSizeY;
			if ( PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4 )
			{
				// PVRTC has minimum 2 blocks width and height
				NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
				NumBlocksY = FMath::Max<uint32>(NumBlocksY, 2);
			}

			const uint32 MipBytes = NumBlocksX * NumBlocksY * BlockBytes;
			const int32 BufferIndex = DstMipIndex * (bCubemap ? 6 : 1) * this->GetEffectiveSizeZ() + ArrayIndex;
			
			// Standard path with a PBO mirroring ever slice of a texture to allow multiple simulataneous maps
			if (!IsValidRef(PixelBuffers[BufferIndex]))
			{
				PixelBuffers[BufferIndex] = new FOpenGLPixelBuffer(0, MipBytes, BUF_Dynamic);
			}
			
			TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = PixelBuffers[BufferIndex];
			check(PixelBuffer->GetSize() == MipBytes);
			check(!PixelBuffer->IsLocked());
			
			// Transfer data from texture to pixel buffer.
			// This may be further optimized by caching information if surface content was changed since last lock.
			{
				// Use a texture stage that's not likely to be used for draws, to avoid waiting
				OpenGLRHI->CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Src->Target, Src->Resource, -1, this->GetNumMips());
				
				glBindBuffer( GL_PIXEL_PACK_BUFFER, PixelBuffer->Resource );
				
#if PLATFORM_MAC // glReadPixels is async with PBOs - glGetTexImage is not: radr://16011763
				if(Attachment == GL_COLOR_ATTACHMENT0 && !GLFormat.bCompressed)
				{
					GLuint SourceFBO = Src->GetOpenGLFramebuffer(ArrayIndex, SrcMipIndex);
					check(SourceFBO > 0);
					glBindFramebuffer(UGL_READ_FRAMEBUFFER, SourceFBO);
					FOpenGL::ReadBuffer(Attachment);
					glPixelStorei(GL_PACK_ALIGNMENT, 1);
					glReadPixels(0, 0, MipSizeX, MipSizeY, GLFormat.Format, GLFormat.Type, 0 );
					glPixelStorei(GL_PACK_ALIGNMENT, 4);
					ContextState.Framebuffer = (GLuint)-1;
				}
				else
#endif
				if (GLFormat.bCompressed)
				{
					FOpenGL::GetCompressedTexImage(Src->bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Src->Target,
												   SrcMipIndex,
												   0);	// offset into PBO
				}
				else
				{
					glPixelStorei(GL_PACK_ALIGNMENT, 1);
					FOpenGL::GetTexImage(Src->bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Src->Target,
										 SrcMipIndex,
										 GLFormat.Format,
										 GLFormat.Type,
										 0);	// offset into PBO
					glPixelStorei(GL_PACK_ALIGNMENT, 4);
				}
			}
			
			// copy the texture data
			// Upload directly into Dst to avoid out-of-band synchronisation caused by glMapBuffer!
			{
				CachedBindPixelUnpackBuffer( PixelBuffer->Resource );
				
				// Use a texture stage that's not likely to be used for draws, to avoid waiting
				OpenGLRHI->CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, Resource, -1, this->GetNumMips());
				
				if( this->GetSizeZ() )
				{
					// texture 2D array
					if (GLFormat.bCompressed)
					{
						FOpenGL::CompressedTexSubImage3D(Target,
														 DstMipIndex,
														 0,
														 0,
														 ArrayIndex,
														 MipSizeX,
														 MipSizeY,
														 1,
														 GLFormat.InternalFormat[bSRGB],
														 PixelBuffer->GetSize(),
														 0);
					}
					else
					{
						glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
						check( FOpenGL::SupportsTexture3D() );
						FOpenGL::TexSubImage3D(Target,
											   DstMipIndex,
											   0,
											   0,
											   ArrayIndex,
											   MipSizeX,
											   MipSizeY,
											   1,
											   GLFormat.Format,
											   GLFormat.Type,
											   0);	// offset into PBO
						glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
					}
				}
				else
				{
					if (GLFormat.bCompressed)
					{
						if (GetAllocatedStorageForMip(DstMipIndex,ArrayIndex))
						{
							glCompressedTexSubImage2D(bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
													  DstMipIndex,
													  0,
													  0,
													  MipSizeX,
													  MipSizeY,
													  GLFormat.InternalFormat[bSRGB],
													  PixelBuffer->GetSize(),
													  0);	// offset into PBO
						}
						else
						{
							glCompressedTexImage2D(bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
												   DstMipIndex,
												   GLFormat.InternalFormat[bSRGB],
												   MipSizeX,
												   MipSizeY,
												   0,
												   PixelBuffer->GetSize(),
												   0);	// offset into PBO
							SetAllocatedStorageForMip(DstMipIndex,ArrayIndex);
						}
					}
					else
					{
						// All construction paths should have called TexStorage2D or TexImage2D. So we will
						// always call TexSubImage2D.
						check(GetAllocatedStorageForMip(DstMipIndex,ArrayIndex) == true);
						glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
						glTexSubImage2D(bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
										DstMipIndex,
										0,
										0,
										MipSizeX,
										MipSizeY,
										GLFormat.Format,
										GLFormat.Type,
										0);	// offset into PBO
						glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
					}
				}
			}
			
			// need to free PBO if we aren't keeping shadow copies
			PixelBuffers[BufferIndex] = NULL;
			
			// No need to restore texture stage; leave it like this,
			// and the next draw will take care of cleaning it up; or
			// next operation that needs the stage will switch something else in on it.
		}
	}
	
	// Reset the buffer bindings on exit only
	glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
	CachedBindPixelUnpackBuffer(0);
}

template<typename RHIResourceType>
TOpenGLTexture<RHIResourceType>::~TOpenGLTexture()
{
	VERIFY_GL_SCOPE();

	OpenGLTextureDeleted( this );

	if( Resource != 0 )
	{
		OpenGLRHI->InvalidateTextureResourceInCache( Resource );
		glDeleteTextures( 1, &Resource );
	}

	ReleaseOpenGLFramebuffers(OpenGLRHI,this);
}

/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

/**
* Creates a 2D RHI texture resource
* @param SizeX - width of the texture to create
* @param SizeY - height of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
FTexture2DRHIRef FOpenGLDynamicRHI::RHICreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,uint32 NumSamples,uint32 Flags,FResourceBulkDataInterface* BulkData)
{
	return (FRHITexture2D*)CreateOpenGLTexture(SizeX,SizeY,false,false,Format,NumMips,NumSamples,1, Flags,BulkData);
}

FTexture2DRHIRef FOpenGLDynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,uint32 Flags,void** InitialMipData,uint32 NumInitialMips)
{
	check(0);
	return FTexture2DRHIRef();
}

void FOpenGLDynamicRHI::RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D,FTexture2DRHIParamRef SrcTexture2D)
{
	check(0);
}

FTexture2DArrayRHIRef FOpenGLDynamicRHI::RHICreateTexture2DArray(uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 Format,uint32 NumMips,uint32 Flags,FResourceBulkDataInterface* BulkData)
{
	VERIFY_GL_SCOPE();

	SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateTextureTime);

	check( FOpenGL::SupportsTexture3D() );

	if(NumMips == 0)
	{
		NumMips = FindMaxMipmapLevel(SizeX, SizeY);
	}

	if (GRHIFeatureLevel <= ERHIFeatureLevel::ES2)
	{
		// Remove sRGB read flag when not supported
		Flags &= ~TexCreate_SRGB;
	}

	GLuint TextureID = 0;
	FOpenGL::GenTextures(1, &TextureID);

	const GLenum Target = GL_TEXTURE_2D_ARRAY;

	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
	CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, TextureID, 0, NumMips);

	glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, NumMips > 1 ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
	if( FOpenGL::SupportsTextureFilterAnisotropic() )
	{
		glTexParameteri(Target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
	}
	glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(Target, GL_TEXTURE_MAX_LEVEL, NumMips - 1);

	const bool bSRGB = (Flags&TexCreate_SRGB) != 0;
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Format];
	if (GLFormat.InternalFormat[bSRGB] == GL_NONE)
	{
		UE_LOG(LogRHI, Fatal,TEXT("Texture format '%s' not supported."), GPixelFormats[Format].Name);
	}

	checkf(!GLFormat.bCompressed, TEXT("%s compressed 2D texture arrays not currently supported by the OpenGL RHI"), GPixelFormats[Format].Name);

	// Make sure PBO is disabled
	CachedBindPixelUnpackBuffer(ContextState, 0);

	uint8* Data = BulkData ? (uint8*)BulkData->GetResourceBulkData() : NULL;
	uint32 MipOffset = 0;

	FOpenGL::TexStorage3D( Target, NumMips, GLFormat.InternalFormat[bSRGB], SizeX, SizeY, SizeZ, GLFormat.Format, GLFormat.Type );

	if (Data)
	{
		for(uint32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			FOpenGL::TexSubImage3D(
				/*Target=*/ Target,
				/*Level=*/ MipIndex,
				0,
				0,
				0,
				/*SizeX=*/ FMath::Max<uint32>(1,(SizeX >> MipIndex)),
				/*SizeY=*/ FMath::Max<uint32>(1,(SizeY >> MipIndex)),
				/*SizeZ=*/ SizeZ,
				/*Format=*/ GLFormat.Format,
				/*Type=*/ GLFormat.Type,
				/*Data=*/ Data ? &Data[MipOffset] : NULL
				);

			uint32 SysMemPitch      =  FMath::Max<uint32>(1,SizeX >> MipIndex) * GPixelFormats[Format].BlockBytes;
			uint32 SysMemSlicePitch =  FMath::Max<uint32>(1,SizeY >> MipIndex) * SysMemPitch;
			MipOffset               += SizeZ * SysMemSlicePitch;
		}
	}
	
	// Determine the attachment point for the texture.	
	GLenum Attachment = GL_NONE;
	if(Flags & TexCreate_RenderTargetable)
	{
		Attachment = GL_COLOR_ATTACHMENT0;
	}
	else if(Flags & TexCreate_DepthStencilTargetable)
	{
		Attachment = (FOpenGL::SupportsCombinedDepthStencilAttachment() && Format == PF_DepthStencil) ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
	}
	else if(Flags & TexCreate_ResolveTargetable)
	{
		Attachment = (Format == PF_DepthStencil && FOpenGL::SupportsCombinedDepthStencilAttachment())
			? GL_DEPTH_STENCIL_ATTACHMENT
			: ((Format == PF_ShadowDepth || Format == PF_D24)
			? GL_DEPTH_ATTACHMENT
			: GL_COLOR_ATTACHMENT0);
	}

	FOpenGLTexture2DArray* Texture = new FOpenGLTexture2DArray(this,TextureID,Target,Attachment,SizeX,SizeY,SizeZ,NumMips,1, SizeZ, (EPixelFormat)Format,false,true,Flags);
	OpenGLTextureAllocated( Texture, Flags );

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.

	return Texture;
}

FTexture3DRHIRef FOpenGLDynamicRHI::RHICreateTexture3D(uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 Format,uint32 NumMips,uint32 Flags,FResourceBulkDataInterface* BulkData)
{
	VERIFY_GL_SCOPE();

	SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateTextureTime);

	check( FOpenGL::SupportsTexture3D() );

	if(NumMips == 0)
	{
		NumMips = FindMaxMipmapLevel(SizeX, SizeY, SizeZ);
	}

	if (GRHIFeatureLevel <= ERHIFeatureLevel::ES2)
	{
		// Remove sRGB read flag when not supported
		Flags &= ~TexCreate_SRGB;
	}

	GLuint TextureID = 0;
	FOpenGL::GenTextures(1, &TextureID);

	const GLenum Target = GL_TEXTURE_3D;

	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
	CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, TextureID, 0, NumMips);

	glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(Target, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	if( FOpenGL::SupportsTextureFilterAnisotropic() )
	{
		glTexParameteri(Target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
	}
	glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(Target, GL_TEXTURE_MAX_LEVEL, NumMips - 1);

	const bool bSRGB = (Flags&TexCreate_SRGB) != 0;
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Format];
	if (GLFormat.InternalFormat[bSRGB] == GL_NONE)
	{
		UE_LOG(LogRHI, Fatal,TEXT("Texture format '%s' not supported."), GPixelFormats[Format].Name);
	}

	// Make sure PBO is disabled
	CachedBindPixelUnpackBuffer(ContextState,0);

	uint8* Data = BulkData ? (uint8*)BulkData->GetResourceBulkData() : NULL;
	uint32 MipOffset = 0;

	FOpenGL::TexStorage3D( Target, NumMips, GLFormat.InternalFormat[bSRGB], SizeX, SizeY, SizeZ, GLFormat.Format, GLFormat.Type );

	if (Data)
	{
		for(uint32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			FOpenGL::TexSubImage3D(
				/*Target=*/ Target,
				/*Level=*/ MipIndex,
				0,
				0,
				0,
				/*SizeX=*/ FMath::Max<uint32>(1,(SizeX >> MipIndex)),
				/*SizeY=*/ FMath::Max<uint32>(1,(SizeY >> MipIndex)),
				/*SizeZ=*/ FMath::Max<uint32>(1,(SizeZ >> MipIndex)),
				/*Format=*/ GLFormat.Format,
				/*Type=*/ GLFormat.Type,
				/*Data=*/ Data ? &Data[MipOffset] : NULL
				);

			uint32 SysMemPitch      =  FMath::Max<uint32>(1,SizeX >> MipIndex) * GPixelFormats[Format].BlockBytes;
			uint32 SysMemSlicePitch =  FMath::Max<uint32>(1,SizeY >> MipIndex) * SysMemPitch;
			MipOffset               += FMath::Max<uint32>(1,SizeZ >> MipIndex) * SysMemSlicePitch;
		}
	}
	
	// Determine the attachment point for the texture.	
	GLenum Attachment = GL_NONE;
	if(Flags & TexCreate_RenderTargetable)
	{
		Attachment = GL_COLOR_ATTACHMENT0;
	}
	else if(Flags & TexCreate_DepthStencilTargetable)
	{
		Attachment = Format == PF_DepthStencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
	}
	else if(Flags & TexCreate_ResolveTargetable)
	{
		Attachment = (Format == PF_DepthStencil && FOpenGL::SupportsCombinedDepthStencilAttachment())
			? GL_DEPTH_STENCIL_ATTACHMENT
			: ((Format == PF_ShadowDepth || Format == PF_D24)
			? GL_DEPTH_ATTACHMENT
			: GL_COLOR_ATTACHMENT0);
	}

	FOpenGLTexture3D* Texture = new FOpenGLTexture3D(this,TextureID,Target,Attachment,SizeX,SizeY,SizeZ,NumMips,1,1, (EPixelFormat)Format,false,true,Flags);
	OpenGLTextureAllocated( Texture, Flags );

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.

	return Texture;
}

void FOpenGLDynamicRHI::RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
{
}

FShaderResourceViewRHIRef FOpenGLDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,Texture2D);

	FOpenGLShaderResourceView *View = 0;

	if (FOpenGL::SupportsTextureView())
	{
		VERIFY_GL_SCOPE();

		GLuint Resource = 0;

		FOpenGL::GenTextures( 1, &Resource);
		const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Texture2D->GetFormat()];
		const bool bSRGB = (Texture2D->GetFlags()&TexCreate_SRGB) != 0;
		
		FOpenGL::TextureView( Resource, Texture2D->Target, Texture2D->Resource, GLFormat.InternalFormat[bSRGB], MipLevel, 1, 0, 1);
		
		View = new FOpenGLShaderResourceView(this, Resource, Texture2D->Target, MipLevel, true);
	}
	else
	{
		View = new FOpenGLShaderResourceView(this, Texture2D->Resource, Texture2D->Target, MipLevel, false);
	}

	return View;
}

FShaderResourceViewRHIRef FOpenGLDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,Texture2D);

	FOpenGLShaderResourceView *View = 0;

	if (FOpenGL::SupportsTextureView())
	{
		VERIFY_GL_SCOPE();

		GLuint Resource = 0;

		FOpenGL::GenTextures( 1, &Resource);

		if (Format != PF_X24_G8)
		{
			const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Format];
			const bool bSRGB = (Texture2D->GetFlags()&TexCreate_SRGB) != 0;
		
			FOpenGL::TextureView( Resource, Texture2D->Target, Texture2D->Resource, GLFormat.InternalFormat[bSRGB], MipLevel, NumMipLevels, 0, 1);
		}
		else
		{
			// PF_X24_G8 doesn't correspond to a real format under OpenGL
			// The solution is to create a view with the original format, and convertit to return the stencil index
			// To match component locations, texture swizzle needs to be setup too
			const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Texture2D->GetFormat()];

			// create a second depth/stencil view
			FOpenGL::TextureView( Resource, Texture2D->Target, Texture2D->Resource, GLFormat.InternalFormat[0], MipLevel, NumMipLevels, 0, 1);

			// Use a texture stage that's not likely to be used for draws, to avoid waiting
			FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
			CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Texture2D->Target, Resource, 0, NumMipLevels);

			//set the texture to return the stencil index, and then force the components to match D3D
			glTexParameteri( Texture2D->Target, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);
			glTexParameteri( Texture2D->Target, GL_TEXTURE_SWIZZLE_R, GL_ZERO);
			glTexParameteri( Texture2D->Target, GL_TEXTURE_SWIZZLE_G, GL_RED);
			glTexParameteri( Texture2D->Target, GL_TEXTURE_SWIZZLE_B, GL_ZERO);
			glTexParameteri( Texture2D->Target, GL_TEXTURE_SWIZZLE_A, GL_ZERO);
		}
		
		View = new FOpenGLShaderResourceView(this, Resource, Texture2D->Target, MipLevel, true);
	}
	else
	{
		View = new FOpenGLShaderResourceView(this, Texture2D->Resource, Texture2D->Target, MipLevel, false);
	}

	return View;
}

/** Generates mip maps for the surface. */
void FOpenGLDynamicRHI::RHIGenerateMips(FTextureRHIParamRef SurfaceRHI)
{
	VERIFY_GL_SCOPE();

	FOpenGLTextureBase* Texture = GetOpenGLTextureFromRHITexture(SurfaceRHI);

	if ( FOpenGL::SupportsGenerateMipmap())
	{
		GPUProfilingData.RegisterGPUWork(0);

		FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
		// Setup the texture on a disused unit
		// need to figure out how to setup mips properly in no views case
		CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Texture->Target, Texture->Resource, -1, 1);

		FOpenGL::GenerateMipmap( Texture->Target);
	}
	else
	{
		UE_LOG( LogRHI, Fatal, TEXT("Generate Mipmaps unsupported on this OpenGL version"));
	}
}

/**
 * Computes the size in memory required by a given texture.
 *
 * @param	TextureRHI		- Texture we want to know the size of
 * @return					- Size in Bytes
 */
uint32 FOpenGLDynamicRHI::RHIComputeMemorySize(FTextureRHIParamRef TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}

	FOpenGLTextureBase* Texture = GetOpenGLTextureFromRHITexture(TextureRHI);
	return Texture->GetMemorySize();
}

/**
 * Starts an asynchronous texture reallocation. It may complete immediately if the reallocation
 * could be performed without any reshuffling of texture memory, or if there isn't enough memory.
 * The specified status counter will be decremented by 1 when the reallocation is complete (success or failure).
 *
 * Returns a new reference to the texture, which will represent the new mip count when the reallocation is complete.
 * RHIGetAsyncReallocateTexture2DStatus() can be used to check the status of an ongoing or completed reallocation.
 *
 * @param Texture2D		- Texture to reallocate
 * @param NewMipCount	- New number of mip-levels
 * @param RequestStatus	- Will be decremented by 1 when the reallocation is complete (success or failure).
 * @return				- New reference to the texture, or an invalid reference upon failure
 */
FTexture2DRHIRef FOpenGLDynamicRHI::RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2DRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	VERIFY_GL_SCOPE();

	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,Texture2D);

	// Allocate a new texture.
	FOpenGLTexture2D* NewTexture2D = (FOpenGLTexture2D*)CreateOpenGLTexture(NewSizeX,NewSizeY,false,false, Texture2D->GetFormat(),NewMipCount,1,1, Texture2D->GetFlags());
	
	const uint32 BlockSizeX = GPixelFormats[Texture2D->GetFormat()].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[Texture2D->GetFormat()].BlockSizeY;
	const uint32 NumBytesPerBlock = GPixelFormats[Texture2D->GetFormat()].BlockBytes;

	// Use the GPU to asynchronously copy the old mip-maps into the new texture.
	const uint32 NumSharedMips = FMath::Min(Texture2D->GetNumMips(),NewTexture2D->GetNumMips());
	const uint32 SourceMipOffset = Texture2D->GetNumMips()    - NumSharedMips;
	const uint32 DestMipOffset   = NewTexture2D->GetNumMips() - NumSharedMips;

	if (FOpenGL::SupportsCopyImage())
	{
		FOpenGLTexture2D *NewOGLTexture2D = (FOpenGLTexture2D*)NewTexture2D;
		FOpenGLTexture2D *OGLTexture2D = (FOpenGLTexture2D*)Texture2D;
		NewOGLTexture2D->CloneViaCopyImage( OGLTexture2D, NumSharedMips, SourceMipOffset, DestMipOffset);
	}
	else if (FOpenGL::SupportsCopyTextureLevels())
	{
		FOpenGL::CopyTextureLevels( NewTexture2D->Resource, Texture2D->Resource, SourceMipOffset, NumSharedMips);
	}
	else if (FOpenGL::SupportsPixelBuffers())
	{
		FOpenGLTexture2D *NewOGLTexture2D = (FOpenGLTexture2D*)NewTexture2D;
		FOpenGLTexture2D *OGLTexture2D = (FOpenGLTexture2D*)Texture2D;
		NewOGLTexture2D->CloneViaPBO( OGLTexture2D, NumSharedMips, SourceMipOffset, DestMipOffset);
	}
	else
	{
		for(uint32 MipIndex = 0;MipIndex < NumSharedMips;++MipIndex)
		{
			const uint32 MipSizeX = FMath::Max<uint32>(1,NewSizeX >> (MipIndex+DestMipOffset));
			const uint32 MipSizeY = FMath::Max<uint32>(1,NewSizeY >> (MipIndex+DestMipOffset));
			const uint32 NumMipBlocks = Align(MipSizeX,BlockSizeX) / BlockSizeX * Align(MipSizeY,BlockSizeY) / BlockSizeY;

			// Lock old and new texture.
			uint32 SrcStride;
			uint32 DestStride;

			void* Src = RHILockTexture2D( Texture2D, MipIndex + SourceMipOffset, RLM_ReadOnly, SrcStride, false );
			void* Dst = RHILockTexture2D( NewTexture2D, MipIndex + DestMipOffset, RLM_WriteOnly, DestStride, false );
			check(SrcStride == DestStride);
			FMemory::Memcpy( Dst, Src, NumMipBlocks * NumBytesPerBlock );
			RHIUnlockTexture2D( Texture2D, MipIndex + SourceMipOffset, false );
			RHIUnlockTexture2D( NewTexture2D, MipIndex + DestMipOffset, false );
		}
	}

	// Decrement the thread-safe counter used to track the completion of the reallocation, since D3D handles sequencing the
	// async mip copies with other D3D calls.
	RequestStatus->Decrement();

	return NewTexture2D;
}

/**
 * Returns the status of an ongoing or completed texture reallocation:
 *	TexRealloc_Succeeded	- The texture is ok, reallocation is not in progress.
 *	TexRealloc_Failed		- The texture is bad, reallocation is not in progress.
 *	TexRealloc_InProgress	- The texture is currently being reallocated async.
 *
 * @param Texture2D		- Texture to check the reallocation status for
 * @return				- Current reallocation status
 */
ETextureReallocationStatus FOpenGLDynamicRHI::RHIFinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Succeeded;
}

/**
 * Cancels an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to cancel
 * @param bBlockUntilCompleted	If true, blocks until the cancellation is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus FOpenGLDynamicRHI::RHICancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Succeeded;
}

void* FOpenGLDynamicRHI::RHILockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,Texture);
	return Texture->Lock(MipIndex,0,LockMode,DestStride);
}

void FOpenGLDynamicRHI::RHIUnlockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,bool bLockWithinMiptail)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,Texture);
	Texture->Unlock(MipIndex, 0);
}

void* FOpenGLDynamicRHI::RHILockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture2DArray,Texture);
	return Texture->Lock(MipIndex,TextureIndex,LockMode,DestStride);
}

void FOpenGLDynamicRHI::RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Texture2DArray,Texture);
	Texture->Unlock(MipIndex, TextureIndex);
}

void FOpenGLDynamicRHI::RHIUpdateTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,const FUpdateTextureRegion2D& UpdateRegion,uint32 SourcePitch,const uint8* SourceData)
{
	VERIFY_GL_SCOPE();
	check( FOpenGL::SupportsPixelBuffers() );

	DYNAMIC_CAST_OPENGLRESOURCE(Texture2D,Texture);

	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
	CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Texture->Target, Texture->Resource, 0, Texture->GetNumMips());
	CachedBindPixelUnpackBuffer(ContextState, 0);

	EPixelFormat PixelFormat = Texture->GetFormat();
	check(GPixelFormats[PixelFormat].BlockSizeX == 1);
	check(GPixelFormats[PixelFormat].BlockSizeY == 1);
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
	const uint32 FormatBPP = GPixelFormats[PixelFormat].BlockBytes;
	checkf(!GLFormat.bCompressed, TEXT("RHIUpdateTexture2D not currently supported for compressed (%s) textures by the OpenGL RHI"), GPixelFormats[PixelFormat].Name);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, SourcePitch / FormatBPP);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(Texture->Target, MipIndex, UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.Width, UpdateRegion.Height,
		GLFormat.Format, GLFormat.Type, SourceData);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.
}

void FOpenGLDynamicRHI::RHIUpdateTexture3D(FTexture3DRHIParamRef TextureRHI,uint32 MipIndex,const FUpdateTextureRegion3D& UpdateRegion,uint32 SourceRowPitch,uint32 SourceDepthPitch,const uint8* SourceData)
{
	VERIFY_GL_SCOPE();
	check( FOpenGL::SupportsPixelBuffers() && FOpenGL::SupportsTexture3D() );
	DYNAMIC_CAST_OPENGLRESOURCE(Texture3D,Texture);

	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
	CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Texture->Target, Texture->Resource, 0, Texture->GetNumMips());
	CachedBindPixelUnpackBuffer(ContextState, 0);

	EPixelFormat PixelFormat = Texture->GetFormat();
	check(GPixelFormats[PixelFormat].BlockSizeX == 1);
	check(GPixelFormats[PixelFormat].BlockSizeY == 1);

	// TO DO - add appropriate offsets to source data when necessary
	check(UpdateRegion.SrcX == 0);
	check(UpdateRegion.SrcY == 0);
	check(UpdateRegion.SrcZ == 0);

	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
	const uint32 FormatBPP = GPixelFormats[PixelFormat].BlockBytes;
	checkf(!GLFormat.bCompressed, TEXT("RHIUpdateTexture3D not currently supported for compressed (%s) textures by the OpenGL RHI"), GPixelFormats[PixelFormat].Name);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, SourceRowPitch / FormatBPP);

	check( SourceDepthPitch % ( FormatBPP * UpdateRegion.Width ) == 0 );
	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, SourceDepthPitch / UpdateRegion.Width / FormatBPP);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	FOpenGL::TexSubImage3D(Texture->Target, MipIndex, UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.DestZ, UpdateRegion.Width, UpdateRegion.Height, UpdateRegion.Depth,
		GLFormat.Format, GLFormat.Type, SourceData);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.
}

void FOpenGLDynamicRHI::InvalidateTextureResourceInCache(GLuint Resource)
{
	for (int32 SamplerIndex = 0; SamplerIndex < FOpenGL::GetMaxCombinedTextureImageUnits(); ++SamplerIndex)
	{
		if (SharedContextState.Textures[SamplerIndex].Resource == Resource)
		{
			SharedContextState.Textures[SamplerIndex].Target = GL_NONE;
			SharedContextState.Textures[SamplerIndex].Resource = 0;
		}

		if (RenderingContextState.Textures[SamplerIndex].Resource == Resource)
		{
			RenderingContextState.Textures[SamplerIndex].Target = GL_NONE;
			RenderingContextState.Textures[SamplerIndex].Resource = 0;
		}
	}
}

void FOpenGLDynamicRHI::InvalidateUAVResourceInCache(GLuint Resource)
{
	for (int32 UAVIndex = 0; UAVIndex < OGL_MAX_COMPUTE_STAGE_UAV_UNITS; ++UAVIndex)
	{
		if (SharedContextState.UAVs[UAVIndex].Resource == Resource)
		{
			SharedContextState.UAVs[UAVIndex].Format = GL_NONE;
			SharedContextState.UAVs[UAVIndex].Resource = 0;
		}

		if (RenderingContextState.UAVs[UAVIndex].Resource == Resource)
		{
			RenderingContextState.UAVs[UAVIndex].Format = GL_NONE;
			RenderingContextState.UAVs[UAVIndex].Resource = 0;
		}
	}
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/
FTextureCubeRHIRef FOpenGLDynamicRHI::RHICreateTextureCube( uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData )
{
	return (FRHITextureCube*)CreateOpenGLTexture(Size,Size,true, false, Format, NumMips, 1, 1, Flags);
}

FTextureCubeRHIRef FOpenGLDynamicRHI::RHICreateTextureCubeArray( uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData )
{
	return (FRHITextureCube*)CreateOpenGLTexture(Size,Size,true, true, Format, NumMips, 1, 6 * ArraySize, Flags);
}

void* FOpenGLDynamicRHI::RHILockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	DYNAMIC_CAST_OPENGLRESOURCE(TextureCube,TextureCube);
	return TextureCube->Lock(MipIndex,FaceIndex + 6 * ArrayIndex,LockMode,DestStride);
}

void FOpenGLDynamicRHI::RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	DYNAMIC_CAST_OPENGLRESOURCE(TextureCube,TextureCube);
	TextureCube->Unlock(MipIndex,FaceIndex + ArrayIndex * 6);
}

void FOpenGLDynamicRHI::RHIBindDebugLabelName(FTextureRHIParamRef TextureRHI, const TCHAR* Name)
{
	FOpenGLTextureBase* Texture = GetOpenGLTextureFromRHITexture(TextureRHI);
	FOpenGL::LabelObject(GL_TEXTURE, Texture->Resource, TCHAR_TO_ANSI(Name));
}

void FOpenGLDynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
}

void FOpenGLDynamicRHI::RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
}
