// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "BlueprintUtilities.h"
#include "LatentActions.h"

namespace
{
	void SanitizePropertyName(FString& PropertyName)
	{
		// Sanitize the name
		for (int32 i = 0; i < PropertyName.Len(); ++i)
		{
			TCHAR& C = PropertyName[i];

			const bool bGoodChar =
				((C >= 'A') && (C <= 'Z')) || ((C >= 'a') && (C <= 'z')) ||		// A-Z (upper and lowercase) anytime
				(C == '_') ||													// _ anytime
				((i > 0) && (C >= '0') && (C <= '9'));							// 0-9 after the first character

			if (!bGoodChar)
			{
				C = '_';
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// UTimelineTemplate

UTimelineTemplate::UTimelineTemplate(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	TimelineLength = 5.0f;
	TimelineGuid = FGuid::NewGuid();
	bReplicated = false;
}

FName UTimelineTemplate::GetDirectionPropertyName() const
{
	const FString TimelineName = TimelineTemplateNameToVariableName(GetFName());
	FString PropertyName = FString::Printf(TEXT("%s__Direction_%s"), *TimelineName, *TimelineGuid.ToString());
	SanitizePropertyName(PropertyName);
	return FName(*PropertyName);
}

FName UTimelineTemplate::GetTrackPropertyName(const FName TrackName) const
{
	const FString TimelineName = TimelineTemplateNameToVariableName(GetFName());
	FString PropertyName = FString::Printf(TEXT("%s_%s_%s"), *TimelineName, *TrackName.ToString(), *TimelineGuid.ToString());
	SanitizePropertyName(PropertyName);
	return FName(*PropertyName);
}

int32 UTimelineTemplate::FindFloatTrackIndex(const FName& FloatTrackName)
{
	for(int32 i=0; i<FloatTracks.Num(); i++)
	{
		if(FloatTracks[i].TrackName == FloatTrackName)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 UTimelineTemplate::FindVectorTrackIndex(const FName& VectorTrackName)
{
	for(int32 i=0; i<VectorTracks.Num(); i++)
	{
		if(VectorTracks[i].TrackName == VectorTrackName)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 UTimelineTemplate::FindEventTrackIndex(const FName& EventTrackName)
{
	for(int32 i=0; i<EventTracks.Num(); i++)
	{
		if(EventTracks[i].TrackName == EventTrackName)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 UTimelineTemplate::FindLinearColorTrackIndex(const FName& ColorTrackName)
{
	for(int32 i=0; i<LinearColorTracks.Num(); i++)
	{
		if(LinearColorTracks[i].TrackName == ColorTrackName)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool UTimelineTemplate::IsNewTrackNameValid(const FName& NewTrackName)
{
	// can't be NAME_None
	if(NewTrackName == NAME_None)
	{
		return false;
	}

	// Check each type of track to see if it already exists
	return	FindFloatTrackIndex(NewTrackName) == INDEX_NONE && 
			FindVectorTrackIndex(NewTrackName) == INDEX_NONE &&
			FindEventTrackIndex(NewTrackName) == INDEX_NONE;
}

FName UTimelineTemplate::GetUpdateFunctionName() const
{
	const FString TimelineName = TimelineTemplateNameToVariableName(GetFName());
	FString UpdateFuncString = FString::Printf(TEXT("%s__UpdateFunc"), *TimelineName);
	return FName(*UpdateFuncString);
}

FName UTimelineTemplate::GetFinishedFunctionName() const
{
	const FString TimelineName = TimelineTemplateNameToVariableName(GetFName());
	FString FinishedFuncString = FString::Printf(TEXT("%s__FinishedFunc"), *TimelineName);
	return FName(*FinishedFuncString);
}

FName UTimelineTemplate::GetEventTrackFunctionName(int32 EventTrackIndex) const
{
	check(EventTrackIndex < EventTracks.Num());

	const FName TrackName = EventTracks[EventTrackIndex].TrackName;
	const FString TimelineName = TimelineTemplateNameToVariableName(GetFName());
	FString UpdateFuncString = FString::Printf(TEXT("%s__%s__EventFunc"), *TimelineName, *TrackName.ToString());
	return FName(*UpdateFuncString);
}

int32 UTimelineTemplate::FindMetaDataEntryIndexForKey(const FName& Key)
{
	for(int32 i=0; i<MetaDataArray.Num(); i++)
	{
		if(MetaDataArray[i].DataKey == Key)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FString UTimelineTemplate::GetMetaData(const FName& Key)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	check(EntryIndex != INDEX_NONE);
	return MetaDataArray[EntryIndex].DataValue;
}

void UTimelineTemplate::SetMetaData(const FName& Key, const FString& Value)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	if(EntryIndex != INDEX_NONE)
	{
		MetaDataArray[EntryIndex].DataValue = Value;
	}
	else
	{
		MetaDataArray.Add( FBPVariableMetaDataEntry(Key, Value) );
	}
}

void UTimelineTemplate::RemoveMetaData(const FName& Key)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	if(EntryIndex != INDEX_NONE)
	{
		MetaDataArray.RemoveAt(EntryIndex);
	}
}

FString UTimelineTemplate::MakeUniqueCurveName(UObject* Obj, UObject* InOuter)
{
	FString OriginalName = Obj->GetName();
	UClass* Class = Obj->GetClass();
	FName TestName(*OriginalName);
	while(StaticFindObjectFast(Class, InOuter, TestName))
	{
		TestName = FName(*OriginalName, TestName.GetNumber()+1);
	}
	return TestName.ToString();
}

FString UTimelineTemplate::TimelineTemplateNameToVariableName(FName Name)
{
	static const FString TemplatetPostfix(TEXT("_Template"));
	FString NameStr = Name.ToString();
	// >>> Backwards Compatibility:  VER_UE4_EDITORONLY_BLUEPRINTS
	if (NameStr.EndsWith(TemplatetPostfix))
	// <<< End Backwards Compatibility
	{
		NameStr = NameStr.LeftChop(TemplatetPostfix.Len());
	}
	return NameStr;
}

FString UTimelineTemplate::TimelineVariableNameToTemplateName(FName Name)
{
	return Name.ToString() + TEXT("_Template");
}

void UTimelineTemplate::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	UObject* NewCurveOuter = GetOuter();

	// The outer should always be a BlueprintGeneratedClass, however the Curves get put into the Blueprint, however if they belong to the Transient package, that's where the duplicated Blueprint should be.
	if(GetOutermost() != GetTransientPackage())
	{
		if(UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GetOuter()))
		{
			NewCurveOuter = BPClass->ClassGeneratedBy;
		}
	}

	for(TArray<struct FTTFloatTrack>::TIterator It = FloatTracks.CreateIterator();It;++It)
	{
		FTTFloatTrack& Track = *It;
		if( Track.CurveFloat != NULL )
		{
			// Do not duplicate external curves unless duplicating to a transient package
			if(!Track.CurveFloat->GetOuter()->IsA(UPackage::StaticClass()) || GetOutermost() == GetTransientPackage())
			{
				Track.CurveFloat = DuplicateObject<UCurveFloat>(Track.CurveFloat, NewCurveOuter, *MakeUniqueCurveName(Track.CurveFloat, NewCurveOuter));
			}
		}
		else
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Timeline %s Track %s has an invalid curve.  Please fix!"), *TimelineTemplateNameToVariableName(GetFName()), *Track.TrackName.ToString());
		}
	}

	for(TArray<struct FTTEventTrack>::TIterator It = EventTracks.CreateIterator();It;++It)
	{
		FTTEventTrack& Track = *It;
		if( Track.CurveKeys != NULL )
		{
			// Do not duplicate external curves unless duplicating to a transient package
			if(!Track.CurveKeys->GetOuter()->IsA(UPackage::StaticClass()) || GetOutermost() == GetTransientPackage())
			{
				Track.CurveKeys = DuplicateObject<UCurveFloat>(Track.CurveKeys, NewCurveOuter, *MakeUniqueCurveName(Track.CurveKeys, NewCurveOuter));
			}
		}
		else
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Timeline %s Track %s has an invalid curve.  Please fix!"), *TimelineTemplateNameToVariableName(GetFName()), *Track.TrackName.ToString());
		}
	}

	for(TArray<struct FTTVectorTrack>::TIterator It = VectorTracks.CreateIterator();It;++It)
	{
		FTTVectorTrack& Track = *It;
		if( Track.CurveVector != NULL )
		{
			// Do not duplicate external curves unless duplicating to a transient package
			if(!Track.CurveVector->GetOuter()->IsA(UPackage::StaticClass()) || GetOutermost() == GetTransientPackage())
			{
				Track.CurveVector = DuplicateObject<UCurveVector>(Track.CurveVector, NewCurveOuter, *MakeUniqueCurveName(Track.CurveVector, NewCurveOuter));
			}
		}
		else
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Timeline %s Track %s has an invalid curve.  Please fix!"), *TimelineTemplateNameToVariableName(GetFName()), *Track.TrackName.ToString());
		}
	}

	for(TArray<struct FTTLinearColorTrack>::TIterator It = LinearColorTracks.CreateIterator();It;++It)
	{
		FTTLinearColorTrack& Track = *It;
		if( Track.CurveLinearColor != NULL )
		{
			// Do not duplicate external curves unless duplicating to a transient package
			if(!Track.CurveLinearColor->GetOuter()->IsA(UPackage::StaticClass()) || GetOutermost() == GetTransientPackage())
			{
				Track.CurveLinearColor = DuplicateObject<UCurveLinearColor>(Track.CurveLinearColor, NewCurveOuter, *MakeUniqueCurveName(Track.CurveLinearColor, NewCurveOuter));
			}
		}
		else
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Timeline %s Track %s has an invalid curve.  Please fix!"), *TimelineTemplateNameToVariableName(GetFName()), *Track.TrackName.ToString());
		}
	}

	TimelineGuid = FGuid::NewGuid();
}

bool FTTTrackBase::operator==( const FTTTrackBase& T2 ) const
{
	return (TrackName == T2.TrackName) &&
		   (bIsExternalCurve == T2.bIsExternalCurve);
}


bool FTTEventTrack::operator==( const FTTEventTrack& T2 ) const
{
	return FTTTrackBase::operator==(T2) && (CurveKeys && T2.CurveKeys) && 
		   (*CurveKeys == *T2.CurveKeys);
}

bool FTTFloatTrack::operator==( const FTTFloatTrack& T2 ) const
{
	return FTTTrackBase::operator==(T2) &&  (CurveFloat && T2.CurveFloat) && 
		(*CurveFloat == *T2.CurveFloat);
}

bool FTTVectorTrack::operator==( const FTTVectorTrack& T2 ) const
{
	return FTTTrackBase::operator==(T2)  &&  (CurveVector && T2.CurveVector) && 
		 (*CurveVector == *T2.CurveVector);
}

bool FTTLinearColorTrack::operator==( const FTTLinearColorTrack& T2 ) const
{
	return FTTTrackBase::operator==(T2)  &&  (CurveLinearColor && T2.CurveLinearColor) && 
		 (*CurveLinearColor == *T2.CurveLinearColor);
}

