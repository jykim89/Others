// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "MessageLog.h"
#include "ParticleDefinitions.h"

#define LOCTEXT_NAMESPACE "AnimNotifyState_Trail"

DEFINE_LOG_CATEGORY(LogAnimTrails);

/////////////////////////////////////////////////////
// UAnimNotifyState_Trail

UAnimNotifyState_Trail::UAnimNotifyState_Trail(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	PSTemplate = NULL;
	FirstSocketName = NAME_None;
	SecondSocketName = NAME_None;
	WidthScaleMode = ETrailWidthMode_FromCentre;
	WidthScaleCurve = NAME_None;

#if WITH_EDITORONLY_DATA
	bRenderGeometry = true;
	bRenderSpawnPoints = false;
	bRenderTangents = false;
	bRenderTessellation = false;
#endif // WITH_EDITORONLY_DATA
}

void UAnimNotifyState_Trail::AnimNotifyEventChanged(class USkeletalMeshComponent* MeshComp, class UAnimSequence* AnimSeq, FAnimNotifyEvent * OwnerEvent)
{
}

void UAnimNotifyState_Trail::NotifyBegin(class USkeletalMeshComponent * MeshComp, class UAnimSequence * AnimSeq)
{
	bool bError = ValidateInput(MeshComp);

	TArray<USceneComponent*> Children;
	MeshComp->GetChildrenComponents(false, Children);
	
	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	float Width = 1.0f;
	if (WidthScaleCurve != NAME_None && AnimInst)
	{
		Width = AnimInst->GetCurveValue(WidthScaleCurve);
	}

	bool bFoundExistingTrail = false;
	for (USceneComponent* Comp : Children)
	{
		if (UParticleSystemComponent* ParticleComp = Cast<UParticleSystemComponent>(Comp))
		{
			TArray< FParticleAnimTrailEmitterInstance* > TrailEmitters;
			ParticleComp->GetTrailEmitters(this, TrailEmitters);

			//If there are any trails, ensure the template hasn't been changed. Also destroy the component if there are erros.
			if (TrailEmitters.Num() > 0 && (bError || PSTemplate != ParticleComp->Template))
			{
				//The PSTemplate was changed so we need to destroy this system and create it again with the new template. May be able to just change the template?
				ParticleComp->DestroyComponent();
			}
			else
			{
				for (FParticleAnimTrailEmitterInstance* Trail : TrailEmitters)
				{
					bFoundExistingTrail = true;
					Trail->BeginTrail(this);
					Trail->SetTrailSourceData(FirstSocketName, SecondSocketName, WidthScaleMode, Width);

#if WITH_EDITORONLY_DATA
					Trail->SetTrailDebugData( bRenderGeometry, bRenderSpawnPoints, bRenderTessellation, bRenderTangents);
#endif
				}
			}
		}
	}

	if (!bFoundExistingTrail && !bError)
	{
		if (UParticleSystemComponent* NewParticleComp = UGameplayStatics::SpawnEmitterAttached(PSTemplate, MeshComp))
		{
			TArray< FParticleAnimTrailEmitterInstance* > TrailEmitters;
			NewParticleComp->GetTrailEmitters(this, TrailEmitters, true);

			for (FParticleAnimTrailEmitterInstance* Trail : TrailEmitters)
			{
				Trail->BeginTrail(this);
				Trail->SetTrailSourceData(FirstSocketName, SecondSocketName, WidthScaleMode, Width);

#if WITH_EDITORONLY_DATA
				Trail->SetTrailDebugData(bRenderGeometry, bRenderSpawnPoints, bRenderTessellation, bRenderTangents);
#endif
			}
		}
	}

	Received_NotifyBegin(MeshComp, AnimSeq);
}

void UAnimNotifyState_Trail::NotifyTick(class USkeletalMeshComponent * MeshComp, class UAnimSequence * AnimSeq, float FrameDeltaTime)
{
	bool bError = ValidateInput(MeshComp, true);

	TArray<USceneComponent*> Children;
	MeshComp->GetChildrenComponents(false, Children);

	UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
	float Width = 1.0f;
	if (WidthScaleCurve != NAME_None && AnimInst)
	{
		Width = AnimInst->GetCurveValue(WidthScaleCurve);
	}

	bool bFoundExistingTrail = false;
	for (USceneComponent* Comp : Children)
	{
		if (UParticleSystemComponent* ParticleComp = Cast<UParticleSystemComponent>(Comp))
		{
			TArray< FParticleAnimTrailEmitterInstance* > TrailEmitters;
			ParticleComp->GetTrailEmitters(this, TrailEmitters);
			if (bError && TrailEmitters.Num() > 0)
			{
				Comp->DestroyComponent();
			}
			else
			{
				for (FParticleAnimTrailEmitterInstance* Trail : TrailEmitters)
				{
					Trail->SetTrailSourceData(FirstSocketName, SecondSocketName, WidthScaleMode, Width);

#if WITH_EDITORONLY_DATA
					Trail->SetTrailDebugData(bRenderGeometry, bRenderSpawnPoints, bRenderTessellation, bRenderTangents);
#endif
				}
			}
		}
	}

	Received_NotifyTick(MeshComp, AnimSeq, FrameDeltaTime);
}

void UAnimNotifyState_Trail::NotifyEnd(class USkeletalMeshComponent * MeshComp, class UAnimSequence * AnimSeq)
{
	TArray<USceneComponent*> Children;
	MeshComp->GetChildrenComponents(false, Children);

	bool bFoundExistingTrail = false;
	for (USceneComponent* Comp : Children)
	{
		if (UParticleSystemComponent* ParticleComp = Cast<UParticleSystemComponent>(Comp))
		{
			TArray< FParticleAnimTrailEmitterInstance* > TrailEmitters;
			ParticleComp->GetTrailEmitters(this, TrailEmitters);
			for (FParticleAnimTrailEmitterInstance* Trail : TrailEmitters)
			{
				Trail->EndTrail();
			}
		}
	}

	Received_NotifyEnd(MeshComp, AnimSeq);
}

bool UAnimNotifyState_Trail::ValidateInput(class USkeletalMeshComponent * MeshComp, bool bReportErrors/* =false */)
{
#if WITH_EDITORONLY_DATA
	bool bError = false;
	FText FirstSocketEqualsNoneErrorText = FText::Format( LOCTEXT("NoneFirstSocket", "{0}: Must set First Socket Name."), FText::FromString(GetName()));
	FText SecondSocketEqualsNoneErrorText = FText::Format( LOCTEXT("NoneSecondSocket", "{0}: Must set Second Socket Name."), FText::FromString(GetName()));
	FText PSTemplateEqualsNoneErrorText = FText::Format( LOCTEXT("NonePSTemplate", "{0}: Trail must have a PSTemplate."), FText::FromString(GetName()));
	FString PSTemplateName = PSTemplate ? PSTemplate->GetName() : "";
	FText PSTemplateInvalidErrorText = FText::Format(LOCTEXT("InvalidPSTemplateFmt", "{0}: {1} does not contain any trail emittter."), FText::FromString(GetName()), FText::FromString(PSTemplateName));

	MeshComp->ClearAnimNotifyErrors(this);

	//Validate the user input and report any errors.
	if (FirstSocketName == NAME_None)
	{
		if( bReportErrors )
			MeshComp->ReportAnimNotifyError(FirstSocketEqualsNoneErrorText, this);
		bError = true;
	}

	if (SecondSocketName == NAME_None)
	{
		if( bReportErrors )
			MeshComp->ReportAnimNotifyError(SecondSocketEqualsNoneErrorText, this);
		bError = true;
	}

	if (!PSTemplate)
	{
		if( bReportErrors )
			MeshComp->ReportAnimNotifyError( PSTemplateEqualsNoneErrorText, this);
		bError = true;
	}
	else
	{
		if (!PSTemplate->ContainsEmitterType(UParticleModuleTypeDataAnimTrail::StaticClass()))
		{
			if( bReportErrors )
				MeshComp->ReportAnimNotifyError(PSTemplateInvalidErrorText, this);
			bError = true;
		}
	}
	return bError;
#else
	return false;
#endif // WITH_EDITORONLY_DATA
}

#undef  LOCTEXT_NAMESPACE