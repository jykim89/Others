// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

DEFINE_LOG_CATEGORY_STATIC(LogArray, Warning, All);

//////////////////////////////////////////////////////////////////////////
// UKismetArrayLibrary

UKismetArrayLibrary::UKismetArrayLibrary(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UKismetArrayLibrary::FilterArray(const TArray<AActor*>& TargetArray, TSubclassOf<class AActor> FilterClass, TArray<AActor*>& FilteredArray)
{
	FilteredArray.Empty();
	for (auto It = TargetArray.CreateConstIterator(); It; It++)
	{
		AActor* TargetElement = (*It);
		if (TargetElement && TargetElement->IsA(FilterClass))
		{
			FilteredArray.Add(TargetElement);
		}
	}
}

int32 UKismetArrayLibrary::GenericArray_Add(void* TargetArray, const UArrayProperty* ArrayProp, const int32& NewItem)
{
	int32 NewIndex = 0;
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
		UProperty* InnerProp = ArrayProp->Inner;

		NewIndex = ArrayHelper.AddValue();
		InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(NewIndex), (void*)(&NewItem));
	}
	return NewIndex;
}

void UKismetArrayLibrary::GenericArray_Append(void* TargetArray, const UArrayProperty* TargetArrayProp, void* SourceArray, const UArrayProperty* SourceArrayProperty)
{
	if(TargetArray && SourceArray)
	{
		FScriptArrayHelper TargetArrayHelper(TargetArrayProp, TargetArray);
		FScriptArrayHelper SourceArrayHelper(SourceArrayProperty, SourceArray);

		if(SourceArrayHelper.Num() > 0)
		{
			UProperty* InnerProp = TargetArrayProp->Inner;

			int32 StartIdx = TargetArrayHelper.AddValues(SourceArrayHelper.Num());
			for(int32 x = 0; x < SourceArrayHelper.Num(); ++x, ++StartIdx)
			{
				InnerProp->CopySingleValueToScriptVM(TargetArrayHelper.GetRawPtr(StartIdx), SourceArrayHelper.GetRawPtr(x));
			}
		}
	}
}

void UKismetArrayLibrary::GenericArray_Insert(void* TargetArray, const UArrayProperty* ArrayProp, const int32& NewItem, int32 Index)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
		UProperty* InnerProp = ArrayProp->Inner;

		if (ArrayHelper.IsValidIndex(Index)
			|| (Index >= 0 && Index <= ArrayHelper.Num()) )
		{
			ArrayHelper.InsertValues(Index, 1);
			InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(Index), (void*)(&NewItem));
		}
		else
		{
			UE_LOG(LogArray, Warning, TEXT("Attempted to insert an item into array %s out of bounds [%d/%d]!"), *ArrayProp->GetName(), Index, GetLastIndex(ArrayHelper));
		}
	}
}

void UKismetArrayLibrary::GenericArray_Remove(void* TargetArray, const UArrayProperty* ArrayProp, int32 IndexToRemove)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
		if( ArrayHelper.IsValidIndex(IndexToRemove) )
		{
			ArrayHelper.RemoveValues(IndexToRemove, 1);
		}
		else
		{
			UE_LOG(LogArray, Warning, TEXT("Attempted to remove an item from an invalid index from array %s [%d/%d]!"), *ArrayProp->GetName(), IndexToRemove, GetLastIndex(ArrayHelper));
		}
	}
}


bool UKismetArrayLibrary::GenericArray_RemoveItem( void* TargetArray, const UArrayProperty* ArrayProp, const int32& Item )
{
	bool bRemoved = false;

	if( TargetArray )
	{
		int32 IndexToRemove = GenericArray_Find(TargetArray, ArrayProp, Item);
		while(IndexToRemove != INDEX_NONE)
		{
			GenericArray_Remove(TargetArray, ArrayProp, IndexToRemove);
			bRemoved = true; //removed

			// See if there is another in the array
			IndexToRemove = GenericArray_Find(TargetArray, ArrayProp, Item);
		}
	}

	return bRemoved;
}


void UKismetArrayLibrary::GenericArray_Clear(void* TargetArray, const UArrayProperty* ArrayProp)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);

		ArrayHelper.EmptyValues();
	}
}

void UKismetArrayLibrary::GenericArray_Resize(void* TargetArray, const UArrayProperty* ArrayProp, int32 Size)
{
	if( TargetArray )
	{
		if(Size >= 0)
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
			ArrayHelper.Resize(Size);
		}
		else
		{
			UE_LOG(LogArray, Warning, TEXT("Attempted to resize an array using negative size: Array = %s, Size = %d!"), *ArrayProp->GetName(), Size);
		}
	}
}

int32 UKismetArrayLibrary::GenericArray_Length(void* TargetArray, const UArrayProperty* ArrayProp)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);

		return ArrayHelper.Num();
	}
	
	return 0;
}

int32 UKismetArrayLibrary::GenericArray_LastIndex(void* TargetArray, const UArrayProperty* ArrayProp)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);

		return ArrayHelper.Num() - 1;
	}

	return INDEX_NONE;
}

void UKismetArrayLibrary::GenericArray_Get(void* TargetArray, const UArrayProperty* ArrayProp, int32 Index, int32& Item)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);

		UProperty* InnerProp = ArrayProp->Inner;
		if( ArrayHelper.IsValidIndex(Index) )
		{
			InnerProp->CopyCompleteValueFromScriptVM(&Item, ArrayHelper.GetRawPtr(Index));	
		}
		else
		{
			UE_LOG(LogArray, Warning, TEXT("Attempted to get an item from array %s out of bounds [%d/%d]!"), *ArrayProp->GetName(), Index, GetLastIndex(ArrayHelper));
			InnerProp->InitializeValue(&Item);
		}
	}
}


void UKismetArrayLibrary::GenericArray_Set(void* TargetArray, const UArrayProperty* ArrayProp, int32 Index, const int32& NewItem, bool bSizeToFit)
{
	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, TargetArray);
		UProperty* InnerProp = ArrayProp->Inner;

		// Expand the array, if desired
		if (!ArrayHelper.IsValidIndex(Index) && bSizeToFit && (Index >= 0))
		{
			ArrayHelper.ExpandForIndex(Index);
		}

		if (ArrayHelper.IsValidIndex(Index))
		{
			InnerProp->CopySingleValueToScriptVM(ArrayHelper.GetRawPtr(Index), (void*)(&NewItem));
		}
		else
		{
			UE_LOG(LogArray, Warning, TEXT("Attempted to set an invalid index on array %s [%d/%d]!"), *ArrayProp->GetName(), Index, GetLastIndex(ArrayHelper));
		}
	}
}

int32 UKismetArrayLibrary::GenericArray_Find(void* TargetArray, const UArrayProperty* ArrayProperty, const int32& ItemToFind)
{
	int32 ResultIndex = INDEX_NONE;

	if( TargetArray )
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, TargetArray);
		UProperty* InnerProp = ArrayProperty->Inner;

		// compare against each element in the array
		for (int32 Idx = 0; Idx < ArrayHelper.Num() && ResultIndex == INDEX_NONE; Idx++)
		{
			if (InnerProp->Identical(&ItemToFind,ArrayHelper.GetRawPtr(Idx)))
			{
				ResultIndex = Idx;
			}
		}
	}

	// assign the resulting index
	return ResultIndex;
}

//////////////////////////////////////////////////////////////////////////
// Stubs for the UFunctions declared as kismet callable.  These are never actually called...the CustomThunk code calls the appropriate native function with a void* reference to the array

int32 UKismetArrayLibrary::Array_Add(const TArray<int32>& TargetArray, const UArrayProperty* ArrayProp, const int32& NewItem)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
	return 0;
}

void UKismetArrayLibrary::Array_Insert(const TArray<int32>& TargetArray, const UArrayProperty* ArrayProp, const int32& NewItem, int32 Index)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

void UKismetArrayLibrary::Array_Remove(const TArray<int32>& TargetArray, const UArrayProperty* ArrayProp, int32 IndexToRemove)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

bool UKismetArrayLibrary::Array_RemoveItem(const TArray<int32>& TargetArray, const UArrayProperty* ArrayProp, const int32& IndexToRemove)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
	return true;
}

void UKismetArrayLibrary::Array_Clear(const TArray<int32>& TargetArray, const UArrayProperty* ArrayProp)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

static void Array_Resize(const TArray<int32>& TargetArray, const UArrayProperty* ArrayProperty, int32 Size)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

int32 UKismetArrayLibrary::Array_Length(const TArray<int32>& TargetArray, const UArrayProperty* ArrayProp)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
	return 0;
}

int32 UKismetArrayLibrary::Array_LastIndex(const TArray<int32>& TargetArray, const UArrayProperty* ArrayProp)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
	return 0;
}

void UKismetArrayLibrary::Array_Get(const TArray<int32>& TargetArray, const UArrayProperty* ArrayProp, int32 Index, int32& Item)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

void UKismetArrayLibrary::Array_Set(const TArray<int32>& TargetArray, const UArrayProperty* ArrayProp, int32 Index, const int32& NewItem, bool bSizeToFit)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
}

int32 UKismetArrayLibrary::Array_Find(const TArray<int32>& TargetArray, const UArrayProperty* ArrayProperty, const int32& ItemToFind)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.  Call the Generic* equivalent instead
	check(0);
	return 0;
}

void UKismetArrayLibrary::SetArrayPropertyByName(UObject* Object, FName PropertyName, const TArray<int32>& Value)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.
	check(0);
}