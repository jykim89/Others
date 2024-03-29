// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyCommands.cpp: Empty RHI commands implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"


void FEmptyDynamicRHI::RHIGpuTimeBegin(uint32 Hash, bool bCompute)
{
	return;
}

void FEmptyDynamicRHI::RHIGpuTimeEnd(uint32 Hash, bool bCompute)
{
	return;
}


void FEmptyDynamicRHI::RHISetStreamSource(uint32 StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI,uint32 Stride,uint32 Offset)
{
	DYNAMIC_CAST_EMPTYRESOURCE(VertexBuffer,VertexBuffer);

}

void FEmptyDynamicRHI::RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets)
{

}

void FEmptyDynamicRHI::RHISetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(RasterizerState,NewState);

}

void FEmptyDynamicRHI::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(ComputeShader,ComputeShader);

}

void FEmptyDynamicRHI::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) 
{ 

}

void FEmptyDynamicRHI::RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset) 
{ 
	DYNAMIC_CAST_EMPTYRESOURCE(VertexBuffer,ArgumentBuffer);

}

void FEmptyDynamicRHI::RHISetViewport(uint32 MinX,uint32 MinY,float MinZ,uint32 MaxX,uint32 MaxY,float MaxZ)
{

}

void FEmptyDynamicRHI::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) 
{ 

}

void FEmptyDynamicRHI::RHISetScissorRect(bool bEnable,uint32 MinX,uint32 MinY,uint32 MaxX,uint32 MaxY)
{

}

void FEmptyDynamicRHI::RHISetBoundShaderState( FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(BoundShaderState,BoundShaderState);

}


void FEmptyDynamicRHI::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(UnorderedAccessView, UAV);

}

void FEmptyDynamicRHI::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 UAVIndex,FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount)
{
	DYNAMIC_CAST_EMPTYRESOURCE(UnorderedAccessView, UAV);

}


void FEmptyDynamicRHI::RHISetShaderTexture(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderTexture(FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderTexture(FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderTexture(FPixelShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShader, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{

}


void FEmptyDynamicRHI::RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{

}

void FEmptyDynamicRHI::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{

}


void FEmptyDynamicRHI::RHISetShaderSampler(FVertexShaderRHIParamRef VertexShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(SamplerState,NewState);
	DYNAMIC_CAST_EMPTYRESOURCE(VertexShader,VertexShader);

}

void FEmptyDynamicRHI::RHISetShaderSampler(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(SamplerState,NewState);

}

void FEmptyDynamicRHI::RHISetShaderSampler(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(SamplerState,NewState);

}

void FEmptyDynamicRHI::RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(SamplerState,NewState);

}

void FEmptyDynamicRHI::RHISetShaderSampler(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(SamplerState,NewState);

}

void FEmptyDynamicRHI::RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(SamplerState,NewState);

}


void FEmptyDynamicRHI::RHISetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{

}

void FEmptyDynamicRHI::RHISetShaderParameter(FHullShaderRHIParamRef HullShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{

}

void FEmptyDynamicRHI::RHISetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{

}

void FEmptyDynamicRHI::RHISetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{

}

void FEmptyDynamicRHI::RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{

}

void FEmptyDynamicRHI::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{

}

void FEmptyDynamicRHI::RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(UniformBuffer,Buffer);
	DYNAMIC_CAST_EMPTYRESOURCE(VertexShader,VertexShader);

}

void FEmptyDynamicRHI::RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(UniformBuffer,Buffer);
}

void FEmptyDynamicRHI::RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(UniformBuffer,Buffer);
}

void FEmptyDynamicRHI::RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(UniformBuffer,Buffer);
}

void FEmptyDynamicRHI::RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(UniformBuffer,Buffer);
}

void FEmptyDynamicRHI::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(UniformBuffer,Buffer);
}


void FEmptyDynamicRHI::RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI, uint32 StencilRef)
{
	DYNAMIC_CAST_EMPTYRESOURCE(DepthStencilState,NewState);

}

void FEmptyDynamicRHI::RHISetBlendState(FBlendStateRHIParamRef NewStateRHI, const FLinearColor& BlendFactor)
{
	DYNAMIC_CAST_EMPTYRESOURCE(BlendState,NewState);

}


void FEmptyDynamicRHI::RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, 
	FTextureRHIParamRef NewDepthStencilTargetRHI, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs)
{

}

void FEmptyDynamicRHI::RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
}


// Occlusion/Timer queries.
void FEmptyDynamicRHI::RHIBeginRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(RenderQuery,Query);

	Query->Begin();
}

void FEmptyDynamicRHI::RHIEndRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(RenderQuery,Query);

	Query->End();
}


void FEmptyDynamicRHI::RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
}

void FEmptyDynamicRHI::RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	DYNAMIC_CAST_EMPTYRESOURCE(VertexBuffer,ArgumentBuffer);

}


void FEmptyDynamicRHI::RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 MinIndex,
	uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	DYNAMIC_CAST_EMPTYRESOURCE(IndexBuffer,IndexBuffer);

}

void FEmptyDynamicRHI::RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	DYNAMIC_CAST_EMPTYRESOURCE(IndexBuffer,IndexBuffer);
	DYNAMIC_CAST_EMPTYRESOURCE(StructuredBuffer,ArgumentsBuffer);

}

void FEmptyDynamicRHI::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FIndexBufferRHIParamRef IndexBufferRHI,FVertexBufferRHIParamRef ArgumentBufferRHI,uint32 ArgumentOffset)
{
	DYNAMIC_CAST_EMPTYRESOURCE(IndexBuffer,IndexBuffer);
	DYNAMIC_CAST_EMPTYRESOURCE(VertexBuffer,ArgumentBuffer);

}


/** Some locally global variables to track the pending primitive information uised in RHIEnd*UP functions */
static void *GPendingDrawPrimitiveUPVertexData = NULL;
static uint32 GPendingNumVertices;
static uint32 GPendingVertexDataStride;

static void *GPendingDrawPrimitiveUPIndexData = NULL;
static uint32 GPendingPrimitiveType;
static uint32 GPendingNumPrimitives;
static uint32 GPendingMinVertexIndex;
static uint32 GPendingIndexDataStride;

void FEmptyDynamicRHI::RHIBeginDrawPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
{
	checkSlow(GPendingDrawPrimitiveUPVertexData == NULL);
//	GPendingDrawPrimitiveUPVertexData = 
	OutVertexData = GPendingDrawPrimitiveUPVertexData;

	GPendingPrimitiveType = PrimitiveType;
	GPendingNumPrimitives = NumPrimitives;
	GPendingNumVertices = NumVertices;
	GPendingVertexDataStride = VertexDataStride;
}


void FEmptyDynamicRHI::RHIEndDrawPrimitiveUP()
{

	// free used mem
	GPendingDrawPrimitiveUPVertexData = NULL;
}

void FEmptyDynamicRHI::RHIBeginDrawIndexedPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
{
	checkSlow(GPendingDrawPrimitiveUPVertexData == NULL);
	checkSlow(GPendingDrawPrimitiveUPIndexData == NULL);

//	GPendingDrawPrimitiveUPVertexData = 
	OutVertexData = GPendingDrawPrimitiveUPVertexData;

//	GPendingDrawPrimitiveUPIndexData = 
	OutIndexData = GPendingDrawPrimitiveUPIndexData;

	GPendingPrimitiveType = PrimitiveType;
	GPendingNumPrimitives = NumPrimitives;
	GPendingMinVertexIndex = MinVertexIndex;
	GPendingIndexDataStride = IndexDataStride;

	GPendingNumVertices = NumVertices;
	GPendingVertexDataStride = VertexDataStride;
}

void FEmptyDynamicRHI::RHIEndDrawIndexedPrimitiveUP()
{

	// free used mem
	GPendingDrawPrimitiveUPIndexData = NULL;
	GPendingDrawPrimitiveUPVertexData = NULL;
}


void FEmptyDynamicRHI::RHIClear(bool bClearColor,const FLinearColor& Color, bool bClearDepth,float Depth, bool bClearStencil,uint32 Stencil, FIntRect ExcludeRect)
{
	FEmptyDynamicRHI::RHIClearMRT(bClearColor, 1, &Color, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
}

void FEmptyDynamicRHI::RHIClearMRT(bool bClearColor,int32 NumClearColors,const FLinearColor* ClearColorArray,bool bClearDepth,float Depth,bool bClearStencil,uint32 Stencil, FIntRect ExcludeRect)
{

}



void FEmptyDynamicRHI::RHISuspendRendering()
{
	// Not supported
}

void FEmptyDynamicRHI::RHIResumeRendering()
{
	// Not supported
}

bool FEmptyDynamicRHI::RHIIsRenderingSuspended()
{
	// Not supported
	return false;
}

void FEmptyDynamicRHI::RHIBlockUntilGPUIdle()
{

}

uint32 FEmptyDynamicRHI::RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

void FEmptyDynamicRHI::RHIAutomaticCacheFlushAfterComputeShader(bool bEnable) 
{

}

void FEmptyDynamicRHI::RHIFlushComputeShaderCache()
{

}
