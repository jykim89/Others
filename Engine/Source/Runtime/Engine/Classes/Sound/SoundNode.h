// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "SoundNode.generated.h"

/*-----------------------------------------------------------------------------
	USoundNode helper macros. 
-----------------------------------------------------------------------------*/

#define DECLARE_SOUNDNODE_ELEMENT(Type,Name)													\
	Type& Name = *((Type*)(Payload));															\
	Payload += sizeof(Type);														

#define DECLARE_SOUNDNODE_ELEMENT_PTR(Type,Name)												\
	Type* Name = (Type*)(Payload);																\
	Payload += sizeof(Type);														

#define	RETRIEVE_SOUNDNODE_PAYLOAD( Size )														\
		uint8*	Payload					= NULL;													\
		uint32*	RequiresInitialization	= NULL;													\
		{																						\
			uint32* TempOffset = ActiveSound.SoundNodeOffsetMap.Find(NodeWaveInstanceHash);		\
			uint32 Offset;																		\
			if( !TempOffset )																	\
			{																					\
				Offset = ActiveSound.SoundNodeData.AddZeroed( Size + sizeof(uint32));				\
				ActiveSound.SoundNodeOffsetMap.Add( NodeWaveInstanceHash, Offset );				\
				RequiresInitialization = (uint32*) &ActiveSound.SoundNodeData[Offset];			\
				*RequiresInitialization = 1;													\
				Offset += sizeof(uint32);															\
			}																					\
			else																				\
			{																					\
				RequiresInitialization = (uint32*) &ActiveSound.SoundNodeData[*TempOffset];		\
				Offset = *TempOffset + sizeof(uint32);											\
			}																					\
			Payload = &ActiveSound.SoundNodeData[Offset];										\
		}

UCLASS(abstract, hidecategories=Object, editinlinenew)
class ENGINE_API USoundNode : public UObject
{
	GENERATED_UCLASS_BODY()

	static const int32 MAX_ALLOWED_CHILD_NODES = 32;

	UPROPERTY()
	TArray<class USoundNode*> ChildNodes;

#if WITH_EDITORONLY_DATA
	/** X position of node in the editor, so old UEdGraphNode data not lost.*/
	UPROPERTY()
	int32 NodePosX_DEPRECATED;

	/** Y position of node in the editor, so old UEdGraphNode data not lost. */
	UPROPERTY()
	int32 NodePosY_DEPRECATED;

	/** Node's Graph representation, used to get position. */
	class USoundCueGraphNode*	GraphNode;
#endif

public:
	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif
#if WITH_EDITORONLY_DATA
	virtual void Serialize(FArchive& Ar) OVERRIDE;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#endif //WITH_EDITORONLY_DATA
	// End UObject Interface

	//
	// USoundNode interface. 
	//

	/**
	 * Notifies the sound node that a wave instance in its subtree has finished.
	 *
	 * @param WaveInstance	WaveInstance that was finished 
	 */
	virtual bool NotifyWaveInstanceFinished( struct FWaveInstance* WaveInstance )
	{
		return( false );
	}

	/** 
	 * Returns the maximum distance this sound can be heard from.
	 *
	 * @param	CurrentMaxDistance	The max audible distance of all parent nodes
	 * @return	float of the greater of this node's max audible distance and its parent node's max audible distance
	 */
	virtual float MaxAudibleDistance( float CurrentMaxDistance ) 
	{ 
		return( CurrentMaxDistance ); 
	}

	/** 
	 * Returns the maximum duration this sound node will play for. 
	 *
	 * @return	float of number of seconds this sound will play for. INDEFINITELY_LOOPING_DURATION means forever.
	 */
	virtual float GetDuration( void );

	virtual void ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances );

	/**
	 * Returns an array of all (not just active) nodes.
	 */
	virtual void GetAllNodes( TArray<USoundNode*>& SoundNodes ); 

	/**
	 * Returns the maximum number of child nodes this node can possibly have
	 */
	virtual int32 GetMaxChildNodes() const
	{ 
		return 1 ; 
	}

	/** Returns the minimum number of child nodes this node must have */
	virtual int32 GetMinChildNodes() const
	{ 
		return 0;
	}


	/** 
	 * Editor interface. 
	 */

	/**
	 * Called by the Sound Cue Editor for nodes which allow children.  The default behaviour is to
	 * attach a single connector. Dervied classes can override to eg add multiple connectors.
	 */
	virtual void CreateStartingConnectors( void );
	virtual void InsertChildNode( int32 Index );
	virtual void RemoveChildNode( int32 Index );
#if WITH_EDITOR
	/**
	 * Set the entire Child Node array directly, allows GraphNodes to fully control node layout.
	 * Can be overwritten to set up additional parameters that are tied to children.
	 */
	virtual void SetChildNodes(TArray<USoundNode*>& InChildNodes);

	/** Get the name of a specific input pin */
	virtual FString GetInputPinName(int32 PinIndex) const { return TEXT(""); }

	virtual FString GetTitle() const { return GetClass()->GetDescription(); }

	/** Helper function to set the position of a sound node on a grid */
	void PlaceNode(int32 NodeColumn, int32 NodeRow, int32 RowCount );

	/** Called as PIE begins */
	virtual void OnBeginPIE(const bool bIsSimulating) {};

	/** Called as PIE ends */
	virtual void OnEndPIE(const bool bIsSimulating) {};
#endif //WITH_EDITOR

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	virtual FString GetUniqueString() const;
	static FORCEINLINE UPTRINT GetNodeWaveInstanceHash(const UPTRINT ParentWaveInstanceHash, const USoundNode* ChildNode, const uint32 ChildIndex)
	{
		checkf(ChildIndex < MAX_ALLOWED_CHILD_NODES, TEXT("Too many children (%d) in SoundCue '%s'"), ChildIndex, *CastChecked<USoundCue>(ChildNode->GetOuter())->GetFullName());
		return ((ParentWaveInstanceHash << ChildIndex) ^ (UPTRINT)ChildNode);
	}

	static FORCEINLINE UPTRINT GetNodeWaveInstanceHash(const UPTRINT ParentWaveInstanceHash, const UPTRINT ChildNodeHash, const uint32 ChildIndex)
	{
		checkf(ChildIndex < MAX_ALLOWED_CHILD_NODES, TEXT("Too many children (%d) in SoundCue"), ChildIndex);
		return ((ParentWaveInstanceHash << ChildIndex) ^ ChildNodeHash);
	}
};

