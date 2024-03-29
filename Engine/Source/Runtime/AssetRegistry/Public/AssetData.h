// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogAssetData, Log, All);
/** A class to hold important information about an assets found by the Asset Registry */
class FAssetData
{
public:

	/** The object path for the asset in the form 'Package.GroupNames.AssetName' */
	FName ObjectPath;
	/** The name of the package in which the asset is found */
	FName PackageName;
	/** The path to the package in which the asset is found */
	FName PackagePath;
	/** The '.' delimited list of group names in which the asset is found. NAME_None if there were no groups */
	FName GroupNames;
	/** The name of the asset without the package or groups */
	FName AssetName;
	/** The name of the asset's class */
	FName AssetClass;
	/** The map of values for properties that were marked AssetRegistrySearchable */
	TMap<FName, FString> TagsAndValues;
	/** The IDs of the chunks this asset is located in for streaming install.  Empty if not assigned to a chunk */
	TArray<int32> ChunkIDs;

public:
	/** Default constructor */
	FAssetData()
	{}

	/** Constructor */
	FAssetData(FName InPackageName, FName InPackagePath, FName InGroupNames, FName InAssetName, FName InAssetClass, const TMap<FName, FString>& InTags, const TArray<int32>& InChunkIDs)
	{
		PackageName = InPackageName;
		PackagePath = InPackagePath;
		GroupNames = InGroupNames;
		AssetName = InAssetName;
		AssetClass = InAssetClass;
		TagsAndValues = InTags;

		FString ObjectPathStr = PackageName.ToString() + TEXT(".");

		if ( GroupNames != NAME_None )
		{
			ObjectPathStr += GroupNames.ToString() + TEXT(".");
		}

		ObjectPathStr += AssetName.ToString();

		ObjectPath = FName(*ObjectPathStr);
		ChunkIDs = InChunkIDs;
	}

	/** Constructor taking a UObject */
	FAssetData(const UObject* InAsset)
	{
		if ( InAsset != NULL )
		{
			const UPackage* Outermost = InAsset->GetOutermost();

			// Find the group names
			FString GroupNamesStr;
			FString AssetNameStr;
			InAsset->GetPathName(Outermost).Split(TEXT("."), &GroupNamesStr, &AssetNameStr, ESearchCase::CaseSensitive);

			PackageName = Outermost->GetFName();
			PackagePath = FName(*FPackageName::GetLongPackagePath(Outermost->GetName()));
			GroupNames = FName(*GroupNamesStr);
			AssetName = InAsset->GetFName();
			AssetClass = InAsset->GetClass()->GetFName();
			ObjectPath = FName(*InAsset->GetPathName());

			TArray<UObject::FAssetRegistryTag> TagList;
			InAsset->GetAssetRegistryTags(TagList);

			for ( auto TagIt = TagList.CreateConstIterator(); TagIt; ++TagIt )
			{
				TagsAndValues.Add(TagIt->Name, TagIt->Value);
			}

			ChunkIDs = Outermost->GetChunkIDs();
		}
	}

	/** FAssetDatas are equal if their object paths match */
	bool operator==(const FAssetData& Other) const
	{
		return ObjectPath == Other.ObjectPath;
	}

	/** Checks to see if this AssetData refers to an asset or is NULL */
	bool IsValid() const
	{
		return ObjectPath != NAME_None;
	}
	
	/** Returns true if this asset was found in a UAsset file */
	bool IsUAsset() const
	{
		return FPackageName::GetLongPackageAssetName(PackageName.ToString()) == AssetName.ToString();
	}

	/** Returns the full name for the asset in the form: Class ObjectPath */
	FString GetFullName() const
	{
		return FString::Printf(TEXT("%s %s"), *AssetClass.ToString(), *ObjectPath.ToString());
	}

	/** Returns the name for the asset in the form: Class'ObjectPath' */
	FString GetExportTextName() const
	{
		return FString::Printf(TEXT("%s'%s'"), *AssetClass.ToString(), *ObjectPath.ToString());
	}

	/** Returns true if the this asset is a redirector. */
	bool IsRedirector() const
	{
		if ( AssetClass == UObjectRedirector::StaticClass()->GetFName() )
		{
			return true;
		}

		return false;
	}

	/** Returns the class UClass if it is loaded. It is not possible to load the class if it is unloaded since we only have the short name. */
	UClass* GetClass() const
	{
		if ( !IsValid() )
		{
			// Dont even try to find the class if the objectpath isn't set
			return NULL;
		}

		UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE, *AssetClass.ToString());

		if (!FoundClass)
		{
			// Look for class redirectors
			FName NewPath = ULinkerLoad::FindNewNameForClass(AssetClass, false);

			if (NewPath != NAME_None)
			{
				FoundClass = FindObject<UClass>(ANY_PACKAGE, *NewPath.ToString());
			}
		}
		return FoundClass;
	}

	/** Convert to a StringAssetReference for loading */
	struct FStringAssetReference ToStringReference() const
	{
		return FStringAssetReference(ObjectPath.ToString());
	}

	/** Returns the asset UObject if it is loaded or loads the asset if it is unloaded then returns the result */
	UObject* GetAsset() const
	{
		if ( !IsValid() )
		{
			// Dont even try to find the object if the objectpath isn't set
			return NULL;
		}

		UObject* Asset = FindObject<UObject>(NULL, *ObjectPath.ToString());
		if ( Asset == NULL )
		{
			Asset = LoadObject<UObject>(NULL, *ObjectPath.ToString());
		}

		return Asset;
	}

	/** Returns true if the asset is loaded */
	bool IsAssetLoaded() const
	{
		return IsValid() && FindObject<UObject>(NULL, *ObjectPath.ToString()) != NULL;
	}
	
	/** Prints the details of the asset to the log */
	void PrintAssetData() const
	{
		UE_LOG(LogAssetData, Log, TEXT("    FAssetData for %s"), *ObjectPath.ToString());
		UE_LOG(LogAssetData, Log, TEXT("    ============================="));
		UE_LOG(LogAssetData, Log, TEXT("        PackageName: %s"), *PackageName.ToString());
		UE_LOG(LogAssetData, Log, TEXT("        PackagePath: %s"), *PackagePath.ToString());
		UE_LOG(LogAssetData, Log, TEXT("        GroupNames: %s"), *GroupNames.ToString());
		UE_LOG(LogAssetData, Log, TEXT("        AssetName: %s"), *AssetName.ToString());
		UE_LOG(LogAssetData, Log, TEXT("        AssetClass: %s"), *AssetClass.ToString());
		UE_LOG(LogAssetData, Log, TEXT("        TagsAndValues: %d"), TagsAndValues.Num());

		for (TMap<FName, FString>::TConstIterator TagIt(TagsAndValues); TagIt; ++TagIt)
		{
			UE_LOG(LogAssetData, Log, TEXT("            %s : %s"), *TagIt.Key().ToString(), *TagIt.Value());
		}

		UE_LOG(LogAssetData, Log, TEXT("        ChunkIDs: %d"), ChunkIDs.Num());

		for (auto ChunkIt = ChunkIDs.CreateConstIterator(); ChunkIt; ++ChunkIt)
		{
			UE_LOG(LogAssetData, Log, TEXT("                 %d"), *ChunkIt);
		}
	}

	/** Get the first FAssetData of a particular class from an Array of FAssetData */
	static FAssetData GetFirstAssetDataOfClass(const TArray<FAssetData>& Assets, const UClass* DesiredClass)
	{
		for(int32 AssetIdx=0; AssetIdx<Assets.Num(); AssetIdx++)
		{
			const FAssetData& Data = Assets[AssetIdx];
			UClass* AssetClass = Data.GetClass();
			if( AssetClass != NULL && AssetClass->IsChildOf(DesiredClass) )
			{
				return Data;
			}
		}
		return FAssetData();
	}

	/** Convenience template for finding first asset of a class */
	template <class T>
	static T* GetFirstAsset(const TArray<FAssetData>& Assets)
	{
		UClass* DesiredClass = T::StaticClass();
		UObject* Asset = FAssetData::GetFirstAssetDataOfClass(Assets, DesiredClass).GetAsset();
		check(Asset == NULL || Asset->IsA(DesiredClass));
		return (T*)Asset;
	}

	/** Operator for serialization */
	friend FArchive& operator<<(FArchive& Ar, FAssetData& AssetData)
	{
		// serialize out the asset info
		Ar << AssetData.ObjectPath;
		Ar << AssetData.PackagePath;
		Ar << AssetData.AssetClass;
		Ar << AssetData.GroupNames;

		// these are derived from ObjectPath, probably can skip serializing at the expense of runtime string manipulation
		Ar << AssetData.PackageName;
		Ar << AssetData.AssetName;

		Ar << AssetData.TagsAndValues;

		if (Ar.UE4Ver() >= VER_UE4_CHANGED_CHUNKID_TO_BE_AN_ARRAY_OF_CHUNKIDS)
		{
			Ar << AssetData.ChunkIDs;
		}
		else if (Ar.UE4Ver() >= VER_UE4_ADDED_CHUNKID_TO_ASSETDATA_AND_UPACKAGE)
		{
			// loading old assetdata.  we weren't using this value yet, so throw it away
			int ChunkID = -1;
			Ar << ChunkID;
		}

		return Ar;
	}
};
