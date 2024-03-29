// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "LineBatchComponent.generated.h"

USTRUCT()
struct FBatchedLine
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector Start;

	UPROPERTY()
	FVector End;

	UPROPERTY()
	FLinearColor Color;

	UPROPERTY()
	float Thickness;

	UPROPERTY()
	float RemainingLifeTime;

	UPROPERTY()
	uint8 DepthPriority;

	FBatchedLine()
		: Start(ForceInit)
		, End(ForceInit)
		, Color(ForceInit)
		, Thickness(0)
		, RemainingLifeTime(0)
		, DepthPriority(0)
	{}
	FBatchedLine(const FVector& InStart, const FVector& InEnd, const FLinearColor& InColor, float InLifeTime, float InThickness, uint8 InDepthPriority)
		:	Start(InStart)
		,	End(InEnd)
		,	Color(InColor)
		,	Thickness(InThickness)
		,	RemainingLifeTime(InLifeTime)
		,	DepthPriority(InDepthPriority)
	{}
};

USTRUCT()
struct FBatchedPoint
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector Position;

	UPROPERTY()
	FLinearColor Color;

	UPROPERTY()
	float PointSize;

	UPROPERTY()
	float RemainingLifeTime;

	UPROPERTY()
	uint8 DepthPriority;

	FBatchedPoint()
		: Position(ForceInit)
		, Color(ForceInit)
		, PointSize(0)
		, RemainingLifeTime(0)
		, DepthPriority(0)
	{}
	FBatchedPoint(const FVector& InPosition, const FLinearColor& InColor, float InPointSize, float InLifeTime, uint8 InDepthPriority)
		:	Position(InPosition)
		,	Color(InColor)
		,	PointSize(InPointSize)
		,	RemainingLifeTime(InLifeTime)
		,	DepthPriority(InDepthPriority)
	{}
};

struct FBatchedMesh
{
public:
	FBatchedMesh()
		: RemainingLifeTime(0)
	{};

	/**
	 * MeshVerts - linear array of world space vertex positions
	 * MeshIndices - array of indices into MeshVerts.  Each triplet is a tri.  i.e. [0,1,2] is first tri, [3,4,5] is 2nd tri, etc
	 */
	FBatchedMesh(TArray<FVector> const& InMeshVerts, TArray<int32> const& InMeshIndices, FColor const& InColor, uint8 InDepthPriority, float LifeTime)
		: MeshVerts(InMeshVerts), MeshIndices(InMeshIndices), 
		  Color(InColor), DepthPriority(InDepthPriority), RemainingLifeTime(LifeTime)
	{}

	TArray<FVector> MeshVerts;
	TArray<int32> MeshIndices;
	FColor Color;
	uint8 DepthPriority;
	float RemainingLifeTime;
};

UCLASS(MinimalAPI)
class ULineBatchComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	TArray<struct FBatchedLine> BatchedLines;

	TArray<struct FBatchedPoint> BatchedPoints;

	float DefaultLifeTime;

	TArray<struct FBatchedMesh> BatchedMeshes;

	/** Provide many lines to draw - faster than calling DrawLine many times. */
	void DrawLines(const TArray<FBatchedLine>& InLines);

	/** Draw a box */
	ENGINE_API void DrawBox(const FBox& Box, const FMatrix& TM, const FColor& Color, uint8 DepthPriorityGroup);

	/** Draw an arrow */
	ENGINE_API void DrawDirectionalArrow(const FMatrix& ArrowToWorld,FColor InColor,float Length,float ArrowSize,uint8 DepthPriority);

	/** Draw a circle */
	ENGINE_API void DrawCircle(const FVector& Base, const FVector& X, const FVector& Y, FColor Color, float Radius, int32 NumSides, uint8 DepthPriority);

	virtual void DrawLine(
		const FVector& Start,
		const FVector& End,
		const FLinearColor& Color,
		uint8 DepthPriority,
		float Thickness = 0.0f,
		float LifeTime = 0.0f
		);
	virtual void DrawPoint(
		const FVector& Position,
		const FLinearColor& Color,
		float PointSize,
		uint8 DepthPriority,
		float LifeTime = 0.0f
		);

	/** Draw a box */
	void DrawSolidBox(FBox const& Box, FTransform const& Xform, const FColor& Color, uint8 DepthPriority, float LifeTime);
	void DrawMesh(TArray<FVector> const& Verts, TArray<int32> const& Indices, FColor const& Color, uint8 DepthPriority, float LifeTime);

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() OVERRIDE;
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const OVERRIDE;
	// End UPrimitiveComponent interface.
	
	
	// Begin UActorComponent interface.
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) OVERRIDE;
	// End UActorComponent interface.

	ENGINE_API void Flush();
};



