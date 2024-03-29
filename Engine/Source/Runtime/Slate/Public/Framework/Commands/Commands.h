// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

//
// COMMAND DECLARATION
//

#define LOC_DEFINE_REGION

SLATE_API void UI_COMMAND_Function( FBindingContext* This, TSharedPtr< FUICommandInfo >& OutCommand, const TCHAR* OutCommandName, const TCHAR* OutCommandNameUnderscoreTooltip, const ANSICHAR* DotOutCommandName, const TCHAR* FriendlyName, const TCHAR* InDescription, const EUserInterfaceActionType::Type CommandType, const FInputGesture& InDefaultGesture );

#define UI_COMMAND( CommandId, FriendlyName, InDescription, CommandType, InDefaultGesture ) \
	UI_COMMAND_Function( this, CommandId, TEXT(#CommandId), TEXT(#CommandId) TEXT("_ToolTip"), "." #CommandId, TEXT(FriendlyName), TEXT(InDescription), CommandType, InDefaultGesture );

#undef LOC_DEFINE_REGION

/** A base class for a set of commands. Inherit from it to make a set of commands. See MainFrameActions for an example. */
template<typename CommandContextType>
class TCommands : public FBindingContext
{
public:
	

	/** Use this method to register commands. Usually done in StartupModule(). */
	static void Register()
	{
		if ( !Instance.IsValid() )
		{
			// We store the singleton instances in the FInputBindingManager in order to prevent
			// different modules from instantiating their own version of TCommands<CommandContextType>.
			TSharedRef<CommandContextType> NewInstance = MakeShareable( new CommandContextType() );

			TSharedPtr<FBindingContext> ExistingBindingContext = FInputBindingManager::Get().GetContextByName( NewInstance->GetContextName() );
			if (ExistingBindingContext.IsValid())
			{
				// Someone already made this set of commands, and registered it.
				Instance = StaticCastSharedPtr<CommandContextType>(ExistingBindingContext);
			}
			else
			{
				// Make a new set of commands and register it.
				Instance = NewInstance;

				// Registering the first command will add the NewInstance into the Binding Manager, who holds on to it.
				NewInstance->RegisterCommands();

				// Notify that new commands have been registered
				CommandsChanged.Broadcast();
			}
		}
	}

	/** Get the singleton instance of this set of commands. */
	static const CommandContextType& Get()
	{
		return *(Instance.Pin());
	}

	/** Use this method to clean up any resources used by the command set. Usually done in ShutdownModule() */
	static void Unregister()
	{
		// The instance may not be valid if it was never used.
		if( Instance.IsValid() )
		{
			FInputBindingManager::Get().RemoveContextByName(Instance.Pin()->GetContextName());
			
			// Notify that new commands have been unregistered
			CommandsChanged.Broadcast();

			check(!Instance.IsValid());
		}
	}

	/** Get the BindingContext for this set of commands. */
	static const FBindingContext& GetContext()
	{
		check( Instance.IsValid() );
		return *(Instance.Pin());
	}
	
protected:
	
	/** Construct a set of commands; call this from your custom commands class. */
	TCommands( const FName InContextName, const FText& InContextDesc, const FName InContextParent, const FName InStyleSetName )
		: FBindingContext( InContextName, InContextDesc, InContextParent, InStyleSetName )
	{
	}
	virtual ~TCommands()
	{
	}

	/** A static instance of the command set. */
	static TWeakPtr< CommandContextType > Instance;

	/** Pure virtual to override; describe and instantiate the commands in here by using the UI COMMAND macro. */
	virtual void RegisterCommands() = 0;

};

template<typename CommandContextType>
TWeakPtr< CommandContextType > TCommands<CommandContextType>::Instance = NULL;
