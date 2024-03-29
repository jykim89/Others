// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// CheatManager
// Object within playercontroller that manages development "cheat"
// commands only spawned in single player mode
// No cheat manager is created in shipping builds.
//=============================================================================

#pragma once
#include "CheatManager.generated.h"

/** Debug Trace info for capturing **/
struct FDebugTraceInfo
{
	// Line Trace Start
	FVector LineTraceStart;

	// Line Trace End
	FVector LineTraceEnd;

	// Hit Normal Start
	FVector HitNormalStart;

	// Hit Normal End
	FVector HitNormalEnd;

	// Hit ImpactNormal End
	FVector HitImpactNormalEnd;

	// Hit Location
	FVector HitLocation;

	// Half collision capsule height
	float CapsuleHalfHeight;

	// Half collision capsule radius
	float CapsuleRadius;

	// this is when blocked and penetrating
	uint32 bInsideOfObject:1;

	FDebugTraceInfo()
		: LineTraceStart(ForceInit)
		, LineTraceEnd(ForceInit)
		, HitNormalStart(ForceInit)
		, HitNormalEnd(ForceInit)
		, HitLocation(ForceInit)
		, CapsuleHalfHeight(0)
		, CapsuleRadius(0)
		, bInsideOfObject(false)
	{
	}

};

UCLASS(Within=PlayerController)
class ENGINE_API UCheatManager : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Debug camera - used to have independent camera without stopping gameplay */
	UPROPERTY()
	class ADebugCameraController* DebugCameraControllerRef;

	/** Debug camera - used to have independent camera without stopping gameplay */
	UPROPERTY()
	TSubclassOf<class ADebugCameraController>  DebugCameraControllerClass;

	// Trace/Sweep debug start
	/** If we should should perform a debug capsule trace and draw results. Toggled with DebugCapsuleSweep() */
	uint32 bDebugCapsuleSweep:1;

	/** If we should should perform a debug capsule trace and draw results. Toggled with DebugCapsuleSweep() */
	uint32 bDebugCapsuleSweepPawn:1;

	/** If we should trace complex collision in debug capsule sweeps. Set with DebugCapsuleSweepComplex() */
	uint32 bDebugCapsuleTraceComplex:1;

	/** How far debugf trace should go out from player viewpoint */
	float DebugTraceDistance;

	/** Half distance between debug capsule sphere ends. Total heigh of capsule is 2*(this + DebugCapsuleRadius). */
	float DebugCapsuleHalfHeight;

	/** Radius of debug capsule */
	float DebugCapsuleRadius;

	/** How long to draw the normal result */
	float DebugTraceDrawNormalLength;

	/** what channel are we tracing **/
	TEnumAsByte<enum ECollisionChannel> DebugTraceChannel;

	/** array of information for capturing **/
	TArray<struct FDebugTraceInfo> DebugTraceInfoList;

	/** array of information for capturing **/
	TArray<struct FDebugTraceInfo> DebugTracePawnInfoList;

	/** Index of the array for current trace to overwrite.  Whenever you capture, this index will be increased **/
	int32 CurrentTraceIndex;

	/** Index of the array for current trace to overwrite.  Whenever you capture, this index will be increased **/
	int32 CurrentTracePawnIndex;

	/** Pause the game for Delay seconds. */
	UFUNCTION(exec)
	virtual void FreezeFrame(float Delay);

	/* Teleport to surface player is looking at. */
	UFUNCTION(exec)
	virtual void Teleport();

	/* Scale the player's size to be F * default size. */
	UFUNCTION(exec)
	virtual void ChangeSize(float F);

	/** Pawn can fly. */
	UFUNCTION(exec)
	virtual void Fly();

	/** Return to walking movement mode from Fly or Ghost cheat. */
	UFUNCTION(exec)
	virtual void Walk();

	/** Pawn no longer collides with the world, and can fly */
	UFUNCTION(exec)
	virtual void Ghost();

	/** Invulnerability cheat. */
	UFUNCTION(exec)
	virtual void God();

	/** Modify time dilation to change apparent speed of passage of time.  Slomo 0.1 makes everything move very slowly, Slomo 10 makes everything move very fast. */
	UFUNCTION(exec)
	virtual void Slomo(float T);

	/** Damage the actor you're looking at (sourced from the player). */
	UFUNCTION(exec)
	void DamageTarget(float DamageAmount);

	/** Destroy the actor you're looking at. */
	UFUNCTION(exec)
	virtual void DestroyTarget();

	/** Destroy all actors of class aClass */
	UFUNCTION(exec)
	virtual void DestroyAll(TSubclassOf<class AActor>  aClass);

	/** Destroys (by calling destroy directly) all non-player pawns of class aClass in the level */
	UFUNCTION(exec)
	virtual void DestroyPawns(TSubclassOf<class APawn> aClass);

	/** Load Classname and spawn an actor of that class */
	UFUNCTION(exec)
	virtual void Summon(const FString& ClassName);

	// Toggle AI ignoring the player.
	UFUNCTION(exec)
	virtual void AIIgnorePlayers();

	/** Freeze everything in the level except for players. */
	UFUNCTION(exec)
	virtual void PlayersOnly();

	/** Make controlled pawn the viewtarget again. */
	UFUNCTION(exec)
	virtual void ViewSelf();

	/** View from the point of view of player with PlayerName S. */
	UFUNCTION(exec)
	virtual void ViewPlayer(const FString& S);

	/** View from the point of view of AActor with Name ActorName. */
	UFUNCTION(exec)
	virtual void ViewActor(FName ActorName);

	/** View from the point of view of an AActor of class DesiredClass.  Each subsequent ViewClass cycles through the list of actors of that class. */
	UFUNCTION(exec)
	virtual void ViewClass(TSubclassOf<class AActor> DesiredClass);

	/** Stream in the given level. */
	UFUNCTION(exec)
	virtual void StreamLevelIn(FName PackageName);

	/** Load the given level. */
	UFUNCTION(exec)
	virtual void OnlyLoadLevel(FName PackageName);

	/** Stream out the given level. */
	UFUNCTION(exec)
	virtual void StreamLevelOut(FName PackageName);

	/** Toggle between debug camera/player camera without locking gameplay and with locking local player controller input. */
	UFUNCTION(exec)
	virtual void ToggleDebugCamera();

	/** toggles AI logging */
	UFUNCTION(exec)
	virtual void ToggleAILogging();

	UFUNCTION(reliable, server, WithValidation)
	virtual void ServerToggleAILogging();

	/** makes various AI logging categories verbose */
	UFUNCTION(exec)
	virtual void AILoggingVerbose();

	/** Toggle capsule trace debugging. Will trace a capsule from current view point and show where it hits the world */
	UFUNCTION(exec)
	virtual void DebugCapsuleSweep();

	/** Change Trace capsule size **/
	UFUNCTION(exec)
	virtual void DebugCapsuleSweepSize(float HalfHeight, float Radius);

	/** Change Trace Channel **/
	UFUNCTION(exec)
	virtual void DebugCapsuleSweepChannel(enum ECollisionChannel Channel);

	/** Change Trace Complex setting **/
	UFUNCTION(exec)
	virtual void DebugCapsuleSweepComplex(bool bTraceComplex);

	/** Capture current trace and add to persistent list **/
	UFUNCTION(exec)
	virtual void DebugCapsuleSweepCapture();

	/** Capture current local PC's pawn's location and add to persistent list **/
	UFUNCTION(exec)
	virtual void DebugCapsuleSweepPawn();

	/** Clear persistent list for trace capture **/
	UFUNCTION(exec)
	virtual void DebugCapsuleSweepClear();

	/** Test all volumes in the world to the player controller's view location**/
	UFUNCTION(exec)
	virtual void TestCollisionDistance();

	/** Spawns a Slate Widget Inspector in game **/
	UFUNCTION(exec)
	virtual void WidgetReflector();

	/** Builds the navigation mesh (or rebuilds it). **/
	UFUNCTION(exec)
	virtual void RebuildNavigation();

	/** Sets navigation drawing distance. Relevant only in non-editor modes. **/
	UFUNCTION(exec)
	void SetNavDrawDistance(float DrawDistance);

	/** Dump online session information */
	UFUNCTION(exec)
	virtual void DumpOnlineSessionState();

	UFUNCTION(exec)
	virtual void DumpVoiceMutingState();

	/**
	 * This will move the player and set their rotation to the passed in values.
	 * We have this version of the BugIt family as it is easier to type in just raw numbers in the console.
	 */
	UFUNCTION(exec)
	virtual void BugItGo(float X, float Y, float Z, float Pitch, float Yaw, float Roll);

	/**
	 * This will move the player and set their rotation to the passed in values.
	 * We have this version of the BugIt family strings can be passed in from the game ?options easily
	 */
	virtual void BugItGoString(const FString& TheLocation, const FString& TheRotation);

	/**
	* This function is used to print out the BugIt location.  It prints out copy and paste versions for both IMing someone to type in
	* and also a gameinfo ?options version so that you can append it to your launching url and be taken to the correct place.
	* Additionally, it will take a screen shot so reporting bugs is a one command action!
	*
	**/
	UFUNCTION(exec)
	virtual void BugIt(const FString& ScreenShotDescription = TEXT(""));

	/** This will create a BugItGo string for us.  Nice for calling form c++ where you just want the string and no Screenshots **/
	UFUNCTION(exec)
	virtual void BugItStringCreator(FVector ViewLocation, FRotator ViewRotation, FString& GoString, FString& LocString);

	/** This will force a flush of the output log to file*/
	UFUNCTION(exec)
	virtual void FlushLog();

	/** Logs the current location in bugit format without taking screenshot and further routing. */
	UFUNCTION(exec)
	virtual void LogLoc();

	/** Translate world origin to this player position */
	UFUNCTION(exec)
	void SetWorldOrigin();

	/** toggle "always on" GameplayDebuggingComponent's channels*/
	UFUNCTION(exec)
	void ToggleGameplayDebugView(const FString& ViewName);

	/** insta-runs EQS query for GameplayDebugComponent selected AI */
	UFUNCTION(exec)
	void RunEQS(const FString& QueryName);

	/**
	 * This will move the player and set their rotation to the passed in values.
	 * This actually does the location / rotation setting.  Additionally it will set you as ghost as the level may have
	 * changed since the last time you were here.  And the bug may actually be inside of something.
	 */
	virtual void BugItWorker( FVector TheLocation, FRotator TheRotation );

	/** Bug it log to file */
	virtual void LogOutBugItGoToLogFile( const FString& InScreenShotDesc, const FString& InGoString, const FString& InLocString );

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	
	/** Do any trace debugging that is currently enabled */
	void TickCollisionDebug();

	/** Add Debug Trace info into current index - used when DebugCapsuleSweepPawn is on**/
	void AddCapsuleSweepDebugInfo(
		const FVector & LineTraceStart, 
		const FVector & LineTraceEnd, 
		const FVector & HitImpactLocation, 
		const FVector & HitNormal, 
		const FVector & HitImpactNormal, 
		const FVector & HitLocation, 
		float CapsuleHalfheight, 
		float CapsuleRadius, 
		bool bTracePawn, 
		bool bInsideOfObject );	

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	/** streaming level debugging */
	virtual void SetLevelStreamingStatus(FName PackageName, bool bShouldBeLoaded, bool bShouldBeVisible);

	/** 
	 * Called when CheatManager is created to allow any needed initialization.  
	 * This is not an actor, so we need a stand in for PostInitializeComponents 
	 */
	virtual void InitCheatManager();

	/** Use the Outer Player Controller to get a World.  */
	UWorld* GetWorld() const;
protected:

	/** Do game specific bugIt */
	virtual bool DoGameSpecificBugItLog(FOutputDevice& OutputFile) { return true; }

	/** Switch controller to debug camera without locking gameplay and with locking local player controller input */
	virtual void EnableDebugCamera();
	
	/** Switch controller from debug camera back to normal controller */
	virtual void DisableDebugCamera();
};



