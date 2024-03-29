// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "GameplayTagAssetInterface.h"
#include "AttributeSet.h"
#include "GameplayEffect.generated.h"

DECLARE_DELEGATE_OneParam(FOnGameplayAttributeEffectExecuted, struct FGameplayModifierEvaluatedData &);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAttributeGameplayEffectSpecExected, const FGameplayAttribute &, const struct FGameplayEffectSpec &, struct FGameplayModifierEvaluatedData &);

//FGameplayEffectModCallbackData
//DECLARE_DELEGATE_TwoParam(FOnGameplayEffectSpecExected, const struct FGameplayEffectSpec &, struct FGameplayModifierEvaluatedData &);

#define SKILL_SYSTEM_AGGREGATOR_DEBUG 1

#if SKILL_SYSTEM_AGGREGATOR_DEBUG
	#define SKILL_AGG_DEBUG( Format, ... ) *FString::Printf(Format, ##__VA_ARGS__)
#else
	#define SKILL_AGG_DEBUG( Format, ... ) NULL
#endif

class UGameplayEffect;

UENUM(BlueprintType)
namespace EGameplayModOp
{
	enum Type
	{		
		// Numeric
		Additive = 0		UMETA(DisplayName="Add"),
		Multiplicitive		UMETA(DisplayName="Multiply"),
		Division			UMETA(DisplayName="Divide"),

		// Other
		Override 			UMETA(DisplayName="Override"),	// This should always be the first non numeric ModOp
		Callback			UMETA(DisplayName="Custom"),

		// This must always be at the end
		Max					UMETA(DisplayName="Invalid")
	};
}

/**
 * Tells us what thing a GameplayEffect modifies.
 */
UENUM(BlueprintType)
namespace EGameplayMod
{
	enum Type
	{
		Attribute = 0,		// Modifies this Attributes
		OutgoingGE,			// Modifies Outgoing Gameplay Effects (that modify this Attribute)
		IncomingGE,			// Modifies Incoming Gameplay Effects (that modify this Attribute)
		ActiveGE,			// Modifies currently active Gameplay Effects

		// This must always be at the end
		Max					UMETA(DisplayName="Invalid")
	};
}

/**
 * Tells us what a GameplayEffect modifies when being applied to another GameplayEffect
 */
UENUM(BlueprintType)
namespace EGameplayModEffect
{
	enum Type
	{
		Magnitude			= 0x01,		// Modifies magnitude of a GameplayEffect (Always default for Attribute mod)
		Duration			= 0x02,		// Modifies duration of a GameplayEffect
		ChanceApplyTarget	= 0x04,		// Modifies chance to apply GameplayEffect to target
		ChanceApplyEffect	= 0x08,		// Modifies chance to apply GameplayEffect to GameplayEffect
		LinkedGameplayEffect= 0x10,		// Adds a linked GameplayEffect to a GameplayEffect

		// This must always be at the end
		All					= 0xFF		UMETA(DisplayName="Invalid")
	};
}

/**
 * Tells us how to handle copying gameplay effect when it is applied.
 *	Default means to use context - e.g, OutgoingGE is are always snapshots, IncomingGE is always Link
 *	AlwaysSnapshot vs AlwaysLink let mods themselves override
 */
UENUM(BlueprintType)
namespace EGameplayEffectCopyPolicy
{
	enum Type
	{
		Default = 0			UMETA(DisplayName="Default"),
		AlwaysSnapshot		UMETA(DisplayName="AlwaysSnapshot"),
		AlwaysLink			UMETA(DisplayName="AlwaysLink")
	};
}

UENUM(BlueprintType)
namespace EGameplayEffectStackingPolicy
{
	enum Type
	{
		Unlimited = 0		UMETA(DisplayName = "NoRule"),
		Highest				UMETA(DisplayName = "Strongest"),
		Lowest				UMETA(DisplayName = "Weakest"),
		Replaces			UMETA(DisplayName = "MostRecent"),
		Callback			UMETA(DisplayName = "Custom"),

		// This must always be at the end
		Max					UMETA(DisplayName = "Invalid")
	};
}

FString EGameplayModOpToString( int32 Type );

FString EGameplayModToString( int32 Type );

FString EGameplayModEffectToString( int32 Type );

FString EGameplayEffectCopyPolicyToString(int32 Type);

FString EGameplayEffectStackingPolicyToString(int32 Type);

USTRUCT()
struct FGameplayModifierCallbacks
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, Category = GameplayModifier)
	TArray<TSubclassOf<class UGameplayEffectExtension> >	ExtensionClasses;
};

USTRUCT()
struct FGameplayEffectStackingCallbacks
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, Category = GEStack)
	TArray<TSubclassOf<class UGameplayEffectStackingExtension> >	ExtensionClasses;
};

/**
* Defines how A GameplayEffect levels
*	Normally, GameplayEffect levels are specified when they are created.
*	They can also be tied to their instigators attribute.
*		For example, a Damage apply GameplayEffect that 'levels' based on the PhysicalDamage attribute
*/
USTRUCT()
struct SKILLSYSTEM_API FGameplayEffectLevelDef
{
	GENERATED_USTRUCT_BODY()

	/** When true, whatever creates of owns this will pass in a level. E.g, level is not intrinsic to this definition. */
	UPROPERTY(EditDefaultsOnly, Category = GameplayEffectLevel)
	bool InheritLevelFromOwner;

	/** If set, the gameplay effects level will be tied to this attribute on the instigator */
	UPROPERTY(EditDefaultsOnly, Category = GameplayEffectLevel, meta = (FilterMetaTag="HideFromLevelInfos"))
	FGameplayAttribute	Attribute;

	/** If true, take snapshot of attribute level when the gameplay effect is initialized. Otherwise, the level of the gameplay effect will update as the attribute it is tied to updates */
	UPROPERTY(EditDefaultsOnly, Category = GameplayEffectLevel)
	bool TakeSnapshotOnInit;
};

/**
 * FGameplayModifierInfo
 *	Tells us "Who/What we" modify
 *	Does not tell us how exactly
 *
 */
USTRUCT()
struct SKILLSYSTEM_API FGameplayModifierInfo
{
	GENERATED_USTRUCT_BODY()

	FGameplayModifierInfo()
		: ModifierType( EGameplayMod::Attribute )
		, ModifierOp( EGameplayModOp::Additive )
		, EffectType( EGameplayModEffect::Magnitude )
		, TargetEffect( NULL )
	{

	}

	UPROPERTY(EditDefaultsOnly, Category = GameplayModifier)
	FScalableFloat Magnitude; // Not modified from defaults

	/** What this modifies - Attribute, OutgoingGEs, IncomingGEs, ACtiveGEs. */
	UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
	TEnumAsByte<EGameplayMod::Type> ModifierType;

	/** The Attribute we modify or the GE we modify modifies. */
	UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
	FGameplayAttribute Attribute;

	/** The numeric operation of this modifier: Override, Add, Multiply  */
	UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
	TEnumAsByte<EGameplayModOp::Type> ModifierOp;

	/** If we modify an effect, this is what we modify about it (Duration, Magnitude, etc) */
	UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
	TEnumAsByte<EGameplayModEffect::Type> EffectType;

	/** If we are linking a gameplay effect to another effect, this is the effect to link */
	UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
	UGameplayEffect* TargetEffect;

	// The thing I modify requires these tags
	UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
	FGameplayTagContainer RequiredTags;

	// The thing I modify must not have any of these tags
	UPROPERTY(EditDefaultsOnly, Category = GameplayModifier)
	FGameplayTagContainer IgnoreTags;
	
	/** This modifiers tags. These tags are passed to any other modifiers that this modifies. */
	UPROPERTY(EditDefaultsOnly, Category=GameplayModifier)
	FGameplayTagContainer OwnedTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = GameplayEffect)
	FGameplayEffectLevelDef	LevelInfo;

	FString ToSimpleString() const
	{
		return FString::Printf(TEXT("%s %s %s BaseVaue: %s"), *EGameplayModToString(ModifierType), *EGameplayModOpToString(ModifierOp), *EGameplayModEffectToString(EffectType), *Magnitude.ToSimpleString());
	}

	UPROPERTY(EditDefaultsOnly, Category = GameplayModifier)
	FGameplayModifierCallbacks	Callbacks;
};

/**
 * FGameplayEffectCue
 *	This is a cosmetic cue that can be tied to a UGameplayEffect. 
 *  This is essentially a GameplayTag + a Min/Max level range that is used to map the level of a GameplayEffect to a normalized value used by the GameplayCue system.
 */
USTRUCT()
struct FGameplayEffectCue
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectCue()
		: MinLevel(0.f)
		, MaxLevel(0.f)
	{
	}

	FGameplayEffectCue(FName InTagName, float InMinLevel, float InMaxLevel)
		: MinLevel(InMinLevel)
		, MaxLevel(InMaxLevel)
	{
		GameplayCueTags.AddTag(InTagName);
	}

	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	float	MinLevel;

	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	float	MaxLevel;

	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	FGameplayTagContainer GameplayCueTags;

	float NormalizeLevel(float InLevel)
	{
		float Range = MaxLevel - MinLevel;
		if (Range <= KINDA_SMALL_NUMBER)
		{
			return 1.f;
		}

		return FMath::Clamp((InLevel - MinLevel) / Range, 0.f, 1.0f);
	}
};

/**
 * UGameplayEffect
 *	The GameplayEffect definition. This is the data asset defined in the editor that drives everything.
 */
UCLASS(BlueprintType)
class SKILLSYSTEM_API UGameplayEffect : public UDataAsset, public IGameplayTagAssetInterface
{
	GENERATED_UCLASS_BODY()

public:
	/** Infinite duration */
	static const float INFINITE_DURATION;

	/** No duration; Time specifying instant application of an effect */
	static const float INSTANT_APPLICATION;

	/** Constant specifying that the combat effect has no period and doesn't check for over time application */
	static const float NO_PERIOD;
	
	UPROPERTY(EditDefaultsOnly, Category=GameplayEffect)
	FScalableFloat	Duration;

	UPROPERTY(EditDefaultsOnly, Category=GameplayEffect)
	FScalableFloat	Period;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=GameplayEffect)
	TArray<FGameplayModifierInfo> Modifiers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category=GameplayEffect)
	FGameplayEffectLevelDef	LevelInfo;
		
	// "I can only be applied to targets that have these tags"
	// "I can only exist on CE buckets on targets that have these tags":
	
	/** Container of gameplay tags that have to be present on the target for the effect to be applied */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Application)
	FGameplayTagContainer ApplicationRequiredTargetTags;

	// "I can only be applied if my instigator has these tags"

	/** Container of gameplay tags that have to be present on the instigator for the effect to be applied */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Application)
	FGameplayTagContainer ApplicationRequiredInstigatorTags;

	UPROPERTY(EditDefaultsOnly, Category=Application, meta=(GameplayAttribute="True"))
	FScalableFloat	ChanceToApplyToTarget;

	UPROPERTY(EditDefaultsOnly, Category = Application, meta = (GameplayAttribute = "True"))
	FScalableFloat	ChanceToApplyToGameplayEffect;

	// other gameplay effects  that will be applied to the target of this effect
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayEffect)
	TArray<UGameplayEffect*> TargetEffects;

	// Modify duration of CEs

	// Modify MaxStacks of CEs (maybe... probably not)

	// ------------------------------------------------
	// Functions
	// ------------------------------------------------

	/**
	 * Determines if the set of supplied gameplay tags are enough to satisfy the application tag requirements of the effect
	 * 
	 * @param InstigatorTags	Owned gameplay tags of the instigator applying the effect
	 * @param TargetTags		Owned gameplay tags of the target about to be affected by the effect
	 */
	bool AreApplicationTagRequirementsSatisfied(const TSet<FName>& InstigatorTags, const TSet<FName>& TargetTags) const;

	// ------------------------------------------------
	// New Tagging functionality
	// ------------------------------------------------

	// "These are my tags"
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Tags)
	FGameplayTagContainer GameplayEffectTags;

	// "In order to affect another GE, they must have ALL of these tags"
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Tags)
	FGameplayTagContainer GameplayEffectRequiredTags;
	
	// "In order to affect another GE, they must NOT have ANY of these tags"
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Tags)
	FGameplayTagContainer GameplayEffectIgnoreTags;

	/** Can this GameplayEffect modify the input parameter, based on tags  */
	bool AreGameplayEffectTagRequirementsSatisfied(const UGameplayEffect *GameplayEffectToBeModified) const
	{
		bool HasRequired = GameplayEffectToBeModified->GameplayEffectTags.HasAllTags(GameplayEffectRequiredTags);
		bool HasIgnored = GameplayEffectIgnoreTags.Num() > 0 && GameplayEffectToBeModified->GameplayEffectTags.HasAnyTag(GameplayEffectIgnoreTags);

		return HasRequired && !HasIgnored;
	}

	// ------------------------------------------------
	// Gameplay tag interface
	// ------------------------------------------------

	/** Overridden to return requirements tags */
	virtual void GetOwnedGameplayTags(TSet<FName>& OutTags) const OVERRIDE;

	/** Overridden to check against requirements tags */
	virtual bool HasOwnedGameplayTag(FName TagToCheck) const OVERRIDE;

	/** Get the "clear tags" for the effect */
	virtual void GetClearGameplayTags(TSet<FName>& OutTags) const;

	void ValidateGameplayEffect()
	{
		// Instant modifiers cannot have incoming/outgoing combat effect modifiers
	}

	/** Container of "owned" gameplay tags that are applied to actor with this effect applied to them */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Tags)
	FGameplayTagContainer OwnedTagsContainer;

	/** Container of gameplay tags to be cleared upon effect application; Any active effects with these tags that can be cleared, will be */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Tags)
	FGameplayTagContainer ClearTagsContainer;
	
	// Used to quickly tell if a GameplayEffect modifies another GameplayEffect (or a set of attributes)
	bool ModifiesAnyProperties(EGameplayMod::Type ModType, const TSet<UProperty> & Properties)
	{
		return false;
	}

	// -----------------------------------------------
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Advanced)
	TEnumAsByte<EGameplayEffectCopyPolicy::Type>	CopyPolicy;

	// ----------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Display)
	TArray<FGameplayEffectCue>	GameplayCues;

	/** Description of this combat effect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Display)
	FText Description;

	// ----------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	TEnumAsByte<EGameplayEffectStackingPolicy::Type>	StackingPolicy;

	UPROPERTY(EditDefaultsOnly, Category = Stacking)
	TSubclassOf<class UGameplayEffectStackingExtension> StackingExtension;
};

/**
* This handle is required for things outside of FActiveGameplayEffectsContainer to refer to a specific active GameplayEffect
*	For example if a skill needs to create an active effect and then destroy that specific effect that it created, it has to do so
*	through a handle. a pointer or index into the active list is not sufficient.
*/
USTRUCT(BlueprintType)
struct FActiveGameplayEffectHandle
{
	GENERATED_USTRUCT_BODY()

	FActiveGameplayEffectHandle()
		: Handle(INDEX_NONE)
	{

	}

	FActiveGameplayEffectHandle(int32 InHandle)
		: Handle(InHandle)
	{

	}

	bool IsValid() const
	{
		return Handle != INDEX_NONE;
	}

	FActiveGameplayEffectHandle GetNextHandle()
	{
		return FActiveGameplayEffectHandle(Handle + 1);
	}

	bool operator==(const FActiveGameplayEffectHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FActiveGameplayEffectHandle& Other) const
	{
		return Handle != Other.Handle;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d"), Handle);
	}

private:

	UPROPERTY()
	int32 Handle;
};

/**
 * FGameplayEffectLevelSpec
 *	Level Specification. This can be a static, constant level specified on creation or it can be dynamically tied to a source's attribute value.
 *	For example, a GameplayEffect could be made whose level is tied to its instigators PhysicalDamage or Intelligence attribute.
 */

struct FGameplayEffectLevelSpec
{
	static const float INVALID_LEVEL;

	FGameplayEffectLevelSpec()
		:ConstantLevel(INVALID_LEVEL)
		,CachedLevel(INVALID_LEVEL)
	{
	}

	FGameplayEffectLevelSpec(float InLevel, const FGameplayEffectLevelDef &Def, class AActor *InSource)
		: ConstantLevel(InLevel)
		, CachedLevel(InLevel)
		, Source(InSource)
	{
		if (Def.Attribute.GetUProperty() != NULL)
		{
			Attribute = Def.Attribute;
		}

		if (Def.TakeSnapshotOnInit)
		{
			SnapshotLevel();
		}
	}

	void ApplyNewDef(const FGameplayEffectLevelDef &Def, TSharedPtr<FGameplayEffectLevelSpec> &OutSharedPtr) const
	{
		if (Def.InheritLevelFromOwner)
		{
			return;
		}

		check(OutSharedPtr.IsValid());
		if (Def.Attribute != OutSharedPtr->Attribute)
		{
			// In def levels off something different
			// make a new level spec
			OutSharedPtr = TSharedPtr<FGameplayEffectLevelSpec>(new FGameplayEffectLevelSpec(INVALID_LEVEL, Def, Source.Get()));
		}
	}

	/** Dynamic simply means the level may change. It is not constant. */
	bool IsDynamic() const
	{
		return ConstantLevel == INVALID_LEVEL && Attribute.GetUProperty() != NULL;
	}

	/** Valid means we have some meaningful data. If we have an INVALID_LEVEL constant value and are not tied to a dynamic property, then we are invalid. */
	bool IsValid() const
	{
		return ConstantLevel != INVALID_LEVEL || Attribute.GetUProperty() != NULL;
	}

	float GetLevel() const;

	void SnapshotLevel()
	{
		// This should snapshot the current level (if dynamic/delegate) and save off its value so that it doesn't change
		ConstantLevel = GetLevel();
		Source = NULL;
	}

	void RegisterLevelDependancy(TWeakPtr<struct FAggregator> OwningAggregator);

	void PrintAll() const;

	mutable float ConstantLevel;	// Final/constant level. Once this is set we are locked at the given level.
	mutable float CachedLevel;		// Last read value. Needed in case we lose our source, we use the last known level.

	TWeakObjectPtr<class AActor>	Source;

	FGameplayAttribute	Attribute;
};

/**
 * FGameplayEffectInstigatorContext
 *	Data struct for an instigator. This is still being fleshed out. We will want to track actors but also be able to provide some level of tracking for actors that are destroyed.
 *	We may need to store some positional information as well.
 *
 */
USTRUCT()
struct FGameplayEffectInstigatorContext
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectInstigatorContext()
		: Instigator(NULL)
	{
	}

	void GetOwnedGameplayTags(OUT TSet<FName> &OwnedTags)
	{
		IGameplayTagAssetInterface* TagInterface = InterfaceCast<IGameplayTagAssetInterface>(Instigator);
		if (TagInterface)
		{
			TagInterface->GetOwnedGameplayTags(OwnedTags);
		}
	}

	void AddInstigator(class AActor *InInstigator);

	FString ToString() const
	{
		return Instigator ? Instigator->GetName() : FString(TEXT("NONE"));
	}

	/** Should always return the original instigator that started the whole chain */
	AActor * GetOriginalInstigator()
	{
		return Instigator;
	}

	class UAttributeComponent * GetOriginInstigatorAttributeComponent() const
	{
		return InstigatorAttributeComponent;
	}

	// @todo: Need a way to track instigator per stack
	/** Instigator controller */
	UPROPERTY(NotReplicated)
	class AActor* Instigator;

	UPROPERTY(NotReplicated)
	class UAttributeComponent *InstigatorAttributeComponent;
};

/**
 * FAggregatorRef
 *	A reference to an FAggregator. The reference may be weak or hard, and this can be changed over the lifetime of the FAggregatorRef.
 *	
 *	There are cases where we want weak references in an aggregator chain. 
 *		For example a RunSpeed buff, which when it is destroyed we want the RunSpeed attribute aggregator to recalculate the RunSpeed value.
 *
 *	There are cases where we want to make a copy of what we are weak referencing and make the reference a hard ref to that copy
 *		For example, a DOT which is buffed is attached to a target. We want to make a copy of the DOT and its buff then give it to the target as a hard ref so that if 
 *		the buff expires on the source, the applied DOT is still buffed.
 *
 */
USTRUCT()
struct FAggregatorRef
{
	GENERATED_USTRUCT_BODY()

	friend struct FAggregatorRef;

	FAggregatorRef()
	{
	}

	FAggregatorRef(struct FAggregator *Src)
		: SharedPtr(Src)
	{		 
		WeakPtr = SharedPtr;
	}

	FAggregatorRef(struct FAggregatorRef *Src)
	{
		SetSoftRef(Src);
	}

	void MakeHardRef()
	{
		check(WeakPtr.IsValid());
		SharedPtr = WeakPtr.Pin();
	}
	void MakeSoftRef()
	{
		check(WeakPtr.IsValid());
		SharedPtr.Reset();
	}

	void SetSoftRef(FAggregatorRef *Src)
	{
		check(!SharedPtr.IsValid());
		WeakPtr = Src->SharedPtr;
	}
	
	FAggregator * Get()
	{
		return WeakPtr.IsValid() ? WeakPtr.Pin().Get() : NULL;
	}

	const FAggregator * Get() const
	{
		return WeakPtr.IsValid() ? WeakPtr.Pin().Get() : NULL;
	}

	bool IsValid() const
	{
		return WeakPtr.IsValid();
	}

	/** Become a hard reference to a new copy of what we are reference  */
	void MakeUnique();

	/** Become a hard reference to a new copy of what we are reference AND make new copies/hard refs of the complete modifier chain in our FAggregator */
	void MakeUniqueDeep();

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);	

	FString ToString() const;
	void PrintAll() const;

	UPROPERTY()
	int32	Foo;

private:

	TSharedPtr<struct FAggregator>	SharedPtr;
	TWeakPtr<struct FAggregator> WeakPtr;
};

template<>
struct TStructOpsTypeTraits< FAggregatorRef > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithNetSerializer = true
	};
};

/**
 * GameplayModifierEvaluatedData
 *	This is the data that FAggregator aggregates and turns into FGameplayModifierEvaluatedData.
 *  It is distinct from FGameplayModifierEvaluatedData in that FGameplayModifierData ia level has not been applied to this data.
 *  FGameplayModifierData::Magnitude is an FScalableFloat which describes a numeric value for a given level.
 * 
 */
struct FGameplayModifierData
{
	FGameplayModifierData()
	{
		
	}

	FGameplayModifierData(const FGameplayModifierInfo &Info, const FGlobalCurveDataOverride *CurveData)
	{
		Magnitude = Info.Magnitude.MakeFinalizedCopy(CurveData);
		Tags = Info.OwnedTags;

		// Fixme: this is static data, should be a reference
		RequireTags = Info.RequiredTags;
		IgnoreTags = Info.IgnoreTags;

		if (Info.Callbacks.ExtensionClasses.Num() > 0)
		{
			Callbacks = &Info.Callbacks;
		}
	}

	FGameplayModifierData(FScalableFloat InMagnitude)
	{
		// Magnitude may scale based on our level
		Magnitude = InMagnitude;
		Callbacks = NULL;
	}

	FGameplayModifierData(float InMagnitude, const FGameplayModifierCallbacks * InCallbacks)
	{
		// Magnitude will be fixed at this value
		Magnitude.SetValue(InMagnitude);
		Callbacks = InCallbacks;
	}

	// That magnitude that we modify by
	FScalableFloat Magnitude;

	// The tags I have
	FGameplayTagContainer Tags;
	
	FGameplayTagContainer RequireTags;
	FGameplayTagContainer IgnoreTags;

	// Callback information for custom logic pre/post evaluation
	const FGameplayModifierCallbacks * Callbacks;

	void PrintAll() const;
};

/**
 * GameplayModifierEvaluatedData
 *	This is the output from an FAggregator: a numeric value and a set of GameplayTags.
 */
struct FGameplayModifierEvaluatedData
{
	FGameplayModifierEvaluatedData()
		: Magnitude(0.f)
		, Callbacks(NULL)
		, IsValid(false)
	{
	}

	FGameplayModifierEvaluatedData(float InMagnitude, const FGameplayModifierCallbacks * InCallbacks = NULL, FActiveGameplayEffectHandle InHandle = FActiveGameplayEffectHandle(), const FGameplayTagContainer *InTags = NULL)
		: Magnitude(InMagnitude)
		, Callbacks(InCallbacks)
		, Handle(InHandle)
		, IsValid(true)
	{
		if (InTags)
		{
			Tags = *InTags;
		}
	}

	float	Magnitude;
	FGameplayTagContainer Tags;
	const FGameplayModifierCallbacks * Callbacks;
	FActiveGameplayEffectHandle	Handle;	// Handle of the active gameplay effect that originated us. Will be invalid in many cases
	bool IsValid;

	// Helper function for building up final values during an aggregation
	void Aggregate(OUT FGameplayTagContainer &OutTags, OUT float &OutMagnitude, const float Bias=0.f) const;

	void InvokePreExecute(struct FGameplayEffectModCallbackData &Data) const;
	void InvokePostExecute(const struct FGameplayEffectModCallbackData &Data) const;

	void PrintAll() const;
};

/**
 * FAggregator - a data structure for aggregating stuff in GameplayEffects.
 *	Aggregates a numeric value (float) and a set of gameplay tags. This could be further extended.
 *
 *	Aggregation is done with BaseData + Mods[].
 *	-BaseData is simply the base data. We are initiliazed with base data and base data can be directly modified via ::ExecuteMod.
 *	-Mods[] are lists of other FAggregators. That is, we have a list for each EGameplayModOp: Add, multiply, override.
 *	-These lists contain FAggregatorRefs, which may be soft or hard refs to other FAggregators.
 *	-::Evalate() takes our BaseData, and then crawls through ours Mods[] list and aggregates a final output (FGameplayModifierEvaluatedData)
 *	-Results of ::Evaluate are cached in CachedData.
 *	-FAggregator also keeps a list of weak ptrs to other FAggregators that are dependant on us. If we change, we let these aggregators know, so they can invalidate their cached data.
 *
 *
 */
struct FAggregator : public TSharedFromThis<FAggregator>
{
	DECLARE_DELEGATE_OneParam(FOnDirty, FAggregator*);

	FAggregator();
	FAggregator(const FGameplayModifierData &InBaseData, TSharedPtr<FGameplayEffectLevelSpec> LevelInfo, const TCHAR* InDebugString);
	FAggregator(const FScalableFloat &InBaseMagnitude, TSharedPtr<FGameplayEffectLevelSpec> LevelInfo, const TCHAR* InDebugString);
	FAggregator(const FGameplayModifierEvaluatedData &InEvalData, const TCHAR* InDebugString);
	FAggregator(const FAggregator &In);
	virtual ~FAggregator();

	FAggregator & MarkDirty();
	void CleaerAllDependancies();

	const FGameplayModifierEvaluatedData& Evaluate() const;

	void PreEvaluate(struct FGameplayEffectModCallbackData &Data) const;
	void PostEvaluate(const struct FGameplayEffectModCallbackData &Data) const;

	void TakeSnapshotOfLevel();
	void ApplyMod(EGameplayModOp::Type ModType, FAggregatorRef Ref, bool TakeSnapshot);
	
	void ExecuteModAggr(EGameplayModOp::Type ModType, FAggregatorRef Ref);
	void ExecuteMod(EGameplayModOp::Type ModType, const FGameplayModifierEvaluatedData& EvaluatedData);

	void AddDependantAggregator(TWeakPtr<FAggregator> InDependant);

	void RegisterLevelDependancies();

	TSharedPtr<FGameplayEffectLevelSpec> Level;
	FActiveGameplayEffectHandle	ActiveHandle;	// Handle to owning active effect. Will be null in many cases.

	FGameplayModifierData		BaseData;
	TArray<FAggregatorRef>		Mods[EGameplayModOp::Max];

	TArray<TWeakPtr<FAggregator > >	Dependants;

	FOnDirty	OnDirty;

	void PrintAll() const;
	void RefreshDependencies();
	void MakeUniqueDeep();

	void SetFromNetSerialize(float NetSerialize);

	// ----------------------------------------------------------------------------
	// This is data only used in debugging/tracking where aggregator's came from
	// ----------------------------------------------------------------------------
#if SKILL_SYSTEM_AGGREGATOR_DEBUG
	FString DebugString;
	mutable int32 CopiesMade;

	FString ToSimpleString() const
	{
		return FString::Printf(TEXT("0x%X %s. CacheValid: %d Mods: [%d/%d/%d]"), this, *DebugString, CachedData.IsValid, 
			GetNumValidMods(EGameplayModOp::Override), GetNumValidMods(EGameplayModOp::Additive), GetNumValidMods(EGameplayModOp::Multiplicitive) );
	}

	struct FAllocationStats
	{
		FAllocationStats()
		{
			Reset();
		}

		void Reset()
		{
			FMemory::Memzero(this, sizeof(FAllocationStats));
		}

		int32 DefaultCStor;
		int32 ModifierCStor;
		int32 ScalableFloatCstor;
		int32 FloatCstor;
		int32 CopyCstor;

		int32 DependantsUpdated;
	};

	static FAllocationStats AllocationStats;
#else

	FString ToSimpleString() const
	{
		return FString::Printf(TEXT("CacheValid: %d Mods: [%d/%d/%d]"), CachedData.IsValid, 
			GetNumValidMods(EGameplayModOp::Override), GetNumValidMods(EGameplayModOp::Additive), GetNumValidMods(EGameplayModOp::Multiplicitive) );
	}

#endif
	

private:

	int32 GetNumValidMods(EGameplayModOp::Type Type) const
	{
		int32 Num=0;
		for (const FAggregatorRef &Agg : Mods[Type])
		{
			if (Agg.IsValid())
			{
				Num++;
			}
		}
		return Num;
	}

	mutable FGameplayModifierEvaluatedData	CachedData;
};

/** 
 * Qualification context for applying modifiers.
 *	For example a Modifier may be setup in data to only apply to OutgoingGE mods.
 *	The FModifierQualifier is the data structure to hold the 'what type of modifier are we applying' data.
 *
 *  This should ideally only hold data that is outside of FGameplayEffectSpec or FGameplayModifierSpec.
 *	For example, specs know what they can and can't modify. We don't need to duplicate that there. 
 *  FModifierQualifier is meant to hold the data that comes from the calling context that is not intrinsic 
 *  to the existing data structures.
 *
 *	This class uses an optional initialization idiom such that you can do things like:
 *		FModifierQualifier().Handle(InHandle).(InType).etc...
 */
USTRUCT(BlueprintType)
struct FModifierQualifier
{
	GENERATED_USTRUCT_BODY()

	FModifierQualifier()
		: MyType(EGameplayMod::Max)
	{
	}

	// ----------------------------------

	FModifierQualifier& Type(EGameplayMod::Type InType)
	{
		MyType = InType;
		return *this;
	}

	EGameplayMod::Type Type() const
	{
		return MyType;
	}

	// ----------------------------------
	// IgnoreHandle - ignore this handle completely. For example when executing an active gameplay effect, we never
	// want to apply it to itself (either as IncomingGE or activeGE). It is ignored in all contexts.
	//

	FModifierQualifier& IgnoreHandle(FActiveGameplayEffectHandle InHandle)
	{
		MyIgnoreHandle = InHandle;
		return *this;
	}

	FActiveGameplayEffectHandle IgnoreHandle() const
	{
		return MyIgnoreHandle;
	}

	// ----------------------------------
	// ExclusiveTarget - sometimes we need to only apply a modifier to a specific active gameplay effect.
	// We may only be able to use the handle if there are multiple instances of the same gameplay effect.
	// This only applies in the context of applying/executing to a target. 'We only modify this active effect'.
	// ExclusiveTarget is not checked in the context of applying outgoing/incoming GE modifiers to the spec.
	//

	FModifierQualifier& ExclusiveTarget(FActiveGameplayEffectHandle InHandle)
	{
		MyExclusiveTargetHandle = InHandle;
		return *this;
	}

	FActiveGameplayEffectHandle ExclusiveTarget() const
	{
		return MyExclusiveTargetHandle;
	}

	// ----------------------------------

	bool TestTarget(FActiveGameplayEffectHandle InHandle) const
	{
		if (MyIgnoreHandle.IsValid() && MyIgnoreHandle == InHandle)
			return false;

		if (MyExclusiveTargetHandle.IsValid() && MyExclusiveTargetHandle != InHandle)
			return false;

		return true;
	}

	FString ToString() const
	{
		return EGameplayModToString(MyType);
	}

private:
	EGameplayMod::Type	MyType;
	FActiveGameplayEffectHandle MyIgnoreHandle;		// Do not modify this handle
	FActiveGameplayEffectHandle MyExclusiveTargetHandle;	// Only modify this handle
};

/**
 * Modifier Specification
 *	-Const data (FGameplayModifierInfo) tells us what we modify, what we can modify
 *	-Mutable Aggregated data tells us how we modify (magnitude).
 *  
 * Modifiers can be modified. A modifier spec holds these modifications along with a reference to the const data about the modifier.
 * 
 */
struct FModifierSpec
{
	FModifierSpec(const FGameplayModifierInfo &InInfo, TSharedPtr<FGameplayEffectLevelSpec> InLevel, const FGlobalCurveDataOverride *CurveData, AActor *Owner = NULL, float Level = 0.f);

	// Hard Ref to what we modify, this stuff is const and never changes
	const FGameplayModifierInfo &Info;
	
	FAggregatorRef Aggregator;

	TSharedPtr<FGameplayEffectSpec> TargetEffectSpec;

	bool CanModifyInContext(const FModifierQualifier &QualifierContext) const;
	// returns true if this GameplayEffect can modify Other, false otherwise
	bool CanModifyModifier(FModifierSpec &Other, const FModifierQualifier &QualifierContext) const;
	
	void ApplyModTo(FModifierSpec &Other, bool TakeSnapshot) const;
	void ExecuteModOn(FModifierSpec &Other) const;

	FString ToSimpleString() const
	{
		return Info.ToSimpleString();
	}

	// Can this GameplayEffect modify the input parameter, based on tags
	// Returns true if it can modify the input parameter, false otherwise
	bool AreTagRequirementsSatisfied(const FModifierSpec &ModifierToBeModified) const;

	void PrintAll() const;
};

/**
 * GameplayEffect Specification. Tells us:
 *	-What UGameplayEffect (const data)
 *	-What Level
 *  -Who instigated
 *  
 * FGameplayEffectSpec is modifiable. We start with initial conditions and modifications be applied to it. In this sense, it is stateful/mutable but it
 * is still distinct from an FActiveGameplayEffect which in an applied instance of an FGameplayEffectSpec.
 */
USTRUCT()
struct FGameplayEffectSpec
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectSpec()
		: ModifierLevel( TSharedPtr< FGameplayEffectLevelSpec >( new FGameplayEffectLevelSpec() ) )
		, Duration(new FAggregator(FGameplayModifierEvaluatedData(0.f, NULL, FActiveGameplayEffectHandle()), SKILL_AGG_DEBUG(TEXT("Uninitialized Duration"))))
		, Period(new FAggregator(FGameplayModifierEvaluatedData(0.f, NULL, FActiveGameplayEffectHandle()), SKILL_AGG_DEBUG(TEXT("Uninitialized Period"))))
		, StackedAttribName(NAME_None)
	{
		// If we initialize a GameplayEffectSpec with no level object passed in.
	}

	FGameplayEffectSpec( const UGameplayEffect *InDef, AActor *Owner, float Level, const FGlobalCurveDataOverride *CurveData );
	
	UPROPERTY()
	const UGameplayEffect * Def;

	// Replicated	
	TSharedPtr< FGameplayEffectLevelSpec > ModifierLevel;
	
	// Replicated
	FGameplayEffectInstigatorContext InstigatorStack; // This tells us how we got here (who / what applied us)

	float GetDuration() const;
	float GetPeriod() const;
	EGameplayEffectStackingPolicy::Type GetStackingType() const;
	float GetMagnitude(const FGameplayAttribute &Attribute) const;

	// other effects that need to be applied to the target if this effect is successful
	TArray< TSharedRef< FGameplayEffectSpec > > TargetEffectSpecs;

	UPROPERTY()
	FAggregatorRef	Duration;

	FAggregatorRef	Period;

	// How this combines with other gameplay effects
	EGameplayEffectStackingPolicy::Type StackingPolicy;
	FName StackedAttribName;

	// The spec needs to own these FModifierSpecs so that other people can keep TSharedPtr to it.
	// The stuff in this array is OWNED by this spec

	TArray<FModifierSpec>	Modifiers;

	void MakeUnique();

	void InitModifiers(const FGlobalCurveDataOverride *CurveData, AActor *Owner, float Level);

	int32 ApplyModifiersFrom(FGameplayEffectSpec &InSpec, const FModifierQualifier &QualifierContext);
	int32 ExecuteModifiersFrom(const FGameplayEffectSpec &InSpec, const FModifierQualifier &QualifierContext);

	bool ShouldApplyAsSnapshot(const FModifierQualifier &QualifierContext) const;

	FString ToSimpleString() const
	{
		return FString::Printf(TEXT("%s"), *Def->GetName());
	}

	void PrintAll() const;

	// Callbacks
	FOnAttributeGameplayEffectSpecExected	OnExecute;
};

/**
 * Active GameplayEffect instance
 *	-What GameplayEffect Spec
 *	-Start time
 *  -When to execute next
 *  -Replication callbacks
 *
 */
USTRUCT()
struct FActiveGameplayEffect : public FFastArraySerializerItem
{
	GENERATED_USTRUCT_BODY()

	FActiveGameplayEffect()
		: StartGameStateTime(0)
		, StartWorldTime(0.f)
		, NextExecuteTime(0.f)
	{
	}

	FActiveGameplayEffect(FActiveGameplayEffectHandle InHandle, const FGameplayEffectSpec &InSpec, float CurrentWorldTime, int32 InStartGameStateTime)
		: Handle(InHandle)
		, Spec(InSpec)
		, StartGameStateTime(InStartGameStateTime)
		, StartWorldTime(CurrentWorldTime)
		, NextExecuteTime(0.f)
	{
		// Init NextExecuteTime if necessary
		float Period = GetPeriod();
		if (Period != UGameplayEffect::NO_PERIOD)
		{
			NextExecuteTime = CurrentWorldTime + SMALL_NUMBER;
		}

		for (FModifierSpec &Mod : Spec.Modifiers)
		{
			Mod.Aggregator.Get()->ActiveHandle = InHandle;
		}
	}

	FActiveGameplayEffectHandle Handle;

	UPROPERTY()
	FGameplayEffectSpec Spec;

	/** Game time this started */
	UPROPERTY()
	int32 StartGameStateTime;

	UPROPERTY(NotReplicated)
	float StartWorldTime;

	UPROPERTY(NotReplicated)
	float NextExecuteTime;
	
	float GetDuration() const
	{
		return Spec.GetDuration();
	}

	float GetPeriod() const
	{
		return Spec.GetPeriod();
	}

	void AdvanceNextExecuteTime(float CurrentTime, float SpillOver=0.f)
	{
		NextExecuteTime += GetPeriod();
	}

	void PrintAll() const;

	void PreReplicatedRemove(const struct FActiveGameplayEffectsContainer &InArray);
	void PostReplicatedAdd(const struct FActiveGameplayEffectsContainer &InArray);
	void PostReplicatedChange(const struct FActiveGameplayEffectsContainer &InArray) { }

	bool operator==(const FActiveGameplayEffect& Other)
	{
		return Handle == Other.Handle;
	}
};


USTRUCT()
struct FActiveGameplayEffectData
{
	GENERATED_USTRUCT_BODY()

	FActiveGameplayEffectData()
		: Handle()
		, Duration(0.f)
		, Magnitude(0.f)
	{
	}

	FActiveGameplayEffectData(const FActiveGameplayEffectHandle InHandle, const float InDuration, const float InMagnitude)
		: Handle(InHandle)
		, Duration(InDuration)
		, Magnitude(InMagnitude)
	{
	}

	UPROPERTY()
	FActiveGameplayEffectHandle	Handle;

	UPROPERTY()
	float	Duration;

	UPROPERTY()
	float	Magnitude;
};

/** Generic querying data structure for active GameplayEffects. Lets us ask things like:
 *		-Give me duration/magnitude of active gameplay effects with these tags
 *		-
 */
USTRUCT()
struct FActiveGameplayEffectQuery
{
	GENERATED_USTRUCT_BODY()

	FActiveGameplayEffectQuery()
		: TagContainer(NULL)
	{
	}

	FActiveGameplayEffectQuery(const FGameplayTagContainer * InTagContainer)
		: TagContainer(InTagContainer)
	{
	}

	const FGameplayTagContainer *	TagContainer;
};

/**
 * Active GameplayEffects Container
 *	-Bucket of ActiveGameplayEffects
 *	-Needed for FFastArraySerialization
 *  
 * This should only be used by UAttributeComponent. All of this could just live in UAttributeComponent except that we need a distinct USTRUCT to implement FFastArraySerializer.
 *
 */
USTRUCT()
struct FActiveGameplayEffectsContainer : public FFastArraySerializer
{
	GENERATED_USTRUCT_BODY();

	FActiveGameplayEffectsContainer() : bNeedToRecalculateStacks(false) {};

	UPROPERTY()
	TArray< FActiveGameplayEffect >	GameplayEffects;
	
	FActiveGameplayEffect & CreateNewActiveGameplayEffect(const FGameplayEffectSpec &Spec);

	void ApplyActiveEffectsTo(OUT FGameplayEffectSpec &Spec, const FModifierQualifier &QualifierContext);

	void ApplySpecToActiveEffectsAndAttributes(FGameplayEffectSpec &Spec, const FModifierQualifier &QualifierContext);
		
	void ExecuteActiveEffectsFrom(const FGameplayEffectSpec &Spec, const FModifierQualifier &QualifierContext);
	
	bool ExecuteGameplayEffect(FActiveGameplayEffectHandle Handle);	// This should not be outward facing to the skill system API, should only be called by the owning attribute component

	void AddDependancyToAttribute(FGameplayAttribute Attribute, const TWeakPtr<FAggregator> InDependant);

	bool RemoveActiveGameplayEffect(FActiveGameplayEffectHandle Handle);
	
	float GetGameplayEffectDuration(FActiveGameplayEffectHandle Handle) const;

	float GetGameplayEffectMagnitude(FActiveGameplayEffectHandle Handle, FGameplayAttribute Attribute) const;

	// returns true if the handle points to an effect in this container that is not a stacking effect or an effect in this container that does stack and is applied by the current stacking rules
	// returns false if the handle points to an effect that is not in this container or is not applied because of the current stacking rules
	bool IsGameplayEffectActive(FActiveGameplayEffectHandle Handle) const;

	void PrintAllGameplayEffects() const;

	int32 GetNumGameplayEffects() const
	{
		return GameplayEffects.Num();
	}

	float GetGameplayEffectMagnitudeByTag(FActiveGameplayEffectHandle Handle, FName InTagName) const;

	void OnPropertyAggregatorDirty(FAggregator* Aggregator, FGameplayAttribute Attribute);

	class UAttributeComponent *Owner;

	void TEMP_TickActiveEffects(float DeltaSeconds);

	// recalculates all of the stacks in the current container
	void RecalculateStacking();

	bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms)
	{
		return FastArrayDeltaSerialize<FActiveGameplayEffect>(GameplayEffects, DeltaParms, *this);
	}

	void PreDestroy();

	bool bNeedToRecalculateStacks;

	bool HasAnyTags(FGameplayTagContainer &Tags);

	bool CanApplyAttributeModifiers(const UGameplayEffect *GameplayEffect, float Level, AActor *Instigator);
	
	TArray<float> GetActiveEffectsTimeRemaining(const FActiveGameplayEffectQuery Query) const;

	int32 GetGameStateTime() const;
	float GetWorldTime() const;

private:

	FAggregatorRef & FindOrCreateAttributeAggregator(FGameplayAttribute Attribute);

	FActiveGameplayEffectHandle LastAssignedHandle;

	TMap<FGameplayAttribute, FAggregatorRef> OngoingPropertyEffects;

	bool IsNetAuthority() const;
};

template<>
struct TStructOpsTypeTraits< FActiveGameplayEffectsContainer > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
