// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MessageEndpoint.h: Declares the FMessageEndpoint class.
=============================================================================*/

#pragma once


/**
 * Type definition for shared pointers to instances of FMessageEndpoint.
 */
typedef TSharedPtr<class FMessageEndpoint, ESPMode::ThreadSafe> FMessageEndpointPtr;

/**
 * Type definition for shared references to instances of FMessageEndpoint.
 */
typedef TSharedRef<class FMessageEndpoint, ESPMode::ThreadSafe> FMessageEndpointRef;


/**
 * DEPRECATED: Delegate type for error notifications.
 *
 * The first parameter is the  context of the sent message.
 * The second parameter is the error string.
 */
DECLARE_DELEGATE_TwoParams(FOnMessageEndpointError, const IMessageContextRef&, const FString&);

/**
 * Delegate type for received messages.
 *
 * The first parameter is the received message.
 * The return value indicates whether the message should be handled.
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnMessageEndpointReceiveMessage, const IMessageContextRef&);


/**
 * Implements a message endpoint for sending and receiving messages on a message bus.
 *
 * This class provides a convenient implementation of the IRecieveMessages and ISendMessages interfaces,
 * which allow consumers to send and receive messages on a message bus. The endpoint allows for receiving
 * messages synchronously as they arrive, as well as asynchronously through an inbox that can be polled.
 *
 * By default, messages are received synchronously on the thread that the endpoint was created on.
 * If the message consumer is thread-safe, a more efficient message dispatch can be enabled by calling
 * the SetRecipientThread(ENamedThreads::AnyThread) method.
 *
 * Endpoints that are destroyed or receive messages on non-Game threads should use the static function
 * FMessageEndpoint::SafeRelease() to dispose of the endpoint. This will ensure that there are no race
 * conditions between endpoint destruction and the receiving of messages.
 *
 * @todo gmp: Messaging reloadability; figure out how to auto-reconnect endpoints
 */
class FMessageEndpoint
	: public TSharedFromThis<FMessageEndpoint, ESPMode::ThreadSafe>
	, public IReceiveMessages
	, public ISendMessages
{
public:

	/**
	 * Type definition for the endpoint builder.
	 *
	 * When building message endpoints that receive messages on AnyThread, use the SafeRelease
	 * helper function to avoid race conditions when destroying the objects that own the endpoints.
	 *
	 * @see SafeRelease
	 */
	typedef struct FMessageEndpointBuilder Builder;

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InName The endpoint's name (for debugging purposes).
	 * @param InBus The message bus to attach this endpoint to.
	 * @param InHandlers The collection of message handlers to register.
	 */
	FMessageEndpoint( const FName& InName, const IMessageBusRef& InBus, const TArray<IMessageHandlerPtr>& InHandlers )
		: Address(FGuid::NewGuid())
		, BusPtr(InBus)
		, Enabled(true)
		, Handlers(InHandlers)
		, Id(FGuid::NewGuid())
		, InboxEnabled(false)
		, Name(InName)
	{ }

	/**
	 * Destructor.
	 */
	~FMessageEndpoint( )
	{
		IMessageBusPtr Bus = BusPtr.Pin();

		if (Bus.IsValid())
		{
			Bus->Unregister(Address);
		}
	}

public:

	/**
	 * Disables this endpoint.
	 *
	 * A disabled endpoint will not receive any subscribed messages until it is enabled again.
	 * Endpoints should be created in an enabled state by default.
	 *
	 * @see Enable
	 */
	void Disable( )
	{
		Enabled = false;
	}

	/**
	 * Enables this endpoint.
	 *
	 * An activated endpoint will receive subscribed messages.
	 * Endpoints should be created in an enabled state by default.
	 *
	 * @see Disable
	 */
	void Enable( )
	{
		Enabled = true;
	}

	/**
	 * Gets the endpoint's message address.
	 *
	 * @return Message address.
	 */
	const FMessageAddress& GetAddress( ) const
	{
		return Address;
	}

	/**
	 * Checks whether this endpoint is connected to the bus.
	 */
	bool IsConnected( ) const
	{
		return BusPtr.IsValid();
	}

	/**
	 * Checks whether this endpoint is enabled.
	 *
	 * @return true if the endpoint is enabled, false otherwise.
	 */
	bool IsEnabled( ) const
	{
		return Enabled;
	}

	/**
	 * Sets the name of the thread to receive messages on.
	 *
	 * Use this method to receive messages on a particular thread, for example, if the
	 * consumer owning this endpoint is not thread-safe. The default value is ThreadAny.
	 *
	 * ThreadAny is the fastest way to receive messages. It should be used if the receiving
	 * code is completely thread-safe and if it is sufficiently fast. ThreadAny MUST NOT
	 * be used if the receiving code is not thread-safe. It also SHOULD NOT be used if the
	 * code includes time consuming operations, because it will block the message router,
	 * causing no other messages to be delivered in the meantime.
	 *
	 * @param NamedThread The name of the thread to receive messages on.
	 */
	void SetRecipientThread( ENamedThreads::Type NamedThread )
	{
		RecipientThread = NamedThread;
	}

public:

	/**
	 * Defers processing of the given message by the specified time delay.
	 *
	 * The message is effectively delivered again to this endpoint after the
	 * original sent time plus the time delay have elapsed.
	 *
	 * @param Message The context of the message to defer.
	 * @param Delay The time delay.
	 */
	void Defer( const IMessageContextRef& Context, const FTimespan& Delay )
	{
		IMessageBusPtr Bus = GetBusIfEnabled();

		if (Bus.IsValid())
		{
			Bus->Forward(Context, TArrayBuilder<FMessageAddress>().Add(Address), Context->GetScope(), Delay, AsShared());
		}
	}

	/**
	 * Forwards a previously received message.
	 *
	 * Messages can only be forwarded to endpoints within the same process.
	 *
	 * @param Context The context of the message to forward.
	 * @param Recipients The list of message recipients to forward the message to.
	 * @param ForwardingScope The scope of the forwarded message.
	 * @param Delay The time delay.
	 */
	void Forward( const IMessageContextRef& Context, const TArray<FMessageAddress>& Recipients, EMessageScope::Type ForwardingScope, const FTimespan& Delay )
	{
		IMessageBusPtr Bus = GetBusIfEnabled();

		if (Bus.IsValid())
		{
			Bus->Forward(Context, Recipients, ForwardingScope, Delay, AsShared());
		}
	}

	/**
	 * Publishes a message to all subscribed recipients within the specified scope.
	 *
	 * @param Message The message to publish.
	 * @param TypeInfo The message's type information.
	 * @param Scope The message scope.
	 * @param Fields The message content.
	 * @param Delay The delay after which to publish the message.
	 * @param Expiration The time at which the message expires.
	 */
	void Publish( void* Message, UScriptStruct* TypeInfo, EMessageScope::Type Scope, const FTimespan& Delay, const FDateTime& Expiration )
	{
		IMessageBusPtr Bus = GetBusIfEnabled();

		if (Bus.IsValid())
		{
			Bus->Publish(Message, TypeInfo, Scope, Delay, Expiration, AsShared());
		}
	}

	/**
	 * Sends a message to the specified list of recipients.
	 *
	 * @param Message The message to send.
	 * @param TypeInfo The message's type information.
	 * @param Attachment An optional binary data attachment.
	 * @param Recipients The message recipients.
	 * @param Delay The delay after which to send the message.
	 * @param Expiration The time at which the message expires.
	 */
	void Send( void* Message, UScriptStruct* TypeInfo, const IMessageAttachmentPtr& Attachment, const TArray<FMessageAddress>& Recipients, const FTimespan& Delay, const FDateTime& Expiration )
	{
		IMessageBusPtr Bus = GetBusIfEnabled();

		if (Bus.IsValid())
		{
			Bus->Send(Message, TypeInfo, Attachment, Recipients, Delay, Expiration, AsShared());
		}
	}

	/**
	 * Subscribes a message handler.
	 *
	 * @param MessageType The type name of the messages to subscribe to.
	 * @param ScopeRange The range of message scopes to include in the subscription.
	 */
	void Subscribe( const FName& MessageType, const FMessageScopeRange& ScopeRange )
	{
		IMessageBusPtr Bus = GetBusIfEnabled();

		if (Bus.IsValid())
		{
			Bus->Subscribe(AsShared(), MessageType, ScopeRange);
		}
	}

	/**
	 * Unsubscribes this endpoint from the specified message type.
	 *
	 * @param MessageType The type of message to unsubscribe (NAME_All = all types).
	 *
	 * @see Subscribe
	 */
	void Unsubscribe( const FName& TopicPattern )
	{
		IMessageBusPtr Bus = GetBusIfEnabled();

		if (Bus.IsValid())
		{
			Bus->Unsubscribe(AsShared(), TopicPattern);
		}
	}

public:

	/**
	 * Disables the inbox for unhandled messages.
	 *
	 * The inbox is disabled by default.
	 *
	 * @see EnableInbox
	 * @see IsInboxEmpty
	 * @see IsInboxEnabled
	 * @see ProcessInbox
	 * @see ReceiveFromInbox
	 */
	void DisableInbox( )
	{
		InboxEnabled = false;
	}

	/**
	 * Enables the inbox for unhandled messages.
	 *
	 * If enabled, the inbox will queue up all received messages. Use ProcessInbox() to synchronously
	 * invoke the registered message handlers for all queued up messages, or ReceiveFromInbox() to
	 * manually receive one message from the inbox at a time. The inbox is disabled by default.
	 *
	 * @see DisableInbox
	 * @see IsInboxEmpty
	 * @see IsInboxEnabled
	 * @see ProcessInbox
	 * @see ReceiveFromInbox
	 */
	void EnableInbox( )
	{
		InboxEnabled = true;
	}

	/**
	 * Checks whether the inbox is empty.
	 *
	 * @return true if the inbox is empty, false otherwise.
	 *
	 * @see DisableInbox
	 * @see EnableInbox
	 * @see IsInboxEnabled
	 * @see ProcessInbox
	 * @see ReceiveFromInbox
	 */
	bool IsInboxEmpty( ) const
	{
		return Inbox.IsEmpty();
	}

	/**
	 * Checks whether the inbox is enabled.
	 *
	 * @see DisableInbox
	 * @see EnableInbox
	 * @see IsInboxEmpty
	 * @see ProcessInbox
	 * @see ReceiveFromInbox
	 */
	bool IsInboxEnabled( ) const
	{
		return InboxEnabled;
	}

	/**
	 * Calls the matching message handlers for all messages queued up in the inbox.
	 *
	 * Note that an incoming message will only be queued up in the endpoint's inbox if the inbox has
	 * been enabled and no matching message handler handled it. The inbox is disabled by default and
	 * must be enabled using the EnableInbox() method.
	 *
	 * @see IsInboxEmpty
	 * @see ReceiveFromInbox
	 */
	void ProcessInbox( )
	{
		IMessageContextPtr Context;

		while (Inbox.Dequeue(Context))
		{
			ProcessMessage(Context.ToSharedRef());
		}
	}

	/**
	 * Receives a single message from the endpoint's inbox.
	 *
	 * Note that an incoming message will only be queued up in the endpoint's inbox if the inbox has
	 * been enabled and no matching message handler handled it. The inbox is disabled by default and
	 * must be enabled using the EnableInbox() method.
	 *
	 * @param OutContext Will hold the context of the received message.
	 *
	 * @return true if a message was received, false if the inbox was empty.
	 *
	 * @see DisableInbox
	 * @see EnableInbox
	 * @see IsInboxEnabled
	 * @see ProcessInbox
	 */
	bool ReceiveFromInbox( IMessageContextPtr& OutContext )
	{
		return Inbox.Dequeue(OutContext);
	}

public:

	/**
	 * Returns a delegate to be invoked when the endpoint receives a message.
	 *
	 * This callback delegate is called every time a message is received by this
	 * endpoint and before it is being handled by any registered message handlers.
	 *
	 * @return The delegate.
	 */
	FOnMessageEndpointReceiveMessage& OnReceiveMessage( )
	{
		return ReceiveDelegate;
	}

public:

	// Begin IReceiveMessages interface

	virtual FName GetDebugName( ) const OVERRIDE
	{
		return Name;
	}

	virtual const FGuid& GetRecipientId( ) const OVERRIDE
	{
		return Id;
	}

	virtual ENamedThreads::Type GetRecipientThread( ) const OVERRIDE
	{
		return RecipientThread;
	}

	virtual bool IsLocal( ) const OVERRIDE
	{
		return true;
	}

	virtual void ReceiveMessage( const IMessageContextRef& Context ) OVERRIDE
	{
		if (!Enabled)
		{
			return;
		}

		if (ReceiveDelegate.IsBound() && !ReceiveDelegate.Execute(Context))
		{
			return;
		}

		if (InboxEnabled)
		{
			Inbox.Enqueue(Context);
		}
		else
		{
			ProcessMessage(Context);
		}
	}

	// End IReceiveMessages interface

public:

	// Begin ISendMessages interface

	virtual FMessageAddress GetSenderAddress( ) OVERRIDE
	{
		return Address;
	}

	virtual void NotifyMessageError( const IMessageContextRef& Context, const FString& Error ) OVERRIDE
	{
		ErrorDelegate.ExecuteIfBound(Context, Error);
	}

	// End ISendMessages interface

public:

	/**
	 * Immediately forwards a previously received message to the specified recipient.
	 *
	 * Messages can only be forwarded to endpoints within the same process.
	 *
	 * @param Context The context of the message to forward.
	 * @param Recipient The address of the recipient to forward the message to.
	 * @param ForwardingScope The scope of the forwarded message.
	 */
	void Forward( const IMessageContextRef& Context, const FMessageAddress& Recipient, EMessageScope::Type ForwardingScope )
	{
		Forward(Context, TArrayBuilder<FMessageAddress>().Add(Recipient), ForwardingScope, FTimespan::Zero());
	}

	/**
	 * Forwards a previously received message to the specified recipient after a given delay.
	 *
	 * Messages can only be forwarded to endpoints within the same process.
	 *
	 * @param Context The context of the message to forward.
	 * @param Recipient The address of the recipient to forward the message to.
	 * @param ForwardingScope The scope of the forwarded message.
	 * @param Delay The delay after which to publish the message.
	 */
	void Forward( const IMessageContextRef& Context, const FMessageAddress& Recipient, EMessageScope::Type ForwardingScope, const FTimespan& Delay )
	{
		Forward(Context, TArrayBuilder<FMessageAddress>().Add(Recipient), ForwardingScope, Delay);
	}

	/**
	 * Immediately forwards a previously received message to the specified list of recipients.
	 *
	 * Messages can only be forwarded to endpoints within the same process.
	 *
	 * @param Context The context of the message to forward.
	 * @param Recipients The list of message recipients to forward the message to.
	 * @param ForwardingScope The scope of the forwarded message.
	 */
	void Forward( const IMessageContextRef& Context, const TArray<FMessageAddress>& Recipients, EMessageScope::Type ForwardingScope )
	{
		Forward(Context, Recipients, ForwardingScope, FTimespan::Zero());
	}

	/**
	 * Immediately publishes a message to all subscribed recipients.
	 *
	 * @param Message The message to publish.
	 */
	template<typename MessageType>
	void Publish( MessageType* Message )
	{
		Publish(Message, MessageType::StaticStruct(), EMessageScope::Network, FTimespan::Zero(), FDateTime::MaxValue());
	}

	/**
	 * Immediately pa message to all subscribed recipients within the specified scope.
	 *
	 * @param Message The message to publish.
	 * @param Scope The message scope.
	 */
	template<typename MessageType>
	void Publish( MessageType* Message, EMessageScope::Type Scope )
	{
		Publish(Message, MessageType::StaticStruct(), Scope, FTimespan::Zero(), FDateTime::MaxValue());
	}

	/**
	 * Publishes a message to all subscribed recipients after a given delay.
	 *
	 * @param Message The message to publish.
	 * @param Delay The delay after which to publish the message.
	 */
	template<typename MessageType>
	void Publish( MessageType* Message, const FTimespan& Delay )
	{
		Publish(Message, MessageType::StaticStruct(), EMessageScope::Network, Delay, FDateTime::MaxValue());
	}

	/**
	 * Publishes a message to all subscribed recipients within the specified scope after a given delay.
	 *
	 * @param Message The message to publish.
	 * @param Scope The message scope.
	 * @param Delay The delay after which to publish the message.
	 */
	template<typename MessageType>
	void Publish( MessageType* Message, EMessageScope::Type Scope, const FTimespan& Delay )
	{
		Publish(Message, MessageType::StaticStruct(), Scope, Delay, FDateTime::MaxValue());
	}

	/**
	 * Publishes a message to all subscribed recipients within the specified scope.
	 *
	 * @param Message The message to publish.
	 * @param Scope The message scope.
	 * @param Fields The message content.
	 * @param Delay The delay after which to publish the message.
	 * @param Expiration The time at which the message expires.
	 */
	template<typename MessageType>
	void Publish( MessageType* Message, EMessageScope::Type Scope, const FTimespan& Delay, const FDateTime& Expiration )
	{
		Publish(Message, MessageType::StaticStruct(), Scope, Delay, Expiration);
	}

	/**
	 * Immediately sends a message to the specified recipient.
	 *
	 * @param MessageType The type of message to send.
	 * @param Message The message to send.
	 * @param Recipient The message recipient.
	 */
	template<typename MessageType>
	void Send( MessageType* Message, const FMessageAddress& Recipient )
	{
		Send(Message, MessageType::StaticStruct(), NULL, TArrayBuilder<FMessageAddress>().Add(Recipient), FTimespan::Zero(), FDateTime::MaxValue());
	}

	/**
	 * Sends a message to the specified recipient after a given delay.
	 *
	 * @param MessageType The type of message to send.
	 * @param Message The message to send.
	 * @param Recipient The message recipient.
	 * @param Delay The delay after which to send the message.
	 */
	template<typename MessageType>
	void Send( MessageType* Message, const FMessageAddress& Recipient, const FTimespan& Delay )
	{
		Send(Message, MessageType::StaticStruct(), NULL, TArrayBuilder<FMessageAddress>().Add(Recipient), Delay, FDateTime::MaxValue());
	}

	/**
	 * Sends a message with fields and expiration to the specified recipient after a given delay.
	 *
	 * @param MessageType The type of message to send.
	 * @param Message The message to send.
	 * @param Recipient The message recipient.
	 * @param Expiration The time at which the message expires.
	 * @param Delay The delay after which to send the message.
	 */
	template<typename MessageType>
	void Send( MessageType* Message, const FMessageAddress& Recipient, const FTimespan& Delay, const FDateTime& Expiration )
	{
		Send(Message, MessageType::StaticStruct(), NULL, TArrayBuilder<FMessageAddress>().Add(Recipient), Delay, Expiration);
	}

	/**
	 * Sends a message with fields and attachment to the specified recipient.
	 *
	 * @param MessageType The type of message to send.
	 * @param Message The message to send.
	 * @param Attachment An optional binary data attachment.
	 * @param Recipient The message recipient.
	 */
	template<typename MessageType>
	void Send( MessageType* Message, const IMessageAttachmentPtr& Attachment, const FMessageAddress& Recipient )
	{
		Send(Message, MessageType::StaticStruct(), Attachment, TArrayBuilder<FMessageAddress>().Add(Recipient), FTimespan::Zero(), FDateTime::MaxValue());
	}

	/**
	 * Sends a message with fields, attachment and expiration to the specified recipient after a given delay.
	 *
	 * @param MessageType The type of message to send.
	 * @param Message The message to send.
	 * @param Attachment An optional binary data attachment.
	 * @param Recipient The message recipient.
	 * @param Expiration The time at which the message expires.
	 * @param Delay The delay after which to send the message.
	 */
	template<typename MessageType>
	void Send( MessageType* Message, const IMessageAttachmentPtr& Attachment, const FMessageAddress& Recipient, const FDateTime& Expiration, const FTimespan& Delay )
	{
		Send(Message, MessageType::StaticStruct(), Attachment, TArrayBuilder<FMessageAddress>().Add(Recipient), Delay, Expiration);
	}

	/**
	 * Immediately sends a message to the specified list of recipients.
	 *
	 * @param MessageType The type of message to send.
	 * @param Message The message to send.
	 * @param Recipients The message recipients.
	 */
	template<typename MessageType>
	void Send( MessageType* Message, const TArray<FMessageAddress>& Recipients )
	{
		Send(Message, MessageType::StaticStruct(), NULL, Recipients, FTimespan::Zero(), FDateTime::MaxValue());
	}

	/**
	 * Sends a message to the specified list of recipients after a given delay after a given delay.
	 *
	 * @param MessageType The type of message to send.
	 * @param Message The message to send.
	 * @param Recipients The message recipients.
	 * @param Delay The delay after which to send the message.
	 */
	template<typename MessageType>
	void Send( MessageType* Message, const TArray<FMessageAddress>& Recipients, const FTimespan& Delay )
	{
		Send(Message, MessageType::StaticStruct(), NULL, Recipients, Delay, FDateTime::MaxValue());
	}

	/**
	 * Sends a message with fields and attachment to the specified list of recipients after a given delay.
	 *
	 * @param MessageType The type of message to send.
	 * @param Message The message to send.
	 * @param Attachment An optional binary data attachment.
	 * @param Recipients The message recipients.
	 * @param Delay The delay after which to send the message.
	 */
	template<typename MessageType>
	void Send( MessageType* Message, const IMessageAttachmentPtr& Attachment, const TArray<FMessageAddress>& Recipients, const FTimespan& Delay )
	{
		Send(Message, MessageType::StaticStruct(), Attachment, Recipients, Delay, FDateTime::MaxValue());
	}

	/**
	 * Sends a message to the specified list of recipients.
	 *
	 * @param MessageType The type of message to send.
	 * @param Message The message to send.
	 * @param Attachment An optional binary data attachment.
	 * @param Recipients The message recipients.
	 * @param Delay The delay after which to send the message.
	 * @param Expiration The time at which the message expires.
	 */
	template<typename MessageType>
	void Send( MessageType* Message, const IMessageAttachmentPtr& Attachment, const TArray<FMessageAddress>& Recipients, const FTimespan& Delay, const FDateTime& Expiration )
	{
		Send(Message, MessageType::StaticStruct(), Attachment, Recipients, Delay, Expiration);
	}

	/**
	 * Template method to subscribe the message endpoint to the specified type of messages with the default message scope.
	 *
	 * The default message scope is all messages excluding loopback messages.
	 *
	 * @param HandlerType The type of the class handling the message.
	 * @param MessageType The type of messages to subscribe to.
	 * @param Handler The class handling the messages.
	 * @param HandlerFunc The class function handling the messages.
	 */
	template<class MessageType>
	void Subscribe( )
	{
		Subscribe(MessageType::StaticStruct()->GetFName(), FMessageScopeRange::AtLeast(EMessageScope::Thread));
	}

	/**
	 * Template method to subscribe the message endpoint to the specified type and scope of messages.
	 *
	 * @param HandlerType The type of the class handling the message.
	 * @param MessageType The type of messages to subscribe to.
	 * @param Handler The class handling the messages.
	 * @param HandlerFunc The class function handling the messages.
	 * @param ScopeRange The range of message scopes to include in the subscription.
	 */
	template<class MessageType>
	void Subscribe( const FMessageScopeRange& ScopeRange )
	{
		Subscribe(MessageType::StaticStruct()->GetFName(), ScopeRange);
	}

	/**
	 * Unsubscribes this endpoint from all message types.
	 *
	 * @see Subscribe
	 */
	void Unsubscribe( )
	{
		Unsubscribe(NAME_All);
	}

	/**
	 * Template method to unsubscribe the endpoint from the specified message type.
	 *
	 * @param MessageType The type of message to unsubscribe (NAME_All = all types).
	 *
	 * @see Subscribe
	 */
	template<class MessageType>
	void Unsubscribe( )
	{
		Unsubscribe(MessageType::StaticStruct()->GetFName());
	}

public:

	/**
	 * Safely releases a message endpoint that is receiving messages on AnyThread.
	 *
	 * When an object that owns a message endpoint receiving on AnyThread is being destroyed,
	 * it is possible that the endpoint can outlive the object for a brief period of time if
	 * the Messaging system is dispatching messages to it. The purpose of this helper function
	 * is to block the calling thread while any messages are being dispatched, so that the
	 * endpoint does not invoke any message handlers after the object has been destroyed.
	 *
	 * Note: When calling this function make sure that no other object is holding on to
	 * the endpoint, or otherwise the caller may get blocked forever.
	 *
	 * @param Endpoint The message endpoint to release.
	 */
	static void SafeRelease( FMessageEndpointPtr& Endpoint )
	{
		TWeakPtr<FMessageEndpoint, ESPMode::ThreadSafe> EndpointPtr = Endpoint;
		Endpoint.Reset();
		while (EndpointPtr.IsValid());
	}

protected:

	/**
	 * Gets a shared pointer to the message bus if this endpoint is enabled.
	 *
	 * @return The message bus.
	 */
	FORCEINLINE IMessageBusPtr GetBusIfEnabled( ) const
	{
		if (Enabled)
		{
			return BusPtr.Pin();
		}

		return nullptr;
	}

	/**
	 * Forwards the given message context to matching message handlers.
	 *
	 * @param Context The context of the message to handle.
	 */
	void ProcessMessage( const IMessageContextRef& Context )
	{
		if (!Context->IsValid())
		{
			return;
		}

		for (int32 HandlerIndex = 0; HandlerIndex < Handlers.Num(); ++HandlerIndex)
		{
			IMessageHandlerPtr& Handler = Handlers[HandlerIndex];

			if (Handler->GetHandledMessageType() == Context->GetMessageType())
			{
				Handler->HandleMessage(Context);
			}
		}
	}

private:

	// Holds the endpoint's identifier.
	const FMessageAddress Address;

	// Holds a weak pointer to the message bus.
	IMessageBusWeakPtr BusPtr;

	// Hold a flag indicating whether this endpoint is active.
	bool Enabled;

	// Holds the registered message handlers.
	TArray<IMessageHandlerPtr> Handlers;

	// Holds the endpoint's unique identifier (for debugging purposes).
	const FGuid Id;

	// Holds the endpoint's message inbox.
	TQueue<IMessageContextPtr, EQueueMode::Mpsc> Inbox;

	// Holds a flag indicating whether the inbox is enabled.
	bool InboxEnabled;

	// Holds the endpoint's name (for debugging purposes).
	const FName Name;

	// Holds a delegate that is invoked when a message has been received.
	FOnMessageEndpointReceiveMessage ReceiveDelegate;

	// Holds the name of the thread on which to receive messages.
	ENamedThreads::Type RecipientThread;

private:

	// Holds a delegate that is invoked in case of messaging errors.
	FOnMessageEndpointError ErrorDelegate;
};
