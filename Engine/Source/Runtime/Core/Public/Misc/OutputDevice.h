// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OutputDevice.h: FOutputDevice and OutputDebugString type functions
=============================================================================*/

#pragma once

class FText;

// Globals.
#define GLog FOutputDeviceRedirector::Get()
CORE_API extern class FOutputDevice*				GThrow;
CORE_API extern class FOutputDeviceError*			GError;
CORE_API extern class FFeedbackContext*				GWarn;


/*-----------------------------------------------------------------------------
	VarArgs helper macros.
-----------------------------------------------------------------------------*/

// phantom definitions to help VAX parse our VARARG_* macros (VAX build 1440)
#ifdef VISUAL_ASSIST_HACK
	#define VARARG_DECL( FuncRet, StaticFuncRet, Return, FuncName, Pure, FmtType, ExtraDecl, ExtraCall ) FuncRet FuncName( ExtraDecl FmtType Fmt, ... )
	#define VARARG_BODY( FuncRet, FuncName, FmtType, ExtraDecl ) FuncRet FuncName( ExtraDecl FmtType Fmt, ... )
#endif // VISUAL_ASSIST_HACK


#define GET_VARARGS(msg, msgsize, len, lastarg, fmt) \
{ \
	va_list ap; \
	va_start(ap, lastarg); \
	FCString::GetVarArgs(msg, msgsize, len, fmt, ap); \
}
#define GET_VARARGS_WIDE(msg, msgsize, len, lastarg, fmt) \
{ \
	va_list ap; \
	va_start(ap, lastarg); \
	FCStringWide::GetVarArgs(msg, msgsize, len, fmt, ap); \
}
#define GET_VARARGS_ANSI(msg, msgsize, len, lastarg, fmt) \
{ \
	va_list ap; \
	va_start(ap, lastarg); \
	FCStringAnsi::GetVarArgs(msg, msgsize, len, fmt, ap); \
}
#define GET_VARARGS_RESULT(msg, msgsize, len, lastarg, fmt, result) \
{ \
	va_list ap; \
	va_start(ap, lastarg); \
	result = FCString::GetVarArgs(msg, msgsize, len, fmt, ap); \
	if (result >= msgsize) \
	{ \
		result = -1; \
	} \
}
#define GET_VARARGS_RESULT_WIDE(msg, msgsize, len, lastarg, fmt, result) \
{ \
	va_list ap; \
	va_start(ap, lastarg); \
	result = FCStringWide::GetVarArgs(msg, msgsize, len, fmt, ap); \
	if (result >= msgsize) \
	{ \
		result = -1; \
	} \
}
#define GET_VARARGS_RESULT_ANSI(msg, msgsize, len, lastarg, fmt, result) \
{ \
	va_list ap; \
	va_start(ap, lastarg); \
	result = FCStringAnsi::GetVarArgs(msg, msgsize, len, fmt, ap); \
	if (result >= msgsize) \
	{ \
		result = -1; \
	} \
}

/*-----------------------------------------------------------------------------
	Ugly VarArgs type checking (debug builds only).
-----------------------------------------------------------------------------*/

#define VARARG_EXTRA(A) A,
#define VARARG_NONE
#define VARARG_PURE =0

#if PLATFORM_WINDOWS

static inline uint32			CheckVA(uint32 dw)		{ return dw; }
static inline uint8			CheckVA(uint8 b)			{ return b; }
static inline int32			CheckVA(int32 i)			{ return i; }
static inline uint64			CheckVA(uint64 qw)		{ return qw; }
static inline int64		CheckVA(int64 sqw)		{ return sqw; }
static inline double		CheckVA(double d)		{ return d; }
static inline long		CheckVA(long d)		{ return d; }
static inline long		CheckVA(unsigned long d)		{ return d; }
static inline TCHAR			CheckVA(TCHAR c)		{ return c; }
static inline void*			CheckVA(ANSICHAR* s)	{ return (void*)s; }
static inline bool		CheckVA(bool b) { return b; }
template<class T> T*		CheckVA(T* p)			{ return p; }
template<class T> const T*	CheckVA(const T* p)		{ return p; }

#define VARARG_DECL( FuncRet, StaticFuncRet, Return, FuncName, Pure, FmtType, ExtraDecl, ExtraCall )	\
	FuncRet FuncName##__VA( ExtraDecl FmtType Fmt, ... ) Pure;  \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt) {Return FuncName##__VA(ExtraCall (Fmt));} \
	template<class T1> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1) {T1 v1=CheckVA(V1);Return FuncName##__VA(ExtraCall (Fmt),(v1));} \
	template<class T1,class T2> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2));} \
	template<class T1,class T2,class T3> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3));} \
	template<class T1,class T2,class T3,class T4> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4));} \
	template<class T1,class T2,class T3,class T4,class T5> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19,class T20> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19,T20 V20) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);T20 v20=CheckVA(V20);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19),(v20));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19,class T20,class T21> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19,T20 V20, T21 V21) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);T20 v20=CheckVA(V20);T21 v21=CheckVA(V21);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19),(v20),(v21));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19,class T20,class T21,class T22> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19,T20 V20, T21 V21, T22 V22) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);T20 v20=CheckVA(V20);T21 v21=CheckVA(V21);T22 v22=CheckVA(V22);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19),(v20),(v21),(v22));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19,class T20,class T21,class T22,class T23> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19,T20 V20, T21 V21, T22 V22, T23 V23) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);T20 v20=CheckVA(V20);T21 v21=CheckVA(V21);T22 v22=CheckVA(V22);T23 v23=CheckVA(V23);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19),(v20),(v21),(v22),(v23));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19,class T20,class T21,class T22,class T23,class T24> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19,T20 V20, T21 V21, T22 V22, T23 V23, T24 V24) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);T20 v20=CheckVA(V20);T21 v21=CheckVA(V21);T22 v22=CheckVA(V22);T23 v23=CheckVA(V23);T24 v24=CheckVA(V24);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19),(v20),(v21),(v22),(v23),(v24));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19,class T20,class T21,class T22,class T23,class T24,class T25> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19,T20 V20, T21 V21, T22 V22, T23 V23, T24 V24, T25 V25) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);T20 v20=CheckVA(V20);T21 v21=CheckVA(V21);T22 v22=CheckVA(V22);T23 v23=CheckVA(V23);T24 v24=CheckVA(V24);T25 v25=CheckVA(V25);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19),(v20),(v21),(v22),(v23),(v24),(v25));} \
	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8,class T9,class T10,class T11,class T12,class T13,class T14,class T15,class T16,class T17,class T18,class T19,class T20,class T21,class T22,class T23,class T24,class T25,class T26> \
	StaticFuncRet FuncName(ExtraDecl FmtType Fmt,T1 V1,T2 V2,T3 V3,T4 V4,T5 V5,T6 V6,T7 V7,T8 V8,T9 V9,T10 V10,T11 V11,T12 V12,T13 V13,T14 V14,T15 V15,T16 V16,T17 V17,T18 V18,T19 V19,T20 V20, T21 V21, T22 V22, T23 V23, T24 V24, T25 V25, T26 V26) {T1 v1=CheckVA(V1);T2 v2=CheckVA(V2);T3 v3=CheckVA(V3);T4 v4=CheckVA(V4);T5 v5=CheckVA(V5);T6 v6=CheckVA(V6);T7 v7=CheckVA(V7);T8 v8=CheckVA(V8);T9 v9=CheckVA(V9);T10 v10=CheckVA(V10);T11 v11=CheckVA(V11);T12 v12=CheckVA(V12);T13 v13=CheckVA(V13);T14 v14=CheckVA(V14);T15 v15=CheckVA(V15);T16 v16=CheckVA(V16);T17 v17=CheckVA(V17);T18 v18=CheckVA(V18);T19 v19=CheckVA(V19);T20 v20=CheckVA(V20);T21 v21=CheckVA(V21);T22 v22=CheckVA(V22);T23 v23=CheckVA(V23);T24 v24=CheckVA(V24);T25 v25=CheckVA(V25);T26 v26=CheckVA(V26);Return FuncName##__VA(ExtraCall (Fmt),(v1),(v2),(v3),(v4),(v5),(v6),(v7),(v8),(v9),(v10),(v11),(v12),(v13),(v14),(v15),(v16),(v17),(v18),(v19),(v20),(v21),(v22),(v23),(v24),(v25),(v26));}



#define VARARG_BODY( FuncRet, FuncName, FmtType, ExtraDecl )		\
	FuncRet FuncName##__VA( ExtraDecl  FmtType Fmt, ... )

#else  // !_MSC_VER

#define VARARG_DECL( FuncRet, StaticFuncRet, Return, FuncName, Pure, FmtType, ExtraDecl, ExtraCall )	\
	FuncRet FuncName( ExtraDecl FmtType Fmt, ... ) Pure
#define VARARG_BODY( FuncRet, FuncName, FmtType, ExtraDecl )		\
	FuncRet FuncName( ExtraDecl FmtType Fmt, ... )

#endif // _MSC_VER

/** 
 * Enum that defines the verbosity levels of the logging system.
 * Also defines some non-verbosity levels that are hacks that allow
 * breaking on a given log line or setting the color.
**/
namespace ELogVerbosity
{
	enum Type
	{
		NoLogging		= 0,
		Fatal,
		Error,
		Warning,
		Display,
		Log,
		Verbose,
		VeryVerbose,
		All				= VeryVerbose,
		NumVerbosity,
		VerbosityMask	= 0xf,
		SetColor		= 0x40, // not actually a verbosity, used to set the color of an output device 
		BreakOnLog		= 0x80
	};
}
checkAtCompileTime(ELogVerbosity::NumVerbosity - 1 < ELogVerbosity::VerbosityMask, bad_ELogVerbosity_VerbosityMask);
checkAtCompileTime(!(ELogVerbosity::VerbosityMask & ELogVerbosity::BreakOnLog), bad_ELogVerbosity_VerbosityMask2);
checkAtCompileTime((ELogVerbosity::VerbosityMask | ELogVerbosity::BreakOnLog) < 256, bad_ELogVerbosity_VerbosityMask3); // we use a byte for storage

/**
 * Enum that defines how the log times are to be displayed.
 */
namespace ELogTimes
{
	enum Type
	{
		None,
		UTC,
		SinceGStartTime
	};
}

// An output device.
class CORE_API FOutputDevice
{
public:
	FOutputDevice()
		 : bSuppressEventTag      (false)
		 , bAutoEmitLineTerminator(true)
	{}
	virtual ~FOutputDevice(){}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS

	FOutputDevice(const FOutputDevice&) = default;
	FOutputDevice& operator=(const FOutputDevice&) = default;

	#if PLATFORM_COMPILER_HAS_RVALUE_REFERENCES

		FOutputDevice(FOutputDevice&&) = default;
		FOutputDevice& operator=(FOutputDevice&&) = default;

	#endif

#else

	FORCEINLINE FOutputDevice(const FOutputDevice& Other)
		 : bSuppressEventTag      (Other.bSuppressEventTag)
		 , bAutoEmitLineTerminator(Other.bAutoEmitLineTerminator)
	{
	}

	FORCEINLINE FOutputDevice& operator=(const FOutputDevice& Other)
	{
		bSuppressEventTag       = Other.bSuppressEventTag;
		bAutoEmitLineTerminator = Other.bAutoEmitLineTerminator;
		return *this;
	}

	#if PLATFORM_COMPILER_HAS_RVALUE_REFERENCES

		FORCEINLINE FOutputDevice(FOutputDevice&& Other)
			 : bSuppressEventTag      (Other.bSuppressEventTag)
			 , bAutoEmitLineTerminator(Other.bAutoEmitLineTerminator)
		{
		}

		FORCEINLINE FOutputDevice& operator=(FOutputDevice&& Other)
		{
			bSuppressEventTag       = Other.bSuppressEventTag;
			bAutoEmitLineTerminator = Other.bAutoEmitLineTerminator;
			return *this;
		}

	#endif

#endif

	// static helpers
	static const TCHAR* VerbosityToString(ELogVerbosity::Type Verbosity);
	static FString FormatLogLine(ELogVerbosity::Type Verbosity, const class FName& Category, const TCHAR* Message = NULL, ELogTimes::Type LogTime = ELogTimes::None);


	// FOutputDevice interface.
	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )=0;
	virtual void Flush()
	{
	}

	/**
	 * Closes output device and cleans up. This can't happen in the destructor
	 * as we might have to call "delete" which cannot be done for static/ global
	 * objects.
	 */
	virtual void TearDown()
	{
	}

	void SetSuppressEventTag(bool bInSuppressEventTag)
	{
		bSuppressEventTag = bInSuppressEventTag;
	}
	FORCEINLINE bool GetSuppressEventTag() const	{	return bSuppressEventTag;	}
	void SetAutoEmitLineTerminator(bool bInAutoEmitLineTerminator)
	{
		bAutoEmitLineTerminator = bInAutoEmitLineTerminator;
	}
	FORCEINLINE bool GetAutoEmitLineTerminator() const	{	return bAutoEmitLineTerminator;	}


	/**
	 * @return whether this output device can be used on any thread.
	 */
	virtual bool CanBeUsedOnAnyThread() const
	{
		return false;
	}

	// Simple text printing.
	void Log( const TCHAR* S );
	void Log( ELogVerbosity::Type Verbosity, const TCHAR* S );
	void Log( const class FName& Category, ELogVerbosity::Type Verbosity, const TCHAR* Str );
	void Log( const FString& S );
	void Log( const FText& S );
	void Log( ELogVerbosity::Type Verbosity, const FString& S );
	void Log( const class FName& Category, ELogVerbosity::Type Verbosity, const FString& S );

	VARARG_DECL( void, void, {}, Logf, VARARG_NONE, const TCHAR*, VARARG_NONE, VARARG_NONE );
	VARARG_DECL( void, void, {}, Logf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(ELogVerbosity::Type Verbosity), VARARG_EXTRA(Verbosity) );
	VARARG_DECL( void, void, {}, CategorizedLogf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const class FName& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity), VARARG_EXTRA(Category) VARARG_EXTRA(Verbosity) );
protected:
	/** Whether to output the 'Log: ' type front... */
	bool bSuppressEventTag;
	/** Whether to output a line-terminator after each log call... */
	bool bAutoEmitLineTerminator;
};

// Error device.
class CORE_API FOutputDeviceError : public FOutputDevice
{
public:
	virtual void HandleError()=0;
};

/**
 * FMsg 
 * This struct contains functions for messaging with tools or debug logs.
 **/
struct CORE_API FMsg
{
	/** Sends a message to a remote tool. */
	static void SendNotificationString( const TCHAR* Message );

	/** Sends a formatted message to a remote tool. */
	static void VARARGS SendNotificationStringf( const TCHAR *Format, ... );

	/** Log function */
	VARARG_DECL( static void, static void, {}, Logf, VARARG_NONE, const TCHAR*, VARARG_EXTRA(const ANSICHAR* File) VARARG_EXTRA(int32 Line) VARARG_EXTRA(const class FName& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity), VARARG_EXTRA(File) VARARG_EXTRA(Line) VARARG_EXTRA(Category) VARARG_EXTRA(Verbosity) );
};

/**
 * FDebug
 * These functions offer debugging and diagnostic functionality and its presence 
 * depends on compiler switches.
 **/
#if DO_CHECK || DO_GUARD_SLOW
struct CORE_API FDebug
{
	/** Failed assertion handler.  Warning: May be called at library startup time. */
	static void VARARGS AssertFailed( const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Format=TEXT(""), ... );

	/**
	 * Called when an 'ensure' assertion fails; gathers stack data and generates and error report.
	 *
	 * @param	Expr	Code expression ANSI string (#code)
	 * @param	File	File name ANSI string (__FILE__)
	 * @param	Line	Line number (__LINE__)
	 * @param	Msg		Informative error message text
	 */
	static void EnsureFailed( const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Msg );

	/**
	 * Triggers a (potentially) non-fatal, non-intrusive error if bExpressionResult is zero
	 *
	 * @param	bExpressionResult	Expression that will trigger an error if resolves to zero
	 * @param	Expr	Code expression ANSI string (#code)
	 * @param	File	File name ANSI string (__FILE__)
	 * @param	Line	Line number (__LINE__)
	 * @param	Msg		Optional informative error message text
	 */
	static FORCEINLINE bool EnsureNotFalse( bool bExpressionResult, const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Msg = TEXT( "" ) )
	{
		if( bExpressionResult == 0 )
		{
			EnsureFailed( Expr, File, Line, Msg );
		}
	
		return bExpressionResult;
	}

	/**
	 * Triggers a (potentially) non-fatal, non-intrusive error if bExpressionResult is zero.  Takes a formatted string.
	 *
	 * @param	bExpressionResult	Expression that will trigger an error if resolves to zero
	 * @param	Expr	Code expression ANSI string (#code)
	 * @param	File	File name ANSI string (__FILE__)
	 * @param	Line	Line number (__LINE__)
	 * @param	FormattedMsg	Informative error message text with variable args
	 */
	static bool VARARGS EnsureNotFalseFormatted( bool bExpressionResult, const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* FormattedMsg, ... );
};
#endif // DO_CHECK || DO_GUARD_SLOW


/** 
 * FMessageDialog
 * These functions open a message dialog and display the specified informations
 * these.
 **/
struct CORE_API FMessageDialog
{
	/** Pops up a message dialog box containing the input string. */
	static void Debugf( const FText& Message );

	/** Pops up a message dialog box containing the last system error code in string form. */
	static void ShowLastError();

	static EAppReturnType::Type Open( EAppMsgType::Type MessageType, const FText& Message );
};


/** 
 * FError
 * Set of functions for error reporting 
 **/
struct CORE_API FError
{
	/** low level fater error handler. */
	static void VARARGS LowLevelFatal(const ANSICHAR* File, int32 Line, const TCHAR* Format=TEXT(""), ... );

	/** @name Exception handling */
	//@{
	/** For throwing string-exceptions which safely propagate through guard/unguard. */
	VARARG_DECL( static void VARARGS, static void, VARARG_NONE, Throwf, VARARG_NONE, const TCHAR*, VARARG_NONE, VARARG_NONE );
	//@}
};


// Any object that is capable of taking commands.
class CORE_API FExec
{
public:
	/**
	* Exec handler
	*
	* @param	InWorld	World context
	* @param	Cmd		Command to parse
	* @param	Ar		Output device to log to
	*
	* @return	true if command was handled, false otherwise
	*/
	virtual bool Exec( class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) PURE_VIRTUAL(FExec::Exec,return false;)
};


