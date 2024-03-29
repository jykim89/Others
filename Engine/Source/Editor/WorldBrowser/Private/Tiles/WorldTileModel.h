// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

class FWorldTileCollectionModel;
class UWorldTileDetails;
//
typedef TArray<TSharedPtr<class FWorldTileModel>> FWorldTileModelList;


/**
 * The non-UI presentation logic for a single Level (Tile) in world composition
 */
class FWorldTileModel 
	: public FLevelModel	
{
public:
	struct FCompareByLongPackageName
	{
		FORCEINLINE bool operator()(const TSharedPtr<FLevelModel>& A, 
									const TSharedPtr<FLevelModel>& B) const
		{
			return A->GetLongPackageName() < B->GetLongPackageName();
		}
	};

	enum EWorldDirections
	{
		XNegative,
		YNegative,
		XPositive,
		YPositive
	};

	/**
	 * 
	 */
	struct FLandscapeImportSettings
	{
		// Depending on landscape guid import code will spawn Landscape actor or LandscapeProxy actor
		FGuid								LandscapeGuid;
		FTransform							LandscapeTransform;
		UMaterialInterface*					LandscapeMaterial;
		int32								ComponentSizeQuads;
		int32								SectionsPerComponent;
		int32								QuadsPerSection;
		int32								SizeX;
		int32								SizeY;
		TArray<uint16>						HeightData;
		TArray<FLandscapeImportLayerInfo>	ImportLayers;
		FString								HeightmapFilename;
	};
	
	/**
	 *	FWorldTileModel Constructor
	 *
	 *	@param	InEditor		The UEditorEngine to use
	 *	@param	InWorldData		WorldBrowser world data
	 *	@param	TileIdx			Tile index in world composition tiles list
	 */
	FWorldTileModel(const TWeakObjectPtr<UEditorEngine>& InEditor, 
					FWorldTileCollectionModel& InWorldData, int32 TileIdx);
	~FWorldTileModel();

public:
	// FLevelModel interface
	virtual UObject* GetNodeObject() OVERRIDE;
	virtual ULevel* GetLevelObject() const OVERRIDE;
	virtual FName GetAssetName() const OVERRIDE;
	virtual FName GetLongPackageName() const OVERRIDE;
	virtual void Update() OVERRIDE;
	virtual void LoadLevel() OVERRIDE;
	virtual void SetVisible(bool bVisible) OVERRIDE;
	virtual bool HitTest2D(const FVector2D& Point) const OVERRIDE;
	virtual FVector2D GetLevelPosition2D() const OVERRIDE;
	virtual FVector2D GetLevelSize2D() const OVERRIDE;
	virtual void OnDrop(const TSharedPtr<FLevelDragDropOp>& Op) OVERRIDE;
	virtual bool IsGoodToDrop(const TSharedPtr<FLevelDragDropOp>& Op) const OVERRIDE;
	virtual void GetGridItemTooltipFields(TArray< TPair<TAttribute<FText>, TAttribute<FText>> >& CustomFields) const OVERRIDE;
	virtual void OnLevelAddedToWorld(ULevel* InLevel) OVERRIDE;
	virtual void OnLevelRemovedFromWorld() OVERRIDE;
	virtual void OnParentChanged() OVERRIDE;
	// FLevelModel interface end
		
	/** @return The landscape actor in case this level is landscape based */
	ALandscapeProxy* GetLandcape() const;
	
	/** Adds new streaming level*/
	void AddStreamingLevel(UClass* InStreamingClass, const FName& InPackageName);

	/** Assigns level to provided layer*/
	void AssignToLayer(const FWorldTileLayer& InLayer);

	/**	@return Whether tile is root of hierarchy */
	bool IsRootTile() const;

	/** Whether level should be visible in given area*/
	bool ShouldBeVisible(FBox EditableArea) const;

	/**	@return Whether level is shelved */
	bool IsShelved() const;

	/** Hide a level from the editor */
	void Shelve();
	
	/** Show a level in the editor */
	void Unshelve();

	/** Whether this level landscape based or not */
	bool IsLandscapeBased() const;
	
	/** Whether this level has ALandscapeProxy or not */
	bool IsLandscapeProxy() const;
	
	/** Whether this level in provided layers list */
	bool IsInLayersList(const TArray<FWorldTileLayer>& InLayerList) const;
	
	/** @return Level position in shifted space */
	FVector2D GetLevelCurrentPosition() const;

	/** @return Level relative position */
	FIntPoint GetRelativeLevelPosition() const;
	
	/** @return Level absolute position in non shifted space */
	FIntPoint GetAbsoluteLevelPosition() const;
	
	/** @return Calculates Level absolute position in non shifted space based on relative position */
	FIntPoint CalcAbsoluteLevelPosition() const;
		
	/** @return ULevel bounding box in shifted space*/
	FBox GetLevelBounds() const;
	
	/** @return Landscape component world size */
	FVector2D GetLandscapeComponentSize() const;

	/** Translate level center to new position */
	void SetLevelPosition(const FIntPoint& InPosition);

	/** Recursively sort all children by name */
	void SortRecursive();

	/** 
	 *	@return associated streaming level object for this tile
	 *	Creates a new object in case it does not exists in a persistent world
	 */
	ULevelStreaming* GetAssosiatedStreamingLevel();

	/**  */
	bool CreateAdjacentLandscapeProxy(ALandscapeProxy* SourceLandscape, FIntPoint SourceTileOffset, FWorldTileModel::EWorldDirections InWhere);

	/**  */
	ALandscapeProxy* ImportLandscape(const FLandscapeImportSettings& Settings);

private:
	/** Flush world info to package and level objects */
	void OnLevelInfoUpdated();

	/** Fixup invalid streaming objects inside level */
	void FixupStreamingObjects();

	/** Handle case when level which is based on landscape was moved or changed visibility  */
	void FixLandscapeSectionsOffset();
	
	/** Handler for LevelBoundsActorUpdated event */
	void OnLevelBoundsActorUpdated();

	/** Spawns AlevelBounds actor in the level in case it doesn't has one */
	void EnsureLevelHasBoundsActor();
	
	/** Handler for PositionChanged event from Tile details object  */
	void OnPositionPropertyChanged();
	
	/** Handler for ParentPackageName event from Tile details object  */
	void OnParentPackageNamePropertyChanged();
	
	/** Handler for StreamingLevels event from Tile details object  */
	void OnStreamingLevelsPropertyChanged();
	
	/** Handler for LOD settings changes event from Tile details object  */
	void OnLODSettingsPropertyChanged();
	
	/** Handler for ZOrder chnages event from Tile details object  */
	void OnZOrderPropertyChanged();
	
	/** Tile tooltips support */
	FText GetPositionText() const;
	FText GetBoundsExtentText() const;
	FText GetLevelLayerNameText() const;
	FText GetLevelLayerDistanceText() const;
	
public:
	/** This tile index in world composition tile list */
	int32									TileIdx;
	
	/** Package name this item represents */
	FName									AssetName;
	
	/** UObject which holds tile properties to be able to edit them via details panel*/
	UWorldTileDetails*						TileDetails;
	
	/** The Level this object represents */
	TWeakObjectPtr<ULevel>					LoadedLevel;

	// Whether this level was shelved: hidden by World Browser decision
	bool									bWasShelved;

private:
	/** Whether level has landscape components in it */
	TWeakObjectPtr<ALandscapeProxy>			Landscape;
	FVector									LandscapeComponentResolution;
	TArray<FIntPoint>						LandscapeComponentsXY;
	FIntRect								LandscapeComponentsRectXY;
	FVector2D								LandscapeComponentSize;
	FVector2D								LandscapePosition;	
};
