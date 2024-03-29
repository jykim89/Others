// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	URL.cpp: Various file-management functions.
=============================================================================*/

#include "EnginePrivate.h"
#include "Net/UnrealNetwork.h"

/*-----------------------------------------------------------------------------
	FURL Statics.
-----------------------------------------------------------------------------*/

// Variables.
FUrlConfig FURL::UrlConfig;
bool FURL::bDefaultsInitialized=false;

// Static init.
void FURL::StaticInit()
{
	UrlConfig.Init();
	bDefaultsInitialized = true;
}
void FURL::StaticExit()
{
	UrlConfig.Reset();
	bDefaultsInitialized = false;
}

FArchive& operator<<( FArchive& Ar, FURL& U )
{
	Ar << U.Protocol << U.Host << U.Map << U.Portal << U.Op << U.Port << U.Valid;
	return Ar;
}

/*-----------------------------------------------------------------------------
	Internal.
-----------------------------------------------------------------------------*/

static bool ValidNetChar( const TCHAR* c )
{
	// NOTE: We purposely allow for SPACE characters inside URL strings, since we need to support player aliases
	//   on the URL that potentially have spaces in them.

	// @todo: Support true URL character encode/decode (e.g. %20 for spaces), so that we can be compliant with
	//   URL protocol specifications

	// NOTE: EQUALS characters (=) are not checked here because they're valid within fragments, but incoming
	//   option data should always be filtered of equals signs

	// / Is now allowed because absolute paths are required in various places

	if( FCString::Strchr( c, '?' ) || FCString::Strchr( c, '#' ) )		// ? and # delimit fragments
	{		
		return false;
	}

	return true;
}

/*-----------------------------------------------------------------------------
	Constructors.
-----------------------------------------------------------------------------*/

//
// Constuct a purely default, local URL from an optional filename.
//
FURL::FURL( const TCHAR* LocalFilename )
:	Protocol	( UrlConfig.DefaultProtocol )
,	Host		( UrlConfig.DefaultHost )
,	Port		( UrlConfig.DefaultPort )
,	Op			()
,	Portal		( UrlConfig.DefaultPortal )
,	Valid		( 1 )
{
	// strip off any extension from map name
	if (LocalFilename)
	{
		Map = FPaths::GetBaseFilename(LocalFilename);
	}
	else
	{
		Map = UGameMapsSettings::GetGameDefaultMap();
	}
}

//
// Helper function.
//
static inline TCHAR* HelperStrchr( TCHAR* Src, TCHAR A, TCHAR B )
{
	TCHAR* AA = FCString::Strchr( Src, A );
	TCHAR* BB = FCString::Strchr( Src, B );
	return (AA && (!BB || AA<BB)) ? AA : BB;
}


/**
 * Static: Removes any special URL characters from the specified string
 *
 * @param Str String to be filtered
 */
void FURL::FilterURLString( FString& Str )
{
	FString NewString;
	for( int32 CurCharIndex = 0; CurCharIndex < Str.Len(); ++CurCharIndex )
	{
		const TCHAR CurChar = Str[ CurCharIndex ];

		if( CurChar != ':' && CurChar != '?' && CurChar != '#' && CurChar != '=' )
		{
			NewString.AppendChar( Str[ CurCharIndex ] );
		}
	}

	Str = NewString;
}




//
// Construct a URL from text and an optional relative base.
//
FURL::FURL( FURL* Base, const TCHAR* TextURL, ETravelType Type )
:	Protocol	( UrlConfig.DefaultProtocol )
,	Host		( UrlConfig.DefaultHost )
,	Port		( UrlConfig.DefaultPort )
,	Map			( UGameMapsSettings::GetGameDefaultMap() )
,	Op			()
,	Portal		( UrlConfig.DefaultPortal )
,	Valid		( 1 )
{
	check(TextURL);

	if( !bDefaultsInitialized )
	{
		FURL::StaticInit();
		Protocol = UrlConfig.DefaultProtocol;
		Host = UrlConfig.DefaultHost;
		Port = UrlConfig.DefaultPort;
		Portal = UrlConfig.DefaultPortal;
	}

	// Make a copy.
	const int32 URLLength = FCString::Strlen(TextURL);
	TCHAR* TempURL = new TCHAR[URLLength+1];
	TCHAR* URL = TempURL;

	FCString::Strcpy( TempURL, URLLength + 1, TextURL );

	// Copy Base.
	if( Type==TRAVEL_Relative )
	{
		check(Base);
		Protocol = Base->Protocol;
		Host     = Base->Host;
		Map      = Base->Map;
		Portal   = Base->Portal;
		Port     = Base->Port;
	}
	if( Type==TRAVEL_Relative || Type==TRAVEL_Partial )
	{
		check(Base);
		for( int32 i=0; i<Base->Op.Num(); i++ )
		{
			new(Op)FString(Base->Op[i]);
		}
	}

	// Skip leading blanks.
	while( *URL == ' ' )
	{
		URL++;
	}

	// Options.
	TCHAR* s = HelperStrchr(URL,'?','#');
	if( s )
	{
		TCHAR OptionChar=*s, NextOptionChar=0;
		*(s++) = 0;
		do
		{
			TCHAR* t = HelperStrchr(s,'?','#');
			if( t )
			{
				NextOptionChar = *t;
				*(t++) = 0;
			}
			if( !ValidNetChar( s ) )
			{
				*this = FURL();
				Valid = 0;
				break;
			}
			if( OptionChar=='?' )
			{
				if (s && s[0] == '-')
				{
					// Remove an option if it starts with -
					s++;
					RemoveOption( s );
				}
				else
				{
					AddOption( s );
				}
			}
			else
			{
				Portal = s;
			}
			s = t;
			OptionChar = NextOptionChar;
		} while( s );
	}

	if (Valid == 1)
	{
		// Handle pure filenames & Posix paths.
		bool FarHost=0;
		bool FarMap=0;
		if( FCString::Strlen(URL)>2 && (URL[1]==':' || (URL[0]=='/' && !FPackageName::IsValidLongPackageName(URL, true))) )
		{
			// Pure filename.
			Protocol = UrlConfig.DefaultProtocol;
			Host = UrlConfig.DefaultHost;
			Map = URL;
			Portal = UrlConfig.DefaultPortal;
			URL = NULL;
			FarHost = 1;
			FarMap = 1;
			Host = TEXT("");
		}
		else
		{
			// Determine location of the first opening square bracket.
			// Square brackets enclose an IPv6 address.
			const TCHAR* SquareBracket = FCString::Strchr(URL, '[');

			// Parse protocol. Don't consider colons that occur after the opening square
			// brace, because they are valid characters in an IPv6 address.
			if
			(	(FCString::Strchr(URL,':')!=NULL)
			&&	(FCString::Strchr(URL,':')>URL+1)
			&&  (!SquareBracket || (FCString::Strchr(URL,':')<SquareBracket))
			&&	(FCString::Strchr(URL,'.')==NULL || FCString::Strchr(URL,':')<FCString::Strchr(URL,'.')) )
			{
				TCHAR* ss = URL;
				URL      = FCString::Strchr(URL,':');
				*(URL++)   = 0;
				Protocol = ss;
			}

			// Parse optional leading double-slashes.
			if( *URL=='/' && *(URL+1) =='/' )
			{
				URL += 2;
				FarHost = 1;
				Host = TEXT("");
			}

			// Parse optional host name and port.
			const TCHAR* Dot = FCString::Strchr(URL,'.');
			const int32 ExtLen = FPackageName::GetMapPackageExtension().Len();

			if
			(	(Dot)
			&&	(Dot-URL>0)
			&&	(FCString::Strnicmp( Dot, *FPackageName::GetMapPackageExtension(), FPackageName::GetMapPackageExtension().Len() )!=0 || FChar::IsAlnum(Dot[ExtLen]) )
			&&	(FCString::Strnicmp( Dot+1,*UrlConfig.DefaultSaveExt, UrlConfig.DefaultSaveExt.Len() )!=0 || FChar::IsAlnum(Dot[UrlConfig.DefaultSaveExt.Len()+1]) )
			&&	(FCString::Strnicmp( Dot+1,TEXT("demo"), 4 ) != 0 || FChar::IsAlnum(Dot[5])) )
			{
				TCHAR* ss = URL;
				URL     = FCString::Strchr(URL,'/');
				if( URL )
				{
					*(URL++) = 0;
				}
				TCHAR* t = FCString::Strchr(ss,':');
				if( t )
				{
					// Port.
					*(t++) = 0;
					Port = FCString::Atoi( t );
				}
				Host = ss;
				if( FCString::Stricmp(*Protocol,*UrlConfig.DefaultProtocol)==0 )
				{
					Map = UGameMapsSettings::GetGameDefaultMap();
				}
				else
				{
					Map = TEXT("");
				}
				FarHost = 1;
			}

			// Parse IPv6 host.
			if(URL && SquareBracket)
			{
				const TCHAR* ss = URL;
				URL = FCString::Strchr(URL, ']');
				if (URL)
				{
					*URL = 0;
					Host = SquareBracket + 1;
				}
				if( FCString::Stricmp(*Protocol,*UrlConfig.DefaultProtocol)==0 )
				{
					Map = UGameMapsSettings::GetGameDefaultMap();
				}
				else
				{
					Map = TEXT("");
				}
				FarHost = 1;
			}
		}
	}

	// Parse optional map
	if (Valid == 1 && URL && *URL)
	{
		// Map.
		if (*URL != '/')
		{
			// find full pathname from short map name
			FString MapFullName;
			FText MapNameError;
			if (FPaths::FileExists(URL))
			{
				Map = FPackageName::FilenameToLongPackageName(URL);
			}
			else if (!FPackageName::DoesPackageNameContainInvalidCharacters(URL, &MapNameError) && FPackageName::SearchForPackageOnDisk(FString(URL) + FPackageName::GetMapPackageExtension(), &MapFullName))
			{
				Map = MapFullName;
			}
			else
			{
				// can't find file, invalidate and bail
				UE_CLOG(MapNameError.ToString().Len() > 0, LogLongPackageNames, Warning, TEXT("URL: %s: %s"), URL, *MapNameError.ToString());
				*this = FURL();
				Valid = 0;
			}
		}
		else
		{
			// already a full pathname
			Map = URL;
		}

	}
	
	// Validate everything.
	// The FarHost check does not serve any purpose I can see, and will just cause valid URLs to fail (URLs with no options, why does a URL
	// need an option to be valid?)
	if (Valid == 1 && (!ValidNetChar(*Protocol) || !ValidNetChar(*Host) /*|| !ValidNetChar(*Map)*/ || !ValidNetChar(*Portal) /*|| (!FarHost && !FarMap && !Op.Num())*/))
	{
		*this = FURL();
		Valid = 0;
	}

	// If Valid == 1, success.

	delete[] TempURL;
}

/*-----------------------------------------------------------------------------
	Conversion to text.
-----------------------------------------------------------------------------*/

//
// Convert this URL to text.
//
FString FURL::ToString (bool FullyQualified) const
{
	FString Result;

	// Emit protocol.
	if ((Protocol != UrlConfig.DefaultProtocol) || FullyQualified)
	{
		Result += Protocol;
		Result += TEXT(":");

		if (Host != UrlConfig.DefaultHost)
		{
			Result += TEXT("//");
		}
	}

	// Emit host.
	if ((Host != UrlConfig.DefaultHost) || (Port != UrlConfig.DefaultPort))
	{
		Result += Host;

		if (Port != UrlConfig.DefaultPort)
		{
			Result += TEXT(":");
			Result += FString::Printf(TEXT("%i"), Port);
		}

		Result += TEXT("/");
	}

	// Emit map.
	if (Map.Len() > 0)
	{
		Result += Map;
	}

	// Emit options.
	for (int32 i = 0; i < Op.Num(); i++)
	{
		Result += TEXT("?");
		Result += Op[i];
	}

	// Emit portal.
	if (Portal.Len() > 0)
	{
		Result += TEXT("#");
		Result += Portal;
	}

	return Result;
}

/*-----------------------------------------------------------------------------
	Informational.
-----------------------------------------------------------------------------*/

//
// Return whether this URL corresponds to an internal object, i.e. an Unreal
// level which this app can try to connect to locally or on the net. If this
// is false, the URL refers to an object that a remote application like Internet
// Explorer can execute.
//
bool FURL::IsInternal() const
{
	return Protocol == UrlConfig.DefaultProtocol;
}

//
// Return whether this URL corresponds to an internal object on this local 
// process. In this case, no Internet use is necessary.
//
bool FURL::IsLocalInternal() const
{
	return IsInternal() && Host.Len()==0;
}

//
// Add a unique option to the URL, replacing any existing one.
//
void FURL::AddOption( const TCHAR* Str )
{
	int32 Match = FCString::Strchr(Str, '=') ? (FCString::Strchr(Str, '=') - Str) : FCString::Strlen(Str);
	int32 i;

	for (i = 0; i < Op.Num(); i++)
	{
		if (FCString::Strnicmp(*Op[i], Str, Match) == 0 && ((*Op[i])[Match] == '=' || (*Op[i])[Match] == '\0'))
		{
			break;
		}
	}

	if (i == Op.Num())
	{
		new(Op) FString(Str);
	}
	else
	{
		Op[i] = Str;
	}
}


//
// Remove an option from the URL
//
void FURL::RemoveOption( const TCHAR* Key, const TCHAR* Section, const FString& Filename )
{
	if ( !Key )
		return;

	for ( int32 i = Op.Num() - 1; i >= 0; i-- )
	{
		if ( Op[i].Left(FCString::Strlen(Key)) == Key )
		{
			FConfigSection* Sec = GConfig->GetSectionPrivate( Section ? Section : TEXT("DefaultPlayer"), 0, 0, Filename );
			if ( Sec )
			{
				if (Sec->Remove( Key ) > 0)
					GConfig->Flush( 0, Filename );
			}

			Op.RemoveAt(i);
		}
	}
}

//
// Load URL from config.
//
void FURL::LoadURLConfig( const TCHAR* Section, const FString& Filename )
{
	TArray<FString> Options;
	GConfig->GetSection( Section, Options, Filename );
	for( int32 i=0;i<Options.Num();i++ )
	{
		AddOption( *Options[i] );
	}
}

//
// Save URL to config.
//
void FURL::SaveURLConfig( const TCHAR* Section, const TCHAR* Item, const FString& Filename ) const
{
	for( int32 i=0; i<Op.Num(); i++ )
	{
		TCHAR Temp[1024];
		FCString::Strcpy( Temp, *Op[i] );
		TCHAR* Value = FCString::Strchr(Temp,'=');
		if( Value )
		{
			*Value++ = 0;
			if( FCString::Stricmp(Temp,Item)==0 )
				GConfig->SetString( Section, Temp, Value, Filename );
		}
	}
}

//
// See if the URL contains an option string.
//
bool FURL::HasOption( const TCHAR* Test ) const
{
	return GetOption( Test, NULL ) != NULL;
}

const TCHAR* FURL::GetOption( const TCHAR* Match, const TCHAR* Default ) const
{
	const int32 Len = FCString::Strlen(Match);
	
	if( Len > 0 )
	{
		for( int32 i = 0; i < Op.Num(); i++ ) 
		{
			const TCHAR* s = *Op[i];
			if( FCString::Strnicmp( s, Match, Len ) == 0 ) 
			{
				if (s[Len-1] == '=' || s[Len] == '=' || s[Len] == '\0') 
				{
					return s + Len;
				}
			}
		}
	}

	return Default;
}

/*-----------------------------------------------------------------------------
	Comparing.
-----------------------------------------------------------------------------*/

//
// Compare two URL's to see if they refer to the same exact thing.
//
bool FURL::operator==( const FURL& Other ) const
{
	if
	(	Protocol	!= Other.Protocol
	||	Host		!= Other.Host
	||	Map			!= Other.Map
	||	Port		!= Other.Port
	||  Op.Num()    != Other.Op.Num() )
		return 0;

	for( int32 i=0; i<Op.Num(); i++ )
		if( Op[i] != Other.Op[i] )
			return 0;

	return 1;
}

