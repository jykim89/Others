// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "BSPOps.h"

DEFINE_LOG_CATEGORY_STATIC(LogEditorTransaction, Log, All);

/*-----------------------------------------------------------------------------
	A single transaction.
-----------------------------------------------------------------------------*/

void FTransaction::FObjectRecord::SerializeContents( FArchive& Ar, int32 InOper )
{
	if( Array )
	{
		//UE_LOG( LogEditorTransaction, Log, TEXT("Array %s %i*%i: %i"), Object ? *Object->GetFullName() : TEXT("Invalid Object"), Index, ElementSize, InOper);

		check((SIZE_T)Array>=(SIZE_T)Object+sizeof(UObject));
		check((SIZE_T)Array+sizeof(FScriptArray)<=(SIZE_T)Object+Object->GetClass()->GetPropertiesSize());
		check(ElementSize!=0);
		check(Serializer!=NULL);
		check(Index>=0);
		check(Count>=0);
		if( InOper==1 )
		{
			// "Saving add order" or "Undoing add order" or "Redoing remove order".
			if( Ar.IsLoading() )
			{
				checkSlow(Index+Count<=Array->Num());
				for( int32 i=Index; i<Index+Count; i++ )
				{
					Destructor( (uint8*)Array->GetData() + i*ElementSize );
				}
				Array->Remove( Index, Count, ElementSize );
			}
		}
		else
		{
			// "Undo/Redo Modify" or "Saving remove order" or "Undoing remove order" or "Redoing add order".
			if( InOper==-1 && Ar.IsLoading() )
			{
				Array->Insert( Index, Count, ElementSize );
				FMemory::Memzero( (uint8*)Array->GetData() + Index*ElementSize, Count*ElementSize );
			}

			// Serialize changed items.
			check(Index+Count<=Array->Num());
			for( int32 i=Index; i<Index+Count; i++ )
			{
				Serializer( Ar, (uint8*)Array->GetData() + i*ElementSize );
			}
		}
	}
	else
	{
		//UE_LOG(LogEditorTransaction, Log,  TEXT("Object %s"), *Object->GetFullName());
		check(Index==0);
		check(ElementSize==0);
		check(Serializer==NULL);
		Object->Serialize( Ar );
	}
}

void FTransaction::FObjectRecord::Restore( FTransaction* Owner )
{
	if( !bRestored )
	{
		bRestored = true;
		TArray<uint8> FlipData;
		TArray<UObject*> FlipReferencedObjects;
		TArray<FName> FlipReferencedNames;
		if( Owner->bFlip )
		{
			FWriter Writer( FlipData, FlipReferencedObjects, FlipReferencedNames, bWantsBinarySerialization );
			SerializeContents( Writer, -Oper );
		}
		FTransaction::FObjectRecord::FReader Reader( Owner, Data, ReferencedObjects, ReferencedNames, bWantsBinarySerialization );
		SerializeContents( Reader, Oper );
		if( Owner->bFlip )
		{
			Exchange( Data, FlipData );
			Exchange( ReferencedObjects, FlipReferencedObjects );
			Exchange( ReferencedNames, FlipReferencedNames );
			Oper *= -1;
		}
	}
}

void FTransaction::RemoveRecords( int32 Count /* = 1  */ )
{
	if ( Records.Num() >= Count )
	{
		Records.RemoveAt( Records.Num() - Count, Count );

		// Kill our object maps that are used to track redundant saves
		ObjectMap.Empty();
	}
}

/**
 * Outputs the contents of the ObjectMap to the specified output device.
 */
void FTransaction::DumpObjectMap(FOutputDevice& Ar) const
{
	Ar.Logf( TEXT("===== DumpObjectMap %s ==== "), *Title.ToString() );
	for ( ObjectMapType::TConstIterator It(ObjectMap) ; It ; ++It )
	{
		const UObject* CurrentObject	= It.Key();
		const int32 SaveCount				= It.Value();
		Ar.Logf( TEXT("%i\t: %s"), SaveCount, *CurrentObject->GetPathName() );
	}
	Ar.Logf( TEXT("=== EndDumpObjectMap %s === "), *Title.ToString() );
}

FArchive& operator<<( FArchive& Ar, FTransaction::FObjectRecord& R )
{
	check(R.Object);
	FMemMark Mark(FMemStack::Get());
	Ar << R.Object;
	Ar << R.Data;
	Ar << R.ReferencedObjects;
	Ar << R.ReferencedNames;
	Mark.Pop();
	return Ar;
}

void FTransaction::FObjectRecord::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( Object );
	for( int32 ObjIndex = 0; ObjIndex < ReferencedObjects.Num(); ObjIndex++ )
	{
		Collector.AddReferencedObject( ReferencedObjects[ ObjIndex ] );
	}
}

void FTransaction::AddReferencedObjects( FReferenceCollector& Collector )
{
	for( int32 Index = 0; Index < Records.Num(); Index++ )
	{
		Records[ Index ].AddReferencedObjects( Collector );
	}
	for( ObjectMapType::TIterator It( ObjectMap ); It; ++It )
	{
		Collector.AddReferencedObject( It.Key() );
	}
}

// FTransactionBase interface.
void FTransaction::SaveObject( UObject* Object )
{
	check(Object);
	Object->CheckDefaultSubobjects();

	int32* SaveCount = ObjectMap.Find(Object);
	if ( !SaveCount )
	{
		ObjectMap.Add(Object,1);
		// Save the object.
		new( Records )FObjectRecord( this, Object, NULL, 0, 0, 0, 0, NULL, NULL );
	}
	else
	{
		++(*SaveCount);
	}
}

void FTransaction::SaveArray( UObject* Object, FScriptArray* Array, int32 Index, int32 Count, int32 Oper, int32 ElementSize, STRUCT_AR Serializer, STRUCT_DTOR Destructor )
{
	check(Object);
	check(Array);
	check(ElementSize);
	check(Serializer);
	check(Object->IsValidLowLevel());
	check((SIZE_T)Array>=(SIZE_T)Object);
	check((SIZE_T)Array+sizeof(FScriptArray)<=(SIZE_T)Object+Object->GetClass()->PropertiesSize);
	check(Index>=0);
	check(Count>=0);
	check(Index+Count<=Array->Num());

	// don't serialize the array if the object is contained within a PIE package
	if( Object->HasAnyFlags(RF_Transactional) && (Object->GetOutermost()->PackageFlags&PKG_PlayInEditor) == 0 )
	{
		// Save the array.
		new( Records )FObjectRecord( this, Object, Array, Index, Count, Oper, ElementSize, Serializer, Destructor );
	}
}

void FTransaction::SetPrimaryObject(UObject* InObject)
{
	if (PrimaryObject == NULL)
	{
		PrimaryObject = InObject;
	}
}

/**
 * Enacts the transaction.
 */
void FTransaction::Apply()
{
	checkSlow(Inc==1||Inc==-1);

	// Figure out direction.
	const int32 Start = Inc==1 ? 0             : Records.Num()-1;
	const int32 End   = Inc==1 ? Records.Num() :              -1;

	// Init objects.
	TArray<UObject*> ChangedObjects;
	for( int32 i=Start; i!=End; i+=Inc )
	{
		Records[i].bRestored = false;
		if(ChangedObjects.Find(Records[i].Object) == INDEX_NONE)
		{
			Records[i].Object->CheckDefaultSubobjects();
			Records[i].Object->PreEditUndo();
			ChangedObjects.Add(Records[i].Object);
		}
	}
	for( int32 i=Start; i!=End; i+=Inc )
	{
		Records[i].Restore( this );
	}

	NumModelsModified = 0;		// Count the number of UModels that were changed.
	for(int32 ObjectIndex = 0;ObjectIndex < ChangedObjects.Num();ObjectIndex++)
	{
		UObject* ChangedObject = ChangedObjects[ObjectIndex];
		UModel* Model = Cast<UModel>(ChangedObject);
		if( Model && Model->Nodes.Num() )
		{
			FBSPOps::bspBuildBounds( Model );
			++NumModelsModified;
		}
		ChangedObject->PostEditUndo();
	}
	
	// Rebuild BSP here instead of waiting for the next tick since
	// multiple transaction events can occur in a single tick
	if (ABrush::NeedsRebuild())
	{
		GEditor->RebuildAlteredBSP();
	}

	// Flip it.
	if( bFlip )
	{
		Inc *= -1;
	}
	for(int32 ObjectIndex = 0;ObjectIndex < ChangedObjects.Num();ObjectIndex++)
	{
		UObject* ChangedObject = ChangedObjects[ObjectIndex];
		ChangedObject->CheckDefaultSubobjects();
	}
}

SIZE_T FTransaction::DataSize() const
{
	SIZE_T Result=0;
	for( int32 i=0; i<Records.Num(); i++ )
	{
		Result += Records[i].Data.Num();
	}
	return Result;
}

/**
 * Get all the objects that are part of this transaction.
 * @param	Objects		[out] Receives the object list.  Previous contents are cleared.
 */
void FTransaction::GetTransactionObjects(TArray<UObject*>& Objects)
{
	Objects.Empty(); // Just in case.

	for(int32 i=0; i<Records.Num(); i++)
	{
		UObject* obj = Records[i].Object;
		if(obj)
		{
			Objects.AddUnique(obj);
		}
	}
}


/*-----------------------------------------------------------------------------
	Transaction tracking system.
-----------------------------------------------------------------------------*/
UTransactor::UTransactor(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

UTransBuffer::UTransBuffer(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

UTransBuffer::UTransBuffer( const class FPostConstructInitializeProperties& PCIP, SIZE_T InMaxMemory )
:	UTransactor(PCIP)
,	MaxMemory( InMaxMemory )
{
	// Reset.
	Reset( NSLOCTEXT("UnrealEd", "Startup", "Startup") );
	CheckState();

	UE_LOG(LogInit, Log, TEXT("Transaction tracking system initialized") );
}

// UObject interface.
void UTransBuffer::Serialize( FArchive& Ar )
{
	check( !Ar.IsPersistent() );

	CheckState();

	Super::Serialize( Ar );

	if ( IsObjectSerializationEnabled() || !Ar.IsObjectReferenceCollector() )
	{
		Ar << UndoBuffer;
	}
	Ar << ResetReason << UndoCount << ActiveCount;

	CheckState();
}

void UTransBuffer::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		CheckState();
		UE_LOG(LogExit, Log, TEXT("Transaction tracking system shut down") );
	}
	Super::FinishDestroy();
}

void UTransBuffer::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	UTransBuffer* This = CastChecked<UTransBuffer>(InThis);
	This->CheckState();

	if ( This->IsObjectSerializationEnabled() )
	{
		// We cannot support undoing across GC if we allow it to eliminate references so we need
		// to suppress it.
		Collector.AllowEliminatingReferences(false);
		for( int32 Index = 0; Index < This->UndoBuffer.Num(); Index++ )
		{
			This->UndoBuffer[ Index ].AddReferencedObjects( Collector );
		}
		Collector.AllowEliminatingReferences(true);
	}

	This->CheckState();

	Super::AddReferencedObjects( This, Collector );
}

int32 UTransBuffer::Begin( const TCHAR* SessionContext, const FText& Description )
{
	CheckState();
	const int32 Result = ActiveCount;
	if( ActiveCount++==0 )
	{
		// Cancel redo buffer.
		if( UndoCount )
		{
			UndoBuffer.RemoveAt( UndoBuffer.Num()-UndoCount, UndoCount );
		}
		UndoCount = 0;

		// Purge previous transactions if too much data occupied.
		while( GetUndoSize() > MaxMemory )
		{
			UndoBuffer.RemoveAt( 0 );
		}

		// Begin a new transaction.
		GUndo = new(UndoBuffer)FTransaction( SessionContext, Description, 1 );
	}
	CheckState();
	return Result;
}


int32 UTransBuffer::End()
{
	CheckState();
	const int32 Result = ActiveCount;
	// Don't assert as we now purge the buffer when resetting.
	// So, the active count could be 0, but the code path may still call end.
	if (ActiveCount >= 1)
	{
		if( --ActiveCount==0 )
		{
#if 0 // @todo DB: please don't remove this code -- thanks! :)
			// End the current transaction.
			if ( GUndo && GLog )
			{
				// @todo DB: Fix this potentially unsafe downcast.
				static_cast<FTransaction*>(GUndo)->DumpObjectMap( *GLog );
			}
#endif
			GUndo = NULL;
		}
		CheckState();
	}
	return ActiveCount;
}


void UTransBuffer::Reset( const FText& Reason )
{
	CheckState();

	if( ActiveCount != 0 )
	{
		FString ErrorMessage = TEXT("");
		ErrorMessage += FString::Printf(TEXT("Non zero active count in UTransBuffer::Reset") LINE_TERMINATOR );
		ErrorMessage += FString::Printf(TEXT("ActiveCount : %d"	) LINE_TERMINATOR, ActiveCount );
		ErrorMessage += FString::Printf(TEXT("SessionName : %s"	) LINE_TERMINATOR, *GetUndoContext(false).Context );
		ErrorMessage += FString::Printf(TEXT("Reason      : %s"	) LINE_TERMINATOR, *Reason.ToString() );

		ErrorMessage += FString::Printf( LINE_TERMINATOR );
		ErrorMessage += FString::Printf(TEXT("Purging the undo buffer...") LINE_TERMINATOR );

		UE_LOG(LogEditorTransaction, Log, TEXT("%s"), *ErrorMessage);

	
		// Clear out the transaction buffer...
		Cancel(0);
	}

	// Reset all transactions.
	UndoBuffer.Empty();
	UndoCount    = 0;
	ResetReason  = Reason;
	ActiveCount  = 0;

	CheckState();
}


void UTransBuffer::Cancel( int32 StartIndex /*=0*/ )
{
	CheckState();

	// if we don't have any active actions, we shouldn't have an active transaction at all
	if ( ActiveCount > 0 )
	{
		if ( StartIndex == 0 )
		{
			// clear the global pointer to the soon-to-be-deleted transaction
			GUndo = NULL;
			
			// remove the currently active transaction from the buffer
			UndoBuffer.Pop();
		}
		else
		{
			FTransaction& Transaction = UndoBuffer.Last();
			Transaction.RemoveRecords(ActiveCount - StartIndex);
		}

		// reset the active count
		ActiveCount = StartIndex;
	}

	CheckState();
}


bool UTransBuffer::CanUndo( FText* Text )
{
	CheckState();
	if( ActiveCount )
	{
		if( Text )
		{
			*Text = NSLOCTEXT("TransactionSystem", "CantUndoDuringTransaction", "(Can't undo while action is in progress)");
		}
		return false;
	}
	if( UndoBuffer.Num()==UndoCount )
	{
		if( Text )
		{
			*Text = FText::Format( NSLOCTEXT("TransactionSystem", "CantUndoAfter", "(Can't undo after: {0})"), ResetReason );
		}
		return false;
	}
	return true;
}


bool UTransBuffer::CanRedo( FText* Text )
{
	CheckState();
	if( ActiveCount )
	{
		if( Text )
		{
			*Text = NSLOCTEXT("TransactionSystem", "CantRedoDuringTransaction", "(Can't redo while action is in progress)");
		}
		return 0;
	}
	if( UndoCount==0 )
	{
		if( Text )
		{
			*Text = NSLOCTEXT("TransactionSystem", "NothingToRedo", "(Nothing to redo)");
		}
		return 0;
	}
	return 1;
}


const FTransaction* UTransBuffer::GetTransaction( int32 QueueIndex ) const
{
	if (UndoBuffer.Num() > QueueIndex)
	{
		return &UndoBuffer[QueueIndex];
	}

	return NULL;
}


FUndoSessionContext UTransBuffer::GetUndoContext( bool bCheckWhetherUndoPossible )
{
	FUndoSessionContext Context;
	FText Title;
	if( bCheckWhetherUndoPossible && !CanUndo( &Title ) )
	{
		Context.Title = Title;
		return Context;
	}

	const FTransaction* Transaction = &UndoBuffer[ UndoBuffer.Num() - (UndoCount + 1) ];
	return Transaction->GetContext();
}


FUndoSessionContext UTransBuffer::GetRedoContext()
{
	FUndoSessionContext Context;
	FText Title;
	if( !CanRedo( &Title ) )
	{
		Context.Title = Title;
		return Context;
	}

	const FTransaction* Transaction = &UndoBuffer[ UndoBuffer.Num() - UndoCount ];
	return Transaction->GetContext();
}


bool UTransBuffer::Undo()
{
	CheckState();

	if (!CanUndo())
	{
		UndoDelegate.Broadcast(FUndoSessionContext(), false);

		return false;
	}

	// Apply the undo changes.
	GIsTransacting = true;
	{
		FTransaction& Transaction = UndoBuffer[ UndoBuffer.Num() - ++UndoCount ];
		UE_LOG(LogEditorTransaction, Log,  TEXT("Undo %s"), *Transaction.GetTitle().ToString() );

		BeforeRedoUndoDelegate.Broadcast(Transaction.GetContext());
		Transaction.Apply();
		UndoDelegate.Broadcast(Transaction.GetContext(), true);
	}
	GIsTransacting = false;

	CheckState();

	return true;
}

bool UTransBuffer::Redo()
{
	CheckState();

	if (!CanRedo())
	{
		RedoDelegate.Broadcast(FUndoSessionContext(), false);

		return false;
	}

	// Apply the redo changes.
	GIsTransacting = true;
	{
		FTransaction& Transaction = UndoBuffer[ UndoBuffer.Num() - UndoCount-- ];
		UE_LOG(LogEditorTransaction, Log,  TEXT("Redo %s"), *Transaction.GetTitle().ToString() );

		BeforeRedoUndoDelegate.Broadcast(Transaction.GetContext());
		Transaction.Apply();
		RedoDelegate.Broadcast(Transaction.GetContext(), true);
	}
	GIsTransacting = false;

	CheckState();

	return true;
}

bool UTransBuffer::EnableObjectSerialization()
{
	return --DisallowObjectSerialization == 0;
}

bool UTransBuffer::DisableObjectSerialization()
{
	return ++DisallowObjectSerialization == 0;
}

ITransaction* UTransBuffer::CreateInternalTransaction()
{
	return new FTransaction( TEXT("Internal") );
}


SIZE_T UTransBuffer::GetUndoSize() const
{
	SIZE_T Result=0;
	for( int32 i=0; i<UndoBuffer.Num(); i++ )
	{
		Result += UndoBuffer[i].DataSize();
	}
	return Result;
}


void UTransBuffer::CheckState() const
{
	// Validate the internal state.
	check(UndoBuffer.Num()>=UndoCount);
	check(ActiveCount>=0);
}


void UTransBuffer::SetPrimaryUndoObject(UObject* PrimaryObject)
{
	// Only record the primary object if its transactional, not in any of the temporary packages and theres an active transaction
	if ( PrimaryObject && PrimaryObject->HasAnyFlags( RF_Transactional ) &&
		( (PrimaryObject->GetOutermost()->PackageFlags & ( PKG_PlayInEditor|PKG_ContainsScript|PKG_CompiledIn ) ) == 0 ) )
	{
		const int32 NumTransactions = UndoBuffer.Num();
		const int32 CurrentTransactionIdx = NumTransactions - (UndoCount + 1);

		if ( CurrentTransactionIdx >= 0 )
		{
			FTransaction* Transaction = &UndoBuffer[ CurrentTransactionIdx ];
			Transaction->SetPrimaryObject(PrimaryObject);
		}
	}
}
