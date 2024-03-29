// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PartyBeaconState.generated.h"

/** The result code that will be returned during party reservation */
UENUM()
namespace EPartyReservationResult
{
	enum Type
	{
		// Pending request due to async operation
		RequestPending,
		// An unknown error happened
		GeneralError,
		// All available reservations are booked
		PartyLimitReached,
		// Wrong number of players to join the session
		IncorrectPlayerCount,
		// No response from the host
		RequestTimedOut,
		// Already have a reservation entry for the requesting party leader
		ReservationDuplicate,
		// Couldn't find the party leader specified for a reservation update request 
		ReservationNotFound,
		// Space was available and it's time to join
		ReservationAccepted,
		// The beacon is paused and not accepting new connections
		ReservationDenied,
		// This player is banned
		ReservationDenied_Banned,
		// The reservation request was canceled before being sent
		ReservationRequestCanceled
	};
}

namespace EPartyReservationResult
{
	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EPartyReservationResult::Type SessionType)
	{
		switch (SessionType)
		{
		case RequestPending:
			{
				return TEXT("Pending Request");
			}
		case GeneralError:
			{
				return TEXT("General Error");
			}
		case PartyLimitReached:
			{
				return TEXT("Party Limit Reached");
			}
		case IncorrectPlayerCount:
			{
				return TEXT("Incorrect Player Count");
			}
		case RequestTimedOut:
			{
				return TEXT("Request Timed Out");
			}
		case ReservationDuplicate:
			{
				return TEXT("Reservation Duplicate");
			}
		case ReservationNotFound:
			{
				return TEXT("Reservation Not Found");
			}
		case ReservationAccepted:
			{
				return TEXT("Reservation Accepted");
			}
		case ReservationDenied:
			{
				return TEXT("Reservation Denied");
			}
		case ReservationDenied_Banned:
			{
				return TEXT("Reservation Banned");
			}
		case ReservationRequestCanceled:
			{
				return TEXT("Request Canceled");
			}
		}
		return TEXT("");
	}
}

/** A single player reservation */
USTRUCT()
struct FPlayerReservation
{
	GENERATED_USTRUCT_BODY()
	
	#if CPP
	FPlayerReservation() :
		ElapsedTime(0.0f)
	{}
	#endif

	/** Unique id for this reservation */
	UPROPERTY(Transient)
	FUniqueNetIdRepl UniqueId;

	/** Info needed to validate user credentials when joining a server */
	UPROPERTY(Transient)
	FString ValidationStr;

	/** Elapsed time since player made reservation and was last seen */
	UPROPERTY(Transient)
	float ElapsedTime;
};

/** A whole party reservation */
USTRUCT()
struct FPartyReservation
{
	GENERATED_USTRUCT_BODY()

	/** Team assigned to this party */
	UPROPERTY(Transient)
	int32 TeamNum;

	/** Player initiating the request */
	UPROPERTY(Transient)
	FUniqueNetIdRepl PartyLeader;

	/** All party members (including party leader) in the reservation */
	UPROPERTY(Transient)
	TArray<FPlayerReservation> PartyMembers;
};

/**
 * A beacon host used for taking reservations for an existing game session
 */
UCLASS(transient, notplaceable, config=Engine)
class ONLINESUBSYSTEMUTILS_API UPartyBeaconState : public UObject
{
	GENERATED_UCLASS_BODY()

	/**
	 * Initialize this state object 
	 *
	 * @param InTeamCount number of teams to make room for
	 * @param InTeamSize size of each team
	 * @param InMaxReservation max number of reservations allowed
	 * @param InSessionName name of session related to the beacon
	 * @param InForceTeamNum team to force players on if applicable (usually only 1 team games)
	 *
	 * @return true if successful created, false otherwise
	 */
	virtual bool InitState(int32 InTeamCount, int32 InTeamSize, int32 InMaxReservations, FName InSessionName, int32 InForceTeamNum);

	/**
	 * Reconfigures the beacon for a different team/player count configuration
	 * Allows dedicated server to change beacon parameters after a playlist configuration has been made
	 * Does no real checking against current reservations because we assume the UI wouldn't let
	 * this party start a gametype if they were too big to fit on a team together
	 *
	 * @param InNumTeams the number of teams that are expected to join
	 * @param InNumPlayersPerTeam the number of players that are allowed to be on each team
	 * @param InNumReservations the total number of players to allow to join (if different than team * players)
	 */
	virtual bool ReconfigureTeamAndPlayerCount(int32 InNumTeams, int32 InNumPlayersPerTeam, int32 InNumReservations);

	/**
	 * Add a reservation to the beacon state, tries to assign a team
	 * 
	 * @param ReservationRequest reservation to possibly add to this state
	 *
	 * @return true if successful, false otherwise
	 */
	virtual bool AddReservation(const FPartyReservation& ReservationRequest);

	/**
	 * Remove an entire reservation from this state object
	 *
	 * @param PartyLeader leader holding the reservation for the party 
	 *
	 * @return true if successful, false if reservation not found
	 */
	virtual bool RemoveReservation(const FUniqueNetIdRepl& PartyLeader);

	/**
	 * Remove a single player from their party's reservation
	 *
	 * PlayerId player to remove
	 *
	 * @return true if player found and removed, false otherwise
	 */
	virtual bool RemovePlayer(const FUniqueNetIdRepl& PlayerId);

	/**
	 * @return the name of the session associated with this beacon state
	 */
	virtual FName GetSessionName() const { return SessionName; }

	/**
	 * @return all reservations in this beacon state
	 */
	virtual TArray<FPartyReservation>& GetReservations() { return Reservations; }

	/**
	 * Get an existing reservation for a given party
	 *
	 * @param PartyLeader UniqueId of party leader for a reservation
	 *
	 * @return index of reservation, INDEX_NONE otherwise
	 */
	virtual int32 GetExistingReservation(const FUniqueNetIdRepl& PartyLeader);

	/**
	* Get the current reservation count inside the beacon
	* this is NOT the number of players in the game
	*
	* @return number of consumed reservations
	*/
	virtual int32 GetReservationCount() const { return Reservations.Num(); }

	/**
	 * @return the number of actually used reservations across all parties
	 */
	virtual int32 GetNumConsumedReservations() const { return NumConsumedReservations; }

	/**
	 * Determine if this reservation fits all rules for fitting in the game
	 * 
	 * @param ReservationRequest reservation to test for 
	 *
	 * @return true if reservation can be added to this state, false otherwise
	 */
	virtual bool DoesReservationFit(const FPartyReservation& ReservationRequest) const;

	/**
	 * @return true if the beacon is currently at max capacity
	 */
	virtual bool IsBeaconFull() const { return NumConsumedReservations == MaxReservations; }

	/**
	 * Get the number of teams.
	 *
	 * @return The number of teams.
	 */
	virtual int32 GetNumTeams() const { return NumTeams; }

	/**
	 * Get the number of current players on a given team.
	 *
	 * @param TeamIdx team of interest
	 *
	 * @return The number of teams.
	 */
	virtual int32 GetNumPlayersOnTeam(int32 TeamIdx) const;

	/**
	 * Get the team index for a given player
	 *
	 * @param PlayerId uniqueid for the player of interest
	 *
	 * @return team index for the given player, INDEX_NONE otherwise
	 */
	virtual int32 GetTeamForCurrentPlayer(const FUniqueNetId& PlayerId) const;

	/**
	 * Does a given player id have an existing reservation
	 *
	 * @param PlayerId uniqueid of the player to check
	 *
	 * @return true if a reservation exists, false otherwise
	 */
	virtual bool PlayerHasReservation(const FUniqueNetId& PlayerId) const;

	/**
	 * Obtain player validation string from party reservation entry
	 *
	 * @param PlayerId unique id of player to find validation in an existing reservation
	 * @param OutValidation [out] validation string used when player requested a reservation
	 *
	 * @return true if reservation exists for player
	 *
	 */
	virtual bool GetPlayerValidation(const FUniqueNetId& PlayerId, FString& OutValidation) const;

	/**
	 * Randomly assign a team for the reservation configuring the beacon
	 */
	virtual void InitTeamArray();

	/**
	* Determine if there are any teams that can fit the current party request.
	*
	* @param ReservationRequest reservation request to fit
	* @return true if there are teams available, false otherwise
	*/
	virtual bool AreTeamsAvailable(const FPartyReservation& ReservationRequest) const;

	/**
	* Determine the team number for the given party reservation request.
	* Uses the list of current reservations to determine what teams have open slots.
	*
	* @param PartyRequest the party reservation request received from the client beacon
	* @return index of the team to assign to all members of this party
	*/
	virtual int32 GetTeamAssignment(const FPartyReservation& Party);

	/**
	 * Output current state of reservations to log
	 */
	virtual void DumpReservations() const;

protected:

	/** Session tied to the beacon */
	UPROPERTY(Transient)
	FName SessionName;
	/** Number of currently consumed reservations */
	UPROPERTY(Transient)
	int32 NumConsumedReservations;
	/** Maximum allowed reservations */
	UPROPERTY(Transient)
	int32 MaxReservations;
	/** Number of teams in the game */
	UPROPERTY(Transient)
	int32 NumTeams;
	/** Number of players on each team for balancing */
	UPROPERTY(Transient)
	int32 NumPlayersPerTeam;
	/** Team that the host has been assigned to */
	UPROPERTY(Transient)
	int32 ReservedHostTeamNum;
	/** Team that everyone is forced to in single team games */
	UPROPERTY(Transient)
	int32 ForceTeamNum;

	/** Current reservations in the system */
	UPROPERTY(Transient)
	TArray<FPartyReservation> Reservations;
	/** Players that are expected to join shortly */
	TArray< TSharedPtr<FUniqueNetId> > PlayersPendingJoin;

	friend class APartyBeaconHost;
};
