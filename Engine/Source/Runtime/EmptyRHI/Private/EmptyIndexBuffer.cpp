// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyIndexBuffer.cpp: Empty Index buffer RHI implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"


/** Constructor */
FEmptyIndexBuffer::FEmptyIndexBuffer(uint32 InStride, uint32 InSize, uint32 InUsage)
	: FRHIIndexBuffer(InStride, InSize, InUsage)
{
}

void* FEmptyIndexBuffer::Lock(EResourceLockMode LockMode, uint32 Size)
{
	return NULL;
}

void FEmptyIndexBuffer::Unlock()
{

}

FIndexBufferRHIRef FEmptyDynamicRHI::RHICreateIndexBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 InUsage)
{
	// make the RHI object, which will allocate memory
	FEmptyIndexBuffer* IndexBuffer = new FEmptyIndexBuffer(Stride, Size, InUsage);
	
	if(ResourceArray)
	{
		check(Size == ResourceArray->GetResourceDataSize());

		// make a buffer usable by CPU
		void* Buffer = RHILockIndexBuffer(IndexBuffer, 0, Size, RLM_WriteOnly);

		// copy the contents of the given data into the buffer
		FMemory::Memcpy(Buffer, ResourceArray->GetResourceData(), Size);

		RHIUnlockIndexBuffer(IndexBuffer);

		// Discard the resource array's contents.
		ResourceArray->Discard();
	}

	return IndexBuffer;
}

void* FEmptyDynamicRHI::RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	DYNAMIC_CAST_EMPTYRESOURCE(IndexBuffer,IndexBuffer);

	return (uint8*)IndexBuffer->Lock(LockMode, Size) + Offset;
}

void FEmptyDynamicRHI::RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(IndexBuffer,IndexBuffer);

	IndexBuffer->Unlock();
}
