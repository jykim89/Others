// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 * This header contains the code for serialization of script bytecode and [eventually] tagged property values.
 * Extracted to header file to allow custom definitions of the macros used by these methods
 */
#ifdef VISUAL_ASSIST_HACK
	EExprToken SerializeExpr( int32&, FArchive& );
	int32 iCode=0;
	FArchive Ar;
	TArray<uint8> Script;
	EExprToken Expr=(EExprToken)0;
#endif

#ifndef XFER
#ifdef REQUIRES_ALIGNED_INT_ACCESS
	#define XFER(T) \
		{ \
		T Temp; \
		if (!Ar.IsLoading()) \
		{ \
			FMemory::Memcpy( &Temp, &Script[iCode], sizeof(T) ); \
		} \
		Ar << Temp; \
		if (!Ar.IsSaving()) \
		{ \
			FMemory::Memcpy( &Script[iCode], &Temp, sizeof(T) ); \
		} \
		iCode += sizeof(T); \
		}
#else
	#define XFER(T) {Ar << *(T*)&Script[iCode]; iCode += sizeof(T); }
#endif
#endif

#ifndef XFERNAME
	#define XFERNAME() XFER(FName)
#endif	//XFERNAME

#ifndef XFERPTR 
	#define XFERPTR(T) \
	{ \
   	    T AlignedPtr = NULL; \
		ScriptPointerType TempCode; \
        if (!Ar.IsLoading()) \
		{ \
			FMemory::Memcpy( &TempCode, &Script[iCode], sizeof(ScriptPointerType) ); \
			AlignedPtr = (T)(TempCode); \
		} \
		Ar << AlignedPtr; \
		if (!Ar.IsSaving()) \
		{ \
			TempCode = (ScriptPointerType)(AlignedPtr); \
			FMemory::Memcpy( &Script[iCode], &TempCode, sizeof(ScriptPointerType) ); \
		} \
		iCode += sizeof(ScriptPointerType); \
	}
#endif	//	XFERPTR


#ifndef XFER_FUNC_POINTER
	#define XFER_FUNC_POINTER	XFERPTR(UStruct*)
#endif	// XFER_FUNC_POINTER

#ifndef XFER_FUNC_NAME
	#define XFER_FUNC_NAME		XFERNAME()
#endif	// XFER_FUNC_NAME

#ifndef XFER_PROP_POINTER
	#define XFER_PROP_POINTER	XFERPTR(UProperty*)
#endif

#ifndef XFER_OBJECT_POINTER
	#define XFER_OBJECT_POINTER(Type)	XFERPTR(Type)
#endif

/** UStruct::SerializeExpr() */
#ifdef SERIALIZEEXPR_INC
	EExprToken Expr=(EExprToken)0;

	// Get expr token.
	XFER(uint8);
	Expr = (EExprToken)Script[iCode-1];

	switch( Expr )
	{
		case EX_PrimitiveCast:
		{
			// A type conversion.
			XFER(uint8); //which kind of conversion
			SerializeExpr( iCode, Ar );
			break;
		}
		case EX_ObjToInterfaceCast:
		case EX_CrossInterfaceCast:
		{
			// A conversion from an object or interface variable to a native interface variable.  
			// We use a different bytecode to avoid the branching each time we process a cast token.
			
			XFERPTR(UClass*);	// the interface class to convert to
			SerializeExpr( iCode, Ar );
			break;
		}
		case EX_Let:
		case EX_LetObj:
		case EX_LetWeakObjPtr:
		case EX_LetBool:
		case EX_LetDelegate:
		case EX_LetMulticastDelegate:
		{
			SerializeExpr( iCode, Ar ); // Variable expr.
			SerializeExpr( iCode, Ar ); // Assignment expr.
			break;
		}
		case EX_StructMemberContext:
		{
			XFERPTR(UProperty*);        // struct member expr.
			SerializeExpr( iCode, Ar ); // struct expr.
			break;
		}
		case EX_Jump:
		{
			XFER(CodeSkipSizeType); // Code offset.
			break;
		}
		case EX_ComputedJump:
		{
			SerializeExpr( iCode, Ar ); // Integer expression, specifying code offset.
			break;
		}
		case EX_LocalVariable:
		case EX_InstanceVariable:
		case EX_LocalOutVariable:
		{
			XFER_PROP_POINTER;
			break;
		}
		case EX_InterfaceContext:
		{
			SerializeExpr(iCode,Ar);
			break;
		}
		case EX_PushExecutionFlow:
			{
				XFER(CodeSkipSizeType);		// location to push
				break;
			}
		case EX_Nothing:
		case EX_EndOfScript:
		case EX_EndFunctionParms:
		case EX_EndStructConst:
		case EX_EndArray:
		case EX_IntZero:
		case EX_IntOne:
		case EX_True:
		case EX_False:
		case EX_NoObject:
		case EX_Self:
		case EX_EndParmValue:
		case EX_PopExecutionFlow:
		case EX_DeprecatedOp4A:
		{
			break;
		}
		case EX_WireTracepoint:
		case EX_Tracepoint:
		{
			break;
		}
		case EX_Breakpoint:
		{
			if (Ar.IsLoading())
			{
				// Turn breakpoints into tracepoints on load
				Script[iCode-1] = EX_Tracepoint;
			}
			break;
		}
		case EX_Return:
		{
			SerializeExpr( iCode, Ar ); // Return expression.
			break;
		}
		case EX_FinalFunction:
		{
			XFER_FUNC_POINTER;											// Stack node.
			while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms ); // Parms.
			break;
		}
		case EX_VirtualFunction:
		{
			XFER_FUNC_NAME;												// Virtual function name.
			while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms );	// Parms.
			break;
		}
		case EX_CallMulticastDelegate:
		{
			XFER_FUNC_POINTER;											// Stack node.
			while( SerializeExpr( iCode, Ar ) != EX_EndFunctionParms ); // Parms.
			break;
		}
		case EX_Context:
		case EX_Context_FailSilent:
		{
			SerializeExpr( iCode, Ar ); // Object expression.
			XFER(CodeSkipSizeType);		// Code offset for NULL expressions.
			XFERPTR(UField*);			// Property corresponding to the r-value data, in case the l-value needs to be mem-zero'd
			XFER(uint8);					// Property type, in case the r-value is a non-property such as dynamic array length
			SerializeExpr( iCode, Ar ); // Context expression.
			break;
		}
		case EX_AddMulticastDelegate:
		case EX_RemoveMulticastDelegate:
		{
			SerializeExpr( iCode, Ar );	// Delegate property to assign to
			SerializeExpr( iCode, Ar ); // Delegate to add to the MC delegate for broadcast
			break;
		}
		case EX_ClearMulticastDelegate:
		{
			SerializeExpr( iCode, Ar );	// Delegate property to clear
			break;
		}
		case EX_IntConst:
		{
			XFER(int32);
			break;
		}
		case EX_SkipOffsetConst:
		{
			XFER(CodeSkipSizeType);
			break;
		}
		case EX_FloatConst:
		{
			XFER(float);
			break;
		}
		case EX_StringConst:
		{
			do XFER(uint8) while( Script[iCode-1] );
			break;
		}
		case EX_UnicodeStringConst:
		{
			do XFER(uint16) while( Script[iCode-1] || Script[iCode-2] );
			break;
		}
		case EX_TextConst:
		{
			SerializeExpr( iCode, Ar );
			SerializeExpr( iCode, Ar );
			SerializeExpr( iCode, Ar );
			break;
		}
		case EX_ObjectConst:
		{
			XFER_OBJECT_POINTER(UObject*);
			break;
		}
		case EX_NameConst:
		{
			XFERNAME();
			break;
		}
		case EX_RotationConst:
		{
			XFER(int32); XFER(int32); XFER(int32);
			break;
		}
		case EX_VectorConst:
		{
			XFER(float); XFER(float); XFER(float);
			break;
		}
		case EX_TransformConst:
		{
			// Rotation
			XFER(float); XFER(float); XFER(float); XFER(float);
			// Translation
			XFER(float); XFER(float); XFER(float);
			// Scale
			XFER(float); XFER(float); XFER(float);
			break;
		}
		case EX_StructConst:
		{
			XFERPTR(UScriptStruct*);	// Struct.
			XFER(int32);					// Serialized struct size
			while( SerializeExpr( iCode, Ar ) != EX_EndStructConst );
			break;
		}
		case EX_SetArray:
		{
			// If not loading, or its a newer version
			if((GetLinker() == NULL) || !Ar.IsLoading() || (Ar.UE4Ver() >= VER_UE4_CHANGE_SETARRAY_BYTECODE))
			{
				// Array property to assign to
				EExprToken TargetToken = SerializeExpr( iCode, Ar );
			}
			else
			{
				// Array Inner Prop
				XFERPTR(UProperty*);
			}
		
			while( SerializeExpr( iCode, Ar) != EX_EndArray );
			break;
		}
		case EX_ByteConst:
		case EX_IntConstByte:
		{
			XFER(uint8);
			break;
		}
		case EX_MetaCast:
		{
			XFER_OBJECT_POINTER(UClass*);
			SerializeExpr( iCode, Ar );
			break;
		}
		case EX_DynamicCast:
		{
			XFER_OBJECT_POINTER(UClass*);
			SerializeExpr( iCode, Ar );
			break;
		}
		case EX_JumpIfNot:
		{
			XFER(CodeSkipSizeType);		// Code offset.
			SerializeExpr( iCode, Ar ); // Boolean expr.
			break;
		}
		case EX_PopExecutionFlowIfNot:
		{
			SerializeExpr( iCode, Ar ); // Boolean expr.
			break;
		}
		case EX_Assert:
		{
			XFER(uint16); // Line number.
			XFER(uint8); // debug mode or not
			SerializeExpr( iCode, Ar ); // Assert expr.
			break;
		}
		case EX_Skip:
		{
			XFER(CodeSkipSizeType);		// Skip size.
			SerializeExpr( iCode, Ar ); // Expression to possibly skip.
			break;
		}
		case EX_InstanceDelegate:
		{
			XFER_FUNC_NAME;				// the name of the function assigned to the delegate.
			break;
		}
		case EX_BindDelegate:
		{
			XFER_FUNC_NAME;
			SerializeExpr( iCode, Ar );	// Delegate property to assign to
			SerializeExpr( iCode, Ar ); 
			break;
		}
		default:
		{
			// This should never occur.
			UE_LOG(LogScriptSerialization, Warning, TEXT("Error: Unknown bytecode 0x%02X; ignoring it"), (uint8)Expr );
			break;
		}
	}

#endif	//!TAGGED_PROPERTIES_ONLY || SERIALIZEEXPR_ONLY

