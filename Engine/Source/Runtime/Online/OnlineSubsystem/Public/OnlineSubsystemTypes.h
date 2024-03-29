// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OnlineSubsystemPackage.h"

/** Name given to default OSS instances (disambiguates for PIE) */
#define DEFAULT_INSTANCE FName(TEXT("DefaultInstance"))

/** Maximum players supported on a given platform */
#if PLATFORM_XBOXONE
#define MAX_LOCAL_PLAYERS 4
#elif PLATFORM_PS4
#define MAX_LOCAL_PLAYERS 4
#else
#define MAX_LOCAL_PLAYERS 1
#endif

#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS 0
#endif

#ifndef E_FAIL
#define E_FAIL (uint32)-1
#endif

#ifndef E_NOTIMPL
#define E_NOTIMPL (uint32)-2
#endif

#ifndef ERROR_IO_PENDING
#define ERROR_IO_PENDING 997
#endif

#ifndef S_OK
#define S_OK 0
#endif

/**
 * Generates a random nonce (number used once) of the desired length
 *
 * @param Nonce the buffer that will get the randomized data
 * @param Length the number of bytes to generate random values for
 */
inline void GenerateNonce(uint8* Nonce, uint32 Length)
{
//@todo joeg -- switch to CryptGenRandom() if possible or something equivalent
	// Loop through generating a random value for each byte
	for (uint32 NonceIndex = 0; NonceIndex < Length; NonceIndex++)
	{
		Nonce[NonceIndex] = (uint8)(FMath::Rand() & 255);
	}
}

/** Possible login states */
namespace ELoginStatus
{
	enum Type
	{
		/** Player has not logged in or chosen a local profile */
		NotLoggedIn,
		/** Player is using a local profile but is not logged in */
		UsingLocalProfile,
		/** Player has been validated by the platform specific authentication service */
		LoggedIn
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(ELoginStatus::Type EnumVal)
	{
		switch (EnumVal)
		{
			case NotLoggedIn:
			{
				return TEXT("NotLoggedIn");
			}
			case UsingLocalProfile:
			{
				return TEXT("UsingLocalProfile");
			}
			case LoggedIn:
			{
				return TEXT("LoggedIn");
			}
		}
		return TEXT("");
	}
};

/** Possible connection states */
namespace EOnlineServerConnectionStatus
{
	enum Type
	{
		/** Gracefully disconnected from the online servers */
		NotConnected,
		/** Connected to the online servers just fine */
		Connected,
		/** Connection was lost for some reason */
		ConnectionDropped,
		/** Can't connect because of missing network connection */
		NoNetworkConnection,
		/** Service is temporarily unavailable */
		ServiceUnavailable,
		/** An update is required before connecting is possible */
		UpdateRequired,
		/** Servers are too busy to handle the request right now */
		ServersTooBusy,
		/** Disconnected due to duplicate login */
		DuplicateLoginDetected,
		/** Can't connect because of an invalid/unknown user */
		InvalidUser
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineServerConnectionStatus::Type EnumVal)
	{
		switch (EnumVal)
		{
			case NotConnected:
			{
				return TEXT("NotConnected");
			}
			case Connected:
			{
				return TEXT("Connected");
			}
			case ConnectionDropped:
			{
				return TEXT("ConnectionDropped");
			}
			case NoNetworkConnection:
			{
				return TEXT("NoNetworkConnection");
			}
			case ServiceUnavailable:
			{
				return TEXT("ServiceUnavailable");
			}
			case UpdateRequired:
			{
				return TEXT("UpdateRequired");
			}
			case ServersTooBusy:
			{
				return TEXT("ServersTooBusy");
			}
			case DuplicateLoginDetected:
			{
				return TEXT("DuplicateLoginDetected");
			}
			case InvalidUser:
			{
				return TEXT("InvalidUser");
			}
		}
		return TEXT("");
	}
};

/** Possible feature privilege access levels */
namespace EFeaturePrivilegeLevel
{
	enum Type
	{
		/** Not defined for the platform service */
		Undefined,
		/** Parental controls have disabled this feature */
		Disabled,
		/** Parental controls allow this feature only with people on their friends list */
		EnabledFriendsOnly,
		/** Parental controls allow this feature everywhere */
		Enabled
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EFeaturePrivilegeLevel::Type EnumVal)
	{
		switch (EnumVal)
		{
			case Undefined:
			{
				return TEXT("Undefined");
			}
			case Disabled:
			{
				return TEXT("Disabled");
			}
			case EnabledFriendsOnly:
			{
				return TEXT("EnabledFriendsOnly");
			}
			case Enabled:
			{
				return TEXT("Enabled");
			}
		}
		return TEXT("");
	}
};

/** The state of an async task (read friends, read content, write cloud file, etc) request */
namespace EOnlineAsyncTaskState
{
	enum Type
	{
		/** The task has not been started */
		NotStarted,
		/** The task is currently being processed */
		InProgress,
		/** The task has completed successfully */
		Done,
		/** The task failed to complete */
		Failed
	};  

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineAsyncTaskState::Type EnumVal)
	{
		switch (EnumVal)
		{
			case NotStarted:
			{
				return TEXT("NotStarted");
			}
			case InProgress:
			{
				return TEXT("InProgress");
			}
			case Done:
			{
				return TEXT("Done");
			}
			case Failed:
			{
				return TEXT("Failed");
			}
		}
		return TEXT("");
	}
};

/** The possible friend states for a friend entry */
namespace EOnlineFriendState
{
	enum Type
	{
		/** Not currently online */
		Offline,
		/** Signed in and online */
		Online,
		/** Signed in, online, and idle */
		Away,
		/** Signed in, online, and asks to be left alone */
		Busy
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineFriendState::Type EnumVal)
	{
		switch (EnumVal)
		{
			case Offline:
			{
				return TEXT("Offline");
			}
			case Online:
			{
				return TEXT("Online");
			}
			case Away:
			{
				return TEXT("Away");
			}
			case Busy:
			{
				return TEXT("Busy");
			}
		}
		return TEXT("");
	}
};

/** Leaderboard entry sort types */
namespace ELeaderboardSort
{
	enum Type
	{
		/** Don't sort at all */
		None,
		/** Sort ascending */
		Ascending,
		/** Sort descending */
		Descending
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(ELeaderboardSort::Type EnumVal)
	{
		switch (EnumVal)
		{
		case None:
			{
				return TEXT("None");
			}
		case Ascending:
			{
				return TEXT("Ascending");
			}
		case Descending:
			{
				return TEXT("Descending");
			}
		}
		return TEXT("");
	}
};

/** Leaderboard display format */
namespace ELeaderboardFormat
{
	enum Type
	{
		/** A raw number */
		Number,
		/** Time, in seconds */
		Seconds,
		/** Time, in milliseconds */
		Milliseconds
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(ELeaderboardFormat::Type EnumVal)
	{
		switch (EnumVal)
		{
		case Number:
			{
				return TEXT("Number");
			}
		case Seconds:
			{
				return TEXT("Seconds");
			}
		case Milliseconds:
			{
				return TEXT("Milliseconds");
			}
		}
		return TEXT("");
	}
};

/** How to upload leaderboard score updates */
namespace ELeaderboardUpdateMethod
{
	enum Type
	{
		/** If current leaderboard score is better than the uploaded one, keep the current one */
		KeepBest,
		/** Leaderboard score is always replaced with uploaded value */
		Force
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(ELeaderboardUpdateMethod::Type EnumVal)
	{
		switch (EnumVal)
		{
		case KeepBest:
			{
				return TEXT("KeepBest");
			}
		case Force:
			{
				return TEXT("Force");
			}
		}
		return TEXT("");
	}
};

/** Enum indicating the state the LAN beacon is in */
namespace ELanBeaconState
{
	enum Type
	{
		/** The lan beacon is disabled */
		NotUsingLanBeacon,
		/** The lan beacon is responding to client requests for information */
		Hosting,
		/** The lan beacon is querying servers for information */
		Searching
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(ELanBeaconState::Type EnumVal)
	{
		switch (EnumVal)
		{

		case NotUsingLanBeacon:
			{
				return TEXT("NotUsingLanBeacon");
			}
		case Hosting:
			{
				return TEXT("Hosting");
			}
		case Searching:
			{
				return TEXT("Searching");
			}
		}
		return TEXT("");
	}
};

/** Enum indicating the current state of the online session (in progress, ended, etc.) */
namespace EOnlineSessionState
{
	enum Type
	{
		/** An online session has not been created yet */
		NoSession,
		/** An online session is in the process of being created */
		Creating,
		/** Session has been created but the session hasn't started (pre match lobby) */
		Pending,
		/** Session has been asked to start (may take time due to communication with backend) */
		Starting,
		/** The current session has started. Sessions with join in progress disabled are no longer joinable */
		InProgress,
		/** The session is still valid, but the session is no longer being played (post match lobby) */
		Ending,
		/** The session is closed and any stats committed */
		Ended,
		/** The session is being destroyed */
		Destroying
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineSessionState::Type EnumVal)
	{
		switch (EnumVal)
		{

		case NoSession:
			{
				return TEXT("NoSession");
			}
		case Creating:
			{
				return TEXT("Creating");
			}
		case Pending:
			{
				return TEXT("Pending");
			}
		case Starting:
			{
				return TEXT("Starting");
			}
		case InProgress:
			{
				return TEXT("InProgress");
			}
		case Ending:
			{
				return TEXT("Ending");
			}
		case Ended:
			{
				return TEXT("Ended");
			}
		case Destroying:
			{
				return TEXT("Destroying");
			}
		}
		return TEXT("");
	}
};

/** The types of advertisement of settings to use */
namespace EOnlineDataAdvertisementType
{
	enum Type
	{
		/** Don't advertise via the online service or QoS data */
		DontAdvertise,
		/** Advertise via the server ping data only */
		ViaPingOnly,
		/** Advertise via the online service only */
		ViaOnlineService,
		/** Advertise via the online service and via the ping data */
		ViaOnlineServiceAndPing
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineDataAdvertisementType::Type EnumVal)
	{
		switch (EnumVal)
		{
		case DontAdvertise:
			{
				return TEXT("DontAdvertise");
			}
		case ViaPingOnly:
			{
				return TEXT("ViaPingOnly");
			}
		case ViaOnlineService:
			{
				return TEXT("OnlineService");
			}
		case ViaOnlineServiceAndPing:
			{
				return TEXT("OnlineServiceAndPing");
			}
		}
		return TEXT("");
	}
}

/** The types of comparison operations for a given search query */
namespace EOnlineComparisonOp
{
	enum Type
	{
		Equals,
		NotEquals,
		GreaterThan,
		GreaterThanEquals,
		LessThan,
		LessThanEquals,
		Near
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineComparisonOp::Type EnumVal)
	{
		switch (EnumVal)
		{
		case Equals:
			{
				return TEXT("Equals");
			}
		case NotEquals:
			{
				return TEXT("NotEquals");
			}
		case GreaterThan:
			{
				return TEXT("GreaterThan");
			}
		case GreaterThanEquals:
			{
				return TEXT("GreaterThanEquals");
			}
		case LessThan:
			{
				return TEXT("LessThan");
			}
		case LessThanEquals:
			{
				return TEXT("LessThanEquals");
			}
		case Near:
			{
				return TEXT("Near");
			}
		}
		return TEXT("");
	}
}

/** Return codes for the GetCached functions in the various subsystems. */
namespace EOnlineCachedResult
{
	enum Type
	{
		Success, /** The requested data was found and returned successfully. */
		NotFound /** The requested data was not found in the cache, and the out parameter was not modified. */
	};

	/**
	 * @param EnumVal the enum to convert to a string
	 * @return the stringified version of the enum passed in
	 */
	inline const TCHAR* ToString(EOnlineCachedResult::Type EnumVal)
	{
		switch (EnumVal)
		{
		case Success:
			{
				return TEXT("Success");
			}
		case NotFound:
			{
				return TEXT("NotFound");
			}
		}
		return TEXT("");
	}
}

/*
 *	Base class for anything meant to be opaque so that the data can be passed around 
 *  without consideration for the data it contains.
 *	A human readable version of the data is available via the ToString() function
 *	Otherwise, nothing but platform code should try to operate directly on the data
 */
class IOnlinePlatformData
{
protected:

	/** Hidden on purpose */
	IOnlinePlatformData()
	{
	}

	/** Hidden on purpose */
	IOnlinePlatformData(const IOnlinePlatformData& Src)
	{
	}

	/** Hidden on purpose */
	IOnlinePlatformData& operator=(const IOnlinePlatformData& Src)
	{
		return *this;
	}

	virtual bool Compare(const IOnlinePlatformData& Other) const
	{
		return (GetSize() == Other.GetSize()) &&
			(FMemory::Memcmp(GetBytes(), Other.GetBytes(), GetSize()) == 0);
	}

public:

	virtual ~IOnlinePlatformData() {}

	/**
	 *	Comparison operator
	 */
	bool operator==(const IOnlinePlatformData& Other) const
	{
		return Other.Compare(*this);
	}

	bool operator!=(const IOnlinePlatformData& Other) const
	{
		return !(IOnlinePlatformData::operator==(Other));
	}
	
	/** 
	 * Get the raw byte representation of this opaque data
	 * This data is platform dependent and shouldn't be manipulated directly
	 *
	 * @return byte array of size GetSize()
	 */
	virtual const uint8* GetBytes() const = 0;

	/** 
	 * Get the size of the opaque data
	 *
	 * @return size in bytes of the data representation
	 */
	virtual int32 GetSize() const = 0;

	/** 
	 * Check the validity of the opaque data
	 *
	 * @return true if this is well formed data, false otherwise
	 */
	virtual bool IsValid() const = 0;

	/** 
	 * Platform specific conversion to string representation of data
	 *
	 * @return data in string form 
	 */
	virtual FString ToString() const = 0;

	/** 
	 * Get a human readable representation of the opaque data
	 * Shouldn't be used for anything other than logging/debugging
	 *
	 * @return data in string form 
	 */
	virtual FString ToDebugString() const = 0;
};


/** 
 * Abstraction of a profile service online Id 
 * The class is meant to be opaque (see IOnlinePlatformData)
 */
class FUniqueNetId : public IOnlinePlatformData
{
protected:

	/** Hidden on purpose */
	FUniqueNetId()
	{
	}

	/** Hidden on purpose */
	FUniqueNetId(const FUniqueNetId& Src)
	{
	}

	/** Hidden on purpose */
	FUniqueNetId& operator=(const FUniqueNetId& Src)
	{
		return *this;
	}

public:

	/**
	 * @return hex encoded string representation of unique id
	 */
	FString GetHexEncodedString() const
	{
		if (GetSize() > 0 && GetBytes() != NULL)
		{
			return BytesToHex(GetBytes(),GetSize());
		}
		return FString();
	}	

	virtual ~FUniqueNetId() {}
};

/**
 * TArray helper for FindMatch() function
 */
struct FUniqueNetIdMatcher
{
private:
	/** Target for comparison in the TArray */
	const FUniqueNetId& UniqueIdTarget;

public:
	FUniqueNetIdMatcher(const FUniqueNetId& InUniqueIdTarget) :
		UniqueIdTarget(InUniqueIdTarget)
	{
	}

	/**
	 * Match a given unique Id against the one stored in this struct
	 *
	 * @return true if they are an exact match, false otherwise
	 */
 	bool Matches(const FUniqueNetId& Candidate) const
 	{
 		return UniqueIdTarget == Candidate;
 	}
 
	/**
	 * Match a given unique Id against the one stored in this struct
	 *
	 * @return true if they are an exact match, false otherwise
	 */
 	bool Matches(const TSharedPtr<FUniqueNetId>& Candidate) const
 	{
 		return UniqueIdTarget == *Candidate;
 	}

	/**
	 * Match a given unique Id against the one stored in this struct
	 *
	 * @return true if they are an exact match, false otherwise
	 */
	bool Matches(const TSharedRef<FUniqueNetId>& Candidate) const
	{
		return UniqueIdTarget == *Candidate;
	}
};

/**
 * Unique net id wrapper for a string
 */
class FUniqueNetIdString : public FUniqueNetId
{
public:
	/** Holds the net id for a player */
	FString UniqueNetIdStr;

	/** Default constructor */
	FUniqueNetIdString()
	{
	}

	/**
	 * Constructs this object with the specified net id
	 *
	 * @param InUniqueNetId the id to set ours to
	 */
	explicit FUniqueNetIdString(const FString& InUniqueNetId) 
		: UniqueNetIdStr(InUniqueNetId)
	{
	}

	/**
	 * Copy Constructor
	 *
	 * @param Src the id to copy
	 */
	explicit FUniqueNetIdString(const FUniqueNetId& Src) 
		: UniqueNetIdStr(Src.ToString())
	{
	}

	/**
	 * Copy Constructor
	 *
	 * @param Src the id to copy
	 */
	explicit FUniqueNetIdString(const FUniqueNetIdString& Src) 
		: UniqueNetIdStr(Src.UniqueNetIdStr)
	{
	}

	// IOnlinePlatformData

	virtual const uint8* GetBytes() const OVERRIDE
	{
		return (const uint8*)UniqueNetIdStr.GetCharArray().GetData();
	}

	virtual int32 GetSize() const OVERRIDE
	{
		return UniqueNetIdStr.GetCharArray().GetTypeSize() * UniqueNetIdStr.GetCharArray().Num();
	}

	virtual bool IsValid() const OVERRIDE
	{
		return !UniqueNetIdStr.IsEmpty();
	}

	virtual FString ToString() const OVERRIDE
	{
		return UniqueNetIdStr;
	}

	virtual FString ToDebugString() const OVERRIDE
	{
		return UniqueNetIdStr;
	}

	/** Needed for TMap::GetTypeHash() */
	friend uint32 GetTypeHash(const FUniqueNetIdString& A)
	{
		return ::GetTypeHash(A.UniqueNetIdStr);
	}
};

/** 
 * Abstraction of a profile service shared file handle
 * The class is meant to be opaque (see IOnlinePlatformData)
 */
class FSharedContentHandle : public IOnlinePlatformData
{
protected:

	/** Hidden on purpose */
	FSharedContentHandle()
	{
	}

	/** Hidden on purpose */
	FSharedContentHandle(const FSharedContentHandle& Src)
	{
	}

	/** Hidden on purpose */
	FSharedContentHandle& operator=(const FSharedContentHandle& Src)
	{
		return *this;
	}

public:

	virtual ~FSharedContentHandle() {}
};

/** 
 * Abstraction of a session's platform specific info
 * The class is meant to be opaque (see IOnlinePlatformData)
 */
class FOnlineSessionInfo : public IOnlinePlatformData
{
protected:

	/** Hidden on purpose */
	FOnlineSessionInfo()
	{
	}

	/** Hidden on purpose */
	FOnlineSessionInfo(const FOnlineSessionInfo& Src)
	{
	}

	/** Hidden on purpose */
	FOnlineSessionInfo& operator=(const FOnlineSessionInfo& Src)
	{
		return *this;
	}

public:

	virtual ~FOnlineSessionInfo() {}

	/**
	 * Get the session id associated with this session
	 *
	 * @return session id for this session
	 */
	virtual const FUniqueNetId& GetSessionId() const = 0;
};

/** Holds metadata about a given downloadable file */
struct FCloudFileHeader
{
	/** Hash value, if applicable, of the given file contents */
    FString Hash;
	/** Filename as downloaded */
    FString DLName;
	/** Logical filename, maps to the downloaded filename */
    FString FileName;
	/** File size */
    int32 FileSize;

    /** Constructors */
    FCloudFileHeader() :
		FileSize(0)
	{}

	FCloudFileHeader(const FString& InFileName, const FString& InDLName, int32 InFileSize) :
		DLName(InDLName),
		FileName(InFileName),
		FileSize(InFileSize)
	{}
};

/** Holds the data used in downloading a file asynchronously from the online service */
struct FCloudFile
{
	/** The name of the file as requested */
	FString FileName;
	/** The async state the file download is in */
	EOnlineAsyncTaskState::Type AsyncState;
	/** The buffer of data for the file */
	TArray<uint8> Data;

	/** Constructors */
	FCloudFile() :
		AsyncState(EOnlineAsyncTaskState::NotStarted)
	{
	}

	FCloudFile(const FString& InFileName) :
		FileName(InFileName),
		AsyncState(EOnlineAsyncTaskState::NotStarted)
	{
	}

	virtual ~FCloudFile() {}
};

/**
 * Base for all online user info
 */
class FOnlineUser
{
public:
	/** 
	 * @return Id associated with the user account provided by the online service during registration 
	 */
	virtual TSharedRef<FUniqueNetId> GetUserId() const = 0;
	/**
	 * @return the real name for the user if known
	 */
	virtual FString GetRealName() const = 0;
	/**
	 * @return the nickname of the user if known
	 */
	virtual FString GetDisplayName() const = 0;
	/** 
	 * @return Any additional user data associated with a registered user
	 */
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const = 0;
};

/**
 * User account information returned via IOnlineIdentity interface
 */
class FUserOnlineAccount : public FOnlineUser
{
public:
	/**
	 * @return Access token which is provided to user once authenticated by the online service
	 */
	virtual FString GetAccessToken() const = 0;
	/** 
	 * @return Any additional auth data associated with a registered user
	 */
	virtual bool GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const = 0;
};

/** 
 * Friend list invite states 
 */
namespace EInviteStatus
{
	enum Type
	{
		/** unknown state */
		Unknown,
		/** Friend has accepted the invite */
		Accepted,
		/** Friend has sent player an invite, but it has not been accepted/rejected */
		PendingInbound,
		/** Player has sent friend an invite, but it has not been accepted/rejected */
		PendingOutbound
	};

	/** 
	 * @return the stringified version of the enum passed in 
	 */
	inline const TCHAR* ToString(EInviteStatus::Type EnumVal)
	{
		switch (EnumVal)
		{
			case Unknown:
			{
				return TEXT("Unknown");
			}
			case Accepted:
			{
				return TEXT("Accepted");
			}
			case PendingInbound:
			{
				return TEXT("PendingInbound");
			}
			case PendingOutbound:
			{
				return TEXT("PendingOutbound");
			}
		}
		return TEXT("");
	}
};

/**
 * Friend user info returned via IOnlineFriends interface
 */
class FOnlineFriend : public FOnlineUser
{
public:
	/**
	 * @return the current invite status of a friend wrt to user that queried
	 */
	virtual EInviteStatus::Type GetInviteStatus() const = 0;
	/**
	 * @return presence info for an online friend
	 */
	virtual const class FOnlineUserPresence& GetPresence() const = 0;
};

/** The possible permission categories we can choose from to read from the server */
namespace EOnlineSharingReadCategory
{
	enum Type
	{
		None			= 0x00,
		// Read access to posts on the users feeds
		Posts			= 0x01,
		// Read access for a users friend information, and all data about those friends. e.g. Friends List and Individual Friends Birthday
		Friends			= 0x02,
		// Read access to a users mailbox
		Mailbox			= 0x04,
		// Read the current online status of a user
		OnlineStatus	= 0x08,
		// Read a users profile information, e.g. Users Birthday
		ProfileInfo		= 0x10,	
		// Read information about the users locations and location history
		LocationInfo	= 0x20,

		Default			= ProfileInfo|LocationInfo,
	};



	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineSharingReadCategory::Type CategoryType)
	{
		switch (CategoryType)
		{
		case None:
			{
				return TEXT("Category undefined");
			}
		case Posts:
			{
				return TEXT("Posts");
			}
		case Friends:
			{
				return TEXT("Friends");
			}
		case Mailbox:
			{
				return TEXT("Mailbox");
			}
		case OnlineStatus:
			{
				return TEXT("Online Status");
			}
		case ProfileInfo:
			{
				return TEXT("Profile Information");
			}
		case LocationInfo:
			{
				return TEXT("Location Information");
			}
		}
		return TEXT("");
	}
}


/** The possible permission categories we can choose from to publish to the server */
namespace EOnlineSharingPublishingCategory
{
	enum Type
	{
		None			= 0x00,
		// Permission to post to a users news feed
		Posts			= 0x01,
		// Permission to manage a users friends list. Add/Remove contacts
		Friends			= 0x02,
		// Manage a users account settings, such as pages they subscribe to, or which notifications they receive
		AccountAdmin	= 0x04,
		// Manage a users events. This features the capacity to create events as well as respond to events.
		Events			= 0x08,

		Default			= None,
	};


	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineSharingPublishingCategory::Type CategoryType)
	{
		switch (CategoryType)
		{
		case None:
			{
				return TEXT("Category undefined");
			}
		case Posts:
			{
				return TEXT("Posts");
			}
		case Friends:
			{
				return TEXT("Friends");
			}
		case AccountAdmin:
			{
				return TEXT("Account Admin");
			}
		case Events:
			{
				return TEXT("Events");
			}
		}
		return TEXT("");
	}
}


/** Privacy permissions used for Online Status updates */
namespace EOnlineStatusUpdatePrivacy
{
	enum Type
	{
		OnlyMe,			// Post will only be visible to the user alone
		OnlyFriends,	// Post will only be visible to the user and the users friends
		Everyone,		// Post will be visible to everyone
	};

	inline const TCHAR* ToString(EOnlineStatusUpdatePrivacy::Type PrivacyType)
	{
		switch (PrivacyType)
		{
		case OnlyMe:
			return TEXT("Only Me");
		case OnlyFriends:
			return TEXT("Only Friends");
		case Everyone:
			return TEXT("Everyone");
		}
	}
}

class FJsonValue;

/** Notification object, used to send messages between systems */
struct FOnlineNotification
{
	/** A string defining the type of this notification, used to determine how to parse the payload */
	FString TypeStr;

	/** The payload of this notification */
	TSharedPtr<FJsonValue> Payload;

	FOnlineNotification() 
	{

	}

	FOnlineNotification(const FString& InTypeStr, const TSharedPtr<FJsonValue>& InPayload)
		: TypeStr(InTypeStr), Payload(InPayload)
	{

	}
};


