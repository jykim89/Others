// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

DEFINE_LOG_CATEGORY_STATIC(LogCollisionProfile, Warning, All)

#define MIN_CUSTOMIZABLE_COLLISIONCHANNEL	ECC_GameTraceChannel1
#define MAX_CUSTOMIZABLE_COLLISIONCHANNEL	ECC_GameTraceChannel18
#define IS_VALID_COLLISIONCHANNEL(x) ((x)> ECC_Destructible && (x) < ECC_OverlapAll_Deprecated)

// do not chnage this name. This value is serialize to other object, if you change, it will mess up serialization, you'll need to fix up name by versioning
FName UCollisionProfile::CustomCollisionProfileName = FName(TEXT("Custom"));

//////////////////////////////////////////////////////////////////////////
FCollisionResponseTemplate::FCollisionResponseTemplate()
	: CollisionEnabled(ECollisionEnabled::NoCollision)
	, ObjectType(ECollisionChannel::ECC_WorldStatic)
	, HelpMessage(TEXT("Needs description"))
	, bCanModify(true)
	, ResponseToChannels()
{
}

bool FCollisionResponseTemplate::IsEqual(const TEnumAsByte<ECollisionEnabled::Type> InCollisionEnabled, 
	const TEnumAsByte<enum ECollisionChannel> InObjectType, 
	const struct FCollisionResponseContainer & InResponseToChannels)
{
	return (CollisionEnabled == InCollisionEnabled && ObjectType == InObjectType && 
		FMemory::Memcmp(&InResponseToChannels, &ResponseToChannels, sizeof(FCollisionResponseContainer))== 0);
}

//////////////////////////////////////////////////////////////////////////

ENGINE_API const FName UCollisionProfile::NoCollision_ProfileName = FName(TEXT("NoCollision"));
ENGINE_API const FName UCollisionProfile::BlockAll_ProfileName = FName(TEXT("BlockAll"));
ENGINE_API const FName UCollisionProfile::PhysicsActor_ProfileName = FName(TEXT("PhysicsActor"));
ENGINE_API const FName UCollisionProfile::BlockAllDynamic_ProfileName = FName(TEXT("BlockAllDynamic"));
ENGINE_API const FName UCollisionProfile::Pawn_ProfileName = FName(TEXT("Pawn"));
ENGINE_API const FName UCollisionProfile::Vehicle_ProfileName = FName(TEXT("Vehicle"));

UCollisionProfile::UCollisionProfile(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

UCollisionProfile * UCollisionProfile::Get()
{
	static bool bInitialized = false;
	// this is singletone. Use default object
	UCollisionProfile * CollisionProfile = CastChecked<UCollisionProfile>(UCollisionProfile::StaticClass()->GetDefaultObject());

	if (!bInitialized)
	{
		CollisionProfile->LoadProfileConfig();
		bInitialized = true;
	}

	return CollisionProfile;
}

bool UCollisionProfile::GetProfileTemplate(FName ProfileName, struct FCollisionResponseTemplate & ProfileData) const
{
	// verify if it is in redirect first
	if ( ProfileName != NAME_None )
	{
		return FindProfileData(Profiles, ProfileName, ProfileData);
	}

	return false;
}

bool UCollisionProfile::CheckRedirect(FName ProfileName, FBodyInstance & BodyInstance, struct FCollisionResponseTemplate & Template) const
{
	// make sure we're not setting invalid collision profile name
	if ( FBodyInstance::IsValidCollisionProfileName(ProfileName) )
	{
		const FName * NewName = ProfileRedirectsMap.Find(ProfileName);
		if ( NewName )
		{
			// if it's same as old version
			// we can use redirect
			// otherwise we don't want to use redirect
			BodyInstance.CollisionProfileName = *NewName;

			// if new profile name exists
			if ( *NewName != NAME_None )
			{
				check (FindProfileData(Profiles, *NewName, Template));
			}

			return true;
		}
	}

	return false;
}

bool UCollisionProfile::FindProfileData(const TArray<FCollisionResponseTemplate>	& ProfileList, FName ProfileName, struct FCollisionResponseTemplate & ProfileData) const
{
	if ( ProfileName != NAME_None )
	{
		for ( auto Iter = ProfileList.CreateConstIterator(); Iter; ++Iter )
		{
			if ( (*Iter).Name == ProfileName )
			{
				ProfileData = (*Iter);
				return true;
			}
		}
	}

	return false;
}

bool UCollisionProfile::ReadConfig(FName ProfileName, FBodyInstance & BodyInstance) const
{
	FCollisionResponseTemplate Template;

	// first check redirect
	// if that fails, just get profile
	if ( CheckRedirect(ProfileName, BodyInstance, Template) ||
		GetProfileTemplate(ProfileName, Template) )
	{
		// note that this can be called during loading or run-time (because of the function)
		// from property, it just uses property handle to set all data
		// but we can't use functions - i.e. SetCollisionEnabled - 
		// which will reset ProfileName by default
		BodyInstance.CollisionEnabled = Template.CollisionEnabled;
		BodyInstance.ObjectType = Template.ObjectType;
		BodyInstance.CollisionResponses.SetCollisionResponseContainer(Template.ResponseToChannels);
		BodyInstance.ResponseToChannels_DEPRECATED = Template.ResponseToChannels;

		// if valid instance, make sure to update physics filter data
		if (BodyInstance.IsValidBodyInstance())
		{
			BodyInstance.UpdatePhysicsFilterData();
		}
		return true;
	}

	return false;
}

const FCollisionResponseTemplate * UCollisionProfile::GetProfileByIndex(int32 Index) const
{ 
	if (Profiles.IsValidIndex(Index))
	{
		return &Profiles[Index]; 
	}

	return NULL;
}

// @todo check with Terrence, if this gets saved or not to local
void UCollisionProfile::AddChannelRedirect(FName OldName, FName NewName)
{
	if(OldName != NewName)
	{
		FName & NewValue = CollisionChannelRedirectsMap.FindOrAdd(OldName);
		NewValue = NewName;

		CollisionChannelRedirects.Empty();

		// now add back to it
		TArray<FString> KeyValues;
		for(auto Iter=CollisionChannelRedirectsMap.CreateConstIterator(); Iter; ++Iter)
		{
			const FName &Key = Iter.Key();
			const FName &Value = Iter.Value();
			CollisionChannelRedirects.Add(FRedirector(Key, Value));
		}

		// if change collision channel redirect, it will requires for all profiles to refresh that data
		for(auto Iter=Profiles.CreateIterator(); Iter; ++Iter)
		{
			SaveCustomResponses(*Iter);
		}
	}
}

void UCollisionProfile::AddProfileRedirect(FName OldName, FName NewName)
{
	if(OldName != NewName)
	{
		ProfileRedirectsMap.FindOrAdd(OldName) = NewName;

		ProfileRedirects.Empty();

		// now add back to it
		TArray<FString> KeyValues;
		for(auto Iter=ProfileRedirectsMap.CreateConstIterator(); Iter; ++Iter)
		{
			const FName &Key = Iter.Key();
			const FName &Value = Iter.Value();
			ProfileRedirects.Add(FRedirector(Key, Value));
		}
	}
}

void UCollisionProfile::LoadProfileConfig(bool bForceInit)
{
	/** 
	 * This function loads all config data to memory
	 * 
	 * 1. First it fixes the meta data for each custom channel name since those meta data is used for #2
	 * 2. Load Default Profile so that it can be used later
	 * 3. Second it sets up Correct ResponseToChannel for all profiles
	 * 4. It loads profile redirect data 
	 **/
	// read "EngineTraceChanne" and "GameTraceChanne" and set meta data
	FConfigSection* Configs = GConfig->GetSectionPrivate( TEXT("/Script/Engine.CollisionProfile"), false, true, GEngineIni );

	// before any op, verify if profiles contains invalid name - such as Custom profile name - remove all of them
	for (auto Iter=Profiles.CreateConstIterator(); Iter; ++Iter)
	{
		// make sure it doens't have any 
		if (Iter->Name == CustomCollisionProfileName)
		{
			UE_LOG(LogCollisionProfile, Error, TEXT("Profiles contain invalid name : %s is reserved for internal use"), *CustomCollisionProfileName.ToString());
			Profiles.RemoveAt(Iter.GetIndex());
		}
	}
	// 1. First loads all meta data for custom channels
	// this can be used in #2, so had to fix up first

	// this is to replace ECollisionChannel's DisplayName to be defined by users
	FString EngineTraceChannel	= TEXT("EngineTraceChannel");
	FString GameTraceChannel	= TEXT("GameTraceChannel");

	// find the enum
	UEnum * Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ECollisionChannel"), true);
	// we need this Enum
	check (Enum);
	UStruct * Struct = FCollisionResponseContainer::StaticStruct(); 
	check (Struct);
	const FString KeyName = TEXT("DisplayName");
	const FString TraceType = TEXT("TraceQuery");
	const FString TraceValue = TEXT("1");
	const FString HiddenMeta = TEXT("Hidden");

	// need to initialize displaynames separate
	int32 NumEnum = Enum->NumEnums();
	ChannelDisplayNames.Empty(NumEnum);
	ChannelDisplayNames.AddZeroed(NumEnum);
	TraceTypeMapping.Empty();
	ObjectTypeMapping.Empty();

	// first go through enum entry, and add suffix to displaynames
	int32 PrefixLen = FString(TEXT("ECC_")).Len();

	// need to have mapping table between ECollisionChannel and EObjectTypeQuery/ETraceTypeQuery
	UEnum * ObjectTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EObjectTypeQuery"), true);
	UEnum * TraceTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ETraceTypeQuery"), true);
	check (ObjectTypeEnum && TraceTypeEnum);

	for ( int32 EnumIndex=0; EnumIndex<NumEnum; ++EnumIndex )
	{
		FString EnumName = Enum->GetEnumName(EnumIndex);
		EnumName = EnumName.RightChop(PrefixLen);
		FName DisplayName = FName(*EnumName);

		if ( IS_VALID_COLLISIONCHANNEL(EnumIndex) )
		{
			// verify if the Struct name matches
			// this is to avoid situations where they mismatch and causes random bugs
			UField * Field = FindField<UField>(Struct, DisplayName);

			if (!Field)
			{
				// error - this is bad. This means somebody changed variable name without changing channel enum
				UE_LOG(LogCollisionProfile, Error, TEXT("Variable (%s) isn't found for Channel (%s). \nPlease make sure you name matches between ECollisionChannel and FCollisionResponseContainer."), *DisplayName.ToString(), *EnumName);
			}

#if WITH_EDITOR
			// clear displayname since we're setting it in below
			Enum->RemoveMetaData(*KeyName, EnumIndex);
			if (Enum->HasMetaData(*HiddenMeta, EnumIndex) == false)
			{
				Enum->SetMetaData(*HiddenMeta, NULL, EnumIndex);
			}
#endif			
		}
		else
		{
			// fix up FCollisionQueryFlag AllObjects flag, if trace type=1
			ECollisionChannel CollisionChannel = (ECollisionChannel)EnumIndex;
			// for any engine level collision profile, we hard coded here
			// meta data doesn't work in cooked build, so we'll have to manually handle them
			if ((CollisionChannel == ECC_Visibility) || (CollisionChannel == ECC_Camera))
			{
				// remove from object query flags
				FCollisionQueryFlag::Get().RemoveFromAllObjectsQueryFlag(CollisionChannel);
				TraceTypeMapping.Add(CollisionChannel);
			}
			else if ( CollisionChannel < ECC_OverlapAll_Deprecated )
			{
				ObjectTypeMapping.Add(CollisionChannel);
			}
		}

		ChannelDisplayNames[EnumIndex] = DisplayName;
	}

	// Now Load Channel setups, and set display names if customized
	// also initialize DefaultResposneContainer with default response for each channel
	FCollisionResponseContainer::DefaultResponseContainer.SetAllChannels(ECR_Block);

	for (auto Iter=DefaultChannelResponses.CreateConstIterator(); Iter; ++Iter)
	{
		const FCustomChannelSetup& CustomChannel = *Iter;
		int32 EnumIndex = CustomChannel.Channel;
		// make sure it is the range of channels we allow to change
		if ( IS_VALID_COLLISIONCHANNEL(EnumIndex) )
		{
			if ( CustomChannel.Name != NAME_None )
			{
				// before you set meta data, you'll have to save this value to modify 
				// ECollisionResponseContainer variables meta data
				FString VariableName = ChannelDisplayNames[EnumIndex].ToString();
				FString DisplayValue = CustomChannel.Name.ToString();

				// also has to set this for internal use
				ChannelDisplayNames[EnumIndex] = FName(*DisplayValue);

#if WITH_EDITOR
				// set displayvalue for this enum entry
				Enum->SetMetaData(*KeyName, *DisplayValue, EnumIndex);
				// also need to remove "Hidden"
				Enum->RemoveMetaData(*HiddenMeta, EnumIndex);
#endif 
				// now add MetaData type for trace type if it does
				if (CustomChannel.bTraceType)
				{
#if WITH_EDITOR
					// add to enum
					Enum->SetMetaData(*TraceType, *TraceValue, EnumIndex);
#endif
					// remove from all object queries
					FCollisionQueryFlag::Get().RemoveFromAllObjectsQueryFlag(CustomChannel.Channel);
					TraceTypeMapping.Add(CustomChannel.Channel);
				}
				// if it has display value
				else 
				{
#if WITH_EDITOR
					// add to enum
					Enum->RemoveMetaData(*TraceType, EnumIndex);
#endif
					ObjectTypeMapping.Add(CustomChannel.Channel);

					if (CustomChannel.bStaticObject)
					{
						// add to static object 
						FCollisionQueryFlag::Get().AddToAllStaticObjectsQueryFlag(CustomChannel.Channel);
					}
				}
#if WITH_EDITOR
				// now enum is fixed, so find member variable for the field
				UField * Field = FindField<UField>(Struct, FName(*VariableName));
				// I verified up in the class, this can't happen
				check (Field);
				Field->SetMetaData(*KeyName, *DisplayValue);
#endif
			}
			else
			{
				// missing name
				UE_LOG(LogCollisionProfile, Warning, TEXT("Name can't be empty for Channel (%d) "), EnumIndex );
			}

			// allow it to set default response
			FCollisionResponseContainer::DefaultResponseContainer.SetResponse((ECollisionChannel) EnumIndex, CustomChannel.DefaultResponse);
		}
		else
		{
			// you can't customize those channels
			UE_LOG(LogCollisionProfile, Warning, TEXT("Default Setup doesn't allow for predefined engine channels (%d) "), EnumIndex);
		}
	}

#if WITH_EDITOR
	// now propagate all changes to the channels to EObjectTypeQuery and ETraceTypeQuery for blueprint accessibility
	// this code is to show in editor more friendly, so it doesn't have to be set if it's not for editor
	int32 ObjectTypeEnumIndex = 0;
	int32 TraceTypeEnumIndex = 0;

	// first go through fill up ObjectType Enum
	for ( int32 EnumIndex=0; EnumIndex<NumEnum; ++EnumIndex )
	{
		// if not hidden
		const FString & Hidden = Enum->GetMetaData(*HiddenMeta, EnumIndex);
		if ( Hidden.IsEmpty() )
		{
			const FString & DisplayName = Enum->GetMetaData(*KeyName, EnumIndex);
			if ( !DisplayName.IsEmpty() )
			{
				// find out trace type or object type
				if (Enum->GetMetaData(*TraceType, EnumIndex) == TraceValue)
				{
					TraceTypeEnum->RemoveMetaData(*HiddenMeta, TraceTypeEnumIndex);
					TraceTypeEnum->SetMetaData(*KeyName, *DisplayName, TraceTypeEnumIndex);
					++TraceTypeEnumIndex;
				}
				else
				{
					ObjectTypeEnum->RemoveMetaData(*HiddenMeta, ObjectTypeEnumIndex);
					ObjectTypeEnum->SetMetaData(*KeyName, *DisplayName, ObjectTypeEnumIndex);
					++ObjectTypeEnumIndex;
				}
			}
		}
	}

	// make sure TraceTypeEnumIndex matches TraceTypeMapping
	check (TraceTypeMapping.Num() == TraceTypeEnumIndex);
	check (ObjectTypeMapping.Num() == ObjectTypeEnumIndex);
#endif

	// collision redirector has to be loaded before profile
	CollisionChannelRedirectsMap.Empty();

	for(auto Iter = CollisionChannelRedirects.CreateConstIterator(); Iter; ++Iter)
	{
		FName OldName = Iter->OldName;
		FName NewName = Iter->NewName;

		// at least we need to have OldName
		if(OldName!= NAME_None && NewName != NAME_None)
		{
			// add to pair
			CollisionChannelRedirectsMap.Add(OldName, NewName);
		}
		else
		{
			// print error 
			UE_LOG(LogCollisionProfile, Warning, TEXT("CollisionChannel Redirects : Name Can't be none (%s: %s)"), *OldName.ToString(), *NewName.ToString());
		}
	}

	// 2. Second loads all set up back to ResponseToChannels
	// this does a lot of iteration, but this only happens once loaded, so it's better to be convenient than efficient
	// fill up Profiles data
	FillProfileData(Profiles, Enum, KeyName, EditProfiles);

	// 3. It loads redirect data  - now time to load profile redirect
	ProfileRedirectsMap.Empty();

	// handle profile redirect here
	for(auto Iter = ProfileRedirects.CreateConstIterator(); Iter; ++Iter)
	{
		FName OldName = Iter->OldName;
		FName NewName = Iter->NewName;

		// at least we need to have OldName
		if(OldName!= NAME_None && NewName != NAME_None)
		{
			FCollisionResponseTemplate Template;

			// make sure the template exists
			if(FindProfileData(Profiles, NewName, Template))
			{
				// add to pair
				ProfileRedirectsMap.Add(OldName, NewName);
			}
			else
			{
				// print error
				UE_LOG(LogCollisionProfile, Warning, TEXT("ProfileRedirect (%s : %s) - New Name (\'%s\') isn't found "),
					*OldName.ToString(), *NewName.ToString(), *NewName.ToString());
			}
		}
	}

#if WITH_EDITOR
	if (bForceInit)
	{
		// go through objects, and see if we can reinitialize profile
		for(TObjectIterator<UPrimitiveComponent> PrimIt; PrimIt; ++PrimIt)
		{
			PrimIt->UpdateCollisionProfile();
		}
	}
#endif
}

void UCollisionProfile::FillProfileData(TArray<FCollisionResponseTemplate>	& ProfileList, const UEnum * CollisionChannelEnum, const FString & KeyName, TArray<FCustomProfile> & EditProfileList)
{
	// first go through if same name is found later, delete previous entry, this way if game overrides, 
	// we delete engine version and use game version
	int32 NumProfile = ProfileList.Num();
	for ( int32 PriIndex=NumProfile-1; PriIndex>=0 ; --PriIndex )
	{
		FCollisionResponseTemplate& Template = ProfileList[PriIndex];
		// now we iterate previous list
		for ( int32 SubIndex=PriIndex-1; SubIndex>=0; --SubIndex )
		{
			FCollisionResponseTemplate& SubTemplate = ProfileList[SubIndex];
			// if name is same
			if (Template.Name != NAME_None && Template.Name == SubTemplate.Name)
			{
				// delete SubTemplate
				ProfileList.RemoveAt(SubIndex);
				// now you need to decrease all index, otherwise you'll get messed up
				// you're iterating through same list
				--PriIndex;
				--SubIndex;
			}
		}
	}
	// --------------------------------------------------------------------------
	// this is a bit convoluted, but to make editing easier this seems best way
	// my intention here is really to allow users to edit easier
	// First using "DisplayName" will be more user-friendly
	// Second, allowing default reponse simplifies a lot of things
	// --------------------------------------------------------------------------
	for (auto ProfileIter = ProfileList.CreateIterator(); ProfileIter; ++ProfileIter)
	{
		FCollisionResponseTemplate& Template = *ProfileIter;

		if (Template.ObjectTypeName!=NAME_None)
		{
			// find what object type it is and fill it up
			int32 EnumIndex = ReturnContainerIndexFromChannelName(Template.ObjectTypeName);
			if (EnumIndex != INDEX_NONE)
			{
				Template.ObjectType = (ECollisionChannel)EnumIndex;
			}
			else
			{
				UE_LOG(LogCollisionProfile, Warning, TEXT("Profile (%s) ObjectTypeName (%s) is invalid. "), 
					*Template.Name.ToString(), *Template.ObjectTypeName.ToString());
			}
		}

		// set to engine default
		Template.ResponseToChannels = FCollisionResponseContainer::DefaultResponseContainer;

		LoadCustomResponses(Template, CollisionChannelEnum, Template.CustomResponses);
		//---------------------------------------------------------------------------------------
		// now load EditProfiles for extra edition of profiles for each game project
		// this one, only search the profile and see if it can change values
		//---------------------------------------------------------------------------------------
		for (auto CustomIter = EditProfileList.CreateIterator(); CustomIter; ++CustomIter)
		{
			if ( (*CustomIter).Name == Template.Name )
			{
				LoadCustomResponses(Template, CollisionChannelEnum, (*CustomIter).CustomResponses);
				break;
			}
		}
	}
}

int32 UCollisionProfile::LoadCustomResponses(FCollisionResponseTemplate & Template, const UEnum * CollisionChannelEnum, TArray<FResponseChannel>	& CustomResponses) const
{
	int32 NumOfItemsCustomized=0;

	// now loads all custom setups
	for (auto ChannelIter = CustomResponses.CreateIterator(); ChannelIter; ++ChannelIter)
	{
		FResponseChannel & Custom = *ChannelIter;
		bool bValueFound=false;

		int32 EnumIndex = ReturnContainerIndexFromChannelName(Custom.Channel);
		if (EnumIndex != INDEX_NONE)
		{
			// use the enum index to set to response to channel
			Template.ResponseToChannels.EnumArray[EnumIndex] = Custom.Response;
			bValueFound = true;
			++NumOfItemsCustomized;
		}
		else
		{
			// print error
			UE_LOG(LogCollisionProfile, Warning, TEXT("Profile (%s) - Custom Channel Name = \'%s\' hasn't been found"), 
				*Template.Name.ToString(), *Custom.Channel.ToString());
		}
	}

	return (NumOfItemsCustomized == CustomResponses.Num());
}

void UCollisionProfile::SaveCustomResponses(FCollisionResponseTemplate & Template) const
{
	const FCollisionResponseContainer & DefaultContainer = FCollisionResponseContainer::GetDefaultResponseContainer();

	Template.CustomResponses.Empty();
	for(int32 Index=0; Index<32; ++Index)
	{
		if(Template.ResponseToChannels.EnumArray[Index] != DefaultContainer.EnumArray[Index])
		{
			Template.CustomResponses.Add( FResponseChannel(ChannelDisplayNames[Index], (ECollisionResponse)(Template.ResponseToChannels.EnumArray[Index])));
		}
	}
}

int32 UCollisionProfile::ReturnContainerIndexFromChannelName(FName & DisplayName)  const
{
	// if we don't find it in new name
	// @note: I think we can search redirect first in case anybody would like to reuse the name
	// but that seems overhead moving forward. However that is possibility. 
	// this code is only one that has to support old redirects
	// other code should only use new names
	int32 NameIndex = ChannelDisplayNames.Find(DisplayName);
	if (NameIndex == INDEX_NONE)
	{
		// search for redirects
		const FName * NewName = CollisionChannelRedirectsMap.Find(DisplayName);
		if (NewName)
		{
			DisplayName = *NewName;
			return ChannelDisplayNames.Find(*NewName);
		}
	}

	return NameIndex;
}

FName UCollisionProfile::ReturnChannelNameFromContainerIndex(int32 ContainerIndex) const
{
	if (ChannelDisplayNames.IsValidIndex(ContainerIndex))
	{
		return ChannelDisplayNames[ContainerIndex];
	}

	return TEXT("");
}

ECollisionChannel UCollisionProfile::ConvertToCollisionChannel(bool TraceType, int32 Index) const
{
	if (TraceType)
	{
		if ( TraceTypeMapping.IsValidIndex(Index) )
		{
			return TraceTypeMapping[Index];
		}
	}
	else
	{
		if ( ObjectTypeMapping.IsValidIndex(Index) )
		{
			return ObjectTypeMapping[Index];
		}
	}

	// invalid
	return ECC_MAX;
}

EObjectTypeQuery UCollisionProfile::ConvertToObjectType(ECollisionChannel CollisionChannel) const
{
	if (CollisionChannel < ECC_MAX)
	{
		int32 ObjectTypeIndex = 0;
		for(const auto& MappedCollisionChannel : ObjectTypeMapping)
		{
			if(MappedCollisionChannel == CollisionChannel)
			{
				return (EObjectTypeQuery)ObjectTypeIndex;
			}

			ObjectTypeIndex++;
		}
	}

	return ObjectTypeQuery_MAX;
}

ETraceTypeQuery UCollisionProfile::ConvertToTraceType(ECollisionChannel CollisionChannel) const
{
	if (CollisionChannel < ECC_MAX)
	{
		int32 TraceTypeIndex = 0;
		for(const auto& MappedCollisionChannel : TraceTypeMapping)
		{
			if(MappedCollisionChannel == CollisionChannel)
			{
				return (ETraceTypeQuery)TraceTypeIndex;
			}

			TraceTypeIndex++;
		}
	}

	return TraceTypeQuery_MAX;
}