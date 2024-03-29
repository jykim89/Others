// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UniquePtr.h"

template <typename T>
class TUniqueObj
{
public:
	TUniqueObj()
		: Obj_(MakeUnique<T>())
	{
	}

	template <typename Arg>
	explicit TUniqueObj(Arg&& arg)
		: Obj_(MakeUnique<T>(Forward<Arg>(arg)))
	{
	}

	      T* operator->()       { return Obj_.Get(); }
	const T* operator->() const { return Obj_.Get(); }

	      T& operator*()       { return *Obj_; }
	const T& operator*() const { return *Obj_; }

private:
	TUniquePtr<T> Obj_;
};

/** Information about class source and generated headers */
struct FClassHeaderInfo
{
	FString SourceFilename;
	FString GeneratedFilename;
	bool bHasChanged;
	
	FClassHeaderInfo()
		: bHasChanged(false)
	{
	}

	explicit FClassHeaderInfo(const FString& InSourceFilename)
		: SourceFilename(InSourceFilename)
		, bHasChanged(false)
	{
	}
};

struct FManifestModule;

extern TMap<UClass*, FString>                   GClassStrippedHeaderTextMap;
extern TMap<UClass*, FString>                   GClassSourceFileMap;
extern TMap<UClass*, FClassHeaderInfo>          GClassGeneratedFileMap;
extern TMap<UClass*, TUniqueObj<TArray<FName>>> GClassDependentOnMap;
extern TMap<UClass*, FString>                   GClassHeaderNameWithNoPathMap;
extern TMap<UClass*, FString>                   GClassModuleRelativePathMap;
extern TMap<UClass*, FString>                   GClassIncludePathMap;
extern TSet<UClass*>                            GExportedClasses;
extern TSet<UClass*>                            GPublicClassSet;
extern TMap<UProperty*, FString>                GArrayDimensions;
extern TMap<UPackage*,  const FManifestModule*> GPackageToManifestModuleMap;
