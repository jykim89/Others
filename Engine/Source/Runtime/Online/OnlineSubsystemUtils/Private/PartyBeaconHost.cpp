// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"

APartyBeaconHost::APartyBeaconHost(const FPostConstructInitializeProperties& PCIP) :
	Super(PCIP),
	State(NULL)
{
	PrimaryActorTick.bCanEverTick = true;
}

bool APartyBeaconHost::InitHost()
{
	if (AOnlineBeaconHost::InitHost())
	{
		OnBeaconConnected(FName(TEXT("PartyBeacon"))).BindUObject(this, &APartyBeaconHost::ClientConnected);
		return true;
	}

	return false;
}

bool APartyBeaconHost::InitHostBeacon(int32 InTeamCount, int32 InTeamSize, int32 InMaxReservations, FName InSessionName, int32 InForceTeamNum)
{
	UE_LOG(LogBeacon, Verbose, TEXT("InitHostBeacon TeamCount:%d TeamSize:%d MaxSize:%d"), InTeamCount, InTeamSize, InMaxReservations);
	if (InMaxReservations > 0)
	{
		if (InitHost())
		{
			State = ConstructObject<UPartyBeaconState>(GetPartyBeaconHostClass());
			if (State->InitState(InTeamCount, InTeamSize, InMaxReservations, InSessionName, InForceTeamNum))
			{
				return true;
			}
		}
		else
		{
			UE_LOG(LogBeacon, Warning, TEXT("Failed to initialize beacon"));
		}
	}

	return false;
}

bool APartyBeaconHost::InitFromBeaconState(UPartyBeaconState* PrevState)
{
	if (!State && PrevState)
	{
		UE_LOG(LogBeacon, Verbose, TEXT("InitFromBeaconState TeamCount:%d TeamSize:%d MaxSize:%d"), PrevState->NumTeams, PrevState->NumPlayersPerTeam, PrevState->MaxReservations);

		if (InitHost())
		{
			State = PrevState;
			return true;
		}
		else
		{
			UE_LOG(LogBeacon, Warning, TEXT("Failed to initialize beacon"));
		}
	}

	return false;
}

bool APartyBeaconHost::ReconfigureTeamAndPlayerCount(int32 InNumTeams, int32 InNumPlayersPerTeam, int32 InNumReservations)
{
	bool bSuccess = false;
	if (NetDriver != NULL && State != NULL)
	{
		bSuccess = State->ReconfigureTeamAndPlayerCount(InNumTeams, InNumPlayersPerTeam, InNumReservations);
		UE_LOG(LogBeacon, Log,
			TEXT("Beacon (%s) reconfiguring team and player count."),
			*BeaconName);
	}
	else
	{
		UE_LOG(LogBeacon, Warning,
			TEXT("Beacon (%s) hasn't been initialized yet, can't change team and player count."),
			*BeaconName);
	}

	return bSuccess;
}

void APartyBeaconHost::Tick(float DeltaTime)
{
	if (State)
	{
		UWorld* World = GetWorld();
		IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);

		if (SessionInt.IsValid())
		{
			FName SessionName = State->GetSessionName();
			TArray<FPartyReservation>& Reservations = State->GetReservations();

			FNamedOnlineSession* Session = SessionInt->GetNamedSession(SessionName);
			if (Session)
			{
				TArray< TSharedPtr<FUniqueNetId> > PlayersToLogout;
				for (int32 ResIdx = 0; ResIdx < Reservations.Num(); ResIdx++)
				{
					FPartyReservation& PartyRes = Reservations[ResIdx];

					// Determine if we have a client connection for the reservation 
					bool bIsConnectedReservation = false;
					for (int32 ClientIdx = 0; ClientIdx < ClientActors.Num(); ClientIdx++)
					{
						APartyBeaconClient* Client = Cast<APartyBeaconClient>(ClientActors[ClientIdx]);
						if (Client)
						{
							const FPartyReservation& ClientRes = Client->GetPendingReservation();
							if (ClientRes.PartyLeader == PartyRes.PartyLeader)
							{
								bIsConnectedReservation = true;
								break;
							}
						}
						else
						{
							UE_LOG(LogBeacon, Error, TEXT("Missing PartyBeaconClient found in ClientActors array"));
							ClientActors.RemoveAtSwap(ClientIdx);
							ClientIdx--;
						}
					}

					// Don't update clients that are still connected
					if (bIsConnectedReservation)
					{
						for (int32 PlayerIdx = 0; PlayerIdx < PartyRes.PartyMembers.Num(); PlayerIdx++)
						{
							FPlayerReservation& PlayerEntry = PartyRes.PartyMembers[PlayerIdx];
							PlayerEntry.ElapsedTime = 0.0f;
						}
					}
					// Once a client beacon disconnects, update the elapsed time since they were found as a registrant in the game session
					else
					{
						for (int32 PlayerIdx = 0; PlayerIdx < PartyRes.PartyMembers.Num(); PlayerIdx++)
						{
							FPlayerReservation& PlayerEntry = PartyRes.PartyMembers[PlayerIdx];

							// Determine if the player is the owner of the session	
							const bool bIsSessionOwner = Session->OwningUserId.IsValid() && (*Session->OwningUserId == *PlayerEntry.UniqueId);

							// Determine if the player member is registered in the game session
							if (SessionInt->IsPlayerInSession(SessionName, *PlayerEntry.UniqueId) ||
								// Never timeout the session owner
								bIsSessionOwner)
							{
								FUniqueNetIdMatcher PlayerMatch(*PlayerEntry.UniqueId);
								int32 FoundIdx = State->PlayersPendingJoin.FindMatch(PlayerMatch);
								if (FoundIdx != INDEX_NONE)
								{
									UE_LOG(LogBeacon, Display, TEXT("Beacon (%s): pending player %s found in session (%s)."),
										*GetName(),
										*PlayerEntry.UniqueId->ToDebugString(),
										*SessionName.ToString());

									// reset elapsed time since found
									PlayerEntry.ElapsedTime = 0.0f;
									// also remove from pending join list
									State->PlayersPendingJoin.RemoveAtSwap(FoundIdx);
								}
							}
							else
							{
								// update elapsed time
								PlayerEntry.ElapsedTime += DeltaTime;

								// if the player is pending it's initial join then check against TravelSessionTimeoutSecs instead
								FUniqueNetIdMatcher PlayerMatch(*PlayerEntry.UniqueId);
								int32 FoundIdx = State->PlayersPendingJoin.FindMatch(PlayerMatch);
								const bool bIsPlayerPendingJoin = FoundIdx != INDEX_NONE;
								// if the timeout has been exceeded then add to list of players 
								// that need to be logged out from the beacon
								if ((bIsPlayerPendingJoin && PlayerEntry.ElapsedTime > TravelSessionTimeoutSecs) ||
									(!bIsPlayerPendingJoin && PlayerEntry.ElapsedTime > SessionTimeoutSecs))
								{
									PlayersToLogout.AddUnique(PlayerEntry.UniqueId.GetUniqueNetId());
								}
							}
						}
					}
				}

				// Logout any players that timed out
				for (int32 LogoutIdx = 0; LogoutIdx < PlayersToLogout.Num(); LogoutIdx++)
				{
					bool bFound = false;
					const TSharedPtr<FUniqueNetId>& UniqueId = PlayersToLogout[LogoutIdx];
					float ElapsedSessionTime = 0.f;
					for (int32 ResIdx = 0; ResIdx < Reservations.Num(); ResIdx++)
					{
						const FPartyReservation& PartyRes = Reservations[ResIdx];
						for (int32 PlayerIdx = 0; PlayerIdx < PartyRes.PartyMembers.Num(); PlayerIdx++)
						{
							const FPlayerReservation& PlayerEntry = PartyRes.PartyMembers[PlayerIdx];
							if (*PlayerEntry.UniqueId == *UniqueId)
							{
								ElapsedSessionTime = PlayerEntry.ElapsedTime;
								bFound = true;
								break;
							}
						}

						if (bFound)
						{
							break;
						}
					}

					UE_LOG(LogBeacon, Display, TEXT("Beacon (%s): player logout due to timeout for %s, elapsed time = %0.3f"),
						*GetName(),
						*UniqueId->ToDebugString(),
						ElapsedSessionTime);
					// Also remove from pending join list
					State->PlayersPendingJoin.RemoveSingleSwap(UniqueId);
					// let the beacon handle the logout and notifications/delegates
					FUniqueNetIdRepl RemovedId(UniqueId);
					HandlePlayerLogout(RemovedId);
				}
			}
		}
	}
}

int32 APartyBeaconHost::GetNumPlayersOnTeam(int32 TeamIdx) const
{
	int32 Result = 0;
	if (NetDriver != NULL && State != NULL)
	{
		Result = State->GetNumPlayersOnTeam(TeamIdx);
	}
	else
	{
		UE_LOG(LogBeacon, Warning,
			TEXT("Beacon (%s) hasn't been initialized yet, can't get team player count."),
			*BeaconName);
	}

	return Result;
}

int32 APartyBeaconHost::GetTeamForCurrentPlayer(const FUniqueNetId& PlayerId) const
{
	int32 TeamNum = INDEX_NONE;
	if (PlayerId.IsValid())
	{
		if (State)
		{
			TeamNum = State->GetTeamForCurrentPlayer(PlayerId);
		}
	}
	else
	{
		UE_LOG(LogBeacon, Display, TEXT("Invalid player when attempting to find team assignment"));
	}

	return TeamNum;
}

void APartyBeaconHost::NewPlayerAdded(const FPlayerReservation& NewPlayer)
{
	UE_LOG(LogBeacon, Verbose, TEXT("Beacon adding player %s"), *NewPlayer.UniqueId->ToDebugString());
	if (State)
	{
		State->PlayersPendingJoin.Add(NewPlayer.UniqueId.GetUniqueNetId());
	}
}

void APartyBeaconHost::HandlePlayerLogout(const FUniqueNetIdRepl& PlayerId)
{
	if (PlayerId.IsValid())
	{
		UE_LOG(LogBeacon, Verbose, TEXT("HandlePlayerLogout %s"), *PlayerId->ToDebugString());

		if (State)
		{
			if (State->RemovePlayer(PlayerId))
			{
				ReservationChanged.ExecuteIfBound();
			}
		}
	}
}

bool APartyBeaconHost::PlayerHasReservation(const FUniqueNetId& PlayerId) const
{
	bool bHasReservation = false;
	if (State)
	{
		bHasReservation = State->PlayerHasReservation(PlayerId);
	}
	else
	{
		UE_LOG(LogBeacon, Warning,
			TEXT("Beacon (%s) hasn't been initialized yet, no reservations."),
			*BeaconName);
	}

	return bHasReservation;
}

bool APartyBeaconHost::GetPlayerValidation(const FUniqueNetId& PlayerId, FString& OutValidation) const
{
	OutValidation = FString();

	bool bHasValidation = false;
	if (State)
	{
		bHasValidation = State->GetPlayerValidation(PlayerId, OutValidation);
	}
	else
	{
		UE_LOG(LogBeacon, Warning,
			TEXT("Beacon (%s) hasn't been initialized yet, no validation."),
			*BeaconName);
	}

	return bHasValidation;
}

EPartyReservationResult::Type APartyBeaconHost::AddPartyReservation(const FPartyReservation& ReservationRequest)
{
	EPartyReservationResult::Type Result = EPartyReservationResult::GeneralError;

	if (!State || BeaconState == EBeaconState::DenyRequests)
	{
		return EPartyReservationResult::ReservationDenied;
	}

	if (State->DoesReservationFit(ReservationRequest))
	{
		bool bContinue = true;
		if (ValidatePlayers.IsBound())
		{
			bContinue = ValidatePlayers.Execute(ReservationRequest.PartyMembers);
		}

		if (bContinue)
		{
			const int32 ExistingReservationIdx = State->GetExistingReservation(ReservationRequest.PartyLeader);
			if (ExistingReservationIdx != INDEX_NONE)
			{
				const TArray<FPartyReservation>& Reservations = State->GetReservations();
				const FPartyReservation& ExistingReservation = Reservations[ExistingReservationIdx];
				if (ReservationRequest.PartyMembers.Num() == ExistingReservation.PartyMembers.Num())
				{
					// Clean up the game entities for these duplicate players
					DuplicateReservation.ExecuteIfBound(ReservationRequest);

					// Add all players back into the pending join list
					for (int32 Count = 0; Count < ReservationRequest.PartyMembers.Num(); Count++)
					{
						NewPlayerAdded(ReservationRequest.PartyMembers[Count]);
					}

					Result = EPartyReservationResult::ReservationDuplicate;
				}
				else
				{
					Result = EPartyReservationResult::IncorrectPlayerCount;
				}
			}
			else
			{
				if (State->AreTeamsAvailable(ReservationRequest))
				{
					if (State->AddReservation(ReservationRequest))
					{
						// Keep track of newly added players
						for (const FPlayerReservation& PartyMember : ReservationRequest.PartyMembers)
						{
							NewPlayerAdded(PartyMember);
						}

						ReservationChanged.ExecuteIfBound();
						if (State->IsBeaconFull())
						{
							ReservationsFull.ExecuteIfBound();
						}

						Result = EPartyReservationResult::ReservationAccepted;
					}
					else
					{
						Result = EPartyReservationResult::IncorrectPlayerCount;
					}
				}
				else
				{
					Result = EPartyReservationResult::PartyLimitReached;
				}
			}
		}
		else
		{
			Result = EPartyReservationResult::ReservationDenied_Banned;
		}
	}
	else
	{
		Result = EPartyReservationResult::IncorrectPlayerCount;
	}

	return Result;
}

void APartyBeaconHost::RemovePartyReservation(const FUniqueNetIdRepl& PartyLeader)
{
	if (State && State->RemoveReservation(PartyLeader))
	{
		CancelationReceived.ExecuteIfBound(*PartyLeader);
		ReservationChanged.ExecuteIfBound();
	}
	else
	{
		UE_LOG(LogBeacon, Warning, TEXT("Failed to find reservation to cancel for leader %s:"), *PartyLeader->ToString());
	}
}

bool APartyBeaconHost::DoesSessionMatch(const FString& SessionId) const
{
	if (State)
	{
		UWorld* World = GetWorld();
		FName SessionName = State->GetSessionName();

		IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
		FNamedOnlineSession* Session = SessionInt.IsValid() ? SessionInt->GetNamedSession(SessionName) : NULL;
		if (Session && Session->SessionInfo.IsValid() && !SessionId.IsEmpty() && Session->SessionInfo->GetSessionId().ToString() == SessionId)
		{
			return true;
		}
	}

	return false;
}

void APartyBeaconHost::ProcessReservationRequest(APartyBeaconClient* Client, const FString& SessionId, const FPartyReservation& ReservationRequest)
{
	UE_LOG(LogBeacon, Verbose, TEXT("ProcessReservationRequest %s SessionId %s PartyLeader: %s from (%s)"), 
		Client ? *Client->GetName() : TEXT("NULL"), 
		*SessionId,
		ReservationRequest.PartyLeader.IsValid() ? *ReservationRequest.PartyLeader->ToString() : TEXT("INVALID"),
		Client ? *Client->GetNetConnection()->LowLevelDescribe() : TEXT("NULL"));

	if (Client)
	{
		EPartyReservationResult::Type Result = EPartyReservationResult::ReservationDenied;

		if (DoesSessionMatch(SessionId))
		{
			Result = AddPartyReservation(ReservationRequest);
		}

		Client->ClientReservationResponse(Result);
	}
}

void APartyBeaconHost::ProcessCancelReservationRequest(APartyBeaconClient* Client, const FUniqueNetIdRepl& PartyLeader)
{
	UE_LOG(LogBeacon, Verbose, TEXT("ProcessCancelReservationRequest %s PartyLeader: %s from (%s)"), 
		Client ? *Client->GetName() : TEXT("NULL"), 
		PartyLeader.IsValid() ? *PartyLeader->ToString() : TEXT("INVALID"),
		Client ? *Client->GetNetConnection()->LowLevelDescribe() : TEXT("NULL"));

	if (Client)
	{
		RemovePartyReservation(PartyLeader);
	}
}

void APartyBeaconHost::ClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection)
{
	UE_LOG(LogBeacon, Verbose, TEXT("ClientConnected %s from (%s)"), 
		NewClientActor ? *NewClientActor->GetName() : TEXT("NULL"), 
		NewClientActor ? *NewClientActor->GetNetConnection()->LowLevelDescribe() : TEXT("NULL"));
}

AOnlineBeaconClient* APartyBeaconHost::SpawnBeaconActor()
{	
	FActorSpawnParameters SpawnInfo;
	APartyBeaconClient* BeaconActor = GetWorld()->SpawnActor<APartyBeaconClient>(APartyBeaconClient::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
	if (BeaconActor)
	{
		BeaconActor->SetBeaconOwner(this);
	}

	return BeaconActor;
}

void APartyBeaconHost::DumpReservations() const
{
	UE_LOG(LogBeacon, Display, TEXT("Debug info for Beacon: %s"), *BeaconName);
	if (State)
	{
		State->DumpReservations();
	}
	UE_LOG(LogBeacon, Display, TEXT(""));
}
