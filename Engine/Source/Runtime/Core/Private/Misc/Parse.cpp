// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CorePrivate.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
/**
 * Needed for the console command "DumpConsoleCommands"
 * How it works:
 *   - GConsoleCommandLibrary is set to point at a local instance of ConsoleCommandLibrary
 *   - a dummy command search is triggered which gathers all commands in a hashed set
 *   - sort all gathered commands in human friendly way
 *   - log all commands
 *   - GConsoleCommandLibrary is set 0
 */
class ConsoleCommandLibrary
{
public:
	ConsoleCommandLibrary(const FString& InPattern);

	~ConsoleCommandLibrary();

	void OnParseCommand(const TCHAR* Match)
	{
		// -1 to not take the "*" after the pattern into account
		if(FCString::Strnicmp(Match, *Pattern, Pattern.Len() - 1) == 0)
		{
			KnownNames.Add(Match);
		}
	}

private:

	TSet<FString>		KnownNames;
	const FString&		Pattern;

	friend void ConsoleCommandLibrary_DumpLibrary(class UWorld* InWorld, FExec& SubSystem, const FString& Pattern, FOutputDevice& Ar);
};

// 0 if gathering of names is deactivated
ConsoleCommandLibrary* GConsoleCommandLibrary;

ConsoleCommandLibrary::ConsoleCommandLibrary(const FString& InPattern) :Pattern(InPattern)
{
	// activate name gathering
	GConsoleCommandLibrary = this;
}

ConsoleCommandLibrary::~ConsoleCommandLibrary()
{
	// deactivate name gathering
	GConsoleCommandLibrary = 0;
}



class FConsoleVariableDumpVisitor 
{
public:
	// @param Name must not be 0
	// @param CVar must not be 0
	static void OnConsoleVariable(const TCHAR *Name, IConsoleObject* CVar,TSet<FString>& Sink)
	{
		if(CVar->TestFlags(ECVF_Unregistered))
		{
			return;
		}

		Sink.Add(Name);
	}
};

/** Needed for the console command "DumpConsoleCommands" */
void ConsoleCommandLibrary_DumpLibrary(UWorld* InWorld, FExec& SubSystem, const FString& Pattern, FOutputDevice& Ar)
{
	ConsoleCommandLibrary LocalConsoleCommandLibrary(Pattern);

	FOutputDeviceNull Null;

	bool bExecuted = SubSystem.Exec( InWorld, *Pattern, Null);


	{
		IConsoleManager::Get().ForEachConsoleObject(
			FConsoleObjectVisitor::CreateStatic< TSet<FString>& >(
			&FConsoleVariableDumpVisitor::OnConsoleVariable,
			LocalConsoleCommandLibrary.KnownNames ) );
	}


	LocalConsoleCommandLibrary.KnownNames.Sort( TLess<FString>() );

	for(TSet<FString>::TConstIterator It(LocalConsoleCommandLibrary.KnownNames); It; ++It)
	{
		const FString Name = *It;

		Ar.Logf(TEXT("%s"), *Name);
	}
	Ar.Logf(TEXT(""));

	// the pattern (e.g. Motion*) should not really trigger the execution
	if(bExecuted)
	{
		Ar.Logf(TEXT("ERROR: The function was supposed to only find matching commands but not have any side effect."));
		Ar.Logf(TEXT("However Exec() returned true which means we either executed a command or the command parsing returned true where it shouldn't."));
	}
}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

//
// Get a string from a text string.
//
bool FParse::Value(
	const TCHAR* Stream, 
	const TCHAR* Match,
	TCHAR*		 Value,
	int32			 MaxLen,
	bool		 bShouldStopOnComma
)
{
	const TCHAR* Found = FCString::Strfind(Stream,Match);
	const TCHAR* Start;

	if( Found )
	{
		Start = Found + FCString::Strlen(Match);
		if( *Start == '\x22' )
		{
			// Quoted string with spaces.
			FCString::Strncpy( Value, Start+1, MaxLen );
			Value[MaxLen-1]=0;
			TCHAR* Temp = FCString::Strstr( Value, TEXT("\x22") );
			if( Temp != NULL )
				*Temp=0;
		}
		else
		{
			// Non-quoted string without spaces.
			FCString::Strncpy( Value, Start, MaxLen );
			Value[MaxLen-1]=0;
			TCHAR* Temp;
			Temp = FCString::Strstr( Value, TEXT(" ")  ); if( Temp ) *Temp=0;
			Temp = FCString::Strstr( Value, TEXT("\r") ); if( Temp ) *Temp=0;
			Temp = FCString::Strstr( Value, TEXT("\n") ); if( Temp ) *Temp=0;
			Temp = FCString::Strstr( Value, TEXT("\t") ); if( Temp ) *Temp=0;
			if (bShouldStopOnComma)
			{
				Temp = FCString::Strstr( Value, TEXT(",")  ); if( Temp ) *Temp=0;
			}
		}
		return 1;
	}
	else return 0;
}

//
// Checks if a command-line parameter exists in the stream.
//
bool FParse::Param( const TCHAR* Stream, const TCHAR* Param )
{
	const TCHAR* Start = Stream;
	if( *Stream )
	{
		while( (Start=FCString::Strfind(Start+1,Param)) != NULL )
		{
			if( Start>Stream && (Start[-1]=='-' || Start[-1]=='/') )
			{
				const TCHAR* End = Start + FCString::Strlen(Param);
				if ( End == NULL || *End == 0 || FChar::IsWhitespace(*End) )
				{
					return true;
				}
			}
		}
	}
	return false;
}

// 
// Parse a string.
//
bool FParse::Value( const TCHAR* Stream, const TCHAR* Match, FString& Value, bool bShouldStopOnComma )
{
	TCHAR Temp[4096]=TEXT("");
	if( FParse::Value( Stream, Match, Temp, ARRAY_COUNT(Temp), bShouldStopOnComma ) )
	{
		Value = Temp;
		return 1;
	}
	else return 0;
}

// 
// Parse an Text token
// This is expected to in the form NSLOCTEXT("Namespace","Key","SourceString") or LOCTEXT("Key","SourceString")
//
bool FParse::Text( const TCHAR* Str, FText& Value, const TCHAR* Namespace )
{
	while( FChar::IsWhitespace( *Str ) )
	{
		++Str;
	}

	//this prevents our source code text gatherer from trying to gather the following messages
#define LOC_DEFINE_REGION
	const bool bFoundNSLocText = FCString::Strncmp( Str, TEXT("NSLOCTEXT"), 9 ) == 0;
	const bool bFoundLocText = !bFoundNSLocText && FCString::Strncmp( Str, TEXT("LOCTEXT"), 7 ) == 0;
#undef LOC_DEFINE_REGION

	if( bFoundNSLocText || (Namespace && bFoundLocText) )
	{
		Str += bFoundNSLocText ? 9 : 7;
		while( FChar::IsWhitespace( *Str ) )
		{
			++Str;
		}

		if( *Str != '(' )
		{
			return false;
		}
		++Str;

		while( FChar::IsWhitespace( *Str ) )
		{
			++Str;
		}

		if( *Str )
		{
			bool bInQuotes = false;
			bool bHaveEscapeCode = false;
			bool bError = false;
			int32 StringCount = 0;
			int32 CommaCount = 0;
			FString ParsedString;
			FString NamespaceString;
			FString KeyString;
			FString SourceString;
			const int32 ExpectedStringCount = bFoundNSLocText ? 3 : 2;

			while( *Str && *Str != ')' && !bError )
			{
				const TCHAR c = *Str;
				if( bInQuotes )
				{
					// Parsing string
					if( bHaveEscapeCode )
					{
						// Handle escape code
						if( c == 'r' )
						{
							ParsedString += '\r';
						}
						else if( c == 'n' )
						{
							ParsedString += '\n';
						}
						else if( c == '"' )
						{
							ParsedString += '"';
						}
						else if( c == 't' )
						{
							ParsedString += '\t';
						}
						else if( c == '\\' )
						{
							ParsedString += '\\';
						}
						else
						{
							bError = true;
						}
						bHaveEscapeCode = false;
					}
					else
					{
						if( c == '"' )
						{
							// Closing quote of parsed string
							if( !ParsedString.IsEmpty() )
							{
								bInQuotes = false;
								++StringCount;
								if( StringCount == ExpectedStringCount - 2 )
								{
									NamespaceString = ParsedString;
								}
								else if( StringCount == ExpectedStringCount - 1 )
								{
									KeyString = ParsedString;
								}						
							}
							else
							{
								bError = true;
							}
						}
						else if( c == '\\' )
						{
							bHaveEscapeCode = true;
						}
						else
						{
							ParsedString += c;
						}
					}
				}
				else
				{
					// In between strings
					if( c == '"' )
					{
						// Opening quote of parsed string
						if( StringCount < ExpectedStringCount && StringCount == CommaCount )
						{
							ParsedString = TEXT("");
							bInQuotes = true;
						}
						else
						{
							bError = true;
						}
					}
					else if( c == ',' )
					{
						if( CommaCount >= StringCount )
						{
							bError = true;
						}
						else
						{
							++CommaCount;
						}
					}
					else if( !FChar::IsWhitespace( c ) )
					{
						bError = true;
					}
				}
				++Str;
			}

			if( *Str == ')' && !bError && StringCount == ExpectedStringCount )
			{
				if ( !FText::FindText( bFoundNSLocText ? NamespaceString : Namespace, KeyString, /*OUT*/Value ) )
				{
					Value = FText::FromString( ParsedString );
				}

				return true;
			}
		}
	}

	return false;
}

// 
// Parse an Text.
// This is expected to in the form NSLOCTEXT("Namespace","Key","SourceString") or LOCTEXT("Key","SourceString")
//
bool FParse::Value( const TCHAR* Stream, const TCHAR* Match, FText& Value, const TCHAR* Namespace )
{
	// The FText 
	Stream = FCString::Strfind( Stream, Match );
	if( Stream )
	{
		Stream += FCString::Strlen( Match );
		return FParse::Text( Stream, Value, Namespace );
	}

	return 0;
}

//
// Parse a quadword.
//
bool FParse::Value( const TCHAR* Stream, const TCHAR* Match, uint64& Value )
{
	return FParse::Value( Stream, Match, *(int64*)&Value );
}

//
// Parse a signed quadword.
//
bool FParse::Value( const TCHAR* Stream, const TCHAR* Match, int64& Value )
{
	TCHAR Temp[4096]=TEXT(""), *Ptr=Temp;
	if( FParse::Value( Stream, Match, Temp, ARRAY_COUNT(Temp) ) )
	{
		Value = 0;
		bool Negative = (*Ptr=='-');
		Ptr += Negative;
		while( *Ptr>='0' && *Ptr<='9' )
			Value = Value*10 + *Ptr++ - '0';
		if( Negative )
			Value = -Value;
		return 1;
	}
	else return 0;
}

//
// Get a name.
//
bool FParse::Value(	const TCHAR* Stream, const TCHAR* Match, FName& Name )
{
	TCHAR TempStr[NAME_SIZE];

	if( !FParse::Value(Stream,Match,TempStr,NAME_SIZE) )
	{
		return 0;
	}

	Name = FName(TempStr);

	return 1;
}

//
// Get a uint32.
//
bool FParse::Value( const TCHAR* Stream, const TCHAR* Match, uint32& Value )
{
	const TCHAR* Temp = FCString::Strfind(Stream,Match);
	TCHAR* End;
	if( Temp==NULL )
		return 0;
	Value = FCString::Strtoi( Temp + FCString::Strlen(Match), &End, 10 );

	return 1;
}

//
// Get a byte.
//
bool FParse::Value( const TCHAR* Stream, const TCHAR* Match, uint8& Value )
{
	const TCHAR* Temp = FCString::Strfind(Stream,Match);
	if( Temp==NULL )
		return 0;
	Temp += FCString::Strlen( Match );
	Value = (uint8)FCString::Atoi( Temp );
	return Value!=0 || FChar::IsDigit(Temp[0]);
}

//
// Get a signed byte.
//
bool FParse::Value( const TCHAR* Stream, const TCHAR* Match, int8& Value )
{
	const TCHAR* Temp = FCString::Strfind(Stream,Match);
	if( Temp==NULL )
		return 0;
	Temp += FCString::Strlen( Match );
	Value = FCString::Atoi( Temp );
	return Value!=0 || FChar::IsDigit(Temp[0]);
}

//
// Get a word.
//
bool FParse::Value( const TCHAR* Stream, const TCHAR* Match, uint16& Value )
{
	const TCHAR* Temp = FCString::Strfind( Stream, Match );
	if( Temp==NULL )
		return 0;
	Temp += FCString::Strlen( Match );
	Value = (uint16)FCString::Atoi( Temp );
	return Value!=0 || FChar::IsDigit(Temp[0]);
}

//
// Get a signed word.
//
bool FParse::Value( const TCHAR* Stream, const TCHAR* Match, int16& Value )
{
	const TCHAR* Temp = FCString::Strfind( Stream, Match );
	if( Temp==NULL )
		return 0;
	Temp += FCString::Strlen( Match );
	Value = (int16)FCString::Atoi( Temp );
	return Value!=0 || FChar::IsDigit(Temp[0]);
}

//
// Get a floating-point number.
//
bool FParse::Value( const TCHAR* Stream, const TCHAR* Match, float& Value )
{
	const TCHAR* Temp = FCString::Strfind( Stream, Match );
	if( Temp==NULL )
		return 0;
	Value = FCString::Atof( Temp+FCString::Strlen(Match) );
	return 1;
}

//
// Get a signed double word.
//
bool FParse::Value( const TCHAR* Stream, const TCHAR* Match, int32& Value )
{
	const TCHAR* Temp = FCString::Strfind( Stream, Match );
	if( Temp==NULL )
		return 0;
	Value = FCString::Atoi( Temp + FCString::Strlen(Match) );
	return 1;
}

//
// Get a boolean value.
//
bool FParse::Bool( const TCHAR* Stream, const TCHAR* Match, bool& OnOff )
{
	TCHAR TempStr[16];
	if( FParse::Value( Stream, Match, TempStr, 16 ) )
	{
		OnOff = FCString::ToBool(TempStr);
		return 1;
	}
	else return 0;
}

//
// Get a globally unique identifier.
//
bool FParse::Value( const TCHAR* Stream, const TCHAR* Match, class FGuid& Guid )
{
	TCHAR Temp[256];
	if( !FParse::Value( Stream, Match, Temp, ARRAY_COUNT(Temp) ) )
		return 0;

	Guid.A = Guid.B = Guid.C = Guid.D = 0;
	if( FCString::Strlen(Temp)==32 )
	{
		TCHAR* End;
		Guid.D = FCString::Strtoi( Temp+24, &End, 16 ); Temp[24]=0;
		Guid.C = FCString::Strtoi( Temp+16, &End, 16 ); Temp[16]=0;
		Guid.B = FCString::Strtoi( Temp+8,  &End, 16 ); Temp[8 ]=0;
		Guid.A = FCString::Strtoi( Temp+0,  &End, 16 ); Temp[0 ]=0;
	}
	return 1;
}


//
// Sees if Stream starts with the named command.  If it does,
// skips through the command and blanks past it.  Returns 1 of match,
// 0 if not.
//
bool FParse::Command( const TCHAR** Stream, const TCHAR*  Match, bool bParseMightTriggerExecution )
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(GConsoleCommandLibrary)
	{
		GConsoleCommandLibrary->OnParseCommand(Match);
		
		if(bParseMightTriggerExecution)
		{
			// Better we fail the test - we only wanted to find all commands.
			return false;
		}
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	while( (**Stream==' ')||(**Stream==9) )
		(*Stream)++;

	if( FCString::Strnicmp(*Stream,Match,FCString::Strlen(Match))==0 )
	{
		*Stream += FCString::Strlen(Match);
		if( !FChar::IsAlnum(**Stream))
		{
			while ((**Stream==' ')||(**Stream==9)) (*Stream)++;
			return 1; // Success.
		}
		else
		{
			*Stream -= FCString::Strlen(Match);
			return 0; // Only found partial match.
		}
	}
	else return 0; // No match.
}

//
// Get next command.  Skips past comments and cr's.
//
void FParse::Next( const TCHAR** Stream )
{
	// Skip over spaces, tabs, cr's, and linefeeds.
	SkipJunk:
	while( **Stream==' ' || **Stream==9 || **Stream==13 || **Stream==10 )
		++*Stream;

	if( **Stream==';' )
	{
		// Skip past comments.
		while( **Stream!=0 && **Stream!=10 && **Stream!=13 )
			++*Stream;
		goto SkipJunk;
	}

	// Upon exit, *Stream either points to valid Stream or a nul.
}

//
// Grab the next space-delimited string from the input stream.
// If quoted, gets entire quoted string.
//
bool FParse::Token( const TCHAR*& Str, TCHAR* Result, int32 MaxLen, bool UseEscape )
{
	int32 Len=0;

	// Skip preceeding spaces and tabs.
	while( FChar::IsWhitespace(*Str) )
	{
		Str++;
	}

	if( *Str == TEXT('"') )
	{
		// Get quoted string.
		Str++;
		while( *Str && *Str!=TEXT('"') && (Len+1)<MaxLen )
		{
			TCHAR c = *Str++;
			if( c=='\\' && UseEscape )
			{
				// Get escape.
				c = *Str++;
				if( !c )
				{
					break;
				}
			}
			if( (Len+1)<MaxLen )
			{
				Result[Len++] = c;
			}
		}
		if( *Str==TEXT('"') )
		{
			Str++;
		}
	}
	else
	{
		// Get unquoted string (that might contain a quoted part, which will be left intact).
		// For example, -ARG="foo bar baz", will be treated as one token, with quotes intact
		bool bInQuote = false;

		while (1)
		{
			TCHAR Character = *Str;
			if ((Character == 0) || (FChar::IsWhitespace(Character) && !bInQuote))
			{
				break;
			}
			Str++;

			// Preserve escapes if they're in a quoted string (the check for " is in the else to let \" work as expected)
			if (Character == TEXT('\\') && UseEscape && bInQuote)
			{
				if ((Len+1) < MaxLen)
				{
					Result[Len++] = Character;
				}

				Character = *Str;
				if (!Character)
				{
					break;
				}
				Str++;
			}
			else if (Character == TEXT('"'))
			{
				bInQuote = !bInQuote;
			}

			if( (Len+1)<MaxLen )
			{
				Result[Len++] = Character;
			}
		}
	}
	Result[Len]=0;
	return Len!=0;
}

bool FParse::Token( const TCHAR*& Str, FString& Arg, bool UseEscape )
{
	Arg.Empty();

	// Skip preceeding spaces and tabs.
	while( FChar::IsWhitespace(*Str) )
	{
		Str++;
	}

	if ( *Str == TEXT('"') )
	{
		// Get quoted string.
		Str++;
		while( *Str && *Str != TCHAR('"') )
		{
			TCHAR c = *Str++;
			if( c==TEXT('\\') && UseEscape )
			{
				// Get escape.
				c = *Str++;
				if( !c )
				{
					break;
				}
			}

			Arg += c;
		}

		if ( *Str == TEXT('"') )
		{
			Str++;
		}
	}
	else
	{
		// Get unquoted string (that might contain a quoted part, which will be left intact).
		// For example, -ARG="foo bar baz", will be treated as one token, with quotes intact
		bool bInQuote = false;

		while (1)
		{
			TCHAR Character = *Str;
			if ((Character == 0) || (FChar::IsWhitespace(Character) && !bInQuote))
			{
				break;
			}
			Str++;

			// Preserve escapes if they're in a quoted string (the check for " is in the else to let \" work as expected)
			if (Character == TEXT('\\') && UseEscape && bInQuote)
			{
				Arg += Character;

				Character = *Str;
				if (!Character)
				{
					break;
				}
				Str++;
			}
			else if (Character == TEXT('"'))
			{
				bInQuote = !bInQuote;
			}

			Arg += Character;
		}
	}

	return Arg.Len() > 0;
}
FString FParse::Token( const TCHAR*& Str, bool UseEscape )
{
	TCHAR Buffer[1024];
	if( FParse::Token( Str, Buffer, ARRAY_COUNT(Buffer), UseEscape ) )
		return Buffer;
	else
		return TEXT("");
}

bool FParse::AlnumToken(const TCHAR*& Str, FString& Arg)
{
	Arg.Empty();

	// Skip preceeding spaces and tabs.
	while (FChar::IsWhitespace(*Str))
	{
		Str++;
	}

	while (FChar::IsAlnum(*Str) || *Str == TEXT('_'))
	{
		Arg += *Str;
		Str++;
	}

	return Arg.Len() > 0;
}

//
// Get a line of Stream (everything up to, but not including, CR/LF.
// Returns 0 if ok, nonzero if at end of stream and returned 0-length string.
//
bool FParse::Line
(
	const TCHAR**	Stream,
	TCHAR*			Result,
	int32				MaxLen,
	bool			Exact
)
{
	bool GotStream=0;
	bool IsQuoted=0;
	bool Ignore=0;

	*Result=0;
	while( **Stream!=0 && **Stream!=10 && **Stream!=13 && --MaxLen>0 )
	{
		// Start of comments.
		if( !IsQuoted && !Exact && (*Stream)[0]=='/' && (*Stream)[1]=='/' )
			Ignore = 1;
		
		// Command chaining.
		if( !IsQuoted && !Exact && **Stream=='|' )
			break;

		// Check quoting.
		IsQuoted = IsQuoted ^ (**Stream==34);
		GotStream=1;

		// Got stuff.
		if( !Ignore )
			*(Result++) = *((*Stream)++);
		else
			(*Stream)++;
	}
	if( Exact )
	{
		// Eat up exactly one CR/LF.
		if( **Stream == 13 )
			(*Stream)++;
		if( **Stream == 10 )
			(*Stream)++;
	}
	else
	{
		// Eat up all CR/LF's.
		while( **Stream==10 || **Stream==13 || **Stream=='|' )
			(*Stream)++;
	}
	*Result=0;
	return **Stream!=0 || GotStream;
}
bool FParse::Line
(
	const TCHAR**	Stream,
	FString&		Result,
	bool			Exact
)
{
	bool GotStream=0;
	bool IsQuoted=0;
	bool Ignore=0;

	Result = TEXT("");

	while( **Stream!=0 && **Stream!=10 && **Stream!=13 )
	{
		// Start of comments.
		if( !IsQuoted && !Exact && (*Stream)[0]=='/' && (*Stream)[1]=='/' )
			Ignore = 1;

		// Command chaining.
		if( !IsQuoted && !Exact && **Stream=='|' )
			break;

		// Check quoting.
		IsQuoted = IsQuoted ^ (**Stream==34);
		GotStream=1;

		// Got stuff.
		if( !Ignore )
		{
			Result.AppendChar( *((*Stream)++) );
		}
		else
		{
			(*Stream)++;
		}
	}
	if( Exact )
	{
		// Eat up exactly one CR/LF.
		if( **Stream == 13 )
			(*Stream)++;
		if( **Stream == 10 )
			(*Stream)++;
	}
	else
	{
		// Eat up all CR/LF's.
		while( **Stream==10 || **Stream==13 || **Stream=='|' )
			(*Stream)++;
	}

	return **Stream!=0 || GotStream;
}

bool FParse::LineExtended(const TCHAR** Stream, FString& Result, int32& LinesConsumed, bool Exact)
{
	bool GotStream=0;
	bool IsQuoted=0;
	bool Ignore=0;
	int32 BracketDepth = 0;

	Result = TEXT("");
	LinesConsumed = 0;

	while (**Stream != 0 && ((**Stream != 10 && **Stream != 13) || BracketDepth > 0))
	{
		// Start of comments.
		if( !IsQuoted && !Exact && (*Stream)[0]=='/' && (*Stream)[1]=='/' )
			Ignore = 1;

		// Command chaining.
		if( !IsQuoted && !Exact && **Stream=='|' )
			break;

		GotStream = 1;

		// bracketed line break
		if (**Stream == 10 || **Stream == 13)
		{
			checkSlow(BracketDepth > 0);

			Result.AppendChar(TEXT(' '));
			LinesConsumed++;
			(*Stream)++;
			if (**Stream == 10 || **Stream == 13)
			{
				(*Stream)++;
			}
		}
		// allow line break if the end of the line is a backslash
		else if (!IsQuoted && (*Stream)[0] == '\\' && ((*Stream)[1] == 10 || (*Stream)[1] == 13))
		{
			Result.AppendChar(TEXT(' '));
			LinesConsumed++;
			(*Stream) += 2;
			if (**Stream == 10 || **Stream == 13)
			{
				(*Stream)++;
			}
		}
		// check for starting or ending brace
		else if (!IsQuoted && **Stream == '{')
		{
			BracketDepth++;
			(*Stream)++;
		}
		else if (!IsQuoted && **Stream == '}' && BracketDepth > 0)
		{
			BracketDepth--;
			(*Stream)++;
		}
		else
		{
			// Check quoting.
			IsQuoted = IsQuoted ^ (**Stream==34);

			// Got stuff.
			if( !Ignore )
			{
				Result.AppendChar( *((*Stream)++) );
			}
			else
			{
				(*Stream)++;
			}
		}
	}
	if (**Stream == 0)
	{
		if (GotStream)
		{
			LinesConsumed++;
		}
	}
	else if (Exact)
	{
		// Eat up exactly one CR/LF.
		if (**Stream == 13 || **Stream == 10)
		{
			LinesConsumed++;
			if (**Stream == 13)
			{
				(*Stream)++;
			}
			if( **Stream == 10 )
			{
				(*Stream)++;
			}
		}
	}
	else
	{
		// Eat up all CR/LF's.
		while (**Stream == 10 || **Stream == 13 || **Stream == '|')
		{
			if (**Stream != '|')
			{
				LinesConsumed++;
			}
			if (((*Stream)[0] == 10 && (*Stream)[1] == 13) || ((*Stream)[0] == 13 && (*Stream)[1] == 10))
			{
				(*Stream)++;
			}
			(*Stream)++;
		}
	}

	return **Stream!=0 || GotStream;
}

uint32 FParse::HexNumber (const TCHAR* HexString)
{
	uint32 Ret = 0;

	while (*HexString)
	{
		Ret *= 16;
		Ret += FParse::HexDigit(*HexString++);
	}

	return Ret;
}

bool FParse::Resolution(const TCHAR* InResolution, uint32& OutX, uint32& OutY, int32& OutWindowMode)
{
	if(*InResolution)
	{
		FString CmdString(InResolution);
		CmdString = CmdString.Trim().TrimTrailing().ToLower();

		//Retrieve the X dimensional value
		const uint32 X = FMath::Max(FCString::Atof(*CmdString), 0.0f);

		// Determine whether the user has entered a resolution and extract the Y dimension.
		FString YString;

		// Find separator between values (Example of expected format: 1280x768)
		const TCHAR* YValue = NULL;
		if(FCString::Strchr(*CmdString,'x'))
		{
			YValue = const_cast<TCHAR*> (FCString::Strchr(*CmdString,'x')+1);
			YString = YValue;
			// Remove any whitespace from the end of the string
			YString = YString.Trim().TrimTrailing();
		}

		// If the Y dimensional value exists then setup to use the specified resolution.
		uint32 Y = 0;
		if ( YValue && YString.Len() > 0 )
		{
			// See if there is a fullscreen flag on the end
			FString FullScreenChar = YString.Mid(YString.Len() - 1);
			FString WindowFullScreenChars = YString.Mid(YString.Len() - 2);
			int32 WindowMode = OutWindowMode;
			if (!FullScreenChar.IsNumeric())
			{
				int StringTripLen = 0;

				if (WindowFullScreenChars == TEXT("wf"))
				{
					WindowMode = EWindowMode::WindowedFullscreen;
					StringTripLen = 2;
				}
				else if (FullScreenChar == TEXT("f"))
				{
					WindowMode = EWindowMode::Fullscreen;
					StringTripLen = 1;
				}
				else if (FullScreenChar == TEXT("w"))
				{
					WindowMode = EWindowMode::Windowed;
					StringTripLen = 1;
				}

				YString = YString.Left(YString.Len() - StringTripLen).Trim().TrimTrailing();
			}

			if (YString.IsNumeric())
			{
				Y = FMath::Max(FCString::Atof(YValue), 0.0f);
				OutX = X;
				OutY = Y;
				OutWindowMode = WindowMode;
				return true;
			}
		}
	}

	return false;
}

bool FParse::Resolution( const TCHAR* InResolution, uint32& OutX, uint32& OutY )
{
	int32 WindowModeDummy;
	return Resolution(InResolution, OutX, OutY, WindowModeDummy);
}