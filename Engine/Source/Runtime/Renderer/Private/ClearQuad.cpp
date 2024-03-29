// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "RendererPrivate.h"
#include "OneColorShader.h"

FGlobalBoundShaderState GClearMRTBoundShaderState[8];

// TODO support ExcludeRect
void DrawClearQuadMRT(bool bClearColor,int32 NumClearColors,const FLinearColor* ClearColorArray,bool bClearDepth,float Depth,bool bClearStencil,uint32 Stencil)
{
	// Set new states
	FBlendStateRHIParamRef BlendStateRHI;
		
	if (NumClearColors <= 1)
	{
		BlendStateRHI = bClearColor
			? TStaticBlendState<>::GetRHI()
			: TStaticBlendState<CW_NONE>::GetRHI();
	}
	else
	{
		BlendStateRHI = bClearColor
			? TStaticBlendState<>::GetRHI()
			: TStaticBlendStateWriteMask<CW_NONE,CW_NONE,CW_NONE,CW_NONE,CW_NONE,CW_NONE,CW_NONE,CW_NONE>::GetRHI();
	}
		
	const FDepthStencilStateRHIParamRef DepthStencilStateRHI = 
		(bClearDepth && bClearStencil)
			? TStaticDepthStencilState<
				true, CF_Always,
				true,CF_Always,SO_Replace,SO_Replace,SO_Replace,
				false,CF_Always,SO_Replace,SO_Replace,SO_Replace,
				0xff,0xff
				>::GetRHI()
			: bClearDepth
				? TStaticDepthStencilState<true, CF_Always>::GetRHI()
				: bClearStencil
					? TStaticDepthStencilState<
						false, CF_Always,
						true,CF_Always,SO_Replace,SO_Replace,SO_Replace,
						false,CF_Always,SO_Replace,SO_Replace,SO_Replace,
						0xff,0xff
						>::GetRHI()
					: TStaticDepthStencilState<false, CF_Always>::GetRHI();

	RHISetRasterizerState( TStaticRasterizerState<FM_Solid,CM_None>::GetRHI() );
	RHISetBlendState( BlendStateRHI );
	RHISetDepthStencilState( DepthStencilStateRHI );

	// Set the new shaders
	TShaderMapRef<TOneColorVS<true> > VertexShader(GetGlobalShaderMap());

	FOneColorPS* PixelShader = NULL;

	// Set the shader to write to the appropriate number of render targets
	// On AMD PC hardware, outputting to a color index in the shader without a matching render target set has a significant performance hit
	if (NumClearColors <= 1)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<1> > MRTPixelShader(GetGlobalShaderMap());
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 2)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<2> > MRTPixelShader(GetGlobalShaderMap());
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 3)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<3> > MRTPixelShader(GetGlobalShaderMap());
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 4)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<4> > MRTPixelShader(GetGlobalShaderMap());
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 5)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<5> > MRTPixelShader(GetGlobalShaderMap());
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 6)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<6> > MRTPixelShader(GetGlobalShaderMap());
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 7)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<7> > MRTPixelShader(GetGlobalShaderMap());
		PixelShader = *MRTPixelShader;
	}
	else if (NumClearColors == 8)
	{
		TShaderMapRef<TOneColorPixelShaderMRT<8> > MRTPixelShader(GetGlobalShaderMap());
		PixelShader = *MRTPixelShader;
	}

	SetGlobalBoundShaderState(GClearMRTBoundShaderState[FMath::Max(NumClearColors - 1, 0)], GetVertexDeclarationFVector4(), *VertexShader, PixelShader);
	FLinearColor ShaderClearColors[MaxSimultaneousRenderTargets];
	FMemory::MemZero(ShaderClearColors);

	for (int32 i = 0; i < NumClearColors; i++)
	{
		ShaderClearColors[i] = ClearColorArray[i];
	}

	SetShaderValueArray(PixelShader->GetPixelShader(),PixelShader->ColorParameter,ShaderClearColors,NumClearColors);
		
	{
		// Draw a fullscreen quad
		/*if(ExcludeRect.Width() > 0 && ExcludeRect.Height() > 0)
		{
			// with a hole in it (optimization in case the hardware has non constant clear performance)
			FVector4 OuterVertices[4];
			OuterVertices[0].Set( -1.0f,  1.0f, Depth, 1.0f );
			OuterVertices[1].Set(  1.0f,  1.0f, Depth, 1.0f );
			OuterVertices[2].Set(  1.0f, -1.0f, Depth, 1.0f );
			OuterVertices[3].Set( -1.0f, -1.0f, Depth, 1.0f );

			float InvViewWidth = 1.0f / Viewport.Width;
			float InvViewHeight = 1.0f / Viewport.Height;
			FVector4 FractionRect = FVector4(ExcludeRect.Min.X * InvViewWidth, ExcludeRect.Min.Y * InvViewHeight, (ExcludeRect.Max.X - 1) * InvViewWidth, (ExcludeRect.Max.Y - 1) * InvViewHeight);

			FVector4 InnerVertices[4];
			InnerVertices[0].Set( FMath::Lerp(-1.0f,  1.0f, FractionRect.X), FMath::Lerp(1.0f, -1.0f, FractionRect.Y), Depth, 1.0f );
			InnerVertices[1].Set( FMath::Lerp(-1.0f,  1.0f, FractionRect.Z), FMath::Lerp(1.0f, -1.0f, FractionRect.Y), Depth, 1.0f );
			InnerVertices[2].Set( FMath::Lerp(-1.0f,  1.0f, FractionRect.Z), FMath::Lerp(1.0f, -1.0f, FractionRect.W), Depth, 1.0f );
			InnerVertices[3].Set( FMath::Lerp(-1.0f,  1.0f, FractionRect.X), FMath::Lerp(1.0f, -1.0f, FractionRect.W), Depth, 1.0f );
				
			FVector4 Vertices[10];
			Vertices[0] = OuterVertices[0];
			Vertices[1] = InnerVertices[0];
			Vertices[2] = OuterVertices[1];
			Vertices[3] = InnerVertices[1];
			Vertices[4] = OuterVertices[2];
			Vertices[5] = InnerVertices[2];
			Vertices[6] = OuterVertices[3];
			Vertices[7] = InnerVertices[3];
			Vertices[8] = OuterVertices[0];
			Vertices[9] = InnerVertices[0];

			RHIDrawPrimitiveUP(PT_TriangleStrip, 8, Vertices, sizeof(Vertices[0]) );
		}
		else*/
		{
			// without a hole
			FVector4 Vertices[4];
			Vertices[0].Set( -1.0f,  1.0f, Depth, 1.0f );
			Vertices[1].Set(  1.0f,  1.0f, Depth, 1.0f );
			Vertices[2].Set( -1.0f, -1.0f, Depth, 1.0f );
			Vertices[3].Set(  1.0f, -1.0f, Depth, 1.0f );
			RHIDrawPrimitiveUP(PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]) );
		}
	}
}
