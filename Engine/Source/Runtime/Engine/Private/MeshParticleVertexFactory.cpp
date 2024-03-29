// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshParticleVertexFactory.cpp: Mesh particle vertex factory implementation
=============================================================================*/

#include "EnginePrivate.h"
#include "ParticleDefinitions.h"
#include "MeshParticleVertexFactory.h"


class FMeshParticleVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:

	virtual void Bind(const FShaderParameterMap& ParameterMap) OVERRIDE
	{
		Transform1.Bind(ParameterMap,TEXT("Transform1"));
		Transform2.Bind(ParameterMap,TEXT("Transform2"));
		Transform3.Bind(ParameterMap,TEXT("Transform3"));
		SubUVParams.Bind(ParameterMap,TEXT("SubUVParams"));
		SubUVLerp.Bind(ParameterMap,TEXT("SubUVLerp"));
		DynamicParameter.Bind(ParameterMap,TEXT("DynamicParameter"));
		ParticleColor.Bind(ParameterMap,TEXT("ParticleColor"));
	}

	virtual void Serialize(FArchive& Ar) OVERRIDE
	{
		Ar << Transform1;
		Ar << Transform2;
		Ar << Transform3;
		Ar << SubUVParams;
		Ar << SubUVLerp;
		Ar << DynamicParameter;
		Ar << ParticleColor;
	}

	virtual void SetMesh(FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const OVERRIDE
	{
		const bool bInstanced = GRHIFeatureLevel >= ERHIFeatureLevel::SM3;
		FMeshParticleVertexFactory* MeshParticleVF = (FMeshParticleVertexFactory*)VertexFactory;
		FVertexShaderRHIParamRef VertexShaderRHI = Shader->GetVertexShader();
		SetUniformBufferParameter( VertexShaderRHI, Shader->GetUniformBufferParameter<FMeshParticleUniformParameters>(), MeshParticleVF->GetUniformBuffer() );

		if (!bInstanced)
		{
			FMeshParticleVertexFactory::FBatchParametersCPU* BatchParameters = (FMeshParticleVertexFactory::FBatchParametersCPU*)BatchElement.UserData;
			const FMeshParticleInstanceVertex* Vertex = BatchParameters->InstanceBuffer + BatchElement.UserIndex;
			const FMeshParticleInstanceVertexDynamicParameter* DynamicVertex = BatchParameters->DynamicParameterBuffer + BatchElement.UserIndex;

			SetShaderValue(VertexShaderRHI, Transform1, Vertex->Transform[0]);
			SetShaderValue(VertexShaderRHI, Transform2, Vertex->Transform[1]);
			SetShaderValue(VertexShaderRHI, Transform3, Vertex->Transform[2]);
			SetShaderValue(VertexShaderRHI, SubUVParams, FVector4((float)Vertex->SubUVParams[0], (float)Vertex->SubUVParams[1], (float)Vertex->SubUVParams[2], (float)Vertex->SubUVParams[3]));
			SetShaderValue(VertexShaderRHI, SubUVLerp, Vertex->SubUVLerp);
			SetShaderValue(VertexShaderRHI, DynamicParameter, FVector4(DynamicVertex->DynamicValue[0], DynamicVertex->DynamicValue[1], DynamicVertex->DynamicValue[2], DynamicVertex->DynamicValue[3]));
			SetShaderValue(VertexShaderRHI, ParticleColor, FVector4(Vertex->Color.Component(0), Vertex->Color.Component(1), Vertex->Color.Component(2), Vertex->Color.Component(3)));
		}
	}

private:
	// Used only when instancing is off (ES2)
	FShaderParameter Transform1;
	FShaderParameter Transform2;
	FShaderParameter Transform3;
	FShaderParameter SubUVParams;
	FShaderParameter SubUVLerp;
	FShaderParameter DynamicParameter;
	FShaderParameter ParticleColor;
};


void FMeshParticleVertexFactory::InitRHI()
{
	FVertexDeclarationElementList Elements;

	const bool bInstanced = GRHIFeatureLevel >= ERHIFeatureLevel::SM3;

	if (Data.bInitialized)
	{
		if(bInstanced)
		{
			// Stream 0 - Instance data
			{
				FVertexStream VertexStream;
				VertexStream.VertexBuffer = NULL;
				VertexStream.Stride = 0;
				VertexStream.Offset = 0;
				Streams.Add(VertexStream);
	
				Elements.Add(FVertexElement(0, Data.TransformComponent[0].Offset, Data.TransformComponent[0].Type, 8, Data.TransformComponent[0].bUseInstanceIndex));
				Elements.Add(FVertexElement(0, Data.TransformComponent[1].Offset, Data.TransformComponent[1].Type, 9, Data.TransformComponent[1].bUseInstanceIndex));
				Elements.Add(FVertexElement(0, Data.TransformComponent[2].Offset, Data.TransformComponent[2].Type, 10, Data.TransformComponent[2].bUseInstanceIndex));
	
				Elements.Add(FVertexElement(0, Data.SubUVs.Offset, Data.SubUVs.Type, 11, Data.SubUVs.bUseInstanceIndex));
				Elements.Add(FVertexElement(0, Data.SubUVLerpAndRelTime.Offset, Data.SubUVLerpAndRelTime.Type, 12, Data.SubUVLerpAndRelTime.bUseInstanceIndex));
	
				Elements.Add(FVertexElement(0, Data.ParticleColorComponent.Offset, Data.ParticleColorComponent.Type, 14, Data.ParticleColorComponent.bUseInstanceIndex));
				Elements.Add(FVertexElement(0, Data.VelocityComponent.Offset, Data.VelocityComponent.Type, 15, Data.VelocityComponent.bUseInstanceIndex));
			}

			// Stream 1 - Dynamic parameter
			{
				FVertexStream VertexStream;
				VertexStream.VertexBuffer = NULL;
				VertexStream.Stride = 0;
				VertexStream.Offset = 0;
				Streams.Add(VertexStream);
	
				Elements.Add(FVertexElement(1, 0, VET_Float4, 13, true));
			}
		}

		if(Data.PositionComponent.VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(Data.PositionComponent,0));
		}

		// only tangent,normal are used by the stream. the binormal is derived in the shader
		uint8 TangentBasisAttributes[2] = { 1, 2 };
		for(int32 AxisIndex = 0;AxisIndex < 2;AxisIndex++)
		{
			if(Data.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
			{
				Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex],TangentBasisAttributes[AxisIndex]));
			}
		}

		// Vertex color
		if(Data.VertexColorComponent.VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(Data.VertexColorComponent,3));
		}
		else
		{
			//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
			//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
			FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
			Elements.Add(AccessStreamComponent(NullColorComponent,3));
		}
		
		if(Data.TextureCoordinates.Num())
		{
			const int32 BaseTexCoordAttribute = 4;
			for(int32 CoordinateIndex = 0;CoordinateIndex < Data.TextureCoordinates.Num();CoordinateIndex++)
			{
				Elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[CoordinateIndex],
					BaseTexCoordAttribute + CoordinateIndex
					));
			}

			for(int32 CoordinateIndex = Data.TextureCoordinates.Num();CoordinateIndex < MAX_TEXCOORDS;CoordinateIndex++)
			{
				Elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
					BaseTexCoordAttribute + CoordinateIndex
					));
			}
		}

		if(Streams.Num() > 0)
		{
			InitDeclaration(Elements,Data);
			check(IsValidRef(GetDeclaration()));
		}
	}
}

void FMeshParticleVertexFactory::SetInstanceBuffer(const FVertexBuffer* InstanceBuffer, uint32 StreamOffset, uint32 Stride)
{
	Streams[0].VertexBuffer = InstanceBuffer;
	Streams[0].Offset = StreamOffset;
	Streams[0].Stride = Stride;
}

void FMeshParticleVertexFactory::SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, uint32 StreamOffset, uint32 Stride)
{
	if (InDynamicParameterBuffer)
	{
		Streams[1].VertexBuffer = InDynamicParameterBuffer;
		Streams[1].Offset = StreamOffset;
		Streams[1].Stride = Stride;
	}
	else
	{
		Streams[1].VertexBuffer = &GNullDynamicParameterVertexBuffer;
		Streams[1].Offset = 0;
		Streams[1].Stride = 0;
	}
}

bool FMeshParticleVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Material->IsUsedWithMeshParticles() || Material->IsSpecialEngineMaterial());
}

void FMeshParticleVertexFactory::SetData(const DataType& InData)
{
	check(IsInRenderingThread());
	Data = InData;
	UpdateRHI();
}


FVertexFactoryShaderParameters* FMeshParticleVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FMeshParticleVertexFactoryShaderParameters() : NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FMeshParticleVertexFactory,"MeshParticleVertexFactory",true,false,true,false,false);
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FMeshParticleUniformParameters,TEXT("MeshParticleVF"));