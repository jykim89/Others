// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnrealType.h: Unreal engine base type definitions.
=============================================================================*/

#pragma once

#include "ObjectBase.h"

COREUOBJECT_API DECLARE_LOG_CATEGORY_EXTERN(LogType, Log, All);

/*-----------------------------------------------------------------------------
	UProperty.
-----------------------------------------------------------------------------*/

// Property exporting flags.
enum EPropertyPortFlags
{
	/** No special property exporint flags */
	PPF_None						= 0x00000000,

	/** Indicates that property data should be treated as text */
	PPF_Localized					= 0x00000001,

	/** Indicates that property data should be wrapped in quotes (for some types of properties) */
	PPF_Delimited					= 0x00000002,

	/** Indicates that the object reference should be verified */
	PPF_CheckReferences				= 0x00000004, 
	
	PPF_ExportsNotFullyQualified	= 0x00000008,
	
	PPF_AttemptNonQualifiedSearch	= 0x00000010,
	
	/** Indicates that importing values for config or localized properties is disallowed */
	PPF_RestrictImportTypes			= 0x00000020,
	
	//								= 0x00000040,
	
	/** only include properties that have the CPF_Localized flag for ImportText/ExportText */
	PPF_LocalizedOnly				= 0x00000080,

	/** only include properties which are marked CPF_InstancedReference */
	PPF_SubobjectsOnly				= 0x00000100,

	/**
	 * Only applicable to component properties (for now)
	 * Indicates that two object should be considered identical
	 * if the property values for both objects are all identical
	 */
	PPF_DeepComparison				= 0x00000200,

	/**
	 * Similar to PPF_DeepComparison, except that template components are always compared using standard object
	 * property comparison logic (basically if the pointers are different, then the property isn't identical)
	 */
	PPF_DeepCompareInstances		= 0x00000400,

	/**
	 * Set if this operation is copying in memory (for copy/paste) instead of exporting to a file. There are
	 * some subtle differences between the two
	 */
	PPF_Copy						= 0x00000800,

	/** Set when duplicating objects via serialization */
	PPF_Duplicate					= 0x00001000,

	/** Indicates that object property values should be exported without the package or class information */
	PPF_SimpleObjectText			= 0x00002000,

	/** parsing default properties - allow text for transient properties to be imported - also modifies ObjectProperty importing slightly for subobjects */
	PPF_ParsingDefaultProperties	= 0x00008000,

	/** indicates that non-categorized transient properties should be exported (by default, they would not be) */
	PPF_IncludeTransient			= 0x00020000,

	/** modifies behavior of UProperty::Identical - indicates that the comparison is between an object and its archetype */
	PPF_DeltaComparison				= 0x00040000,

	/** indicates that we're exporting properties for display in the property window. - used to hide EditHide items in collapsed structs */
	PPF_PropertyWindow				= 0x00080000,

	PPF_NoInternalArcheType			= 0x00100000,

	/** Force fully qualified object names (for debug dumping) */
	PPF_DebugDump					= 0x00200000,

	/** Set when duplicating objects for PIE */
	PPF_DuplicateForPIE				= 0x00400000,

	/** Set when exporting just an object declaration, to be followed by another call with PPF_SeparateDefine */
	PPF_SeparateDeclare				= 0x00800000,

	/** Set when exporting just an object definition, preceded by another call with PPF_SeparateDeclare */
	PPF_SeparateDefine				= 0x01000000,

	/** Used by 'watch value' while blueprint debugging*/
	PPF_BlueprintDebugView			= 0x02000000,

	/** Exporting properties for console variables. */
	PPF_ConsoleVariable				= 0x04000000,
};

enum EPropertyExportCPPFlags
{
	/** Indicates that there are no special C++ export flags */
	CPPF_None						=	0x00000000,
	/** Indicates that we are exporting this property's CPP text for an optional parameter value */
	CPPF_OptionalValue				=	0x00000001,
	/** Indicates that we are exporting this property's CPP text for an argument or return value */
	CPPF_ArgumentOrReturnValue		=	0x00000002,
	/** Indicates thet we are exporting this property's CPP text for C++ definition of a function. */
	CPPF_Implementation = 0x00000004,
};

namespace EExportedDeclaration
{
	enum Type
	{
		Local,
		Member,
		Parameter
	};
}

//
// An UnrealScript variable.
//
class COREUOBJECT_API UProperty : public UField
{
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(UProperty,UField,CLASS_Abstract,CoreUObject,CASTCLASS_UProperty,NO_API)
	DECLARE_WITHIN(UField)

	// Persistent variables.
	int32		ArrayDim;
	int32		ElementSize;
	uint64		PropertyFlags;
	uint16		RepIndex;

	FName		RepNotifyFunc;

private:
	// In memory variables (generated during Link()).
	int32		Offset_Internal;

public:
	/** In memory only: Linked list of properties from most-derived to base **/
	UProperty*	PropertyLinkNext;
	/** In memory only: Linked list of object reference properties from most-derived to base **/
	UProperty*  NextRef;
	/** In memory only: Linked list of properties requiring destruction. Note this does not include things that will be destroyed byt he native destructor **/
	UProperty*	DestructorLinkNext;
	/** In memory only: Linked list of properties requiring post constructor initialization.**/
	UProperty*	PostConstructLinkNext;

public:
	// Constructors.
	UProperty(const class FPostConstructInitializeProperties& PCIP);
	UProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags );

	// UObject interface
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	// End of UObject interface

	/** parses and imports a text definition of a single property's value (if array, may be an individual element)
	 * also includes parsing of special operations for array properties (Add/Remove/RemoveIndex/Empty)
	 * @param Str	the string to parse
	 * @param DestData	base location the parsed property should place its data (DestData + ParsedProperty->Offset)
	 * @param ObjectStruct	the struct containing the valid fields
	 * @param SubobjectOuter	owner of DestData and any subobjects within it
	 * @param PortFlags	property import flags
	 * @param Warn	output device for any error messages
	 * @param DefinedProperties (out)	list of properties/indices that have been parsed by previous calls, so duplicate definitions cause an error
	 * @return pointer to remaining text in the stream (even on failure, but on failure it may not be advanced past the entire key/value pair)
	 */
	static const TCHAR* ImportSingleProperty( const TCHAR* Str, void* DestData, class UStruct* ObjectStruct, UObject* SubobjectOuter, int32 PortFlags,
											FOutputDevice* Warn, TArray<struct FDefinedProperty>& DefinedProperties );

	// UHT interface
	void ExportCppDeclaration( FOutputDevice& Out, EExportedDeclaration::Type DeclarationType, const TCHAR* ArrayDimOverride = NULL ) const;
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const;
	virtual bool PassCPPArgsByRef() const { return false; }

	/**
	 * Returns the C++ name of the property, including the _DEPRECATED suffix if the 
	 * property is deprecated.
	 *
	 * @return C++ name of property
	 */
	FString GetNameCPP() const;

	/**
	 * Returns the text to use for exporting this property to header file.
	 *
	 * @param	ExtendedTypeText	for property types which use templates, will be filled in with the type
	 * @param	CPPExportFlags		flags for modifying the behavior of the export
	 */
	virtual FString GetCPPType( FString* ExtendedTypeText=NULL, uint32 CPPExportFlags=0 ) const PURE_VIRTUAL(UProperty::GetCPPType,return TEXT(""););
	// End of UHT interface

private:
	/** Set the alignment offset for this property 
	 * @return the size of the structure including this newly added property
	*/
	int32 SetupOffset();

public:
	/** Return offset of property from container base. */
	FORCEINLINE int32 GetOffset_ForDebug() const
	{
		return Offset_Internal;
	}
	/** Return offset of property from container base. */
	FORCEINLINE int32 GetOffset_ForUFunction() const
	{
		return Offset_Internal;
	}
	/** Return offset of property from container base. */
	FORCEINLINE int32 GetOffset_ForGC() const
	{
		return Offset_Internal;
	}
	/** Return offset of property from container base. */
	FORCEINLINE int32 GetOffset_ReplaceWith_ContainerPtrToValuePtr() const
	{
		return Offset_Internal;
	}

	void LinkWithoutChangingOffset(FArchive& Ar)
	{
		LinkInternal(Ar);
	}

	int32 Link(FArchive& Ar)
	{
		LinkInternal(Ar);
		return SetupOffset();
	}
protected:
	virtual void LinkInternal(FArchive& Ar);
public:
	/**
	 * Determines whether the property values are identical.
	 * 
	 * @param	A			property data to be compared, already offset
	 * @param	B			property data to be compared, already offset
	 * @param	PortFlags	allows caller more control over how the property values are compared
	 *
	 * @return	true if the property values are identical
	 */
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags=0 ) const PURE_VIRTUAL(UProperty::Identical,return false;);

	/**
	 * Determines whether the property values are identical.
	 * 
	 * @param	A			property container of data to be compared, NOT offset
	 * @param	B			property container of data to be compared, NOT offset
	 * @param	PortFlags	allows caller more control over how the property values are compared
	 *
	 * @return	true if the property values are identical
	 */
	bool Identical_InContainer( const void* A, const void* B, int32 ArrayIndex = 0, uint32 PortFlags=0 ) const
	{
		return Identical( ContainerPtrToValuePtr<void>(A, ArrayIndex), B ? ContainerPtrToValuePtr<void>(B, ArrayIndex) : NULL, PortFlags );
	}

	/**
	 * Serializes the property with the struct's data residing in Data.
	 *
	 * @param	Ar				the archive to use for serialization
	 * @param	Data			pointer to the location of the beginning of the struct's property data
	 */
	void SerializeBinProperty( FArchive& Ar, void* Data )
	{
		if( ShouldSerializeValue(Ar) )
		{
			UProperty* OldSerializedProperty = GSerializedProperty;
			for (int32 Idx = 0; Idx < ArrayDim; Idx++)
			{
				GSerializedProperty = this;
				SerializeItem( Ar, ContainerPtrToValuePtr<void>(Data, Idx), 0 );
			}
			GSerializedProperty = OldSerializedProperty;
		}
	}
	/**
	 * Serializes the property with the struct's data residing in Data, unless it matches the default
	 *
	 * @param	Ar				the archive to use for serialization
	 * @param	Data			pointer to the location of the beginning of the struct's property data
	 * @param	DefaultData		pointer to the location of the beginning of the data that should be compared against
	 * @param	DefaultStruct	struct corresponding to the block of memory located at DefaultData 
	 */
	void SerializeNonMatchingBinProperty( FArchive& Ar, void* Data, void const* DefaultData, UStruct* DefaultStruct)
	{
		if( ShouldSerializeValue(Ar) )
		{
			for (int32 Idx = 0; Idx < ArrayDim; Idx++)
			{
				void *Target = ContainerPtrToValuePtr<void>(Data, Idx);
				void const* Default = ContainerPtrToValuePtrForDefaults<void>(DefaultStruct, DefaultData, Idx);
				if ( !Identical(Target, Default, Ar.GetPortFlags()) )
				{
					UProperty* OldSerializedProperty = GSerializedProperty;
					GSerializedProperty = this;
					SerializeItem( Ar, Target, 0, Default );
					GSerializedProperty = OldSerializedProperty;
				}
			}
		}
	}

	virtual void SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes=0, void const* Defaults=NULL ) const PURE_VIRTUAL(UProperty::SerializeItem,);
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope = NULL ) const PURE_VIRTUAL(UProperty::ExportTextItem,);
	const TCHAR* ImportText( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText = (FOutputDevice*)GWarn ) const
	{
		if ( !ValidateImportFlags(PortFlags,ErrorText) || Buffer == NULL )
		{
			return NULL;
		}
		return ImportText_Internal( Buffer, Data, PortFlags, OwnerObject, ErrorText );
	}
protected:
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const PURE_VIRTUAL(UProperty::ImportText,return NULL;);
public:
	
	bool ExportText_Direct( FString& ValueStr, const void* Data, const void* Delta, UObject* Parent, int32 PortFlags, UObject* ExportRootScope = NULL ) const;
	FORCEINLINE bool ExportText_InContainer( int32 Index, FString& ValueStr, const void* Data, const void* Delta, UObject* Parent, int32 PortFlags, UObject* ExportRootScope = NULL ) const
	{
		return ExportText_Direct(ValueStr, ContainerPtrToValuePtr<void>(Data, Index), ContainerPtrToValuePtrForDefaults<void>(NULL, Delta, Index), Parent, PortFlags, ExportRootScope);
	}

private:

	FORCEINLINE void* ContainerPtrToValuePtrInternal(void const* ContainerPtr, int32 ArrayIndex, bool bKnownToBeUObject) const
	{
		if (bKnownToBeUObject)
		{
			// in the future, these checks will be tested if the property is supposed be from a UClass
			// need something for networking, since those are NOT live uobjects, just memory blocks
			check(((UObject*)ContainerPtr)->IsValidLowLevel()); // Check its a valid UObject that was passed in
			check(GetOuter()->IsA(UClass::StaticClass())); // Check that the outer of this property is a UClass (not another property)

			// Check that the object we are accessing is of the class that contains this property
			checkf(((UObject*)ContainerPtr)->IsA((UClass*)GetOuter()), TEXT("'%s' is of class '%s' however property '%s' belongs to class '%s'") 
																	 , *((UObject*)ContainerPtr)->GetName()
																	 , *((UObject*)ContainerPtr)->GetClass()->GetName()
																	 , *GetName()
																	 , *((UClass*)GetOuter())->GetName());
		}
		if (0)
		{
			// in the future, these checks will be tested if the property is NOT relative to a UClass
			check(!GetOuter()->IsA(UClass::StaticClass())); // Check we are _not_ calling this on a direct child property of a UClass, you should pass in a UObject* in that case
		}
		check(ArrayIndex < ArrayDim);
		check(ContainerPtr);
		return (void*)(((uint8*)ContainerPtr) + Offset_Internal + (ElementSize * ArrayIndex));
	}
public:

	/** 
	 *	Get the pointer to property value in a supplied 'container'. 
	 *	You can _only_ call this function on a UObject* or a uint8*. If the property you want is a 'top level' UObject property, you _must_
	 *	call the function passing in a UObject* and not a uint8*. There are checks inside the function to vertify this.
	 *	@param	ContainerPtr			UObject* or uint8* to container of property value
	 *	@param	ArrayIndex				In array case, index of array element we want
	 */
	template<typename ValueType>
	FORCEINLINE ValueType* ContainerPtrToValuePtr(UObject* ContainerPtr, int32 ArrayIndex = 0) const
	{
		return (ValueType*)ContainerPtrToValuePtrInternal(ContainerPtr, ArrayIndex, true);
	}
	template<typename ValueType>
	FORCEINLINE ValueType* ContainerPtrToValuePtr(void* ContainerPtr, int32 ArrayIndex = 0) const
	{
		return (ValueType*)ContainerPtrToValuePtrInternal(ContainerPtr, ArrayIndex, false);
	}
	template<typename ValueType>
	FORCEINLINE ValueType const* ContainerPtrToValuePtr(UObject const* ContainerPtr, int32 ArrayIndex = 0) const
	{
		return (ValueType const*)ContainerPtrToValuePtrInternal(ContainerPtr, ArrayIndex, true);
	}
	template<typename ValueType>
	FORCEINLINE ValueType const* ContainerPtrToValuePtr(void const* ContainerPtr, int32 ArrayIndex = 0) const
	{
		return (ValueType const*)ContainerPtrToValuePtrInternal(ContainerPtr, ArrayIndex, false);
	}

	// Default variants, these accept and return NULL, and also check the property against the size of the container. 
	// If we copying from a baseclass (like for a CDO), then this will give NULL for a property that doesn't belong to the baseclass
	template<typename ValueType>
	FORCEINLINE ValueType* ContainerPtrToValuePtrForDefaults(UStruct* ContainerClass, UObject* ContainerPtr, int32 ArrayIndex = 0) const
	{
		if (ContainerPtr && IsInContainer(ContainerClass))
		{
			return (ValueType*)ContainerPtrToValuePtrInternal(ContainerPtr, ArrayIndex, true);
		}
		return NULL;
	}
	template<typename ValueType>
	FORCEINLINE ValueType* ContainerPtrToValuePtrForDefaults(UStruct* ContainerClass, void* ContainerPtr, int32 ArrayIndex = 0) const
	{
		if (ContainerPtr && IsInContainer(ContainerClass))
		{
			return (ValueType*)ContainerPtrToValuePtrInternal(ContainerPtr, ArrayIndex, false);
		}
		return NULL;
	}
	template<typename ValueType>
	FORCEINLINE ValueType const* ContainerPtrToValuePtrForDefaults(UStruct* ContainerClass, UObject const* ContainerPtr, int32 ArrayIndex = 0) const
	{
		if (ContainerPtr && IsInContainer(ContainerClass))
		{
			return (ValueType const*)ContainerPtrToValuePtrInternal(ContainerPtr, ArrayIndex, true);
		}
		return NULL;
	}
	template<typename ValueType>
	FORCEINLINE ValueType const* ContainerPtrToValuePtrForDefaults(UStruct* ContainerClass, void const* ContainerPtr, int32 ArrayIndex = 0) const
	{
		if (ContainerPtr && IsInContainer(ContainerClass))
		{
			return (ValueType const*)ContainerPtrToValuePtrInternal(ContainerPtr, ArrayIndex, false);
		}
		return NULL;
	}
	/** See if the offset of this property is below the supplied container size */
	FORCEINLINE bool IsInContainer(int32 ContainerSize) const
	{
		return (Offset_Internal + GetSize() <= ContainerSize);
	}
	/** See if the offset of this property is below the supplied container size */
	FORCEINLINE bool IsInContainer(UStruct* ContainerClass) const
	{
		return (Offset_Internal + GetSize() <= (ContainerClass ? ContainerClass->GetPropertiesSize() : MAX_int32));
	}

	/**
	 * Copy the value for a single element of this property.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET + INDEX * SIZE, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 *									INDEX = the index that you want to copy.  for properties which are not arrays, this should always be 0
	 *									SIZE = the ElementSize of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 * @param	InstancingParams	contains information about instancing (if any) to perform
	 */
	FORCEINLINE void CopySingleValue( void* Dest, void const* Src ) const
	{
		if(Dest != Src)
		{
			if (PropertyFlags & CPF_IsPlainOldData)
			{
				FMemory::Memcpy( Dest, Src, ElementSize );
			}
			else
			{
				CopyValuesInternal(Dest, Src, 1);
			}
		}
	}
protected:
	virtual void CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const
	{
		check(0); // if you are not memcpyable, then you need to deal with the virtual call
	}

public:
	/**
	 * Copy the value for all elements of this property.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 * @param	InstancingParams	contains information about instancing (if any) to perform
	 */
	FORCEINLINE void CopyCompleteValue( void* Dest, void const* Src ) const
	{
		if(Dest != Src)
		{
			if (PropertyFlags & CPF_IsPlainOldData)
			{
				FMemory::Memcpy( Dest, Src, ElementSize * ArrayDim );
			}
			else
			{
				CopyValuesInternal(Dest, Src, ArrayDim);
			}
		}
	}
	FORCEINLINE void CopyCompleteValue_InContainer( void* Dest, void const* Src ) const
	{
		return CopyCompleteValue(ContainerPtrToValuePtr<void>(Dest), ContainerPtrToValuePtr<void>(Src));
	}

	/**
	 * Copy the value for a single element of this property. To the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET + INDEX * SIZE, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 *									INDEX = the index that you want to copy.  for properties which are not arrays, this should always be 0
	 *									SIZE = the ElementSize of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopySingleValueToScriptVM( void* Dest, void const* Src ) const
	{
		CopySingleValue(Dest, Src);
	}
	/**
	 * Copy the value for all elements of this property. To the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopyCompleteValueToScriptVM( void* Dest, void const* Src ) const
	{
		CopyCompleteValue(Dest, Src);
	}

	/**
	 * Copy the value for a single element of this property. From the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET + INDEX * SIZE, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 *									INDEX = the index that you want to copy.  for properties which are not arrays, this should always be 0
	 *									SIZE = the ElementSize of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopySingleValueFromScriptVM( void* Dest, void const* Src ) const
	{
		CopySingleValue(Dest, Src);
	}
	/**
	 * Copy the value for all elements of this property. From the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopyCompleteValueFromScriptVM( void* Dest, void const* Src ) const
	{
		CopyCompleteValue(Dest, Src);
	}

	/**
	 * Zeros the value for this property. The existing data is assumed valid (so for example this calls FString::Empty)
	 * This only does one item and not the entire fixed size array.
	 *
	 * @param	Data		the address of the value for this property that should be cleared.
	 */
	FORCEINLINE void ClearValue( void* Data ) const
	{
		if (HasAllPropertyFlags(CPF_NoDestructor | CPF_ZeroConstructor))
		{
			FMemory::Memzero( Data, ElementSize );
		}
		else
		{
			ClearValueInternal((uint8*)Data);
		}
	}
	/**
	 * Zeros the value for this property. The existing data is assumed valid (so for example this calls FString::Empty)
	 * This only does one item and not the entire fixed size array.
	 *
	 * @param	Data		the address of the container of the value for this property that should be cleared.
	 */
	FORCEINLINE void ClearValue_InContainer( void* Data, int32 ArrayIndex = 0 ) const
	{
		if (HasAllPropertyFlags(CPF_NoDestructor | CPF_ZeroConstructor))
		{
			FMemory::Memzero( ContainerPtrToValuePtr<void>(Data, ArrayIndex), ElementSize );
		}
		else
		{
			ClearValueInternal(ContainerPtrToValuePtr<uint8>(Data, ArrayIndex));
		}
	}
protected:
	virtual void ClearValueInternal( void* Data ) const;
public:
	/**
	 * Destroys the value for this property. The existing data is assumed valid (so for example this calls FString::Empty)
	 * This does the entire fixed size array.
	 *
	 * @param	Dest		the address of the value for this property that should be destroyed.
	 */
	FORCEINLINE void DestroyValue( void* Dest ) const
	{
		if (!(PropertyFlags & CPF_NoDestructor))
		{
			DestroyValueInternal(Dest);
		}
	}
	/**
	 * Destroys the value for this property. The existing data is assumed valid (so for example this calls FString::Empty)
	 * This does the entire fixed size array.
	 *
	 * @param	Dest		the address of the container containing the value that should be destroyed.
	 */
	FORCEINLINE void DestroyValue_InContainer( void* Dest ) const
	{
		if (!(PropertyFlags & CPF_NoDestructor))
		{
			DestroyValueInternal(ContainerPtrToValuePtr<void>(Dest));
		}
	}
protected:
	virtual void DestroyValueInternal( void* Dest ) const;
public:

	/**
	 * Zeros, copies from the default, or calls the constructor for on the value for this property. 
	 * The existing data is assumed invalid (so for example this might indirectly call FString::FString,
	 * This will do the entire fixed size array.
	 *
	 * @param	Dest		the address of the value for this property that should be cleared.
	 */
	FORCEINLINE void InitializeValue( void* Dest ) const
	{
		if (PropertyFlags & CPF_ZeroConstructor)
		{
			FMemory::Memzero(Dest,ElementSize * ArrayDim);
		}
		else
		{
			InitializeValueInternal(Dest);
		}
	}
	/**
	 * Zeros, copies from the default, or calls the constructor for on the value for this property. 
	 * The existing data is assumed invalid (so for example this might indirectly call FString::FString,
	 * This will do the entire fixed size array.
	 *
	 * @param	Dest		the address of the container of value for this property that should be cleared.
	 */
	FORCEINLINE void InitializeValue_InContainer( void* Dest ) const
	{
		if (PropertyFlags & CPF_ZeroConstructor)
		{
			FMemory::Memzero(ContainerPtrToValuePtr<void>(Dest),ElementSize * ArrayDim);
		}
		else
		{
			InitializeValueInternal(ContainerPtrToValuePtr<void>(Dest));
		}
	}
protected:
	virtual void InitializeValueInternal( void* Dest ) const;
public:

	/**
	 * Verify that modifying this property's value via ImportText is allowed.
	 * 
	 * @param	PortFlags	the flags specified in the call to ImportText
	 * @param	ErrorText	[out] set to the error message that should be displayed if returns false
	 *
	 * @return	true if ImportText should be allowed
	 */
	bool ValidateImportFlags( uint32 PortFlags, FOutputDevice* ErrorText = NULL ) const;
	bool ShouldPort( uint32 PortFlags=0 ) const;
	virtual FName GetID() const;
	virtual bool IsLocalized() const;

	/**
	 * Creates new copies of components
	 * 
	 * @param	Data				pointer to the address of the instanced object referenced by this UComponentProperty
	 * @param	DefaultData			pointer to the address of the default value of the instanced object referenced by this UComponentProperty
	 * @param	Owner				the object that contains this property's data
	 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
	 */
	virtual void InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph ) {}

	virtual int32 GetMinAlignment() const { return 1; }

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference.
	 *
	 * @return true if property (or sub- properties) contain a UObject reference, false otherwise
	 */
	virtual bool ContainsObjectReference() const;

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * weak UObject reference.
	 *
	 * @return true if property (or sub- properties) contain a weak UObject reference, false otherwise
	 */
	virtual bool ContainsWeakObjectReference() const;

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference that is marked CPF_NeedCtorLink (i.e. instanced keyword).
	 *
	 * @return true if property (or sub- properties) contain a UObjectProperty that is marked CPF_NeedCtorLink, false otherwise
	 */
	FORCEINLINE bool ContainsInstancedObjectProperty() const
	{
		return (PropertyFlags&(CPF_ContainsInstancedReference | CPF_InstancedReference)) != 0;
	}

	/**
	 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
	 * to the passed in BaseOffset which is used by e.g. arrays of structs.
	 */
	virtual void EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, int32 BaseOffset );

	FORCEINLINE int32 GetSize() const
	{
		return ArrayDim * ElementSize;
	}
	bool ShouldSerializeValue( FArchive& Ar ) const;

	/**
	 * Determines whether this property value is eligible for copying when duplicating an object
	 * 
	 * @return	true if this property value should be copied into the duplicate object
	 */
	bool ShouldDuplicateValue() const
	{
		return ShouldPort() && GetOwnerClass() != UObject::StaticClass();
	}

	/**
	 * Returns the first UProperty in this property's Outer chain that does not have a UProperty for an Outer
	 */
	UProperty* GetOwnerProperty()
	{
		UProperty* Result=this;
		for ( UProperty* PropBase=Cast<UProperty>(GetOuter()); PropBase; PropBase=Cast<UProperty>(PropBase->GetOuter()) )
		{
			Result = PropBase;
		}
		return Result;
	}

	/**
	 * Returns this property's propertyflags
	 */
	FORCEINLINE uint64 GetPropertyFlags() const
	{
		return PropertyFlags;
	}
	FORCEINLINE void SetPropertyFlags( uint64 NewFlags )
	{
		PropertyFlags |= NewFlags;
	}
	FORCEINLINE void ClearPropertyFlags( uint64 NewFlags )
	{
		PropertyFlags &= ~NewFlags;
	}
	/**
	 * Used to safely check whether any of the passed in flags are set. This is required
	 * as PropertyFlags currently is a 64 bit data type and bool is a 32 bit data type so
	 * simply using PropertyFlags&CPF_MyFlagBiggerThanMaxInt won't work correctly when
	 * assigned directly to an bool.
	 *
	 * @param FlagsToCheck	Object flags to check for.
	 *
	 * @return				true if any of the passed in flags are set, false otherwise  (including no flags passed in).
	 */
	FORCEINLINE bool HasAnyPropertyFlags( uint64 FlagsToCheck ) const
	{
		return (PropertyFlags & FlagsToCheck) != 0 || FlagsToCheck == CPF_AllFlags;
	}
	/**
	 * Used to safely check whether all of the passed in flags are set. This is required
	 * as PropertyFlags currently is a 64 bit data type and bool is a 32 bit data type so
	 * simply using PropertyFlags&CPF_MyFlagBiggerThanMaxInt won't work correctly when
	 * assigned directly to an bool.
	 *
	 * @param FlagsToCheck	Object flags to check for
	 *
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	FORCEINLINE bool HasAllPropertyFlags( uint64 FlagsToCheck ) const
	{
		return ((PropertyFlags & FlagsToCheck) == FlagsToCheck);
	}

	/**
	 * Returns the replication owner, which is the property itself, or NULL if this isn't important for replication.
	 * It is relevant if the property is a net relevant and not being run in the editor
	 */
	FORCEINLINE UProperty* GetRepOwner()
	{
		return (!GIsEditor && ((PropertyFlags & CPF_Net) != 0)) ? this : NULL;
	}

	/**
	 * Editor-only properties are those that only are used with the editor is present or cannot be removed from serialisation.
	 * Editor-only properties include: EditorOnly properties
	 * Properties that cannot be removed from serialisation are:
	 *		Boolean properties (may affect GCC_BITFIELD_MAGIC computation)
	 *		Native properties (native serialisation)
	 */
	FORCEINLINE bool IsEditorOnlyProperty() const
	{
		return (PropertyFlags & CPF_DevelopmentAssets) != 0;
	}

	/** returns true, if Other is property of exactly the same type */
	virtual bool SameType(const UProperty* Other) const;
};


class COREUOBJECT_API UPropertyHelpers
{
public:

	static const TCHAR* ReadToken( const TCHAR* Buffer, FString& String, bool DottedNames = 0 );

};


/** reference to a property and optional array index used in property text import to detect duplicate references */
struct COREUOBJECT_API FDefinedProperty
{
    UProperty* Property;
    int32 Index;
    bool operator== (const FDefinedProperty& Other) const
    {
        return (Property == Other.Property && Index == Other.Index);
    }
};


/*-----------------------------------------------------------------------------
	TProperty.
-----------------------------------------------------------------------------*/


template<typename InTCppType>
class COREUOBJECT_API TPropertyTypeFundamentals
{
public:
	/** Type of the CPP property **/
	typedef InTCppType TCppType;
	enum
	{
		CPPSize = sizeof(TCppType),
		CPPAlignment = ALIGNOF(TCppType)
	};

	static FORCEINLINE TCHAR const* GetTypeName()
	{
		return TNameOf<TCppType>::GetName();
	}

	/** Get the value of the property at an address to a string, so far only numeric types are supported */
	static FString ToString(void const* A)
	{
		return TTypeToString<TCppType>::ToString(GetPropertyValue(A));
	}

	/** Set the value of the property at an address from a string, so far only numeric types are supported */
	static void FromString(void* A, const TCHAR* Buffer)
	{
		return TTypeFromString<TCppType>::FromString(*GetPropertyValuePtr(A), Buffer);
	}

	/** Convert the address of a value of the property to the proper type */
	static FORCEINLINE TCppType const* GetPropertyValuePtr(void const* A)
	{
		return (TCppType const*)A;
	}
	/** Convert the address of a value of the property to the proper type */
	static FORCEINLINE TCppType* GetPropertyValuePtr(void* A)
	{
		return (TCppType*)A;
	}
	/** Get the value of the property from an address */
	static FORCEINLINE TCppType const& GetPropertyValue(void const* A)
	{
		return *GetPropertyValuePtr(A);
	}
	/** Get the default value of the cpp type, just the default constructor, which works even for things like in32 */
	static FORCEINLINE TCppType GetDefaultPropertyValue()
	{
		return TCppType();
	}
	/** Get the value of the property from an address, unless it is NULL, then return the default value */
	static FORCEINLINE TCppType GetOptionalPropertyValue(void const* B)
	{
		return B ? GetPropertyValue(B) : GetDefaultPropertyValue();
	}
	/** Set the value of a property at an address */
	static FORCEINLINE void SetPropertyValue(void* A, TCppType const& Value)
	{
		*GetPropertyValuePtr(A) = Value;
	}
	/** Initialize the value of a property at an address, this assumes over uninitialized memory */
	static FORCEINLINE TCppType* IntializePropertyValue(void* A)
	{
		return new (A) TCppType();
	}
	/** Destroy the value of a property at an address */
	static FORCEINLINE void DestroyPropertyValue(void* A)
	{
		GetPropertyValuePtr(A)->~TCppType();
	}

protected:
	/** Get the property flags corresponding to this C++ type, from the C++ type traits system */
	static FORCEINLINE uint64 GetComputedFlagsPropertyFlags()
	{
		return 
			(TIsPODType<TCppType>::Value ? CPF_IsPlainOldData : 0) 
			| (TNoDestructorType<TCppType>::Value ? CPF_NoDestructor : 0) 
			| (TIsZeroConstructType<TCppType>::Value ? CPF_ZeroConstructor : 0);

	}
};


template<typename InTCppType, class TInPropertyBaseClass>
class COREUOBJECT_API TProperty : public TInPropertyBaseClass, public TPropertyTypeFundamentals<InTCppType>
{
public:

	typedef InTCppType TCppType;
	typedef TInPropertyBaseClass Super;
	typedef TPropertyTypeFundamentals<InTCppType> TTypeFundamentals;

	TProperty( const class FPostConstructInitializeProperties& PCIP )
		:	Super( PCIP)
	{
		SetElementSize();
	}
	TProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
		:	Super( PCIP, EC_CppProperty, InOffset, InFlags | TTypeFundamentals::GetComputedFlagsPropertyFlags())
	{
		SetElementSize();
	}

	// UHT interface
	virtual FString GetCPPType( FString* ExtendedTypeText=NULL, uint32 CPPExportFlags=0 ) const OVERRIDE
	{
		return FString(TTypeFundamentals::GetTypeName());
	}
	virtual bool PassCPPArgsByRef() const OVERRIDE
	{
		// non-pod data is passed by reference
		return !TIsPODType<TCppType>::Value;
	}
	// End of UHT interface

	// UProperty interface.
	virtual int32 GetMinAlignment() const OVERRIDE
	{
		return TTypeFundamentals::CPPAlignment;
	}
	virtual void LinkInternal(FArchive& Ar) OVERRIDE
	{
		SetElementSize();
		this->PropertyFlags |= TTypeFundamentals::GetComputedFlagsPropertyFlags();

	}
	virtual void CopyValuesInternal( void* Dest, void const* Src, int32 Count ) const OVERRIDE
	{
		for (int32 Index = 0; Index < Count; Index++)
		{
			TTypeFundamentals::GetPropertyValuePtr(Dest)[Index] = TTypeFundamentals::GetPropertyValuePtr(Src)[Index];
		}
	}
	virtual void ClearValueInternal( void* Data ) const OVERRIDE
	{
		TTypeFundamentals::SetPropertyValue(Data, TTypeFundamentals::GetDefaultPropertyValue());
	}
	virtual void InitializeValueInternal( void* Dest ) const OVERRIDE
	{
		TTypeFundamentals::IntializePropertyValue(Dest);
	}
	virtual void DestroyValueInternal( void* Dest ) const OVERRIDE
	{
		for( int32 i = 0; i < this->ArrayDim; ++i )
		{
			TTypeFundamentals::DestroyPropertyValue((uint8*) Dest + i * this->ElementSize);
		}
	}

	/** Convert the address of a container to the address of the property value, in the proper type */
	FORCEINLINE TCppType const* GetPropertyValuePtr_InContainer(void const* A, int32 ArrayIndex = 0) const
	{
		return TTypeFundamentals::GetPropertyValuePtr(Super::template ContainerPtrToValuePtr<void>(A, ArrayIndex));
	}
	/** Convert the address of a container to the address of the property value, in the proper type */
	FORCEINLINE TCppType* GetPropertyValuePtr_InContainer(void* A, int32 ArrayIndex = 0) const
	{
		return TTypeFundamentals::GetPropertyValuePtr(Super::template ContainerPtrToValuePtr<void>(A, ArrayIndex));
	}
	/** Get the value of the property from a container address */
	FORCEINLINE TCppType const& GetPropertyValue_InContainer(void const* A, int32 ArrayIndex = 0) const
	{
		return *GetPropertyValuePtr_InContainer(A, ArrayIndex);
	}
	/** Get the value of the property from a container address, unless it is NULL, then return the default value */
	FORCEINLINE TCppType GetOptionalPropertyValue_InContainer(void const* B, int32 ArrayIndex = 0) const
	{
		return B ? GetPropertyValue_InContainer(B, ArrayIndex) : TTypeFundamentals::GetDefaultPropertyValue();
	}
	/** Set the value of a property in a container */
	FORCEINLINE void SetPropertyValue_InContainer(void* A, TCppType const& Value, int32 ArrayIndex = 0) const
	{
		*GetPropertyValuePtr_InContainer(A, ArrayIndex) = Value;
	}

protected:
	FORCEINLINE void SetElementSize()
	{
		this->ElementSize = TTypeFundamentals::CPPSize;
	}
	// End of UProperty interface
};

template<typename InTCppType, class TInPropertyBaseClass>
class COREUOBJECT_API TProperty_WithEqualityAndSerializer : public TProperty<InTCppType, TInPropertyBaseClass>
{

public:
	typedef TProperty<InTCppType, TInPropertyBaseClass> Super;
	typedef InTCppType TCppType;
	typedef typename Super::TTypeFundamentals TTypeFundamentals;

	TProperty_WithEqualityAndSerializer( const class FPostConstructInitializeProperties& PCIP )
		:	Super( PCIP)
	{
	}
	TProperty_WithEqualityAndSerializer( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
		:	Super( PCIP, EC_CppProperty, InOffset, InFlags)
	{
	}

	// UProperty interface.
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags=0 ) const OVERRIDE
	{
		return TTypeFundamentals::GetPropertyValue(A) == TTypeFundamentals::GetOptionalPropertyValue(B);
	}
	virtual void SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const OVERRIDE
	{
		Ar << *TTypeFundamentals::GetPropertyValuePtr(Value);
	}
	// End of UProperty interface

};

class COREUOBJECT_API UNumericProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UNumericProperty,UProperty,CLASS_Abstract,CoreUObject,CASTCLASS_UNumericProperty)

	UNumericProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
		:	UProperty( PCIP, EC_CppProperty, InOffset, InFlags )
	{}

	// UProperty interface.
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const OVERRIDE;

	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const OVERRIDE;
	// End of UProperty interface

	// UNumericProperty interface.

	/** Return true if this property is for a floating point number **/
	virtual bool IsFloatingPoint() const
	{
		return false;
	}

	/** Return true if this property is for a integral or enum type **/
	virtual bool IsInteger() const
	{
		return true;
	}

	/** Return true if this property is a UByteProperty with a non-null Enum **/
	FORCEINLINE bool IsEnum() const
	{
		return !!GetIntPropertyEnum();
	}

	/** Return teh UEnum if this property is a UByteProperty with a non-null Enum **/
	virtual UEnum* GetIntPropertyEnum() const
	{
		return NULL;
	}

	/** 
	 * Set the value of an unsigned integral property type
	 * @param Data - pointer to property data to set
	 * @param Value - Value to set data to
	**/
	virtual void SetIntPropertyValue(void* Data, uint64 Value) const
	{
		check(0);
	}

	/** 
	 * Set the value of a signed integral property type
	 * @param Data - pointer to property data to set
	 * @param Value - Value to set data to
	**/
	virtual void SetIntPropertyValue(void* Data, int64 Value) const
	{
		check(0);
	}

	/** 
	 * Set the value of a floating point property type
	 * @param Data - pointer to property data to set
	 * @param Value - Value to set data to
	**/
	virtual void SetFloatingPointPropertyValue(void* Data, double Value) const
	{
		check(0);
	}

	/** 
	 * Set the value of any numeric type from a string point
	 * @param Data - pointer to property data to set
	 * @param Value - Value (as a string) to set 
	 * CAUTION: This routine does not do enum name conversion
	**/
	virtual void SetNumericPropertyValueFromString(void * Data, TCHAR const* Value) const
	{
		check(0);
	}

	/** 
	 * Gets the value of a signed integral property type
	 * @param Data - pointer to property data to get
	 * @return Data as a signed int
	**/
	virtual int64 GetSignedIntPropertyValue(void const* Data) const
	{
		check(0);
		return 0;
	}

	/** 
	 * Gets the value of an unsigned integral property type
	 * @param Data - pointer to property data to get
	 * @return Data as an unsigned int
	**/
	virtual uint64 GetUnsignedIntPropertyValue(void const* Data) const
	{
		check(0);
		return 0;
	}

	/** 
	 * Gets the value of an floating point property type
	 * @param Data - pointer to property data to get
	 * @return Data as a double
	**/
	virtual double GetFloatingPointPropertyValue(void const* Data) const
	{
		check(0);
		return 0.0;
	}

	/** 
	 * Get the value of any numeric type and return it as a string
	 * @param Data - pointer to property data to get
	 * @return Data as a string
	 * CAUTION: This routine does not do enum name conversion
	**/
	virtual FString GetNumericPropertyValueToString(void const* Data) const
	{
		check(0);
		return FString();
	}
	// End of UNumericProperty interface
};

template<typename InTCppType>
class COREUOBJECT_API TProperty_Numeric : public TProperty_WithEqualityAndSerializer<InTCppType, UNumericProperty>
{

public:
	typedef TProperty_WithEqualityAndSerializer<InTCppType, UNumericProperty> Super;
	typedef InTCppType TCppType;
	typedef typename Super::TTypeFundamentals TTypeFundamentals;
	TProperty_Numeric( const class FPostConstructInitializeProperties& PCIP )
		:	Super( PCIP)
	{
	}
	TProperty_Numeric( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
		:	Super( PCIP, EC_CppProperty, InOffset, InFlags)
	{
	}

	// UNumericProperty interface.

	virtual bool IsFloatingPoint() const OVERRIDE
	{
		return TIsFloatType<TCppType>::Value;
	}
	virtual bool IsInteger() const OVERRIDE
	{
		return TIsIntegralType<TCppType>::Value;
	}
	virtual void SetIntPropertyValue(void* Data, uint64 Value) const OVERRIDE
	{
		check(TIsIntegralType<TCppType>::Value);
		TTypeFundamentals::SetPropertyValue(Data, Value);
	}
	virtual void SetIntPropertyValue(void* Data, int64 Value) const OVERRIDE
	{
		check(TIsIntegralType<TCppType>::Value);
		TTypeFundamentals::SetPropertyValue(Data, Value);
	}
	virtual void SetFloatingPointPropertyValue(void* Data, double Value) const OVERRIDE
	{
		check(TIsFloatType<TCppType>::Value);
		TTypeFundamentals::SetPropertyValue(Data, Value);
	}
	virtual void SetNumericPropertyValueFromString(void * Data, TCHAR const* Value) const OVERRIDE
	{
		TTypeFundamentals::FromString(Data, Value);
	}
	virtual FString GetNumericPropertyValueToString(void const* Data) const OVERRIDE
	{
		return TTypeFundamentals::ToString(Data);
	}
	virtual int64 GetSignedIntPropertyValue(void const* Data) const OVERRIDE
	{
		check(TIsIntegralType<TCppType>::Value);
		return TTypeFundamentals::GetPropertyValue(Data);
	}
	virtual uint64 GetUnsignedIntPropertyValue(void const* Data) const OVERRIDE
	{
		check(TIsIntegralType<TCppType>::Value);
		return TTypeFundamentals::GetPropertyValue(Data);
	}
	virtual double GetFloatingPointPropertyValue(void const* Data) const OVERRIDE
	{
		check(TIsFloatType<TCppType>::Value);
		return TTypeFundamentals::GetPropertyValue(Data);
	}
	// End of UNumericProperty interface

};

/*-----------------------------------------------------------------------------
	UByteProperty.
-----------------------------------------------------------------------------*/

//
// Describes an unsigned byte value or 255-value enumeration variable.
//
class COREUOBJECT_API UByteProperty : public TProperty_Numeric<uint8>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UByteProperty,TProperty_Numeric<uint8>,0,CoreUObject,CASTCLASS_UByteProperty)

	// Variables.
	UEnum* Enum;

	UByteProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, UEnum* InEnum=NULL )
	:	TProperty_Numeric( PCIP, EC_CppProperty, InOffset, InFlags)
	,	Enum( InEnum )
	{
	}

	// UObject interface.
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End of UObject interface

	// UHT interface
	virtual FString GetCPPType( FString* ExtendedTypeText=NULL, uint32 CPPExportFlags=0 ) const OVERRIDE;
	// End of UHT interface

	// UProperty interface.
	virtual void SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const OVERRIDE;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const OVERRIDE;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const OVERRIDE;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const OVERRIDE;
	// End of UProperty interface

	// UNumericProperty interface.
	virtual UEnum* GetIntPropertyEnum() const OVERRIDE
	{
		return Enum;
	}
	// End of UNumericProperty interface

};

/*-----------------------------------------------------------------------------
	UInt8Property.
-----------------------------------------------------------------------------*/

//
// Describes a 8-bit signed integer variable.
//
class COREUOBJECT_API UInt8Property : public TProperty_Numeric<int8>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UInt8Property,TProperty_Numeric<int8>,0,CoreUObject,CASTCLASS_UInt8Property)

	UInt8Property( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
		:	TProperty_Numeric( PCIP, EC_CppProperty, InOffset, InFlags)
	{
	}
};

/*-----------------------------------------------------------------------------
	UInt16Property.
-----------------------------------------------------------------------------*/

//
// Describes a 16-bit signed integer variable.
//
class COREUOBJECT_API UInt16Property : public TProperty_Numeric<int16>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UInt16Property,TProperty_Numeric<int16>,0,CoreUObject,CASTCLASS_UInt16Property)

	UInt16Property( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
	:	TProperty_Numeric( PCIP, EC_CppProperty, InOffset, InFlags)
	{
	}
};


/*-----------------------------------------------------------------------------
	UIntProperty.
-----------------------------------------------------------------------------*/

//
// Describes a 32-bit signed integer variable.
//
class COREUOBJECT_API UIntProperty : public TProperty_Numeric<int32>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UIntProperty,TProperty_Numeric<int32>,0,CoreUObject,CASTCLASS_UIntProperty)

	UIntProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
	:	TProperty_Numeric( PCIP, EC_CppProperty, InOffset, InFlags)
	{
	}
};

/*-----------------------------------------------------------------------------
	UInt64Property.
-----------------------------------------------------------------------------*/

//
// Describes a 64-bit signed integer variable.
//
class COREUOBJECT_API UInt64Property : public TProperty_Numeric<int64>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UInt64Property,TProperty_Numeric<int64>,0,CoreUObject,CASTCLASS_UInt64Property)

	UInt64Property( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
	:	TProperty_Numeric( PCIP, EC_CppProperty, InOffset, InFlags )
	{
	}
};

/*-----------------------------------------------------------------------------
	UUInt16Property.
-----------------------------------------------------------------------------*/

//
// Describes a 16-bit unsigned integer variable.
//
class COREUOBJECT_API UUInt16Property : public TProperty_Numeric<uint16>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UUInt16Property,TProperty_Numeric<uint16>,0,CoreUObject,CASTCLASS_UUInt16Property)

	UUInt16Property( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
	:	TProperty_Numeric( PCIP, EC_CppProperty, InOffset, InFlags )
	{
	}
};

/*-----------------------------------------------------------------------------
	UUInt32Property.
-----------------------------------------------------------------------------*/

//
// Describes a 32-bit unsigned integer variable.
//
class COREUOBJECT_API UUInt32Property : public TProperty_Numeric<uint32>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UUInt32Property,TProperty_Numeric<uint32>,0,CoreUObject,CASTCLASS_UUInt32Property)

	UUInt32Property( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
	:	TProperty_Numeric( PCIP, EC_CppProperty, InOffset, InFlags )
	{
	}
};

/*-----------------------------------------------------------------------------
	UUInt64Property.
-----------------------------------------------------------------------------*/

//
// Describes a 64-bit unsigned integer variable.
//
class COREUOBJECT_API UUInt64Property : public TProperty_Numeric<uint64>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UUInt64Property,TProperty_Numeric<uint64>,0,CoreUObject,CASTCLASS_UUInt64Property)

	UUInt64Property( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
	:	TProperty_Numeric( PCIP, EC_CppProperty, InOffset, InFlags )
	{
	}
};

/*-----------------------------------------------------------------------------
	UFloatProperty.
-----------------------------------------------------------------------------*/

//
// Describes an IEEE 32-bit floating point variable.
//
class COREUOBJECT_API UFloatProperty : public TProperty_Numeric<float>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UFloatProperty,TProperty_Numeric<float>,0,CoreUObject,CASTCLASS_UFloatProperty)

	UFloatProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
		:	TProperty_Numeric( PCIP, EC_CppProperty, InOffset, InFlags )
	{
	}
};

/*-----------------------------------------------------------------------------
	UDoubleProperty.
-----------------------------------------------------------------------------*/

//
// Describes an IEEE 64-bit floating point variable.
//
class COREUOBJECT_API UDoubleProperty : public TProperty_Numeric<double>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UDoubleProperty,TProperty_Numeric<double>,0,CoreUObject,CASTCLASS_UDoubleProperty)

	UDoubleProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
		:	TProperty_Numeric( PCIP, EC_CppProperty, InOffset, InFlags )
	{
	}
};



/*-----------------------------------------------------------------------------
	UBoolProperty.
-----------------------------------------------------------------------------*/

//
// Describes a single bit flag variable residing in a 32-bit unsigned double word.
//
class COREUOBJECT_API UBoolProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR(UBoolProperty,UProperty,0,CoreUObject,CASTCLASS_UBoolProperty, NO_API)

	// Variables.
private:

	/** Size of the bitfield/bool property. Equal to ElementSize but used to check if the property has been properly initialized (0-8, where 0 means uninitialized). */
	uint8 FieldSize;
	/** Offset from the memeber variable to the byte of the property (0-7). */
	uint8 ByteOffset;
	/** Mask of the byte byte with the property value. */
	uint8 ByteMask;
	/** Mask of the field with the property value. Either equal to ByteMask or 255 in case of 'bool' type. */
	uint8 FieldMask;

public:

	UBoolProperty( const class FPostConstructInitializeProperties& PCIP );
	
	/**
	 * Constructor.
	 *
	 * @param PCIP Properties.
	 * @param ECppProperty Unused.
	 * @param InOffset Offset of the property.
	 * @param InCategory Category of the property.
	 * @param InFlags Property flags.
	 * @param InBitMask Bitmask of the bitfield this property represents.
	 * @param InElementSize Sizeof of the boolean type this property represents.
	 * @param bIsNativeBool true if this property represents C++ bool type.
	 */
	UBoolProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, uint32 InBitMask, uint32 InElementSize, bool bIsNativeBool );

	// UObject interface.
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	// End of UObject interface

	// UHT interface
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const OVERRIDE;
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const OVERRIDE;
	// End of UHT interface

	// UProperty interface.
	virtual void LinkInternal(FArchive& Ar) OVERRIDE;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const OVERRIDE;
	virtual void SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const OVERRIDE;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const OVERRIDE;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const OVERRIDE;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const OVERRIDE;
	virtual void CopyValuesInternal( void* Dest, void const* Src, int32 Count ) const OVERRIDE;
	virtual void ClearValueInternal( void* Data ) const OVERRIDE;
	virtual void InitializeValueInternal( void* Dest ) const OVERRIDE;
	virtual int32 GetMinAlignment() const OVERRIDE;
	// End of UProperty interface

	// Emulate the CPP type API, see TPropertyTypeFundamentals
	// this is incomplete as some operations make no sense for bitfields, for example they don't have a usable address
	typedef bool TCppType;
	FORCEINLINE bool GetPropertyValue(void const* A) const
	{
		check(FieldSize != 0);
		uint8* ByteValue = (uint8*)A + ByteOffset;
		return !!(*ByteValue & FieldMask);
	}
	FORCEINLINE bool GetPropertyValue_InContainer(void const* A, int32 ArrayIndex = 0) const
	{
		return GetPropertyValue(ContainerPtrToValuePtr<void>(A, ArrayIndex));
	}
	static FORCEINLINE bool GetDefaultPropertyValue()
	{
		return false;
	}
	FORCEINLINE bool GetOptionalPropertyValue(void const* B) const
	{
		return B ? GetPropertyValue(B) : GetDefaultPropertyValue();
	}
	FORCEINLINE bool GetOptionalPropertyValue_InContainer(void const* B, int32 ArrayIndex = 0) const
	{
		return B ? GetPropertyValue_InContainer(B, ArrayIndex) : GetDefaultPropertyValue();
	}
	FORCEINLINE void SetPropertyValue(void* A, bool Value) const
	{
		check(FieldSize != 0);
		uint8* ByteValue = (uint8*)A + ByteOffset;
		*ByteValue = ((*ByteValue) & ~FieldMask) | (Value ? ByteMask : 0);
	}
	FORCEINLINE void SetPropertyValue_InContainer(void* A, bool Value, int32 ArrayIndex = 0) const
	{
		SetPropertyValue(ContainerPtrToValuePtr<void>(A, ArrayIndex), Value);
	}
	// End of the CPP type API

	/** 
	 * Sets the bitfield/bool type and size. 
	 * This function must be called before UBoolProperty can be used.
	 *
	 * @param InSize size of the bitfield/bool type.
	 * @param bIsNativeBool true if this property represents C++ bool type.
	 */
	void SetBoolSize( const uint32 InSize, const bool bIsNativeBool = false, const uint32 InBitMask = 0 );

	/**
	 * If the return value is true this UBoolProperty represents C++ bool type.
	 */
	FORCEINLINE bool IsNativeBool() const
	{
		return FieldMask == 0xff;
	}
};

/*-----------------------------------------------------------------------------
	UObjectPropertyBase.
-----------------------------------------------------------------------------*/

//
// Describes a reference variable to another object which may be nil.
//
class COREUOBJECT_API UObjectPropertyBase : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UObjectPropertyBase,UProperty,CLASS_Abstract,CoreUObject,CASTCLASS_UObjectPropertyBase)

	// Variables.
	class UClass* PropertyClass;

	UObjectPropertyBase( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass=NULL )
	:	UProperty( PCIP, EC_CppProperty, InOffset, InFlags )
	,	PropertyClass( InClass )
	{}

	// UObject interface
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End of UObject interface

	// UProperty interface
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const OVERRIDE;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const OVERRIDE;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const OVERRIDE;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const OVERRIDE;
	virtual FName GetID() const OVERRIDE;
	virtual void InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph ) OVERRIDE;
	virtual bool SameType(const UProperty* Other) const OVERRIDE;
	/**
	 * Copy the value for a single element of this property. To the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET + INDEX * SIZE, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 *									INDEX = the index that you want to copy.  for properties which are not arrays, this should always be 0
	 *									SIZE = the ElementSize of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopySingleValueToScriptVM( void* Dest, void const* Src ) const
	{
		*(UObject**)Dest = GetObjectPropertyValue(Src);
	}
	/**
	 * Copy the value for all elements of this property. To the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopyCompleteValueToScriptVM( void* Dest, void const* Src ) const
	{
		for (int32 Index = 0; Index < ArrayDim; Index++)
		{
			((UObject**)Dest)[Index] = GetObjectPropertyValue(((uint8*)Src) + Index * ElementSize);
		}
	}

	/**
	 * Copy the value for a single element of this property. From the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET + INDEX * SIZE, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 *									INDEX = the index that you want to copy.  for properties which are not arrays, this should always be 0
	 *									SIZE = the ElementSize of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopySingleValueFromScriptVM( void* Dest, void const* Src ) const
	{
		SetObjectPropertyValue(Dest, *(UObject**)Src);
	}
	/**
	 * Copy the value for all elements of this property. From the script VM.
	 * 
	 * @param	Dest				the address where the value should be copied to.  This should always correspond to the BASE + OFFSET, where
	 *									BASE = (for member properties) the address of the UObject which contains this data, (for locals/parameters) the address of the space allocated for the function's locals
	 *									OFFSET = the Offset of this UProperty
	 * @param	Src					the address of the value to copy from. should be evaluated the same way as Dest
	 */
	virtual void CopyCompleteValueFromScriptVM( void* Dest, void const* Src ) const
	{
		checkSlow(ElementSize == sizeof(UObject*)); // the idea that script pointers are the same size as weak pointers is maybe required, maybe not
		for (int32 Index = 0; Index < ArrayDim; Index++)
		{
			SetObjectPropertyValue(((uint8*)Dest) + Index * ElementSize, ((UObject**)Src)[Index]);
		}
	}

	// End of UProperty interface

	// UObjectPropertyBase interface
public:

	/**
	 * Parses a text buffer into an object reference.
	 *
	 * @param	Property			the property that the value is being importing to
	 * @param	OwnerObject			the object that is importing the value; used for determining search scope.
	 * @param	RequiredMetaClass	the meta-class for the object to find; if the object that is resolved is not of this class type, the result is NULL.
	 * @param	PortFlags			bitmask of EPropertyPortFlags that can modify the behavior of the search
	 * @param	Buffer				the text to parse; should point to a textual representation of an object reference.  Can be just the object name (either fully 
	 *								fully qualified or not), or can be formatted as a const object reference (i.e. SomeClass'SomePackage.TheObject')
	 *								When the function returns, Buffer will be pointing to the first character after the object value text in the input stream.
	 * @param	ResolvedValue		receives the object that is resolved from the input text.
	 *
	 * @return	true if the text is successfully resolved into a valid object reference of the correct type, false otherwise.
	 */
	static bool ParseObjectPropertyValue( const UProperty* Property, UObject* OwnerObject, UClass* RequiredMetaClass, uint32 PortFlags, const TCHAR*& Buffer, UObject*& out_ResolvedValue );
	static UObject* FindImportedObject( const UProperty* Property, UObject* OwnerObject, UClass* ObjectClass, UClass* RequiredMetaClass, const TCHAR* Text, uint32 PortFlags = 0);

	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const
	{
		check(0);
		return NULL;
	}
	FORCEINLINE UObject* GetObjectPropertyValue_InContainer(const void* PropertyValueAddress, int32 ArrayIndex = 0) const
	{
		return GetObjectPropertyValue(ContainerPtrToValuePtr<void>(PropertyValueAddress, ArrayIndex));
	}
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const
	{
		check(0);
	}
	FORCEINLINE void SetObjectPropertyValue_InContainer(void* PropertyValueAddress, UObject* Value, int32 ArrayIndex = 0) const
	{
		SetObjectPropertyValue(ContainerPtrToValuePtr<void>(PropertyValueAddress, ArrayIndex), Value);
	}

protected:
	virtual bool AllowCrossLevel() const
	{
		return false;
	}

	virtual void CheckValidObject(void* Value) const;
	// End of UObjectPropertyBase interface

};

template<typename InTCppType>
class COREUOBJECT_API TUObjectPropertyBase : public TProperty<InTCppType, UObjectPropertyBase>
{
public:
	typedef TProperty<InTCppType, UObjectPropertyBase> Super;
	typedef InTCppType TCppType;
	typedef typename Super::TTypeFundamentals TTypeFundamentals;

	TUObjectPropertyBase( const class FPostConstructInitializeProperties& PCIP )
		:	Super( PCIP)
	{
	}
	TUObjectPropertyBase( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass )
		:	Super( PCIP, EC_CppProperty, InOffset, InFlags)
	{
		this->PropertyClass = InClass;
	}

	// UProperty interface.
	virtual bool ContainsObjectReference() const OVERRIDE
	{
		return !TIsWeakPointerType<InTCppType>::Value;
	}
	virtual bool ContainsWeakObjectReference() const OVERRIDE
	{
		return TIsWeakPointerType<InTCppType>::Value;
	}
	// End of UProperty interface
};


//
// Describes a reference variable to another object which may be nil.
//
class COREUOBJECT_API UObjectProperty : public TUObjectPropertyBase<UObject*>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UObjectProperty,TUObjectPropertyBase<UObject*>,0,CoreUObject,CASTCLASS_UObjectProperty)

	UObjectProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass )
	:	TUObjectPropertyBase( PCIP, EC_CppProperty, InOffset, InFlags, InClass )
	{
	}

	// UHT interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  OVERRIDE;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const OVERRIDE;
	// End of UHT interface

	// UProperty interface
	virtual void SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const OVERRIDE;
	virtual void EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, int32 BaseOffset ) OVERRIDE;
	// End of UProperty interface
	// UObjectPropertyBase interface
	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const OVERRIDE
	{
		return GetPropertyValue(PropertyValueAddress);
	}
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const OVERRIDE
	{
		SetPropertyValue(PropertyValueAddress, Value);
	}
	// End of UObjectPropertyBase interface
};

//
// Describes a reference variable to another object which may be nil, and may turn nil at any point
//
class COREUOBJECT_API UWeakObjectProperty : public TUObjectPropertyBase<FWeakObjectPtr>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UWeakObjectProperty,TUObjectPropertyBase<FWeakObjectPtr>,0,CoreUObject,CASTCLASS_UWeakObjectProperty)

	UWeakObjectProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass )
	:	TUObjectPropertyBase( PCIP, EC_CppProperty, InOffset, InFlags, InClass )
	{
	}
	
	// UHT interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  OVERRIDE;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const OVERRIDE;
	// End of UHT interface

	// UProperty interface
	virtual void SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const OVERRIDE;
	// End of UProperty interface

	// UObjectProperty interface
	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const OVERRIDE
	{
		return GetPropertyValue(PropertyValueAddress).Get();
	}
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const OVERRIDE
	{
		SetPropertyValue(PropertyValueAddress, TCppType(Value));
	}
	// End of UObjectProperty interface
};

//
// Describes a reference variable to another object which may be nil, and will become valid or invalid at any point
//
class COREUOBJECT_API ULazyObjectProperty : public TUObjectPropertyBase<FLazyObjectPtr>
{
	DECLARE_CASTED_CLASS_INTRINSIC(ULazyObjectProperty,TUObjectPropertyBase<FLazyObjectPtr>,0,CoreUObject,CASTCLASS_ULazyObjectProperty)

	ULazyObjectProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass )
		:	TUObjectPropertyBase( PCIP, EC_CppProperty, InOffset, InFlags, InClass )
	{
	}

	// UHT interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  OVERRIDE;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const OVERRIDE;
	// End of UHT interface

	// UProperty interface
	virtual FName GetID() const OVERRIDE;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const OVERRIDE;
	virtual void SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const OVERRIDE;
	// End of UProperty interface

	// UObjectProperty interface
	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const OVERRIDE
	{
		return GetPropertyValue(PropertyValueAddress).Get();
	}
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const OVERRIDE
	{
		SetPropertyValue(PropertyValueAddress, TCppType(Value));
	}
	virtual bool AllowCrossLevel() const OVERRIDE
	{
		return true;
	}
	// End of UObjectProperty interface
};

//
// Describes a reference variable to another object which may be nil, and will become valid or invalid at any point
//
class COREUOBJECT_API UAssetObjectProperty : public TUObjectPropertyBase<FAssetPtr>
{
	DECLARE_CASTED_CLASS_INTRINSIC(UAssetObjectProperty,TUObjectPropertyBase<FAssetPtr>,0,CoreUObject,CASTCLASS_UAssetObjectProperty)

	UAssetObjectProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InClass )
		:	TUObjectPropertyBase( PCIP, EC_CppProperty, InOffset, InFlags, InClass )
	{}

	// UHT interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  OVERRIDE;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const OVERRIDE;
	// End of UHT interface

	// UProperty interface
	virtual FName GetID() const OVERRIDE;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const OVERRIDE;
	virtual void SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const OVERRIDE;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const OVERRIDE;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const OVERRIDE;
	// End of UProperty interface

	// UObjectProperty interface
	virtual UObject* GetObjectPropertyValue(const void* PropertyValueAddress) const OVERRIDE
	{
		return GetPropertyValue(PropertyValueAddress).Get();
	}
	virtual void SetObjectPropertyValue(void* PropertyValueAddress, UObject* Value) const OVERRIDE
	{
		SetPropertyValue(PropertyValueAddress, TCppType(Value));
	}
	virtual bool AllowCrossLevel() const OVERRIDE
	{
		return true;
	}
	// End of UObjectProperty interface
};

/*-----------------------------------------------------------------------------
	UClassProperty.
-----------------------------------------------------------------------------*/

//
// Describes a reference variable to another object which may be nil.
//
class COREUOBJECT_API UClassProperty : public UObjectProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UClassProperty,UObjectProperty,0,CoreUObject,CASTCLASS_UClassProperty)

	// Variables.
	class UClass* MetaClass;
public:
	UClassProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InMetaClass )
	:	UObjectProperty( PCIP, EC_CppProperty, InOffset, InFlags, UClass::StaticClass() )
	,	MetaClass( InMetaClass )
	{
	}

	// UObject interface
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End of UObject interface

	// UHT interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  OVERRIDE;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const OVERRIDE;
	// End of UHT interface

	// UProperty interface
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const OVERRIDE;
	virtual bool SameType(const UProperty* Other) const OVERRIDE;
	// End of UProperty interface

protected:
	virtual void CheckValidObject(void* Value) const OVERRIDE;
};

/*-----------------------------------------------------------------------------
	UAssetSubclassOfProperty.
-----------------------------------------------------------------------------*/

//
// Describes a reference variable to another class which may be nil, and will become valid or invalid at any point
//
class COREUOBJECT_API UAssetClassProperty : public UAssetObjectProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UAssetClassProperty,UAssetObjectProperty,0,CoreUObject,CASTCLASS_UAssetClassProperty)

	// Variables.
	class UClass* MetaClass;
public:
	UAssetClassProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InMetaClass )
		:	Super(PCIP, EC_CppProperty, InOffset, InFlags, UClass::StaticClass() )
		,	MetaClass( InMetaClass )
	{}

	// UHT interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  OVERRIDE;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const OVERRIDE;
	// End of UHT interface

	// UObject interface
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End of UObject interface

	// UProperty interface
	virtual bool SameType(const UProperty* Other) const OVERRIDE;
	// End of UProperty interface
};

/*-----------------------------------------------------------------------------
	UInterfaceProperty.
-----------------------------------------------------------------------------*/

/**
 * This variable type provides safe access to a native interface pointer.  The data class for this variable is FScriptInterface, and is exported to auto-generated
 * script header files as a TScriptInterface.
 */

// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty<FScriptInterface, UProperty> UInterfaceProperty_Super;

class COREUOBJECT_API UInterfaceProperty : public UInterfaceProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UInterfaceProperty,UInterfaceProperty_Super,0,CoreUObject,CASTCLASS_UInterfaceProperty)

	/** The native interface class that this interface property refers to */
	class	UClass*		InterfaceClass;
public:
	typedef UInterfaceProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UInterfaceProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, UClass* InInterfaceClass )
		:	UInterfaceProperty_Super( PCIP, EC_CppProperty, InOffset, (InFlags & ~CPF_InterfaceClearMask) )
		,	InterfaceClass( InInterfaceClass )
	{
	}

	// UHT interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  OVERRIDE;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const OVERRIDE;
	// End of UHT interface

	// UProperty interface
	virtual void LinkInternal(FArchive& Ar) OVERRIDE;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const OVERRIDE;
	virtual void SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const OVERRIDE;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const OVERRIDE;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const OVERRIDE;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const OVERRIDE;
	virtual bool ContainsObjectReference() const OVERRIDE;
	virtual bool SameType(const UProperty* Other) const OVERRIDE;
	// End of UProperty interface

	// UObject interface
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	virtual void EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, int32 BaseOffset ) OVERRIDE;
	// End of UObject interface
};

/*-----------------------------------------------------------------------------
	UNameProperty.
-----------------------------------------------------------------------------*/

//
// Describes a name variable pointing into the global name table.
//

// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty_WithEqualityAndSerializer<FName, UProperty> UNameProperty_Super;

class COREUOBJECT_API UNameProperty : public UNameProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UNameProperty,UNameProperty_Super,0,CoreUObject,CASTCLASS_UNameProperty)
public:
	typedef UNameProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UNameProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
	:	UNameProperty_Super( PCIP, EC_CppProperty, InOffset, InFlags )
	{
	}

	// UProperty interface
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const OVERRIDE;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const OVERRIDE;
	// End of UProperty interface
};

/*-----------------------------------------------------------------------------
	UStrProperty.
-----------------------------------------------------------------------------*/

//
// Describes a dynamic string variable.
//

// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty_WithEqualityAndSerializer<FString, UProperty> UStrProperty_Super;

class COREUOBJECT_API UStrProperty : public UStrProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UStrProperty,UStrProperty_Super,0,CoreUObject,CASTCLASS_UStrProperty)
public:
	typedef UStrProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UStrProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
	:	UStrProperty_Super( PCIP, EC_CppProperty, InOffset, InFlags)
	{
	}

	// UProperty interface
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const OVERRIDE;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const OVERRIDE;
	// End of UProperty interface
};

/*-----------------------------------------------------------------------------
	UArrayProperty.
-----------------------------------------------------------------------------*/

//
// Describes a dynamic array.
//

// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty<FScriptArray, UProperty> UArrayProperty_Super;

class COREUOBJECT_API UArrayProperty : public UArrayProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UArrayProperty,UArrayProperty_Super,0,CoreUObject,CASTCLASS_UArrayProperty)

	// Variables.
	UProperty* Inner;

public:
	typedef UArrayProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UArrayProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags )
	:	UArrayProperty_Super( PCIP, EC_CppProperty, InOffset, InFlags)
	{
	}

	// UObject interface
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End of UObject interface

	// UField interface
	virtual void AddCppProperty( UProperty* Property ) OVERRIDE;
	// End of UField interface

	// UProperty interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  OVERRIDE;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const OVERRIDE;
	virtual void LinkInternal(FArchive& Ar) OVERRIDE;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const OVERRIDE;
	virtual void SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const OVERRIDE;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const OVERRIDE;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const OVERRIDE;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const OVERRIDE;
	virtual void CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const OVERRIDE;
	virtual void ClearValueInternal( void* Data ) const OVERRIDE;
	virtual void DestroyValueInternal( void* Dest ) const OVERRIDE;
	virtual bool IsLocalized() const OVERRIDE;
	virtual bool PassCPPArgsByRef() const OVERRIDE;
	virtual void InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph ) OVERRIDE;
	virtual bool ContainsObjectReference() const OVERRIDE;
	virtual bool ContainsWeakObjectReference() const OVERRIDE;
	virtual void EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, int32 BaseOffset ) OVERRIDE;
	virtual bool SameType(const UProperty* Other) const OVERRIDE;
	// End of UProperty interface
};

/**
 * FScriptArrayHelper: Pseudo dynamic array. Used to work with array properties in a sensible way.
 **/
class FScriptArrayHelper
{
public:
	/**
	 *	Constructor, brings together a property and an instance of the property located in memory
	 *	@param	InProperty: the property associated with this memory
	 *	@param	InArray: pointer to raw memory that corresponds to this array. This can be NULL, and sometimes is, but in that case almost all operations will crash.
	**/
	FORCEINLINE FScriptArrayHelper(const UArrayProperty* InProperty, const void *InArray)
		: InnerProperty(InProperty->Inner)
		, Array((FScriptArray*)InArray)  //@todo, we are casting away the const here
		, ElementSize(InnerProperty->ElementSize)
	{
		check(ElementSize > 0);
		check(InnerProperty);
	}

	/**
	 *	Index range check
	 *	@param	Index: Index to check
	 *	@return true if accessing this element is legal.
	**/
	FORCEINLINE bool IsValidIndex( int32 Index ) const
	{
		return Index >= 0 && Index < Num();
	}
	/**
	 *	Return the number of elements in the array.
	 *	@return	The number of elements in the array.
	**/
	FORCEINLINE int32 Num() const
	{
		checkSlow(Array->Num() >= 0); 
		return Array->Num();
	}
	/**
	 *	Static version of Num() used when you don't need to bother to construct a FScriptArrayHelper. Returns the number of elements in the array.
	 *	@param	Target: pointer to the raw memory associated with a FScriptArray
	 *	@return The number of elements in the array.
	**/
	static FORCEINLINE int32 Num(const void *Target)
	{
		checkSlow(((const FScriptArray*)Target)->Num() >= 0); 
		return ((const FScriptArray*)Target)->Num();
	}
	/**
	 *	Returns a uint8 pointer to an element in the array
	 *	@param	Index: index of the item to return a pointer to.
	 *	@return	Pointer to this element, or NULL if the array is empty
	**/
	FORCEINLINE uint8* GetRawPtr(int32 Index = 0)
	{
		if (!Num())
		{
			checkSlow(!Index);
			return NULL;
		}
		checkSlow(IsValidIndex(Index)); 
		return (uint8*)Array->GetData() + Index * ElementSize;
	}
	/**
	*	Empty the array, then add blank, constructed values to a given size.
	*	@param	Count: the number of items the array will have on completion.
	**/
	void EmptyAndAddValues(int32 Count)
	{ 
		check(Count>=0);
		checkSlow(Num() >= 0); 
		EmptyValues(Count);
		if (Count)
		{
			AddValues(Count);
		}
	}
	/**
	*	Empty the array, then add uninitialized values to a given size.
	*	@param	Count: the number of items the array will have on completion.
	**/
	void EmptyAndAddUninitializedValues(int32 Count)
	{ 
		check(Count>=0);
		checkSlow(Num() >= 0); 
		EmptyValues(Count);
		if (Count)
		{
			AddUninitializedValues(Count);
		}
	}
	/**
	*	Expand the array, if needed, so that the given index is valid
	*	@param	Index: index for the item that we want to ensure is valid
	*	@return true if expansion was necessary
	*	NOTE: This is not a count, it is an INDEX, so the final count will be at least Index+1 this matches the usage.
	**/
	bool ExpandForIndex(int32 Index)
	{ 
		check(Index>=0);
		checkSlow(Num() >= 0); 
		if (Index >= Num())
		{
			AddValues(Index - Num() + 1);
			return true;
		}
		return false;
	}
	/**
	*	Add or remove elements to set the array to a given size.
	*	@param	Count: the number of items the array will have on completion.
	**/
	void Resize(int32 Count)
	{ 
		check(Count>=0);
		int32 OldNum = Num();
		if (Count > OldNum)
		{
			AddValues(Count - OldNum);
		}
		else if (Count < OldNum)
		{
			RemoveValues(Count, OldNum - Count);
		}
	}
	/**
	*	Add blank, constructed values to the end of the array.
	*	@param	Count: the number of items to insert.
	*	@return	the index of the first newly added item.
	**/
	int32 AddValues(int32 Count)
	{ 
		const int32 OldNum = AddUninitializedValues(Count);		
		ConstructItems(OldNum, Count);
		return OldNum;
	}
	/**
	*	Add a blank, constructed values to the end of the array.
	*	@return	the index of the newly added item.
	**/
	FORCEINLINE int32 AddValue()
	{ 
		return AddValues(1);
	}
	/**
	 *	Insert blank, constructed values into the array.
	 *	@param	Index: index of the first inserted item after completion
	 *	@param	Count: the number of items to insert.
	**/
	void InsertValues( int32 Index, int32 Count = 1)
	{
		check(Count>0);
		check(Index>=0 && Index <= Num());
		Array->Insert(Index, Count, ElementSize);
		ConstructItems(Index, Count);
	}
	/**
	 *	Remove all values from the array, calling destructors, etc as appropriate.
	 *	@param Slack: used to presize the array for a subsequent add, to avoid reallocation.
	**/
	void EmptyValues(int32 Slack = 0)
	{
		checkSlow(Slack>=0);
		const int32 OldNum = Num();
		if (OldNum)
		{
			DestructItems(0, OldNum);
		}
		if (OldNum || Slack)
		{
			Array->Empty(Slack, ElementSize);
		}
	}
	/**
	 *	Remove values from the array, calling destructors, etc as appropriate.
	 *	@param Index: first item to remove.
	 *	@param Count: number of items to remove.
	**/
	void RemoveValues(int32 Index, int32 Count = 1)
	{
		check(Count>0);
		check(Index>=0 && Index + Count <= Num());
		DestructItems(Index, Count);
		Array->Remove(Index, Count, ElementSize);
	}

	/**
	*	Clear values in the array. The meaning of clear is defined by the property system.
	*	@param Index: first item to clear.
	*	@param Count: number of items to clear.
	**/
	void ClearValues(int32 Index, int32 Count = 1)
	{
		check(Count>0);
		check(Index>=0);
		ClearItems(Index, Count);
	}

	/**
	 *	Swap two elements in the array, does not call constructors and destructors
	 *	@param A index of one item to swap.
	 *	@param B index of the other item to swap.
	**/
	void SwapValues(int32 A, int32 B)
	{
		Array->SwapMemory(A, B, ElementSize);
	}
	/**
	 *	Used by memory counting archives to accumlate the size of this array.
	 *	@param Ar archive to accumulate sizes
	**/
	void CountBytes( FArchive& Ar  )
	{
		Array->CountBytes(Ar, ElementSize);
	}		
private:

	/**
	*	Add unintialized values to the end of the array.
	*	@param	Count: the number of items to insert.
	*	@return	the index of the first newly added item.
	**/
	int32 AddUninitializedValues(int32 Count)
	{ 
		check(Count>0);
		checkSlow(Num() >= 0); 
		const int32 OldNum = Array->Add(Count, ElementSize);		
		return OldNum;
	}
	/**
	 *	Internal function to call into the property system to construct / initialize elements.
	 *	@param Index: first item to .
	 *	@param Count: number of items to .
	**/
	void ConstructItems(int32 Index, int32 Count)
	{
		checkSlow(Count > 0);
		checkSlow(Index >= 0); 
		checkSlow(Index <= Num());
		checkSlow(Index + Count <= Num());
		uint8 *Dest = GetRawPtr(Index);
		if (InnerProperty->PropertyFlags & CPF_ZeroConstructor)
		{
			FMemory::Memzero(Dest, Count * ElementSize);
		}
		else
		{
		for (int32 LoopIndex = 0 ; LoopIndex < Count; LoopIndex++, Dest += ElementSize)
		{
			InnerProperty->InitializeValue(Dest);
		}
	}
	}
	/**
	 *	Internal function to call into the property system to destruct elements.
	 *	@param Index: first item to .
	 *	@param Count: number of items to .
	**/
	void DestructItems(int32 Index, int32 Count)
	{
		if (!(InnerProperty->PropertyFlags & (CPF_IsPlainOldData | CPF_NoDestructor)))
		{
		checkSlow(Count > 0);
		checkSlow(Index >= 0); 
		checkSlow(Index < Num());
		checkSlow(Index + Count <= Num());
		uint8 *Dest = GetRawPtr(Index);
		for (int32 LoopIndex = 0 ; LoopIndex < Count; LoopIndex++, Dest += ElementSize)
		{
			InnerProperty->DestroyValue(Dest);
		}
	}
	}
	/**
	 *	Internal function to call into the property system to clear elements.
	 *	@param Index: first item to .
	 *	@param Count: number of items to .
	**/
	void ClearItems(int32 Index, int32 Count)
	{
		checkSlow(Count > 0);
		checkSlow(Index >= 0); 
		checkSlow(Index < Num());
		checkSlow(Index + Count <= Num());
		uint8 *Dest = GetRawPtr(Index);
		if ((InnerProperty->PropertyFlags & (CPF_ZeroConstructor | CPF_NoDestructor)) == (CPF_ZeroConstructor | CPF_NoDestructor))
		{
			FMemory::Memzero(Dest, Count * ElementSize);
		}
		else
		{
		for (int32 LoopIndex = 0 ; LoopIndex < Count; LoopIndex++, Dest += ElementSize)
		{
			InnerProperty->ClearValue(Dest);
		}
	}
	}

	const UProperty* InnerProperty;
	FScriptArray* Array;
	int32 ElementSize;
};

class FScriptArrayHelper_InContainer : public FScriptArrayHelper
{
public:
	FORCEINLINE FScriptArrayHelper_InContainer(const UArrayProperty* InProperty, const void* InArray, int32 FixedArrayIndex=0)
		:FScriptArrayHelper(InProperty, InProperty->ContainerPtrToValuePtr<void>(InArray, FixedArrayIndex))
	{
	}
};

/*-----------------------------------------------------------------------------
	UAttributeProperty.
-----------------------------------------------------------------------------*/

//
// Describes a TAttribute .
//

class COREUOBJECT_API UAttributeProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UAttributeProperty,UProperty,0,CoreUObject,CASTCLASS_UAttributeProperty)

	// Variables.
	UProperty* Inner;
public:
	UAttributeProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, int32 InElementSize )
	:	UProperty( PCIP, EC_CppProperty, InOffset, InFlags)
	{
		ElementSize = InElementSize;
	}

	// UObject interface
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End of UObject interface

	// UField interface
	virtual void AddCppProperty( UProperty* Property ) OVERRIDE;
	// End of UField interface

	// UProperty interface
	virtual FString GetCPPMacroType(FString& ExtendedTypeText) const  OVERRIDE;
	virtual FString GetCPPType(FString* ExtendedTypeText, uint32 CPPExportFlags) const OVERRIDE;
	virtual void LinkInternal(FArchive& Ar) OVERRIDE;
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags) const OVERRIDE;
	virtual void SerializeItem(FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults) const OVERRIDE;
	virtual bool NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL) const OVERRIDE;
	virtual void ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const OVERRIDE;
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText) const OVERRIDE;
	virtual void CopyValuesInternal(void* Dest, void const* Src, int32 Count) const OVERRIDE;
	virtual void ClearValueInternal(void* Data) const OVERRIDE;
	virtual void DestroyValueInternal(void* Dest) const OVERRIDE;
	virtual bool IsLocalized() const OVERRIDE;
	virtual bool PassCPPArgsByRef() const OVERRIDE;
	virtual void InstanceSubobjects(void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph) OVERRIDE;
	virtual bool ContainsObjectReference() const OVERRIDE;
	virtual bool ContainsWeakObjectReference() const OVERRIDE;
	virtual void EmitReferenceInfo(FGCReferenceTokenStream* ReferenceTokenStream, int32 BaseOffset) OVERRIDE;
	virtual bool SameType(const UProperty* Other) const OVERRIDE;
	// End of UProperty interface
};

/*-----------------------------------------------------------------------------
	UStructProperty.
-----------------------------------------------------------------------------*/

//
// Describes a structure variable embedded in (as opposed to referenced by) 
// an object.
//
class COREUOBJECT_API UStructProperty : public UProperty
{
	DECLARE_CASTED_CLASS_INTRINSIC(UStructProperty,UProperty,0,CoreUObject,CASTCLASS_UStructProperty)

	// Variables.
	class UScriptStruct* Struct;
public:
	UStructProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, UScriptStruct* InStruct );

	// UObject interface
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End of UObject interface

	// UProperty interface
	virtual FString GetCPPMacroType( FString& ExtendedTypeText ) const  OVERRIDE;
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const OVERRIDE;
	virtual void LinkInternal(FArchive& Ar) OVERRIDE;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const OVERRIDE;
	virtual void SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const OVERRIDE;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const OVERRIDE;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const OVERRIDE;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const OVERRIDE;
	virtual void CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const OVERRIDE;
	virtual void ClearValueInternal( void* Data ) const OVERRIDE;
	virtual void DestroyValueInternal( void* Dest ) const OVERRIDE;
	virtual void InitializeValueInternal( void* Dest ) const OVERRIDE;
	virtual bool IsLocalized() const OVERRIDE;
	virtual void InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph ) OVERRIDE;
	virtual int32 GetMinAlignment() const OVERRIDE;
	virtual bool ContainsObjectReference() const OVERRIDE;
	virtual bool ContainsWeakObjectReference() const OVERRIDE;
	virtual void EmitReferenceInfo( FGCReferenceTokenStream* ReferenceTokenStream, int32 BaseOffset ) OVERRIDE;
	virtual bool SameType(const UProperty* Other) const OVERRIDE;
	// End of UProperty interface

	bool UseNativeSerialization() const;
	bool UseBinarySerialization(const FArchive& Ar) const;
	bool UseBinaryOrNativeSerialization(const FArchive& Ar) const;

public:

#if HACK_HEADER_GENERATOR
	/**
	 * Some native structs, like FIntPoint, FIntRect, FVector2D, FVector, FPlane, FRotator, FCylinder have a default constructor that does nothing and require EForceInit
	 * Since it is name-based, this is not a fast routine intended to be used for header generation only
	 * 
	 * @return	true if this struct requires the EForceInit constructor to initialize
	 */
	bool HasNoOpConstructor() const;
#endif

protected:
	static void UStructProperty_ExportTextItem(class UScriptStruct* InStruct, FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope);
};

/*-----------------------------------------------------------------------------
	UDelegateProperty.
-----------------------------------------------------------------------------*/

/**
 * Describes a pointer to a function bound to an Object.
 */
// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty<FScriptDelegate, UProperty> UDelegateProperty_Super;

class COREUOBJECT_API UDelegateProperty : public UDelegateProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UDelegateProperty,UDelegateProperty_Super,0,CoreUObject,CASTCLASS_UDelegateProperty)

	/** Points to the source delegate function (the function declared with the delegate keyword) used in the declaration of this delegate property. */
	UFunction* SignatureFunction;
public:

	typedef UDelegateProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UDelegateProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, UFunction* InSignatureFunction = NULL )
		: UDelegateProperty_Super( PCIP, EC_CppProperty, InOffset, InFlags)
		, SignatureFunction(InSignatureFunction)
	{
	}

	// UObject interface
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	// End of UObject interface

	// UProperty interface
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const OVERRIDE;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const OVERRIDE;
	virtual void SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const OVERRIDE;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const OVERRIDE;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const OVERRIDE;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const OVERRIDE;
	virtual bool ContainsWeakObjectReference() const OVERRIDE;
	virtual void InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph ) OVERRIDE;
	virtual bool SameType(const UProperty* Other) const OVERRIDE;
	// End of UProperty interface
};


/*-----------------------------------------------------------------------------
	UMulticastDelegateProperty.
-----------------------------------------------------------------------------*/

/**
 * Describes a pointer to a function bound to an Object.
 */
// need to break this out a different type so that the DECLARE_CASTED_CLASS_INTRINSIC macro can digest the comma
typedef TProperty<FMulticastScriptDelegate, UProperty> UMulticastDelegateProperty_Super;

class COREUOBJECT_API UMulticastDelegateProperty : public UMulticastDelegateProperty_Super
{
	DECLARE_CASTED_CLASS_INTRINSIC(UMulticastDelegateProperty,UMulticastDelegateProperty_Super,0,CoreUObject,CASTCLASS_UMulticastDelegateProperty)

	/** Points to the source delegate function (the function declared with the delegate keyword) used in the declaration of this delegate property. */
	UFunction* SignatureFunction;
public:

	typedef UMulticastDelegateProperty_Super::TTypeFundamentals TTypeFundamentals;
	typedef TTypeFundamentals::TCppType TCppType;

	UMulticastDelegateProperty( const class FPostConstructInitializeProperties& PCIP, ECppProperty, int32 InOffset, uint64 InFlags, UFunction* InSignatureFunction = NULL )
		: TProperty( PCIP, EC_CppProperty, InOffset, InFlags )
		, SignatureFunction(InSignatureFunction)
	{
	}

	// UObject interface
	virtual void Serialize( FArchive& Ar ) OVERRIDE;
	// End of UObject interface

	// UProperty interface
	virtual FString GetCPPType( FString* ExtendedTypeText, uint32 CPPExportFlags ) const OVERRIDE;
	virtual bool Identical( const void* A, const void* B, uint32 PortFlags ) const OVERRIDE;
	virtual void SerializeItem( FArchive& Ar, void* Value, int32 MaxReadBytes, void const* Defaults ) const OVERRIDE;
	virtual bool NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData = NULL ) const OVERRIDE;
	virtual void ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const OVERRIDE;
	virtual const TCHAR* ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* OwnerObject, FOutputDevice* ErrorText ) const OVERRIDE;
	virtual bool ContainsWeakObjectReference() const OVERRIDE;
	virtual void InstanceSubobjects( void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph ) OVERRIDE;
	virtual bool SameType(const UProperty* Other) const OVERRIDE;
	// End of UProperty interface

protected:
	friend class UProperty;

	const TCHAR* ImportText_Add( const TCHAR* Buffer, void* PropertyValue, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const;
	const TCHAR* ImportText_Remove( const TCHAR* Buffer, void* PropertyValue, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const;
};


/**
 * This class represents the chain of member properties leading to an internal struct property.  It is used
 * for tracking which member property corresponds to the UScriptStruct that owns a particular property.
 */
class COREUOBJECT_API FEditPropertyChain : public TDoubleLinkedList<UProperty*>
{

public:
	/** Constructors */
	FEditPropertyChain() : ActivePropertyNode(NULL), ActiveMemberPropertyNode(NULL) {}

	/**
	 * Sets the ActivePropertyNode to the node associated with the property specified.
	 *
	 * @param	NewActiveProperty	the UProperty that is currently being evaluated by Pre/PostEditChange
	 *
	 * @return	true if the ActivePropertyNode was successfully changed to the node associated with the property
	 *			specified.  false if there was no node corresponding to that property.
	 */
	bool SetActivePropertyNode( UProperty* NewActiveProperty );

	/**
	 * Sets the ActiveMemberPropertyNode to the node associated with the property specified.
	 *
	 * @param	NewActiveMemberProperty		the member UProperty which contains the property currently being evaluated
	 *										by Pre/PostEditChange
	 *
	 * @return	true if the ActiveMemberPropertyNode was successfully changed to the node associated with the
	 *			property specified.  false if there was no node corresponding to that property.
	 */
	bool SetActiveMemberPropertyNode( UProperty* NewActiveMemberProperty );

	/**
	 * Returns the node corresponding to the currently active property.
	 */
	TDoubleLinkedListNode* GetActiveNode() const;

	/**
	 * Returns the node corresponding to the currently active property, or if the currently active property
	 * is not a member variable (i.e. inside of a struct/array), the node corresponding to the member variable
	 * which contains the currently active property.
	 */
	TDoubleLinkedListNode* GetActiveMemberNode() const;

protected:
	/**
	 * In a hierarchy of properties being edited, corresponds to the property that is currently
	 * being processed by Pre/PostEditChange
	 */
	TDoubleLinkedListNode* ActivePropertyNode;

	/**
	 * In a hierarchy of properties being edited, corresponds to the class member property which
	 * contains the property that is currently being processed by Pre/PostEditChange.  This will
	 * only be different from the ActivePropertyNode if the active property is contained within a struct,
	 * dynamic array, or static array.
	 */
	TDoubleLinkedListNode* ActiveMemberPropertyNode;


	/** TDoubleLinkedList interface */
	/**
	 * Updates the size reported by Num().  Child classes can use this function to conveniently
	 * hook into list additions/removals.
	 *
	 * This version ensures that the ActivePropertyNode and ActiveMemberPropertyNode point to a valid nodes or NULL if this list is empty.
	 *
	 * @param	NewListSize		the new size for this list
	 */
	virtual void SetListSize( int32 NewListSize );
};


//-----------------------------------------------------------------------------
//EPropertyNodeFlags - Flags used internally by property editors
//-----------------------------------------------------------------------------
namespace EPropertyChangeType
{
	typedef uint32 Type;

	//default value.  Add new enums to add new functionality.
	const Type Unspecified = 1 << 0;
	//Array Add
	const Type ArrayAdd = 1 << 1;
	//Value Set
	const Type ValueSet = 1 << 2;
	//Duplicate
	const Type Duplicate = 1 << 3;
	//Interactive, e.g. dragging a slider. Will be followed by a ValueSet when finished.
	const Type Interactive = 1 << 4;
};

/**
 * Structure for passing pre and post edit change events
 */
struct FPropertyChangedEvent
{
	//default constructor
	FPropertyChangedEvent(UProperty* InProperty, const bool bInChangesTopology=false, EPropertyChangeType::Type InChangeType=EPropertyChangeType::Unspecified)
	{
		Property = InProperty;
		MemberProperty = InProperty;
		bChangesTopology = bInChangesTopology;
		ChangeType = InChangeType;

		//default to out of bounds/unused
		ObjectIteratorIndex = -1;
		//default to no array index knowledge
		ArrayIndicesPerObject = NULL;
	}

	void SetActiveMemberProperty( UProperty* InActiveMemberProperty )
	{
		MemberProperty = InActiveMemberProperty;
	}

	/**
	 * Saves off map of array indices per object being set.
	 */
	void SetArrayIndexPerObject(const TArray< TMap<FString,int32> >& InArrayIndices)
	{
		ArrayIndicesPerObject = &InArrayIndices; 
	}

	/**
	 * Gets the Array Index of the "current object" based on a particular name
	 * InName - Name of the property to find the array index for
	 */
	int32 GetArrayIndex (const FString& InName)
	{
		//default to unknown index
		int32 Retval = -1;
		if (ArrayIndicesPerObject && ArrayIndicesPerObject->IsValidIndex(ObjectIteratorIndex))
		{
			const int32* ValuePtr = (*ArrayIndicesPerObject)[ObjectIteratorIndex].Find(InName);
			if (ValuePtr)
			{
				Retval = *ValuePtr;
			}
		}
		return Retval;
	}

	/**
	 * The actual property that changed
	 */
	UProperty* Property;

	/**
	 * The member property of the object that PostEditChange is being called on.  
	 * For example if the property that changed is inside a struct on the object, this property is the struct property
	 */
	UProperty* MemberProperty;

	bool bChangesTopology;
	EPropertyChangeType::Type ChangeType;
	//Used by the param system to say which object is receiving the event in the case of multi-select
	int32                       ObjectIteratorIndex;
private:
	//In the property window, multiple objects can be selected at once.  In the case of adding/inserting to an array, each object COULD have different indices for the new entries in the array
	const TArray< TMap<FString,int32> >* ArrayIndicesPerObject;
};

/**
 * Structure for passing pre and post edit change events
 */
struct FPropertyChangedChainEvent : public FPropertyChangedEvent
{
	FPropertyChangedChainEvent(FEditPropertyChain& InPropertyChain, FPropertyChangedEvent& SrcChangeEvent) :
		FPropertyChangedEvent(SrcChangeEvent),
		PropertyChain(InPropertyChain)
	{
	}
	FEditPropertyChain& PropertyChain;
};

/*-----------------------------------------------------------------------------
TFieldIterator.
-----------------------------------------------------------------------------*/

/** TFieldIterator construction flags */
namespace EFieldIteratorFlags
{
	enum SuperClassFlags
	{
		ExcludeSuper = 0,	// Exclude super class
		IncludeSuper		// Include super class
	};

	enum DeprecatedPropertyFlags
	{
		ExcludeDeprecated = 0,	// Exclude deprecated properties
		IncludeDeprecated		// Include deprecated properties
	};

	enum InterfaceClassFlags
	{
		ExcludeInterfaces = 0,	// Exclude interfaces
		IncludeInterfaces		// Include interfaces
	};
}

//
// For iterating through a linked list of fields.
//
template <class T>
class TFieldIterator
{
private:
	/** The object being searched for the specified field */
	const UStruct* Struct;
	/** The current location in the list of fields being iterated */
	UField* Field;
	/** The index of the current interface being iterated */
	int32 InterfaceIndex;
	/** Whether to include the super class or not */
	const bool bIncludeSuper;
	/** Whether to include deprecated fields or not */
	const bool bIncludeDeprecated;
	/** Whether to include interface fields or not */
	const bool bIncludeInterface;

public:
	TFieldIterator(const UStruct*                               InStruct,
	               EFieldIteratorFlags::SuperClassFlags         InSuperClassFlags      = EFieldIteratorFlags::IncludeSuper,
	               EFieldIteratorFlags::DeprecatedPropertyFlags InDeprecatedFieldFlags = EFieldIteratorFlags::IncludeDeprecated,
	               EFieldIteratorFlags::InterfaceClassFlags     InInterfaceFieldFlags  = EFieldIteratorFlags::ExcludeInterfaces)
		: Struct            ( InStruct )
		, Field             ( InStruct ? InStruct->Children : NULL )
		, InterfaceIndex    ( -1 )
		, bIncludeSuper     ( InSuperClassFlags      == EFieldIteratorFlags::IncludeSuper )
		, bIncludeDeprecated( InDeprecatedFieldFlags == EFieldIteratorFlags::IncludeDeprecated )
		, bIncludeInterface ( InInterfaceFieldFlags  == EFieldIteratorFlags::IncludeInterfaces && InStruct && InStruct->IsA(UClass::StaticClass()) )
	{
		IterateToNext();
	}

	/** conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
	{ 
		return Field != NULL; 
	}
	/** inverse of the "bool" operator */
	FORCEINLINE bool operator !() const 
	{
		return !(bool)*this;
	}

	inline friend bool operator==(const TFieldIterator<T>& Lhs, const TFieldIterator<T>& Rhs) { return Lhs.Field == Rhs.Field; }
	inline friend bool operator!=(const TFieldIterator<T>& Lhs, const TFieldIterator<T>& Rhs) { return Lhs.Field != Rhs.Field; }

	inline void operator++()
	{
		checkSlow(Field);
		Field = Field->Next;
		IterateToNext();
	}
	inline T* operator*()
	{
		checkSlow(Field);
		return (T*)Field;
	}
	inline T* operator->()
	{
		checkSlow(Field);
		return (T*)Field;
	}
	inline const UStruct* GetStruct()
	{
		return Struct;
	}
protected:
	inline void IterateToNext()
	{
		      UField*  CurrentField  = Field;
		const UStruct* CurrentStruct = Struct;

		while (CurrentStruct)
		{
			while (CurrentField)
			{
				UClass* FieldClass = CurrentField->GetClass();

				if (FieldClass->HasAllCastFlags(T::StaticClassCastFlags()) &&
					(
						   bIncludeDeprecated
						|| !FieldClass->HasAllCastFlags(CASTCLASS_UProperty)
						|| !((UProperty*)CurrentField)->HasAllPropertyFlags(CPF_Deprecated)
					)
				)
				{
					Struct = CurrentStruct;
					Field  = CurrentField;
					return;
				}

				CurrentField = CurrentField->Next;
			}

			if (bIncludeInterface)
			{
				// We shouldn't be able to get here for non-classes
				UClass* CurrentClass = (UClass*)CurrentStruct;
				++InterfaceIndex;
				if (InterfaceIndex < CurrentClass->Interfaces.Num())
				{
					FImplementedInterface& Interface = CurrentClass->Interfaces[InterfaceIndex];
					CurrentField = Interface.Class->Children;
					continue;
				}
			}

			if (bIncludeSuper)
			{
				CurrentStruct = CurrentStruct->GetInheritanceSuper();
				if (CurrentStruct)
				{
					CurrentField   = CurrentStruct->Children;
					InterfaceIndex = -1;
					continue;
				}
			}

			break;
		}

		Struct = CurrentStruct;
		Field  = CurrentField;
	}
};

template <typename T>
struct TFieldRange
{
	TFieldRange(const UStruct*                               InStruct,
	            EFieldIteratorFlags::SuperClassFlags         InSuperClassFlags      = EFieldIteratorFlags::IncludeSuper,
	            EFieldIteratorFlags::DeprecatedPropertyFlags InDeprecatedFieldFlags = EFieldIteratorFlags::IncludeDeprecated,
	            EFieldIteratorFlags::InterfaceClassFlags     InInterfaceFieldFlags  = EFieldIteratorFlags::ExcludeInterfaces)
		: Begin(InStruct, InSuperClassFlags, InDeprecatedFieldFlags, InInterfaceFieldFlags)
	{
	}

	friend TFieldIterator<T> begin(const TFieldRange& Range) { return Range.Begin; }
	friend TFieldIterator<T> end  (const TFieldRange& Range) { return TFieldIterator<T>(NULL); }

	TFieldIterator<T> Begin;
};

/*-----------------------------------------------------------------------------
	Field templates.
-----------------------------------------------------------------------------*/

//
// Find a typed field in a struct.
//
template <class T> T* FindField( const UStruct* Owner, const TCHAR* FieldName )
{
	// lookup the string name in the Name hash
	FName Name(FieldName, FNAME_Find);
	// If we didn't find it, we know the field won't exist in this Struct
	if (Name == NAME_None)
		return NULL;
	// Search by comparing FNames (INTs), not strings
	for( TFieldIterator<T>It( Owner ); It; ++It )
	{
		if( It->GetFName() == Name )
		{
			return *It;
		}
	}
	// If we didn't find it, return no field
	return NULL;
}

template <class T> T* FindField( const UStruct* Owner, FName FieldName )
{
	// Search by comparing FNames (INTs), not strings
	for( TFieldIterator<T>It( Owner ); It; ++It )
	{
		if( It->GetFName() == FieldName )
		{
			return *It;
		}
	}

	// If we didn't find it, return no field
	return NULL;
}

/**
 * Search for the named field within the specified scope, including any Outer classes; assert on failure.
 *
 * @param	Scope		the scope to search for the field in
 * @param	FieldName	the name of the field to search for.
 */
template<typename T>
T* FindFieldChecked( const UStruct* Scope, FName FieldName )
{
	if ( FieldName != NAME_None && Scope != NULL )
	{
		const UStruct* InitialScope = Scope;
		for ( ; Scope != NULL; Scope = Cast<const UStruct>(Scope->GetOuter()) )
		{
			for ( TFieldIterator<T> It(Scope); It; ++It )
			{
				if ( It->GetFName() == FieldName )
				{
					return *It;
				}
			}
		}
	
		UE_LOG( LogType, Fatal, TEXT("Failed to find %s %s in %s"), *T::StaticClass()->GetName(), *FieldName.ToString(), *InitialScope->GetFullName() );
	}

	return NULL;
}

/**
 * Dynamically cast a property to the specified type; if the type is a UArrayProperty, will return the UArrayProperty's Inner member, if it is of the correct type.
 */
template<typename T>
T* SmartCastProperty( UProperty* Src )
{
	T* Result = Cast<T>(Src);
	if ( Result == NULL )
	{
		UArrayProperty* ArrayProp = Cast<UArrayProperty>(Src);
		if ( ArrayProp != NULL )
		{
			Result = Cast<T>(ArrayProp->Inner);
		}
	}
	return Result;
}

/**
 * Determine if this object has SomeObject in its archetype chain.
 */
inline bool UObject::IsBasedOnArchetype(  const UObject* const SomeObject ) const
{
	if ( this && SomeObject != this )
	{
		for ( UObject* Template = GetArchetype(); Template; Template = Template->GetArchetype() )
		{
			if ( SomeObject == Template )
			{
				return true;
			}
		}
	}

	return false;
}


/*-----------------------------------------------------------------------------
	C++ property macros.
-----------------------------------------------------------------------------*/

#define CPP_PROPERTY(name)	FPostConstructInitializeProperties(), EC_CppProperty, STRUCT_OFFSET(ThisClass, name)
#define CPP_PROPERTY_BASE(name, base)	FPostConstructInitializeProperties(), EC_CppProperty, STRUCT_OFFSET(base, name)

/** 
	The mac does not interpret a pointer to a bool* that is say 0x40 as true!, so we need to use uint8 for that. 
	this littler helper provides the correct type to use for bitfield determination
**/
template<typename T>
struct FTestType
{
	typedef T TestType;
};
template<>
struct FTestType<bool>
{
	checkAtCompileTime(sizeof(bool)==sizeof(uint8), bool_is_not_one_byte);
	typedef uint8 TestType;
};


struct COREUOBJECT_API DetermineBitfieldOffsetAndMask
{
	int32 Offset;
	uint32 BitMask;
	DetermineBitfieldOffsetAndMask()
		: Offset(0)
		, BitMask(0)
	{
	}

	/**
	 * Allocates a buffer large enough to hold the entire class which is being processed.
	 *
	 * @param SizeOf size of the class to calculate the bitfield properties for.
	 */
	void* AllocateBuffer(const SIZE_T SizeOf)
	{
		static SIZE_T CurrentSize = 0;
		static void *Buffer = NULL;
		if (!Buffer || SizeOf > CurrentSize)
		{
			FMemory::Free(Buffer);
			Buffer = FMemory::Malloc(SizeOf);
			FMemory::Memzero(Buffer, SizeOf);
			CurrentSize = SizeOf;
		}
	#if DO_GUARD_SLOW
		// make sure the memory is zero
		{
			uint8* ByteBuffer = (uint8*)Buffer;
			for (uint32 TestOffset = 0; TestOffset < SizeOf; TestOffset++)
			{
				checkSlow(!ByteBuffer[TestOffset]);
			}
		}
	#endif
		return Buffer;
	}

	/**
	 * Determines bitfield offset and mask
	 * 
	 * @param SizeOf Size of the class with the bitfield.
	 */
	template<typename BitfieldType>
	void DoDetermineBitfieldOffsetAndMask(const SIZE_T SizeOf)
	{
		typedef typename FTestType<BitfieldType>::TestType TTestType;
		checkAtCompileTime(sizeof(TTestType)==sizeof(BitfieldType), wrong_size_for_test_type);

		void* Buffer = AllocateBuffer(SizeOf);
		TTestType* Test = (TTestType*)Buffer;
		Offset = 0;
		BitMask = 0;
		SetBit(Buffer, true);
		// Here we are making the assumption that bitfields are aligned in the struct. Probably true. 
		// If not, it may be ok unless we are on a page boundary or something, but the check will fire in that case.
		// Have faith.
		for (uint32 TestOffset = 0; TestOffset < SizeOf / sizeof(BitfieldType); TestOffset++)
		{
			if (Test[TestOffset])
			{
				Offset = TestOffset * sizeof(BitfieldType);
				BitMask = (uint32)Test[TestOffset];
				check(FMath::RoundUpToPowerOfTwo(uint32(BitMask)) == uint32(BitMask)); // better be only one bit on
				break;
			}
		}
		SetBit(Buffer, false); // return the memory to zero
		check(BitMask); // or there was not a uint32 aligned chunk of memory that actually got the one.
	}
protected:
	virtual void SetBit(void* Scratch, bool Value) = 0;
};

/** build a struct that has a method that will return the bitmask for a bitfield **/
#define CPP_BOOL_PROPERTY_BITMASK_STRUCT(BitFieldName, ClassName, BitfieldType) \
	struct FDetermineBitMask_##ClassName##_##BitFieldName : public DetermineBitfieldOffsetAndMask\
	{ \
		FDetermineBitMask_##ClassName##_##BitFieldName() \
		{ \
			DoDetermineBitfieldOffsetAndMask<BitfieldType>(sizeof(ClassName)); \
		} \
		virtual void SetBit(void* Scratch, bool Value) \
		{ \
			((ClassName*)Scratch)->BitFieldName = (BitfieldType)Value; \
		} \
	} DetermineBitMask_##ClassName##_##BitFieldName

/** helper to retrieve the bitmask **/
#define CPP_BOOL_PROPERTY_BITMASK(BitFieldName, ClassName) \
	DetermineBitMask_##ClassName##_##BitFieldName.BitMask

/** helper to retrieve the offset **/
#define CPP_BOOL_PROPERTY_OFFSET(BitFieldName, ClassName) \
	DetermineBitMask_##ClassName##_##BitFieldName.Offset

/** helper to calculate an array's dimensions **/
#define CPP_ARRAY_DIM(ArrayName, ClassName) \
	(sizeof(((ClassName*)0)->ArrayName) / sizeof(((ClassName*)0)->ArrayName[0]))
