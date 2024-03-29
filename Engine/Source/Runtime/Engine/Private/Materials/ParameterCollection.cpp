// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ParameterCollection.h"

UMaterialParameterCollection::UMaterialParameterCollection(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{}

void UMaterialParameterCollection::PostLoad()
{
	Super::PostLoad();
	
	if (!StateId.IsValid())
	{
		StateId = FGuid::NewGuid();
	}

	CreateBufferStruct();

	// Create an instance for this collection in every world
	for (TObjectIterator<UWorld> It; It; ++It)
	{
		UWorld* CurrentWorld = *It;
		CurrentWorld->AddParameterCollectionInstance(this, true);
	}
}

#if WITH_EDITOR

int32 PreviousScalarCount = 0;
int32 PreviousVectorCount = 0;

void UMaterialParameterCollection::PreEditChange(class FEditPropertyChain& PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	PreviousScalarCount = ScalarParameters.Num();
	PreviousVectorCount = VectorParameters.Num();
}

void UMaterialParameterCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// If the array counts have changed, an element has been added or removed, and we need to update the uniform buffer layout,
	// Which also requires recompiling any referencing materials
	if (ScalarParameters.Num() != PreviousScalarCount
		|| VectorParameters.Num() != PreviousVectorCount)
	{
		// Limit the count of parameters to fit within uniform buffer limits
		const uint32 MaxScalarParameters = 1024;

		if (ScalarParameters.Num() > MaxScalarParameters)
		{
			ScalarParameters.RemoveAt(MaxScalarParameters, ScalarParameters.Num() - MaxScalarParameters);
		}

		const uint32 MaxVectorParameters = 1024;

		if (VectorParameters.Num() > MaxVectorParameters)
		{
			VectorParameters.RemoveAt(MaxVectorParameters, VectorParameters.Num() - MaxVectorParameters);
		}

		// Generate a new Id so that unloaded materials that reference this collection will update correctly on load
		StateId = FGuid::NewGuid();

		// Update the uniform buffer layout
		CreateBufferStruct();

		// Recreate each instance of this collection
		for (TObjectIterator<UWorld> It; It; ++It)
		{
			UWorld* CurrentWorld = *It;
			CurrentWorld->AddParameterCollectionInstance(this, false);
		}

		// Create a material update context so we can safely update materials using this parameter collection.
		{
			FMaterialUpdateContext UpdateContext;

			// Go through all materials in memory and recompile them if they use this material parameter collection
			for (TObjectIterator<UMaterial> It; It; ++It)
			{
				UMaterial* CurrentMaterial = *It;

				bool bRecompile = false;

				// Preview materials often use expressions for rendering that are not in their Expressions array, 
				// And therefore their MaterialParameterCollectionInfos are not up to date.
				if (CurrentMaterial->bIsPreviewMaterial)
				{
					bRecompile = true;
				}
				else
				{
					for (int32 FunctionIndex = 0; FunctionIndex < CurrentMaterial->MaterialParameterCollectionInfos.Num(); FunctionIndex++)
					{
						if (CurrentMaterial->MaterialParameterCollectionInfos[FunctionIndex].ParameterCollection == this)
						{
							bRecompile = true;
							break;
						}
					}
				}

				if (bRecompile)
				{
					UpdateContext.AddMaterial(CurrentMaterial);

					// Propagate the change to this material
					CurrentMaterial->PreEditChange(NULL);
					CurrentMaterial->PostEditChange();
					CurrentMaterial->MarkPackageDirty();
				}
			}
		}
	}

	// Update each world's scene with the new instance, and update each instance's uniform buffer to reflect the changes made by the user
	for (TObjectIterator<UWorld> It; It; ++It)
	{
		UWorld* CurrentWorld = *It;
		CurrentWorld->UpdateParameterCollectionInstances(true);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
 * Helper function for creating unique item names within a list of existing items
 *   InBaseName - Desired name prefix (will generate Prefix<N>)
 *   InExistingItems - Array of existing items which we want to ensure uniqueness within
 *   OutName - Target FName for result
 *   InNewIndex - Index of value that has just been added, so we can make sure we don't check against ourselves
 **/
template <class T>
inline void CreateUniqueName(const TCHAR* InBaseName, TArray<T>& InExistingItems, FName& OutName, int32 InNewIndex)
{
	int32 Index = 0;
	bool bMatchFound = true;

	while (bMatchFound)
	{
		bMatchFound = false;
		OutName = FName(*FString::Printf(TEXT("%s%u"), InBaseName, Index++));

		for (int32 i = 0; i < InExistingItems.Num(); ++i)
		{
			if (i != InNewIndex && InExistingItems[i].ParameterName == OutName)
			{
				bMatchFound = true;
				break;
			}
		}
	}
}

void UMaterialParameterCollection::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// Auto-populate with useful parameter names
	if (ScalarParameters.Num() > PreviousScalarCount)
	{
		int32 NewArrayIndex = PropertyChangedEvent.GetArrayIndex("ScalarParameters");

		if (ScalarParameters.IsValidIndex(NewArrayIndex))
		{
			CreateUniqueName<FCollectionScalarParameter>(TEXT("Scalar"), ScalarParameters, ScalarParameters[NewArrayIndex].ParameterName, NewArrayIndex);
		}
	}

	if (VectorParameters.Num() > PreviousVectorCount)
	{
		int32 NewArrayIndex = PropertyChangedEvent.GetArrayIndex("VectorParameters");

		if (VectorParameters.IsValidIndex(NewArrayIndex))
		{
			CreateUniqueName<FCollectionVectorParameter>(TEXT("Vector"), VectorParameters, VectorParameters[NewArrayIndex].ParameterName, NewArrayIndex);
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

FName UMaterialParameterCollection::GetParameterName(const FGuid& Id) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			return Parameter.ParameterName;
		}
	}

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			return Parameter.ParameterName;
		}
	}

	return NAME_None;
}

FGuid UMaterialParameterCollection::GetParameterId(FName ParameterName) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return Parameter.Id;
		}
	}

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return Parameter.Id;
		}
	}

	return FGuid();
}

void UMaterialParameterCollection::GetParameterIndex(const FGuid& Id, int32& OutIndex, int32& OutComponentIndex) const
{
	// The parameter and component index allocated in this function must match the memory layout in UMaterialParameterCollectionInstance::GetParameterData

	OutIndex = -1;
	OutComponentIndex = -1;

	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			// Scalar parameters are packed into float4's
			OutIndex = ParameterIndex / 4;
			OutComponentIndex = ParameterIndex % 4;
			break;
		}
	}

	const int32 VectorParameterBase = FMath::DivideAndRoundUp(ScalarParameters.Num(), 4);

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			OutIndex = ParameterIndex + VectorParameterBase;
			break;
		}
	}
}

void UMaterialParameterCollection::GetParameterNames(TArray<FName>& OutParameterNames, bool bVectorParameters) const
{
	if (bVectorParameters)
	{
		for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
		{
			const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];
			OutParameterNames.Add(Parameter.ParameterName);
		}
	}
	else
	{
		for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
		{
			const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];
			OutParameterNames.Add(Parameter.ParameterName);
		}
	}
}

const FCollectionScalarParameter* UMaterialParameterCollection::GetScalarParameterByName(FName ParameterName) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return &Parameter;
		}
	}

	return NULL;
}

const FCollectionVectorParameter* UMaterialParameterCollection::GetVectorParameterByName(FName ParameterName) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return &Parameter;
		}
	}

	return NULL;
}

FShaderUniformBufferParameter* ConstructCollectionUniformBufferParameter() { return NULL; }

void UMaterialParameterCollection::CreateBufferStruct()
{	
	TArray<FUniformBufferStruct::FMember> Members;
	uint32 NextMemberOffset = 0;

	const uint32 NumVectors = FMath::DivideAndRoundUp(ScalarParameters.Num(), 4) + VectorParameters.Num();
	new(Members) FUniformBufferStruct::FMember(TEXT("Vectors"),NextMemberOffset,UBMT_FLOAT32,EShaderPrecisionModifier::Half,1,4,NumVectors,NULL);
	const uint32 VectorArraySize = NumVectors * sizeof(FVector4);
	NextMemberOffset += VectorArraySize;

	const uint32 StructSize = Align(NextMemberOffset,UNIFORM_BUFFER_STRUCT_ALIGNMENT);
	UniformBufferStruct = new FUniformBufferStruct(
		TEXT("MaterialCollection"),
		TEXT("MaterialCollection"),
		ConstructCollectionUniformBufferParameter,
		StructSize,
		Members,
		false
		);
}

UMaterialParameterCollectionInstance::UMaterialParameterCollectionInstance(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	Resource = NULL;
}

void UMaterialParameterCollectionInstance::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		Resource = new FMaterialParameterCollectionInstanceResource();
	}
}

void UMaterialParameterCollectionInstance::SetCollection(UMaterialParameterCollection* InCollection, UWorld* InWorld)
{
	Collection = InCollection;
	World = InWorld;

	UpdateRenderState();
}

bool UMaterialParameterCollectionInstance::SetScalarParameterValue(FName ParameterName, float ParameterValue)
{
	check(World && Collection);

	if (Collection->GetScalarParameterByName(ParameterName))
	{
		float* ExistingValue = ScalarParameterValues.Find(ParameterName);
		bool bUpdateUniformBuffer = false;

		if (ExistingValue && *ExistingValue != ParameterValue)
		{
			// Update the existing instance override if the new value is different
			bUpdateUniformBuffer = true;
			*ExistingValue = ParameterValue;
		}
		else if (!ExistingValue)
		{
			// Add a new instance override
			bUpdateUniformBuffer = true;
			ScalarParameterValues.Add(ParameterName, ParameterValue);
		}

		if (bUpdateUniformBuffer)
		{
			//@todo - only update uniform buffers max once per frame
			UpdateRenderState();
		}

		return true;
	}

	return false;
}

bool UMaterialParameterCollectionInstance::SetVectorParameterValue(FName ParameterName, const FLinearColor& ParameterValue)
{
	check(World && Collection);

	if (Collection->GetVectorParameterByName(ParameterName))
	{
		FLinearColor* ExistingValue = VectorParameterValues.Find(ParameterName);
		bool bUpdateUniformBuffer = false;

		if (ExistingValue && *ExistingValue != ParameterValue)
		{
			// Update the existing instance override if the new value is different
			bUpdateUniformBuffer = true;
			*ExistingValue = ParameterValue;
		}
		else if (!ExistingValue)
		{
			// Add a new instance override
			bUpdateUniformBuffer = true;
			VectorParameterValues.Add(ParameterName, ParameterValue);
		}

		if (bUpdateUniformBuffer)
		{
			//@todo - only update uniform buffers max once per frame
			UpdateRenderState();
		}

		return true;
	}

	return false;
}

bool UMaterialParameterCollectionInstance::GetScalarParameterValue(FName ParameterName, float& OutParameterValue) const
{
	const FCollectionScalarParameter* Parameter = Collection->GetScalarParameterByName(ParameterName);

	if (Parameter)
	{
		const float* InstanceValue = ScalarParameterValues.Find(ParameterName);
		OutParameterValue = InstanceValue != NULL ? *InstanceValue : Parameter->DefaultValue;
		return true;
	}

	return false;
}

bool UMaterialParameterCollectionInstance::GetVectorParameterValue(FName ParameterName, FLinearColor& OutParameterValue) const
{
	const FCollectionVectorParameter* Parameter = Collection->GetVectorParameterByName(ParameterName);

	if (Parameter)
	{
		const FLinearColor* InstanceValue = VectorParameterValues.Find(ParameterName);
		OutParameterValue = InstanceValue != NULL ? *InstanceValue : Parameter->DefaultValue;
		return true;
	}

	return false;
}

void UMaterialParameterCollectionInstance::UpdateRenderState()
{
	// Propagate the new values to the rendering thread
	TArray<FVector4> ParameterData;
	GetParameterData(ParameterData);
	Resource->GameThread_UpdateContents(Collection ? Collection->StateId : FGuid(), ParameterData);
	// Update the world's scene with the new uniform buffer pointer
	World->UpdateParameterCollectionInstances(false);
}

void UMaterialParameterCollectionInstance::GetParameterData(TArray<FVector4>& ParameterData) const
{
	// The memory layout created here must match the index assignment in UMaterialParameterCollection::GetParameterIndex

	if (Collection)
	{
		ParameterData.Empty(FMath::DivideAndRoundUp(Collection->ScalarParameters.Num(), 4) + Collection->VectorParameters.Num());

		for (int32 ParameterIndex = 0; ParameterIndex < Collection->ScalarParameters.Num(); ParameterIndex++)
		{
			const FCollectionScalarParameter& Parameter = Collection->ScalarParameters[ParameterIndex];

			// Add a new vector for each packed vector
			if (ParameterIndex % 4 == 0)
			{
				ParameterData.Add(FVector4(0, 0, 0, 0));
			}

			FVector4& CurrentVector = ParameterData.Last();
			const float* InstanceData = ScalarParameterValues.Find(Parameter.ParameterName);
			// Pack into the appropriate component of this packed vector
			CurrentVector[ParameterIndex % 4] = InstanceData ? *InstanceData : Parameter.DefaultValue;
		}

		for (int32 ParameterIndex = 0; ParameterIndex < Collection->VectorParameters.Num(); ParameterIndex++)
		{
			const FCollectionVectorParameter& Parameter = Collection->VectorParameters[ParameterIndex];
			const FLinearColor* InstanceData = VectorParameterValues.Find(Parameter.ParameterName);
			ParameterData.Add(InstanceData ? *InstanceData : Parameter.DefaultValue);
		}
	}
}

void UMaterialParameterCollectionInstance::FinishDestroy()
{
	if (Resource)
	{
		Resource->GameThread_Destroy();
		Resource = NULL;
	}

	Super::FinishDestroy();
}

void FMaterialParameterCollectionInstanceResource::GameThread_UpdateContents(const FGuid& InId, const TArray<FVector4>& Data)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		FUpdateCollectionCommand,
		FGuid,Id,InId,
		TArray<FVector4>,Data,Data,
		FMaterialParameterCollectionInstanceResource*,Resource,this,
	{
		Resource->UpdateContents(Id, Data);
	});
}

void FMaterialParameterCollectionInstanceResource::GameThread_Destroy()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FDestroyCollectionCommand,
		FMaterialParameterCollectionInstanceResource*,Resource,this,
	{
		delete Resource;
	});
}

FMaterialParameterCollectionInstanceResource::~FMaterialParameterCollectionInstanceResource()
{
	check(IsInRenderingThread());
	UniformBuffer.SafeRelease();
}

void FMaterialParameterCollectionInstanceResource::UpdateContents(const FGuid& InId, const TArray<FVector4>& Data)
{
	UniformBuffer.SafeRelease();

	Id = InId;

	if (InId != FGuid() && Data.Num() > 0)
	{
		UniformBuffer = RHICreateUniformBuffer(Data.GetData(), Data.GetTypeSize() * Data.Num(), UniformBuffer_MultiUse);
	}
}