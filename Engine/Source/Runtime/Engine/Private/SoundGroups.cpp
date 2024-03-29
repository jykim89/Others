// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Audio.h"
#include "Sound/SoundGroups.h"

USoundGroups::USoundGroups(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void USoundGroups::Initialize() const
{
	for (int32 ProfileIndex = 0; ProfileIndex < SoundGroupProfiles.Num(); ++ProfileIndex)
	{
		SoundGroupMap.Add(SoundGroupProfiles[ProfileIndex].SoundGroup, SoundGroupProfiles[ProfileIndex]);
	}

	if (!SoundGroupMap.Find(SOUNDGROUP_Default))
	{
		UE_LOG(LogAudio, Warning, TEXT("Missing default SoundGroup profile. Creating default with no decompression."));

		SoundGroupMap.Add(SOUNDGROUP_Default, FSoundGroup());
	}

#if WITH_EDITOR
	UEnum* SoundGroupEnum = FindObjectChecked<UEnum>(NULL, TEXT("/Script/Engine.SoundGroups.ESoundGroup"));

	for (const auto& It : SoundGroupMap)
	{
		if (It.Value.DisplayName.Len() > 0)
		{
			SoundGroupEnum->SetMetaData(TEXT("DisplayName"), *It.Value.DisplayName, It.Key);
			SoundGroupEnum->RemoveMetaData(TEXT("Hidden"), It.Key);
		}
		else
		{
			if (SoundGroupEnum->HasMetaData(TEXT("Hidden"), It.Key))
			{
				UE_LOG(LogAudio, Warning, TEXT("Custom Game SoundGroup profile for %s defined but no display name supplied."), *SoundGroupEnum->GetEnumText(It.Key).ToString());
			}
			SoundGroupEnum->RemoveMetaData(TEXT("Hidden"), It.Key);
		}
	}
#endif
}

const FSoundGroup& USoundGroups::GetSoundGroup(const ESoundGroup SoundGroup) const
{
	// Initialize the settings if this gets called early enough to require it
	if (SoundGroupMap.Num() == 0)
	{
		Initialize();
	}

	const FSoundGroup* SG = SoundGroupMap.Find(SoundGroup);
	if (SG == NULL)
	{
		UEnum* SoundGroupEnum = FindObjectChecked<UEnum>(NULL, TEXT("/Script/Engine.SoundGroups.ESoundGroup"));
		
		UE_LOG(LogAudio, Warning, TEXT("Requested SoundGroup %s does not have defined profile.  Using SOUNDGROUP_Default."), *SoundGroupEnum->GetEnumText(SoundGroup).ToString());
		return SoundGroupMap.FindChecked(SOUNDGROUP_Default);
	}

	return *SG;
}