// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Components/SplineMeshComponent.h"
#include "LandscapeSplineSegment.generated.h"

USTRUCT()
struct FLandscapeSplineInterpPoint
{
	GENERATED_USTRUCT_BODY()

	/** Center Point */
	UPROPERTY()
	FVector Center;

	/** Left Point */
	UPROPERTY()
	FVector Left;

	/** Right Point */
	UPROPERTY()
	FVector Right;

	/** Left Falloff Point */
	UPROPERTY()
	FVector FalloffLeft;

	/** Right FalloffPoint */
	UPROPERTY()
	FVector FalloffRight;

	/** Start/End Falloff fraction */
	UPROPERTY()
	float StartEndFalloff;

	FLandscapeSplineInterpPoint()
	{
	}

	FLandscapeSplineInterpPoint(FVector InCenter, FVector InLeft, FVector InRight, FVector InFalloffLeft, FVector InFalloffRight, float InStartEndFalloff) :
		Center(InCenter),
		Left(InLeft),
		Right(InRight),
		FalloffLeft(InFalloffLeft),
		FalloffRight(InFalloffRight),
		StartEndFalloff(InStartEndFalloff)
	{
	}
};

USTRUCT()
struct FLandscapeSplineSegmentConnection
{
	GENERATED_USTRUCT_BODY()

	// Control point connected to this end of the segment
	UPROPERTY()
	class ULandscapeSplineControlPoint* ControlPoint;

	// Tangent length of the connection
	UPROPERTY(EditAnywhere, Category=LandscapeSplineSegmentConnection)
	float TangentLen;

	// Socket on the control point that we are connected to
	UPROPERTY(EditAnywhere, Category=LandscapeSplineSegmentConnection)
	FName SocketName;

	FLandscapeSplineSegmentConnection()
		: ControlPoint(NULL)
		, TangentLen(0.0f)
		, SocketName(NAME_None)
	{
	}
};

// Deprecated
UENUM()
enum LandscapeSplineMeshOrientation
{
	LSMO_XUp,
	LSMO_YUp,
	LSMO_MAX,
};

USTRUCT()
struct FLandscapeSplineMeshEntry
{
	GENERATED_USTRUCT_BODY()

	/** Mesh to use on the spline */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry)
	class UStaticMesh* Mesh;

	/** Overrides mesh's materials */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry)
	TArray<class UMaterialInterface*> MaterialOverrides;

	/** Whether to center the mesh horizontally on the spline */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry)
	uint32 bCenterH:1;

	/** X/Y offset of the mesh relative to the spline */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry)
	FVector2D Offset;

	/** Whether to scale the mesh to fit the width of the spline */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry)
	uint32 bScaleToWidth:1;

	/** Scale of the spline mesh, (Z=Forwards) */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry)
	FVector Scale;

	/** Orientation of the spline mesh, X=Up or Y=Up */
	UPROPERTY()
	TEnumAsByte<enum LandscapeSplineMeshOrientation> Orientation_DEPRECATED;

	/** Chooses the forward axis for the spline mesh orientation */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry)
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis;

	/** Chooses the up axis for the spline mesh orientation */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshEntry)
	TEnumAsByte<ESplineMeshAxis::Type> UpAxis;

	FLandscapeSplineMeshEntry() :
		Mesh(NULL),
		MaterialOverrides(),
		bCenterH(true),
		Offset(0, 0),
		bScaleToWidth(true),
		Scale(1,1,1),
		Orientation_DEPRECATED(LSMO_YUp),
		ForwardAxis(ESplineMeshAxis::X),
		UpAxis(ESplineMeshAxis::Z)
	{
	}

	bool IsValid() const;
};


UCLASS(Within=LandscapeSplinesComponent,autoExpandCategories=(LandscapeSplineSegment,LandscapeSplineMeshes),MinimalAPI)
class ULandscapeSplineSegment : public UObject
{
	GENERATED_UCLASS_BODY()

// Directly editable data:
	UPROPERTY(EditAnywhere, EditFixedSize, Category=LandscapeSplineSegment)
	FLandscapeSplineSegmentConnection Connections[2];

#if WITH_EDITORONLY_DATA
	/**
	 * Name of blend layer to paint when applying spline to landscape
	 * If "none", no layer is painted
	 */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineSegment)
	FName LayerName;

	/** Spline meshes from this list are used in random order along the spline. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshes)
	TArray<struct FLandscapeSplineMeshEntry> SplineMeshes;

	/** Random seed used for choosing which spline meshes to use. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshes)
	int32 RandomSeed;

	/**  Max draw distance for all the mesh pieces used in this spline */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = LandscapeSplineMeshes, meta = (DisplayName = "Max Draw Distance"))
	float LDMaxDrawDistance;

	/** If the spline is above the terrain, whether to raise the terrain up to the level of the spline when applying it to the landscape. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineSegment)
	uint32 bRaiseTerrain:1;

	/** If the spline is below the terrain, whether to lower the terrain down to the level of the spline when applying it to the landscape. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineSegment)
	uint32 bLowerTerrain:1;

	/** Whether to generate collision for the Spline Meshes. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshes)
	uint32 bEnableCollision:1;

	/** Whether the Spline Meshes should cast a shadow. */
	UPROPERTY(EditAnywhere, Category=LandscapeSplineMeshes)
	uint32 bCastShadow:1;

protected:
	UPROPERTY(Transient)
	uint32 bSelected : 1;

	UPROPERTY(Transient)
	uint32 bNavDirty : 1;
#endif

// Procedural data:
protected:
	/** Actual data for spline. */
	UPROPERTY()
	FInterpCurveVector SplineInfo;

	/** Spline points */
	UPROPERTY()
	TArray<FLandscapeSplineInterpPoint> Points;

	/** Bounds of points */
	UPROPERTY()
	FBox Bounds;

	/** Spline meshes */
	UPROPERTY(TextExportTransient)
	TArray<class USplineMeshComponent*> MeshComponents;

public:
	const FBox& GetBounds() const { return Bounds; }
	const TArray<FLandscapeSplineInterpPoint>& GetPoints() const { return Points; }

#if WITH_EDITOR
	bool IsSplineSelected() const { return bSelected; }
	virtual void SetSplineSelected(bool bInSelected);

	virtual void AutoFlipTangents();
	virtual void UpdateSplinePoints(bool bUpdateCollision = true);
	void UpdateSplineEditorMesh();
	virtual void DeleteSplinePoints();
#endif

	virtual void FindNearest( const FVector& InLocation, float& t, FVector& OutLocation, FVector& OutTangent );

	// Begin UObject Interface
	virtual void Serialize(FArchive& Ar) OVERRIDE;
	virtual void PostLoad() OVERRIDE;
	virtual bool Modify(bool bAlwaysMarkDirty = true) OVERRIDE;
#if WITH_EDITOR
	virtual void PostEditUndo() OVERRIDE;
	virtual void PostDuplicate(bool bDuplicateForPIE) OVERRIDE;
	virtual void PostEditImport() OVERRIDE;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) OVERRIDE;
#endif // WITH_EDITOR
protected:
	virtual void PostInitProperties() OVERRIDE;
public:
	// End UObject Interface

	void RegisterComponents();
	void UnregisterComponents();

	virtual bool OwnsComponent(const class USplineMeshComponent* SplineMeshComponent) const;

protected:
#if WITH_EDITOR
	bool FixSelfIntersection(FVector FLandscapeSplineInterpPoint::* Side);
#endif

	friend class FEdModeLandscape;
};
