// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

// Do not remove this header guard! It ensures that DelegateCombinations.h can only be included from here.
#ifndef __Delegate_h__
#define __Delegate_h__

#pragma once

#include "SharedPointer.h"
#include "WeakObjectPtrTemplates.h"
#include "IDelegateInstance.h"
#include "DelegateBase.h"
#include "MulticastDelegateBase.h"


/**
 *  C++ DELEGATES
 *  -----------------------------------------------------------------------------------------------
 *
 *	This system allows you to call member functions on C++ objects in a generic, yet type-safe way.
 *  Using delegates, you can dynamically bind to a member function of an arbitrary object,
 *	then call functions on the object, even if the caller doesn't know the object's type.
 *
 *	The system predefines various combinations of generic function signatures with which you can
 *	declare a delegate type from, filling in the type names for return value and parameters with
 *	whichever types you need.
 *
 *	Both single-cast and multi-cast delegates are supported, as well as "dynamic" delegates which
 *	can be safely serialized to disk.  Additionally, delegates may define "payload" data which
 *	will stored and passed directly to bound functions.
 *
 *
 *
 *  DELEGATE FEATURES
 *  -----------------------------------------------------------------------------------------------
 *
 *	Currently we support delegate signatures using any combination of the following:
 *   		- Functions returning a value
 *			- Up to four "payload" variables
 *   		- Up to eight function parameters
 *   		- Functions declared as 'const'
 *
 *  Multi-cast delegates are also supported, using the 'DECLARE_MULTICAST_DELEGATE...' macros.
 *  Multi-cast delegates allow you to attach multiple function delegates, then execute them all at
 *  once by calling a single "Broadcast()" function.  Multi-cast delegate signatures are not allowed
 *	to use a return value.
 *
 *	You can assign "payload data" to your delegates!  These are arbitrary variables that will be passed
 *  directly to any bound function when it is invoked.  This is really useful as it allows you to store
 *  parameters within the delegate it self at bind-time.  All delegate types (except for "dynamic") supports
 *	payload variables automatically!
 *
 *	When binding to a delegate, you can pass payload data along.  This example passes two custom variables,
 *	a bool and an int32 to a delegate.  Then when the delegate is invoked, these parameters will be passed
 *	to your bound function.  The extra variable arguments must always be accepted after the delegate
 *	type parameter arguments.
 *
 *			MyDelegate.BindStatic( &MyFunction, true, 20 );
 *
 *	Remember to look at the table at the bottom of this documentation comment for the macro names to
 *	use for each function signature type.
 *
 *
 *
 *  DELEGATES EXAMPLE
 *  -----------------------------------------------------------------------------------------------
 *
 *	Suppose you have a class with a method that you'd like to be able to call from anywhere: 
 *
 *		class FLogWriter
 *		{
 *			void WriteToLog( FString );
 *		};
 *
 *	To call the WriteToLog function, we'll need to create a delegate type for that function's signature.
 *	To do this, you will first declare the delegate using one of the macros below.  For example, here
 *	is a simple delegate type:
 *
 *		DECLARE_DELEGATE_OneParam( FStringDelegate, FString );
 *
 *	This creates a delegate type called 'FStringDelegate' that takes a single parameter of type 'FString'.
 *
 *	Here's an example of how you'd use this 'FStringDelegate' in a class:
 *
 *		class FMyClass
 *		{
 *			FStringDelegate WriteToLogDelegate;
 *		};
 *
 *	This allows your class to hold a pointer to a method in an arbitrary class.  The only thing the
 *	class really knows about this delegate is it's function signature.
 *
 *	Now, to assign the delegate, simply create an instance of your delegate class, passing along the
 *	class that owns the method as a template parameter.  You'll also pass the instance of your object
 *	and the actual function address of the method.  So, here we'll create an instance of our 'FLogWriter'
 *	class, then create a delegate for the 'WriteToLog' method of that object instance:
 *
 *		FSharedRef< FLogWriter > LogWriter( new FLogWriter() );
 *
 *		WriteToLogDelegate.BindSP( LogWriter, &FLogWriter::WriteToLog );
 *
 *	You've just dynamically bound a delegate to a method of a class!  Pretty simple, right?
 *
 *	Note that the 'SP' part of 'BindSP' stands for 'shared pointer', because we're binding to an
 *  object that's owned by a shared pointer.  There are versions for different object types,
 *	such as BindRaw() and BindUObject().  You can bind to global function pointers with BindStatic().
 *
 *	Now, your 'WriteToLog' method can be called by FMyClass without it even knowing anything about
 *	the 'FLogWriter' class!  To call a your delegate, just use the 'Execute()' method:
 *
 *		WriteToLogDelegate.Execute( TEXT( "Delegates are spiffy!" ) );
 *
 *  If you call Execute() before binding a function to the delegate, an assertion will be triggered.  In
 *  many cases, you'll instead want to do this:
 *
 *		WriteToLogDelegate.ExecuteIfBound( TEXT( "Only executes if a function was bound!" ) );
 *
 *	That's pretty much all there is to it!!  You can read below for a bit more information.
 *
 *
 *
 *	MORE INFORMATION
 *  -----------------------------------------------------------------------------------------------
 *
 *	The delegate system understands certain types of objects, and additional features are enabled when
 *  using these objects.  If you bind a delegate to a member of a UObject or shared pointer class, the
 *  delegate system can keep a weak reference to the object, so that if the object gets destroyed out
 *  from underneath the delegate, you'll be able to handle these cases by calling IsBound() or
 *  ExecuteIfBound() functions.  Note the special binding syntax for the various types of supported objects.
 *
 *  It's perfectly safe to copy delegate objects.  Delegates can be passed around by value but this is
 *	generally not recommended since they do have to allocate memory on the heap.  Pass them by reference
 *  when possible!
 *
 *  Delegate signature declarations can exist at global scope, within a namespace or even within a class
 *  declaration (but not function bodies.)
 *
 *
 *
 *  FUNCTION SIGNATURES
 *  -----------------------------------------------------------------------------------------------
 *
 *  Use this table to find the declaration macro to use to declare your delegate.
 *
 *	Function signature									|	Declaration macro
 *  ------------------------------------------------------------------------------------------------------------------------------------------------------------
 *	void Function()										|	DECLARE_DELEGATE( DelegateName )
 *	void Function( <Param1> )							|	DECLARE_DELEGATE_OneParam( DelegateName, Param1Type )
 *	void Function( <Param1>, <Param2> )					|	DECLARE_DELEGATE_TwoParams( DelegateName, Param1Type, Param2Type )
 *	void Function( <Param1>, <Param2>, ... )			|	DECLARE_DELEGATE_<Num>Params( DelegateName, Param1Type, Param2Type, ... )
 *	<RetVal> Function()									|	DECLARE_DELEGATE_RetVal( RetValType, DelegateName )
 *	<RetVal> Function( <Param1> )						|	DECLARE_DELEGATE_RetVal_OneParam( RetValType, DelegateName, Param1Type )
 *	<RetVal> Function( <Param1>, <Param2> )				|	DECLARE_DELEGATE_RetVal_TwoParams( RetValType, DelegateName, Param1Type, Param2Type )
 *	<RetVal> Function( <Param1>, <Param2>, ... )		|	DECLARE_DELEGATE_RetVal_<Num>Params( RetValType, DelegateName, Param1Type, Param2Type, ... )
 *  ------------------------------------------------------------------------------------------------------------------------------------------------------------
 *
 *  Remember, there are three different delegate types you can define (any of the above signatures will work):
 *
 *                       Single-cast delegates:  DECLARE_DELEGATE...()
 *                        Multi-cast delegates:  DECLARE_MULTICAST_DELEGATE...()
 *	 Dynamic (UObject, serializable) delegates:  DECLARE_DYNAMIC_DELEGATE...()
 */


// This suffix is appended to all header exported delegates
#define HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX TEXT("__DelegateSignature")

/** Helper macro that enables passing comma-separated arguments as a single macro parameter */
#define FUNC_CONCAT( ... ) __VA_ARGS__

/** Declare the user's delegate object */
// NOTE: The last parameter is variadic and is used as the 'template args' for this delegate's classes (__VA_ARGS__)
#define FUNC_DECLARE_DELEGATE( Suffix, DelegateName, ... ) \
	typedef TBaseDelegate_##Suffix< __VA_ARGS__ > DelegateName;

/** Declare the user's multicast delegate object */
// NOTE: The last parameter is variadic and is used as the 'template args' for this delegate's classes (__VA_ARGS__)
#define FUNC_DECLARE_MULTICAST_DELEGATE( Suffix, MulticastDelegateName, ... ) \
	typedef TMulticastDelegate_##Suffix< __VA_ARGS__ > MulticastDelegateName;

#define FUNC_DECLARE_EVENT( OwningType, EventName, Suffix, ... ) \
	class EventName : public TBaseMulticastDelegate_##Suffix< __VA_ARGS__ >  { friend class OwningType; };

/** Declare user's dynamic delegate, with wrapper proxy method for executing the delegate */
#define FUNC_DECLARE_DYNAMIC_DELEGATE( Suffix, DynamicDelegateName, ExecFunction, FuncParamList, FuncParamPassThru, ... ) \
	class DynamicDelegateName : public TBaseDynamicDelegate_##Suffix< __VA_ARGS__ > \
	{ \
	public: \
		/** Default constructor */ \
		DynamicDelegateName() \
		{ \
		} \
		\
		/** Construction from an FScriptDelegate must be explicit.  This is really only used by UObject system internals. */ \
		explicit DynamicDelegateName( const TScriptDelegate<>& InScriptDelegate ) \
			: TBaseDynamicDelegate_##Suffix( InScriptDelegate ) \
		{ \
		} \
		\
		/** Execute the delegate.  If the function pointer is not valid, an error will occur. */ \
		inline void Execute( FuncParamList ) const \
		{ \
			/* Verify that the user object is still valid.  We only have a weak reference to it. */ \
			checkSlow( IsBound() ); \
			ExecFunction( FuncParamPassThru ); \
		} \
		/** Execute the delegate, but only if the function pointer is still valid */ \
		inline bool ExecuteIfBound( FuncParamList ) const \
		{ \
			if( IsBound() ) \
			{ \
				ExecFunction( FuncParamPassThru ); \
				return true; \
			} \
			return false; \
		} \
	};

// Helper struct for converting uint32 return value to bool
template <typename RetVal>
struct TRetValCast
{
	inline RetVal operator()( RetVal Val ) const
	{
		return Val;
	}
	inline bool operator()( uint32 Val ) const
	{
		return !!Val;
	}
};

#define FUNC_DECLARE_DYNAMIC_DELEGATE_RETVAL( Suffix, DynamicDelegateName, ExecFunction, RetValType, FuncParamList, FuncParamPassThru, ... ) \
	class DynamicDelegateName : public TBaseDynamicDelegate_##Suffix< __VA_ARGS__ > \
	{ \
	public: \
		/** Default constructor */ \
		DynamicDelegateName() \
		{ \
		} \
		\
		/** Construction from an FScriptDelegate must be explicit.  This is really only used by UObject system internals. */ \
		explicit DynamicDelegateName( const TScriptDelegate<>& InScriptDelegate ) \
			: TBaseDynamicDelegate_##Suffix( InScriptDelegate ) \
		{ \
		} \
		\
		/** Execute the delegate.  If the function pointer is not valid, an error will occur. */ \
		inline RetValType Execute( FuncParamList ) const \
		{ \
			/* Verify that the user object is still valid.  We only have a weak reference to it. */ \
			checkSlow( IsBound() ); \
			TRetValCast<RetValType> CastRetVal; \
			return CastRetVal( ExecFunction( FuncParamPassThru ) ); \
		} \
	};


/** Declare user's dynamic multi-cast delegate, with wrapper proxy method for executing the delegate */
#define FUNC_DECLARE_DYNAMIC_MULTICAST_DELEGATE( Suffix, DynamicMulticastDelegateName, ExecFunction, FuncParamList, FuncParamPassThru, ... ) \
class DynamicMulticastDelegateName : public TBaseDynamicMulticastDelegate_##Suffix< __VA_ARGS__ > \
	{ \
	public: \
		/** Default constructor */ \
		DynamicMulticastDelegateName() \
		{ \
		} \
		\
		/** Construction from an FMulticastScriptDelegate must be explicit.  This is really only used by UObject system internals. */ \
		explicit DynamicMulticastDelegateName( const TMulticastScriptDelegate<>& InMulticastScriptDelegate ) \
			: TBaseDynamicMulticastDelegate_##Suffix( InMulticastScriptDelegate ) \
		{ \
		} \
		\
		/** Broadcasts this delegate to all bound objects, except to those that may have expired */ \
		void Broadcast( FuncParamList ) const \
		{ \
			ExecFunction( FuncParamPassThru ); \
		} \
	};


// Simple macro chain to concatenate code text
#define FUNC_COMBINE_ACTUAL( A, B ) A##B
#define FUNC_COMBINE( A, B ) FUNC_COMBINE_ACTUAL( A, B )



// Helper macro for calling BindDynamic() on dynamic delegates.  Automatically generates the function name string.
#define BindDynamic( UserObject, FuncName ) __Internal_BindDynamic( UserObject, FuncName, FString( #FuncName ) )

// Helper macro for calling AddDynamic() on dynamic multi-cast delegates.  Automatically generates the function name string.
#define AddDynamic( UserObject, FuncName ) __Internal_AddDynamic( UserObject, FuncName, FString( #FuncName ) )

// Helper macro for calling AddUniqueDynamic() on dynamic multi-cast delegates.  Automatically generates the function name string.
#define AddUniqueDynamic( UserObject, FuncName ) __Internal_AddUniqueDynamic( UserObject, FuncName, FString( #FuncName ) )

// Helper macro for calling RemoveDynamic() on dynamic multi-cast delegates.  Automatically generates the function name string.
#define RemoveDynamic( UserObject, FuncName ) __Internal_RemoveDynamic( UserObject, FuncName, FString( #FuncName ) )

// Helper macro for calling IsAlreadyBound() on dynamic multi-cast delegates.  Automatically generates the function name string.
#define IsAlreadyBound( UserObject, FuncName ) __Internal_IsAlreadyBound( UserObject, FuncName, FString( #FuncName ) )


/*********************************************************************************************************************/

// We define this as a guard to prevent DelegateSignatureImpl.inl being included outside of this file
#define FUNC_INCLUDING_INLINE_IMPL


#ifndef UE_BUILD_DOCS
#include "DelegateCombinations.h"
#endif


/*********************************************************************************************************************/

#define DECLARE_DERIVED_EVENT( OwningType, BaseTypeEvent, EventName ) \
	class EventName : public BaseTypeEvent { friend class OwningType; };


// No longer allowed to include DelegateSignatureImpl.inl
#undef FUNC_INCLUDING_INLINE_IMPL

// Undefine temporary macros
#undef FUNC_COMBINE_ACTUAL
#undef FUNC_COMBINE

// Simple delegate used by various utilities such as timers
DECLARE_DELEGATE( FSimpleDelegate );
DECLARE_MULTICAST_DELEGATE( FSimpleMulticastDelegate );


#endif		// __Delegate_h__
