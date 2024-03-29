// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CorePrivate.h"

FScopeLogTime::FScopeLogTime( const TCHAR* InName, FTotalTimeAndCount* InGlobal /*= nullptr */, EScopeLogTimeUnits InUnits /*= ScopeLog_Milliseconds */ ) 
: StartTime( FPlatformTime::Seconds() )
, Name( InName )
, Cumulative( InGlobal )
, Units( InUnits )
{}

FScopeLogTime::~FScopeLogTime()
{
	const double ScopedTime = FPlatformTime::Seconds() - StartTime;
	const FString DisplayUnitsString = GetDisplayUnitsString();
	if( Cumulative )
	{
		Cumulative->Key += ScopedTime;
		Cumulative->Value++;

		const double Average = Cumulative->Key / (double)Cumulative->Value;
		UE_LOG( LogStats, Log, TEXT( "%32s - %6.3f %s - Total %6.2f s / %5u / %6.3f %s" ), *Name, GetDisplayScopedTime(ScopedTime), *DisplayUnitsString, Cumulative->Key, Cumulative->Value, GetDisplayScopedTime(Average), *DisplayUnitsString );
	}
	else
	{
		UE_LOG( LogStats, Log, TEXT( "%32s - %6.3f %s" ), *Name, GetDisplayScopedTime(ScopedTime), *DisplayUnitsString );
	}
}

double FScopeLogTime::GetDisplayScopedTime(double InScopedTime) const
{
	switch(Units)
	{
		case ScopeLog_Seconds: return InScopedTime;
		case ScopeLog_Milliseconds:
		default:
			return InScopedTime * 1000.0f;
	}

	return InScopedTime * 1000.0f;
}

FString FScopeLogTime::GetDisplayUnitsString() const
{
	switch (Units)
	{
		case ScopeLog_Seconds: return TEXT("s");
		case ScopeLog_Milliseconds:
		default:
			return TEXT("ms");
	}

	return TEXT("ms");
}