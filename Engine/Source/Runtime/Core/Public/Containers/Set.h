// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Set.h: Set definitions.
=============================================================================*/

#pragma once

#include "SparseArray.h"

/**
 * The base KeyFuncs type with some useful definitions for all KeyFuncs; meant to be derived from instead of used directly.
 * bInAllowDuplicateKeys=true is slightly faster because it allows the TSet to skip validating that
 * there isn't already a duplicate entry in the TSet.
  */
template<typename ElementType,typename InKeyType,bool bInAllowDuplicateKeys = false>
struct BaseKeyFuncs
{
	typedef InKeyType KeyType;
	typedef typename TCallTraits<InKeyType>::ParamType KeyInitType;
	typedef typename TCallTraits<ElementType>::ParamType ElementInitType;

	enum { bAllowDuplicateKeys = bInAllowDuplicateKeys };
};

/**
 * A default implementation of the KeyFuncs used by TSet which uses the element as a key.
 */
template<typename ElementType,bool bInAllowDuplicateKeys = false>
struct DefaultKeyFuncs : BaseKeyFuncs<ElementType,ElementType,bInAllowDuplicateKeys>
{
	typedef typename TCallTraits<ElementType>::ParamType KeyInitType;
	typedef typename TCallTraits<ElementType>::ParamType ElementInitType;

	/**
	 * @return The key used to index the given element.
	 */
	static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element;
	}

	/**
	 * @return True if the keys match.
	 */
	static FORCEINLINE bool Matches(KeyInitType A,KeyInitType B)
	{
		return A == B;
	}

	/** Calculates a hash index for a key. */
	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key);
	}
};

// Forward declaration.
template<
	typename InElementType,
	typename KeyFuncs = DefaultKeyFuncs<InElementType>,
	typename Allocator = FDefaultSetAllocator
	>
class TSet;

/** This is used to provide type specific behavior for a move which will destroy B. */
/** Should be in UnrealTemplate but isn't for Clang build reasons - will move later */
template<typename T>
FORCEINLINE void MoveByRelocate(T& A, T& B)
{
	// Destruct the previous value of A.
	A.~T();

	// Relocate B into the 'hole' left by the destruction of A, leaving a hole in B instead.
	RelocateItems(&A, &B, 1);
}

/** Either NULL or an identifier for an element of a set. */
class FSetElementId
{
public:

	template<typename,typename,typename>
	friend class TSet;

	/** Default constructor. */
	FORCEINLINE FSetElementId():
		Index(INDEX_NONE)
	{}

	/** @return a boolean value representing whether the id is NULL. */
	FORCEINLINE bool IsValidId() const
	{
		return Index != INDEX_NONE;
	}

	/** Comparison operator. */
	FORCEINLINE friend bool operator==(const FSetElementId& A,const FSetElementId& B)
	{
		return A.Index == B.Index;
	}

	FORCEINLINE int32 AsInteger() const
	{
		return Index;
	}

	FORCEINLINE static FSetElementId FromInteger(int32 Integer)
	{
		return FSetElementId(Integer);
	}

private:

	/** The index of the element in the set's element array. */
	int32 Index;

	/** Initialization constructor. */
	FORCEINLINE FSetElementId(int32 InIndex):
		Index(InIndex)
	{}

	/** Implicit conversion to the element index. */
	FORCEINLINE operator int32() const
	{
		return Index;
	}
};

/** An element in the set. */
template <typename ElementType>
class TSetElement
{
public:

	/** The element's value. */
	ElementType Value;

	/** The id of the next element in the same hash bucket. */
	mutable FSetElementId HashNextId;

	/** The hash bucket that the element is currently linked to. */
	mutable int32 HashIndex;

	/** Default constructor. */
	FORCEINLINE TSetElement()
	{}

	/** Initialization constructor. */
	template <typename InitType>
	FORCEINLINE TSetElement(const InitType& InValue):
		Value(InValue)
	{}

	/** Serializer. */
	FORCEINLINE friend FArchive& operator<<(FArchive& Ar,TSetElement& Element)
	{
		return Ar << Element.Value;
	}

	// Comparison operators
	FORCEINLINE bool operator==(const TSetElement& Other) const
	{
		return Value == Other.Value;
	}
	FORCEINLINE bool operator!=(const TSetElement& Other) const
	{
		return Value != Other.Value;
	}
};

/**
 * A set with an optional KeyFuncs parameters for customizing how the elements are compared and searched.  
 * E.g. You can specify a mapping from elements to keys if you want to find elements by specifying a subset of 
 * the element type.  It uses a TSparseArray of the elements, and also links the elements into a hash with a 
 * number of buckets proportional to the number of elements.  Addition, removal, and finding are O(1).
 *
 **/
template<
	typename InElementType,
	typename KeyFuncs /*= DefaultKeyFuncs<ElementType>*/,
	typename Allocator /*= FDefaultSetAllocator*/
	>
class TSet
{
	friend struct TContainerTraits<TSet>;

	typedef typename KeyFuncs::KeyInitType     KeyInitType;
	typedef typename KeyFuncs::ElementInitType ElementInitType;

	typedef TSetElement<InElementType> SetElementType;

public:
	typedef InElementType ElementType;

	/** Initialization constructor. */
	FORCEINLINE TSet()
	:	HashSize(0)
	{}

	/** Copy constructor. */
	FORCEINLINE TSet(const TSet& Copy)
	:	HashSize(0)
	{
		*this = Copy;
	}

	/** Destructor. */
	FORCEINLINE ~TSet()
	{
		HashSize = 0;
	}

	/** Assignment operator. */
	TSet& operator=(const TSet& Copy)
	{
		if(this != &Copy)
		{
			Empty(Copy.Num());
			for(TConstIterator CopyIt(Copy);CopyIt;++CopyIt)
			{
				Add(*CopyIt);
			}
		}
		return *this;
	}

#if PLATFORM_COMPILER_HAS_RVALUE_REFERENCES

private:
	template <typename SetType>
	static FORCEINLINE typename TEnableIf<TContainerTraits<SetType>::MoveWillEmptyContainer>::Type MoveOrCopy(SetType& ToSet, SetType& FromSet)
	{
		ToSet.Elements = (ElementArrayType&&)FromSet.Elements;

		ToSet.Hash.MoveToEmpty(FromSet.Hash);

		ToSet  .HashSize = FromSet.HashSize;
		FromSet.HashSize = 0;
	}

	template <typename SetType>
	static FORCEINLINE typename TEnableIf<!TContainerTraits<SetType>::MoveWillEmptyContainer>::Type MoveOrCopy(SetType& ToSet, SetType& FromSet)
	{
		ToSet = FromSet;
	}

public:
	/** Move constructor. */
	TSet(TSet&& Other)
		: HashSize(0)
	{
		MoveOrCopy(*this, Other);
	}

	/** Move assignment operator. */
	TSet& operator=(TSet&& Other)
	{
		if (this != &Other)
		{
			MoveOrCopy(*this, Other);
		}

		return *this;
	}

#endif

	/**
	 * Removes all elements from the set, potentially leaving space allocated for an expected number of elements about to be added.
	 * @param ExpectedNumElements - The number of elements about to be added to the set.
	 */
	void Empty(int32 ExpectedNumElements = 0)
	{
		// Empty the elements array, and reallocate it for the expected number of elements.
		Elements.Empty(ExpectedNumElements);

		// Resize the hash to the desired size for the expected number of elements.
		if(!ConditionalRehash(ExpectedNumElements,true))
		{
			// If the hash was already the desired size, clear the references to the elements that have now been removed.
			if(HashSize)
			{
				for(int32 HashIndex = 0;HashIndex < HashSize;HashIndex++)
				{
					GetTypedHash(HashIndex) = FSetElementId();
				}
			}
		}
	}

	/** Shrinks the set's element storage to avoid slack. */
	FORCEINLINE void Shrink()
	{
		Elements.Shrink();
		Relax();
	}

	/** Compacts the allocated elements into a contiguous range. */
	FORCEINLINE void Compact()
	{
		Elements.Compact();
		Rehash();
	}

	/** Relaxes the set's hash to a size strictly bounded by the number of elements in the set. */
	FORCEINLINE void Relax()
	{
		ConditionalRehash(Elements.Num(),true);
	}

	/** 
	 * Helper function to return the amount of memory allocated by this container 
	 * @return number of bytes allocated by this container
	 */
	FORCEINLINE uint32 GetAllocatedSize( void ) const
	{
		return Elements.GetAllocatedSize() + (HashSize * sizeof(FSetElementId));
	}

	/** Tracks the container's memory use through an archive. */
	FORCEINLINE void CountBytes(FArchive& Ar)
	{
		Elements.CountBytes(Ar);
		Ar.CountBytes(HashSize * sizeof(int32),HashSize * sizeof(FSetElementId));
	}

	/** @return the number of elements. */
	FORCEINLINE int32 Num() const
	{
		return Elements.Num();
	}

	/**
	 * Checks whether an element id is valid.
	 * @param Id - The element id to check.
	 * @return true if the element identifier refers to a valid element in this set.
	 */
	FORCEINLINE bool IsValidId(FSetElementId Id) const
	{
		return	Id.IsValidId() && 
				Id >= 0 &&
				Id < Elements.GetMaxIndex() &&
				Elements.IsAllocated(Id);
	}

	/** Accesses the identified element's value. */
	FORCEINLINE ElementType& operator[](FSetElementId Id)
	{
		return Elements[Id].Value;
	}

	/** Accesses the identified element's value. */
	FORCEINLINE const ElementType& operator[](FSetElementId Id) const
	{
		return Elements[Id].Value;
	}

	/**
	 * Adds an element to the set.
	 *
	 * @param	InElement					Element to add to set
	 * @param	bIsAlreadyInSetPtr	[out]	Optional pointer to bool that will be set depending on whether element is already in set
	 * @return	A pointer to the element stored in the set.
	 */
	FORCEINLINE FSetElementId Add(const InElementType&  InElement, bool* bIsAlreadyInSetPtr = NULL) { return Emplace(         InElement , bIsAlreadyInSetPtr); }
	FORCEINLINE FSetElementId Add(      InElementType&& InElement, bool* bIsAlreadyInSetPtr = NULL) { return Emplace(MoveTemp(InElement), bIsAlreadyInSetPtr); }

	/**
	 * Adds an element to the set.
	 *
	 * @param	Args						The argument(s) to be forwarded to the set element's constructor.
	 * @param	bIsAlreadyInSetPtr	[out]	Optional pointer to bool that will be set depending on whether element is already in set
	 * @return	A pointer to the element stored in the set.
	 */
	template <typename ArgsType>
	FSetElementId Emplace(ArgsType&& Args,bool* bIsAlreadyInSetPtr = NULL)
	{
		// Create a new element.
		FSparseArrayAllocationInfo ElementAllocation = Elements.AddUninitialized();
		FSetElementId ElementId(ElementAllocation.Index);
		auto& Element = *new(ElementAllocation) SetElementType(Forward<ArgsType>(Args));

		bool bIsAlreadyInSet = false;
		if (!KeyFuncs::bAllowDuplicateKeys)
		{
			// If the set doesn't allow duplicate keys, check for an existing element with the same key as the element being added.
			FSetElementId ExistingId = FindId(KeyFuncs::GetSetKey(Element.Value));
			bIsAlreadyInSet = ExistingId.IsValidId();
			if (bIsAlreadyInSet)
			{
				// If there's an existing element with the same key as the new element, replace the existing element with the new element.
				MoveByRelocate(Elements[ExistingId].Value, Element.Value);

				// Then remove the new element.
				Elements.RemoveAtUninitialized(ElementId);

				// Then point the return value at the replaced element.
				ElementId = ExistingId;
			}
		}

		if (!bIsAlreadyInSet)
		{
			// Check if the hash needs to be resized.
			if(!ConditionalRehash(Elements.Num()))
			{
				// If the rehash didn't add the new element to the hash, add it.
				HashElement(ElementId,Element);
			}
		}

		if (bIsAlreadyInSetPtr)
		{
			*bIsAlreadyInSetPtr = bIsAlreadyInSet;
		}

		return ElementId;
	}

	/** Assignment operator. */
	void Append(const TArray<ElementType>& InElements)
	{
		for(int32 i=0; i<InElements.Num(); i++)
		{
			Add(InElements[i]);
		}
	}

	/**
	 * Add all items from another set to our set (union without creating a new set)
	 * @param OtherSet - The other set of items to add.
     */
	void Append( const TSet& OtherSet )
	{
		for(TConstIterator SetIt(OtherSet);SetIt;++SetIt)
		{
			Add(*SetIt);
		}
	}

	/**
	 * Removes an element from the set.
	 * @param Element - A pointer to the element in the set, as returned by Add or Find.
	 */
	void Remove(FSetElementId ElementId)
	{
		if(HashSize)
		{
			const auto& ElementBeingRemoved = Elements[ElementId];

			// Remove the element from the hash.
			for(FSetElementId* NextElementId = &GetTypedHash(ElementBeingRemoved.HashIndex);
				NextElementId->IsValidId();
				NextElementId = &Elements[*NextElementId].HashNextId)
			{
				if(*NextElementId == ElementId)
				{
					*NextElementId = ElementBeingRemoved.HashNextId;
					break;
				}
			}
		}

		// Remove the element from the elements array.
		Elements.RemoveAt(ElementId);
	}

	/**
	 * Finds an element with the given key in the set.
	 * @param Key - The key to search for.
	 * @return The id of the set element matching the given key, or the NULL id if none matches.
	 */
	FSetElementId FindId(KeyInitType Key) const
	{
		if(HashSize)
		{
			for(FSetElementId ElementId = GetTypedHash(KeyFuncs::GetKeyHash(Key));
				ElementId.IsValidId();
				ElementId = Elements[ElementId].HashNextId)
			{
				if(KeyFuncs::Matches(KeyFuncs::GetSetKey(Elements[ElementId].Value),Key))
				{
					// Return the first match, regardless of whether the set has multiple matches for the key or not.
					return ElementId;
				}
			}
		}
		return FSetElementId();
	}

	/**
	 * Finds an element with the given key in the set.
	 * @param Key - The key to search for.
	 * @return A pointer to an element with the given key.  If no element in the set has the given key, this will return NULL.
	 */
	FORCEINLINE ElementType* Find(KeyInitType Key)
	{
		FSetElementId ElementId = FindId(Key);
		if(ElementId.IsValidId())
		{
			return &Elements[ElementId].Value;
		}
		else
		{
			return NULL;
		}
	}
	
	/**
	 * Finds an element with the given key in the set.
	 * @param Key - The key to search for.
	 * @return A const pointer to an element with the given key.  If no element in the set has the given key, this will return NULL.
	 */
	FORCEINLINE const ElementType* Find(KeyInitType Key) const
	{
		FSetElementId ElementId = FindId(Key);
		if(ElementId.IsValidId())
		{
			return &Elements[ElementId].Value;
		}
		else
		{
			return NULL;
		}
	}

	/**
	 * Removes all elements from the set matching the specified key.
	 * @param Key - The key to match elements against.
	 * @return The number of elements removed.
	 */
	int32 Remove(KeyInitType Key)
	{
		int32 NumRemovedElements = 0;

		if(HashSize)
		{
			FSetElementId* NextElementId = &GetTypedHash(KeyFuncs::GetKeyHash(Key));
			while(NextElementId->IsValidId())
			{
				auto& Element = Elements[*NextElementId];
				if(KeyFuncs::Matches(KeyFuncs::GetSetKey(Element.Value),Key))
				{
					// This element matches the key, remove it from the set.  Note that Remove sets *NextElementId to point to the next
					// element after the removed element in the hash bucket.
					Remove(*NextElementId);
					NumRemovedElements++;

					if(!KeyFuncs::bAllowDuplicateKeys)
					{
						// If the hash disallows duplicate keys, we're done removing after the first matched key.
						break;
					}
				}
				else
				{
					NextElementId = &Element.HashNextId;
				}
			}
		}

		return NumRemovedElements;
	}

	/**
	 * Checks if the element contains an element with the given key.
	 * @param Key - The key to check for.
	 * @return true if the set contains an element with the given key.
	 */
	FORCEINLINE bool Contains(KeyInitType Key) const
	{
		return FindId(Key).IsValidId();
	}

	/**
	 * Sorts the set's elements using the provided comparison class.
	 */
	template <typename PREDICATE_CLASS>
	void Sort( const PREDICATE_CLASS& Predicate )
	{
		// Sort the elements according to the provided comparison class.
		Elements.Sort( FElementCompareClass< PREDICATE_CLASS >( Predicate ) );

		// Rehash.
		Rehash();
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,TSet& Set)
	{
		// Load the set's new elements.
		Ar << Set.Elements;

		if(Ar.IsLoading())
		{
			// Free the old hash.
			Set.Hash.ResizeAllocation(0,0,sizeof(FSetElementId));
			Set.HashSize = 0;

			// Hash the newly loaded elements.
			Set.ConditionalRehash(Set.Elements.Num());
		}

		return Ar;
	}

	/**
	 * Describes the set's contents through an output device.
	 * @param Ar - The output device to describe the set's contents through.
	 */
	void Dump(FOutputDevice& Ar)
	{
		Ar.Logf( TEXT("TSet: %i elements, %i hash slots"), Elements.Num(), HashSize );
		for(int32 HashIndex = 0;HashIndex < HashSize;HashIndex++)
		{
			// Count the number of elements in this hash bucket.
			int32 NumElementsInBucket = 0;
			for(FSetElementId ElementId = GetTypedHash(HashIndex);
				ElementId.IsValidId();
				ElementId = Elements[ElementId].HashNextId)
			{
				NumElementsInBucket++;
			}

			Ar.Logf(TEXT("   Hash[%i] = %i"),HashIndex,NumElementsInBucket);
		}
	}

	bool VerifyHashElementsKey(KeyInitType Key)
	{
		bool bResult=true;
		if(HashSize)
		{
			// iterate over all elements for the hash entry of the given key 
			// and verify that the ids are valid
			FSetElementId ElementId = GetTypedHash(KeyFuncs::GetKeyHash(Key));
			while( ElementId.IsValidId() )
			{
				if( !IsValidId(ElementId) )
				{
					bResult=false;
					break;
				}
				ElementId = Elements[ElementId].HashNextId;
			}
		}
		return bResult;
	}

	void DumpHashElements(FOutputDevice& Ar)
	{
		for(int32 HashIndex = 0;HashIndex < HashSize;HashIndex++)
		{
			Ar.Logf(TEXT("   Hash[%i]"),HashIndex);

			// iterate over all elements for the all hash entries 
			// and dump info for all elements
			FSetElementId ElementId = GetTypedHash(HashIndex);
			while( ElementId.IsValidId() )
			{
				if( !IsValidId(ElementId) )
				{
					Ar.Logf(TEXT("		!!INVALID!! ElementId = %d"),ElementId.Index);
				}
				else
				{
					Ar.Logf(TEXT("		VALID ElementId = %d"),ElementId.Index);
				}
				ElementId = Elements[ElementId].HashNextId;
			}
		}
	}

	// Legacy comparison operators.  Note that these also test whether the set's elements were added in the same order!
	friend bool LegacyCompareEqual(const TSet& A,const TSet& B)
	{
		return A.Elements == B.Elements;
	}
	friend bool LegacyCompareNotEqual(const TSet& A,const TSet& B)
	{
		return A.Elements != B.Elements;
	}

	/** @return the intersection of two sets. (A AND B)*/
	TSet Intersect(const TSet& OtherSet) const
	{
		TSet Result;
		for(TConstIterator SetIt(*this);SetIt;++SetIt)
		{
			if(OtherSet.Contains(*SetIt))
			{
				Result.Add(*SetIt);
			}
		}
		return Result;
	}

	/** @return the union of two sets. (A OR B)*/
	TSet Union(const TSet& OtherSet) const
	{
		TSet Result;
		for(TConstIterator SetIt(*this);SetIt;++SetIt)
		{
			Result.Add(*SetIt);
		}
		for(TConstIterator SetIt(OtherSet);SetIt;++SetIt)
		{
			Result.Add(*SetIt);
		}
		return Result;
	}

	/** @return the complement of two sets. (A not in B)*/
	TSet Difference(const TSet& OtherSet) const
	{
		TSet Result;
		for(TConstIterator SetIt(*this);SetIt;++SetIt)
		{
			if(!OtherSet.Contains(*SetIt))
			{
				Result.Add(*SetIt);
			}
		}
		return Result;
	}

	/**
	 * Determine whether the specified set is entirely included within this set
	 * 
	 * @param OtherSet	Set to check
	 * 
	 * @return True if the other set is entirely included in this set, false if it is not
	 */
	bool Includes(const TSet<ElementType,KeyFuncs,Allocator>& OtherSet) const
	{
		bool bIncludesSet = true;
		for(TConstIterator OtherSetIt(OtherSet); OtherSetIt; ++OtherSetIt)
		{
			if (!Contains(*OtherSetIt))
			{
				bIncludesSet = false;
				break;
			}
		}
		return bIncludesSet;
	}

	/** @return a TArray of the elements */
	TArray<ElementType> Array() const
	{
		TArray<ElementType> Result;
		Result.Empty(Num());
		for(TConstIterator SetIt(*this);SetIt;++SetIt)
		{
			Result.Add(*SetIt);
		}
		return Result;
	}

private:
	/** Extracts the element value from the set's element structure and passes it to the user provided comparison class. */
	template <typename PREDICATE_CLASS>
	class FElementCompareClass
	{
		TDereferenceWrapper< ElementType, PREDICATE_CLASS > Predicate;

	public:
		FORCEINLINE FElementCompareClass( const PREDICATE_CLASS& InPredicate )
			: Predicate( InPredicate )
		{}

		FORCEINLINE bool operator()( const SetElementType& A,const SetElementType& B ) const
		{
			return Predicate( A.Value, B.Value );
		}
	};

	typedef TSparseArray<SetElementType,typename Allocator::SparseArrayAllocator>     ElementArrayType;
	typedef typename Allocator::HashAllocator::template ForElementType<FSetElementId> HashType;

	ElementArrayType Elements;

	mutable HashType Hash;
	mutable int32    HashSize;

	FORCEINLINE FSetElementId& GetTypedHash(int32 HashIndex) const
	{
		return ((FSetElementId*)Hash.GetAllocation())[HashIndex & (HashSize - 1)];
	}

	/**
	 * Accesses an element in the set.
	 * This is needed because the iterator classes aren't friends of FSetElementId and so can't access the element index.
	 */
	FORCEINLINE const SetElementType& GetInternalElement(FSetElementId Id) const
	{
		return Elements[Id];
	}
	FORCEINLINE SetElementType& GetInternalElement(FSetElementId Id)
	{
		return Elements[Id];
	}

	/**
	 * Translates an element index into an element ID.
	 * This is needed because the iterator classes aren't friends of FSetElementId and so can't access the FSetElementId private constructor.
	 */
	static FORCEINLINE FSetElementId IndexToId(int32 Index)
	{
		return FSetElementId(Index);
	}

	/** Adds an element to the hash. */
	FORCEINLINE void HashElement(FSetElementId ElementId,const SetElementType& Element) const
	{
		// Compute the hash bucket the element goes in.
		Element.HashIndex = KeyFuncs::GetKeyHash(KeyFuncs::GetSetKey(Element.Value)) & (HashSize-1);

		// Link the element into the hash bucket.
		Element.HashNextId = GetTypedHash(Element.HashIndex);
		GetTypedHash(Element.HashIndex) = ElementId;
	}

	/**
	 * Checks if the hash has an appropriate number of buckets, and if not resizes it.
	 * @param NumHashedElements - The number of elements to size the hash for.
	 * @param bAllowShrinking - true if the hash is allowed to shrink.
	 * @return true if the set was rehashed.
	 */
	bool ConditionalRehash(int32 NumHashedElements,bool bAllowShrinking = false) const
	{
		// Calculate the desired hash size for the specified number of elements.
		const int32 DesiredHashSize = Allocator::GetNumberOfHashBuckets(NumHashedElements);

		// If the hash hasn't been created yet, or is smaller than the desired hash size, rehash.
		if(NumHashedElements > 0 &&
			(!HashSize ||
			HashSize < DesiredHashSize ||
			(HashSize > DesiredHashSize && bAllowShrinking)))
		{
			HashSize = DesiredHashSize;
			Rehash();
			return true;
		}
		else
		{
			return false;
		}
	}

	/** Resizes the hash. */
	void Rehash() const
	{
		// Free the old hash.
		Hash.ResizeAllocation(0,0,sizeof(FSetElementId));

		if(HashSize)
		{
			// Allocate the new hash.
			checkSlow(!(HashSize&(HashSize-1)));
			Hash.ResizeAllocation(0,HashSize,sizeof(FSetElementId));
			for(int32 HashIndex = 0;HashIndex < HashSize;HashIndex++)
			{
				GetTypedHash(HashIndex) = FSetElementId();
			}

			// Add the existing elements to the new hash.
			for(typename ElementArrayType::TConstIterator ElementIt(Elements);ElementIt;++ElementIt)
			{
				HashElement(FSetElementId(ElementIt.GetIndex()),*ElementIt);
			}
		}
	}

	/** The base type of whole set iterators. */
	template<bool bConst>
	class TBaseIterator
	{
	private:
		friend class TSet;

		typedef typename TChooseClass<bConst,const ElementType,ElementType>::Result ItElementType;

	public:
		typedef typename TChooseClass<bConst,typename ElementArrayType::TConstIterator,typename ElementArrayType::TIterator>::Result ElementItType;

		FORCEINLINE TBaseIterator(const ElementItType& InElementIt)
			: ElementIt(InElementIt)
		{
		}

		/** Advances the iterator to the next element. */
		FORCEINLINE TBaseIterator& operator++()
		{
			++ElementIt;
			return *this;
		}

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
		{ 
			return !!ElementIt; 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		// Accessors.
		FORCEINLINE FSetElementId GetId() const
		{
			return TSet::IndexToId(ElementIt.GetIndex());
		}
		FORCEINLINE ItElementType* operator->() const
		{
			return &ElementIt->Value;
		}
		FORCEINLINE ItElementType& operator*() const
		{
			return ElementIt->Value;
		}

		FORCEINLINE friend bool operator==(const TBaseIterator& Lhs, const TBaseIterator& Rhs) { return Lhs.ElementIt == Rhs.ElementIt; }
		FORCEINLINE friend bool operator!=(const TBaseIterator& Lhs, const TBaseIterator& Rhs) { return Lhs.ElementIt != Rhs.ElementIt; }

		ElementItType ElementIt;
	};

	/** The base type of whole set iterators. */
	template<bool bConst>
	class TBaseKeyIterator
	{
	private:
		typedef typename TChooseClass<bConst,const TSet,TSet>::Result SetType;
		typedef typename TChooseClass<bConst,const ElementType,ElementType>::Result ItElementType;

	public:
		/** Initialization constructor. */
		FORCEINLINE TBaseKeyIterator(SetType& InSet,KeyInitType InKey)
		:	Set(InSet)
		,	Key(InKey)
		,	Id()
		{
			// The set's hash needs to be initialized to find the elements with the specified key.
			Set.ConditionalRehash(Set.Elements.Num());
			if(Set.HashSize)
			{
				NextId = Set.GetTypedHash(KeyFuncs::GetKeyHash(Key));
				++(*this);
			}
		}

		/** Advances the iterator to the next element. */
		FORCEINLINE TBaseKeyIterator& operator++()
		{
			Id = NextId;

			while(Id.IsValidId())
			{
				NextId = Set.GetInternalElement(Id).HashNextId;
				checkSlow(Id != NextId);

				if(KeyFuncs::Matches(KeyFuncs::GetSetKey(Set[Id]),Key))
				{
					break;
				}

				Id = NextId;
			}
			return *this;
		}

		SAFE_BOOL_OPERATORS(TBaseKeyIterator<bConst>)

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
		{ 
			return Id.IsValidId(); 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		// Accessors.
		FORCEINLINE ItElementType* operator->() const
		{
			return &Set[Id];
		}
		FORCEINLINE ItElementType& operator*() const
		{
			return Set[Id];
		}

	protected:
		SetType& Set;
		typename TTypeTraits<typename KeyFuncs::KeyType>::ConstPointerType Key;
		FSetElementId Id;
		FSetElementId NextId;
	};

public:

	/** Used to iterate over the elements of a const TSet. */
	class TConstIterator : public TBaseIterator<true>
	{
		friend class TSet;

	public:
		FORCEINLINE TConstIterator(const typename TBaseIterator<true>::ElementItType& InElementId)
			: TBaseIterator<true>(InElementId)
		{
		}

		FORCEINLINE TConstIterator(const TSet& InSet)
			: TBaseIterator<true>(begin(InSet.Elements))
		{
		}
	};

	/** Used to iterate over the elements of a TSet. */
	class TIterator : public TBaseIterator<false>
	{
		friend class TSet;

	public:
		FORCEINLINE TIterator(TSet& InSet, const typename TBaseIterator<false>::ElementItType& InElementId)
			: TBaseIterator<false>(InElementId)
			, Set                 (InSet)
		{
		}

		FORCEINLINE TIterator(TSet& InSet)
			: TBaseIterator<false>(begin(InSet.Elements))
			, Set                 (InSet)
		{
		}

		/** Removes the current element from the set. */
		FORCEINLINE void RemoveCurrent()
		{
			Set.Remove(TBaseIterator<false>::GetId());
		}

	private:
		TSet& Set;
	};
	
	/** Used to iterate over the elements of a const TSet. */
	class TConstKeyIterator : public TBaseKeyIterator<true>
	{
	public:
		FORCEINLINE TConstKeyIterator(const TSet& InSet,KeyInitType InKey):
			TBaseKeyIterator<true>(InSet,InKey)
		{}
	};

	/** Used to iterate over the elements of a TSet. */
	class TKeyIterator : public TBaseKeyIterator<false>
	{
	public:
		FORCEINLINE TKeyIterator(TSet& InSet,KeyInitType InKey)
		:	TBaseKeyIterator<false>(InSet,InKey)
		,	Set(InSet)
		{}

		/** Removes the current element from the set. */
		FORCEINLINE void RemoveCurrent()
		{
			Set.Remove(TBaseKeyIterator<false>::Id);
			TBaseKeyIterator<false>::Id = FSetElementId();
		}
	private:
		TSet& Set;
	};

	/** Creates an iterator for the contents of this set */
	FORCEINLINE TIterator CreateIterator()
	{
		return TIterator(*this);
	}

	/** Creates a const iterator for the contents of this set */
	FORCEINLINE TConstIterator CreateConstIterator() const
	{
		return TConstIterator(*this);
	}

private:
	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE friend TIterator      begin(      TSet& Set) { return TIterator     (Set, begin(Set.Elements)); }
	FORCEINLINE friend TConstIterator begin(const TSet& Set) { return TConstIterator(     begin(Set.Elements)); }
	FORCEINLINE friend TIterator      end  (      TSet& Set) { return TIterator     (Set, end  (Set.Elements)); }
	FORCEINLINE friend TConstIterator end  (const TSet& Set) { return TConstIterator(     end  (Set.Elements)); }
};

template<typename ElementType, typename KeyFuncs, typename Allocator>
struct TContainerTraits<TSet<ElementType, KeyFuncs, Allocator> > : public TContainerTraitsBase<TSet<ElementType, KeyFuncs, Allocator> >
{
	enum { MoveWillEmptyContainer =
		PLATFORM_COMPILER_HAS_RVALUE_REFERENCES &&
		TContainerTraits<typename TSet<ElementType, KeyFuncs, Allocator>::ElementArrayType>::MoveWillEmptyContainer &&
		TAllocatorTraits<typename Allocator::HashAllocator>::SupportsMove };
};

/** A specialization of the exchange function that avoids reallocating when exchanging two sets. */
template<
	typename InElementType,
	typename KeyFuncs /*= DefaultKeyFuncs<ElementType>*/,
	typename Allocator /*= FDefaultSetAllocator*/
>
FORCEINLINE void Exchange(TSet<InElementType,KeyFuncs,Allocator>& A,TSet<InElementType,KeyFuncs,Allocator>& B)
{
	FMemory::Memswap(&A,&B,sizeof(TSet<InElementType,KeyFuncs,Allocator>));
}

